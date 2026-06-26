# 使用 Alpine 基础镜像，体积小（~50MB）
FROM python:3.11-alpine

WORKDIR /app

# 安装依赖
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# 复制代码
COPY server.py dashboard.html .

# 数据卷挂载点（data.db 会存在这里）
VOLUME /app/data

EXPOSE 5000

# 确保 data.db 在数据卷里
CMD ["python3", "server.py"]
