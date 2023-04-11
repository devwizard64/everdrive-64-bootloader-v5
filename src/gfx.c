#include "firmware.h"

u16 *gfx_buff;
u16 *g_disp_ptr;
u16 g_cur_pal;
u16 g_cons_ptr;
u8 g_last_x;
u8 g_last_y;

void gSetPal(u16 pal) {
    g_cur_pal = pal;
}

void gAppendString(u8 *str) {
    while (*str != 0)*g_disp_ptr++ = *str++ + g_cur_pal;
}

void gAppendStringLen(u8 *str, u8 len) {
    while (*str != 0 && len--)*g_disp_ptr++ = *str++ + g_cur_pal;
}

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
    g_disp_ptr = &gfx_buff[g_cons_ptr];
    g_last_x = x;
    g_last_y = y;
}

void gSetX(u8 x) {

    gSetXY(x, g_last_y);
}

void gSetY(u8 y) {

    gSetXY(g_last_x, y);
}

u8 gGetX() {

    return g_last_x;
}

u8 gGetY() {

    return g_last_y;
}

void GfxResetXY() {

    gSetXY(G_BORDER_X, G_BORDER_Y);
}

void gConsPrintLen(u8 *str, u8 len) {

    g_disp_ptr = &gfx_buff[g_cons_ptr];
    g_cons_ptr += G_SCREEN_W;
    g_last_y++;
    gAppendStringLen(str, len);
}

void gConsPrint(u8 *str) {

    gConsPrintLen(str, G_MAX_STR_LEN);
}

void GfxChangePal(u8 y, u16 pal) {

    u16 *ptr = &gfx_buff[y * G_SCREEN_W];
    u8 len = G_SCREEN_W;
    while (len--)
    {
        *ptr = (*ptr & 0x7F) + pal;
        ptr++;
    }
}

void GfxFillVLine(u16 chr, u8 x, u8 y, u8 height) {

    gSetXY(x, y);
    u16 ptr = g_cons_ptr;
    u16 val = chr + g_cur_pal;
    while (height--)
    {
        gfx_buff[ptr] = val;
        ptr += G_SCREEN_W;
    }
}

void GfxDumpHex(u8 *a0, u16 len) {

    for (u16 i = 0; i < len; i++) {
        if (!(i & 0xf)) gConsPrint((u8 *)"");
        gAppendHex8(*a0++);
    }
}

void GfxFillHLine(u16 chr, u8 x, u8 y, u8 width) {

    gSetXY(x, y);
    u16 val = chr + g_cur_pal;
    u16 len = width*2;
    GfxFill(&gfx_buff[g_cons_ptr], val, len);
}

void GfxAppendRect(u16 chr, u8 width, u8 height) {

    u16 val = chr + g_cur_pal;
    u16 ptr = g_cons_ptr;
    u8 len = width*2;
    while (height--) {
        GfxFill(&gfx_buff[ptr], val, len);
        ptr += G_SCREEN_W;
    }
}

void GfxFillRect(u16 chr, u8 x, u8 y, u8 width, u8 height) {

    gSetXY(x, y);
    GfxAppendRect(chr, width, height);
}

void gCleanScreen() {

    g_cur_pal = 0;
    GfxResetXY();
    GfxFill(gfx_buff, 0, G_SCREEN_W * G_SCREEN_H * 2);
    gSetPal(PAL_B1);
}

void GfxPrintCenterLen(u8 *str, u8 len) {

    u8 width = slen(str);
    if (width > len) width = len;
    u16 ptr = g_cons_ptr;
    gSetX((G_SCREEN_W-width) / 2);
    gConsPrintLen(str, len);
    g_cons_ptr = ptr + G_SCREEN_W;
}

void GfxPrintCenter(u8 *str) {

    GfxPrintCenterLen(str, G_MAX_STR_LEN);
}

void GfxAppendDec(u32 val) {

    u8 buff[16];
    buff[0] = '\0';
    FmtDecBuff(buff, val);
    gAppendString(buff);
}

void gInit(u16 *gfx_mem) {

    gfx_buff = gfx_mem;
    gCleanScreen();
    FmtInit(0);
}
