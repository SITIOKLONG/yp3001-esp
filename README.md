# Smart Toilet BLE Firmware

Board: Seeed XIAO ESP32S3  
Servo signal: D0 / GPIO 1

## BLE protocol

Device name:

```text
SmartToilet-XXXX
```

Service UUID:

```text
12345678-1234-1234-1234-1234567890ab
```

Characteristics:

| Purpose | UUID | Properties | Value |
|---|---|---|---|
| Voltage | `...90ac` | Read / Notify | `"1.42"` |
| Servo | `...90ad` | Read / Write | `"1"` = 90°, `"0"` = 0° |

Voltage is sent every 2 seconds.

## Power-saving behavior

- BLE advertising window: **4 seconds on, 10 seconds off**
- Once connected, the board stays connected and pauses the duty cycle
- After disconnect, the 4s/10s window resumes
- Servo PWM is detached **1 second after movement**
  - This stops the servo holding current
  - The servo may go limp after detaching; use a mechanical hold if needed

## Build and flash

```bash
cd /Users/jacksit/git/yp3001-esp
pio run
pio run -t upload --upload-port /dev/cu.usbmodem101
pio device monitor --port /dev/cu.usbmodem101
```

The serial monitor prints the BLE device name, advertising window, PWM detach, and voltage updates.

### PlatformIO linker workaround

`platformio.ini` includes a small post-build script:

```text
scripts/dedupe_libs.py
```

It removes duplicate library entries from the PlatformIO link command. Without it, the older Xtensa linker can fail with errors such as:

```text
cannot find -lnewlib
cannot find -lstdc++
```

## Test

Use the iOS app in:

```text
/Users/jacksit/git/yp3001-app
```

Open the app, scan for `SmartToilet-XXXX`, connect, and use the servo controls.
