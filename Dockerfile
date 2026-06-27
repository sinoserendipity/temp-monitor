# 使用 Alpine 基础镜像，体积小（~50MB）
FROM python:3.11-alpine

WORKDIR /app

# 安装依赖
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# 复制代码
COPY server.py dashboard.html .

# 数据库由 docker-compose 挂载管理，不在镜像里声明 VOLUME
EXPOSE 5000

# 使用 gunicorn 生产服务器（4 workers）
CMD ["gunicorn", "-w", "4", "-b", "0.0.0.0:5000", "server:app"]
