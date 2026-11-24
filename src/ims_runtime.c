#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "ims_runtime.h"
#include "ims_identity.h"
#include "ims_pdn.h"
#include "ims_registration.h"
#include "ims_socket.h"
#include "sip_transaction.h"
#include "sms_incoming.h"
#include "sms_store.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define REPORT_QUEUE_SIZE 4
#define LOG(message) ((void)write(STDERR_FILENO, message, sizeof(message) - 1))

static void log_errno(const char *stage)
{
    char line[128];
    int n = snprintf(line, sizeof(line), "%s: errno %d.\n", stage, errno);
    if (n > 0 && (size_t)n < sizeof(line)) (void)write(STDERR_FILENO, line, (size_t)n);
}

static volatile sig_atomic_t stopping;
static int background;

static void update_status(const char *value)
{
    int fd;
    if (!background) return;
    fd = open("/tmp/zxct-ims.status", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) { (void)write(fd, value, strlen(value)); close(fd); }
}

static void stop_handler(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

struct report_slot {
    char packet[4096], call_id[64], transaction_key[1024], sip_response[2048];
    size_t length, sip_response_length;
    struct sip_transaction transaction;
    int active;
};

struct runtime {
    struct at_http *at;
    struct usim usim;
    struct ims_identity identity;
    struct ims_pdn_info pdn;
    struct ims_registration registration;
    struct sip_transaction register_transaction;
    struct report_slot reports[REPORT_QUEUE_SIZE];
    struct sms_incoming_result incoming;
    sqlite3 *db;
    char local_address[INET6_ADDRSTRLEN], register_uri[300];
    char call_id[64], tag[64], branch[96], cnonce[64], contact_user[32];
    char outgoing[4096];
    unsigned char incoming_packet[4096];
    size_t outgoing_length;
    uint32_t serial, register_cseq;
    uint16_t client_port, server_port;
    unsigned int queue_head, queue_count, pcscf_index;
    int udp_fd, esp_fd, register_active, register_protected;
};

static uint64_t monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int random_values(void *out, size_t length)
{
    unsigned char *p = out;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) { log_errno("Random source failed"); return -1; }
    while (length) {
        ssize_t n = read(fd, p, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return -1; }
        p += n; length -= (size_t)n;
    }
    close(fd);
    return 0;
}

static int open_bootstrap(struct runtime *r, unsigned int *port)
{
    struct sockaddr_in6 local, remote;
    socklen_t size = sizeof(local);
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) { log_errno("IMS UDP socket failed"); return -1; }
    memset(&local, 0, sizeof(local)); local.sin6_family = AF_INET6;
    memcpy(&local.sin6_addr, r->pdn.address, 16);
    memset(&remote, 0, sizeof(remote)); remote.sin6_family = AF_INET6;
    remote.sin6_port = htons(5060); memcpy(&remote.sin6_addr, r->pdn.pcscf[r->pcscf_index], 16);
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, r->pdn.interface, strlen(r->pdn.interface) + 1)) {
        log_errno("IMS interface binding failed"); close(fd); return -1;
    }
    if (bind(fd, (struct sockaddr *)&local, sizeof(local))) {
        log_errno("IMS source binding failed"); close(fd); return -1;
    }
    if (connect(fd, (struct sockaddr *)&remote, sizeof(remote))) {
        log_errno("IMS UDP connect failed"); close(fd); return -1;
    }
    if (getsockname(fd, (struct sockaddr *)&local, &size)) {
        log_errno("IMS local endpoint query failed"); close(fd); return -1;
    }
    *port = ntohs(local.sin6_port);
    return fd;
}

static int authenticate(void *context, const unsigned char rand[16],
                        const unsigned char autn[16], struct usim_aka_result *result)
{
    return usim_authenticate(context, rand, autn, result);
}

