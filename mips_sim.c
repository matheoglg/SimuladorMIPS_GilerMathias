#include "mips_sim.h"

// Inicialización de la CPU: limpia memorias, registros y reinicia el PC
void mips_init(MIPS_State *state) {
    memset(state, 0, sizeof(MIPS_State));
    state->pc = 0x00000000;
}

// Carga de instrucciones en memoria convirtiendo cadenas de 32 bits binarios ASCII a uint32_t
bool load_program_from_binary_txt(MIPS_State *state, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: No se pudo abrir el archivo %s\n", filename);
        return false;
    }

    char line[128];
    uint32_t addr = 0;

    while (fgets(line, sizeof(line), file) && addr < MEM_SIZE_WORDS) {
        // Ignora líneas vacías, saltos de línea y comentarios
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') continue;

        uint32_t instruction = 0;
        int bit_count = 0;

        // Construcción bit a bit desplazando a la izquierda
        for (int i = 0; line[i] != '\0' && line[i] != '\n' && line[i] != '\r'; i++) {
            if (line[i] == '0' || line[i] == '1') {
                instruction = (instruction << 1) | (line[i] - '0');
                bit_count++;
            } else if (line[i] == ' ' || line[i] == '\t') {
                continue;
            } else if (line[i] == '#') {
                break; // Ignora comentarios alineados en la misma línea
            }
        }

        // Solo almacena palabras completas de 32 bits
        if (bit_count == 32) {
            state->inst_mem[addr] = instruction;
            addr++;
        }
    }

    fclose(file);
    printf("Programa cargado: %u instrucciones leídas exitosamente desde '%s'.\n", addr, filename);
    return true;
}

// -------------------------------------------------------------------
// MÓDULOS DE CONTROL DE HAZARDS
// -------------------------------------------------------------------

// Hazard Detection Unit: Inyecta un 'stall' si la instrucción en EX es LW y la instrucción en ID lee su destino
void hazard_detection_unit(MIPS_State *state) {
    state->stall = false;

    if (state->id_ex.mem_read) {
        uint8_t if_id_rs = (state->if_id.instruction >> 21) & 0x1F;
        uint8_t if_id_rt = (state->if_id.instruction >> 16) & 0x1F;

        // Si la instrucción en ID usa como fuente el registro cargado por LW
        if ((state->id_ex.rt == if_id_rs) || (state->id_ex.rt == if_id_rt)) {
            state->stall = true; // Stall activado (bloquea PC e IF/ID, inyecta NOP en ID/EX)
        }
    }
}

// Forwarding Unit: Resuelve hazards de datos reenviando desde EX/MEM o MEM/WB a la ALU
void forwarding_unit(MIPS_State *state) {
    state->forward_a = 0;
    state->forward_b = 0;

    // EX Hazard: Reenvío prioritario desde el resultado inmediato de la ALU en EX/MEM
    if (state->ex_mem.reg_write && (state->ex_mem.write_reg != 0)) {
        if (state->ex_mem.write_reg == state->id_ex.rs) state->forward_a = 2;
        if (state->ex_mem.write_reg == state->id_ex.rt) state->forward_b = 2;
    }

    // MEM Hazard: Reenvío desde MEM/WB (solo si no hubo forwarding desde EX/MEM)
    if (state->mem_wb.reg_write && (state->mem_wb.write_reg != 0)) {
        if (state->forward_a == 0 && (state->mem_wb.write_reg == state->id_ex.rs)) {
            state->forward_a = 1;
        }
        if (state->forward_b == 0 && (state->mem_wb.write_reg == state->id_ex.rt)) {
            state->forward_b = 1;
        }
    }
}

// -------------------------------------------------------------------
// ETAPAS DEL PIPELINE
// -------------------------------------------------------------------

// 1. INSTRUCTION FETCH (IF): Obtiene la instrucción de memoria y actualiza el PC
void stage_fetch(MIPS_State *state, MIPS_State *next) {
    if (state->stall) {
        next->if_id = state->if_id; // Congela el registro IF/ID durante un stall
        return;
    }
    uint32_t word_addr = state->pc >> 2; // Conversión de byte-addressing a word-indexing
    next->if_id.instruction = (word_addr < MEM_SIZE_WORDS) ? state->inst_mem[word_addr] : 0;
    next->if_id.pc = state->pc + 4;
    next->pc = state->pc + 4;
}

