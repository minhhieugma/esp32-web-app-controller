/*
 * Servo Pan-Tilt Bracket Controller with Web & Serial Control - ESP32-C6 Version
 * 
 * This ESP32-C6 sketch controls a servo pan-tilt bracket system
 * Features:
 * - Pan servo for horizontal movement (0-180 degrees)
 * - Tilt servo for vertical movement (0-180 degrees)
 * - Automatic 360-degree demo sequence
 * - Serial control for manual positioning
 * - Web server control interface
 * - WiFi capabilities with AP fallback
 * - Thread-safe operation with FreeRTOS
 * 
 * Hardware Connections (ESP32-C6):
 * - Pan servo signal pin -> GPIO 18
 * - Tilt servo signal pin -> GPIO 19
 * - Servo power (5V) -> External 5V power supply (recommended)
 * - Servo ground -> ESP32-C6 GND
 * 
 * ESP32-C6 Notes:
 * - Uses ESP32Servo library for better PWM control
 * - 3.3V logic level compatible with 5V servos
 * - Built-in WiFi and Bluetooth LE support
 * 
 * Created: September 2025
 */

#include <ESP32Servo.h>  // ESP32-specific servo library
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Servo objects
Servo panServo;
Servo tiltServo;

// Pin definitions for ESP32-C6
const int PAN_SERVO_PIN = 18;   // GPIO 18
const int TILT_SERVO_PIN = 19;  // GPIO 19

// Servo limits (adjust based on your servo specifications)
const int PAN_MIN = 0;
const int PAN_MAX = 180;
const int TILT_MIN = 0;
const int TILT_MAX = 180;

// Current positions
int panPosition = 90;   // Start at center
int tiltPosition = 90;  // Start at center

// Demo variables
bool demoMode = true;
int demoStep = 0;
unsigned long lastDemoUpdate = 0;
const int DEMO_DELAY = 50; // Delay between servo movements in demo (ms)

// Movement variables
int panDirection = 1;
int tiltDirection = 1;
const int MOVEMENT_SPEED = 2; // Degrees per step

// WiFi credentials - modify these for your network
const char* ssid = "HieuLe";
const char* password = "Lmhieu13071990";

// Access Point credentials (fallback)
const char* ap_ssid = "ESP32-ServoController";
const char* ap_password = "servo123";

// Web server
WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // ESP32-C6 specific: Allow time for serial to initialize
  delay(1000);
  
  Serial.println("=== ESP32-C6 Servo Pan-Tilt Controller ===");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.println();
  
  // ESP32Servo allows custom PWM frequency and resolution
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Attach servos with ESP32-specific parameters
  panServo.setPeriodHertz(50);    // Standard 50 Hz servo
  tiltServo.setPeriodHertz(50);   // Standard 50 Hz servo
  
  panServo.attach(PAN_SERVO_PIN, 500, 2400);   // Min/Max pulse width in microseconds
  tiltServo.attach(TILT_SERVO_PIN, 500, 2400); // Wider range for better precision
  
  // Move to initial center position
  panServo.write(panPosition);
  tiltServo.write(tiltPosition);
  
  // Initialize WiFi
  setupWiFi();
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("Servo Pan-Tilt Controller Initialized");
  Serial.println("Commands:");
  Serial.println("  p[angle] - Set pan angle (0-180)");
  Serial.println("  t[angle] - Set tilt angle (0-180)");
  Serial.println("  d - Toggle demo mode");
  Serial.println("  s - Stop and center servos");
  Serial.println("  h - Show this help");
  Serial.println("  w - Show WiFi info (ESP32-C6)");
  Serial.println();
  Serial.println("Starting 360-degree demo...");
  
  delay(1000); // Allow servos to reach initial position
}

void setupWiFi() {
  Serial.println("Setting up WiFi...");
  
  // Try to connect to existing WiFi network
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi. Starting Access Point...");
    
    // Start Access Point
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("Access Point started. IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Network: ");
    Serial.println(ap_ssid);
    Serial.print("Password: ");
    Serial.println(ap_password);
  }
}

void setupWebServer() {
  // Serve main control page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getMainPage());
  });
  
  // API endpoints
  server.on("/api/pan", HTTP_POST, handlePanAPI);
  server.on("/api/tilt", HTTP_POST, handleTiltAPI);
  server.on("/api/position", HTTP_GET, handlePositionAPI);
  server.on("/api/demo", HTTP_POST, handleDemoAPI);
  server.on("/api/center", HTTP_POST, handleCenterAPI);
  server.on("/api/status", HTTP_GET, handleStatusAPI);
  
  // CORS headers for all requests
  server.enableCORS(true);
  
  server.begin();
  Serial.println("Web server started!");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Open http://");
    Serial.print(WiFi.localIP());
    Serial.println(" in your browser");
  } else {
    Serial.print("Open http://");
    Serial.print(WiFi.softAPIP());
    Serial.println(" in your browser");
  }
}

