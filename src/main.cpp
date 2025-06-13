#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>

// Pin definitions
#define LASER_BREAK_PIN 14
#define BUTTON_UP_PIN 5
#define BUTTON_DOWN_PIN 4
#define BUTTON_QUANTUM_PIN 0
#define LED_ACTIVITY_PIN 16
#define LED_BUILTIN_PIN 2  // Built-in blue LED for network activity

// RGB LED pins for status indication
#define RGB_RED_PIN 15
#define RGB_GREEN_PIN 12
#define RGB_BLUE_PIN 13

// Timing constants
#define BUTTON_DEBOUNCE_MS 500
#define LASER_COOLDOWN_MS 2000
#define WIFI_RETRY_DELAY_MS 5000
#define LED_BLINK_INTERVAL_MS 250
#define OWN_GOAL_BLINK_INTERVAL_MS 250

// WiFi credentials - UPDATE THESE
const char* ssid = "Foosball_Table";  // Replace with your Pi's AP SSID
const char* password = "ilovefoosball";  // Replace with your Pi's AP password

// API configuration
const char* baseUrl = "http://192.168.4.1:3000";  // Replace with Pi's IP
// String playerColor = "red";
String playerColor = "blue";

String opponentColor = (playerColor == "red") ? "blue" : "red";

// Global objects
ESP8266WebServer server(80);
WiFiClient wifiClient;
HTTPClient httpClient;

// State variables
struct ButtonState {
  bool currentState;
  bool lastState;
  unsigned long lastDebounceTime;
  bool pressed;
};

ButtonState buttonUp = {false, false, 0, false};
ButtonState buttonDown = {false, false, 0, false};
ButtonState buttonQuantum = {false, false, 0, false};

bool lastLaserState = false;
unsigned long lastLaserBreakTime = 0;
bool quantumMode = false;

// LED state variables
bool ledActivityState = false;
unsigned long lastLedBlinkTime = 0;
bool ownGoalBlinking = false;
unsigned long ownGoalBlinkStartTime = 0;
bool goalFlashing = false;
unsigned long goalFlashStartTime = 0;

// WiFi state
bool wifiConnected = false;
unsigned long lastWifiRetry = 0;
bool connectingBlinkState = false;
unsigned long lastConnectingBlink = 0;

// Network activity LED state
unsigned long networkActivityTime = 0;
bool networkActivityLED = false;

// Function declarations
void setupWiFi();
void setupServer();
void handleWiFi();
void updateButtons();
void updateButton(ButtonState* button, int pin);
void handleLaserBreak();
void handleButtonActions();
void updateLEDs();
void triggerOwnGoalBlink();
void triggerGoalFlash();
void sendGoalAPI();
void sendAddPointAPI(int amount);
void sendResetAPI();
void setRGBColor(int red, int green, int blue);
void setStatusColor(String status);
void flashNetworkActivity();
void updateNetworkActivityLED();
void startupBlink();
void sendTestOpponentScore();

void setup() {
  Serial.begin(115200);
  Serial.println("FoosHack ESP8266 Starting...");

  // Show startup status - Purple
  setStatusColor("startup");

  // Initialize pins
  pinMode(LASER_BREAK_PIN, INPUT_PULLUP);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_QUANTUM_PIN, INPUT_PULLUP);
  pinMode(LED_ACTIVITY_PIN, OUTPUT);
  pinMode(LED_BUILTIN_PIN, OUTPUT);

  // Initialize RGB LED pins
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  // Initialize LEDs (off)
  digitalWrite(LED_ACTIVITY_PIN, LOW);
  digitalWrite(LED_BUILTIN_PIN, HIGH); // Built-in LED off (inverted logic)
  setRGBColor(0, 0, 0); // Turn off RGB LED

  // Startup blink sequence - 3 blinks to confirm boot
  Serial.println("Performing startup blink sequence...");
  startupBlink();

  // Show connecting status - Yellow
  setStatusColor("connecting");

  // Initialize WiFi
  setupWiFi();

  // Setup HTTP server endpoints
  setupServer();

  // Show ready status - Player color
  setStatusColor("ready");

  Serial.println("Setup complete!");
}

