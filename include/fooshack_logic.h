#ifndef FOOSHACK_LOGIC_H
#define FOOSHACK_LOGIC_H

#ifndef NATIVE_ENV
#include <Arduino.h>
#endif
#include <ArduinoJson.h>

// Mock Arduino types for native testing
#ifdef NATIVE_ENV
#include <string>
class String {
public:
    String() : data("") {}
    String(const char* str) : data(str) {}
    String(const String& other) : data(other.data) {}
    
    String& operator=(const String& other) {
        data = other.data;
        return *this;
    }
    
    bool operator==(const String& other) const {
        return data == other.data;
    }
    
    bool operator==(const char* str) const {
        return data == str;
    }
    
    const char* c_str() const {
        return data.c_str();
    }
    
private:
    std::string data;
};
#endif

// Timing constants
#define BUTTON_DEBOUNCE_MS 500
#define LASER_COOLDOWN_MS 2000
#define WIFI_RETRY_DELAY_MS 5000
#define LED_BLINK_INTERVAL_MS 250
#define OWN_GOAL_BLINK_INTERVAL_MS 250

// Button state structure
struct ButtonState {
  bool currentState;
  bool lastState;
  unsigned long lastDebounceTime;
  bool pressed;
};

// LED state structure
struct LEDState {
  bool ledActivityState;
  unsigned long lastLedBlinkTime;
  bool ownGoalBlinking;
  unsigned long ownGoalBlinkStartTime;
  bool goalFlashing;
  unsigned long goalFlashStartTime;
};

// Game state structure
struct GameState {
  String playerColor;
  String opponentColor;
  bool quantumMode;
  bool wifiConnected;
  unsigned long lastLaserBreakTime;
  bool lastLaserState;
};

// Function declarations for testable logic
class FooshackLogic {
public:
  // Button logic
  static bool shouldButtonPress(ButtonState* button, bool currentReading, unsigned long currentTime);
  static void updateButtonState(ButtonState* button, bool currentReading, unsigned long currentTime);
  
  // Timing logic
  static bool isInCooldownPeriod(unsigned long lastEventTime, unsigned long currentTime, unsigned long cooldownMs);
  static bool shouldBlinkLED(unsigned long startTime, unsigned long currentTime, unsigned long intervalMs, unsigned long durationMs);
  
  // JSON payload creation
  static String createGoalPayload(const String& playerColor);
  static String createPointPayload(const String& playerColor, int amount, bool quantumMode);
  static String createResetPayload(const String& playerColor);
  static String createStatusResponse(const String& playerColor, bool wifiConnected, bool quantumMode);
  
  // Game logic
  static String getOpponentColor(const String& playerColor);
  static bool isValidPlayerColor(const String& color);
  
  // LED state management
  static void updateLEDState(LEDState* ledState, unsigned long currentTime);
  static bool shouldTriggerGoalFlash(LEDState* ledState, unsigned long currentTime);
  static bool shouldTriggerOwnGoalBlink(LEDState* ledState, unsigned long currentTime);
  
  // Laser break detection logic
  static bool shouldTriggerGoal(bool currentLaserState, bool lastLaserState, 
                               unsigned long lastBreakTime, unsigned long currentTime);
};

#endif // FOOSHACK_LOGIC_H