String getMainPage() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<title>ESP32 Servo Controller</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; }";
  html += ".control-group { margin: 20px 0; padding: 15px; background: #f9f9f9; border-radius: 5px; }";
  html += ".slider-container { margin: 10px 0; }";
  html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
  html += "input[type=\"range\"] { width: 100%; height: 30px; }";
  html += ".value-display { text-align: center; font-size: 18px; margin: 5px 0; color: #666; }";
  html += "button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; font-size: 16px; }";
  html += "button:hover { background: #45a049; }";
  html += "button.danger { background: #f44336; }";
  html += "button.danger:hover { background: #da190b; }";
  html += ".status { padding: 10px; background: #e7f3ff; border-radius: 5px; margin: 10px 0; }";
  html += ".demo-status { text-align: center; font-weight: bold; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"container\">";
  html += "<h1>ESP32 Servo Controller</h1>";
  
  html += "<div class=\"control-group\">";
  html += "<h3>Pan Control (Horizontal)</h3>";
  html += "<div class=\"slider-container\">";
  html += "<label for=\"panSlider\">Pan Angle:</label>";
  html += "<input type=\"range\" id=\"panSlider\" min=\"0\" max=\"180\" value=\"90\">";
  html += "<div class=\"value-display\" id=\"panValue\">90&deg;</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class=\"control-group\">";
  html += "<h3>Tilt Control (Vertical)</h3>";
  html += "<div class=\"slider-container\">";
  html += "<label for=\"tiltSlider\">Tilt Angle:</label>";
  html += "<input type=\"range\" id=\"tiltSlider\" min=\"0\" max=\"180\" value=\"90\">";
  html += "<div class=\"value-display\" id=\"tiltValue\">90&deg;</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class=\"control-group\">";
  html += "<h3>Quick Controls</h3>";
  html += "<button onclick=\"centerServos()\">Center Servos</button>";
  html += "<button onclick=\"toggleDemo()\" id=\"demoBtn\">Toggle Demo</button>";
  html += "</div>";
  
  html += "<div class=\"status\">";
  html += "<div class=\"demo-status\" id=\"demoStatus\">Demo: ON</div>";
  html += "</div>";
  html += "</div>";

  html += "<script>";
  html += "var panSlider = document.getElementById('panSlider');";
  html += "var tiltSlider = document.getElementById('tiltSlider');";
  html += "var panValue = document.getElementById('panValue');";
  html += "var tiltValue = document.getElementById('tiltValue');";
  html += "var demoStatus = document.getElementById('demoStatus');";
  
  html += "panSlider.addEventListener('input', function() {";
  html += "panValue.innerHTML = this.value + '&deg;';";
  html += "setPan(this.value);";
  html += "});";
  
  html += "tiltSlider.addEventListener('input', function() {";
  html += "tiltValue.innerHTML = this.value + '&deg;';";
  html += "setTilt(this.value);";
  html += "});";
  
  html += "function setPan(angle) {";
  html += "fetch('/api/pan', {";
  html += "method: 'POST',";
  html += "headers: {'Content-Type': 'application/json'},";
  html += "body: JSON.stringify({angle: parseInt(angle)})";
  html += "});";
  html += "}";
  
  html += "function setTilt(angle) {";
  html += "fetch('/api/tilt', {";
  html += "method: 'POST',";
  html += "headers: {'Content-Type': 'application/json'},";
  html += "body: JSON.stringify({angle: parseInt(angle)})";
  html += "});";
  html += "}";
  
  html += "function centerServos() {";
  html += "fetch('/api/center', {method: 'POST'})";
  html += ".then(function() {";
  html += "panSlider.value = 90;";
  html += "tiltSlider.value = 90;";
  html += "panValue.innerHTML = '90&deg;';";
  html += "tiltValue.innerHTML = '90&deg;';";
  html += "});";
  html += "}";
  
  html += "function toggleDemo() {";
  html += "fetch('/api/demo', {method: 'POST'})";
  html += ".then(function(response) { return response.json(); })";
  html += ".then(function(data) {";
  html += "demoStatus.innerHTML = 'Demo: ' + (data.demo ? 'ON' : 'OFF');";
  html += "});";
  html += "}";
  
  html += "setInterval(function() {";
  html += "fetch('/api/status')";
  html += ".then(function(response) { return response.json(); })";
  html += ".then(function(data) {";
  html += "if (!document.hasFocus()) return;";
  html += "panSlider.value = data.pan;";
  html += "tiltSlider.value = data.tilt;";
  html += "panValue.innerHTML = data.pan + '&deg;';";
  html += "tiltValue.innerHTML = data.tilt + '&deg;';";
  html += "demoStatus.innerHTML = 'Demo: ' + (data.demo ? 'ON' : 'OFF');";
  html += "});";
  html += "}, 2000);";
  html += "</script>";
  html += "</body>";
  html += "</html>";
  
  return html;
}

