# AI Development System

一个全面的自动化AI驱动开发系统，支持代码分析、测试、报告和改进，适用于多种编程语言和项目类型。

[![Python Version](https://img.shields.io/badge/python-3.7%2B-blue.svg)](https://python.org)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-stable-brightgreen.svg)](https://github.com/your-repo/ai-dev-system)

## 🚀 特性

### 🔍 代码分析
- **多语言静态分析**: 支持Python、Java、C++、JavaScript等主流语言
- **代码质量检查**: 复杂度分析、重复代码检测、安全漏洞扫描
- **错误模式识别**: 20+种预定义错误模式，支持自定义模式
- **性能分析**: 性能瓶颈识别和优化建议

### 🛠️ 构建与测试集成
- **多构建系统支持**: Make、CMake、Maven、Gradle、npm、Visual Studio等
- **测试框架集成**: pytest、unittest、JUnit、Jest、Mocha、GoogleTest等
- **覆盖率分析**: 多种覆盖率格式支持，详细报告生成
- **CI/CD集成**: GitHub Actions、GitLab CI、Jenkins、Azure Pipelines等

### 📊 报告与可视化
- **多格式报告**: JSON、Markdown、HTML格式输出
- **交互式仪表板**: 实时项目状态监控
- **自定义模板**: 支持自定义报告模板
- **数据导出**: 支持分析数据导出和分享

### 🤖 智能修复
- **自动代码修复**: 常见问题的自动修复
- **备份与恢复**: 安全的修复操作，支持一键回滚
- **修复策略**: 多种修复策略，可配置修复规则
- **批量处理**: 支持批量问题修复

### ⚙️ 工作流自动化
- **自定义工作流**: 灵活的工作流定义和执行
- **依赖管理**: 支持任务依赖和条件执行
- **重试机制**: 失败任务自动重试
- **并行处理**: 支持并行任务执行

## 📦 安装

### 系统要求
- Python 3.7+
- 支持的操作系统: Windows、Linux、macOS

### 快速安装
```bash
# 克隆仓库
git clone https://github.com/your-repo/PortMaster-AI-Dev-System.git
cd PortMaster-AI-Dev-System

# 安装依赖
pip install -r requirements.txt

# 验证安装
python main.py --help
```

### 开发环境安装
```bash
# 创建虚拟环境
python -m venv venv
source venv/bin/activate  # Linux/macOS
# 或
venv\Scripts\activate  # Windows

# 安装开发依赖
pip install -r requirements-dev.txt

# 运行测试
pytest
```

## 🚀 快速开始

### 1. 基本使用

```bash
# 分析项目
python main.py analyze /path/to/your/project

# 运行测试
python main.py test /path/to/your/project --coverage

# 生成HTML报告
python main.py report /path/to/your/project --format html

# 自动修复问题
python main.py fix /path/to/your/project --auto-fix
```

### 2. 编程接口

```python
from ai_dev_system import MasterController, ConfigManager
from pathlib import Path

# 创建配置管理器
config = ConfigManager()
config.set('test.collect_coverage', True)
config.set('reporting.output_formats', ['html', 'json'])

# 创建主控制器
controller = MasterController(Path("/path/to/project"), config)

# 运行完整分析
results = controller.run_full_workflow()

# 查看结果
print(f"构建状态: {'成功' if results['build']['success'] else '失败'}")
print(f"测试通过: {results['test']['passed_tests']}")
print(f"代码覆盖率: {results['coverage']['line_coverage']:.1f}%")
```

### 3. 配置文件

创建项目配置文件 `ai_dev_config.yaml`:

```yaml
# 项目配置
project:
  type: python
  language: python
  build_system: setuptools
  test_framework: pytest

# 构建配置
build:
  enabled: true
  timeout: 300
  parallel_jobs: 1

# 测试配置
test:
  enabled: true
  coverage_threshold: 80.0
  collect_coverage: true

# 报告配置
reporting:
  output_formats:
    - html
    - json
  include_charts: true
```

## 📖 详细文档

### 核心概念

#### 1. 分析器 (Analyzers)
- **StaticAnalyzer**: 静态代码分析
- **CoverageAnalyzer**: 代码覆盖率分析
- **ErrorPatternAnalyzer**: 错误模式识别
- **PerformanceAnalyzer**: 性能分析

#### 2. 集成器 (Integrators)
- **BuildIntegrator**: 构建系统集成
- **TestIntegrator**: 测试框架集成
- **CICDIntegrator**: CI/CD平台集成

#### 3. 控制器 (Controllers)
- **MasterController**: 主控制器，协调所有组件
- **FixController**: 修复控制器，处理代码修复
- **WorkflowRunner**: 工作流运行器

### 配置系统

系统采用分层配置机制：
1. **默认配置**: 内置默认设置
2. **系统配置**: 全局系统配置
3. **项目配置**: 项目特定配置
4. **命令行参数**: 运行时参数覆盖

配置文件支持YAML和JSON格式，支持环境变量替换。

### 工作流系统

自定义工作流示例：

```yaml
name: "Custom Analysis Workflow"
description: "项目特定分析流程"

steps:
  - name: "代码格式化"
    type: "custom"
    command: "black ."

  - name: "静态分析"
    type: "analyze"
    dependencies: ["代码格式化"]

  - name: "构建项目"
    type: "build"
    dependencies: ["静态分析"]

  - name: "运行测试"
    type: "test"
    dependencies: ["构建项目"]

  - name: "生成报告"
    type: "report"
    dependencies: ["运行测试"]
```

## 🔧 高级功能

### 1. 自定义分析规则

```python
from ai_dev_system import StaticAnalyzer

analyzer = StaticAnalyzer(project_path)

# 添加自定义规则
custom_rule = {
    'id': 'custom_naming',
    'name': '自定义命名规则',
    'pattern': r'def\s+([a-z]+_[a-z]+)',
    'message': '函数名应使用驼峰命名法',
    'severity': 'medium'
}

analyzer.add_custom_rule(custom_rule)
```

### 2. 插件系统

```python
from ai_dev_system import MasterController

# 创建自定义插件
class CustomPlugin:
    def analyze(self, project_path):
        # 自定义分析逻辑
        return issues

# 注册插件
controller = MasterController(project_path, config)
controller.register_plugin('custom_analyzer', CustomPlugin())
```

### 3. 批量项目分析

```python
from ai_dev_system import analyze_project
from pathlib import Path

projects = [
    Path("/path/to/project1"),
    Path("/path/to/project2"),
    Path("/path/to/project3")
]

results = {}
for project in projects:
    results[project.name] = analyze_project(project)

# 生成汇总报告
# ...
```

## 🎯 支持的项目类型

| 语言 | 构建系统 | 测试框架 | 状态 |
|------|----------|----------|------|
| Python | setuptools, poetry, pip | pytest, unittest | ✅ 完全支持 |
| Java | Maven, Gradle | JUnit, TestNG | ✅ 完全支持 |
| C++ | CMake, Make | GoogleTest, Catch2 | ✅ 完全支持 |
| JavaScript | npm, yarn | Jest, Mocha | ✅ 完全支持 |
| TypeScript | npm, yarn | Jest, Mocha | ✅ 完全支持 |
| C# | MSBuild | NUnit, xUnit | ✅ 完全支持 |
| Go | go mod | go test | ✅ 完全支持 |
| Rust | Cargo | cargo test | ✅ 完全支持 |

## 🤝 贡献指南

我们欢迎各种形式的贡献！

### 开发环境设置

```bash
# Fork并克隆仓库
git clone https://github.com/your-username/PortMaster-AI-Dev-System.git
cd PortMaster-AI-Dev-System

# 创建开发分支
git checkout -b feature/your-feature-name

# 安装开发依赖
pip install -r requirements-dev.txt

# 运行测试
pytest

# 运行代码质量检查
flake8 ai_dev_system
mypy ai_dev_system
```

### 提交规范

- 使用清晰的提交信息
- 一个提交只做一件事
- 包含相应的测试
- 遵循代码风格规范

## 📄 许可证

本项目采用MIT许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 🆘 支持

- 📖 [完整文档](docs/)
- 🐛 [问题反馈](https://github.com/your-repo/ai-dev-system/issues)
- 💬 [讨论区](https://github.com/your-repo/ai-dev-system/discussions)
- 📧 [邮件支持](mailto:support@example.com)

## 🎉 致谢

感谢所有为这个项目做出贡献的开发者和用户！

---

**AI Development System** - 让代码分析变得简单而强大 🚀