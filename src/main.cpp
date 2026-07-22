/*
 * LED Controller API v1.1.0
 * 
 * This program provides a comprehensive JSON-based API for controlling:
 * - Two RGB LED strips (NeoPixel) with individual pixel control
 * - Single LED with digital/analog control
 * - Two relays for switching external devices
 * - Sensor reading (analog and digital modes)
 * 
 * All commands are sent via Serial console in JSON format
 * Type 'help' for complete documentation and examples
 */

#include <Arduino.h>        // Core Arduino functions
#include <Adafruit_NeoPixel.h>  // Library for controlling NeoPixel LED strips
#include <ArduinoJson.h>    // Library for JSON parsing and generation
#include <Wire.h>           // I2C library for LM75 temperature sensor
#include <EEPROM.h>         // EEPROM library for persistent storage

// ================================
// EEPROM ADDRESSES
// ================================
const int EEPROM_MAGIC = 0;        // 2 bytes: magic number to check if EEPROM is initialized
const int EEPROM_NUM_LEDS_RGB1 = 2; // 2 bytes: number of LEDs in strip 1
const int EEPROM_NUM_LEDS_RGB2 = 4; // 2 bytes: number of LEDs in strip 2
const int EEPROM_DEFAULT_R1 = 6;    // 1 byte: default red for strip 1
const int EEPROM_DEFAULT_G1 = 7;    // 1 byte: default green for strip 1
const int EEPROM_DEFAULT_B1 = 8;    // 1 byte: default blue for strip 1
const int EEPROM_DEFAULT_R2 = 9;    // 1 byte: default red for strip 2
const int EEPROM_DEFAULT_G2 = 10;   // 1 byte: default green for strip 2
const int EEPROM_DEFAULT_B2 = 11;   // 1 byte: default blue for strip 2
const int EEPROM_LB_THRESHOLD = 12; // 2 bytes: LB sensor threshold
const int EEPROM_BRIGHTNESS_RGB1 = 14; // 1 byte: brightness for strip 1 (0-255)
const int EEPROM_BRIGHTNESS_RGB2 = 15; // 1 byte: brightness for strip 2 (0-255)
const int EEPROM_LED_DEFAULT = 16; // 1 byte: default LED brightness (0-255)
const int EEPROM_EFFECTS_BASE = 17;    // Effect defaults start here
const int EEPROM_EFFECT_RECORD_SIZE = 10; // r, g, b, brightness, interval, repeats, count
const int EEPROM_EFFECTS_PER_STRIP = 6; // running, charging, center, rainbow, flash, random
const int EEPROM_EFFECT_STRIDE = EEPROM_EFFECT_RECORD_SIZE * EEPROM_EFFECTS_PER_STRIP;
const int EEPROM_STARTUP_MODE_RGB1 = 157;   // 1 byte: startup mode for strip 1 (solid/effect)
const int EEPROM_STARTUP_EFFECT_RGB1 = 158;  // 1 byte: startup effect for strip 1
const int EEPROM_STARTUP_MODE_RGB2 = 159;    // 1 byte: startup mode for strip 2 (solid/effect)
const int EEPROM_STARTUP_EFFECT_RGB2 = 160;   // 1 byte: startup effect for strip 2
const int EEPROM_AUTO_RESET_12H = 161;        // 1 byte: auto reset toggle (0/1)

const uint16_t EEPROM_MAGIC_VALUE = 0xABCD; // Magic number to verify EEPROM data

// ================================
// PIN ASSIGNMENTS
// ================================
const int RGB1 = D1;      // Pin for RGB LED strip #1 (Ring-Top)
const int RGB2 = D2;      // Pin for RGB LED strip #2 (Door)
const int ledPin = D5;    // Pin for single LED (Barcode Scanner)
const int relay1 = D6;    // Pin for Relay #1 (Intercom control)
const int relay2 = D0;    // Pin for Relay #2 (General purpose)
const int RS = D7;        // Digital input from Ticket Barrier (ON/OFF)
const int LB = A0;        // Analog input from Paper Full Sensor (0-1023)

// ================================
// LED STRIP CONFIGURATION (LOADED FROM EEPROM)
// ================================
int numLedsRgb1 = 300;  // Number of LEDs in strip #1 (loaded from EEPROM)
int numLedsRgb2 = 300;  // Number of LEDs in strip #2 (loaded from EEPROM)
int defaultR1 = 255;    // Default red for strip 1
int defaultG1 = 120;    // Default green for strip 1
int defaultB1 = 40;    // Default blue for strip 1
int defaultR2 = 255;    // Default red for strip 2
int defaultG2 = 120;    // Default green for strip 2
int defaultB2 = 40;    // Default blue for strip 2
int brightnessRgb1 = 131; // Brightness for strip 1 (0-255)
int brightnessRgb2 = 131; // Brightness for strip 2 (0-255)
int ledDefaultBrightness = 110; // Default LED brightness (0-255, 0=off)
bool autoReset12hEnabled = true; // Auto reset toggle for a 12-hour cycle
uint32_t autoReset12hStartMs = 0;  // Start time for the current 12-hour cycle
const uint32_t AUTO_RESET_12H_MS = 12UL * 60UL * 60UL * 1000UL;

enum StartupMode : uint8_t {
  STARTUP_SOLID = 0,
  STARTUP_EFFECT = 1
};

enum RgbEffectMode : uint8_t {
  RGB_EFFECT_RUNNING = 0,
  RGB_EFFECT_CHARGING = 1,
  RGB_EFFECT_CENTER = 2,
  RGB_EFFECT_RAINBOW = 3,
  RGB_EFFECT_FLASH = 4,
  RGB_EFFECT_RANDOM = 5,
  RGB_EFFECT_BREATHING = 6,
  RGB_EFFECT_COUNT = 7
};

struct RgbEffectDefaults {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  uint16_t intervalMs;
  uint16_t repeats;
  uint16_t count;
};

struct RgbAnimationState {
  bool active;
  RgbEffectMode mode;
  uint16_t startIndex;
  uint16_t endIndex;
  uint16_t bandCount;
  uint16_t intervalMs;
  uint16_t repeatsRemaining;
  uint16_t step;
  uint16_t maxSteps;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  bool flashOn;
  uint16_t randomHue;
  uint16_t randomTargetHue;
  uint16_t randomHueStep;
  uint32_t lastUpdateMs;
};

const uint8_t RGB_STRIP_COUNT = 2;

uint8_t startupMode[RGB_STRIP_COUNT] = {STARTUP_SOLID, STARTUP_SOLID};
RgbEffectMode startupEffect[RGB_STRIP_COUNT] = {RGB_EFFECT_RAINBOW, RGB_EFFECT_RAINBOW};

RgbEffectDefaults rgbEffectDefaults[RGB_STRIP_COUNT][RGB_EFFECT_COUNT];
RgbAnimationState rgbAnimationState[RGB_STRIP_COUNT];

// ================================
// NEOPIXEL OBJECTS (INITIALIZED IN SETUP)
// ================================
// Initialize NeoPixel strips with:
// - Number of LEDs, Pin, Color order (GRB) + Data rate (800KHz)
Adafruit_NeoPixel* rgb1;
Adafruit_NeoPixel* rgb2;

// ================================
// GLOBAL VARIABLES
// ================================
int lbThreshold = 512;    // Threshold for LB sensor digital mode (0-1023 range)
String inputBuffer = "";  // Buffer to store incoming serial characters
bool inputComplete = false;  // Flag to indicate when a complete command is received

// PWM State Tracking (for NeoPixel timing conflict fix)
volatile int lastPwmValue = 0;  // Stores the current PWM value on ledPin
volatile bool pwmActive = false; // Tracks if PWM is currently active on ledPin

// ================================
// LM75 TEMPERATURE SENSOR (OPTIONAL)
// ================================
// We use software I2C pins (Wire.begin(SDA, SCL)) to avoid moving the NeoPixel pins (D1/D2).
// LM75A address defaults to 0x48 when A0/A1/A2 are tied to GND.
// Wiring:
//  LM75A VCC -> 3.3V
//  LM75A GND -> GND
//  LM75A SDA -> D3 (GPIO0) with 4.7k pull-up to 3.3V
//  LM75A SCL -> D4 (GPIO2) with 4.7k pull-up to 3.3V
//  A0/A1/A2 -> GND (address 0x48)
// NOTE: D3 (GPIO0) & D4 (GPIO2) must be HIGH at boot; pull-ups satisfy this.
const int I2C_SDA = D3;
const int I2C_SCL = D4;
const uint8_t LM75_ADDR = 0x48;  // Base address with A0/A1/A2 = LOW

// ================================
// FUNCTION PROTOTYPES (DECLARATIONS)
// ================================
// These declarations tell the compiler that these functions exist
// and will be defined later in the file

void processCommand(String command);
void handleRgbCommand(JsonDocument& doc);
void handleLedCommand(JsonDocument& doc);
void handleRelayCommand(JsonDocument& doc);
void handleReadCommand(JsonDocument& doc);
void handleConfigCommand(JsonDocument& doc);
void sendSuccess(String message);
void sendError(String message);
void showHelp();
float readLm75Temperature();
bool lm75Available();
void handleStatusCommand();
void loadEepromSettings();
void saveEepromSettings();
void initRgbEffectDefaults();
void loadRgbEffectDefaults(bool& needsSave);
void saveRgbEffectDefaults();
void applyControllerDefaultState();
void updateAutoResetTimer();
void updateRgbAnimations();
void applyStartupState(int strip);
bool startRgbEffectInternal(int strip, RgbEffectMode mode, int startIndex, int endIndex, int red, int green, int blue, int brightness, int intervalMs, int repeats, int count, bool announce);
void startRgbEffect(JsonDocument& doc, int strip, RgbEffectMode mode);
void stopRgbEffect(int strip, bool restoreDefault);
void applyStripDefault(int strip);
void updateRgbEffectDefaults(JsonDocument& doc, RgbEffectMode mode);
void renderRgbEffectFrame(int strip, RgbAnimationState& state);
const char* rgbEffectName(RgbEffectMode mode);
const char* startupModeName(uint8_t mode);
int getEffectEepromOffset(int strip, RgbEffectMode mode);
RgbEffectDefaults makeDefaultEffectConfig(int strip, RgbEffectMode mode);
bool loadEffectDefaultsAtOffset(int offset, RgbEffectDefaults& defaults);
void saveEffectDefaultsAtOffset(int offset, const RgbEffectDefaults& defaults);
void pausePwmForNeoPixel();  // Disable PWM before NeoPixel operations
void resumePwm();             // Restore PWM after NeoPixel operations
bool isByteValue(int value);
uint8_t scaleChannel(int value, int brightness);
uint32_t makeScaledColor(Adafruit_NeoPixel* strip, int r, int g, int b, int brightness);
void showStrip(Adafruit_NeoPixel* strip);
// Brightness is applied in software before writing RGB values to the strip.

