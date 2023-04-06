.globl fpga_data
fpga_data:
    .incbin "src/fpga_data.bin"

.globl fpga_data_len
fpga_data_len:
    .word 122156, 0, 0, 0

.ascii "A golova suselikov puskaet. Dva-dva u Natalicha!"
