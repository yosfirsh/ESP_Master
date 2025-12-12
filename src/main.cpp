#include <Arduino.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <MQTT.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// --------------------------------------------
// WIFI
// --------------------------------------------
const char *ssid = "Wi-Fi Home";
const char *password = "12348765";

// --------------------------------------------
BluetoothSerial SerialBT;
WiFiClient net;
MQTTClient client;

TaskHandle_t btTaskHandle;
TaskHandle_t publishTaskHandle;

SemaphoreHandle_t dataMutex;

String sharedData = "";

// --------------------------------------------
void parseData(const String &input, String &temperature, String &humidity)
{
  if (!input.startsWith("Suhu: "))
    return;

  int suhuStart = input.indexOf("Suhu: ") + 6;
  int suhuEnd = input.indexOf(" °C");

  int humStart = input.indexOf("Kelembapan: ") + 12;
  int humEnd = input.indexOf(" %");

  if (suhuStart < 0 || suhuEnd < 0 || humStart < 0 || humEnd < 0)
    return;

  temperature = input.substring(suhuStart, suhuEnd);
  humidity = input.substring(humStart, humEnd);
}

// --------------------------------------------
void btTask(void *parameter)
{
  while (1)
  {
    if (SerialBT.available())
    {
      String received = SerialBT.readStringUntil('\n');

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      sharedData = received;
      xSemaphoreGive(dataMutex);

      Serial.println("RX Bluetooth: " + received);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// --------------------------------------------
void pubTask(void *parameter)
{
  while (1)
  {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    String dataToPublish = sharedData;
    xSemaphoreGive(dataMutex);

    if (dataToPublish.length() > 0)
    {
      String t, h;
      parseData(dataToPublish, t, h);

      if (t != "" && h != "")
      {
        client.publish("ESP32TEST_Elitech/temperature", t, true, 1);
        client.publish("ESP32TEST_Elitech/humidity", h, true, 1);

        Serial.println("Publish MQTT OK");
      }
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// --------------------------------------------
bool connectMQTT()
{
  Serial.println("Connecting to MQTT...");
  client.setWill("ESP32TEST_Elitech/status", "offline", true, 1);

  if (!client.connect("12345678", "yosfirsh", "yosfirsh"))
  {
    Serial.println("MQTT connect failed");
    return false;
  }

  client.publish("ESP32TEST_Elitech/status", "online", true, 1);
  client.subscribe("ESP32TEST_Elitech/#", 1);

  Serial.println("MQTT connected");
  return true;
}

// --------------------------------------------
void setup()
{
  Serial.begin(115200);

  dataMutex = xSemaphoreCreateMutex();

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  // MQTT
  client.begin("broker.yosfirsh.my.id", net);
  connectMQTT();

  // Bluetooth
  SerialBT.begin("ESP32_Master", true);
  uint8_t mac[6] = {0xD0, 0xEF, 0x76, 0x15, 0x58, 0xB6};
  SerialBT.connect(mac);

  // Tasks
  xTaskCreatePinnedToCore(btTask, "BluetoothTask", 8096, NULL, 1, &btTaskHandle, 0);
  xTaskCreatePinnedToCore(pubTask, "PublishTask", 8096, NULL, 1, &publishTaskHandle, 1);
}

// --------------------------------------------
void loop()
{
  client.loop(); // WAJIB

  if (!client.connected())
    connectMQTT();

  delay(10);
}
