/* 
The project is developed as part of Computer Architecture class
Project Name: Functional Simulator for subset of RISCV Processor

Developer's Name:
Developer's Email id:
Date: 
*/

/* myRISCVSim.cpp
   Purpose: Simulator for a subset of the RISC-V processor.
   Modified to handle an input file with two segments:
     1. Instruction Segment:
         Format: 
         0x0 0x100000b7 , lui x1 0x10000 # <bit pattern comment>
         ...
     2. Data Segment:
         Format:
         Address: 10000000 | Data: 0x03 0x00 0x00 0x00 
         ...
   The loader pads instruction tokens to 8 hex digits.
   A termination instruction (0xffffffff) is implemented.
   A global variable "clockCycles_np" increments once per instruction cycle,
   and the simulator prints the cycle count at the end of each cycle.
   --- NEW: The "mul" instruction (R-type, func3="000", func7="0000001") is now supported.
   --- NEW: A separate stack memory region is added with downward growth.
         The top of the stack is defined by STACK_TOP (0x7FFFFFDC),
         and the stack region extends downward for STACK_SIZE words.
*/

#include <iostream>
#include <fstream>
#include <bitset>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
using namespace std;

#define M 32

// Define the stack region.
// STACK_TOP is the initial value of the stack pointer (x2).
const unsigned int STACK_TOP = 0x7FFFFFDC;
// Define the number of words in the stack.
const unsigned int STACK_SIZE = 1024;  // Adjust as needed.
// Compute the bottom address of the stack region.
const unsigned int STACK_BOTTOM = STACK_TOP - STACK_SIZE * 4;

void write_data_memory_np();

// Global registers, instruction memory, data memory, and new stack memory
static unsigned int X_np[32];       // 32 registers
static unsigned int MEM_np[4000];     // Instruction memory (indexed by byte address)
static int DMEM_np[1000000];          // Data memory
static int STACKMEM_np[STACK_SIZE];   // Separate stack memory (word-addressable)
static unsigned int instruction_word_np;
static unsigned int operand1_np;
static unsigned int operand2_np;

// Global variables
char Type_np = '0';
static bitset<M> inst_np;           // 32-bit instruction
static unsigned int des_reg_np;     // destination register
static int des_res_np;              // result after ALU operation
string subtype_np;                  // subtype of the instruction
static int imm_np;                  // immediate value
static int pc_np = 0;               // Program counter
unsigned int sz_np = 0;             // Number of instructions
unsigned int clockCycles_np = 0;    // Global clock variable

// --- Opcode Type Determination Functions ---
char op_R_type_np(bitset<7> op) {
    bitset<7> opr("0110011");
    return (op == opr) ? 'R' : '0';
}
char op_I_type_np(bitset<7> op) {
    bitset<7> opi1("0010011");
    bitset<7> opi2("1100111");
    bitset<7> opi3("0000011");
    return (op == opi1 || op == opi2 || op == opi3) ? 'I' : '0';
}
char op_J_type_np(bitset<7> op) {
    bitset<7> opj("1101111");
    return (op == opj) ? 'J' : '0';
}
char op_B_type_np(bitset<7> op) {
    bitset<7> opb("1100011");
    return (op == opb) ? 'B' : '0';
}
char op_S_type_np(bitset<7> op) {
    bitset<7> ops("0100011");
    return (op == ops) ? 'S' : '0';
}
char op_U_type_np(bitset<7> op) {
    bitset<7> opu1("0110111");
    bitset<7> opu2("0010111");
    return (op == opu1 || op == opu2) ? 'U' : '0';
}

// --- Utility: Two's Complement Calculation ---
string findTwoscomplement_np(string str) {
    int n = str.length();
    int i = 0;
    for (i = n - 1; i >= 0; i--)
        if (str[i] == '1')
            break;
    if (i == -1)
        return '1' + str;
    for (int k = i - 1; k >= 0; k--) {
        str[k] = (str[k] == '1') ? '0' : '1';
    }
    return str;
}

