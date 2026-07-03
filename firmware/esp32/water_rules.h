/*
 * ============================================================
 *   WATER QUALITY RULES - PROFESSIONAL VERSION
 *   Menggunakan NTU untuk Turbidity
 *   Berdasarkan 60 sampel (30 LAYAK + 30 TIDAK LAYAK)
 * ============================================================
 */

#ifndef WATER_RULES_H
#define WATER_RULES_H

#include <Arduino.h>

// ==================== WATER STANDARDS ====================
struct WaterStandards {
  // ===== pH Range =====
  // Berdasarkan dataset: 6.63 - 9.74 (LAYAK)
  float phMin = 6.5;
  float phMax = 9.8;
  
  // ===== TDS Range =====
  // Standar WHO: < 500 ppm
  // Ideal RO: < 50 ppm
  float tdsMax = 500.0;
  float tdsIdealMax = 50.0;
  
  // ===== Turbidity (NTU) =====
  // Standar WHO: < 5 NTU
  // Ideal RO: < 1 NTU
  float ntuMax = 5.0;        // Maksimal NTU untuk layak
  float ntuIdealMax = 1.0;   // Ideal untuk RO
  
  // ===== Temperature =====
  float tempMin = 15.0;
  float tempMax = 35.0;
};

// ==================== ENUM STATUS ====================
enum WaterQualityLevel {
  QUALITY_LAYAK,
  QUALITY_CUKUP,
  QUALITY_TIDAK_LAYAK
};

// ==================== FUNCTION PROTOTYPES ====================
bool isWaterLayak(float ph, float tds, float ntu, float temp);
String getWaterStatus(float ph, float tds, float ntu, float temp);
WaterQualityLevel getWaterQualityLevel(float ph, float tds, float ntu, float temp);
String getQualityDescription(WaterQualityLevel level);

bool isPHNormal(float ph);
bool isTDSNormal(float tds);
bool isTurbidityNormal(float ntu);
bool isTemperatureNormal(float temp);

// ==================== FUNGSI TAMBAHAN ====================
String getDetailedStatus(float ph, float tds, float ntu, float temp);
String getUnlayakReason(float ph, float tds, float ntu, float temp);
String getTurbidityDescription(float ntu);

// ============================================================
//   IMPLEMENTATION
// ============================================================

/**
 * Cek apakah pH normal (6.5 - 9.8)
 */
inline bool isPHNormal(float ph) {
  return (ph >= 6.5 && ph <= 9.8);
}

/**
 * Cek apakah pH sangat baik (7.0 - 8.5)
 */
inline bool isPHExcellent(float ph) {
  return (ph >= 7.0 && ph <= 8.5);
}

/**
 * Cek apakah pH Asam (< 6.5)
 */
inline bool isPHAsam(float ph) {
  return (ph < 6.5);
}

/**
 * Cek apakah pH Basa (> 9.8)
 */
inline bool isPHBasa(float ph) {
  return (ph > 9.8);
}

/**
 * Cek apakah TDS normal (< 500 ppm)
 */
inline bool isTDSNormal(float tds) {
  return (tds <= 500.0);
}

/**
 * Cek apakah TDS sangat baik (< 50 ppm)
 */
inline bool isTDSExcellent(float tds) {
  return (tds <= 50.0);
}

/**
 * Cek apakah TDS baik (50-200 ppm)
 */
inline bool isTDSGood(float tds) {
  return (tds > 50.0 && tds <= 200.0);
}

/**
 * Cek apakah Turbidity normal (< 5 NTU)
 * Standar WHO untuk air minum
 */
inline bool isTurbidityNormal(float ntu) {
  return (ntu <= 5.0);
}

/**
 * Cek apakah Turbidity sangat baik (< 1 NTU)
 * Ideal untuk RO
 */
inline bool isTurbidityExcellent(float ntu) {
  return (ntu <= 1.0);
}

/**
 * Cek apakah suhu normal (15-35°C)
 */
inline bool isTemperatureNormal(float temp) {
  return (temp >= 15.0 && temp <= 35.0);
}

/**
 * Mendapatkan deskripsi kualitas turbidity
 */
