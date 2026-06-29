#define BUZZER_PIN 2

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("Test Buzzer");
}

void loop() {

  digitalWrite(BUZZER_PIN, HIGH); // Bunyi
  delay(1000);

  digitalWrite(BUZZER_PIN, LOW);  // Diam
  delay(1000);

}
