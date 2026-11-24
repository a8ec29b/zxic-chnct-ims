/* Read-only AF_ALG availability probe for the target's Linux ARM EABI. */
struct sockaddr_alg {
    unsigned short family;
    unsigned char type[14];
    unsigned int feature, mask;
    unsigned char name[64];
};

void *memset(void *dst, int value, unsigned int n)
{
    unsigned char *p = dst;
    while (n--) *p++ = (unsigned char)value;
    return dst;
}

static long call(long nr, long a, long b, long c)
{
    register long r0 __asm__("r0") = a, r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c, r7 __asm__("r7") = nr;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

static void probe(const char *type, const char *name)
{
    struct sockaddr_alg sa = {38, {0}, 0, 0, {0}};
    char result[16];
    int i, n = 0;
    long fd, rc;
    for (i = 0; type[i] && i < 13; i++) sa.type[i] = type[i];
    for (i = 0; name[i] && i < 63; i++) sa.name[i] = name[i];
    fd = call(281, 38, 5, 0);
    rc = fd < 0 ? fd : call(282, fd, (long)&sa, sizeof(sa));
    if (fd >= 0) call(6, fd, 0, 0);
    if (fd < 0) call(4, 1, (long)"socket ", 7);
    else call(4, 1, (long)"bind ", 5);
    call(4, 1, (long)name, i);
    result[n++] = ':'; result[n++] = ' ';
    if (rc < 0) { result[n++] = '-'; rc = -rc; }
    result[n++] = "0123456789abcdef"[(rc >> 4) & 15];
    result[n++] = "0123456789abcdef"[rc & 15]; result[n++] = '\n';
    call(4, 1, (long)result, n);
}

void _start(void)
{
    probe("skcipher", "cbc(aes)");
    probe("hash", "hmac(sha1)");
    probe("hash", "hmac(md5)");
    call(1, 0, 0, 0);
    __builtin_unreachable();
}
