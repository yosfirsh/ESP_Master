#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <MQTT.h>

const char *ssid = "Wi-Fi Home";   // Ganti dengan nama SSID Wi-Fi Anda
const char *password = "12348765"; // Ganti dengan password Wi-Fi Anda

BluetoothSerial SerialBT; // Objek untuk komunikasi Bluetooth Serial
WiFiClient net;
MQTTClient client;

// Task handle
TaskHandle_t btTaskHandle = NULL;      // Task handle untuk Bluetooth
TaskHandle_t publishTaskHandle = NULL; // Task handle untuk publish

// Deklarasi fungsi
void btTask(void *parameter);
void pubTask(void *parameter);

// Variabel global untuk menyimpan data
String sharedData = "";                          // Data yang akan diakses oleh kedua task
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Critical section

void connect()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  client.setWill("ESP32TEST_Elitech/status", "offline", true, 1);
  while (!client.connect("12345678", "yosfirsh", "yosfirsh"))
  {
    delay(500);
  }
  client.publish("ESP32TEST_Elitech/status", "online", true, 1);
  client.subscribe("ESP32TEST_Elitech/#", 1); // Subscribe menggunakan QoS 1
}

void setup()
{
  // Memulai komunikasi serial untuk monitor
  Serial.begin(115200);

  // Menghubungkan ke Wi-Fi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Menghubungkan ke MQTT Broker");
  client.begin("broker.yosfirsh.my.id", net);
  client.setOptions(100, true, 1000);
  Serial.println("Terhubung ke broker");

  connect();

  // Menghubungkan Bluetooth
  SerialBT.begin("ESP32_Master", true); // Aktifkan master mode

  uint8_t address[6] = {0xD0, 0xEF, 0x76, 0x15, 0x58, 0xB6}; // Alamat MAC slave

  Serial.println("Trying to connect to Slave...");
  // Ganti alamat MAC dengan milik Slave
  if (SerialBT.connect(address))
  {
    Serial.println("Successfully connected to Slave!");
  }
  else
  {
    Serial.println("Failed to connect to Slave.");
  }

  // Membuat task untuk menerima data Bluetooth
  xTaskCreatePinnedToCore(
      btTask,           // Nama fungsi task
      "Bluetooth Task", // Nama task
      10000,            // Ukuran stack task (pilih sesuai kebutuhan)
      NULL,             // Parameter untuk task (tidak ada)
      1,                // Prioritas task (semakin tinggi, semakin sering dijalankan)
      &btTaskHandle,    // Task handle untuk referensi
      0                 // Core tempat task akan dijalankan (0 atau 1)
  );

  // Membuat task untuk publish data
  xTaskCreatePinnedToCore(
      pubTask,            // Nama fungsi task
      "Publish Task",     // Nama task
      10000,              // Ukuran stack task
      NULL,               // Parameter untuk task (tidak ada)
      1,                  // Prioritas task
      &publishTaskHandle, // Task handle untuk referensi
      1                   // Core tempat task akan dijalankan (0 atau 1)
  );
}

void loop()
{
  if (!client.connected())
  {
    connect();
  }
  delay(100);
}

void parseData(const String &input, String &temperature, String &humidity)
{
  // Cari posisi awal dan akhir substring
  int suhuStart = input.indexOf("Suhu: ") + 6;
  int suhuEnd = input.indexOf(" °C");
  int kelembapanStart = input.indexOf("Kelembapan: ") + 12;
  int kelembapanEnd = input.indexOf(" %");

  // Ekstrak suhu dan kelembapan
  temperature = input.substring(suhuStart, suhuEnd);
  humidity = input.substring(kelembapanStart, kelembapanEnd);
}

// Fungsi task untuk menerima data dari Bluetooth
void btTask(void *parameter)
{
  while (true)
  {
    // Periksa apakah ada data yang diterima dari slave
    if (SerialBT.available()) // Jika ada data yang masuk
    {
      String receivedData = SerialBT.readString(); // Membaca data dari slave
      Serial.print("Data received from Slave: ");
      Serial.println(receivedData); // Tampilkan data yang diterima

      // Menulis data ke variabel global dalam critical section
      portENTER_CRITICAL(&mux);
      sharedData = receivedData;
      portEXIT_CRITICAL(&mux);
    }

    // Delay untuk memberi waktu sistem untuk menjalankan task lainnya
    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay 100ms
  }
}

void pubTask(void *parameter)
{
  while (true)
  {
    // Membaca data dari variabel global dalam critical section
    portENTER_CRITICAL(&mux);
    String dataToPublish = sharedData;
    portEXIT_CRITICAL(&mux);

    if (dataToPublish != "")
    {
      // Deklarasikan variabel untuk suhu dan kelembapan
      String temperature, humidity;

      // Panggil fungsi parsing
      parseData(dataToPublish, temperature, humidity);

      client.publish("ESP32TEST_Elitech/temperature", temperature, true, 1);
      client.publish("ESP32TEST_Elitech/humidity", humidity, true, 1);
      Serial.println("Kirim data ke broker");
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
