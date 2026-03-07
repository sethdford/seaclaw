#ifndef SC_INPUT_GUARD_H
#define SC_INPUT_GUARD_H
#include "seaclaw/core/error.h"
#include <stddef.h>
typedef enum sc_injection_risk {
    SC_INJECTION_SAFE = 0,
    SC_INJECTION_SUSPICIOUS = 1,
    SC_INJECTION_HIGH_RISK = 2,
} sc_injection_risk_t;
sc_error_t sc_input_guard_check(const char *message, size_t message_len,
                                sc_injection_risk_t *out_risk);
#endif