const uint8_t NEOPIXEL_LIBRARY_BRIGHTNESS = 255;


/**
 * COMMAND PROCESSOR
 * Parses incoming commands and routes them to appropriate handlers
 * Supports both text commands ("help") and JSON commands
 * 
 * @param command The complete command string received via serial
 */
void processCommand(String command) {
  command.trim();  // Remove leading/trailing whitespace
  
  // ================================
  // HANDLE HELP COMMAND
  // ================================
  // Check if user typed "help" (case insensitive)
  if (command.equalsIgnoreCase("help")) {
    showHelp();  // Display complete API documentation
    return;
  }
  
  // ================================
  // PARSE JSON COMMAND
  // ================================
  // Create JSON document with 1024 bytes capacity
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, command);
  
  // Check if JSON parsing failed
  if (error) {
    sendError("Invalid JSON format");
    return;
  }
  
  // Extract the action field from JSON
  String action = doc["action"];
  
  // ================================
  // ROUTE TO APPROPRIATE HANDLER
  // ================================
  if (action == "rgb") {
    handleRgbCommand(doc);      // Handle RGB strip commands
  } else if (action == "led") {
    handleLedCommand(doc);      // Handle single LED commands
  } else if (action == "relay") {
    handleRelayCommand(doc);    // Handle relay commands
  } else if (action == "read") {
    handleReadCommand(doc);     // Handle sensor reading commands
  } else if (action == "config") {
    handleConfigCommand(doc);   // Handle configuration commands
  } else if (action == "status") {
    handleStatusCommand();      // Handle status query
  } else {
    sendError("Unknown action: " + action);  // Unknown command
  }
}

/**
 * RGB STRIP COMMAND HANDLER
 * Handles all RGB LED strip operations including:
 * - Single pixel control
 * - Range of pixels control
 * - Entire strip control
 * - Strip clearing
 * 
 * @param doc Reference to the JSON document containing command parameters
 */
void handleRgbCommand(JsonDocument& doc) {
  // Extract parameters from JSON with default values
  int strip = doc["strip"] | 0;           // Which strip (1 or 2)
  String mode = doc["mode"] | "single";   // Operation mode
  
  // ================================
  // VALIDATE STRIP NUMBER
  mode.toLowerCase();
  // ================================
  if (strip != 1 && strip != 2) {
    sendError("Invalid strip number. Use 1 or 2");
    return;
  }
  
  // Get pointer to the correct NeoPixel strip object
  Adafruit_NeoPixel* currentStrip = (strip == 1) ? rgb1 : rgb2;
  
  // ================================
  // HANDLE DIFFERENT OPERATION MODES
  // ================================
  
  if (mode == "single") {
    // ================================
    // SINGLE PIXEL CONTROL
    // ================================
    stopRgbEffect(strip, false);
    // Extract pixel number and RGB values
    int pixel = doc["pixel"] | 0;  // Pixel index (0-77)
    int r = doc["r"] | 0;          // Red value (0-255)
    int g = doc["g"] | 0;          // Green value (0-255)
    int b = doc["b"] | 0;          // Blue value (0-255)
    int brightness = doc["brightness"] | (strip == 1 ? brightnessRgb1 : brightnessRgb2); // Brightness (0-255)
    
    // Validate pixel number is within strip range
    if (pixel >= 0 && pixel < currentStrip->numPixels()) {
      if (!isByteValue(r) || !isByteValue(g) || !isByteValue(b)) {
        sendError("Invalid RGB values. Range: 0-255");
        return;
      }
      if (!isByteValue(brightness)) {
        sendError("Invalid brightness value. Range: 0-255");
        return;
      }
      currentStrip->setPixelColor(pixel, makeScaledColor(currentStrip, r, g, b, brightness));
      showStrip(currentStrip);
      delay(5);  // Small delay to ensure LED update completes
      sendSuccess("Pixel " + String(pixel) + " on strip " + String(strip) + " set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ") with brightness " + String(brightness));
    } else {
      sendError("Invalid pixel number. Range: 0-" + String(currentStrip->numPixels() - 1));
    }
    
  } else if (mode == "range") {
    // ================================
    // PIXEL RANGE CONTROL
    // ================================
    stopRgbEffect(strip, false);
    // Extract range parameters and RGB values
    int start = doc["start"] | 0;                           // Start pixel
    int end = doc["end"] | (currentStrip->numPixels() - 1); // End pixel
    int r = doc["r"] | 0;                                   // Red value
    int g = doc["g"] | 0;                                   // Green value
    int b = doc["b"] | 0;                                   // Blue value
    int brightness = doc["brightness"] | (strip == 1 ? brightnessRgb1 : brightnessRgb2); // Brightness (0-255)

    // Validate range parameters
    if (start >= 0 && end < currentStrip->numPixels() && start <= end) {
      if (!isByteValue(r) || !isByteValue(g) || !isByteValue(b)) {
        sendError("Invalid RGB values. Range: 0-255");
        return;
      }
      if (!isByteValue(brightness)) {
        sendError("Invalid brightness value. Range: 0-255");
        return;
      }
      uint32_t color = makeScaledColor(currentStrip, r, g, b, brightness);
      // Set color for all pixels in the range
      for (int i = start; i <= end; i++) {
        currentStrip->setPixelColor(i, color);
      }
      showStrip(currentStrip);
      delay(10);  // Longer delay for range operations
      sendSuccess("Pixels " + String(start) + "-" + String(end) + " on strip " + String(strip) + " set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ") with brightness " + String(brightness));
    } else {
      sendError("Invalid pixel range");
    }
    
  } else if (mode == "all") {
    // ================================
    // ALL PIXELS CONTROL
    // ================================
    stopRgbEffect(strip, false);
    // Extract RGB values
    int r = doc["r"] | 0;  // Red value
    int g = doc["g"] | 0;  // Green value
    int b = doc["b"] | 0;  // Blue value
    int brightness = doc["brightness"] | (strip == 1 ? brightnessRgb1 : brightnessRgb2); // Brightness (0-255)

    if (!isByteValue(r) || !isByteValue(g) || !isByteValue(b)) {
      sendError("Invalid RGB values. Range: 0-255");
      return;
    }
    if (!isByteValue(brightness)) {
      sendError("Invalid brightness value. Range: 0-255");
      return;
    }
    uint32_t color = makeScaledColor(currentStrip, r, g, b, brightness);
    // Set all pixels to the same color
    for (int i = 0; i < currentStrip->numPixels(); i++) {
      currentStrip->setPixelColor(i, color);
    }
    showStrip(currentStrip);
    delay(15);  // Longer delay for full strip operations
    sendSuccess("All pixels on strip " + String(strip) + " set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ") with brightness " + String(brightness));
    
  } else if (mode == "clear") {
    // ================================
    // CLEAR STRIP (TURN OFF ALL LEDS)
    // ================================
    stopRgbEffect(strip, false);
    currentStrip->clear();  // Set all pixels to black (off)
    showStrip(currentStrip);
    delay(10);  // Ensure clear operation completes
    sendSuccess("Strip " + String(strip) + " cleared");

  } else if (mode == "default" || mode == "stop") {
    // ================================
    // RESTORE DEFAULT STRIP STATE
    // ================================
    stopRgbEffect(strip, true);
    sendSuccess("Strip " + String(strip) + " restored to default state");

  } else if (mode == "running" || mode == "run" || mode == "chase") {
    startRgbEffect(doc, strip, RGB_EFFECT_RUNNING);

  } else if (mode == "charging" || mode == "charge") {
    startRgbEffect(doc, strip, RGB_EFFECT_CHARGING);

  } else if (mode == "center" || mode == "center_fill" || mode == "both_ends") {
    startRgbEffect(doc, strip, RGB_EFFECT_CENTER);

  } else if (mode == "rainbow") {
    startRgbEffect(doc, strip, RGB_EFFECT_RAINBOW);

  } else if (mode == "flash" || mode == "blink") {
    startRgbEffect(doc, strip, RGB_EFFECT_FLASH);

  } else if (mode == "random" || mode == "sparkle") {
    startRgbEffect(doc, strip, RGB_EFFECT_RANDOM);

  } else if (mode == "breathing" || mode == "breathe") {
    startRgbEffect(doc, strip, RGB_EFFECT_BREATHING);
    
  } else {
    // ================================
    // INVALID MODE
    // ================================
    sendError("Invalid mode. Use: single, range, all, clear, default, running, charging, center, rainbow, flash, random, breathing");
  }
}

/**
 * SINGLE LED COMMAND HANDLER
 * Controls the single LED connected to ledPin
 * Supports both digital (on/off) and analog (PWM brightness) control
 * 
 * @param doc Reference to the JSON document containing command parameters
 */
void handleLedCommand(JsonDocument& doc) {
  // Extract mode parameter (default to "digital")
  String mode = doc["mode"] | "digital";
  
  if (mode == "digital") {
    // ================================
    // DIGITAL MODE (ON/OFF CONTROL)
    // ================================
    bool state = doc["state"] | false;  // Extract state (true=ON, false=OFF)
    digitalWrite(ledPin, state ? HIGH : LOW);  // Set LED state
    pwmActive = false;  // PWM is no longer active in digital mode
    sendSuccess("LED set to " + String(state ? "ON" : "OFF"));
    
  } else if (mode == "analog") {
    // ================================
    // ANALOG MODE (PWM BRIGHTNESS CONTROL)
    // ================================
    int value = doc["value"] | 0;  // Extract brightness value (0-255)
    
    // Validate brightness value range
    if (value >= 0 && value <= 255) {
      analogWrite(ledPin, value);  // Set PWM brightness (0=off, 255=full brightness)
      lastPwmValue = value;  // Track current PWM value
      pwmActive = (value > 0);  // PWM is active if value > 0
      sendSuccess("LED analog value set to " + String(value));
    } else {
      sendError("Invalid analog value. Range: 0-255");
    }
    
  } else {
    // ================================
    // INVALID MODE
    // ================================
    sendError("Invalid LED mode. Use: digital, analog");
  }
}

/**
 * RELAY COMMAND HANDLER
 * Controls relay outputs for switching external devices
 * Supports independent control of both relays
 * 
 * @param doc Reference to the JSON document containing command parameters
 */
