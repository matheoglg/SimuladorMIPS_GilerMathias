#ifndef MIPS_SIM_H
#define MIPS_SIM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define NUM_REGS 32
#define MEM_SIZE_WORDS 1024 // 4KB de memoria

// Estructura de Registros entre Etapas del Pipeline
typedef struct {
    uint32_t pc;
    uint32_t instruction;
} IF_ID_Register;

typedef struct {
    uint32_t pc_plus_4;
    uint32_t read_data_1;
    uint32_t read_data_2;
    uint32_t imm_extended;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    // Señales de Control
    bool reg_dst;
    bool alu_src;
    bool mem_to_reg;
    bool reg_write;
    bool mem_read;
    bool mem_write;
    bool branch;
    bool bne;
    bool jump;
    bool jr;
    uint8_t alu_op;
} ID_EX_Register;

typedef struct {
    uint32_t alu_result;
    uint32_t write_data_mem;
    uint8_t write_reg;
    bool zero;
    // Señales de Control pasadas a MEM y WB
    bool mem_to_reg;
    bool reg_write;
    bool mem_read;
    bool mem_write;
} EX_MEM_Register;

typedef struct {
    uint32_t alu_result;
    uint32_t mem_read_data;
    uint8_t write_reg;
    bool mem_to_reg;
    bool reg_write;
} MEM_WB_Register;

// Estado Global de la Máquina
typedef struct {
    uint32_t pc;
    uint32_t registers[NUM_REGS];
    uint32_t inst_mem[MEM_SIZE_WORDS];
    uint32_t data_mem[MEM_SIZE_WORDS];
    
    // Registros Internos Pipeline
    IF_ID_Register if_id;
    ID_EX_Register id_ex;
    EX_MEM_Register ex_mem;
    MEM_WB_Register mem_wb;
} MIPS_State;

// Prototipos de las 5 Etapas
void stage_fetch(MIPS_State *state);
void stage_decode(MIPS_State *state);
void stage_execute(MIPS_State *state);
void stage_memory(MIPS_State *state);
void stage_writeback(MIPS_State *state);

// Helpers, Carga de Archivos y Tests
void mips_init(MIPS_State *state);
bool load_program_from_binary_txt(MIPS_State *state, const char *filename);
void run_unit_tests(void);
void run_system_test(void);

#endif // MIPS_SIM_H