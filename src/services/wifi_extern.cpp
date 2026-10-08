#include "services/wifi_extern.h"
#include <WiFi.h>
#include <Preferences.h>
#include <esp32-hal-log.h>
#include "services/wifi_ap.h"

static const char* TAG = "wifi_extern";
static constexpr const char* WIFI_NAMESPACE = "wifi";
static constexpr const char* WIFI_SSID = "ssid";
static constexpr const char* WIFI_PASS= "pass";

static void save_credentials(const char* name, const char* password);

void wifi_connect(const char* name, const char* password){
    if (!name || !name[0]) return;
    save_credentials(name, password);

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

// save the wifi info so the next time it will connect automatically

static void save_credentials(const char* name, const char* password) {
    Preferences prefs;
    if (!prefs.begin(WIFI_NAMESPACE, false)) {
        ESP_LOGE(TAG, "could not open WiFi preferences");
        return;
    }
    prefs.putString(WIFI_SSID, name ? name : "");
    prefs.putString(WIFI_PASS, password ? password : "");
    prefs.end();
}

void wifi_restore_saved(void) {
    Preferences prefs;
    if (!prefs.begin(WIFI_NAMESPACE, true)) {
        ESP_LOGE(TAG, "could not open WiFi preferences");
        return;
    }

    String ssid = prefs.getString(WIFI_SSID, "");
    String password = prefs.getString(WIFI_PASS, "");
    prefs.end();

    if (ssid.length() == 0) {
        ESP_LOGI(TAG, "no saved WiFi network");
        return;
    }
    wifi_connect(ssid.c_str(), password.c_str());
}


