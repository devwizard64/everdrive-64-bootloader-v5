#include "everdrive.h"

#define REG_BASE        0x1F800000
#define REG_FPG_CFG     0x0000
#define REG_USB_CFG     0x0004
#define REG_TIMER       0x000C
#define REG_BOOT_CFG    0x0010
#define REG_IOM_VER     0x0014
#define REG_I2C_CMD     0x0018
#define REG_I2C_DAT     0x001C

#define REG_FPG_DAT     0x0200
#define REG_USB_DAT     0x0400

#define REG_SYS_CFG     0x8000
#define REG_KEY         0x8004
#define REG_DMA_STA     0x8008
#define REG_DMA_ADDR    0x8008
#define REG_DMA_LEN     0x800C
#define REG_RTC_SET     0x8010
#define REG_RAM_CFG     0x8018
#define REG_IOM_CFG     0x801C
#define REG_SDIO        0x8020
#define REG_MCN_VER     0x8040
#define REG_SDIO_ARD    0x8200
#define REG_IOM_DAT     0x8400
#define REG_DD_TBL      0x8800
#define REG_SD_CMD_RD   (REG_SDIO + 0x00*4)
#define REG_SD_CMD_WR   (REG_SDIO + 0x01*4)
#define REG_SD_DAT_RD   (REG_SDIO + 0x02*4)
#define REG_SD_DAT_WR   (REG_SDIO + 0x03*4)
#define REG_SD_STATUS   (REG_SDIO + 0x04*4)

#define DMA_STA_BUSY    0x0001
#define DMA_STA_ERROR   0x0002
#define DMA_STA_LOCK    0x0080

#define SD_CFG_BITLEN   0x000F
#define SD_CFG_SPD      0x0010
#define SD_STA_BUSY     0x0080

#define CFG_BROM_ON     0x0001
#define CFG_REGS_OFF    0x0002
#define CFG_SWAP_ON     0x0004

#define FPG_CFG_NCFG    0x0001
#define FPG_STA_CDON    0x0001
#define FPG_STA_NSTAT   0x0002

#define I2C_CMD_DAT     0x10
#define I2C_CMD_STA     0x20
#define I2C_CMD_END     0x30

#define IOM_CFG_SS      0x0001
#define IOM_CFG_RST     0x0002
#define IOM_CFG_ACT     0x0080
#define IOM_STA_CDN     0x0001

#define USB_LE_CFG      0x8000
#define USB_LE_CTR      0x4000

#define USB_CFG_ACT     0x0200
#define USB_CFG_RD      0x0400
#define USB_CFG_WR      0x0000

#define USB_STA_ACT     0x0200
#define USB_STA_RXF     0x0400
#define USB_STA_TXE     0x0800
#define USB_STA_PWR     0x1000
#define USB_STA_BSY     0x2000

#define USB_CMD_RD_NOP  (USB_LE_CFG | USB_LE_CTR | USB_CFG_RD)
#define USB_CMD_RD      (USB_LE_CFG | USB_LE_CTR | USB_CFG_RD | USB_CFG_ACT)
#define USB_CMD_WR_NOP  (USB_LE_CFG | USB_LE_CTR | USB_CFG_WR)
#define USB_CMD_WR      (USB_LE_CFG | USB_LE_CTR | USB_CFG_WR | USB_CFG_ACT)

#define REG_LAT 0x04
#define REG_PWD 0x04

#define ROM_LAT 0x40
#define ROM_PWD 0x12

#define REG_ADDR(reg)   (KSEG1 | REG_BASE | (reg))

u32 bi_reg_rd(u16 reg);
void bi_reg_wr(u16 reg, u32 val);
void bi_usb_init();
u8 bi_usb_busy();

u16 biBootCfg;
u16 biSysCfg;
u16 bi_sd_cfg;
u16 bi_old_sd_mode;

int bios_80000560(void)
{
    return 0;
}

