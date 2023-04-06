#include "firmware.h"

typedef struct PI_regs_s {
    /** @brief Uncached address in RAM where data should be found */
    void * ram_address;
    /** @brief Address of data on peripheral */
    unsigned long pi_address;
    /** @brief How much data to read from RAM into the peripheral */
    unsigned long read_length;
    /** @brief How much data to write to RAM from the peripheral */
    unsigned long write_length;
    /** @brief Status of the PI, including DMA busy */
    unsigned long status;
} PI_regs_s;

typedef struct SI_regs_s {
    /** @brief Uncached address in RAM where data should be found */
    volatile void * DRAM_addr;
    /** @brief Address to read when copying from PIF RAM */
    volatile void * PIF_addr_read;
    /** @brief Reserved word */
    uint32_t reserved1;
    /** @brief Reserved word */
    uint32_t reserved2;
    /** @brief Address to write when copying to PIF RAM */
    volatile void * PIF_addr_write;
    /** @brief Reserved word */
    uint32_t reserved3;
    /** @brief SI status, including DMA busy and IO busy */
    uint32_t status;
} SI_regs_t;


const u32 ntsc_320[] = {
    0x00013002, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000200, 0x00000400
};


const u32 mpal_320[] = {
    0x00013002, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    0x00000200, 0x00000400
};

const u32 pal_320[] = {
    0x00013002, 0x00000000, 0x00000140, 0x00000200,
    0x00000000, 0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    0x00000200, 0x00000400
};

#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
#define SI_STATUS_IO_BUSY   ( 1 << 1 )
#define SYS_MAX_PIXEL_W   320
#define SYS_MAX_PIXEL_H   240

void sysDisplayInit();
void sysPI_rd_safe(void *ram, unsigned long pi_address, unsigned long len);
void sysPI_wr_safe(void *ram, unsigned long pi_address, unsigned long len);

u64 pifCmdRTCInfo[] =
{
    0x00000000FF010306, 0xFFFFFFFFFE000000,
    0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000001,
};

u64 pifCmdEepRead[] =
{
    0x0000000002080400, 0xFFFFFFFFFFFFFFFF,
    0xFFFE000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000001,
};

//static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *) 0xa4600000;
//static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *) 0xa4800000;
//static void * const PIF_RAM = (void *) 0x1fc007c0;
BootStrap *sys_boot_strap = (BootStrap *) 0x80000300;

extern u8 font[];
Screen screen;

u16 pal[16] = {
    RGB(0, 0, 0), RGB(31, 31, 31), RGB(16, 16, 16), RGB(28, 28, 2),
    RGB(8, 8, 8), RGB(31, 0, 0), RGB(0, 31, 0), RGB(12, 12, 12),
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0aa0
};

vu32 *vregs = (vu32 *) 0xa4400000;

#if 0
#else
extern u16 *g_disp_ptr;
extern u16 g_cur_pal;
extern u16 g_cons_ptr;
extern u8 g_last_x;
extern u8 g_last_y;
extern u16 *g_buff;
asm(
".section .scommon\n"
".globl g_last_x; g_last_x: .half 0\n"
".globl g_cur_pal; g_cur_pal: .half 0\n"
".globl g_buff; g_buff: .word 0\n"
".globl g_last_y; g_last_y: .word 0\n"
".globl g_disp_ptr; g_disp_ptr: .word 0\n"
".globl g_cons_ptr; g_cons_ptr: .word 0\n"
".globl w_80029338; w_80029338: .word 0\n"
".half 0\n"
".globl hu_8002933E; hu_8002933E: .half 0\n"
".word 0\n"
);
#endif

u16 hu_8002A020 = 0;

void gSetPal(u16 pal) {
    g_cur_pal = pal;
}