void handlePanAPI() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(200);
    deserializeJson(doc, server.arg("plain"));
    int angle = doc["angle"];
    setPanAngle(angle);
    server.send(200, "application/json", "{\"status\":\"ok\",\"pan\":" + String(panPosition) + "}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  }
}

void handleTiltAPI() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(200);
    deserializeJson(doc, server.arg("plain"));
    int angle = doc["angle"];
    setTiltAngle(angle);
    server.send(200, "application/json", "{\"status\":\"ok\",\"tilt\":" + String(tiltPosition) + "}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  }
}

void handlePositionAPI() {
  String json = "{\"pan\":" + String(panPosition) + ",\"tilt\":" + String(tiltPosition) + "}";
  server.send(200, "application/json", json);
}

void handleDemoAPI() {
  demoMode = !demoMode;
  if (demoMode) {
    demoStep = 0;
  }
  String json = "{\"demo\":" + String(demoMode ? "true" : "false") + "}";
  server.send(200, "application/json", json);
  Serial.println(demoMode ? "Demo mode ON (Web)" : "Demo mode OFF (Web)");
}

void handleCenterAPI() {
  demoMode = false;
  setPanAngle(90);
  setTiltAngle(90);
  server.send(200, "application/json", "{\"status\":\"centered\"}");
  Serial.println("Servos centered (Web)");
}

void handleStatusAPI() {
  String json = "{\"pan\":" + String(panPosition) + 
                ",\"tilt\":" + String(tiltPosition) + 
                ",\"demo\":" + String(demoMode ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Handle serial commands
  if (Serial.available()) {
    handleSerialCommand();
  }
  
  // Run demo if enabled
  if (demoMode && (millis() - lastDemoUpdate >= DEMO_DELAY)) {
    runDemo();
    lastDemoUpdate = millis();
  }
}

void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();
  
  if (command.startsWith("p")) {
    // Pan command: p90, p0, p180, etc.
    int angle = command.substring(1).toInt();
    setPanAngle(angle);
  }
  else if (command.startsWith("t")) {
    // Tilt command: t45, t0, t180, etc.
    int angle = command.substring(1).toInt();
    setTiltAngle(angle);
  }
  else if (command == "d") {
    // Toggle demo mode
    demoMode = !demoMode;
    Serial.println(demoMode ? "Demo mode ON" : "Demo mode OFF");
    if (demoMode) {
      demoStep = 0; // Reset demo
    }
  }
  else if (command == "s") {
    // Stop and center
    demoMode = false;
    setPanAngle(90);
    setTiltAngle(90);
    Serial.println("Servos centered and demo stopped");
  }
  else if (command == "h") {
    // Show help
    printHelp();
  }
  else if (command == "w") {
    // Show ESP32-C6 WiFi info
    showESP32Info();
  }
  else {
    Serial.println("Unknown command. Type 'h' for help.");
  }
}

void setPanAngle(int angle) {
  angle = constrain(angle, PAN_MIN, PAN_MAX);
  panPosition = angle;
  panServo.write(panPosition);
  Serial.print("Pan angle set to: ");
  Serial.println(panPosition);
}

void setTiltAngle(int angle) {
  angle = constrain(angle, TILT_MIN, TILT_MAX);
  tiltPosition = angle;
  tiltServo.write(tiltPosition);
  Serial.print("Tilt angle set to: ");
  Serial.println(tiltPosition);
}