void bios_80000568(void)
{
}

int bios_80000570(void)
{
    return 0;
}

int bios_80000578(void)
{
    return 0;
}

int bios_80000580(void)
{
    return 0;
}

void bios_80000588(void)
{
}

void BiDDTblWr()
{
    u8 buff[BI_DD_TBL_SIZE];
    sysPI_wr(buff, REG_ADDR(REG_DD_TBL), BI_DD_TBL_SIZE);
}

void BiCartRomWr(void *ram, unsigned long rom_address, unsigned long len)
{
    sysPI_wr(ram, BI_ROM_ADDR|rom_address, len);
}

void bi_reg_wr(u16 reg, u32 val) {

    sysPI_wr(&val, REG_ADDR(reg), 4);
}

void BiBootCfgClr(u16 cfg)
{
    biBootCfg &= ~cfg;
    bi_reg_wr(REG_BOOT_CFG, biBootCfg);
}

void BiBootCfgSet(u16 cfg)
{
    biBootCfg |= cfg;
    bi_reg_wr(REG_BOOT_CFG, biBootCfg);
}

void bi_set_save_type(u8 type) {

    bi_reg_wr(REG_RAM_CFG, type);
}

void bi_sd_bitlen(u8 val) {

    bi_sd_cfg &= ~SD_CFG_BITLEN;
    bi_sd_cfg |= (val & SD_CFG_BITLEN);
    bi_reg_wr(REG_SD_STATUS, bi_sd_cfg);
}

//this function gives time for setting stable values on open bus
void bi_sd_switch_mode(u16 mode) {

    if (bi_old_sd_mode == mode)return;
    bi_old_sd_mode = mode;

    u16 old_sd_cfg = bi_sd_cfg;
    bi_sd_bitlen(0);
    bi_reg_wr(mode, 0xffff);
    bi_sd_cfg = old_sd_cfg;
    bi_reg_wr(REG_SD_STATUS, old_sd_cfg);
}

void bi_sd_dat_wr(u8 val) {
    bi_sd_switch_mode(REG_SD_DAT_WR);
    bi_reg_wr(REG_SD_DAT_WR, 0x00ff | (val << 8));
    //bi_sd_busy();
}

void bi_sd_speed(u8 speed) {

    if (speed == BI_DISK_SPD_LO) {
        bi_sd_cfg &= ~SD_CFG_SPD;
    } else {
        bi_sd_cfg |= SD_CFG_SPD;
    }

    bi_reg_wr(REG_SD_STATUS, bi_sd_cfg);
}

void BiBootRomOff()
{
    biSysCfg &= ~CFG_BROM_ON;
    bi_reg_wr(REG_SYS_CFG, biSysCfg);
}

void BiBootRomOn()
{
    biSysCfg |= CFG_BROM_ON;
    bi_reg_wr(REG_SYS_CFG, biSysCfg);
}

void BiLockRegs()
{
    bi_reg_wr(REG_KEY, 0);
}

//swaps bytes copied from SD card. only affects reads to ROM area
void bi_wr_swap(u8 swap_on) {

    if (swap_on) {
        biSysCfg |= CFG_SWAP_ON;
    } else {
        biSysCfg &= ~CFG_SWAP_ON;
    }
    bi_reg_wr(REG_SYS_CFG, biSysCfg);
}

void BiTimerSet(u16 timer)
{
    bi_reg_wr(REG_TIMER, timer);
}

u32 BiUsbWrNop()
{
    bi_reg_wr(REG_USB_CFG, USB_CMD_WR_NOP);
    return REG_ADDR(REG_USB_DAT);
}

u32 BiUsbRdNop()
{
    bi_reg_wr(REG_USB_CFG, USB_CMD_RD_NOP);
    return REG_ADDR(REG_USB_DAT);
}

void bi_usb_rd_start() {

    bi_reg_wr(REG_USB_CFG, USB_CMD_RD | 512);
}

