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

/*
 * ============================================================
 *   FLOW SENSOR MODULE - ESP32
 *   Menggunakan sensor flow YF-S201 atau sejenis
 *   Pin: GPIO18
 * ============================================================
 */

#include <Preferences.h>  // Untuk menyimpan kalibrasi

// ==================== PIN DEFINITIONS ====================
#define FLOW_PIN        18

// ==================== FLOW KALIBRASI ====================
float PULSES_PER_LITER   = 310.0;    // Jumlah pulsa per liter (sesuai sensor)
float CALIBRATION_FACTOR = 1.082;    // Faktor koreksi
const float STEP_ML      = 250.0;    // Snap per 250 mL
const float TOLERANCE_L  = 0.05;     // Toleransi snapping

// ==================== GLOBALS ====================
volatile unsigned long pulseCount = 0;
float totalVolumeL   = 0.0;         // Total volume aktual
float displayVolumeL = 0.0;         // Volume yang ditampilkan (snapped)
float flowRateLPM    = 0.0;         // Flow rate L/menit
unsigned long lastFlowCalc  = 0;
unsigned long lastPulseSnap = 0;
float lastVolForFlow = 0.0;

// Calibration mode
bool  calibMode       = false;
float calibTargetL    = 0.0;
unsigned long calibStartPulse = 0;

Preferences prefs;

// ==================== ISR ====================
void IRAM_ATTR flowISR() {
  pulseCount++;
}

// ============================================================
//   INIT FLOW SENSOR
// ============================================================
void initFlowSensor() {
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);
  
  // Load kalibrasi dari NVS (jika ada)
  prefs.begin("flow_cal", true);
  float savedPPL = prefs.getFloat("ppl", 0.0);
  float savedCal = prefs.getFloat("cal", 0.0);
  prefs.end();
  
  if (savedPPL > 0) {
    PULSES_PER_LITER   = savedPPL;
    CALIBRATION_FACTOR = (savedCal > 0) ? savedCal : 1.0;
    Serial.printf("[FLOW] Loaded P/L=%.2f, Factor=%.3f\n", 
                  PULSES_PER_LITER, CALIBRATION_FACTOR);
  }
  
  Serial.println("[FLOW] Sensor initialized");
}

// ============================================================
//   UPDATE FLOW DATA
// ============================================================
void updateFlow() {
  noInterrupts();
  unsigned long cp = pulseCount;
  interrupts();

  // Volume aktual
  totalVolumeL = (float)cp / PULSES_PER_LITER * CALIBRATION_FACTOR;

  // Snap ke kelipatan STEP_ML mL
  float stepL   = STEP_ML / 1000.0;
  float snapped = floor(totalVolumeL / stepL) * stepL;
  float nextStep = snapped + stepL;
  displayVolumeL = (abs(totalVolumeL - nextStep) <= TOLERANCE_L)
                   ? nextStep : snapped;

  // Flow rate (L/menit) dihitung tiap 2 detik
  unsigned long now = millis();
  if (now - lastFlowCalc >= 2000) {
    float deltaVol  = totalVolumeL - lastVolForFlow;
    float deltaSec  = (now - lastFlowCalc) / 1000.0;
    flowRateLPM     = (deltaVol / deltaSec) * 60.0;
    if (flowRateLPM < 0) flowRateLPM = 0;
    lastVolForFlow  = totalVolumeL;
    lastFlowCalc    = now;
  }
}

// ============================================================
//   RESET VOLUME
// ============================================================
void resetVolume() {
  noInterrupts();
  pulseCount = 0;
  interrupts();
  totalVolumeL = 0.0;
  displayVolumeL = 0.0;
  flowRateLPM = 0.0;
  lastVolForFlow = 0.0;
  Serial.println("[FLOW] Volume reset");
}

// ============================================================
//   START CALIBRATION
// ============================================================
void startFlowCalibration(float targetLiter) {
  noInterrupts();
  calibStartPulse = pulseCount;
  interrupts();
  calibTargetL = targetLiter;
  calibMode    = true;

  Serial.printf("\n[FLOW CALIB] Target: %.1f L\n", targetLiter);
  Serial.println("[FLOW CALIB] Alirkan air dan kirim 'k' jika selesai");
}

