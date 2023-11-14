#include "everdrive.h"

u8 *fmtPtr;

void GfxFill(u16 *dst, u16 val, u32 len) {

    len /= 2;
    while (len--) *dst++ = val;
}

void memcopy(const void *src, void *dst, u32 len) {

    memcpy(dst, src, len);
}

void memfill(void *dst, u8 val, u32 len) {

    memset(dst, val, len);
}

u8 StrCmp(u8 *str1, u8 *str2, u8 len) {

    u8 s1;
    u8 s2;
    while (len--) {

        s1 = *str1++;
        s2 = *str2++;

        if (s1 != s2) return 0;
    }

    return 1;
}

u8 streq(u8 *str1, u8 *str2) {

    u8 len1 = slen(str1);
    u8 len2 = slen(str2);
    if (len1 != len2) return 0;
    return streql(str1, str2, len1);
}

u8 streql(u8 *str1, u8 *str2, u8 len) {

    u8 s1;
    u8 s2;
    while (len--) {

        s1 = *str1++;
        s2 = *str2++;
        if (s1 >= 'A' && s1 <= 'Z')s1 |= 0x20;
        if (s2 >= 'A' && s2 <= 'Z')s2 |= 0x20;

        if (s1 != s2) return 0;
    }

    return 1;
}

u8 slen(u8 *str) {

    u8 len = 0;
    while (*str++)len++;
    return len;
}

u8 *StrGetExt(u8 *str);

u8 StrHasExt(u8 *str1, u8 *str2) {

    if (*str1 == '.') str1++;
    return streq(str1, StrGetExt(str2));
}

u8 StrHasExtList(u8 **list, u8 *str) {

    for (; *list; list++) {
        if (StrHasExt(*list, str)) return 1;
    }
    return 0;
}

u8 *StrGetBasename(u8 *str) {

    u8 *ptr = str;
    while (*str++) {
        if (*str == '/') ptr = str + 1;
    }
    return ptr;
}

void scopy(u8 *src, u8 *dst) {

    while (*src != 0) {
        *dst++ = *src++;
    }
    *dst = 0;
}

void FmtInit(u8 *str) {

    fmtPtr = str;
    if (fmtPtr) {

        while (*fmtPtr) fmtPtr++;
    }
}

void FmtStr(u8 *str) {

    while (*str) *fmtPtr++ = *str++;
    *fmtPtr = 0;
}

void FmtDec(u32 val) {

    u8 buff[11];
    u8 *str = buff + 10;
    *str = 0;
    if (val == 0) *--str = '0';
    while (val) {

        *--str = '0' + val % 10;
        val /= 10;
    }

    FmtStr(str);
}

void FmtHex8(u8 val) {

    u8 hi = val >> 4;
    u8 lo = val & 15;
    hi += (hi < 10 ? '0' : '7');
    lo += (lo < 10 ? '0' : '7');
    *fmtPtr++ = hi;
    *fmtPtr++ = lo;
    *fmtPtr = 0;
}

void FmtHex16(u16 val) {

    FmtHex8(val >> 8);
    FmtHex8(val);
}

void FmtHex32(u32 val) {

    FmtHex16(val >> 16);
    FmtHex16(val);
}

void FmtStrBuff(u8 *buff, u8 *str) {

    u8 *ptr = fmtPtr;
    FmtInit(buff);
    FmtStr(str);
    fmtPtr = ptr;
}

void FmtDecBuff(u8 *buff, u32 val) {

    u8 *ptr = fmtPtr;
    FmtInit(buff);
    FmtDec(val);
    fmtPtr = ptr;
}

u8 StrGetMaxLen(u8 **list) {

    u8 max = 0;
    if (list) {
        for (u8 i = 0; list[i]; i++) {
            u8 len = slen(list[i]);
            if (max < len) max = len;
        }
    }
    return max;
}

void strhicase(u8 *str, u16 len) {

    while (*str != 0 && len) {
        if (*str >= 'a' && *str <= 'z')*str &= ~0x20;
        str++;
        len--;
    }

}

u8 *StrGetExt(u8 *str) {

    u8 *ptr = 0;
    while (*str) {
        if (*str == '.') ptr = str;
        str++;
    }
    if (ptr) str = ptr + 1;
    return str;
}

u8 *StrTrimSpace(u8 *str) {

    while (*str == ' ') *str-- = 0;
    *++str = 0;
    return str;
}

void StrStrip(u8 *str) {

    u8 *ptr = str;
    u8 start = 0;
    while (*ptr == ' ') {
        ptr++;
        start++;
    }
    u8 len = 0;
    u8 pos = 0;
    while (*ptr != 0) {
        pos++;
        if (*ptr != ' ') len = pos;
        ptr++;
    }
    u8 *dst = str;
    u8 *src = str + start;
    while (len--) *dst++ = *src++;
    *dst = 0;
}

void FmtTime(u16 time) {

    FmtHex8(SysDecToBCD(time >> 11));
    FmtStr((u8 *)":");
    FmtHex8(SysDecToBCD(time >> 5 & 63));
    FmtStr((u8 *)":");
    FmtHex8(SysDecToBCD(2*(time & 31)));
}

void FmtDate(u16 date) {

    FmtHex8(SysDecToBCD(date & 31));
    FmtStr((u8 *)".");
    FmtHex8(SysDecToBCD(date >> 5 & 15));
    FmtStr((u8 *)".");
    FmtDec(1980 + (date >> 9));
}