// --- Instruction Subtype Selection ---
string subtype_select_np(bitset<3> func3, bitset<7> func7, bitset<7> op) {
    string Func3 = func3.to_string(), Func7 = func7.to_string(), Op = op.to_string();
    switch (Type_np) {
        case 'R': {
            // Check for mul first.
            if (Func3 == "000" && Func7 == "0000001")
                subtype_np = "mul";
            else if (Func3 == "100" && Func7 == "0000001")
                subtype_np = "div";
            else if (Func3 == "110" && Func7 == "0000001")
                subtype_np = "rem";
            else if (Func3 == "000" && Func7 == "0000000")
                subtype_np = "add";
            else if (Func3 == "111" && Func7 == "0000000")
                subtype_np = "and";
            else if (Func3 == "110" && Func7 == "0000000")
                subtype_np = "or";
            else if (Func3 == "001" && Func7 == "0000000")
                subtype_np = "sll";
            else if (Func3 == "010" && Func7 == "0000000")
                subtype_np = "slt";
            else if (Func3 == "101" && Func7 == "0100000")
                subtype_np = "sra";
            else if (Func3 == "101" && Func7 == "0000000")
                subtype_np = "srl";
            else if (Func3 == "000" && Func7 == "0100000")
                subtype_np = "sub";
            else if (Func3 == "100" && Func7 == "0000000")
                subtype_np = "xor";
            break;
        }
        case 'I': {
            if (Func3 == "000" && Op == "0010011")
                subtype_np = "addi";
            else if (Func3 == "111")
                subtype_np = "andi";
            else if (Func3 == "110")
                subtype_np = "ori";
            else if (Func3 == "000" && Op == "0000011")
                subtype_np = "lb";
            else if (Func3 == "001" && Op == "0000011")
                subtype_np = "lh";
            else if (Func3 == "010" && Op == "0000011") 
                subtype_np = "lw";
            else if (Func3 == "011" && Op == "0000011")
                subtype_np = "ld";
            else if (Func3 == "000" && Op == "1100111")
                subtype_np = "jalr";
            else if (Func3 == "001" && Op == "0010011")
                subtype_np = "slli";
            break;
        }
        case 'B': {
            if (Func3 == "000")
                subtype_np = "beq";
            else if (Func3 == "001")
                subtype_np = "bne";
            else if (Func3 == "101")
                subtype_np = "bge";
            else if (Func3 == "100")
                subtype_np = "blt";
            break;
        }
        case 'J': {
            subtype_np = "jal";
            break;
        }
        case 'S': {
            if (Func3 == "000")
                subtype_np = "sb";
            else if (Func3 == "001")
                subtype_np = "sh";
            else if (Func3 == "010")
                subtype_np = "sw";
            else if (Func3 == "011")
                subtype_np = "sd";
            break;
        }
        case 'U': {
            if (Op == "0010111")
                subtype_np = "auipc";
            else if (Op == "0110111")
                subtype_np = "lui";
            break;
        }
        default:
            cout << "error" << endl;
    }
    return Func3;
}

// --- Memory and Register Dumping Functions ---
void load_resister_np() {
    FILE *fp = fopen("register.mem", "w");
    if (fp == NULL) {
        printf("Error opening register.mem for writing.");
        return;
    }
    for (unsigned int i = 0; i < 32; i++) {
        fprintf(fp, "x%u - %u\n", i, X_np[i]);
    }
    fclose(fp);
}

void load_Memory_np() {
    FILE *fp = fopen("D_Memory.mem", "w");
    if (fp == NULL) {
        printf("Error opening D_Memory.mem for writing.");
        return;
    }
    
    fprintf(fp, "=== DATA MEMORY CONTENTS ===\n");
    // Dump a portion of DMEM_np (e.g., first 50 words)
    for (unsigned int i = 0; i < 50; i++) {
        unsigned int addr = 0x10000000 + i * 4;
        fprintf(fp, "Addr 0x%08x: 0x%08x\n", addr, DMEM_np[i]);
    }
    fclose(fp);
    
    // Also print to console for immediate visibility
    cout << "DATA MEMORY DUMP (first 50 locations):" << endl;
    for (int i = 0; i < 50; i++) {
        cout << "DMEM_np[" << i << "] (addr 0x" << hex << (0x10000000 + i * 4)
             << "): 0x" << DMEM_np[i] << dec << endl;
    }
    
    // Dump Stack Memory
    fp = fopen("stack_mem.mem", "w");
    if (fp == NULL) {
        printf("Error opening stack_mem.mem for writing.");
        return;
    }
    fprintf(fp, "=== STACK MEMORY CONTENTS ===\n");
    for (unsigned int i = 0; i < STACK_SIZE; i++) {
        // Compute the effective address for each word in the stack.
        unsigned int addr = STACK_TOP - i * 4;
        fprintf(fp, "Addr 0x%08x: 0x%08x\n", addr, STACKMEM_np[i]);
    }
    fclose(fp);
    
    cout << "\nSTACK MEMORY DUMP (first 50 locations):" << endl;
    for (int i = 0; i < 50 && i < (int)STACK_SIZE; i++) {
        cout << "STACKMEM_np[" << i << "] (addr 0x" << hex << (STACK_TOP - i * 4)
             << "): 0x" << STACKMEM_np[i] << dec << endl;
    }
}

// Exit the simulator (writing out memories first)
void swi_exit_np() {
    cout << "Terminating simulation after " << clockCycles_np << " clock cycles." << endl;
    load_resister_np();
    load_Memory_np();
    // Remove persistent state file so that next run resets the state.
    if (remove("sim_state.dat") != 0) {
        perror("Error deleting state file");
    } else {
        cout << "State reset (sim_state.dat removed)." << endl;
    }
    exit(0);
}

// --- Processor Initialization ---
void reset_proc_np() {
    for (int i = 0; i < 32; i++)
        X_np[i] = 0;
    // Initialize the stack pointer (x2) to the top of the stack.
    X_np[2] = STACK_TOP;  
    for (int i = 0; i < 4000; i++)
        MEM_np[i] = 0;
    for (int i = 0; i < 1000000; i++)
        DMEM_np[i] = 0;
    // Initialize STACKMEM_np to 0.
    for (int i = 0; i < STACK_SIZE; i++)
        STACKMEM_np[i] = 0;
}

// --- Read and Write Word Helpers ---
int read_word_np(unsigned int *mem, unsigned int address) {
    int *data = (int *)(mem + address);
    return *data;
}
void write_word_np(unsigned int *mem, unsigned int address, unsigned int data) {
    int *data_p = (int *)(mem + address);
    *data_p = data;
    sz_np++;  // count the number of instructions
}

