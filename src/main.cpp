/*
 * LED Controller API v1.0
 * 
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
// LED STRIP CONFIGURATION
// ================================
const int NUM_LEDS_RGB_1 = 78;  // Number of LEDs in strip #1
const int NUM_LEDS_RGB_2 = 78;  // Number of LEDs in strip #2

// ================================
// NEOPIXEL OBJECTS
// ================================
// Initialize NeoPixel strips with:
// - Number of LEDs, Pin, Color order (GRB) + Data rate (800KHz)
Adafruit_NeoPixel rgb1 = Adafruit_NeoPixel(NUM_LEDS_RGB_1, RGB1, NEO_GRB + NEO_KHZ800); 
Adafruit_NeoPixel rgb2 = Adafruit_NeoPixel(NUM_LEDS_RGB_2, RGB2, NEO_GRB + NEO_KHZ800);

// ================================
// GLOBAL VARIABLES
// ================================
int lbThreshold = 512;    // Threshold for LB sensor digital mode (0-1023 range)
String inputBuffer = "";  // Buffer to store incoming serial characters
bool inputComplete = false;  // Flag to indicate when a complete command is received

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
  // ================================
  if (strip != 1 && strip != 2) {
    sendError("Invalid strip number. Use 1 or 2");
    return;
  }
  
  // Get pointer to the correct NeoPixel strip object
  Adafruit_NeoPixel* currentStrip = (strip == 1) ? &rgb1 : &rgb2;
  
  // ================================
  // HANDLE DIFFERENT OPERATION MODES
  // ================================
  
  if (mode == "single") {
    // ================================
    // SINGLE PIXEL CONTROL
    // ================================
    // Extract pixel number and RGB values
    int pixel = doc["pixel"] | 0;  // Pixel index (0-77)
    int r = doc["r"] | 0;          // Red value (0-255)
    int g = doc["g"] | 0;          // Green value (0-255)
    int b = doc["b"] | 0;          // Blue value (0-255)
    
    // Validate pixel number is within strip range
    if (pixel >= 0 && pixel < currentStrip->numPixels()) {
      currentStrip->setPixelColor(pixel, currentStrip->Color(r, g, b));
      currentStrip->show();  // Update the physical LEDs
      delay(5);  // Small delay to ensure LED update completes
      sendSuccess("Pixel " + String(pixel) + " on strip " + String(strip) + " set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    } else {
      sendError("Invalid pixel number. Range: 0-" + String(currentStrip->numPixels() - 1));
    }
    
  } else if (mode == "range") {
    // ================================
    // PIXEL RANGE CONTROL
    // ================================
    // Extract range parameters and RGB values
    int start = doc["start"] | 0;                           // Start pixel
    int end = doc["end"] | (currentStrip->numPixels() - 1); // End pixel
    int r = doc["r"] | 0;                                   // Red value
    int g = doc["g"] | 0;                                   // Green value
    int b = doc["b"] | 0;                                   // Blue value
    
    // Validate range parameters
    if (start >= 0 && end < currentStrip->numPixels() && start <= end) {
      // Set color for all pixels in the range
      for (int i = start; i <= end; i++) {
        currentStrip->setPixelColor(i, currentStrip->Color(r, g, b));
      }
      currentStrip->show();  // Update the physical LEDs
      delay(10);  // Longer delay for range operations
      sendSuccess("Pixels " + String(start) + "-" + String(end) + " on strip " + String(strip) + " set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    } else {
      sendError("Invalid pixel range");
    }
    
  } else if (mode == "all") {
    // ================================
    // ALL PIXELS CONTROL
    // ================================
    // Extract RGB values
    int r = doc["r"] | 0;  // Red value
    int g = doc["g"] | 0;  // Green value
    int b = doc["b"] | 0;  // Blue value
    
    // Set all pixels to the same color
    for (int i = 0; i < currentStrip->numPixels(); i++) {
      currentStrip->setPixelColor(i, currentStrip->Color(r, g, b));
    }
    currentStrip->show();  // Update the physical LEDs
    delay(15);  // Longer delay for full strip operations
    sendSuccess("All pixels on strip " + String(strip) + " set to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    
  } else if (mode == "clear") {
    // ================================
    // CLEAR STRIP (TURN OFF ALL LEDS)
    // ================================
    currentStrip->clear();  // Set all pixels to black (off)
    currentStrip->show();   // Update the physical LEDs
    delay(10);  // Ensure clear operation completes
    sendSuccess("Strip " + String(strip) + " cleared");
    
  } else {
    // ================================
    // INVALID MODE
    // ================================
    sendError("Invalid mode. Use: single, range, all, clear");
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
    sendSuccess("LED set to " + String(state ? "ON" : "OFF"));
    
  } else if (mode == "analog") {
    // ================================
    // ANALOG MODE (PWM BRIGHTNESS CONTROL)
    // ================================
    int value = doc["value"] | 0;  // Extract brightness value (0-255)
    
    // Validate brightness value range
    if (value >= 0 && value <= 255) {
      analogWrite(ledPin, value);  // Set PWM brightness (0=off, 255=full brightness)
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
      sendSuccess("LB threshold set to " + String(lbThreshold));
    } else {
      sendError("Invalid threshold value. Range: 0-1023");
    }
    
  } else {
    // ================================
    // INVALID CONFIGURATION SETTING
    // ================================
    sendError("Invalid setting. Available: lb_threshold");
  }
}

/**
 * SUCCESS RESPONSE SENDER
 * Sends a standardized JSON success response
 * 
 * @param message Success message to include in response
 */
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
  Serial.println("=== LED Controller API Documentation ===");
  Serial.println();
  Serial.println("All commands use JSON format. Examples:");
  Serial.println();
  
  // ================================
  // RGB STRIP CONTROL EXAMPLES
  // ================================
  Serial.println("1. RGB STRIP CONTROL:");
  Serial.println("   Single pixel: {\"action\":\"rgb\",\"strip\":1,\"mode\":\"single\",\"pixel\":0,\"r\":255,\"g\":0,\"b\":0}");
  Serial.println("   Range:        {\"action\":\"rgb\",\"strip\":1,\"mode\":\"range\",\"start\":0,\"end\":9,\"r\":0,\"g\":255,\"b\":0}");
  Serial.println("   All pixels:   {\"action\":\"rgb\",\"strip\":2,\"mode\":\"all\",\"r\":0,\"g\":0,\"b\":255}");
  Serial.println("   Clear strip:  {\"action\":\"rgb\",\"strip\":1,\"mode\":\"clear\"}");
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
  Serial.println("   Set threshold: {\"action\":\"config\",\"setting\":\"lb_threshold\",\"value\":600}");
  Serial.println();
  
  // ================================
  // PARAMETER EXPLANATIONS
  // ================================
  Serial.println("PARAMETERS:");
  Serial.println("- strip: 1 (Ring-Top) or 2 (Door)");
  Serial.println("- pixel: 0-77 (78 LEDs total per strip)");
  Serial.println("- r,g,b: 0-255 (RGB color values)");
  Serial.println("- value: 0-255 (analog LED brightness)");
  Serial.println("- state: true/false");
  Serial.println("- lb_threshold: 0-1023 (analog threshold for digital mode)");
  Serial.println();
  
  // ================================
  // RESPONSE FORMAT INFO
  // ================================
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
  // SERIAL COMMUNICATION SETUP
  // ================================
  Serial.begin(115200);  // Start serial communication at 115200 baud rate
  Serial.println();      // Print empty line for clarity
  Serial.println("=== LED Controller API v1.0 ===");
  Serial.println("Type 'help' for available commands");
  Serial.println("Ready for JSON commands...");
  
  // ================================
  // PIN INITIALIZATION
  // ================================
  // Set pin modes for outputs (LEDs and relays)
  pinMode(ledPin, OUTPUT);  // Single LED pin as output
  pinMode(relay1, OUTPUT);  // Relay 1 pin as output
  pinMode(relay2, OUTPUT);  // Relay 2 pin as output
  
  // Set pin modes for inputs (sensors)
  pinMode(RS, INPUT);       // Ticket Barrier sensor as digital input
  pinMode(LB, INPUT);       // Paper Full sensor as analog input
  
  // ================================
  // NEOPIXEL STRIP INITIALIZATION
  // ================================
  // Initialize RGB strip #1 (Ring-Top)
  rgb1.begin();   // Prepare the NeoPixel library
  rgb1.clear();   // Turn off all LEDs
  rgb1.show();    // Apply the changes (all LEDs off)
  
  // Initialize RGB strip #2 (Door)
  rgb2.begin();   // Prepare the NeoPixel library
  rgb2.clear();   // Turn off all LEDs
  rgb2.show();    // Apply the changes (all LEDs off)
  
  // ================================
  // SET DEFAULT HARDWARE STATES
  // ================================
  digitalWrite(relay1, LOW);  // Turn relay 1 OFF (default state)
  digitalWrite(relay2, LOW);  // Turn relay 2 OFF (default state)
  digitalWrite(ledPin, LOW);  // Turn single LED OFF (default state)
  
  // ================================
  // SERIAL BUFFER SETUP
  // ================================
  inputBuffer.reserve(200);   // Pre-allocate 200 bytes for serial input buffer

  // ================================
  // I2C (LM75) INITIALIZATION
  // ================================
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(5);
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
    
    processCommand(inputBuffer);  // Parse and execute the command
    inputBuffer = "";             // Clear the buffer for next command
    inputComplete = false;        // Reset the completion flag
    
    // Small delay to ensure response is sent before next command
    delay(10);
  }
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