void BiCartRamWr(void *ram, unsigned long ram_address, unsigned long len)
{
    bi_set_save_type(SAVE_SRM128K);
    sysPI_wr(ram, BI_RAM_ADDR|ram_address, len);
    bi_set_save_type(SAVE_OFF);
}

void BiDDTblRd(void *ram)
{
    sysPI_rd(ram, REG_ADDR(REG_DD_TBL), BI_DD_TBL_SIZE);
}

void BiCartRomRd(void *ram, unsigned long rom_address, unsigned long len)
{
    sysPI_rd(ram, BI_ROM_ADDR|rom_address, len);
}

int BiBootRomRd(void *ram, unsigned long rom_address, unsigned long len)
{
    u32 lat = IO_READ(PI_BSD_DOM1_LAT_REG);
    u32 pwd = IO_READ(PI_BSD_DOM1_PWD_REG);
    IO_WRITE(PI_BSD_DOM1_LAT_REG, 0x42);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x16);
    BiBootRomOn();
    BiCartRomRd(ram, rom_address, len);
    BiBootRomOff();
    IO_WRITE(PI_BSD_DOM1_LAT_REG, lat);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, pwd);
    return 0;
}

void BiCartRamRd(void *ram, unsigned long ram_address, unsigned long len)
{
    bi_set_save_type(SAVE_SRM128K);
    sysPI_rd(ram, BI_RAM_ADDR|ram_address, len);
    bi_set_save_type(SAVE_OFF);
}

u32 bi_reg_rd(u16 reg) {

    u32 val;
    sysPI_rd(&val, REG_ADDR(reg), 4);
    return val;
}

u16 BiBootCfgGet(u16 cfg)
{
    biBootCfg = bi_reg_rd(REG_BOOT_CFG);
    return biBootCfg & cfg;
}

void BiI2CDatWr(u8 *src, u8 len)
{
    while (len--)
    {
        bi_reg_wr(REG_I2C_DAT, *src++);
        while (bi_reg_rd(REG_I2C_CMD) & 0x80);
    }
}

void BiI2CDatRd(void *dst, u16 len)
{
    u8 *ptr = dst;
    while (len--)
    {
        bi_reg_wr(REG_I2C_DAT, 0xFF);
        while (bi_reg_rd(REG_I2C_CMD) & 0x80);
        *ptr++ = bi_reg_rd(REG_I2C_DAT);
    }
}

u8 BiI2CStatus()
{
    return bi_reg_rd(REG_I2C_CMD) & 1;
}

u8 BiI2CIo(u8 dat)
{
    bi_reg_wr(REG_I2C_DAT, dat);
    while (bi_reg_rd(REG_I2C_CMD) & 0x80);
    return bi_reg_rd(REG_I2C_DAT);
}

u8 BiI2CDat(u8 dat)
{
    bi_reg_wr(REG_I2C_DAT, dat);
    while (bi_reg_rd(REG_I2C_CMD) & 0x80);
    return bi_reg_rd(REG_I2C_CMD) & 1;
}

void BiI2CSetWr()
{
    u32 cmd = bi_reg_rd(REG_I2C_CMD) & 0xFF;
    bi_reg_wr(REG_I2C_CMD, cmd | I2C_CMD_DAT | 1);
}

void BiI2CSetRd()
{
    u32 cmd = bi_reg_rd(REG_I2C_CMD) & 0xFF;
    bi_reg_wr(REG_I2C_CMD, cmd | I2C_CMD_DAT | 0);
}

void BiI2CEnd()
{
    bi_reg_wr(REG_I2C_CMD, I2C_CMD_END);
    bi_reg_wr(REG_I2C_DAT, 0xFF);
    while (bi_reg_rd(REG_I2C_CMD) & 0x80);
}