static int begin_registration(struct runtime *r)
{
    struct ims_register_request req;
    uint32_t random[6];
    unsigned int port;
    if (random_values(random, sizeof(random)) ||
        !inet_ntop(AF_INET6, r->pdn.address, r->local_address, sizeof(r->local_address))) return -1;
    r->udp_fd = open_bootstrap(r, &port);
    if (r->udp_fd < 0) return -1;
    snprintf(r->register_uri, sizeof(r->register_uri), "sip:%s", r->identity.domain);
    if (!r->contact_user[0]) {
        snprintf(r->call_id, sizeof(r->call_id), "%08x%08x", random[0], random[1]);
        snprintf(r->tag, sizeof(r->tag), "%08x", random[2]);
        snprintf(r->contact_user, sizeof(r->contact_user), "%08x", random[1]);
        r->client_port = (uint16_t)(10000 + random[2] % 50000);
        r->server_port = (uint16_t)(10000 + random[3] % 50000);
        if (r->client_port == r->server_port) r->server_port++;
    }
    if (r->register_cseq >= 0x7ffffffeU) return -1;
    snprintf(r->branch, sizeof(r->branch), "z9hG4bK%08x%08x", random[3], random[4]);
    snprintf(r->cnonce, sizeof(r->cnonce), "%08x%08x", random[4], random[5]);
    memset(&req, 0, sizeof(req));
    req.private_identity = r->identity.private_identity; req.public_identity = r->identity.public_identity;
    req.realm = r->identity.domain; req.register_uri = r->register_uri;
    req.contact_user = r->contact_user; req.local_address = r->local_address;
    req.call_id = r->call_id; req.from_tag = r->tag; req.branch = r->branch;
    req.via_port = req.contact_port = port; req.expires = 3600; req.cseq = ++r->register_cseq;
    req.cnonce = r->cnonce; req.nonce_count = 1;
    req.security.spi_client = random[0] < 256 ? random[0] + 256 : random[0];
    req.security.spi_server = random[1] < 256 ? random[1] + 256 : random[1];
    if (req.security.spi_client == req.security.spi_server)
        req.security.spi_server = req.security.spi_client == 0xffffffffU ? req.security.spi_client - 1 : req.security.spi_client + 1;
    req.security.port_client = r->client_port; req.security.port_server = r->server_port;
    if (ims_registration_init(&r->registration, &req, r->pdn.address, r->pdn.pcscf[r->pcscf_index]) ||
        ims_registration_begin(&r->registration, r->outgoing, sizeof(r->outgoing), &r->outgoing_length)) return -1;
    sip_tx_start(&r->register_transaction, monotonic_ms());
    r->register_active = 1; r->register_protected = 0;
    return 0;
}

static int send_register(struct runtime *r)
{
    if (r->register_protected)
        return ims_socket_send(r->esp_fd, &r->registration.transport, IMS_FLOW_CLIENT,
                               r->outgoing, r->outgoing_length);
    return send(r->udp_fd, r->outgoing, r->outgoing_length, 0) == (ssize_t)r->outgoing_length ? 0 : -1;
}

static int report_response(struct runtime *r, const struct sip_message *message)
{
    struct report_slot *slot;
    const struct sip_slice *id, *cseq;
    const char *line = message->start_line.data;
    if (!r->queue_count || message->start_line.length < 12 || memcmp(line, "SIP/2.0 ", 8)) return 0;
    slot = &r->reports[r->queue_head];
    id = sip_header_get(message, "call-id"); cseq = sip_header_get(message, "cseq");
    if (!id || id->length != strlen(slot->call_id) || memcmp(id->data, slot->call_id, id->length) ||
        !cseq || cseq->length != 9 || memcmp(cseq->data, "1 MESSAGE", 9)) return 0;
    if (line[8] == '1') return 1;
    if (line[8] != '2') return -1;
    memset(slot, 0, sizeof(*slot));
    r->queue_head = (r->queue_head + 1) % REPORT_QUEUE_SIZE; r->queue_count--;
    return 1;
}

