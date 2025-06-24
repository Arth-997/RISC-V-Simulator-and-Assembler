#ifndef TEXT_SEGMENT_H
#define TEXT_SEGMENT_H

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <algorithm>
#include <string>
#include <fstream>
#include <iomanip>
using namespace std;

// Helper function: trim leading and trailing whitespace
string trim(const string &str) {
    const string whitespace = " \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    return (start == string::npos) ? "" : str.substr(start, end - start + 1);
}

// Helper function: explicit two's complement conversion for negative immediates
string explicitTwoComplement(int value, int bits) {
    // value should be negative; create a mask for the desired number of bits.
    unsigned int mask = (1 << bits) - 1;
    int twosComp = ((~(-value) + 1) & mask);
    return bitset<32>(twosComp).to_string().substr(32 - bits, bits);
}

// Struct to encapsulate instruction properties
struct InstructionSet {
    unordered_map<string, string> instructionFormats;
    unordered_map<string, string> opcodeMap;
    unordered_map<string, string> funct3Map;
    unordered_map<string, string> funct7Map;
    unordered_map<string, bool> immediateMap;
    unordered_map<string, string> registerMap;
    unordered_map<string, int> labelMap;

    // Function to add label
    void add_pair(const string &label, int address) {
        labelMap[label] = address;
    }

    // Retrieve label address
    int currentPC(const string &label) {
        return labelMap.count(label) ? labelMap[label] : -1;
    }

    // Two's Complement Conversion using bitset slicing (for non-negative values)
    string signedValue(int value, int bits) {
        return bitset<32>(value).to_string().substr(32 - bits, bits);
    }

    // Initialize maps
    void initialize() {
        // R-Format Instructions
        string rFormat[] = {"add", "and", "or", "sll", "slt", "sra", "srl", "sub", "xor", "mul", "div", "rem"};
        for (const auto &inst : rFormat) {
            instructionFormats[inst] = "R";
            opcodeMap[inst] = "0110011";
            immediateMap[inst] = false;
        }

        funct3Map = {{"add", "000"}, {"and", "111"}, {"or", "110"}, {"sll", "001"},
                     {"slt", "010"}, {"sra", "101"}, {"srl", "101"}, {"sub", "000"},
                     {"xor", "100"}, {"mul", "000"}, {"div", "100"}, {"rem", "110"}};

        funct7Map = {{"add", "0000000"}, {"sub", "0100000"}, {"and", "0000000"},
                     {"or", "0000000"}, {"sll", "0000000"}, {"slt", "0000000"},
                     {"sra", "0100000"}, {"srl", "0000000"}, {"xor", "0000000"},
                     {"mul", "0000001"}, {"div", "0000001"}, {"rem", "0000001"}};

        // Register Map
        for (int i = 0; i < 32; i++) {
            registerMap["x" + to_string(i)] = bitset<5>(i).to_string();
        }

        // I-Format Instructions
        string iFormat[] = {"addi", "andi", "ori", "lb", "ld", "lh", "lw", "jalr"};
        for (const auto &inst : iFormat) {
            instructionFormats[inst] = "I";
            immediateMap[inst] = true;
        }

        opcodeMap["addi"] = "0010011";  funct3Map["addi"] = "000";
        opcodeMap["andi"] = "0010011";  funct3Map["andi"] = "111";
        opcodeMap["ori"]  = "0010011";  funct3Map["ori"]  = "110";
        opcodeMap["lb"]   = "0000011";  funct3Map["lb"]   = "000";
        opcodeMap["ld"]   = "0000011";  funct3Map["ld"]   = "011";
        opcodeMap["lh"]   = "0000011";  funct3Map["lh"]   = "001";
        opcodeMap["lw"]   = "0000011";  funct3Map["lw"]   = "010";
        opcodeMap["jalr"] = "1100111";  funct3Map["jalr"] = "000";

        // S-Format Instructions
        string sFormat[] = {"sb", "sw", "sd", "sh"};
        for (const auto &inst : sFormat) {
            instructionFormats[inst] = "S";
            immediateMap[inst] = true;
        }

        opcodeMap["sb"] = "0100011";  funct3Map["sb"] = "000";
        opcodeMap["sw"] = "0100011";  funct3Map["sw"] = "010";
        opcodeMap["sd"] = "0100011";  funct3Map["sd"] = "011";
        opcodeMap["sh"] = "0100011";  funct3Map["sh"] = "001";

        // SB-Format Instructions
        string sbFormat[] = {"beq", "bne", "bge", "blt"};
        for (const auto &inst : sbFormat) {
            instructionFormats[inst] = "SB";
            immediateMap[inst] = true;
        }

        opcodeMap["beq"] = "1100011";  funct3Map["beq"] = "000";
        opcodeMap["bne"] = "1100011";  funct3Map["bne"] = "001";
        opcodeMap["bge"] = "1100011";  funct3Map["bge"] = "101";
        opcodeMap["blt"] = "1100011";  funct3Map["blt"] = "100";

        // U-Format Instructions
        opcodeMap["auipc"] = "0010111";
        opcodeMap["lui"]   = "0110111";
        instructionFormats["auipc"] = instructionFormats["lui"] = "U";
        immediateMap["auipc"] = immediateMap["lui"] = true;

        // UJ-Format Instructions
        opcodeMap["jal"] = "1101111";
        instructionFormats["jal"] = "UJ";
        immediateMap["jal"] = true;
    }
};

