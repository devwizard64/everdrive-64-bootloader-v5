#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

/******************************************************************************/
/* sys.h                                                                      */
/******************************************************************************/

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned long
#define u64 unsigned long long

#define vu8 volatile unsigned char
#define vu16 volatile unsigned short
#define vu32 volatile unsigned long
#define vu64 volatile unsigned long long

#define s8 signed char
#define s16 short
#define s32 long
#define s64 long long

#define CIC_6101 1
#define CIC_6102 2
#define CIC_6103 3
#define CIC_5101 4
#define CIC_6105 5
#define CIC_6106 6
#define CIC_5167 7

#define REGION_PAL      0
#define REGION_NTSC     1
#define REGION_MPAL     2

#define KSEG0           0x80000000
#define KSEG1           0xA0000000

#define VI_BASE_REG     0x04400000
#define	VI_CONTROL	(VI_BASE_REG + 0x00)
#define	VI_FRAMEBUFFER	(VI_BASE_REG + 0x04)
#define	VI_WIDTH	(VI_BASE_REG + 0x08)
#define	VI_V_INT	(VI_BASE_REG + 0x0C)
#define	VI_CUR_LINE	(VI_BASE_REG + 0x10)
#define	VI_TIMING	(VI_BASE_REG + 0x14)
#define	VI_V_SYNC	(VI_BASE_REG + 0x18)
#define	VI_H_SYNC	(VI_BASE_REG + 0x1C)
#define	VI_H_SYNC2	(VI_BASE_REG + 0x20)
#define	VI_H_LIMITS	(VI_BASE_REG + 0x24)
#define	VI_COLOR_BURST	(VI_BASE_REG + 0x28)
#define	VI_H_SCALE	(VI_BASE_REG + 0x2C)
#define	VI_VSCALE	(VI_BASE_REG + 0x30)

#define PI_BSD_DOM1_LAT_REG	(PI_BASE_REG+0x14)
#define PI_BSD_DOM1_PWD_REG	(PI_BASE_REG+0x18)
#define PI_BSD_DOM1_PGS_REG	(PI_BASE_REG+0x1C)    /*   page size */
#define PI_BSD_DOM1_RLS_REG	(PI_BASE_REG+0x20)

#define PI_BSD_DOM2_LAT_REG	(PI_BASE_REG+0x24)    /* Domain 2 latency */
#define PI_BSD_DOM2_PWD_REG	(PI_BASE_REG+0x28)    /*   pulse width */
#define PI_BSD_DOM2_PGS_REG	(PI_BASE_REG+0x2C)    /*   page size */
#define PI_BSD_DOM2_RLS_REG	(PI_BASE_REG+0x30)    /*   release duration */

#define	PHYS_TO_K1(x)           ((u32)(x)|KSEG1)        /* physical to kseg1 */
#define	IO_WRITE(addr,data)	(*(vu32 *)PHYS_TO_K1(addr)=(u32)(data))
#define	IO_READ(addr)		(*(vu32 *)PHYS_TO_K1(addr))

#define PI_STATUS_REG   (PI_BASE_REG+0x10)
#define PI_BASE_REG     0x04600000

#define VI_BASE_REG     0x04400000
#define VI_STATUS_REG   (VI_BASE_REG+0x00)
#define VI_CONTROL_REG  VI_STATUS_REG
#define VI_CURRENT_REG  (VI_BASE_REG+0x10)

#define RGB(r, g, b) ((r << 11) | (g << 6) | (b << 1))

typedef struct {
    vu32 region;
    vu32 rom_type;
    vu32 base_addr;
    vu32 rst_type;
    vu32 cic_type;
    vu32 os_ver;
    vu32 ram_size;
    vu32 app_buff;
} BootStrap;

typedef struct {
    u32 w;
    u32 h;
    u32 pixel_w;
    u32 pixel_h;
    u32 char_h;
    u32 buff_len;
    u32 buff_sw;
    u16 * buff[2];
    u16 *current;
    u16 *bgr_ptr;
} Screen;

#define G_SCREEN_W      40 //screen.w
#define G_SCREEN_H      30 //screen.h


void sysInit();
void sysRepaint();
void sysVsync();
u8 sysGetTvType();
void sysPI_rd(void *ram, unsigned long pi_address, unsigned long len);
void sysPI_wr(void *ram, unsigned long pi_address, unsigned long len);

/******************************************************************************/
/* gfx.h                                                                      */
/******************************************************************************/

#define G_SCREEN_W      40 //screen.w
#define G_SCREEN_H      30 //screen.h
#define G_BORDER_X      2
#define G_BORDER_Y      2
#define G_MAX_STR_LEN   (G_SCREEN_W - G_BORDER_X*2)

#define PAL_B1          0x1000
#define PAL_B2          0x2000
#define PAL_B3          0x3000
#define PAL_G1          0x1400
#define PAL_G2          0x2400
#define PAL_G3          0x3400

#define PAL_BR          0x5000
#define PAL_BG          0x6000
#define PAL_WG          0x1700
#define PAL_RB          0x0500

