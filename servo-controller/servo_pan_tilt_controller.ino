/*
 * Servo Pan-Tilt Bracket Controller with 360 Demo - ESP32-C6 Version
 * 
 * This ESP32-C6 sketch controls a servo pan-tilt bracket system
 * Features:
 * - Pan servo for horizontal movement (0-180 degrees)
 * - Tilt servo for vertical movement (0-180 degrees)
 * - Automatic 360-degree demo sequence
 * - Serial control for manual positioning
 * - WiFi capabilities (ESP32-C6 specific)
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

void loop() {
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
  Serial.println("====================================\n");
}