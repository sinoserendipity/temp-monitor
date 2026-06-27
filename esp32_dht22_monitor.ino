/*
 * ESP32 + DHT22 温湿度监控
 * ========================
 * 每 60 秒读取一次，POST 到服务器
 *
 * 接线（DHT22 -> ESP32）：
 *   VCC  -> 3.3V
 *   DATA -> D4 (GPIO4)
 *   GND  -> GND
 *   DATA 和 VCC 之间需加 10kΩ 上拉电阻
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ====== WiFi 配置 ======
const char* WIFI_SSID     = "wifi_SSID";
const char* WIFI_PASSWORD = "wifi_password";

// ====== 服务器配置 ======
const char* SERVER_HOST   = "www.example.com";
const int   SERVER_PORT   = 443;
const char* DEVICE_NAME   = "客厅";            // 改：传感器位置
const int   INTERVAL_SEC  = 60;                // 上报间隔（秒）

// ====== DHT22 配置 ======
#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
unsigned long lastSend = 0;

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
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // 组装 JSON（不加 pressure，服务器会自动填 0）
  JsonDocument doc;
  doc["device"]   = DEVICE_NAME;
  doc["temp"]     = temp;
  doc["humidity"] = humidity;

  String json;
  serializeJson(doc, json);

  Serial.print("📤 发送: ");
  Serial.println(json);

  // HTTPS POST（跳过证书验证，内网用）
  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("⚠ 连服务器失败");
    return;
  }

  client.println("POST /api/data HTTP/1.1");
  client.println("Host: " + String(SERVER_HOST));
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
  Serial.println("🌡 ESP32 + DHT22 温湿度监控");
  Serial.println("==============================");

  dht.begin();
  connectWiFi();

  Serial.println("🟢 开始运行，每 " + String(INTERVAL_SEC) + " 秒上报一次\n");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSend < INTERVAL_SEC * 1000UL) {
    return;
  }
  lastSend = now;

  float temp     = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temp) || isnan(humidity)) {
    Serial.println("⚠ DHT22 读取失败");
    return;
  }

  Serial.print("🌡 ");
  Serial.print(temp);
  Serial.print("°C  💧 ");
  Serial.print(humidity);
  Serial.println("%");

  sendData(temp, humidity);
}