// Struct to hold the fields of an instruction for detailed output
struct InstructionFields {
    string opcode;
    string funct3;
    string funct7;
    string rd;
    string rs1;
    string rs2;
    string immediate;
};

// Function to generate R-Type machine code
string generateRFormatMachineCode(InstructionSet &instSet, const string &instructionLine) {
    stringstream ss(instructionLine);
    string instruction, rd, rs1, rs2;
    
    ss >> instruction >> rd >> rs1 >> rs2; // Extract instruction and registers

    // Remove the excess whitespaces using the trim function
    instruction = trim(instruction);

    // Now remove the commas from the registers
    rd.erase(remove(rd.begin(), rd.end(), ','), rd.end());
    rs1.erase(remove(rs1.begin(), rs1.end(), ','), rs1.end());
    rs2.erase(remove(rs2.begin(), rs2.end(), ','), rs2.end());

    string opcode = instSet.opcodeMap[instruction];
    string funct3 = instSet.funct3Map[instruction];
    string funct7 = instSet.funct7Map[instruction];

    string rdBinary = instSet.registerMap[rd];
    string rs1Binary = instSet.registerMap[rs1];
    string rs2Binary = instSet.registerMap[rs2];

    if (opcode.empty() || funct3.empty() || funct7.empty() || 
        rdBinary.empty() || rs1Binary.empty() || rs2Binary.empty()) {
        return "ERROR: Invalid instruction or register!";
    }

    // Assemble machine code: funct7 + rs2 + rs1 + funct3 + rd + opcode
    return funct7 + rs2Binary + rs1Binary + funct3 + rdBinary + opcode;
}

// Function to generate I-Type machine code
// Helper function to parse offset and register from memory reference format (e.g., "8(x13)")
std::pair<string, string> parseMemoryReference(const string &memRef) {
    string immStr, rs;
    size_t openParenPos = memRef.find('(');
    size_t closeParenPos = memRef.find(')');
    
    if (openParenPos != string::npos && closeParenPos != string::npos) {
        // Extract immediate (offset) - everything before the opening parenthesis
        immStr = memRef.substr(0, openParenPos);
        immStr = trim(immStr); // Remove any leading/trailing whitespace
        
        // Extract register - everything between the parentheses
        rs = memRef.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
        rs = trim(rs); // Remove any leading/trailing whitespace
    } else {
        // If not in the expected format, return empty strings
        immStr = "";
        rs = "";
    }
    
    return {immStr, rs};
}

