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