// Write the data memory to file (if needed)
void write_data_memory_np() {
    FILE *fp = fopen("data_out.mc", "w");
    if (fp == NULL) {
        printf("Error opening data_out.mc for writing.");
        return;
    }
    for (unsigned int i = 0; i < 1000000; i += 4) {
        fprintf(fp, "Address: %08x Data: %08x\n", 0x10000000 + i, DMEM_np[i]);
    }
    fclose(fp);
}

// --- Modified Loader: Handles Both Instruction and Data Segments ---
// Reads from "optimizedbbs.mc". For instruction lines, expects:
//   "0x<addr> 0x<instruction> , <assembly> # <comment>"
// The loader pads the instruction field to 8 hex digits.
// Modify load_program_memory_np to support assembler directives
void load_program_memory_np(bool skipdata) {
    FILE *fp = fopen("factorial.mc", "r");
    if (fp == NULL) {
        printf("Error opening input file optimizedbbs.mc\n");
        exit(1);
    }
    
    char line[1024];
    bool in_text_segment = true;  // By default, we start in the text segment
    unsigned int current_addr = 0;
    unsigned int data_addr = 0x10000000;
    
    // Process each line
    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines
        if (strlen(line) < 2) continue;
        
        // Handle directives
        if (line[0] == '.') {
            // Handle .text directive
            if (strncmp(line, ".text", 5) == 0) {
                in_text_segment = true;
                continue;
            }
            // Handle .data directive
            else if (strncmp(line, ".data", 5) == 0) {
                in_text_segment = false;
                continue;
            }
            if (!in_text_segment && skipdata) {
                continue;  // Skip data segment if requested
            }
            // Handle data directives in data segment
            if (!in_text_segment && !skipdata) {
                if (strncmp(line, ".byte", 5) == 0) {
                    // Parse byte value
                    int value;
                    if (sscanf(line + 5, "%i", &value) == 1) {
                        unsigned int index = (data_addr - 0x10000000) / 4;
                        int byte_pos = data_addr % 4;
                        unsigned int mask = ~(0xFF << (byte_pos * 8));
                        DMEM_np[index] = (DMEM_np[index] & mask) | ((value & 0xFF) << (byte_pos * 8));
                        data_addr += 1;
                    }
                }
                else if (strncmp(line, ".half", 5) == 0) {
                    // Parse half-word value
                    int value;
                    if (sscanf(line + 5, "%i", &value) == 1) {
                        // Ensure address is aligned
                        data_addr = (data_addr + 1) & ~1;
                        unsigned int index = (data_addr - 0x10000000) / 4;
                        int half_pos = (data_addr % 4) / 2;
                        unsigned int mask = ~(0xFFFF << (half_pos * 16));
                        DMEM_np[index] = (DMEM_np[index] & mask) | ((value & 0xFFFF) << (half_pos * 16));
                        data_addr += 2;
                    }
                }
                else if (strncmp(line, ".word", 5) == 0) {
                    // Parse word value
                    int value;
                    if (sscanf(line + 5, "%i", &value) == 1) {
                        // Ensure address is aligned
                        data_addr = (data_addr + 3) & ~3;
                        unsigned int index = (data_addr - 0x10000000) / 4;
                        DMEM_np[index] = value;
                        data_addr += 4;
                    }
                }
                else if (strncmp(line, ".dword", 6) == 0) {
                    // Parse double word value (64-bit)
                    long long value;
                    if (sscanf(line + 6, "%lli", &value) == 1) {
                        // Ensure address is aligned
                        data_addr = (data_addr + 7) & ~7;
                        unsigned int index = (data_addr - 0x10000000) / 4;
                        DMEM_np[index] = value & 0xFFFFFFFF;
                        DMEM_np[index + 1] = (value >> 32) & 0xFFFFFFFF;
                        data_addr += 8;
                    }
                }
                else if (strncmp(line, ".asciz", 6) == 0) {
                    // Parse null-terminated string
                    char str[256];
                    char* p = line + 6;
                    while (*p && (*p == ' ' || *p == '\t')) p++; // Skip whitespace
                    
                    if (*p == '"') {
                        p++; // Skip opening quote
                        int i = 0;
                        while (*p && *p != '"' && *p != '\n') {
                            str[i++] = *p++;
                        }
                        str[i] = '\0'; // Null terminate
                        
                        // Store string including null terminator
                        for (i = 0; str[i] != '\0'; i++) {
                            unsigned int index = (data_addr - 0x10000000) / 4;
                            int byte_pos = data_addr % 4;
                            unsigned int mask = ~(0xFF << (byte_pos * 8));
                            DMEM_np[index] = (DMEM_np[index] & mask) | ((str[i] & 0xFF) << (byte_pos * 8));
                            data_addr++;
                        }
                        // Add null terminator
                        unsigned int index = (data_addr - 0x10000000) / 4;
                        int byte_pos = data_addr % 4;
                        unsigned int mask = ~(0xFF << (byte_pos * 8));
                        DMEM_np[index] = (DMEM_np[index] & mask);
                        data_addr++;
                    }
                }
                continue;
            }
        }
        
        // Check if we reached a data segment marker
        if (strstr(line, ";; DATA SEGMENT") != NULL) {
            in_text_segment = false;
            continue;
        }
        
        // Process instruction or data line depending on current segment
        if (in_text_segment) {
            // Process instruction
            char addrStr[64], instStr[64];
            if (sscanf(line, "%s %s", addrStr, instStr) == 2) {
                // Remove trailing comma if present
                int len = strlen(instStr);
                if (instStr[len - 1] == ',') {
                    instStr[len - 1] = '\0';
                }
                
                // Process hex instruction
                if (strncmp(instStr, "0x", 2) == 0) {
                    char hexDigits[64];
                    strcpy(hexDigits, instStr + 2);
                    int dlen = strlen(hexDigits);
                    char padded[9];
                    memset(padded, '0', 8);
                    padded[8] = '\0';
                    strcpy(padded + (8 - dlen), hexDigits);
                    unsigned int instruction = strtoul(padded, NULL, 16);
                    unsigned int address = strtoul(addrStr, NULL, 16);
                    write_word_np(MEM_np, address, instruction);
                }
            }
        } else if (!skipdata) {
            // Process data section format: "Address: XXXX | Data: XX XX XX XX"
            if (strncmp(line, "Address:", 8) == 0) {
                unsigned int d_address, b0, b1, b2, b3;
                if (sscanf(line, "Address: %x | Data: %x %x %x %x", &d_address, &b0, &b1, &b2, &b3) == 5) {
                    unsigned int data = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
                    unsigned int index = (d_address - 0x10000000) / 4;
                    DMEM_np[index] = data;
                    cout << "Loaded data at address 0x" << hex << d_address 
                         << " (index " << dec << index << "): 0x" 
                         << hex << data << dec << endl;
                }
            }
        }
    }
    
    fclose(fp);
    if (!skipdata){
    pc_np = 0;
    }
}

