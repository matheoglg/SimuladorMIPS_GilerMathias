#include "mips_sim.h"
#include <assert.h>

void run_unit_tests(void) {
    printf("=== EJECUTANDO UNIT TESTS ===\n");
    MIPS_State sim;

    // Test 1: $zero inmutable
    mips_init(&sim);
    sim.mem_wb.reg_write = true;
    sim.mem_wb.write_reg = 0;
    sim.mem_wb.alu_result = 0xDEADBEEF;
    stage_writeback(&sim);
    assert(sim.registers[0] == 0);
    printf("[PASS] Unit Test 1: $zero permanece inalterado en 0.\n");

    // Test 2: ALU SUB
    mips_init(&sim);
    sim.id_ex.read_data_1 = 50;
    sim.id_ex.read_data_2 = 20;
    sim.id_ex.alu_src     = false;
    sim.id_ex.alu_op      = 0x22;
    stage_execute(&sim);
    assert(sim.ex_mem.alu_result == 30);
    printf("[PASS] Unit Test 2: Operación SUB de ALU correcta.\n");

    // Test 3: Memoria SW / LW
    mips_init(&sim);
    sim.ex_mem.mem_write = true;
    sim.ex_mem.alu_result = 0x00000010;
    sim.ex_mem.write_data_mem = 0x12345678;
    stage_memory(&sim);
    assert(sim.data_mem[4] == 0x12345678);

    sim.ex_mem.mem_write = false;
    sim.ex_mem.mem_read = true;
    stage_memory(&sim);
    assert(sim.mem_wb.mem_read_data == 0x12345678);
    printf("[PASS] Unit Test 3: Acceso a Memoria alineado exitoso.\n\n");
}

void run_system_test(void) {
    printf("=== EJECUTANDO TEST INTEGRAL (MEMORIA SINTÉTICA) ===\n");
    MIPS_State sim;
    mips_init(&sim);

    sim.inst_mem[0] = 0x2008000F; // addi $t0, $zero, 15
    sim.inst_mem[1] = 0x2009001B; // addi $t1, $zero, 27
    sim.inst_mem[2] = 0x01095020; // add  $t2, $t0, $t1
    sim.inst_mem[3] = 0xAD6A0004; // sw   $t2, 4($t3)
    sim.inst_mem[4] = 0x8D6B0004; // lw   $t3, 4($t3)

    for (int cycle = 0; cycle < 12; cycle++) {
        stage_writeback(&sim);
        stage_memory(&sim);
        stage_execute(&sim);
        stage_decode(&sim);
        stage_fetch(&sim);
    }

    assert(sim.registers[8] == 15);
    assert(sim.registers[9] == 27);
    assert(sim.registers[10] == 42);
    assert(sim.registers[11] == 42);
    printf("[PASS] Test de Sistema Integrado completado con éxito.\n\n");
}