void gAppendString(u8 *str) {
    while (*str != 0)*g_disp_ptr++ = *str++ + g_cur_pal;
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
"/*0x80004AF0*/  lbu     $v0, 0x0000($a0)\n"
"/*0x80004AF4*/  beqz    $v0, .L80004B44\n"
"/*0x80004AF8*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80004AFC*/  beqz    $a1, .L80004B44\n"
"/*0x80004B00*/  lui     $a3, %hi(g_disp_ptr)\n"
"/*0x80004B04*/  addiu   $a1, $a1, -0x0001\n"
"/*0x80004B08*/  lw      $v1, %lo(g_disp_ptr)($a3)\n"
"/*0x80004B0C*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80004B10*/  j       .L80004B20\n"
"/*0x80004B14*/  lui     $t0, %hi(g_cur_pal)\n"
".L80004B18:\n"
"/*0x80004B18*/  beqz    $a1, .L80004B44\n"
"/*0x80004B1C*/  andi    $a1, $a2, 0x00FF\n"
".L80004B20:\n"
"/*0x80004B20*/  lhu     $a2, %lo(g_cur_pal)($t0)\n"
"/*0x80004B24*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80004B28*/  addu    $v0, $v0, $a2\n"
"/*0x80004B2C*/  sh      $v0, 0x0000($v1)\n"
"/*0x80004B30*/  addiu   $v1, $v1, 0x0002\n"
"/*0x80004B34*/  sw      $v1, %lo(g_disp_ptr)($a3)\n"
"/*0x80004B38*/  lbu     $v0, 0x0000($a0)\n"
"/*0x80004B3C*/  bnez    $v0, .L80004B18\n"
"/*0x80004B40*/  addiu   $a2, $a1, -0x0001\n"
".L80004B44:\n"
"/*0x80004B44*/  jr      $ra\n"
"/*0x80004B48*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

void gAppendChar(u8 chr) {

    *g_disp_ptr++ = chr + g_cur_pal;
}

void gAppendHex4(u8 val) {

    val += (val < 10 ? '0' : '7');
    *g_disp_ptr++ = val + g_cur_pal;
}

void gAppendHex8(u8 val) {

    gAppendHex4(val >> 4);
    gAppendHex4(val & 15);
}

void gAppendHex16(u16 val) {

    gAppendHex8(val >> 8);
    gAppendHex8(val);
}

void gAppendHex32(u32 val) {

    gAppendHex16(val >> 16);
    gAppendHex16(val);

}

void gSetXY(u8 x, u8 y) {

    g_cons_ptr = x + y * G_SCREEN_W;
    g_disp_ptr = &g_buff[g_cons_ptr];
    g_last_x = x;
    g_last_y = y;
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80004CD0*/  lui     $v1, %hi(g_last_y)\n"
"/*0x80004CD4*/  lbu     $v0, %lo(g_last_y)($v1)\n"
"/*0x80004CD8*/  andi    $a0, $a0, 0x00FF\n"
"/*0x80004CDC*/  sll     $a2, $v0, 3\n"
"/*0x80004CE0*/  sll     $a1, $v0, 5\n"
"/*0x80004CE4*/  addu    $a1, $a2, $a1\n"
"/*0x80004CE8*/  lui     $a2, %hi(g_buff)\n"
"/*0x80004CEC*/  addu    $a1, $a0, $a1\n"
"/*0x80004CF0*/  lw      $a3, %lo(g_buff)($a2)\n"
"/*0x80004CF4*/  sll     $a2, $a1, 1\n"
"/*0x80004CF8*/  addu    $a3, $a3, $a2\n"
"/*0x80004CFC*/  lui     $a2, %hi(g_disp_ptr)\n"
"/*0x80004D00*/  sw      $a3, %lo(g_disp_ptr)($a2)\n"
"/*0x80004D04*/  sb      $v0, %lo(g_last_y)($v1)\n"
"/*0x80004D08*/  lui     $a2, %hi(g_last_x)\n"
"/*0x80004D0C*/  lui     $v0, %hi(g_cons_ptr)\n"
"/*0x80004D10*/  sb      $a0, %lo(g_last_x)($a2)\n"
"/*0x80004D14*/  jr      $ra\n"
"/*0x80004D18*/  sh      $a1, %lo(g_cons_ptr)($v0)\n"
"/*0x80004D1C*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80004D20\n"
"sys_80004D20:\n"
"/*0x80004D20*/  andi    $a0, $a0, 0x00FF\n"
"/*0x80004D24*/  lui     $v1, %hi(g_last_x)\n"
"/*0x80004D28*/  lbu     $a1, %lo(g_last_x)($v1)\n"
"/*0x80004D2C*/  sll     $a2, $a0, 3\n"
"/*0x80004D30*/  sll     $v0, $a0, 5\n"
"/*0x80004D34*/  addu    $v0, $a2, $v0\n"
"/*0x80004D38*/  lui     $a2, %hi(g_buff)\n"
"/*0x80004D3C*/  addu    $v0, $a1, $v0\n"
"/*0x80004D40*/  lw      $a3, %lo(g_buff)($a2)\n"
"/*0x80004D44*/  sb      $a1, %lo(g_last_x)($v1)\n"
"/*0x80004D48*/  sll     $a2, $v0, 1\n"
"/*0x80004D4C*/  lui     $v1, %hi(g_last_y)\n"
"/*0x80004D50*/  addu    $a3, $a3, $a2\n"
"/*0x80004D54*/  sb      $a0, %lo(g_last_y)($v1)\n"
"/*0x80004D58*/  lui     $a2, %hi(g_disp_ptr)\n"
"/*0x80004D5C*/  lui     $v1, %hi(g_cons_ptr)\n"
"/*0x80004D60*/  sw      $a3, %lo(g_disp_ptr)($a2)\n"
"/*0x80004D64*/  jr      $ra\n"
"/*0x80004D68*/  sh      $v0, %lo(g_cons_ptr)($v1)\n"
"/*0x80004D6C*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

u8 sys_80004D70(void)
{
    return g_last_x;
}

u8 sys_80004D80(void)
{
    return g_last_y;
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80004D90\n"
"sys_80004D90:\n"
"/*0x80004D90*/  lui     $v0, %hi(g_buff)\n"
"/*0x80004D94*/  lw      $a0, %lo(g_buff)($v0)\n"
"/*0x80004D98*/  lui     $v1, %hi(g_cons_ptr)\n"
"/*0x80004D9C*/  li      $a1, 0x0052\n"
"/*0x80004DA0*/  addiu   $a0, $a0, 0x00A4\n"
"/*0x80004DA4*/  sh      $a1, %lo(g_cons_ptr)($v1)\n"
"/*0x80004DA8*/  lui     $v1, %hi(g_disp_ptr)\n"
"/*0x80004DAC*/  li      $v0, 0x0002\n"
"/*0x80004DB0*/  sw      $a0, %lo(g_disp_ptr)($v1)\n"
"/*0x80004DB4*/  lui     $v1, %hi(g_last_y)\n"
"/*0x80004DB8*/  sb      $v0, %lo(g_last_y)($v1)\n"
"/*0x80004DBC*/  lui     $v1, %hi(g_last_x)\n"
"/*0x80004DC0*/  jr      $ra\n"
"/*0x80004DC4*/  sb      $v0, %lo(g_last_x)($v1)\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80004DC8\n"
"sys_80004DC8:\n"
"/*0x80004DC8*/  lui     $a3, %hi(g_cons_ptr)\n"
"/*0x80004DCC*/  lhu     $v1, %lo(g_cons_ptr)($a3)\n"
"/*0x80004DD0*/  lui     $a2, %hi(g_last_y)\n"
"/*0x80004DD4*/  lui     $v0, %hi(g_buff)\n"
"/*0x80004DD8*/  lw      $t1, %lo(g_buff)($v0)\n"
"/*0x80004DDC*/  lbu     $t0, %lo(g_last_y)($a2)\n"
"/*0x80004DE0*/  sll     $v0, $v1, 1\n"
"/*0x80004DE4*/  addu    $v0, $t1, $v0\n"
"/*0x80004DE8*/  addiu   $t0, $t0, 0x0001\n"
"/*0x80004DEC*/  addiu   $t1, $v1, 0x0028\n"
"/*0x80004DF0*/  lui     $v1, %hi(g_disp_ptr)\n"
"/*0x80004DF4*/  sh      $t1, %lo(g_cons_ptr)($a3)\n"
"/*0x80004DF8*/  sb      $t0, %lo(g_last_y)($a2)\n"
"/*0x80004DFC*/  sw      $v0, %lo(g_disp_ptr)($v1)\n"
"/*0x80004E00*/  lbu     $a2, 0x0000($a0)\n"
"/*0x80004E04*/  beqz    $a2, .L80004E4C\n"
"/*0x80004E08*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80004E0C*/  beqz    $a1, .L80004E4C\n"
"/*0x80004E10*/  addiu   $a1, $a1, -0x0001\n"
"/*0x80004E14*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80004E18*/  j       .L80004E28\n"
"/*0x80004E1C*/  lui     $t0, %hi(g_cur_pal)\n"
".L80004E20:\n"
"/*0x80004E20*/  beqz    $a1, .L80004E4C\n"
"/*0x80004E24*/  andi    $a1, $a3, 0x00FF\n"
".L80004E28:\n"
"/*0x80004E28*/  lhu     $a3, %lo(g_cur_pal)($t0)\n"
"/*0x80004E2C*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80004E30*/  addu    $a2, $a2, $a3\n"
"/*0x80004E34*/  sh      $a2, 0x0000($v0)\n"
"/*0x80004E38*/  addiu   $v0, $v0, 0x0002\n"
"/*0x80004E3C*/  sw      $v0, %lo(g_disp_ptr)($v1)\n"
"/*0x80004E40*/  lbu     $a2, 0x0000($a0)\n"
"/*0x80004E44*/  bnez    $a2, .L80004E20\n"
"/*0x80004E48*/  addiu   $a3, $a1, -0x0001\n"
".L80004E4C:\n"
"/*0x80004E4C*/  jr      $ra\n"
"/*0x80004E50*/  nop\n"
"/*0x80004E54*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

void sys_80004DC8(u8 *, int);

void sys_80004E58(u8 *str) {

    sys_80004DC8(str, 36);
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80004E60*/  andi    $a0, $a0, 0x00FF\n"
"/*0x80004E64*/  lui     $v0, %hi(g_buff)\n"
"/*0x80004E68*/  sll     $v1, $a0, 6\n"
"/*0x80004E6C*/  lw      $v0, %lo(g_buff)($v0)\n"
"/*0x80004E70*/  sll     $a0, $a0, 4\n"
"/*0x80004E74*/  addu    $a0, $a0, $v1\n"
"/*0x80004E78*/  addu    $v0, $v0, $a0\n"
"/*0x80004E7C*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80004E80*/  li      $v1, 0x0027\n"
"/*0x80004E84*/  li      $a2, 0x00FF\n"
".L80004E88:\n"
"/*0x80004E88*/  lhu     $a0, 0x0000($v0)\n"
"/*0x80004E8C*/  addiu   $v1, $v1, -0x0001\n"
"/*0x80004E90*/  andi    $a0, $a0, 0x007F\n"
"/*0x80004E94*/  addu    $a0, $a1, $a0\n"
"/*0x80004E98*/  andi    $v1, $v1, 0x00FF\n"
"/*0x80004E9C*/  sh      $a0, 0x0000($v0)\n"
"/*0x80004EA0*/  bne     $v1, $a2, .L80004E88\n"
"/*0x80004EA4*/  addiu   $v0, $v0, 0x0002\n"
"/*0x80004EA8*/  jr      $ra\n"
"/*0x80004EAC*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80004EB0*/  andi    $a2, $a2, 0x00FF\n"
"/*0x80004EB4*/  sll     $v1, $a2, 3\n"
"/*0x80004EB8*/  sll     $v0, $a2, 5\n"
"/*0x80004EBC*/  addu    $v0, $v1, $v0\n"
"/*0x80004EC0*/  lui     $v1, %hi(g_buff)\n"
"/*0x80004EC4*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80004EC8*/  lw      $t0, %lo(g_buff)($v1)\n"
"/*0x80004ECC*/  lui     $v1, %hi(g_cur_pal)\n"
"/*0x80004ED0*/  addu    $v0, $a1, $v0\n"
"/*0x80004ED4*/  lhu     $t1, %lo(g_cur_pal)($v1)\n"
"/*0x80004ED8*/  sll     $v1, $v0, 1\n"
"/*0x80004EDC*/  addu    $t1, $a0, $t1\n"
"/*0x80004EE0*/  addu    $t2, $t0, $v1\n"
"/*0x80004EE4*/  lui     $a0, %hi(g_disp_ptr)\n"
"/*0x80004EE8*/  sw      $t2, %lo(g_disp_ptr)($a0)\n"
"/*0x80004EEC*/  lui     $a0, %hi(g_last_x)\n"
"/*0x80004EF0*/  sb      $a1, %lo(g_last_x)($a0)\n"
"/*0x80004EF4*/  lui     $a0, %hi(g_last_y)\n"
"/*0x80004EF8*/  sb      $a2, %lo(g_last_y)($a0)\n"
"/*0x80004EFC*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80004F00*/  lui     $a0, %hi(g_cons_ptr)\n"
"/*0x80004F04*/  sh      $v0, %lo(g_cons_ptr)($a0)\n"
"/*0x80004F08*/  beqz    $a3, .L80004F50\n"
"/*0x80004F0C*/  andi    $t1, $t1, 0xFFFF\n"
"/*0x80004F10*/  addiu   $a3, $a3, -0x0001\n"
"/*0x80004F14*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80004F18*/  sll     $a0, $a3, 5\n"
"/*0x80004F1C*/  sll     $a1, $a3, 3\n"
"/*0x80004F20*/  addu    $a1, $a1, $a0\n"
"/*0x80004F24*/  addiu   $a0, $v0, 0x0028\n"
"/*0x80004F28*/  addu    $a1, $a0, $a1\n"
"/*0x80004F2C*/  j       .L80004F40\n"
"/*0x80004F30*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80004F34*/  nop\n"
".L80004F38:\n"
"/*0x80004F38*/  addiu   $a0, $v0, 0x0028\n"
"/*0x80004F3C*/  sll     $v1, $v0, 1\n"
".L80004F40:\n"
"/*0x80004F40*/  addu    $v1, $t0, $v1\n"
"/*0x80004F44*/  andi    $v0, $a0, 0xFFFF\n"
"/*0x80004F48*/  bne     $v0, $a1, .L80004F38\n"
"/*0x80004F4C*/  sh      $t1, 0x0000($v1)\n"
".L80004F50:\n"
"/*0x80004F50*/  jr      $ra\n"
"/*0x80004F54*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80004F58*/  addiu   $sp, $sp, -0x0048\n"
"/*0x80004F5C*/  sd      $s2, 0x0030($sp)\n"
"/*0x80004F60*/  andi    $s2, $a1, 0xFFFF\n"
"/*0x80004F64*/  sd      $s1, 0x0028($sp)\n"
"/*0x80004F68*/  sd      $ra, 0x0040($sp)\n"
"/*0x80004F6C*/  sd      $s3, 0x0038($sp)\n"
"/*0x80004F70*/  sd      $s0, 0x0020($sp)\n"
"/*0x80004F74*/  beqz    $s2, .L80004FDC\n"
"/*0x80004F78*/  daddu   $s1, $a0, $zero\n"
"/*0x80004F7C*/  lui     $s3, %hi(u_800287E0)\n"
"/*0x80004F80*/  addiu   $s3, %lo(u_800287E0)\n"
"/*0x80004F84*/  j       .L80004FAC\n"
"/*0x80004F88*/  daddu   $s0, $zero, $zero\n"
"/*0x80004F8C*/  nop\n"
".L80004F90:\n"
"/*0x80004F90*/  lbu     $a0, 0x0000($s1)\n"
"/*0x80004F94*/  jal     gAppendHex8\n"
"/*0x80004F98*/  addiu   $s1, $s1, 0x0001\n"
"/*0x80004F9C*/  andi    $v0, $s0, 0xFFFF\n"
"/*0x80004FA0*/  sltu    $v0, $v0, $s2\n"
"/*0x80004FA4*/  beqz    $v0, .L80004FE0\n"
"/*0x80004FA8*/  ld      $ra, 0x0040($sp)\n"
".L80004FAC:\n"
"/*0x80004FAC*/  andi    $v0, $s0, 0x000F\n"
".L80004FB0:\n"
"/*0x80004FB0*/  bnez    $v0, .L80004F90\n"
"/*0x80004FB4*/  addiu   $s0, $s0, 0x0001\n"
"/*0x80004FB8*/  jal     sys_80004E58\n"
"/*0x80004FBC*/  daddu   $a0, $s3, $zero\n"
"/*0x80004FC0*/  lbu     $a0, 0x0000($s1)\n"
"/*0x80004FC4*/  jal     gAppendHex8\n"
"/*0x80004FC8*/  addiu   $s1, $s1, 0x0001\n"
"/*0x80004FCC*/  andi    $v0, $s0, 0xFFFF\n"
"/*0x80004FD0*/  sltu    $v0, $v0, $s2\n"
"/*0x80004FD4*/  bnez    $v0, .L80004FB0\n"
"/*0x80004FD8*/  andi    $v0, $s0, 0x000F\n"
".L80004FDC:\n"
"/*0x80004FDC*/  ld      $ra, 0x0040($sp)\n"
".L80004FE0:\n"
"/*0x80004FE0*/  ld      $s3, 0x0038($sp)\n"
"/*0x80004FE4*/  ld      $s2, 0x0030($sp)\n"
"/*0x80004FE8*/  ld      $s1, 0x0028($sp)\n"
"/*0x80004FEC*/  ld      $s0, 0x0020($sp)\n"
"/*0x80004FF0*/  jr      $ra\n"
"/*0x80004FF4*/  addiu   $sp, $sp, 0x0048\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80004FF8*/  andi    $v0, $a2, 0x00FF\n"
"/*0x80004FFC*/  sll     $v1, $v0, 5\n"
"/*0x80005000*/  sll     $t0, $v0, 3\n"
"/*0x80005004*/  andi    $t1, $a1, 0x00FF\n"
"/*0x80005008*/  addu    $t0, $t0, $v1\n"
"/*0x8000500C*/  lui     $v1, %hi(g_buff)\n"
"/*0x80005010*/  lw      $a2, %lo(g_buff)($v1)\n"
"/*0x80005014*/  addu    $t0, $t1, $t0\n"
"/*0x80005018*/  lui     $v1, %hi(g_cur_pal)\n"
"/*0x8000501C*/  lhu     $a1, %lo(g_cur_pal)($v1)\n"
"/*0x80005020*/  sll     $v1, $t0, 1\n"
"/*0x80005024*/  addu    $v1, $a2, $v1\n"
"/*0x80005028*/  andi    $a2, $a3, 0x00FF\n"
"/*0x8000502C*/  lui     $a3, %hi(g_last_x)\n"
"/*0x80005030*/  sb      $t1, %lo(g_last_x)($a3)\n"
"/*0x80005034*/  lui     $a3, %hi(g_last_y)\n"
"/*0x80005038*/  addu    $a1, $a0, $a1\n"
"/*0x8000503C*/  sb      $v0, %lo(g_last_y)($a3)\n"
"/*0x80005040*/  lui     $v0, %hi(g_cons_ptr)\n"
"/*0x80005044*/  sh      $t0, %lo(g_cons_ptr)($v0)\n"
"/*0x80005048*/  daddu   $a0, $v1, $zero\n"
"/*0x8000504C*/  lui     $v0, %hi(g_disp_ptr)\n"
"/*0x80005050*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80005054*/  sll     $a2, $a2, 1\n"
"/*0x80005058*/  j       sys_80005320\n"
"/*0x8000505C*/  sw      $v1, %lo(g_disp_ptr)($v0)\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"sys_80005060:\n"
"/*0x80005060*/  addiu   $sp, $sp, -0x0050\n"
"/*0x80005064*/  lui     $v0, %hi(g_cur_pal)\n"
"/*0x80005068*/  sd      $s4, 0x0040($sp)\n"
"/*0x8000506C*/  sd      $s0, 0x0020($sp)\n"
"/*0x80005070*/  lhu     $s4, %lo(g_cur_pal)($v0)\n"
"/*0x80005074*/  andi    $s0, $a2, 0x00FF\n"
"/*0x80005078*/  lui     $v0, %hi(g_cons_ptr)\n"
"/*0x8000507C*/  sd      $s1, 0x0028($sp)\n"
"/*0x80005080*/  sd      $ra, 0x0048($sp)\n"
"/*0x80005084*/  sd      $s3, 0x0038($sp)\n"
"/*0x80005088*/  sd      $s2, 0x0030($sp)\n"
"/*0x8000508C*/  andi    $a0, $a0, 0xFFFF\n"
"/*0x80005090*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80005094*/  beqz    $s0, .L800050DC\n"
"/*0x80005098*/  lhu     $s1, %lo(g_cons_ptr)($v0)\n"
"/*0x8000509C*/  addu    $s4, $a0, $s4\n"
"/*0x800050A0*/  sll     $s3, $a1, 1\n"
"/*0x800050A4*/  andi    $s4, $s4, 0xFFFF\n"
"/*0x800050A8*/  andi    $s3, $s3, 0x00FF\n"
"/*0x800050AC*/  lui     $s2, %hi(g_buff)\n"
".L800050B0:\n"
"/*0x800050B0*/  lw      $a0, %lo(g_buff)($s2)\n"
"/*0x800050B4*/  sll     $v0, $s1, 1\n"
"/*0x800050B8*/  addiu   $s0, $s0, -0x0001\n"
"/*0x800050BC*/  addu    $a0, $a0, $v0\n"
"/*0x800050C0*/  daddu   $a1, $s4, $zero\n"
"/*0x800050C4*/  daddu   $a2, $s3, $zero\n"
"/*0x800050C8*/  addiu   $s1, $s1, 0x0028\n"
"/*0x800050CC*/  jal     sys_80005320\n"
"/*0x800050D0*/  andi    $s0, $s0, 0x00FF\n"
"/*0x800050D4*/  bnez    $s0, .L800050B0\n"
"/*0x800050D8*/  andi    $s1, $s1, 0xFFFF\n"
".L800050DC:\n"
"/*0x800050DC*/  ld      $ra, 0x0048($sp)\n"
"/*0x800050E0*/  ld      $s4, 0x0040($sp)\n"
"/*0x800050E4*/  ld      $s3, 0x0038($sp)\n"
"/*0x800050E8*/  ld      $s2, 0x0030($sp)\n"
"/*0x800050EC*/  ld      $s1, 0x0028($sp)\n"
"/*0x800050F0*/  ld      $s0, 0x0020($sp)\n"
"/*0x800050F4*/  jr      $ra\n"
"/*0x800050F8*/  addiu   $sp, $sp, 0x0050\n"
"/*0x800050FC*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005100\n"
"sys_80005100:\n"
"/*0x80005100*/  andi    $v0, $a2, 0x00FF\n"
"/*0x80005104*/  sll     $a2, $v0, 3\n"
"/*0x80005108*/  sll     $v1, $v0, 5\n"
"/*0x8000510C*/  andi    $t0, $a1, 0x00FF\n"
"/*0x80005110*/  addu    $v1, $a2, $v1\n"
"/*0x80005114*/  lui     $a1, %hi(g_buff)\n"
"/*0x80005118*/  addu    $v1, $t0, $v1\n"
"/*0x8000511C*/  lw      $t1, %lo(g_buff)($a1)\n"
"/*0x80005120*/  sll     $a1, $v1, 1\n"
"/*0x80005124*/  addu    $t1, $t1, $a1\n"
"/*0x80005128*/  andi    $a1, $a3, 0x00FF\n"
"/*0x8000512C*/  lui     $a3, %hi(g_disp_ptr)\n"
"/*0x80005130*/  sw      $t1, %lo(g_disp_ptr)($a3)\n"
"/*0x80005134*/  lui     $a3, %hi(g_last_x)\n"
"/*0x80005138*/  sb      $t0, %lo(g_last_x)($a3)\n"
"/*0x8000513C*/  lbu     $a2, 0x0027($sp)\n"
"/*0x80005140*/  lui     $a3, %hi(g_last_y)\n"
"/*0x80005144*/  sb      $v0, %lo(g_last_y)($a3)\n"
"/*0x80005148*/  andi    $a0, $a0, 0xFFFF\n"
"/*0x8000514C*/  lui     $v0, %hi(g_cons_ptr)\n"
"/*0x80005150*/  j       sys_80005060\n"
"/*0x80005154*/  sh      $v1, %lo(g_cons_ptr)($v0)\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005158\n"
"sys_80005158:\n"
"/*0x80005158*/  lui     $v0, %hi(g_buff)\n"
"/*0x8000515C*/  lw      $a3, %lo(g_buff)($v0)\n"
"/*0x80005160*/  li      $t0, 0x0052\n"
"/*0x80005164*/  lui     $v1, %hi(g_cons_ptr)\n"
"/*0x80005168*/  daddu   $a0, $a3, $zero\n"
"/*0x8000516C*/  sh      $t0, %lo(g_cons_ptr)($v1)\n"
"/*0x80005170*/  addiu   $a3, $a3, 0x00A4\n"
"/*0x80005174*/  lui     $v1, %hi(g_disp_ptr)\n"
"/*0x80005178*/  addiu   $sp, $sp, -0x0030\n"
"/*0x8000517C*/  li      $v0, 0x0002\n"
"/*0x80005180*/  sw      $a3, %lo(g_disp_ptr)($v1)\n"
"/*0x80005184*/  lui     $v1, %hi(g_last_y)\n"
"/*0x80005188*/  sd      $s0, 0x0020($sp)\n"
"/*0x8000518C*/  sb      $v0, %lo(g_last_y)($v1)\n"
"/*0x80005190*/  lui     $s0, %hi(g_cur_pal)\n"
"/*0x80005194*/  lui     $v1, %hi(g_last_x)\n"
"/*0x80005198*/  daddu   $a1, $zero, $zero\n"
"/*0x8000519C*/  li      $a2, 0x0960\n"
"/*0x800051A0*/  sd      $ra, 0x0028($sp)\n"
"/*0x800051A4*/  sb      $v0, %lo(g_last_x)($v1)\n"
"/*0x800051A8*/  jal     sys_80005320\n"
"/*0x800051AC*/  sh      $zero, %lo(g_cur_pal)($s0)\n"
"/*0x800051B0*/  ld      $ra, 0x0028($sp)\n"
"/*0x800051B4*/  li      $v0, 0x1000\n"
"/*0x800051B8*/  sh      $v0, %lo(g_cur_pal)($s0)\n"
"/*0x800051BC*/  ld      $s0, 0x0020($sp)\n"
"/*0x800051C0*/  jr      $ra\n"
"/*0x800051C4*/  addiu   $sp, $sp, 0x0030\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"sys_800051C8:\n"
"/*0x800051C8*/  addiu   $sp, $sp, -0x0048\n"
"/*0x800051CC*/  andi    $a1, $a1, 0x00FF\n"
"/*0x800051D0*/  sd      $s2, 0x0038($sp)\n"
"/*0x800051D4*/  sd      $s1, 0x0030($sp)\n"
"/*0x800051D8*/  sd      $s0, 0x0028($sp)\n"
"/*0x800051DC*/  sd      $a1, 0x0020($sp)\n"
"/*0x800051E0*/  sd      $ra, 0x0040($sp)\n"
"/*0x800051E4*/  jal     sys_800054B8\n"
"/*0x800051E8*/  daddu   $s2, $a0, $zero\n"
"/*0x800051EC*/  ld      $a1, 0x0020($sp)\n"
"/*0x800051F0*/  andi    $v0, $v0, 0x00FF\n"
"/*0x800051F4*/  lui     $s0, %hi(g_cons_ptr)\n"
"/*0x800051F8*/  sltu    $a0, $a1, $v0\n"
"/*0x800051FC*/  beqz    $a0, .L80005208\n"
"/*0x80005200*/  lhu     $s1, %lo(g_cons_ptr)($s0)\n"
"/*0x80005204*/  daddu   $v0, $a1, $zero\n"
".L80005208:\n"
"/*0x80005208*/  li      $a3, 0x0028\n"
"/*0x8000520C*/  lui     $a2, %hi(g_last_y)\n"
"/*0x80005210*/  subu    $v0, $a3, $v0\n"
"/*0x80005214*/  lbu     $v1, %lo(g_last_y)($a2)\n"
"/*0x80005218*/  srl     $a3, $v0, 31\n"
"/*0x8000521C*/  addu    $v0, $a3, $v0\n"
"/*0x80005220*/  sll     $a0, $v1, 3\n"
"/*0x80005224*/  sra     $a3, $v0, 1\n"
"/*0x80005228*/  sll     $v0, $v1, 5\n"
"/*0x8000522C*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80005230*/  addu    $v0, $a0, $v0\n"
"/*0x80005234*/  lui     $a0, %hi(g_buff)\n"
"/*0x80005238*/  addu    $v0, $a3, $v0\n"
"/*0x8000523C*/  lw      $t1, %lo(g_buff)($a0)\n"
"/*0x80005240*/  sll     $a0, $v0, 1\n"
"/*0x80005244*/  addu    $t1, $t1, $a0\n"
"/*0x80005248*/  lui     $t0, %hi(g_disp_ptr)\n"
"/*0x8000524C*/  daddu   $a0, $s2, $zero\n"
"/*0x80005250*/  sw      $t1, %lo(g_disp_ptr)($t0)\n"
"/*0x80005254*/  lui     $t0, %hi(g_last_x)\n"
"/*0x80005258*/  sh      $v0, %lo(g_cons_ptr)($s0)\n"
"/*0x8000525C*/  sb      $a3, %lo(g_last_x)($t0)\n"
"/*0x80005260*/  jal     sys_80004DC8\n"
"/*0x80005264*/  sb      $v1, %lo(g_last_y)($a2)\n"
"/*0x80005268*/  ld      $ra, 0x0040($sp)\n"
"/*0x8000526C*/  addiu   $s1, $s1, 0x0028\n"
"/*0x80005270*/  sh      $s1, %lo(g_cons_ptr)($s0)\n"
"/*0x80005274*/  ld      $s2, 0x0038($sp)\n"
"/*0x80005278*/  ld      $s1, 0x0030($sp)\n"
"/*0x8000527C*/  ld      $s0, 0x0028($sp)\n"
"/*0x80005280*/  jr      $ra\n"
"/*0x80005284*/  addiu   $sp, $sp, 0x0048\n"
"\n"
".set reorder\n"
".set at\n"
);

void sys_800051C8(u8 *, int);

void sys_80005288(u8 *str) {

    sys_800051C8(str, 36);
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005290\n"
"sys_80005290:\n"
"/*0x80005290*/  addiu   $sp, $sp, -0x0040\n"
"/*0x80005294*/  sd      $s0, 0x0030($sp)\n"
"/*0x80005298*/  addiu   $s0, $sp, 0x0020\n"
"/*0x8000529C*/  daddu   $a1, $a0, $zero\n"
"/*0x800052A0*/  daddu   $a0, $s0, $zero\n"
"/*0x800052A4*/  sd      $ra, 0x0038($sp)\n"
"/*0x800052A8*/  jal     sys_80005860\n"
"/*0x800052AC*/  sb      $zero, 0x0020($sp)\n"
"/*0x800052B0*/  lbu     $v0, 0x0020($sp)\n"
"/*0x800052B4*/  beqz    $v0, .L800052E8\n"
"/*0x800052B8*/  lui     $a2, %hi(g_disp_ptr)\n"
"/*0x800052BC*/  lw      $v1, %lo(g_disp_ptr)($a2)\n"
"/*0x800052C0*/  lui     $a1, %hi(g_cur_pal)\n"
"/*0x800052C4*/  nop\n"
".L800052C8:\n"
"/*0x800052C8*/  lhu     $a0, %lo(g_cur_pal)($a1)\n"
"/*0x800052CC*/  addiu   $s0, $s0, 0x0001\n"
"/*0x800052D0*/  addu    $v0, $v0, $a0\n"
"/*0x800052D4*/  sh      $v0, 0x0000($v1)\n"
"/*0x800052D8*/  lbu     $v0, 0x0000($s0)\n"
"/*0x800052DC*/  bnez    $v0, .L800052C8\n"
"/*0x800052E0*/  addiu   $v1, $v1, 0x0002\n"
"/*0x800052E4*/  sw      $v1, %lo(g_disp_ptr)($a2)\n"
".L800052E8:\n"
"/*0x800052E8*/  ld      $ra, 0x0038($sp)\n"
"/*0x800052EC*/  ld      $s0, 0x0030($sp)\n"
"/*0x800052F0*/  jr      $ra\n"
"/*0x800052F4*/  addiu   $sp, $sp, 0x0040\n"
"\n"
".set reorder\n"
".set at\n"
);

void sys_80005158();
void sys_80005618(int);

void sys_800052F8(u16 *buff)
{
    g_buff = buff;
    sys_80005158();
    sys_80005618(0);
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005320\n"
"sys_80005320:\n"
"/*0x80005320*/  srl     $a2, $a2, 1\n"
"/*0x80005324*/  beqz    $a2, .L80005340\n"
"/*0x80005328*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x8000532C*/  nop\n"
".L80005330:\n"
"/*0x80005330*/  addiu   $a2, $a2, -0x0001\n"
"/*0x80005334*/  sh      $a1, 0x0000($a0)\n"
"/*0x80005338*/  bnez    $a2, .L80005330\n"
"/*0x8000533C*/  addiu   $a0, $a0, 0x0002\n"
".L80005340:\n"
"/*0x80005340*/  jr      $ra\n"
"/*0x80005344*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

void memcopy(const void *src, void *dest, unsigned long n)
{
    memcpy(dest, src, n);
}

void memfill(void *s, u8 c, unsigned long n)
{
    memset(s, c, n);
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005360\n"
"sys_80005360:\n"
"/*0x80005360*/  j       .L80005370\n"
"/*0x80005364*/  andi    $a2, $a2, 0x00FF\n"
".L80005368:\n"
"/*0x80005368*/  addiu   $a0, $a0, 0x0001\n"
"/*0x8000536C*/  addiu   $a1, $a1, 0x0001\n"
".L80005370:\n"
"/*0x80005370*/  beqz    $a2, .L80005390\n"
"/*0x80005374*/  addiu   $v0, $a2, -0x0001\n"
"/*0x80005378*/  lbu     $a3, 0x0000($a0)\n"
"/*0x8000537C*/  lbu     $v1, 0x0000($a1)\n"
"/*0x80005380*/  beq     $a3, $v1, .L80005368\n"
"/*0x80005384*/  andi    $a2, $v0, 0x00FF\n"
"/*0x80005388*/  jr      $ra\n"
"/*0x8000538C*/  daddu   $v0, $zero, $zero\n"
".L80005390:\n"
"/*0x80005390*/  jr      $ra\n"
"/*0x80005394*/  li      $v0, 0x0001\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"sys_80005398:\n"
"/*0x80005398*/  lbu     $v0, 0x0000($a0)\n"
"/*0x8000539C*/  beqz    $v0, .L800053BC\n"
"/*0x800053A0*/  daddu   $v1, $zero, $zero\n"
"/*0x800053A4*/  addiu   $v0, $a0, 0x0001\n"
".L800053A8:\n"
"/*0x800053A8*/  lbu     $a2, 0x0000($v0)\n"
"/*0x800053AC*/  addiu   $v1, $v1, 0x0001\n"
"/*0x800053B0*/  andi    $v1, $v1, 0x00FF\n"
"/*0x800053B4*/  bnez    $a2, .L800053A8\n"
"/*0x800053B8*/  addiu   $v0, $v0, 0x0001\n"
".L800053BC:\n"
"/*0x800053BC*/  lbu     $v0, 0x0000($a1)\n"
"/*0x800053C0*/  beqz    $v0, .L800053E4\n"
"/*0x800053C4*/  daddu   $a2, $zero, $zero\n"
"/*0x800053C8*/  addiu   $v0, $a1, 0x0001\n"
"/*0x800053CC*/  nop\n"
".L800053D0:\n"
"/*0x800053D0*/  lbu     $a3, 0x0000($v0)\n"
"/*0x800053D4*/  addiu   $a2, $a2, 0x0001\n"
"/*0x800053D8*/  andi    $a2, $a2, 0x00FF\n"
"/*0x800053DC*/  bnez    $a3, .L800053D0\n"
"/*0x800053E0*/  addiu   $v0, $v0, 0x0001\n"
".L800053E4:\n"
"/*0x800053E4*/  beq     $v1, $a2, .L80005400\n"
"/*0x800053E8*/  nop\n"
".L800053EC:\n"
"/*0x800053EC*/  jr      $ra\n"
"/*0x800053F0*/  daddu   $v0, $zero, $zero\n"
"/*0x800053F4*/  nop\n"
".L800053F8:\n"
"/*0x800053F8*/  bne     $a2, $v0, .L800053EC\n"
"/*0x800053FC*/  addiu   $a1, $a1, 0x0001\n"
".L80005400:\n"
"/*0x80005400*/  beqz    $v1, .L80005448\n"
"/*0x80005404*/  addiu   $v0, $v1, -0x0001\n"
"/*0x80005408*/  lbu     $a2, 0x0000($a0)\n"
"/*0x8000540C*/  andi    $v1, $v0, 0x00FF\n"
"/*0x80005410*/  addiu   $a3, $a2, -0x0041\n"
"/*0x80005414*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80005418*/  sltiu   $a3, $a3, 0x001A\n"
"/*0x8000541C*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005420*/  beqz    $a3, .L8000542C\n"
"/*0x80005424*/  lbu     $v0, 0x0000($a1)\n"
"/*0x80005428*/  ori     $a2, $a2, 0x0020\n"
".L8000542C:\n"
"/*0x8000542C*/  addiu   $a3, $v0, -0x0041\n"
"/*0x80005430*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80005434*/  sltiu   $a3, $a3, 0x001A\n"
"/*0x80005438*/  bnezl   $a3, .L800053F8\n"
"/*0x8000543C*/  ori     $v0, $v0, 0x0020\n"
"/*0x80005440*/  j       .L800053F8\n"
"/*0x80005444*/  nop\n"
".L80005448:\n"
"/*0x80005448*/  jr      $ra\n"
"/*0x8000544C*/  li      $v0, 0x0001\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005450\n"
"sys_80005450:\n"
"/*0x80005450*/  andi    $a2, $a2, 0x00FF\n"
"/*0x80005454*/  beqz    $a2, .L800054A8\n"
"/*0x80005458*/  addiu   $v0, $a2, -0x0001\n"
"/*0x8000545C*/  nop\n"
".L80005460:\n"
"/*0x80005460*/  lbu     $v1, 0x0000($a0)\n"
"/*0x80005464*/  andi    $a2, $v0, 0x00FF\n"
"/*0x80005468*/  addiu   $a3, $v1, -0x0041\n"
"/*0x8000546C*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80005470*/  sltiu   $a3, $a3, 0x001A\n"
"/*0x80005474*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005478*/  beqz    $a3, .L80005484\n"
"/*0x8000547C*/  lbu     $v0, 0x0000($a1)\n"
"/*0x80005480*/  ori     $v1, $v1, 0x0020\n"
".L80005484:\n"
"/*0x80005484*/  addiu   $a3, $v0, -0x0041\n"
"/*0x80005488*/  andi    $a3, $a3, 0x00FF\n"
"/*0x8000548C*/  sltiu   $a3, $a3, 0x001A\n"
"/*0x80005490*/  bnezl   $a3, .L80005498\n"
"/*0x80005494*/  ori     $v0, $v0, 0x0020\n"
".L80005498:\n"
"/*0x80005498*/  bne     $v1, $v0, .L800054B0\n"
"/*0x8000549C*/  addiu   $a1, $a1, 0x0001\n"
"/*0x800054A0*/  bnez    $a2, .L80005460\n"
"/*0x800054A4*/  addiu   $v0, $a2, -0x0001\n"
".L800054A8:\n"
"/*0x800054A8*/  jr      $ra\n"
"/*0x800054AC*/  li      $v0, 0x0001\n"
".L800054B0:\n"
"/*0x800054B0*/  jr      $ra\n"
"/*0x800054B4*/  daddu   $v0, $zero, $zero\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_800054B8\n"
"sys_800054B8:\n"
"/*0x800054B8*/  lbu     $v1, 0x0000($a0)\n"
"/*0x800054BC*/  beqz    $v1, .L800054DC\n"
"/*0x800054C0*/  daddu   $v0, $zero, $zero\n"
"/*0x800054C4*/  addiu   $a0, $a0, 0x0001\n"
".L800054C8:\n"
"/*0x800054C8*/  lbu     $v1, 0x0000($a0)\n"
"/*0x800054CC*/  addiu   $v0, $v0, 0x0001\n"
"/*0x800054D0*/  andi    $v0, $v0, 0x00FF\n"
"/*0x800054D4*/  bnez    $v1, .L800054C8\n"
"/*0x800054D8*/  addiu   $a0, $a0, 0x0001\n"
".L800054DC:\n"
"/*0x800054DC*/  jr      $ra\n"
"/*0x800054E0*/  nop\n"
"/*0x800054E4*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_800054E8\n"
"sys_800054E8:\n"
"/*0x800054E8*/  lbu     $v0, 0x0000($a0)\n"
"/*0x800054EC*/  lbu     $v1, 0x0000($a1)\n"
"/*0x800054F0*/  xori    $v0, $v0, 0x002E\n"
"/*0x800054F4*/  sltiu   $v0, $v0, 0x0001\n"
"/*0x800054F8*/  beqz    $v1, .L80005528\n"
"/*0x800054FC*/  addu    $a0, $a0, $v0\n"
"/*0x80005500*/  daddu   $a2, $zero, $zero\n"
"/*0x80005504*/  li      $v0, 0x002E\n"
".L80005508:\n"
"/*0x80005508*/  beql    $v1, $v0, .L80005510\n"
"/*0x8000550C*/  daddu   $a2, $a1, $zero\n"
".L80005510:\n"
"/*0x80005510*/  addiu   $a1, $a1, 0x0001\n"
"/*0x80005514*/  lbu     $v1, 0x0000($a1)\n"
"/*0x80005518*/  bnez    $v1, .L80005508\n"
"/*0x8000551C*/  nop\n"
"/*0x80005520*/  bnezl   $a2, .L80005528\n"
"/*0x80005524*/  addiu   $a1, $a2, 0x0001\n"
".L80005528:\n"
"/*0x80005528*/  j       sys_80005398\n"
"/*0x8000552C*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005530*/  addiu   $sp, $sp, -0x0038\n"
"/*0x80005534*/  sd      $s0, 0x0020($sp)\n"
"/*0x80005538*/  daddu   $s0, $a0, $zero\n"
"/*0x8000553C*/  lw      $a0, 0x0000($a0)\n"
"/*0x80005540*/  sd      $s1, 0x0028($sp)\n"
"/*0x80005544*/  sd      $ra, 0x0030($sp)\n"
"/*0x80005548*/  bnez    $a0, .L80005560\n"
"/*0x8000554C*/  daddu   $s1, $a1, $zero\n"
"/*0x80005550*/  j       .L80005590\n"
"/*0x80005554*/  ld      $ra, 0x0030($sp)\n"
".L80005558:\n"
"/*0x80005558*/  beqz    $a0, .L80005590\n"
"/*0x8000555C*/  ld      $ra, 0x0030($sp)\n"
".L80005560:\n"
"/*0x80005560*/  daddu   $a1, $s1, $zero\n"
"/*0x80005564*/  jal     sys_800054E8\n"
"/*0x80005568*/  addiu   $s0, $s0, 0x0004\n"
"/*0x8000556C*/  beqzl   $v0, .L80005558\n"
"/*0x80005570*/  lw      $a0, 0x0000($s0)\n"
"/*0x80005574*/  ld      $ra, 0x0030($sp)\n"
"/*0x80005578*/  li      $v0, 0x0001\n"
"/*0x8000557C*/  ld      $s1, 0x0028($sp)\n"
"/*0x80005580*/  ld      $s0, 0x0020($sp)\n"
"/*0x80005584*/  jr      $ra\n"
"/*0x80005588*/  addiu   $sp, $sp, 0x0038\n"
"/*0x8000558C*/  nop\n"
".L80005590:\n"
"/*0x80005590*/  daddu   $v0, $zero, $zero\n"
"/*0x80005594*/  ld      $s1, 0x0028($sp)\n"
"/*0x80005598*/  ld      $s0, 0x0020($sp)\n"
"/*0x8000559C*/  jr      $ra\n"
"/*0x800055A0*/  addiu   $sp, $sp, 0x0038\n"
"/*0x800055A4*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x800055A8*/  lbu     $v1, 0x0000($a0)\n"
"/*0x800055AC*/  daddu   $v0, $a0, $zero\n"
"/*0x800055B0*/  li      $a1, 0x002F\n"
"/*0x800055B4*/  beqz    $v1, .L800055D4\n"
"/*0x800055B8*/  addiu   $a0, $a0, 0x0002\n"
"/*0x800055BC*/  lbu     $v1, -0x0001($a0)\n"
".L800055C0:\n"
"/*0x800055C0*/  beql    $v1, $a1, .L800055E0\n"
"/*0x800055C4*/  daddu   $v0, $a0, $zero\n"
"/*0x800055C8*/  addiu   $a0, $a0, 0x0001\n"
".L800055CC:\n"
"/*0x800055CC*/  bnezl   $v1, .L800055C0\n"
"/*0x800055D0*/  lbu     $v1, -0x0001($a0)\n"
".L800055D4:\n"
"/*0x800055D4*/  jr      $ra\n"
"/*0x800055D8*/  nop\n"
"/*0x800055DC*/  nop\n"
".L800055E0:\n"
"/*0x800055E0*/  j       .L800055CC\n"
"/*0x800055E4*/  addiu   $a0, $a0, 0x0001\n"
"/*0x800055E8*/  lbu     $v0, 0x0000($a0)\n"
"/*0x800055EC*/  beqz    $v0, .L8000560C\n"
"/*0x800055F0*/  nop\n"
"/*0x800055F4*/  nop\n"
".L800055F8:\n"
"/*0x800055F8*/  sb      $v0, 0x0000($a1)\n"
"/*0x800055FC*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005600*/  lbu     $v0, 0x0000($a0)\n"
"/*0x80005604*/  bnez    $v0, .L800055F8\n"
"/*0x80005608*/  addiu   $a1, $a1, 0x0001\n"
".L8000560C:\n"
"/*0x8000560C*/  jr      $ra\n"
"/*0x80005610*/  sb      $zero, 0x0000($a1)\n"
"/*0x80005614*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"sys_80005618:\n"
"/*0x80005618*/  lui     $v1, %hi(w_80029338)\n"
"/*0x8000561C*/  bnez    $a0, .L80005634\n"
"/*0x80005620*/  sw      $a0, %lo(w_80029338)($v1)\n"
"/*0x80005624*/  j       .L80005640\n"
"/*0x80005628*/  nop\n"
"/*0x8000562C*/  nop\n"
".L80005630:\n"
"/*0x80005630*/  sw      $a0, %lo(w_80029338)($v1)\n"
".L80005634:\n"
"/*0x80005634*/  lbu     $v0, 0x0000($a0)\n"
"/*0x80005638*/  bnez    $v0, .L80005630\n"
"/*0x8000563C*/  addiu   $a0, $a0, 0x0001\n"
".L80005640:\n"
"/*0x80005640*/  jr      $ra\n"
"/*0x80005644*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005648*/  lbu     $v1, 0x0000($a0)\n"
"/*0x8000564C*/  beqz    $v1, .L8000567C\n"
"/*0x80005650*/  lui     $a1, %hi(w_80029338)\n"
"/*0x80005654*/  lw      $v0, %lo(w_80029338)($a1)\n"
"/*0x80005658*/  sb      $v1, 0x0000($v0)\n"
".L8000565C:\n"
"/*0x8000565C*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005660*/  sw      $v0, %lo(w_80029338)($a1)\n"
"/*0x80005664*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005668*/  lbu     $v1, 0x0000($a0)\n"
"/*0x8000566C*/  bnezl   $v1, .L8000565C\n"
"/*0x80005670*/  sb      $v1, 0x0000($v0)\n"
".L80005674:\n"
"/*0x80005674*/  jr      $ra\n"
"/*0x80005678*/  sb      $zero, 0x0000($v0)\n"
".L8000567C:\n"
"/*0x8000567C*/  lui     $v0, %hi(w_80029338)\n"
"/*0x80005680*/  j       .L80005674\n"
"/*0x80005684*/  lw      $v0, %lo(w_80029338)($v0)\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005688\n"
"sys_80005688:\n"
"/*0x80005688*/  addiu   $sp, $sp, -0x0010\n"
"/*0x8000568C*/  sb      $zero, 0x000A($sp)\n"
"/*0x80005690*/  bnez    $a0, .L800056D8\n"
"/*0x80005694*/  addiu   $v0, $sp, 0x000A\n"
"/*0x80005698*/  li      $v0, 0x0030\n"
"/*0x8000569C*/  sb      $v0, 0x0009($sp)\n"
"/*0x800056A0*/  li      $v1, 0x0030\n"
"/*0x800056A4*/  addiu   $v0, $sp, 0x0009\n"
"/*0x800056A8*/  lui     $a1, %hi(w_80029338)\n"
".L800056AC:\n"
"/*0x800056AC*/  lw      $a0, %lo(w_80029338)($a1)\n"
".L800056B0:\n"
"/*0x800056B0*/  sb      $v1, 0x0000($a0)\n"
"/*0x800056B4*/  addiu   $v0, $v0, 0x0001\n"
"/*0x800056B8*/  lbu     $v1, 0x0000($v0)\n"
"/*0x800056BC*/  addiu   $a0, $a0, 0x0001\n"
"/*0x800056C0*/  bnez    $v1, .L800056B0\n"
"/*0x800056C4*/  sw      $a0, %lo(w_80029338)($a1)\n"
".L800056C8:\n"
"/*0x800056C8*/  sb      $zero, 0x0000($a0)\n"
"/*0x800056CC*/  jr      $ra\n"
"/*0x800056D0*/  addiu   $sp, $sp, 0x0010\n"
"/*0x800056D4*/  nop\n"
".L800056D8:\n"
"/*0x800056D8*/  dsll32  $v1, $a0, 0\n"
"/*0x800056DC*/  dsrl32  $v1, $v1, 0\n"
"/*0x800056E0*/  dsll    $a2, $v1, 4\n"
"/*0x800056E4*/  dsll    $a1, $v1, 2\n"
"/*0x800056E8*/  dsubu   $a1, $a2, $a1\n"
"/*0x800056EC*/  dsll    $a2, $a1, 4\n"
"/*0x800056F0*/  daddu   $a1, $a1, $a2\n"
"/*0x800056F4*/  dsll    $a2, $a1, 8\n"
"/*0x800056F8*/  daddu   $a1, $a1, $a2\n"
"/*0x800056FC*/  dsll    $a2, $a1, 16\n"
"/*0x80005700*/  daddu   $a1, $a1, $a2\n"
"/*0x80005704*/  daddu   $a1, $a1, $v1\n"
"/*0x80005708*/  dsrl32  $a1, $a1, 3\n"
"/*0x8000570C*/  sll     $a2, $a1, 1\n"
"/*0x80005710*/  sll     $v1, $a1, 3\n"
"/*0x80005714*/  addu    $v1, $a2, $v1\n"
"/*0x80005718*/  subu    $v1, $a0, $v1\n"
"/*0x8000571C*/  addiu   $v1, $v1, 0x0030\n"
"/*0x80005720*/  addiu   $v0, $v0, -0x0001\n"
"/*0x80005724*/  andi    $v1, $v1, 0x00FF\n"
"/*0x80005728*/  daddu   $a0, $a1, $zero\n"
"/*0x8000572C*/  bnez    $a1, .L800056D8\n"
"/*0x80005730*/  sb      $v1, 0x0000($v0)\n"
"/*0x80005734*/  bnezl   $v1, .L800056AC\n"
"/*0x80005738*/  li      $a1, 0x80030000\n"
"/*0x8000573C*/  lui     $v0, %hi(w_80029338)\n"
"/*0x80005740*/  j       .L800056C8\n"
"/*0x80005744*/  lw      $a0, %lo(w_80029338)($v0)\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005748\n"
"sys_80005748:\n"
"/*0x80005748*/  andi    $a0, $a0, 0x00FF\n"
"/*0x8000574C*/  srl     $a1, $a0, 4\n"
"/*0x80005750*/  sltiu   $v0, $a1, 0x000A\n"
"/*0x80005754*/  andi    $a0, $a0, 0x000F\n"
"/*0x80005758*/  bnez    $v0, .L80005764\n"
"/*0x8000575C*/  li      $a3, 0x0030\n"
"/*0x80005760*/  li      $a3, 0x0037\n"
".L80005764:\n"
"/*0x80005764*/  sltiu   $v0, $a0, 0x000A\n"
"/*0x80005768*/  bnez    $v0, .L80005774\n"
"/*0x8000576C*/  li      $a2, 0x0030\n"
"/*0x80005770*/  li      $a2, 0x0037\n"
".L80005774:\n"
"/*0x80005774*/  lui     $v1, %hi(w_80029338)\n"
"/*0x80005778*/  lw      $v0, %lo(w_80029338)($v1)\n"
"/*0x8000577C*/  addu    $a1, $a3, $a1\n"
"/*0x80005780*/  addu    $a0, $a2, $a0\n"
"/*0x80005784*/  addiu   $t0, $v0, 0x0002\n"
"/*0x80005788*/  sb      $a1, 0x0000($v0)\n"
"/*0x8000578C*/  sb      $a0, 0x0001($v0)\n"
"/*0x80005790*/  sw      $t0, %lo(w_80029338)($v1)\n"
"/*0x80005794*/  jr      $ra\n"
"/*0x80005798*/  sb      $zero, 0x0002($v0)\n"
"/*0x8000579C*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_800057A0\n"
"sys_800057A0:\n"
"/*0x800057A0*/  addiu   $sp, $sp, -0x0030\n"
"/*0x800057A4*/  sd      $s0, 0x0020($sp)\n"
"/*0x800057A8*/  andi    $s0, $a0, 0xFFFF\n"
"/*0x800057AC*/  sd      $ra, 0x0028($sp)\n"
"/*0x800057B0*/  jal     sys_80005748\n"
"/*0x800057B4*/  srl     $a0, $s0, 8\n"
"/*0x800057B8*/  andi    $a0, $s0, 0x00FF\n"
"/*0x800057BC*/  ld      $ra, 0x0028($sp)\n"
"/*0x800057C0*/  ld      $s0, 0x0020($sp)\n"
"/*0x800057C4*/  j       sys_80005748\n"
"/*0x800057C8*/  addiu   $sp, $sp, 0x0030\n"
"/*0x800057CC*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x800057D0*/  addiu   $sp, $sp, -0x0030\n"
"/*0x800057D4*/  sd      $s0, 0x0020($sp)\n"
"/*0x800057D8*/  daddu   $s0, $a0, $zero\n"
"/*0x800057DC*/  sd      $ra, 0x0028($sp)\n"
"/*0x800057E0*/  jal     sys_800057A0\n"
"/*0x800057E4*/  srl     $a0, $a0, 16\n"
"/*0x800057E8*/  andi    $a0, $s0, 0xFFFF\n"
"/*0x800057EC*/  ld      $ra, 0x0028($sp)\n"
"/*0x800057F0*/  ld      $s0, 0x0020($sp)\n"
"/*0x800057F4*/  j       sys_800057A0\n"
"/*0x800057F8*/  addiu   $sp, $sp, 0x0030\n"
"/*0x800057FC*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005800*/  lui     $v0, %hi(w_80029338)\n"
"/*0x80005804*/  lw      $a2, %lo(w_80029338)($v0)\n"
"/*0x80005808*/  bnez    $a0, .L8000581C\n"
"/*0x8000580C*/  sw      $a0, %lo(w_80029338)($v0)\n"
"/*0x80005810*/  j       .L80005848\n"
"/*0x80005814*/  lbu     $v1, 0x0000($a1)\n"
".L80005818:\n"
"/*0x80005818*/  sw      $a0, %lo(w_80029338)($v0)\n"
".L8000581C:\n"
"/*0x8000581C*/  lbu     $v1, 0x0000($a0)\n"
"/*0x80005820*/  bnezl   $v1, .L80005818\n"
"/*0x80005824*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005828*/  lbu     $v1, 0x0000($a1)\n"
"/*0x8000582C*/  beqzl   $v1, .L80005854\n"
"/*0x80005830*/  sb      $zero, 0x0000($a0)\n"
"/*0x80005834*/  sb      $v1, 0x0000($a0)\n"
".L80005838:\n"
"/*0x80005838*/  addiu   $a0, $a0, 0x0001\n"
"/*0x8000583C*/  sw      $a0, %lo(w_80029338)($v0)\n"
"/*0x80005840*/  addiu   $a1, $a1, 0x0001\n"
"/*0x80005844*/  lbu     $v1, 0x0000($a1)\n"
".L80005848:\n"
"/*0x80005848*/  bnezl   $v1, .L80005838\n"
"/*0x8000584C*/  sb      $v1, 0x0000($a0)\n"
"/*0x80005850*/  sb      $zero, 0x0000($a0)\n"
".L80005854:\n"
"/*0x80005854*/  jr      $ra\n"
"/*0x80005858*/  sw      $a2, %lo(w_80029338)($v0)\n"
"/*0x8000585C*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_80005860\n"
"sys_80005860:\n"
"/*0x80005860*/  addiu   $sp, $sp, -0x0038\n"
"/*0x80005864*/  daddu   $v0, $a0, $zero\n"
"/*0x80005868*/  sd      $s0, 0x0020($sp)\n"
"/*0x8000586C*/  lui     $s0, %hi(w_80029338)\n"
"/*0x80005870*/  sd      $s1, 0x0028($sp)\n"
"/*0x80005874*/  sd      $ra, 0x0030($sp)\n"
"/*0x80005878*/  lw      $s1, %lo(w_80029338)($s0)\n"
"/*0x8000587C*/  daddu   $a0, $a1, $zero\n"
"/*0x80005880*/  bnez    $v0, .L80005894\n"
"/*0x80005884*/  sw      $v0, %lo(w_80029338)($s0)\n"
"/*0x80005888*/  j       .L800058A0\n"
"/*0x8000588C*/  nop\n"
".L80005890:\n"
"/*0x80005890*/  sw      $v0, %lo(w_80029338)($s0)\n"
".L80005894:\n"
"/*0x80005894*/  lbu     $v1, 0x0000($v0)\n"
"/*0x80005898*/  bnez    $v1, .L80005890\n"
"/*0x8000589C*/  addiu   $v0, $v0, 0x0001\n"
".L800058A0:\n"
"/*0x800058A0*/  jal     sys_80005688\n"
"/*0x800058A4*/  nop\n"
"/*0x800058A8*/  ld      $ra, 0x0030($sp)\n"
"/*0x800058AC*/  sw      $s1, %lo(w_80029338)($s0)\n"
"/*0x800058B0*/  ld      $s1, 0x0028($sp)\n"
"/*0x800058B4*/  ld      $s0, 0x0020($sp)\n"
"/*0x800058B8*/  jr      $ra\n"
"/*0x800058BC*/  addiu   $sp, $sp, 0x0038\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x800058C0*/  beqz    $a0, .L8000592C\n"
"/*0x800058C4*/  nop\n"
"/*0x800058C8*/  lw      $v1, 0x0000($a0)\n"
"/*0x800058CC*/  beqz    $v1, .L8000592C\n"
"/*0x800058D0*/  daddu   $a3, $zero, $zero\n"
"/*0x800058D4*/  daddu   $v0, $zero, $zero\n"
".L800058D8:\n"
"/*0x800058D8*/  lbu     $a2, 0x0000($v1)\n"
"/*0x800058DC*/  daddu   $a1, $zero, $zero\n"
"/*0x800058E0*/  beqz    $a2, .L800058FC\n"
"/*0x800058E4*/  addiu   $v1, $v1, 0x0001\n"
".L800058E8:\n"
"/*0x800058E8*/  lbu     $a2, 0x0000($v1)\n"
"/*0x800058EC*/  addiu   $a1, $a1, 0x0001\n"
"/*0x800058F0*/  andi    $a1, $a1, 0x00FF\n"
"/*0x800058F4*/  bnez    $a2, .L800058E8\n"
"/*0x800058F8*/  addiu   $v1, $v1, 0x0001\n"
".L800058FC:\n"
"/*0x800058FC*/  sltu    $v1, $a1, $v0\n"
"/*0x80005900*/  bnezl   $v1, .L80005908\n"
"/*0x80005904*/  daddu   $a1, $v0, $zero\n"
".L80005908:\n"
"/*0x80005908*/  addiu   $a3, $a3, 0x0001\n"
"/*0x8000590C*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80005910*/  sll     $v0, $a3, 2\n"
"/*0x80005914*/  addu    $v0, $a0, $v0\n"
"/*0x80005918*/  lw      $v1, 0x0000($v0)\n"
"/*0x8000591C*/  bnez    $v1, .L800058D8\n"
"/*0x80005920*/  daddu   $v0, $a1, $zero\n"
"/*0x80005924*/  jr      $ra\n"
"/*0x80005928*/  nop\n"
".L8000592C:\n"
"/*0x8000592C*/  jr      $ra\n"
"/*0x80005930*/  daddu   $v0, $zero, $zero\n"
"/*0x80005934*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005938*/  lbu     $v0, 0x0000($a0)\n"
"/*0x8000593C*/  beqz    $v0, .L8000598C\n"
"/*0x80005940*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80005944*/  beqz    $a1, .L8000598C\n"
"/*0x80005948*/  li      $a2, -0x0021\n"
"/*0x8000594C*/  j       .L80005960\n"
"/*0x80005950*/  addiu   $v1, $v0, -0x0061\n"
"/*0x80005954*/  nop\n"
".L80005958:\n"
"/*0x80005958*/  beqz    $a1, .L8000598C\n"
"/*0x8000595C*/  addiu   $v1, $v0, -0x0061\n"
".L80005960:\n"
"/*0x80005960*/  andi    $v1, $v1, 0x00FF\n"
"/*0x80005964*/  addiu   $a1, $a1, -0x0001\n"
"/*0x80005968*/  sltiu   $v1, $v1, 0x001A\n"
"/*0x8000596C*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80005970*/  beqz    $v1, .L8000597C\n"
"/*0x80005974*/  and     $v0, $v0, $a2\n"
"/*0x80005978*/  sb      $v0, 0x0000($a0)\n"
".L8000597C:\n"
"/*0x8000597C*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005980*/  lbu     $v0, 0x0000($a0)\n"
"/*0x80005984*/  bnez    $v0, .L80005958\n"
"/*0x80005988*/  nop\n"
".L8000598C:\n"
"/*0x8000598C*/  jr      $ra\n"
"/*0x80005990*/  nop\n"
"/*0x80005994*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005998*/  lbu     $v1, 0x0000($a0)\n"
"/*0x8000599C*/  beqz    $v1, .L800059D0\n"
"/*0x800059A0*/  daddu   $v0, $a0, $zero\n"
"/*0x800059A4*/  daddu   $a1, $zero, $zero\n"
"/*0x800059A8*/  li      $a0, 0x002E\n"
"/*0x800059AC*/  nop\n"
".L800059B0:\n"
"/*0x800059B0*/  beql    $v1, $a0, .L800059B8\n"
"/*0x800059B4*/  daddu   $a1, $v0, $zero\n"
".L800059B8:\n"
"/*0x800059B8*/  addiu   $v0, $v0, 0x0001\n"
"/*0x800059BC*/  lbu     $v1, 0x0000($v0)\n"
"/*0x800059C0*/  bnez    $v1, .L800059B0\n"
"/*0x800059C4*/  nop\n"
"/*0x800059C8*/  bnezl   $a1, .L800059D0\n"
"/*0x800059CC*/  addiu   $v0, $a1, 0x0001\n"
".L800059D0:\n"
"/*0x800059D0*/  jr      $ra\n"
"/*0x800059D4*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
".globl sys_800059D8\n"
"sys_800059D8:\n"
"/*0x800059D8*/  lbu     $v1, 0x0000($a0)\n"
"/*0x800059DC*/  li      $v0, 0x0020\n"
"/*0x800059E0*/  bnel    $v1, $v0, .L80005A04\n"
"/*0x800059E4*/  sb      $zero, 0x0001($a0)\n"
"/*0x800059E8*/  li      $v1, 0x0020\n"
"/*0x800059EC*/  sb      $zero, 0x0000($a0)\n"
".L800059F0:\n"
"/*0x800059F0*/  addiu   $a0, $a0, -0x0001\n"
"/*0x800059F4*/  lbu     $v0, 0x0000($a0)\n"
"/*0x800059F8*/  beql    $v0, $v1, .L800059F0\n"
"/*0x800059FC*/  sb      $zero, 0x0000($a0)\n"
"/*0x80005A00*/  sb      $zero, 0x0001($a0)\n"
".L80005A04:\n"
"/*0x80005A04*/  jr      $ra\n"
"/*0x80005A08*/  addiu   $v0, $a0, 0x0001\n"
"/*0x80005A0C*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005A10*/  lbu     $v1, 0x0000($a0)\n"
"/*0x80005A14*/  li      $v0, 0x0020\n"
"/*0x80005A18*/  bne     $v1, $v0, .L80005AAC\n"
"/*0x80005A1C*/  daddu   $v0, $a0, $zero\n"
"/*0x80005A20*/  daddu   $a2, $zero, $zero\n"
"/*0x80005A24*/  li      $a1, 0x0020\n"
".L80005A28:\n"
"/*0x80005A28*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005A2C*/  lbu     $v1, 0x0000($v0)\n"
"/*0x80005A30*/  addiu   $a2, $a2, 0x0001\n"
"/*0x80005A34*/  beq     $v1, $a1, .L80005A28\n"
"/*0x80005A38*/  andi    $a2, $a2, 0x00FF\n"
".L80005A3C:\n"
"/*0x80005A3C*/  beqz    $v1, .L80005AA4\n"
"/*0x80005A40*/  daddu   $a1, $zero, $zero\n"
"/*0x80005A44*/  daddu   $a3, $zero, $zero\n"
"/*0x80005A48*/  li      $t0, 0x0020\n"
"/*0x80005A4C*/  addiu   $a1, $a1, 0x0001\n"
".L80005A50:\n"
"/*0x80005A50*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005A54*/  beq     $v1, $t0, .L80005A60\n"
"/*0x80005A58*/  andi    $a1, $a1, 0x00FF\n"
"/*0x80005A5C*/  daddu   $a3, $a1, $zero\n"
".L80005A60:\n"
"/*0x80005A60*/  lbu     $v1, 0x0000($v0)\n"
"/*0x80005A64*/  bnezl   $v1, .L80005A50\n"
"/*0x80005A68*/  addiu   $a1, $a1, 0x0001\n"
"/*0x80005A6C*/  beqz    $a3, .L80005AA4\n"
"/*0x80005A70*/  addiu   $a3, $a3, -0x0001\n"
"/*0x80005A74*/  andi    $a3, $a3, 0x00FF\n"
"/*0x80005A78*/  addu    $a2, $a0, $a2\n"
"/*0x80005A7C*/  addiu   $a3, $a3, 0x0001\n"
"/*0x80005A80*/  daddu   $v0, $zero, $zero\n"
"/*0x80005A84*/  nop\n"
".L80005A88:\n"
"/*0x80005A88*/  addu    $v1, $a2, $v0\n"
"/*0x80005A8C*/  lbu     $a1, 0x0000($v1)\n"
"/*0x80005A90*/  addu    $v1, $a0, $v0\n"
"/*0x80005A94*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005A98*/  bne     $v0, $a3, .L80005A88\n"
"/*0x80005A9C*/  sb      $a1, 0x0000($v1)\n"
"/*0x80005AA0*/  addu    $a0, $a0, $v0\n"
".L80005AA4:\n"
"/*0x80005AA4*/  jr      $ra\n"
"/*0x80005AA8*/  sb      $zero, 0x0000($a0)\n"
".L80005AAC:\n"
"/*0x80005AAC*/  j       .L80005A3C\n"
"/*0x80005AB0*/  daddu   $a2, $zero, $zero\n"
"/*0x80005AB4*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005AB8*/  addiu   $sp, $sp, -0x0048\n"
"/*0x80005ABC*/  sd      $s2, 0x0030($sp)\n"
"/*0x80005AC0*/  andi    $s2, $a0, 0xFFFF\n"
"/*0x80005AC4*/  srl     $a0, $s2, 11\n"
"/*0x80005AC8*/  sd      $ra, 0x0040($sp)\n"
"/*0x80005ACC*/  sd      $s3, 0x0038($sp)\n"
"/*0x80005AD0*/  sd      $s0, 0x0020($sp)\n"
"/*0x80005AD4*/  jal     sys_800065B8\n"
"/*0x80005AD8*/  sd      $s1, 0x0028($sp)\n"
"/*0x80005ADC*/  lui     $s3, %hi(u_800287E8)\n"
"/*0x80005AE0*/  jal     sys_80005748\n"
"/*0x80005AE4*/  daddu   $a0, $v0, $zero\n"
"/*0x80005AE8*/  lbu     $v1, %lo(u_800287E8)($s3)\n"
"/*0x80005AEC*/  beqz    $v1, .L80005B88\n"
"/*0x80005AF0*/  addiu   $s0, $s3, %lo(u_800287E8)\n"
"/*0x80005AF4*/  lui     $s1, %hi(w_80029338)\n"
"/*0x80005AF8*/  lw      $v0, %lo(w_80029338)($s1)\n"
"/*0x80005AFC*/  daddu   $a0, $s0, $zero\n"
".L80005B00:\n"
"/*0x80005B00*/  sb      $v1, 0x0000($v0)\n"
"/*0x80005B04*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005B08*/  lbu     $v1, 0x0000($a0)\n"
"/*0x80005B0C*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005B10*/  bnez    $v1, .L80005B00\n"
"/*0x80005B14*/  sw      $v0, %lo(w_80029338)($s1)\n"
".L80005B18:\n"
"/*0x80005B18*/  srl     $a0, $s2, 5\n"
"/*0x80005B1C*/  andi    $a0, $a0, 0x003F\n"
"/*0x80005B20*/  jal     sys_800065B8\n"
"/*0x80005B24*/  sb      $zero, 0x0000($v0)\n"
"/*0x80005B28*/  jal     sys_80005748\n"
"/*0x80005B2C*/  daddu   $a0, $v0, $zero\n"
"/*0x80005B30*/  lbu     $v1, %lo(u_800287E8)($s3)\n"
"/*0x80005B34*/  beqz    $v1, .L80005B58\n"
"/*0x80005B38*/  lw      $v0, %lo(w_80029338)($s1)\n"
"/*0x80005B3C*/  nop\n"
".L80005B40:\n"
"/*0x80005B40*/  sb      $v1, 0x0000($v0)\n"
"/*0x80005B44*/  addiu   $s0, $s0, 0x0001\n"
"/*0x80005B48*/  lbu     $v1, 0x0000($s0)\n"
"/*0x80005B4C*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005B50*/  bnez    $v1, .L80005B40\n"
"/*0x80005B54*/  sw      $v0, %lo(w_80029338)($s1)\n"
".L80005B58:\n"
"/*0x80005B58*/  andi    $a0, $s2, 0x001F\n"
"/*0x80005B5C*/  sll     $a0, $a0, 1\n"
"/*0x80005B60*/  jal     sys_800065B8\n"
"/*0x80005B64*/  sb      $zero, 0x0000($v0)\n"
"/*0x80005B68*/  daddu   $a0, $v0, $zero\n"
"/*0x80005B6C*/  ld      $ra, 0x0040($sp)\n"
"/*0x80005B70*/  ld      $s3, 0x0038($sp)\n"
"/*0x80005B74*/  ld      $s2, 0x0030($sp)\n"
"/*0x80005B78*/  ld      $s1, 0x0028($sp)\n"
"/*0x80005B7C*/  ld      $s0, 0x0020($sp)\n"
"/*0x80005B80*/  j       sys_80005748\n"
"/*0x80005B84*/  addiu   $sp, $sp, 0x0048\n"
".L80005B88:\n"
"/*0x80005B88*/  lui     $s1, %hi(w_80029338)\n"
"/*0x80005B8C*/  j       .L80005B18\n"
"/*0x80005B90*/  lw      $v0, %lo(w_80029338)($s1)\n"
"/*0x80005B94*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005B98*/  addiu   $sp, $sp, -0x0048\n"
"/*0x80005B9C*/  sd      $s2, 0x0030($sp)\n"
"/*0x80005BA0*/  andi    $s2, $a0, 0xFFFF\n"
"/*0x80005BA4*/  andi    $a0, $s2, 0x001F\n"
"/*0x80005BA8*/  sd      $ra, 0x0040($sp)\n"
"/*0x80005BAC*/  sd      $s3, 0x0038($sp)\n"
"/*0x80005BB0*/  sd      $s0, 0x0020($sp)\n"
"/*0x80005BB4*/  jal     sys_800065B8\n"
"/*0x80005BB8*/  sd      $s1, 0x0028($sp)\n"
"/*0x80005BBC*/  lui     $s3, %hi(u_800287F0)\n"
"/*0x80005BC0*/  jal     sys_80005748\n"
"/*0x80005BC4*/  daddu   $a0, $v0, $zero\n"
"/*0x80005BC8*/  lbu     $v1, %lo(u_800287F0)($s3)\n"
"/*0x80005BCC*/  beqz    $v1, .L80005C60\n"
"/*0x80005BD0*/  addiu   $s0, $s3, %lo(u_800287F0)\n"
"/*0x80005BD4*/  lui     $s1, %hi(w_80029338)\n"
"/*0x80005BD8*/  lw      $v0, %lo(w_80029338)($s1)\n"
"/*0x80005BDC*/  daddu   $a0, $s0, $zero\n"
".L80005BE0:\n"
"/*0x80005BE0*/  sb      $v1, 0x0000($v0)\n"
"/*0x80005BE4*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80005BE8*/  lbu     $v1, 0x0000($a0)\n"
"/*0x80005BEC*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005BF0*/  bnez    $v1, .L80005BE0\n"
"/*0x80005BF4*/  sw      $v0, %lo(w_80029338)($s1)\n"
".L80005BF8:\n"
"/*0x80005BF8*/  srl     $a0, $s2, 5\n"
"/*0x80005BFC*/  andi    $a0, $a0, 0x000F\n"
"/*0x80005C00*/  jal     sys_800065B8\n"
"/*0x80005C04*/  sb      $zero, 0x0000($v0)\n"
"/*0x80005C08*/  jal     sys_80005748\n"
"/*0x80005C0C*/  daddu   $a0, $v0, $zero\n"
"/*0x80005C10*/  lbu     $v1, %lo(u_800287F0)($s3)\n"
"/*0x80005C14*/  beqz    $v1, .L80005C38\n"
"/*0x80005C18*/  lw      $v0, %lo(w_80029338)($s1)\n"
"/*0x80005C1C*/  nop\n"
".L80005C20:\n"
"/*0x80005C20*/  sb      $v1, 0x0000($v0)\n"
"/*0x80005C24*/  addiu   $s0, $s0, 0x0001\n"
"/*0x80005C28*/  lbu     $v1, 0x0000($s0)\n"
"/*0x80005C2C*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005C30*/  bnez    $v1, .L80005C20\n"
"/*0x80005C34*/  sw      $v0, %lo(w_80029338)($s1)\n"
".L80005C38:\n"
"/*0x80005C38*/  sb      $zero, 0x0000($v0)\n"
"/*0x80005C3C*/  srl     $a0, $s2, 9\n"
"/*0x80005C40*/  ld      $ra, 0x0040($sp)\n"
"/*0x80005C44*/  ld      $s3, 0x0038($sp)\n"
"/*0x80005C48*/  ld      $s2, 0x0030($sp)\n"
"/*0x80005C4C*/  ld      $s1, 0x0028($sp)\n"
"/*0x80005C50*/  ld      $s0, 0x0020($sp)\n"
"/*0x80005C54*/  addiu   $a0, $a0, 0x07BC\n"
"/*0x80005C58*/  j       sys_80005688\n"
"/*0x80005C5C*/  addiu   $sp, $sp, 0x0048\n"
".L80005C60:\n"
"/*0x80005C60*/  lui     $s1, %hi(w_80029338)\n"
"/*0x80005C64*/  j       .L80005BF8\n"
"/*0x80005C68*/  lw      $v0, %lo(w_80029338)($s1)\n"
"\n"
".set reorder\n"
".set at\n"
);

void gDrawChar8X8(u32 val, u32 x, u32 y) {

    u64 tmp;
    u32 font_val;
    u8 *font_ptr = &font[(val & 0xff) * 8];
    u64 *ptr = (u64 *) & screen.current[ (x * 8 + y * 8 * screen.pixel_w)];
    u64 pal_bgr = pal[(val >> 8) & 0x0f];
    u64 pal_txt = pal[val >> 12];

    for (int i = 0; i < 4; i++) {
        pal_bgr = (pal_bgr << 16) | pal_bgr;
    }

    for (u32 i = 0; i < 8; i++) {

        font_val = *font_ptr++;

        tmp = pal_bgr;
        if ((font_val & 0x80) != 0)tmp = (tmp & 0x0000FFFFFFFFFFFF) | (pal_txt << 48);
        if ((font_val & 0x40) != 0)tmp = (tmp & 0xFFFF0000FFFFFFFF) | (pal_txt << 32);
        if ((font_val & 0x20) != 0)tmp = (tmp & 0xFFFFFFFF0000FFFF) | (pal_txt << 16);
        if ((font_val & 0x10) != 0)tmp = (tmp & 0xFFFFFFFFFFFF0000) | (pal_txt);
        *ptr++ = tmp;

        tmp = pal_bgr;
        if ((font_val & 0x08) != 0)tmp = (tmp & 0x0000FFFFFFFFFFFF) | (pal_txt << 48);
        if ((font_val & 0x04) != 0)tmp = (tmp & 0xFFFF0000FFFFFFFF) | (pal_txt << 32);
        if ((font_val & 0x02) != 0)tmp = (tmp & 0xFFFFFFFF0000FFFF) | (pal_txt << 16);
        if ((font_val & 0x01) != 0)tmp = (tmp & 0xFFFFFFFFFFFF0000) | (pal_txt);
        *ptr++ = tmp;

        ptr += (screen.pixel_w - 8) / 4;
    }
}

void gVblank() {

    while (vregs[4] != 0x200);
}

void gVsync() {

    while (vregs[4] == 0x200);
    while (vregs[4] != 0x200);
}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005E60*/  lui     $a0, 0xA4400010 >> 16\n"
"/*0x80005E64*/  lui     $v0, %hi(vregs)\n"
"/*0x80005E68*/  sw      $a0, %lo(vregs)($v0)\n"
"/*0x80005E6C*/  ori     $a0, 0xA4400010 & 0xFFFF\n"
".L80005E70:\n"
"/*0x80005E70*/  lw      $v0, 0x0000($a0)\n"
"/*0x80005E74*/  sltiu   $v0, $v0, 0x000B\n"
"/*0x80005E78*/  beqz    $v0, .L80005E70\n"
"/*0x80005E7C*/  lui     $v1, %hi(0xA4400000)\n"
"/*0x80005E80*/  ori     $v0, $v1, 0x0024\n"
"/*0x80005E84*/  sw      $zero, %lo(0xA4400000)($v1)\n"
"/*0x80005E88*/  sw      $zero, 0x0000($v0)\n"
"/*0x80005E8C*/  jr      $ra\n"
"/*0x80005E90*/  nop\n"
"/*0x80005E94*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

u8 sys_80005E98(void)
{
    return sys_boot_strap->region;
}

#if 0
extern u16 hu_8002933E;
extern u16 hu_8002A020;
void sys_80005EB0(void)
{
    u16 v0 = hu_8002933E + 1;
    u16 t0 = v0 + ((u32 *)0x80000100)[v0 & 0x1065]/8;
    u16 a3 = t0 + ((u32 *)0x80000400)[t0 & 0x1FFF]/2;
    u16 v1 = ((u32 *)0x80000100)[a3 & 0x1065];
    hu_8002933E = a3 ^ hu_8002A020++ ^ v1;
}
#else
asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80005EB0*/  lui     $a0, %hi(hu_8002933E)\n"
"/*0x80005EB4*/  lhu     $v0, %lo(hu_8002933E)($a0)\n"
"/*0x80005EB8*/  lui     $v1, %hi(0x80000102)\n"
"/*0x80005EBC*/  addiu   $v0, $v0, 0x0001\n"
"/*0x80005EC0*/  andi    $v0, $v0, 0xFFFF\n"
"/*0x80005EC4*/  andi    $a1, $v0, 0x1065\n"
"/*0x80005EC8*/  sll     $a1, $a1, 2\n"
"/*0x80005ECC*/  addu    $a1, $v1, $a1\n"
"/*0x80005ED0*/  lw      $t0, 0x0100($a1)\n"
"/*0x80005ED4*/  lui     $a1, %hi(hu_8002A020)\n"
"/*0x80005ED8*/  srl     $t0, $t0, 3\n"
"/*0x80005EDC*/  addu    $t0, $v0, $t0\n"
"/*0x80005EE0*/  andi    $t0, $t0, 0xFFFF\n"
"/*0x80005EE4*/  andi    $v0, $t0, 0x1FFF\n"
"/*0x80005EE8*/  sll     $v0, $v0, 2\n"
"/*0x80005EEC*/  addu    $v0, $v1, $v0\n"
"/*0x80005EF0*/  lw      $a3, 0x0400($v0)\n"
"/*0x80005EF4*/  lhu     $a2, %lo(hu_8002A020)($a1)\n"
"/*0x80005EF8*/  srl     $a3, $a3, 1\n"
"/*0x80005EFC*/  addu    $a3, $t0, $a3\n"
"/*0x80005F00*/  andi    $a3, $a3, 0xFFFF\n"
"/*0x80005F04*/  andi    $v0, $a3, 0x1065\n"
"/*0x80005F08*/  sll     $v0, $v0, 2\n"
"/*0x80005F0C*/  addu    $v1, $v1, $v0\n"
"/*0x80005F10*/  lhu     $v1, %lo(0x80000102)($v1)\n"
"/*0x80005F14*/  xor     $v0, $a3, $a2\n"
"/*0x80005F18*/  xor     $v0, $v0, $v1\n"
"/*0x80005F1C*/  addiu   $a2, $a2, 0x0001\n"
"/*0x80005F20*/  sh      $a2, %lo(hu_8002A020)($a1)\n"
"/*0x80005F24*/  jr      $ra\n"
"/*0x80005F28*/  sh      $v0, %lo(hu_8002933E)($a0)\n"
"\n"
".set reorder\n"
".set at\n"
);
#endif

void SysSiBusy()
{
    volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *) 0xa4800000;
    while (SI_regs->status & (SI_STATUS_DMA_BUSY|SI_STATUS_IO_BUSY));
}

void SysPifmacro(u64 *cmd, u64 *resp)
{
    volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *) 0xa4800000;
    void * const PIF_RAM = (void *) 0x1fc007c0;
    u64 sp20[8];
    u64 sp60[8];
    data_cache_hit_writeback_invalidate(sp20, 64);
    memcopy(cmd, UncachedAddr(sp20), 64);
    disable_interrupts();
    SysSiBusy();
    MEMORY_BARRIER();
    SI_regs->DRAM_addr = sp20;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM;
    MEMORY_BARRIER();
    SysSiBusy();
    data_cache_hit_writeback_invalidate(sp60, 64);
    MEMORY_BARRIER();
    SI_regs->DRAM_addr = sp60;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    MEMORY_BARRIER();
    SysSiBusy();
    enable_interrupts();
    memcopy(UncachedAddr(sp60), resp, 64);
}

void sysDisplayInit() {

    u32 i;
    u32 *v_setup;
    u32 region = *(u32 *) 0x80000300;

    disable_interrupts();


    screen.w = 40;
    screen.h = 30;
    screen.pixel_w = 320;
    screen.pixel_h = 240;
    screen.buff_len = screen.pixel_w * screen.pixel_h;
    screen.char_h = 8;
    v_setup = region == REGION_NTSC ? (u32*) ntsc_320 : region == REGION_PAL ? (u32*) pal_320 : (u32*) mpal_320;

    while (IO_READ(VI_CURRENT_REG) > 10);
    IO_WRITE(VI_CONTROL_REG, 0);

    vregs[1] = (vu32) screen.current;
    vregs[2] = v_setup[2];
    for (i = 5; i < 9; i++)vregs[i] = v_setup[i];
    vregs[9] = v_setup[9]; //bnack
    vregs[10] = v_setup[10];
    vregs[11] = v_setup[11];
    vregs[3] = v_setup[3];
    vregs[12] = v_setup[12];
    vregs[13] = v_setup[13];
    vregs[0] = v_setup[0];


    enable_interrupts();
}

void sleep(u32 ms) {

    u32 current_ms = get_ticks_ms();

    while (get_ticks_ms() - current_ms < ms);

}

void gRepaint() {

    u16 *chr_ptr = g_buff;

    screen.buff_sw = (screen.buff_sw ^ 1) & 1;
    screen.current = screen.buff[screen.buff_sw];


    for (u32 y = 0; y < screen.h; y++) {
        for (u32 x = 0; x < screen.w; x++) {

            gDrawChar8X8(*chr_ptr++, x, y);
        }
    }

    data_cache_hit_writeback(screen.current, screen.buff_len * 2);
    gVblank();
    vregs[1] = (vu32) screen.current;

}

void sysPI_wr(void *ram, unsigned long pi_address, unsigned long len) {

    volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *) 0xa4600000;

    pi_address &= 0x1FFFFFFF;

    data_cache_hit_writeback(ram, len);
    disable_interrupts();

    while (dma_busy());
    IO_WRITE(PI_STATUS_REG, 3);
    PI_regs->ram_address = ram;
    PI_regs->pi_address = pi_address; //(pi_address | 0x10000000) & 0x1FFFFFFF;
    PI_regs->read_length = len - 1;
    while (dma_busy());

    enable_interrupts();

}

