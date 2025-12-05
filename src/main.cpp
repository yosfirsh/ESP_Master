#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <MQTT.h>

// --------------------------------------------
// Konfigurasi WiFi
// --------------------------------------------
const char *ssid = "Wi-Fi Home";   // Nama WiFi
const char *password = "12348765"; // Password WiFi

// --------------------------------------------
// Objek komunikasi
// --------------------------------------------
BluetoothSerial SerialBT; // Objek untuk komunikasi Bluetooth Classic
WiFiClient net;           // Client untuk koneksi TCP/IP
MQTTClient client;        // Client MQTT

// --------------------------------------------
// Task handle FreeRTOS
// --------------------------------------------
TaskHandle_t btTaskHandle = NULL;      // Task penerima data Bluetooth
TaskHandle_t publishTaskHandle = NULL; // Task pengirim data ke MQTT

// --------------------------------------------
// Variabel global untuk berbagi data antar task
// --------------------------------------------
String sharedData = "";                          // Buffer data yang diterima
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Mutex untuk critical section

// Deklarasi fungsi task
void btTask(void *parameter);
void pubTask(void *parameter);

// --------------------------------------------
// Fungsi untuk menghubungkan ke MQTT broker
// --------------------------------------------
void connect()
{
  // Tunggu sampai WiFi terhubung
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  // Set Last Will (status mati)
  client.setWill("ESP32TEST_Elitech/status", "offline", true, 1);

  // Loop sampai berhasil terhubung ke broker MQTT
  while (!client.connect("12345678", "yosfirsh", "yosfirsh"))
  {
    delay(500);
  }

  // Publish status online
  client.publish("ESP32TEST_Elitech/status", "online", true, 1);

  // Subscribe semua topic di channel
  client.subscribe("ESP32TEST_Elitech/#", 1);
}

// --------------------------------------------
// Setup utama (dipanggil sekali)
// --------------------------------------------
void setup()
{
  Serial.begin(115200); // Serial monitor

  // --------------------------------------------
  // Koneksi ke WiFi
  // --------------------------------------------
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  // Tunggu hingga ESP32 terhubung
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // --------------------------------------------
  // Koneksi ke MQTT broker
  // --------------------------------------------
  Serial.println("Menghubungkan ke MQTT Broker");
  client.begin("broker.yosfirsh.my.id", net); // Host broker
  client.setOptions(100, true, 1000);         // Keep-alive & timeout
  Serial.println("Terhubung ke broker");

  connect(); // Jalankan fungsi koneksi MQTT

  // --------------------------------------------
  // Koneksi Bluetooth (Master Mode)
  // --------------------------------------------
  SerialBT.begin("ESP32_Master", true); // Mode master

  // Alamat MAC slave yang ingin dihubungkan
  uint8_t address[6] = {0xD0, 0xEF, 0x76, 0x15, 0x58, 0xB6};

  Serial.println("Trying to connect to Slave...");

  if (SerialBT.connect(address))
    Serial.println("Successfully connected to Slave!");
  else
    Serial.println("Failed to connect to Slave.");

  // --------------------------------------------
  // Membuat task: Bluetooth Receiver
  // --------------------------------------------
  xTaskCreatePinnedToCore(
      btTask,           // Fungsi task
      "Bluetooth Task", // Nama task
      10000,            // Stack size
      NULL,             // Parameter
      1,                // Prioritas
      &btTaskHandle,    // Handle task
      0                 // Core 0
  );

  // --------------------------------------------
  // Membuat task: Publish ke MQTT
  // --------------------------------------------
  xTaskCreatePinnedToCore(
      pubTask,            // Fungsi task
      "Publish Task",     // Nama task
      10000,              // Stack size
      NULL,               // Parameter
      1,                  // Prioritas
      &publishTaskHandle, // Handle task
      1                   // Core 1
  );
}

void loop()
{
  // Reconnect MQTT jika terputus
  if (!client.connected())
  {
    connect();
  }

  delay(100);
}

// --------------------------------------------
// Fungsi parsing data "Suhu: xx °C, Kelembapan: yy %"
// --------------------------------------------
void parseData(const String &input, String &temperature, String &humidity)
{
  int suhuStart = input.indexOf("Suhu: ") + 6;
  int suhuEnd = input.indexOf(" °C");
  int kelembapanStart = input.indexOf("Kelembapan: ") + 12;
  int kelembapanEnd = input.indexOf(" %");

  temperature = input.substring(suhuStart, suhuEnd);
  humidity = input.substring(kelembapanStart, kelembapanEnd);
}

// --------------------------------------------
// TASK 1: Menerima data dari Bluetooth Slave
// --------------------------------------------
void btTask(void *parameter)
{
  while (true)
  {
    // Jika ada data dari Bluetooth
    if (SerialBT.available())
    {
      String receivedData = SerialBT.readString(); // Ambil data
      Serial.print("Data received from Slave: ");
      Serial.println(receivedData);

      // Simpan ke sharedData dalam critical section
      portENTER_CRITICAL(&mux);
      sharedData = receivedData;
      portEXIT_CRITICAL(&mux);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay 100ms
  }
}

// --------------------------------------------
// TASK 2: Publish data via MQTT setiap 5 detik
// --------------------------------------------
void pubTask(void *parameter)
{
  while (true)
  {
    // Ambil data dari sharedData
    portENTER_CRITICAL(&mux);
    String dataToPublish = sharedData;
    portEXIT_CRITICAL(&mux);

    if (dataToPublish != "")
    {
      String temperature, humidity;

      // Parsing data sensor
      parseData(dataToPublish, temperature, humidity);

      // Publish ke MQTT broker
      client.publish("ESP32TEST_Elitech/temperature", temperature, true, 1);
      client.publish("ESP32TEST_Elitech/humidity", humidity, true, 1);

      Serial.println("Kirim data ke broker");
    }

    // Delay 5 detik
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
