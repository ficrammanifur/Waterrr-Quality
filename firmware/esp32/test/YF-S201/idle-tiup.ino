/*
  ESP32 Flow Meter Test
  GPIO 18
*/

#define FLOW_PIN 19

volatile uint32_t pulseCount = 0;

void IRAM_ATTR flowISR() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);

  Serial.println("================================");
  Serial.println("FLOW METER TEST");
  Serial.println("GPIO : 18");
  Serial.println("================================");
}

void loop() {

  static unsigned long lastTime = 0;

  if (millis() - lastTime >= 1000) {

    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    Serial.print("Pulse/s : ");
    Serial.println(pulses);

    lastTime = millis();
  }

}