// Function to generate I-Type machine code - updated to handle parentheses syntax
// Function to generate I-Type machine code
// Function to generate I-Type machine code
string generateIFormatMachineCode(InstructionSet &instSet, const string &instructionLine) {
    stringstream ss(instructionLine);
    string instruction, rd, remaining;
    
    // Extract instruction and destination register
    ss >> instruction >> rd;
    
    // Remove the excess whitespaces and commas
    instruction = trim(instruction);
    rd = trim(rd);
    rd.erase(remove(rd.begin(), rd.end(), ','), rd.end());
    
    // Get the remaining part of the instruction
    getline(ss, remaining);
    remaining = trim(remaining);
    
    string rs1, immStr;
    
    // Check if this is a load instruction
    bool isLoadInstruction = (instruction == "lb" || instruction == "lh" || instruction == "lw" || instruction == "ld");
    
    if (isLoadInstruction) {
        // First, try to handle the parenthesized format: lb x12, 8(x13)
        auto [offset, baseReg] = parseMemoryReference(remaining);
        
        if (!offset.empty() && !baseReg.empty()) {
            // Successfully parsed the memory reference format
            immStr = offset;
            rs1 = baseReg;
        } else {
            // Handle the non-parenthesized format: ld x18, 32 x19
            stringstream remainingSS(remaining);
            string imm, reg;
            remainingSS >> imm >> reg;
            
            // Remove any commas
            imm.erase(remove(imm.begin(), imm.end(), ','), imm.end());
            
            if (!imm.empty() && !reg.empty()) {
                immStr = imm;
                rs1 = reg;
            } else {
                return "ERROR: Invalid instruction format!";
            }
        }
    } else {
        // Standard I-format instructions (addi, etc.)
        stringstream remainingSS(remaining);
        string part1, part2;
        remainingSS >> part1 >> part2;
        
        // Remove commas if any
        part1.erase(remove(part1.begin(), part1.end(), ','), part1.end());
        
        if (part2.empty()) {
            return "ERROR: Invalid instruction format!";
        }
        
        rs1 = part1;
        immStr = part2;
    }

    string opcode = instSet.opcodeMap[instruction];
    string funct3 = instSet.funct3Map[instruction];

    string rdBinary = instSet.registerMap[rd];
    string rs1Binary = instSet.registerMap[rs1];

    if (opcode.empty() || funct3.empty() || rdBinary.empty() || rs1Binary.empty()) {
        return "ERROR: Invalid instruction or register!";
    }

    int imm = 0;
    try {
        if (immStr.empty()) {
            return "ERROR: Missing immediate value!";
        } else if (immStr.substr(0, 2) == "0b") { // Binary Immediate
            imm = stoi(immStr.substr(2), nullptr, 2);
        } else if (immStr.substr(0, 2) == "0x") { // Hexadecimal Immediate
            imm = stoi(immStr.substr(2), nullptr, 16);
        } else { // Decimal Immediate
            imm = stoi(immStr);
        }
    } catch (const std::exception& e) {
        return "ERROR: Invalid immediate value!";
    }

    if (imm < -2048 || imm > 2047) {
        return "ERROR: Immediate value out of range!";
    }

    // Use explicit two's complement conversion if the immediate is negative.
    string immBinary = (imm < 0) ? explicitTwoComplement(imm, 12)
                                 : bitset<12>(imm).to_string();

    // Assemble machine code: immediate + rs1 + funct3 + rd + opcode
    return immBinary + rs1Binary + funct3 + rdBinary + opcode;
}

// Function to generate S-Format machine code
string generateSFormatMachineCode(InstructionSet &instSet, const string &instructionLine) {
    stringstream ss(instructionLine);
    string instruction, rs2, remaining;

    // Extract instruction and source register (value to store)
    ss >> instruction >> rs2;

    // Remove the excess whitespaces and commas
    instruction = trim(instruction);
    rs2 = trim(rs2);
    rs2.erase(remove(rs2.begin(), rs2.end(), ','), rs2.end());

    // Get the remaining part of the instruction
    getline(ss, remaining);
    remaining = trim(remaining);
    
    string rs1, immStr;
    
    // First, try to handle the parenthesized format: sb x22, 8(x23)
    auto [offset, baseReg] = parseMemoryReference(remaining);
    
    if (!offset.empty() && !baseReg.empty()) {
        // Successfully parsed the memory reference format
        immStr = offset;
        rs1 = baseReg;
    } else {
        // Handle the non-parenthesized format: sb x22, 8 x23
        stringstream remainingSS(remaining);
        string imm, reg;
        remainingSS >> imm >> reg;
        
        // Remove any commas
        imm.erase(remove(imm.begin(), imm.end(), ','), imm.end());
        
        if (!imm.empty() && !reg.empty()) {
            immStr = imm;
            rs1 = reg;
        } else {
            return "ERROR: Invalid instruction format!";
        }
    }

    string opcode = instSet.opcodeMap[instruction];
    string funct3 = instSet.funct3Map[instruction];

    string rs2Binary = instSet.registerMap[rs2];
    string rs1Binary = instSet.registerMap[rs1];

    if (opcode.empty() || funct3.empty() || rs2Binary.empty() || rs1Binary.empty()) {
        return "ERROR: Invalid instruction or register!";
    }

    int imm = 0;
    try {
        if (immStr.empty()) {
            return "ERROR: Missing immediate value!";
        } else if (immStr.substr(0, 2) == "0b") { // Binary Immediate
            imm = stoi(immStr.substr(2), nullptr, 2);
        } else if (immStr.substr(0, 2) == "0x") { // Hexadecimal Immediate
            imm = stoi(immStr.substr(2), nullptr, 16);
        } else { // Decimal Immediate
            imm = stoi(immStr);
        }
    } catch (const std::exception& e) {
        return "ERROR: Invalid immediate value!";
    }
    
    if (imm < -2048 || imm > 2047) {
        return "ERROR: Immediate value out of range!";
    }

    // Use explicit two's complement conversion if negative.
    string immBinary = (imm < 0) ? explicitTwoComplement(imm, 12)
                                 : bitset<12>(imm).to_string();

    string immUpper = immBinary.substr(0, 7);  // imm[11:5]
    string immLower = immBinary.substr(7);     // imm[4:0]

    // Assemble machine code: imm[11:5] + rs2 + rs1 + funct3 + imm[4:0] + opcode
    return immUpper + rs2Binary + rs1Binary + funct3 + immLower + opcode;
}

