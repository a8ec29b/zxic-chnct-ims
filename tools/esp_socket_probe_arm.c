/* Check only whether an IPv6 raw ESP socket can be opened. */
static long call(long nr, long a, long b, long c)
{
    register long r0 __asm__("r0") = a, r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c, r7 __asm__("r7") = nr;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}

void _start(void)
{
    long fd = call(281, 10, 3, 50);
    if (fd < 0) {
        call(4, 1, (long)"ESP socket unavailable\n", 23);
        call(1, 1, 0, 0);
    }
    call(6, fd, 0, 0);
    call(4, 1, (long)"ESP socket available\n", 21);
    call(1, 0, 0, 0);
    __builtin_unreachable();
}
