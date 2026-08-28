#include "storage/device_prefs_store.h"

#include <FS.h>
#include <SD_MMC.h>
#include <string.h>

#include "esp32-hal-log.h"
#include "storage/atomic_file.h"
#include "storage/sd_card.h"

static const char* TAG = "devprefs";
static const char* PATH = "/settings.json";

const char* device_prefs_store_path(void) { return PATH; }

bool device_prefs_store_load(device_prefs_t* out) {
    if (!out) return false;
    device_prefs_defaults(out);
    if (!sd_card_mount()) return false;

    atomic_file_recover(PATH);
    if (!SD_MMC.exists(PATH)) return true;

    File f = SD_MMC.open(PATH, FILE_READ);
    if (!f) {
        ESP_LOGW(TAG, "cannot open %s", PATH);
        return false;
    }
    char buf[192];
    size_t n = f.readBytes(buf, sizeof(buf) - 1);
    f.close();
    buf[n] = '\0';

    if (!device_prefs_parse(buf, n, out)) {
        ESP_LOGW(TAG, "%s unreadable - using defaults", PATH);
        device_prefs_defaults(out);
        return false;
    }
    return true;
}

bool device_prefs_store_save(const device_prefs_t* p) {
    if (!p) return false;
    if (!sd_card_mount()) return false;

    device_prefs_t c = *p;
    device_prefs_clamp(&c);

    char buf[64];
    size_t n = device_prefs_format(&c, buf, sizeof(buf));
    if (n == 0) return false;
    return atomic_file_write(PATH, (const uint8_t*)buf, n, "settings.json");
}
