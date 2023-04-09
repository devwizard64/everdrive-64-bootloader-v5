.align 4

.globl fpga_data
fpga_data:
.incbin "src/fpga_data.rbf"
.balign 16

.word -1
.balign 16

.word 12
.balign 16

.globl fpga_data_len
fpga_data_len:
.word 122156
.balign 16

.ascii "A golova suselikov puskaet. Dva-dva u Natalicha!"
.balign 16