// 2. INSTRUCTION DECODE (ID): Decodifica la instrucción, lee registros y genera señales de control
void stage_decode(MIPS_State *state, MIPS_State *next) {
    hazard_detection_unit(state);

    // En caso de Stall, inyecta una burbuja (NOP con todas las señales en 0) a la etapa EX
    if (state->stall) {
        memset(&next->id_ex, 0, sizeof(ID_EX_Register));
        return;
    }

    uint32_t inst = state->if_id.instruction;
    uint8_t opcode = (inst >> 26) & 0x3F;
    uint8_t rs     = (inst >> 21) & 0x1F;
    uint8_t rt     = (inst >> 16) & 0x1F;
    uint8_t rd     = (inst >> 11) & 0x1F;
    uint8_t funct  = inst & 0x3F;
    int16_t imm16  = (int16_t)(inst & 0xFFFF);

    next->id_ex.rs = rs;
    next->id_ex.rt = rt;
    next->id_ex.rd = rd;
    next->id_ex.pc_plus_4 = state->if_id.pc;

    // Latch de operandos (lectura asíncrona, R0 siempre devuelve 0)
    next->id_ex.read_data_1 = (rs == 0) ? 0 : state->registers[rs];
    next->id_ex.read_data_2 = (rt == 0) ? 0 : state->registers[rt];

    // Extensión de signo (aritmética vs lógica sin signo)
    if (opcode == 0x0C || opcode == 0x0D || opcode == 0x0E) {
        next->id_ex.imm_extended = (uint32_t)(inst & 0xFFFF); // Extensión de ceros (ANDI, ORI, XORI)
    } else {
        next->id_ex.imm_extended = (uint32_t)((int32_t)imm16); // Extensión de signo
    }

    // Reset de la unidad de control
    next->id_ex.reg_dst   = false;
    next->id_ex.alu_src   = false;
    next->id_ex.mem_to_reg = false;
    next->id_ex.reg_write = false;
    next->id_ex.mem_read  = false;
    next->id_ex.mem_write = false;
    next->id_ex.branch    = false;
    next->id_ex.bne       = false;
    next->id_ex.jump      = false;
    next->id_ex.jr        = false;
    next->id_ex.alu_op    = 0;

    // Generación de señales de control según la decodificación del Opcode
    if (opcode == 0x00) { // Tipo R
        if (funct == 0x08) {
            next->id_ex.jr = true;
        } else if (inst != 0) { // Excluye NOPs
            next->id_ex.reg_dst   = true;
            next->id_ex.reg_write = true;
            next->id_ex.alu_op    = funct;
        }
    } else { // Tipo I / J
        switch (opcode) {
            case 0x08: // addi
                next->id_ex.reg_dst   = false;
                next->id_ex.alu_src   = true;  // Selecciona el inmediato para la ALU
                next->id_ex.reg_write = true;
                next->id_ex.mem_to_reg = false;
                next->id_ex.alu_op    = 0x20;
                break;
            case 0x23: // lw
                next->id_ex.alu_src    = true;
                next->id_ex.mem_to_reg = true;
                next->id_ex.reg_write  = true;
                next->id_ex.mem_read   = true;
                next->id_ex.alu_op     = 0x20;
                break;
            case 0x2B: // sw
                next->id_ex.alu_src   = true;
                next->id_ex.mem_write = true;
                next->id_ex.alu_op    = 0x20;
                break;
            case 0x04: // beq
                next->id_ex.branch = true;
                next->id_ex.alu_op = 0x22;
                break;
            case 0x05: // bne
                next->id_ex.bne    = true;
                next->id_ex.alu_op = 0x22;
                break;
            case 0x02: // j
                next->id_ex.jump = true;
                break;
        }
    }
}

