# ESP32-C6 Servo Pan-Tilt Bracket Controller

This project provides ESP32-C6 optimized code to control a servo pan-tilt bracket with automatic 360-degree demo functionality.

## 🚀 ESP32-C6 Specific Features

- **High Performance**: RISC-V 32-bit processor running at up to 160 MHz
- **WiFi 6 Ready**: Built-in 802.11ax WiFi support for future web control
- **Bluetooth LE**: Integrated Bluetooth 5.0 Low Energy
- **Advanced PWM**: 16 independent PWM channels with precise servo control
- **Low Power**: Optimized power management for battery operation
- **USB Programming**: Built-in USB Serial/JTAG controller

## 🎯 Features

- **Dual Servo Control**: Independent control of pan (horizontal) and tilt (vertical) servos
- **360° Demo Mode**: Automated demonstration with multiple movement patterns:
  - Pan sweep (left-right scanning)
  - Combined pan-tilt movement
  - Circular motion patterns
  - Random positioning
- **Serial Control**: Manual control via Arduino IDE Serial Monitor
- **System Monitoring**: Real-time ESP32-C6 system information
- **Safety Limits**: Built-in angle constraints to protect servos
- **Thread-Safe**: Utilizes FreeRTOS for smooth operation

## 📋 Hardware Requirements

### Components Needed
- **ESP32-C6 Development Board** (ESP32-C6-DevKitC-1 recommended)
- **2x SG90 Servo Motors** (or similar 180° servos)
- **Pan-Tilt Bracket** (compatible with your servos)
- **Jumper Wires** (Female-to-Male for breadboard connections)
- **Breadboard** (optional, for prototyping)
- **External 5V Power Supply** (2A+ recommended for reliable servo operation)
- **Logic Level Converter** (optional, though ESP32-C6 is 5V tolerant on most pins)

### 🔌 Wiring Diagram (ESP32-C6)

```
ESP32-C6 DevKit    |    Pan Servo (SG90)    |    Tilt Servo (SG90)
-------------------|------------------------|-------------------------
GPIO 18            |    Signal (Orange)     |    
GPIO 19            |                        |    Signal (Orange)  
External 5V        |    VCC (Red)           |    VCC (Red)
GND                |    GND (Brown/Black)   |    GND (Brown/Black)
```

**ESP32-C6 Pinout Notes:**
- GPIO 18 & 19: PWM-capable pins with excellent servo control
- 3.3V Logic: Compatible with 5V servo signal inputs
- USB-C: For programming and serial communication
- Built-in LED: Usually on GPIO8 (may vary by board)

## 💻 Software Setup

### 1. Install Arduino IDE
Download from: https://www.arduino.cc/en/software

### 2. Install ESP32 Board Package
1. Open Arduino IDE
2. Go to **File** → **Preferences**
3. Add this URL to "Additional Board Manager URLs":
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json
   ```
4. Go to **Tools** → **Board** → **Boards Manager**
5. Search for "ESP32" and install **"esp32" by Espressif Systems**
6. Select **Board**: "ESP32C6 Dev Module"

### 3. Install Required Library
1. Go to **Tools** → **Library Manager**
2. Search for "ESP32Servo"
3. Install **"ESP32Servo" by Kevin Harrington**

### 4. Upload Code
1. Connect ESP32-C6 to computer via USB-C
2. Select correct **Board**: Tools → Board → ESP32 Arduino → ESP32C6 Dev Module
3. Select correct **Port**: Tools → Port → (your COM port)
4. Open `servo_pan_tilt_controller.ino`
5. Click **Upload** button (→)

## 🎮 Usage

### Demo Mode (Default)
The system starts in demo mode automatically. The ESP32-C6 will display system information and run through these phases:

1. **Pan Sweep**: Servo sweeps horizontally from 0° to 180°
2. **Combined Movement**: Both servos move in coordinated patterns
3. **Circular Motion**: Creates smooth circular scanning patterns
4. **Random Positioning**: Moves to random positions for variety

### Manual Control
Open Serial Monitor (**Tools** → **Serial Monitor**, set to **115200 baud**):

| Command | Description | Example |
|---------|-------------|---------|
| `p[angle]` | Set pan angle (0-180°) | `p90` (center), `p0` (left), `p180` (right) |
| `t[angle]` | Set tilt angle (0-180°) | `t45` (up), `t90` (center), `t135` (down) |
| `d` | Toggle demo mode on/off | `d` |
| `s` | Stop and center servos | `s` |
| `h` | Show help menu | `h` |
| `w` | Show ESP32-C6 system info | `w` |

### Example Commands
```
p0      // Pan to leftmost position
t180    // Tilt to lowest position  
p90     // Pan to center
t90     // Tilt to center
d       // Start/stop demo
s       // Emergency stop and center
w       // Show system information
```

## ⚙️ Configuration

### ESP32-C6 Specific Settings
```cpp
// GPIO Pins (can be changed to any PWM-capable pin)
const int PAN_SERVO_PIN = 18;   // GPIO 18
const int TILT_SERVO_PIN = 19;  // GPIO 19

