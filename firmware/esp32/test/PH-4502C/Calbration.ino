const int phPin = 32;

struct CalibrationPoint {
  float ph;
  float voltage;
};

CalibrationPoint points[3];
int pointCount = 0;

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);

  Serial.println("\n=== AUTO PH CALIBRATION ===");
  Serial.println("Masukkan nilai buffer (contoh: 4.00)");
  Serial.println("Lalu tekan ENTER");
}

void loop() {

  // Jika user memasukkan nilai pH
  if (Serial.available()) {

    float targetPH = Serial.parseFloat();

    if (targetPH > 0) {

      Serial.println("--------------------------------");
      Serial.print("Kalibrasi buffer pH ");
      Serial.println(targetPH, 2);
      Serial.println("Mengambil sampel selama 10 detik...");
      Serial.println("Jangan gerakkan probe.");

      unsigned long startTime = millis();

      long adcSum = 0;
      int sampleCount = 0;

      while (millis() - startTime < 10000) {

        adcSum += analogRead(phPin);
        sampleCount++;

        delay(50);
      }

      float avgADC = adcSum / (float)sampleCount;
      float voltage = avgADC * (3.3 / 4095.0);

      points[pointCount].ph = targetPH;
      points[pointCount].voltage = voltage;

      Serial.print("Selesai -> Voltage = ");
      Serial.print(voltage, 3);
      Serial.println(" V");

      pointCount++;

      if (pointCount >= 2) {

        // gunakan titik pertama dan terakhir
        float slope =
          (points[pointCount - 1].ph - points[0].ph) /
          (points[pointCount - 1].voltage - points[0].voltage);

        float offset =
          points[0].ph - (slope * points[0].voltage);

        Serial.println("\n===== HASIL KALIBRASI =====");
        Serial.print("Slope  = ");
        Serial.println(slope, 4);

        Serial.print("Offset = ");
        Serial.println(offset, 4);

        Serial.println("\nGunakan pada program utama:");
        Serial.print("float slope = ");
        Serial.print(slope, 4);
        Serial.println(";");

        Serial.print("float offset = ");
        Serial.print(offset, 4);
        Serial.println(";");
      }

      if (pointCount == 3) {
        Serial.println("\nKalibrasi 3 titik selesai.");
        Serial.println("Data yang tersimpan:");

        for (int i = 0; i < 3; i++) {
          Serial.print("pH ");
          Serial.print(points[i].ph, 2);
          Serial.print(" -> ");
          Serial.print(points[i].voltage, 3);
          Serial.println(" V");
        }

        pointCount = 0;

        Serial.println("\nMulai kalibrasi baru...");
      }

      Serial.println("\nMasukkan buffer berikutnya:");
    }
  }
}