inline String getTurbidityDescription(float ntu) {
  if (ntu <= 0.5) return "SANGAT JERNIH";
  if (ntu <= 1.0) return "JERNIH";
  if (ntu <= 2.0) return "CUKUP JERNIH";
  if (ntu <= 5.0) return "AGAK KERUH";
  if (ntu <= 10.0) return "KERUH";
  return "SANGAT KERUH";
}

/**
 * Menentukan apakah air LAYAK
 */
inline bool isWaterLayak(float ph, float tds, float ntu, float temp) {
  return (isPHNormal(ph) && 
          isTDSNormal(tds) && 
          isTurbidityNormal(ntu) && 
          isTemperatureNormal(temp));
}

/**
 * Mendapatkan level kualitas air
 */
inline WaterQualityLevel getWaterQualityLevel(float ph, float tds, float ntu, float temp) {
  bool phOK = isPHNormal(ph);
  bool tdsOK = isTDSNormal(tds);
  bool turbOK = isTurbidityNormal(ntu);
  bool tempOK = isTemperatureNormal(temp);
  
  if (phOK && tdsOK && turbOK && tempOK) {
    if (isPHExcellent(ph) && isTDSExcellent(tds) && isTurbidityExcellent(ntu)) {
      return QUALITY_LAYAK;
    }
    return QUALITY_LAYAK;
  }
  
  int failCount = 0;
  if (!phOK) failCount++;
  if (!tdsOK) failCount++;
  if (!turbOK) failCount++;
  if (!tempOK) failCount++;
  
  if (failCount <= 1) {
    return QUALITY_CUKUP;
  }
  
  return QUALITY_TIDAK_LAYAK;
}

/**
 * Mendapatkan deskripsi kualitas air
 */
inline String getQualityDescription(WaterQualityLevel level) {
  switch(level) {
    case QUALITY_LAYAK:
      return "LAYAK - Air sangat baik ✓";
    case QUALITY_CUKUP:
      return "CUKUP - Perlu perhatian ⚠️";
    case QUALITY_TIDAK_LAYAK:
      return "TIDAK LAYAK - Tidak aman ✗";
    default:
      return "UNKNOWN";
  }
}

/**
 * Mendapatkan status air (LAYAK/TIDAK LAYAK)
 */
inline String getWaterStatus(float ph, float tds, float ntu, float temp) {
  return isWaterLayak(ph, tds, ntu, temp) ? "LAYAK" : "TIDAK LAYAK";
}

/**
 * Mendapatkan status detail
 */
inline String getDetailedStatus(float ph, float tds, float ntu, float temp) {
  String status = getWaterStatus(ph, tds, ntu, temp);
  if (status == "LAYAK") {
    return status + " ✓";
  } else {
    return status + " ✗ (" + getUnlayakReason(ph, tds, ntu, temp) + ")";
  }
}

/**
 * Mendapatkan alasan tidak layak
 */
inline String getUnlayakReason(float ph, float tds, float ntu, float temp) {
  String reasons = "";
  bool first = true;
  String separator;
  
  if (!isPHNormal(ph)) {
    separator = first ? "" : ", ";
    if (isPHAsam(ph)) {
      reasons += separator + "pH Asam (" + String(ph, 2) + ")";
    } else if (isPHBasa(ph)) {
      reasons += separator + "pH Basa (" + String(ph, 2) + ")";
    } else {
      reasons += separator + "pH (" + String(ph, 2) + ")";
    }
    first = false;
  }
  
  if (!isTDSNormal(tds)) {
    separator = first ? "" : ", ";
    reasons += separator + "TDS Tinggi (" + String(tds, 0) + " ppm)";
    first = false;
  }
  
  if (!isTurbidityNormal(ntu)) {
    separator = first ? "" : ", ";
    reasons += separator + "Kekeruhan (" + String(ntu, 2) + " NTU)";
    first = false;
  }
  
  if (!isTemperatureNormal(temp)) {
    separator = first ? "" : ", ";
    if (temp < 15) {
      reasons += separator + "Suhu Dingin (" + String(temp, 1) + "°C)";
    } else {
      reasons += separator + "Suhu Panas (" + String(temp, 1) + "°C)";
    }
    first = false;
  }
  
  if (reasons.isEmpty()) {
    reasons = "Semua parameter normal";
  }
  
  return reasons;
}

#endif // WATER_RULES_H
