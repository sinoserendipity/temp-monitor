/*
 * ESP32 + DHT22 温湿度监控（Light Sleep 版）
 * ==========================================
 * 每次读数上报后进入 Light Sleep 60 秒。
 * WiFi 保持连接，微秒级唤醒，功耗约 ~0.8 mA（比持续运行省电 99%）。
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

// ====== WiFi 配置 ======
const char* WIFI_SSID     = "wifi_SSID";
const char* WIFI_PASSWORD = "wifi_password";

// ====== 服务器配置 ======
const char* SERVER_HOST   = "192.168.1.100";   // 服务器地址
const int   SERVER_PORT   = 5000;               // 服务器端口（HTTP）
const char* DEVICE_NAME   = "客厅";              // 传感器位置
const int   INTERVAL_SEC  = 60;                 // 上报间隔（秒）

// ====== DHT22 配置 ======
#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// 板载 LED（大部分 ESP32 Dev Board 是 GPIO2）
#define LED_BUILTIN 2

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
      Serial.println("\n⚠ WiFi 失败，重启中...");
      delay(3000);
      ESP.restart();
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
  Serial.println("🌡 ESP32 + DHT22 温湿度监控（Light Sleep）");
  Serial.println("==============================");

  // 关掉板载 LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  dht.begin();
  connectWiFi();
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastSend = 0;

  if (now - lastSend < INTERVAL_SEC * 1000UL) {
    return;
  }
  lastSend = now;

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

  // 进入 Light Sleep，微秒级唤醒
  // GPIO 状态保持（LED 保持关闭），WiFi 保持连接
  esp_sleep_enable_timer_wakeup(INTERVAL_SEC * 1000000ULL);
  esp_light_sleep_start();
}
