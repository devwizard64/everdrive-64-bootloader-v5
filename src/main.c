#include "everdrive.h"

extern u8 mcn_data[];
extern u32 mcn_data_len;

#if 0
u8 mainErrFpg;
u16 mainSerialNo;
u8 mainErrSdr;
u16 hu_8002930E;
#else
extern u8 mainErrFpg;
extern u16 mainSerialNo;
extern u8 mainErrSdr;
asm(
".section .scommon\n"
".globl mainErrFpg; mainErrFpg: .half 0\n"
".globl mainSerialNo; mainSerialNo: .half 0\n"
".globl mainErrSdr; mainErrSdr: .half 0\n"
".half 0\n"
);
#endif

u16 char_buff[G_SCREEN_W * G_SCREEN_H];

void MainShowError(u8);
void printError(u8);
u8 MainLoadOS();
void boot_simulator(u8 cic);
u8 MainTestSdr();
u8 MainTestFpg();

int main(void)
{
    u8 resp;
    disable_interrupts();
    set_AI_interrupt(0);
    set_VI_interrupt(0, 0);
    set_PI_interrupt(0);
    set_DP_interrupt(0);
    IO_WRITE(PI_STATUS_REG, 3);
    IO_WRITE(PI_BSD_DOM1_LAT_REG, 0x40);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x12);
    IO_WRITE(PI_BSD_DOM1_PGS_REG, 0x07);
    IO_WRITE(PI_BSD_DOM1_RLS_REG, 0x03);
    IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
    IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x12);
    IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x07);
    IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x03);
    resp = MainLoadOS();
    MainShowError(resp);
    return 0;
}

void MainShowError(u8 err)
{
    sysInit();
    gInit(char_buff);
    printError(err);
    sysRepaint();
    for (;;) usbListener();
}

void printError(u8 err)
{
    gSetPal(PAL_G1);
    GfxFillRect(' ', 0, 0, G_SCREEN_W, G_SCREEN_H);
    gSetPal(PAL_B1);
    GfxFillRect(' ', G_BORDER_X, G_BORDER_Y, G_SCREEN_W-G_BORDER_X*2, 4);
    GfxFillRect('-', G_BORDER_X, G_BORDER_Y, G_SCREEN_W-G_BORDER_X*2, 1);
    GfxFillRect('-', G_BORDER_X, G_BORDER_Y+3, G_SCREEN_W-G_BORDER_X*2, 1);
    gSetXY(G_BORDER_X, G_BORDER_Y+1);
    gConsPrint((u8 *)" EverDrive-64 bootloader v");
    GfxAppendDec(5);
    gAppendString((u8 *)".");
    gAppendHex8(0x00);
    gConsPrint((u8 *)" ERROR:");
    gAppendHex8(err);
    if (mainErrFpg || mainErrSdr)
    {
        gSetPal(PAL_BR);
        if (mainErrSdr) gAppendString((u8 *)"+SDR");
        if (mainErrFpg) gAppendString((u8 *)"+FPG");
    }
    gSetPal(PAL_G1);
    gSetXY(G_BORDER_X, G_SCREEN_H-G_BORDER_Y-1);
    gConsPrint((u8 *)"2019 krikzz");
    gSetXY(G_SCREEN_W-G_BORDER_X-7, G_SCREEN_H-G_BORDER_Y-1);
    gConsPrint((u8 *)"SN:");
    gAppendHex16(mainSerialNo);
    GfxResetXY();
    gSetY(G_SCREEN_H/2);
    if (err >= 0xC0 && err <= 0xCA)
    {
        GfxPrintCenter((u8 *)"SD card not found");
    }
    else if (err >= 0xD2 && err <= 0xD8)
    {
        GfxPrintCenter((u8 *)"DISK IO error");
    }
    else if (err == 0xF0 || err == 0xF7 || err == 0xF5)
    {
        gSetY(G_SCREEN_H/2 - 5);
        GfxPrintCenter((u8 *)"File not found: SD:/ED64/OS64.V64");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"INSTRUCTIONS:");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"1)Go to: http://krikzz.com");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"2)Download latest OS64.zip");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"3)Unzip files to hard drive");
        gConsPrint((u8 *)"");
        gConsPrint((u8 *)"4)Copy ED64 folder to SD card");
    }
    else if (err == 0xF8)
    {
        gSetY(G_SCREEN_H/2 - 1);
        GfxPrintCenter((u8 *)"Unknown file system");
        gConsPrint((u8 *)"");
        GfxPrintCenter((u8 *)"Please use FAT32");
    }
    else if (err >= 0xF1 && err <= 0xFB)
    {
        GfxPrintCenter((u8 *)"FAT error");
    }
    else if (err == 0x55)
    {
        GfxPrintCenter((u8 *)"Hardware error");
    }
    else if (err == BI_ERR_MCN_CFG || err == BI_ERR_IOM_CFG)
    {
        GfxPrintCenter((u8 *)"FPGA configuration error");
    }
    else
    {
        GfxPrintCenter((u8 *)"Unexpected error");
    }
}