void gInit(u16 *gfx_mem);
void gCleanScreen();
void gConsPrint(u8 *str);
void gSetXY(u8 x, u8 y);
void gSetPal(u16 pal);
void gAppendString(u8 *str);
void gAppendChar(u8 chr);
void gAppendHex8(u8 val);
void gAppendHex16(u16 val);
void gAppendHex32(u32 val);
void gAppendHex32(u32 val);

/******************************************************************************/
/* bios.h                                                                     */
/******************************************************************************/

#define BI_ROM_ADDR    (KSEG1 | 0x10000000)
#define BI_RAM_ADDR    (KSEG1 | 0x08000000)

#define BI_ERR_I2C_CMD          0xB0
#define BI_ERR_I2C_TOUT         0xB1
#define BI_ERR_USB_TOUT         0xB2
#define BI_ERR_FPG_CFG          0xB3
#define BI_ERR_MCN_CFG          0xB3
#define BI_ERR_IOM_CFG          0xB4

#define BI_DISK_SPD_LO  0
#define BI_DISK_SPD_HI  1

#define BI_BCFG_BOOTMOD 1
#define BI_BCFG_SD_INIT 2
#define BI_BCFG_SD_TYPE 4
#define BI_BCFG_GAMEMOD 8
#define BI_BCFG_CICLOCK 0x8000

#define BI_DD_TBL_SIZE  2048
#define BI_DD_PGE_SIZE  0x8000


#define BI_MIN_USB_SIZE 16

#define SAVE_OFF        0
#define SAVE_EEP4K      1
#define SAVE_EEP16K     2
#define SAVE_SRM32K     3
#define SAVE_SRM96K     4
#define SAVE_FLASH      5
#define SAVE_SRM128K    6 
#define SAVE_MPAK       8
#define SAVE_DD64       16

void bi_init();

u8 bi_usb_can_rd();
u8 bi_usb_can_wr();
u8 bi_usb_rd(void *dst, u32 len);
u8 bi_usb_wr(void *src, u32 len);
void bi_usb_rd_start();
u8 bi_usb_rd_end(void *dst);


void bi_sd_speed(u8 speed);
void bi_sd_bitlen(u8 val);
u8 bi_sd_cmd_rd();
void bi_sd_cmd_wr(u8 val);
u8 bi_sd_dat_rd();
void bi_sd_dat_wr(u8 val);
u8 bi_sd_to_ram(void *dst, u16 slen);
u8 bi_sd_to_rom(u32 dst, u16 slen);
u8 bi_ram_to_sd(void *src, u16 slen);

void bi_set_save_type(u8 type);
void bi_wr_swap(u8 swap_on);

/******************************************************************************/
/* disk.h                                                                     */
/******************************************************************************/

#define DISK_ERR_INIT   0xD0
#define DISK_ERR_CTO    0xD1
#define DISK_ERR_RD1    0xD2//cmd tout
#define DISK_ERR_RD2    0xD2//io error
#define DISK_ERR_WR1    0xD3//cmd tout
#define DISK_ERR_WR2    0xD3//io error

u8 diskInit();
u8 diskReadToRam(u32 sd_addr, void *dst, u16 slen);
u8 diskReadToRom(u32 sd_addr, u32 dst, u16 slen);
u8 diskRead(void *dst, u32 saddr, u32 slen);
u8 diskWrite(u32 saddr, void *src, u16 slen);
u8 diskCloseRW();
u8 diskStop();

/******************************************************************************/
/* everdrive.h                                                                */
/******************************************************************************/

void boot_simulator(u8 cic);

/******************************************************************************/
/* usb.h                                                                      */
/******************************************************************************/

u8 usbListener();

/******************************************************************************/

#define MEMORY_BARRIER() asm volatile ("" : : : "memory")

/* bios.c */
void bios_80000568();
void BiCartRomWr(void *ram, unsigned long rom_address, unsigned long len);
void BiBootCfgClr(u16 cfg);
void BiBootCfgSet(u16 cfg);
void BiLockRegs();
void BiCartRomRd(void *ram, unsigned long rom_address, unsigned long len);
int BiBootRomRd(void *ram, unsigned long rom_address, unsigned long len);
u16 BiBootCfgGet(u16 cfg);
void BiCartRomFill(u8 c, unsigned long rom_address, unsigned long len);
u8 BiMCNWr(void *src, u32 len);

/* main.c */
void simulate_pif_boot();

/* fat.c */
u32 fat_80003730();
u8 fat_80003C00(int, u32);
u8 fatOpenFileByeName(u8 *name, u32 wr_sectors);
u8 fatInit(void *);

/* gfx.c */
void gSetY(u8 y);
void GfxResetXY();
void GfxFillRect(u16 chr, u8 x, u8 y, u8 width, u8 height);
void GfxPrintCenter(u8 *str);
void GfxAppendDec(u32 val);

/* fmt.c */
void GfxFill(u16 *dst, u16 val, u32 len);
void memcopy(const void *src, void *dst, u32 len);
void memfill(void *dst, u8 val, u32 len);
u8 streql(u8 *str1, u8 *str2, u8 len);
u8 slen(u8 *str);
void FmtInit(u8 *str);
void FmtDecBuff(u8 *buff, u32 val);

/* sys.c */
void sysExecPIF(void *inblock, void *outblock);
void sleep(u32 ms);
u8 SysDecToBCD(u8 a0);
void sdCrc16(void *src, u16 *crc_out);