// 3. EXECUTE (EX): Selección de operandos con MUX de Forwarding y cálculo en la ALU
void stage_execute(MIPS_State *state, MIPS_State *next) {
    forwarding_unit(state);

    // MUX de Forwarding para el operando A
    uint32_t op1 = state->id_ex.read_data_1;
    if (state->forward_a == 2) {
        op1 = state->ex_mem.alu_result;
    } else if (state->forward_a == 1) {
        op1 = state->mem_wb.mem_to_reg ? state->mem_wb.mem_read_data : state->mem_wb.alu_result;
    }

    // MUX de Forwarding para la ruta de registro del operando B
    uint32_t reg_op2 = state->id_ex.read_data_2;
    if (state->forward_b == 2) {
        reg_op2 = state->ex_mem.alu_result;
    } else if (state->forward_b == 1) {
        reg_op2 = state->mem_wb.mem_to_reg ? state->mem_wb.mem_read_data : state->mem_wb.alu_result;
    }

    // MUX ALUSrc: Selecciona entre registro reenviado o inmediato extendido
    uint32_t op2 = state->id_ex.alu_src ? state->id_ex.imm_extended : reg_op2;
    uint32_t res = 0;

    // Unidad Aritmético-Lógica (ALU)
    switch (state->id_ex.alu_op) {
        case 0x20: res = op1 + op2; break;
        case 0x22: res = op1 - op2; break;
        case 0x24: res = op1 & op2; break;
        case 0x25: res = op1 | op2; break;
        case 0x27: res = ~(op1 | op2); break;
        case 0x26: res = op1 ^ op2; break;
        default:   res = 0; break;
    }

    // Latch hacia EX/MEM
    next->ex_mem.alu_result     = res;
    next->ex_mem.write_data_mem = reg_op2; // Pasa el valor reenviado para las instrucciones SW
    next->ex_mem.zero           = (res == 0);
    next->ex_mem.write_reg      = state->id_ex.reg_dst ? state->id_ex.rd : state->id_ex.rt;

    // Propagación de señales de control
    next->ex_mem.mem_to_reg = state->id_ex.mem_to_reg;
    next->ex_mem.reg_write  = state->id_ex.reg_write;
    next->ex_mem.mem_read   = state->id_ex.mem_read;
    next->ex_mem.mem_write  = state->id_ex.mem_write;
}

// 4. MEMORY (MEM): Acceso a la Memoria de Datos (Load/Store)
void stage_memory(MIPS_State *state, MIPS_State *next) {
    uint32_t addr = state->ex_mem.alu_result;
    uint32_t word_addr = addr >> 2; // Alineamiento por palabra (bloques de 4 bytes)

    if (state->ex_mem.mem_read) {
        if (word_addr < MEM_SIZE_WORDS) {
            next->mem_wb.mem_read_data = state->data_mem[word_addr];
        }
    } else if (state->ex_mem.mem_write) {
        if (word_addr < MEM_SIZE_WORDS) {
            next->data_mem[word_addr] = state->ex_mem.write_data_mem;
        }
    }

    // Propagación de datos y señales al latch MEM/WB
    next->mem_wb.alu_result = state->ex_mem.alu_result;
    next->mem_wb.write_reg  = state->ex_mem.write_reg;
    next->mem_wb.reg_write  = state->ex_mem.reg_write;
    next->mem_wb.mem_to_reg = state->ex_mem.mem_to_reg;
}

// 5. WRITE BACK (WB): Escritura síncrona en el banco de registros
void stage_writeback(MIPS_State *state, MIPS_State *next) {
    if (state->mem_wb.reg_write) {
        uint8_t dest = state->mem_wb.write_reg;
        uint32_t data = state->mem_wb.mem_to_reg ? state->mem_wb.mem_read_data : state->mem_wb.alu_result;

        printf("[WB Stage] Escribiendo dato %u en registro R%d\n", data, dest);
        
        // Protege R0 ($zero) contra escrituras no deseadas
        if (dest != 0) {
            next->registers[dest] = data;
        }
    }
}

// Formateo y despliegue en consola del estado interno de la CPU
void print_mips_state(const MIPS_State *state) {
    printf("\n========================================================\n");
    printf("                  ESTADO DEL PROCESADOR                 \n");
    printf("========================================================\n");
    printf(" Program Counter (PC): 0x%08X\n", state->pc);
    printf("--------------------------------------------------------\n");
    printf(" REGISTROS DE PROPÓSITO GENERAL:\n");

    const char *reg_names[32] = {
        "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
        "$t0",   "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
        "$s0",   "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
        "$t8",   "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
    };

    for (int i = 0; i < 32; i += 4) {
        printf(" %-5s (%2d): %-6d | %-5s (%2d): %-6d | %-5s (%2d): %-6d | %-5s (%2d): %-6d\n",
            reg_names[i],   i,   state->registers[i],
            reg_names[i+1], i+1, state->registers[i+1],
            reg_names[i+2], i+2, state->registers[i+2],
            reg_names[i+3], i+3, state->registers[i+3]);
    }

    printf("--------------------------------------------------------\n");
    printf(" MEMORIA DE DATOS (Primeras palabras no nulas):\n");
    bool empty = true;
    for (int i = 0; i < 16; i++) {
        if (state->data_mem[i] != 0) {
            printf("  Mem[0x%08X / Word %d] = %d (0x%08X)\n", i * 4, i, state->data_mem[i], state->data_mem[i]);
            empty = false;
        }
    }
    if (empty) printf("  (Memoria de datos vacía)\n");
    printf("========================================================\n\n");
}