// ============================================================
//   FINISH CALIBRATION
// ============================================================
void finishFlowCalibration() {
  if (!calibMode) {
    Serial.println("[FLOW CALIB] Tidak dalam mode kalibrasi");
    return;
  }
  
  noInterrupts();
  unsigned long endPulse = pulseCount;
  interrupts();

  unsigned long delta = endPulse - calibStartPulse;
  if (delta < 10) {
    Serial.println("[FLOW CALIB] Gagal: pulsa terlalu sedikit!");
    calibMode = false;
    return;
  }

  float newPPL = (float)delta / calibTargetL;
  PULSES_PER_LITER = newPPL;
  CALIBRATION_FACTOR = 1.0;

  // Simpan ke NVS
  prefs.begin("flow_cal", false);
  prefs.putFloat("ppl", newPPL);
  prefs.putFloat("cal", 1.0);
  prefs.end();

  Serial.printf("[FLOW CALIB] Selesai! P/L=%.2f, Delta=%lu pulsa\n", 
                newPPL, delta);
  Serial.println("[FLOW CALIB] Kirim 'r' untuk reset volume");

  calibMode = false;
}

// ============================================================
//   GET FLOW DATA
// ============================================================
float getTotalVolume() {
  return totalVolumeL;
}

float getDisplayVolume() {
  return displayVolumeL;
}

float getFlowRate() {
  return flowRateLPM;
}

unsigned long getPulseCount() {
  return pulseCount;
}

bool isCalibrationMode() {
  return calibMode;
}

// ============================================================
//   PRINT FLOW DATA (Serial)
// ============================================================
void printFlowData() {
  int mL = (int)round(displayVolumeL * 1000.0);
  Serial.println("=== FLOW DATA ===");
  Serial.printf("Volume Display : %.3f L (%d mL)\n", displayVolumeL, mL);
  Serial.printf("Volume Raw     : %.3f L\n", totalVolumeL);
  Serial.printf("Flow Rate      : %.2f L/min\n", flowRateLPM);
  Serial.printf("Total Pulses   : %lu\n", pulseCount);
  Serial.printf("P/L            : %.2f\n", PULSES_PER_LITER);
  Serial.printf("Factor         : %.3f\n", CALIBRATION_FACTOR);
  Serial.println("=================");
}

// ============================================================
//   EXAMPLE SETUP & LOOP (Test Program)
// ============================================================
/*
void setup() {
  Serial.begin(115200);
  initFlowSensor();
  Serial.println("\n=== FLOW SENSOR TEST ===");
  Serial.println("Commands:");
  Serial.println("r  = Reset volume");
  Serial.println("c1 = Kalibrasi 1 liter");
  Serial.println("c2 = Kalibrasi 2 liter");
  Serial.println("k  = Selesai kalibrasi");
  Serial.println("p  = Print data");
  Serial.println("========================");
}

void loop() {
  updateFlow();
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "r") {
      resetVolume();
    } else if (cmd == "c1") {
      startFlowCalibration(1.0);
    } else if (cmd == "c2") {
      startFlowCalibration(2.0);
    } else if (cmd == "k" && calibMode) {
      finishFlowCalibration();
    } else if (cmd == "p") {
      printFlowData();
    }
  }
  
  delay(100);
}
*/

/*
  ESP32 - TDS Sensor
  Pin Analog : GPIO 33
*/

#define TDS_PIN 33
#define VREF 3.3          // Tegangan ADC ESP32
#define ADC_RESOLUTION 4095.0

void setup() {
  Serial.begin(115200);

  analogReadResolution(12); // ADC 12-bit

  Serial.println("=================================");
  Serial.println("TDS Sensor ESP32");
  Serial.println("GPIO : 33");
  Serial.println("=================================");
}

void loop() {

  // Baca ADC
  int adcValue = analogRead(TDS_PIN);

  // Konversi ke tegangan
  float voltage = adcValue * VREF / ADC_RESOLUTION;

  // Faktor kompensasi suhu (25°C)
  float compensationCoefficient = 1.0;
  float compensationVoltage = voltage / compensationCoefficient;

  // Rumus dari DFRobot
  float tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage
                  - 255.86 * compensationVoltage * compensationVoltage
                  + 857.39 * compensationVoltage)
                  * 0.5;

  Serial.print("ADC      : ");
  Serial.print(adcValue);

  Serial.print(" | Voltage : ");
  Serial.print(voltage, 3);
  Serial.print(" V");

  Serial.print(" | TDS : ");
  Serial.print(tdsValue, 2);
  Serial.println(" ppm");

  delay(1000);
}

