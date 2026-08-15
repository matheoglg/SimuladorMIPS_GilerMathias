#ifndef MIPS_SIM_H
#define MIPS_SIM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define NUM_REGS 32
#define MEM_SIZE_WORDS 1024 // Capacidad de memoria expressada en palabras de 32 bits

/* --- REGISTROS INTER-ETAPAS DEL PIPELINE --- */

// Latch entre Fetch e Instruction Decode
typedef struct {
    uint32_t pc;
    uint32_t instruction;
} IF_ID_Register;

// Latch entre Instruction Decode y Execute (propaga operandos y señales de control)
typedef struct {
    uint32_t pc_plus_4;
    uint32_t read_data_1;
    uint32_t read_data_2;
    uint32_t imm_extended;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    
    // Señales de Control para las etapas EX, MEM y WB
    bool reg_dst;        // Selección del registro destino (rt vs rd)
    bool alu_src;        // Selección del segundo operando de la ALU (rt vs Inmediato)
    bool mem_to_reg;     // Selección del dato a escribir en RF (ALU vs Memoria)
    bool reg_write;      // Habilitación de escritura en banco de registros
    bool mem_read;       // Lectura en memoria de datos
    bool mem_write;      // Escritura en memoria de datos
    bool branch;         // Control de salto condicional (BEQ)
    bool bne;            // Control de salto condicional (BNE)
    bool jump;           // Control de salto incondicional
    bool jr;             // Control de salto a registro
    uint8_t alu_op;      // Operación a ejecutar por la ALU
} ID_EX_Register;

// Latch entre Execute y Memory Access
typedef struct {
    uint32_t alu_result;
    uint32_t write_data_mem; // Dato a escribir en SW
    uint8_t write_reg;       // Índice del registro destino final
    bool zero;              // Bandera Zero de la ALU
    
    // Señales de Control propagadas para MEM y WB
    bool mem_to_reg;
    bool reg_write;
    bool mem_read;
    bool mem_write;
} EX_MEM_Register;

// Latch entre Memory Access y Write Back
typedef struct {
    uint32_t alu_result;
    uint32_t mem_read_data;  // Dato leído desde memoria (LW)
    uint8_t write_reg;       // Índice del registro destino final
    
    // Señales de Control propagadas para WB
    bool mem_to_reg;
    bool reg_write;
} MEM_WB_Register;

/* --- ESTADO GLOBAL DE LA CPU --- */

typedef struct {
    uint32_t pc;
    uint32_t registers[NUM_REGS];
    uint32_t inst_mem[MEM_SIZE_WORDS]; // Memoria de Instrucciones
    uint32_t data_mem[MEM_SIZE_WORDS]; // Memoria de Datos

    // Registros inter-etapa del pipeline
    IF_ID_Register if_id;
    ID_EX_Register id_ex;
    EX_MEM_Register ex_mem;
    MEM_WB_Register mem_wb;

    // Control de Hazards y Reenvío
    bool stall;          // Inyección de burbuja por Load-Use Hazard
    uint8_t forward_a;   // Selectores MUX de forwarding para operando A (00: RF, 10: EX/MEM, 01: MEM/WB)
    uint8_t forward_b;   // Selectores MUX de forwarding para operando B
} MIPS_State;

/* --- PROTOTIPOS DE LAS ETAPAS DEL PIPELINE --- */

void stage_fetch(MIPS_State *state, MIPS_State *next);
void stage_decode(MIPS_State *state, MIPS_State *next);
void stage_execute(MIPS_State *state, MIPS_State *next);
void stage_memory(MIPS_State *state, MIPS_State *next);
void stage_writeback(MIPS_State *state, MIPS_State *next);

/* --- UNIDADES DE CONTROL DE HAZARDS --- */

void hazard_detection_unit(MIPS_State *state);
void forwarding_unit(MIPS_State *state);

/* --- FUNCIONES AUXILIARES Y DE SISTEMA --- */

void mips_init(MIPS_State *state);
bool load_program_from_binary_txt(MIPS_State *state, const char *filename);
void print_mips_state(const MIPS_State *state);

/* --- SUITE DE PRUEBAS --- */

void run_unit_tests(void);
void run_system_test(void);

#endif // MIPS_SIM_H