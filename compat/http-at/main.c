#include "ims_app.h"
#include "ims_runtime.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct at_http at = {"127.0.0.1", NULL, {0}};
    char password[256];
    size_t length = 0;
    char ch;
    ssize_t n;
    int result, daemon = 0;
    if (argc == 2 && !strcmp(argv[1], "--daemon")) daemon = 1;
    else if (argc != 1) return 1;
    while (length + 1 < sizeof(password)) {
        n = read(STDIN_FILENO, &ch, 1);
        if (n < 0 && errno == EINTR) continue;
        if (n != 1) return 1;
        if (ch == '\n' || ch == '\r') break;
        password[length++] = ch;
    }
    if (length + 1 == sizeof(password)) return 1;
    password[length] = 0;
    result = ims_app_run(&at, password, ims_runtime_start, &daemon);
    memset(password, 0, sizeof(password));
    if (result == IMS_APP_NOT_TELECOM)
        (void)write(STDERR_FILENO, "SIM is not China Telecom; exiting.\n", 35);
    else if (result == IMS_APP_SIM_UNAVAILABLE)
        (void)write(STDERR_FILENO, "SIM identity unavailable; exiting.\n", 35);
    else if (result == IMS_APP_LOGIN_FAILED)
        (void)write(STDERR_FILENO, "Local modem login failed.\n", 26);
    return result;
}
