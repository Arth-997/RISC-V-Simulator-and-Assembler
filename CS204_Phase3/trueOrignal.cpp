#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
using namespace std;

#include "nonPipelined.h"



#define M 32

//------------------------------------------------------
// Memory Configuration Constants
//------------------------------------------------------
const unsigned int STACK_TOP = 0x7FFFFFDC;
const unsigned int STACK_SIZE = 1024;  // in words
const unsigned int STACK_BOTTOM = STACK_TOP - STACK_SIZE * 4;
const unsigned int INSTRUCTION_MEMORY_SIZE = 4000; // in words
const unsigned int DATA_MEMORY_SIZE = 1000000; // in words
const unsigned int STACK_MEMORY_SIZE = 1024; // same as STACK_SIZE
const unsigned int DATA_MEMORY_BASE = 0x10000000; // Base address of data memory

//------------------------------------------------------
// Global Register File and Memories
//------------------------------------------------------
static unsigned int X[32];       // 32 registers
static unsigned int MEM[INSTRUCTION_MEMORY_SIZE];     // Instruction memory (word-addressed)
static int DMEM[DATA_MEMORY_SIZE];          // Data memory
static int STACKMEM[STACK_MEMORY_SIZE];   // Stack memory
static unsigned int instruction_word;

//------------------------------------------------------
// Global Simulation Variables
//------------------------------------------------------
static bitset<M> inst;           // 32-bit instruction as bitset
static unsigned int des_reg;     // destination register
static int des_res;              // ALU result
string subtype;                  // Instruction subtype string
static int imm;                  // Immediate value
static int pc = 0;               // Program counter (byte-addressed)
unsigned int sz = 0;             // Number of instructions (set by the loader)
unsigned int clockCycles = 0;    // Clock cycle counter


// Add after the PipelineStatistics structure definition


struct InstructionTrace {
    int instructionNum;
    bool active;
    unsigned int pc;
    unsigned int instruction;
    
    // Cycle counters for each stage
    int fetchCycle;
    int decodeCycle;
    int executeCycle;
    int memoryCycle;
    int writebackCycle;
    
    // Results at each stage
    string decodeInfo;
    int executeResult;
    int memoryResult;
    int writebackResult;
    
InstructionTrace() : instructionNum(-1), active(false), pc(0), instruction(0),
                     fetchCycle(-1), decodeCycle(-1), executeCycle(-1), 
                     memoryCycle(-1), writebackCycle(-1), decodeInfo(""),
                     executeResult(0), memoryResult(0), writebackResult(0){}
};
// Global trace object
InstructionTrace currentTrace;

//------------------------------------------------------
// Global Control Signals Structure
//------------------------------------------------------
struct ControlSignals {
    bool regWrite;
    bool memRead;
    bool memWrite;
    bool memToReg;
    bool aluSrc;
    bool branch;
    bool jump;
    int aluOp;        // ALU operation code
};

//------------------------------------------------------
// Branch Predictor Structures
//------------------------------------------------------
const unsigned int BTB_SIZE = 16;
 
struct BTBEntry {
    bool valid;
    unsigned int branchPC;  // PC of the branch instruction
    unsigned int targetPC;  // Predicted target if taken
};
 
// Global BTB and one-bit PHT (PHT[i]==true means predict taken)
BTBEntry BTB[BTB_SIZE];
bool PHT[BTB_SIZE];
 
struct KnobSettings {
    bool printDataMemoryAtEnd = true; // Print DMEM at simulation end
    int dataPrintStart = 0;    
    int dataPrintCount = 10;
    
    bool pipeliningEnabled = true;    
    bool forwardingEnabled = true;    
    bool printRegisterEachCycle = false;    
    bool printPipelineRegisters = false;    
    bool printBranchPredictorInfo = false;    
    bool saveCycleSnapshots = false;    
    string inputFile = "";
    
    // Trace functionality settings
    bool traceInstructionEnabled = true;  // Knob5: Trace a specific instruction number
    int traceInstructionNum = -1;         // Instruction number to trace
    unsigned int traceInstructionPC = 0;  // PC address to trace
    bool traceByPC = false;              // Whether to trace by PC rather than sequence number
};

struct PipelineStatistics {
    unsigned int totalCycles = 0;           // Stat1: Total cycles
    unsigned int instructionsExecuted = 0;  // Stat2: Total instructions executed
    double CPI = 0.0;                       // Stat3: CPI
    unsigned int dataTransferInst = 0;      // Stat4: Number of load/store instructions executed
    unsigned int aluInst = 0;               // Stat5: Number of ALU instructions executed
    unsigned int controlInst = 0;           // Stat6: Number of control instructions executed
    unsigned int totalStalls = 0;           // Stat7: Total pipeline stalls inserted
    unsigned int dataHazardCount = 0;       // Stat8: Data hazards detected
    unsigned int controlHazardCount = 0;    // Stat9: Control hazards detected
    unsigned int branchMispredCount = 0;    // Stat10: Branch mispredictions
    unsigned int dataHazardStalls = 0;      // Stat11: Stalls due to data hazards
    unsigned int controlHazardStalls = 0;   // Stat12: Stalls due to control hazards
};

KnobSettings knobs;
PipelineStatistics stats;
unsigned int instructionCounter = 0; // Unique instruction sequence number

//------------------------------------------------------
// Pipeline Register Structures
//------------------------------------------------------
// IF/ID Pipeline Register
struct IF_ID_Register {
    bool valid;             // true if valid, false if bubble
    unsigned int pc;        // PC value of fetched instruction
    unsigned int instruction; // 32-bit fetched instruction
    unsigned int predictedPC; // Predicted next PC from branch predictor
};
 
// ID/EX Pipeline Register
struct ID_EX_Register {
    bool valid;
    unsigned int pc;
    char instType;          // 'R', 'I', 'S', 'B', 'U', or 'J'
    string subType;
    unsigned int rs1;       // Source register numbers
    unsigned int rs2;
    unsigned int rd;
    int rs1Value;           // Operand values (may be forwarded)
    int rs2Value;
    int immediate;
    ControlSignals control;
    unsigned int instructionWord;
    unsigned int instructionNum;  // Unique sequence number
};
 
// EX/MEM Pipeline Register
struct EX_MEM_Register {
    bool valid;
    unsigned int pc;
    char instType;
    string subType;
    unsigned int rd;
    int aluResult;
    int rs2Value;         // For store instructions
    unsigned int memAddress; // Computed memory address for loads/stores
    bool branchTaken;     // Outcome of branch computation
    ControlSignals control;
    unsigned int instructionWord;
    unsigned int instructionNum;
};
 
// MEM/WB Pipeline Register
struct MEM_WB_Register {
    bool valid;
    unsigned int pc;
    char instType;
    string subType;
    unsigned int rd;
    int aluResult;
    int memData;
    ControlSignals control;
    unsigned int instructionWord;
    unsigned int instructionNum;
};


// Add this after the other pipeline register structures
struct WB_Complete_Register {
    bool valid;
    unsigned int pc;
    char instType;
    string subType;
    unsigned int rd;
    int result;    // Final value written to register
    bool regWrite; // Whether this instruction wrote to a register
    unsigned int destReg; // Register written to
    unsigned int instructionNum;
};

// Global instance
WB_Complete_Register wb_complete = {false, 0, '0', "", 0, 0, false, 0, 0};

//------------------------------------------------------
// Pipeline Snapshot Structure for Cycle-by-Cycle Visualization
//------------------------------------------------------
struct PipelineSnapshot {
    IF_ID_Register if_id;
    ID_EX_Register id_ex;
    EX_MEM_Register ex_mem;
    MEM_WB_Register mem_wb;
    WB_Complete_Register wb_complete;  // Add this line
    unsigned int pc;
    unsigned int clockCycles;
    BTBEntry BTB_state[BTB_SIZE];
    bool PHT_state[BTB_SIZE];
};
 
// Container to store snapshots
vector<PipelineSnapshot> snapshots;
 
// Flag to enable snapshot saving
bool saveCycleSnapshots = false;
 
// Global Pipeline Registers
IF_ID_Register if_id = {false, 0, 0, 0};
ID_EX_Register id_ex = {false, 0, '0', "", 0, 0, 0, 0, 0, 0, {false, false, false, false, false, false, false, 0}, 0, 0};
EX_MEM_Register ex_mem = {false, 0, '0', "", 0, 0, 0, 0, false, {false, false, false, false, false, false, false, 0}, 0, 0};
MEM_WB_Register mem_wb = {false, 0, '0', "", 0, 0, 0, {false, false, false, false, false, false, false, 0}, 0, 0};
 
//------------------------------------------------------
// Pipeline Control Flags
//------------------------------------------------------
bool stall_fetch = false;
bool stall_decode = false;
bool flush_pipeline = false;
unsigned int nextPC = 0; // New PC after flush
 
//------------------------------------------------------
// Temporary Results Structure for In-Flight Values
//------------------------------------------------------
struct TempResults {
    bool exValid;
    unsigned int exRd;
    int exResult;
    bool exRegWrite;
    
    bool memValid;
    unsigned int memRd;
    int memResult;
    int memData;
    bool memRegWrite;
    bool memToReg;
 
    TempResults() : exValid(false), exRd(0), exResult(0), exRegWrite(false),
                   memValid(false), memRd(0), memResult(0), memData(0), memRegWrite(false), 
                   memToReg(false) {}
                   
    // Add a clear method to reset state
    void clear() {
        exValid = false;
        exRd = 0;
        exResult = 0;
        exRegWrite = false;
        
        memValid = false;
        memRd = 0;
        memResult = 0;
        memData = 0;
        memRegWrite = false;
        memToReg = false;
    }
};
 
TempResults tempResults;
 
//------------------------------------------------------
// Utility: Trim whitespace from both ends
//------------------------------------------------------
string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

// only two forwarding‐source stages
enum ForwardStage {
    EX_MEM,    // data sitting in EX/MEM
    MEM_WB     // data sitting in MEM/WB (or tempResults)
};


struct ForwardingInfo {
    int          value;
    ForwardStage stage;
};

struct ForwardingBuffer {
    ForwardingInfo saved[32];
    bool           valid[32];

    ForwardingBuffer() {
        for(int i = 0; i < 32; i++) valid[i] = false;
    }

    void saveValue(unsigned reg, int val, ForwardStage src) {
        if (reg == 0) return;
        saved[reg].value = val;
        saved[reg].stage = src;
        valid[reg] = true;
    }

    bool getValue(unsigned reg, int &outVal, ForwardStage &outStage) const {
        if (reg != 0 && valid[reg]) {
            outVal   = saved[reg].value;
            outStage = saved[reg].stage;
            return true;
        }
        return false;
    }
};

 
void saveForwardingData(ForwardingBuffer &fBuffer) {
    // MEM/WB source
    if (tempResults.memValid && tempResults.memRegWrite && tempResults.memRd != 0) {
        int v = tempResults.memToReg ? tempResults.memData : tempResults.memResult;
        fBuffer.saveValue(tempResults.memRd, v, MEM_WB);
    }
    // EX/MEM source
    if (ex_mem.valid && ex_mem.control.regWrite && ex_mem.rd != 0) {
        fBuffer.saveValue(ex_mem.rd, ex_mem.aluResult, EX_MEM);
    }
    // …you can skip the tempResults.exValid line since EX/MEM already covers it…
}

 
//------------------------------------------------------
// Output Detailed Pipeline Stage Information
//------------------------------------------------------
void outputPipelineStageDetails() {
    if (knobs.traceInstructionEnabled && !knobs.printPipelineRegisters) {
        return;
    }
    cout << "-------------------------------------" << endl;
    cout << "Cycle " << clockCycles << " Pipeline Details:" << endl;
   
    // IF stage
    if(if_id.valid) {
        cout << "IF: PC = 0x" << hex << if_id.pc
             << ", Instruction = 0x" << hex << if_id.instruction << endl;
    } else {
        cout << "IF: Bubble" << endl;
    }
   
    // ID stage
    if(id_ex.valid) {
        cout << "ID: PC = 0x" << hex << id_ex.pc
             << ", Instruction Type = " << id_ex.instType
             << ", Subtype = " << id_ex.subType
             << ", rs1 = x" << dec << id_ex.rs1
             << ", rs2 = x" << dec << id_ex.rs2
             << ", rd = x" << dec << id_ex.rd << endl;
    } else {
        cout << "ID: Bubble" << endl;
    }
   
    // EX stage
    if(ex_mem.valid) {
        cout << "EX: PC = 0x" << hex << ex_mem.pc
             << ", Instruction Type = " << ex_mem.instType
             << ", Subtype = " << ex_mem.subType
             << ", ALU Result = " << dec << ex_mem.aluResult << endl;
    } else {
        cout << "EX: Bubble" << endl;
    }
   
    // MEM stage
    if(mem_wb.valid) {
        cout << "MEM: PC = 0x" << hex << mem_wb.pc
             << ", Instruction Type = " << mem_wb.instType
             << ", Subtype = " << mem_wb.subType;
        if(mem_wb.control.memRead) {
            cout << ", Read Data = " << dec << mem_wb.memData;
        }
        cout << endl;
    } else {
        cout << "MEM: Bubble" << endl;
    }
   
    // WB stage
    if(mem_wb.valid && mem_wb.control.regWrite && mem_wb.rd != 0) {
        cout << "WB: PC = 0x" << hex << mem_wb.pc
             << ", Writing to x" << dec << mem_wb.rd
             << " = " << dec << (mem_wb.control.memToReg ? mem_wb.memData : mem_wb.aluResult) << endl;
    } else {
        cout << "WB: Bubble or no register write" << endl;
    }
}
 
