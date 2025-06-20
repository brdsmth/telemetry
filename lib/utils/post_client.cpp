#include "post_client.h"
#include <WiFi.h>
#include <HTTPClient.h>

namespace post_client {

    void sendJsonPost(const String& url, const String& jsonPayload) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[POST] ❌ WiFi not connected.");
            return;
        }

        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        int responseCode = http.POST(jsonPayload);

        if (responseCode > 0) {
            String response = http.getString();
            Serial.printf("[POST] ✅ Sent. Code: %d\nResponse: %s\n", responseCode, response.c_str());
        } else {
            Serial.printf("[POST] ❌ Failed. Code: %d (%s)\n",
                          responseCode, http.errorToString(responseCode).c_str());
        }

        http.end();
    }

}
