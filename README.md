---
# 📘 **ESP32 Bluetooth-to-MQTT Gateway**

Menghubungkan ESP32 (master) ke ESP32/alat lain (slave) melalui **Bluetooth Classic**, lalu mengirimkan data hasil parsing ke **MQTT Broker**.
Project ini menggunakan **FreeRTOS Multitasking**, sehingga proses Bluetooth dan MQTT berjalan paralel tanpa saling mengganggu.
---

## 🚀 **Fitur Utama**

- 🔵 **Bluetooth Serial Master**
  ESP32 terhubung otomatis ke alamat MAC perangkat slave.

- ☁️ **MQTT Client**
  Mengirim data suhu dan kelembapan ke broker MQTT dengan QoS dan retained message.

- 🔄 **FreeRTOS Multitasking**

  - Task 1 → Menerima data dari Bluetooth
  - Task 2 → Publish data ke MQTT
    Masing-masing task berjalan di core CPU berbeda.

- 📡 **Auto Reconnect MQTT & WiFi**

- 🔍 **Parsing Data Otomatis**
  Format data dari slave:

  ```
  Suhu: xx °C
  Kelembapan: yy %
  ```

- 🧵 **Critical Section / Mutex**
  Menghindari konflik saat dua task mengakses variabel yang sama.

---

## 📂 **Struktur Program**

- `setup()`

  - Koneksi WiFi
  - Inisialisasi MQTT
  - Koneksi Bluetooth Master → Slave
  - Membuat FreeRTOS task

- `loop()`

  - Menjaga koneksi MQTT tetap hidup

- `btTask()`

  - Membaca data Bluetooth ke variabel global

- `pubTask()`

  - Parsing → Publish ke MQTT

- `parseData()`

  - Menyaring nilai suhu & kelembapan

---

## ⚙️ **Kebutuhan Hardware**

- ESP32 DevKit (dua unit jika butuh perangkat slave)
- Bluetooth Classic support
- Jaringan WiFi
- MQTT broker (contoh: Mosquitto, EMQX, HiveMQ)

---

## 🛠️ **Library yang Digunakan**

| Library             | Fungsi                              |
| ------------------- | ----------------------------------- |
| `WiFi.h`            | Menghubungkan ESP32 ke WiFi         |
| `BluetoothSerial.h` | Komunikasi Bluetooth Classic Serial |
| `MQTT.h`            | MQTT Client                         |
| `FreeRTOS`          | Multitasking dalam ESP32            |
| `Arduino.h`         | Library dasar Arduino               |

---

## 📡 **Topik MQTT**

Program ini mengirim data ke:

| Topic                           | Keterangan                    |
| ------------------------------- | ----------------------------- |
| `ESP32TEST_Elitech/status`      | online / offline              |
| `ESP32TEST_Elitech/temperature` | Data suhu hasil parsing       |
| `ESP32TEST_Elitech/humidity`    | Data kelembapan hasil parsing |
| `ESP32TEST_Elitech/#`           | Semua topic untuk subscribe   |

Contoh payload:

```json
{
  "temperature": "27.5",
  "humidity": "60"
}
```

---

## 🔧 **Cara Kerja Sistem**

### 1. ESP32 Master

- Connect ke WiFi
- Connect ke MQTT Broker
- Connect ke Bluetooth Slave
- Membuat dua FreeRTOS task:

### 2. Task 1 — Bluetooth Receiver

- Menerima string data dari slave
- Menyimpan ke `sharedData` menggunakan mutex

### 3. Task 2 — MQTT Publisher

- Membaca `sharedData`
- Mem-parsing bagian suhu & kelembapan
- Publish ke MQTT tiap 5 detik

---

## 🧩 **Contoh Input Data dari Slave**

```
Suhu: 28.3 °C
Kelembapan: 55 %
```

Output ke broker:

```
ESP32TEST_Elitech/temperature → 28.3
ESP32TEST_Elitech/humidity → 55
```

---

## 📈 **Flowchart Data**

```
[Bluetooth Slave]
        ↓
   (Bluetooth)
        ↓
[ESP32 Task btTask]
        ↓
 sharedData (mutex)
        ↓
[ESP32 Task pubTask]
        ↓
   Parsing Data
        ↓
     MQTT Publish
        ↓
   [MQTT Broker]
```

---

## 💡 **Catatan Tambahan**

- Gunakan range stack task minimal 8000–10000 jika data string cukup besar.
- Ubah MAC address Bluetooth slave sesuai perangkatmu:

  ```cpp
  uint8_t address[6] = {0xD0, 0xEF, 0x76, 0x15, 0x58, 0xB6};
  ```

- Jika ingin menambah sensor lain, cukup modifikasi bagian parsing.

---

## 📜 **Lisensi**

Project ini bebas digunakan untuk pembelajaran, riset, dan pengembangan IoT profesional.

---
