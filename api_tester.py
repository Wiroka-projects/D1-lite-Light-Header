"""
LED Controller API Tester with FastAPI Web Interface
===================================================

This FastAPI application provides a web interface to test all functions
of the LED Controller API running on an Arduino/ESP8266.

Features:
- Test all RGB strip operations (single pixel, range, all, clear)
- Test RGB animations (running, charging, center fill, rainbow, flash, random)
- Control single LED (digital/analog)
- Control relays
- Read sensors (LB analog/digital, RS digital, Temperature LM75)
- Configure system settings and animation defaults
- Real-time serial communication with the Arduino

Requirements:
pip install fastapi uvicorn pyserial

Usage:
1. Connect your Arduino/ESP8266 to a COM port
2. Update the SERIAL_PORT variable below
3. Run: python api_tester.py
4. Open browser: http://localhost:8000
"""

from fastapi import FastAPI, Request, HTTPException, Form
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import serial
import json
import time
from typing import Optional
import uvicorn
import argparse
import sys

# ================================
# COMMAND LINE ARGUMENTS
# ================================
parser = argparse.ArgumentParser(description='LED Controller API Tester')
parser.add_argument('--port', type=str, help='Serial port for Arduino (e.g., COM10, /dev/ttyUSB0)')
args = parser.parse_args()

# ================================
# CONFIGURATION
# ================================
SERIAL_PORT = args.port  # From command line argument
BAUD_RATE = 115200
TIMEOUT = 2  # Serial timeout in seconds

# ================================
# SERIAL CONNECTION
# ================================
def get_serial_port():
    """Get serial port from argument or prompt user"""
    port = SERIAL_PORT
    if not port:
        print("Available serial ports:")
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        for p in ports:
            print(f"  {p.device}: {p.description}")
        
        port = input("Enter Arduino serial port: ").strip()
        if not port:
            print("No port specified. Exiting.")
            sys.exit(1)
    return port

