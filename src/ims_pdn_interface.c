#define _POSIX_C_SOURCE 200809L
#include "ims_pdn.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int command(const char *path, const char *const args[])
{
    int status, fd;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (!pid) {
        fd = open("/dev/null", O_RDWR);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
        execv(path, (char *const *)args);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) != pid) return -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

int ims_pdn_prepare_interface(struct ims_pdn_info *info)
{
    char dev[16], command_line[64], prefix_text[128], address[INET6_ADDRSTRLEN + 8];
    unsigned char prefix[16];
    const char *up[] = {"ifconfig", dev, "up", NULL};
    const char *slaac[] = {"zte_ipv6_slaac", "-i", dev, NULL};
    const char *addr[] = {"ip", "-6", "addr", "add", address, "dev", dev, NULL};
    const char *route[] = {"ip", "-6", "route", "add", address, "dev", dev, NULL};
    FILE *f;
    size_t i, n;
    if (info->cid < 1 || info->cid > 8 || info->address[0] != 0xfe ||
        (info->address[1] & 0xc0) != 0x80) return -1;
    snprintf(dev, sizeof(dev), "wan%d", info->cid);
    if (command("/sbin/ifconfig", up) || command("/sbin/zte_ipv6_slaac", slaac)) return -1;
    snprintf(command_line, sizeof(command_line), "/bin/nv get %s_ipv6_prefix_info", dev);
    f = popen(command_line, "r");
    if (!f) return -1;
    if (!fgets(prefix_text, sizeof(prefix_text) - 2, f)) { pclose(f); return -1; }
    if (pclose(f)) return -1;
    n = strcspn(prefix_text, "\r\n"); prefix_text[n] = 0;
    if (!n || prefix_text[n - 1] != ':') return -1;
    prefix_text[n++] = ':'; prefix_text[n] = 0;
    if (inet_pton(AF_INET6, prefix_text, prefix) != 1 || (prefix[0] & 0xe0) != 0x20) return -1;
    memcpy(prefix + 8, info->address + 8, 8);
    if (!inet_ntop(AF_INET6, prefix, address, sizeof(address))) return -1;
    strcat(address, "/126");
    if (command("/bin/ip", addr)) return -1;
    for (i = 0; i < info->pcscf_count; i++) {
        if (!inet_ntop(AF_INET6, info->pcscf[i], address, sizeof(address))) return -1;
        strcat(address, "/128");
        (void)command("/bin/ip", route);
    }
    return 0;
}