void handleRelayCommand(JsonDocument& doc) {
  // Extract relay number and desired state
  int relay = doc["relay"] | 0;     // Which relay (1 or 2)
  bool state = doc["state"] | false; // Desired state (true=ON, false=OFF)
  
  if (relay == 1) {
    // ================================
    // CONTROL RELAY 1 (INTERCOM)
    // ================================
    digitalWrite(relay1, state ? HIGH : LOW);  // Set relay 1 state
    sendSuccess("Relay 1 set to " + String(state ? "ON" : "OFF"));
    
  } else if (relay == 2) {
    // ================================
    // CONTROL RELAY 2 (GENERAL PURPOSE)
    // ================================
    digitalWrite(relay2, state ? HIGH : LOW);  // Set relay 2 state
    sendSuccess("Relay 2 set to " + String(state ? "ON" : "OFF"));
    
  } else {
    // ================================
    // INVALID RELAY NUMBER
    // ================================
    sendError("Invalid relay number. Use 1 or 2");
  }
}

/**
 * SENSOR READING COMMAND HANDLER
 * Reads values from connected sensors:
 * - LB (Paper Full Sensor): Analog input with digital threshold option
 * - RS (Ticket Barrier): Digital input only
 * 
 * @param doc Reference to the JSON document containing command parameters
 */
void handleReadCommand(JsonDocument& doc) {
  // Extract which sensor to read
  String sensor = doc["sensor"] | "";

  if (sensor == "temp") {
    // ================================
    // LM75 TEMPERATURE SENSOR
    // ================================
    if (!lm75Available()) {
      sendError("LM75 not responding at 0x48");
      return;
    }
    float t = readLm75Temperature();
    if (isnan(t)) {
      sendError("LM75 read error");
      return;
    }
    JsonDocument response;
    response["status"] = "success";
    response["sensor"] = "temp";
    response["celsius"] = t;
    response["resolution"] = "0.5";  // LM75A 9-bit => 0.5°C steps
    response["address"] = "0x48";
    serializeJson(response, Serial);
    Serial.println();
    return;  // Do not continue to other branches
  }
  
  if (sensor == "lb") {
    // ================================
    // LB SENSOR (PAPER FULL SENSOR)
    // ================================
    String mode = doc["mode"] | "analog";  // Reading mode (analog or digital)
    
    if (mode == "analog") {
      // ================================
      // ANALOG MODE - RAW VALUE (0-1023)
      // ================================
      int value = analogRead(LB);  // Read raw analog value
      
      // Create and send JSON response with sensor data
      JsonDocument response;
      response["status"] = "success";
      response["sensor"] = "lb";
      response["mode"] = "analog";
      response["value"] = value;           // Raw analog value
      response["range"] = "0-1023";        // Valid range information
      serializeJson(response, Serial);
      Serial.println();
      
    } else if (mode == "digital") {
      // ================================
      // DIGITAL MODE - THRESHOLD COMPARISON
      // ================================
      int value = analogRead(LB);           // Read raw analog value
      bool digital = value > lbThreshold;   // Compare with threshold
      
      // Create and send JSON response with both digital and raw values
      JsonDocument response;
      response["status"] = "success";
      response["sensor"] = "lb";
      response["mode"] = "digital";
      response["value"] = digital ? 1 : 0;  // Digital result (0 or 1)
      response["threshold"] = lbThreshold;  // Current threshold setting
      response["raw_value"] = value;        // Include raw value for reference
      serializeJson(response, Serial);
      Serial.println();
      
    } else {
      // ================================
      // INVALID LB MODE
      // ================================
      sendError("Invalid LB mode. Use: analog, digital");
    }
    
  } else if (sensor == "rs") {
    // ================================
    // RS SENSOR (TICKET BARRIER)
    // ================================
    bool state = digitalRead(RS);  // Read digital state (HIGH/LOW)
    
    // Create and send JSON response
    JsonDocument response;
    response["status"] = "success";
    response["sensor"] = "rs";
    response["value"] = state ? 1 : 0;  // Convert boolean to 1/0
    serializeJson(response, Serial);
    Serial.println();
    
  } else {
    // ================================
    // INVALID SENSOR NAME
    // ================================
    sendError("Invalid sensor. Use: lb, rs");
  }
}

/**
 * CONFIGURATION COMMAND HANDLER
 * Handles system configuration changes
 * Currently supports setting the LB sensor threshold
 * 
 * @param doc Reference to the JSON document containing command parameters
 */
void handleConfigCommand(JsonDocument& doc) {
  // Extract configuration setting name
  String setting = doc["setting"] | "";
  
  if (setting == "lb_threshold") {
    // ================================
    // SET LB SENSOR THRESHOLD
    // ================================
    int threshold = doc["value"] | lbThreshold;  // Extract new threshold value
    
    // Validate threshold is within valid analog range
    if (threshold >= 0 && threshold <= 1023) {
      lbThreshold = threshold;  // Update global threshold variable
      saveEepromSettings();     // Save to EEPROM
      sendSuccess("LB threshold set to " + String(lbThreshold));
    } else {
      sendError("Invalid threshold value. Range: 0-1023");
    }
    
  } else if (setting == "rgb1_pixels") {
    // ================================
    // SET NUMBER OF PIXELS IN RGB STRIP 1
    // ================================
    int pixels = doc["value"] | numLedsRgb1;
    
    if (pixels >= 1 && pixels <= 1000) {
      stopRgbEffect(1, false);
      numLedsRgb1 = pixels;
      rgb1->updateLength(numLedsRgb1);
      rgb1->setBrightness(NEOPIXEL_LIBRARY_BRIGHTNESS);
      rgb1->clear();
      showStrip(rgb1);
      saveEepromSettings();
      sendSuccess("RGB1 pixels set to " + String(numLedsRgb1));
    } else {
      sendError("Invalid pixel count. Range: 1-1000");
    }
    
  } else if (setting == "rgb2_pixels") {
    // ================================
    // SET NUMBER OF PIXELS IN RGB STRIP 2
    // ================================
    int pixels = doc["value"] | numLedsRgb2;
    
    if (pixels >= 1 && pixels <= 1000) {
      stopRgbEffect(2, false);
      numLedsRgb2 = pixels;
      rgb2->updateLength(numLedsRgb2);
      rgb2->setBrightness(NEOPIXEL_LIBRARY_BRIGHTNESS);
      rgb2->clear();
      showStrip(rgb2);
      saveEepromSettings();
      sendSuccess("RGB2 pixels set to " + String(numLedsRgb2));
    } else {
      sendError("Invalid pixel count. Range: 1-1000");
    }
    
  } else if (setting == "rgb1_default_color") {
    // ================================
    // SET DEFAULT COLOR FOR RGB STRIP 1
    // ================================
    int r = doc["r"] | defaultR1;
    int g = doc["g"] | defaultG1;
    int b = doc["b"] | defaultB1;
    
    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
      defaultR1 = r;
      defaultG1 = g;
      defaultB1 = b;
      saveEepromSettings();
      sendSuccess("RGB1 default color set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    } else {
      sendError("Invalid RGB values. Range: 0-255");
    }
    
  } else if (setting == "rgb2_default_color") {
    // ================================
    // SET DEFAULT COLOR FOR RGB STRIP 2
    // ================================
    int r = doc["r"] | defaultR2;
    int g = doc["g"] | defaultG2;
    int b = doc["b"] | defaultB2;
    
    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
      defaultR2 = r;
      defaultG2 = g;
      defaultB2 = b;
      saveEepromSettings();
      sendSuccess("RGB2 default color set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    } else {
      sendError("Invalid RGB values. Range: 0-255");
    }
    
  } else if (setting == "rgb1_brightness") {
    // ================================
    // SET BRIGHTNESS FOR RGB STRIP 1
    // ================================
    int brightness = doc["value"] | brightnessRgb1;
    
    if (brightness >= 0 && brightness <= 255) {
      brightnessRgb1 = brightness;
      saveEepromSettings();
      sendSuccess("RGB1 brightness set to " + String(brightness));
    } else {
      sendError("Invalid brightness value. Range: 0-255");
    }
    
  } else if (setting == "rgb2_brightness") {
    // ================================
    // SET BRIGHTNESS FOR RGB STRIP 2
    // ================================
    int brightness = doc["value"] | brightnessRgb2;
    
    if (brightness >= 0 && brightness <= 255) {
      brightnessRgb2 = brightness;
      saveEepromSettings();
      sendSuccess("RGB2 brightness set to " + String(brightness));
    } else {
      sendError("Invalid brightness value. Range: 0-255");
    }
    
  } else if (setting == "brightness") {
    // ================================
    // SET BRIGHTNESS FOR BOTH RGB STRIPS
    // ================================
    int brightness = doc["value"] | brightnessRgb1;
    
    if (brightness >= 0 && brightness <= 255) {
      brightnessRgb1 = brightness;
      brightnessRgb2 = brightness;
      saveEepromSettings();
      sendSuccess("Both RGB strips brightness set to " + String(brightness));
    } else {
      sendError("Invalid brightness value. Range: 0-255");
    }
    
  } else if (setting == "led_default") {
    // ================================
    // SET DEFAULT LED BRIGHTNESS
    // ================================
    int brightness = doc["value"] | ledDefaultBrightness;
    
    if (brightness >= 0 && brightness <= 255) {
      ledDefaultBrightness = brightness;
      saveEepromSettings();
      analogWrite(ledPin, ledDefaultBrightness); // Apply immediately
      lastPwmValue = ledDefaultBrightness;
      pwmActive = (ledDefaultBrightness > 0);
      sendSuccess("LED default brightness set to " + String(ledDefaultBrightness));
    } else {
      sendError("Invalid brightness value. Range: 0-255");
    }

  } else if (setting == "running_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_RUNNING);

  } else if (setting == "charging_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_CHARGING);

  } else if (setting == "center_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_CENTER);

  } else if (setting == "rainbow_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_RAINBOW);

  } else if (setting == "flash_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_FLASH);

  } else if (setting == "random_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_RANDOM);

  } else if (setting == "breathing_default") {
    updateRgbEffectDefaults(doc, RGB_EFFECT_BREATHING);

  } else if (setting == "startup_mode") {
    int strip = doc["strip"] | 0;
    String value = doc["value"] | "";
    value.toLowerCase();

    if (strip != 1 && strip != 2) {
      sendError("Invalid strip number. Use 1 or 2");
      return;
    }

    if (value == "solid") {
      startupMode[strip - 1] = STARTUP_SOLID;
      saveEepromSettings();
      sendSuccess("Startup mode for strip " + String(strip) + " set to solid");
    } else if (value == "effect") {
      startupMode[strip - 1] = STARTUP_EFFECT;
      saveEepromSettings();
      sendSuccess("Startup mode for strip " + String(strip) + " set to effect");
    } else {
      sendError("Invalid startup mode. Use: solid, effect");
    }

  } else if (setting == "startup_effect") {
    int strip = doc["strip"] | 0;
    String value = doc["value"] | "";
    value.toLowerCase();

    if (strip != 1 && strip != 2) {
      sendError("Invalid strip number. Use 1 or 2");
      return;
    }

    RgbEffectMode effect = RGB_EFFECT_RAINBOW;
    if (value == "running" || value == "run" || value == "chase") {
      effect = RGB_EFFECT_RUNNING;
    } else if (value == "charging" || value == "charge") {
      effect = RGB_EFFECT_CHARGING;
    } else if (value == "center" || value == "center_fill" || value == "both_ends") {
      effect = RGB_EFFECT_CENTER;
    } else if (value == "rainbow") {
      effect = RGB_EFFECT_RAINBOW;
    } else if (value == "flash" || value == "blink") {
      effect = RGB_EFFECT_FLASH;
    } else if (value == "random" || value == "sparkle") {
      effect = RGB_EFFECT_RANDOM;
    } else if (value == "breathing" || value == "breathe") {
      effect = RGB_EFFECT_BREATHING;
    } else {
      sendError("Invalid startup effect. Use: running, charging, center_fill, rainbow, flash, random, breathing");
      return;
    }

    startupEffect[strip - 1] = effect;
    saveEepromSettings();
    sendSuccess("Startup effect for strip " + String(strip) + " set to " + String(rgbEffectName(effect)));

  } else if (setting == "auto_reset_12h") {
    String value = doc["value"] | "";
    value.toLowerCase();

    bool enable;
    if (doc["value"].is<bool>()) {
      enable = doc["value"].as<bool>();
    } else if (value == "activate" || value == "active" || value == "on" || value == "true" || value == "enable" || value == "enabled") {
      enable = true;
    } else if (value == "deactivate" || value == "inactive" || value == "off" || value == "false" || value == "disable" || value == "disabled") {
      enable = false;
    } else {
      sendError("Invalid auto reset value. Use: activate, deactivate, on, off, true, false");
      return;
    }

    autoReset12hEnabled = enable;
    autoReset12hStartMs = millis();
    saveEepromSettings();
    sendSuccess(String("12h auto reset ") + (autoReset12hEnabled ? "activated" : "deactivated"));
    
  } else {
    // ================================
    // INVALID CONFIGURATION SETTING
    // ================================
    sendError("Invalid setting. Available: lb_threshold, rgb1_pixels, rgb2_pixels, rgb1_default_color, rgb2_default_color, rgb1_brightness, rgb2_brightness, brightness, led_default, running_default, charging_default, center_default, rainbow_default, flash_default, random_default, breathing_default, startup_mode, startup_effect, auto_reset_12h");
  }
}