//------------------------------------------------------
// NEW: Save a snapshot of the current pipeline registers and state
//------------------------------------------------------
void store_pipeline_snapshot() {
    PipelineSnapshot snap;
    snap.if_id = if_id;
    snap.id_ex = id_ex;
    snap.ex_mem = ex_mem;
    snap.mem_wb = mem_wb;
    snap.wb_complete = wb_complete;  // Add this line
    snap.pc = pc;
    snap.clockCycles = clockCycles;
    // Save branch predictor state too
    for (unsigned int i = 0; i < BTB_SIZE; i++) {
        snap.BTB_state[i] = BTB[i];
        snap.PHT_state[i] = PHT[i];
    }
    snapshots.push_back(snap);
}
 
//------------------------------------------------------
// NEW: Dump all pipeline snapshots to "cycle_snapshots.log"
//------------------------------------------------------
void dump_pipeline_snapshots() {
    ofstream snapFile("cycle_snapshots.log");
    if(!snapFile.is_open()){
        cerr << "Error: Could not open cycle_snapshots.log for writing." << endl;
        return;
    }
   
    for(const auto& snap : snapshots) {
        snapFile << "----------------------------------------------------" << endl;
        snapFile << "Cycle " << snap.clockCycles << " Pipeline State:" << endl;
       
        // IF stage
        if(snap.if_id.valid) {
            snapFile << "IF: PC = 0x" << hex << snap.if_id.pc
                     << ", Instruction = 0x" << hex << snap.if_id.instruction << dec << endl;
        } else {
            snapFile << "IF: Bubble" << endl;
        }
       
        // ID stage
        if(snap.id_ex.valid) {
            snapFile << "ID: PC = 0x" << hex << snap.id_ex.pc
                     << ", Type = " << snap.id_ex.instType
                     << ", Subtype = " << snap.id_ex.subType << dec
                     << ", rs1 = x" << snap.id_ex.rs1
                     << ", rs2 = x" << snap.id_ex.rs2
                     << ", rd = x" << snap.id_ex.rd << endl;
        } else {
            snapFile << "ID: Bubble" << endl;
        }
       
        // EX stage
        if(snap.ex_mem.valid) {
            snapFile << "EX: PC = 0x" << hex << snap.ex_mem.pc
                     << ", Type = " << snap.ex_mem.instType
                     << ", Subtype = " << snap.ex_mem.subType << dec
                     << ", ALU Result = " << snap.ex_mem.aluResult << endl;
        } else {
            snapFile << "EX: Bubble" << endl;
        }
       
        // MEM stage - Use mem_wb for MEM stage, not ex_mem
        if(snap.mem_wb.valid) {
            snapFile << "MEM: PC = 0x" << hex << snap.mem_wb.pc
                     << ", Type = " << snap.mem_wb.instType
                     << ", Subtype = " << snap.mem_wb.subType << dec;
            if(snap.mem_wb.control.memRead) {
                snapFile << ", Read Data = " << snap.mem_wb.memData;
            }
            snapFile << endl;
        } else {
            snapFile << "MEM: Bubble" << endl;
        }
       
        // WB stage - Now using wb_complete for the completed instruction
        if(snap.wb_complete.valid) {
            snapFile << "WB: PC = 0x" << hex << snap.wb_complete.pc
                     << ", Type = " << snap.wb_complete.instType
                     << ", Subtype = " << snap.wb_complete.subType << dec;
            if(snap.wb_complete.regWrite && snap.wb_complete.destReg != 0) {
                snapFile << ", Writing to x" << snap.wb_complete.destReg
                       << " = " << snap.wb_complete.result;
            }
            snapFile << endl;
        }

        // Completed instruction (post-writeback)
        if(snap.wb_complete.valid) {
            snapFile << "Completed: PC = 0x" << hex << snap.wb_complete.pc
                     << ", Type = " << snap.wb_complete.instType 
                     << ", Subtype = " << snap.wb_complete.subType << dec;
            if(snap.wb_complete.regWrite && snap.wb_complete.destReg != 0) {
                snapFile << ", Wrote x" << snap.wb_complete.destReg
                       << " = " << snap.wb_complete.result;
            }
            snapFile << endl;
        }
    }
    snapFile.close();
    cout << "Pipeline snapshots written to cycle_snapshots.log" << endl;
}
 
//------------------------------------------------------
// NEW: Save simulator state to file for step functionality
//------------------------------------------------------
void save_state() {
    ofstream outfile("sim_state.dat", ios::binary | ios::trunc); // Use trunc to overwrite
    if (!outfile) {
        cerr << "Error: Could not open file 'sim_state.dat' for saving state." << endl;
        return;
    }

    // Define a version marker for format tracking
    const unsigned int STATE_VERSION = 0x03000001; // Increment if format changes
    outfile.write(reinterpret_cast<const char*>(&STATE_VERSION), sizeof(STATE_VERSION));

    // Save core simulation state
    outfile.write(reinterpret_cast<const char*>(&pc), sizeof(pc));
    outfile.write(reinterpret_cast<const char*>(&clockCycles), sizeof(clockCycles));
    outfile.write(reinterpret_cast<const char*>(&instructionCounter), sizeof(instructionCounter));
    outfile.write(reinterpret_cast<const char*>(&sz), sizeof(sz));

    // Save registers
    outfile.write(reinterpret_cast<const char*>(X), sizeof(X));

    // Save all memory areas (use constants for size)
    outfile.write(reinterpret_cast<const char*>(MEM), sizeof(unsigned int) * INSTRUCTION_MEMORY_SIZE);
    outfile.write(reinterpret_cast<const char*>(DMEM), sizeof(int) * DATA_MEMORY_SIZE);
    outfile.write(reinterpret_cast<const char*>(STACKMEM), sizeof(int) * STACK_MEMORY_SIZE);

    // In save_state():
    // filepath: /home/arth/Desktop/FinalPipeline/trueOrignal.cpp
    // Save pipeline registers
    outfile.write(reinterpret_cast<const char*>(&if_id), sizeof(if_id));
    outfile.write(reinterpret_cast<const char*>(&id_ex), sizeof(id_ex));
    outfile.write(reinterpret_cast<const char*>(&ex_mem), sizeof(ex_mem));
    outfile.write(reinterpret_cast<const char*>(&mem_wb), sizeof(mem_wb));
    outfile.write(reinterpret_cast<const char*>(&wb_complete), sizeof(wb_complete)); // Add this line

    // Save branch predictor state
    outfile.write(reinterpret_cast<const char*>(BTB), sizeof(BTB));
    outfile.write(reinterpret_cast<const char*>(PHT), sizeof(PHT));

    // Save pipeline control flags
    outfile.write(reinterpret_cast<const char*>(&stall_fetch), sizeof(stall_fetch));
    outfile.write(reinterpret_cast<const char*>(&stall_decode), sizeof(stall_decode));
    outfile.write(reinterpret_cast<const char*>(&flush_pipeline), sizeof(flush_pipeline));
    outfile.write(reinterpret_cast<const char*>(&nextPC), sizeof(nextPC));

    // Save statistics
    outfile.write(reinterpret_cast<const char*>(&stats), sizeof(stats));

    // Save temporary results
    outfile.write(reinterpret_cast<const char*>(&tempResults), sizeof(tempResults));

    if (!outfile) {
        cerr << "Error: Failed to write complete state to sim_state.dat." << endl;
        outfile.close(); // Attempt to close even on error
        // Optionally: delete the potentially corrupt file
        // remove("sim_state.dat");
        return;
    }

    outfile.close();
    cout << "State saved to sim_state.dat (Version: " << hex << STATE_VERSION << dec << ")" << endl;

    // Save data memory to a separate text file for visualization (optional but useful)
    ofstream dmem_file("D_Memory.mem");
    if(dmem_file) {
        dmem_file << "=== DATA MEMORY CONTENTS ===" << endl;
        // Print a reasonable range, adjust as needed
        int print_count = min((int)DATA_MEMORY_SIZE, knobs.dataPrintCount);
        int print_start = min((int)DATA_MEMORY_SIZE - print_count, knobs.dataPrintStart);
        print_start = max(0, print_start); // Ensure start is not negative

        for(int i = 0; i < print_count; ++i) {
            unsigned int mem_index = print_start + i;
            if (mem_index >= DATA_MEMORY_SIZE) break; // Boundary check
            unsigned int addr = DATA_MEMORY_BASE + mem_index * 4;
            dmem_file << "Addr 0x" << hex << setw(8) << setfill('0') << addr
                      << ": 0x" << hex << setw(8) << setfill('0') << DMEM[mem_index]
                      << " (" << dec << DMEM[mem_index] << ")" << endl;
        }
        dmem_file.close();
    }

    // Save stack memory to a separate text file for visualization (optional but useful)
    ofstream stack_file("stack_mem.mem");
    if(stack_file) {
        stack_file << "=== STACK MEMORY CONTENTS (Top Down) ===" << endl;
        // Print top 100 words or STACK_SIZE, whichever is smaller
        int stack_print_count = min((int)STACK_MEMORY_SIZE, 100);
        for(int i = 0; i < stack_print_count; ++i) {
             if (i >= STACK_MEMORY_SIZE) break; // Boundary check
            unsigned int addr = STACK_TOP - i * 4;
            stack_file << "Addr 0x" << hex << setw(8) << setfill('0') << addr
                       << ": 0x" << hex << setw(8) << setfill('0') << STACKMEM[i]
                       << " (" << dec << STACKMEM[i] << ")" << endl;
        }
        stack_file.close();
    }

    // Save register contents to a text file for visualization (optional but useful)
    ofstream reg_file("register.mem");
    if(reg_file) {
        for(int i = 0; i < 32; i++) {
            reg_file << "x" << dec << i << " - 0x" << hex << X[i] << " (" << dec << X[i] << ")" << endl;
        }
        reg_file.close();
    }
}
 
