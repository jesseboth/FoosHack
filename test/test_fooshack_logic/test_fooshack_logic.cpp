#include <unity.h>
#include <ArduinoJson.h>
#include <string>

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

// Button state structure
struct ButtonState {
  bool currentState;
  bool lastState;
  unsigned long lastDebounceTime;
  bool pressed;
};

// Simple test implementations (inline for testing)
class FooshackLogic {
public:
  static bool isInCooldownPeriod(unsigned long lastEventTime, unsigned long currentTime, unsigned long cooldownMs) {
    return (currentTime - lastEventTime) < cooldownMs;
  }
  
  static void updateButtonState(ButtonState* button, bool currentReading, unsigned long currentTime) {
    // Update debounce time if reading changed
    if (currentReading != button->lastState) {
      button->lastDebounceTime = currentTime;
    }

    // Update current state if debounce period has passed
    if ((currentTime - button->lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
      if (currentReading != button->currentState) {
        button->currentState = currentReading;
        if (button->currentState) {
          button->pressed = true;
        }
      }
    }

    button->lastState = currentReading;
  }
  
  static String createGoalPayload(const String& playerColor) {
    JsonDocument doc;
    doc["player"] = playerColor.c_str();
    
    std::string payload;
    serializeJson(doc, payload);
    return String(payload.c_str());
  }
  
  static String getOpponentColor(const String& playerColor) {
    if (playerColor == "red") {
      return String("blue");
    } else if (playerColor == "blue") {
      return String("red");
    }
    return String("");
  }
  
  static bool shouldTriggerGoal(bool currentLaserState, bool lastLaserState, 
                               unsigned long lastBreakTime, unsigned long currentTime) {
    // Detect laser break (transition from unbroken to broken)
    if (currentLaserState && !lastLaserState) {
      // Check cooldown period
      if (!isInCooldownPeriod(lastBreakTime, currentTime, LASER_COOLDOWN_MS)) {
        return true;
      }
    }
    return false;
  }
};

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

// Test timing logic
void test_cooldown_period_detection(void) {
    unsigned long lastEvent = 1000;
    unsigned long currentTime = 1500;
    unsigned long cooldown = 2000;
    
    bool inCooldown = FooshackLogic::isInCooldownPeriod(lastEvent, currentTime, cooldown);
    TEST_ASSERT_TRUE(inCooldown); // Should be in cooldown
    
    currentTime = 3500; // Past cooldown period
    inCooldown = FooshackLogic::isInCooldownPeriod(lastEvent, currentTime, cooldown);
    TEST_ASSERT_FALSE(inCooldown); // Should not be in cooldown
}

// Test button debouncing logic
void test_button_state_updates_correctly(void) {
    ButtonState button = {false, false, 0, false};
    
    // Initial state
    TEST_ASSERT_FALSE(button.currentState);
    TEST_ASSERT_FALSE(button.pressed);
    
    // Press button
    FooshackLogic::updateButtonState(&button, true, 100);
    
    // Should update debounce time but not state yet
    TEST_ASSERT_EQUAL(100, button.lastDebounceTime);
    TEST_ASSERT_FALSE(button.currentState);
    
    // After debounce period
    FooshackLogic::updateButtonState(&button, true, 100 + BUTTON_DEBOUNCE_MS + 1);
    TEST_ASSERT_TRUE(button.currentState);
    TEST_ASSERT_TRUE(button.pressed);
}

// Test game logic
void test_opponent_color_detection(void) {
    String opponent = FooshackLogic::getOpponentColor("red");
    TEST_ASSERT_EQUAL_STRING("blue", opponent.c_str());
    
    opponent = FooshackLogic::getOpponentColor("blue");
    TEST_ASSERT_EQUAL_STRING("red", opponent.c_str());
    
    opponent = FooshackLogic::getOpponentColor("invalid");
    TEST_ASSERT_EQUAL_STRING("", opponent.c_str());
}

// Test laser break detection
void test_laser_break_goal_detection(void) {
    unsigned long lastBreakTime = 1000;
    unsigned long currentTime = 4000; // Past cooldown
    
    // Test transition from unbroken to broken
    bool shouldTrigger = FooshackLogic::shouldTriggerGoal(true, false, lastBreakTime, currentTime);
    TEST_ASSERT_TRUE(shouldTrigger);
    
    // Test no transition
    shouldTrigger = FooshackLogic::shouldTriggerGoal(true, true, lastBreakTime, currentTime);
    TEST_ASSERT_FALSE(shouldTrigger);
    
    // Test during cooldown period
    currentTime = 2000; // Within cooldown
    shouldTrigger = FooshackLogic::shouldTriggerGoal(true, false, lastBreakTime, currentTime);
    TEST_ASSERT_FALSE(shouldTrigger);
}

// Test JSON payload creation
void test_goal_payload_creation(void) {
    String payload = FooshackLogic::createGoalPayload("red");
    
    // Parse the JSON to verify structure
    JsonDocument doc;
    deserializeJson(doc, payload.c_str());
    
    TEST_ASSERT_EQUAL_STRING("red", doc["player"]);
}

// Main test runner
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Core logic tests
    RUN_TEST(test_cooldown_period_detection);
    RUN_TEST(test_button_state_updates_correctly);
    RUN_TEST(test_opponent_color_detection);
    RUN_TEST(test_laser_break_goal_detection);
    RUN_TEST(test_goal_payload_creation);
    
    return UNITY_END();
}
