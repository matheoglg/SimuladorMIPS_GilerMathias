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

```
### 2. Ejecutar Unit tests y system test
./mips_sim

### 3. Ejecutar un programa binario desde txt
./mips_sim program.txt


## Diagramas de Arquitectura y Pipeline

### 1. Flujo de Procesamiento y Unidades de Control
El siguiente diagrama describe el paso de datos a través de las 5 etapas del pipeline (`IF`, `ID`, `EX`, `MEM`, `WB`), registrando cómo la **Hazard Detection Unit** y la **Forwarding Unit** interceptan las señales.

```mermaid
graph TD
    %% Etapas del Pipeline
    subgraph IF [1. Instruction Fetch]
        PC[Program Counter] --> INST_MEM[Instruction Memory]
        INST_MEM --> IF_ID[Latch IF/ID]
    end

    subgraph ID [2. Instruction Decode]
        IF_ID --> REG_FILE[Register File]
        IF_ID --> HAZARD[Hazard Detection Unit]
        HAZARD -->|Stall Signal| PC
        HAZARD -->|Flush/NOP| ID_EX
        REG_FILE --> ID_EX[Latch ID/EX]
    end

    subgraph EX [3. Execute]
        ID_EX --> MUX_A[MUX Forward A]
        ID_EX --> MUX_B[MUX Forward B]
        FORWARD[Forwarding Unit] -->|Control A| MUX_A
        FORWARD -->|Control B| MUX_B
        MUX_A --> ALU[ALU Control & Unit]
        MUX_B --> ALU
        ALU --> EX_MEM[Latch EX/MEM]
    end

    subgraph MEM [4. Memory Access]
        EX_MEM --> DATA_MEM[Data Memory]
        EX_MEM -->|Adelanto EX/MEM| FORWARD
        DATA_MEM --> MEM_WB[Latch MEM/WB]
    end

    subgraph WB [5. Write Back]
        MEM_WB -->|Adelanto MEM/WB| FORWARD
        MEM_WB -->|Escritura de Datos| REG_FILE
    end

    %% Estilos de las cajas
    style HAZARD fill:#f9f,stroke:#333,stroke-width:2px
    style FORWARD fill:#bbf,stroke:#333,stroke-width:2px
```

---

### 2. Estructura de Registros Intermedios (`MIPS_State`)
Representación de las estructuras en C definidas para simular los latches interetapa del procesador:

```mermaid
classDiagram
    class MIPS_State {
        +uint32_t pc
        +uint32_t registers[32]
        +uint32_t inst_mem[MEM_SIZE]
        +uint32_t data_mem[MEM_SIZE]
        +bool stall
        +uint8_t forward_a
        +uint8_t forward_b
    }

    class IF_ID_Register {
        +uint32_t instruction
        +uint32_t pc
    }

    class ID_EX_Register {
        +uint8_t rs
        +uint8_t rt
        +uint8_t rd
        +uint32_t read_data_1
        +uint32_t read_data_2
        +uint32_t imm_extended
        +bool reg_dst
        +bool alu_src
        +bool mem_to_reg
        +bool reg_write
        +bool mem_read
        +bool mem_write
    }

    class EX_MEM_Register {
        +uint32_t alu_result
        +uint32_t write_data_mem
        +uint8_t write_reg
        +bool reg_write
        +bool mem_read
        +bool mem_write
    }

    class MEM_WB_Register {
        +uint32_t alu_result
        +uint32_t mem_read_data
        +uint8_t write_reg
        +bool reg_write
        +bool mem_to_reg
    }

    MIPS_State *-- IF_ID_Register
    MIPS_State *-- ID_EX_Register
    MIPS_State *-- EX_MEM_Register
    MIPS_State *-- MEM_WB_Register
```