static int receive_sms(struct runtime *r, const unsigned char *data, size_t length,
                       const struct sip_message *message, enum ims_flow flow)
{
    struct report_slot *slot;
    struct sms_report_request request;
    struct sip_slice verify = {r->registration.security_verify, r->registration.verify.length};
    struct sip_slice route = {r->registration.service_route, strlen(r->registration.service_route)};
    char tag[64], branch[96], transaction_key[1024];
    unsigned int index, i;
    if (sip_message_transaction_key(message, transaction_key, sizeof(transaction_key))) return 0;
    for (i = 0; i < r->queue_count; i++) {
        index = (r->queue_head + i) % REPORT_QUEUE_SIZE;
        slot = &r->reports[index];
        if (!strcmp(transaction_key, slot->transaction_key))
            return ims_socket_send(r->esp_fd, &r->registration.transport, flow,
                                   slot->sip_response, slot->sip_response_length);
    }
    if (r->queue_count == REPORT_QUEUE_SIZE) return 0;
    index = (r->queue_head + r->queue_count) % REPORT_QUEUE_SIZE;
    slot = &r->reports[index];
    r->serial++;
    snprintf(slot->call_id, sizeof(slot->call_id), "%.16s-%u", r->call_id, r->serial);
    snprintf(tag, sizeof(tag), "%.8s-%u", r->tag, r->serial);
    snprintf(branch, sizeof(branch), "%.23s-r%u", r->branch, r->serial);
    memset(&request, 0, sizeof(request));
    request.identity = r->identity.public_identity; request.local_address = r->local_address;
    request.call_id = slot->call_id; request.tag = tag; request.branch = branch;
    request.local_port = r->registration.request.security.port_server; request.cseq = 1;
    request.security_verify = &verify;
    if (route.length) request.service_route = &route;
    if (sms_incoming_process(r->db, data, length, (sqlite3_int64)time(NULL), r->tag, &request, &r->incoming)) {
        LOG("IMS SMS processing failed.\n"); return 0;
    }
    LOG("IMS SMS stored or duplicate handled.\n");
    memcpy(slot->packet, r->incoming.report_message, r->incoming.report_message_length);
    memcpy(slot->transaction_key, transaction_key, strlen(transaction_key) + 1);
    memcpy(slot->sip_response, r->incoming.sip_response, r->incoming.sip_response_length);
    slot->length = r->incoming.report_message_length;
    slot->sip_response_length = r->incoming.sip_response_length;
    slot->active = 0; r->queue_count++;
    if (ims_socket_send(r->esp_fd, &r->registration.transport, flow,
                        r->incoming.sip_response, r->incoming.sip_response_length)) return -1;
    return 0;
}

static int process_message(struct runtime *r, const unsigned char *data, size_t length,
                           int protected_transport, enum ims_flow flow)
{
    struct sip_message message;
    enum ims_reg_event event;
    int report;
    if (sip_parse(data, length, &message)) { LOG("IMS SIP parsing failed.\n"); return 0; }
    if (message.start_line.length >= 8 && !memcmp(message.start_line.data, "MESSAGE ", 8)) {
        LOG("IMS MESSAGE received.\n");
        if (protected_transport) return receive_sms(r, data, length, &message, flow);
        return 0;
    }
    report = protected_transport ? report_response(r, &message) : 0;
    if (report) return report < 0 ? -1 : 0;
    if (!r->register_active) return 0;
    event = ims_registration_response(&r->registration, &message, protected_transport,
                                      authenticate, &r->usim, r->outgoing,
                                      sizeof(r->outgoing), &r->outgoing_length);
    r->register_cseq = r->registration.request.cseq;
    if (event == IMS_REG_ERROR) {
        if (r->registration.failure == IMS_REG_FAILURE_SECURITY)
            LOG("No supported null-encryption IMS security offer.\n");
        else if (r->registration.failure == IMS_REG_FAILURE_AKA)
            LOG("SIM authentication failed.\n");
        else if (r->registration.failure == IMS_REG_FAILURE_CHALLENGE)
            LOG("IMS authentication challenge unsupported.\n");
        else if (r->registration.failure == IMS_REG_FAILURE_RESPONSE) {
            char code[32];
            int n = snprintf(code, sizeof(code), "IMS response: %.3s.\n", message.start_line.data + 8);
            if (n > 0 && (size_t)n < sizeof(code)) (void)write(STDERR_FILENO, code, (size_t)n);
        }
        return -1;
    }
    if (event == IMS_REG_REGISTERED) {
        r->register_active = 0;
        update_status("registered\n");
        if (r->registration.server.integrity == IMS_HMAC_MD5_96)
            LOG("IMS registered: ESP null, HMAC-MD5-96.\n");
        else LOG("IMS registered: ESP null, HMAC-SHA1-96.\n");
        return 0;
    }
    if (event == IMS_REG_SEND_CLEAR || event == IMS_REG_SEND_PROTECTED) {
        r->register_protected = event == IMS_REG_SEND_PROTECTED;
        if (r->register_protected && r->esp_fd < 0) {
            r->esp_fd = ims_socket_open(&r->registration.transport, r->pdn.interface);
            if (r->esp_fd < 0) return -1;
        }
        sip_tx_start(&r->register_transaction, monotonic_ms());
    }
    return 0;
}

