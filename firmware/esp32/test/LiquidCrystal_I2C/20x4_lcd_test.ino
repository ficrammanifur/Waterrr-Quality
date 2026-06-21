#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD dengan alamat I2C 0x27, 20 kolom, dan 4 baris
LiquidCrystal_I2C lcd(0x27, 20, 4); 

void setup() {
  // ESP32 secara default menggunakan SDA 21 dan SCL 22
  Wire.begin(21, 22);

  // Inisialisasi LCD
  lcd.init();         // Jika error, coba ganti dengan lcd.begin();
  lcd.backlight();    // Menyalakan lampu latar (backlight)

  // Menulis di Baris 1 (Baris indeks 0)
  lcd.setCursor(0, 0); 
  lcd.print("Sistem Inisialisasi");

  // Menulis di Baris 2 (Baris indeks 1)
  lcd.setCursor(0, 1); 
  lcd.print("Tipe LCD : 20x4");

  // Menulis di Baris 3 (Baris indeks 2)
  lcd.setCursor(0, 2); 
  lcd.print("I2C Addr : 0x27");

  // Menulis di Baris 4 (Baris indeks 3)
  lcd.setCursor(0, 3); 
  lcd.print("Status   : OK!");
}

void loop() {
  // Kosong
}
