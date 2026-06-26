# ESP32 + BME280 室内温湿度监控

## 项目结构

```
temp_monitor/
├── esp32_bme280_monitor.ino    # ESP32 端代码（Arduino IDE）
├── server.py                   # 服务器端（Python/Flask）
├── dashboard.html              # Web 仪表盘（Chart.js）
└── requirements.txt            # Python 依赖
```

## 一、接线（BME280 -> ESP32）

| BME280 | ESP32 |
|--------|-------|
| VCC    | 3.3V  |
| GND    | GND   |
| SCL    | GPIO 22 |
| SDA    | GPIO 21 |

## 二、上传 ESP32

1. Arduino IDE 中安装以下库（库管理器搜索安装）：
   - `Adafruit BME280 Library`
   - `Adafruit Unified Sensor`
   - `ArduinoJson` (by Benoit Blanchon)

2. 打开 `esp32_bme280_monitor.ino`，修改顶部配置：
   - `WIFI_SSID` / `WIFI_PASSWORD` — 你家 WiFi
   - `SERVER_HOST` — 跑 server.py 那台机器的 IP
   - `DEVICE_NAME` — 比如"客厅"、"卧室"

3. 选择 ESP32 开发板，上传

## 三、启动服务器

在你的 Unraid 或任意一台局域网的机器上：

```bash
pip install -r requirements.txt
python3 server.py
```

浏览器打开 `http://这台机器IP:5000` 就能看到仪表盘。

## 四、接口说明

- `POST /api/data` — ESP32 上报数据
- `GET /api/data?hours=24&device=客厅` — 取历史数据
- `GET /api/devices` — 列出所有设备名
- `GET /` — Web 仪表盘