/**
 * STATUS COMMAND HANDLER
 * Returns all current configuration values as JSON
 * Useful for debugging EEPROM persistence and verifying settings
 */
void handleStatusCommand() {
  JsonDocument response;
  response["status"] = "success";
  response["rgb1_pixels"] = numLedsRgb1;
  response["rgb2_pixels"] = numLedsRgb2;
  response["rgb1_actual_pixels"] = (int)rgb1->numPixels();
  response["rgb2_actual_pixels"] = (int)rgb2->numPixels();
  response["rgb1_brightness"] = brightnessRgb1;
  response["rgb2_brightness"] = brightnessRgb2;
  response["rgb1_default_r"] = defaultR1;
  response["rgb1_default_g"] = defaultG1;
  response["rgb1_default_b"] = defaultB1;
  response["rgb2_default_r"] = defaultR2;
  response["rgb2_default_g"] = defaultG2;
  response["rgb2_default_b"] = defaultB2;
  response["lb_threshold"] = lbThreshold;
  response["led_default"] = ledDefaultBrightness;
  response["pwm_active"] = pwmActive;
  response["pwm_value"] = lastPwmValue;
  response["rgb1_effect"] = rgbAnimationState[0].active ? rgbEffectName(rgbAnimationState[0].mode) : "none";
  response["rgb2_effect"] = rgbAnimationState[1].active ? rgbEffectName(rgbAnimationState[1].mode) : "none";
  response["rgb1_effect_active"] = rgbAnimationState[0].active;
  response["rgb2_effect_active"] = rgbAnimationState[1].active;
  response["rgb1_effect_repeats"] = rgbAnimationState[0].repeatsRemaining;
  response["rgb2_effect_repeats"] = rgbAnimationState[1].repeatsRemaining;
  response["rgb1_effect_interval"] = rgbAnimationState[0].intervalMs;
  response["rgb2_effect_interval"] = rgbAnimationState[1].intervalMs;
  response["startup_mode_rgb1"] = startupModeName(startupMode[0]);
  response["startup_mode_rgb2"] = startupModeName(startupMode[1]);
  response["startup_effect_rgb1"] = rgbEffectName(startupEffect[0]);
  response["startup_effect_rgb2"] = rgbEffectName(startupEffect[1]);
  response["version"] = "1.1.0";
  response["auto_reset_12h_enabled"] = autoReset12hEnabled;
  uint32_t autoResetElapsed = (uint32_t)(millis() - autoReset12hStartMs);
  response["auto_reset_12h_remaining_ms"] = autoReset12hEnabled ? (autoResetElapsed >= AUTO_RESET_12H_MS ? 0 : (AUTO_RESET_12H_MS - autoResetElapsed)) : 0;
  serializeJson(response, Serial);
  Serial.println();
}

void sendSuccess(String message) {
  JsonDocument response;
  response["status"] = "success";
  response["message"] = message;
  serializeJson(response, Serial);  // Send JSON to serial
  Serial.println();                 // Add newline
}

/**
 * ERROR RESPONSE SENDER
 * Sends a standardized JSON error response
 * 
 * @param message Error message to include in response
 */
void sendError(String message) {
  JsonDocument response;
  response["status"] = "error";
  response["message"] = message;
  serializeJson(response, Serial);  // Send JSON to serial
  Serial.println();                 // Add newline
}

/**
 * HELP DOCUMENTATION DISPLAY
 * Shows complete API documentation with examples for all available commands
 * Called when user types "help" in the serial console
 */
