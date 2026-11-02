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

## Build and flash

```bash
cd /Users/jacksit/git/yp3001-esp
pio run
pio run -t upload --upload-port /dev/cu.usbmodem101
pio device monitor --port /dev/cu.usbmodem101
```

The serial monitor prints the BLE device name and voltage updates.

## Test

Use the iOS app in:

```text
/Users/jacksit/git/yp3001-app
```

Open the app, scan for `SmartToilet-XXXX`, connect, and use the servo controls.