void sysPI_rd(void *ram, unsigned long pi_address, unsigned long len) {

    volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *) 0xa4600000;

    pi_address &= 0x1FFFFFFF;

    data_cache_hit_writeback_invalidate(ram, len);
    disable_interrupts();

    while (dma_busy());
    IO_WRITE(PI_STATUS_REG, 3);
    PI_regs->ram_address = ram;
    PI_regs->pi_address = pi_address; //(pi_address | 0x10000000) & 0x1FFFFFFF;
    PI_regs->write_length = len - 1;
    while (dma_busy());

    enable_interrupts();
}

void sysInit() {

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

    rdp_init();

    screen.buff_sw = 0;
    screen.buff[0] = (u16 *) malloc(SYS_MAX_PIXEL_W * SYS_MAX_PIXEL_H * 2);
    screen.buff[1] = (u16 *) malloc(SYS_MAX_PIXEL_W * SYS_MAX_PIXEL_H * 2);
    screen.current = screen.buff[screen.buff_sw];
    screen.bgr_ptr = 0;

    sysDisplayInit();

}

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x800064C8*/  andi    $v0, $a0, 0xFFFF\n"
"/*0x800064CC*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x800064D0*/  divu    $zero, $v0, $a1\n"
"/*0x800064D4*/  teq     $a1, $zero, 7\n"
"/*0x800064D8*/  mfhi    $v1\n"
"/*0x800064DC*/  andi    $v1, $v1, 0xFFFF\n"
"/*0x800064E0*/  beqz    $v1, .L800064F4\n"
"/*0x800064E4*/  nop\n"
"/*0x800064E8*/  addu    $v0, $a1, $v0\n"
"/*0x800064EC*/  subu    $v0, $v0, $v1\n"
"/*0x800064F0*/  andi    $v0, $v0, 0xFFFF\n"
".L800064F4:\n"
"/*0x800064F4*/  jr      $ra\n"
"/*0x800064F8*/  nop\n"
"/*0x800064FC*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80006500*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80006504*/  beqz    $a1, .L8000654C\n"
"/*0x80006508*/  daddu   $v0, $zero, $zero\n"
"/*0x8000650C*/  addiu   $a1, $a1, -0x0001\n"
"/*0x80006510*/  addiu   $v0, $a0, 0x0001\n"
"/*0x80006514*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x80006518*/  addu    $a1, $v0, $a1\n"
"/*0x8000651C*/  daddu   $v1, $zero, $zero\n"
"/*0x80006520*/  daddu   $v0, $zero, $zero\n"
"/*0x80006524*/  nop\n"
".L80006528:\n"
"/*0x80006528*/  lbu     $a2, 0x0000($a0)\n"
"/*0x8000652C*/  addiu   $a0, $a0, 0x0001\n"
"/*0x80006530*/  addu    $v1, $v1, $a2\n"
"/*0x80006534*/  andi    $v1, $v1, 0x00FF\n"
"/*0x80006538*/  addu    $v0, $v1, $v0\n"
"/*0x8000653C*/  bne     $a0, $a1, .L80006528\n"
"/*0x80006540*/  andi    $v0, $v0, 0x00FF\n"
"/*0x80006544*/  sll     $v1, $v1, 8\n"
"/*0x80006548*/  or      $v0, $v1, $v0\n"
".L8000654C:\n"
"/*0x8000654C*/  jr      $ra\n"
"/*0x80006550*/  nop\n"
"/*0x80006554*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);