// Function to generate U-Format machine code
string generateUFormatMachineCode(InstructionSet &instSet, const string &instructionLine) {
    stringstream ss(instructionLine);
    string instruction, rd, immStr;

    ss >> instruction >> rd >> immStr; // Extract instruction name, register, and immediate

    // Remove the excess whitespaces using the trim function
    instruction = trim(instruction);

    // Now remove the commas from the registers
    rd.erase(remove(rd.begin(), rd.end(), ','), rd.end());
    string opcode = instSet.opcodeMap[instruction];
    string rdBinary = instSet.registerMap[rd];

    int imm = stoi(immStr);
    string immBinary = bitset<20>(imm & 0xFFFFF).to_string();

    // Assemble machine code: immediate (20 bits) + rd + opcode
    return immBinary + rdBinary + opcode;
}

// Generate SB-Format Machine Code
string generateSBFormatMachineCode(InstructionSet &instSet, const string &instructionLine, int prog_counter) {
    stringstream ss(instructionLine);
    string instruction, rs1, rs2, label;
    ss >> instruction >> rs1 >> rs2 >> label;

    //remove the excess whitespaces using the trim function
    instruction = trim(instruction);

    //now remove the commas from the registers
    rs1.erase(remove(rs1.begin(), rs1.end(), ','), rs1.end());
    rs2.erase(remove(rs2.begin(), rs2.end(), ','), rs2.end());

    string opcode = instSet.opcodeMap[instruction];
    string funct3 = instSet.funct3Map[instruction];
    string rs1Binary = instSet.registerMap[rs1];
    string rs2Binary = instSet.registerMap[rs2];

    int imm = instSet.currentPC(label) - prog_counter;
    string immBinary = (imm < 0) ? explicitTwoComplement(imm, 12) : bitset<12>(imm).to_string();

    // Assemble machine code: imm[11:5] + rs2 + rs1 + funct3 + imm[4:0] + opcode
    return immBinary.substr(0, 7) + rs2Binary + rs1Binary + funct3 + immBinary.substr(7) + opcode;
}

// Generate UJ-Format Machine Code
string generateUJFormatMachineCode(InstructionSet &instSet, const string &instructionLine, int prog_counter) {
    stringstream ss(instructionLine);
    string instruction, rd, label;
    ss >> instruction >> rd >> label;

    //remove the excess whitespaces using the trim function
    instruction = trim(instruction);

    //now remove the commas from the registers
    rd.erase(remove(rd.begin(), rd.end(), ','), rd.end());

    string opcode = instSet.opcodeMap[instruction];
    string rdBinary = instSet.registerMap[rd];

    int offset = instSet.currentPC(label) - prog_counter;
    string immBinary = (offset < 0) ? explicitTwoComplement(offset, 20) : bitset<20>(offset).to_string();

    // Assemble machine code: imm[20] + imm[10:1] + imm[11] + imm[19:12] + rd + opcode
    return immBinary.substr(0, 1) + immBinary.substr(10, 10) + immBinary.substr(9, 1) + immBinary.substr(1, 8) + rdBinary + opcode;
}