// --- Simulation Stages ---
void fetch_np() {
    inst_np = MEM_np[pc_np];
    cout << "fetch_np instruction: 0x" << setw(8) << setfill('0') << hex << inst_np.to_ulong()
         << " from address 0x" << setw(8) << setfill('0') << hex << pc_np << endl;
    // Terminate if the instruction equals 0xffffffff.
    if (inst_np.to_ulong() == 0xffffffff)
        swi_exit_np();
}

void decode_np() {
    cout << "Decode:" << endl;
    bitset<7> op, func7;
    bitset<3> func3;
    bitset<5> rs1, rs2, rd;
    
    for (int i = 0; i < 7; i++)
        op[i] = inst_np[i];
    
    // Determine instruction type
    while (1) {
        if (Type_np == '0')
            Type_np = op_R_type_np(op);
        if (Type_np == '0')
            Type_np = op_I_type_np(op);
        if (Type_np == '0')
            Type_np = op_J_type_np(op);
        if (Type_np == '0')
            Type_np = op_B_type_np(op);
        if (Type_np == '0')
            Type_np = op_S_type_np(op);
        if (Type_np == '0')
            Type_np = op_U_type_np(op);
        break;
    }
    cout << "Format of instruction: " << Type_np << endl;
    
    int j = 0;
    for (int i = 25; i < 32; i++) {
        func7[j++] = inst_np[i];
    }
    j = 0;
    for (int i = 12; i < 15; i++) {
        func3[j++] = inst_np[i];
    }
    j = 0;
    for (int i = 15; i < 20; i++) {
        rs1[j++] = inst_np[i];
    }
    operand1_np = rs1.to_ulong();
    j = 0;
    for (int i = 20; i < 25; i++) {
        rs2[j++] = inst_np[i];
    }
    operand2_np = rs2.to_ulong();
    
    switch (Type_np) {
        case 'R': {
            j = 0;
            for (int i = 7; i < 12; i++) {
                rd[j++] = inst_np[i];
            }
            des_reg_np = rd.to_ulong();
            cout << "Operand1: " << operand1_np << ", Operand2: " << operand2_np
                 << ", RD: " << des_reg_np << endl;
            break;
        }
        case 'I': {
            bitset<12> immb;
            j = 0;
            bool isneg = false;
            for (int i = 7; i < 12; i++) {
                rd[j++] = inst_np[i];
            }
            des_reg_np = rd.to_ulong();
            j = 0;
            for (int i = 20; i < 32; i++) {
                immb[j++] = inst_np[i];
            }
            cout << "DEBUG: I-type immediate bits: " << immb << endl;
            if (immb[11] == 1) {
                isneg = true;
                string s1 = immb.to_string();
                string s = findTwoscomplement_np(s1);
                bitset<12> opl(s);
                immb = opl;
            }
            imm_np = immb.to_ulong();
            if (isneg)
                imm_np = -1 * imm_np;
            cout << "DEBUG: Final immediate value: " << imm_np << " (0x" << hex << imm_np << dec << ")" << endl;
            cout << "Immediate: " << imm_np << ", Operand1: " << operand1_np
                 << ", RD: " << des_reg_np << endl;
            break;
        }
        case 'S': {
            bitset<12> immb;
            j = 0;
            for (int i = 7; i < 12; i++) {
                immb[j++] = inst_np[i];
            }
            for (int i = 25; i < 32; i++) {
                immb[j++] = inst_np[i];
            }
            imm_np = immb.to_ulong();
            if (immb[11] == 1)
                imm_np = -1 * imm_np;
            cout << "Immediate: " << imm_np << ", Operand1: " << operand1_np
                 << ", Operand2: " << operand2_np << endl;
            break;
        }
        case 'B': {
            bitset<13> immb;
            bool isneg = false;
            immb[0] = 0;
            j = 1;
            for (int i = 8; i < 12; i++) {
                immb[j++] = inst_np[i];
            }
            for (int i = 25; i < 31; i++) {
                immb[j++] = inst_np[i];
            }
            immb[j++] = inst_np[7];
            immb[j++] = inst_np[31];
            if (immb[12] == 1) {
                isneg = true;
                string s1 = immb.to_string();
                string s = findTwoscomplement_np(s1);
                bitset<13> opl(s);
                immb = opl;
            }
            imm_np = immb.to_ulong();
            if (isneg)
                imm_np = -1 * imm_np;
            cout << "Immediate: " << imm_np << ", Operand1: " << operand1_np
                 << ", Operand2: " << operand2_np << endl;
            break;
        }
        case 'U': {
            j = 0;
            for (int i = 7; i < 12; i++) {
                rd[j++] = inst_np[i];
            }
            des_reg_np = rd.to_ulong();
            bitset<20> immb;
            j = 0;
            for (int i = 12; i < 32; i++) {
                immb[j++] = inst_np[i];
            }
            imm_np = immb.to_ulong();
            cout << "Immediate: " << imm_np << ", RD: " << des_reg_np << endl;
            break;
        }
        case 'J': {
            j = 0;
            bitset<21> immb;
            bool isneg = false;
            immb[0] = 0;
            j = 12;
            for (int i = 12; i < 20; i++) {
                immb[j++] = inst_np[i];
            }
            immb[11] = inst_np[20];
            j = 1;
            for (int i = 21; i < 31; i++) {
                immb[j++] = inst_np[i];
            }
            immb[20] = inst_np[31];
            if (immb[20] == 1) {
                isneg = true;
                string s1 = immb.to_string();
                string s = findTwoscomplement_np(s1);
                bitset<21> opl(s);
                immb = opl;
            }
            imm_np = immb.to_ulong();
            if (isneg)
                imm_np = -1 * imm_np;
            j = 0;
            for (int i = 7; i < 12; i++) {
                rd[j++] = inst_np[i];
            }
            des_reg_np = rd.to_ulong();
            cout << "Immediate: " << imm_np << ", RD: " << des_reg_np << endl;
            break;
        }
        default:
            cout << "error" << endl;
    }
    subtype_select_np(func3, func7, op);
}

