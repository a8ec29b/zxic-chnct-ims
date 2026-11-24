#include "ims_identity.h"

#include <stdio.h>
#include <string.h>

struct tlv { unsigned char tag; const unsigned char *value; size_t length, used; };
struct file_info { unsigned int size, record_size, records; };

static int tlv_read(const unsigned char *data, size_t length, struct tlv *out)
{
    size_t n, header = 2;
    if (length < 2) return -1;
    out->tag = data[0]; n = data[1];
    if (n == 0x81) {
        if (length < 3) return -1;
        n = data[2]; header++;
    } else if (n >= 0x80) return -1;
    if (n > length - header) return -1;
    out->value = data + header; out->length = n; out->used = header + n;
    return 0;
}

static int exchange(struct usim *sim, const unsigned char *cmd, size_t cmdlen,
                    unsigned char *out, size_t *length)
{
    unsigned char get[] = {0, 0xc0, 0, 0, 0};
    size_t capacity = *length;
    if (usim_transmit(sim, cmd, cmdlen, out, length) || *length < 2) return -1;
    if (out[*length - 2] == 0x61 || out[*length - 2] == 0x9f) {
        get[4] = out[*length - 1]; *length = capacity;
        if (usim_transmit(sim, get, sizeof(get), out, length) || *length < 2) return -1;
    }
    if (out[*length - 2] == 0x6a && out[*length - 1] == 0x82) return 1;
    if (out[*length - 2] != 0x90 || out[*length - 1] != 0) return -1;
    *length -= 2;
    return 0;
}

static int select_file(struct usim *sim, unsigned int id, struct file_info *info)
{
    unsigned char cmd[] = {0, 0xa4, 0, 4, 2, 0, 0}, data[256];
    size_t len = sizeof(data), pos = 0;
    struct tlv outer, field;
    int rc;
    cmd[5] = (unsigned char)(id >> 8); cmd[6] = (unsigned char)id;
    rc = exchange(sim, cmd, sizeof(cmd), data, &len);
    if (rc) return rc;
    if (tlv_read(data, len, &outer) || outer.tag != 0x62) return -1;
    memset(info, 0, sizeof(*info));
    while (pos < outer.length) {
        if (tlv_read(outer.value + pos, outer.length - pos, &field)) return -1;
        if (field.tag == 0x80 && field.length == 2)
            info->size = ((unsigned int)field.value[0] << 8) | field.value[1];
        if (field.tag == 0x82 && field.length >= 5) {
            info->record_size = ((unsigned int)field.value[2] << 8) | field.value[3];
            info->records = field.value[4];
        }
        pos += field.used;
    }
    return 0;
}

static int select_aid(struct usim *sim, const unsigned char *aid, size_t length)
{
    unsigned char cmd[21] = {0, 0xa4, 4, 4}, result[256];
    size_t n = sizeof(result);
    if (!length || length > 16) return -1;
    cmd[4] = (unsigned char)length; memcpy(cmd + 5, aid, length);
    return exchange(sim, cmd, length + 5, result, &n);
}

static int read_file(struct usim *sim, unsigned int id, int record,
                     unsigned int number, unsigned char *data, size_t *length)
{
    struct file_info info;
    unsigned char cmd[] = {0, 0xb0, 0, 0, 0};
    unsigned int size;
    int rc = select_file(sim, id, &info);
    if (rc) return rc;
    size = record ? info.record_size : info.size;
    if (!size || size > 255 || (record && (!number || number > info.records))) return -1;
    if (record) { cmd[1] = 0xb2; cmd[2] = (unsigned char)number; cmd[3] = 4; }
    cmd[4] = (unsigned char)size;
    return exchange(sim, cmd, sizeof(cmd), data, length);
}

static int text_value(const unsigned char *data, size_t length, char out[256])
{
    struct tlv value;
    if (tlv_read(data, length, &value) || value.tag != 0x80 ||
        !value.length || value.length >= 256 || memchr(value.value, 0, value.length)) return -1;
    memcpy(out, value.value, value.length); out[value.length] = 0;
    return 0;
}

