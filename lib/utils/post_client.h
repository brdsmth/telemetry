#ifndef POST_CLIENT_H
#define POST_CLIENT_H

#include <Arduino.h>

namespace post_client {
    void sendJsonPost(const String& url, const String& jsonPayload);
}

#endif // POST_CLIENT_H