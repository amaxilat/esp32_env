/*
  IAQ Accuracy=0 could either mean:
  BSEC was just started, and the sensor is stabilizing (this lasts normally 5min in LP mode or 20min in ULP mode),
  there was a timing violation (i.e. BSEC was called too early or too late), which should be indicated by a warning/error flag by BSEC,
  IAQ Accuracy=1 means the background history of BSEC is uncertain. This typically means the gas sensor data was too stable for BSEC to clearly define its references,
  IAQ Accuracy=2 means BSEC found a new calibration data and is currently calibrating,
  IAQ Accuracy=3 means BSEC calibrated successfully.
*/

#ifdef USE_BSEC
#include <EEPROM.h>

//const uint8_t bsec_config_iaq[] = {
//#include "config/generic_33v_3s_4d/bsec_iaq.txt"
//};

// Helper functions declarations
void checkIaqSensorStatus(void);

#define STATE_SAVE_PERIOD  UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day


// Create an object of the class Bsec
Bsec iaqSensor;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;

float bme_sensors[5]; //temperature,humidity,iaq,iaqstate,pressure
String output;

void loadBSECState(void);
void updateState(void);

#endif

void setup_bsec() {
#ifdef USE_BSEC
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length

  Wire.begin();
  iaqSensor.begin(0x77, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();
  //  iaqSensor.setConfig(bsec_config_iaq);
  //  checkIaqSensorStatus();
  loadBSECState();


  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };
  iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();
  // Print the header
  output = "Timestamp [ms], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";
  Serial.println(output);
#endif
}

bool loop_bsec() {
#ifdef USE_BSEC
  unsigned long time_trigger = millis();
  if (iaqSensor.run()) { // If new data is available
    output = String(time_trigger);
    output += ", " + String(iaqSensor.gasResistance);
    output += ", " + String(iaqSensor.iaq);
    output += ", " + String(iaqSensor.iaqAccuracy);
    output += ", " + String(iaqSensor.temperature);
    output += ", " + String(iaqSensor.humidity);
    output += ", " + String(iaqSensor.staticIaq);
    output += ", " + String(iaqSensor.co2Equivalent);
    output += ", " + String(iaqSensor.breathVocEquivalent);
    Serial.println(output);
    bme_sensors[0] = iaqSensor.temperature;
    bme_sensors[1] = iaqSensor.humidity;
    //float pressure = bme.pressure;
    bme_sensors[2] = iaqSensor.iaqAccuracy + ( ((float)random(9)) / 10);
    bme_sensors[3] = iaqSensor.iaq;
    bme_sensors[4] = iaqSensor.pressure * 100;
    updateState();
    return true;
  } else {
    checkIaqSensorStatus();
    return false;
  }

#else
  return false;
#endif
}

void notify_bsec(BLECharacteristic** characteristics, int offset ) {
#ifdef USE_BSEC
  char sensorStrValue[12];
  for (int i = 0; i < 5; i++) {
    if (bme_sensors[i] == 0) {
      continue;
    }
    sprintf(sensorStrValue, "%.2f", bme_sensors[i]);
    //TODO: check if this works good with the values or we need to change to string
    characteristics[offset + i]->setValue(sensorStrValue);
    characteristics[offset + i]->notify(true);

    delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
  }
#endif
}

#ifdef USE_BSEC
// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK) {
    if (iaqSensor.status < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
    } else {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme680Status != BME680_OK) {
    if (iaqSensor.bme680Status < BME680_OK) {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    } else {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}


void loadBSECState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM
    Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      Serial.println(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
    Serial.println("EEPROM commited");
  }
}

void updateState(void)
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0) {
    if (iaqSensor.iaqAccuracy >= 3) {
      Serial.println("Will write state to EEPROM");

      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      Serial.println("Will write state to EEPROM2");
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
      EEPROM.write(i + 1, bsecState[i]);
      Serial.println(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}
#endif