// Function to determine instruction format
string getInstructionFormat(const string &opcode, const string &funct3 = "", const string &funct7 = "") {
    if (opcode == "0110011") return "R";  // R-Type
    if (opcode == "0010011" || opcode == "0000011" || opcode == "1100111") return "I"; // I-Type
    if (opcode == "0100011") return "S";  // S-Type
    if (opcode == "1100011") return "SB"; // SB-Type
    if (opcode == "0110111" || opcode == "0010111") return "U";  // U-Type
    if (opcode == "1101111") return "UJ"; // UJ-Type
    return "UNKNOWN";
}

// Function to extract instruction fields from machine code
InstructionFields extractFieldsFromMachineCode(const string& machineCode, const string& format, 
                                             const string& instruction, InstructionSet& instSet) {
    InstructionFields fields;
    
    // Extract common fields
    fields.opcode = instSet.opcodeMap[instruction];
    
    if (format != "U" && format != "UJ") {
        fields.funct3 = instSet.funct3Map[instruction];
    } else {
        fields.funct3 = "NULL";
    }
    
    if (format == "R") {
        fields.funct7 = instSet.funct7Map[instruction];
    } else {
        fields.funct7 = "NULL";
    }
    
    // Extract format-specific fields
    if (format == "R") {
        // R-Format: funct7[31:25] + rs2[24:20] + rs1[19:15] + funct3[14:12] + rd[11:7] + opcode[6:0]
        fields.rd = machineCode.substr(20, 5);
        fields.rs1 = machineCode.substr(12, 5);
        fields.rs2 = machineCode.substr(7, 5);
        fields.immediate = "NULL";
    } 
    else if (format == "I") {
        // I-Format: imm[31:20] + rs1[19:15] + funct3[14:12] + rd[11:7] + opcode[6:0]
        fields.rd = machineCode.substr(20, 5);
        fields.rs1 = machineCode.substr(12, 5);
        fields.rs2 = "NULL";
        fields.immediate = machineCode.substr(0, 12);
    } 
    else if (format == "S") {
        // S-Format: imm[31:25] + rs2[24:20] + rs1[19:15] + funct3[14:12] + imm[11:7] + opcode[6:0]
        fields.rd = "NULL";
        fields.rs1 = machineCode.substr(12, 5);
        fields.rs2 = machineCode.substr(7, 5);
        fields.immediate = machineCode.substr(0, 7) + machineCode.substr(20, 5);
    } 
    else if (format == "SB") {
        // SB-Format: imm[31:25] + rs2[24:20] + rs1[19:15] + funct3[14:12] + imm[11:7] + opcode[6:0]
        fields.rd = "NULL";
        fields.rs1 = machineCode.substr(12, 5);
        fields.rs2 = machineCode.substr(7, 5);
        fields.immediate = machineCode.substr(0, 7) + machineCode.substr(20, 5);
    } 
    else if (format == "U") {
        // U-Format: imm[31:12] + rd[11:7] + opcode[6:0]
        fields.rd = machineCode.substr(20, 5);
        fields.rs1 = "NULL";
        fields.rs2 = "NULL";
        fields.immediate = machineCode.substr(0, 20);
    } 
    else if (format == "UJ") {
        // UJ-Format: imm[31:12] + rd[11:7] + opcode[6:0]
        fields.rd = machineCode.substr(20, 5);
        fields.rs1 = "NULL";
        fields.rs2 = "NULL";
        fields.immediate = machineCode.substr(0, 20);
    }
    
    return fields;
}

// Add this function to generate a termination instruction
string generateTerminationCode() {
    // Using an illegal instruction opcode (all 1s) as termination marker
    // This will produce a 32-bit word of all 1s (0xFFFFFFFF)
    return string(32, '1');
}

// Helper function for second pass: trim whitespace
string trim2(const string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

void first_pass(const string& filename, InstructionSet& InstructionSet) {
    ifstream file(filename);
    string line;
    int temp_pc = 0;
    bool in_text_segment = false;
    bool in_data_segment = false;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line == ".text") {
            in_text_segment = true;
            in_data_segment = false;
            continue;
        } else if (line == ".data") {
            in_data_segment = true;
            in_text_segment = false;
            continue;
        }

        if (!in_text_segment) continue;

        size_t colon_pos = line.find(":");
        if (colon_pos != string::npos) {
            string label = trim(line.substr(0, colon_pos));
            InstructionSet.add_pair(label, temp_pc);
            continue;
        }
        
        // Only increment PC for actual instructions in text segment
        stringstream ss(line);
        string instruction;
        ss >> instruction;
        instruction = trim(instruction);
        
        if (InstructionSet.instructionFormats.find(instruction) != InstructionSet.instructionFormats.end()) {
            temp_pc += 4;
        }
    }
}

