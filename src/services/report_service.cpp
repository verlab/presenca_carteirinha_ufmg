#include "services/report_service.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "services/export_service.h"
#include "services/roster_service.h"
#include "services/wifi_extern.h"
#include "services/rtc_service.h"
#include <time.h>

// firebase realtime database host and secret (from the firebase console)
static constexpr const char* HOST = "sistema-presenca-ufmg-default-rtdb.firebaseio.com";
static constexpr const char* SECRET = "zoxmy29AQMUaLFV01ygPVlmjmeWruHX3hXC9bzki";

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
bool report_upload(const class_rec_t* cls){
    if(!cls || !wifi_is_connected()) return false;

    int freq[ROSTER_MAX_CLASS_STUDENTS];
    if(!export_compute_freq(cls, freq)) return false;
    int limit = (cls->total_classes > 0) ? (cls->total_classes * 25) / 100 : 0;

    JsonDocument doc;
    doc["class_name"] = cls->name;
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

    String url = "https://" + String(HOST) + "/reports/" + String(cls->code) +".json?auth=" + String(SECRET);
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int code = http.PUT(body);
    http.end();
    return code == 200;
}
