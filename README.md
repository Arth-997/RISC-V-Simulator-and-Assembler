# RISC-V Assembler and Simulator Suite  

This project provides a complete toolchain for working with RISC-V assembly: an assembler that converts RISC-V assembly code to machine code, and a pipelined processor simulator with a modern GUI for visualizing and analyzing RISC-V program execution.

---

## Table of Contents
- [Project Overview](#project-overview)
- [Directory Structure](#directory-structure)
- [Features](#features)
- [Requirements](#requirements)
- [Setup Instructions](#setup-instructions)
- [Usage](#usage)
  - [Assembler](#assembler)
  - [Simulator (CLI/GUI)](#simulator-cligui)
- [Input/Output File Formats](#inputoutput-file-formats)
- [Example Files](#example-files)
- [Acknowledgments](#acknowledgments)

---

## Project Overview

- **Assembler (CS204_Phase1):**
  - Converts RISC-V assembly code into machine code (.mc format).
  - Supports all major RISC-V instruction formats (R, I, S, SB, U, UJ).
  - Handles both text and data segments, with output in a structured, annotated format.

- **Simulator (CS204_Phase3):**
  - Simulates the execution of RISC-V machine code in a pipelined processor architecture.
  - Provides a feature-rich GUI for step-by-step execution, pipeline visualization, and performance analysis.
  - Supports both pipelined and non-pipelined execution modes.

---

## Directory Structure

```
CS204_Phase1/
  main.cpp              # Assembler entry point
  TEXT_SEGMENT.h        # Text segment parsing and code generation
  DATA_SEGMENT.h        # Data segment parsing and storage
  test.asm, testForDataSegment.asm  # Example assembly files
  output.mc             # Example output machine code
  readme.md             # (Legacy) Assembler documentation

CS204_Phase3/
  GUI.py                # PyQt5 GUI for the simulator
  trueOrignal.cpp       # Main pipelined simulator logic
  nonPipelined.cpp      # Non-pipelined simulator logic
  nonPipelined.h        # Non-pipelined simulator header
  Makefile.unknown      # Makefile for building the simulator
  *.mc                  # Example machine code files (bubblesort.mc, fib.mc, factorial.mc)
  README.md             # (Legacy) Simulator documentation
```

---

## Features

### Assembler
- Supports RISC-V instruction formats: R, I, S, SB (branch), U, UJ (jump).
- Parses `.text` and `.data` segments, outputting machine code and data in little-endian format.
- Provides detailed output with instruction breakdown and comments.
- Error handling for invalid instructions, unsupported formats, and immediate value range checks.

### Simulator & GUI
- **Pipelined Execution Visualization:** Real-time view of IF, ID, EX, MEM, WB stages.
- **Step-by-step Execution:** Cycle-by-cycle simulation and inspection.
- **Data Forwarding & Hazard Visualization:** Toggle and observe pipeline hazards, stalls, and bubbles.
- **Memory & Register Inspection:** View register and memory contents at any cycle.
- **Branch Prediction Analysis:** Track and analyze branch predictor performance.
- **Performance Statistics:** CPI, instruction counts, stalls, and more.
- **Customizable Options:** Configure simulation parameters via GUI.
- **Snapshotting:** Save and review processor state at each cycle.

---

## Requirements

- **Assembler:**
  - C++ compiler (g++ recommended)

- **Simulator:**
  - C++ compiler (g++ recommended)
  - Python 3.6+
  - PyQt5 (`pip install PyQt5`)

---

## Setup Instructions

### 1. Clone the Repository
```bash
git clone <repository-url>
cd <repository-directory>
```

### 2. Build the Assembler (CS204_Phase1)
```bash
cd CS204_Phase1
g++ main.cpp -o assembler
```

### 3. Build the Simulator (CS204_Phase3)
- **With Makefile:**
  ```bash
  cd ../CS204_Phase3
  make -f Makefile.unknown
  # or manually:
  g++ -std=c++11 -Wall -Wextra -o risc_v_simulator trueOrignal.cpp nonPipelined.cpp
  ```

### 4. Install Python Dependencies (for GUI)
```bash
pip install PyQt5
```

---

## Usage

### Assembler
1. Prepare your RISC-V assembly file (e.g., `test.asm`).
2. Run the assembler:
   ```bash
   ./assembler test.asm output.mc
   ```
   - Input: Assembly file
   - Output: Machine code file (`output.mc`)

### Simulator (CLI/GUI)

#### Command-Line Simulator
- Run the simulator on a `.mc` file:
  ```bash
  ./risc_v_simulator --input bubblesort.mc
  # Additional flags:
  #   --no-pipeline         # Disable pipelining
  #   --no-forwarding       # Disable data forwarding
  #   --print-registers     # Print registers each cycle
  #   --print-pipeline      # Print pipeline registers
  #   --print-bp            # Print branch predictor info
  #   --save-snapshots      # Save cycle-by-cycle snapshots
  #   --trace <N|PC>        # Trace instruction by number or PC
  #   --step                # Enable step mode
  ```

#### GUI Simulator
1. Run the GUI:
   ```bash
   python3 GUI.py
   ```
2. Select a `.mc` file from the dropdown.
3. Configure simulation options (pipelining, forwarding, tracing, etc.).
4. Use the GUI to run, step, and inspect the simulation.

---

## Input/Output File Formats

### Assembly Input (for Assembler)
- Standard RISC-V assembly with `.text` and `.data` segments.
- Example:
  ```assembly
  .data
  arr: .word 10, 9, 8, 7
  .text
  addi x1, x0, 5
  lw x2, 0(x1)
  add x3, x1, x2
  ```

### Machine Code Input (for Simulator)
- `.mc` files: Each line contains instruction address, hex code, assembly, and comments.
- Example:
  ```
  0x0 0x003100b3 , add x1 x2 x3 # 0110011-000-0000000-00001-00010-00011-NULL # x1 = x2 + x3
  0x4 0x0062f233 , and x4 x5 x6 # 0110011-111-0000000-00100-00101-00110-NULL # x4 = x5 & x6
  ...
  Address: 10000000 | Data: 0x12 0x2a 0x41 0xaa
  ```

---

## Example Files
- `CS204_Phase1/test.asm`, `CS204_Phase1/testForDataSegment.asm`: Example assembly files.
- `CS204_Phase1/output.mc`, `CS204_Phase3/bubblesort.mc`, `fib.mc`, `factorial.mc`: Example machine code files.

---

## Acknowledgments
This project was developed as part of a Computer Architecture course to help students understand RISC-V assembly, pipelined processor architecture, and simulation techniques.

--- 
