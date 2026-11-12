#include "services/report_service.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "services/export_service.h"
#include "services/roster_service.h"
#include "services/wifi_extern.h"
#include "services/rtc_service.h"
#include <time.h>
#include <esp_random.h>

// firebase realtime database host and secret (from the firebase console)
static constexpr const char* HOST = "sistema-presenca-ufmg-default-rtdb.firebaseio.com";
static constexpr const char* SECRET = "zoxmy29AQMUaLFV01ygPVlmjmeWruHX3hXC9bzki";
static constexpr const char* KEY = "AIzaSyDprMb7gtMLeLUWlpxF5vH7TWTpclE4kIo";
static constexpr const char* EMAIL = "sistema.presenca@ufmg.br";
static constexpr const char* PASSWORD = "12345678";

// last token uplodaded per class, deleted when a new one is made 
static char prev_token[ROSTER_MAX_CLASSES][33];

// 128 bit random token 
static void make_token(char* token, size_t size) {
    uint32_t a[16];
    esp_fill_random(a, sizeof(a));
    for (int i = 0; i < 16; i++) {
        snprintf(token + i * 2, 3, "%02x", a[i]);
    }
}

// signs in the device account and return token 
static bool firebase_token(String& token) {
    HTTPClient http;
    if (!http.begin("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" +
                    String(KEY))) {
        return false;
    }
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["email"] = EMAIL;
    doc["password"] = PASSWORD;
    doc["returnSecureToken"] = true;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String response_body = http.getString();
    if (code != 200) {
        http.end();
        return false;
    }

    JsonDocument response;
    DeserializationError error = deserializeJson(response, response_body);
    http.end();
    if (error) return false;

    token = response["idToken"].as<String>();
    return token.length() > 0;
}

static void delete_report(const char* token, const String& auth){
    HTTPClient http;
    http.begin("https://" + String(HOST) + "/reports/" + String(token) + ".json?auth=" + auth);
    http.sendRequest("DELETE");
    http.end();
}

static bool ntp_timestamp(uint32_t* timestamp) {
    if (!timestamp || !wifi_is_connected()) return false;

   // utc time
    configTime(0, 0, "pool.ntp.org", "time.google.com");

    constexpr time_t MIN_VALID_UNIX = 1704067200; // 2024-01-01
    const uint32_t started = millis();
    time_t now = time(nullptr);

    // wait up to 15 seconds for a valid time 
    while (now < MIN_VALID_UNIX &&
           static_cast<uint32_t>(millis() - started) < 15000) {
        delay(250);
        now = time(nullptr);
    }

    if (now < MIN_VALID_UNIX) return false;

    *timestamp = static_cast<uint32_t>(now);
    return true;
}

// uploads the class report (name, id, FREQ per student, 25% absence limit and
// an expiry time) to /reports/<code>.json, replacing the previous one
bool report_upload(const class_rec_t* cls, char* out_token, size_t cap){
    if (!cls || !out_token || cap == 0 || !wifi_is_connected()) return false;

    String auth;
    if (!firebase_token(auth)) return false;

    char token[33];
    make_token(token, sizeof(token));

    int freq[ROSTER_MAX_CLASS_STUDENTS];
    if (!export_compute_freq(cls, freq)) return false;
    int limit = (cls->total_classes > 0) ? (cls->total_classes * 25) / 100 : 0;

    JsonDocument doc;
    doc["class_name"] = cls->name;
    doc["class_code"] = cls->code;
    doc["limit"] = limit;

    uint32_t now = rtc_timestamp();

    if (now == 0 && !ntp_timestamp(&now)) {
        return false;
    }

    doc["expires_at"] = ((uint64_t)now + 1ULL * 60ULL) * 1000ULL;

    JsonArray array = doc["students"].to<JsonArray>();
    for(int i = 0; i < cls-> roster_count; i++){
        const student_t* student = roster_student_at(cls->roster[i]);
        if(!student) continue;
        JsonObject object = array.add<JsonObject>();
        object["name"] = student->name;
        object["id"] = student->id;
        object["freq"] = freq[i];
    }

    String body;
    serializeJson(doc, body);

    String url = "https://" + String(HOST) + "/reports/" + String(token) + ".json?auth=" + auth;
    
    HTTPClient http;
    if (!http.begin(url)) {
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    
    int code = http.PUT(body);
    if (code != 200) {
        http.end();
        return false;
    }
    http.end();

    int class_index = roster_class_index(cls->code);
    if (class_index >= 0) {
        if(prev_token[class_index][0]) {
            if (prev_token[class_index][0]) delete_report(prev_token[class_index], auth);
            snprintf(prev_token[class_index], sizeof(prev_token[class_index]), "%s", token);
        }
    }
    snprintf(out_token, cap, "%s", token);
    return true;
}
