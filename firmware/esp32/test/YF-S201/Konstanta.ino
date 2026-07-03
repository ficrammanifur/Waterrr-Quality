// const float ML_PER_PULSE = 1000.0 / 922.0;
// = 1.0846 mL per pulse

#define FLOW_PIN 19

volatile unsigned long pulseCount = 0;

void IRAM_ATTR flowISR() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);

  Serial.println("================================");
  Serial.println("Ketik 's' untuk START");
  Serial.println("Ketik 'e' untuk END");
  Serial.println("================================");
}

void loop() {

  if (Serial.available()) {

    char c = Serial.read();

    if (c == 's') {

      noInterrupts();
      pulseCount = 0;
      interrupts();

      Serial.println("START...");
    }

    if (c == 'e') {

      noInterrupts();
      unsigned long total = pulseCount;
      interrupts();

      Serial.print("TOTAL PULSE = ");
      Serial.println(total);
    }

  }
}
