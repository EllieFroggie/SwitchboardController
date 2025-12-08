#include <Oversampling.h>

Oversampling adc(10, 13, 1);

int readPot(uint8_t sensor) {
  static int PREVIOUS[16];
  static double DIFF = -1;
  static bool INIT = false;
  int PERCENT;
  int SENSOR;

  if (!INIT) {
    for (int i = 0; i < 16; i++) PREVIOUS[i] = -1;
    INIT = true;

  }
  
  SENSOR = adc.read(sensor);
  PERCENT = floor((SENSOR / 8183.0) * 100);

  if (SENSOR != PREVIOUS[sensor]) {
    DIFF = abs(PREVIOUS[sensor] - SENSOR);

    if (DIFF > 2) {
      
      Serial.println((String)sensor + ":" + (String)PERCENT);
      PREVIOUS[sensor] = SENSOR;
      delay(50);
      return 0;
    }
  }
}

void setup() {

  Serial.begin(9600);

}

void loop() {

  if (readPot(A0)) {
    delay(50);
  }
  if (readPot(A6)) {
    delay(50);
  }


}