int ims_identity_read(struct usim *sim, struct ims_identity *identity)
{
    static const unsigned char usim_aid[] = {0xa0,0,0,0,0x87,0x10,0x02};
    static const unsigned char isim_prefix[] = {0xa0,0,0,0,0x87,0x10,0x04};
    struct file_info info;
    struct tlv entry, field;
    unsigned char data[256], cmd[] = {0,0xb2,0,4,0};
    size_t len, pos;
    unsigned int index;
    int rc = -1, domain_status;
    memset(identity, 0, sizeof(*identity));
    if (select_file(sim, 0x3f00, &info) || select_file(sim, 0x2f00, &info) ||
        !info.record_size || info.record_size > 255) goto out;
    for (index = 1; index <= info.records; index++) {
        cmd[2] = (unsigned char)index; cmd[4] = (unsigned char)info.record_size;
        len = sizeof(data);
        if (exchange(sim, cmd, sizeof(cmd), data, &len)) goto out;
        if (!len || data[0] == 0xff) continue;
        if (tlv_read(data, len, &entry) || entry.tag != 0x61) goto out;
        pos = 0;
        while (pos < entry.length && entry.value[pos] != 0xff) {
            if (tlv_read(entry.value + pos, entry.length - pos, &field)) goto out;
            if (field.tag == 0x4f && field.length >= sizeof(isim_prefix) && field.length <= 16 &&
                !memcmp(field.value, isim_prefix, sizeof(isim_prefix))) {
                memcpy(identity->isim_aid, field.value, field.length);
                identity->isim_aid_length = field.length;
                break;
            }
            pos += field.used;
        }
        if (identity->isim_aid_length) break;
    }
    if (!identity->isim_aid_length) { rc = 1; goto out; }
    if (select_aid(sim, identity->isim_aid, identity->isim_aid_length)) goto out;
    len = sizeof(data);
    if (read_file(sim, 0x6f02, 0, 0, data, &len) || text_value(data, len, identity->private_identity)) goto out;
    len = sizeof(data);
    domain_status = read_file(sim, 0x6f03, 0, 0, data, &len);
    if (domain_status == 1) {
        const char *domain = strchr(identity->private_identity, '@');
        if (!domain || !domain[1]) goto out;
        snprintf(identity->domain, sizeof(identity->domain), "%s", domain + 1);
    } else if (domain_status || text_value(data, len, identity->domain)) goto out;
    if (select_file(sim, 0x6f04, &info) || !info.records) goto out;
    for (index = 1; index <= info.records; index++) {
        len = sizeof(data);
        if (read_file(sim, 0x6f04, 1, index, data, &len)) goto out;
        if (len && data[0] != 0xff && !text_value(data, len, identity->public_identity)) break;
    }
    if (!identity->public_identity[0]) goto out;
    rc = 0;
out:
    if (select_aid(sim, usim_aid, sizeof(usim_aid))) rc = -1;
    if (rc < 0) memset(identity, 0, sizeof(*identity));
    return rc;
}

int ims_identity_from_imsi(const char *imsi, struct ims_identity *identity)
{
    if (strlen(imsi) != 15 || strspn(imsi, "0123456789") != 15 ||
        (memcmp(imsi, "46003", 5) && memcmp(imsi, "46005", 5) &&
         memcmp(imsi, "46011", 5) && memcmp(imsi, "46012", 5))) return -1;
    memset(identity, 0, sizeof(*identity));
    snprintf(identity->domain, sizeof(identity->domain), "ims.mnc0%c%c.mcc460.3gppnetwork.org", imsi[3], imsi[4]);
    snprintf(identity->private_identity, sizeof(identity->private_identity), "%.15s@%.64s", imsi, identity->domain);
    snprintf(identity->public_identity, sizeof(identity->public_identity), "sip:%.15s@%.64s", imsi, identity->domain);
    return 0;
}
