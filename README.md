# EverDrive-64 bootloader v5.00

A decompilation of the built-in bootloader ROM of EverDrive-64 X-series
cartridges.  This is NOT a decompilation of the downloadable OS ROM.

## Building
1. Install GMP, MPFR and Texinfo: `apt install libgmp-dev libmpfr-dev texinfo`
2. If you have not already, set `MAKEFLAGS`: `export MAKEFLAGS=-j$(nproc)`
3. Run `tools/setup`
4. Run `make`

## Decompiling

### Using decomp.me (recommended)
* Go to https://decomp.me/new
* Under Compiler, select `GCC 4.4.0 (mips64-elf)`
* Take the assembly starting with the label and ending right before the end
  directives, then remove all occurrences of `"` and `\n`.  Paste this into the
  Target assembly field.
* Paste the contents of `context.h` in the Context field.
* After creating the scratch, under the Options tab, paste the following into
  the arguments field: `-mtune=vr4300 -march=vr4300 -std=gnu99 -Wall -G0 -O2`

### Using `./diff`
Run `./diff <start> <stop> [flags]`, where:
* `start` is the start RAM address of the function, minus 0x80000000.
* `stop` is the end RAM address of the function, minus 0x80000000.
* `flags` are additional flags passed to `diff`, such as `-y`