//------------------------------------------------------
// NEW: Load simulator state from file for step functionality
//------------------------------------------------------
bool load_state() {
    ifstream infile("sim_state.dat", ios::binary);
    if (!infile) {
        // This is expected on the very first run, not necessarily an error yet.
        // cout << "Info: State file 'sim_state.dat' not found. Starting fresh." << endl;
        return false;
    }

    // Define the expected version marker
    const unsigned int EXPECTED_STATE_VERSION = 0x03000001;
    unsigned int file_version = 0;

    // Read and check version marker first
    infile.read(reinterpret_cast<char*>(&file_version), sizeof(file_version));
    if (!infile || file_version != EXPECTED_STATE_VERSION) {
        cerr << "Error: State file 'sim_state.dat' has unexpected version (Expected: 0x"
             << hex << EXPECTED_STATE_VERSION << ", Found: 0x" << file_version << ") or is corrupted." << dec << endl;
        infile.close();
        return false;
    }

    // Read core simulation state
    infile.read(reinterpret_cast<char*>(&pc), sizeof(pc));
    infile.read(reinterpret_cast<char*>(&clockCycles), sizeof(clockCycles));
    infile.read(reinterpret_cast<char*>(&instructionCounter), sizeof(instructionCounter));
    infile.read(reinterpret_cast<char*>(&sz), sizeof(sz));

    // Read registers
    infile.read(reinterpret_cast<char*>(X), sizeof(X));

    // Read all memory areas
    infile.read(reinterpret_cast<char*>(MEM), sizeof(unsigned int) * INSTRUCTION_MEMORY_SIZE);
    infile.read(reinterpret_cast<char*>(DMEM), sizeof(int) * DATA_MEMORY_SIZE);
    infile.read(reinterpret_cast<char*>(STACKMEM), sizeof(int) * STACK_MEMORY_SIZE);

    // Read pipeline registers
    // In load_state():
    // filepath: /home/arth/Desktop/FinalPipeline/trueOrignal.cpp
    // Read pipeline registers
    infile.read(reinterpret_cast<char*>(&if_id), sizeof(if_id));
    infile.read(reinterpret_cast<char*>(&id_ex), sizeof(id_ex));
    infile.read(reinterpret_cast<char*>(&ex_mem), sizeof(ex_mem));
    infile.read(reinterpret_cast<char*>(&mem_wb), sizeof(mem_wb));
    infile.read(reinterpret_cast<char*>(&wb_complete), sizeof(wb_complete)); // Add this line

    // Read branch predictor state
    infile.read(reinterpret_cast<char*>(BTB), sizeof(BTB));
    infile.read(reinterpret_cast<char*>(PHT), sizeof(PHT));

    // Read pipeline control flags
    infile.read(reinterpret_cast<char*>(&stall_fetch), sizeof(stall_fetch));
    infile.read(reinterpret_cast<char*>(&stall_decode), sizeof(stall_decode));
    infile.read(reinterpret_cast<char*>(&flush_pipeline), sizeof(flush_pipeline));
    infile.read(reinterpret_cast<char*>(&nextPC), sizeof(nextPC));

    // Read statistics
    infile.read(reinterpret_cast<char*>(&stats), sizeof(stats));

    // Read temporary results
    infile.read(reinterpret_cast<char*>(&tempResults), sizeof(tempResults));

    // Check for read errors or if we didn't reach EOF (unexpected extra data)
    infile.peek(); // Check EOF status
    if (!infile || !infile.eof()) {
         if (!infile && !infile.eof()) { // Genuine read error
             cerr << "Error: Failed reading state from 'sim_state.dat'." << endl;
         } else { // Reached EOF cleanly
             // This is the expected successful case
         }
         // Handle potential corruption if not EOF
         if (!infile.eof()) {
             cerr << "Error: State file 'sim_state.dat' contains unexpected extra data." << endl;
             infile.close();
             return false;
         }
    }


    infile.close();
    cout << "State loaded successfully from sim_state.dat (Version: " << hex << file_version << dec << ")" << endl;
    return true;
}
 
//------------------------------------------------------
// Output Data Hazard Information
//------------------------------------------------------
void outputDataHazardInfo(unsigned int src_reg, unsigned int dest_reg) {
    cout << "DATA HAZARD DETECTED: Between registers x" << src_reg << " and x" << dest_reg << endl;
    cout << "  Instruction at PC 0x" << hex << id_ex.pc << " needs data from PC 0x" << ex_mem.pc << endl;
}
 
void outputForwardingInfo(unsigned reg,
    ForwardStage from,
    const char* toStageStr,
    int val)
{
    const char* fromStr = (from == EX_MEM ? "EX/MEM" : "MEM/WB");
    cout << "FORWARDING: "
    << fromStr
    << "→ "
    << toStageStr
    << ", x" << reg
    << " = " << val
    << endl;
}
//------------------------------------------------------
// Output Control Hazard Information
//------------------------------------------------------
void outputControlHazardInfo(unsigned int branch_pc, bool predicted, bool actual) {
    cout << "CONTROL HAZARD: Branch at PC 0x" << hex << branch_pc << endl;
    cout << "  Predicted: " << (predicted ? "Taken" : "Not Taken")
         << ", Actual: " << (actual ? "Taken" : "Not Taken") << endl;
    cout << "  Branch Misprediction: Flushing pipeline" << endl;
}
 
//------------------------------------------------------
// Initialize Branch Predictor Tables
//------------------------------------------------------
void initializeBranchPredictor() {
    // Clear BTB entries
    for(unsigned int i = 0; i < BTB_SIZE; i++) {
        BTB[i].valid = false;
        BTB[i].branchPC = 0;
        BTB[i].targetPC = 0;
    }
   
    // Initialize Pattern History Table to predict not taken
    for(unsigned int i = 0; i < BTB_SIZE; i++) {
        PHT[i] = false;  // Initialize to predict "not taken"
    }
   
    cout << "Branch predictor initialized with " << BTB_SIZE << " entries" << endl;
}
 
//------------------------------------------------------
// Dump Register File to "register.mem"
//------------------------------------------------------
void dump_registers() {
    FILE *fp = fopen("register.mem", "w");
    if(fp == NULL) {
        perror("Error opening register.mem for writing");
        return;
    }
    for(unsigned int i = 0; i < 32; i++) {
        fprintf(fp, "x%u - %u\n", i, X[i]);
    }
    fclose(fp);
}
 
//------------------------------------------------------
// Dump Data Memory and Stack Memory to files
//------------------------------------------------------
void dump_memory() {
    // Dump DMEM
    FILE *fp = fopen("D_Memory.mem", "w");
    if(fp == NULL) {
        perror("Error opening D_Memory.mem for writing");
        return;
    }
    fprintf(fp, "=== DATA MEMORY CONTENTS ===\n");
    for(unsigned int i = 0; i < DATA_MEMORY_SIZE && i < 50; i++) {
        unsigned int addr = DATA_MEMORY_BASE + i * 4;
        fprintf(fp, "Addr 0x%08x: 0x%08x\n", addr, DMEM[i]);
    }
    fclose(fp);
 
    // Dump STACKMEM
    fp = fopen("stack_mem.mem", "w");
    if(fp == NULL) {
        perror("Error opening stack_mem.mem for writing");
        return;
    }
    fprintf(fp, "=== STACK MEMORY CONTENTS ===\n");
    for(unsigned int i = 0; i < STACK_SIZE && i < 50; i++) {
        unsigned int addr = STACK_TOP - i * 4;
        fprintf(fp, "Addr 0x%08x: 0x%08x\n", addr, STACKMEM[i]);
    }
    fclose(fp);
}
 
//------------------------------------------------------
// Dump Branch Predictor Info to "BP_info.txt"
//------------------------------------------------------
void dump_BP() {
    ofstream bpFile("BP_info.txt");
    if(!bpFile.is_open()) {
        cerr << "Error: Could not open BP_info.txt for writing." << endl;
        return;
    }
    bpFile << "Branch Predictor Status:\n";
    bpFile << "Index\tValid\tBranchPC\tTargetPC\tPrediction\n";
    for(unsigned int i = 0; i < BTB_SIZE; i++) {
        bpFile << i << "\t" << (BTB[i].valid ? "Yes" : "No")
               << "\t0x" << hex << BTB[i].branchPC
               << "\t0x" << hex << BTB[i].targetPC
               << "\t" << (PHT[i] ? "Taken" : "Not Taken") << "\n";
    }
    bpFile.close();
}
 
//------------------------------------------------------
// Print Data Memory Contents (for debugging)
//------------------------------------------------------
void printDataMemory(int startIndex, int count) {
    cout << "-------------------------------------" << endl;
    cout << "Data Memory Contents:" << endl;
    for (int i = startIndex; i < startIndex + count && i < DATA_MEMORY_SIZE; i++) {
        cout << "DMEM[" << dec << i << "] (Address 0x" << hex << (DATA_MEMORY_BASE + i * 4)
             << "): 0x" << hex << DMEM[i] << " (" << dec << DMEM[i] << ")" << endl;
    }
    cout << "-------------------------------------" << endl;
}
 
//------------------------------------------------------
// Loader: Parse input file containing text and data segments
//------------------------------------------------------
bool loadInputFile(const string &filename) {
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error: Could not open input file: " << filename << endl;
        return false;
    }
   
    string line;
    bool inDataSegment = false;
    unsigned int maxInstAddress = 0;
   
    while(getline(infile, line)) {
        line = trim(line);
        if(line.empty())
            continue;
        // Switch to data segment when marker is found.
        if(line.find(";; DATA SEGMENT") != string::npos) {
            inDataSegment = true;
            continue;
        }
        // Skip comment-only lines.
        if(line[0] == ';')
            continue;
       
        if(!inDataSegment) {
            // Process instruction line: Format "0xADDRESS 0xINSTRUCTION , <asm> # <comment>"
            stringstream ss(line);
            string addrToken, instToken;
            ss >> addrToken >> instToken;
            if(addrToken.empty() || instToken.empty())
                continue;
            if(instToken.back() == ',')
                instToken.pop_back();
            unsigned int address = stoul(addrToken, nullptr, 16);
            unsigned int instVal = stoul(instToken, nullptr, 16);
            unsigned int index = address / 4;
            if(index < INSTRUCTION_MEMORY_SIZE)
                MEM[index] = instVal;
            else {
                cerr << "Warning: Instruction address 0x" << hex << address << " exceeds MEM size." << endl;
            }
            if(address > maxInstAddress)
                maxInstAddress = address;
            // Continue processing even if termination marker encountered.
            if(instVal == 0xffffffff)
                continue;
        } else {
            // Process data segment line: Format "Address: ADDR | Data: 0x?? 0x?? 0x?? 0x??"
            if(strncmp(line.c_str(), "Address:", 8) == 0) {
                unsigned int d_address, b0, b1, b2, b3;
                if(sscanf(line.c_str(), "Address: %x | Data: %x %x %x %x", &d_address, &b0, &b1, &b2, &b3) == 5) {
                    unsigned int data = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
                    unsigned int index = (d_address - DATA_MEMORY_BASE) / 4;
                    cout << "Loaded data at address 0x" << hex << d_address
                         << " (DMEM index " << dec << index << "): 0x" << hex
                         << setw(8) << setfill('0') << data << dec << endl;
                    if(index < DATA_MEMORY_SIZE)
                        DMEM[index] = data;
                    else
                        cerr << "Warning: Data address 0x" << hex << d_address << " exceeds DMEM size." << dec << endl;
                } else {
                    cerr << "Warning: Failed to parse data line: " << line << endl;
                }
            }
        }
    }
    infile.close();
    sz = (maxInstAddress / 4) + 1;
    cout << "Loaded " << sz << " instructions from " << filename << endl;
    return true;
}
 
