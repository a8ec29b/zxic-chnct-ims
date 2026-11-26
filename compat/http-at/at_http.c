#define _GNU_SOURCE
#include "at_http.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static char *form_encode(const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    char *out = malloc(strlen(s) * 3 + 1), *p = out;
    if (!out) return NULL;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            *p++ = (char)c;
        else { *p++ = '%'; *p++ = hex[c >> 4]; *p++ = hex[c & 15]; }
    }
    *p = 0;
    return out;
}

static char *base64(const char *s)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = strlen(s), i, k = 0;
    char *out = malloc((n + 2) / 3 * 4 + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i += 3) {
        unsigned int v = (unsigned char)s[i] << 16;
        if (i + 1 < n) v |= (unsigned char)s[i + 1] << 8;
        if (i + 2 < n) v |= (unsigned char)s[i + 2];
        out[k++] = table[v >> 18]; out[k++] = table[(v >> 12) & 63];
        out[k++] = i + 1 < n ? table[(v >> 6) & 63] : '=';
        out[k++] = i + 2 < n ? table[v & 63] : '=';
    }
    out[k] = 0;
    return out;
}

static int send_all(int fd, const char *data, size_t n)
{
    while (n) {
        ssize_t sent = send(fd, data, n, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return -1;
        data += sent; n -= (size_t)sent;
    }
    return 0;
}

static int post(struct at_http *client, const char *body, char *response, size_t capacity)
{
    struct sockaddr_in addr;
    struct timeval timeout = {30, 0};
    char header[1024], *end, *cookie;
    size_t used = 0;
    int fd, header_size, status = -1;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(80);
    if (inet_pton(AF_INET, client->address, &addr.sin_addr) != 1) return -1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (client->interface && setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
                                        client->interface, strlen(client->interface) + 1)) goto out;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) goto out;
    header_size = snprintf(header, sizeof(header),
        "POST /goform/goform_set_cmd_process HTTP/1.0\r\n"
        "Host: %s\r\nReferer: http://%s/index.html\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Cookie: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        client->address, client->address, client->cookie, strlen(body));
    if (header_size < 0 || (size_t)header_size >= sizeof(header) ||
        send_all(fd, header, (size_t)header_size) || send_all(fd, body, strlen(body))) goto out;
    while (used + 1 < capacity) {
        ssize_t n = recv(fd, response + used, capacity - used - 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto out;
        if (!n) break;
        used += (size_t)n;
    }
    if (used + 1 == capacity) goto out;
    response[used] = 0;
    if (strncmp(response, "HTTP/1.", 7) || response[9] != '2' || response[10] != '0' || response[11] != '0') goto out;
    end = strstr(response, "\r\n\r\n");
    if (end) end += 4;
    else {
        end = strstr(response, "\n\n");
        if (!end) goto out;
        end += 2;
    }
    cookie = strcasestr(response, "\nSet-Cookie:");
    if (cookie && cookie < end) {
        size_t len;
        cookie += 12;
        while (*cookie == ' ') cookie++;
        len = strcspn(cookie, ";\r\n");
        if (len >= sizeof(client->cookie)) goto out;
        memcpy(client->cookie, cookie, len); client->cookie[len] = 0;
    }
    memmove(response, end, used - (size_t)(end - response) + 1);
    status = 0;
out:
    close(fd);
    return status;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int ascii_space(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

/* The firmware returns a JSON object with a string-valued result field. */
static int json_result(const char *json, char *result, size_t size)
{
    const char *p = strstr(json, "\"result\"");
    size_t used = 0;
    if (!p) return -1;
    p += 8;
    while (ascii_space(*p)) p++;
    if (*p++ != ':') return -1;
    while (ascii_space(*p)) p++;
    if (*p++ != '"') return -1;
    while (*p && *p != '"') {
        unsigned int c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            if (c == 'r') c = '\r'; else if (c == 'n') c = '\n';
            else if (c == 't') c = '\t'; else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c == 'u') {
                int a, b, d, e;
                if (strlen(p) < 4 || (a = hexval(p[0])) < 0 || (b = hexval(p[1])) < 0 ||
                    (d = hexval(p[2])) < 0 || (e = hexval(p[3])) < 0) return -1;
                c = (unsigned int)((a << 12) | (b << 8) | (d << 4) | e); p += 4;
                if (c == 0 || c >= 0x80) return -1;
            } else if (c != '"' && c != '\\' && c != '/') return -1;
        }
        if (!c || used + 1 >= size) return -1;
        result[used++] = (char)c;
    }
    if (*p != '"') return -1;
    result[used] = 0;
    return 0;
}

int at_http_login(struct at_http *client, const char *password)
{
    char response[4096], result[64];
    char *encoded = base64(password), *form;
    int rc = -1;
    if (!encoded) return -1;
    form = form_encode(encoded);
    memset(encoded, 0, strlen(encoded)); free(encoded);
    if (form) {
        size_t size = strlen(form) + 32;
        char *body = malloc(size);
        if (body) {
            snprintf(body, size, "goformId=LOGIN&password=%s", form);
            if (!post(client, body, response, sizeof(response)) &&
                !json_result(response, result, sizeof(result)) &&
                (!strcmp(result, "0") || !strcmp(result, "4"))) rc = 0;
            memset(body, 0, strlen(body)); free(body);
        }
        memset(form, 0, strlen(form)); free(form);
    }
    return rc;
}

int at_http_command(struct at_http *client, const char *command, char *result, size_t size)
{
    char response[16384];
    char *form = form_encode(command), *body;
    int rc = -1;
    if (!form) return -1;
    body = malloc(strlen(form) + 48);
    if (body) {
        sprintf(body, "goformId=EXECUTE_AT_COMMAND&at_cmd=%s", form);
        if (!post(client, body, response, sizeof(response))) rc = json_result(response, result, size);
        memset(body, 0, strlen(body));
        free(body);
    }
    memset(form, 0, strlen(form));
    free(form);
    return rc;
}

int at_http_command_once(struct at_http *client, const char *command, char *result, size_t size)
{
    return at_http_command(client, command, result, size);
}

int at_http_command_on(struct at_http *client, enum at_transport transport,
                       const char *command, char *result, size_t size)
{
    if (transport != AT_TRANSPORT_WEB) return -1;
    return at_http_command(client, command, result, size);
}
