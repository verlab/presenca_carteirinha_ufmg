#include "services/wifi_extern.h"
#include <WiFi.h>
#include <Preferences.h>
#include <esp32-hal-log.h>
#include "services/wifi_ap.h"

static const char* TAG = "wifi_extern";
static constexpr const char* WIFI_NAMESPACE = "wifi";
static constexpr const char* WIFI_SSID = "ssid";
static constexpr const char* WIFI_PASS= "pass";
static constexpr uint32_t WIFI_RETRY_PERIOD_MS = 10000;
static TaskHandle_t s_retry_task = nullptr;

// salvar infos do wifi para o prox uso
static bool save_credentials(const char* name, const char* password) {
    const char* ssid = name ? name : "";
    const char* pass = password ? password : "";
    Preferences prefs;
    prefs.begin(WIFI_NAMESPACE, false);
    
    const size_t ssid_written = prefs.putString(WIFI_SSID, ssid);
    const size_t pass_written = prefs.putString(WIFI_PASS, pass);
    const bool saved = ssid_written == strlen(ssid) && pass_written == strlen(pass);
    prefs.end();

    if (!saved) ESP_LOGE(TAG, "could not verify saved WiFi credentials for %s", ssid);
    return saved;
}

bool wifi_get_saved_credentials(char* name, size_t name_cap, char* password,
                                size_t password_cap) {
    if (name && name_cap) name[0] = '\0';
    if (password && password_cap) password[0] = '\0';

    Preferences prefs;
    if (!prefs.begin(WIFI_NAMESPACE, true)) {
        ESP_LOGE(TAG, "could not open WiFi preferences for reading");
        return false;
    }
    String ssid = prefs.getString(WIFI_SSID, "");
    String pass = prefs.getString(WIFI_PASS, "");
    prefs.end();

    if (name && name_cap) snprintf(name, name_cap, "%s", ssid.c_str());
    if (password && password_cap) snprintf(password, password_cap, "%s", pass.c_str());
    return ssid.length() > 0;
}

void wifi_restore(void) {
    char ssid[33];
    char password[64];
    if (!wifi_get_saved_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGI(TAG, "no saved WiFi network");
        return;
    }
    ESP_LOGI(TAG, "restoring saved WiFi network: %s", ssid);
    wifi_connect(ssid, password);
}

void wifi_connect(const char* name, const char* password){
    if (!name || !name[0]) return;
    if (!save_credentials(name, password)) {
        ESP_LOGE(TAG, "Past WiFi were not saved; connection will be temporary");
    }

    if(wifi_ap_is_running()){
        wifi_ap_stop();
    }

    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(name, password);
    ESP_LOGI(TAG, "connecting to %s", name);
}

void wifi_disconnect(void){
    WiFi.disconnect(false);
    ESP_LOGI(TAG, "disconnected");
}

bool wifi_is_connected(void){
    return WiFi.status() == WL_CONNECTED;
}

void wifi_ip(char* out,  size_t cap){
    if(!cap) return;
    if(!wifi_is_connected()){
        out[0] = '\0';
        return;
    }
    snprintf(out, cap, "%s", WiFi.localIP().toString().c_str());
}