void showHelp() {
  Serial.println();
  Serial.println("=== LED Controller API v1.1.0 Documentation ===");
  Serial.println();
  Serial.println("All commands use JSON format. Examples:");
  Serial.println();
  
  // ================================
  // RGB STRIP CONTROL EXAMPLES
  // ================================
  Serial.println("1. RGB STRIP CONTROL:");
  Serial.println("   Single pixel: {\"action\":\"rgb\",\"strip\":1,\"mode\":\"single\",\"pixel\":0,\"r\":255,\"g\":0,\"b\":0,\"brightness\":128}");
  Serial.println("   Range:        {\"action\":\"rgb\",\"strip\":1,\"mode\":\"range\",\"start\":0,\"end\":9,\"r\":0,\"g\":255,\"b\":0,\"brightness\":200}");
  Serial.println("   All pixels:   {\"action\":\"rgb\",\"strip\":2,\"mode\":\"all\",\"r\":0,\"g\":0,\"b\":255,\"brightness\":255}");
  Serial.println("   Clear strip:  {\"action\":\"rgb\",\"strip\":1,\"mode\":\"clear\"}");
  Serial.println("   Running:      {\"action\":\"rgb\",\"strip\":1,\"mode\":\"running\",\"r\":255,\"g\":120,\"b\":40,\"brightness\":180,\"count\":12,\"time\":40,\"repeatingtime\":0}");
  Serial.println("   Charging:     {\"action\":\"rgb\",\"strip\":1,\"mode\":\"charging\",\"r\":0,\"g\":180,\"b\":255,\"brightness\":200,\"time\":35,\"repeatingtime\":0}");
  Serial.println("   Center fill:  {\"action\":\"rgb\",\"strip\":1,\"mode\":\"center_fill\",\"r\":0,\"g\":255,\"b\":120,\"brightness\":180,\"time\":35,\"repeatingtime\":0}");
  Serial.println("   Rainbow:      {\"action\":\"rgb\",\"strip\":1,\"mode\":\"rainbow\",\"brightness\":180,\"time\":20,\"repeatingtime\":0}");
  Serial.println("   Flash:        {\"action\":\"rgb\",\"strip\":1,\"mode\":\"flash\",\"r\":255,\"g\":255,\"b\":255,\"brightness\":255,\"time\":250,\"repeatingtime\":0}");
  Serial.println("   Random:       {\"action\":\"rgb\",\"strip\":1,\"mode\":\"random\",\"brightness\":255,\"time\":80,\"repeatingtime\":0}");
  Serial.println("   Breathing:    {\"action\":\"rgb\",\"strip\":1,\"mode\":\"breathing\",\"r\":255,\"g\":120,\"b\":40,\"brightness\":180,\"time\":25,\"repeatingtime\":0}");
  Serial.println("   Stop/Default: {\"action\":\"rgb\",\"strip\":1,\"mode\":\"default\"}");
  Serial.println();
  
  // ================================
  // LED CONTROL EXAMPLES
  // ================================
  Serial.println("2. LED CONTROL:");
  Serial.println("   Digital:      {\"action\":\"led\",\"mode\":\"digital\",\"state\":true}");
  Serial.println("   Analog:       {\"action\":\"led\",\"mode\":\"analog\",\"value\":128}");
  Serial.println();
  
  // ================================
  // RELAY CONTROL EXAMPLES
  // ================================
  Serial.println("3. RELAY CONTROL:");
  Serial.println("   Relay 1 ON:   {\"action\":\"relay\",\"relay\":1,\"state\":true}");
  Serial.println("   Relay 2 OFF:  {\"action\":\"relay\",\"relay\":2,\"state\":false}");
  Serial.println();
  
  // ================================
  // SENSOR READING EXAMPLES
  // ================================
  Serial.println("4. SENSOR READING:");
  Serial.println("   LB analog:    {\"action\":\"read\",\"sensor\":\"lb\",\"mode\":\"analog\"}");
  Serial.println("   LB digital:   {\"action\":\"read\",\"sensor\":\"lb\",\"mode\":\"digital\"}");
  Serial.println("   RS state:     {\"action\":\"read\",\"sensor\":\"rs\"}");
  Serial.println("   Temp LM75:    {\"action\":\"read\",\"sensor\":\"temp\"}");
  Serial.println();
  
  // ================================
  // CONFIGURATION EXAMPLES
  // ================================
  Serial.println("5. CONFIGURATION:");
  Serial.println("   Set threshold:     {\"action\":\"config\",\"setting\":\"lb_threshold\",\"value\":600}");
  Serial.println("   Set RGB1 pixels:   {\"action\":\"config\",\"setting\":\"rgb1_pixels\",\"value\":100}  (applies immediately)");
  Serial.println("   Set RGB2 pixels:   {\"action\":\"config\",\"setting\":\"rgb2_pixels\",\"value\":100}  (applies immediately)");
  Serial.println("   Set RGB1 default:  {\"action\":\"config\",\"setting\":\"rgb1_default_color\",\"r\":255,\"g\":255,\"b\":255}");
  Serial.println("   Set RGB2 default:  {\"action\":\"config\",\"setting\":\"rgb2_default_color\",\"r\":255,\"g\":255,\"b\":255}");
  Serial.println("   Set RGB1 bright:   {\"action\":\"config\",\"setting\":\"rgb1_brightness\",\"value\":128}");
  Serial.println("   Set RGB2 bright:   {\"action\":\"config\",\"setting\":\"rgb2_brightness\",\"value\":200}");
  Serial.println("   Set LED default:   {\"action\":\"config\",\"setting\":\"led_default\",\"value\":128}");
  Serial.println("   Running default:   {\"action\":\"config\",\"setting\":\"running_default\",\"strip\":1,\"r\":255,\"g\":120,\"b\":40,\"brightness\":180,\"time\":40,\"repeatingtime\":0,\"count\":12}");
  Serial.println("   Charging default:  {\"action\":\"config\",\"setting\":\"charging_default\",\"strip\":1,\"r\":0,\"g\":180,\"b\":255,\"brightness\":200,\"time\":35,\"repeatingtime\":0}");
  Serial.println("   Center default:    {\"action\":\"config\",\"setting\":\"center_default\",\"strip\":1,\"r\":0,\"g\":255,\"b\":120,\"brightness\":180,\"time\":35,\"repeatingtime\":0}");
  Serial.println("   Rainbow default:   {\"action\":\"config\",\"setting\":\"rainbow_default\",\"strip\":1,\"brightness\":180,\"time\":20,\"repeatingtime\":0}");
  Serial.println("   Flash default:     {\"action\":\"config\",\"setting\":\"flash_default\",\"strip\":1,\"r\":255,\"g\":255,\"b\":255,\"brightness\":255,\"time\":250,\"repeatingtime\":0}");
  Serial.println("   Random default:    {\"action\":\"config\",\"setting\":\"random_default\",\"strip\":1,\"brightness\":255,\"time\":80,\"repeatingtime\":0}");
  Serial.println("   Breathing default: {\"action\":\"config\",\"setting\":\"breathing_default\",\"strip\":1,\"r\":255,\"g\":120,\"b\":40,\"brightness\":180,\"time\":25,\"repeatingtime\":0}");
  Serial.println("   Startup mode:      {\"action\":\"config\",\"setting\":\"startup_mode\",\"strip\":1,\"value\":\"effect\"}");
  Serial.println("   Startup effect:    {\"action\":\"config\",\"setting\":\"startup_effect\",\"strip\":1,\"value\":\"rainbow\"}");
  Serial.println("   12h auto reset:    {\"action\":\"config\",\"setting\":\"auto_reset_12h\",\"value\":\"activate\"}");
  Serial.println("                      {\"action\":\"config\",\"setting\":\"auto_reset_12h\",\"value\":\"deactivate\"}");
  Serial.println();
  
  // ================================
  // PARAMETER EXPLANATIONS
  // ================================
  Serial.println("PARAMETERS:");
  Serial.println("- strip: 1 (Ring-Top) or 2 (Door)");
  Serial.println("- pixel: 0 to (pixels-1) per strip");
  Serial.println("- r,g,b: 0-255 (RGB color values)");
  Serial.println("- brightness: 0-255 (optional, defaults to the configured strip brightness)");
  Serial.println("- count: running-light length in LEDs");
  Serial.println("- time: animation step interval in milliseconds");
  Serial.println("- repeatingtime: repeat count, 0 = infinite until the next command");
  Serial.println("- value: 0-255 (analog LED brightness)");
  Serial.println("- state: true/false");
  Serial.println("- lb_threshold: 0-1023 (analog threshold for digital mode)");
  Serial.println("- rgbX_pixels: 1-1000 (number of LEDs in strip, applies immediately)");
  Serial.println("- rgbX_default_color: RGB values for startup color");
  Serial.println("- rgbX_brightness: 0-255 (configured default brightness for RGB commands)");
  Serial.println("- led_default: 0-255 (default LED brightness at startup)");
  Serial.println("- effect defaults: running_default, charging_default, center_default, rainbow_default, flash_default, random_default, breathing_default");
  Serial.println("- startup_mode: solid or effect (per strip)");
  Serial.println("- startup_effect: running, charging, center_fill, rainbow, flash, random, breathing (per strip)");
  Serial.println("- auto_reset_12h: activate or deactivate a periodic 12-hour controller reset");
  Serial.println();
  
  // ================================
  // RESPONSE FORMAT INFO
  // ================================
  Serial.println("6. STATUS:");
  Serial.println("   Get config:   {\"action\":\"status\"}");
  Serial.println();
  Serial.println("All responses are in JSON format with 'status' field.");
  Serial.println("Ready for commands...");
  Serial.println();
}



/**
 * SETUP FUNCTION
 * Runs once when the microcontroller starts
 * Initializes all hardware components and serial communication
 */
void setup()
{
  // ================================
  // LOAD EEPROM SETTINGS
  // ================================
  loadEepromSettings();
  
  // ================================
  // SERIAL COMMUNICATION SETUP
  // ================================
  Serial.begin(115200);  // Start serial communication at 115200 baud rate
  Serial.println();      // Print empty line for clarity
  Serial.println("=== LED Controller API v1.1.0 ===");
  Serial.println("Type 'help' for available commands");
  Serial.println("Config: RGB1=" + String(numLedsRgb1) + "px, RGB2=" + String(numLedsRgb2) + "px");
  Serial.println("Colors: RGB1=(" + String(defaultR1) + "," + String(defaultG1) + "," + String(defaultB1) + ") RGB2=(" + String(defaultR2) + "," + String(defaultG2) + "," + String(defaultB2) + ")");
  Serial.println("Brightness defaults: RGB1=" + String(brightnessRgb1) + " RGB2=" + String(brightnessRgb2) + " LED=" + String(ledDefaultBrightness));
  Serial.println("Startup: strip1=" + String(startupModeName(startupMode[0])) + "/" + String(rgbEffectName(startupEffect[0])) + " strip2=" + String(startupModeName(startupMode[1])) + "/" + String(rgbEffectName(startupEffect[1])));
  Serial.println("Auto reset: " + String(autoReset12hEnabled ? "enabled" : "disabled") + " (12h)");
  Serial.println("Animations: running, charging, center_fill, rainbow, flash, random, breathing");
  Serial.println("Ready for JSON commands...");
  autoReset12hStartMs = millis();
  
  // ================================
  // PIN INITIALIZATION
  // ================================
  // Set pin modes for outputs (LEDs and relays)
  pinMode(ledPin, OUTPUT);  // Single LED pin as output
  pinMode(relay1, OUTPUT);  // Relay 1 pin as output
  pinMode(relay2, OUTPUT);  // Relay 2 pin as output
  analogWriteRange(255);    // Match the program's 0-255 LED brightness API
  analogWriteFreq(200);     // Lower PWM frequency to reduce visible flicker during NeoPixel updates
  randomSeed(micros());     // Seed random sparkle effects
  
  // Set pin modes for inputs (sensors)
  pinMode(RS, INPUT);       // Ticket Barrier sensor as digital input
  pinMode(LB, INPUT);       // Paper Full sensor as analog input
  
  // ================================
  // NEOPIXEL STRIP INITIALIZATION
  // ================================
  // Initialize RGB strip #1 (Ring-Top) with loaded pixel count
  rgb1 = new Adafruit_NeoPixel(numLedsRgb1, RGB1, NEO_GRB + NEO_KHZ800);
  rgb1->begin();   // Prepare the NeoPixel library
  rgb1->setBrightness(NEOPIXEL_LIBRARY_BRIGHTNESS);  // Keep library brightness neutral
  for (int i = 0; i < rgb1->numPixels(); i++) {
    rgb1->setPixelColor(i, makeScaledColor(rgb1, defaultR1, defaultG1, defaultB1, brightnessRgb1));  // Set all pixels to default color
  }
  showStrip(rgb1);   // Apply the changes
  
  // Initialize RGB strip #2 (Door) with loaded pixel count
  rgb2 = new Adafruit_NeoPixel(numLedsRgb2, RGB2, NEO_GRB + NEO_KHZ800);
  rgb2->begin();   // Prepare the NeoPixel library
  rgb2->setBrightness(NEOPIXEL_LIBRARY_BRIGHTNESS);  // Keep library brightness neutral
  for (int i = 0; i < rgb2->numPixels(); i++) {
    rgb2->setPixelColor(i, makeScaledColor(rgb2, defaultR2, defaultG2, defaultB2, brightnessRgb2));  // Set all pixels to default color
  }
  showStrip(rgb2);   // Apply the changes
  
  // ================================
  // SET DEFAULT HARDWARE STATES
  // ================================
  digitalWrite(relay1, HIGH);  // Turn relay 1 OFF (HIGH default state)
  digitalWrite(relay2, HIGH);  // Turn relay 2 OFF (HIGH default state)
  analogWrite(ledPin, ledDefaultBrightness);  // Set LED to default brightness
  lastPwmValue = ledDefaultBrightness;  // Track initial PWM value
  pwmActive = (ledDefaultBrightness > 0);  // PWM is active if default brightness > 0

  applyStartupState(1);
  applyStartupState(2);
  
  // ================================
  // SERIAL BUFFER SETUP
  // ================================
  inputBuffer.reserve(200);   // Pre-allocate 200 bytes for serial input buffer

  // ================================
  // I2C (LM75) INITIALIZATION
  // ================================
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);  // Increased delay for LM75 initialization
  if (lm75Available()) {
    Serial.println("LM75 detected at 0x48");
  } else {
    Serial.println("LM75 not detected (optional sensor)");
  }
}