arduino = None
try:
    port = get_serial_port()
    arduino = serial.Serial(port, BAUD_RATE, timeout=TIMEOUT)
    time.sleep(2)  # Wait for Arduino to initialize
    print(f"✅ Connected to Arduino on {port}")
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
                    <form onsubmit="testRgbSingle(event); return false;">
                        <div class="form-group">
                            <label>Strip:</label>
                            <select id="rgb_single_strip">
                                <option value="1">1 (Ring-Top)</option>
                                <option value="2">2 (Door)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Pixel (0-500):</label>
                            <input type="number" id="rgb_single_pixel" min="0" max="500" value="0">
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
                            <div class="inline">
                                <label>Bright:</label>
                                <input type="number" id="rgb_single_brightness" min="0" max="255" value="255" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Set Single Pixel</button>
                    </form>

                    <h3>Pixel Range</h3>
                    <form onsubmit="testRgbRange(event); return false;">
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
                                <input type="number" id="rgb_range_start" min="0" max="500" value="0">
                            </div>
                            <div class="inline">
                                <label>End:</label>
                                <input type="number" id="rgb_range_end" min="0" max="500" value="9">
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
                            <div class="inline">
                                <label>Bright:</label>
                                <input type="number" id="rgb_range_brightness" min="0" max="255" value="255" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Set Range</button>
                    </form>

                    <h3>All Pixels / Clear</h3>
                    <form onsubmit="testRgbAll(event); return false;">
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
                            <div class="inline">
                                <label>Bright:</label>
                                <input type="number" id="rgb_all_brightness" min="0" max="255" value="255" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Set All Pixels</button>
                        <button type="button" onclick="clearStrip()">Clear Strip</button>
                    </form>

                    <h3>Animated Effects</h3>
                    <form onsubmit="testRgbEffect(event); return false;">
                        <div class="form-group">
                            <label>Strip:</label>
                            <select id="rgb_effect_strip">
                                <option value="1">1 (Ring-Top)</option>
                                <option value="2">2 (Door)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Effect:</label>
                            <select id="rgb_effect_mode">
                                <option value="running">Running</option>
                                <option value="charging">Charging</option>
                                <option value="center_fill">Center Fill</option>
                                <option value="rainbow">Rainbow</option>
                                <option value="flash">Flash</option>
                                <option value="random">Random</option>
                                <option value="breathing">Breathing</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <div class="inline">
                                <label>R:</label>
                                <input type="number" id="rgb_effect_r" min="0" max="255" value="255" class="color-input">
                            </div>
                            <div class="inline">
                                <label>G:</label>
                                <input type="number" id="rgb_effect_g" min="0" max="255" value="120" class="color-input">
                            </div>
                            <div class="inline">
                                <label>B:</label>
                                <input type="number" id="rgb_effect_b" min="0" max="255" value="40" class="color-input">
                            </div>
                            <div class="inline">
                                <label>Bright:</label>
                                <input type="number" id="rgb_effect_brightness" min="0" max="255" value="180" class="color-input">
                            </div>
                        </div>
                        <div class="form-group">
                            <div class="inline">
                                <label>Time ms:</label>
                                <input type="number" id="rgb_effect_time" min="1" max="10000" value="40" class="color-input">
                            </div>
                            <div class="inline">
                                <label>Repeat:</label>
                                <input type="number" id="rgb_effect_repeat" min="0" max="10000" value="0" class="color-input">
                            </div>
                            <div class="inline">
                                <label>Count:</label>
                                <input type="number" id="rgb_effect_count" min="1" max="1000" value="12" class="color-input">
                            </div>
                        </div>
                        <button type="submit">Start Effect</button>
                        <button type="button" onclick="stopRgbEffect()">Stop / Default</button>
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
                    <form onsubmit="testLedAnalog(event); return false;">
                        <div class="form-group">
                            <label>Brightness (0-255):</label>
                        <input type="range" id="led_brightness" min="0" max="255" value="128" 
                               oninput="document.getElementById('led_brightness_display').textContent = this.value">
                        <span id="led_brightness_display">128</span>
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
                
                <h3>RGB Strip Brightness</h3>
                <form onsubmit="setRgbBrightness(event); return false;">
                    <div class="form-group">
                        <label>Strip:</label>
                        <select id="brightness_strip">
                            <option value="1">1 (Ring-Top)</option>
                            <option value="2">2 (Door)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Brightness (0-255):</label>
                        <input type="range" id="brightness_value" min="0" max="255" value="255" 
                               oninput="document.getElementById('brightness_display').textContent = this.value">
                        <span id="brightness_display">255</span>
                    </div>
                    <button type="submit">Set Brightness</button>
                </form>

                <h3>RGB Strip Pixel Counts</h3>
                <form onsubmit="setRgbPixels(event); return false;">
                    <div class="form-group">
                        <label>Strip:</label>
                        <select id="pixels_strip">
                            <option value="1">1 (Ring-Top)</option>
                            <option value="2">2 (Door)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Pixels (1-1000):</label>
                        <input type="number" id="pixels_value" min="1" max="1000" value="150">
                    </div>
                    <button type="submit">Set Pixel Count</button>
                </form>

                <h3>RGB Strip Default Colors</h3>
                <form onsubmit="setRgbDefaultColor(event); return false;">
                    <div class="form-group">
                        <label>Strip:</label>
                        <select id="default_strip">
                            <option value="1">1 (Ring-Top)</option>
                            <option value="2">2 (Door)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <div class="inline">
                            <label>R:</label>
                            <input type="number" id="default_r" min="0" max="255" value="255" class="color-input">
                        </div>
                        <div class="inline">
                            <label>G:</label>
                            <input type="number" id="default_g" min="0" max="255" value="255" class="color-input">
                        </div>
                        <div class="inline">
                            <label>B:</label>
                            <input type="number" id="default_b" min="0" max="255" value="255" class="color-input">
                        </div>
                    </div>
                    <button type="submit">Set Default Color</button>
                </form>

                <h3>LED Default Brightness</h3>
                <form onsubmit="setLedDefault(event); return false;">
                    <div class="form-group">
                        <label>Default Brightness (0-255):</label>
                        <input type="range" id="led_default_value" min="0" max="255" value="0" 
                               oninput="document.getElementById('led_default_display').textContent = this.value">
                        <span id="led_default_display">0</span>
                    </div>
                    <button type="submit">Set LED Default</button>
                </form>

                <h3>Startup Behavior</h3>
                <form onsubmit="setStartupBehavior(event); return false;">
                    <div class="form-group">
                        <label>Strip:</label>
                        <select id="startup_strip">
                            <option value="1">1 (Ring-Top)</option>
                            <option value="2">2 (Door)</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Boot Mode:</label>
                        <select id="startup_mode">
                            <option value="solid">Solid color</option>
                            <option value="effect">Effect</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Boot Effect:</label>
                        <select id="startup_effect">
                            <option value="running">Running</option>
                            <option value="charging">Charging</option>
                            <option value="center_fill">Center fill</option>
                            <option value="rainbow">Rainbow</option>
                            <option value="flash">Flash</option>
                            <option value="random">Random</option>
                            <option value="breathing">Breathing</option>
                        </select>
                    </div>
                    <button type="submit">Save Startup Behavior</button>
                </form>

                <h3>RGB Effect Defaults</h3>
                <form onsubmit="setRgbEffectDefault(event); return false;">
                    <div class="form-group">
                        <label>Strip:</label>
                        <select id="effect_default_strip">
                            <option value="1">1 (Ring-Top)</option>
                            <option value="2">2 (Door)</option>
                            <option value="0">Both strips</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Effect:</label>
                        <select id="effect_default_mode">
                            <option value="running">Running default</option>
                            <option value="charging">Charging default</option>
                            <option value="center_fill">Center fill default</option>
                            <option value="rainbow">Rainbow default</option>
                            <option value="flash">Flash default</option>
                            <option value="random">Random default</option>
                            <option value="breathing">Breathing default</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <div class="inline">
                            <label>R:</label>
                            <input type="number" id="effect_default_r" min="0" max="255" value="255" class="color-input">
                        </div>
                        <div class="inline">
                            <label>G:</label>
                            <input type="number" id="effect_default_g" min="0" max="255" value="120" class="color-input">
                        </div>
                        <div class="inline">
                            <label>B:</label>
                            <input type="number" id="effect_default_b" min="0" max="255" value="40" class="color-input">
                        </div>
                        <div class="inline">
                            <label>Bright:</label>
                            <input type="number" id="effect_default_brightness" min="0" max="255" value="180" class="color-input">
                        </div>
                    </div>
                    <div class="form-group">
                        <div class="inline">
                            <label>Time ms:</label>
                            <input type="number" id="effect_default_time" min="1" max="10000" value="40" class="color-input">
                        </div>
                        <div class="inline">
                            <label>Repeat:</label>
                            <input type="number" id="effect_default_repeat" min="0" max="10000" value="0" class="color-input">
                        </div>
                        <div class="inline">
                            <label>Count:</label>
                            <input type="number" id="effect_default_count" min="1" max="1000" value="12" class="color-input">
                        </div>
                    </div>
                    <button type="submit">Save Effect Default</button>
                </form>

                <div id="config_response" class="response"></div>
            </div>

            <!-- Help Section -->
            <div class="section help-section">
                <h2>📖 API Documentation</h2>
                <button onclick="showHelp()">Get Help from Arduino</button>
                <button onclick="getStatus()" style="background:#27ae60">Get Device Status</button>
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
                    b: parseInt(document.getElementById('rgb_single_b').value),
                    brightness: parseInt(document.getElementById('rgb_single_brightness').value)
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
                    b: parseInt(document.getElementById('rgb_range_b').value),
                    brightness: parseInt(document.getElementById('rgb_range_brightness').value)
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
                    b: parseInt(document.getElementById('rgb_all_b').value),
                    brightness: parseInt(document.getElementById('rgb_all_brightness').value)
                };
                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
                return false;
            }

            async function testRgbEffect(event) {
                event.preventDefault();
                const strip = parseInt(document.getElementById('rgb_effect_strip').value);
                const mode = document.getElementById('rgb_effect_mode').value;
                const data = {
                    action: 'rgb',
                    strip: strip,
                    mode: mode,
                    r: parseInt(document.getElementById('rgb_effect_r').value),
                    g: parseInt(document.getElementById('rgb_effect_g').value),
                    b: parseInt(document.getElementById('rgb_effect_b').value),
                    brightness: parseInt(document.getElementById('rgb_effect_brightness').value),
                    time: parseInt(document.getElementById('rgb_effect_time').value),
                    repeatingtime: parseInt(document.getElementById('rgb_effect_repeat').value),
                    count: parseInt(document.getElementById('rgb_effect_count').value)
                };

                if (mode === 'rainbow') {
                    delete data.r;
                    delete data.g;
                    delete data.b;
                }

                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
                return false;
            }

            async function stopRgbEffect() {
                const data = {
                    action: 'rgb',
                    strip: parseInt(document.getElementById('rgb_effect_strip').value),
                    mode: 'default'
                };
                const response = await callAPI('/test/rgb', data);
                displayResponse('rgb_response', response);
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

            async function setRgbPixels(event) {
                event.preventDefault();
                const strip = parseInt(document.getElementById('pixels_strip').value);
                const pixels = parseInt(document.getElementById('pixels_value').value);
                const response = await callAPI('/test/config/pixels', { strip, pixels });
                displayResponse('config_response', response);
                return false;
            }

            async function setRgbBrightness(event) {
                event.preventDefault();
                const strip = parseInt(document.getElementById('brightness_strip').value);
                const brightness = parseInt(document.getElementById('brightness_value').value);
                const response = await callAPI('/test/config/brightness', { strip, brightness });
                displayResponse('config_response', response);
                return false;
            }

            async function setRgbDefaultColor(event) {
                event.preventDefault();
                const strip = parseInt(document.getElementById('default_strip').value);
                const r = parseInt(document.getElementById('default_r').value);
                const g = parseInt(document.getElementById('default_g').value);
                const b = parseInt(document.getElementById('default_b').value);
                const response = await callAPI('/test/config/default_color', { strip, r, g, b });
                displayResponse('config_response', response);
                return false;
            }

            async function setLedDefault(event) {
                event.preventDefault();
                const brightness = parseInt(document.getElementById('led_default_value').value);
                const response = await callAPI('/test/config/led_default', { brightness });
                displayResponse('config_response', response);
                return false;
            }

            async function setRgbEffectDefault(event) {
                event.preventDefault();
                const strip = parseInt(document.getElementById('effect_default_strip').value);
                const mode = document.getElementById('effect_default_mode').value;
                const settingMap = {
                    running: 'running_default',
                    charging: 'charging_default',
                    center_fill: 'center_default',
                    rainbow: 'rainbow_default',
                    flash: 'flash_default',
                    random: 'random_default',
                    breathing: 'breathing_default'
                };
                const data = {
                    action: 'config',
                    setting: settingMap[mode],
                    strip: strip,
                    r: parseInt(document.getElementById('effect_default_r').value),
                    g: parseInt(document.getElementById('effect_default_g').value),
                    b: parseInt(document.getElementById('effect_default_b').value),
                    brightness: parseInt(document.getElementById('effect_default_brightness').value),
                    time: parseInt(document.getElementById('effect_default_time').value),
                    repeatingtime: parseInt(document.getElementById('effect_default_repeat').value),
                    count: parseInt(document.getElementById('effect_default_count').value)
                };

                if (mode === 'rainbow') {
                    delete data.r;
                    delete data.g;
                    delete data.b;
                }

                const response = await callAPI('/test/config', data);
                displayResponse('config_response', response);
                return false;
            }

            async function setStartupBehavior(event) {
                event.preventDefault();
                const strip = parseInt(document.getElementById('startup_strip').value);
                const mode = document.getElementById('startup_mode').value;
                const effect = document.getElementById('startup_effect').value;

                const modeResponse = await callAPI('/test/config', {
                    action: 'config',
                    setting: 'startup_mode',
                    strip: strip,
                    value: mode
                });

                if (mode === 'effect') {
                    const effectResponse = await callAPI('/test/config', {
                        action: 'config',
                        setting: 'startup_effect',
                        strip: strip,
                        value: effect
                    });
                    displayResponse('config_response', effectResponse);
                } else {
                    displayResponse('config_response', modeResponse);
                }

                return false;
            }

            async function showHelp() {
                const response = await callAPI('/help');
                document.getElementById('help_content').textContent = response.help || response.message;
            }

            async function getStatus() {
                const response = await callAPI('/test/status', {});
                document.getElementById('help_content').textContent = JSON.stringify(response, null, 2);
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

@app.post("/test/status")
async def test_status(request: Request):
    """Get device status and current configuration"""
    data_cmd = {"action": "status"}
    response = send_command(json.dumps(data_cmd))
    return response

@app.post("/test/config/pixels")
async def test_config_pixels(request: Request):
    """Set pixel count for RGB strip"""
    data = await request.json()
    strip = data.get("strip")
    pixels = data.get("pixels")
    if strip is None or pixels is None:
        return {"status": "error", "message": "Missing strip or pixels parameter"}
    data_cmd = {
        "action": "config",
        "setting": f"rgb{strip}_pixels",
        "value": pixels
    }
    response = send_command(json.dumps(data_cmd))
    return response

@app.post("/test/config/brightness")
async def test_config_brightness(request: Request):
    """Set brightness for RGB strip"""
    data = await request.json()
    strip = data.get("strip")
    brightness = data.get("brightness")
    print(f"Setting brightness for RGB strip {strip}: {brightness}; {data}")
    if strip is None or brightness is None:
        return {"status": "error", "message": "Missing strip or brightness parameter"}
    data_cmd = {
        "action": "config",
        "setting": f"rgb{strip}_brightness",
        "value": brightness
    }
    response = send_command(json.dumps(data_cmd))
    return response

@app.post("/test/config/default_color")
async def test_config_default_color(request: Request):
    """Set default color for RGB strip"""
    data = await request.json()
    strip = data.get("strip")
    r = data.get("r")
    g = data.get("g")
    b = data.get("b")
    if strip is None or r is None or g is None or b is None:
        return {"status": "error", "message": "Missing strip, r, g, or b parameter"}
    data_cmd = {
        "action": "config",
        "setting": f"rgb{strip}_default_color",
        "r": r,
        "g": g,
        "b": b
    }
    response = send_command(json.dumps(data_cmd))
    return response

@app.post("/test/config/led_default")
async def test_config_led_default(request: Request):
    """Set LED default brightness"""
    data = await request.json()
    brightness = data.get("brightness")
    if brightness is None:
        return {"status": "error", "message": "Missing brightness parameter"}
    data_cmd = {
        "action": "config",
        "setting": "led_default",
        "value": brightness
    }
    response = send_command(json.dumps(data_cmd))
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
        return {"status": "connected", "port": arduino.port, "baud_rate": BAUD_RATE}
    else:
        return {"status": "disconnected", "message": "Arduino not connected"}

@app.get("/info")
async def get_info():
    """Get API information"""
    return {
        "title": "LED Controller API Tester",
        "version": "1.0.0",
        "description": "Web interface for testing Arduino LED Controller API",
        "arduino_port": arduino.port if arduino else "Not connected",
        "features": [
            "RGB Strip Control (single pixel, range, all pixels, clear)",
            "RGB Animations (running, charging, center fill, rainbow, flash, random)",
            "Single LED Control (digital/analog)",
            "Relay Control (2 relays)",
            "Sensor Reading (LB analog/digital, RS digital, LM75 temperature)",
            "Configuration (threshold, pixel counts, default colors, brightness, animation defaults)",
            "Complete API testing"
        ]
    }

# ================================
# MAIN EXECUTION
# ================================

if __name__ == "__main__":
    print("🚀 Starting LED Controller API Tester...")
    if arduino:
        print(f"📡 Arduino Port: {arduino.port}")
    else:
        print("📡 Arduino not connected")
    print(f"🌐 Web Interface: http://localhost:8090")
    print("📖 API Documentation: http://localhost:8090/docs")
    
    uvicorn.run(app, host="0.0.0.0", port=8090)
