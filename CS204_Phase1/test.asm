.data
variable1: .word 123
string2: .asciiz "This is a test"
array1: .half 10, 20, 30, 40, 50
string1: .asciiz "Hello, World!"
array2: .byte 1, 2, 3, 4, 5, '1'
variable4: .byte 'a', 'b', 'c', 1, 2, 3
array3: .half 100, 200, 300, 400, 500
variable3: .word 999
array4: .word 55, 66, 77, 88, 99
array5: .word 11, 22, 33, 44, 55
string4: .asciiz "Welcome to RISC-V"
array6: .byte 6, 7, 8, 9, 10
variable5: .word 987
array7: .half 333, 444, 555, 666, 777

.text
add x2, x3, x4
andi x5, x6, 10
or x7, x8, x9
sll x10, x11, x3
slt x12, x13, x14
sra x15, x16, x2
srl x17, x18, x4
sub x19, x20, x21
xor x22, x23, x24
mul x25, x26, x27
div x28, x29, x30
rem x31, x1, x2

label1:
label2:
label3:
sb x2, 8(x3)
sw x4, 4(x5)
sd x6, 0(x7)
sh x8, 12(x9)

beq x10, x11, label1
bne x12, x13, label2
bge x14, x15, label3
blt x16, x17, label4

auipc x18, 0x1000
lui x19, 0x2000
jal x20, label5

lb x21, 16(x22)
ld x23, 0(x24)

sb x25, 4(x26)
label4:
lw x27, 8(x28)

lui x29, 0x3000
auipc x30, 0x4000
label5:
jalr x31, 0(x1)