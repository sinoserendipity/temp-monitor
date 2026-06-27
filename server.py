#!/usr/bin/env python3
"""
ESP32 + BME280 温度监控服务器
==============================
用法：
  1. 安装依赖：pip install flask
  2. 运行：python3 server.py
  3. 打开浏览器访问 http://本机IP:5000

数据自动存储在同目录下的 data.db（SQLite）。
支持多设备同时上报（device 字段区分）。
"""

import sqlite3
import os
from datetime import datetime, timezone, timedelta

from flask import Flask, request, jsonify, send_file

app = Flask(__name__)
DB_PATH = "/app/data/data.db"


def init_db():
    """初始化数据库表"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            device    TEXT    NOT NULL,
            temp      REAL    NOT NULL,
            humidity  REAL    NOT NULL,
            pressure  REAL    NOT NULL,
            timestamp TEXT    NOT NULL
        )
    """)
    # 按时间和设备建索引，查询更快
    c.execute("""
        CREATE INDEX IF NOT EXISTS idx_device_time 
        ON readings(device, timestamp)
    """)
    conn.commit()
    conn.close()

# 启动时初始化数据库（gunicorn 和 python3 server.py 都会执行）
init_db()


# ---------- API ----------

@app.route("/api/data", methods=["POST"])
def receive_data():
    """ESP32 上报数据"""
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "invalid JSON"}), 400

    device = data.get("device", "unknown")
    temp = data.get("temp")
    humidity = data.get("humidity")
    pressure = data.get("pressure")

    if temp is None or humidity is None:
        return jsonify({"error": "missing fields (temp, humidity)"}), 400
    if pressure is None:
        pressure = 0.0

    now = datetime.now(timezone.utc).isoformat()

    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute(
        "INSERT INTO readings (device, temp, humidity, pressure, timestamp) VALUES (?, ?, ?, ?, ?)",
        (device, temp, humidity, pressure, now),
    )
    conn.commit()
    conn.close()

    print(f"[{now}] {device}: {temp}°C, {humidity}%, {pressure}hPa")
    return jsonify({"ok": True}), 201


@app.route("/api/data", methods=["GET"])
def get_data():
    """获取历史数据，前端画图用"""
    device = request.args.get("device", "%")
    hours = request.args.get("hours", 24, type=int)

    since = (datetime.now(timezone.utc) - timedelta(hours=hours)).isoformat()

    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute(
        """SELECT temp, humidity, pressure, timestamp 
           FROM readings 
           WHERE device LIKE ? AND timestamp >= ? 
           ORDER BY timestamp ASC""",
        (device, since),
    )
    rows = c.fetchall()
    conn.close()

    return jsonify({
        "device": device,
        "since": since,
        "count": len(rows),
        "data": [
            {"temp": r[0], "humidity": r[1], "pressure": r[2], "time": r[3]}
            for r in rows
        ],
    })


@app.route("/api/devices")
def list_devices():
    """列出所有上报过的设备名"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT DISTINCT device FROM readings ORDER BY device")
    devices = [r[0] for r in c.fetchall()]
    conn.close()
    return jsonify({"devices": devices})


# ---------- 前端页面 ----------

@app.route("/")
def index():
    """返回仪表盘页面"""
    return send_file(os.path.join(os.path.dirname(__file__), "dashboard.html"))


if __name__ == "__main__":
    print(f"🌡  温度监控服务已启动")
    print(f"📂 数据库: {DB_PATH}")
    print(f"🌐 访问地址: http://0.0.0.0:5000")
    print(f"📡 API: POST /api/data  ← ESP32 上报到这里")
    print(f"📊 仪表盘: GET  /       ← 浏览器打开看曲线")
    print()
    app.run(host="0.0.0.0", port=5000, debug=False)