void BiI2CStart()
{
    u32 cmd = bi_reg_rd(REG_I2C_CMD) & 0xFF;
    bi_reg_wr(REG_I2C_CMD, I2C_CMD_STA);
    bi_reg_wr(REG_I2C_DAT, 0xFF);
    while (bi_reg_rd(REG_I2C_CMD) & 0x80);
    bi_reg_wr(REG_I2C_CMD, cmd | I2C_CMD_DAT | 1);
}

u8 __BiI2CWr(u16 addr, void *src, u16 len)
{
    u8 dev = addr < 0x100 ? 0xA2 : 0xD0;
    u8 *ptr = src;
    for (;;)
    {
        u8 tout = 0xFF;
        for (tout = 0xFF; tout > 0; tout--)
        {
            BiI2CStart();
            if (!BiI2CDat(dev)) break;
            BiI2CEnd();
        }
        if (tout == 0) return BI_ERR_I2C_TOUT;
        if (len == 0) break;
        if (BiI2CDat(addr & 0xFF)) return BI_ERR_I2C_CMD;
        u8 n = 8;
        if (n > len) n = len;
        BiI2CDatWr(ptr, n);
        BiI2CEnd();
        ptr += n;
        len -= n;
        addr += n;
    }
    BiI2CEnd();
    return 0;
}

u8 BiI2CWr(void *src, u16 addr, u16 len)
{
    return __BiI2CWr(addr, src, len);
}

u8 __BiI2CRd(u16 addr, void *dst, u16 len)
{
    u8 dev = addr < 0x100 ? 0xA2 : 0xD0;
    BiI2CStart();
    if (BiI2CDat(dev)) return BI_ERR_I2C_CMD;
    if (BiI2CDat(addr & 0xFF)) return BI_ERR_I2C_CMD;
    BiI2CStart();
    if (BiI2CDat(dev | 1)) return BI_ERR_I2C_CMD;
    BiI2CSetRd();
    BiI2CDatRd(dst, len);
    BiI2CSetWr();
    BiI2CDat(0xFF);
    BiI2CEnd();
    return 0;
}

u8 BiRTCRd(u8 *dst)
{
    u8 buff[16];
    u8 resp = BiI2CRd(buff, 0x100, 16);
    if (resp) return resp;
    dst[0] = buff[0];
    dst[1] = buff[1];
    dst[2] = buff[2] & 0x3F;
    dst[4] = buff[3] & 0x07;
    dst[3] = buff[4];
    dst[5] = buff[5] & 0x1F;
    dst[6] = buff[6];
    dst[7] = buff[15] & 0x80;
    return resp;
}

u8 BiI2CRd(void *dst, u16 addr, u16 len)
{
    return __BiI2CRd(addr, dst, len);
}

u8 bi_sd_to_rom(u32 dst, u16 slen) {

    u16 resp = DMA_STA_BUSY;

    bi_reg_wr(REG_DMA_ADDR, dst*512);
    bi_reg_wr(REG_DMA_LEN, slen);

    bi_sd_switch_mode(REG_SD_DAT_RD);
    while ((resp & DMA_STA_BUSY)) {
        resp = bi_reg_rd(REG_DMA_STA);
    }

    if ((resp & DMA_STA_ERROR))return 0xD3;

    return 0;
}

u8 bi_sd_dat_rd() {

    bi_sd_switch_mode(REG_SD_DAT_RD);
    bi_reg_wr(REG_SD_DAT_RD, 0xffff);
    //bi_sd_busy();
    return bi_reg_rd(REG_SD_DAT_RD);
}

void bi_sd_busy() {
    while ((bi_reg_rd(REG_SD_STATUS) & SD_STA_BUSY) != 0);
}

void bi_sd_cmd_wr(u8 val) {
    bi_sd_switch_mode(REG_SD_CMD_WR);
    bi_reg_wr(REG_SD_CMD_WR, val);
    bi_sd_busy();
}

