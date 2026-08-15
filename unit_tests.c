#include "mips_sim.h"

void run_unit_tests(void) {
    printf("=== EJECUTANDO UNIT TESTS ===\n");
    MIPS_State sim;

    // Test 1: $zero inmutable
    mips_init(&sim);
    sim.mem_wb.reg_write = true;
    sim.mem_wb.write_reg = 0;
    sim.mem_wb.alu_result = 0xDEADBEEF;
    stage_writeback(&sim, &sim);
    if (sim.registers[0] == 0) {
        printf("[PASS] Unit Test 1: $zero permanece inalterado en 0.\n");
    } else {
        printf("[FAIL] Unit Test 1: $zero fue modificado a %d.\n", sim.registers[0]);
    }

    // Test 2: ALU SUB
    mips_init(&sim);
    sim.id_ex.read_data_1 = 50;
    sim.id_ex.read_data_2 = 20;
    sim.id_ex.alu_src     = false;
    sim.id_ex.alu_op      = 0x22;
    stage_execute(&sim, &sim);
    if (sim.ex_mem.alu_result == 30) {
        printf("[PASS] Unit Test 2: Operación SUB de ALU correcta.\n");
    } else {
        printf("[FAIL] Unit Test 2: ALU SUB dio %d (Esperado: 30).\n", sim.ex_mem.alu_result);
    }

    // Test 3: Memoria SW / LW
    mips_init(&sim);
    sim.ex_mem.mem_write = true;
    sim.ex_mem.alu_result = 0x00000010;
    sim.ex_mem.write_data_mem = 0x12345678;
    stage_memory(&sim, &sim);
    
    bool sw_ok = (sim.data_mem[4] == 0x12345678);

    sim.ex_mem.mem_write = false;
    sim.ex_mem.mem_read = true;
    stage_memory(&sim, &sim);
    bool lw_ok = (sim.mem_wb.mem_read_data == 0x12345678);

    if (sw_ok && lw_ok) {
        printf("[PASS] Unit Test 3: Acceso a Memoria alineado exitoso.\n");
    } else {
        printf("[FAIL] Unit Test 3: Falló lectura/escritura de memoria.\n");
    }

    // Test 4: Forwarding EX hazard
    mips_init(&sim);
    sim.id_ex.rs = 8;
    sim.id_ex.rt = 9;
    sim.id_ex.read_data_1 = 0; // Valor viejo
    sim.id_ex.read_data_2 = 5;
    sim.ex_mem.reg_write = true;
    sim.ex_mem.write_reg = 8;
    sim.ex_mem.alu_result = 10; // Valor a reenviar
    forwarding_unit(&sim);
    if (sim.forward_a == 2) {
        printf("[PASS] Unit Test 4: Forwarding Unit detectó hazard EX correctamente.\n");
    } else {
        printf("[FAIL] Unit Test 4: Forwarding A dio %d (Esperado: 2).\n", sim.forward_a);
    }

    // Test 5: Hazard Detection (Stall)
    mips_init(&sim);
    sim.id_ex.mem_read = true;
    sim.id_ex.rt = 10;
    sim.if_id.instruction = 0x01495820; // Usa rs = 10 ($t2)
    hazard_detection_unit(&sim);
    if (sim.stall) {
        printf("[PASS] Unit Test 5: Hazard Detection Unit inyectó Stall (Load-Use).\n\n");
    } else {
        printf("[FAIL] Unit Test 5: No se detectó el Load-Use hazard.\n\n");
    }
}

void run_system_test(void) {
    printf("=== EJECUTANDO TEST INTEGRAL (CON FORWARDING Y SINCRO) ===\n");
    MIPS_State sim;
    mips_init(&sim);

    // Instrucciones de prueba en código máquina Hex
    sim.inst_mem[0] = 0x2008000F; // addi $t0, $zero, 15
    sim.inst_mem[1] = 0x2009001B; // addi $t1, $zero, 27
    sim.inst_mem[2] = 0x01095020; // add  $t2, $t0, $t1  ($t2 = 42 con forwarding)
    sim.inst_mem[3] = 0xAD6A0004; // sw   $t2, 4($t3)
    sim.inst_mem[4] = 0x8D6B0004; // lw   $t3, 4($t3)

    // Ejecución de pipeline respetando la lectura del estado actual
    for (int cycle = 0; cycle < 30; cycle++) {
        MIPS_State next = sim; // Copia del estado actual

        stage_writeback(&sim, &next);
        stage_memory(&sim, &next);
        stage_execute(&sim, &next);
        stage_decode(&sim, &next);
        stage_fetch(&sim, &next);

        sim = next; // Actualización al final del ciclo de reloj
    }

    if (sim.registers[10] == 42) {
        printf("[PASS] Test de Sistema: Forwarding completado exitosamente ($t2 = 42).\n\n");
    } else {
        printf("[FAIL] Test de Sistema: $t2 dio %d (Esperado: 42).\n\n", sim.registers[10]);
    }
}