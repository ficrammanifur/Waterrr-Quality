/*
 * ============================================================
 *   SMART WATER QUALITY MONITORING SYSTEM
 *   For Reverse Osmosis Depot
 *   ESP32 - WITHOUT PUMP CONTROL
 *   LCD: Minimalist Display
 * ============================================================
 * 
 *   Sensors:
 *   - pH Sensor (GPIO32)
 *   - TDS Sensor (GPIO33)
 *   - Turbidity Sensor (GPIO35) - NTU output
 *   - DS18B20 Temperature (GPIO25)
 *   - Flow Sensor (GPIO18 - Interrupt)
 * 
 *   Outputs:
 *   - LCD I2C 20x4 (0x27)
 *   - LED Hijau (GPIO26) - Indikator LAYAK
 *   - LED Merah (GPIO27) - Indikator TIDAK LAYAK
 * 
 *   Communication:
 *   - MQTT via WiFi
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "water_rules.h"

// ==================== PIN DEFINITIONS ====================
#define PH_PIN          32
#define TDS_PIN         33
#define TURBIDITY_PIN   35
#define DS18B20_PIN     18
#define FLOW_PIN        19
#define LED_HIJAU_PIN   26
#define LED_MERAH_PIN   27
#define LCD_SDA         21
#define LCD_SCL         22
#define LCD_ADDR        0x27
#define BUZZER_PIN      2

// ==================== LCD ====================
LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

// ==================== DS18B20 ====================
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

// ==================== WiFi & MQTT ====================
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiManager wifiManager;
bool wifiConnected = false;
bool mqttConnected = false;

// ==================== MQTT CONFIG ====================
#define MQTT_BROKER     "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "esp32-ro-monitor-001"
#define MQTT_TOPIC_ALL  "watermon/all"

// ==================== SENSOR GLOBALS ====================
float phValue = 7.0;
float phFiltered = 7.0;
float tdsValue = 0.0;
float temperatureC = 25.0;
int turbidityADC = 0;
float turbidityNTU = 0.0;
float turbidityPercent = 0.0;
String turbStatus = "JERNIH";
String waterStatus = "MENUNGGU";

// ==================== FLOW SENSOR ====================
volatile unsigned long pulseCount = 0;
float totalVolumeL = 0.0;
float displayVolumeL = 0.0;
float flowRateLPM = 0.0;
float PULSES_PER_LITER = 310.0;
float CALIBRATION_FACTOR = 1.082;
const float STEP_ML = 250.0;
unsigned long lastFlowCalc = 0;
float lastVolForFlow = 0.0;
bool calibMode = false;
float calibTargetL = 0.0;
unsigned long calibStartPulse = 0;

// ==================== BUFFER ====================
#define AVG_SAMPLES 20
int phADCBuf[AVG_SAMPLES];
int phIdx = 0;
float phEMA = 7.0;
float tdsBuf[AVG_SAMPLES];
int tdsIdx = 0;
int turbBuf[AVG_SAMPLES];
int turbIdx = 0;

// ==================== pH CALIBRATION ====================
const float V4 = 1.350;  const float PH4 = 4.00;
const float V7 = 0.874;  const float PH7 = 6.86;
const float V9 = 0.485;  const float PH9 = 9.18;

// ==================== TURBIDITY CALIBRATION ====================
int TURB_AIR_ADC = 3800;
int TURB_KERUH_ADC = 1800;
int TURB_AIR = 4095;
int TURB_WATER = 3031;

// ==================== TIMING ====================
const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long LCD_INTERVAL = 1000;
const unsigned long MQTT_INTERVAL = 5000;
unsigned long lastSensorRead = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastMQTTPublish = 0;

// ==================== FILTER HEALTH ====================
Preferences prefs;
float filterHealth = 100.0;
int daysLeft = 999;
float dailyVolume[7] = {0};
int dailyIndex = 0;
unsigned long lastDayUpdate = 0;

// ==================== TDS CALIBRATION DATA ====================
struct TDSCalibrationPoint {
    float voltage;
    float ppm;
};

const TDSCalibrationPoint calData[] = {
    {0.5566, 1},    // Amidis (RO water)
    {0.6035, 129},  // Aqua
    {0.6657, 148},  // Le Minerale
    {0.6283, 170},  // Vit
    {0.5941, 181},  // Ades
    {0.7010, 233},  // Keran Mentah
    {0.6420, 216},  // Sumur Mentah
    {0.7707, 356},  // Selokan
    {1.3492, 492},  // Kolam Renang
};

const int calDataCount = sizeof(calData) / sizeof(calData[0]);

// ==================== FUNCTION PROTOTYPES ====================
void initWiFi();
void initMQTT();
void mqttReconnect();
void publishMQTT();
void readSensors();
void readPH();
void readTDS();
void readTemperature();
void readTurbidity();
void updateFlow();
void updateLCD();
void updateLEDs();
void checkWaterQuality();
void initFlowSensor();
void IRAM_ATTR flowISR();
void resetVolume();
void startFlowCalibration(float targetLiter);
void finishFlowCalibration();
float calculatePH(float voltage);
float calculateTDS_Improved(float voltage, float temp);
float adcToNTU(int adc);
void updateFilterHealth();
void updateDailyVolume();
void printStatus();
void beep(int times);
void debugTurbidity();
void calibrateTurbidity();

// ============================================================
//   SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("\n========================================");
  Serial.println("  SMART RO WATER QUALITY MONITOR");
  Serial.println("  System Initializing...");
  Serial.println("  LCD: Minimalist Mode");
  Serial.println("========================================\n");
  
  analogReadResolution(12);
  
  pinMode(BUZZER_PIN, OUTPUT);
  beep(2);
  
  pinMode(LED_HIJAU_PIN, OUTPUT);
  pinMode(LED_MERAH_PIN, OUTPUT);
  digitalWrite(LED_HIJAU_PIN, LOW);
  digitalWrite(LED_MERAH_PIN, LOW);
  
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  SMART RO MONITOR");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing...");
  lcd.setCursor(0, 2);
  lcd.print("  v3.0");
  lcd.setCursor(0, 3);
  lcd.print("  Please Wait");
  delay(2000);
  
  ds18b20.begin();
  initFlowSensor();
  
  for (int i = 0; i < AVG_SAMPLES; i++) {
    phADCBuf[i] = 2048;
    tdsBuf[i] = 0.5;
    turbBuf[i] = 3800;
  }
  Serial.println("[OK] Sensors initialized");
  
  Serial.println("\n[TDS CALIBRATION] Using improved segmented linear calibration");
  Serial.printf("[TDS CALIBRATION] Based on %d data points from dataset\n", calDataCount);
  
  Serial.println("\n[TURBIDITY] Using NTU (Nephelometric Turbidity Unit)");
  Serial.println("[TURBIDITY] Standard: < 5 NTU for drinking water\n");
  
  initWiFi();
  
  if (wifiConnected) {
    initMQTT();
    mqttReconnect();
  }
  
  prefs.begin("filter", true);
  displayVolumeL = prefs.getFloat("volume", 0.0);
  prefs.end();
  
  Serial.println("[OK] System ready!");
  Serial.println("========================================\n");
  Serial.println("Commands:");
  Serial.println("  status   - Show all sensor data");
  Serial.println("  r        - Reset volume");
  Serial.println("  c1/c2    - Flow calibration");
  Serial.println("  k        - Finish flow calibration");
  Serial.println("  reset    - Reset filter health");
  Serial.println("  test     - Test buzzer");
  Serial.println("  turb     - Debug turbidity sensor");
  Serial.println("  cal_turb - Calibrate turbidity sensor");
  Serial.println("========================================\n");
  
  lcd.clear();
  beep(3);
}

// ============================================================
//   LOOP
// ============================================================
void loop() {
  unsigned long now = millis();
  
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
    updateFilterHealth();
    checkWaterQuality();
    updateDailyVolume();
  }
  
  if (now - lastLCDUpdate >= LCD_INTERVAL) {
    lastLCDUpdate = now;
    updateLCD();
  }
  
  updateLEDs();
  
  if (wifiConnected && (now - lastMQTTPublish >= MQTT_INTERVAL)) {
    lastMQTTPublish = now;
    if (!mqttClient.connected()) {
      mqttConnected = false;
      mqttReconnect();
    }
    publishMQTT();
  }
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "r") {
      resetVolume();
      Serial.println("[CMD] Volume reset");
    } else if (cmd == "c1") {
      startFlowCalibration(1.0);
      Serial.println("[CMD] Calibration started - 1L");
    } else if (cmd == "c2") {
      startFlowCalibration(2.0);
      Serial.println("[CMD] Calibration started - 2L");
    } else if (cmd == "k" && calibMode) {
      finishFlowCalibration();
      Serial.println("[CMD] Calibration finished");
    } else if (cmd == "status") {
      printStatus();
    } else if (cmd == "reset") {
      resetFilterHealth();
      Serial.println("[CMD] Filter health reset");
    } else if (cmd == "test") {
      beep(5);
      Serial.println("[CMD] Test buzzer OK");
    } else if (cmd == "turb") {
      debugTurbidity();
    } else if (cmd == "cal_turb") {
      calibrateTurbidity();
    } else if (cmd.startsWith("cal")) {
      int spaceIndex = cmd.indexOf(' ');
      if (spaceIndex > 0) {
        float knownPPM = cmd.substring(spaceIndex + 1).toFloat();
        if (knownPPM > 0) {
          calibrateTDS(knownPPM);
        }
      }
    }
  }
  
  delay(10);
}

// ============================================================
//   SENSOR FUNCTIONS
// ============================================================

float calculatePH(float voltage) {
  if (voltage >= V7) {
    return PH4 + (PH7 - PH4) * (V4 - voltage) / (V4 - V7);
  } else {
    return PH7 + (PH9 - PH7) * (V7 - voltage) / (V7 - V9);
  }
}

void readPH() {
  phADCBuf[phIdx] = analogRead(PH_PIN);
  phIdx = (phIdx + 1) % AVG_SAMPLES;
  
  long sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) {
    sum += phADCBuf[i];
  }
  float avgADC = sum / (float)AVG_SAMPLES;
  float voltage = avgADC * (3.3 / 4095.0);
  
  float phRaw = calculatePH(voltage);
  phEMA = (0.85 * phEMA) + (0.15 * phRaw);
  phFiltered = constrain(phEMA, 0.0, 14.0);
  phValue = phFiltered;
}

float calculateTDS_Improved(float voltage, float temp) {
    float tempComp = 1.0 + 0.02 * (temp - 25.0);
    float vComp = voltage / tempComp;
    
    float tds;
    
    if (vComp <= 0.56) {
        tds = (vComp - 0.5566) * 1000 + 1;
        if (tds < 0) tds = 0;
    }
    else if (vComp <= 0.60) {
        float v1 = 0.5566, ppm1 = 1;
        float v2 = 0.6035, ppm2 = 129;
        float slope = (ppm2 - ppm1) / (v2 - v1);
        tds = slope * (vComp - v1) + ppm1;
    }
    else if (vComp <= 0.68) {
        float v1 = 0.6035, ppm1 = 129;
        float v2 = 0.7010, ppm2 = 233;
        float slope = (ppm2 - ppm1) / (v2 - v1);
        tds = slope * (vComp - v1) + ppm1;
    }
    else if (vComp <= 0.78) {
        float v1 = 0.7010, ppm1 = 233;
        float v2 = 0.7707, ppm2 = 356;
        float slope = (ppm2 - ppm1) / (v2 - v1);
        tds = slope * (vComp - v1) + ppm1;
    }
    else {
        float v1 = 0.7707, ppm1 = 356;
        float v2 = 1.3492, ppm2 = 492;
        float slope = (ppm2 - ppm1) / (v2 - v1);
        tds = slope * (vComp - v1) + ppm1;
    }
    
    if (tds < 0) tds = 0;
    if (tds > 5000) tds = 5000;
    
    return tds;
}

void calibrateTDS(float knownPPM) {
    Serial.println("\n=== TDS CALIBRATION ===");
    Serial.println("Place sensor in solution with known PPM");
    Serial.printf("Target PPM: %.0f\n", knownPPM);
    Serial.println("Reading in 5 seconds...");
    delay(5000);
    
    float sumV = 0;
    int samples = 50;
    for (int i = 0; i < samples; i++) {
        int adc = analogRead(TDS_PIN);
        sumV += adc * (3.3 / 4095.0);
        delay(50);
    }
    float avgV = sumV / samples;
    
    Serial.printf("Average Voltage: %.4f V\n", avgV);
    Serial.printf("ADC Average: %.0f\n", (avgV / 3.3) * 4095.0);
    Serial.printf("Current TDS Reading: %.1f ppm\n", calculateTDS_Improved(avgV, temperatureC));
    
    float currentTDS = calculateTDS_Improved(avgV, temperatureC);
    if (currentTDS > 0) {
        float correctionFactor = knownPPM / currentTDS;
        Serial.printf("Correction Factor: %.3f\n", correctionFactor);
    }
    
    Serial.println("========================\n");
}

void readTDS() {
    int adc = analogRead(TDS_PIN);
    float voltage = adc * (3.3 / 4095.0);
    
    tdsBuf[tdsIdx] = voltage;
    tdsIdx = (tdsIdx + 1) % AVG_SAMPLES;
    
    float sum = 0;
    for (int i = 0; i < AVG_SAMPLES; i++) {
        sum += tdsBuf[i];
    }
    float avgV = sum / AVG_SAMPLES;
    
    tdsValue = constrain(calculateTDS_Improved(avgV, temperatureC), 0, 9999);
}

void readTemperature() {
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);
  if (t > -50 && t < 125) {
    temperatureC = t;
  }
}

float adcToNTU(int adc) {
  if (adc >= TURB_AIR_ADC) {
    return 0.0;
  }
  if (adc <= TURB_KERUH_ADC) {
    return 100.0;
  }
  
  float ntu = 100.0 * (TURB_AIR_ADC - adc) / (TURB_AIR_ADC - TURB_KERUH_ADC);
  
  if (ntu < 0) ntu = 0;
  if (ntu > 100) ntu = 100;
  
  return ntu;
}

void readTurbidity() {
  turbBuf[turbIdx] = analogRead(TURBIDITY_PIN);
  turbIdx = (turbIdx + 1) % AVG_SAMPLES;
  
  long sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) {
    sum += turbBuf[i];
  }
  turbidityADC = sum / AVG_SAMPLES;
  
  turbidityNTU = adcToNTU(turbidityADC);
  turbidityPercent = constrain(100.0 - (turbidityNTU * 1.0), 0, 100);
  
  if (turbidityNTU <= 0.5) {
    turbStatus = "SANGAT JERNIH";
  } else if (turbidityNTU <= 1.0) {
    turbStatus = "JERNIH";
  } else if (turbidityNTU <= 2.0) {
    turbStatus = "CUKUP JERNIH";
  } else if (turbidityNTU <= 5.0) {
    turbStatus = "AGAK KERUH";
  } else if (turbidityNTU <= 10) {
    turbStatus = "KERUH";
  } else {
    turbStatus = "SANGAT KERUH";
  }
}

void readSensors() {
  readTemperature();
  readPH();
  readTDS();
  readTurbidity();
  updateFlow();
}

// ============================================================
//   FLOW SENSOR FUNCTIONS
// ============================================================

void IRAM_ATTR flowISR() {
  pulseCount++;
}

void initFlowSensor() {
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);
  
  prefs.begin("flow_cal", true);
  float savedPPL = prefs.getFloat("ppl", 0.0);
  float savedCal = prefs.getFloat("cal", 0.0);
  prefs.end();
  
  if (savedPPL > 0) {
    PULSES_PER_LITER = savedPPL;
    CALIBRATION_FACTOR = (savedCal > 0) ? savedCal : 1.0;
    Serial.printf("[FLOW] Loaded P/L=%.2f\n", PULSES_PER_LITER);
  }
}

void updateFlow() {
  noInterrupts();
  unsigned long cp = pulseCount;
  interrupts();
  
  totalVolumeL = (float)cp / PULSES_PER_LITER * CALIBRATION_FACTOR;
  
  float stepL = STEP_ML / 1000.0;
  float snapped = floor(totalVolumeL / stepL) * stepL;
  displayVolumeL = snapped;
  
  unsigned long now = millis();
  if (now - lastFlowCalc >= 2000) {
    float deltaVol = totalVolumeL - lastVolForFlow;
    float deltaSec = (now - lastFlowCalc) / 1000.0;
    flowRateLPM = (deltaVol / deltaSec) * 60.0;
    if (flowRateLPM < 0) flowRateLPM = 0;
    lastVolForFlow = totalVolumeL;
    lastFlowCalc = now;
  }
}

void resetVolume() {
  noInterrupts();
  pulseCount = 0;
  interrupts();
  totalVolumeL = 0.0;
  displayVolumeL = 0.0;
  flowRateLPM = 0.0;
  lastVolForFlow = 0.0;
}

void startFlowCalibration(float targetLiter) {
  noInterrupts();
  calibStartPulse = pulseCount;
  interrupts();
  calibTargetL = targetLiter;
  calibMode = true;
  Serial.printf("[CALIB] Target: %.1f L\n", targetLiter);
}

void finishFlowCalibration() {
  if (!calibMode) return;
  
  noInterrupts();
  unsigned long endPulse = pulseCount;
  interrupts();
  
  unsigned long delta = endPulse - calibStartPulse;
  if (delta < 10) {
    Serial.println("[CALIB] Failed: too few pulses!");
    calibMode = false;
    return;
  }
  
  float newPPL = (float)delta / calibTargetL;
  PULSES_PER_LITER = newPPL;
  CALIBRATION_FACTOR = 1.0;
  
  prefs.begin("flow_cal", false);
  prefs.putFloat("ppl", newPPL);
  prefs.putFloat("cal", 1.0);
  prefs.end();
  
  Serial.printf("[CALIB] Complete! P/L=%.2f\n", newPPL);
  calibMode = false;
}

// ============================================================
//   FILTER HEALTH FUNCTIONS
// ============================================================

void updateFilterHealth() {
  float volumeFactor = (displayVolumeL / 30000.0) * 100.0;
  if (volumeFactor > 100.0) volumeFactor = 100.0;
  float volumeScore = 100.0 - volumeFactor;
  
  float tdsScore = 0.0;
  if (tdsValue <= 50) tdsScore = 100.0;
  else if (tdsValue <= 100) tdsScore = 80.0;
  else if (tdsValue <= 200) tdsScore = 60.0;
  else if (tdsValue <= 300) tdsScore = 40.0;
  else if (tdsValue <= 400) tdsScore = 20.0;
  else tdsScore = 0.0;
  
  float turbidityScore = 0.0;
  if (turbidityNTU <= 1.0) turbidityScore = 100.0;
  else if (turbidityNTU <= 2.0) turbidityScore = 80.0;
  else if (turbidityNTU <= 5.0) turbidityScore = 60.0;
  else if (turbidityNTU <= 10.0) turbidityScore = 40.0;
  else if (turbidityNTU <= 20.0) turbidityScore = 20.0;
  else turbidityScore = 0.0;
  
  filterHealth = (volumeScore * 0.4) + (tdsScore * 0.3) + (turbidityScore * 0.3);
  filterHealth = constrain(filterHealth, 0.0, 100.0);
  
  float avgDaily = 0;
  int count = 0;
  for (int i = 0; i < 7; i++) {
    if (dailyVolume[i] > 0) {
      avgDaily += dailyVolume[i];
      count++;
    }
  }
  avgDaily = (count > 0) ? (avgDaily / count) : 1.0;
  
  float remaining = (displayVolumeL < 30000.0) ? (30000.0 - displayVolumeL) : 0;
  daysLeft = (avgDaily > 0 && remaining > 0) ? ceil(remaining / avgDaily) : 0;
}

void updateDailyVolume() {
  unsigned long now = millis();
  if (now - lastDayUpdate >= 86400000UL) {
    dailyVolume[dailyIndex] = displayVolumeL;
    dailyIndex = (dailyIndex + 1) % 7;
    lastDayUpdate = now;
  }
}

void resetFilterHealth() {
  displayVolumeL = 0.0;
  for (int i = 0; i < 7; i++) {
    dailyVolume[i] = 0;
  }
  dailyIndex = 0;
  filterHealth = 100.0;
  daysLeft = 999;
  
  prefs.begin("filter", false);
  prefs.putFloat("volume", 0.0);
  prefs.end();
}

// ============================================================
//   WATER QUALITY CHECK
// ============================================================

void checkWaterQuality() {
  bool layak = isWaterLayak(phValue, tdsValue, turbidityNTU, temperatureC);
  waterStatus = layak ? "LAYAK" : "TIDAK LAYAK";
  updateLEDs();
}

void updateLEDs() {
  bool layak = isWaterLayak(phValue, tdsValue, turbidityNTU, temperatureC);
  
  if (layak) {
    digitalWrite(LED_HIJAU_PIN, HIGH);
    digitalWrite(LED_MERAH_PIN, LOW);
  } else {
    digitalWrite(LED_HIJAU_PIN, LOW);
    digitalWrite(LED_MERAH_PIN, HIGH);
  }
}

// ============================================================
//   LCD FUNCTIONS - MINIMALIST STYLE
//   Baris 1: Title + WiFi
//   Baris 2: pH 7.20  TDS 18
//   Baris 3: 96.7NTU  25.0C
//   Baris 4: STATUS: LAYAK
// ============================================================

void updateLCD() {
  lcd.clear();
  
  // ===== BARIS 1: Title + WiFi =====
  lcd.setCursor(0, 0);
  lcd.print("SMART RO MONITOR");
  if (wifiConnected) {
    lcd.setCursor(19, 0);
    lcd.print("W");
  }
  
  // ===== BARIS 2: pH & TDS =====
  lcd.setCursor(0, 1);
  char buf[21];
  sprintf(buf, "pH %.2f  TDS %.0f", phValue, tdsValue);
  lcd.print(buf);
  
  // ===== BARIS 3: Turbidity NTU & Temperature (TANPA LABEL) =====
  lcd.setCursor(0, 2);
  // Format: "XX.XNTU  XX.XC" (14 karakter, pas di 20 karakter)
  sprintf(buf, "%.1fNTU  %.1fC", turbidityNTU, temperatureC);
  lcd.print(buf);
  
  // ===== BARIS 4: Status =====
  lcd.setCursor(0, 3);
  bool layak = isWaterLayak(phValue, tdsValue, turbidityNTU, temperatureC);
  if (layak) {
    lcd.print("STATUS: LAYAK    ");
  } else {
    lcd.print("STATUS:TIDAK LAYAK");
  }
}

// ============================================================
//   WiFi & MQTT FUNCTIONS
// ============================================================

void initWiFi() {
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setDebugOutput(false);
  
  Serial.println("[WIFI] Starting WiFiManager...");
  Serial.println("[WIFI] If not connected, open hotspot 'WaterMonitor'");
  
  bool connected = wifiManager.autoConnect("WaterMonitor", "water123");
  
  if (connected) {
    wifiConnected = true;
    Serial.printf("[WIFI] Connected to: %s\n", WiFi.SSID().c_str());
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    wifiConnected = false;
    Serial.println("[WIFI] Timeout - running OFFLINE mode");
  }
}

void initMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(30);
}

void mqttReconnect() {
  if (mqttClient.connected()) return;
  if (!wifiConnected) return;
  
  Serial.print("[MQTT] Connecting...");
  bool ok = mqttClient.connect(MQTT_CLIENT_ID);
  
  if (ok) {
    mqttConnected = true;
    Serial.println(" OK");
  } else {
    mqttConnected = false;
    Serial.printf(" FAILED (rc=%d)\n", mqttClient.state());
  }
}

void publishMQTT() {
  if (!mqttConnected) return;
  
  char json[512];
  sprintf(json,
    "{"
    "\"ph\":%.2f,"
    "\"tds\":%.0f,"
    "\"turbidity_ntu\":%.2f,"
    "\"temperature\":%.2f,"
    "\"status\":\"%s\","
    "\"health\":%.0f,"
    "\"days_left\":%d,"
    "\"volume\":%.3f,"
    "\"flow_rate\":%.2f"
    "}",
    phValue,
    tdsValue,
    turbidityNTU,
    temperatureC,
    waterStatus.c_str(),
    filterHealth,
    daysLeft,
    displayVolumeL,
    flowRateLPM
  );
  
  mqttClient.publish(MQTT_TOPIC_ALL, json);
}

// ============================================================
//   UTILITY FUNCTIONS
// ============================================================

void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

void printStatus() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║        SYSTEM STATUS                 ║");
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ pH          : %6.2f                  ║\n", phValue);
  Serial.printf("║ TDS         : %6.0f ppm              ║\n", tdsValue);
  Serial.printf("║ Temperature : %6.2f °C               ║\n", temperatureC);
  Serial.printf("║ Turbidity   : %6.2f NTU (%s)   ║\n", turbidityNTU, turbStatus.c_str());
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ Volume      : %8.3f L               ║\n", displayVolumeL);
  Serial.printf("║ Flow Rate   : %8.2f L/min           ║\n", flowRateLPM);
  Serial.printf("║ Pulses      : %8lu                  ║\n", pulseCount);
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ Status      : %s                    ║\n", waterStatus.c_str());
  Serial.printf("║ Filter      : %6.0f %%               ║\n", filterHealth);
  Serial.printf("║ Days Left   : %6d                   ║\n", daysLeft);
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ WiFi        : %s                    ║\n", wifiConnected ? "Connected" : "Offline");
  Serial.printf("║ MQTT        : %s                    ║\n", mqttConnected ? "Connected" : "Disconnected");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  String detail = getDetailedStatus(phValue, tdsValue, turbidityNTU, temperatureC);
  Serial.printf("[DETAIL] %s\n", detail.c_str());
  Serial.printf("[TDS] Raw Voltage: %.4f V\n", tdsBuf[(tdsIdx - 1 + AVG_SAMPLES) % AVG_SAMPLES]);
  Serial.printf("[TURB] ADC: %d, NTU: %.2f\n", turbidityADC, turbidityNTU);
}

// ============================================================
//   TURBIDITY DEBUG & CALIBRATION
// ============================================================

void debugTurbidity() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║        TURBIDITY DEBUG              ║");
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ ADC Raw       : %6d                  ║\n", turbidityADC);
  Serial.printf("║ NTU           : %8.2f               ║\n", turbidityNTU);
  Serial.printf("║ Status        : %s             ║\n", turbStatus.c_str());
  Serial.printf("║ TURB_AIR_ADC  : %6d                  ║\n", TURB_AIR_ADC);
  Serial.printf("║ TURB_KERUH_ADC: %6d                  ║\n", TURB_KERUH_ADC);
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  if (turbidityADC > 3500) {
    Serial.println("✅ ADC > 3500 → Air SANGAT JERNIH (NTU < 1.0)");
    Serial.println("   → Status seharusnya LAYAK");
  } else if (turbidityADC > 3000) {
    Serial.println("✅ ADC 3000-3500 → Air JERNIH (NTU 1.0-5.0)");
    Serial.println("   → Status seharusnya LAYAK");
  } else if (turbidityADC > 2500) {
    Serial.println("⚠️ ADC 2500-3000 → Air AGAK KERUH (NTU 5.0-20.0)");
  } else if (turbidityADC > 2000) {
    Serial.println("❌ ADC 2000-2500 → Air KERUH (NTU 20.0-50.0)");
  } else {
    Serial.println("❌ ADC < 2000 → Air SANGAT KERUH (NTU > 50.0)");
  }
  
  Serial.println("\nRekomendasi:");
  if (turbidityADC < 3000) {
    Serial.println("1. Periksa sensor di air jernih → ADC harus > 3500");
    Serial.println("2. Bersihkan lensa sensor turbidity");
    Serial.println("3. Periksa koneksi kabel sensor");
    Serial.println("4. Kalibrasi ulang dengan perintah 'cal_turb'");
  } else {
    Serial.println("1. Sensor terlihat normal");
    Serial.println("2. Jika air memang jernih, kalibrasi ulang dengan 'cal_turb'");
  }
  Serial.println("");
}

void calibrateTurbidity() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║     TURBIDITY CALIBRATION           ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  Serial.println("Step 1: Place sensor in CLEAR water");
  Serial.println("(Air yang sangat jernih / aquades)");
  Serial.println("Press any key when ready...");
  while (!Serial.available()) {
    delay(100);
  }
  Serial.read();
  
  long sumClear = 0;
  Serial.print("Reading ADC in clear water...");
  for (int i = 0; i < 50; i++) {
    sumClear += analogRead(TURBIDITY_PIN);
    delay(50);
    if (i % 10 == 0) Serial.print(".");
  }
  int clearADC = sumClear / 50;
  Serial.println(" DONE!");
  Serial.printf("ADC in CLEAR water: %d\n", clearADC);
  
  Serial.println("\nStep 2: Place sensor in TURBID water");
  Serial.println("(Air yang keruh / campuran tanah)");
  Serial.println("Press any key when ready...");
  while (!Serial.available()) {
    delay(100);
  }
  Serial.read();
  
  long sumTurbid = 0;
  Serial.print("Reading ADC in turbid water...");
  for (int i = 0; i < 50; i++) {
    sumTurbid += analogRead(TURBIDITY_PIN);
    delay(50);
    if (i % 10 == 0) Serial.print(".");
  }
  int turbidADC = sumTurbid / 50;
  Serial.println(" DONE!");
  Serial.printf("ADC in TURBID water: %d\n", turbidADC);
  
  TURB_AIR_ADC = clearADC;
  TURB_KERUH_ADC = turbidADC;
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║     CALIBRATION COMPLETE            ║");
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ TURB_AIR_ADC   : %6d                  ║\n", TURB_AIR_ADC);
  Serial.printf("║ TURB_KERUH_ADC : %6d                  ║\n", TURB_KERUH_ADC);
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  Serial.println("Calibration saved! Test with 'status' command");
}
