#include <PinButton.h>
#include <Oversampling.h>

#define SWITCH2 2
#define SWITCH3 3

Oversampling adc(10, 13, 1);
int SENSORS[] = {0, 1, 4};

PinButton button_one(SWITCH2);
PinButton button_two(SWITCH3);

unsigned long UPDATE[16] = {0};
const unsigned long INTERVAL = 50;
static int PREVIOUS[16] = {-1};

void readPot(uint8_t sensor) {

  static bool INIT = false;
  int SENSOR = adc.read(sensor);
  int PERCENT = abs(round((SENSOR / 8183.0) * 100) - 100);
  
  if (PERCENT != PREVIOUS[sensor]) {

    if (abs(PERCENT - PREVIOUS[sensor]) >= 1) {
      Serial.println((String)sensor + ":" + (String)PERCENT);
      PREVIOUS[sensor] = PERCENT;
    }
  }
}

void setup() {

  Serial.begin(9600);
  pinMode(SWITCH2, INPUT_PULLUP);
  pinMode(SWITCH3, INPUT_PULLUP);

}

void loop() {

  button_one.update();
  button_two.update();

  // 0 Single, 1 Double, 2 Long
  if (button_one.isSingleClick()) {
    Serial.println("5:0");
  } else if (button_one.isDoubleClick()) {
    Serial.println("5:1");
  }

  if (button_two.isSingleClick()) {
    Serial.println("6:0");
  } else if (button_two.isDoubleClick()) {
    Serial.println("6:1");
  }
  
  for (int i = 0; i < 3; i++) {
    int s = SENSORS[i];
    if (millis() - UPDATE[s] >= INTERVAL) {
      readPot(s);
      UPDATE[s] = millis();
    }
  }

}
