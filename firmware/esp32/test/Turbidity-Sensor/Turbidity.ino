/*
  ESP32 - Turbidity Sensor
  GPIO 35
*/

const int turbidityPin = 35;

// Nilai default (akan ditimpa hasil kalibrasi)
int nilaiDiAir = 4095;
int nilaiDiUdara = 3031;

// Membaca ADC dengan averaging
int bacaADC(int jumlahSample = 20) {
  long total = 0;

  for (int i = 0; i < jumlahSample; i++) {
    total += analogRead(turbidityPin);
    delay(10);
  }

  return total / jumlahSample;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12); // ADC 0-4095

  Serial.println("=================================");
  Serial.println(" Turbidity Sensor ESP32");
  Serial.println(" GPIO 35");
  Serial.println("=================================\n");

  // =============================
  // Kalibrasi AIR
  // =============================
  Serial.println("1. Celupkan sensor ke AIR JERNIH...");
  delay(3000);

  nilaiDiAir = bacaADC();

  Serial.print("Nilai di AIR : ");
  Serial.println(nilaiDiAir);

  // =============================
  // Kalibrasi UDARA
  // =============================
  Serial.println("\n2. Angkat sensor ke UDARA...");
  delay(3000);

  nilaiDiUdara = bacaADC();

  Serial.print("Nilai di UDARA : ");
  Serial.println(nilaiDiUdara);

  Serial.println("\n=== KALIBRASI SELESAI ===");
  Serial.println("-------------------------");
}

void loop() {

  int nilai = bacaADC(10);

  int persenKekeruhan;

  if (nilai >= nilaiDiAir) {
    persenKekeruhan = 0;
  }
  else if (nilai <= nilaiDiUdara) {
    persenKekeruhan = 100;
  }
  else {
    persenKekeruhan = map(nilai,
                          nilaiDiUdara,
                          nilaiDiAir,
                          100,
                          0);
  }

  int persenKejernihan = 100 - persenKekeruhan;

  Serial.print("ADC : ");
  Serial.print(nilai);

  Serial.print(" | Keruh : ");
  Serial.print(persenKekeruhan);
  Serial.print("%");

  Serial.print(" | Jernih : ");
  Serial.print(persenKejernihan);
  Serial.print("%");

  Serial.print(" | ");

  if (persenKejernihan >= 80) {
    Serial.println("AIR SANGAT JERNIH");
  }
  else if (persenKejernihan >= 50) {
    Serial.println("AIR CUKUP JERNIH");
  }
  else if (persenKejernihan >= 20) {
    Serial.println("AIR AGAK KERUH");
  }
  else {
    Serial.println("AIR KERUH / DI UDARA");
  }

  delay(500);
}
