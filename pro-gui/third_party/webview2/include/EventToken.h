// Minimal EventToken.h for MinGW (Windows SDK normally provides this).
// Required by WebView2.h for EventRegistrationToken.
#pragma once

#include <stdint.h>

typedef struct EventRegistrationToken {
    int64_t value;
} EventRegistrationToken;
