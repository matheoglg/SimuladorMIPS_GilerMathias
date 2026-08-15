#include "mips_sim.h"

int main(int argc, char *argv[]) {
    printf("========================================================\n");
    printf(" SIMULADOR MIPS 5 ETAPAS (PIPELINED IN C)\n");
    printf("========================================================\n\n");

    // Ejecución de la suite de validación (unidades de control y pipeline)
    run_unit_tests();
    run_system_test();

    // Procesamiento del programa del usuario si se proporciona la ruta del binario
    if (argc > 1) {
        MIPS_State cpu;
        mips_init(&cpu);

        // Carga de instrucciones en la memoria principal del simulador
        if (!load_program_from_binary_txt(&cpu, argv[1])) {
            return 1;
        }

        printf("Ejecutando simulación de '%s'...\n", argv[1]);

        /* 
         * Ciclo de reloj: 30 iteraciones aseguran el drenaje completo del pipeline (flush/drain).
         * Las etapas se ejecutan en orden inverso (WB -> IF) para simular el comportamiento 
         * concurrente del hardware sin generar condiciones de carrera en el estado del ciclo actual.
         */
        for (int cycle = 0; cycle < 30; cycle++) {
            MIPS_State next = cpu; // Doble buffer: acumula cambios para el siguiente flanco de reloj

            stage_writeback(&cpu, &next);
            stage_memory(&cpu, &next);
            stage_execute(&cpu, &next);
            stage_decode(&cpu, &next);
            stage_fetch(&cpu, &next);

            cpu = next; // Transición síncrona de estado al flanco de subida
        }

        // Volcado final del banco de registros y memoria de datos
        print_mips_state(&cpu);
    }

    return 0;
}