/**
 * MAIN LOOP FUNCTION
 * Runs continuously after setup() completes
 * Handles incoming serial data and processes complete commands
 */
void loop()
{
  // ================================
  // SERIAL INPUT HANDLING
  // ================================
  // Check if there are characters available in the serial buffer
  while (Serial.available()) {
    char inChar = (char)Serial.read();  // Read one character
    
    // Check if we received a newline (end of command)
    if (inChar == '\n' || inChar == '\r') {
      inputComplete = true;  // Mark that we have a complete command
    } else if (inChar >= 32 && inChar <= 126) {  // Only accept printable characters
      inputBuffer += inChar;  // Add character to our input buffer
    }
  }
  
  // ================================
  // COMMAND PROCESSING
  // ================================
  // If we have a complete command, process it
  if (inputComplete) {
    // Clear any remaining serial data to prevent command mixing
    while (Serial.available()) {
      Serial.read();  // Flush remaining characters
    }
    
    // Only process non-empty commands
    if (inputBuffer.length() > 0) {
      processCommand(inputBuffer);  // Parse and execute the command
    }
    inputBuffer = "";             // Clear the buffer for next command
    inputComplete = false;        // Reset the completion flag
    
    // Small delay to ensure response is sent before next command
    delay(10);
  }

  updateAutoResetTimer();
  updateRgbAnimations();
  yield();
}

// ================================
// EEPROM HELPER FUNCTIONS
// ================================
void loadEepromSettings() {
  EEPROM.begin(512); // Initialize EEPROM with 512 bytes
  
  // Check magic number
  uint16_t magic = (EEPROM.read(EEPROM_MAGIC) << 8) | EEPROM.read(EEPROM_MAGIC + 1);
  if (magic != EEPROM_MAGIC_VALUE) {
    // EEPROM not initialized, use defaults and save
    initRgbEffectDefaults();
    saveEepromSettings();
    return;
  }
  
  // Load settings
  numLedsRgb1 = (EEPROM.read(EEPROM_NUM_LEDS_RGB1) << 8) | EEPROM.read(EEPROM_NUM_LEDS_RGB1 + 1);
  numLedsRgb2 = (EEPROM.read(EEPROM_NUM_LEDS_RGB2) << 8) | EEPROM.read(EEPROM_NUM_LEDS_RGB2 + 1);
  defaultR1 = EEPROM.read(EEPROM_DEFAULT_R1);
  defaultG1 = EEPROM.read(EEPROM_DEFAULT_G1);
  defaultB1 = EEPROM.read(EEPROM_DEFAULT_B1);
  defaultR2 = EEPROM.read(EEPROM_DEFAULT_R2);
  defaultG2 = EEPROM.read(EEPROM_DEFAULT_G2);
  defaultB2 = EEPROM.read(EEPROM_DEFAULT_B2);
  lbThreshold = (EEPROM.read(EEPROM_LB_THRESHOLD) << 8) | EEPROM.read(EEPROM_LB_THRESHOLD + 1);
  brightnessRgb1 = EEPROM.read(EEPROM_BRIGHTNESS_RGB1);
  brightnessRgb2 = EEPROM.read(EEPROM_BRIGHTNESS_RGB2);
  ledDefaultBrightness = EEPROM.read(EEPROM_LED_DEFAULT);
  startupMode[0] = EEPROM.read(EEPROM_STARTUP_MODE_RGB1);
  startupEffect[0] = (RgbEffectMode)EEPROM.read(EEPROM_STARTUP_EFFECT_RGB1);
  startupMode[1] = EEPROM.read(EEPROM_STARTUP_MODE_RGB2);
  startupEffect[1] = (RgbEffectMode)EEPROM.read(EEPROM_STARTUP_EFFECT_RGB2);
  uint8_t autoResetValue = EEPROM.read(EEPROM_AUTO_RESET_12H);
  autoReset12hEnabled = (autoResetValue == 1);
  
  // Validate loaded values
  if (numLedsRgb1 < 1 || numLedsRgb1 > 1000) numLedsRgb1 = 300;
  if (numLedsRgb2 < 1 || numLedsRgb2 > 1000) numLedsRgb2 = 300;
  if (defaultR1 < 0 || defaultR1 > 255) defaultR1 = 255;
  if (defaultG1 < 0 || defaultG1 > 255) defaultG1 = 120;
  if (defaultB1 < 0 || defaultB1 > 255) defaultB1 = 40;
  if (defaultR2 < 0 || defaultR2 > 255) defaultR2 = 255;
  if (defaultG2 < 0 || defaultG2 > 255) defaultG2 = 120;
  if (defaultB2 < 0 || defaultB2 > 255) defaultB2 = 40;
  if (lbThreshold < 0 || lbThreshold > 1023) lbThreshold = 512;
  if (brightnessRgb1 < 0 || brightnessRgb1 > 255) brightnessRgb1 = 131;
  if (brightnessRgb2 < 0 || brightnessRgb2 > 255) brightnessRgb2 = 131;
  if (ledDefaultBrightness < 0 || ledDefaultBrightness > 255) ledDefaultBrightness = 110;
  if (startupMode[0] > STARTUP_EFFECT) startupMode[0] = STARTUP_SOLID;
  if (startupMode[1] > STARTUP_EFFECT) startupMode[1] = STARTUP_SOLID;
  if (startupEffect[0] >= RGB_EFFECT_COUNT) startupEffect[0] = RGB_EFFECT_RAINBOW;
  if (startupEffect[1] >= RGB_EFFECT_COUNT) startupEffect[1] = RGB_EFFECT_RAINBOW;

  initRgbEffectDefaults();
  bool needsSave = false;
  loadRgbEffectDefaults(needsSave);
  if (needsSave) {
    saveEepromSettings();
  }
}

void saveEepromSettings() {
  EEPROM.write(EEPROM_MAGIC, (EEPROM_MAGIC_VALUE >> 8) & 0xFF);
  EEPROM.write(EEPROM_MAGIC + 1, EEPROM_MAGIC_VALUE & 0xFF);
  
  EEPROM.write(EEPROM_NUM_LEDS_RGB1, (numLedsRgb1 >> 8) & 0xFF);
  EEPROM.write(EEPROM_NUM_LEDS_RGB1 + 1, numLedsRgb1 & 0xFF);
  
  EEPROM.write(EEPROM_NUM_LEDS_RGB2, (numLedsRgb2 >> 8) & 0xFF);
  EEPROM.write(EEPROM_NUM_LEDS_RGB2 + 1, numLedsRgb2 & 0xFF);
  
  EEPROM.write(EEPROM_DEFAULT_R1, defaultR1);
  EEPROM.write(EEPROM_DEFAULT_G1, defaultG1);
  EEPROM.write(EEPROM_DEFAULT_B1, defaultB1);
  
  EEPROM.write(EEPROM_DEFAULT_R2, defaultR2);
  EEPROM.write(EEPROM_DEFAULT_G2, defaultG2);
  EEPROM.write(EEPROM_DEFAULT_B2, defaultB2);
  
  EEPROM.write(EEPROM_LB_THRESHOLD, (lbThreshold >> 8) & 0xFF);
  EEPROM.write(EEPROM_LB_THRESHOLD + 1, lbThreshold & 0xFF);
  
  EEPROM.write(EEPROM_BRIGHTNESS_RGB1, brightnessRgb1);
  EEPROM.write(EEPROM_BRIGHTNESS_RGB2, brightnessRgb2);
  
  EEPROM.write(EEPROM_LED_DEFAULT, ledDefaultBrightness);
  EEPROM.write(EEPROM_STARTUP_MODE_RGB1, startupMode[0]);
  EEPROM.write(EEPROM_STARTUP_EFFECT_RGB1, (uint8_t)startupEffect[0]);
  EEPROM.write(EEPROM_STARTUP_MODE_RGB2, startupMode[1]);
  EEPROM.write(EEPROM_STARTUP_EFFECT_RGB2, (uint8_t)startupEffect[1]);
  EEPROM.write(EEPROM_AUTO_RESET_12H, autoReset12hEnabled ? 1 : 0);
  saveRgbEffectDefaults();
  
  EEPROM.commit(); // Save changes
}

// ================================
// LM75 HELPER FUNCTIONS
// ================================
float readLm75Temperature() {
  // Point to temperature register (0x00)
  Wire.beginTransmission(LM75_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) { // repeated start
    return NAN;
  }
  if (Wire.requestFrom((int)LM75_ADDR, 2) != 2) {
    return NAN;
  }
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  int16_t raw = ((int16_t)msb << 8) | lsb; // 16-bit container
  raw >>= 7; // 9-bit two's complement (LSB = 0.5°C)
  return raw * 0.5f;
}

bool lm75Available() {
  Wire.beginTransmission(LM75_ADDR);
  return (Wire.endTransmission() == 0);
}

// ================================
// PWM / NEOPIXEL TIMING FIX FUNCTIONS
// ================================
/**
 * PAUSE PWM FOR NEOPIXEL COMMUNICATION
 * Temporarily disables PWM on ledPin to prevent timing conflicts
 * with NeoPixel bitbanging communication.
 * The PWM value is saved and can be restored with resumePwm().
 * 
 * This fixes the issue where analogWrite() on D5 interferes with
 * NeoPixel operations on D1/D2 due to timing conflicts on the ESP8266.
 */
void pausePwmForNeoPixel() {
  if (pwmActive) {
    // Save current PWM value and disable it
    analogWrite(ledPin, 0);  // Turn off PWM (write 0)
    delayMicroseconds(100);  // Brief delay for PWM to settle
  }
}

/**
 * RESUME PWM AFTER NEOPIXEL COMMUNICATION
 * Restores PWM on ledPin to the previously saved value.
 * Call this after NeoPixel show() operations complete.
 */
void resumePwm() {
  if (pwmActive && lastPwmValue > 0) {
    delayMicroseconds(100);  // Brief delay before resuming PWM
    analogWrite(ledPin, lastPwmValue);  // Restore previous PWM value
  }
}

bool isByteValue(int value) {
  return value >= 0 && value <= 255;
}

uint8_t scaleChannel(int value, int brightness) {
  return (uint32_t(value) * uint32_t(brightness) + 127) / 255;
}

uint32_t makeScaledColor(Adafruit_NeoPixel* strip, int r, int g, int b, int brightness) {
  return strip->Color(scaleChannel(r, brightness), scaleChannel(g, brightness), scaleChannel(b, brightness));
}

void showStrip(Adafruit_NeoPixel* strip) {
  strip->show();
}

