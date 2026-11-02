// ESP32-S3 BLE smart toilet firmware
//
// Board: Seeed XIAO ESP32S3
// Servo signal: D0 / GPIO 1
//
// BLE service:
//   Service UUID:  12345678-1234-1234-1234-1234567890ab
//   Voltage UUID:  12345678-1234-1234-1234-1234567890ac  (read/notify)
//   Servo UUID:    12345678-1234-1234-1234-1234567890ad  (read/write)
//
// Value formats are plain text:
//   Voltage: "1.42"
//   Servo:   "1" = ON / 90 degrees, "0" = OFF / 0 degrees

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>
#include <esp_mac.h>

#define SERVICE_UUID  "12345678-1234-1234-1234-1234567890ab"
#define VOLTAGE_UUID  "12345678-1234-1234-1234-1234567890ac"
#define SERVO_UUID    "12345678-1234-1234-1234-1234567890ad"

#define SERVO_PIN           1
#define SERVO_LEDC_CHANNEL  0
#define SERVO_LEDC_FREQ     50
#define SERVO_LEDC_RES      14
#define SERVO_DUTY_0        410
#define SERVO_DUTY_90       1229

static BLEServer *bleServer = nullptr;
static BLECharacteristic *voltageCharacteristic = nullptr;
static BLECharacteristic *servoCharacteristic = nullptr;
static bool deviceConnected = false;
static bool servoOn = false;
static bool servoPwmAttached = false;
static unsigned long servoDetachAt = 0;
static bool advertisingWindowOpen = false;
static unsigned long advertisingWindowStartedAt = 0;
static unsigned long advertisingPausedAt = 0;
static char deviceName[32];

#define ADVERTISING_WINDOW_MS 2000
#define ADVERTISING_PAUSE_MS  3000
#define SERVO_HOLD_MS         1000

static void startAdvertisingWindow()
{
    BLEDevice::startAdvertising();
    advertisingWindowOpen = true;
    advertisingWindowStartedAt = millis();
    Serial.println("BLE advertising window open (2s)");
}

static void pauseAdvertising()
{
    BLEDevice::stopAdvertising();
    advertisingWindowOpen = false;
    advertisingPausedAt = millis();
    Serial.println("BLE advertising paused (3s)");
}

static void servoAttach()
{
    if (!servoPwmAttached) {
        ledcAttachPin(SERVO_PIN, SERVO_LEDC_CHANNEL);
        servoPwmAttached = true;
    }
}

static void servoDetach()
{
    if (servoPwmAttached) {
        ledcWrite(SERVO_LEDC_CHANNEL, 0);
        ledcDetachPin(SERVO_PIN);
        servoPwmAttached = false;
        Serial.println("Servo PWM detached");
    }
}

static void setServo(bool on)
{
    servoAttach();
    servoOn = on;
    ledcWrite(SERVO_LEDC_CHANNEL, on ? SERVO_DUTY_90 : SERVO_DUTY_0);
    servoDetachAt = millis() + SERVO_HOLD_MS;
    Serial.printf("Servo %s (%d degrees), PWM detach in %dms\n",
                  on ? "ON" : "OFF", on ? 90 : 0, SERVO_HOLD_MS);
}

static void servoInit()
{
    ledcSetup(SERVO_LEDC_CHANNEL, SERVO_LEDC_FREQ, SERVO_LEDC_RES);
    setServo(false);
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override
    {
        deviceConnected = true;
        advertisingWindowOpen = false;
        Serial.println("BLE client connected; advertising paused");
    }

    void onDisconnect(BLEServer *server) override
    {
        deviceConnected = false;
        Serial.println("BLE client disconnected");
        startAdvertisingWindow();
    }
};

class ServoCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) override
    {
        std::string value = characteristic->getValue();
        Serial.printf("Servo write: %s\n", value.c_str());

        if (value == "1" || value == "ON" || value == "on") {
            setServo(true);
            characteristic->setValue("1");
        } else {
            setServo(false);
            characteristic->setValue("0");
        }
    }
};

void setup()
{
    Serial.begin(115200);
    delay(500);

    servoInit();

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macString[13];
    snprintf(macString, sizeof(macString), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Short human-readable name: last 3 bytes of the MAC.
    snprintf(deviceName, sizeof(deviceName), "SmartToilet-%s", macString + 6);
    Serial.printf("Starting BLE device: %s (MAC %s)\n", deviceName, macString);

    BLEDevice::init(deviceName);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    BLEService *service = bleServer->createService(SERVICE_UUID);

    voltageCharacteristic = service->createCharacteristic(
        VOLTAGE_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    voltageCharacteristic->setValue("0.00");
    voltageCharacteristic->addDescriptor(new BLE2902());

    servoCharacteristic = service->createCharacteristic(
        SERVO_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    servoCharacteristic->setValue("0");
    servoCharacteristic->setCallbacks(new ServoCallbacks());

    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);

    // Put the full MAC in manufacturer data so the app can identify boards
    // even when iOS caches an old GAP name. Company ID 0xFFFF + 6 MAC bytes.
    std::string manufacturerData;
    manufacturerData.push_back((char)0xFF);
    manufacturerData.push_back((char)0xFF);
    for (int i = 0; i < 6; i++) {
        manufacturerData.push_back((char)mac[i]);
    }

    BLEAdvertisementData scanResponse;
    scanResponse.setName(deviceName);
    scanResponse.setManufacturerData(manufacturerData);
    advertising->setScanResponseData(scanResponse);

    startAdvertisingWindow();
}

void loop()
{
    // Stop the servo pulses after it has had time to reach the target.
    if (servoDetachAt != 0 && millis() >= servoDetachAt) {
        servoDetach();
        servoDetachAt = 0;
    }

    // Advertise only 2 seconds out of every 5 seconds while disconnected.
    // Once a phone connects, stay connected and stop the duty cycle.
    if (!deviceConnected) {
        if (advertisingWindowOpen) {
            if (millis() - advertisingWindowStartedAt >= ADVERTISING_WINDOW_MS) {
                pauseAdvertising();
            }
        } else if (millis() - advertisingPausedAt >= ADVERTISING_PAUSE_MS) {
            startAdvertisingWindow();
        }
    }

    static unsigned long lastReport = 0;
    if (millis() - lastReport >= 2000) {
        lastReport = millis();

        float voltage = 0.5f + (float)random(0, 281) / 100.0f;
        char value[8];
        snprintf(value, sizeof(value), "%.2f", voltage);
        voltageCharacteristic->setValue(value);
        if (deviceConnected) {
            voltageCharacteristic->notify();
        }
        Serial.printf("Voltage: %s V (servo %s)\n", value, servoOn ? "ON" : "OFF");
    }
    delay(50);
}