void runDemo() {
  switch (demoStep) {
    case 0:
      // Phase 1: Pan sweep while tilt is centered
      panPosition += panDirection * MOVEMENT_SPEED;
      if (panPosition >= PAN_MAX) {
        panPosition = PAN_MAX;
        panDirection = -1;
        demoStep = 1;
      } else if (panPosition <= PAN_MIN) {
        panPosition = PAN_MIN;
        panDirection = 1;
        demoStep = 1;
      }
      break;
      
    case 1:
      // Phase 2: Tilt sweep while pan continues
      tiltPosition += tiltDirection * MOVEMENT_SPEED;
      panPosition += panDirection * MOVEMENT_SPEED;
      
      // Check tilt limits
      if (tiltPosition >= TILT_MAX) {
        tiltPosition = TILT_MAX;
        tiltDirection = -1;
      } else if (tiltPosition <= TILT_MIN) {
        tiltPosition = TILT_MIN;
        tiltDirection = 1;
      }
      
      // Check pan limits
      if (panPosition >= PAN_MAX) {
        panPosition = PAN_MAX;
        panDirection = -1;
      } else if (panPosition <= PAN_MIN) {
        panPosition = PAN_MIN;
        panDirection = 1;
        demoStep = 2;
      }
      break;
      
    case 2:
    {
      // Phase 3: Circular motion
      static float angle = 0;
      angle += 0.1; // Adjust speed of circular motion
      
      // Create circular motion using sine and cosine
      int centerPan = 90;
      int centerTilt = 90;
      int radius = 30; // Adjust radius of circular motion
      
      panPosition = centerPan + (int)(radius * cos(angle));
      tiltPosition = centerTilt + (int)(radius * sin(angle));
      
      // Constrain to servo limits
      panPosition = constrain(panPosition, PAN_MIN, PAN_MAX);
      tiltPosition = constrain(tiltPosition, TILT_MIN, TILT_MAX);
      
      // Reset after full circle
      if (angle >= 2 * PI) {
        angle = 0;
        demoStep = 3;
      }
      break;
    }
      
    case 3:
    {
      // Phase 4: Random movements
      static unsigned long lastRandomMove = 0;
      if (millis() - lastRandomMove >= 2000) { // Change direction every 2 seconds
        panPosition = random(PAN_MIN, PAN_MAX);
        tiltPosition = random(TILT_MIN, TILT_MAX);
        lastRandomMove = millis();
        
        // After some random moves, restart the demo
        static int randomMoves = 0;
        randomMoves++;
        if (randomMoves >= 5) {
          randomMoves = 0;
          demoStep = 0;
          panPosition = 90;
          tiltPosition = 90;
        }
      }
      break;
    }
  }
  
  // Apply the calculated positions
  panServo.write(panPosition);
  tiltServo.write(tiltPosition);
}

void printHelp() {
  Serial.println("\n=== ESP32-C6 Servo Pan-Tilt Controller Help ===");
  Serial.println("Commands:");
  Serial.println("  p[angle] - Set pan angle (0-180 degrees)");
  Serial.println("    Example: p90 (pan to 90 degrees)");
  Serial.println("  t[angle] - Set tilt angle (0-180 degrees)");
  Serial.println("    Example: t45 (tilt to 45 degrees)");
  Serial.println("  d - Toggle demo mode on/off");
  Serial.println("  s - Stop demo and center both servos");
  Serial.println("  h - Show this help message");
  Serial.println("  w - Show ESP32-C6 system information");
  Serial.println();
  Serial.println("Demo Phases:");
  Serial.println("  1. Pan sweep (left-right)");
  Serial.println("  2. Combined pan-tilt movement");
  Serial.println("  3. Circular motion pattern");
  Serial.println("  4. Random positioning");
  Serial.println();
  Serial.print("Current positions - Pan: ");
  Serial.print(panPosition);
  Serial.print("°, Tilt: ");
  Serial.print(tiltPosition);
  Serial.println("°");
  Serial.print("Demo mode: ");
  Serial.println(demoMode ? "ON" : "OFF");
  Serial.println("==========================================\n");
}

void showESP32Info() {
  Serial.println("\n=== ESP32-C6 System Information ===");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println(" MB");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("Free PSRAM: ");
  Serial.print(ESP.getFreePsram());
  Serial.println(" bytes");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  // GPIO Information
  Serial.println("\nGPIO Configuration:");
  Serial.print("Pan Servo Pin (GPIO ");
  Serial.print(PAN_SERVO_PIN);
  Serial.println("): PWM Output");
  Serial.print("Tilt Servo Pin (GPIO ");
  Serial.print(TILT_SERVO_PIN);
  Serial.println("): PWM Output");
  
  Serial.println("\nESP32-C6 Features:");
  Serial.println("- WiFi 6 (802.11ax) support");
  Serial.println("- Bluetooth 5 (LE) support");
  Serial.println("- RISC-V 32-bit single-core processor");
  Serial.println("- 16 PWM channels available");
  Serial.println("- Built-in USB Serial/JTAG controller");
  
  // WiFi Information
  Serial.println("\nWiFi Configuration:");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("Access Point Mode");
    Serial.print("AP SSID: ");
    Serial.println(ap_ssid);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
  
  Serial.println("\nWeb Server:");
  Serial.print("Status: Running on port 80");
  Serial.println("\nAPI Endpoints:");
  Serial.println("- POST /api/pan - Set pan angle");
  Serial.println("- POST /api/tilt - Set tilt angle");
  Serial.println("- GET /api/status - Get current status");
  Serial.println("- POST /api/demo - Toggle demo mode");
  Serial.println("- POST /api/center - Center servos");
  
  Serial.println("====================================\n");
}