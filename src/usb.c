#include "firmware.h"

void UsbCmdc(u32 *buff)
{
    u32 data[512/4];
    u32 dst = buff[1];
    u32 len = buff[2];
    u32 fill = buff[3];
    for (int i = 0; i < 512/4; i++) data[i] = fill;
    while (len--)
    {
        sysPI_wr(data, dst, 512);
        dst += 512;
    }
}

u8 UsbCmdw(u32 *buff)
{
    return bi_usb_rd((void *)buff[1], 512*buff[2]);
}

u8 UsbCmdr(u32 *buff)
{
    return bi_usb_wr((void *)buff[1], 512*buff[2]);
}

u8 UsbResp(u8 resp)
{
    u8 buff[16];
    buff[0] = 'c';
    buff[1] = 'm';
    buff[2] = 'd';
    buff[3] = 'r';
    buff[4] = resp;
    return bi_usb_wr(buff, 16);
}

u8 UsbCmdW(u32 *buff)
{
    u8 resp = 0;
    u8 data[512];
    u32 dst = buff[1];
    u32 len = buff[2];
    if (len) bi_usb_rd_start();
    while (len--)
    {
        resp = bi_usb_rd_end(data);
        if (len) bi_usb_rd_start();
        if (resp) return resp;
        sysPI_wr(data, dst, 512);
        dst += 512;
    }
    return resp;
}

u8 UsbCmdR(u32 *buff)
{
    u8 resp = 0;
    u8 data[512];
    u32 src = buff[1];
    u32 len = buff[2];
    while (len--)
    {
        sysPI_rd(data, src, 512);
        resp = bi_usb_wr(data, 512);
        if (resp) return resp;
        src += 512;
    }
    return resp;
}

u8 UsbCmdf(u32 *buff)
{
    u8 resp;
    u8 data[0x40000];
    u32 len = buff[2];
    if (len > 0x40000) return 0x16;
    resp = bi_usb_rd(data, 512*len);
    if (resp) return resp;
    resp = bios_80001BF0(data, 512*len);
    return resp;
}

u8 usbListener()
{
    u8 resp;
    u8 cmd;
    u8 buff[16];
    if (!bi_usb_can_rd()) return 0;
    resp = bi_usb_rd(buff, 16);
    if (resp) return resp;
    if (buff[0] != 'c' || buff[1] != 'm' || buff[2] != 'd') return 0;
    cmd = buff[3];
    if (cmd == 't')
    {
        resp = UsbResp(0);
    }
    else if (cmd == 'r')
    {
        resp = UsbCmdr((u32 *)buff);
    }
    else if (cmd == 'w')
    {
        resp = UsbCmdw((u32 *)buff);
    }
    else if (cmd == 'R')
    {
        resp = UsbCmdR((u32 *)buff);
        return resp;
    }
    else if (cmd == 'W')
    {
        resp = UsbCmdW((u32 *)buff);
        return resp;
    }
    else if (cmd == 'f')
    {
        resp = UsbCmdf((u32 *)buff);
        UsbResp(resp);
        return resp;
    }
    else if (cmd == 's')
    {
        BiBootCfgClr(BI_BCFG_BOOTMOD);
        MainBootOS();
    }
    else if (cmd == 'c')
    {
        UsbCmdc((u32 *)buff);
    }
    return resp;
}
