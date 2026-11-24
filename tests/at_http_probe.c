#include "at_http.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct at_http client = {"127.0.0.1", NULL, {0}, 0, 0, 0, 0};
    char password[256], result[1024];
    if (argc > 1) client.address = argv[1];
    if (argc > 2) client.interface = argv[2];
    if (!fgets(password, sizeof(password), stdin)) return 1;
    password[strcspn(password, "\r\n")] = 0;
    if (at_http_login(&client, password)) return 2;
    memset(password, 0, sizeof(password));
    if (at_http_command(&client, "AT", result, sizeof(result))) return 3;
    if (!strstr(result, "OK")) return 4;
    puts("AT: OK");
    return 0;
}
