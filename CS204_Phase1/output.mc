0x0 0x003100b3 , add x1 x2 x3 # 0110011-000-0000000-00001-00010-00011-NULL # x1 = x2 + x3
0x4 0x0062f233 , and x4 x5 x6 # 0110011-111-0000000-00100-00101-00110-NULL # x4 = x5 & x6
0x8 0x009463b3 , or x7 x8 x9 # 0110011-110-0000000-00111-01000-01001-NULL # x7 = x8 | x9
0xc 0x00c59533 , sll xa xb xc # 0110011-001-0000000-01010-01011-01100-NULL # x10 = x11 << x12
0x10 0x00f726b3 , slt xd xe xf # 0110011-010-0000000-01101-01110-01111-NULL # x13 = (x14 < x15) ? 1 : 0
0x14 0x4128d833 , sra x10 x11 x12 # 0110011-101-0100000-10000-10001-10010-NULL # x16 = x17 >> x18 (arithmetic)
0x18 0x015a59b3 , srl x13 x14 x15 # 0110011-101-0000000-10011-10100-10101-NULL # x19 = x20 >> x21 (logical)
0x1c 0x418b8b33 , sub x16 x17 x18 # 0110011-000-0100000-10110-10111-11000-NULL # x22 = x23 - x24
0x20 0x01bd4cb3 , xor x19 x1a x1b # 0110011-100-0000000-11001-11010-11011-NULL # x25 = x26 ^ x27
0x24 0x03ee8e33 , mul x1c x1d x1e # 0110011-000-0000001-11100-11101-11110-NULL # x28 = x29 * x30
0x28 0x0220cfb3 , div x1f x1 x2 # 0110011-100-0000001-11111-00001-00010-NULL # x31 = x1 / x2
0x2c 0x025261b3 , rem x3 x4 x5 # 0110011-110-0000001-00011-00100-00101-NULL # x3 = x4 % x5
0x30 0x06438313 , addi x6 x7 64 # 0010011-000-NULL-00110-00111-NULL-000001100100 # x6 = x7 + 100
0x34 0x0ff4f413 , andi x8 x9 ff # 0010011-111-NULL-01000-01001-NULL-000011111111 # x8 = x9 & 0xFF
0x38 0x00f5e513 , ori xa xb f # 0010011-110-NULL-01010-01011-NULL-000000001111 # x10 = x11 | 0x0F
0x3c 0x00868603 , lb xc 8 xd # 0000011-000-NULL-01100-01101-NULL-000000001000 # Load byte from [x13+8] to x12
0x40 0x01079703 , lh xe 10 xf # 0000011-001-NULL-01110-01111-NULL-000000010000 # Load half-word from [x15+16] to x14
0x44 0x0188a803 , lw x10 18 x11 # 0000011-010-NULL-10000-10001-NULL-000000011000 # Load word from [x17+24] to x16
0x48 0x0209b903 , ld x12 x13 20 # 0000011-011-NULL-10010-10011-NULL-000000100000 # Load double-word from [x19+32] to x18
0x4c 0x028a8a67 , jalr x14 x15 28 # 1100111-000-NULL-10100-10101-NULL-000000101000 # Jump to [x21+40], store return address in x20
0x50 0x016b8423 , sb x16 8 x17 # 0100011-000-NULL-NULL-10111-10110-000000001000 # Store byte from x22 to [x23+8]
0x54 0x018c9823 , sh x18 10 x19 # 0100011-001-NULL-NULL-11001-11000-000000010000 # Store half-word from x24 to [x25+16]
0x58 0x01adac23 , sw x1a 18 x1b # 0100011-010-NULL-NULL-11011-11010-000000011000 # Store word from x26 to [x27+24]
0x5c 0x03ceb023 , sd x1c 20 x1d # 0100011-011-NULL-NULL-11101-11100-000000100000 # Store double-word from x28 to [x29+32]
0x60 0xffff0e63 , beq x1e x1f  # 1100011-000-NULL-NULL-11110-11111-111111111100 # Branch if x30 == x31 to loop
0x64 0x00209a63 , bne x1 x2 target1 # 1100011-001-NULL-NULL-00001-00010-000000010100 # Branch if x1 != x2 to target1
0x68 0x0041da63 , bge x3 x4 target2 # 1100011-101-NULL-NULL-00011-00100-000000010100 # Branch if x3 >= x4 to target2
0x6c 0x0062ca63 , blt x5 x6 target3 # 1100011-100-NULL-NULL-00101-00110-000000010100 # Branch if x5 < x6 to target3
0x70 0x000003b7 , lui x7 0x0 # 0110111-NULL-NULL-00111-NULL-NULL-00000000000000000000 # x7 = 0x12345000
0x74 0x00000417 , auipc x8 0x0 # 0010111-NULL-NULL-01000-NULL-NULL-00000000000000000000 # x8 = PC + 0x67890000
0x78 0x018004ef , jal x9  # 1101111-NULL-NULL-01001-NULL-NULL-00000001100000000000 # Jump to target4, store return address in x9
0x7c 0x00c58533 , add xa xb xc # 0110011-000-0000000-01010-01011-01100-NULL # Target for bne instruction
0x80 0x40f706b3 , sub xd xe xf # 0110011-000-0100000-01101-01110-01111-NULL # Target for bge instruction
0x84 0x0128e833 , or x10 x11 x12 # 0110011-110-0000000-10000-10001-10010-NULL # Target for blt instruction
0x88 0x015a79b3 , and x13 x14 x15 # 0110011-111-0000000-10011-10100-10101-NULL # Target for jal instruction
0x8c 0x00000013 , addi x0 x0 0 # 0010011-000-NULL-00000-00000-NULL-000000000000 # NOP instruction
0x90 0xffffffff , TERMINATE # End of text segment marker


