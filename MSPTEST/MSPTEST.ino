#define I_WAKEUP A1
#define I_DATA   A0
#define O_AWAKE  6
#define O_CLK    5
#define O_RST    A2

void setup() {
  Serial.begin(57600);
  pinMode(I_DATA, INPUT);
  pinMode(I_DATA, INPUT);
  pinMode(O_CLK,   OUTPUT);
  pinMode(O_AWAKE, OUTPUT);
  pinMode(O_RST, OUTPUT);
  digitalWrite(O_RST, LOW);
  _delay_ms(500);
  pinMode(O_RST, INPUT);
}

void loop() {
  uint8_t pA1 = digitalRead(I_WAKEUP);
  uint8_t res = 0;
  if (pA1 == HIGH) {
    digitalWrite(O_AWAKE, HIGH);
    while (digitalRead(I_WAKEUP) == HIGH);

    for (uint8_t i = 0; i < 8; i++) {
      digitalWrite(O_CLK, HIGH);
      _delay_us(1500);
      if (digitalRead(I_DATA)) bitSet(res, i);
      digitalWrite(O_CLK, LOW);
      _delay_us(1500);
    }
    digitalWrite(O_AWAKE, LOW);
    Serial.print("Result: ");
    Serial.println(res, BIN);
  }
}
