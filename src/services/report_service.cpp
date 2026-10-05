#include "services/report_service.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "services/export_service.h"
#include "services/roster_service.h"
#include "services/wifi_extern.h"

static constexpr const char* HOST = "sistema-presenca-ufmg-default-rtdb.firebaseio.com";
static constexpr const char* SECRET = "zoxmy29AQMUaLFV01ygPVlmjmeWruHX3hXC9bzki";

// faz o put para o firebase com um json do relatorio de freq
bool report_upload(const class_rec_t* cls){
    if(!cls || !wifi_is_connected()) return false;

    int freq[ROSTER_MAX_CLASS_STUDENTS];
    if(!export_compute_freq(cls, freq)) return false;
    int limit = (cls->total_classes > 0) ? (cls->total_classes * 25) / 100 : 0;

    JsonDocument doc;
    doc["class_name"] = cls->name;
    doc["limit"] = limit;

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