;; DATA SEGMENT (starting at address 0x10000000)
Address: 10000000 | Data: 0x12 0x2a 0x41 0xaa 
Address: 10000004 | Data: 0xcd 0xab 0x39 0x30 
Address: 10000008 | Data: 0x5a 0x00 0x78 0x56 
Address: 1000000c | Data: 0x34 0x12 0x78 0x56 
Address: 10000010 | Data: 0x34 0x12 0x51 0x00 
Address: 10000014 | Data: 0x00 0x00 0xef 0xcd 
Address: 10000018 | Data: 0xab 0x90 0x78 0x56 
Address: 1000001c | Data: 0x34 0x12 0xb1 0x68 
Address: 10000020 | Data: 0xde 0x3a 0x00 0x00 
Address: 10000024 | Data: 0x00 0x00 0x01 0x02 
Address: 10000028 | Data: 0x03 0x04 0x05 0x64 
Address: 1000002c | Data: 0x00 0xc8 0x00 0x2c 
Address: 10000030 | Data: 0x01 0xe8 0x03 0x00 
Address: 10000034 | Data: 0x00 0xd0 0x07 0x00 
Address: 10000038 | Data: 0x00 0xb8 0x0b 0x00 
Address: 1000003c | Data: 0x00 0x48 0x65 0x6c 
Address: 10000040 | Data: 0x6c 0x6f 0x2c 0x20 
Address: 10000044 | Data: 0x57 0x6f 0x72 0x6c 
Address: 10000048 | Data: 0x64 0x21 0x00 0x54 
Address: 1000004c | Data: 0x68 0x69 0x73 0x20 
Address: 10000050 | Data: 0x69 0x73 0x20 0x61 
Address: 10000054 | Data: 0x20 0x6e 0x75 0x6c 
Address: 10000058 | Data: 0x6c 0x2d 0x74 0x65 
Address: 1000005c | Data: 0x72 0x6d 0x69 0x6e 
Address: 10000060 | Data: 0x61 0x74 0x65 0x64 
Address: 10000064 | Data: 0x20 0x73 0x74 0x72 
Address: 10000068 | Data: 0x69 0x6e 0x67 0x00 
Address: 1000006c | Data: 0x00 0x4e 0x65 0x77 
Address: 10000070 | Data: 0x6c 0x69 0x6e 0x65 
Address: 10000074 | Data: 0x5c 0x6e 0x54 0x61 
Address: 10000078 | Data: 0x62 0x20 0x42 0x61 
Address: 1000007c | Data: 0x63 0x6b 0x73 0x6c 
Address: 10000080 | Data: 0x61 0x73 0x68 0x5c 
Address: 10000084 | Data: 0x5c 0x00 0xff 0xff 
Address: 10000088 | Data: 0xff 0xff 0xff 0xff 
Address: 1000008c | Data: 0xff 0x01 0x78 0x56 
Address: 10000090 | Data: 0x34 0x12 
