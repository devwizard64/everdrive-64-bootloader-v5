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
void simulate_pif_boot();
void sysSI_dmaBusy(void);
void sysExecPIF(void *inblock, void *outblock);
void sysBlank();

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

BootStrap *sys_boot_strap = (BootStrap *) 0x80000300;

extern u16 *gfx_buff;
extern u8 font[];

Screen screen;


u16 pal[16] = {
    RGB(0, 0, 0), RGB(31, 31, 31), RGB(16, 16, 16), RGB(28, 28, 2),
    RGB(8, 8, 8), RGB(31, 0, 0), RGB(0, 31, 0), RGB(12, 12, 12),
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0aa0
};

vu32 *vregs = (vu32 *) 0xa4400000;

u16 hu_8002933C;
u16 hu_8002933E;
u16 hu_80029340;
u16 hu_8002A020 = 0;

void sysDrawChar8X8(u32 val, u32 x, u32 y) {

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

void sysBlank() {
    while (vregs[4] != 0x200);
}

void sysVsync() {

    while (vregs[4] == 0x200);
    while (vregs[4] != 0x200);
}


void sysDisplayOFF() {

    vregs = (vu32 *) 0xa4400000;

    while (IO_READ(VI_CURRENT_REG) > 10);
    IO_WRITE(VI_CONTROL_REG, 0);
    vregs[9] = 0;

}

u8 sysGetTvType() {
    return sys_boot_strap->region;
}

u16 SysRandom()
{
    u16 a = hu_8002933E + 1;
    u16 b = ((u32 *)0x80000100)[a & 0x1065]/8 + a;
    u16 c = ((u32 *)0x80000400)[b & 0x1FFF]/2 + b;
    u16 d = ((u32 *)0x80000100)[c & 0x1065];
    hu_8002933E = hu_8002A020++ ^ c ^ d;
    return hu_8002933E;
}

void sysSI_dmaBusy(void) {
    volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *) 0xa4800000;
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY));
}

void sysExecPIF(void *inblock, void *outblock)
{
    volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *) 0xa4800000;
    void * const PIF_RAM = (void *) 0x1fc007c0;
    u64 sp20[8];
    u64 sp60[8];
    data_cache_hit_writeback_invalidate(sp20, 64);
    memcopy(inblock, UncachedAddr(sp20), 64);
    disable_interrupts();
    sysSI_dmaBusy();
    MEMORY_BARRIER();
    SI_regs->DRAM_addr = sp20;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM;
    MEMORY_BARRIER();
    sysSI_dmaBusy();
    data_cache_hit_writeback_invalidate(sp60, 64);
    MEMORY_BARRIER();
    SI_regs->DRAM_addr = sp60;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    MEMORY_BARRIER();
    sysSI_dmaBusy();
    enable_interrupts();
    memcopy(UncachedAddr(sp60), outblock, 64);
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

void sysRepaint() {

    u16 *chr_ptr = gfx_buff;

    screen.buff_sw = (screen.buff_sw ^ 1) & 1;
    screen.current = screen.buff[screen.buff_sw];


    for (u32 y = 0; y < screen.h; y++) {
        for (u32 x = 0; x < screen.w; x++) {

            sysDrawChar8X8(*chr_ptr++, x, y);
        }
    }

    data_cache_hit_writeback(screen.current, screen.buff_len * 2);
    sysBlank();
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

u16 sys_800064C8(u16 a0, u16 a1)
{
    return a0%a1 ? a0 - a0%a1 + a1 : a0;
}

u16 sys_80006500(void *a0, u16 len)
{
    u8 *ptr = a0;
    u8 a = 0;
    u8 b = 0;
    while (len--)
    {
        a += *ptr++;
        b += a;
    }
    return a << 8 | b;
}

u16 sys_80006558(void *a0, u16 a1, u16 a2)
{
    u8 *ptr = a0;
    while (a1--)
    {
        a2 = crc_16_table[(a2 >> 8) ^ *ptr++] ^ (a2 << 8);
    }
    return a2;
}

u8 SysDecToBCD(u8 a0)
{
    if (a0 > 99) a0 = 99;
    return (a0/10) << 4 | (a0%10);
}

u8 SysBCDToDec(u8 a0)
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
