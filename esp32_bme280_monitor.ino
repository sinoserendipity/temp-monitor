/*
 * ESP32 + BME280 温湿度气压监控（健壮重连版）
 * ==========================================
 * 每 60 秒读取一次数据，POST 到服务器
 *
 * 接线（BME280 -> ESP32）：
 *   VCC -> 3.3V
 *   GND -> GND
 *   SCL -> GPIO 22
 *   SDA -> GPIO 21
 */

#include <WiFi.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>

// ====== 修改这里的配置 ======
const char* WIFI_SSID     = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的WiFi密码";
const char* SERVER_HOST   = "192.168.1.100";  // 跑 server.py 的电脑 IP
const int   SERVER_PORT   = 5000;
const char* DEVICE_NAME   = "客厅";            // 传感器位置名称
const int   INTERVAL_SEC  = 60;                // 上报间隔（秒）
// ============================

Adafruit_BME280 bme;

// BME280 I2C 地址通常是 0x76 或 0x77
uint8_t bmeAddress = 0x76;

unsigned long lastSend = 0;
int consecutive_failures = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("\n📶 正在初始化 WiFi 连接...");
  
  // 1. 彻底断开旧连接，释放状态机，防止死锁
  WiFi.disconnect(true); 
  delay(1000);

  // 2. 显式设置 STA 模式并开启自动重连
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tries++;
    if (tries > 40) {  // 约 20 秒超时
      Serial.println("\n⚠ WiFi 连接超时，重启 ESP32...");
      delay(3000);
      ESP.restart(); // 终极兜底：直接重启
    }
  }
  Serial.println("\n✅ WiFi 已连接，IP: " + WiFi.localIP().toString());
  Serial.println("📡 服务器地址: http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT));
}

bool initBME280() {
  // 先试 0x76，不行再试 0x77
  for (int i = 0; i < 2; i++) {
    bmeAddress = (i == 0) ? 0x76 : 0x77;
    if (bme.begin(bmeAddress)) {
      Serial.println("✅ BME280 已初始化，地址: 0x" + String(bmeAddress, HEX));
      return true;
    }
    delay(100);
  }

  Serial.println("⚠ 未检测到 BME280，检查接线");
  return false;
}

void sendData(float temp, float humidity, float pressure) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // 组装 JSON
  JsonDocument doc;
  doc["device"]    = DEVICE_NAME;
  doc["temp"]      = temp;
  doc["humidity"]  = humidity;
  doc["pressure"]  = pressure;

  String json;
  serializeJson(doc, json);

  Serial.print("📤 发送: ");
  Serial.println(json);

  // HTTP POST
  WiFiClient client;
  client.setTimeout(10); // 设置读写超时（10秒）

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("⚠ 连接服务器失败");
    consecutive_failures++;
    
    // 如果连续 3 次连不上服务器，强制重置 WiFi
    if (consecutive_failures >= 3) {
      Serial.println("⚠ 连续 3 次尝试连接服务器失败，强制重置 WiFi...");
      WiFi.disconnect(true);
      consecutive_failures = 0;
    }
    return;
  }

  consecutive_failures = 0; // 发送成功，清零失败计数

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
      Serial.println("⚠ 服务器响应超时");
      client.stop();
      return;
    }
  }

  // 读第一行响应状态
  String line = client.readStringUntil('\n');
  Serial.println("📥 服务器: " + line);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================");
  Serial.println("🌡 ESP32 + BME280 温湿度气压监控");
  Serial.println("==============================");

  connectWiFi();

  if (!initBME280()) {
    Serial.println("⚠ 建议检查：");
    Serial.println("   1. 接线是否正确（VCC 接 3.3V，不是 5V）");
    Serial.println("   2. I2C 地址是 0x76 还是 0x77");
    Serial.println("   3. BME280 模块是否损坏");
  }

  // 连接后立即读取并上报一次
  if (bme.begin(bmeAddress)) {
    float temp      = bme.readTemperature();
    float humidity  = bme.readHumidity();
    float pressure  = bme.readPressure() / 100.0F; // Pa -> hPa

    if (!isnan(temp) && !isnan(humidity) && !isnan(pressure)) {
      Serial.print("🌡 ");
      Serial.print(temp);
      Serial.print("°C  💧 ");
      Serial.print(humidity);
      Serial.print("%  📊 ");
      Serial.print(pressure);
      Serial.println(" hPa");

      sendData(temp, humidity, pressure);
    } else {
      Serial.println("⚠ 首次 BME280 读取失败");
    }
  }
  lastSend = millis();

  Serial.println("\n🟢 开始运行，每 " + String(INTERVAL_SEC) + " 秒上报一次\n");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSend < INTERVAL_SEC * 1000UL) {
    return;
  }
  lastSend = now;

  if (!bme.begin(bmeAddress)) {
    Serial.println("⚠ BME280 读取失败，跳过本轮");
    return;
  }

  float temp      = bme.readTemperature();
  float humidity  = bme.readHumidity();
  float pressure  = bme.readPressure() / 100.0F;  // Pa -> hPa

  Serial.print("🌡 ");
  Serial.print(temp);
  Serial.print("°C  💧 ");
  Serial.print(humidity);
  Serial.print("%  📊 ");
  Serial.print(pressure);
  Serial.println(" hPa");

  sendData(temp, humidity, pressure);
}