void execute_np() {
    cout << "Operation is " << subtype_np << endl;
    cout << "execute_np:" << endl;
    if (Type_np == 'R') {
        if (subtype_np == "add") {
            des_res_np = X_np[operand1_np] + X_np[operand2_np];
            cout << "Adding " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "mul") {
            des_res_np = X_np[operand1_np] * X_np[operand2_np];
            cout << "Multiplying " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "div") {
            if (X_np[operand2_np] == 0) {
                des_res_np = -1; // Handle division by zero
                cout << "Division by zero! Setting result to -1" << endl;
            } else {
                des_res_np = static_cast<int>(X_np[operand1_np]) / static_cast<int>(X_np[operand2_np]);
                cout << "Dividing " << operand1_np << " by " << operand2_np << endl;
            }
        }
        else if (subtype_np == "rem") {
            if (X_np[operand2_np] == 0) {
                des_res_np = X_np[operand1_np]; // Handle remainder by zero
                cout << "Remainder by zero! Setting result to the dividend" << endl;
            } else {
                des_res_np = static_cast<int>(X_np[operand1_np]) % static_cast<int>(X_np[operand2_np]);
                cout << "Remainder of " << operand1_np << " divided by " << operand2_np << endl;
            }
        }
        else if (subtype_np == "sub") {
            des_res_np = X_np[operand1_np] - X_np[operand2_np];
            cout << "Subtracting " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "and") {
            des_res_np = X_np[operand1_np] & X_np[operand2_np];
            cout << "Bitwise AND " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "or") {
            des_res_np = X_np[operand1_np] | X_np[operand2_np];
            cout << "Bitwise OR " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "sll") {
            des_res_np = X_np[operand1_np] << X_np[operand2_np];
            cout << "Shift Left " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "slt") {
            des_res_np = (X_np[operand1_np] < X_np[operand2_np]) ? 1 : 0;
            cout << "Set Less Than " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "sra") {
            des_res_np = X_np[operand1_np] >> X_np[operand2_np];
            cout << "Shift Right Arithmetic " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "srl") {
            des_res_np = X_np[operand1_np] >> X_np[operand2_np];
            cout << "Shift Right Logical " << operand1_np << " and " << operand2_np << endl;
        }
        else if (subtype_np == "xor") {
            des_res_np = X_np[operand1_np] ^ X_np[operand2_np];
            cout << "Bitwise XOR " << operand1_np << " and " << operand2_np << endl;
        }
        pc_np = pc_np + 4;
    }
    else if (Type_np == 'I') {
        if (subtype_np == "addi") {
            des_res_np = X_np[operand1_np] + imm_np;
            cout << "Adding " << operand1_np << " and " << imm_np << endl;
        }
        else if (subtype_np == "andi") {
            des_res_np = X_np[operand1_np] & imm_np;
            cout << "Bitwise AND " << operand1_np << " and " << imm_np << endl;
        }
        else if (subtype_np == "ori") {
            des_res_np = X_np[operand1_np] | imm_np;
            cout << "Bitwise OR " << operand1_np << " and " << imm_np << endl;
        }
        else if (subtype_np == "lb" || subtype_np == "lh" || subtype_np == "lw") {
            des_res_np = X_np[operand1_np] + imm_np;
            cout << "Calculating memory address: " << operand1_np << " + " << imm_np << endl;
        }
        else if (subtype_np == "jalr") {
            des_res_np = pc_np + 4;
            pc_np = X_np[operand1_np] + imm_np;
            cout << "jalr: new PC = " << X_np[operand1_np] + imm_np << endl;
        }
        else if (subtype_np == "slli") {
            des_res_np = X_np[operand1_np] << imm_np;
            cout << "Shift Left " << operand1_np << " by " << imm_np << endl;
        }
        if (subtype_np != "jalr")
            pc_np = pc_np + 4;
    }
    else if (Type_np == 'B') {
        if (subtype_np == "beq") {
            if (X_np[operand1_np] == X_np[operand2_np]) {
                pc_np += imm_np;
                cout << "Branch taken (beq): PC += " << imm_np << endl;
            } else {
                pc_np += 4;
                cout << "Branch not taken (beq): PC += 4" << endl;
            }
        }
        else if (subtype_np == "bne") {
            if (X_np[operand1_np] != X_np[operand2_np]) {
                pc_np += imm_np;
                cout << "Branch taken (bne): PC += " << imm_np << endl;
            } else {
                pc_np += 4;
                cout << "Branch not taken (bne): PC += 4" << endl;
            }
        }
        else if (subtype_np == "bge") {
            if (X_np[operand1_np] >= X_np[operand2_np]) {
                pc_np += imm_np;
                cout << "Branch taken (bge): PC += " << imm_np << endl;
            } else {
                pc_np += 4;
                cout << "Branch not taken (bge): PC += 4" << endl;
            }
        }
        else if (subtype_np == "blt") {
            if (X_np[operand1_np] < X_np[operand2_np]) {
                pc_np += imm_np;
                cout << "Branch taken (blt): PC += " << imm_np << endl;
            } else {
                pc_np += 4;
                cout << "Branch not taken (blt): PC += 4" << endl;
            }
        }
    }
    else if (Type_np == 'J') {
        des_res_np = pc_np + 4;
        pc_np += imm_np;
        cout << "Jump (jal): new PC = " << pc_np << " (immediate " << imm_np << ")" << endl;
    }
    else if (Type_np == 'S') {
        des_res_np = X_np[operand1_np] + imm_np;
        pc_np = pc_np + 4;
        cout << "Store: calculated memory address = " << des_res_np << endl;
    }
    else if (Type_np == 'U') {
        if (subtype_np == "auipc") {
            des_res_np = pc_np + (imm_np << 12);
            cout << "auipc: PC + (imm<<12) = " << des_res_np << endl;
        }
        else if (subtype_np == "lui") {
            des_res_np = imm_np << 12;
            cout << "lui: imm << 12 = " << des_res_np << endl;
        }
        pc_np = pc_np + 4;
    }
}