const u8 crc_bit_table[256] = {
    0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15, 0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55,
    0x02, 0x03, 0x06, 0x07, 0x12, 0x13, 0x16, 0x17, 0x42, 0x43, 0x46, 0x47, 0x52, 0x53, 0x56, 0x57,
    0x08, 0x09, 0x0C, 0x0D, 0x18, 0x19, 0x1C, 0x1D, 0x48, 0x49, 0x4C, 0x4D, 0x58, 0x59, 0x5C, 0x5D,
    0x0A, 0x0B, 0x0E, 0x0F, 0x1A, 0x1B, 0x1E, 0x1F, 0x4A, 0x4B, 0x4E, 0x4F, 0x5A, 0x5B, 0x5E, 0x5F,
    0x20, 0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35, 0x60, 0x61, 0x64, 0x65, 0x70, 0x71, 0x74, 0x75,
    0x22, 0x23, 0x26, 0x27, 0x32, 0x33, 0x36, 0x37, 0x62, 0x63, 0x66, 0x67, 0x72, 0x73, 0x76, 0x77,
    0x28, 0x29, 0x2C, 0x2D, 0x38, 0x39, 0x3C, 0x3D, 0x68, 0x69, 0x6C, 0x6D, 0x78, 0x79, 0x7C, 0x7D,
    0x2A, 0x2B, 0x2E, 0x2F, 0x3A, 0x3B, 0x3E, 0x3F, 0x6A, 0x6B, 0x6E, 0x6F, 0x7A, 0x7B, 0x7E, 0x7F,
    0x80, 0x81, 0x84, 0x85, 0x90, 0x91, 0x94, 0x95, 0xC0, 0xC1, 0xC4, 0xC5, 0xD0, 0xD1, 0xD4, 0xD5,
    0x82, 0x83, 0x86, 0x87, 0x92, 0x93, 0x96, 0x97, 0xC2, 0xC3, 0xC6, 0xC7, 0xD2, 0xD3, 0xD6, 0xD7,
    0x88, 0x89, 0x8C, 0x8D, 0x98, 0x99, 0x9C, 0x9D, 0xC8, 0xC9, 0xCC, 0xCD, 0xD8, 0xD9, 0xDC, 0xDD,
    0x8A, 0x8B, 0x8E, 0x8F, 0x9A, 0x9B, 0x9E, 0x9F, 0xCA, 0xCB, 0xCE, 0xCF, 0xDA, 0xDB, 0xDE, 0xDF,
    0xA0, 0xA1, 0xA4, 0xA5, 0xB0, 0xB1, 0xB4, 0xB5, 0xE0, 0xE1, 0xE4, 0xE5, 0xF0, 0xF1, 0xF4, 0xF5,
    0xA2, 0xA3, 0xA6, 0xA7, 0xB2, 0xB3, 0xB6, 0xB7, 0xE2, 0xE3, 0xE6, 0xE7, 0xF2, 0xF3, 0xF6, 0xF7,
    0xA8, 0xA9, 0xAC, 0xAD, 0xB8, 0xB9, 0xBC, 0xBD, 0xE8, 0xE9, 0xEC, 0xED, 0xF8, 0xF9, 0xFC, 0xFD,
    0xAA, 0xAB, 0xAE, 0xAF, 0xBA, 0xBB, 0xBE, 0xBF, 0xEA, 0xEB, 0xEE, 0xEF, 0xFA, 0xFB, 0xFE, 0xFF,
};

