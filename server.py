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
    conn.execute("PRAGMA journal_mode=WAL;")
    conn.execute("PRAGMA busy_timeout=5000;")
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

    # 启动时清理超过 730 天的老数据
    cutoff = (datetime.now(timezone.utc) - timedelta(days=730)).isoformat()
    deleted = conn.execute("DELETE FROM readings WHERE timestamp < ?", (cutoff,)).rowcount
    if deleted:
        print(f"[cleanup] 删除了 {deleted} 条超过 730 天的记录")

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

    if not isinstance(device, str):
        return jsonify({"error": "device must be a string"}), 400

    device = device.strip() or "unknown"

    try:
        temp = float(temp)
        humidity = float(humidity)
        pressure = 0.0 if pressure is None else float(pressure)
    except (TypeError, ValueError):
        return jsonify({"error": "temp, humidity, pressure must be numbers"}), 400

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
    """获取历史数据，前端画图用（支持按小时范围或按日历日期）"""
    device = request.args.get("device", "%")
    hours = request.args.get("hours", type=int)
    from_date = request.args.get("from")
    to_date = request.args.get("to")

    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()

    if from_date:
        # ---------- 按日历日期查询 ----------
        since = from_date
        # 计算实际小时差决定降采样级别
        from_dt = datetime.fromisoformat(from_date)
        to_dt = datetime.fromisoformat(to_date) if to_date else (from_dt + timedelta(days=31))
        span_hours = (to_dt - from_dt).total_seconds() / 3600

        conditions = "device LIKE ? AND timestamp >= ?"
        params = [device, since]
        if to_date:
            conditions += " AND timestamp < ?"
            params.append(to_date)

        if span_hours <= 48:
            query = f"""SELECT AVG(temp), AVG(humidity), AVG(pressure),
                        datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 120 * 120, 'unixepoch') || 'Z'
                        FROM readings WHERE {conditions} GROUP BY 4 ORDER BY 4 ASC"""
            mode = "avg_2m"
        elif span_hours <= 168:
            query = f"""SELECT AVG(temp), AVG(humidity), AVG(pressure),
                        datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 900 * 900, 'unixepoch') || 'Z'
                        FROM readings WHERE {conditions} GROUP BY 4 ORDER BY 4 ASC"""
            mode = "avg_15m"
        elif span_hours <= 720:
            query = f"""SELECT AVG(temp), AVG(humidity), AVG(pressure),
                        datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 3600 * 3600, 'unixepoch') || 'Z'
                        FROM readings WHERE {conditions} GROUP BY 4 ORDER BY 4 ASC"""
            mode = "avg_1h"
        else:
            query = f"""SELECT AVG(temp), AVG(humidity), AVG(pressure),
                        datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 21600 * 21600, 'unixepoch') || 'Z'
                        FROM readings WHERE {conditions} GROUP BY 4 ORDER BY 4 ASC"""
            mode = "avg_6h"

        c.execute(query, params)
    else:
        # ---------- 按小时范围查询（原有逻辑） ----------
        if hours is None:
            hours = 24
        since = (datetime.now(timezone.utc) - timedelta(hours=hours)).isoformat()

        if hours <= 48:
            c.execute(
                """SELECT AVG(temp), AVG(humidity), AVG(pressure),
                          datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 120 * 120, 'unixepoch') || 'Z'
                   FROM readings
                   WHERE device LIKE ? AND timestamp >= ?
                   GROUP BY 4 ORDER BY 4 ASC""",
                (device, since),
            )
            mode = "avg_2m"
        elif hours <= 168:
            c.execute(
                """SELECT AVG(temp), AVG(humidity), AVG(pressure),
                          datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 900 * 900, 'unixepoch') || 'Z'
                   FROM readings
                   WHERE device LIKE ? AND timestamp >= ?
                   GROUP BY 4 ORDER BY 4 ASC""",
                (device, since),
            )
            mode = "avg_15m"
        elif hours <= 720:
            c.execute(
                """SELECT AVG(temp), AVG(humidity), AVG(pressure),
                          datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 3600 * 3600, 'unixepoch') || 'Z'
                   FROM readings
                   WHERE device LIKE ? AND timestamp >= ?
                   GROUP BY 4 ORDER BY 4 ASC""",
                (device, since),
            )
            mode = "avg_1h"
        else:
            c.execute(
                """SELECT AVG(temp), AVG(humidity), AVG(pressure),
                          datetime(CAST(strftime('%s', timestamp) AS INTEGER) / 21600 * 21600, 'unixepoch') || 'Z'
                   FROM readings
                   WHERE device LIKE ? AND timestamp >= ?
                   GROUP BY 4 ORDER BY 4 ASC""",
                (device, since),
            )
            mode = "avg_6h"

    rows = c.fetchall()
    conn.close()

    return jsonify({
        "device": device,
        "since": since if not from_date else from_date,
        "to": to_date or "",
        "count": len(rows),
        "mode": mode,
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


print(f"🌡  温度监控服务已启动")
print(f"📂 数据库: {DB_PATH}")
print(f"🌐 访问地址: http://0.0.0.0:5000")
print(f"📡 API: POST /api/data  ← ESP32 上报到这里")
print(f"📊 仪表盘: GET  /       ← 浏览器打开看曲线")
print()