// Modify the mem_op_np function to handle ld and sd 
void mem_op_np() {
    cout << "Memory stage:" << endl;
    // For load instructions
    if (subtype_np == "lw" || subtype_np == "lh" || subtype_np == "lb" || subtype_np == "ld") {
        cout << "Loading from memory at effective address 0x" 
             << setw(8) << setfill('0') << hex << des_res_np << dec << endl;
        
        // Check if the effective address is in the stack region.
        if (des_res_np >= STACK_BOTTOM && des_res_np <= STACK_TOP) {
            // Compute index based on downward growth.
            unsigned int index = (STACK_TOP - des_res_np) / 4;
            unsigned int orig_des_res = des_res_np;
            
            if (subtype_np == "lb") {
                // Load byte (8 bits) and sign-extend
                int byte_offset = des_res_np % 4;
                unsigned char byte_val = (STACKMEM_np[index] >> (byte_offset * 8)) & 0xFF;
                des_res_np = (byte_val & 0x80) ? (byte_val | 0xFFFFFF00) : byte_val;
            } 
            else if (subtype_np == "lh") {
                // Load half-word (16 bits) and sign-extend
                int hw_offset = (des_res_np % 4) / 2;
                unsigned short hw_val = (STACKMEM_np[index] >> (hw_offset * 16)) & 0xFFFF;
                des_res_np = (hw_val & 0x8000) ? (hw_val | 0xFFFF0000) : hw_val;
            }
            else if (subtype_np == "lw") {
                // Load word (32 bits)
                des_res_np = STACKMEM_np[index];
            }
            else if (subtype_np == "ld") {
                // Load double word (64 bits) - in 32-bit implementation, use two words
                // For simplicity, just load the lower 32 bits
                des_res_np = STACKMEM_np[index];
                cout << "Note: ld only loading lower 32 bits in this 32-bit implementation" << endl;
            }
            
            cout << "  Address 0x" << hex << orig_des_res 
                 << " → STACKMEM_np[" << dec << index << "] = 0x" 
                 << hex << des_res_np << dec << endl;
        }
        // Otherwise, if address falls in data memory region.
        else if (des_res_np >= 0x10000000) {
            unsigned int index = (des_res_np - 0x10000000) / 4;
            unsigned int orig_des_res = des_res_np;
            
            if (subtype_np == "lb") {
                // Load byte (8 bits) and sign-extend
                int byte_offset = des_res_np % 4;
                unsigned char byte_val = (DMEM_np[index] >> (byte_offset * 8)) & 0xFF;
                des_res_np = (byte_val & 0x80) ? (byte_val | 0xFFFFFF00) : byte_val;
            } 
            else if (subtype_np == "lh") {
                // Load half-word (16 bits) and sign-extend
                int hw_offset = (des_res_np % 4) / 2;
                unsigned short hw_val = (DMEM_np[index] >> (hw_offset * 16)) & 0xFFFF;
                des_res_np = (hw_val & 0x8000) ? (hw_val | 0xFFFF0000) : hw_val;
            }
            else if (subtype_np == "lw") {
                // Load word (32 bits)
                des_res_np = DMEM_np[index];
            }
            else if (subtype_np == "ld") {
                // Load double word (64 bits) - in 32-bit implementation, use two words
                // For simplicity, just load the lower 32 bits
                des_res_np = DMEM_np[index];
                cout << "Note: ld only loading lower 32 bits in this 32-bit implementation" << endl;
            }
            
            cout << "  Address 0x" << hex << orig_des_res 
                 << " → DMEM_np[" << dec << index << "] = 0x" 
                 << hex << des_res_np << dec << endl;
        } else {
            cout << "Error: Address 0x" << hex << des_res_np << " out of bounds." << dec << endl;
            des_res_np = 0;
        }
    }
    // For store instructions
    else if (subtype_np == "sw" || subtype_np == "sh" || subtype_np == "sb" || subtype_np == "sd") {
        cout << "Storing to memory at effective address 0x" 
             << setw(8) << setfill('0') << hex << des_res_np << dec << endl;
        // Check if the effective address is in the stack region.
        if (des_res_np >= STACK_BOTTOM && des_res_np <= STACK_TOP) {
            unsigned int index = (STACK_TOP - des_res_np) / 4;
            unsigned int old_value = STACKMEM_np[index];
            
            if (subtype_np == "sb") {
                // Store byte (8 bits)
                int byte_offset = des_res_np % 4;
                unsigned int mask = ~(0xFF << (byte_offset * 8));
                STACKMEM_np[index] = (STACKMEM_np[index] & mask) | ((X_np[operand2_np] & 0xFF) << (byte_offset * 8));
            }
            else if (subtype_np == "sh") {
                // Store half-word (16 bits)
                int hw_offset = (des_res_np % 4) / 2;
                unsigned int mask = ~(0xFFFF << (hw_offset * 16));
                STACKMEM_np[index] = (STACKMEM_np[index] & mask) | ((X_np[operand2_np] & 0xFFFF) << (hw_offset * 16));
            }
            else if (subtype_np == "sw") {
                // Store word (32 bits)
                STACKMEM_np[index] = X_np[operand2_np];
            }
            else if (subtype_np == "sd") {
                // Store double word (64 bits) - in 32-bit implementation, only lower 32 bits
                STACKMEM_np[index] = X_np[operand2_np];
                cout << "Note: sd only storing lower 32 bits in this 32-bit implementation" << endl;
            }
            
            cout << "  Address 0x" << hex << des_res_np 
                 << " → STACKMEM_np[" << dec << index << "] = 0x" 
                 << hex << X_np[operand2_np] << " (was 0x" << old_value << ")" << dec << endl;
        }
        // Otherwise, if address is in data memory.
        else if (des_res_np >= 0x10000000) {
            unsigned int index = (des_res_np - 0x10000000) / 4;
            unsigned int old_value = DMEM_np[index];
            
            if (subtype_np == "sb") {
                // Store byte (8 bits)
                int byte_offset = des_res_np % 4;
                unsigned int mask = ~(0xFF << (byte_offset * 8));
                DMEM_np[index] = (DMEM_np[index] & mask) | ((X_np[operand2_np] & 0xFF) << (byte_offset * 8));
            }
            else if (subtype_np == "sh") {
                // Store half-word (16 bits)
                int hw_offset = (des_res_np % 4) / 2;
                unsigned int mask = ~(0xFFFF << (hw_offset * 16));
                DMEM_np[index] = (DMEM_np[index] & mask) | ((X_np[operand2_np] & 0xFFFF) << (hw_offset * 16));
            }
            else if (subtype_np == "sw") {
                // Store word (32 bits)
                DMEM_np[index] = X_np[operand2_np];
            }
            else if (subtype_np == "sd") {
                // Store double word (64 bits) - in 32-bit implementation, only lower 32 bits
                DMEM_np[index] = X_np[operand2_np];
                cout << "Note: sd only storing lower 32 bits in this 32-bit implementation" << endl;
            }
            
            cout << "  Address 0x" << hex << des_res_np 
                 << " → DMEM_np[" << dec << index << "] = 0x" 
                 << hex << X_np[operand2_np] << " (was 0x" << old_value << ")" << dec << endl;
        } else {
            cout << "Error: Address 0x" << hex << des_res_np << " out of bounds." << dec << endl;
        }
    }
    else {
        cout << "No memory operation performed." << endl;
    }
}

