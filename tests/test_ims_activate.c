#include "ims_activate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct packet { long type; unsigned char data[256]; };

static void send_response(int queue, int message_id, const char *text)
{
    struct packet packet = {0};
    size_t n = strlen(text);
    packet.type = 1;
    packet.data[8] = (unsigned char)message_id;
    packet.data[9] = (unsigned char)(message_id >> 8);
    memcpy(packet.data + 16, text, n);
    assert(msgsnd(queue, &packet, 16 + n, 0) == 0);
}

int main(void)
{
    int queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    assert(queue >= 0);
    assert(ims_activate_poll(queue) == 0);
    send_response(queue, 5422, "\r\nOK\r\n");
    assert(ims_activate_poll(queue) == 0);
    send_response(queue, 5423, "\r\n+ZPDPACT: 2\r\n");
    assert(ims_activate_poll(queue) == 0);
    send_response(queue, 5423, "\r\nOK\r\n");
    assert(ims_activate_poll(queue) == 1);
    send_response(queue, 5423, "\r\n+CME ERROR: 3\r\n");
    assert(ims_activate_poll(queue) == -1);
    send_response(queue, 5531, "123456789012");
    assert(ims_activate_poll(queue) == 2);
    ims_activate_end(queue);
    puts("ims_activate: ok");
    return 0;
}
