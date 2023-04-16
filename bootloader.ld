OUTPUT_FORMAT("elf32-bigmips")
OUTPUT_ARCH(mips)
EXTERN(_start)
ENTRY(_start)
SECTIONS
{
    . = 0x80000400;
    .text :
    {
        FILL(0);

        build/src/libdragon/entrypoint.o(.boot);

        . = ALIGN(16);
        __text_start = .;

        build/src/bios.o(.text);
        build/src/main.o(.text);
        build/src/usb.o(.text);
        build/src/disk.o(.text);
        build/src/fat.o(.text);
        build/src/gfx.o(.text);
        build/src/str.o(.text);
        build/src/sys.o(.text);
        build/src/incs.o(.text);
        build/src/libdragon/n64sys.o(.text);
        build/src/libdragon/interrupt.o(.text);
        build/src/libdragon/rdp.o(.text);
        build/src/libdragon/dma.o(.text);
        build/src/libdragon/inthandler.o(.text);
        build/src/libdragon/display.o(.text);
        build/src/libdragon/exception.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-malloc.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-mallocr.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-memcpy.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-memset.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-mlock.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-sbrkr.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-freer.o(.text);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-reent.o(.text);
        tools/mips64/mips64-elf/lib/libnosys.a:sbrk.o(.text);

        build/src/main.o(.rodata);
        build/src/fat.o(.rodata);
        build/src/sys.o(.rodata);
        build/src/libdragon/display.o(.rodata);

        build/src/fat.o(.rodata.str1.8);
        build/src/gfx.o(.rodata.str1.8);
        build/src/str.o(.rodata.str1.8);
        build/src/sys.o(.rodata.str1.8);
        build/src/libdragon/exception.o(.rodata.str1.8);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-impure.o(.rodata.str1.8);

        build/src/libdragon/rdp.o(.rodata.cst8);

        build/src/eh_frame.o(.rodata);

        __text_end = .;
    }
    .data :
    {
        FILL(0xAA);
        . = ALIGN(16);
        __data_start = .;

        build/src/bios.o(.data);
        build/src/sys.o(.data);
        build/src/libdragon/n64sys.o(.data);
        build/src/libdragon/interrupt.o(.data);
        build/src/libdragon/rdp.o(.data);
        build/src/libdragon/display.o(.data);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-mallocr.o(.data);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-impure.o(.data);

        . = ALIGN(16);
        _gp = .;

        tools/mips64/mips64-elf/lib/libc.a:lib_a-impure.o(.sdata);

        __data_end = .;
    }
    .bss (NOLOAD) :
    {
        __bss_start = .;

        build/src/bios.o(.scommon);
        build/src/main.o(.scommon);
        build/src/disk.o(.scommon);
        build/src/fat.o(.scommon);
        build/src/gfx.o(.scommon);
        build/src/str.o(.scommon);
        build/src/sys.o(.scommon);
        build/src/libdragon/display.o(.scommon);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-reent.o(.scommon);

        build/src/main.o(.sbss);
        build/src/libdragon/inthandler.o(.sbss);

        build/src/main.o(COMMON);
        build/src/disk.o(COMMON);
        build/src/fat.o(COMMON);
        build/src/sys.o(COMMON);
        build/src/libdragon/display.o(COMMON);

        build/src/bios.o(.bss);
        build/src/sys.o(.bss);
        build/src/libdragon/interrupt.o(.bss);
        build/src/libdragon/rdp.o(.bss);
        build/src/libdragon/inthandler.o(.bss);
        build/src/libdragon/display.o(.bss);
        build/src/libdragon/exception.o(.bss);
        tools/mips64/mips64-elf/lib/libc.a:lib_a-mallocr.o(.bss);
        tools/mips64/mips64-elf/lib/libnosys.a:sbrk.o(.bss);

        __bss_end = .;
        end = .;
    }
    /DISCARD/ : {*(*)}
}
