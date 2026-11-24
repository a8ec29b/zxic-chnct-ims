#include "sms_pdu.h"

#include <string.h>

/* 3GPP TS 23.038 default alphabet, represented as Unicode code points. */
static const unsigned short gsm7[128] = {
    0x0040,0x00a3,0x0024,0x00a5,0x00e8,0x00e9,0x00f9,0x00ec,0x00f2,0x00c7,0x000a,0x00d8,0x00f8,0x000d,0x00c5,0x00e5,
    0x0394,0x005f,0x03a6,0x0393,0x039b,0x03a9,0x03a0,0x03a8,0x03a3,0x0398,0x039e,0,0x00c6,0x00e6,0x00df,0x00c9,
    0x0020,0x0021,0x0022,0x0023,0x00a4,0x0025,0x0026,0x0027,0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x00a1,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005a,0x00c4,0x00d6,0x00d1,0x00dc,0x00a7,
    0x00bf,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007a,0x00e4,0x00f6,0x00f1,0x00fc,0x00e0
};

static unsigned int extension(unsigned int c)
{
    switch (c) {
    case 0x0a: return 0x0c; case 0x14: return '^'; case 0x28: return '{';
    case 0x29: return '}'; case 0x2f: return '\\'; case 0x3c: return '[';
    case 0x3d: return '~'; case 0x3e: return ']'; case 0x40: return '|';
    case 0x65: return 0x20ac; case 0x1b: return ' ';
    default: return gsm7[c];
    }
}

static unsigned int septet(const unsigned char *p, size_t len, size_t bit)
{
    size_t pos = bit / 8;
    unsigned int v = p[pos] >> (bit % 8);
    if (bit % 8 > 1 && pos + 1 < len) v |= (unsigned int)p[pos + 1] << (8 - bit % 8);
    return v & 0x7f;
}

static int append_utf8(char *out, size_t cap, size_t *used, unsigned int cp)
{
    if (cp < 0x80) {
        if (*used + 1 >= cap) return -1;
        out[(*used)++] = (char)cp;
    } else if (cp < 0x800) {
        if (*used + 2 >= cap) return -1;
        out[(*used)++] = (char)(0xc0 | (cp >> 6));
        out[(*used)++] = (char)(0x80 | (cp & 63));
    } else {
        if (*used + 3 >= cap) return -1;
        out[(*used)++] = (char)(0xe0 | (cp >> 12));
        out[(*used)++] = (char)(0x80 | ((cp >> 6) & 63));
        out[(*used)++] = (char)(0x80 | (cp & 63));
    }
    out[*used] = 0;
    return 0;
}

static int decode_gsm(const unsigned char *p, size_t len, size_t bit, size_t count,
                      char *out, size_t cap, int as_hex)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t i, used = 0;
    int escaped = 0;
    if (bit + count * 7 > len * 8) return -1;
    for (i = 0; i < count; i++) {
        unsigned int c = septet(p, len, bit + i * 7), cp;
        if (!escaped && c == 0x1b) { escaped = 1; continue; }
        cp = escaped ? extension(c) : gsm7[c]; escaped = 0;
        if (as_hex) {
            if (used + 4 >= cap) return -1;
            out[used++] = digits[cp >> 12]; out[used++] = digits[(cp >> 8) & 15];
            out[used++] = digits[(cp >> 4) & 15]; out[used++] = digits[cp & 15];
        } else if (append_utf8(out, cap, &used, cp)) return -1;
    }
    if (escaped) return -1;
    out[used] = 0;
    return 0;
}

static int address(const unsigned char *p, size_t len, char *out, size_t cap, size_t *taken)
{
    static const char symbols[] = "0123456789*#abc";
    size_t digits, bytes, i, n = 0;
    if (len < 2) return -1;
    digits = p[0]; bytes = (digits + 1) / 2;
    if (len < bytes + 2) return -1;
    if ((p[1] & 0x70) == 0x50) {
        if (decode_gsm(p + 2, bytes, 0, digits * 4 / 7, out, cap, 0)) return -1;
    } else {
        if ((p[1] & 0x70) == 0x10) out[n++] = '+';
        if (n + digits >= cap) return -1;
        for (i = 0; i < digits; i++) {
            unsigned int d = (p[2 + i / 2] >> (4 * (i % 2))) & 15;
            if (d >= 15) return -1;
            out[n++] = symbols[d];
        }
        out[n] = 0;
    }
    *taken = bytes + 2;
    return 0;
}

static int bcd(unsigned char b)
{
    if ((b & 15) > 9 || (b >> 4) > 9) return -1;
    return (b & 15) * 10 + (b >> 4);
}