void loop() {
  // Handle WiFi connection
  handleWiFi();

  // Handle HTTP server
  server.handleClient();

  // Read and debounce buttons
  updateButtons();

  // Handle laser break detection
  handleLaserBreak();

  // Handle button actions
  handleButtonActions();

  // Update LED states
  updateLEDs();

  // Update network activity LED
  updateNetworkActivityLED();

  // Small delay to prevent overwhelming the loop
  delay(10);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  bool blinkState = false;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    // Blink yellow while connecting
    if (blinkState) {
      setRGBColor(255, 255, 0); // Yellow on
    } else {
      setRGBColor(0, 0, 0); // Off
    }
    blinkState = !blinkState;

    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());

    // Wait 5 seconds then send test opponent score
    Serial.println("Waiting 5 seconds before sending test opponent score...");
    delay(5000);
    sendTestOpponentScore();
  } else {
    wifiConnected = false;
    Serial.println();
    Serial.println("WiFi connection failed!");
  }
}

void setupServer() {
  // Endpoint to receive opponent score notifications
  server.on("/scoreMade", HTTP_POST, []() {
    Serial.println("=== INCOMING API: /scoreMade ===");

    // Flash network activity LED for incoming data
    flashNetworkActivity();

    String body = server.arg("plain");
    Serial.print("Request body: ");
    Serial.println(body);

    // Parse JSON
    JsonDocument doc;
    deserializeJson(doc, body);
    String scoringPlayer = doc["player"];

    Serial.print("Scoring player: ");
    Serial.println(scoringPlayer);
    Serial.print("This device player: ");
    Serial.println(playerColor);

    // Handle own goal celebration
    if (scoringPlayer == playerColor) {
      Serial.println("THIS PLAYER SCORED! Triggering celebration blink sequence");
      triggerOwnGoalBlink();
    } else {
      Serial.println("Opponent scored - no action needed (goal flash already handled by defending side)");
    }

    server.send(200, "application/json", "{}");
    Serial.println("Sent 200 OK response");
  });

  // Status endpoint
  server.on("/status", HTTP_GET, []() {
    Serial.println("=== INCOMING API: /status ===");

    // Flash network activity LED for incoming data
    flashNetworkActivity();

    JsonDocument doc;
    doc["player"] = playerColor;
    doc["wifi"] = wifiConnected;
    doc["quantumMode"] = quantumMode;

    String response;
    serializeJson(doc, response);

    Serial.print("Sending status response: ");
    Serial.println(response);

    server.send(200, "application/json", response);
    Serial.println("Status request completed");
  });

  server.begin();
  Serial.println("HTTP server started on port 80");
  Serial.println("Available endpoints:");
  Serial.println("  POST /scoreMade - Receive opponent score notifications");
  Serial.println("  GET /status - Device status information");
}

void handleWiFi() {
  unsigned long currentTime = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      // Just disconnected
      wifiConnected = false;
      setStatusColor("disconnected");
    }

    // Check if we're in reconnection mode
    if (currentTime - lastWifiRetry < WIFI_RETRY_DELAY_MS) {
      // Blink yellow while trying to reconnect
      if (currentTime - lastConnectingBlink > 500) {
        connectingBlinkState = !connectingBlinkState;
        if (connectingBlinkState) {
          setRGBColor(255, 255, 0); // Yellow on
        } else {
          setRGBColor(0, 0, 0); // Off
        }
        lastConnectingBlink = currentTime;
      }
    } else {
      // Time to retry connection
      Serial.println("WiFi disconnected, attempting reconnection...");
      setStatusColor("connecting");
      WiFi.reconnect();
      lastWifiRetry = currentTime;
    }
  } else if (!wifiConnected) {
    wifiConnected = true;
    setStatusColor("ready");
    Serial.println("WiFi reconnected!");
  }
}

void updateButtons() {
  updateButton(&buttonUp, BUTTON_UP_PIN);
  updateButton(&buttonDown, BUTTON_DOWN_PIN);
  updateButton(&buttonQuantum, BUTTON_QUANTUM_PIN);
}