u8 bi_sd_cmd_rd() {

    bi_sd_switch_mode(REG_SD_CMD_RD);
    bi_reg_wr(REG_SD_CMD_RD, 0xffff);
    bi_sd_busy();
    return bi_reg_rd(REG_SD_CMD_RD);
}

u8 BiGetIOMVer() {

    return bi_reg_rd(REG_IOM_VER);
}

u8 BiGetMCNVer() {

    return bi_reg_rd(REG_MCN_VER);
}

u16 BiTimerGet() {

    return bi_reg_rd(REG_TIMER);
}

u8 bi_usb_busy() {

    u32 tout = 0;

    while ((bi_reg_rd(REG_USB_CFG) & USB_STA_ACT) != 0) {

        if (tout++ != 8192)continue;
        bi_reg_wr(REG_USB_CFG, USB_CMD_RD_NOP);
        return BI_ERR_USB_TOUT;
    }

    return 0;
}

u8 bi_usb_wr(void *src, u32 len) {

    u8 resp = 0;
    u16 blen, baddr;

    bi_reg_wr(REG_USB_CFG, USB_CMD_WR_NOP);

    while (len) {

        blen = 512; //tx block len
        if (blen > len)blen = len;
        baddr = 512 - blen; //address in fpga internal buffer. data length equal to 512-int buffer addr

        sysPI_wr(src, REG_ADDR(REG_USB_DAT + baddr), blen); //copy data to the internal buffer
        src += 512;

        bi_reg_wr(REG_USB_CFG, USB_CMD_WR | baddr); //usb write request

        resp = bi_usb_busy(); //wait until the requested data amount is transferred
        if (resp)break; //timeout

        len -= blen;
    }

    return resp;
}

u8 bi_usb_can_wr() {

    u32 status = bi_reg_rd(REG_USB_CFG) & (USB_STA_PWR | USB_STA_TXE);
    if (status == USB_STA_PWR)return 1;
    return 0;
}

u8 bi_usb_can_rd() {

    u32 status = bi_reg_rd(REG_USB_CFG) & (USB_STA_PWR | USB_STA_RXF);
    if (status == USB_STA_PWR)return 1;
    return 0;
}

u8 bi_sd_to_ram(void *dst, u16 slen) {

    u16 i;
    u8 crc[8];
    u32 old_pwd = IO_READ(PI_BSD_DOM1_PWD_REG);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x09);


    while (slen--) {

        bi_sd_bitlen(1);
        i = 1;
        while (bi_sd_dat_rd() != 0xf0) {
            i++;
            if (i == 0) {
                IO_WRITE(PI_BSD_DOM1_PWD_REG, old_pwd);
                return DISK_ERR_WR1;
            }
        }

        bi_sd_bitlen(4);

        bi_sd_switch_mode(REG_SD_DAT_RD);
        sysPI_rd(dst, REG_ADDR(REG_SDIO_ARD), 512);
        sysPI_rd(crc, REG_ADDR(REG_SDIO_ARD), 8);
        dst += 512;

    }

    IO_WRITE(PI_BSD_DOM1_PWD_REG, old_pwd);

    return 0;
}

u8 bi_usb_rd(void *dst, u32 len) {

    u8 resp = 0;
    u16 blen, baddr;

    while (len) {

        blen = 512; //rx block len
        if (blen > len)blen = len;
        baddr = 512 - blen; //address in fpga internal buffer. requested data length equal to 512-int buffer addr


        bi_reg_wr(REG_USB_CFG, USB_CMD_RD | baddr); //usb read request. fpga will receive usb bytes until the buffer address reaches 512

        resp = bi_usb_busy(); //wait until requested data amount will be transferred to the internal buffer
        if (resp)break; //timeout

        sysPI_rd(dst, REG_ADDR(REG_USB_DAT + baddr), blen); //get data from internal buffer

        dst += blen;
        len -= blen;
    }

    return resp;
}

