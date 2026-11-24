#define _GNU_SOURCE
#include "ims_activate.h"

#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int ims_activate_begin(void)
{
    static const char command[] = "AT+ZPDPACT=0,\"IPV4V6\",\"ims\",0,1,1,0,\"\",\"\",\"\",0,1\r\n";
    int key, queue = -1;
    void *nv, *softap, *atutils;
    int (*send_request)(int, const char *, int, int, int);
    nv = dlopen("/lib/libnvram.so", RTLD_LAZY | RTLD_GLOBAL);
    softap = nv ? dlopen("/lib/libsoftap.so", RTLD_LAZY | RTLD_GLOBAL) : NULL;
    atutils = softap ? dlopen("/lib/libatutils.so", RTLD_LAZY | RTLD_GLOBAL) : NULL;
    send_request = atutils ? dlsym(atutils, "send_app_req") : NULL;
    if (send_request) {
        for (key = 4659; key >= 4402; key--) {
            queue = msgget((key_t)key, IPC_CREAT | IPC_EXCL | 0600);
            if (queue >= 0 || errno != EEXIST) break;
        }
        if (queue >= 0) (void)send_request(key, command, (int)strlen(command), 60, 1);
    }
    if (atutils) dlclose(atutils);
    if (softap) dlclose(softap);
    if (nv) dlclose(nv);
    return queue;
}

int ims_activate_poll(int queue)
{
    struct { long type; unsigned char data[4097]; } response;
    const char *p;
    ssize_t length = msgrcv(queue, &response, sizeof(response.data) - 1, 0, IPC_NOWAIT | MSG_NOERROR);
    if (length < 0) return errno == ENOMSG || errno == EINTR ? 0 : -1;
    /* The firmware sends event 5531 for ZPDPACT; IMS discovery confirms readiness. */
    if (length >= 28 && response.data[8] == 0x9b && response.data[9] == 0x15) return 2;
    if (length < 16 || response.data[8] != 0x2f || response.data[9] != 0x15) return 0;
    response.data[length] = 0;
    p = (const char *)response.data + 16;
    while (*p) {
        const char *end;
        size_t n;
        while (*p == '\r' || *p == '\n') p++;
        end = p + strcspn(p, "\r\n");
        n = (size_t)(end - p);
        if (n == 2 && !memcmp(p, "OK", 2)) return 1;
        if ((n == 5 && !memcmp(p, "ERROR", 5)) ||
            (n >= 11 && !memcmp(p, "+CME ERROR:", 11))) return -1;
        p = end;
    }
    return 0;
}

void ims_activate_end(int queue)
{
    (void)msgctl(queue, IPC_RMID, NULL);
}
