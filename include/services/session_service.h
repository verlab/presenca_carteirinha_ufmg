#pragma once

#include <stdbool.h>

// Initializes the active session state and underlying hardware tracking modules.
// Returns true on successful initialization, or false if allocation/bus setup fails.
bool session_service_init(void);