static int run_connection(struct runtime *r)
{
    uint64_t renew_at = 0;
    if (begin_registration(r)) return -1;
    for (;;) {
        fd_set readfds;
        struct timeval timeout = {1, 0};
        uint64_t now = monotonic_ms();
        enum sip_tx_action action;
        int count, maxfd = r->udp_fd;
        if (stopping) return 0;
        if (!now) return -1;
        if (r->register_active) {
            action = sip_tx_poll(&r->register_transaction, now);
            if (action == SIP_TX_TIMEOUT) { LOG("IMS registration timed out.\n"); return -1; }
            if (action == SIP_TX_SEND && send_register(r)) { log_errno("IMS register send failed"); return -1; }
        } else if (!renew_at) renew_at = now + (uint64_t)r->registration.expires_seconds * 800;
        if (r->queue_count) {
            struct report_slot *slot = &r->reports[r->queue_head];
            if (!slot->active) { sip_tx_start(&slot->transaction, now); slot->active = 1; }
            action = sip_tx_poll(&slot->transaction, now);
            if (action == SIP_TX_TIMEOUT) return -1;
            if (action == SIP_TX_SEND && ims_socket_send(r->esp_fd, &r->registration.transport,
                IMS_FLOW_CLIENT, slot->packet, slot->length)) return -1;
        } else if (renew_at && now >= renew_at) {
            if (ims_registration_refresh(&r->registration, r->outgoing, sizeof(r->outgoing),
                                         &r->outgoing_length)) return -1;
            r->register_cseq = r->registration.request.cseq;
            r->register_active = 1; r->register_protected = 1; renew_at = 0;
            sip_tx_start(&r->register_transaction, now);
        }
        FD_ZERO(&readfds); FD_SET(r->udp_fd, &readfds);
        if (r->esp_fd >= 0) { FD_SET(r->esp_fd, &readfds); if (r->esp_fd > maxfd) maxfd = r->esp_fd; }
        count = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (count < 0) { if (errno == EINTR) continue; return -1; }
        if (FD_ISSET(r->udp_fd, &readfds)) {
            ssize_t n = recv(r->udp_fd, r->incoming_packet, sizeof(r->incoming_packet), 0);
            if (n < 0 || process_message(r, r->incoming_packet, (size_t)n, 0, IMS_FLOW_CLIENT)) return -1;
        }
        if (r->esp_fd >= 0 && FD_ISSET(r->esp_fd, &readfds)) {
            const unsigned char *data;
            size_t length;
            enum ims_flow flow;
            if (!ims_socket_receive(r->esp_fd, &r->registration.transport, r->incoming_packet,
                                    sizeof(r->incoming_packet), &flow, &data, &length) &&
                process_message(r, data, length, 1, flow)) return -1;
        }
    }
}

