#include <Arduino.h>

static const int RXPin = 17, TXPin = 18;
static const uint32_t GPSBaud = 115200;

String rev;

void SentSerial(const char *p_char) {
  for (int i = 0; i < strlen(p_char); i++) {
    Serial1.write(p_char[i]);
    delay(10);
  }
  Serial1.write('\r');
  delay(10);
  Serial1.write('\n');
  delay(10);
}

bool SentMessage(const char *p_char, unsigned long timeout = 2000) {
  SentSerial(p_char);

  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (Serial1.available()) {
      rev = Serial1.readString();
      if (rev.indexOf("OK") != -1) {
        Serial.println("Got OK!");
        return true;
      }
    }
  }
  Serial.println("Timeout!");
  return false;
}

// Function to make HTTP GET request
bool makeHTTPGetRequest(const char* url, const char* data = nullptr) {
  Serial.println("Making HTTP GET request to: " + String(url));
  
  // Initialize HTTP service
  if (!SentMessage("AT+HTTPINIT", 3000)) {
    Serial.println("Failed to initialize HTTP service");
    return false;
  }
  
  // Set HTTP parameters
  String urlParam = "AT+HTTPPARA=\"URL\",\"" + String(url) + "\"";
  if (!SentMessage(urlParam.c_str(), 3000)) {
    Serial.println("Failed to set URL parameter");
    return false;
  }
  
  // Set content type if sending data
  if (data != nullptr) {
    if (!SentMessage("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 2000)) {
      Serial.println("Failed to set content type");
      return false;
    }
    
    // Set data length
    String dataLenParam = "AT+HTTPDATA=" + String(strlen(data)) + ",10000";
    Serial.println("Setting data length: " + dataLenParam);
    SentSerial(dataLenParam.c_str());
    delay(1000);
    
    // Check for "DOWNLOAD" prompt
    if (Serial1.available()) {
      rev = Serial1.readString();
      Serial.println("HTTPDATA Response: " + rev);
      if (rev.indexOf("DOWNLOAD") == -1) {
        Serial.println("Did not receive DOWNLOAD prompt");
        return false;
      }
    }
    
    // Send data
    Serial.println("Sending data: " + String(data));
    SentSerial(data);
    delay(2000); // Give more time for data transmission
    
    // Check for OK after data transmission
    if (Serial1.available()) {
      rev = Serial1.readString();
      Serial.println("Data transmission response: " + rev);
      if (rev.indexOf("OK") == -1) {
        Serial.println("Data transmission failed");
        return false;
      }
    }
  }
  
  // Execute HTTP GET action
  if (!SentMessage("AT+HTTPACTION=0", 10000)) { // GET action, 10 second timeout
    Serial.println("Failed to execute HTTP GET");
    return false;
  }
  
  // Read HTTP response
  delay(2000); // Wait for response
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("HTTP Response: " + rev);
  }
  
  // Read HTTP data
  Serial.println("Reading HTTP data...");
  SentSerial("AT+HTTPREAD");
  delay(1000);
  
  // Check for response format: +HTTPREAD: <length>,<timeout>
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("HTTPREAD Response: " + rev);
    
    // If we get a response with length, wait for the actual data
    if (rev.indexOf("+HTTPREAD:") != -1) {
      delay(2000); // Wait for the actual data
      if (Serial1.available()) {
        rev = Serial1.readString();
        Serial.println("HTTP Data: " + rev);
      }
    }
  }
  
  // Terminate HTTP service
  SentMessage("AT+HTTPTERM", 2000);
  
  return true;
}

