#ifndef DATA_SEGMENT_H
#define DATA_SEGMENT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <limits>  // Added this include for std::numeric_limits

// Helper function: store an integer into the memory vector in little endian format.
void storeIntegerLittleEndian(std::vector<uint8_t>& memory, uint64_t value, size_t byteCount) {
    for (size_t i = 0; i < byteCount; i++) {
        memory.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }
}

// Helper function: try to convert a token to an integer. 
// If conversion fails and token is a single character, use its ASCII value.
uint64_t parseValue(const std::string &token) {
    // handle character literals like .word 'A'
    if (token[0] == '\'') {
        if (token.size() == 3 && token[2] == '\'') {
            return static_cast<uint64_t>(token[1]);
        }
        std::cerr << "Error: invalid character literal " << token << std::endl;
        exit(1);
    }

    try {
        size_t pos = 0;
        int base = 10;
        if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            base = 16;
        }
        return std::stoull(token, &pos, base);
    } catch (...) {
        // If not a valid number and token is a single character, return its ASCII value.
        if (token.size() == 1)
            return static_cast<uint64_t>(token[0]);
        // Otherwise, fallback to the first character.
        return static_cast<uint64_t>(token[0]);
    }
}

// Check whether the given value fits in the given number of bytes.
bool valueFits(uint64_t value, size_t byteCount) {
    uint64_t maxVal = 0;
    if (byteCount == 1)
        maxVal = 0xFF; // 255
    else if (byteCount == 2)
        maxVal = 0xFFFF; // 65535
    else if (byteCount == 4)
        maxVal = 0xFFFFFFFF; // 4294967295
    else if (byteCount == 8)
        maxVal = std::numeric_limits<uint64_t>::max();
    else
        return false;
    
    return value <= maxVal;
}

// Modified function to parse the .data segment.
// When a line with ".text" is encountered, the stream is rewound so that the directive
// is not consumed.
void parseDataSegment(std::istream& in, std::vector<uint8_t>& memory) {
    std::string line;
    while (true) {
        // Save current stream position.
        std::streampos pos = in.tellg();
        if (!std::getline(in, line))
            break;
        // If a new section (e.g. ".text") is encountered, rewind and break.
        if (line.find(".text") != std::string::npos) {
            in.seekg(pos);
            break;
        }
        
        // Skip blank lines
        if (line.find_first_not_of(" \t") == std::string::npos)
            continue;

        // Remove comments (anything following a '#')
        if(line.find("#") != std::string::npos) {
            line = line.substr(0, line.find("#"));
        }
        
        // Remove commas
        std::string tempLine = line;
        tempLine.erase(std::remove(tempLine.begin(), tempLine.end(), ','), tempLine.end());

        std::istringstream iss(tempLine);
        std::string token;
        std::string currentDirective = "";
        bool directiveActive = false;
        
        // Tokenize the line
        while (iss >> token) {
            // Remove any trailing commas
            if (!token.empty() && token.back() == ',') {
                token.pop_back();
            }
            // Skip label tokens (ending with ':')
            if (token.back() == ':')
                continue;
            
            // Check if the token is a directive (starts with '.')
            if (token[0] == '.') {
                currentDirective = token;
                directiveActive = true;
                // Special handling for .asciz/.asciiz: process the entire string literal.
                if (currentDirective == ".asciz" || currentDirective == ".asciiz") {
                    size_t quotePos = line.find('"');
                    if (quotePos != std::string::npos) {
                        size_t endQuotePos = line.find('"', quotePos + 1);
                        if (endQuotePos != std::string::npos) {
                            std::string strLiteral = line.substr(quotePos + 1, endQuotePos - quotePos - 1);
                            // Store each character as a byte.
                            for (char c : strLiteral) {
                                memory.push_back(static_cast<uint8_t>(c));
                            }
                            // Append the null terminator.
                            memory.push_back(0x00);
                        }
                    }
                    // Stop processing further tokens on this line.
                    break;
                }
                continue;
            }
            
            // If a directive is active, treat tokens as operands.
            if (directiveActive) {
                size_t byteCount = 0;
                if (currentDirective == ".byte")
                    byteCount = 1;
                else if (currentDirective == ".half")
                    byteCount = 2;
                else if (currentDirective == ".word")
                    byteCount = 4;
                else if (currentDirective == ".dword")
                    byteCount = 8;
                else {
                    // Unknown directive: skip.
                    continue;
                }
                
                // Parse the token value.
                uint64_t value = parseValue(token);
                
                // Check if the value fits in the given byte count.
                if (!valueFits(value, byteCount)) {
                    std::cerr << "Error: value " << value << " does not fit in directive " 
                              << currentDirective << std::endl;
                    exit(1);
                }
                
                // Store the value in little endian order.
                storeIntegerLittleEndian(memory, value, byteCount);
            }
        }
    }
}

#endif // DATA_SEGMENT_H
