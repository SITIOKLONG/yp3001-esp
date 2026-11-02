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
static char deviceName[32];

static void setServo(bool on)
{
    servoOn = on;
    ledcWrite(SERVO_LEDC_CHANNEL, on ? SERVO_DUTY_90 : SERVO_DUTY_0);
    Serial.printf("Servo %s (%d degrees)\n", on ? "ON" : "OFF", on ? 90 : 0);
}

static void servoInit()
{
    ledcSetup(SERVO_LEDC_CHANNEL, SERVO_LEDC_FREQ, SERVO_LEDC_RES);
    ledcAttachPin(SERVO_PIN, SERVO_LEDC_CHANNEL);
    setServo(false);
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override
    {
        deviceConnected = true;
        Serial.println("BLE client connected");
    }

    void onDisconnect(BLEServer *server) override
    {
        deviceConnected = false;
        Serial.println("BLE client disconnected");
        delay(200);
        BLEDevice::startAdvertising();
        Serial.println("BLE advertising restarted");
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

    BLEDevice::startAdvertising();

    Serial.println("BLE advertising started");
}

void loop()
{
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