//------------------------------------------------------
// Hazard Detection Unit: Load-Use Hazard Check
//------------------------------------------------------
void hazardDetection() {
    stall_decode = false;
    stall_fetch = false;

    // --- Existing Load-Use Hazard Detection ---
    // This correctly stalls when forwarding is off for load-use cases.
    if(if_id.valid && id_ex.valid) {
        if(id_ex.control.memRead) {
            unsigned int instr = if_id.instruction;
            unsigned int rs1_field = (instr >> 15) & 0x1F;
            unsigned int rs2_field = (instr >> 20) & 0x1F;
           // if((id_ex.rd != 0) && ((id_ex.rd == rs1_field) || (id_ex.rd == rs2_field))) {
            if((id_ex.rd != 0) && ((id_ex.rd == rs1_field) || (id_ex.rd == rs2_field))) {
                    // if it's a store and we have forwarding, skip the stall
                    unsigned int nextOpcode = instr & 0x7F;
                    bool isStore = (nextOpcode == 0x23);
                    if(knobs.forwardingEnabled && isStore) {
                        // we'll forward MEM/WB→EX/MEM for stores, so no stall
                    } else {
                        stall_decode = stall_fetch = true;
                        stats.dataHazardCount++;
                        stats.dataHazardStalls++;
                        stats.totalStalls++;

                        if (knobs.printPipelineRegisters) {
                            cout << "STALL: Load-Use Hazard Detected (Forwarding "
                                 << (knobs.forwardingEnabled ? "Enabled - Store Special Case" : "Disabled")
                                 << ")" << endl;
                            outputDataHazardInfo(
                                id_ex.rd,
                                (id_ex.rd == rs1_field) ? rs1_field : rs2_field
                            );
                        }
                    }
            }
        }
    }

    // --- NEW: General RAW Hazard Detection (No Forwarding) ---
    // This checks for dependencies on instructions in ID/EX and EX/MEM
    // when forwarding is disabled.
    if (!knobs.forwardingEnabled && if_id.valid && !stall_decode) { // Only check if not already stalled
        unsigned int instr_in_if_id = if_id.instruction;
        unsigned int rs1_needed = (instr_in_if_id >> 15) & 0x1F;
        unsigned int rs2_needed = (instr_in_if_id >> 20) & 0x1F;
        bool needs_rs1 = (rs1_needed != 0);
        bool needs_rs2 = false; // Determine if rs2 is actually used by the opcode type

        // Rough check for instructions using rs2 (R, S, B types primarily)
        unsigned int opcode = instr_in_if_id & 0x7F;
        if (opcode == 0x33 || opcode == 0x23 || opcode == 0x63) {
             needs_rs2 = (rs2_needed != 0);
        }
        // Note: This rs2 check is simplified; a full decode would be more accurate,
        // but this covers the most common cases needing rs2.

        bool hazard_found = false;

        // Check dependency on instruction in ID/EX stage
        if (id_ex.valid && id_ex.control.regWrite && id_ex.rd != 0) {
            if ((needs_rs1 && id_ex.rd == rs1_needed) || (needs_rs2 && id_ex.rd == rs2_needed)) {
                hazard_found = true;
                 if (knobs.printPipelineRegisters) {
                     cout << "STALL: RAW Hazard Detected (No Forwarding): IF/ID needs x"
                          << (needs_rs1 && id_ex.rd == rs1_needed ? rs1_needed : rs2_needed)
                          << " from ID/EX (PC 0x" << hex << id_ex.pc << ")" << dec << endl;
                 }
            }
        }

        // Check dependency on instruction in EX/MEM stage (if no hazard with ID/EX found yet)
        if (!hazard_found && ex_mem.valid && ex_mem.control.regWrite && ex_mem.rd != 0) {
             if ((needs_rs1 && ex_mem.rd == rs1_needed) || (needs_rs2 && ex_mem.rd == rs2_needed)) {
                 hazard_found = true;
                 if (knobs.printPipelineRegisters) {
                     cout << "STALL: RAW Hazard Detected (No Forwarding): IF/ID needs x"
                          << (needs_rs1 && ex_mem.rd == rs1_needed ? rs1_needed : rs2_needed)
                          << " from EX/MEM (PC 0x" << hex << ex_mem.pc << ")" << dec << endl;
                 }
             }
        }

        // If any RAW hazard was found without forwarding, stall
        if (hazard_found) {
            stall_decode = true;
            stall_fetch = true; // Keep the current instruction in IF/ID
            stats.dataHazardCount++; // Count as data hazard
            stats.dataHazardStalls++;
            stats.totalStalls++;
        }
    }

    // Check for control hazards for branch/jump instructions in decode stage.
    if (id_ex.valid && (id_ex.instType == 'B' || id_ex.instType == 'J' ||
        (id_ex.instType == 'I' && id_ex.subType == "jalr"))) {
        // Additional early branch hazard handling can be added here if needed.
    }
}
 
//------------------------------------------------------
// Update the parseCommandLineArgs function
//------------------------------------------------------
void parseCommandLineArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if(arg == "--no-pipeline")
            knobs.pipeliningEnabled = false;
        else if(arg == "--no-forwarding")
            knobs.forwardingEnabled = false;
        else if(arg == "--print-registers")
            knobs.printRegisterEachCycle = true;
        else if(arg == "--print-pipeline")
            knobs.printPipelineRegisters = true;
        // Modify this part in parseCommandLineArgs

        else if(arg == "--trace") {
            if(i + 1 < argc) {
                knobs.printPipelineRegisters = false;
                knobs.traceInstructionEnabled = true;
                string trace_val = argv[++i];
                
                // Check if the input is a hex address (starts with 0x)
                if (trace_val.substr(0, 2) == "0x") {
                    // Convert hex PC to instruction number
                    unsigned int trace_pc = stoul(trace_val, nullptr, 16);
                    knobs.traceInstructionPC = trace_pc;
                    knobs.traceByPC = true;
                    cout << "Will trace instruction at PC 0x" << hex << trace_pc << dec << endl;
                } else {
                    // It's a standard instruction number
                    knobs.traceInstructionNum = stoi(trace_val);
                    cout << "Will trace instruction #" << knobs.traceInstructionNum << endl;
                }
            }
        }

        else if(arg == "--print-bp")
            knobs.printBranchPredictorInfo = true;
        else if(arg == "--input") {
            if(i + 1 < argc)
                knobs.inputFile = argv[++i];
        }
        else if(arg == "--print-memory") {
            knobs.printDataMemoryAtEnd = true;
            if(i + 2 < argc) {
                knobs.dataPrintStart = stoi(argv[++i]);
                knobs.dataPrintCount = stoi(argv[++i]);
            }
        }
        else if(arg == "--step") {
            // Flag for step mode.
        }
        else if(arg == "--save-snapshots") {
            knobs.saveCycleSnapshots = true;
        }
    }
}
 
//------------------------------------------------------
// Print Branch Predictor Status (BTB and PHT)
//------------------------------------------------------
void printBranchPredictor() {
    cout << "-------------------------------------" << endl;
    cout << "Cycle: " << clockCycles << endl;
    cout << "-------------------------------------" << endl;
    cout << "Branch Predictor Status:" << endl;
    cout << "Index\tValid\tBranchPC\tTargetPC\tPrediction" << endl;
    for (unsigned int i = 0; i < BTB_SIZE; i++) {
        cout << i << "\t"
             << (BTB[i].valid ? "Yes" : "No") << "\t0x" << hex << BTB[i].branchPC
             << "\t0x" << hex << BTB[i].targetPC << "\t"
             << (PHT[i] ? "Taken" : "Not Taken") << endl;
    }
}
 
//------------------------------------------------------
// Fetch Stage with Branch Prediction
//------------------------------------------------------
void fetch() {
    if(stall_fetch)
        return;
    if(flush_pipeline) {
        if_id.valid = false;
        return;
    }
    if(pc < sz * 4) { // 4 bytes per instruction
        unsigned int index = (pc / 4) % BTB_SIZE;
        unsigned int predicted = pc + 4; // default sequential prediction
        if(BTB[index].valid && BTB[index].branchPC == pc) {
            if(PHT[index])
                predicted = BTB[index].targetPC;
            else
                predicted = pc + 4;
        }
 
        instruction_word = MEM[pc / 4];
        if_id.valid = true;
        if_id.pc = pc;
        if_id.instruction = instruction_word;
        if_id.predictedPC = predicted;
 
        pc = predicted;
        nextPC = pc;
 
        if(knobs.printPipelineRegisters) {
            cout << "Fetch: Fetched 0x" << hex << instruction_word
                 << " from address 0x" << hex << if_id.pc
                 << ", predicted next PC: 0x" << hex << predicted << endl;
        }
    } else {
        if_id.valid = false;
        if(knobs.printPipelineRegisters)
            cout << "Fetch: No instruction to fetch." << endl;
    }
    // Add at the end of the fetch() function, just before the closing brace

    // Track instruction if tracing is enabled
    if ((knobs.traceInstructionEnabled && instructionCounter == knobs.traceInstructionNum) ||
        (knobs.traceInstructionEnabled && knobs.traceByPC && if_id.pc == knobs.traceInstructionPC)) {
        currentTrace.active = true;
        currentTrace.instructionNum = instructionCounter;
        currentTrace.pc = if_id.pc;
        currentTrace.instruction = instruction_word;
        currentTrace.fetchCycle = clockCycles + 1; // +1 because we increment later
        
        cout << "\n--- TRACE: Instruction #" << dec<<instructionCounter
        << " (0x" << hex << instruction_word << ") ---" << endl;
        cout << "FETCH at cycle " << dec << clockCycles + 1 << endl;
        cout<< "Contents of F/Dec buffer are: " << endl;
        cout << "  PC: 0x" << hex << if_id.pc << endl;
        cout << "  Instruction: 0x" << hex << instruction_word << endl;
        cout << "  Predicted next PC: 0x" << hex << if_id.predictedPC << endl;
        // check if it is control instruction or not
        unsigned int opcode = instruction_word & 0x7F;
        if (opcode == 0x63 || opcode == 0x6F) {
            cout << "  Control instruction detected." << endl;
        } else {
            cout << "  Not a control instruction." << endl;
        }
        // BTB hit or not
        unsigned int index = (pc / 4) % BTB_SIZE; // Calculate BTB index
        if (BTB[index].valid && BTB[index].branchPC == pc) {
            cout << "  BTB hit." << endl;
        } else {
            cout << "  BTB miss." << endl;
        }
        // BP
        if (PHT[index]) {
            cout << "  Prediction: Taken." << endl;
        } else {
            cout << "  Prediction: Not Taken." << endl;
        }
        cout << "-------------------------------------" << endl;
    }
}
 
