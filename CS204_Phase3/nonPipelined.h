#ifndef NON_PIPELINE_H
#define NON_PIPELINE_H

// Include necessary headers but avoid redefining global variables
#include <iostream>
#include <fstream>
#include <bitset>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

using namespace std;

// Define all global variables with _np suffix to avoid conflicts
static unsigned int X_np[32];              // Registers
static unsigned int MEM_np[4000];          // Instruction memory
static int DMEM_np[1000000];               // Data memory
static int STACKMEM_np[1024];              // Stack memory
static unsigned int instruction_word_np;
static unsigned int operand1_np;
static unsigned int operand2_np;

// Global variables
static char Type_np = '0';
static bitset<32> inst_np;                // 32-bit instruction
static unsigned int des_reg_np;           // destination register
static int des_res_np;                    // result after ALU operation
static string subtype_np;                 // subtype of the instruction
static int imm_np;                        // immediate value
static unsigned int pc_np = 0;            // Program counter
static unsigned int sz_np = 0;            // Number of instructions
static unsigned int clockCycles_np = 0;   // Global clock variable

// Function declarations for non-pipelined mode
void reset_proc_np();
void load_program_memory_np( bool skipdata);
void run_riscvsim_np();
int run_step_np();
void fetch_np();
void decode_np();
void execute_np();
void mem_op_np();
void write_back_np();
void save_state_np();
bool load_state_np();

// Rest of the file implementation with _np suffix for all functions and variables...
// (Include all the function implementations from original nonPipelined.cpp)

#endif // NON_PIPELINE_H