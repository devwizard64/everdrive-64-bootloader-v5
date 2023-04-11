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
void sysPI_rd(void *ram, unsigned long pi_address, unsigned long len);
void sysPI_wr(void *ram, unsigned long pi_address, unsigned long len);



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
void gRepaint();
void gVsync();

/******************************************************************************/
/* bios.h                                                                     */
/******************************************************************************/

#define BI_SIZE_ROM     0x4000000       //rom size
#define BI_SIZE_BRM     0x20000         //backup ram size

#define BI_ADDR_ROM    (KSEG1 | 0x10000000)
#define BI_ADDR_BRM    (KSEG1 | 0x08000000)

#define BI_ERR_I2C_CMD          0xB0
#define BI_ERR_I2C_TOUT         0xB1
#define BI_ERR_USB_TOUT         0xB2
#define BI_ERR_FPG_CFG          0xB3

//sd controller speed select. LO speed only for init procedure
#define BI_DISK_SPD_LO  0x00
#define BI_DISK_SPD_HI  0x01

//bootloader flags
#define BI_BCFG_BOOTMOD 0x01   
#define BI_BCFG_SD_INIT 0x02
#define BI_BCFG_SD_TYPE 0x04
#define BI_BCFG_GAMEMOD 0x08
#define BI_BCFG_CICLOCK 0x8000

//dd table to know data areas which should be saved
#define BI_DD_TBL_SIZE  2048
#define BI_DD_PGE_SIZE  0x8000

#define CART_ID_V2      0xED640007
#define CART_ID_V3      0xED640008
#define CART_ID_X7      0xED640013
#define CART_ID_X5      0xED640014

//game cfg register flags
#define SAVE_OFF        0x0000
#define SAVE_EEP4K      0x0001
#define SAVE_EEP16K     0x0002
#define SAVE_SRM32K     0x0003
#define SAVE_SRM96K     0x0004
#define SAVE_FLASH      0x0005
#define SAVE_SRM128K    0x0006
#define SAVE_DD64       0x0010



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

void bi_game_cfg_set(u8 type); //set save type
void bi_wr_swap(u8 swap_on);
u8 bi_get_cart_id();

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
void BiCartRomWr(void *ram, unsigned long cart_address, unsigned long len);
void BiBootCfgClr(u16 cfg);
void BiBootCfgSet(u16 cfg);
void BiLockRegs();
void BiCartRomRd(void *ram, unsigned long cart_address, unsigned long len);
int BiBootRomRd(void *ram, unsigned long cart_address, unsigned long len);
u16 BiBootCfgGet(u16 cfg);
void BiCartRomFill(u8 c, unsigned long cart_address, unsigned long len);
u8 BiFPGAWr(void *src, u32 len);

/* main.c */
void MainBootOS();

/* fat.c */
int fat_80003730();
int fat_80003C00(int, int);
u8 fat_80004820(u8 *, int);
int fat_80004870(void *);

/* gfx.c */
void gSetY(u8 y);
void GfxResetXY();
void GfxFillRect(u16 chr, u8 x, u8 y, u8 width, u8 height);
void GfxPrintCenter(u8 *str);
void GfxAppendDec(u32 val);
void GfxInit(u16 *buff);
extern u16 *g_buff;

/* fmt.c */
void GfxFill(u16 *dst, u16 val, u32 len);
void memcopy(const void *src, void *dst, u32 len);
void memfill(void *dst, u8 val, u32 len);
u8 streql(u8 *str1, u8 *str2, u8 len);
u8 slen(u8 *str);
void FmtInit(u8 *str);
void FmtDecBuff(u8 *buff, u32 val);

/* sys.c */
void SysPifmacro(u64 *cmd, u64 *resp);
void sleep(u32 ms);
u8 SysDecToBCD(u8 a0);
void sdCrc16(void *src, u16 *crc_out);
extern u64 pifCmdRTCInfo[];
extern u64 pifCmdEepRead[];

/* fpga_data.s */
extern u8 fpga_data[];
extern u32 fpga_data_len;
