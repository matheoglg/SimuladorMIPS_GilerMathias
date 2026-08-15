#include "mips_sim.h"

int main(int argc, char *argv[]) {
    printf("===========================================\n");
    printf(" SIMULADOR MIPS 5 ETAPAS (PIPELINED IN C)  \n");
    printf("===========================================\n\n");

    // 1. Correr Suite de Pruebas
    run_unit_tests();
    run_system_test();

    // 2. Ejecutar Programa desde Archivo TXT si se especifica o existe uno por defecto
    const char *filename = (argc > 1) ? argv[1] : "program.txt";
    
    MIPS_State sim;
    mips_init(&sim);

    if (load_program_from_binary_txt(&sim, filename)) {
        printf("--- Iniciando Simulación de Archivo '%s' ---\n", filename);
        
        // Simular 20 ciclos de clock
        for (int cycle = 0; cycle < 20; cycle++) {
            stage_writeback(&sim);
            stage_memory(&sim);
            stage_execute(&sim);
            stage_decode(&sim);
            stage_fetch(&sim);
        }

        printf("\n--- Estado Final de Registros Relevantes ---\n");
        for (int i = 0; i < 16; i++) {
            printf("Reg $%d: %d (0x%08X)\n", i, sim.registers[i], sim.registers[i]);
        }
    }

    return 0;
}