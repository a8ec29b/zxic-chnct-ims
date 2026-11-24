#ifndef AT_HTTP_H
#define AT_HTTP_H

#include <stddef.h>

struct at_http {
    const char *address;
    const char *interface;
    char cookie[256];
};

/* The password is only used to log in; the caller owns its storage. */
int at_http_login(struct at_http *client, const char *password);

/* One request per command; a failed or ambiguous request is not replayed. */
int at_http_command(struct at_http *client, const char *command,
                    char *result, size_t result_size);
int at_http_command_once(struct at_http *client, const char *command,
                         char *result, size_t result_size);

#endif
