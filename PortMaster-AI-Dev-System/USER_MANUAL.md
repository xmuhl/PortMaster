# AI Development System - 用户手册

## 目录

1. [简介](#简介)
2. [安装与配置](#安装与配置)
3. [快速开始](#快速开始)
4. [命令行使用](#命令行使用)
5. [编程接口](#编程接口)
6. [配置系统](#配置系统)
7. [工作流管理](#工作流管理)
8. [报告系统](#报告系统)
9. [高级功能](#高级功能)
10. [故障排除](#故障排除)

## 简介

AI Development System 是一个全面的自动化代码分析和改进系统，支持多种编程语言和项目类型。本手册将帮助您快速上手并充分利用系统的各项功能。

### 核心功能

- **静态代码分析**: 检测代码质量问题、安全漏洞、性能问题
- **自动化测试**: 集成多种测试框架，自动执行测试并生成覆盖率报告
- **构建集成**: 支持主流构建系统，自动验证构建过程
- **智能修复**: 自动修复常见代码问题
- **报告生成**: 生成多格式的详细分析报告
- **CI/CD集成**: 与主流CI/CD平台无缝集成

## 安装与配置

### 系统要求

- Python 3.7 或更高版本
- 2GB 以上可用内存
- 1GB 以上可用磁盘空间

### 安装步骤

1. **下载系统**
   ```bash
   git clone https://github.com/your-repo/PortMaster-AI-Dev-System.git
   cd PortMaster-AI-Dev-System
   ```

2. **安装依赖**
   ```bash
   pip install -r requirements.txt
   ```

3. **验证安装**
   ```bash
   python main.py --help
   ```

### 初次配置

1. **创建配置文件**
   ```bash
   python main.py config init
   ```

2. **验证配置**
   ```bash
   python main.py config validate
   ```

3. **查看配置**
   ```bash
   python main.py config show
   ```

## 快速开始

### 第一次使用

1. **分析您的项目**
   ```bash
   python main.py analyze /path/to/your/project
   ```

2. **查看分析结果**
   分析完成后，系统会在 `output/reports/` 目录下生成HTML报告。

3. **运行测试**
   ```bash
   python main.py test /path/to/your/project --coverage
   ```

4. **修复发现的问题**
   ```bash
   python main.py fix /path/to/your/project --auto-fix
   ```

### 典型工作流程

```bash
# 1. 分析项目
python main.py analyze ./my-project

# 2. 查看HTML报告
# 打开 output/reports/index.html

# 3. 运行测试
python main.py test ./my-project --coverage

# 4. 自动修复问题
python main.py fix ./my-project --auto-fix

# 5. 重新验证
python main.py analyze ./my-project
```

## 命令行使用

### 基本语法

```bash
python main.py [GLOBAL_OPTIONS] COMMAND [COMMAND_OPTIONS] PROJECT_PATH
```

### 全局选项

- `--config, -c PATH`: 指定配置文件
- `--output, -o PATH`: 指定输出目录（默认：output）
- `--log-level LEVEL`: 设置日志级别（DEBUG, INFO, WARNING, ERROR, CRITICAL）
- `--debug, -d`: 启用调试模式

### 主要命令

#### 1. analyze - 完整分析

```bash
python main.py analyze [OPTIONS] PROJECT_PATH
```

选项：
- `--skip-build`: 跳过构建验证
- `--skip-tests`: 跳过测试执行
- `--skip-coverage`: 跳过覆盖率分析

示例：
```bash
# 完整分析
python main.py analyze ./my-project

# 跳过构建，只分析代码质量
python main.py analyze ./my-project --skip-build
```

#### 2. build - 构建项目

```bash
python main.py build [OPTIONS] PROJECT_PATH
```

选项：
- `--type {debug,release,clean}`: 构建类型（默认：debug）

示例：
```bash
# Debug构建
python main.py build ./my-project --type debug

# Release构建
python main.py build ./my-project --type release

# 清理构建
python main.py build ./my-project --type clean
```

#### 3. test - 运行测试

```bash
python main.py test [OPTIONS] PROJECT_PATH
```

选项：
- `--coverage`: 收集覆盖率信息
- `--framework FRAMEWORK`: 指定测试框架

示例：
```bash
# 运行测试
python main.py test ./my-project

# 运行测试并收集覆盖率
python main.py test ./my-project --coverage

# 使用指定测试框架
python main.py test ./my-project --framework pytest
```

#### 4. fix - 修复问题

```bash
python main.py fix [OPTIONS] PROJECT_PATH
```

选项：
- `--auto-fix`: 自动应用修复
- `--backup`: 修复前创建备份（默认启用）
- `--issue-types TYPES`: 指定要修复的问题类型

示例：
```bash
# 分析问题（不自动修复）
python main.py fix ./my-project

# 自动修复所有问题
python main.py fix ./my-project --auto-fix

# 只修复语法错误
python main.py fix ./my-project --auto-fix --issue-types syntax_error
```

#### 5. report - 生成报告

```bash
python main.py report [OPTIONS] PROJECT_PATH
```

选项：
- `--format {json,markdown,html}`: 报告格式（默认：html）
- `--template TEMPLATE`: 自定义报告模板
- `--data-file PATH`: 使用指定的分析数据文件

示例：
```bash
# 生成HTML报告
python main.py report ./my-project --format html

# 生成Markdown报告
python main.py report ./my-project --format markdown

# 使用已有数据生成报告
python main.py report ./my-project --data-file analysis-data.json
```

#### 6. workflow - 运行自定义工作流

```bash
python main.py workflow [OPTIONS] PROJECT_PATH
```

选项：
- `--workflow-file PATH`: 工作流配置文件
- `--workflow-name NAME`: 预定义工作流名称

示例：
```bash
# 运行自定义工作流
python main.py workflow ./my-project --workflow-file custom-workflow.yaml

# 运行预定义工作流
python main.py workflow ./my-project --workflow-name quick-analysis
```

#### 7. config - 配置管理

```bash
python main.py config ACTION [OPTIONS]
```

操作：
- `show`: 显示当前配置
- `validate`: 验证配置
- `init`: 初始化配置文件

示例：
```bash
# 显示配置
python main.py config show

# 验证配置
python main.py config validate

# 初始化配置文件
python main.py config init --force
```

## 编程接口

### 基本使用

```python
from ai_dev_system import MasterController, ConfigManager
from pathlib import Path

# 创建配置管理器
config = ConfigManager()

# 创建主控制器
controller = MasterController(Path("/path/to/project"), config)

# 运行完整分析
results = controller.run_full_workflow()

# 处理结果
if results['build']['success']:
    print("构建成功")
else:
    print("构建失败")

print(f"测试通过率: {results['test']['passed_tests'] / results['test']['total_tests'] * 100:.1f}%")
```

### 自定义配置

```python
# 加载配置文件
config = ConfigManager("/path/to/config.yaml", project_path)

# 修改配置
config.set('test.coverage_threshold', 90.0)
config.set('reporting.output_formats', ['html', 'json'])

# 更新配置
config.update({
    'build': {
        'timeout': 600,
        'parallel_jobs': 4
    }
})

# 保存配置
config.save_config()
```

### 使用单个组件

```python
from ai_dev_system import StaticAnalyzer, TestIntegrator

# 静态分析
analyzer = StaticAnalyzer(project_path)
analysis_result = analyzer.analyze()

# 测试集成
test_integrator = TestIntegrator(project_path)
test_result = test_integrator.execute_tests(include_coverage=True)
```

### 工作流管理

```python
from ai_dev_system import WorkflowRunner

# 创建工作流运行器
runner = WorkflowRunner(project_path, config)

# 加载工作流
workflow = runner.load_workflow("custom-workflow.yaml")

# 运行工作流
results = runner.run_workflow(workflow)

# 检查结果
for step_result in results:
    print(f"步骤 {step_result['name']}: {'成功' if step_result['success'] else '失败'}")
```

## 配置系统

### 配置文件结构

系统支持YAML和JSON格式的配置文件。基本结构如下：

```yaml
# 系统设置
system:
  name: "AI Development System"
  version: "1.0.0"
  debug_mode: false
  log_level: "INFO"

# 项目设置
project:
  type: python  # 项目类型
  language: python  # 编程语言
  build_system: setuptools  # 构建系统
  test_framework: pytest  # 测试框架
  source_directories:  # 源代码目录
    - "src/"
    - "lib/"
  test_directories:  # 测试目录
    - "tests/"
    - "test/"

# 构建配置
build:
  enabled: true
  timeout: 300  # 超时时间（秒）
  parallel_jobs: 1  # 并行任务数
  artifacts:  # 构建产物
    - "dist/"
    - "build/"

# 测试配置
test:
  enabled: true
  timeout: 600
  coverage_threshold: 80.0  # 覆盖率阈值
  collect_coverage: true
  test_patterns:  # 测试文件模式
    python:
      - "test_*.py"
      - "*_test.py"
    javascript:
      - "*.test.js"
      - "*.test.ts"

# 静态分析配置
static_analysis:
  enabled: true
  timeout: 300
  tools:  # 分析工具
    python:
      - "pylint"
      - "flake8"
    javascript:
      - "eslint"
  complexity_threshold: 10  # 复杂度阈值
  security_scan: true  # 安全扫描

# 报告配置
reporting:
  enabled: true
  output_formats:  # 输出格式
    - "html"
    - "json"
    - "markdown"
  output_directory: "reports"
  include_charts: true  # 包含图表
  dashboard: true  # 生成仪表板
```

### 环境变量

配置文件支持环境变量替换：

```yaml
# 使用环境变量
database:
  url: "${DATABASE_URL}"
  username: "${DB_USER:admin}"  # 默认值
  password: "${DB_PASSWORD}"

# 文件路径
output:
  directory: "${HOME}/ai-dev-reports"
```

### 项目类型检测

系统可以自动检测项目类型和配置：

```python
from ai_dev_system import ConfigManager

config = ConfigManager(project_path=Path("/path/to/project"))

# 获取检测到的项目配置
project_config = config.get_project_config()
print(f"检测到的语言: {project_config['language']}")
print(f"检测到的构建系统: {project_config['build_system']}")
print(f"检测到的测试框架: {project_config['test_framework']}")
```

## 工作流管理

### 工作流配置

工作流使用YAML格式定义，支持复杂的任务依赖和条件执行：

```yaml
name: "完整分析工作流"
description: "执行完整的项目分析流程"

variables:
  timeout: 300
  parallel: true

steps:
  - name: "环境准备"
    type: "custom"
    command: "pip install -r requirements.txt"
    timeout: 120

  - name: "代码格式化"
    type: "custom"
    command: "black . && isort ."
    dependencies: ["环境准备"]

  - name: "静态分析"
    type: "analyze"
    dependencies: ["代码格式化"]
    timeout: "${timeout}"

  - name: "构建项目"
    type: "build"
    dependencies: ["静态分析"]
    config:
      type: "debug"
      parallel_jobs: 2

  - name: "运行测试"
    type: "test"
    dependencies: ["构建项目"]
    config:
      coverage: true
      framework: "pytest"

  - name: "性能分析"
    type: "analyze"
    dependencies: ["构建项目"]
    config:
      analyzer: "performance"

  - name: "生成报告"
    type: "report"
    dependencies: ["运行测试", "性能分析"]
    config:
      formats: ["html", "json"]
      include_charts: true

  - name: "发送通知"
    type: "custom"
    command: "send-notification.sh"
    dependencies: ["生成报告"]
    condition: "${step.生成报告.success}"
```

### 工作流步骤类型

- `analyze`: 静态分析
- `build`: 项目构建
- `test`: 测试执行
- `report`: 报告生成
- `fix`: 问题修复
- `custom`: 自定义命令

### 工作流执行

```python
from ai_dev_system import WorkflowRunner

runner = WorkflowRunner(project_path, config)

# 运行预定义工作流
results = runner.run_workflow("quick-analysis")

# 运行自定义工作流文件
results = runner.run_workflow_file("custom-workflow.yaml")

# 运行内联工作流
workflow = {
    "name": "快速测试",
    "steps": [
        {"name": "测试", "type": "test", "config": {"coverage": False}}
    ]
}
results = runner.run_workflow_dict(workflow)
```

## 报告系统

### 报告类型

系统支持多种报告格式：

1. **HTML报告**: 交互式报告，包含图表和详细信息
2. **JSON报告**: 结构化数据，便于程序处理
3. **Markdown报告**: 文本格式，便于版本控制

### 报告内容

报告包含以下主要部分：

- **概览**: 项目基本信息和分析摘要
- **构建结果**: 构建状态、错误和警告信息
- **测试结果**: 测试通过率、覆盖率详情
- **代码质量**: 静态分析结果、问题列表
- **性能分析**: 性能瓶颈识别和优化建议
- **趋势分析**: 历史数据对比和趋势图表

### 自定义报告模板

```python
from ai_dev_system import ReportGenerator

generator = ReportGenerator(output_dir, config)

# 使用自定义模板
report_path = generator.generate_report(
    analysis_data,
    format='html',
    template='custom-template.html'
)

# 使用Jinja2模板
from jinja2 import Template

template = Template("""
<!DOCTYPE html>
<html>
<head>
    <title>{{ project_name }} - 分析报告</title>
</head>
<body>
    <h1>{{ project_name }}</h1>
    <p>生成时间: {{ timestamp }}</p>

    <h2>构建状态</h2>
    <p>{{ '成功' if build_success else '失败' }}</p>

    <h2>测试结果</h2>
    <p>通过: {{ test_passed }} / {{ test_total }}</p>
</body>
</html>
""")

report_html = template.render(
    project_name=analysis_data['project']['name'],
    timestamp=analysis_data['timestamp'],
    build_success=analysis_data['build']['success'],
    test_passed=analysis_data['test']['passed_tests'],
    test_total=analysis_data['test']['total_tests']
)
```

### 报告数据结构

```python
# 典型的分析结果数据结构
results = {
    'project': {
        'name': 'MyProject',
        'path': '/path/to/project',
        'type': 'python'
    },
    'timestamp': '2024-01-01T12:00:00Z',
    'build': {
        'success': True,
        'duration': 120.5,
        'warnings': 3,
        'errors': 0,
        'artifacts': ['dist/myapp-1.0.0.tar.gz']
    },
    'test': {
        'total_tests': 150,
        'passed_tests': 145,
        'failed_tests': 3,
        'skipped_tests': 2,
        'coverage': {
            'line_coverage': 85.2,
            'branch_coverage': 78.9,
            'function_coverage': 92.1
        }
    },
    'static_analysis': {
        'issues': [
            {
                'type': 'complexity',
                'severity': 'medium',
                'file': 'src/module.py',
                'line': 45,
                'message': '函数复杂度过高',
                'suggestion': '考虑拆分函数'
            }
        ]
    },
    'performance': {
        'issues': [],
        'metrics': {
            'cpu_time': 2.3,
            'memory_usage': '256MB'
        }
    }
}
```

## 高级功能

### 插件系统

```python
from ai_dev_system import MasterController

# 创建自定义分析器插件
class CustomAnalyzer:
    def __init__(self):
        self.name = "custom_analyzer"

    def analyze(self, project_path):
        # 实现自定义分析逻辑
        issues = []
        # ... 分析代码 ...
        return {
            'analyzer': self.name,
            'issues': issues,
            'metrics': {}
        }

# 注册插件
controller = MasterController(project_path, config)
controller.register_analyzer('custom', CustomAnalyzer())

# 运行包含插件的分析
results = controller.run_full_workflow()
```

### 批量处理

```python
from ai_dev_system import analyze_project
from pathlib import Path
import concurrent.futures

def analyze_single_project(project_path):
    """分析单个项目"""
    try:
        return {
            'path': project_path,
            'result': analyze_project(project_path)
        }
    except Exception as e:
        return {
            'path': project_path,
            'error': str(e)
        }

# 批量分析多个项目
projects = [
    Path("/path/to/project1"),
    Path("/path/to/project2"),
    Path("/path/to/project3")
]

# 并行处理
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    results = list(executor.map(analyze_single_project, projects))

# 处理结果
for result in results:
    if 'error' in result:
        print(f"项目 {result['path']} 分析失败: {result['error']}")
    else:
        print(f"项目 {result['path']} 分析完成")
```

### API集成

```python
from ai_dev_system import MasterController, ConfigManager
from flask import Flask, jsonify, request

app = Flask(__name__)

@app.route('/api/analyze', methods=['POST'])
def api_analyze():
    """提供REST API接口"""
    data = request.json
    project_path = Path(data['project_path'])

    # 创建配置
    config = ConfigManager()
    if 'config' in data:
        config.update(data['config'])

    # 运行分析
    controller = MasterController(project_path, config)
    results = controller.run_full_workflow()

    return jsonify(results)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
```

### 数据导出和导入

```python
from ai_dev_system import MasterController, ConfigManager
import json

# 运行分析并导出数据
controller = MasterController(project_path, config)
results = controller.run_full_workflow()

# 导出到JSON文件
with open('analysis-results.json', 'w') as f:
    json.dump(results, f, indent=2, default=str)

# 从文件导入分析结果
with open('analysis-results.json', 'r') as f:
    loaded_results = json.load(f)

# 生成报告
from ai_dev_system import ReportGenerator
generator = ReportGenerator('output')
report_path = generator.generate_report(loaded_results)
```

## 故障排除

### 常见问题

#### 1. 安装问题

**问题**: `ImportError: No module named 'yaml'`
**解决**:
```bash
pip install pyyaml
```

**问题**: 权限错误
**解决**:
```bash
pip install --user -r requirements.txt
```

#### 2. 配置问题

**问题**: 配置文件验证失败
**解决**:
```bash
# 检查配置语法
python main.py config validate

# 重新生成配置文件
python main.py config init --force
```

**问题**: 环境变量未生效
**解决**: 确保环境变量在运行前设置，或在配置文件中提供默认值。

#### 3. 构建问题

**问题**: 构建工具未找到
**解决**: 安装相应的构建工具：
```bash
# CMake
pip install cmake

# Maven
sudo apt-get install maven  # Ubuntu
brew install maven          # macOS

# Node.js/npm
# 从 https://nodejs.org 下载安装
```

**问题**: 构建超时
**解决**: 增加超时时间：
```yaml
build:
  timeout: 600  # 增加到10分钟
```

#### 4. 测试问题

**问题**: 测试框架未找到
**解决**: 检查项目是否包含测试文件，或手动指定测试框架：
```bash
python main.py test ./project --framework pytest
```

**问题**: 覆盖率收集失败
**解决**: 安装覆盖率工具：
```bash
pip install pytest-cov  # Python
npm install --save-dev jest  # JavaScript
```

#### 5. 报告问题

**问题**: HTML报告无法打开
**解决**: 检查文件权限和路径，确保输出目录存在。

**问题**: 图表不显示
**解决**: 安装图表依赖：
```bash
pip install matplotlib plotly
```

### 日志分析

启用详细日志来诊断问题：

```bash
python main.py analyze ./project --log-level DEBUG --debug
```

查看日志文件：
```bash
tail -f ai_dev_system.log
```

### 性能优化

1. **增加并行度**：
```yaml
build:
  parallel_jobs: 4
test:
  parallel_jobs: 4
```

2. **缓存结果**：
```yaml
performance:
  cache_results: true
  cache_directory: "cache"
```

3. **跳过不必要的步骤**：
```bash
python main.py analyze ./project --skip-coverage
```

### 获取帮助

如果遇到问题，可以：

1. 查看详细文档：`docs/`目录
2. 提交问题：[GitHub Issues](https://github.com/your-repo/ai-dev-system/issues)
3. 联系支持：support@example.com

---

本手册涵盖了AI Development System的主要功能和使用方法。更多详细信息请参考API文档和示例代码。