int ims_runtime_start(struct at_http *at, const char *imsi, void *context)
{
    struct runtime *r;
    struct sigaction action;
    int identity_status, rc = 4;
    background = 0;
    if (context && *(int *)context) {
        pid_t child;
        int fd, ready[2];
        char result;
        if (pipe(ready)) return rc;
        child = fork();
        if (child < 0) { close(ready[0]); close(ready[1]); return rc; }
        if (child > 0) {
            close(ready[1]);
            identity_status = read(ready[0], &result, 1) == 1 && result == '1';
            close(ready[0]);
            return identity_status ? 0 : rc;
        }
        close(ready[0]);
        if (setsid() < 0) _exit(rc);
        fd = open("/dev/null", O_RDWR);
        if (fd < 0 || dup2(fd, STDIN_FILENO) < 0 || dup2(fd, STDOUT_FILENO) < 0 ||
            dup2(fd, STDERR_FILENO) < 0) _exit(rc);
        if (fd > STDERR_FILENO) close(fd);
        result = '1';
        if (write(ready[1], &result, 1) != 1) _exit(rc);
        close(ready[1]);
        background = 1;
    }
    r = calloc(1, sizeof(*r));
    if (!r) return rc;
    update_status("starting\n");
    memset(&action, 0, sizeof(action));
    action.sa_handler = stop_handler;
    sigemptyset(&action.sa_mask);
    stopping = 0;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    r->at = at; r->udp_fd = r->esp_fd = -1;
    if (usim_open(&r->usim, at)) { LOG("Cannot open the SIM application.\n"); goto out; }
    identity_status = ims_identity_read(&r->usim, &r->identity);
    if (identity_status < 0 || (identity_status == 1 && ims_identity_from_imsi(imsi, &r->identity))) {
        LOG("Cannot read IMS identity.\n"); goto out;
    }
    if (sms_store_open(&r->db, "/etc_rw/sms.db") != SQLITE_OK) {
        LOG("Cannot open the device SMS database.\n"); goto out;
    }
    while (!stopping) {
        int failed = 1;
        FILE *interfaces = NULL;
        int pdn_ready = !ims_pdn_acquire(at, &r->pdn);
        if (at->failed) { LOG("AT interfaces unavailable.\n"); goto out; }
        if (pdn_ready) {
            interfaces = fopen("/proc/net/if_inet6", "r");
            pdn_ready = interfaces && !ims_pdn_local_interface(&r->pdn, interfaces);
            if (interfaces) fclose(interfaces);
            if (!pdn_ready && !ims_pdn_prepare_interface(&r->pdn)) {
                interfaces = fopen("/proc/net/if_inet6", "r");
                pdn_ready = interfaces && !ims_pdn_local_interface(&r->pdn, interfaces);
                if (interfaces) fclose(interfaces);
            }
        }
        if (pdn_ready) {
            if (r->pcscf_index >= r->pdn.pcscf_count) r->pcscf_index = 0;
            failed = run_connection(r) != 0;
            if (at->failed) { LOG("AT interfaces unavailable.\n"); goto out; }
            if (failed) {
                update_status("waiting\n");
                r->pcscf_index = (r->pcscf_index + 1) % r->pdn.pcscf_count;
                LOG("IMS connection interrupted; retrying.\n");
            }
        } else { update_status("waiting\n"); LOG("IMS data connection unavailable; retrying.\n"); }
        if (r->udp_fd >= 0) { close(r->udp_fd); r->udp_fd = -1; }
        if (r->esp_fd >= 0) { close(r->esp_fd); r->esp_fd = -1; }
        memset(r->reports, 0, sizeof(r->reports)); r->queue_head = r->queue_count = 0;
        if (failed && !stopping) sleep(15);
    }
    rc = 0;
out:
    ims_pdn_cleanup();
    if (r->udp_fd >= 0) close(r->udp_fd);
    if (r->esp_fd >= 0) close(r->esp_fd);
    if (r->db) sqlite3_close(r->db);
    if (r->usim.channel) usim_close(&r->usim);
    if (background) unlink("/tmp/zxct-ims.status");
    memset(r, 0, sizeof(*r)); free(r);
    return rc;
}
