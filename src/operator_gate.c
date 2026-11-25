#include "operator_gate.h"
#include "modem_info.h"

#include <string.h>

enum operator_gate_result operator_gate(struct at_http *at, char imsi[16])
{
    char response[1024];
    int primary, alternate, other = 0;
    memset(imsi, 0, 16);
    primary = at_http_command(at, "AT+CIMI", response, sizeof(response));
    if (!primary && !modem_supported_imsi(response, imsi)) {
        memset(response, 0, sizeof(response));
        return OPERATOR_GATE_OK;
    }
    if (!primary && !modem_read_imsi(response, imsi)) other = 1;
    memset(imsi, 0, 16);
    alternate = at_http_command_alternate(at, "AT+CIMI", response, sizeof(response));
    if (!alternate && !modem_supported_imsi(response, imsi)) {
        memset(response, 0, sizeof(response));
        return OPERATOR_GATE_OK;
    }
    if (!alternate && !modem_read_imsi(response, imsi)) other = 1;
    memset(imsi, 0, 16);
    memset(response, 0, sizeof(response));
    return other ? OPERATOR_GATE_UNSUPPORTED_OPERATOR : OPERATOR_GATE_UNAVAILABLE;
}
