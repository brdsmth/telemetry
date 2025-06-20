#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

namespace wifi_manager {
    void connectToBestNetwork();
    bool isConnected();
    String getLocalIP();
}

#endif // WIFI_MANAGER_H