void updateButton(ButtonState* button, int pin) {
  bool reading = !digitalRead(pin); // Inverted because of pullup
  unsigned long currentTime = millis();

  if (reading != button->lastState) {
    button->lastDebounceTime = currentTime;
    Serial.print("Button state change detected on pin ");
    Serial.print(pin);
    Serial.print(": ");
    Serial.println(reading ? "PRESSED" : "RELEASED");
  }

  if ((currentTime - button->lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
    if (reading != button->currentState) {
      button->currentState = reading;

      if (button->currentState) {
        button->pressed = true;
        Serial.print("Button CONFIRMED PRESS on pin ");
        Serial.print(pin);
        Serial.print(" after debounce (");
        Serial.print(currentTime - button->lastDebounceTime);
        Serial.println("ms)");
      }
    }
  }

  button->lastState = reading;
}

void handleLaserBreak() {
  bool currentLaserState = !digitalRead(LASER_BREAK_PIN); // Inverted for break beam
  unsigned long currentTime = millis();

  // Log laser state changes
  if (currentLaserState != lastLaserState) {
    Serial.print("Laser break sensor state change: ");
    Serial.print(currentLaserState ? "BROKEN" : "RESTORED");
    Serial.print(" (pin ");
    Serial.print(LASER_BREAK_PIN);
    Serial.println(")");
  }

  // Detect laser break (transition from unbroken to broken)
  if (currentLaserState && !lastLaserState) {
    Serial.print("Laser break detected! Checking cooldown... ");

    // Check cooldown period
    if (currentTime - lastLaserBreakTime > LASER_COOLDOWN_MS) {
      Serial.println("GOAL CONFIRMED! Sending API call.");
      Serial.print("Time since last goal: ");
      Serial.print(currentTime - lastLaserBreakTime);
      Serial.println("ms");

      sendGoalAPI();
      lastLaserBreakTime = currentTime;

      // Trigger 2-second solid LED flash
      Serial.println("Triggering 2-second solid LED flash for goal");
      triggerGoalFlash();
    } else {
      Serial.print("Goal ignored - still in cooldown period. Time remaining: ");
      Serial.print(LASER_COOLDOWN_MS - (currentTime - lastLaserBreakTime));
      Serial.println("ms");
    }
  }

  lastLaserState = currentLaserState;
}

void handleButtonActions() {
  // Check for reset combination (Up + Down pressed simultaneously)
  if (buttonUp.pressed && buttonDown.pressed) {
    Serial.println("=== RESET COMBINATION DETECTED ===");
    Serial.println("Both Up and Down buttons pressed simultaneously");
    Serial.print("Sending reset API for player: ");
    Serial.println(playerColor);
    sendResetAPI();
    buttonUp.pressed = false;
    buttonDown.pressed = false;
    return;
  }

  // Handle individual button presses
  if (buttonUp.pressed) {
    Serial.println("=== UP BUTTON ACTION ===");
    Serial.print("Adding 1 point for player: ");
    Serial.print(playerColor);
    Serial.print(", Quantum mode: ");
    Serial.println(quantumMode ? "ENABLED" : "DISABLED");
    sendAddPointAPI(1);
    buttonUp.pressed = false;
  }

  if (buttonDown.pressed) {
    Serial.println("=== DOWN BUTTON ACTION ===");
    Serial.print("Adding 1 point for player: ");
    Serial.print(playerColor);
    Serial.print(", Quantum mode: ");
    Serial.println(quantumMode ? "ENABLED" : "DISABLED");
    sendAddPointAPI(1);
    buttonDown.pressed = false;
  }

  if (buttonQuantum.pressed) {
    quantumMode = !quantumMode;
    Serial.println("=== QUANTUM MODE TOGGLE ===");
    Serial.print("Quantum mode changed to: ");
    Serial.println(quantumMode ? "ENABLED" : "DISABLED");
    Serial.println("Showing visual feedback on RGB LED");

    // Flash RGB LED to indicate quantum mode toggle
    if (quantumMode) {
      setRGBColor(0, 255, 0); // Green for quantum mode ON
    } else {
      setRGBColor(255, 0, 0); // Red for quantum mode OFF
    }
    delay(300);
    setStatusColor("ready"); // Return to normal status color

    buttonQuantum.pressed = false;
  }
}

void updateLEDs() {
  unsigned long currentTime = millis();

  // Priority order: Goal flash > Own goal blinking
  
  // 1. Goal flash (solid LED for 2 seconds after laser break)
  if (goalFlashing) {
    if (currentTime - goalFlashStartTime < 2000) { // Flash for 2 seconds
      digitalWrite(LED_ACTIVITY_PIN, HIGH); // Solid ON
    } else {
      goalFlashing = false;
      digitalWrite(LED_ACTIVITY_PIN, LOW);
    }
    return; // Skip other LED states while goal flashing
  }

  // 2. Own goal celebration blinking (250ms intervals for 2 seconds)
  if (ownGoalBlinking) {
    if (currentTime - ownGoalBlinkStartTime < 2000) { // Blink for 2 seconds
      if (currentTime - lastLedBlinkTime > OWN_GOAL_BLINK_INTERVAL_MS) { // 250ms intervals
        ledActivityState = !ledActivityState;
        digitalWrite(LED_ACTIVITY_PIN, ledActivityState);
        lastLedBlinkTime = currentTime;
      }
    } else {
      ownGoalBlinking = false;
      digitalWrite(LED_ACTIVITY_PIN, LOW);
    }
  }
}


void sendGoalAPI() {
  Serial.println("=== SENDING GOAL API ===");

  if (!wifiConnected) {
    Serial.println("ERROR: WiFi not connected, cannot send goal API");
    return;
  }

  // Flash network activity LED for outgoing data
  flashNetworkActivity();

  Serial.print("Connecting to: ");
  Serial.println(String(baseUrl) + "/score");

  httpClient.begin(wifiClient, String(baseUrl) + "/score");
  httpClient.addHeader("Content-Type", "application/json");
  // httpClient.addHeader("X-API-Key", "your_api_key"); // Update with actual key

  JsonDocument doc;
  doc["player"] = opponentColor;

  String payload;
  serializeJson(doc, payload);

  Serial.print("Sending payload: ");
  Serial.println(payload);
  Serial.print("Opponent scored against player: ");
  Serial.println(playerColor);

  int httpResponseCode = httpClient.POST(payload);

  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = httpClient.getString();
    Serial.print("Server Response: ");
    Serial.println(response);
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpResponseCode);
  }

  httpClient.end();
  Serial.println("Goal API call completed");
}

