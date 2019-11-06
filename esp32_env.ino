#include <WiFi.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEDescriptor.h>
#include <BLE2902.h>
#include <ble_definitions.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

BLECharacteristic* characteristics [6];
#define SENSORS_START 2


#define USE_BME
#include "bme.h"

bool deviceConnected = false;
bool shouldNotify = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("->deviceConnected");
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("->deviceConnected");
    }
};

void setup() {
  WiFi.mode(WIFI_OFF);
  //start the serial
  Serial.begin(115200);
  Serial.println("booting up");
  //and the ble
  if (!setup_ble()) {
    Serial.println("error with sensor");
    while (1);
  }
  //setup the bme
  setup_bme();
  Serial.println(F("Waiting a client connection to notify..."));
}

void loop() {
  //start with the bme loop
  shouldNotify = loop_bme();
  //continue to the ble loop
  loop_ble();
}

bool setup_ble() {
  if (!btStart()) {
    Serial.println("Failed to initialize controller");
    return false;
  }

  if (esp_bluedroid_init() != ESP_OK) {
    Serial.println("Failed to initialize bluedroid");
    return false;
  }

  if (esp_bluedroid_enable() != ESP_OK) {
    Serial.println("Failed to enable bluedroid");
    return false;
  }
  char bleMacStr[20];
  const uint8_t * mac_pointer = esp_bt_dev_get_address();
  sprintf(bleMacStr, "SparkEnv%02X%02X%02X", mac_pointer[3], mac_pointer[4], mac_pointer[5]);
  //Create the BLE Device
  BLEDevice::init(bleMacStr);

  // Create the BLE Server
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *deviceService = pServer->createService(SERVICE_DEVICE_INFORMATION_UUID);

  characteristics[0] = deviceService->createCharacteristic(
                         CHARACTERISTIC_MANUFACTURER_NAME_STRING_UUID,
                         BLECharacteristic::PROPERTY_READ
                       );
  characteristics[1] = deviceService->createCharacteristic(
                         CHARACTERISTIC_FIRMWARE_REVISION_STRING_UUID,
                         BLECharacteristic::PROPERTY_READ
                       );
  characteristics[0]->addDescriptor(new BLE2902());
  characteristics[0]->setValue("SparkWorks");
  characteristics[1]->addDescriptor(new BLE2902());
  characteristics[1]->setValue("v0.1");

  // Create the BLE Service
  BLEService *sensorService = pServer->createService(SERVICE_GENERIC_ATTRIBUTE_UUID);

  // Create a BLE Characteristic
  characteristics[2] = sensorService->createCharacteristic(
                         CHARACTERISTIC_TEMPERATURE_UUID,
                         BLECharacteristic::PROPERTY_READ   |
                         BLECharacteristic::PROPERTY_NOTIFY
                       );
  characteristics[2]->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x290C)));
  characteristics[3] = sensorService->createCharacteristic(
                         CHARACTERISTIC_HUMIDITY_UUID,
                         BLECharacteristic::PROPERTY_READ   |
                         BLECharacteristic::PROPERTY_NOTIFY
                       );
  characteristics[3]->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x290C)));
  characteristics[4] = sensorService->createCharacteristic(
                         CHARACTERISTIC_VOC_UUID,
                         BLECharacteristic::PROPERTY_READ   |
                         BLECharacteristic::PROPERTY_NOTIFY
                       );
  characteristics[4]->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x290C)));

  Serial.println("characteristics");

  // Start the service
  sensorService->start();
  deviceService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_GENERIC_ATTRIBUTE_UUID);
  pAdvertising->addServiceUUID(SERVICE_DEVICE_INFORMATION_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  pServer->startAdvertising(); // restart advertising
  Serial.println(F("start advertising"));
  return true;
}

void loop_ble() {
  static unsigned long count = 0;
  // notify changed value
  if (deviceConnected && shouldNotify) {
    Serial.print(F("running notify["));
    Serial.print(count++);
    Serial.println(F("]..."));
    shouldNotify = false;
    notify_bme(characteristics, SENSORS_START );
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
