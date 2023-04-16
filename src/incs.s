.align 4
.globl font
font:
.incbin "src/font64.bin"

.align 4
.globl mcn_data
mcn_data:
.incbin "src/mcn_data.rbf"

.align 4
.word -1

.align 4
.word 12

.align 4
.globl mcn_data_len
mcn_data_len:
.word 122156

.align 4
.ascii "A golova suselikov puskaet. Dva-dva u Natalicha!"
