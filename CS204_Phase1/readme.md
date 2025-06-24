# RISC-V Assembler Project

This project implements a RISC-V assembler that converts assembly instructions into machine code. The assembler supports multiple instruction formats and outputs the results in a structured format. 

## Project Structure
- **main.cpp**: The main driver file that handles file I/O, calls the necessary functions, and writes the final output.
- **TEXT_SEGMENT.h**: Handles text segment parsing and machine code generation.
- **DATA_SEGMENT.h**: Handles data segment parsing and storage.
- **testForDataSegment.asm** & **test.asm**: Sample assembly files for testing.
- **output.mc**: The resulting machine code output file.

## Expected Output Format
The output is presented in the following format:
```
0x0 0x003100b3 , add x1 x2 x3 # 0110011-000-0000000-00001-00010-00011-NULL # x1 = x2 + x3
0x4 0x0062f233 , and x4 x5 x6 # 0110011-111-0000000-00100-00101-00110-NULL # x4 = x5 & x6
```
Each line includes:
- **Instruction address**
- **Hexadecimal machine code**
- **Original assembly instruction**
- **Detailed breakdown of instruction fields**
- **Comment explaining the instruction's operation**

## Instruction Formats
### R-Type Instructions
Format: `opcode | rd | funct3 | rs1 | rs2 | funct7`
Supported Instructions: `add`, `and`, `or`, `sll`, `slt`, `sra`, `srl`, `sub`, `xor`, `mul`, `div`, `rem`
- Example: `add x1, x2, x3` → `0x003100b3`

### I-Type Instructions
Format: `opcode | rd | funct3 | rs1 | immediate`
Supported Instructions: `addi`, `andi`, `ori`, `lb`, `lh`, `lw`, `ld`, `jalr`
- Example: `addi x6, x7, 64` → `0x06438313`

### S-Type Instructions
Format: `opcode | imm[11:5] | rs2 | rs1 | funct3 | imm[4:0]`
Supported Instructions: `sb`, `sh`, `sw`, `sd`
- Example: `sb x16, 8(x17)` → `0x016b8423`

### SB-Type Instructions (Branch)
Format: `opcode | imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11]`
Supported Instructions: `beq`, `bne`, `bge`, `blt`
- Example: `beq x1, x2, target1` → `0x00209a63`

### U-Type Instructions
Format: `opcode | rd | immediate[31:12]`
Supported Instructions: `auipc`, `lui`
- Example: `lui x7, 0x0` → `0x000003b7`

### UJ-Type Instructions (Jump)
Format: `opcode | rd | immediate[20|10:1|11|19:12]`
Supported Instructions: `jal`
- Example: `jal x9, 0x100` → `0x018004ef` (Note: Immediate is provided as the third operand)

### JALR Instruction
Format: `opcode | rd | funct3 | rs1 | immediate`
Supported Instructions: `jalr`
- Example: `jalr x14, x15, 28` → `0x028a8a67` (Note: The format is `rd rs immediate` NOT `rd immediate rs`)

## Instruction Syntax Rules
- **Commas (` , `)**: Used to separate operands in most instructions like `add`, `and`, etc.
- **Parentheses (`( )`)**: Used in load/store instructions like `lb x12, 8(x13)` to indicate base register + offset format.
- **Whitespace Only**: Instructions like `jal x9 0x100` or `jalr x14 x15 28` use whitespace instead of commas for the immediate value.
- **Combination of Formats**: Some instructions may mix these conventions. For example, `sb x16, 8(x17)` uses both commas and parentheses.

## Data Segment Format
The `.data` segment is stored in little-endian format with output similar to:
```
Address: 10000000 | Data: 0x12 0x2a 0x41 0xaa 
Address: 10000004 | Data: 0xcd 0xab 0x39 0x30 
```

## How to Run
1. Compile the project using g++:
   ```bash
   g++ main.cpp -o assembler
   ```
2. Run the assembler with input and output files specified:
   ```bash
   ./assembler test.asm output.mc
   ```

## Key Features
- Supports all major RISC-V instruction formats.
- Provides detailed comments explaining the binary format for each instruction.
- Includes proper error handling for invalid instructions, unsupported formats, and immediate value range checks.

## Future Improvements
- Add support for additional RISC-V instructions.
- Enhance the assembler to handle pseudo-instructions.
- Improve error messages for better debugging.