//------------------------------------------------------
// Decode Stage with Two-Pass Data Forwarding
//------------------------------------------------------
void decode() {
    if(stall_decode || !if_id.valid) {
        id_ex.valid = false;
        return;
    }
    if (if_id.valid){
        id_ex.instructionNum=instructionCounter++; 
    }
    unsigned int instruction = if_id.instruction;
    inst = bitset<M>(instruction);
    id_ex.valid = true;
    id_ex.pc = if_id.pc;
    id_ex.instructionWord = instruction;
    id_ex.instructionNum = instructionCounter++;
    ControlSignals control = {false, false, false, false, false, false, false, 0};
    unsigned int opcode = instruction & 0x7F;
    // Decode instruction based on opcode...
    if(opcode == 0x33) { // R-type
        id_ex.instType = 'R';
        id_ex.rs1 = (instruction >> 15) & 0x1F;
        id_ex.rs2 = (instruction >> 20) & 0x1F;
        id_ex.rd = (instruction >> 7) & 0x1F;
        string funct3 = bitset<3>((instruction >> 12) & 0x7).to_string();
        string funct7 = bitset<7>((instruction >> 25) & 0x7F).to_string();
        if(funct3 == "000") {
            if(funct7 == "0000000")
                id_ex.subType = "add";
            else if(funct7 == "0100000")
                id_ex.subType = "sub";
            else if(funct7 == "0000001")
                id_ex.subType = "mul";
        } else if(funct3 == "001" && funct7 == "0000000")
            id_ex.subType = "sll";
        else if(funct3 == "010" && funct7 == "0000000")
            id_ex.subType = "slt";
        else if(funct3 == "011" && funct7 == "0000000")
            id_ex.subType = "sltu";
        else if(funct3 == "100") {
            if(funct7 == "0000000")
                id_ex.subType = "xor";
            else if(funct7 == "0000001")
                id_ex.subType = "div";
        } else if(funct3 == "101") {
            if(funct7 == "0000000")
                id_ex.subType = "srl";
            else if(funct7 == "0100000")
                id_ex.subType = "sra";
        } else if(funct3 == "110") {
            if(funct7 == "0000000")
                id_ex.subType = "or";
            else if(funct7 == "0000001")
                id_ex.subType = "rem";
        } else if(funct3 == "111" && funct7 == "0000000")
            id_ex.subType = "and";
        control.regWrite = true;
        control.aluOp = 2;
        // Initially read register file values.
        id_ex.rs1Value = X[id_ex.rs1];
        id_ex.rs2Value = X[id_ex.rs2];
        stats.aluInst++;
    }
    else if(opcode == 0x13) { // I-type ALU
        id_ex.instType = 'I';
        id_ex.rs1 = (instruction >> 15) & 0x1F;
        id_ex.rd = (instruction >> 7) & 0x1F;
        unsigned int imm_unsigned = (instruction >> 20) & 0xFFF;
        id_ex.immediate = (imm_unsigned & 0x800) ? (imm_unsigned | 0xFFFFF000) : imm_unsigned;
        string funct3 = bitset<3>((instruction >> 12) & 0x7).to_string();
        if(funct3 == "000")
            id_ex.subType = "addi";
        else if(funct3 == "001")
            id_ex.subType = "slli";
        else if(funct3 == "010")
            id_ex.subType = "slti";
        else if(funct3 == "011")
            id_ex.subType = "sltiu";
        else if(funct3 == "100")
            id_ex.subType = "xori";
        else if(funct3 == "101") {
            if((imm_unsigned >> 5) & 0x1)
                id_ex.subType = "srai";
            else
                id_ex.subType = "srli";
        }
        else if(funct3 == "110")
            id_ex.subType = "ori";
        else if(funct3 == "111")
            id_ex.subType = "andi";
        control.regWrite = true;
        control.aluSrc = true;
        control.aluOp = 2;
        id_ex.rs1Value = X[id_ex.rs1];
        id_ex.rs2 = 0;
        stats.aluInst++;
    }
    else if(opcode == 0x03) { // I-type Load
        id_ex.instType = 'I';
        id_ex.rs1 = (instruction >> 15) & 0x1F;
        id_ex.rd = (instruction >> 7) & 0x1F;
        unsigned int imm_unsigned = (instruction >> 20) & 0xFFF;
        id_ex.immediate = (imm_unsigned & 0x800) ? (imm_unsigned | 0xFFFFF000) : imm_unsigned;
        string funct3 = bitset<3>((instruction >> 12) & 0x7).to_string();
        if(funct3 == "000")
            id_ex.subType = "lb";
        else if(funct3 == "001")
            id_ex.subType = "lh";
        else if(funct3 == "010")
            id_ex.subType = "lw";
        else if(funct3 == "100")
            id_ex.subType = "lbu";
        else if(funct3 == "101")
            id_ex.subType = "lhu";
        control.regWrite = true;
        control.memRead = true;
        control.memToReg = true;
        control.aluSrc = true;
        control.aluOp = 0;
        id_ex.rs1Value = X[id_ex.rs1];
        id_ex.rs2 = 0;
        stats.dataTransferInst++;
    }
    else if(opcode == 0x23) { // S-type (Store)
        id_ex.instType = 'S';
        id_ex.rs1 = (instruction >> 15) & 0x1F;
        id_ex.rs2 = (instruction >> 20) & 0x1F;
        unsigned int imm_upper = (instruction >> 25) & 0x7F;
        unsigned int imm_lower = (instruction >> 7) & 0x1F;
        unsigned int imm_unsigned = (imm_upper << 5) | imm_lower;
        id_ex.immediate = (imm_unsigned & 0x800) ? (imm_unsigned | 0xFFFFF000) : imm_unsigned;
        string funct3 = bitset<3>((instruction >> 12) & 0x7).to_string();
        if(funct3 == "000")
            id_ex.subType = "sb";
        else if(funct3 == "001")
            id_ex.subType = "sh";
        else if(funct3 == "010")
            id_ex.subType = "sw";
        control.memWrite = true;
        control.aluSrc = true;
        control.aluOp = 0;
        id_ex.rs1Value = X[id_ex.rs1];
        id_ex.rs2Value = X[id_ex.rs2];
        id_ex.rd = 0;
        stats.dataTransferInst++;
    }
    else if(opcode == 0x63) { // B-type (Branch)
        id_ex.instType = 'B';
        id_ex.rs1 = (instruction >> 15) & 0x1F;
        id_ex.rs2 = (instruction >> 20) & 0x1F;
        unsigned int imm_11 = (instruction >> 7) & 0x1;
        unsigned int imm_4_1 = (instruction >> 8) & 0xF;
        unsigned int imm_10_5 = (instruction >> 25) & 0x3F;
        unsigned int imm_12 = (instruction >> 31) & 0x1;
        unsigned int imm_unsigned = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
        id_ex.immediate = (imm_unsigned & 0x1000) ? (imm_unsigned | 0xFFFFE000) : imm_unsigned;
        string funct3 = bitset<3>((instruction >> 12) & 0x7).to_string();
        if(funct3 == "000")
            id_ex.subType = "beq";
        else if(funct3 == "001")
            id_ex.subType = "bne";
        else if(funct3 == "100")
            id_ex.subType = "blt";
        else if(funct3 == "101")
            id_ex.subType = "bge";
        else if(funct3 == "110")
            id_ex.subType = "bltu";
        else if(funct3 == "111")
            id_ex.subType = "bgeu";
        control.branch = true;
        control.aluOp = 1;
        id_ex.rs1Value = X[id_ex.rs1];
        id_ex.rs2Value = X[id_ex.rs2];
        id_ex.rd = 0;
        stats.controlInst++;
    }
    else if(opcode == 0x6F) { // J-type (jal)
        id_ex.instType = 'J';
        id_ex.rd = (instruction >> 7) & 0x1F;
        unsigned int imm_20 = (instruction >> 31) & 0x1;
        unsigned int imm_10_1 = (instruction >> 21) & 0x3FF;
        unsigned int imm_11 = (instruction >> 20) & 0x1;
        unsigned int imm_19_12 = (instruction >> 12) & 0xFF;
        unsigned int imm_unsigned = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
        id_ex.immediate = (imm_unsigned & 0x100000) ? (imm_unsigned | 0xFFF00000) : imm_unsigned;
        id_ex.subType = "jal";
        control.regWrite = true;
        control.jump = true;
        id_ex.rs1 = 0;
        id_ex.rs2 = 0;
        stats.controlInst++;
    }
    else if(opcode == 0x67) { // I-type (jalr)
        id_ex.instType = 'I';
        id_ex.rs1 = (instruction >> 15) & 0x1F;
        id_ex.rd = (instruction >> 7) & 0x1F;
        unsigned int imm_unsigned = (instruction >> 20) & 0xFFF;
        id_ex.immediate = (imm_unsigned & 0x800) ? (imm_unsigned | 0xFFFFF000) : imm_unsigned;
        id_ex.subType = "jalr";
        control.regWrite = true;
        control.jump = true;
        control.aluSrc = true;
        id_ex.rs1Value = X[id_ex.rs1];
        id_ex.rs2 = 0;
        stats.controlInst++;
    }
    else if(opcode == 0x37 || opcode == 0x17) { // U-type (lui / auipc)
        id_ex.instType = 'U';
        id_ex.rd = (instruction >> 7) & 0x1F;
        unsigned int imm_unsigned = (instruction >> 12) & 0xFFFFF;
        id_ex.immediate = imm_unsigned << 12;
        if(opcode == 0x37)
            id_ex.subType = "lui";
        else
            id_ex.subType = "auipc";
        control.regWrite = true;
        control.aluSrc = true;
        id_ex.rs1 = 0;
        id_ex.rs2 = 0;
        stats.aluInst++;
    }
    else {
        id_ex.valid = false;
        return;
    }
    id_ex.control = control;
 
    if (knobs.forwardingEnabled) {
        ForwardingBuffer fBuffer;
        saveForwardingData(fBuffer);
    
        int          fval;
        ForwardStage fsrc;
    
        // rs1
        if (fBuffer.getValue(id_ex.rs1, fval, fsrc)) {
            id_ex.rs1Value = fval;
            outputForwardingInfo(
                id_ex.rs1,
                fsrc,
                "ID/EX",      // <-- use a string here
                id_ex.rs1Value
            );
        }
    
        // rs2 (for R, B, S)
        if ((id_ex.instType=='R' ||
             id_ex.instType=='B' ||
             id_ex.instType=='S')
            && fBuffer.getValue(id_ex.rs2, fval, fsrc))
        {
            id_ex.rs2Value = fval;
            outputForwardingInfo(
                id_ex.rs2,
                fsrc,
                "ID/EX",      // <-- same here
                id_ex.rs2Value
            );
        }
    }
    
    
    // Add at the end of the decode() function, just before the closing brace
    // Track instruction if tracing is enabled
    if (currentTrace.active && if_id.pc == currentTrace.pc) {
        currentTrace.decodeCycle = clockCycles + 1;
        stringstream ss;
        ss << "Type: " << id_ex.instType << ", Subtype: " << id_ex.subType;
        if (id_ex.rs1 != 0) ss << ", rs1: x" << id_ex.rs1 << " = " << id_ex.rs1Value;
        if (id_ex.rs2 != 0) ss << ", rs2: x" << id_ex.rs2 << " = " << id_ex.rs2Value;
        if (id_ex.rd != 0) ss << ", rd: x" << id_ex.rd;
        if (id_ex.immediate != 0) ss << ", imm: " << id_ex.immediate;
        currentTrace.decodeInfo = ss.str();
    
        cout << "\nDECODE at cycle " << dec << clockCycles + 1 << endl;
        cout << "  " << currentTrace.decodeInfo << endl;
        if (stall_decode) cout << "  ** Stalled due to data hazard **" << endl;
        cout << "Contents of Dec/Exec buffer are: " << endl;
        cout << "  PC: 0x" << hex << id_ex.pc << endl;
        cout << "  Instruction: 0x" << hex << id_ex.instructionWord << endl;
    
        // Data/control dependency
        if (id_ex.control.regWrite) {
            cout << "  Register write enabled." << endl;
        } else {
            cout << "  Register write disabled." << endl;
        }
    
        // Check and print data forwarding paths
        if (knobs.forwardingEnabled) {
            ForwardingBuffer fBuffer;
            saveForwardingData(fBuffer);
    
            int fval;
            ForwardStage fsrc;
    
            cout << "  Forwarding paths to be used:" << endl;
    
            // Check forwarding for rs1
            if (id_ex.rs1 != 0 && fBuffer.getValue(id_ex.rs1, fval, fsrc)) {
                cout << "    rs1 (x" << id_ex.rs1 << ") forwarded from "
                     << (fsrc == EX_MEM ? "EX/MEM" : "MEM/WB")
                     << " with value " << fval << endl;
            }
    
            // Check forwarding for rs2 (for R, B, S types)
            if ((id_ex.instType == 'R' || id_ex.instType == 'B' || id_ex.instType == 'S') &&
                id_ex.rs2 != 0 && fBuffer.getValue(id_ex.rs2, fval, fsrc)) {
                cout << "    rs2 (x" << id_ex.rs2 << ") forwarded from "
                     << (fsrc == EX_MEM ? "EX/MEM" : "MEM/WB")
                     << " with value " << fval << endl;
            }
        }
    }

}
 
