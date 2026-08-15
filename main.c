#include "mips_sim.h"

int main(int argc, char *argv[]) {
    printf("========================================================\n");
    printf(" SIMULADOR MIPS 5 ETAPAS (PIPELINED IN C)\n");
    printf("========================================================\n\n");

    // 1. Correr pruebas
    run_unit_tests();
    run_system_test();

    // 2. Si se pasa un archivo por argumento, ejecutarlo
    if (argc > 1) {
        MIPS_State cpu;
        mips_init(&cpu);

        if (!load_program_from_binary_txt(&cpu, argv[1])) {
            return 1;
        }

        printf("Ejecutando simulación de '%s'...\n", argv[1]);

        // Ejecutar 30 ciclos para permitir que todo el pipeline termine
        for (int cycle = 0; cycle < 30; cycle++) {
            MIPS_State next = cpu; // Copia del estado actual

            stage_writeback(&cpu, &next);
            stage_memory(&cpu, &next);
            stage_execute(&cpu, &next);
            stage_decode(&cpu, &next);
            stage_fetch(&cpu, &next);

            cpu = next; // Actualización al final del ciclo de reloj
        }

        // 3. Imprimir Registros
        print_mips_state(&cpu);
    }

    return 0;
}