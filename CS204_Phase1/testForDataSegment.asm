# Test data segment for all supported directives
.data
    # Test labels
    test_byte:
        .byte 0x12          # Hexadecimal value
        .byte 42            # Decimal value
        .byte 'A'           # Character literal
        .byte 170    # Binary value (170 in decimal)
    
    test_half:
        .half 0xABCD        # Hexadecimal value
        .half 12345         # Decimal value
        .half 'Z'           # Character literal
    
    test_word:
        .word 0x12345678    # Hexadecimal value
        .word 305419896     # Decimal value (0x12345678 in hex)
        .word 'Q'           # Character literal
    
    test_dword:
        .dword 0x1234567890ABCDEF  # Large hexadecimal value
        .dword 987654321           # Large decimal value
    
    # Test multiple values on the same line
    multi_byte: .byte 1, 2, 3, 4, 5
    multi_half: .half 100, 200, 300
    multi_word: .word 1000, 2000, 3000
    
    # Test string literals
    test_asciz: .asciz "Hello, World!"
    test_asciiz: .asciiz "This is a null-terminated string"
    
    # Test empty string
    empty_string: .asciz ""
    
    # Test special characters in strings
    special_chars: .asciz "Newline\nTab Backslash\\"
    
    # Test maximum values for each type
    max_values:
        .byte 0xFF          # Max 8-bit value (255)
        .half 0xFFFF        # Max 16-bit value (65535)
        .word 0xFFFFFFFF    # Max 32-bit value (4294967295)
        #.dword 0xFFFFFFFFFFFFFFFF  # Max 64-bit value
    
    # Test alignment demonstration
    align_test:
        .byte 0x01
        .word 0x12345678    # Should be properly aligned in memory

# This marks the end of the data segment
# Test text segment for all RISC-V instruction formats
.text
    # R-format instructions: (rd, rs1, rs2)
    # add rd, rs1, rs2
    add x1, x2, x3       # x1 = x2 + x3
    and x4, x5, x6       # x4 = x5 & x6
    or  x7, x8, x9       # x7 = x8 | x9
    sll x10, x11, x12    # x10 = x11 << x12
    slt x13, x14, x15    # x13 = (x14 < x15) ? 1 : 0
    sra x16, x17, x18    # x16 = x17 >> x18 (arithmetic)
    srl x19, x20, x21    # x19 = x20 >> x21 (logical)
    sub x22, x23, x24    # x22 = x23 - x24
    xor x25, x26, x27    # x25 = x26 ^ x27
    mul x28, x29, x30    # x28 = x29 * x30
    div x31, x1, x2      # x31 = x1 / x2
    rem x3, x4, x5       # x3 = x4 % x5

    # I-format instructions: (rd, rs1, imm)
    # addi rd, rs1, immediate
    addi x6, x7, 100     # x6 = x7 + 100
    andi x8, x9, 0xFF    # x8 = x9 & 0xFF
    ori  x10 x11 0x0F  # x10 = x11 | 0x0F
    
    # Load instructions (I-format)
    lb x12, 8(x13)       # Load byte from [x13+8] to x12
    lh x14, 16(x15)      # Load half-word from [x15+16] to x14
    lw x16, 24(x17)      # Load word from [x17+24] to x16
    ld x18, 32 x19      # Load double-word from [x19+32] to x18
    
    # Jump instruction (I-format)
    jalr x20, x21, 40    # Jump to [x21+40], store return address in x20

    # S-format instructions: (rs1, rs2, imm)
    # sb rs2, imm(rs1)
    sb x22, 8(x23)       # Store byte from x22 to [x23+8]
    sh x24, 16(x25)      # Store half-word from x24 to [x25+16]
    sw x26, 24(x27)      # Store word from x26 to [x27+24]
    sd x28, 32(x29)      # Store double-word from x28 to [x29+32]

    # SB-format instructions: (rs1, rs2, imm)
    # Branch instructions
loop:
    beq x30, x31, loop   # Branch if x30 == x31 to loop
    bne x1, x2, target1  # Branch if x1 != x2 to target1
    bge x3, x4, target2  # Branch if x3 >= x4 to target2
    blt x5, x6, target3  # Branch if x5 < x6 to target3

    # U-format instructions: (rd, imm)
    # lui rd, imm
    lui x7, 0x12345      # x7 = 0x12345000
    auipc x8, 0x67890    # x8 = PC + 0x67890000

    # UJ-format instructions: (rd, imm)
    # jal rd, imm
    jal x9, target4      # Jump to target4, store return address in x9

    # More code with branch targets
target1:
    add x10, x11, x12    # Target for bne instruction
target2:
    sub x13, x14, x15    # Target for bge instruction
target3:
    or x16, x17, x18     # Target for blt instruction
target4:
    and x19, x20, x21    # Target for jal instruction

exit:
    # Terminate program
    addi x0, x0, 0       # NOP instruction