//------------------------------------------------------
// Execute Stage with Branch Predictor Update
//------------------------------------------------------
void execute() {
    if(!id_ex.valid) {
        ex_mem.valid = false;
        return;
    }
    ex_mem.valid = true;
    ex_mem.pc = id_ex.pc;
    ex_mem.instType = id_ex.instType;
    ex_mem.subType = id_ex.subType;
    ex_mem.rd = id_ex.rd;
    ex_mem.rs2Value = id_ex.rs2Value;
    ex_mem.control = id_ex.control;
    ex_mem.instructionWord = id_ex.instructionWord;
    ex_mem.instructionNum = id_ex.instructionNum;
    ex_mem.branchTaken = false;
    int operand1 = id_ex.rs1Value;
    int operand2 = (id_ex.control.aluSrc ? id_ex.immediate : id_ex.rs2Value);
    switch(id_ex.instType) {
        case 'R':
            if(id_ex.subType=="add") ex_mem.aluResult = operand1+operand2;
            else if(id_ex.subType=="sub") ex_mem.aluResult = operand1-operand2;
            else if(id_ex.subType=="sll") ex_mem.aluResult = operand1 << (operand2 & 0x1F);
            else if(id_ex.subType=="slt") ex_mem.aluResult = (operand1<operand2) ? 1 : 0;
            else if(id_ex.subType=="sltu") ex_mem.aluResult = ((unsigned int)operand1 < (unsigned int)operand2) ? 1 : 0;
            else if(id_ex.subType=="xor") ex_mem.aluResult = operand1 ^ operand2;
            else if(id_ex.subType=="srl") ex_mem.aluResult = (unsigned int)operand1 >> (operand2 & 0x1F);
            else if(id_ex.subType=="sra") ex_mem.aluResult = operand1 >> (operand2 & 0x1F);
            else if(id_ex.subType=="or") ex_mem.aluResult = operand1 | operand2;
            else if(id_ex.subType=="and") ex_mem.aluResult = operand1 & operand2;
            else if(id_ex.subType=="mul") ex_mem.aluResult = operand1 * operand2;
            else if(id_ex.subType=="div") ex_mem.aluResult = (operand2 != 0) ? operand1/operand2 : -1;
            else if(id_ex.subType=="rem") ex_mem.aluResult = (operand2 != 0) ? operand1 % operand2 : operand1;
            else ex_mem.aluResult = 0;
            break;
        case 'I':
            if(id_ex.subType=="addi") ex_mem.aluResult = operand1+operand2;
            else if(id_ex.subType=="slti") ex_mem.aluResult = (operand1<operand2) ? 1 : 0;
            else if(id_ex.subType=="sltiu") ex_mem.aluResult = ((unsigned int)operand1 < (unsigned int)operand2) ? 1 : 0;
            else if(id_ex.subType=="xori") ex_mem.aluResult = operand1 ^ operand2;
            else if(id_ex.subType=="ori") ex_mem.aluResult = operand1 | operand2;
            else if(id_ex.subType=="andi") ex_mem.aluResult = operand1 & operand2;
            else if(id_ex.subType=="slli") ex_mem.aluResult = operand1 << (operand2 & 0x1F);
            else if(id_ex.subType=="srli") ex_mem.aluResult = (unsigned int)operand1 >> (operand2 & 0x1F);
            else if(id_ex.subType=="srai") ex_mem.aluResult = operand1 >> (operand2 & 0x1F);
            else if(id_ex.subType=="jalr") {
                ex_mem.aluResult = id_ex.pc+4;
                int targetPC = (operand1 + operand2) & ~1;
                unsigned int index = (id_ex.pc/4)%BTB_SIZE;
                bool pred = false;
                if(BTB[index].valid && BTB[index].branchPC==id_ex.pc)
                    pred = PHT[index];
                if(pred != true || (BTB[index].valid && BTB[index].targetPC != targetPC)) {
                    flush_pipeline = true;
                    nextPC = targetPC;
                    stats.controlHazardCount++;
                    stats.controlHazardStalls++;
                    stats.branchMispredCount++;
                    PHT[index] = true;
                    BTB[index].valid = true;
                    BTB[index].branchPC = id_ex.pc;
                    BTB[index].targetPC = targetPC;
                }
            }
            else if(id_ex.subType=="lb" || id_ex.subType=="lh" || id_ex.subType=="lw" ||
                    id_ex.subType=="lbu" || id_ex.subType=="lhu") {
                ex_mem.aluResult = operand1+operand2;
                ex_mem.memAddress = operand1+operand2;
            }
            else ex_mem.aluResult = 0;
            break;
        case 'S':
            ex_mem.aluResult = operand1+operand2;
            ex_mem.memAddress = operand1+operand2;
            break;
        case 'B': {
            bool branch_taken = false;
            if(id_ex.subType=="beq") branch_taken = (operand1 == id_ex.rs2Value);
            else if(id_ex.subType=="bne") branch_taken = (operand1 != id_ex.rs2Value);
            else if(id_ex.subType=="blt") branch_taken = (operand1 < id_ex.rs2Value);
            else if(id_ex.subType=="bge") branch_taken = (operand1 >= id_ex.rs2Value);
            else if(id_ex.subType=="bltu") branch_taken = ((unsigned int)operand1 < (unsigned int)id_ex.rs2Value);
            else if(id_ex.subType=="bgeu") branch_taken = ((unsigned int)operand1 >= (unsigned int)id_ex.rs2Value);
            int targetPC = branch_taken ? id_ex.pc + id_ex.immediate : id_ex.pc+4;
            unsigned int index = (id_ex.pc/4)%BTB_SIZE;
            bool pred = false;
            if(BTB[index].valid && BTB[index].branchPC==id_ex.pc)
                pred = PHT[index];
            bool mispredicted = (pred!=branch_taken) || (branch_taken && BTB[index].targetPC != targetPC);
            if(mispredicted) {
                flush_pipeline = true;
                nextPC = targetPC;
                stats.controlHazardCount++;
                stats.controlHazardStalls++;
                stats.branchMispredCount++;
                PHT[index] = branch_taken;
                BTB[index].valid = true;
                BTB[index].branchPC = id_ex.pc;
                BTB[index].targetPC = targetPC;
                if (knobs.printPipelineRegisters) {
                    outputControlHazardInfo(id_ex.pc, pred, branch_taken);
                }
            }
            ex_mem.branchTaken = branch_taken;
            ex_mem.aluResult = id_ex.pc+4;
            break;
        }
        case 'J': {
            ex_mem.aluResult = id_ex.pc+4;
            int targetPC = id_ex.pc+id_ex.immediate;
            unsigned int index = (id_ex.pc/4)%BTB_SIZE;
            bool pred = false;
            if(BTB[index].valid && BTB[index].branchPC==id_ex.pc)
                pred = PHT[index];
            bool mispredicted = (!pred) || (BTB[index].targetPC != targetPC);
            if(mispredicted) {
                flush_pipeline = true;
                nextPC = targetPC;
                stats.controlHazardCount++;
                stats.controlHazardStalls++;
                stats.branchMispredCount++;
                PHT[index] = true;
                BTB[index].valid = true;
                BTB[index].branchPC = id_ex.pc;
                BTB[index].targetPC = targetPC;
                if (knobs.printPipelineRegisters) {
                    outputControlHazardInfo(id_ex.pc, pred, true);
                }
            }
            break;
        }
        case 'U':
            if(id_ex.subType=="lui")
                ex_mem.aluResult = id_ex.immediate;
            else if(id_ex.subType=="auipc")
                ex_mem.aluResult = id_ex.pc + id_ex.immediate;
            break;
        default:
            ex_mem.aluResult = 0;
            break;
    }
    
    if (id_ex.instType == 'S'
        && tempResults.memValid
        && tempResults.memRd == id_ex.rs2
      ) {
          int v = tempResults.memToReg ? tempResults.memData : tempResults.memResult;
          ex_mem.rs2Value = v;
          outputForwardingInfo(
              id_ex.rs2,
              MEM_WB,          // source
              "EX/MEM",        // destination
              ex_mem.rs2Value
          );
      }
      
    
    stats.instructionsExecuted++;

    // Add at the end of the execute() function, just before the closing brace

    // Track instruction if tracing is enabled
    if (currentTrace.active && (ex_mem.instructionNum == currentTrace.instructionNum||ex_mem.pc==currentTrace.pc)) {
        currentTrace.executeCycle = clockCycles + 1;
        currentTrace.executeResult = ex_mem.aluResult;
        
        cout << "\nEXECUTE at cycle " << dec << clockCycles + 1 << endl;
        // Contents of Exe/Mem buffer are ...
        cout << " Contents of Exe/Mem buffer are: " << endl;
        cout << "  PC: 0x" << hex << ex_mem.pc << dec << endl;
        cout << "  Instruction: 0x" << hex << ex_mem.instructionWord << dec << endl;
        cout << "  Instruction Type: " << ex_mem.instType << endl;
        cout << "  Subtype: " << ex_mem.subType << endl;
        
        cout << "  ALU Result: " << dec << ex_mem.aluResult << " (0x" << hex << ex_mem.aluResult << dec << ")" << endl;
        
        if (ex_mem.instType == 'B') {
            cout << "  Branch: " << (ex_mem.branchTaken ? "Taken" : "Not Taken") << endl;
        } else if (ex_mem.instType == 'J' || 
                 (ex_mem.instType == 'I' && ex_mem.subType == "jalr")) {
            cout << "  Jump target: 0x" << hex << nextPC << dec << endl;
        }
        
        if (flush_pipeline) {
            cout << "  ** Caused Pipeline Flush **" << endl;
        }
    }

}
 
//------------------------------------------------------
// Memory Operation Stage
//------------------------------------------------------
void mem_op() {
    if(!ex_mem.valid) {
        mem_wb.valid = false;
        return;
    }
    mem_wb.valid = true;
    mem_wb.pc = ex_mem.pc;
    mem_wb.instType = ex_mem.instType;
    mem_wb.subType = ex_mem.subType;
    mem_wb.rd = ex_mem.rd;
    mem_wb.aluResult = ex_mem.aluResult;
    mem_wb.control = ex_mem.control;
    mem_wb.instructionWord = ex_mem.instructionWord;
    mem_wb.instructionNum = ex_mem.instructionNum;
    mem_wb.memData = 0;
    if(ex_mem.control.memRead) {
        unsigned int address = ex_mem.memAddress;
        if(address >= STACK_BOTTOM && address <= STACK_TOP) {
            unsigned int stack_index = (STACK_TOP - address) / 4;
            if(stack_index < STACK_MEMORY_SIZE) {
                if(ex_mem.subType=="lw")
                    mem_wb.memData = STACKMEM[stack_index];
                else if(ex_mem.subType=="lh") {
                    int halfword = (STACKMEM[stack_index] >> ((address % 4) * 8)) & 0xFFFF;
                    mem_wb.memData = (halfword & 0x8000) ? (halfword | 0xFFFF0000) : halfword;
                }
                else if(ex_mem.subType=="lb") {
                    int byte = (STACKMEM[stack_index] >> ((address % 4) * 8)) & 0xFF;
                    mem_wb.memData = (byte & 0x80) ? (byte | 0xFFFFFF00) : byte;
                }
                else if(ex_mem.subType=="lhu")
                    mem_wb.memData = (STACKMEM[stack_index] >> ((address % 4) * 8)) & 0xFFFF;
                else if(ex_mem.subType=="lbu")
                    mem_wb.memData = (STACKMEM[stack_index] >> ((address % 4) * 8)) & 0xFF;
            }
            else {
                cout << "Error: Stack memory access out of bounds at address 0x" << hex << address << endl;
            }
        }
        else {
            unsigned int data_index = (address - DATA_MEMORY_BASE) / 4;
            if(data_index < DATA_MEMORY_SIZE) {
                if(ex_mem.subType=="lw")
                    mem_wb.memData = DMEM[data_index];
                else if(ex_mem.subType=="lh") {
                    int halfword = (DMEM[data_index] >> ((address % 4) * 8)) & 0xFFFF;
                    mem_wb.memData = (halfword & 0x8000) ? (halfword | 0xFFFF0000) : halfword;
                }
                else if(ex_mem.subType=="lb") {
                    int byte = (DMEM[data_index] >> ((address % 4) * 8)) & 0xFF;
                    mem_wb.memData = (byte & 0x80) ? (byte | 0xFFFFFF00) : byte;
                }
                else if(ex_mem.subType=="lhu")
                    mem_wb.memData = (DMEM[data_index] >> ((address % 4) * 8)) & 0xFFFF;
                else if(ex_mem.subType=="lbu")
                    mem_wb.memData = (DMEM[data_index] >> ((address % 4) * 8)) & 0xFF;
            }
            else {
                cout << "Error: Data memory access out of bounds at address 0x" << hex << address << endl;
            }
        }
    }
    else if(ex_mem.control.memWrite) {
        unsigned int address = ex_mem.memAddress;
        int data = ex_mem.rs2Value;
        if(address >= STACK_BOTTOM && address <= STACK_TOP) {
            unsigned int stack_index = (STACK_TOP - address) / 4;
            if(stack_index < STACK_MEMORY_SIZE) {
                if(ex_mem.subType=="sw")
                    STACKMEM[stack_index] = data;
                else if(ex_mem.subType=="sh") {
                    unsigned int shift = (address % 4)*8;
                    unsigned int mask = ~(0xFFFF << shift);
                    STACKMEM[stack_index] = (STACKMEM[stack_index] & mask) | ((data & 0xFFFF) << shift);
                }
                else if(ex_mem.subType=="sb") {
                    unsigned int shift = (address % 4)*8;
                    unsigned int mask = ~(0xFF << shift);
                    STACKMEM[stack_index] = (STACKMEM[stack_index] & mask) | ((data & 0xFF) << shift);
                }
            }
            else {
                cout << "Error: Stack memory access out of bounds at address 0x" << hex << address << endl;
            }
        }
        else {
            unsigned int data_index = (address - DATA_MEMORY_BASE) / 4;
            if(data_index < DATA_MEMORY_SIZE) {
                if(ex_mem.subType=="sw")
                    DMEM[data_index] = data;
                else if(ex_mem.subType=="sh") {
                    unsigned int shift = (address % 4)*8;
                    unsigned int mask = ~(0xFFFF << shift);
                    DMEM[data_index] = (DMEM[data_index] & mask) | ((data & 0xFFFF) << shift);
                }
                else if(ex_mem.subType=="sb") {
                    unsigned int shift = (address % 4)*8;
                    unsigned int mask = ~(0xFF << shift);
                    DMEM[data_index] = (DMEM[data_index] & mask) | ((data & 0xFF) << shift);
                }
            }
            else {
                cout << "Error: Data memory access out of bounds at address 0x" << hex << address << endl;
            }
        }
    }
    
    if (ex_mem.valid && ex_mem.control.regWrite && ex_mem.rd != 0) {
        tempResults.memValid = true;
        tempResults.memRd = ex_mem.rd;
        tempResults.memResult = ex_mem.aluResult;
        tempResults.memData = mem_wb.memData;
        tempResults.memRegWrite = ex_mem.control.regWrite;
        tempResults.memToReg = ex_mem.control.memToReg;
    }

    // Add at the end of the mem_op() function, just before the closing brace
    // Track instruction if tracing is enabled
    if (currentTrace.active && (mem_wb.instructionNum == currentTrace.instructionNum||ex_mem.pc==currentTrace.pc)) {
        currentTrace.memoryCycle = clockCycles + 1;
        currentTrace.memoryResult = mem_wb.control.memToReg ? mem_wb.memData : mem_wb.aluResult;
        
        cout << "\nMEMORY at cycle " << dec << clockCycles + 1 << endl;
        // Contents of Mem/WB buffer are ...
        cout<<"Contents of Mem/WB buffer are: " << endl;
        cout << "  PC: 0x" << hex << mem_wb.pc << dec << endl;
        cout << "  Instruction: 0x" << hex << mem_wb.instructionWord << dec << endl;
        cout << "  Instruction Type: " << mem_wb.instType << endl;
        cout << "  Subtype: " << mem_wb.subType << endl;
        cout << "  ALU Result: " << dec << mem_wb.aluResult << " (0x" << hex << mem_wb.aluResult << dec << ")" << endl;
        
        if (mem_wb.control.memRead) {
            cout << "  Memory Read: Address 0x" << hex << ex_mem.memAddress 
                 << ", Data " << dec << mem_wb.memData << endl;
        } else if (ex_mem.control.memWrite) {
            cout << "  Memory Write: Address 0x" << hex << ex_mem.memAddress 
                 << ", Data " << dec << ex_mem.rs2Value << endl;
        } else {
            cout << "  No memory operation" << endl;
        }
    }

}
 
