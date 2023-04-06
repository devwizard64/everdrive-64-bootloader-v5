ROOTDIR = tools/mips64
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-
CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
LDFLAGS = -L$(ROOTDIR)/mips64-elf/lib
CFLAGS = -mtune=vr4300 -march=vr4300 -std=gnu99 -I$(ROOTDIR)/mips64-elf/include -Iinclude -Wall
ASFLAGS = -mtune=vr4300 -march=vr4300

PROG_NAME = firmware

FIRM_OBJ := \
	build/src/bios.o \
	build/src/main.o \
	build/src/usb.o \
	build/src/disk.o \
	build/src/fat.o \
	build/src/sys.o \
	build/src/font.o \
	build/src/fpga_data.o \
	build/src/80028848.o

DRAGON_OBJ := \
	build/src/libdragon/entrypoint.o \
	build/src/libdragon/n64sys.o \
	build/src/libdragon/interrupt.o \
	build/src/libdragon/rdp.o \
	build/src/libdragon/dma.o \
	build/src/libdragon/inthandler.o \
	build/src/libdragon/display.o \
	build/src/libdragon/exception.o

TOOLS_OBJ :=

.PHONY: default
default: build/$(PROG_NAME).bin

.PHONY: clean
clean:
	rm -f -r bin build $(TOOLS_OBJ)

build/$(PROG_NAME).bin: build/$(PROG_NAME).elf
	$(OBJCOPY) -O binary $< $@
	sha1sum -c $(PROG_NAME).sha1

build/$(PROG_NAME).elf: $(FIRM_OBJ) $(DRAGON_OBJ)
	$(LD) $(LDFLAGS) -T$(PROG_NAME).ld $(FIRM_OBJ) $(DRAGON_OBJ) -lc -lnosys -Map $(@:.elf=.map) -o $@

$(FIRM_OBJ): CFLAGS += -fno-toplevel-reorder
$(filter-out build/src/main.o,$(FIRM_OBJ)): CFLAGS += -G0 -O2
build/src/sys_bss.o: ASFLAGS += -G0
$(DRAGON_OBJ): CFLAGS += -G0 -O1

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

build/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@

tools/%: tools/%.c
	gcc -O2 -o $@ $<