void bi_usb_init() {

    u8 buff[512];
    u8 resp;
    bi_reg_wr(REG_USB_CFG, USB_CMD_RD_NOP); //turn off usb r/w activity

    //flush fifo buffer
    while (bi_usb_can_rd()) {
        resp = bi_usb_rd(buff, 512);
        if (resp)break;
    }
}

void bi_init() {

    biSysCfg = 0;

    //unlock regs
    bi_reg_wr(REG_KEY, 0xAA55);

    bi_reg_wr(REG_SYS_CFG, biSysCfg);

    //flush usb 
    bi_usb_init();

    bi_sd_cfg = 0;
    bi_reg_wr(REG_SD_STATUS, bi_sd_cfg);

    bi_sd_cmd_rd();
    bi_sd_dat_rd();

    //turn off backup ram
    bi_set_save_type(SAVE_OFF);

    BiBootCfgGet(0xFFFF);
    BiBootCfgSet(BI_BCFG_CICLOCK);
}

u8 bi_usb_rd_end(void *dst) {

    u8 resp = bi_usb_busy();
    if (resp)return resp;

    sysPI_rd(dst, REG_ADDR(REG_USB_DAT), 512);

    return 0;
}

static unsigned long long SI_eeprom_status_block[8] =
{
    0x00000000ff010306,
    0xfffffffffe000000,
    0,
    0,
    0,
    0,
    0,
    1
};
static unsigned long long SI_eeprom_status_output[8];

u32 BiRTCInfo()
{
    sysExecPIF(SI_eeprom_status_block, SI_eeprom_status_output);
    return SI_eeprom_status_output[1] >> 32;
}

void BiRTCSet(u8 *src, u8 flag)
{
    u8 buff[8];
    memset(buff, 0, 8);
    if (src) memcpy(buff, src, 7);
    buff[7] = flag != 0;
    sysPI_wr(buff, REG_ADDR(REG_RTC_SET), 8);
}

u8 BiRTCWr(u8 *src)
{
    u8 buff[16];
    memset(buff, 0, 16);
    buff[0] = src[0];
    buff[1] = src[1];
    buff[2] = src[2] & 0x3F;
    buff[3] = src[4] & 0x07;
    buff[4] = src[3];
    buff[5] = src[5] & 0x1F;
    buff[6] = src[6];
    return BiI2CWr(buff, 0x100, 16);
}

void BiCartFill(u8 c, unsigned long pi_address, unsigned long len)
{
    u8 buff[2048];
    int n = 2048;
    memset(buff, c, 2048);
    while (len)
    {
        if (n > len) n = len;
        sysPI_wr(buff, pi_address, n);
        pi_address += n;
        len -= n;
    }
}

void BiCartRomFill(u8 c, unsigned long rom_address, unsigned long len)
{
    BiCartFill(c, BI_ROM_ADDR|rom_address, len);
}

void BiCartRamFill(u8 c, unsigned long ram_address, unsigned long len)
{
    bi_set_save_type(SAVE_SRM128K);
    BiCartFill(c, BI_RAM_ADDR|ram_address, len);
    bi_set_save_type(SAVE_OFF);
}

static unsigned long long SI_eeprom_read_block[8] =
{
    0x0000000002080400,
    0xffffffffffffffff,
    0xfffe000000000000,
    0,
    0,
    0,
    0,
    1
};
static unsigned long long SI_eeprom_read_output[8];

void BiRTCGet(u8 *dst)
{
    u8 buff[8];
    SI_eeprom_read_block[0] = 0x0000000002090700 | 2;
    sysExecPIF(SI_eeprom_read_block, SI_eeprom_read_output);
    memcopy(&SI_eeprom_read_output[1], buff, 8);
    dst[0] = buff[0];
    dst[1] = buff[1];
    dst[2] = buff[2] & 0x7F;
    dst[3] = buff[3];
    dst[4] = buff[4];
    dst[5] = buff[5];
    dst[6] = buff[6];
}

