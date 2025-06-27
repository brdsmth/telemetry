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

// Function to configure APN settings
bool configureAPN() {
  Serial.println("Configuring APN settings for Hologram...");
  
  // Set PDP context parameters for Hologram APN
  if (!SentMessage("AT+CGDCONT=1,\"IP\",\"hologram\"", 3000)) {
    Serial.println("Failed to set PDP context parameters");
    return false;
  }
  
  // Verify the APN settings
  SentSerial("AT+CGDCONT?");
  delay(1000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("PDP Context Settings: " + rev);
  }
  
  return true;
}

// Function to troubleshoot SIM card detection
bool troubleshootSIMCard() {
  Serial.println("Troubleshooting SIM card detection...");
  
  // First, try to reset the module
  Serial.println("Attempting module reset...");
  SentSerial("AT+CFUN=1,1"); // Reset the module
  delay(10000); // Wait 10 seconds for reset
  
  // Wait for module to be ready
  Serial.println("Waiting for module to be ready...");
  int attempts = 0;
  while (attempts < 20) {
    SentSerial("AT");
    delay(1000);
    if (Serial1.available()) {
      rev = Serial1.readString();
      if (rev.indexOf("OK") != -1) {
        Serial.println("Module is ready!");
        break;
      }
    }
    attempts++;
  }
  
  if (attempts >= 20) {
    Serial.println("Module not responding after reset!");
    return false;
  }
  
  // Check SIM card status multiple times
  Serial.println("Checking SIM card status...");
  for (int i = 0; i < 5; i++) {
    Serial.println("SIM check attempt " + String(i + 1) + "/5");
    SentSerial("AT+CPIN?");
    delay(3000);
    if (Serial1.available()) {
      rev = Serial1.readString();
      Serial.println("SIM Status: " + rev);
      
      if (rev.indexOf("READY") != -1) {
        Serial.println("SIM card detected and ready!");
        return true;
      }
      
      if (rev.indexOf("SIM not inserted") != -1) {
        Serial.println("SIM not detected - checking hardware...");
      }
    }
    delay(2000);
  }
  
  // Try SIM card detection command
  Serial.println("Trying SIM card detection command...");
  SentSerial("AT+CSIM=0");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("CSIM Response: " + rev);
  }
  
  // Check SIM card information
  Serial.println("Checking SIM card information...");
  SentSerial("AT+CCID");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("CCID Response: " + rev);
  }
  
  Serial.println("SIM card troubleshooting complete. Please check:");
  Serial.println("1. SIM card is properly inserted");
  Serial.println("2. SIM card contacts are clean");
  Serial.println("3. SIM card holder is secure");
  Serial.println("4. Power supply is stable");
  
  return false;
}

// Function to initialize SIM card and register to network
bool initializeSIMAndNetwork() {
  Serial.println("Initializing SIM card and network registration...");
  
  // Check SIM card status
  Serial.println("Checking SIM card status...");
  SentSerial("AT+CPIN?");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("SIM Status: " + rev);
    if (rev.indexOf("READY") == -1) {
      Serial.println("SIM card not ready! Starting troubleshooting...");
      if (!troubleshootSIMCard()) {
        Serial.println("SIM card troubleshooting failed!");
        return false;
      }
    }
  }
  
  // Check current operator
  Serial.println("Checking current operator...");
  SentSerial("AT+COPS?");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Current Operator: " + rev);
  }
  
  // Try to set automatic operator selection
  Serial.println("Setting automatic operator selection...");
  if (!SentMessage("AT+COPS=0", 5000)) {
    Serial.println("Failed to set automatic operator selection");
  }
  
  // Check network registration status
  Serial.println("Checking network registration...");
  SentSerial("AT+CREG?");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Network Registration: " + rev);
  }
  
  // If not registered, try to register
  int registrationAttempts = 0;
  const int maxAttempts = 15; // Increased attempts
  
  while (registrationAttempts < maxAttempts) {
    Serial.println("Attempting network registration... (Attempt " + String(registrationAttempts + 1) + "/" + String(maxAttempts) + ")");
    
    // Check current registration status
    SentSerial("AT+CREG?");
    delay(3000);
    if (Serial1.available()) {
      rev = Serial1.readString();
      Serial.println("Registration Status: " + rev);
      
      // Check if registered (0,1 or 0,5 means registered)
      if (rev.indexOf("+CREG: 0,1") != -1 || rev.indexOf("+CREG: 0,5") != -1) {
        Serial.println("Successfully registered to network!");
        return true;
      }
      
      // Check if searching (0,2 means searching)
      if (rev.indexOf("+CREG: 0,2") != -1) {
        Serial.println("Searching for network...");
      }
      
      // Check if denied (0,3 means registration denied)
      if (rev.indexOf("+CREG: 0,3") != -1) {
        Serial.println("Registration denied by network!");
        return false;
      }
    }
    
    // Check signal quality during registration attempts
    SentSerial("AT+CSQ");
    delay(1000);
    if (Serial1.available()) {
      rev = Serial1.readString();
      Serial.println("Signal Quality during registration: " + rev);
    }
    
    // Wait before next attempt
    delay(5000);
    registrationAttempts++;
  }
  
  Serial.println("Failed to register to network after " + String(maxAttempts) + " attempts");
  return false;
}

// Function to activate data connection
bool activateDataConnection() {
  Serial.println("Activating data connection...");
  
  // Initialize SIM and register to network first
  if (!initializeSIMAndNetwork()) {
    Serial.println("Failed to initialize SIM or register to network");
    return false;
  }
  
  // Configure APN
  if (!configureAPN()) {
    Serial.println("Failed to configure APN");
    return false;
  }
  
  // Check signal quality
  Serial.println("Checking signal quality...");
  SentSerial("AT+CSQ");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Signal Quality: " + rev);
  }
  
  // Deactivate any existing PDP context first
  Serial.println("Deactivating any existing PDP context...");
  SentMessage("AT+CGACT=0,1", 3000);
  delay(2000);
  
  // Check current PDP context status
  Serial.println("Checking current PDP context status...");
  SentSerial("AT+CGACT?");
  delay(2000);
  if (Serial1.available()) {
    rev = Serial1.readString();
    Serial.println("Current PDP Context: " + rev);
  }
  
  // Activate PDP context with longer timeout
  Serial.println("Activating PDP context...");
  if (!SentMessage("AT+CGACT=1,1", 15000)) { // Increased timeout to 15 seconds
    Serial.println("Failed to activate PDP context");
    
    // Try alternative activation method
    Serial.println("Trying alternative activation method...");
    SentSerial("AT+CGACT=1,1");
    delay(5000);
    
    // Check if activation succeeded
    SentSerial("AT+CGACT?");
    delay(2000);
    if (Serial1.available()) {
      rev = Serial1.readString();
      Serial.println("PDP Context after retry: " + rev);
      if (rev.indexOf("+CGACT: 1,1") != -1) {
        Serial.println("PDP context activated successfully on retry!");
      } else {
        Serial.println("PDP context activation failed on retry");
        return false;
      }
    } else {
      Serial.println("No response from retry attempt");
      return false;
    }
  } else {
    Serial.println("PDP context activated successfully!");
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