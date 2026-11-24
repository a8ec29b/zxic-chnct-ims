#ifndef AT_HTTP_H
#define AT_HTTP_H

#include <stddef.h>

struct at_http {
    const char *address;
    const char *interface;
    char cookie[256];
    int ipc_ready;
    int http_ready;
    int last_ipc;
    int failed;
};

/* The password is only used to log in; the caller owns its storage. */
int at_http_login(struct at_http *client, const char *password);

/* Try the device AT service first, then the authenticated web interface. */
int at_http_command(struct at_http *client, const char *command,
                    char *result, size_t result_size);

/* Do not replay a command after an ambiguous transport failure. */
int at_http_command_once(struct at_http *client, const char *command,
                         char *result, size_t result_size);

int at_http_command_web(struct at_http *client, const char *command,
                        char *result, size_t result_size);
int at_http_command_alternate(struct at_http *client, const char *command,
                              char *result, size_t result_size);

#endif
