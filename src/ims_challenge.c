#include "ims_challenge.h"
#include "mbedtls/base64.h"

#include <string.h>

static int match(const char *s, size_t len, const char *literal)
{
    size_t i;
    if (strlen(literal) != len) return 0;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned char)literal[i]) return 0;
    }
    return 1;
}

static int has_auth(const char *s)
{
    const char *p = s, *start;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        start = p;
        while (*p && *p != ',') p++;
        while (p > start && (p[-1] == ' ' || p[-1] == '\t')) p--;
        if (match(start, (size_t)(p - start), "auth")) return 1;
        while (*p && *p != ',') p++;
    }
    return 0;
}

int ims_challenge_parse(const struct sip_slice *header, struct ims_challenge *challenge)
{
    const char *p = header->data, *end = p + header->length, *start;
    char value[512], algorithm[64] = "", qop[128] = "";
    unsigned char decoded[384];
    size_t name_length, n, decoded_length;
    unsigned int fields = 0, bit;
    memset(challenge, 0, sizeof(*challenge));
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (end - p < 7 || !match(p, 6, "digest") || (p[6] != ' ' && p[6] != '\t')) return -1;
    p += 6;
    while (p < end) {
        char *target = NULL;
        size_t target_size = 0;
        while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
        if (p == end) break;
        start = p;
        while (p < end && *p != '=' && *p != ' ' && *p != '\t') p++;
        name_length = (size_t)(p - start);
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p == end || *p++ != '=') return -1;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        n = 0;
        if (p < end && *p == '"') {
            p++;
            while (p < end && *p != '"') {
                char c = *p++;
                if (c == '\\') { if (p == end) return -1; c = *p++; }
                if (c == '\r' || c == '\n' || n + 1 >= sizeof(value)) return -1;
                value[n++] = c;
            }
            if (p == end || *p++ != '"') return -1;
        } else {
            while (p < end && *p != ',' && *p != ' ' && *p != '\t') {
                if (n + 1 >= sizeof(value)) return -1;
                value[n++] = *p++;
            }
        }
        value[n] = 0;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p < end && *p != ',') return -1;
        bit = 0;
        if (match(start, name_length, "realm")) { target = challenge->realm; target_size = sizeof(challenge->realm); bit = 1; }
        else if (match(start, name_length, "nonce")) { target = challenge->nonce; target_size = sizeof(challenge->nonce); bit = 2; }
        else if (match(start, name_length, "algorithm")) { target = algorithm; target_size = sizeof(algorithm); bit = 4; }
        else if (match(start, name_length, "qop")) { target = qop; target_size = sizeof(qop); bit = 8; }
        else if (match(start, name_length, "opaque")) { target = challenge->opaque; target_size = sizeof(challenge->opaque); bit = 16; }
        if (bit) {
            if ((fields & bit) || n >= target_size) return -1;
            memcpy(target, value, n + 1); fields |= bit;
        }
    }
    if (!(fields & 1) || !challenge->realm[0] || !(fields & 2) || !challenge->nonce[0] ||
        !(fields & 4) || !match(algorithm, strlen(algorithm), "akav1-md5")) return -1;
    if ((fields & 8) && !has_auth(qop)) return -1;
    challenge->qop_auth = (fields & 8) != 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_length,
                              (const unsigned char *)challenge->nonce, strlen(challenge->nonce)) ||
        decoded_length < 32) return -1;
    memcpy(challenge->rand, decoded, 16); memcpy(challenge->autn, decoded + 16, 16);
    memset(decoded, 0, sizeof(decoded));
    return 0;
}
