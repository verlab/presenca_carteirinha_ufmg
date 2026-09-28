#pragma once
#include <stdint.h>
#include <string.h>

void wifi_connect(const char* name, const char* password);
void wifi_restore_saved(void);
void wifi_disconnect(void);
bool wifi_is_connected(void);
void wifi_ip(char* out,  size_t cap);