u8 MainLoadOS()
{
    u8 resp;
    u8 sp28[0x1400];
    u8 buff[0x40];
    bi_init();
    BiBootRomRd(buff, 0x3FFC0, 0x40);
    mainSerialNo = (s16)buff[10] << 8 | buff[11];
    if (!BiBootCfgGet(BI_BCFG_BOOTMOD))
    {
        resp = BiMCNWr(mcn_data, mcn_data_len);
        if (resp) return resp;
        bi_init();
    }
    mainErrSdr = MainTestSdr();
    mainErrFpg = MainTestFpg();
    if (mainErrSdr || mainErrFpg) return 0x55;
    resp = diskInit();
    if (resp) return resp;
    resp = fatInit(sp28);
    if (resp) return resp;
    resp = fatOpenFileByeName((u8 *)"ED64/OS64.V64", 0);
    if (resp) return resp;
    BiCartRomFill(0, 0x1000, 0x100000);
    resp = fat_80003C00(0, fat_80003730());
    if (resp) return resp;
    simulate_pif_boot();
    return 0x13;
}

void simulate_pif_boot()
{
    diskCloseRW();
    BiLockRegs();
    IO_WRITE(VI_V_INT, 0x3FF);
    IO_WRITE(VI_H_LIMITS, 0);
    IO_WRITE(VI_CUR_LINE, 0);
    boot_simulator(CIC_6102);
}

