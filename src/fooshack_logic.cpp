#include "fooshack_logic.h"

// Button logic implementation
bool FooshackLogic::shouldButtonPress(ButtonState* button, bool currentReading, unsigned long currentTime) {
  // Check if enough time has passed since last debounce
  if ((currentTime - button->lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
    // Check if state has changed and button is now pressed
    if (currentReading != button->currentState && currentReading) {
      return true;
    }
  }
  return false;
}

void FooshackLogic::updateButtonState(ButtonState* button, bool currentReading, unsigned long currentTime) {
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

// Timing logic implementation
bool FooshackLogic::isInCooldownPeriod(unsigned long lastEventTime, unsigned long currentTime, unsigned long cooldownMs) {
  return (currentTime - lastEventTime) < cooldownMs;
}

bool FooshackLogic::shouldBlinkLED(unsigned long startTime, unsigned long currentTime, unsigned long intervalMs, unsigned long durationMs) {
  unsigned long elapsed = currentTime - startTime;
  if (elapsed >= durationMs) {
    return false; // Duration exceeded
  }
  
  // Calculate if we should be in the "on" phase of the blink
  unsigned long cyclePosition = elapsed % (intervalMs * 2);
  return cyclePosition < intervalMs;
}

// JSON payload creation implementation
String FooshackLogic::createGoalPayload(const String& playerColor) {
  JsonDocument doc;
  doc["player"] = playerColor;
  
  String payload;
  serializeJson(doc, payload);
  return payload;
}

String FooshackLogic::createPointPayload(const String& playerColor, int amount, bool quantumMode) {
  JsonDocument doc;
  doc["player"] = playerColor;
  doc["amount"] = amount;
  doc["quantum"] = quantumMode;
  
  String payload;
  serializeJson(doc, payload);
  return payload;
}

String FooshackLogic::createResetPayload(const String& playerColor) {
  JsonDocument doc;
  doc["player"] = playerColor;
  
  String payload;
  serializeJson(doc, payload);
  return payload;
}

String FooshackLogic::createStatusResponse(const String& playerColor, bool wifiConnected, bool quantumMode) {
  JsonDocument doc;
  doc["player"] = playerColor;
  doc["wifi"] = wifiConnected;
  doc["quantumMode"] = quantumMode;
  
  String response;
  serializeJson(doc, response);
  return response;
}

// Game logic implementation
String FooshackLogic::getOpponentColor(const String& playerColor) {
  if (playerColor == "red") {
    return "blue";
  } else if (playerColor == "blue") {
    return "red";
  }
  return ""; // Invalid color
}

bool FooshackLogic::isValidPlayerColor(const String& color) {
  return (color == "red" || color == "blue");
}

// LED state management implementation
void FooshackLogic::updateLEDState(LEDState* ledState, unsigned long currentTime) {
  // Priority order: Goal flash > Own goal blinking
  
  // 1. Goal flash (solid LED for 2 seconds after laser break)
  if (ledState->goalFlashing) {
    if (currentTime - ledState->goalFlashStartTime >= 2000) { // 2 seconds
      ledState->goalFlashing = false;
    }
    return; // Skip other LED states while goal flashing
  }

  // 2. Own goal celebration blinking (250ms intervals for 2 seconds)
  if (ledState->ownGoalBlinking) {
    if (currentTime - ledState->ownGoalBlinkStartTime >= 2000) { // 2 seconds
      ledState->ownGoalBlinking = false;
    } else if (currentTime - ledState->lastLedBlinkTime > OWN_GOAL_BLINK_INTERVAL_MS) {
      ledState->ledActivityState = !ledState->ledActivityState;
      ledState->lastLedBlinkTime = currentTime;
    }
  }
}

bool FooshackLogic::shouldTriggerGoalFlash(LEDState* ledState, unsigned long currentTime) {
  return ledState->goalFlashing && 
         (currentTime - ledState->goalFlashStartTime < 2000);
}

bool FooshackLogic::shouldTriggerOwnGoalBlink(LEDState* ledState, unsigned long currentTime) {
  if (!ledState->ownGoalBlinking) {
    return false;
  }
  
  if (currentTime - ledState->ownGoalBlinkStartTime >= 2000) {
    return false; // Duration exceeded
  }
  
  // Check if we should be in the "on" phase of the blink
  unsigned long elapsed = currentTime - ledState->ownGoalBlinkStartTime;
  unsigned long cyclePosition = elapsed % (OWN_GOAL_BLINK_INTERVAL_MS * 2);
  return cyclePosition < OWN_GOAL_BLINK_INTERVAL_MS;
}

// Laser break detection logic implementation
bool FooshackLogic::shouldTriggerGoal(bool currentLaserState, bool lastLaserState, 
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
