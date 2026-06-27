# ESP32 + DHT22 室内温湿度监控

每 60 秒采集一次温度、湿度，POST 到服务器，浏览器打开 Web 仪表盘实时查看曲线和历史数据。

## 项目结构

```
temp_monitor/
├── esp32_dht22_monitor.ino      # ESP32 端代码（Arduino IDE）
├── server.py                     # 服务器端（Python/Flask）
├── dashboard.html                # Web 仪表盘（Chart.js）
├── Dockerfile                    # Docker 镜像构建
├── docker-compose.yml            # Docker Compose 部署
├── requirements.txt              # Python 依赖
└── .github/workflows/
    └── docker-build.yml          # GitHub Actions 自动构建镜像
```

## 一、接线（DHT22 -> ESP32）

| DHT22 | ESP32 |
|-------|-------|
| VCC   | 3.3V  |
| DATA  | GPIO 4（D4） |
| GND   | GND   |

> **注意：** DATA 和 VCC 之间需要加一只 **10kΩ 上拉电阻**（部分模块已内置）。

## 二、上传 ESP32

1. **Arduino IDE** 中安装以下库（库管理器搜索安装）：
   - `DHT sensor library`（Adafruit）
   - `Adafruit Unified Sensor`
   - `ArduinoJson`（by Benoit Blanchon）

2. 打开 `esp32_dht22_monitor.ino`，修改顶部配置：

```cpp
const char* WIFI_SSID     = "你的WiFi名";       // 修改
const char* WIFI_PASSWORD = "你的WiFi密码";     // 修改
const char* SERVER_HOST   = "服务器IP地址";      // 修改
const char* DEVICE_NAME   = "客厅";              // 修改：传感器位置
```

3. 选择开发板为 **ESP32 Dev Module**，上传

## 三、部署服务器

### 方式一：Docker 部署（推荐）

在任意一台支持 Docker 的机器上（OpenWrt、群晖、Unraid、树莓派等）：

```bash
# 1. 解压项目
tar xzf temp_monitor_docker.tar.gz
cd temp_monitor

# 2. 创建数据目录
mkdir -p data

# 3. 启动
docker compose up -d

# 4. 浏览器打开
# http://这台机器IP:5000
```

管理命令：

| 命令 | 作用 |
|------|------|
| `docker compose up -d` | 启动 |
| `docker compose down` | 停止 |
| `docker compose logs -f` | 看日志 |
| `docker compose pull` | 更新镜像 |

### 方式二：直接运行

```bash
pip install -r requirements.txt
python3 server.py
```

浏览器打开 `http://这台机器IP:5000` 就能看到仪表盘。

### GitHub Actions 自动构建

推送代码到 GitHub 后，Action 自动构建 Docker 镜像并推送到 `ghcr.io`：

```bash
git push origin main
```

在需要部署的机器上可以直接拉取：

```bash
docker pull ghcr.io/<用户名>/temp-monitor:latest
```

## 四、接口说明

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/data` | ESP32 上报温湿度数据 |
| GET | `/api/data?hours=24&device=客厅` | 取历史数据 |
| GET | `/api/devices` | 列出所有设备名 |
| GET | `/` | Web 仪表盘页面 |

## 五、数据持久化

Docker 部署时数据保存在 `/app/data/` 目录下的 `data.db` 中，重启不丢数据。
