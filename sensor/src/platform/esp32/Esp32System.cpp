#include "platform/esp32/Esp32System.h"

#include <esp_mac.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "logger.h"
#include "version.h"

static const char*    kNamespace     = "tlmsys";
static const char*    kBootKey       = "boot";
static const uint32_t kEarliestValid = 1577836800;  // 2020-01-01: anything earlier is "unset"

bool Esp32System::begin() {
    esp_efuse_mac_get_default(mac_);

    if (!prefs_.begin(kNamespace, /*readOnly=*/false)) {
        logln("-----> ERROR: NVS system namespace open failed");
        return false;
    }
    boot_id_ = prefs_.getUShort(kBootKey, 0);
    if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
        boot_id_++;
        prefs_.putUShort(kBootKey, boot_id_);
    }

    char id[13];
    deviceIdHex(id);
    logln("-----> Device " + String(id) + ", boot " + String(boot_id_) +
          (unixTime() ? ", clock set" : ", clock unset"));
    return true;
}

void Esp32System::deviceId(uint8_t out[6]) {
    memcpy(out, mac_, 6);
}

uint32_t Esp32System::uptimeSeconds() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000LL);
}

uint32_t Esp32System::unixTime() {
    time_t t = time(nullptr);
    return t > static_cast<time_t>(kEarliestValid) ? static_cast<uint32_t>(t) : 0;
}

bool Esp32System::setUnixTime(uint32_t unix_time) {
    if (unix_time < kEarliestValid) return false;
    struct timeval tv;
    tv.tv_sec  = static_cast<time_t>(unix_time);
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) return false;
    logln("-----> Clock set to " + String(unix_time) + " by peer");
    return true;
}

const char* Esp32System::firmwareVersion() {
    return FIRMWARE_VERSION;
}

void Esp32System::bleName(char out[11]) {
    snprintf(out, 11, "TLM-%02X%02X%02X", mac_[3], mac_[4], mac_[5]);
}

void Esp32System::deviceIdHex(char out[13]) {
    snprintf(out, 13, "%02x%02x%02x%02x%02x%02x", mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
}
