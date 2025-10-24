# AI Development System - 部署指南

## 目录

1. [部署概述](#部署概述)
2. [系统要求](#系统要求)
3. [安装部署](#安装部署)
4. [配置管理](#配置管理)
5. [Docker部署](#docker部署)
6. [云平台部署](#云平台部署)
7. [CI/CD集成](#cicd集成)
8. [监控和维护](#监控和维护)
9. [故障排除](#故障排除)

## 部署概述

AI Development System 支持多种部署方式，从本地开发环境到企业级生产环境。本指南将帮助您选择合适的部署方式并完成部署过程。

### 部署架构

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   用户界面      │    │   API服务       │    │   分析引擎      │
│  (CLI/Web)      │◄──►│  (REST API)     │◄──►│  (Core Engine)  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌─────────────────┐    ┌─────────────────┐
                       │   数据存储      │    │   外部工具      │
                       │ (File/Database) │    │ (Build/Test)    │
                       └─────────────────┘    └─────────────────┘
```

### 部署模式

1. **单机部署**: 适用于开发环境和小规模使用
2. **容器化部署**: 适用于标准化环境和微服务架构
3. **集群部署**: 适用于大规模生产环境
4. **云原生部署**: 适用于云平台和Serverless架构

## 系统要求

### 最低系统要求

**CPU**: 2核心
**内存**: 4GB RAM
**存储**: 10GB 可用空间
**操作系统**:
- Windows 10/11
- macOS 10.15+
- Ubuntu 18.04+ / CentOS 7+ / RHEL 7+

### 推荐系统要求

**CPU**: 4核心以上
**内存**: 8GB RAM 以上
**存储**: 50GB 可用空间（SSD推荐）
**网络**: 稳定的互联网连接

### 软件依赖

#### 必需依赖
- Python 3.7+
- pip 21.0+

#### 可选依赖
- Docker 20.03+ (容器化部署)
- Node.js 16+ (JavaScript项目分析)
- Java 11+ (Java项目分析)
- Maven 3.6+ / Gradle 6.0+ (Java项目构建)
- CMake 3.15+ / Make (C/C++项目构建)

## 安装部署

### 1. 本地开发环境部署

#### 步骤1: 环境准备

```bash
# 检查Python版本
python --version  # 应该是3.7+

# 升级pip
pip install --upgrade pip

# 创建虚拟环境（推荐）
python -m venv ai-dev-env
source ai-dev-env/bin/activate  # Linux/macOS
# 或
ai-dev-env\Scripts\activate  # Windows
```

#### 步骤2: 安装系统

```bash
# 克隆仓库
git clone https://github.com/your-repo/PortMaster-AI-Dev-System.git
cd PortMaster-AI-Dev-System

# 安装依赖
pip install -r requirements.txt

# 验证安装
python main.py --help
```

#### 步骤3: 初始配置

```bash
# 创建配置文件
python main.py config init

# 验证配置
python main.py config validate

# 测试运行
python main.py analyze ./examples/sample_project
```

### 2. 生产环境部署

#### 步骤1: 系统用户创建

```bash
# 创建专用用户
sudo useradd -m -s /bin/bash ai-dev-system
sudo usermod -aG sudo ai-dev-system

# 切换到专用用户
sudo su - ai-dev-system
```

#### 步骤2: 目录结构创建

```bash
# 创建目录结构
sudo mkdir -p /opt/ai-dev-system
sudo mkdir -p /var/log/ai-dev-system
sudo mkdir -p /var/lib/ai-dev-system
sudo mkdir -p /etc/ai-dev-system

# 设置权限
sudo chown -R ai-dev-system:ai-dev-system /opt/ai-dev-system
sudo chown -R ai-dev-system:ai-dev-system /var/log/ai-dev-system
sudo chown -R ai-dev-system:ai-dev-system /var/lib/ai-dev-system
sudo chown -R ai-dev-system:ai-dev-system /etc/ai-dev-system
```

#### 步骤3: 系统安装

```bash
# 切换到安装目录
cd /opt/ai-dev-system

# 下载并解压系统
wget https://github.com/your-repo/PortMaster-AI-Dev-System/releases/latest/ai-dev-system.tar.gz
tar -xzf ai-dev-system.tar.gz
rm ai-dev-system.tar.gz

# 安装依赖
pip install -r requirements.txt --user
```

#### 步骤4: 服务配置

创建systemd服务文件：

```bash
sudo tee /etc/systemd/system/ai-dev-system.service > /dev/null <<EOF
[Unit]
Description=AI Development System
After=network.target

[Service]
Type=simple
User=ai-dev-system
Group=ai-dev-system
WorkingDirectory=/opt/ai-dev-system
Environment=PATH=/opt/ai-dev-system/.local/bin:/usr/local/bin:/usr/bin:/bin
ExecStart=/opt/ai-dev-system/.local/bin/python main.py serve --host 0.0.0.0 --port 8080
Restart=always
RestartSec=10

# 日志配置
StandardOutput=journal
StandardError=journal
SyslogIdentifier=ai-dev-system

# 安全配置
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/ai-dev-system /var/log/ai-dev-system /tmp

[Install]
WantedBy=multi-user.target
EOF
```

启用和启动服务：

```bash
sudo systemctl daemon-reload
sudo systemctl enable ai-dev-system
sudo systemctl start ai-dev-system
sudo systemctl status ai-dev-system
```

### 3. 反向代理配置

#### Nginx配置

```bash
sudo apt-get install nginx  # Ubuntu/Debian
# 或
sudo yum install nginx      # CentOS/RHEL
```

创建Nginx配置文件：

```bash
sudo tee /etc/nginx/sites-available/ai-dev-system > /dev/null <<EOF
server {
    listen 80;
    server_name your-domain.com;

    # 重定向到HTTPS
    return 301 https://\$server_name\$request_uri;
}

server {
    listen 443 ssl http2;
    server_name your-domain.com;

    # SSL配置
    ssl_certificate /path/to/ssl/cert.pem;
    ssl_certificate_key /path/to/ssl/private.key;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-RSA-AES256-GCM-SHA512:DHE-RSA-AES256-GCM-SHA512;

    # 安全头
    add_header X-Frame-Options DENY;
    add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";

    # 代理配置
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;

        # 超时配置
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }

    # WebSocket支持
    location /ws {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
    }

    # 静态文件缓存
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
EOF
```

启用站点：

```bash
sudo ln -s /etc/nginx/sites-available/ai-dev-system /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

## 配置管理

### 环境变量配置

创建环境配置文件：

```bash
sudo tee /etc/ai-dev-system/environment > /dev/null <<EOF
# 数据库配置
DATABASE_URL=postgresql://user:password@localhost:5432/ai_dev_system

# Redis配置
REDIS_URL=redis://localhost:6379/0

# 日志配置
LOG_LEVEL=INFO
LOG_FILE=/var/log/ai-dev-system/app.log

# 安全配置
SECRET_KEY=your-secret-key-here
JWT_SECRET=your-jwt-secret-here

# 外部服务配置
GITHUB_TOKEN=your-github-token
GITLAB_TOKEN=your-gitlab-token

# 性能配置
MAX_WORKERS=4
CACHE_TTL=3600
UPLOAD_MAX_SIZE=100MB
EOF
```

### 主配置文件

创建生产配置文件：

```bash
sudo tee /etc/ai-dev-system/config.yaml > /dev/null <<EOF
# 生产环境配置
system:
  name: "AI Development System"
  debug_mode: false
  log_level: "INFO"
  log_file: "/var/log/ai-dev-system/app.log"

# 数据存储
storage:
  type: "file"
  base_path: "/var/lib/ai-dev-system"
  backup_path: "/var/backups/ai-dev-system"

# 性能配置
performance:
  max_workers: 4
  cache_enabled: true
  cache_ttl: 3600
  timeout_multiplier: 2.0

# 安全配置
security:
  enable_authentication: true
  session_timeout: 3600
  max_login_attempts: 5
  enable_rate_limiting: true

# 报告配置
reporting:
  output_directory: "/var/lib/ai-dev-system/reports"
  retention_days: 30
  compression: true

# API配置
api:
  host: "0.0.0.0"
  port: 8080
  enable_cors: true
  rate_limit: "100/hour"
EOF
```

## Docker部署

### 1. Dockerfile

```dockerfile
FROM python:3.9-slim

# 设置工作目录
WORKDIR /app

# 安装系统依赖
RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    cmake \
    maven \
    nodejs \
    npm \
    && rm -rf /var/lib/apt/lists/*

# 复制依赖文件
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# 复制应用代码
COPY . .

# 创建非root用户
RUN useradd -m -u 1000 aiuser && chown -R aiuser:aiuser /app
USER aiuser

# 暴露端口
EXPOSE 8080

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 启动命令
CMD ["python", "main.py", "serve", "--host", "0.0.0.0", "--port", "8080"]
```

### 2. Docker Compose

```yaml
version: '3.8'

services:
  ai-dev-system:
    build: .
    ports:
      - "8080:8080"
    volumes:
      - ./data:/app/data
      - ./logs:/app/logs
      - ./config:/app/config
    environment:
      - LOG_LEVEL=INFO
      - DATABASE_URL=postgresql://postgres:password@db:5432/ai_dev
      - REDIS_URL=redis://redis:6379/0
    depends_on:
      - db
      - redis
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3

  db:
    image: postgres:13
    environment:
      - POSTGRES_DB=ai_dev
      - POSTGRES_USER=postgres
      - POSTGRES_PASSWORD=password
    volumes:
      - postgres_data:/var/lib/postgresql/data
    restart: unless-stopped

  redis:
    image: redis:6-alpine
    volumes:
      - redis_data:/data
    restart: unless-stopped

  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
      - ./ssl:/etc/nginx/ssl
    depends_on:
      - ai-dev-system
    restart: unless-stopped

volumes:
  postgres_data:
  redis_data:
```

### 3. 部署命令

```bash
# 构建和启动
docker-compose up -d

# 查看日志
docker-compose logs -f ai-dev-system

# 扩展服务
docker-compose up -d --scale ai-dev-system=3

# 更新服务
docker-compose pull
docker-compose up -d
```

## 云平台部署

### 1. AWS部署

#### 使用EC2

```bash
# 创建EC2实例
aws ec2 run-instances \
    --image-id ami-0c02fb55956c7d316 \
    --instance-type t3.medium \
    --key-name my-key-pair \
    --security-group-ids sg-903004f8 \
    --subnet-id subnet-6e7f829e \
    --user-data file://user-data.sh
```

用户数据脚本 `user-data.sh`:

```bash
#!/bin/bash
yum update -y
yum install -y docker git
systemctl start docker
systemctl enable docker

# 拉取并运行容器
docker run -d \
    --name ai-dev-system \
    -p 8080:8080 \
    -v /data:/app/data \
    your-repo/ai-dev-system:latest
```

#### 使用ECS

```json
{
  "family": "ai-dev-system",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "512",
  "memory": "1024",
  "executionRoleArn": "arn:aws:iam::account:role/ecsTaskExecutionRole",
  "containerDefinitions": [
    {
      "name": "ai-dev-system",
      "image": "your-repo/ai-dev-system:latest",
      "portMappings": [
        {
          "containerPort": 8080,
          "protocol": "tcp"
        }
      ],
      "environment": [
        {
          "name": "LOG_LEVEL",
          "value": "INFO"
        }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/ai-dev-system",
          "awslogs-region": "us-west-2",
          "awslogs-stream-prefix": "ecs"
        }
      }
    }
  ]
}
```

### 2. Google Cloud Platform部署

#### 使用Cloud Run

```bash
# 构建镜像
gcloud builds submit --tag gcr.io/PROJECT_ID/ai-dev-system

# 部署到Cloud Run
gcloud run deploy ai-dev-system \
    --image gcr.io/PROJECT_ID/ai-dev-system \
    --platform managed \
    --region us-central1 \
    --allow-unauthenticated \
    --memory 1Gi \
    --cpu 1 \
    --max-instances 10
```

#### 使用GKE

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ai-dev-system
spec:
  replicas: 3
  selector:
    matchLabels:
      app: ai-dev-system
  template:
    metadata:
      labels:
        app: ai-dev-system
    spec:
      containers:
      - name: ai-dev-system
        image: gcr.io/PROJECT_ID/ai-dev-system:latest
        ports:
        - containerPort: 8080
        env:
        - name: LOG_LEVEL
          value: "INFO"
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
          limits:
            memory: "1Gi"
            cpu: "1000m"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 5
---
apiVersion: v1
kind: Service
metadata:
  name: ai-dev-system-service
spec:
  selector:
    app: ai-dev-system
  ports:
  - port: 80
    targetPort: 8080
  type: LoadBalancer
```

### 3. Azure部署

#### 使用Container Instances

```bash
# 创建资源组
az group create --name ai-dev-rg --location eastus

# 部署容器
az container create \
    --resource-group ai-dev-rg \
    --name ai-dev-system \
    --image your-repo/ai-dev-system:latest \
    --cpu 1 \
    --memory 2 \
    --ports 8080 \
    --environment-variables LOG_LEVEL=INFO
```

#### 使用AKS

```yaml
# k8s-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ai-dev-system
spec:
  replicas: 3
  selector:
    matchLabels:
      app: ai-dev-system
  template:
    metadata:
      labels:
        app: ai-dev-system
    spec:
      containers:
      - name: ai-dev-system
        image: yourregistry.azurecr.io/ai-dev-system:latest
        ports:
        - containerPort: 8080
        env:
        - name: LOG_LEVEL
          valueFrom:
            configMapKeyRef:
              name: ai-dev-config
              key: LOG_LEVEL
---
apiVersion: v1
kind: Service
metadata:
  name: ai-dev-system-service
spec:
  selector:
    app: ai-dev-system
  ports:
  - port: 80
    targetPort: 8080
  type: LoadBalancer
```

## CI/CD集成

### 1. GitHub Actions

```yaml
# .github/workflows/deploy.yml
name: Deploy AI Development System

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Set up Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.9'
    - name: Install dependencies
      run: |
        pip install -r requirements.txt
        pip install -r requirements-dev.txt
    - name: Run tests
      run: |
        pytest tests/ --cov=ai_dev_system --cov-report=xml
    - name: Upload coverage
      uses: codecov/codecov-action@v3

  build:
    needs: test
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'
    steps:
    - uses: actions/checkout@v3
    - name: Build Docker image
      run: |
        docker build -t your-repo/ai-dev-system:${{ github.sha }} .
        docker tag your-repo/ai-dev-system:${{ github.sha }} your-repo/ai-dev-system:latest
    - name: Login to Docker Hub
      uses: docker/login-action@v2
      with:
        username: ${{ secrets.DOCKER_USERNAME }}
        password: ${{ secrets.DOCKER_PASSWORD }}
    - name: Push Docker image
      run: |
        docker push your-repo/ai-dev-system:${{ github.sha }}
        docker push your-repo/ai-dev-system:latest

  deploy:
    needs: build
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'
    steps:
    - name: Deploy to production
      run: |
        # 部署脚本
        curl -X POST "$DEPLOY_WEBHOOK_URL" \
          -H "Authorization: Bearer $DEPLOY_TOKEN" \
          -d '{"image": "your-repo/ai-dev-system:latest"}'
```

### 2. GitLab CI

```yaml
# .gitlab-ci.yml
stages:
  - test
  - build
  - deploy

variables:
  DOCKER_DRIVER: overlay2
  DOCKER_TLS_CERTDIR: "/certs"

test:
  stage: test
  image: python:3.9
  services:
    - postgres:13
    - redis:6
  variables:
    POSTGRES_DB: test_db
    POSTGRES_USER: test_user
    POSTGRES_PASSWORD: test_pass
  script:
    - pip install -r requirements.txt
    - pip install -r requirements-dev.txt
    - pytest tests/ --cov=ai_dev_system
  coverage: '/TOTAL.*\s+(\d+%)$/'
  artifacts:
    reports:
      coverage_report:
        coverage_format: cobertura
        path: coverage.xml

build:
  stage: build
  image: docker:20.10
  services:
    - docker:20.10-dind
  script:
    - docker build -t $CI_REGISTRY_IMAGE:$CI_COMMIT_SHA .
    - docker tag $CI_REGISTRY_IMAGE:$CI_COMMIT_SHA $CI_REGISTRY_IMAGE:latest
    - docker login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD $CI_REGISTRY
    - docker push $CI_REGISTRY_IMAGE:$CI_COMMIT_SHA
    - docker push $CI_REGISTRY_IMAGE:latest
  only:
    - main

deploy:
  stage: deploy
  image: alpine:latest
  before_script:
    - apk add --no-cache curl
  script:
    - curl -X POST "$DEPLOY_URL" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $DEPLOY_TOKEN" \
        -d "{\"image\": \"$CI_REGISTRY_IMAGE:$CI_COMMIT_SHA\"}"
  only:
    - main
  when: manual
```

### 3. Jenkins Pipeline

```groovy
// Jenkinsfile
pipeline {
    agent any

    environment {
        DOCKER_REGISTRY = 'your-registry.com'
        IMAGE_NAME = 'ai-dev-system'
        DEPLOY_URL = credentials('deploy-url')
        DEPLOY_TOKEN = credentials('deploy-token')
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Test') {
            steps {
                sh 'docker build -t test-image -f Dockerfile.test .'
                sh 'docker run --rm test-image'
            }
        }

        stage('Build') {
            steps {
                script {
                    def image = docker.build("${DOCKER_REGISTRY}/${IMAGE_NAME}:${env.BUILD_NUMBER}")
                    docker.withRegistry("https://${DOCKER_REGISTRY}", 'docker-credentials') {
                        image.push()
                        image.push('latest')
                    }
                }
            }
        }

        stage('Deploy') {
            steps {
                sh """
                    curl -X POST "${DEPLOY_URL}" \
                        -H "Content-Type: application/json" \
                        -H "Authorization: Bearer ${DEPLOY_TOKEN}" \
                        -d '{"image": "${DOCKER_REGISTRY}/${IMAGE_NAME}:${BUILD_NUMBER}"}'
                """
            }
        }
    }

    post {
        success {
            echo 'Deployment successful!'
        }
        failure {
            echo 'Deployment failed!'
        }
    }
}
```

## 监控和维护

### 1. 日志管理

#### 配置日志轮转

```bash
sudo tee /etc/logrotate.d/ai-dev-system > /dev/null <<EOF
/var/log/ai-dev-system/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 ai-dev-system ai-dev-system
    postrotate
        systemctl reload ai-dev-system
    endscript
}
EOF
```

#### 日志聚合（ELK Stack）

```yaml
# docker-compose.monitoring.yml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:7.14.0
    environment:
      - discovery.type=single-node
      - "ES_JAVA_OPTS=-Xms512m -Xmx512m"
    volumes:
      - elasticsearch_data:/usr/share/elasticsearch/data
    ports:
      - "9200:9200"

  logstash:
    image: docker.elastic.co/logstash/logstash:7.14.0
    volumes:
      - ./logstash.conf:/usr/share/logstash/pipeline/logstash.conf
    ports:
      - "5044:5044"
    depends_on:
      - elasticsearch

  kibana:
    image: docker.elastic.co/kibana/kibana:7.14.0
    ports:
      - "5601:5601"
    environment:
      ELASTICSEARCH_HOSTS: http://elasticsearch:9200
    depends_on:
      - elasticsearch

volumes:
  elasticsearch_data:
```

### 2. 监控指标

#### Prometheus配置

```yaml
# prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'ai-dev-system'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
    scrape_interval: 5s
```

#### Grafana仪表板

```json
{
  "dashboard": {
    "title": "AI Development System Dashboard",
    "panels": [
      {
        "title": "Request Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(http_requests_total[5m])"
          }
        ]
      },
      {
        "title": "Response Time",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.95, rate(http_request_duration_seconds_bucket[5m]))"
          }
        ]
      }
    ]
  }
}
```

### 3. 健康检查

```python
# 健康检查端点
from flask import Flask, jsonify
import psutil
import time

app = Flask(__name__)

@app.route('/health')
def health_check():
    return jsonify({
        'status': 'healthy',
        'timestamp': time.time(),
        'version': '1.0.0'
    })

@app.route('/ready')
def readiness_check():
    # 检查依赖服务
    checks = {
        'database': check_database(),
        'redis': check_redis(),
        'disk_space': check_disk_space()
    }

    all_healthy = all(checks.values())
    status_code = 200 if all_healthy else 503

    return jsonify({
        'status': 'ready' if all_healthy else 'not_ready',
        'checks': checks
    }), status_code

def check_disk_space():
    return psutil.disk_usage('/').percent < 90

def check_database():
    # 实现数据库连接检查
    return True

def check_redis():
    # 实现Redis连接检查
    return True
```

### 4. 备份策略

```bash
#!/bin/bash
# backup.sh

BACKUP_DIR="/var/backups/ai-dev-system"
DATE=$(date +%Y%m%d_%H%M%S)
CONFIG_FILE="/etc/ai-dev-system/config.yaml"
DATA_DIR="/var/lib/ai-dev-system"

# 创建备份目录
mkdir -p "$BACKUP_DIR/$DATE"

# 备份配置文件
cp "$CONFIG_FILE" "$BACKUP_DIR/$DATE/"

# 备份数据
tar -czf "$BACKUP_DIR/$DATE/data.tar.gz" -C "$DATA_DIR" .

# 备份数据库（如果使用）
pg_dump ai_dev_system | gzip > "$BACKUP_DIR/$DATE/database.sql.gz"

# 清理旧备份（保留30天）
find "$BACKUP_DIR" -type d -mtime +30 -exec rm -rf {} +

echo "Backup completed: $BACKUP_DIR/$DATE"
```

设置定时备份：

```bash
# 添加到crontab
crontab -e

# 每天凌晨2点备份
0 2 * * * /opt/ai-dev-system/scripts/backup.sh
```

## 故障排除

### 常见问题

#### 1. 服务启动失败

**症状**: 服务无法启动，状态为failed

**排查步骤**:
```bash
# 查看服务状态
sudo systemctl status ai-dev-system

# 查看详细日志
sudo journalctl -u ai-dev-system -f

# 检查配置文件
python main.py config validate

# 检查端口占用
sudo netstat -tlnp | grep 8080
```

#### 2. 内存不足

**症状**: 系统响应缓慢，OOM错误

**解决方案**:
```bash
# 增加swap空间
sudo fallocate -l 2G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# 调整系统配置
echo 'vm.swappiness=10' | sudo tee -a /etc/sysctl.conf

# 限制进程内存使用
sudo systemctl edit ai-dev-system
# 添加以下内容：
# [Service]
# MemoryLimit=2G
```

#### 3. 磁盘空间不足

**症状**: 无法生成报告，写入失败

**解决方案**:
```bash
# 清理临时文件
sudo find /tmp -name "ai-dev-*" -mtime +7 -delete

# 清理日志文件
sudo find /var/log/ai-dev-system -name "*.log" -mtime +30 -delete

# 配置日志轮转
sudo logrotate -f /etc/logrotate.d/ai-dev-system

# 移动旧报告到归档
sudo find /var/lib/ai-dev-system/reports -mtime +90 -exec mv {} /var/backups/ai-dev-system/reports/ \;
```

#### 4. 网络连接问题

**症状**: 无法访问外部服务

**排查步骤**:
```bash
# 检查网络连接
ping google.com

# 检查DNS解析
nslookup github.com

# 检查防火墙
sudo ufw status

# 检查代理设置
echo $http_proxy
echo $https_proxy
```

### 性能优化

#### 1. 数据库优化

```sql
-- 创建索引
CREATE INDEX idx_analysis_timestamp ON analysis_results(timestamp);
CREATE INDEX idx_project_name ON projects(name);

-- 配置连接池
ALTER SYSTEM SET max_connections = 100;
ALTER SYSTEM SET shared_buffers = '256MB';
```

#### 2. 缓存优化

```python
# Redis缓存配置
CACHE_CONFIG = {
    'host': 'localhost',
    'port': 6379,
    'db': 0,
    'ttl': 3600,
    'max_connections': 20
}

# 实现缓存装饰器
def cache_result(ttl=3600):
    def decorator(func):
        def wrapper(*args, **kwargs):
            cache_key = f"{func.__name__}:{hash(str(args) + str(kwargs))}"
            result = cache.get(cache_key)
            if result is None:
                result = func(*args, **kwargs)
                cache.setex(cache_key, ttl, result)
            return result
        return wrapper
    return decorator
```

### 安全加固

#### 1. 系统安全

```bash
# 更新系统
sudo apt update && sudo apt upgrade -y

# 配置防火墙
sudo ufw enable
sudo ufw allow ssh
sudo ufw allow 80
sudo ufw allow 443

# 禁用root登录
sudo sed -i 's/PermitRootLogin yes/PermitRootLogin no/' /etc/ssh/sshd_config
sudo systemctl restart ssh

# 配置fail2ban
sudo apt install fail2ban
sudo systemctl enable fail2ban
```

#### 2. 应用安全

```python
# 安全配置
SECURITY_CONFIG = {
    'enable_csrf_protection': True,
    'session_timeout': 3600,
    'max_login_attempts': 5,
    'password_min_length': 12,
    'enable_rate_limiting': True,
    'allowed_hosts': ['your-domain.com'],
    'secure_cookies': True,
    'hsts_enabled': True
}
```

---

此部署指南涵盖了AI Development System的完整部署流程，从本地开发到企业级生产环境。根据您的具体需求选择合适的部署方式，并遵循最佳实践确保系统的稳定性和安全性。