u8 bi_ram_to_sd(void *src, u16 slen) {

    u8 resp;
    u16 crc[4];

    while (slen--) {

        sdCrc16(src, crc);

        bi_sd_bitlen(2);
        bi_sd_dat_wr(0xff);
        bi_sd_dat_wr(0xf0);

        bi_sd_bitlen(4);
        sysPI_wr(src, REG_ADDR(REG_SDIO_ARD), 512);
        sysPI_wr(crc, REG_ADDR(REG_SDIO_ARD), 8);
        src += 512;

        bi_sd_bitlen(1);
        bi_sd_dat_wr(0xff);

        for (int i = 0;; i++) {
            resp = bi_sd_dat_rd();
            if ((resp & 1) == 0)break;
            if (i == 1024)return 0xD6;
        }

        resp = 0;
        for (int i = 0; i < 3; i++) {
            resp <<= 1;
            resp |= bi_sd_dat_rd() & 1;
        }

        resp &= 7;
        if (resp != 0x02) {
            if (resp == 5)return 0xD7; //crc error
            return 0xD8;
        }

        for (int i = 0;; i++) {

            if (bi_sd_dat_rd() == 0xff)break;
            if (i == 65535)return 0xD5;
        }
    }


    return 0;
}

/* Program the LATTICE iCE40 */
u8 BiIOMWr(void *src, u32 len)
{
    u8 resp = 0;
    u8 buff[32];
    u32 pwd = IO_READ(PI_BSD_DOM1_PWD_REG);
    u16 boot_cfg = bi_reg_rd(REG_BOOT_CFG);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x14);
    bi_reg_wr(REG_IOM_CFG, IOM_CFG_ACT|IOM_CFG_RST|IOM_CFG_SS);
    sleep(5);
    bi_reg_wr(REG_IOM_CFG, IOM_CFG_ACT);
    sleep(5);
    bi_reg_wr(REG_IOM_CFG, IOM_CFG_ACT|IOM_CFG_RST);
    sleep(5);
    while (len)
    {
        u32 n = 512;
        if (n > len) n = len;
        sysPI_wr(src, REG_ADDR(REG_IOM_DAT), n);
        src += n;
        len -= n;
    }
    memset(buff, 0xFF, 32);
    sysPI_wr(buff, REG_ADDR(REG_IOM_DAT), 32);
    bi_reg_wr(REG_IOM_CFG, IOM_CFG_RST|IOM_CFG_SS);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, pwd);
    sleep(5);
    if (!(bi_reg_rd(REG_IOM_CFG) & IOM_STA_CDN)) resp = BI_ERR_FPG_CFG;
    bi_reg_wr(REG_BOOT_CFG, boot_cfg);
    return resp;
}

/* Program the ALTERA Cyclone IV */
u8 BiMCNWr(void *src, u32 len)
{
    u8 resp = 0;
    u8 buff[256];
    u32 pwd = IO_READ(PI_BSD_DOM1_PWD_REG);
    IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x14);
    bi_reg_wr(REG_FPG_CFG, 0);
    sleep(5);
    bi_reg_wr(REG_FPG_CFG, FPG_CFG_NCFG);
    sleep(5);
    while (len)
    {
        u32 n = 512;
        if (n > len) n = len;
        sysPI_wr(src, REG_ADDR(REG_FPG_DAT), n);
        src += n;
        len -= n;
    }
    memset(buff, 0xFF, 256);
    /* This can't be right. */
    resp = BI_ERR_FPG_CFG;
    u32 tout = 0;
    while (tout != 2048)
    {
        if (++tout && (bi_reg_rd(REG_FPG_CFG) & FPG_STA_CDON))
        {
            resp = 0;
            tout = 2048;
            continue;
        }
        sysPI_wr(buff, REG_ADDR(REG_FPG_DAT), 256);
    }
    IO_WRITE(PI_BSD_DOM1_PWD_REG, pwd);
    sleep(5);
    return resp;
}
