# Simulador MIPS de 5 Etapas en C

Simulador de un procesador MIPS con pipeline de 5 etapas (**IF, ID, EX, MEM, WB**), implementación de **Forwarding Unit** para resolución de Hazards de Datos y **Hazard Detection Unit** para puestos (stalls) por Load-Use.

---

## Requisitos
- Compilador de C (`gcc` recomendado).
- `make` (opcional).

---

## Compilación y Ejecución

### 1. Compilación manual con GCC
```bash
gcc -Wall -std=c99 main.c mips_sim.c -o mips_sim

### 2. Ejecutar Unit tests y system test
./mips_sim

### 3. Ejecutar un programa binario desde txt
./mips_sim program.txt