void write_back_np() {
    cout << "WriteBack stage:" << endl;
    if (Type_np != 'S' && Type_np != 'B') {
        X_np[des_reg_np] = des_res_np;
        cout << "Storing " << des_res_np << " into register " << des_reg_np << endl;
    } else {
        cout << "No WriteBack operation for this instruction." << endl;
    }
    X_np[0] = 0;  // Ensure x0 remains 0.
    Type_np = '0';
}
void save_state_np() {
    FILE *fp = fopen("sim_state.dat", "wb");
    if (fp == NULL) {
        perror("Error saving state");
        return;
    }
    // Save program counter and clockCycles_np
    fwrite(&pc_np, sizeof(pc_np), 1, fp);
    fwrite(&clockCycles_np, sizeof(clockCycles_np), 1, fp);
    // Save registers
    fwrite(X_np, sizeof(unsigned int), 32, fp);
    // Save DMEM_np (all 1,000,000 integers) – adjust if you only need part of DMEM_np
    fwrite(DMEM_np, sizeof(int), 1000000, fp);
    // Save STACKMEMs
    fwrite(STACKMEM_np, sizeof(int), STACK_SIZE, fp);
    fclose(fp);
    cout << "State saved to sim_state.dat" << endl;
}

// Function to load state from the binary file, if it exists.
bool load_state_np() {
    FILE *fp = fopen("sim_state.dat", "rb");
    if (fp == NULL) {
        // File does not exist – no state to load.
        return false;
    }
    // Load program counter and clockCycles_np
    fread(&pc_np, sizeof(pc_np), 1, fp);
    fread(&clockCycles_np, sizeof(clockCycles_np), 1, fp);
    // Load registers
    fread(X_np, sizeof(unsigned int), 32, fp);
    // Load DMEM_np
    fread(DMEM_np, sizeof(int), 1000000, fp);
    // Load STACKMEM_np
    fread(STACKMEM_np, sizeof(int), STACK_SIZE, fp);
    fclose(fp);
    cout << "State loaded from sim_state.dat" << endl;
    return true;
}


