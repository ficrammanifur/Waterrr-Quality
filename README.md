<h1 align="center">💧 SMART RO WATER QUALITY MONITOR</h1>


<p align="center">
  <img src="Smart+RO+Water+Quality+Monitor" alt="Smart RO Monitor Preview" width="700"/>
</p>

<p align="center">
  <em>Sistem Monitoring Kualitas Air RO Real-time berbasis ESP32 + MQTT</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/last%20update-today-brightgreen" />
  <img src="https://img.shields.io/badge/language-HTML%20%7C%20CSS%20%7C%20JavaScript-blue" />
  <img src="https://img.shields.io/badge/hardware-ESP32-informational" />
  <img src="https://img.shields.io/badge/protocol-MQTT-green" />
  <img src="https://img.shields.io/badge/platform-GitHub%20Pages-orange" />
  <img src="https://img.shields.io/badge/status-Active-success" />
</p>

---

## 📑 Table of Contents
- [✨ Overview](#-overview)
- [🔧 Features](#-features)
- [🏗️ System Architecture](#️-system-architecture)
- [📁 Project Structure](#-project-structure)
- [⚙️ Installation](#️-installation)
- [🚀 Usage](#-usage)
- [🧪 Testing](#-testing)
- [📦 Dependencies](#-dependencies)
- [🔧 Configuration](#-configuration)
- [🐞 Troubleshooting](#-troubleshooting)
- [📄 License](#-license)

---

## ✨ Overview

**Smart RO Water Quality Monitor** adalah sistem monitoring kualitas air Reverse Osmosis secara real-time menggunakan ESP32. Sistem ini memantau parameter penting seperti pH, TDS, Kekeruhan, Suhu, dan Volume produksi, kemudian menampilkan status "LAYAK" atau "TIDAK LAYAK" melalui dashboard web yang dapat diakses dari mana saja.

### 🎯 Fitur Utama
- Monitoring 5 parameter sensor secara simultan
- Penilaian otomatis kualitas air (Rule-based)
- Dashboard real-time dengan grafik
- Kesehatan filter otomatis
- Komunikasi MQTT dua arah

---

## 🔧 Features

- ✅ **Real-time Dashboard** – Monitoring langsung via web
- ✅ **Multi Sensor Integration** – pH, TDS, Turbidity, Temperature, Flow Sensor
- ✅ **Water Quality Assessment** – Status LAYAK / TIDAK LAYAK otomatis
- ✅ **Filter Health Monitoring** – Estimasi umur filter
- ✅ **Flow Rate & Volume Tracking** – Dengan kalibrasi
- ✅ **MQTT Communication** – Menggunakan HiveMQ Public Broker
- ✅ **Responsive Design** – Mobile & Desktop friendly
- ✅ **Auto Reconnect** – MQTT stabil
- ✅ **GitHub Pages Deployment** – Hosting gratis

---

## 🏗️ System Architecture

```text
┌─────────────────┐       MQTT              ┌─────────────────┐
│  Web Dashboard  │ ◄─────────────────────► │   MQTT Broker   │
│ (GitHub Pages)  │ wss://broker.hivemq.com │     (HiveMQ)    │
└─────────────────┘                         └─────────────────┘
                                                   │
                                                   │ MQTT
                                                   ▼
                                           ┌─────────────────┐
                                           │      ESP32      │
                                           │ (Sensors + LCD) │
                                           └─────────────────┘
```

---

## 📁 Project Structure

```text
smart-ro-monitor/
├── 📄 index.html                 # Main Dashboard
├── 📜 script.js                  # MQTT + Logic
├── 🎨 style.css                  # Styling & Responsive
├── 📄 README.md                  # Dokumentasi
├── 📁 esp32/
│   ├── smart_ro_monitor.ino       # Kode Arduino ESP32
│   └── water_rules.h              # Aturan kualitas air
└── 📁 assets/                    # (opsional) Gambar & screenshot
```

---

## ⚙️ Installation

### 1. Clone Repository
```bash
git clone https://github.com/username/smart-ro-monitor.git
cd smart-ro-monitor
```

### 2. Deploy ke GitHub Pages
1. Upload semua file ke repository GitHub
2. Buka **Settings** → **Pages**
3. Pilih **Source**: `Deploy from a branch` → `main` → `/ (root)`
4. Simpan
5. Tunggu beberapa menit, lalu akses di:
   `https://username.github.io/smart-ro-monitor`

### 3. Upload ke ESP32
1. Buka `esp32/smart_ro_monitor.ino` di Arduino IDE
2. Install library:
   - `WiFiManager`
   - `PubSubClient`
   - `LiquidCrystal_I2C`
   - `DallasTemperature`
   - `OneWire`
3. Upload sketch ke ESP32
4. Hubungkan ke WiFi melalui hotspot `WaterMonitor`

---

## 🚀 Usage

1. **Nyalakan ESP32**
2. **Buka Dashboard** di browser
3. **Tunggu koneksi MQTT** (biasanya < 10 detik)
4. Monitoring real-time akan muncul otomatis

**Parameter yang dimonitor:**
- pH, TDS, Kekeruhan (%), Suhu
- Debit air (L/min)
- Total volume produksi
- Kesehatan filter & estimasi hari tersisa

---

## 🧪 Testing

- Buka Console Browser (`F12`) untuk melihat status MQTT
- Kirim perintah via Serial Monitor ESP32:
  - `status` → tampilkan semua data
  - `r` → reset volume
  - `test` → test buzzer

---

## 📦 Dependencies

### Frontend
- [Paho MQTT JS](https://cdn.jsdelivr.net/npm/paho-mqtt)
- [Chart.js](https://cdn.jsdelivr.net/npm/chart.js)

### ESP32
- WiFiManager
- PubSubClient
- LiquidCrystal_I2C
- OneWire + DallasTemperature

---

## 🔧 Configuration

**MQTT Topics:**
- Publish: `watermon/all`

**Broker:**
- `broker.hivemq.com`
- Port WebSocket: `8000` (WSS)

---

## 🐞 Troubleshooting

**Dashboard tidak connect?**
- Pastikan ESP32 sudah online dan publish data
- Cek koneksi WiFi ESP32
- Refresh halaman / bersihkan cache

**Data tidak muncul?**
- Buka Console Browser (`F12`)
- Periksa apakah ada error MQTT

**ESP32 tidak connect ke WiFi?**
- Reset WiFiManager dengan tombol (jika sudah diatur)

---

## 📄 License

Proyek ini open source di bawah lisensi **MIT**.

---

<div align="center">
  <strong>💧 Smart RO Water Quality Monitoring System</strong><br>
  Built with ESP32 • MQTT • GitHub Pages
  <p><a href="#top">⬆ Kembali ke Atas</a></p>
</div>