void flashNetworkActivity() {
  // Flash the built-in LED for network activity (like ethernet port)
  networkActivityTime = millis();
  networkActivityLED = true;
  digitalWrite(LED_BUILTIN_PIN, LOW); // Turn on (inverted logic)
  Serial.println("Network activity flash");
}

void updateNetworkActivityLED() {
  // Handle network activity LED timing
  if (networkActivityLED && (millis() - networkActivityTime > 100)) {
    networkActivityLED = false;
    digitalWrite(LED_BUILTIN_PIN, HIGH); // Turn off (inverted logic)
  }
}

void setRGBColor(int red, int green, int blue) {
  // ESP8266 uses inverted logic for some pins, adjust as needed
  analogWrite(RGB_RED_PIN, red);
  analogWrite(RGB_GREEN_PIN, green);
  analogWrite(RGB_BLUE_PIN, blue);
}

void setStatusColor(String status) {
  if (status == "startup") {
    // Purple - Starting up (brighter purple)
    setRGBColor(255, 0, 255);
    Serial.println("Status: Startup (Purple)");
  }
  else if (status == "connecting") {
    // Yellow - Connecting to WiFi
    setRGBColor(255, 255, 0);
    Serial.println("Status: Connecting (Yellow)");
  }
  else if (status == "ready") {
    // Player color - Ready and connected
    if (playerColor == "red") {
      setRGBColor(255, 0, 0);
      Serial.println("Status: Ready (Red)");
    } else if (playerColor == "blue") {
      setRGBColor(0, 0, 255);
      Serial.println("Status: Ready (Blue)");
    }
  }
  else if (status == "disconnected") {
    // Orange - WiFi disconnected (brighter orange)
    setRGBColor(255, 128, 0);
    Serial.println("Status: Disconnected (Orange)");
  }
  else if (status == "error") {
    // Flashing red - Error state
    setRGBColor(255, 0, 0);
    delay(200);
    setRGBColor(0, 0, 0);
    delay(200);
    setRGBColor(255, 0, 0);
    Serial.println("Status: Error (Flashing Red)");
  }
  else if (status == "off") {
    // Turn off RGB LED
    setRGBColor(0, 0, 0);
    Serial.println("Status: Off");
  }
}

