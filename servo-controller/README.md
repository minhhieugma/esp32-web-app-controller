# Servo Pan-Tilt Bracket Controller

This project provides Arduino code to control a servo pan-tilt bracket with automatic 360-degree demo functionality.

## Features

- **Dual Servo Control**: Independent control of pan (horizontal) and tilt (vertical) servos
- **360° Demo Mode**: Automated demonstration with multiple movement patterns:
  - Pan sweep (left-right scanning)
  - Combined pan-tilt movement
  - Circular motion patterns
  - Random positioning
- **Serial Control**: Manual control via Arduino IDE Serial Monitor
- **Safety Limits**: Built-in angle constraints to protect servos
- **Real-time Feedback**: Position reporting via serial output

## Hardware Requirements

### Components Needed
- Arduino Uno/Nano/ESP32 (any compatible board)
- 2x SG90 servo motors (or similar 180° servos)
- Pan-tilt bracket (compatible with your servos)
- Jumper wires
- Breadboard (optional)
- External 5V power supply (recommended for multiple servos)

### Wiring Diagram

```
Arduino Uno/Nano    |    Pan Servo    |    Tilt Servo
--------------------|-----------------|------------------
Digital Pin 9       |    Signal       |    
Digital Pin 10      |                 |    Signal
5V or External 5V   |    VCC (Red)    |    VCC (Red)
GND                 |    GND (Brown)  |    GND (Brown)
```

**Important Notes:**
- Use external 5V power supply if controlling multiple servos
- Ensure common ground between Arduino and external power supply
- SG90 servos typically use ~500mA each under load

## Software Setup

### 1. Install Arduino IDE
Download from: https://www.arduino.cc/en/software

### 2. Install Required Libraries
The code uses the built-in `Servo` library - no additional installation needed.

### 3. Upload Code
1. Connect your Arduino to computer via USB
2. Open `servo_pan_tilt_controller.ino` in Arduino IDE
3. Select your board type: Tools → Board
4. Select correct COM port: Tools → Port
5. Click Upload button (→)

## Usage

### Demo Mode (Default)
The system starts in demo mode automatically, running through these phases:

1. **Pan Sweep**: Servo sweeps horizontally from 0° to 180°
2. **Combined Movement**: Both servos move in coordinated patterns
3. **Circular Motion**: Creates smooth circular scanning patterns
4. **Random Positioning**: Moves to random positions for variety

### Manual Control
Open Serial Monitor (Tools → Serial Monitor) and use these commands:

| Command | Description | Example |
|---------|-------------|---------|
| `p[angle]` | Set pan angle (0-180°) | `p90` (center), `p0` (left), `p180` (right) |
| `t[angle]` | Set tilt angle (0-180°) | `t45` (up), `t90` (center), `t135` (down) |
| `d` | Toggle demo mode on/off | `d` |
| `s` | Stop and center servos | `s` |
| `h` | Show help menu | `h` |

### Example Commands
```
p0      // Pan to leftmost position
t180    // Tilt to lowest position  
p90     // Pan to center
t90     // Tilt to center
d       // Start/stop demo
s       // Emergency stop and center
```

## Configuration

### Adjusting Servo Limits
Edit these constants in the code to match your servo specifications:

```cpp
const int PAN_MIN = 0;     // Minimum pan angle
const int PAN_MAX = 180;   // Maximum pan angle
const int TILT_MIN = 0;    // Minimum tilt angle
const int TILT_MAX = 180;  // Maximum tilt angle
```

### Changing Demo Speed
Adjust these variables for different demo speeds:

```cpp
const int DEMO_DELAY = 50;      // Delay between movements (ms)
const int MOVEMENT_SPEED = 2;   // Degrees per step
```

### Pin Configuration
Change servo pins if needed:

```cpp
const int PAN_SERVO_PIN = 9;    // Pan servo pin
const int TILT_SERVO_PIN = 10;  // Tilt servo pin
```

## Troubleshooting

### Servos Not Moving
- Check wiring connections
- Verify power supply (5V, sufficient current)
- Ensure correct pin assignments
- Check Serial Monitor for error messages

### Jerky Movement
- Increase `DEMO_DELAY` value
- Decrease `MOVEMENT_SPEED` value
- Check for loose connections
- Use external power supply

### Servo Limits Issues
- Adjust `PAN_MIN/MAX` and `TILT_MIN/MAX` values
- Check mechanical bracket limits
- Ensure servos are properly mounted

### Serial Communication Issues
- Verify baud rate (115200)
- Check USB cable connection
- Try different COM port
- Reset Arduino board

## Applications

This pan-tilt controller can be used for:

- **Security Cameras**: Automated surveillance scanning
- **FPV Systems**: Remote camera positioning
- **Robotics**: Head movement for robots
- **Time-lapse Photography**: Automated camera movement
- **Educational Projects**: Learning servo control and automation
- **Art Installations**: Interactive kinetic sculptures

## Advanced Features

### Adding More Servos
To add additional servos (e.g., for camera focus or zoom):

```cpp
#include <Servo.h>
Servo extraServo;
const int EXTRA_SERVO_PIN = 11;

void setup() {
  extraServo.attach(EXTRA_SERVO_PIN);
  // ... rest of setup
}
```

### Speed Control
For smoother movement, implement acceleration/deceleration:

```cpp
void smoothMove(Servo &servo, int currentPos, int targetPos, int speed) {
  while (currentPos != targetPos) {
    if (currentPos < targetPos) currentPos++;
    else currentPos--;
    servo.write(currentPos);
    delay(speed);
  }
}
```

### Position Feedback
For servos with position feedback, add reading functionality:

```cpp
int readServoPosition(int analogPin) {
  return map(analogRead(analogPin), 0, 1023, 0, 180);
}
```

## License

This code is provided under MIT License. Feel free to modify and use in your projects.

## Version History

- v1.0 (September 2025): Initial release with basic pan-tilt control and demo modes