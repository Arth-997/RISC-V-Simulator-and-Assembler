#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <algorithm>

using namespace std;

// Helper function: store an integer into the memory vector in little endian format.
void storeIntegerLittleEndian(vector<uint8_t>& memory, uint64_t value, size_t byteCount) {
    for (size_t i = 0; i < byteCount; i++) {
        memory.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }
}

// Helper function: try to convert a token to an integer. 
// If conversion fails and token is a single character, use its ASCII value.
uint64_t parseValue(const string &token) {

    // handle character literals like .word 'A'
    if (token[0] == '\'') {
        if (token.size() == 3 && token[2] == '\'') {
            return static_cast<uint64_t>(token[1]);
        }
        cerr << "Error: invalid character literal " << token << endl;
        exit(1);
    }

    try {
        size_t pos = 0;
        int base = 10;
        if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            base = 16;
        }
        return stoull(token, &pos, base);
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
        maxVal = numeric_limits<uint64_t>::max();
    else
        return false;
    
    return value <= maxVal;
}

// Function to parse the .data segment from the input stream and fill the memory vector.
void parseDataSegment(istream& in, vector<uint8_t>& memory) {
    string line;
    // Process until the end of the file or until another segment is encountered (e.g. .text)
    while (getline(in, line)) {
        // Stop parsing if a new section begins (e.g. .text)
        if (line.find(".text") != string::npos)
            break;
        
        // Skip blank lines
        if (line.find_first_not_of(" \t") == string::npos)
            continue;

        // Skip comments
        if(line.find("#") != string::npos) {
            line = line.substr(0, line.find("#"));
        }
        // skip commas 
        line.erase(remove(line.begin(), line.end(), ','), line.end());

        istringstream iss(line);
        string token;
        string currentDirective = "";
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
                // Special handling for .asciz since the string literal may contain spaces.
                if (currentDirective == ".asciz") {
                    size_t quotePos = line.find('"');
                    if (quotePos != string::npos) {
                        size_t endQuotePos = line.find('"', quotePos + 1);
                        if (endQuotePos != string::npos) {
                            string strLiteral = line.substr(quotePos + 1, endQuotePos - quotePos - 1);
                            // Store each character as a byte.
                            for (char c : strLiteral) {
                                memory.push_back(static_cast<uint8_t>(c));
                            }
                            // Append the null terminator.
                            memory.push_back(0x00);
                        }
                    }
                    // No further tokens on this line are processed.
                    break;
                }
                continue; // Proceed to the next token (operands follow the directive).
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
                
                // Check if the value fits in the given byteCount.
                if (!valueFits(value, byteCount)) {
                    cerr << "Error: value " << value << " does not fit in directive " << currentDirective << endl;
                    exit(1);
                }
                
                // Store the value in little endian order.
                storeIntegerLittleEndian(memory, value, byteCount);
            }
        }
    }
}

int main() {
    ifstream infile("test.asm");
    if (!infile) {
        cerr << "Error opening file 'input'" << endl;
        return 1;
    }

    string line;
    // Read until the .data segment is found.
    while (getline(infile, line)) {
        if (line.find(".data") != string::npos)
            break;
    }
    
    vector<uint8_t> memory;
    parseDataSegment(infile, memory);
    
    // Print the memory array in the format: [0x__, 0x__, ...]
    //cout << "[";
    int memoryPos = 0;
    for (size_t i = 0; i < memory.size(); i++) {
        // pring memory location
        cout << "0x" << std::hex << memoryPos << ": ";
        memoryPos++;
        cout << "0x";
        // Print in uppercase hex with at least two digits.
        if (memory[i] < 0x10)
            cout << "0";
        cout << std::hex << std::uppercase << static_cast<int>(memory[i]);
        cout<<endl;
    }
    //cout << "]" << endl;
    
    return 0;
}
