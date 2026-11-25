#define _GNU_SOURCE
#include "ui_server.h"
#include "at_http.h"
#include "operator_gate.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define UI_PORT 18765
#define PID_FILE "/tmp/zxct-ims.pid"
#define STATUS_FILE "/tmp/zxct-ims.status"

static int active_pid(void)
{
    FILE *f = fopen(PID_FILE, "r");
    long pid = 0;
    if (f) { if (fscanf(f, "%ld", &pid) != 1) pid = 0; fclose(f); }
    if (pid > 1 && kill((pid_t)pid, 0) == 0) {
        char path[64], target[256], self[256];
        ssize_t a, b;
        snprintf(path, sizeof(path), "/proc/%ld/exe", pid);
        a = readlink(path, target, sizeof(target) - 1);
        b = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (a > 0 && b > 0) {
            target[a] = 0; self[b] = 0;
            if (!strcmp(target, self)) return (int)pid;
        }
    }
    return 0;
}

static int password_matches(const char *password)
{
    static unsigned int failures;
    static time_t blocked_until;
    time_t now = time(NULL);
    char actual[256];
    FILE *f;
    int ok = 0;
    if (now < blocked_until) return 0;
    f = popen("nv get admin_Password", "r");
    if (!f) return 0;
    if (fgets(actual, sizeof(actual), f)) {
        actual[strcspn(actual, "\r\n")] = 0;
        ok = *actual && !strcmp(actual, password);
        memset(actual, 0, sizeof(actual));
    }
    pclose(f);
    if (ok) failures = 0;
    else if (++failures >= 5) { blocked_until = now + 300; failures = 0; }
    return ok;
}

static int is_hex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int decode(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r == '%') {
            unsigned int c;
            if (!r[1] || !r[2] || !is_hex(r[1]) || !is_hex(r[2]) ||
                sscanf(r + 1, "%2x", &c) != 1 || !c || c == '\n' || c == '\r') return -1;
            *w++ = (char)c; r += 3;
        } else { *w++ = *r == '+' ? ' ' : *r; r++; }
    }
    *w = 0;
    return 0;
}

static int retry(const char *password)
{
    struct at_http at = {"127.0.0.1", NULL, {0}, 0, 0, 0, 0};
    char imsi[16];
    int pid, pipefd[2], i, result;
    pid_t child;
    if (!password_matches(password)) return 403;
    if (at_http_login(&at, password)) return 503;
    result = operator_gate(&at, imsi);
    memset(imsi, 0, sizeof(imsi));
    if (result != OPERATOR_GATE_OK) return result == OPERATOR_GATE_UNSUPPORTED_OPERATOR ? 403 : 503;
    pid = active_pid();
    if (pid) {
        if (kill((pid_t)pid, SIGTERM)) return 503;
        for (i = 0; i < 100 && active_pid(); i++) usleep(100000);
        if (active_pid()) return 503;
    }
    if (pipe(pipefd)) return 503;
    child = fork();
    if (child < 0) { close(pipefd[0]); close(pipefd[1]); return 503; }
    if (!child) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        signal(SIGPIPE, SIG_DFL);
        execl("/proc/self/exe", "zxct-ims", "--daemon", (char *)NULL);
        _exit(127);
    }
    close(pipefd[0]);
    if (write(pipefd[1], password, strlen(password)) < 0 || write(pipefd[1], "\n", 1) != 1) {
        close(pipefd[1]); waitpid(child, NULL, 0); return 503;
    }
    close(pipefd[1]);
    if (waitpid(child, &result, 0) != child || !WIFEXITED(result) || WEXITSTATUS(result)) return 503;
    return 200;
}

static void serve(int fd)
{
    char request[8192], body[128] = "stopped", origin[512] = "", host[512] = "", expected[512];
    char *p, *end, *data;
    size_t used = 0, length = 0;
    int status = 400, n;
    struct timeval timeout = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    while (used + 1 < sizeof(request)) {
        n = (int)recv(fd, request + used, sizeof(request) - used - 1, 0);
        if (n <= 0) return;
        used += (size_t)n; request[used] = 0;
        end = strstr(request, "\r\n\r\n");
        if (!end) continue;
        p = strcasestr(request, "\r\nContent-Length:");
        if (p && p < end) length = (size_t)strtoul(p + 17, NULL, 10);
        if (length >= 1024) return;
        if (used >= (size_t)(end + 4 - request) + length) break;
    }
    end = strstr(request, "\r\n\r\n");
    if (!end) return;
    data = end + 4; data[length] = 0;
    p = strcasestr(request, "\r\nOrigin: ");
    if (p && p < end) sscanf(p + 10, "%511[^\r\n]", origin);
    p = strcasestr(request, "\r\nHost: ");
    if (p && p < end) sscanf(p + 8, "%511[^:\r\n]", host);
    n = snprintf(expected, sizeof(expected), "http://%s", host);
    if (n < 0 || (size_t)n >= sizeof(expected) || !*host || strncmp(origin, expected, (size_t)n)) goto reply;
    if (origin[n]) {
        const char *port = origin + n;
        if (*port++ != ':' || !*port || strspn(port, "0123456789") != strlen(port)) goto reply;
    }
    if (!strncmp(request, "GET /status ", 12)) {
        FILE *f;
        status = 200;
        if (active_pid() && (f = fopen(STATUS_FILE, "r")) != NULL) {
            if (!fgets(body, sizeof(body), f)) strcpy(body, "starting");
            fclose(f); body[strcspn(body, "\r\n")] = 0;
            if (strcmp(body, "registered") && strcmp(body, "starting") && strcmp(body, "waiting")) strcpy(body, "starting");
        }
    } else if (!strncmp(request, "POST /retry ", 12) && length && !decode(data)) {
        status = retry(data);
        memset(data, 0, length);
        strcpy(body, status == 200 ? "starting" : "failed");
    }
reply:
    if (status != 200 && status != 403 && status != 503) status = 400;
    dprintf(fd, "HTTP/1.1 %d %s\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: %s\r\nVary: Origin\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s", status, status == 200 ? "OK" : "Error", status == 400 ? "null" : origin, strlen(body), body);
}

int ui_server_run(void)
{
    struct sockaddr_in addr;
    struct ifreq lan;
    int server, client, one = 1;
    signal(SIGPIPE, SIG_IGN);
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return 1;
    fcntl(server, F_SETFD, FD_CLOEXEC);
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&lan, 0, sizeof(lan));
    strcpy(lan.ifr_name, "br0");
    if (ioctl(server, SIOCGIFADDR, &lan)) { close(server); return 1; }
    memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET;
    addr.sin_addr = ((struct sockaddr_in *)&lan.ifr_addr)->sin_addr;
    addr.sin_port = htons(UI_PORT);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) || listen(server, 4)) { close(server); return 1; }
    while ((client = accept(server, NULL, NULL)) >= 0) {
        fcntl(client, F_SETFD, FD_CLOEXEC);
        serve(client); close(client);
    }
    close(server);
    return 1;
}
