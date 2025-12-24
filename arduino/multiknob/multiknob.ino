#include <Oversampling.h>

Oversampling adc(10, 13, 1);
int SENSORS[] = {A0, A6};

unsigned long UPDATE[16] = {0};
const unsigned long INTERVAL = 50;

void readPot(uint8_t sensor) {
  static int PREVIOUS[16] = {-1};
  static bool INIT = false;
  int SENSOR = adc.read(sensor);
  int PERCENT = round((SENSOR / 8183.0) * 100);
  
  if (PERCENT != PREVIOUS[sensor]) {

    if (abs(PERCENT - PREVIOUS[sensor]) >= 1) {
      
      Serial.println((String)sensor + ":" + (String)PERCENT);
      PREVIOUS[sensor] = PERCENT;
    }
  }
}

void setup() {

  Serial.begin(9600);

}

void loop() {
  
  for (int i = 0; i < 2; i++) {
    int s = SENSORS[i];
    if (millis() - UPDATE[s] >= INTERVAL) {
      readPot(s);
      UPDATE[s] = millis();
    }
  }

}
