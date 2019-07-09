#include <WiFi.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEDescriptor.h>
#include <BLE2902.h>
#include <ble_definitions.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#define HW_CHAR
#ifdef HW_CHAR
BLECharacteristic* characteristics [6];
#else
BLECharacteristic* characteristics [4];
#endif

#define USE_BME
#ifdef USE_BME
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1014.2)
Adafruit_BME680 bme; // I2C
#endif

float sensors[4]; //temperature,humidity,voc,noise

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
  //setup the sensors
  setup_sensors();
  Serial.println(F("Waiting a client connection to notify..."));
}

void loop() {
  //start with the sensors loop
  loop_sensors();
  //continue to the ble loop
  loop_ble();
}

void setup_sensors() {
#ifdef USE_BME
  if (!bme.begin()) {
    Serial.println("error with sensor");
    while (1);
  }
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
#endif
  //TODO: add noise sensor setup here
}

void loop_sensors() {
  static unsigned long lastTempMeasurement = 0;
  if (millis() - lastTempMeasurement > 10000) {
    shouldNotify = true;
#ifdef USE_BME
    if (! bme.performReading()) {
      return;
    }
    sensors[0] = bme.temperature;
    sensors[1] = bme.humidity;
    //float pressure = bme.pressure;
    sensors[2] = bme.gas_resistance / 1000.0 ;
#else
    sensors[0] = random(10, 40);
    sensors[1] = random(0, 100);
    sensors[2] = random(0, 100);
#endif
    //TODO: add actual noise sensor here
    sensors[3] = random(30, 80);
    lastTempMeasurement = millis();
  }
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
  BLEService *sensorService = pServer->createService(SERVICE_GENERIC_ATTRIBUTE_UUID);

  // Create a BLE Characteristic
  for (int i = 0; i < 4; i++) {
    characteristics[i] = sensorService->createCharacteristic(
                           CHARACTERISTIC_TEMPERATURE_UUID,
                           BLECharacteristic::PROPERTY_READ   |
                           BLECharacteristic::PROPERTY_NOTIFY
                         );
    characteristics[i]->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x290C)));
  }

  // Create the BLE Service
  BLEService *deviceService = pServer->createService(SERVICE_DEVICE_INFORMATION_UUID);
#ifdef HW_CHAR
  characteristics[4] = deviceService->createCharacteristic(
                         CHARACTERISTIC_MANUFACTURER_NAME_STRING_UUID,
                         BLECharacteristic::PROPERTY_READ
                       );
  characteristics[5] = deviceService->createCharacteristic(
                         CHARACTERISTIC_FIRMWARE_REVISION_STRING_UUID,
                         BLECharacteristic::PROPERTY_READ
                       );
  characteristics[4]->addDescriptor(new BLE2902());
  characteristics[4]->setValue("SparkWorks");
  characteristics[5]->addDescriptor(new BLE2902());
  characteristics[5]->setValue("v0.1");
#endif
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
    char sensorStrValue[10];
    for (int i = 0; i < 4; i++) {
      sprintf(sensorStrValue, "%.2f", sensors[i]);
      //TODO: check if this works good with the values or we need to change to string
      characteristics[i]->setValue(sensorStrValue);
      characteristics[i]->notify();
      delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
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