// Function to make HTTP POST request
bool makeHTTPPostRequest(const char* url, const char* data) {
  Serial.println("Making HTTP POST request to: " + String(url));
  
  // Initialize HTTP service
  Serial.println("Initializing HTTP service...");
  SentSerial("AT+HTTPINIT");
  delay(1000);
  
  // Check for response
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("HTTP INIT Response: " + rev);
    if (rev.indexOf("ERROR") != -1) {
      Serial.println("HTTP service already initialized, terminating first...");
      SentSerial("AT+HTTPTERM");
      delay(1000);
      SentSerial("AT+HTTPINIT");
      delay(1000);
      if (Serial1.available()) {
        rev = Serial1.readString();
        Serial.println("HTTP INIT Response (after terminate): " + rev);
      }
    }
  }
  
  // Set HTTP parameters
  String urlParam = "AT+HTTPPARA=\"URL\",\"" + String(url) + "\"";
  Serial.println("Setting URL: " + urlParam);
  if (!SentMessage(urlParam.c_str(), 3000)) {
    Serial.println("Failed to set URL parameter");
    return false;
  }
  
  // Set content type
  Serial.println("Setting content type...");
  if (!SentMessage("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 2000)) {
    Serial.println("Failed to set content type");
    return false;
  }
  
  // Set data length
  String dataLenParam = "AT+HTTPDATA=" + String(strlen(data)) + ",10000";
  Serial.println("Setting data length: " + dataLenParam);
  SentSerial(dataLenParam.c_str());
  delay(1000);
  
  // Check for "DOWNLOAD" prompt
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("HTTPDATA Response: " + rev);
    if (rev.indexOf("DOWNLOAD") == -1) {
      Serial.println("Did not receive DOWNLOAD prompt");
      return false;
    }
  }
  
  // Send data
  Serial.println("Sending data: " + String(data));
  SentSerial(data);
  delay(2000); // Give more time for data transmission
  
  // Check for OK after data transmission
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Data transmission response: " + rev);
    if (rev.indexOf("OK") == -1) {
      Serial.println("Data transmission failed");
      return false;
    }
  }
  
  // Execute HTTP POST action
  Serial.println("Executing HTTP POST action...");
  if (!SentMessage("AT+HTTPACTION=1", 10000)) { // POST action, 10 second timeout
    Serial.println("Failed to execute HTTP POST");
    return false;
  }
  
  // Read HTTP response
  delay(2000); // Wait for response
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("HTTP Response: " + rev);
  }
  
  // Read HTTP data
  Serial.println("Reading HTTP data...");
  SentSerial("AT+HTTPREAD");
  delay(1000);
  
  // Check for response format: +HTTPREAD: <length>,<timeout>
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("HTTPREAD Response: " + rev);
    
    // If we get a response with length, wait for the actual data
    if (rev.indexOf("+HTTPREAD:") != -1) {
      delay(2000); // Wait for the actual data
      if (Serial1.available()) {
        rev = Serial1.readString();
        Serial.println("HTTP Data: " + rev);
      }
    }
  }
  
  // Terminate HTTP service
  Serial.println("Terminating HTTP service...");
  SentMessage("AT+HTTPTERM", 2000);
  
  return true;
}

// Function to check network status
bool checkNetworkStatus() {
  Serial.println("Checking network status...");
  
  // Check signal quality
  SentSerial("AT+CSQ");
  delay(1000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Signal Quality: " + rev);
  }
  
  // Check network registration
  SentSerial("AT+CREG?");
  delay(1000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Network Registration: " + rev);
  }
  
  // Check PDP context status
  SentSerial("AT+CGACT?");
  delay(1000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("PDP Context: " + rev);
  }
  
  return true;
}

// Function to activate data connection
bool activateDataConnection() {
  Serial.println("Activating data connection...");
  
  // Activate PDP context
  if (!SentMessage("AT+CGACT=1,1", 5000)) {
    Serial.println("Failed to activate PDP context");
    return false;
  }
  
  // Check activation status
  if (!SentMessage("AT+CGACT?", 2000)) {
    Serial.println("Failed to check activation status");
    return false;
  }
  
  // Check network status
  checkNetworkStatus();
  
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);

  while (!SentMessage("AT", 2000)) {
    delay(1000);
  }
  
  SentMessage("ATD10086;", 2000);
  SentSerial("ATE1;");
  SentSerial("AT+COPS?");
  SentSerial("AT+CGDCONT?");
  SentSerial("AT+SIMCOMATI");
  
  // Activate data connection
  if (activateDataConnection()) {
    Serial.println("Data connection activated successfully!");
    
    // Make HTTP GET request to your server - using shorter URL for testing
    const char* serverUrl = "http://httpbin.org/post";
    const char* testData = "{\"device\":\"SIM7670G\",\"status\":\"connected\",\"timestamp\":\"2024-01-01T12:00:00Z\"}";
    
    Serial.println("Making HTTP POST request to test endpoint...");
    if (makeHTTPPostRequest(serverUrl, testData)) {
      Serial.println("HTTP POST request successful!");
    } else {
      Serial.println("HTTP POST request failed!");
    }
  } else {
    Serial.println("Failed to activate data connection!");
  }
}

void loop() {
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println(rev);
  }
  
  // Make periodic HTTP requests every 30 seconds
  static unsigned long lastRequest = 0;
  if (millis() - lastRequest > 30000) {
    lastRequest = millis();
    
    const char* serverUrl = "http://httpbin.org/post";
    
    // Create JSON string with dynamic uptime
    String jsonData = "{\"device\":\"SIM7670G\",\"status\":\"periodic\",\"timestamp\":\"2024-01-01T12:00:00Z\",\"uptime\":\"" + String(millis()) + "\"}";
    
    Serial.println("Making periodic HTTP POST request...");
    if (makeHTTPPostRequest(serverUrl, jsonData.c_str())) {
      Serial.println("Periodic HTTP POST request successful!");
    } else {
      Serial.println("Periodic HTTP POST request failed!");
    }
  }
}