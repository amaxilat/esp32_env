#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#ifdef USE_BME

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1014.2)
Adafruit_BME680 bme; // I2C

float bme_sensors[3]; //temperature,humidity,voc

#endif

void setup_bme() {
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
}


bool loop_bme() {
#ifdef USE_BME
  static unsigned long lastTempMeasurement = 0;
  if (millis() - lastTempMeasurement > 10000) {
    if (! bme.performReading()) {
      return false;
    }
    bme_sensors[0] = bme.temperature;
    bme_sensors[1] = bme.humidity;
    //float pressure = bme.pressure;
    bme_sensors[2] = bme.gas_resistance / 1000.0 ;
    lastTempMeasurement = millis();
    return true;
  } else {
    return false;
  }
#else
  return false;
#endif

}

void notify_bme(BLECharacteristic** characteristics, int offset ) {
  char sensorStrValue[10];
  for (int i = 0; i < 3; i++) {
    sprintf(sensorStrValue, "%.2f", bme_sensors[i]);
    //TODO: check if this works good with the values or we need to change to string
    characteristics[offset + i]->setValue(sensorStrValue);
    characteristics[offset + i]->notify();
    delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
  }
}