const char* rgbEffectName(RgbEffectMode mode) {
  switch (mode) {
    case RGB_EFFECT_RUNNING: return "running";
    case RGB_EFFECT_CHARGING: return "charging";
    case RGB_EFFECT_CENTER: return "center_fill";
    case RGB_EFFECT_RAINBOW: return "rainbow";
    case RGB_EFFECT_FLASH: return "flash";
    case RGB_EFFECT_RANDOM: return "random";
    case RGB_EFFECT_BREATHING: return "breathing";
    default: return "none";
  }
}

const char* startupModeName(uint8_t mode) {
  switch (mode) {
    case STARTUP_EFFECT: return "effect";
    default: return "solid";
  }
}

int getEffectEepromOffset(int strip, RgbEffectMode mode) {
  if (strip < 1 || strip > 2) {
    return EEPROM_EFFECTS_BASE;
  }
  return EEPROM_EFFECTS_BASE + ((strip - 1) * EEPROM_EFFECT_STRIDE) + (int(mode) * EEPROM_EFFECT_RECORD_SIZE);
}

RgbEffectDefaults makeDefaultEffectConfig(int strip, RgbEffectMode mode) {
  RgbEffectDefaults defaults;
  if (strip == 1) {
    defaults.red = defaultR1;
    defaults.green = defaultG1;
    defaults.blue = defaultB1;
    defaults.brightness = brightnessRgb1;
  } else {
    defaults.red = defaultR2;
    defaults.green = defaultG2;
    defaults.blue = defaultB2;
    defaults.brightness = brightnessRgb2;
  }

  switch (mode) {
    case RGB_EFFECT_RUNNING:
      defaults.intervalMs = 40;
      defaults.repeats = 0;
      defaults.count = max(1, min(12, (strip == 1 ? numLedsRgb1 : numLedsRgb2) / 10));
      break;
    case RGB_EFFECT_CHARGING:
      defaults.intervalMs = 35;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
    case RGB_EFFECT_CENTER:
      defaults.intervalMs = 35;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
    case RGB_EFFECT_RAINBOW:
      defaults.intervalMs = 20;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
    case RGB_EFFECT_FLASH:
      defaults.intervalMs = 250;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
    case RGB_EFFECT_RANDOM:
      defaults.intervalMs = 80;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
    case RGB_EFFECT_BREATHING:
      defaults.intervalMs = 25;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
    default:
      defaults.intervalMs = 40;
      defaults.repeats = 0;
      defaults.count = 0;
      break;
  }

  return defaults;
}

void initRgbEffectDefaults() {
  for (int strip = 1; strip <= 2; strip++) {
    for (int effect = 0; effect < RGB_EFFECT_COUNT; effect++) {
      rgbEffectDefaults[strip - 1][effect] = makeDefaultEffectConfig(strip, static_cast<RgbEffectMode>(effect));
    }
  }
}

bool startRgbEffectInternal(int strip, RgbEffectMode mode, int startIndex, int endIndex, int red, int green, int blue, int brightness, int intervalMs, int repeats, int count, bool announce) {
  if (strip < 1 || strip > 2) {
    if (announce) {
      sendError("Invalid strip number. Use 1 or 2");
    }
    return false;
  }

  Adafruit_NeoPixel* currentStrip = (strip == 1) ? rgb1 : rgb2;
  if (currentStrip == nullptr) {
    if (announce) {
      sendError("RGB strip not initialized");
    }
    return false;
  }

  if (startIndex < 0 || endIndex < 0 || startIndex >= currentStrip->numPixels() || endIndex >= currentStrip->numPixels() || startIndex > endIndex) {
    if (announce) {
      sendError("Invalid pixel range for effect");
    }
    return false;
  }

  if (!isByteValue(red) || !isByteValue(green) || !isByteValue(blue)) {
    if (announce) {
      sendError("Invalid RGB values. Range: 0-255");
    }
    return false;
  }
  if (!isByteValue(brightness)) {
    if (announce) {
      sendError("Invalid brightness value. Range: 0-255");
    }
    return false;
  }
  if (intervalMs < 1 || intervalMs > 10000) {
    if (announce) {
      sendError("Invalid time value. Range: 1-10000");
    }
    return false;
  }
  if (repeats < 0 || repeats > 10000) {
    if (announce) {
      sendError("Invalid repeatingtime value. Range: 0-10000");
    }
    return false;
  }

  int segmentLength = endIndex - startIndex + 1;
  if (segmentLength < 1) {
    if (announce) {
      sendError("Invalid pixel range for effect");
    }
    return false;
  }

  if (mode == RGB_EFFECT_RUNNING) {
    if (count < 1) {
      count = 1;
    }
    if (count > segmentLength) {
      count = segmentLength;
    }
  } else {
    count = 0;
  }

  stopRgbEffect(strip, false);

  RgbAnimationState& state = rgbAnimationState[strip - 1];
  state.active = true;
  state.mode = mode;
  state.startIndex = startIndex;
  state.endIndex = endIndex;
  state.bandCount = count;
  state.intervalMs = intervalMs;
  state.repeatsRemaining = repeats;
  state.step = 0;
  state.maxSteps = 1;
  state.red = (uint8_t)red;
  state.green = (uint8_t)green;
  state.blue = (uint8_t)blue;
  state.brightness = (uint8_t)brightness;
  state.flashOn = true;
  state.randomHue = (uint16_t)random(0, 65536);
  state.randomTargetHue = (uint16_t)random(0, 65536);
  state.randomHueStep = 128;
  state.lastUpdateMs = millis();

  switch (mode) {
    case RGB_EFFECT_RUNNING:
      state.maxSteps = segmentLength;
      if (state.maxSteps < 1) {
        state.maxSteps = 1;
      }
      break;
    case RGB_EFFECT_CHARGING:
    case RGB_EFFECT_CENTER:
      state.maxSteps = max(2, segmentLength * 2);
      break;
    case RGB_EFFECT_RAINBOW:
      state.maxSteps = max(1, segmentLength);
      break;
    case RGB_EFFECT_FLASH:
      state.maxSteps = 2;
      break;
    case RGB_EFFECT_RANDOM:
      state.maxSteps = 32;
      break;
    case RGB_EFFECT_BREATHING:
      state.maxSteps = 64;
      break;
    default:
      state.maxSteps = 1;
      break;
  }

  renderRgbEffectFrame(strip, state);
  showStrip(currentStrip);

  if (announce) {
    String message = String(rgbEffectName(mode)) + " effect started on strip " + String(strip) + " with brightness " + String(brightness) + ", time " + String(intervalMs) + "ms and repeats " + String(repeats);
    if (mode == RGB_EFFECT_RUNNING) {
      message += ", count " + String(count);
    }
    sendSuccess(message);
  }

  return true;
}

void applyStartupState(int strip) {
  if (strip < 1 || strip > 2) {
    return;
  }

  int index = strip - 1;
  if (startupMode[index] == STARTUP_EFFECT) {
    RgbEffectDefaults defaults = rgbEffectDefaults[index][startupEffect[index]];
    startRgbEffectInternal(strip, startupEffect[index], 0, ((strip == 1) ? rgb1 : rgb2)->numPixels() - 1, defaults.red, defaults.green, defaults.blue, defaults.brightness, defaults.intervalMs, defaults.repeats, defaults.count, false);
  } else {
    applyStripDefault(strip);
  }
}

bool loadEffectDefaultsAtOffset(int offset, RgbEffectDefaults& defaults) {
  RgbEffectDefaults loaded;
  loaded.red = EEPROM.read(offset + 0);
  loaded.green = EEPROM.read(offset + 1);
  loaded.blue = EEPROM.read(offset + 2);
  loaded.brightness = EEPROM.read(offset + 3);
  loaded.intervalMs = (EEPROM.read(offset + 4) << 8) | EEPROM.read(offset + 5);
  loaded.repeats = (EEPROM.read(offset + 6) << 8) | EEPROM.read(offset + 7);
  loaded.count = (EEPROM.read(offset + 8) << 8) | EEPROM.read(offset + 9);

  bool valid = loaded.intervalMs >= 1 && loaded.intervalMs <= 10000
    && loaded.repeats <= 10000
    && loaded.count <= 1000;

  if (valid) {
    defaults = loaded;
  }

  return valid;
}

void saveEffectDefaultsAtOffset(int offset, const RgbEffectDefaults& defaults) {
  EEPROM.write(offset + 0, defaults.red);
  EEPROM.write(offset + 1, defaults.green);
  EEPROM.write(offset + 2, defaults.blue);
  EEPROM.write(offset + 3, defaults.brightness);
  EEPROM.write(offset + 4, (defaults.intervalMs >> 8) & 0xFF);
  EEPROM.write(offset + 5, defaults.intervalMs & 0xFF);
  EEPROM.write(offset + 6, (defaults.repeats >> 8) & 0xFF);
  EEPROM.write(offset + 7, defaults.repeats & 0xFF);
  EEPROM.write(offset + 8, (defaults.count >> 8) & 0xFF);
  EEPROM.write(offset + 9, defaults.count & 0xFF);
}

void loadRgbEffectDefaults(bool& needsSave) {
  for (int strip = 1; strip <= 2; strip++) {
    for (int effect = 0; effect < RGB_EFFECT_COUNT; effect++) {
      int offset = getEffectEepromOffset(strip, static_cast<RgbEffectMode>(effect));
      if (!loadEffectDefaultsAtOffset(offset, rgbEffectDefaults[strip - 1][effect])) {
        needsSave = true;
      }
    }
  }
}

void saveRgbEffectDefaults() {
  for (int strip = 1; strip <= 2; strip++) {
    for (int effect = 0; effect < RGB_EFFECT_COUNT; effect++) {
      int offset = getEffectEepromOffset(strip, static_cast<RgbEffectMode>(effect));
      saveEffectDefaultsAtOffset(offset, rgbEffectDefaults[strip - 1][effect]);
    }
  }
}

void applyStripDefault(int strip) {
  Adafruit_NeoPixel* currentStrip = (strip == 1) ? rgb1 : rgb2;
  if (currentStrip == nullptr) {
    return;
  }

  uint8_t red = (strip == 1) ? defaultR1 : defaultR2;
  uint8_t green = (strip == 1) ? defaultG1 : defaultG2;
  uint8_t blue = (strip == 1) ? defaultB1 : defaultB2;
  uint8_t brightness = (strip == 1) ? brightnessRgb1 : brightnessRgb2;
  uint32_t color = makeScaledColor(currentStrip, red, green, blue, brightness);

  for (int i = 0; i < currentStrip->numPixels(); i++) {
    currentStrip->setPixelColor(i, color);
  }
  showStrip(currentStrip);
}