//------------------------------------------------------
// Write-Back Stage
//------------------------------------------------------
void write_back() {
    if(!mem_wb.valid)
        return;
    if(mem_wb.control.regWrite) {
        if(mem_wb.rd != 0) {
            if(mem_wb.control.memToReg)
                X[mem_wb.rd] = mem_wb.memData;
            else
                X[mem_wb.rd] = mem_wb.aluResult;
            if(knobs.printPipelineRegisters) {
                cout << "Write-Back: Writing " << (mem_wb.control.memToReg ? mem_wb.memData : mem_wb.aluResult)
                     << " to register x" << mem_wb.rd << endl;
            }
        } else if(knobs.printPipelineRegisters)
            cout << "Write-Back: Write to x0 ignored" << endl;
    } else if(knobs.printPipelineRegisters)
        cout << "Write-Back: No register write" << endl;

    // Add at the end of write_back() function, before the trace code
    // This ensures we're tracking what just completed writeback
    if(mem_wb.valid) {
        wb_complete.valid = true;
        wb_complete.pc = mem_wb.pc;
        wb_complete.instType = mem_wb.instType;
        wb_complete.subType = mem_wb.subType;
        wb_complete.rd = mem_wb.rd;
        wb_complete.regWrite = mem_wb.control.regWrite;
        wb_complete.destReg = mem_wb.rd;
        wb_complete.instructionNum = mem_wb.instructionNum;
        
        // Store the actual result value (from memory or ALU)
        if(mem_wb.control.regWrite && mem_wb.rd != 0) {
            wb_complete.result = (mem_wb.control.memToReg) ? mem_wb.memData : mem_wb.aluResult;
        } else {
            wb_complete.result = 0;
        }
    } else {
        wb_complete.valid = false;
    }


   // Replace your existing trace code in write_back() with this:

    // Track instruction if tracing is enabled
    if (currentTrace.active && (mem_wb.instructionNum == currentTrace.instructionNum||ex_mem.pc==currentTrace.pc)) {
        currentTrace.writebackCycle = clockCycles;
        currentTrace.writebackResult = mem_wb.control.memToReg ? mem_wb.memData : mem_wb.aluResult;
        
        cout << "\nWRITE-BACK at cycle " << dec << clockCycles << endl;
        if (mem_wb.control.regWrite && mem_wb.rd != 0) {
            cout << "  Register Write: x" << dec << mem_wb.rd << " = " << dec 
                 << (mem_wb.control.memToReg ? mem_wb.memData : mem_wb.aluResult) 
                 << " (0x" << hex << (mem_wb.control.memToReg ? mem_wb.memData : mem_wb.aluResult) << dec << ")" << endl;
        } else {
            cout << "  No register write" << endl;
        }
        
        // Print full trace summary
        cout << "\n--- TRACE SUMMARY:"<<endl;
        cout<<" Instruction " << currentTrace.instructionNum 
             << " (0x" << hex << currentTrace.instruction << dec << ") ---" << endl;
        cout << "  PC: 0x" << hex << currentTrace.pc << dec << endl;
        cout << "  Fetch Cycle: " << currentTrace.fetchCycle << endl;
        cout << "  Decode Cycle: " << currentTrace.decodeCycle << endl;
        cout << "  Execute Cycle: " << currentTrace.executeCycle << endl;
        cout << "  Memory Cycle: " << currentTrace.memoryCycle << endl;
        cout << "  Writeback Cycle: " << currentTrace.writebackCycle << endl;
        cout << "  Total Cycles in Pipeline: " 
             << (currentTrace.writebackCycle - currentTrace.fetchCycle + 1) << endl;
        
               // Define NUM_REGISTERS if not already defined
        #define NUM_REGISTERS 32
        
        // Contents of RegisterFile are ...
        cout << "  Register File Contents:" << endl;
        for (int i = 0; i < NUM_REGISTERS; i++) {
            cout << "    x" << dec << i << ": " << dec << X[i] 
                 << " (0x" << hex << X[i] << ")" << endl;
        }
        // Reset for next instruction to trace
        currentTrace = InstructionTrace();
    }

}
 
//------------------------------------------------------
// Pipeline Register Update (Shifting)
//------------------------------------------------------
void update_pipeline() {
    WB_Complete_Register new_wb_complete = wb_complete;  // Add this line
    MEM_WB_Register new_mem_wb = mem_wb;
    EX_MEM_Register new_ex_mem = ex_mem;
    ID_EX_Register new_id_ex = id_ex;
    IF_ID_Register new_if_id = if_id;
    if(flush_pipeline) {
        new_if_id.valid = false;
        new_id_ex.valid = false;
        flush_pipeline = false;
        pc = nextPC;
        if(knobs.printPipelineRegisters) {
            cout << "Pipeline Flush: New PC = 0x" << hex << pc << endl;
        }
    }
    if(stall_decode) {
        new_id_ex.valid = false;
        new_if_id = if_id;
        stats.totalStalls++;
        stats.dataHazardStalls++;
    }
    if(stall_fetch) {
        new_if_id = if_id;
        stats.totalStalls++;
    }
    wb_complete = new_wb_complete;  // Add this line
    mem_wb = new_mem_wb;
    ex_mem = new_ex_mem;
    id_ex = new_id_ex;
    if_id = new_if_id;
    stall_decode = false;
    stall_fetch = false;
    stats.totalCycles = clockCycles;
}
 
//------------------------------------------------------
// Print Final Statistics Report and Dump State Files
//------------------------------------------------------
void printFinalStatistics() {
    ostringstream oss;
    oss << "-------------------------------------" << endl;
    oss << "Simulation Finished" << endl;
    
    if (!knobs.pipeliningEnabled) {
        // Non-pipelined statistics
        double CPI_np = 1.0; // Always 1.0 in non-pipelined
        oss << "Execution Mode: Non-Pipelined" << endl;
        oss << "Total Cycles: " << clockCycles_np << endl;
        oss << "Instructions Executed: " << sz_np << endl;
        oss << "CPI: " << fixed << setprecision(2) << CPI_np << endl;
    } else {
        // Pipelined statistics
        double CPI = (stats.instructionsExecuted > 0) ? 
                    (double)clockCycles / stats.instructionsExecuted : 0.0;
        oss << "Execution Mode: Pipelined" << endl;
        oss << "Total Cycles: " << stats.totalCycles << endl;
        oss << "Instructions Executed: " << stats.instructionsExecuted << endl;
        oss << "CPI: " << fixed << setprecision(2) << CPI << endl;
        oss << "Load/Store Instructions: " << stats.dataTransferInst << endl;
        oss << "ALU Instructions: " << stats.aluInst << endl;
        oss << "Control Instructions: " << stats.controlInst << endl;
        oss << "Total Stalls: " << stats.totalStalls << endl;
        oss << "Data Hazard Stalls: " << stats.dataHazardStalls << endl;
        oss << "Control Hazard Stalls: " << stats.controlHazardStalls << endl;
        oss << "Data Hazards Detected: " << stats.dataHazardCount << endl;
        oss << "Control Hazards Detected: " << stats.controlHazardCount << endl;
        oss << "Branch Mispredictions: " << stats.branchMispredCount << endl;
    }
    
    cout << oss.str();
    ofstream outfile("stats.out");
    if(outfile.is_open()){
        outfile << oss.str();
        outfile.close();
        cout << "Final statistics written to stats.out" << endl;
    }
    else {
        cerr << "Error: Could not open stats.out for writing." << endl;
    }
    
    // Dump state files
    if (!knobs.pipeliningEnabled) {
        // Use non-pipelined dumping functions
        FILE *fp = fopen("register.mem", "w");
        if (fp) {
            for (unsigned int i = 0; i < 32; i++) {
                fprintf(fp, "x%u - %u\n", i, X_np[i]);
            }
            fclose(fp);
        }
        
        fp = fopen("D_Memory.mem", "w");
        if (fp) {
            // Print DMEM
            for (unsigned int i = 0; i < 50; i++) {
                fprintf(fp, "Addr 0x%08x: 0x%08x\n", 0x10000000 + i*4, DMEM_np[i]);
            }
            fclose(fp);
        }
    } else {
        // Use pipelined dumping functions
        dump_registers();
        dump_memory();
        dump_BP();
    }
}
 