void sendAddPointAPI(int amount) {
  if (!wifiConnected) {
    Serial.println("WiFi not connected, cannot send add point API");
    return;
  }

  // Flash network activity LED for outgoing data
  flashNetworkActivity();

  httpClient.begin(wifiClient, String(baseUrl) + "/point");
  httpClient.addHeader("Content-Type", "application/json");
  httpClient.addHeader("X-API-Key", "your_api_key"); // Update with actual key

  JsonDocument doc;
  doc["player"] = playerColor;
  doc["amount"] = amount;
  doc["quantum"] = quantumMode;

  String payload;
  serializeJson(doc, payload);

  int httpResponseCode = httpClient.POST(payload);

  if (httpResponseCode > 0) {
    String response = httpClient.getString();
    Serial.println("Add point API response: " + String(httpResponseCode));
  } else {
    Serial.println("Add point API error: " + String(httpResponseCode));
  }

  httpClient.end();
}

void sendTestOpponentScore() {
  Serial.println("=== SENDING TEST OPPONENT SCORE ===");

  if (!wifiConnected) {
    Serial.println("ERROR: WiFi not connected, cannot send test opponent score");
    return;
  }

  // Flash network activity LED for outgoing data
  flashNetworkActivity();

  Serial.print("This device player: ");
  Serial.println(playerColor);
  Serial.print("Simulating opponent: ");
  Serial.println(opponentColor);
  Serial.print("Connecting to: ");
  Serial.println(String(baseUrl) + "/scoreMade");

  httpClient.begin(wifiClient, String(baseUrl) + "/scoreMade");
  httpClient.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["player"] = opponentColor;

  String payload;
  serializeJson(doc, payload);

  Serial.print("Sending test payload: ");
  Serial.println(payload);

  int httpResponseCode = httpClient.POST(payload);

  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = httpClient.getString();
    Serial.print("Server Response: ");
    Serial.println(response);
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpResponseCode);
  }

  httpClient.end();
  Serial.println("Test opponent score API call completed");
  Serial.println("This should trigger the LED blink sequence if successful!");
}

void startupBlink() {
  Serial.println("Starting LED activity pin blink sequence (3 blinks)");

  for (int i = 0; i < 3; i++) {
    Serial.print("Blink ");
    Serial.print(i + 1);
    Serial.println(" - ON");
    digitalWrite(LED_ACTIVITY_PIN, HIGH);
    delay(200);

    Serial.print("Blink ");
    Serial.print(i + 1);
    Serial.println(" - OFF");
    digitalWrite(LED_ACTIVITY_PIN, LOW);
    delay(200);
  }

  Serial.println("Startup blink sequence completed");
}

void sendResetAPI() {
  if (!wifiConnected) {
    Serial.println("WiFi not connected, cannot send reset API");
    return;
  }

  // Flash network activity LED for outgoing data
  flashNetworkActivity();

  httpClient.begin(wifiClient, String(baseUrl) + "/reset");
  httpClient.addHeader("Content-Type", "application/json");
  httpClient.addHeader("X-API-Key", "your_api_key"); // Update with actual key

  JsonDocument doc;
  doc["player"] = playerColor;

  String payload;
  serializeJson(doc, payload);

  int httpResponseCode = httpClient.POST(payload);

  if (httpResponseCode > 0) {
    String response = httpClient.getString();
    Serial.println("Reset API response: " + String(httpResponseCode));
  } else {
    Serial.println("Reset API error: " + String(httpResponseCode));
  }

  httpClient.end();
}

void triggerOwnGoalBlink() {
  Serial.println("Own goal scored! Triggering celebration LED blink");
  ownGoalBlinking = true;
  ownGoalBlinkStartTime = millis();
}

void triggerGoalFlash() {
  Serial.println("Triggering goal flash - solid LED for 2 seconds");
  goalFlashing = true;
  goalFlashStartTime = millis();
}