void second_pass(const string& filename, const string& output_filename, InstructionSet& instSet) {
    ifstream file(filename);
    ofstream output(output_filename);
    string line;
    int program_counter = 0;
    bool in_text_segment = false;
    bool in_data_segment = false;
    bool has_instructions = false;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        // Handle segment directives
        if (line == ".text") {
            in_text_segment = true;
            in_data_segment = false;
            continue;
        } else if (line == ".data") {
            in_data_segment = true;
            in_text_segment = false;
            continue;
        }

        // Skip data segment lines
        if (in_data_segment) continue;
        if (!in_text_segment) continue;

        // Skip label lines
        if (line.find(":") != string::npos) continue;
        
        // Skip non-instruction lines (comments or directives starting with .)
        if (line.empty() || line[0] == '#' || line[0] == '.') continue;
        
        // Skip the line with "lowerCase to upperCase" comment
        if (line.find("lowerCase to upperCase") != string::npos) continue;
        
        string instructionLine = line;
        stringstream ss(instructionLine);
        string instruction;
        ss >> instruction;
        instruction = trim(instruction);
        
        // Skip lines that don't contain valid instructions
        if (instSet.instructionFormats.find(instruction) == instSet.instructionFormats.end()) {
            continue;
        }
        
        has_instructions = true;
        
        // Machine code generation
        string machineBinary;
        string format = instSet.instructionFormats[instruction];
        
        if (format == "R") {
            machineBinary = generateRFormatMachineCode(instSet, instructionLine);
        } else if (format == "I") {
            machineBinary = generateIFormatMachineCode(instSet, instructionLine);
        } else if (format == "S") {
            machineBinary = generateSFormatMachineCode(instSet, instructionLine);
        } else if (format == "SB") {
            machineBinary = generateSBFormatMachineCode(instSet, instructionLine, program_counter);
        } else if (format == "U") {
            machineBinary = generateUFormatMachineCode(instSet, instructionLine);
        } else if (format == "UJ") {
            machineBinary = generateUJFormatMachineCode(instSet, instructionLine, program_counter);
        }
        
        // Add error checking
        if (machineBinary.find("ERROR") != string::npos) {
            output << "0x" << hex << program_counter << " 0x" << setw(8) << setfill('0') << 0 
                << " , " << instructionLine << " # " << machineBinary << endl;
        } else {
            // Convert binary machine code to hex
            unsigned long machineCode = 0;
            if (!machineBinary.empty()) {
                machineCode = bitset<32>(machineBinary).to_ulong();
            }
            
            // Generate the comment showing the breakdown of fields
            string comment = " # ";
            
            // Extract field information from the machine code
            InstructionFields fields = extractFieldsFromMachineCode(machineBinary, format, instruction, instSet);
            
            // Build the detailed comment
            comment += fields.opcode + "-" + fields.funct3 + "-" + fields.funct7 + "-";
            
            // Add register information
            comment += fields.rd + "-" + fields.rs1 + "-" + fields.rs2;
            
            // Add immediate if applicable
            if (fields.immediate != "NULL") {
                comment += "-" + fields.immediate;
            } else {
                comment += "-NULL";
            }
            
            // Format output line
            output << "0x" << hex << program_counter << " 0x" << setw(8) << setfill('0') << hex << machineCode 
                << " , " << instructionLine << comment << endl;
        }
        
        program_counter += 4;
    }
    
    // Add termination code after processing all instructions
    if (has_instructions && in_text_segment) {
        string terminationBinary = generateTerminationCode();
        unsigned long terminationCode = bitset<32>(terminationBinary).to_ulong();
        
        output << "0x" << hex << program_counter << " 0x" << setw(8) << setfill('0') << hex << terminationCode 
               << " , " << "TERMINATE" << " # End of text segment marker" << endl;
    }
}

#endif // TEXT_SEGMENT_H