#include "operator_gate.h"
#include "modem_info.h"

#include <string.h>

enum operator_gate_result operator_gate(struct at_http *at, char imsi[16])
{
    char response[1024];
    enum operator_gate_result result;
    memset(imsi, 0, 16);
    if (at_http_command(at, "AT+CIMI", response, sizeof(response))) return OPERATOR_GATE_UNAVAILABLE;
    result = modem_supported_imsi(response, imsi) ? OPERATOR_GATE_UNSUPPORTED_OPERATOR : OPERATOR_GATE_OK;
    memset(response, 0, sizeof(response));
    return result;
}