void run_riscvsim_np() {
    while (true) {
        cout << "-----------------------------------------------------" << endl;
        fetch_np();
        decode_np();
        execute_np();
        mem_op_np();
        write_back_np();
        clockCycles_np++;
        cout << "Clock cycles so far: " << clockCycles_np << endl;
    }
}

// Function to execute a single instruction step in non-pipelined mode
int run_step_np() {
    cout << "-----------------------------------------------------" << endl;
    cout << "Executing single instruction step (non-pipelined mode)" << endl;

    // Variables to hold saved state values - add state loading similar to nonPipelined_main
    unsigned int saved_pc = 0;
    bool stateLoaded = load_state_np();
    if (stateLoaded) {
        // Save the loaded pc
        saved_pc = pc_np;
        // Load instructions from file, since instructions are not saved in sim_state.dat
        load_program_memory_np(true);
        // Restore the saved pc so execution continues where it left off
        pc_np = saved_pc;
        cout << "Loaded saved state - continuing from PC: 0x" << hex << pc_np << dec << endl;
    } else {
        // First run or reset - initialize everything
        cout << "No saved state found - initializing new execution" << endl;
        reset_proc_np();
        load_program_memory_np(false);
    }

    // Check if we have finished execution (PC is beyond instruction memory)
    if (pc_np >= sz_np * 4) {
        cout << "Program execution complete. No more instructions to execute." << endl;
        load_resister_np();
        load_Memory_np();
        // Clean up state file on completion
        if (remove("sim_state.dat") != 0) {
            perror("Error deleting state file");
        } else {
            cout << "State reset (sim_state.dat removed)." << endl;
        }
        return 1; // Return 1 to indicate completion
    }

    // Execute one instruction cycle
    fetch_np();
    decode_np();
    execute_np();
    mem_op_np();
    write_back_np();
    clockCycles_np++;
    
    // Print current status
    cout << "Executed instruction at PC: 0x" << hex << setw(8) << setfill('0') << pc_np - 4 << dec << endl;
    cout << "Instruction word: 0x" << hex << setw(8) << setfill('0') << inst_np.to_ulong() << dec << endl;
    cout << "Instruction type: " << Type_np << ", subtype: " << subtype_np << endl;
    cout << "Clock cycles so far: " << clockCycles_np << endl;
    
    // Write out current state to files so the GUI can read them
    load_resister_np();
    load_Memory_np();
    
    // Save the state for next step
    save_state_np();
    
    // Check if the instruction was the termination instruction (0xffffffff)
    if (inst_np.to_ulong() == 0xffffffff) {
        cout << "Termination instruction encountered." << endl;
        // Clean up state file
        if (remove("sim_state.dat") != 0) {
            perror("Error deleting state file");
        } else {
            cout << "State reset (sim_state.dat removed)." << endl;
        }
        return 1; // Return 1 to indicate completion
    }
    
    cout << "Ready for next step." << endl;
    return 0; // Return 0 to continue execution
}

// Replace the main function with a separate standalone function for direct use
int nonPipelined_main(int argc, char *argv[]) {
    bool stepMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--step") == 0) {
            stepMode = true;
        }
    }

    // Variables to hold saved state values.
    unsigned int saved_pc = 0;
    bool stateLoaded = load_state_np();
    if (stateLoaded) {
        // Save the loaded pc
        saved_pc = pc_np;
        // Load instructions from file, since instructions are not saved in sim_state.dat.
        load_program_memory_np(true);
        // Restore the saved pc so execution continues where it left off.
        pc_np = saved_pc;
    } else {
        reset_proc_np();
        load_program_memory_np(false);
    }

    if (stepMode) {
        // execute_np one instruction cycle
        fetch_np();
        decode_np();
        execute_np();
        mem_op_np();
        write_back_np();
        clockCycles_np++;
        // Write out current state to files so the GUI can read them
        load_resister_np();
        load_Memory_np();
        // Save the state for next step
        save_state_np();
        return 0;
    } else {
        // Run in continuous mode
        run_riscvsim_np();
    }
    return 0;
}