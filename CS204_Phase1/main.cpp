#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <iomanip>
#include <sstream>

#include "DATA_SEGMENT.h" // Include the data segment header
#include "TEXT_SEGMENT.h" // Assuming this header is already present

// Helper function: trim leading and trailing whitespace
std::string trim1(const std::string &str) {
    const std::string whitespace = " \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    size_t end = str.find_last_not_of(whitespace);
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

void proecess_file1(const std::string& filename, const std::string& output_filename) {
    // Initialize instruction set
    InstructionSet instSet;
    instSet.initialize();

    // First pass to collect labels
    first_pass(filename, instSet);
    
    // Open input and output files
    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Error opening file '" << filename << "'" << std::endl;
        exit(1);
    }
    
    std::ofstream outfile(output_filename);
    if (!outfile) {
        std::cerr << "Error creating output file '" << output_filename << "'" << std::endl;
        exit(1);
    }
    
    // Vectors to store data and text segments
    std::vector<uint8_t> dataSegment;
    std::vector<std::string> textSegment;
    std::vector<std::string> formattedInstructions;
    
    std::string line;
    bool in_text_segment = false;
    bool in_data_segment = false;
    int prog_counter = 0;
    
    // Process the file line by line
    while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) continue;
        if (line.find('#') == 0) continue;
        
        // Remove comments if they exist
        std::string comment = "";
        if (line.find('#') != std::string::npos) {
            comment = line.substr(line.find('#'));
            line = line.substr(0, line.find('#'));
        }
        
        line = trim1(line);
        
        // Check for segment directives
        if (line == ".text") {
            in_text_segment = true;
            in_data_segment = false;
            continue;
        } else if (line == ".data") {
            in_data_segment = true;
            in_text_segment = false;
            
            // Parse the data segment.
            parseDataSegment(infile, dataSegment);
            continue;
        }
        
        // Process instructions in the text segment.
        if (in_text_segment) {
            // Skip labels in the second pass.
            if (line.find(":") != std::string::npos)
                continue;
            
            // Extract instruction name.
            std::stringstream ss(line);
            std::string instruction;
            ss >> instruction;
            instruction = trim1(instruction);
            
            // Only process valid instructions.
            if (instSet.instructionFormats.find(instruction) != instSet.instructionFormats.end()) {
                std::string machineCode;
                std::string format = instSet.instructionFormats[instruction];
                
                // Generate appropriate machine code based on instruction format.
                if (format == "R") {
                    machineCode = generateRFormatMachineCode(instSet, line);
                } else if (format == "I") {
                    machineCode = generateIFormatMachineCode(instSet, line);
                } else if (format == "S") {
                    machineCode = generateSFormatMachineCode(instSet, line);
                } else if (format == "SB") {
                    machineCode = generateSBFormatMachineCode(instSet, line, prog_counter);
                } else if (format == "U") {
                    machineCode = generateUFormatMachineCode(instSet, line);
                } else if (format == "UJ") {
                    machineCode = generateUJFormatMachineCode(instSet, line, prog_counter);
                }
                
                if (machineCode.find("ERROR") == std::string::npos) {
                    textSegment.push_back(machineCode);
                    
                    // Get hex representation of binary machine code
                    uint32_t binary_instruction = std::stoul(machineCode, nullptr, 2);
                    std::stringstream hex_instruction;
                    hex_instruction << "0x" << std::setw(8) << std::setfill('0') << std::hex << binary_instruction;
                    
                    // Format instruction for output
                    InstructionFields fields = extractFieldsFromMachineCode(machineCode, format, instruction, instSet);
                    std::stringstream formatted_instruction;
                    
                    // Create the formatted instruction line
                    formatted_instruction << "0x" << std::hex << prog_counter << " " 
                                         << hex_instruction.str() << " , " 
                                         << instruction;
                    
                    // Append operands based on the instruction format
                    if (format == "R") {
                        formatted_instruction << " x" << std::stoi(fields.rd, nullptr, 2) << " x" << std::stoi(fields.rs1, nullptr, 2) << " x" << std::stoi(fields.rs2, nullptr, 2);
                    } else if (format == "I") {
                        int imm_value = std::stoi(fields.immediate, nullptr, 2);
                        if (instruction == "lw" || instruction == "lb" || instruction == "lh" || instruction == "lbu" || instruction == "lhu") {
                            formatted_instruction << " x" << std::stoi(fields.rd, nullptr, 2) << " " << imm_value << " x" << std::stoi(fields.rs1, nullptr, 2);
                        } else {
                            formatted_instruction << " x" << std::stoi(fields.rd, nullptr, 2) << " x" << std::stoi(fields.rs1, nullptr, 2) << " " << imm_value;
                        }
                    } else if (format == "S") {
                        int imm_value = std::stoi(fields.immediate, nullptr, 2);
                        formatted_instruction << " x" << std::stoi(fields.rs2, nullptr, 2) << " " << imm_value << " x" << std::stoi(fields.rs1, nullptr, 2);
                    } else if (format == "SB") {
                        formatted_instruction << " x" << std::stoi(fields.rs1, nullptr, 2) << " x" << std::stoi(fields.rs2, nullptr, 2) << " ";
                        
                        // Find the label for this branch instruction from the label map
                        int target_address = prog_counter + std::stoi(fields.immediate, nullptr, 2);
                        for (const auto& label_pair : instSet.labelMap) {
                            if (label_pair.second == target_address) {
                                formatted_instruction << label_pair.first;
                                break;
                            }
                        }
                    } else if (format == "U") {
                        formatted_instruction << " x" << std::stoi(fields.rd, nullptr, 2) << " 0x" << std::hex << std::stoul(fields.immediate, nullptr, 2);
                    } else if (format == "UJ") {
                        formatted_instruction << " x" << std::stoi(fields.rd, nullptr, 2) << " ";
                        
                        // Find the label for this jump instruction from the label map
                        int target_address = prog_counter + std::stoi(fields.immediate, nullptr, 2);
                        for (const auto& label_pair : instSet.labelMap) {
                            if (label_pair.second == target_address) {
                                formatted_instruction << label_pair.first;
                                break;
                            }
                        }
                    }
                    
                    // Add comments including the machine code details
                    formatted_instruction << " # " << fields.opcode << "-";
                    
                    formatted_instruction << (fields.funct3 != "NULL" ? fields.funct3 : "NULL") << "-";
                    formatted_instruction << (fields.funct7 != "NULL" ? fields.funct7 : "NULL") << "-";
                    formatted_instruction << (fields.rd != "NULL" ? fields.rd : "NULL") << "-";
                    formatted_instruction << (fields.rs1 != "NULL" ? fields.rs1 : "NULL") << "-";
                    formatted_instruction << (fields.rs2 != "NULL" ? fields.rs2 : "NULL") << "-";
                    formatted_instruction << (fields.immediate != "NULL" ? fields.immediate : "NULL");
                    
                    // Add original comment if it exists
                    if (!comment.empty()) {
                        formatted_instruction << " " << comment;
                    }
                    
                    formattedInstructions.push_back(formatted_instruction.str());
                    prog_counter += 4;  // Each instruction is 4 bytes.
                } else {
                    std::cerr << "Error generating machine code for: " << line << std::endl;
                    std::cerr << machineCode << std::endl;
                }
            }
        }
    }
    
    // Add termination code to text segment.
    textSegment.push_back(generateTerminationCode());
    
    // Format the termination instruction
    uint32_t termination_instruction = 0xFFFFFFFF;
    std::stringstream formatted_termination;
    formatted_termination << "0x" << std::hex << prog_counter << " 0x" 
                          << std::setw(8) << std::setfill('0') << std::hex << termination_instruction 
                          << " , TERMINATE # End of text segment marker";
    formattedInstructions.push_back(formatted_termination.str());
    
    // Write formatted text segment to the output file
    for (const auto& formatted_instr : formattedInstructions) {
        outfile << formatted_instr << std::endl;
    }
    
    // Write the data segment to the output file
    if (!dataSegment.empty()) {
        outfile << "\n\n;; DATA SEGMENT (starting at address 0x10000000)\n";
        uint32_t dataAddress = 0x10000000;
        
        for (size_t i = 0; i < dataSegment.size(); i++) {
            // Print address at the beginning of each line (every 4 bytes)
            if (i % 4 == 0) {
                outfile << "Address: " << std::setw(8) << std::setfill('0') << std::hex << dataAddress << " | Data: ";
            }
            
            // Print the byte in hex
            outfile << "0x" << std::setw(2) << std::setfill('0') << std::hex 
                  << static_cast<int>(dataSegment[i]) << " ";
            
            // End the line after every 4 bytes
            if ((i + 1) % 4 == 0 || i == dataSegment.size() - 1) {
                outfile << std::endl;
                dataAddress += 4;
            }
        }
    }
    
    // Write the binary output to a separate binary file (.bin)
    std::string binary_output_filename = output_filename + ".bin";
    std::ofstream binfile(binary_output_filename, std::ios::binary);
    
    if (!binfile) {
        std::cerr << "Error creating binary output file '" << binary_output_filename << "'" << std::endl;
        exit(1);
    }
    
    // Write data segment size (4 bytes)
    uint32_t dataSize = dataSegment.size();
    binfile.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
    
    // Write data segment
    if (!dataSegment.empty()) {
        binfile.write(reinterpret_cast<const char*>(dataSegment.data()), dataSegment.size());
    }
    
    // Write text segment
    for (const auto& instruction : textSegment) {
        // Convert binary string to actual binary.
        uint32_t binary_instruction = std::stoul(instruction, nullptr, 2);
        binfile.write(reinterpret_cast<const char*>(&binary_instruction), sizeof(binary_instruction));
    }
    
    // Close files.
    infile.close();
    outfile.close();
    binfile.close();
    
    std::cout << "Assembly completed successfully!" << std::endl;
    std::cout << "Data segment size: " << dataSegment.size() << " bytes" << std::endl;
    std::cout << "Text segment size: " << textSegment.size() * 4 << " bytes (" << textSegment.size() << " instructions)" << std::endl;
    std::cout << "Output written to: " << output_filename << " and " << binary_output_filename << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>" << std::endl;
        return 1;
    }
    
    std::string input_filename = argv[1];
    std::string output_filename = argv[2];
    
    proecess_file1(input_filename, output_filename);
    
    return 0;
}