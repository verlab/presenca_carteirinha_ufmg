#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void wifi_connect(const char* name, const char* password);
void wifi_restore(void);
bool wifi_get_saved_credentials(char* name, size_t name_cap, char* password, size_t password_cap);
void wifi_disconnect(void);
bool wifi_is_connected(void);
void wifi_ip(char* out,  size_t cap);
