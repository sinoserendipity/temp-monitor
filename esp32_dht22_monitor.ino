/*
 * ESP32 + DHT22 温湿度监控（深度睡眠版）
 * ======================================
 * 每 60 秒唤醒一次，读取数据并上报后进入深度睡眠。
 * 深度睡眠下电流约 10 µA，比持续运行的 ~80 mA 省电 99%。
 *
 * 接线（DHT22 -> ESP32）：
 *   VCC  -> 3.3V
 *   DATA -> D4 (GPIO4)
 *   GND  -> GND
 *   DATA 和 VCC 之间需加 10kΩ 上拉电阻
 */

#include <WiFi.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>

// ====== 深度睡眠配置 ======
#define uS_TO_S_FACTOR 1000000ULL
const int TIME_TO_SLEEP = 60;   // 睡眠时间（秒）

// ====== WiFi 配置 ======
const char* WIFI_SSID     = "wifi_SSID";
const char* WIFI_PASSWORD = "wifi_password";

// ====== 服务器配置 ======
const char* SERVER_HOST   = "192.168.1.100";   // 服务器地址
const int   SERVER_PORT   = 5000;               // 服务器端口（HTTP）
const char* DEVICE_NAME   = "客厅";              // 传感器位置

// ====== DHT22 配置 ======
#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// ---------- WiFi 连接 ----------

void connectWiFi() {
  Serial.print("📶 连接 WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tries++;
    if (tries > 40) {
      Serial.println("\n⚠ WiFi 失败，进入深度睡眠...");
      delay(100);
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }
  }
  Serial.println("\n✅ 已连接，IP: " + WiFi.localIP().toString());
}

// ---------- 上报数据 ----------

void sendData(float temp, float humidity) {
  // 组装 JSON
  JsonDocument doc;
  doc["device"]   = DEVICE_NAME;
  doc["temp"]     = temp;
  doc["humidity"] = humidity;

  String json;
  serializeJson(doc, json);

  Serial.print("📤 发送: ");
  Serial.println(json);

  // HTTP POST（内网，无需 TLS）
  WiFiClient client;
  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("⚠ 连服务器失败");
    return;
  }

  client.println("POST /api/data HTTP/1.1");
  client.println("Host: " + String(SERVER_HOST) + ":" + String(SERVER_PORT));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(json.length());
  client.println();
  client.println(json);

  // 等响应
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("⚠ 响应超时");
      client.stop();
      return;
    }
  }

  String line = client.readStringUntil('\n');
  Serial.println("📥 服务器: " + line);
  client.stop();
}

// ---------- 主程序 ----------

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================");
  Serial.println("🌡 ESP32 + DHT22 温湿度监控（深度睡眠）");
  Serial.println("==============================");

  dht.begin();
  connectWiFi();

  float temp     = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temp) || isnan(humidity)) {
    Serial.println("⚠ DHT22 读取失败");
  } else {
    Serial.print("🌡 ");
    Serial.print(temp);
    Serial.print("°C  💧 ");
    Serial.print(humidity);
    Serial.println("%");

    sendData(temp, humidity);
  }

  Serial.print("\n💤 进入深度睡眠 ");
  Serial.print(TIME_TO_SLEEP);
  Serial.println(" 秒...\n");

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop() {
  // 深度睡眠模式下不会执行到这里
}
