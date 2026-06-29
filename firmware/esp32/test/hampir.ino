/*
 * ============================================================
 *   WATER QUALITY MONITOR - ESP32
 *   v3.0 - Full Integration
 * ============================================================
 *  Sensors  : pH (GPIO32), TDS (GPIO33), DS18B20 (GPIO34),
 *             Turbidity (GPIO35), Flow (GPIO18)
 *  Display  : I2C LCD 20x4 (0x27) — 3 halaman auto-rotate
 *  Network  : WiFiManager (hybrid online/offline)
 *  IoT      : MQTT (opsional, jika WiFi tersambung)
 * ============================================================
 *
 *  Library yang dibutuhkan (install via Library Manager):
 *  - LiquidCrystal_I2C  (Frank de Brabander)
 *  - OneWire            (Paul Stoffregen)
 *  - DallasTemperature  (Miles Burton)
 *  - WiFiManager        (tzapu) ← versi ESP32
 *  - PubSubClient       (Nick O'Leary) ← untuk MQTT
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiManager.h>          // WiFiManager auto-portal
#include <PubSubClient.h>         // MQTT client
#include <Preferences.h>          // NVS storage (simpan kalibrasi)

// ==================== PIN DEFINITIONS ====================
#define PH_PIN          32
#define TDS_PIN         33
#define ONE_WIRE_BUS    34
#define TURBIDITY_PIN   35
#define FLOW_PIN        18
#define BUZZER_PIN       2
#define LCD_ADDR        0x27

// ==================== MQTT CONFIG ====================
// Ganti sesuai broker Anda. Jika tidak pakai MQTT, biarkan kosong ("").
#define MQTT_BROKER     "broker.hivemq.com"   // broker publik gratis
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "esp32-watermon-001"
#define MQTT_USER       ""                    // kosong jika tidak pakai auth
#define MQTT_PASS       ""
#define MQTT_TOPIC_PH        "watermon/ph"
#define MQTT_TOPIC_TDS       "watermon/tds"
#define MQTT_TOPIC_TEMP      "watermon/temperature"
#define MQTT_TOPIC_TURB      "watermon/turbidity"
#define MQTT_TOPIC_FLOW      "watermon/flowrate"
#define MQTT_TOPIC_VOL       "watermon/volume"
#define MQTT_TOPIC_STATUS    "watermon/status"
#define MQTT_TOPIC_ALL       "watermon/all"   // JSON gabungan semua data

// ==================== FLOW KALIBRASI ====================
float PULSES_PER_LITER   = 310.0;
float CALIBRATION_FACTOR = 1.082;
const float STEP_ML      = 250.0;   // Snap per 250 mL
const float TOLERANCE_L  = 0.05;

// ==================== pH KALIBRASI (3 titik) ====================
const float V4  = 1.304;  const float PH4  = 4.00;
const float V7  = 1.048;  const float PH7  = 6.86;
const float V9  = 0.835;  const float PH9  = 9.18;

// ==================== TURBIDITY KALIBRASI ====================
int TURB_AIR   = 4095;   // Sensor di udara
int TURB_WATER = 3031;   // Sensor di air jernih

// ==================== LCD PAGES ====================
#define PAGE_COUNT       3
#define PAGE_INTERVAL    4000   // Ganti halaman tiap 4 detik (ms)
int currentPage = 0;
unsigned long lastPageSwitch = 0;

// ==================== TIMING ====================
const unsigned long SENSOR_INTERVAL  = 1000;
const unsigned long LCD_INTERVAL     = 1200;
const unsigned long SERIAL_INTERVAL  = 3000;
const unsigned long MQTT_INTERVAL    = 5000;
const unsigned long WIFI_CHECK       = 30000;

unsigned long lastSensor  = 0;
unsigned long lastLCD     = 0;
unsigned long lastSerial  = 0;
unsigned long lastMQTT    = 0;
unsigned long lastWifiChk = 0;

// ==================== GLOBALS ====================
LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;

// Status koneksi
bool wifiConnected  = false;
bool mqttConnected  = false;
String wifiSSID     = "";

// Sensor values
float temperatureC     = 25.0;
float phValue          = 7.0;
float phFiltered       = 7.0;
float tdsValue         = 0.0;
int   turbidityADC     = 0;
float turbidityPercent = 0.0;   // kejernihan %
String turbStatus      = "JERNIH";
String waterQuality    = "GOOD";

// Flow
volatile unsigned long pulseCount = 0;
float totalVolumeL   = 0.0;
float displayVolumeL = 0.0;
float flowRateLPM    = 0.0;   // L per menit
unsigned long lastFlowCalc  = 0;
unsigned long lastPulseSnap = 0;
float lastVolForFlow = 0.0;

// Calibration mode
bool  calibMode       = false;
float calibTargetL    = 0.0;
unsigned long calibStartPulse = 0;

// Smoothing buffers
const int AVG_SAMPLES = 20;
int   phADCBuf[AVG_SAMPLES];   int phIdx   = 0;
float phEMA = 7.0;
float tdsBuf[AVG_SAMPLES];     int tdsIdx  = 0;
int   turbBuf[AVG_SAMPLES];    int turbIdx = 0;

// ==================== ISR ====================
void IRAM_ATTR flowISR() {
  pulseCount++;
}

// ============================================================
//   BEEP
// ============================================================
void beep(int times, int onMs = 80, int offMs = 120) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);  delay(offMs);
  }
}

// ============================================================
//   pH — 3-point interpolation
// ============================================================
float calcPH(float v) {
  if (v >= V7)
    return PH4 + (PH7 - PH4) * (V4 - v) / (V4 - V7);
  else
    return PH7 + (PH9 - PH7) * (V7 - v) / (V7 - V9);
}

// ============================================================
//   TDS — DFRobot formula
// ============================================================
float calcTDS(float v, float temp) {
  if (v <= 0) return 0.0;
  float comp = 1.0 + 0.02 * (temp - 25.0);
  float vComp = v / comp;
  float tds = (133.42 * vComp * vComp * vComp
             - 255.86 * vComp * vComp
             + 857.39 * vComp) * 0.5;
  return (tds < 0) ? 0.0 : tds;
}

// ============================================================
//   WATER QUALITY ASSESSMENT
// ============================================================
String assessQuality() {
  // Kriteria sederhana berdasarkan pH & TDS
  bool phOK  = (phFiltered >= 6.5 && phFiltered <= 8.5);
  bool tdsOK = (tdsValue < 500);
  bool turbOK = (turbidityPercent >= 50.0);

  if (phOK && tdsOK && turbOK)  return "GOOD";
  if (!phOK && !tdsOK)          return "BAD";
  return "FAIR";
}

// ============================================================
//   READ SENSORS
// ============================================================
void readTemperature() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t > -50 && t < 125) temperatureC = t;
}

void readPH() {
  phADCBuf[phIdx] = analogRead(PH_PIN);
  phIdx = (phIdx + 1) % AVG_SAMPLES;

  long sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) sum += phADCBuf[i];
  float avgADC = sum / (float)AVG_SAMPLES;
  float v = avgADC * (3.3 / 4095.0);

  float phRaw = calcPH(v);
  phEMA = (0.85 * phEMA) + (0.15 * phRaw);
  phFiltered = constrain(phEMA, 0.0, 14.0);
  phValue = phFiltered;
}

void readTDS() {
  int adc = analogRead(TDS_PIN);
  float v  = adc * (3.3 / 4095.0);
  tdsBuf[tdsIdx] = v;
  tdsIdx = (tdsIdx + 1) % AVG_SAMPLES;

  float sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) sum += tdsBuf[i];
  float avgV = sum / AVG_SAMPLES;
  tdsValue = constrain(calcTDS(avgV, temperatureC), 0, 9999);
}

void readTurbidity() {
  turbBuf[turbIdx] = analogRead(TURBIDITY_PIN);
  turbIdx = (turbIdx + 1) % AVG_SAMPLES;

  long sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) sum += turbBuf[i];
  turbidityADC = sum / AVG_SAMPLES;

  // Hitung persen kejernihan
  if (turbidityADC >= TURB_AIR) {
    turbidityPercent = 100.0;
  } else if (turbidityADC <= TURB_WATER) {
    turbidityPercent = 0.0;
  } else {
    turbidityPercent = (float)(turbidityADC - TURB_WATER) /
                       (float)(TURB_AIR - TURB_WATER) * 100.0;
  }
  turbidityPercent = constrain(turbidityPercent, 0, 100);

  if (turbidityPercent >= 80)      turbStatus = "SANGAT JERNIH";
  else if (turbidityPercent >= 50) turbStatus = "CUKUP JERNIH";
  else if (turbidityPercent >= 20) turbStatus = "AGAK KERUH";
  else                             turbStatus = "KERUH";
}

void updateFlow() {
  noInterrupts();
  unsigned long cp = pulseCount;
  interrupts();

  // Volume aktual
  totalVolumeL = (float)cp / PULSES_PER_LITER * CALIBRATION_FACTOR;

  // Snap ke kelipatan 250 mL
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
//   LCD PAGES
// ============================================================
// Helper: pad string ke N karakter (isi spasi kanan)
String pad(String s, int n) {
  while ((int)s.length() < n) s += ' ';
  if ((int)s.length() > n) s = s.substring(0, n);
  return s;
}

void lcdPrint(int row, String text) {
  lcd.setCursor(0, row);
  lcd.print(pad(text, 20));
}

void showPage1() {
  // Halaman 1 — Parameter Air
  char buf[21];

  lcdPrint(0, "--------------------");
  lcdPrint(1, "Water Quality");

  sprintf(buf, "Temp : %.1f C", temperatureC);
  lcdPrint(2, String(buf));

  sprintf(buf, "pH   : %.2f", phValue);
  // Tambah indikator pH
  if (phValue < 6.5)       strcat(buf, " LOW");
  else if (phValue > 8.5)  strcat(buf, " HI");
  lcdPrint(3, String(buf));
}

void showPage1b() {
  // Halaman 1b — TDS (gantian dengan pH di rotate berikutnya)
  // (kita satukan di page 1 dengan scroll baris 2 baris terakhir)
  // Sebenarnya kita punya 4 baris, tampilkan semua:
  char buf[21];

  lcdPrint(0, "--------------------");

  sprintf(buf, "Temp : %.1f C", temperatureC);
  lcdPrint(1, String(buf));

  sprintf(buf, "pH   : %.2f", phValue);
  lcdPrint(2, String(buf));

  sprintf(buf, "TDS  : %d ppm", (int)tdsValue);
  lcdPrint(3, String(buf));
}

void showPage2() {
  // Halaman 2 — Turbidity & Flow
  char buf[21];

  lcdPrint(0, "--------------------");
  lcdPrint(1, "Water Flow");

  sprintf(buf, "Clear : %d %%", (int)turbidityPercent);
  lcdPrint(2, String(buf));

  int mL = (int)round(displayVolumeL * 1000.0);
  sprintf(buf, "Vol: %.3fL(%dmL)", displayVolumeL, mL);
  lcdPrint(3, String(buf));
}

void showPage3() {
  // Halaman 3 — Status Sistem
  char buf[21];

  lcdPrint(0, "--------------------");
  lcdPrint(1, "Status Air");

  sprintf(buf, "Quality : %s", waterQuality.c_str());
  lcdPrint(2, String(buf));

  if (wifiConnected) {
    if (mqttConnected)
      lcdPrint(3, "MQTT : Connected");
    else
      lcdPrint(3, "WiFi : " + wifiSSID.substring(0, 13));
  } else {
    lcdPrint(3, "Mode : OFFLINE");
  }
}

void updateLCD() {
  unsigned long now = millis();

  // Ganti halaman
  if (now - lastPageSwitch >= PAGE_INTERVAL) {
    lastPageSwitch = now;
    currentPage = (currentPage + 1) % PAGE_COUNT;
    lcd.clear();
  }

  switch (currentPage) {
    case 0: showPage1b(); break;
    case 1: showPage2();  break;
    case 2: showPage3();  break;
  }
}

// ============================================================
//   SERIAL OUTPUT
// ============================================================
void printSerial() {
  int mL = (int)round(displayVolumeL * 1000.0);
  Serial.println("\n==========================================");
  Serial.printf("Volume      : %.3f L (%d mL)\n", displayVolumeL, mL);
  Serial.printf("Raw Volume  : %.3f L\n", totalVolumeL);
  Serial.printf("Flow Rate   : %.2f L/min\n", flowRateLPM);
  Serial.printf("Total Pulses: %lu\n", pulseCount);
  Serial.println("------------------------------------------");
  Serial.printf("pH          : %.2f\n", phValue);
  Serial.printf("TDS         : %.0f ppm\n", tdsValue);
  Serial.printf("Temperature : %.2f C\n", temperatureC);
  Serial.printf("Turbidity   : %.0f%% (%s)\n", turbidityPercent, turbStatus.c_str());
  Serial.println("------------------------------------------");
  Serial.printf("Quality     : %s\n", waterQuality.c_str());
  Serial.printf("WiFi        : %s\n", wifiConnected ? wifiSSID.c_str() : "OFFLINE");
  Serial.printf("MQTT        : %s\n", mqttConnected ? "Connected" : "Disconnected");
  Serial.println("==========================================");
}

// ============================================================
//   MQTT
// ============================================================
void mqttReconnect() {
  if (!wifiConnected) return;
  if (mqttClient.connected()) return;

  Serial.print("[MQTT] Connecting...");
  bool ok;
  if (strlen(MQTT_USER) > 0)
    ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
  else
    ok = mqttClient.connect(MQTT_CLIENT_ID);

  if (ok) {
    mqttConnected = true;
    Serial.println(" OK");
    mqttClient.publish(MQTT_TOPIC_STATUS, "online");
  } else {
    mqttConnected = false;
    Serial.printf(" FAILED (rc=%d)\n", mqttClient.state());
  }
}

void publishMQTT() {
  if (!mqttConnected) return;

  char buf[64];

  sprintf(buf, "%.2f", phValue);
  mqttClient.publish(MQTT_TOPIC_PH, buf);

  sprintf(buf, "%.0f", tdsValue);
  mqttClient.publish(MQTT_TOPIC_TDS, buf);

  sprintf(buf, "%.2f", temperatureC);
  mqttClient.publish(MQTT_TOPIC_TEMP, buf);

  sprintf(buf, "%.0f", turbidityPercent);
  mqttClient.publish(MQTT_TOPIC_TURB, buf);

  sprintf(buf, "%.2f", flowRateLPM);
  mqttClient.publish(MQTT_TOPIC_FLOW, buf);

  sprintf(buf, "%.3f", displayVolumeL);
  mqttClient.publish(MQTT_TOPIC_VOL, buf);

  // JSON gabungan
  int mL = (int)round(displayVolumeL * 1000.0);
  char json[256];
  sprintf(json,
    "{\"ph\":%.2f,\"tds\":%.0f,\"temp\":%.2f,\"clarity\":%.0f,"
    "\"flow\":%.2f,\"vol_L\":%.3f,\"vol_mL\":%d,\"quality\":\"%s\"}",
    phValue, tdsValue, temperatureC, turbidityPercent,
    flowRateLPM, displayVolumeL, mL, waterQuality.c_str()
  );
  mqttClient.publish(MQTT_TOPIC_ALL, json);

  Serial.println("[MQTT] Published");
}

// ============================================================
//   WIFI MANAGER
// ============================================================
void initWiFi() {
  WiFiManager wm;

  // Custom parameter opsional (misal MQTT broker)
  // WiFiManagerParameter custom_mqtt("mqtt", "MQTT Broker", MQTT_BROKER, 40);
  // wm.addParameter(&custom_mqtt);

  // Timeout portal: 60 detik. Jika tidak ada koneksi, lanjut offline.
  wm.setConfigPortalTimeout(60);

  // Nonaktifkan debug WiFiManager (opsional)
  // wm.setDebugOutput(false);

  lcd.clear();
  lcdPrint(0, "====================");
  lcdPrint(1, " WiFi Setup Mode");
  lcdPrint(2, " SSID: WaterMonitor");
  lcdPrint(3, " IP  : 192.168.4.1");

  Serial.println("[WiFi] Starting WiFiManager...");
  Serial.println("[WiFi] Jika belum tersambung, buka hotspot 'WaterMonitor'");
  Serial.println("[WiFi] lalu buka browser ke 192.168.4.1");

  bool connected = wm.autoConnect("WaterMonitor", "water123");

  if (connected) {
    wifiConnected = true;
    wifiSSID      = WiFi.SSID();
    Serial.println("[WiFi] Connected: " + wifiSSID);
    Serial.println("[WiFi] IP: " + WiFi.localIP().toString());

    lcd.clear();
    lcdPrint(0, "WiFi Connected!");
    lcdPrint(1, wifiSSID.substring(0, 20));
    lcdPrint(2, WiFi.localIP().toString());
    lcdPrint(3, "Starting MQTT...");
    delay(2000);

    // Setup MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setKeepAlive(30);
    mqttReconnect();

  } else {
    wifiConnected = false;
    Serial.println("[WiFi] Timeout — berjalan OFFLINE");

    lcd.clear();
    lcdPrint(0, "====================");
    lcdPrint(1, "  MODE OFFLINE");
    lcdPrint(2, "  Data lokal saja");
    lcdPrint(3, "  Sensor aktif OK");
    delay(2000);
  }
}

// ============================================================
//   CALIBRATION (Flow)
// ============================================================
void startCalibration(float targetLiter) {
  noInterrupts();
  calibStartPulse = pulseCount;
  interrupts();
  calibTargetL = targetLiter;
  calibMode    = true;

  lcd.clear();
  lcdPrint(0, "=== KALIBRASI ===");
  char buf[21];
  sprintf(buf, "Target: %.1f L", targetLiter);
  lcdPrint(1, String(buf));
  lcdPrint(2, "Alirkan air");
  lcdPrint(3, "Kirim 'k' selesai");

  Serial.printf("\n[KALIB] Target: %.1f L — kirim 'k' jika selesai\n", targetLiter);
}

void finishCalibration() {
  noInterrupts();
  unsigned long endPulse = pulseCount;
  interrupts();

  unsigned long delta = endPulse - calibStartPulse;
  if (delta < 10) {
    Serial.println("[KALIB] Gagal: pulsa terlalu sedikit!");
    calibMode = false;
    return;
  }

  float newPPL = (float)delta / calibTargetL;
  PULSES_PER_LITER = newPPL;
  CALIBRATION_FACTOR = 1.0;

  // Simpan ke NVS
  prefs.begin("wqm", false);
  prefs.putFloat("ppl", newPPL);
  prefs.putFloat("cal", 1.0);
  prefs.end();

  Serial.printf("[KALIB SELESAI] P/L=%.2f, Delta=%lu pulsa\n", newPPL, delta);

  lcd.clear();
  lcdPrint(0, "KALIBRASI SELESAI!");
  char buf[21];
  sprintf(buf, "P/L: %.2f", newPPL);
  lcdPrint(1, String(buf));
  sprintf(buf, "Pulsa: %lu", delta);
  lcdPrint(2, String(buf));
  lcdPrint(3, "Kirim 'r' utk reset");

  beep(3);
  calibMode = false;
  delay(3000);
  lcd.clear();
}

// ============================================================
//   SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);

  // I2C & LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  // DS18B20
  sensors.begin();

  // Load kalibrasi dari NVS (jika ada)
  prefs.begin("wqm", true);
  float savedPPL = prefs.getFloat("ppl", 0.0);
  float savedCal = prefs.getFloat("cal", 0.0);
  prefs.end();
  if (savedPPL > 0) {
    PULSES_PER_LITER   = savedPPL;
    CALIBRATION_FACTOR = (savedCal > 0) ? savedCal : 1.0;
    Serial.printf("[NVS] Loaded P/L=%.2f, Factor=%.3f\n", PULSES_PER_LITER, CALIBRATION_FACTOR);
  }

  // Init buffers
  for (int i = 0; i < AVG_SAMPLES; i++) {
    phADCBuf[i] = 2048;
    tdsBuf[i]   = 0.5;
    turbBuf[i]  = 3800;
  }

  // Splash
  lcd.clear();
  lcdPrint(0, "====================");
  lcdPrint(1, " Water Quality v3.0");
  lcdPrint(2, " Initializing...");
  lcdPrint(3, "====================");
  beep(2);
  delay(1500);

  // WiFi (dengan fallback offline)
  initWiFi();

  // Serial info
  Serial.println("\n=== WATER QUALITY MONITOR v3.0 ===");
  Serial.printf("P/L: %.2f | Factor: %.3f | Step: %.0f mL\n",
    PULSES_PER_LITER, CALIBRATION_FACTOR, STEP_ML);
  Serial.println("--- PERINTAH SERIAL ---");
  Serial.println("r    = Reset volume");
  Serial.println("c1   = Kalibrasi 1 liter");
  Serial.println("c2   = Kalibrasi 2 liter");
  Serial.println("k    = Selesai kalibrasi");
  Serial.println("wifi = Reset WiFi & masuk portal");
  Serial.println("-----------------------");

  lcd.clear();
  lastPageSwitch = millis();
}

// ============================================================
//   LOOP
// ============================================================
void loop() {
  unsigned long now = millis();

  // MQTT loop
  if (mqttConnected) mqttClient.loop();

  // Update flow (setiap loop)
  updateFlow();

  // Baca semua sensor tiap 1 detik
  if (!calibMode && now - lastSensor >= SENSOR_INTERVAL) {
    lastSensor = now;
    readTemperature();
    readPH();
    readTDS();
    readTurbidity();
    waterQuality = assessQuality();
  }

  // Update LCD tiap 1.2 detik
  if (!calibMode && now - lastLCD >= LCD_INTERVAL) {
    lastLCD = now;
    updateLCD();
  }

  // Serial tiap 3 detik
  if (!calibMode && now - lastSerial >= SERIAL_INTERVAL) {
    lastSerial = now;
    printSerial();
  }

  // MQTT publish tiap 5 detik
  if (wifiConnected && now - lastMQTT >= MQTT_INTERVAL) {
    lastMQTT = now;
    if (!mqttClient.connected()) {
      mqttConnected = false;
      mqttReconnect();
    }
    publishMQTT();
  }

  // Cek WiFi tiap 30 detik
  if (now - lastWifiChk >= WIFI_CHECK) {
    lastWifiChk = now;
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (!wifiConnected) {
      mqttConnected = false;
      Serial.println("[WiFi] Disconnected — mode offline");
    }
  }

  // Serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "r") {
      noInterrupts(); pulseCount = 0; interrupts();
      totalVolumeL = displayVolumeL = 0.0;
      Serial.println(">>> VOLUME RESET <<<");

    } else if (cmd == "c1") {
      startCalibration(1.0);

    } else if (cmd == "c2") {
      startCalibration(2.0);

    } else if (cmd == "k" && calibMode) {
      finishCalibration();

    } else if (cmd == "wifi") {
      Serial.println("[WiFi] Reset & masuk portal...");
      WiFiManager wm;
      wm.resetSettings();
      ESP.restart();
    }
  }

  delayMicroseconds(150);
}