//------------------------------------------------------
// Main Simulation Loop (Alternate Main to Support --input Flag)
//------------------------------------------------------
int mainEntry(int argc, char *argv[]) {
    parseCommandLineArgs(argc, argv);

    // Determine mode (step or continuous)
    bool step_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--step") == 0) {
            step_mode = true;
            break;
        }
    }

    bool stateLoaded = false;
    if (step_mode) {
        cout << "Step mode activated. Attempting to load previous state..." << endl;
        stateLoaded = load_state();
        if (stateLoaded) {
             cout << "State loaded. Proceeding with step execution." << endl;
             // State is loaded, including memory. Do NOT re-initialize or reload input file.
        } else {
            cout << "Failed to load state (or first run). Initializing simulator..." << endl;
            // Initialize memory, registers, BP ONLY if state load failed.
            memset(X, 0, sizeof(X));
            memset(MEM, 0, sizeof(MEM)); // Initialize instruction memory too
            memset(DMEM, 0, sizeof(DMEM));
            memset(STACKMEM, 0, sizeof(STACKMEM));
            initializeBranchPredictor();
            X[2] = STACK_TOP; // Stack pointer initialization
            pc = 0;           // Start from PC 0
            clockCycles = 0;  // Reset clock
            instructionCounter = 0; // Reset counter
            stats = {}; // Reset statistics
            // Reset pipeline registers to initial state
            if_id = {false, 0, 0, 0};
            id_ex = {false, 0, '0', "", 0, 0, 0, 0, 0, 0, {false, false, false, false, false, false, false, 0}, 0, 0};
            ex_mem = {false, 0, '0', "", 0, 0, 0, 0, false, {false, false, false, false, false, false, false, 0}, 0, 0};
            mem_wb = {false, 0, '0', "", 0, 0, 0, {false, false, false, false, false, false, false, 0}, 0, 0};
            wb_complete = {false, 0, '0', "", 0, 0, false, 0, 0}; // Add this line
            // Reset pipeline control flags
            stall_fetch = false;
            stall_decode = false;
            flush_pipeline = false;
            nextPC = 0;
            // Reset temp results
            tempResults = TempResults();


            // If state load failed AND an input file is provided, load it now.
            if (!knobs.inputFile.empty()) {
                cout << "Loading program from input file: " << knobs.inputFile << endl;
                if (!loadInputFile(knobs.inputFile)) {
                    cerr << "Critical Error: Failed to load input file '" << knobs.inputFile << "' after failed state load. Exiting." << endl;
                    return 1;
                }
            } else {
                // If no state loaded and no input file, we can't run.
                 cerr << "Critical Error: Failed to load state and no input file specified via --input. Exiting." << endl;
                 return 1;
            }
        }
    } else {
        // Continuous mode: Always initialize and load the input file.
        cout << "Continuous mode activated. Initializing simulator..." << endl;
        memset(X, 0, sizeof(X));
        memset(MEM, 0, sizeof(MEM));
        memset(DMEM, 0, sizeof(DMEM));
        memset(STACKMEM, 0, sizeof(STACKMEM));
        initializeBranchPredictor();
        X[2] = STACK_TOP; // Stack pointer initialization
        pc = 0;           // Start from PC 0
        clockCycles = 0;  // Reset clock
        instructionCounter = 0; // Reset counter
        stats = {}; // Reset statistics
         // Reset pipeline registers to initial state
        if_id = {false, 0, 0, 0};
        id_ex = {false, 0, '0', "", 0, 0, 0, 0, 0, 0, {false, false, false, false, false, false, false, 0}, 0, 0};
        ex_mem = {false, 0, '0', "", 0, 0, 0, 0, false, {false, false, false, false, false, false, false, 0}, 0, 0};
        mem_wb = {false, 0, '0', "", 0, 0, 0, {false, false, false, false, false, false, false, 0}, 0, 0};
         // Reset pipeline control flags
        stall_fetch = false;
        stall_decode = false;
        flush_pipeline = false;
        nextPC = 0;
         // Reset temp results
        tempResults = TempResults();


        if (!knobs.inputFile.empty()) {
            cout << "Loading program from input file: " << knobs.inputFile << endl;
            if (!loadInputFile(knobs.inputFile)) {
                cerr << "Critical Error: Failed to load input file '" << knobs.inputFile << "'. Exiting." << endl;
                return 1;
            }
        } else {
            cerr << "Critical Error: No input file specified via --input for continuous run. Exiting." << endl;
            return 1;
        }
    }

    // In the mainEntry function where you handle non-pipelined execution:
    if (!knobs.pipeliningEnabled) {
        cout << (step_mode ? "Step" : "Continuous")
            << " mode: running non-pipelined simulator." << endl;

        // Initialize non-pipelined simulator
        reset_proc_np(); 
        
        // Load program into non-pipelined memory
        if (!knobs.inputFile.empty()) {
            // Fix: Pass only a boolean argument instead of string and boolean
            load_program_memory_np(false);
        } else {
            cerr << "Critical Error: No input file specified for non-pipelined mode." << endl;
            return 1;
        }

        // Run in appropriate mode
        if (step_mode) {
            return run_step_np(); // Returns 0 to continue, 1 to exit
        } else {
            run_riscvsim_np(); // Runs until completion
            return 0;
        }
    }


    // --- Simulation Loop ---

    if (step_mode) {
        cout << "\n--- Executing Single Cycle ---" << endl;

        // Print initial state for the step (if requested)
        if(knobs.printPipelineRegisters) {
             cout << "Pipeline State Before Cycle " << clockCycles + 1 << ":" << endl;
             outputPipelineStageDetails(); // Use the detailed print function
        }
        if(knobs.printRegisterEachCycle) {
            cout << "Register File Before Cycle:" << endl;
            for(int i = 0; i < 32; i++){
                cout << "x" << i << " = 0x" << hex << X[i] << " (" << dec << X[i] << ")\t";
                if((i+1) % 4 == 0) cout << endl;
            }
            cout << endl;
        }
        if(knobs.printBranchPredictorInfo) {
             cout << "Branch Predictor Before Cycle:" << endl;
             printBranchPredictor();
        }

        // Execute one cycle
        tempResults.clear(); // Clear temp results at the start of the cycle
        hazardDetection();
        // Run stages in reverse order for correct data flow simulation within a cycle
        write_back();
        mem_op();
        execute();
        decode();
        if(!stall_fetch) fetch(); // Fetch depends on stall detection
        update_pipeline();       // Shift pipeline registers

        clockCycles++; // Increment clock *after* completing the cycle

        // Print state after the cycle (if requested)
        if(knobs.printPipelineRegisters) {
            cout << "\nPipeline State After Cycle " << clockCycles << ":" << endl;
            outputPipelineStageDetails(); // Use the detailed print function
            cout << "--- Pipeline Register Summary ---" << endl;
            cout << "IF/ID:  Valid=" << (if_id.valid ? "T" : "F") << ", PC=0x" << hex << if_id.pc << ", Inst=0x" << if_id.instruction << ", PredPC=0x" << if_id.predictedPC << dec << endl;
            cout << "ID/EX:  Valid=" << (id_ex.valid ? "T" : "F"); if(id_ex.valid) cout << ", PC=0x" << hex << id_ex.pc << ", Type=" << id_ex.instType << ", Sub=" << id_ex.subType << dec; cout << endl;
            cout << "EX/MEM: Valid=" << (ex_mem.valid ? "T" : "F"); if(ex_mem.valid) cout << ", PC=0x" << hex << ex_mem.pc << ", Type=" << ex_mem.instType << ", Sub=" << ex_mem.subType << ", ALU= " << dec << ex_mem.aluResult; cout << endl;
            cout << "MEM/WB: Valid=" << (mem_wb.valid ? "T" : "F"); if(mem_wb.valid) cout << ", PC=0x" << hex << mem_wb.pc << ", Type=" << mem_wb.instType << ", Sub=" << mem_wb.subType; cout << endl;
            cout << "-------------------------------" << endl;
        }
        if(knobs.printRegisterEachCycle) {
            cout << "\nRegister File After Cycle:" << endl;
            for(int i = 0; i < 32; i++){
                cout << "x" << i << " = 0x" << hex << X[i] << " (" << dec << X[i] << ")\t";
                if((i+1) % 4 == 0) cout << endl;
            }
            cout << endl;
        }
        if(knobs.printBranchPredictorInfo) {
            cout << "\nBranch Predictor After Cycle " << clockCycles << ":" << endl;
            printBranchPredictor();
        }

        if(knobs.saveCycleSnapshots) {
            store_pipeline_snapshot();
            dump_pipeline_snapshots(); // Optionally dump immediately or just at the end
        }

        // Dump memory/registers state always at end of step for external tools
        dump_registers();
        dump_memory();

        // Save state for the *next* step.
        save_state();

        cout << "--- Cycle " << clockCycles << " Complete ---" << endl;

        // Check for program termination condition
        bool pipeline_empty = !if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid;
        if (pc >= sz * 4 && pipeline_empty) {
            cout << "\nProgram finished." << endl;
            printFinalStatistics();
            // Optionally clean up state file on completion?
            // remove("sim_state.dat");
        } else {
             cout << "\nReady for next step. Run with --step again." << endl;
        }

        return 0; // Exit after one step

    } else {
        // --- Continuous Run Mode ---
        cout << "\n--- Starting Continuous Simulation ---" << endl;
        while(true) {
             // Check termination condition *before* starting the cycle
             bool pipeline_empty = !if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid;
             if (pc >= sz * 4 && pipeline_empty) {
                 cout << "\n--- Simulation Complete ---" << endl;
                 break; // Exit the loop
             }

            // Execute one cycle
            tempResults.clear();
            hazardDetection();
            write_back();
            mem_op();
            execute();
            decode();
            if(!stall_fetch) fetch();
            update_pipeline();

            clockCycles++;

            // Optional per-cycle printing for continuous mode (can be verbose)
            if(knobs.printPipelineRegisters) {
                 cout << "\nPipeline State After Cycle " << clockCycles << ":" << endl;
                 outputPipelineStageDetails();
                 cout << "--- Pipeline Register Summary ---" << endl;
                 cout << "IF/ID:  Valid=" << (if_id.valid ? "T" : "F") << ", PC=0x" << hex << if_id.pc << ", Inst=0x" << if_id.instruction << ", PredPC=0x" << if_id.predictedPC << dec << endl;
                 cout << "ID/EX:  Valid=" << (id_ex.valid ? "T" : "F"); if(id_ex.valid) cout << ", PC=0x" << hex << id_ex.pc << ", Type=" << id_ex.instType << ", Sub=" << id_ex.subType << dec; cout << endl;
                 cout << "EX/MEM: Valid=" << (ex_mem.valid ? "T" : "F"); if(ex_mem.valid) cout << ", PC=0x" << hex << ex_mem.pc << ", Type=" << ex_mem.instType << ", Sub=" << ex_mem.subType << ", ALU= " << dec << ex_mem.aluResult; cout << endl;
                 cout << "MEM/WB: Valid=" << (mem_wb.valid ? "T" : "F"); if(mem_wb.valid) cout << ", PC=0x" << hex << mem_wb.pc << ", Type=" << mem_wb.instType << ", Sub=" << mem_wb.subType; cout << endl;
                 cout << "-------------------------------" << endl;
            }
            if(knobs.printRegisterEachCycle) {
                 cout << "\nRegister File After Cycle " << clockCycles << ":" << endl;
                 for(int i = 0; i < 32; i++){
                     cout << "x" << i << " = 0x" << hex << X[i] << " (" << dec << X[i] << ")\t";
                     if((i+1) % 4 == 0) cout << endl;
                 }
                 cout << endl;
            }
            if(knobs.printBranchPredictorInfo) {
                 cout << "\nBranch Predictor After Cycle " << clockCycles << ":" << endl;
                 printBranchPredictor();
            }

            if(knobs.saveCycleSnapshots) {
                 store_pipeline_snapshot();
            }

             // Limit cycles to prevent infinite loops in case of issues
             if (clockCycles > 500000) { // Adjust limit as needed
                 cerr << "Warning: Simulation exceeded maximum cycle limit. Terminating." << endl;
                 break;
             }
        } // end while loop

        // Final actions after continuous run completes
        if(knobs.saveCycleSnapshots) {
             dump_pipeline_snapshots(); // Dump all collected snapshots
        }
        dump_registers(); // Dump final state
        dump_memory();
        printFinalStatistics(); // Print final stats
    }

    return 0;
}
 
//------------------------------------------------------
// Main Entry Point
//------------------------------------------------------
int main(int argc, char *argv[]) {
    return mainEntry(argc, argv);
}