void applyControllerDefaultState() {
  stopRgbEffect(1, false);
  stopRgbEffect(2, false);
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  analogWrite(ledPin, ledDefaultBrightness);
  lastPwmValue = ledDefaultBrightness;
  pwmActive = (ledDefaultBrightness > 0);
  applyStartupState(1);
  applyStartupState(2);
}

void updateAutoResetTimer() {
  if (!autoReset12hEnabled) {
    return;
  }

  uint32_t now = millis();
  if ((uint32_t)(now - autoReset12hStartMs) < AUTO_RESET_12H_MS) {
    return;
  }

  autoReset12hStartMs = now;
  applyControllerDefaultState();
}

void stopRgbEffect(int strip, bool restoreDefault) {
  if (strip < 1 || strip > 2) {
    return;
  }

  RgbAnimationState& state = rgbAnimationState[strip - 1];
  state.active = false;
  if (restoreDefault) {
    applyStripDefault(strip);
  }
}

void renderRgbEffectFrame(int strip, RgbAnimationState& state) {
  Adafruit_NeoPixel* currentStrip = (strip == 1) ? rgb1 : rgb2;
  if (currentStrip == nullptr) {
    return;
  }

  int totalPixels = currentStrip->numPixels();
  if (totalPixels <= 0) {
    return;
  }

  int start = state.startIndex;
  int end = state.endIndex;
  if (start < 0) {
    start = 0;
  }
  if (end >= totalPixels) {
    end = totalPixels - 1;
  }
  if (start > end) {
    return;
  }

  int segmentLength = end - start + 1;
  uint32_t color = makeScaledColor(currentStrip, state.red, state.green, state.blue, state.brightness);
  currentStrip->clear();

  switch (state.mode) {
    case RGB_EFFECT_RUNNING: {
      int bandCount = state.bandCount;
      if (bandCount < 1) {
        bandCount = 1;
      }
      if (bandCount > segmentLength) {
        bandCount = segmentLength;
      }

      for (int offset = 0; offset < segmentLength; offset++) {
        int distance = offset - state.step;
        if (distance < 0) {
          distance += segmentLength;
        }
        if (distance < bandCount) {
          uint8_t scaledBrightness = (uint32_t(bandCount - distance) * uint32_t(state.brightness)) / bandCount;
          currentStrip->setPixelColor(start + offset, makeScaledColor(currentStrip, state.red, state.green, state.blue, scaledBrightness));
        }
      }
      break;
    }

    case RGB_EFFECT_CHARGING: {
      int fillCount = (state.step < segmentLength) ? state.step : (segmentLength * 2 - state.step);
      if (fillCount < 0) {
        fillCount = 0;
      }
      if (fillCount > segmentLength) {
        fillCount = segmentLength;
      }
      for (int i = 0; i < fillCount; i++) {
        currentStrip->setPixelColor(start + i, color);
      }
      break;
    }

    case RGB_EFFECT_CENTER: {
      int fillCount;
      if (state.step <= segmentLength) {
        fillCount = state.step;
      } else {
        fillCount = (segmentLength * 2 + 1) - state.step;
      }
      if (fillCount < 0) {
        fillCount = 0;
      }
      if (fillCount > segmentLength) {
        fillCount = segmentLength;
      }
      for (int i = 0; i < (fillCount / 2) + 1; i++) {
        currentStrip->setPixelColor(start + i, color);
        currentStrip->setPixelColor(end - i, color);
      }
      break;
    }

    case RGB_EFFECT_RAINBOW: {
      for (int offset = 0; offset < segmentLength; offset++) {
        uint32_t shiftedOffset = (uint32_t(offset) + uint32_t(state.step)) % uint32_t(max(1, segmentLength));
        uint32_t hue = (shiftedOffset * 65535UL) / uint32_t(max(1, segmentLength));
        currentStrip->setPixelColor(start + offset, currentStrip->ColorHSV((uint16_t)(hue & 0xFFFF), 255, state.brightness));
      }
      break;
    }

    case RGB_EFFECT_FLASH: {
      if ((state.step % 2) == 0) {
        for (int i = start; i <= end; i++) {
          currentStrip->setPixelColor(i, color);
        }
      }
      break;
    }

    case RGB_EFFECT_RANDOM: {
      if (state.randomHueStep == 0) {
        state.randomHueStep = 128;
      }

      int16_t hueDelta = (int16_t)(state.randomTargetHue - state.randomHue);
      if (hueDelta == 0) {
        state.randomTargetHue = (uint16_t)random(0, 65536);
        hueDelta = (int16_t)(state.randomTargetHue - state.randomHue);
      }

      int16_t stepSize = (int16_t)state.randomHueStep;
      if (hueDelta > 0) {
        if (hueDelta < stepSize) {
          stepSize = hueDelta;
        }
        state.randomHue = (uint16_t)(state.randomHue + stepSize);
      } else {
        int16_t distance = (int16_t)(-hueDelta);
        if (distance < stepSize) {
          stepSize = distance;
        }
        state.randomHue = (uint16_t)(state.randomHue - stepSize);
      }

      for (int i = start; i <= end; i++) {
        currentStrip->setPixelColor(i, currentStrip->ColorHSV(state.randomHue, 255, state.brightness));
      }
      break;
    }

    case RGB_EFFECT_BREATHING: {
      uint16_t halfCycle = max<uint16_t>(1, state.maxSteps / 2);
      uint16_t position = state.step <= halfCycle ? state.step : (state.maxSteps - state.step);
      uint8_t minBrightness = max<uint8_t>(8, state.brightness / 8);
      uint8_t currentBrightness = minBrightness + ((uint32_t)(state.brightness - minBrightness) * position) / halfCycle;
      uint32_t breathingColor = makeScaledColor(currentStrip, state.red, state.green, state.blue, currentBrightness);
      for (int i = start; i <= end; i++) {
        currentStrip->setPixelColor(i, breathingColor);
      }
      break;
    }

    default:
      break;
  }
}

void updateRgbAnimations() {
  uint32_t now = millis();

  for (int strip = 1; strip <= 2; strip++) {
    RgbAnimationState& state = rgbAnimationState[strip - 1];
    if (!state.active) {
      continue;
    }

    if (state.intervalMs == 0) {
      state.intervalMs = 1;
    }

    if (state.lastUpdateMs != 0 && (uint32_t)(now - state.lastUpdateMs) < state.intervalMs) {
      continue;
    }

    renderRgbEffectFrame(strip, state);
    showStrip((strip == 1) ? rgb1 : rgb2);
    state.lastUpdateMs = now;

    bool cycleCompleted = false;
    if (state.maxSteps < 1) {
      state.maxSteps = 1;
    }
    state.step++;
    if (state.step >= state.maxSteps) {
      state.step = 0;
      cycleCompleted = true;
    }

    if (cycleCompleted && state.repeatsRemaining > 0) {
      state.repeatsRemaining--;
      if (state.repeatsRemaining == 0) {
        state.active = false;
        applyStripDefault(strip);
      }
    }

    if (state.mode == RGB_EFFECT_RANDOM && cycleCompleted) {
      state.randomHue = state.randomTargetHue;
      state.randomTargetHue = (uint16_t)random(0, 65536);
    }
  }
}

void startRgbEffect(JsonDocument& doc, int strip, RgbEffectMode mode) {
  RgbEffectDefaults defaults = rgbEffectDefaults[strip - 1][mode];
  int startIndex = doc["start"] | 0;
  int endIndex = doc["end"] | (((strip == 1) ? rgb1 : rgb2)->numPixels() - 1);
  int red = doc["r"] | defaults.red;
  int green = doc["g"] | defaults.green;
  int blue = doc["b"] | defaults.blue;
  int brightness = doc["brightness"] | defaults.brightness;
  int intervalMs = doc["time"] | defaults.intervalMs;
  int repeats = defaults.repeats;
  if (doc["repeatingtime"].is<int>()) {
    repeats = doc["repeatingtime"].as<int>();
  } else if (doc["repeat"].is<int>()) {
    repeats = doc["repeat"].as<int>();
  }
  int count = doc["count"] | defaults.count;
  startRgbEffectInternal(strip, mode, startIndex, endIndex, red, green, blue, brightness, intervalMs, repeats, count, true);
}

void updateRgbEffectDefaults(JsonDocument& doc, RgbEffectMode mode) {
  int strip = doc["strip"] | 0;
  bool applyBoth = (strip != 1 && strip != 2);

  for (int targetStrip = 1; targetStrip <= 2; targetStrip++) {
    if (!applyBoth && targetStrip != strip) {
      continue;
    }

    RgbEffectDefaults& defaults = rgbEffectDefaults[targetStrip - 1][mode];
    int red = doc["r"] | defaults.red;
    int green = doc["g"] | defaults.green;
    int blue = doc["b"] | defaults.blue;
    int brightness = doc["brightness"] | defaults.brightness;
    int intervalMs = doc["time"] | defaults.intervalMs;
    int repeats = defaults.repeats;
    if (doc["repeatingtime"].is<int>()) {
      repeats = doc["repeatingtime"].as<int>();
    } else if (doc["repeat"].is<int>()) {
      repeats = doc["repeat"].as<int>();
    }
    int count = doc["count"] | defaults.count;

    if (!isByteValue(red) || !isByteValue(green) || !isByteValue(blue)) {
      sendError("Invalid RGB values. Range: 0-255");
      return;
    }
    if (!isByteValue(brightness)) {
      sendError("Invalid brightness value. Range: 0-255");
      return;
    }
    if (intervalMs < 1 || intervalMs > 10000) {
      sendError("Invalid time value. Range: 1-10000");
      return;
    }
    if (repeats < 0 || repeats > 10000) {
      sendError("Invalid repeatingtime value. Range: 0-10000");
      return;
    }
    if (count < 0 || count > 1000) {
      sendError("Invalid count value. Range: 0-1000");
      return;
    }

    defaults.red = (uint8_t)red;
    defaults.green = (uint8_t)green;
    defaults.blue = (uint8_t)blue;
    defaults.brightness = (uint8_t)brightness;
    defaults.intervalMs = (uint16_t)intervalMs;
    defaults.repeats = (uint16_t)repeats;
    defaults.count = (uint16_t)count;
  }

  saveEepromSettings();

  String scope = applyBoth ? "both strips" : (String("strip ") + String(strip));
  sendSuccess(String(rgbEffectName(mode)) + " defaults updated for " + scope);
}