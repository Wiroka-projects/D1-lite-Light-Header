"""
LED Controller API Tester with FastAPI Web Interface
===================================================

This FastAPI application provides a web interface to test all functions
of the LED Controller API running on an Arduino/ESP8266.

Features:
- Test all RGB strip operations (single pixel, range, all, clear)
- Control single LED (digital/analog)
- Control relays
- Read sensors (LB analog/digital, RS digital, Temperature LM75)
- Configure system settings
- Real-time serial communication with the Arduino

Requirements:
pip install fastapi uvicorn pyserial

Usage:
1. Connect your Arduino/ESP8266 to a COM port
2. Update the SERIAL_PORT variable below
3. Run: python api_tester.py
4. Open browser: http://localhost:8000
"""

from fastapi import FastAPI, HTTPException, Form
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import serial
import json
import time
from typing import Optional
import uvicorn

# ================================
# CONFIGURATION
# ================================
SERIAL_PORT = "COM3"  # Change this to your Arduino's COM port
BAUD_RATE = 115200
TIMEOUT = 2  # Serial timeout in seconds

# ================================
# SERIAL CONNECTION
# ================================
try:
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT)
    time.sleep(2)  # Wait for Arduino to initialize
    print(f"✅ Connected to Arduino on {SERIAL_PORT}")
except Exception as e:
    print(f"❌ Failed to connect to Arduino: {e}")
    arduino = None

# ================================
# FASTAPI APP INITIALIZATION
# ================================
app = FastAPI(
    title="LED Controller API Tester",
    description="Web interface for testing Arduino LED Controller API",
    version="1.0.0"
)

# ================================
# HELPER FUNCTIONS
# ================================

def send_command(command: str) -> dict:
    """Send command to Arduino and return response"""
    if arduino is None:
        return {"status": "error", "message": "Arduino not connected"}
    
    try:
        # Clear any pending input before sending new command
        arduino.reset_input_buffer()
        
        # Send command
        arduino.write((command + '\n').encode())
        arduino.flush()  # Ensure command is sent
        
        # Small delay to let Arduino process
        time.sleep(0.05)
        
        # Read response with timeout
        response = arduino.readline().decode().strip()
        
        if response:
            try:
                return json.loads(response)
            except json.JSONDecodeError:
                return {"status": "error", "message": f"Invalid JSON response: {response}"}
        else:
            return {"status": "error", "message": "No response from Arduino"}
            
    except Exception as e:
        return {"status": "error", "message": f"Serial communication error: {str(e)}"}

def get_help() -> str:
    """Get help documentation from Arduino"""
    if arduino is None:
        return "Arduino not connected"
    
    try:
        arduino.write(b'help\n')
        time.sleep(0.1)
        
        help_text = ""
        while arduino.in_waiting:
            line = arduino.readline().decode().strip()
            help_text += line + "\n"
        
        return help_text
    except Exception as e:
        return f"Error getting help: {e}"

# ================================
# WEB INTERFACE ROUTES
# ================================

