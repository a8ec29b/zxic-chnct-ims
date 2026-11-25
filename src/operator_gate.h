#ifndef OPERATOR_GATE_H
#define OPERATOR_GATE_H

#include "at_http.h"

enum operator_gate_result { OPERATOR_GATE_OK, OPERATOR_GATE_UNAVAILABLE, OPERATOR_GATE_UNSUPPORTED_OPERATOR };

/* Call immediately after HTTP login, before opening the SMS DB or IMS interfaces. */
enum operator_gate_result operator_gate(struct at_http *at, char imsi[16]);

#endif
