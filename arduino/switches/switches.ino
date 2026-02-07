#define SWITCH1 8
#define SWITCH2 9
#define SWITCH3 10

byte STATE1 = 0;
byte STATE2 = 0;
byte STATE3 = 0;
byte PREVIOUS1 = 0;
byte PREVIOUS2 = 0;
byte PREVIOUS3 = 0;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(SWITCH1, INPUT);
  pinMode(SWITCH2, INPUT);
  pinMode(SWITCH3, INPUT);
}

void loop() {

  STATE1 = digitalRead(SWITCH1);
  STATE2 = digitalRead(SWITCH2);
  STATE3 = digitalRead(SWITCH3);

  if(STATE1 != PREVIOUS1) {
    if(STATE1 == HIGH) {
      Serial.println("7:1");
    } else {
      Serial.println("7:0");
    }
    delay(50);
  }

  if(STATE2 != PREVIOUS2) {
    if(STATE2 == HIGH) {
      Serial.println("8:1");
    } else {
      Serial.println("8:0");
    }
     delay(50);
  }

    if(STATE3 != PREVIOUS3) {
    if(STATE3 == HIGH) {
      Serial.println("9:1");
    } else {
      Serial.println("9:0");
    }
    delay(50);
  }

  PREVIOUS3 = STATE3;
  PREVIOUS2 = STATE2;
  PREVIOUS1 = STATE1;
}
