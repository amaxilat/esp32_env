#include <WiFi.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEDescriptor.h>
#include <BLE2902.h>
#include <ble_definitions.h>
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
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  //start the serial
  Serial.begin(115200);
  Serial.println("booting up");
  //and the ble
  setup_ble();
  //setup the sensors
  setup_sensors();
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
  if (millis() - lastTempMeasurement > 30000) {
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

void setup_ble() {
  byte mac[6];
  WiFi.macAddress(mac);
  char bleMacStr[20];
  Serial.println("Here");
  sprintf(bleMacStr, "SparkEnv%02X%02X%02X", mac[3], mac[4], mac[5]);
  //sprintf(bleMacStr, "SparkEnv");
  //Create the BLE Device
  BLEDevice::init(bleMacStr);
  Serial.println("init");

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
  Serial.println(F("Waiting a client connection to notify..."));
}

void loop_ble() {
  // notify changed value
  if (deviceConnected && shouldNotify) {
    Serial.println(F("running notify..."));
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