const u16 crc_16_table[] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

#if 0
u16 sys_80006558(u8 *a0, u16 a1, u16 a2)
{
    while (a1--)
    {
        a2 = crc_16_table[(a2 >> 8) ^ *a0++] ^ (a2 << 8);
    }
    return a2;
}
#else
asm(
".text\n"
".set noreorder\n"
".set noat\n"
"\n"
".balign 8\n"
"/*0x80006558*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x8000655C*/  beqz    $a1, .L800065AC\n"
"/*0x80006560*/  andi    $v0, $a2, 0xFFFF\n"
"/*0x80006564*/  addiu   $a1, $a1, -0x0001\n"
"/*0x80006568*/  andi    $a1, $a1, 0xFFFF\n"
"/*0x8000656C*/  addiu   $v1, $a0, 0x0001\n"
"/*0x80006570*/  lui     $a3, %hi(crc_16_table)\n"
"/*0x80006574*/  addu    $a1, $v1, $a1\n"
"/*0x80006578*/  addiu   $a3, %lo(crc_16_table)\n"
"/*0x8000657C*/  nop\n"
".L80006580:\n"
"/*0x80006580*/  lbu     $v1, 0x0000($a0)\n"
"/*0x80006584*/  srl     $a2, $v0, 8\n"
"/*0x80006588*/  xor     $v1, $a2, $v1\n"
"/*0x8000658C*/  sll     $v1, $v1, 1\n"
"/*0x80006590*/  addu    $v1, $a3, $v1\n"
"/*0x80006594*/  lhu     $v1, 0x0000($v1)\n"
"/*0x80006598*/  sll     $v0, $v0, 8\n"
"/*0x8000659C*/  xor     $v0, $v1, $v0\n"
"/*0x800065A0*/  addiu   $a0, $a0, 0x0001\n"
"/*0x800065A4*/  bne     $a0, $a1, .L80006580\n"
"/*0x800065A8*/  andi    $v0, $v0, 0xFFFF\n"
".L800065AC:\n"
"/*0x800065AC*/  jr      $ra\n"
"/*0x800065B0*/  nop\n"
"/*0x800065B4*/  nop\n"
"\n"
".set reorder\n"
".set at\n"
);
#endif

