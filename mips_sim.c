#include "mips_sim.h"

void mips_init(MIPS_State *state) {
    memset(state, 0, sizeof(MIPS_State));
    state->pc = 0x00000000;
}

// Carga de instrucciones en texto binario (32 caracteres de '0' y '1' por línea)
bool load_program_from_binary_txt(MIPS_State *state, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: No se pudo abrir el archivo %s\n", filename);
        return false;
    }

    char line[128];
    uint32_t addr = 0;

    while (fgets(line, sizeof(line), file) && addr < MEM_SIZE_WORDS) {
        // Ignorar líneas vacías, saltos de línea o comentarios con '#'
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') continue;

        uint32_t instruction = 0;
        int bit_count = 0;

        for (int i = 0; line[i] != '\0' && line[i] != '\n' && line[i] != '\r'; i++) {
            if (line[i] == '0' || line[i] == '1') {
                instruction = (instruction << 1) | (line[i] - '0');
                bit_count++;
            } else if (line[i] == ' ' || line[i] == '\t') {
                continue; // Permitir espacios para legibilidad
            } else if (line[i] == '#') {
                break; // Ignorar comentarios inline
            }
        }

        if (bit_count == 32) {
            state->inst_mem[addr] = instruction;
            addr++;
        }
    }

    fclose(file);
    printf("Programa cargado: %u instrucciones leídas exitosamente desde '%s'.\n", addr, filename);
    return true;
}

// 1. INSTRUCTION FETCH (IF)
void stage_fetch(MIPS_State *state) {
    uint32_t word_addr = state->pc >> 2;
    if (word_addr < MEM_SIZE_WORDS) {
        state->if_id.instruction = state->inst_mem[word_addr];
    } else {
        state->if_id.instruction = 0x00000000;
    }
    state->if_id.pc = state->pc + 4;
    state->pc += 4;
}

// 2. INSTRUCTION DECODE (ID)
void stage_decode(MIPS_State *state) {
    uint32_t inst = state->if_id.instruction;
    
    uint8_t opcode = (inst >> 26) & 0x3F;
    uint8_t rs     = (inst >> 21) & 0x1F;
    uint8_t rt     = (inst >> 16) & 0x1F;
    uint8_t rd     = (inst >> 11) & 0x1F;
    uint8_t funct  = inst & 0x3F;
    int16_t imm16  = (int16_t)(inst & 0xFFFF);

    state->id_ex.rs = rs;
    state->id_ex.rt = rt;
    state->id_ex.rd = rd;
    state->id_ex.pc_plus_4 = state->if_id.pc;
    
    // Lectura de Registros ($zero siempre devuelve 0)
    state->id_ex.read_data_1 = (rs == 0) ? 0 : state->registers[rs];
    state->id_ex.read_data_2 = (rt == 0) ? 0 : state->registers[rt];

    // Extensión de Signo vs Cero
    if (opcode == 0x0C || opcode == 0x0D || opcode == 0x0E) { // andi, ori, xori
        state->id_ex.imm_extended = (uint32_t)(inst & 0xFFFF);
    } else {
        state->id_ex.imm_extended = (uint32_t)((int32_t)imm16);
    }

    // Limpiar estructura de señales de control
    memset(&state->id_ex.reg_dst, 0, sizeof(ID_EX_Register) - offsetof(ID_EX_Register, reg_dst));

    // Generación de Control
    if (opcode == 0x00) { // Tipo R
        if (funct == 0x08) { // jr
            state->id_ex.jr = true;
        } else {
            state->id_ex.reg_dst   = true;
            state->id_ex.reg_write = true;
            state->id_ex.alu_op    = funct;
        }
    } else { // Tipo I / J
        switch (opcode) {
            case 0x08: // addi
                state->id_ex.alu_src   = true;
                state->id_ex.reg_write = true;
                state->id_ex.alu_op    = 0x20;
                break;
            case 0x23: // lw
                state->id_ex.alu_src    = true;
                state->id_ex.mem_to_reg = true;
                state->id_ex.reg_write  = true;
                state->id_ex.mem_read   = true;
                state->id_ex.alu_op     = 0x20;
                break;
            case 0x2B: // sw
                state->id_ex.alu_src   = true;
                state->id_ex.mem_write = true;
                state->id_ex.alu_op    = 0x20;
                break;
            case 0x04: // beq
                state->id_ex.branch = true;
                state->id_ex.alu_op = 0x22;
                break;
            case 0x05: // bne
                state->id_ex.bne    = true;
                state->id_ex.alu_op = 0x22;
                break;
            case 0x02: // j
                state->id_ex.jump = true;
                break;
        }
    }
}

// 3. EXECUTE (EX)
void stage_execute(MIPS_State *state) {
    uint32_t op1 = state->id_ex.read_data_1;
    uint32_t op2 = state->id_ex.alu_src ? state->id_ex.imm_extended : state->id_ex.read_data_2;
    uint32_t res = 0;

    switch (state->id_ex.alu_op) {
        case 0x20: res = op1 + op2; break; // ADD / ADDI / LW / SW
        case 0x22: res = op1 - op2; break; // SUB / BEQ / BNE
        case 0x24: res = op1 & op2; break; // AND
        case 0x25: res = op1 | op2; break; // OR
        case 0x27: res = ~(op1 | op2); break; // NOR
        case 0x26: res = op1 ^ op2; break; // XOR
        default:   res = 0; break;
    }

    state->ex_mem.alu_result     = res;
    state->ex_mem.write_data_mem = state->id_ex.read_data_2;
    state->ex_mem.zero           = (res == 0);
    state->ex_mem.write_reg      = state->id_ex.reg_dst ? state->id_ex.rd : state->id_ex.rt;

    state->ex_mem.mem_to_reg = state->id_ex.mem_to_reg;
    state->ex_mem.reg_write  = state->id_ex.reg_write;
    state->ex_mem.mem_read   = state->id_ex.mem_read;
    state->ex_mem.mem_write  = state->id_ex.mem_write;
}

// 4. MEMORY (MEM)
void stage_memory(MIPS_State *state) {
    uint32_t addr = state->ex_mem.alu_result;
    uint32_t word_addr = addr >> 2;

    if (state->ex_mem.mem_read) {
        if (word_addr < MEM_SIZE_WORDS) {
            state->mem_wb.mem_read_data = state->data_mem[word_addr];
        }
    } else if (state->ex_mem.mem_write) {
        if (word_addr < MEM_SIZE_WORDS) {
            state->data_mem[word_addr] = state->ex_mem.write_data_mem;
        }
    }

    state->mem_wb.alu_result = state->ex_mem.alu_result;
    state->mem_wb.write_reg  = state->ex_mem.write_reg;
    state->mem_wb.mem_to_reg = state->ex_mem.mem_to_reg;
    state->mem_wb.reg_write  = state->ex_mem.reg_write;
}

// 5. WRITE BACK (WB)
void stage_writeback(MIPS_State *state) {
    if (state->mem_wb.reg_write) {
        uint8_t dest = state->mem_wb.write_reg;
        uint32_t data = state->mem_wb.mem_to_reg ? state->mem_wb.mem_read_data : state->mem_wb.alu_result;

        if (dest != 0) {
            state->registers[dest] = data;
        }
    }
}