static long long days_from_civil(int y, unsigned int m, unsigned int d)
{
    int era;
    unsigned int yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned int)(y - era * 400);
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long long)era * 146097 + doe - 719468;
}

static int timestamp(const unsigned char *p, struct sms_deliver *out)
{
    int y = bcd(p[0]), m = bcd(p[1]), d = bcd(p[2]), h = bcd(p[3]);
    int min = bcd(p[4]), sec = bcd(p[5]), zone = bcd(p[6] & 0xf7);
    if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31 || h < 0 || h > 23 ||
        min < 0 || min > 59 || sec < 0 || sec > 59 || zone < 0 || zone > 96) return -1;
    out->year = 2000 + y; out->month = m; out->day = d; out->hour = h;
    out->minute = min; out->second = sec; out->timezone_quarters = (p[6] & 8) ? -zone : zone;
    out->epoch = days_from_civil(out->year, (unsigned int)m, (unsigned int)d) * 86400 +
        h * 3600 + min * 60 + sec - out->timezone_quarters * 900;
    return 0;
}

static int header(const unsigned char *p, size_t len, struct sms_deliver *out, size_t *used)
{
    size_t end, i;
    if (!len || (end = (size_t)p[0] + 1) > len) return -1;
    for (i = 1; i < end;) {
        unsigned int ie, n;
        if (i + 2 > end) return -1;
        ie = p[i++]; n = p[i++];
        if (i + n > end) return -1;
        if (ie == 0x00 && n == 3) {
            out->concat_reference = p[i]; out->total = p[i + 1]; out->sequence = p[i + 2];
        } else if (ie == 0x08 && n == 4) {
            out->concat_reference = ((unsigned int)p[i] << 8) | p[i + 1];
            out->total = p[i + 2]; out->sequence = p[i + 3]; out->concat_16bit = 1;
        } else if (ie == 0x24 || ie == 0x25) return -1;
        i += n;
    }
    if (!out->total || !out->sequence || out->sequence > out->total) return -1;
    *used = end;
    return 0;
}

static int tpdu(const unsigned char *p, size_t len, struct sms_deliver *out)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t pos, n, udl, octets, hlen = 0, i;
    unsigned char flags, dcs;
    int alphabet;
    if (len < 2 || ((p[0] & 3) != 0 && (p[0] & 3) != 3)) return -1;
    flags = *p++ ; len--;
    if (address(p, len, out->sender, sizeof(out->sender), &pos) || len < pos + 10) return -1;
    p += pos; len -= pos;
    if (p[0] == 0x7f && (p[1] & 0x13) == 0x12) return -1;
    dcs = p[1];
    if ((dcs & 0xc0) == 0 && !(dcs & 0x20)) alphabet = (dcs >> 2) & 3;
    else if ((dcs & 0xf0) == 0xf0) alphabet = (dcs & 4) ? 1 : 0;
    else return -1;
    if (alphabet != 0 && alphabet != 2) return -1;
    if (timestamp(p + 2, out)) return -1;
    udl = p[9]; p += 10; len -= 10;
    octets = alphabet == 0 ? (udl * 7 + 7) / 8 : udl;
    if (len != octets) return -1;
    if (flags & 0x40) {
        if (header(p, len, out, &hlen)) return -1;
    }
    if (alphabet == 0) {
        n = (hlen * 8 + 6) / 7;
        if (n > udl) return -1;
        return decode_gsm(p, len, n * 7, udl - n, out->utf16be_hex, sizeof(out->utf16be_hex), 1);
    }
    if ((udl - hlen) & 1 || (udl - hlen) * 2 >= sizeof(out->utf16be_hex)) return -1;
    for (i = hlen; i < udl; i++) {
        out->utf16be_hex[(i - hlen) * 2] = digits[p[i] >> 4];
        out->utf16be_hex[(i - hlen) * 2 + 1] = digits[p[i] & 15];
    }
    out->utf16be_hex[(udl - hlen) * 2] = 0;
    return 0;
}

int sms_decode_rp_data(const unsigned char *data, size_t len, struct sms_deliver *out)
{
    size_t i = 2, n;
    if (len < 5 || (data[0] & 7) != 1) return -1;
    memset(out, 0, sizeof(*out));
    out->rp_reference = data[1]; out->sequence = out->total = 1;
    n = data[i++]; if (n > len - i) return -1; i += n;
    if (i >= len) return -1;
    n = data[i++]; if (n > len - i) return -1; i += n;
    if (i >= len) return -1;
    n = data[i++]; if (!n || n != len - i) return -1;
    return tpdu(data + i, n, out);
}