u8 sys_800065B8(u8 a0)
{
    if (a0 > 99) a0 = 99;
    return (a0/10) << 4 | (a0%10);
}

u8 sys_80006620(u8 a0)
{
    return 10*(a0 >> 4) + (a0 & 0xF);
}

void sdCrc16(void *src, u16 *crc_out) {

    u16 i;
    u16 u;
    u8 *src8;
    u8 val[4];
    u16 crc_table[4];
    u16 tmp1;
    u8 dat;

    for (i = 0; i < 4; i++)crc_table[i] = 0;
    src8 = (u8 *) src;

    for (i = 0; i < 128; i++) {


        dat = *src8++;
        val[3] = (dat & 0x88);
        val[2] = (dat & 0x44) << 1;
        val[1] = (dat & 0x22) << 2;
        val[0] = (dat & 0x11) << 3;

        dat = *src8++;
        val[3] |= (dat & 0x88) >> 1;
        val[2] |= (dat & 0x44);
        val[1] |= (dat & 0x22) << 1;
        val[0] |= (dat & 0x11) << 2;

        dat = *src8++;
        val[3] |= (dat & 0x88) >> 2;
        val[2] |= (dat & 0x44) >> 1;
        val[1] |= (dat & 0x22);
        val[0] |= (dat & 0x11) << 1;

        dat = *src8++;
        val[3] |= (dat & 0x88) >> 3;
        val[2] |= (dat & 0x44) >> 2;
        val[1] |= (dat & 0x22) >> 1;
        val[0] |= (dat & 0x11);

        val[0] = crc_bit_table[val[0]];
        val[1] = crc_bit_table[val[1]];
        val[2] = crc_bit_table[val[2]];
        val[3] = crc_bit_table[val[3]];

        tmp1 = crc_table[0];
        crc_table[0] = crc_16_table[(tmp1 >> 8) ^ val[0]];
        crc_table[0] = crc_table[0] ^ (tmp1 << 8);

        tmp1 = crc_table[1];
        crc_table[1] = crc_16_table[(tmp1 >> 8) ^ val[1]];
        crc_table[1] = crc_table[1] ^ (tmp1 << 8);

        tmp1 = crc_table[2];
        crc_table[2] = crc_16_table[(tmp1 >> 8) ^ val[2]];
        crc_table[2] = crc_table[2] ^ (tmp1 << 8);

        tmp1 = crc_table[3];
        crc_table[3] = crc_16_table[(tmp1 >> 8) ^ val[3]];
        crc_table[3] = crc_table[3] ^ (tmp1 << 8);

    }

    for (i = 0; i < 4; i++) {
        for (u = 0; u < 16; u++) {
            crc_out[3 - i] >>= 1;
            crc_out[3 - i] |= (crc_table[u % 4] & 1) << 15;
            crc_table[u % 4] >>= 1;
        }
    }

}

u16 sys_800068B0(u16 a0, s16 a1, u16 a2, u16 a3)
{
    a1 += a0/a2;
    if (a1 < 0) a1 += a3;
    return a2*(a1%a3) + a0%a2;
}

u16 sys_80006928(u16 a0, s16 a1, u16 a2)
{
    a1 += a0%a2;
    if (a1 < 0) a1 += a2;
    return a1%a2 + a2*(a0/a2);
}