@app.get("/", response_class=HTMLResponse)
async def main_page():
    """Main web interface"""
    html_content = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>LED Controller API Tester</title>
        <meta charset="UTF-8">
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
            .container { max-width: 1200px; margin: 0 auto; }
            .header { background: #2c3e50; color: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; }
            .section { background: white; padding: 20px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
            .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
            .form-group { margin: 10px 0; }
            label { display: block; margin-bottom: 5px; font-weight: bold; }
            input, select, button { padding: 8px; margin: 2px; border: 1px solid #ddd; border-radius: 4px; }
            button { background: #3498db; color: white; cursor: pointer; padding: 10px 20px; }
            button:hover { background: #2980b9; }
            .response { background: #ecf0f1; padding: 10px; border-radius: 4px; margin-top: 10px; font-family: monospace; }
            .success { background: #d5f4e6; border-left: 4px solid #27ae60; }
            .error { background: #fadbd8; border-left: 4px solid #e74c3c; }
            .color-input { width: 60px; }
            .inline { display: inline-block; margin-right: 10px; }
            .help-section { background: #f8f9fa; border: 1px solid #dee2e6; }
        </style>
    </head>
    <body>
        <div class="container">
            <div class="header">
                <h1>🚦 LED Controller API Tester</h1>
                <p>Web interface for testing Arduino LED Controller API functions</p>
            </div>

            <div class="grid">
                <!-- RGB Strip Control -->
                <div class="section">
                    <h2>🌈 RGB Strip Control</h2>
                    
                    <h3>Single Pixel</h3>
                    <form onsubmit="return testRgbSingle(event)">
                        <div class="form-group">
                            <label>Strip:</label>
                            <select id="rgb_single_strip">
                                <option value="1">1 (Ring-Top)</option>
                                <option value="2">2 (Door)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Pixel (0-77):</label>
                            <input type="number" id="rgb_single_pixel" min="0" max="77" value="0">
                        </div>
                        <div class="form-group">
                            <div class="inline">
                                <label>R:</label>
                                <input type="number" id="rgb_single_r" min="0" max="255" value="255" class="color-input">
                            </div>
                            <div class="inline">
                                <label>G:</label>
                                <input type="number" id="rgb_single_g" min="0" max="255" value="0" class="color-input">
                            </div>
                            <div class="inline">
                                <label>B:</label>
                                <input type="number" id="rgb_single_b" min="0" max="255" value="0" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Set Single Pixel</button>
                    </form>

                    <h3>Pixel Range</h3>
                    <form onsubmit="return testRgbRange(event)">
                        <div class="form-group">
                            <label>Strip:</label>
                            <select id="rgb_range_strip">
                                <option value="1">1 (Ring-Top)</option>
                                <option value="2">2 (Door)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <div class="inline">
                                <label>Start:</label>
                                <input type="number" id="rgb_range_start" min="0" max="77" value="0">
                            </div>
                            <div class="inline">
                                <label>End:</label>
                                <input type="number" id="rgb_range_end" min="0" max="77" value="9">
                            </div>
                        </div>
                        <div class="form-group">
                            <div class="inline">
                                <label>R:</label>
                                <input type="number" id="rgb_range_r" min="0" max="255" value="0" class="color-input">
                            </div>
                            <div class="inline">
                                <label>G:</label>
                                <input type="number" id="rgb_range_g" min="0" max="255" value="255" class="color-input">
                            </div>
                            <div class="inline">
                                <label>B:</label>
                                <input type="number" id="rgb_range_b" min="0" max="255" value="0" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Set Range</button>
                    </form>

                    <h3>All Pixels / Clear</h3>
                    <form onsubmit="return testRgbAll(event)">
                        <div class="form-group">
                            <label>Strip:</label>
                            <select id="rgb_all_strip">
                                <option value="1">1 (Ring-Top)</option>
                                <option value="2">2 (Door)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <div class="inline">
                                <label>R:</label>
                                <input type="number" id="rgb_all_r" min="0" max="255" value="0" class="color-input">
                            </div>
                            <div class="inline">
                                <label>G:</label>
                                <input type="number" id="rgb_all_g" min="0" max="255" value="0" class="color-input">
                            </div>
                            <div class="inline">
                                <label>B:</label>
                                <input type="number" id="rgb_all_b" min="0" max="255" value="255" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Set All Pixels</button>
                        <button type="button" onclick="clearStrip()">Clear Strip</button>
                    </form>

                    <div id="rgb_response" class="response"></div>
                </div>

                <!-- LED Control -->
                <div class="section">
                    <h2>💡 LED Control</h2>
                    
                    <h3>Digital Control</h3>
                    <button onclick="testLedDigital(true)">LED ON</button>
                    <button onclick="testLedDigital(false)">LED OFF</button>

                    <h3>Analog Control</h3>
                    <form onsubmit="return testLedAnalog(event)">
                        <div class="form-group">
                            <label>Brightness (0-255):</label>
                            <input type="range" id="led_brightness" min="0" max="255" value="128" 
                                   oninput="document.getElementById('brightness_value').textContent = this.value">
                            <span id="brightness_value">128</span>
                        </div>
                        <button type="submit">Set Brightness</button>
                    </form>

                    <div id="led_response" class="response"></div>
                </div>
            </div>

            <div class="grid">
                <!-- Relay Control -->
                <div class="section">
                    <h2>🔌 Relay Control</h2>
                    
                    <h3>Relay 1 (Intercom)</h3>
                    <button onclick="testRelay(1, true)">Relay 1 ON</button>
                    <button onclick="testRelay(1, false)">Relay 1 OFF</button>

                    <h3>Relay 2 (General)</h3>
                    <button onclick="testRelay(2, true)">Relay 2 ON</button>
                    <button onclick="testRelay(2, false)">Relay 2 OFF</button>

                    <div id="relay_response" class="response"></div>
                </div>

                <!-- Sensor Reading -->
                <div class="section">
                    <h2>📊 Sensor Reading</h2>
                    
                    <h3>LB Sensor (Paper Full)</h3>
                    <button onclick="readSensor('lb', 'analog')">Read Analog Value</button>
                    <button onclick="readSensor('lb', 'digital')">Read Digital Value</button>

                    <h3>RS Sensor (Ticket Barrier)</h3>
                    <button onclick="readSensor('rs')">Read State</button>

                    <h3>Temperature Sensor (LM75)</h3>
                    <button onclick="readSensor('temp')">Read Temperature</button>

                    <div id="sensor_response" class="response"></div>
                </div>
            </div>

            <!-- Configuration -->
            <div class="section">
                <h2>⚙️ Configuration</h2>
                
                <h3>LB Sensor Threshold</h3>
                <form onsubmit="return setThreshold(event)">
                    <div class="form-group">
                        <label>Threshold (0-1023):</label>
                        <input type="number" id="threshold_value" min="0" max="1023" value="512">
                    </div>
                    <button type="submit">Set Threshold</button>
                </form>

                <div id="config_response" class="response"></div>
            </div>

            <!-- Help Section -->
            <div class="section help-section">
                <h2>📖 API Documentation</h2>
                <button onclick="showHelp()">Get Help from Arduino</button>
                <pre id="help_content"></pre>
            </div>
        </div>

        <script>
            // Helper function to make API calls
            async function callAPI(endpoint, data = {}) {
                try {
                    const response = await fetch(endpoint, {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(data)
                    });
                    return await response.json();
                } catch (error) {
                    return { status: 'error', message: 'Network error: ' + error.message };
                }
            }

            // Display response in UI
            function displayResponse(elementId, response) {
                const element = document.getElementById(elementId);
                element.className = 'response ' + (response.status === 'success' ? 'success' : 'error');
                element.textContent = JSON.stringify(response, null, 2);
            }

            // RGB Strip Functions
            async function testRgbSingle(event) {
                event.preventDefault();
                const data = {
                    action: 'rgb',
                    strip: parseInt(document.getElementById('rgb_single_strip').value),
                    mode: 'single',
                    pixel: parseInt(document.getElementById('rgb_single_pixel').value),
                    r: parseInt(document.getElementById('rgb_single_r').value),
                    g: parseInt(document.getElementById('rgb_single_g').value),
                    b: parseInt(document.getElementById('rgb_single_b').value)
                };
                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
                return false;
            }

            async function testRgbRange(event) {
                event.preventDefault();
                const data = {
                    action: 'rgb',
                    strip: parseInt(document.getElementById('rgb_range_strip').value),
                    mode: 'range',
                    start: parseInt(document.getElementById('rgb_range_start').value),
                    end: parseInt(document.getElementById('rgb_range_end').value),
                    r: parseInt(document.getElementById('rgb_range_r').value),
                    g: parseInt(document.getElementById('rgb_range_g').value),
                    b: parseInt(document.getElementById('rgb_range_b').value)
                };
                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
                return false;
            }

            async function testRgbAll(event) {
                event.preventDefault();
                const data = {
                    action: 'rgb',
                    strip: parseInt(document.getElementById('rgb_all_strip').value),
                    mode: 'all',
                    r: parseInt(document.getElementById('rgb_all_r').value),
                    g: parseInt(document.getElementById('rgb_all_g').value),
                    b: parseInt(document.getElementById('rgb_all_b').value)
                };
                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
                return false;
            }

            async function clearStrip() {
                const data = {
                    action: 'rgb',
                    strip: parseInt(document.getElementById('rgb_all_strip').value),
                    mode: 'clear'
                };
                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
            }

            // LED Functions
            async function testLedDigital(state) {
                const data = {
                    action: 'led',
                    mode: 'digital',
                    state: state
                };
                const response = await callAPI('/test/led', data);
                displayResponse('led_response', response);
            }

            async function testLedAnalog(event) {
                event.preventDefault();
                const data = {
                    action: 'led',
                    mode: 'analog',
                    value: parseInt(document.getElementById('led_brightness').value)
                };
                const response = await callAPI('/test/led', data);
                displayResponse('led_response', response);
                return false;
            }

            // Relay Functions
            async function testRelay(relay, state) {
                const data = {
                    action: 'relay',
                    relay: relay,
                    state: state
                };
                const response = await callAPI('/test/relay', data);
                displayResponse('relay_response', response);
            }

            // Sensor Functions
            async function readSensor(sensor, mode = null) {
                const data = {
                    action: 'read',
                    sensor: sensor
                };
                if (mode) {
                    data.mode = mode;
                }
                const response = await callAPI('/test/sensor', data);
                displayResponse('sensor_response', response);
            }

            // Configuration Functions
            async function setThreshold(event) {
                event.preventDefault();
                const data = {
                    action: 'config',
                    setting: 'lb_threshold',
                    value: parseInt(document.getElementById('threshold_value').value)
                };
                const response = await callAPI('/test/config', data);
                displayResponse('config_response', response);
                return false;
            }

            // Help Function
            async function showHelp() {
                const response = await callAPI('/help');
                document.getElementById('help_content').textContent = response.help || response.message;
            }
        </script>
    </body>
    </html>
    """
    return html_content

# ================================
# API TEST ENDPOINTS
# ================================

@app.post("/test/rgb")
async def test_rgb(command: dict):
    """Test RGB strip commands"""
    command_str = json.dumps(command)
    response = send_command(command_str)
    return response

@app.post("/test/led")
async def test_led(command: dict):
    """Test LED commands"""
    command_str = json.dumps(command)
    response = send_command(command_str)
    return response

@app.post("/test/relay")
async def test_relay(command: dict):
    """Test relay commands"""
    command_str = json.dumps(command)
    response = send_command(command_str)
    return response

@app.post("/test/sensor")
async def test_sensor(command: dict):
    """Test sensor reading commands"""
    command_str = json.dumps(command)
    response = send_command(command_str)
    return response

@app.post("/test/config")
async def test_config(command: dict):
    """Test configuration commands"""
    command_str = json.dumps(command)
    response = send_command(command_str)
    return response

@app.post("/help")
async def get_arduino_help():
    """Get help documentation from Arduino"""
    help_text = get_help()
    return {"help": help_text}

# ================================
# STATUS AND INFO ENDPOINTS
# ================================

@app.get("/status")
async def get_status():
    """Get connection status"""
    if arduino and arduino.is_open:
        return {"status": "connected", "port": SERIAL_PORT, "baud_rate": BAUD_RATE}
    else:
        return {"status": "disconnected", "message": "Arduino not connected"}

@app.get("/info")
async def get_info():
    """Get API information"""
    return {
        "title": "LED Controller API Tester",
        "version": "1.0.0",
        "description": "Web interface for testing Arduino LED Controller API",
        "arduino_port": SERIAL_PORT,
        "features": [
            "RGB Strip Control (single pixel, range, all pixels, clear)",
            "Single LED Control (digital/analog)",
            "Relay Control (2 relays)",
            "Sensor Reading (LB analog/digital, RS digital, LM75 temperature)",
            "Configuration (threshold setting)",
            "Complete API testing"
        ]
    }

# ================================
# MAIN EXECUTION
# ================================

if __name__ == "__main__":
    print("🚀 Starting LED Controller API Tester...")
    print(f"📡 Arduino Port: {SERIAL_PORT}")
    print(f"🌐 Web Interface: http://localhost:8000")
    print("📖 API Documentation: http://localhost:8000/docs")
    
    uvicorn.run(app, host="0.0.0.0", port=8000)
