// ini kode gabungan fixx

#include <Arduino.h>

// ================================
// ESP32 pH Sensor (GPIO 32)
// 3-Point Calibration (REAL DATA)
// + Moving Average + EMA smoothing
// ================================

const int phPin = 32;

// smoothing ADC
#define SCOUNT 20
int analogBuffer[SCOUNT];
int analogIndex = 0;

// hasil pH
float voltage = 0.0;
float phValue = 0.0;
float phFiltered = 0.0;

// ================================
// KONSTANTA KALIBRASI REAL KAMU
// ================================
const float V4  = 1.350;  const float PH4  = 4.00;
const float V7  = 0.874;  const float PH7  = 6.86;
const float V9  = 0.485;  const float PH9  = 9.18;

// ================================
// FUNGSI INTERPOLASI 3 TITIK
// ================================
float getPH(float voltage) {

  float ph;

  // RANGE: 4.00 - 6.86
  if (voltage >= V7) {
    ph = PH4 + (PH7 - PH4) * (V4 - voltage) / (V4 - V7);
  }

  // RANGE: 6.86 - 9.18
  else {
    ph = PH7 + (PH9 - PH7) * (V7 - voltage) / (V7 - V9);
  }

  return ph;
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);

  // warm-up buffer
  for (int i = 0; i < SCOUNT; i++) {
    analogBuffer[i] = analogRead(phPin);
    delay(20);
  }

  Serial.println("====================================");
  Serial.println("ESP32 pH SENSOR READY (FINAL MODE)");
  Serial.println("3-POINT INTERPOLATION ACTIVE");
  Serial.println("====================================");
}

void loop() {

  // ambil ADC
  analogBuffer[analogIndex] = analogRead(phPin);
  analogIndex++;
  if (analogIndex >= SCOUNT) analogIndex = 0;

  // moving average ADC
  long sum = 0;
  for (int i = 0; i < SCOUNT; i++) {
    sum += analogBuffer[i];
  }

  float avgADC = sum / (float)SCOUNT;

  // convert ke voltage
  voltage = avgADC * (3.3 / 4095.0);

  // hitung pH (INTERPOLATION)
  float phRaw = getPH(voltage);

  // smoothing pH (EMA filter)
  phFiltered = (0.85 * phFiltered) + (0.15 * phRaw);

  // clamp
  if (phFiltered < 0) phFiltered = 0;
  if (phFiltered > 14) phFiltered = 14;

  // ================================
  // OUTPUT
  // ================================
  Serial.print("ADC      : ");
  Serial.print(avgADC, 2);

  Serial.print(" | Voltage : ");
  Serial.print(voltage, 3);

  Serial.print(" V | pH Raw : ");
  Serial.print(phRaw, 2);

  Serial.print(" | pH Smooth : ");
  Serial.println(phFiltered, 2);

  delay(1000);
}


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