void boot_simulator(u8 cic) {


    static u16 cheats_on; /* 0 = off, 1 = select, 2 = all */
    static u8 game_cic;

    game_cic = cic;
    cheats_on = 0;


    // Start game via CIC boot code
    asm __volatile__(
            ".set noreorder;"

            "lui    $t0, 0x8000;"

            // State required by all CICs
            "move   $s3, $zero;" // osRomType (0: N64, 1: 64DD)
            "lw     $s4, 0x0300($t0);" // osTvType (0: PAL, 1: NTSC, 2: MPAL)
            "move   $s5, $zero;" // osResetType (0: Cold, 1: NMI)
            "lui    $s6, %%hi(cic_ids);" // osCicId (See cic_ids LUT)
            "addu   $s6, $s6, %0;"
            "lbu    $s6, %%lo(cic_ids)($s6);"
            "lw     $s7, 0x0314($t0);" // osVersion

            // Copy PIF code to RSP IMEM (State required by CIC-NUS-6105)
            "lui    $a0, 0xA400;"
            "lui    $a1, %%hi(imem_start);"
            "ori    $a2, $zero, 0x0008;"
            "1:"
            "lw     $t0, %%lo(imem_start)($a1);"
            "addiu  $a1, $a1, 4;"
            "sw     $t0, 0x1000($a0);"
            "addiu  $a2, $a2, -1;"
            "bnez   $a2, 1b;"
            "addiu  $a0, $a0, 4;"

            // Copy CIC boot code to RSP DMEM
            "lui    $t3, 0xA400;"
            "ori    $t3, $t3, 0x0040;" // State required by CIC-NUS-6105
            "move   $a0, $t3;"
            "lui    $a1, 0xB000;"
            "ori    $a2, 0x0FC0;"
            "1:"
            "lw     $t0, 0x0040($a1);"
            "addiu  $a1, $a1, 4;"
            "sw     $t0, 0x0000($a0);"
            "addiu  $a2, $a2, -4;"
            "bnez   $a2, 1b;"
            "addiu  $a0, $a0, 4;"

            // Boot with or without cheats enabled?
            "beqz   %1, 2f;"

            // Patch CIC boot code
            "lui    $a1, %%hi(cic_patch_offsets);"
            "addu   $a1, $a1, %0;"
            "lbu    $a1, %%lo(cic_patch_offsets)($a1);"
            "addu   $a0, $t3, $a1;"
            "lui    $a1, 0x081C;" // "j 0x80700000"
            "ori    $a2, $zero, 0x06;"
            "bne    %0, $a2, 1f;"
            "lui    $a2, 0x8188;"
            "ori    $a2, $a2, 0x764A;"
            "xor    $a1, $a1, $a2;" // CIC-NUS-6106 encryption
            "1:"
            "sw     $a1, 0x0700($a0);" // Patch CIC boot code with jump

            // Patch CIC boot code to disable checksum failure halt
            // Required for CIC-NUS-6105
            "ori    $a2, $zero, 0x05;"
            "beql   %0, $a2, 2f;"
            "sw     $zero, 0x06CC($a0);"

            // Go!
            "2:"
            "lui    $sp, 0xA400;"
            "ori    $ra, $sp, 0x1550;" // State required by CIC-NUS-6105
            "jr     $t3;"
            "ori    $sp, $sp, 0x1FF0;" // State required by CIC-NUS-6105


            // Table of all CIC IDs
            "cic_ids:"
            ".byte  0x00;" // Unused
            ".byte  0x3F;" // NUS-CIC-6101
            ".byte  0x3F;" // NUS-CIC-6102
            ".byte  0x78;" // NUS-CIC-6103
            ".byte  0xAC;" // NUS-CIC-5101
            ".byte  0x91;" // NUS-CIC-6105
            ".byte  0x85;" // NUS-CIC-6106
            ".byte  0xDD;" // NUS-CIC-5167

            "cic_patch_offsets:"
            ".byte  0x00;" // Unused
            ".byte  0x30;" // CIC-NUS-6101
            ".byte  0x2C;" // CIC-NUS-6102
            ".byte  0x20;" // CIC-NUS-6103
            ".byte  0x30;" // NUS-CIC-5101
            ".byte  0x8C;" // CIC-NUS-6105
            ".byte  0x60;" // CIC-NUS-6106
            ".byte  0x30;" // NUS-CIC-5167

            // These instructions are copied to RSP IMEM; we don't execute them.
            "imem_start:"
            "lui    $t5, 0xBFC0;"
            "1:"
            "lw     $t0, 0x07FC($t5);"
            "addiu  $t5, $t5, 0x07C0;"
            "andi   $t0, $t0, 0x0080;"
            "bnezl  $t0, 1b;"
            "lui    $t5, 0xBFC0;"
            "lw     $t0, 0x0024($t5);"
            "lui    $t3, 0xB000;"

            : // outputs
            : "r" (game_cic), // inputs
            "r" (cheats_on)
            : "$4", "$5", "$6", "$8", // clobber
            "$11", "$19", "$20", "$21",
            "$22", "$23", "memory"
            );


    return;
}

/* Test SDRAM */
u8 MainTestSdr()
{
    u32 i;
    u16 buff[0x10000];
    for (i = 0; i < 0x10000; i++)
    {
        buff[i] = i;
    }
    BiCartRomWr(buff, 0, 0x20000);
    BiCartRomRd(buff, 0, 0x20000);
    for (i = 0; i < 0x10000; i++)
    {
        if (buff[i] != i) return 1;
    }
    return 0;
}

/* Test FPGA */
u8 MainTestFpg()
{
    u8 resp = 0;
    BiBootCfgSet(0x10);
    if (!BiBootCfgGet(0x10)) resp = 1;
    BiBootCfgClr(0x10);
    if (BiBootCfgGet(0x10)) resp = 1;
    return resp;
}
