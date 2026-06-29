const int flowSensorPin = 18;

volatile int pulseCount = 0;
float calibrationFactor = 4.5; // Faktor kalibrasi sensor (umumnya 4.5 untuk YF-S201, sesuaikan jika perlu)
unsigned long totalMiliLiter = 0;
unsigned long previousMillis = 0;

// Interrupt Service Routine (ISR) untuk menghitung pulsa
void IRAM_ATTR pulseCounterISR() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  // Mengaktifkan internal pull-up pada Pin 18
  pinMode(flowSensorPin, INPUT_PULLUP);
  
  // Memasang interrupt agar pembacaan pulsa air berjalan di background
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounterISR, FALLING); 

  Serial.println("Flow Sensor mulai membaca...");
}

void loop() {
  // Hitung flow rate dan volume setiap 1 detik (1000 ms)
  if ((millis() - previousMillis) > 1000) { 
    
    // Matikan interrupt sementara agar perhitungan matematika tidak terganggu pulsa baru
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));

    // --- LOGIKA PERHITUNGAN ---
    
    // 1. Hitung Debit Air (Liter per menit)
    float flowRate = ((1000.0 / (millis() - previousMillis)) * pulseCount) / calibrationFactor;
    
    // 2. Hitung volume air yang lewat dalam 1 detik tersebut (dalam mL)
    // (L/min dibagi 60 = L/detik. Dikali 1000 = mL/detik)
    unsigned int volumeInterval = (flowRate / 60) * 1000;
    
    // 3. Akumulasikan ke total volume keseluruhan
    totalMiliLiter += volumeInterval;

    // Tampilkan hasil ke Serial Monitor
    Serial.print("Debit: ");
    Serial.print(flowRate);
    Serial.print(" L/min\t|\tTotal Terisi: ");
    Serial.print(totalMiliLiter);
    Serial.println(" mL");

    // Reset hitungan pulse dan catat waktu terakhir untuk perhitungan detik berikutnya
    pulseCount = 0;
    previousMillis = millis();
    
    // Nyalakan kembali interrupt
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounterISR, FALLING);
  }
}