// ESP32-specific servo parameters
panServo.setPeriodHertz(50);    // 50 Hz PWM frequency
panServo.attach(PAN_SERVO_PIN, 500, 2400);  // Min/Max pulse width (μs)
```

### Adjusting Servo Limits
```cpp
const int PAN_MIN = 0;     // Minimum pan angle
const int PAN_MAX = 180;   // Maximum pan angle
const int TILT_MIN = 0;    // Minimum tilt angle
const int TILT_MAX = 180;  // Maximum tilt angle
```

### Performance Tuning
```cpp
const int DEMO_DELAY = 50;      // Delay between movements (ms)
const int MOVEMENT_SPEED = 2;   // Degrees per step
```

## 🔧 Troubleshooting

### ESP32-C6 Specific Issues

#### Boot Loop or Won't Program
- Hold **BOOT** button while connecting USB
- Try different USB-C cable
- Check if CH340/CP210x drivers are installed
- Use "ESP32C6 Dev Module" board selection

#### Servo Not Moving
- Check 5V external power supply (ESP32-C6 3.3V may be insufficient)
- Verify GPIO pins 18 & 19 are connected correctly
- Test with `w` command to check system info
- Use multimeter to verify 5V power to servos

#### Serial Communication Issues
- Ensure baud rate is set to **115200**
- Try pressing **RST** button after upload
- Check USB-C cable (data + power, not just charging)
- Verify correct COM port selection

#### WiFi Interference
- WiFi module may interfere with PWM on some pins
- If issues occur, try different GPIO pins (2, 4, 5, 12-15, 25-27)
- Disable WiFi if not needed: `WiFi.mode(WIFI_OFF);`

### General Troubleshooting

#### Jerky Movement
- Increase `DEMO_DELAY` value (try 100ms)
- Use external 5V power supply with 2A+ capacity
- Check for loose wiring connections
- Reduce `MOVEMENT_SPEED` for smoother motion

#### Limited Range
- Adjust pulse width parameters: `attach(pin, minPulse, maxPulse)`
- Check mechanical bracket limits
- Verify servo specifications (some are 90° or 270°)

## 🌐 Future Enhancements

### WiFi Web Control
The ESP32-C6's WiFi capabilities enable future features:
- Web-based control interface
- Remote monitoring via smartphone
- Integration with home automation systems
- Over-the-air (OTA) firmware updates

### Bluetooth Control
- Smartphone app control via Bluetooth LE
- Wireless gamepad/joystick input
- Low-power remote operation

### Advanced Features
- **IMU Integration**: Add gyroscope for stabilization
- **Camera Module**: ESP32-CAM integration for live video
- **Multiple Servos**: Expand to 6-axis robotic arm
- **Machine Learning**: TensorFlow Lite for automated tracking

## 🔍 System Information Command

Use `w` command to display:
```
=== ESP32-C6 System Information ===
Chip Model: ESP32-C6
Chip Revision: 1
CPU Frequency: 160 MHz
Flash Size: 4 MB
Free Heap: 290000 bytes
Uptime: 45 seconds

GPIO Configuration:
Pan Servo Pin (GPIO 18): PWM Output
Tilt Servo Pin (GPIO 19): PWM Output

ESP32-C6 Features:
- WiFi 6 (802.11ax) support
- Bluetooth 5 (LE) support
- RISC-V 32-bit single-core processor
- 16 PWM channels available
```

## 📊 Performance Specifications

- **PWM Resolution**: 16-bit (65536 levels)
- **PWM Frequency**: 50 Hz (configurable)
- **Response Time**: <20ms servo update rate
- **Memory Usage**: ~25KB program space, ~8KB RAM
- **Power Consumption**: 
  - ESP32-C6: ~80mA active, ~10μA deep sleep
  - SG90 Servos: ~100-500mA each (depending on load)

## 📝 License

This code is provided under MIT License. Feel free to modify and use in your projects.

## 🆕 Version History

- **v2.0** (September 2025): ESP32-C6 optimized version
  - Added ESP32Servo library support
  - Enhanced PWM control with custom pulse widths
  - Added system information display
  - Improved error handling and debugging
  - Future-ready WiFi/Bluetooth framework

- **v1.0** (September 2025): Initial Arduino version