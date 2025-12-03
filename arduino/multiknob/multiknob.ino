
int readPot(uint8_t sensor) {
  static int PREVIOUS[16];
  static int DIFF = -1;
  static bool INIT = false;

  if (!INIT) {
    for (int i = 0; i < 16; i++) PREVIOUS[i] = -1;
    INIT = true;

  }
  
  int SENSOR = analogRead(sensor);

  if (SENSOR != PREVIOUS[sensor]) {
    DIFF = abs(PREVIOUS[sensor] - SENSOR);

    if (DIFF > 3) {
      Serial.println((String)sensor + ":" + (String)SENSOR);
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
