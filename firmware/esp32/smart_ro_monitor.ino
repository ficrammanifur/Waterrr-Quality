/*
 * ============================================================
 * SMART WATER QUALITY MONITORING SYSTEM
 * For Reverse Osmosis Depot
 * ESP32 - WITHOUT PUMP CONTROL
 * LCD: Minimalist Display
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
#define PH_PIN 32
#define TDS_PIN 33
#define TURBIDITY_PIN 35
#define DS18B20_PIN 18
#define FLOW_PIN 19
#define LCD_SDA 21
#define LCD_SCL 22
#define LCD_ADDR 0x27
#define BUZZER_PIN 2

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
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "esp32-ro-monitor-001"
#define MQTT_TOPIC_ALL "watermon/all"

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
int tdsADCBuf[AVG_SAMPLES];
int tdsIdx = 0;
int turbBuf[AVG_SAMPLES];
int turbIdx = 0;

// ==================== pH CALIBRATION ====================
const float V4 = 1.350; const float PH4 = 4.00;
const float V7 = 0.874; const float PH7 = 6.86;
const float V9 = 0.485; const float PH9 = 9.18;

// ==================== TURBIDITY CALIBRATION ====================
// HASIL KALIBRASI DARI TESTING ANDA
const int ADC_AIR = 1946;     // Nilai di air jernih
const int ADC_UDARA = 1705;   // Nilai di udara

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

// ==================== TDS CONSTANTS ====================
#define VREF 3.3
#define ADC_RESOLUTION 4095.0

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
void checkWaterQuality();
void initFlowSensor();
void IRAM_ATTR flowISR();
void resetVolume();
void startFlowCalibration(float targetLiter);
void finishFlowCalibration();
float calculatePH(float voltage);
float calculateTDS_DFRobot(float voltage, float temp);
float adcToNTU(int adc);
void updateFilterHealth();
void updateDailyVolume();
void printStatus();
void beep(int times);
void debugTurbidity();
void calibrateTurbidity();
void resetFilterHealth();

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\n========================================");
    Serial.println(" SMART RO WATER QUALITY MONITOR");
    Serial.println(" System Initializing...");
    Serial.println("========================================\n");
    
    analogReadResolution(12);
    
    pinMode(BUZZER_PIN, OUTPUT);
    beep(2);
    
    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" SMART RO MONITOR");
    lcd.setCursor(0, 1);
    lcd.print(" Initializing...");
    lcd.setCursor(0, 2);
    lcd.print(" v3.0");
    lcd.setCursor(0, 3);
    lcd.print(" Please Wait");
    delay(2000);
    
    ds18b20.begin();
    initFlowSensor();
    
    for (int i = 0; i < AVG_SAMPLES; i++) {
        phADCBuf[i] = 2048;
        tdsADCBuf[i] = 0;
        turbBuf[i] = 3800;
    }
    
    Serial.println("[OK] Sensors initialized");
    Serial.println("\n[TDS] Using DFRobot formula");
    Serial.println("[TURBIDITY] Calibration loaded:");
    Serial.printf("  ADC_AIR   = %d\n", ADC_AIR);
    Serial.printf("  ADC_UDARA = %d\n", ADC_UDARA);
    Serial.println("[TURBIDITY] Note: Low ADC values detected, sensor may need cleaning\n");
    
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
    Serial.println(" status - Show all sensor data");
    Serial.println(" r - Reset volume");
    Serial.println(" c1/c2 - Flow calibration");
    Serial.println(" k - Finish flow calibration");
    Serial.println(" reset - Reset filter health");
    Serial.println(" test - Test buzzer");
    Serial.println(" turb - Debug turbidity sensor");
    Serial.println(" cal_turb - Calibrate turbidity sensor");
    Serial.println("========================================\n");
    
    lcd.clear();
    beep(3);
}

// ============================================================
// LOOP
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
        }
    }
    
    delay(10);
}

// ============================================================
// SENSOR FUNCTIONS
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
    float voltage = avgADC * (VREF / ADC_RESOLUTION);
    float phRaw = calculatePH(voltage);
    
    phEMA = (0.85 * phEMA) + (0.15 * phRaw);
    phFiltered = constrain(phEMA, 0.0, 14.0);
    phValue = phFiltered;
}

float calculateTDS_DFRobot(float voltage, float temp) {
    float tempCoeff = 1.0 + 0.02 * (temp - 25.0);
    float compVoltage = voltage / tempCoeff;
    
    float tdsValue = (133.42 * compVoltage * compVoltage * compVoltage
                    - 255.86 * compVoltage * compVoltage
                    + 857.39 * compVoltage) * 0.5;
    
    if (tdsValue < 0) tdsValue = 0;
    if (tdsValue > 9999) tdsValue = 9999;
    
    return tdsValue;
}

void readTDS() {
    int adcValue = analogRead(TDS_PIN);
    
    tdsADCBuf[tdsIdx] = adcValue;
    tdsIdx = (tdsIdx + 1) % AVG_SAMPLES;
    
    long sum = 0;
    for (int i = 0; i < AVG_SAMPLES; i++) {
        sum += tdsADCBuf[i];
    }
    int avgADC = sum / AVG_SAMPLES;
    
    float voltage = avgADC * (VREF / ADC_RESOLUTION);
    tdsValue = calculateTDS_DFRobot(voltage, temperatureC);
}

void readTemperature() {
    ds18b20.requestTemperatures();
    float t = ds18b20.getTempCByIndex(0);
    if (t > -50 && t < 125) {
        temperatureC = t;
    }
}

// ==================== TURBIDITY FUNCTIONS ====================
float adcToNTU(int adc) {
    // Menggunakan hasil kalibrasi
    float persenKekeruhan;
    
    if (adc >= ADC_AIR) {
        persenKekeruhan = 0;  // Sangat jernih
    }
    else if (adc <= ADC_UDARA) {
        persenKekeruhan = 100; // Sangat keruh
    }
    else {
        // Mapping linear dari ADC ke persentase kekeruhan
        persenKekeruhan = 100.0 * (ADC_AIR - adc) / (ADC_AIR - ADC_UDARA);
    }
    
    return constrain(persenKekeruhan, 0, 100);
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
    turbidityPercent = 100.0 - turbidityNTU; // Persentase kejernihan
    
    // Status berdasarkan persentase kekeruhan
    if (turbidityNTU <= 10) turbStatus = "SANGAT JERNIH";
    else if (turbidityNTU <= 25) turbStatus = "JERNIH";
    else if (turbidityNTU <= 50) turbStatus = "CUKUP JERNIH";
    else if (turbidityNTU <= 75) turbStatus = "AGAK KERUH";
    else turbStatus = "KERUH";
}

void readSensors() {
    readTemperature();
    readPH();
    readTDS();
    readTurbidity();
    updateFlow();
}

// ============================================================
// FLOW SENSOR FUNCTIONS
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
// FILTER HEALTH FUNCTIONS
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
    if (turbidityNTU <= 10) turbidityScore = 100.0;
    else if (turbidityNTU <= 25) turbidityScore = 80.0;
    else if (turbidityNTU <= 50) turbidityScore = 60.0;
    else if (turbidityNTU <= 75) turbidityScore = 40.0;
    else turbidityScore = 20.0;
    
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
// WATER QUALITY CHECK
// ============================================================
void checkWaterQuality() {
    bool layak = isWaterLayak(phValue, tdsValue, turbidityNTU, temperatureC);
    waterStatus = layak ? "LAYAK" : "TIDAK LAYAK";
}

// ============================================================
// LCD FUNCTIONS
// ============================================================
void updateLCD() {
    lcd.clear();
    
    lcd.setCursor(0, 0);
    lcd.print("SMART RO MONITOR");
    if (wifiConnected) {
        lcd.setCursor(19, 0);
        lcd.print("W");
    }
    
    lcd.setCursor(0, 1);
    char buf[21];
    sprintf(buf, "pH %.2f TDS %.0f", phValue, tdsValue);
    lcd.print(buf);
    
    lcd.setCursor(0, 2);
    sprintf(buf, "%.1fNTU %.1fC", turbidityNTU, temperatureC);
    lcd.print(buf);
    
    lcd.setCursor(0, 3);
    bool layak = isWaterLayak(phValue, tdsValue, turbidityNTU, temperatureC);
    if (layak) {
        lcd.print("STATUS: LAYAK  ");
    } else {
        lcd.print("STATUS:TIDAK LAYAK");
    }
}

// ============================================================
// WiFi & MQTT FUNCTIONS
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
// UTILITY FUNCTIONS
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
    Serial.println("║ SYSTEM STATUS                        ║");
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf("║ pH          : %6.2f                ║\n", phValue);
    Serial.printf("║ TDS         : %6.0f ppm           ║\n", tdsValue);
    Serial.printf("║ Temperature : %6.2f °C            ║\n", temperatureC);
    Serial.printf("║ Turbidity   : %6.2f NTU (%s) ║\n", turbidityNTU, turbStatus.c_str());
    Serial.printf("║ Kejernihan  : %6.1f %%             ║\n", turbidityPercent);
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf("║ Volume      : %8.3f L            ║\n", displayVolumeL);
    Serial.printf("║ Flow Rate   : %8.2f L/min        ║\n", flowRateLPM);
    Serial.printf("║ Pulses      : %8lu               ║\n", pulseCount);
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf("║ Status      : %s            ║\n", waterStatus.c_str());
    Serial.printf("║ Filter      : %6.0f %%             ║\n", filterHealth);
    Serial.printf("║ Days Left   : %6d                ║\n", daysLeft);
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf("║ WiFi        : %s                ║\n", wifiConnected ? "Connected" : "Offline");
    Serial.printf("║ MQTT        : %s                ║\n", mqttConnected ? "Connected" : "Disconnected");
    Serial.println("╚═══════════════════════════════════════╝\n");
    
    int lastADC = tdsADCBuf[(tdsIdx - 1 + AVG_SAMPLES) % AVG_SAMPLES];
    float lastVoltage = lastADC * (VREF / ADC_RESOLUTION);
    Serial.printf("[TDS DEBUG] ADC: %d, Voltage: %.4f V\n", lastADC, lastVoltage);
    Serial.printf("[TURBIDITY] ADC: %d, AIR: %d, UDARA: %d\n", 
                  turbidityADC, ADC_AIR, ADC_UDARA);
    
    // Peringatan jika sensor turbidity bermasalah
    if (turbidityADC < 1000) {
        Serial.println("\n⚠️ PERINGATAN: Sensor Turbidity bermasalah!");
        Serial.println("   - ADC terlalu rendah (< 1000)");
        Serial.println("   - Periksa koneksi sensor");
        Serial.println("   - Bersihkan lensa sensor");
    }
}

// ============================================================
// TURBIDITY DEBUG & CALIBRATION
// ============================================================
void debugTurbidity() {
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║ TURBIDITY DEBUG                     ║");
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf("║ ADC Raw     : %6d                 ║\n", turbidityADC);
    Serial.printf("║ NTU         : %8.2f               ║\n", turbidityNTU);
    Serial.printf("║ Kejernihan  : %8.1f %%            ║\n", turbidityPercent);
    Serial.printf("║ Status      : %s                ║\n", turbStatus.c_str());
    Serial.printf("║ ADC_AIR     : %6d                 ║\n", ADC_AIR);
    Serial.printf("║ ADC_UDARA   : %6d                 ║\n", ADC_UDARA);
    Serial.println("╚═══════════════════════════════════════╝\n");
    
    // Analisis
    if (turbidityADC >= ADC_AIR) {
        Serial.println("✅ ADC >= AIR → Air SANGAT JERNIH (0% keruh)");
    } else if (turbidityADC >= (ADC_AIR + ADC_UDARA) / 2) {
        Serial.println("✅ ADC di atas titik tengah → Air JERNIH");
    } else if (turbidityADC > ADC_UDARA) {
        Serial.println("⚠️ ADC di bawah titik tengah → Air AGAK KERUH");
    } else {
        Serial.println("❌ ADC <= UDARA → Air SANGAT KERUH");
    }
    
    Serial.println("\n⚠️ CATATAN:");
    Serial.printf("   ADC AIR = %d (seharusnya > 3000)\n", ADC_AIR);
    Serial.printf("   ADC UDARA = %d (seharusnya > 2000)\n", ADC_UDARA);
    Serial.println("   Nilai ADC rendah menunjukkan sensor kotor atau rusak!");
    Serial.println("\nSOLUSI:");
    Serial.println("1. Bersihkan lensa sensor dengan kain lembut dan alkohol");
    Serial.println("2. Periksa koneksi kabel sensor");
    Serial.println("3. Jika masih rendah, ganti sensor");
    Serial.println("");
}

void calibrateTurbidity() {
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║ TURBIDITY CALIBRATION               ║");
    Serial.println("╚═══════════════════════════════════════╝\n");
    
    Serial.println("⚠️ PERINGATAN:");
    Serial.println("Nilai ADC Anda sangat rendah:");
    Serial.printf("   ADC AIR = %d (seharusnya > 3000)\n", ADC_AIR);
    Serial.printf("   ADC UDARA = %d (seharusnya > 2000)\n", ADC_UDARA);
    Serial.println("\nIni menandakan sensor KOTOR atau RUSAK!");
    Serial.println("Kalibrasi tetap bisa dilakukan tapi hasilnya tidak akurat.\n");
    
    Serial.println("Step 1: Celupkan sensor ke AIR JERNIH...");
    Serial.println("Tekan ENTER jika sudah siap...");
    
    while (!Serial.available()) {
        delay(100);
    }
    Serial.read();
    
    long sumClear = 0;
    Serial.print("Membaca ADC di air jernih");
    for (int i = 0; i < 50; i++) {
        sumClear += analogRead(TURBIDITY_PIN);
        delay(50);
        if (i % 10 == 0) Serial.print(".");
    }
    int newAir = sumClear / 50;
    Serial.println(" SELESAI!");
    Serial.printf("ADC di AIR JERNIH: %d\n\n", newAir);
    
    Serial.println("Step 2: Angkat sensor ke UDARA...");
    Serial.println("Tekan ENTER jika sudah siap...");
    
    while (!Serial.available()) {
        delay(100);
    }
    Serial.read();
    
    long sumUdara = 0;
    Serial.print("Membaca ADC di udara");
    for (int i = 0; i < 50; i++) {
        sumUdara += analogRead(TURBIDITY_PIN);
        delay(50);
        if (i % 10 == 0) Serial.print(".");
    }
    int newUdara = sumUdara / 50;
    Serial.println(" SELESAI!");
    Serial.printf("ADC di UDARA: %d\n\n", newUdara);
    
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║ HASIL KALIBRASI                     ║");
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.printf("║ ADC_AIR     : %6d                 ║\n", newAir);
    Serial.printf("║ ADC_UDARA   : %6d                 ║\n", newUdara);
    Serial.println("╚═══════════════════════════════════════╝\n");
    
    Serial.println("Copy nilai berikut ke program utama:");
    Serial.println();
    Serial.printf("const int ADC_AIR = %d;\n", newAir);
    Serial.printf("const int ADC_UDARA = %d;\n", newUdara);
    Serial.println();
}
