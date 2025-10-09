# PortMaster 自动化系统使用指南

## 📋 目录

- [系统概述](#系统概述)
- [核心组件](#核心组件)
- [快速开始](#快速开始)
- [工作流模式](#工作流模式)
- [CI/CD集成](#cicd集成)
- [高级用法](#高级用法)
- [故障排除](#故障排除)

---

## 系统概述

PortMaster自动化系统是一个完整的AI驱动开发-测试-修复循环系统，集成了以下功能：

- ✅ **自动构建**: 智能编译管理，支持清理构建和增量构建
- ✅ **自动测试**: 多层次测试（单元→集成→性能→压力）
- ✅ **自动修复**: 基于错误模式的智能修复策略
- ✅ **静态分析**: Cppcheck代码质量检查
- ✅ **覆盖率分析**: OpenCppCoverage代码覆盖率统计
- ✅ **回归测试**: 基线对比和性能回归检测
- ✅ **工作流编排**: 灵活的步骤化自动化流程
- ✅ **CI/CD集成**: GitHub Actions、Jenkins等支持

---

## 核心组件

### 1. 主控制器 (`autonomous_master_controller.py`)

**核心类**: `AutonomousMasterController`

集成所有子模块的主控制器，提供统一的自动化接口。

```python
from autonomous_master_controller import AutonomousMasterController

# 创建控制器
controller = AutonomousMasterController(".")

# 执行自动修复
result = controller.auto_fix_and_rebuild(max_iterations=3)

# 执行完整测试
test_result = controller.run_complete_test_cycle()
```

### 2. 工作流执行器 (`run_automated_workflow.py`)

命令行工具，提供预定义的工作流模式。

**使用方法**:

```bash
# 综合测试流程（静态分析+构建+全部测试+回归）
python run_automated_workflow.py comprehensive

# 快速测试流程（构建+单元测试）
python run_automated_workflow.py quick

# 回归测试流程
python run_automated_workflow.py regression --baseline v1.0.0

# 自动修复流程
python run_automated_workflow.py auto-fix --max-iterations 5
```

### 3. CI/CD集成器 (`ci_cd_integration.py`)

专为CI/CD系统设计的集成工具。

**使用方法**:

```bash
# Pull Request验证
python ci_cd_integration.py pr

# 夜间完整构建
python ci_cd_integration.py nightly

# 发布验证
python ci_cd_integration.py release --version v1.0.0

# GitHub Actions集成
python ci_cd_integration.py pr --github-actions
```

### 4. 使用示例 (`automation_examples.py`)

交互式示例演示，展示各种自动化场景。

```bash
python automation_examples.py
```

---

## 快速开始

### 前置要求

1. **开发环境**:
   - Visual Studio 2022 (Community/Professional/Enterprise)
   - Python 3.7+

2. **可选工具** (用于完整功能):
   - Cppcheck (静态分析)
   - OpenCppCoverage (覆盖率分析)

3. **Python依赖**:
   ```bash
   pip install -r requirements.txt  # 如果有的话
   ```

### 第一次使用

**步骤1: 快速验证**

```bash
# 执行快速测试流程
python run_automated_workflow.py quick
```

**步骤2: 查看示例**

```bash
# 运行交互式示例
python automation_examples.py
```

**步骤3: 执行综合测试**

```bash
# 执行完整的综合测试流程
python run_automated_workflow.py comprehensive
```

---

## 工作流模式

### 1. 综合测试流程 (Comprehensive)

**适用场景**: 提交前的完整验证

**执行步骤**:
1. 静态代码分析 (Cppcheck)
2. 清理构建 (Clean Build)
3. 自动修复 (如果构建失败)
4. 单元测试 (Transport + Protocol)
5. 集成测试 (多层协议栈 + 文件传输)
6. 性能测试 (吞吐量 + 延迟 + 窗口影响)
7. 压力测试 (高负载 + 长时间运行 + 并发)
8. 回归测试 (基线对比)

**命令**:
```bash
python run_automated_workflow.py comprehensive
```

**输出**:
- JSON报告: `automation_reports/workflow_comprehensive_<timestamp>.json`
- Markdown报告 (如有回归): `regression_report.md`

### 2. 快速测试流程 (Quick)

**适用场景**: 开发过程中的快速验证

**执行步骤**:
1. 快速构建 (增量构建)
2. 单元测试

**命令**:
```bash
python run_automated_workflow.py quick
```

**优点**: 快速反馈，适合频繁执行

### 3. 回归测试流程 (Regression)

**适用场景**: 版本对比和质量监控

**执行步骤**:
1. 构建验证
2. 全部测试
3. 回归对比 (与指定基线版本)

**命令**:
```bash
# 与v1.0.0基线对比
python run_automated_workflow.py regression --baseline v1.0.0
```

**输出**:
- 回归报告: `regression_report.md`
- 检测内容:
  - 功能回归 (测试状态变化)
  - 性能回归 (性能指标下降>10%)
  - 时间回归 (执行时间超标>50%)

### 4. 自动修复流程 (Auto-Fix)

**适用场景**: 构建失败时的自动诊断和修复

**执行步骤**:
1. 尝试构建
2. 如果失败，分析错误模式
3. 应用修复策略
4. 重新构建
5. 重复步骤2-4，最多N次迭代

**命令**:
```bash
# 最多尝试5次修复
python run_automated_workflow.py auto-fix --max-iterations 5
```

**支持的错误修复**:
- LNK1104 (进程占用) → 自动关闭进程
- LNK2019 (未解析符号) → 符号分析和建议
- C2065 (未声明标识符) → 头文件检查
- C4996 (不安全函数) → 替换为安全版本

---

## CI/CD集成

### GitHub Actions集成

**创建工作流文件**: `.github/workflows/ci.yml`

```yaml
name: CI

on:
  pull_request:
    branches: [ main, develop ]
  push:
    branches: [ main ]

jobs:
  pr-validation:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.9'

      - name: Run PR Validation
        run: python ci_cd_integration.py pr --github-actions

      - name: Upload Test Reports
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: test-reports
          path: ci_artifacts/

  nightly-build:
    runs-on: windows-latest
    if: github.event_name == 'schedule'
    steps:
      - uses: actions/checkout@v3

      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.9'

      - name: Run Nightly Build
        run: python ci_cd_integration.py nightly

      - name: Upload Build Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: nightly-build
          path: |
            ci_artifacts/
            automation_reports/
```

### Jenkins集成

**Jenkinsfile**:

```groovy
pipeline {
    agent any

    stages {
        stage('PR Validation') {
            when { changeRequest() }
            steps {
                bat 'python ci_cd_integration.py pr'
            }
        }

        stage('Nightly Build') {
            when {
                allOf {
                    branch 'main'
                    triggeredBy 'TimerTrigger'
                }
            }
            steps {
                bat 'python ci_cd_integration.py nightly'
            }
        }

        stage('Release Validation') {
            when {
                buildingTag()
            }
            steps {
                script {
                    def version = env.TAG_NAME
                    bat "python ci_cd_integration.py release --version ${version}"
                }
            }
        }
    }

    post {
        always {
            archiveArtifacts artifacts: 'ci_artifacts/**/*', allowEmptyArchive: true
            junit 'ci_artifacts/**/*.xml'
        }
    }
}
```

---

## 高级用法

### 1. 自定义工作流

**创建自定义步骤**:

```python
from autonomous_master_controller import AutonomousMasterController, AutomationWorkflow

controller = AutonomousMasterController(".")

# 定义自定义步骤
steps = [
    {
        "name": "预处理",
        "type": "custom",
        "handler": lambda ctx: {"success": True, "message": "预处理完成"},
        "enabled": True
    },
    {
        "name": "构建",
        "type": "build",
        "config": {"build_script": "autobuild_x86_debug.bat"},
        "enabled": True
    },
    {
        "name": "条件测试",
        "type": "test",
        "condition": lambda ctx: ctx.get('build', {}).get('success', False),
        "config": {"mode": "all"},
        "enabled": True
    }
]

workflow = AutomationWorkflow("自定义流程", steps)
result = controller.execute_workflow(workflow)
```

### 2. 自定义修复策略

**注册自定义错误修复**:

```python
from autonomous_master_controller import AutonomousMasterController

controller = AutonomousMasterController(".")

# 自定义修复函数
def custom_fix(error_info: dict) -> dict:
    error_code = error_info.get('code', '')

    if error_code == 'C2065':  # 未声明标识符
        return {
            "strategy": "add_include",
            "description": f"添加缺失的头文件",
            "actions": [
                f"检查符号: {error_info.get('symbol')}",
                "添加必要的 #include"
            ]
        }

    return {"strategy": "unknown"}

# 注册策略
controller.fix_library.register_custom_strategy("C2065", custom_fix)

# 使用自定义策略修复
result = controller.auto_fix_and_rebuild(max_iterations=3)
```

### 3. 集成外部工具

**添加自定义分析工具**:

```python
from autonomous_master_controller import AutonomousMasterController

controller = AutonomousMasterController(".")

# 自定义分析步骤
def custom_analysis(ctx):
    # 调用外部工具
    import subprocess
    result = subprocess.run(
        ["custom_tool.exe", "--analyze", "."],
        capture_output=True,
        text=True
    )

    return {
        "success": result.returncode == 0,
        "output": result.stdout,
        "issues_found": parse_tool_output(result.stdout)
    }

# 添加到工作流
steps = [
    {
        "name": "自定义分析",
        "type": "custom",
        "handler": custom_analysis,
        "enabled": True
    },
    # ... 其他步骤
]
```

---

## 故障排除

### 常见问题

#### 1. 构建失败: LNK1104错误

**问题**: `LINK : fatal error LNK1104: 无法打开文件'PortMaster.exe'`

**解决方案**:
```bash
# 使用自动修复
python run_automated_workflow.py auto-fix

# 或手动关闭进程
taskkill /F /IM PortMaster.exe
```

#### 2. Python模块导入错误

**问题**: `ModuleNotFoundError: No module named 'xxx'`

**解决方案**:
```bash
# 确保所有脚本在同一目录
cd /path/to/PortMaster

# 或设置PYTHONPATH
set PYTHONPATH=%PYTHONPATH%;C:\Users\huangl\Desktop\PortMaster
```

#### 3. 静态分析工具未找到

**问题**: `Cppcheck not found`

**解决方案**:
```bash
# 安装Cppcheck
choco install cppcheck

# 或跳过静态分析步骤
# 在工作流中设置 enabled: False
```

#### 4. 覆盖率分析失败

**问题**: `OpenCppCoverage not available`

**解决方案**:
```bash
# 安装OpenCppCoverage
choco install opencppcoverage

# 或使用Visual Studio自带的覆盖率工具
```

### 调试技巧

**1. 启用详细日志**:

```python
import logging
logging.basicConfig(level=logging.DEBUG)

controller = AutonomousMasterController(".")
# 现在会输出详细的调试信息
```

**2. 查看工作流上下文**:

```python
# 在自定义步骤中打印上下文
def debug_step(ctx):
    import json
    print(json.dumps(ctx, indent=2))
    return {"success": True}
```

**3. 单步执行工作流**:

```python
workflow = AutomationWorkflow("调试流程", steps)

# 逐步执行
for i, step in enumerate(workflow.steps):
    print(f"执行步骤 {i+1}: {step['name']}")
    result = workflow.execute_step(step, workflow.context)
    print(f"结果: {result}")

    if not result['success']:
        print(f"步骤失败，停止执行")
        break
```

---

## 性能优化建议

### 1. 增量构建

对于频繁的代码修改，使用增量构建而非完整清理构建：

```bash
# 快速模式（增量构建）
python run_automated_workflow.py quick

# 而不是
python run_automated_workflow.py comprehensive
```

### 2. 选择性测试

仅运行相关的测试套件：

```bash
# 仅单元测试
AutoTest.exe --unit-tests

# 仅集成测试
AutoTest.exe --integration

# 仅性能测试
AutoTest.exe --performance
```

### 3. 并行执行

在CI/CD中并行执行独立任务：

```yaml
# GitHub Actions并行作业
jobs:
  unit-tests:
    runs-on: windows-latest
    steps:
      - run: python run_automated_workflow.py quick

  integration-tests:
    runs-on: windows-latest
    steps:
      - run: AutoTest.exe --integration
```

---

## 最佳实践

### 1. 开发流程

**本地开发**:
```bash
# 1. 修改代码
# 2. 快速验证
python run_automated_workflow.py quick

# 3. 提交前综合检查
python run_automated_workflow.py comprehensive
```

**Pull Request**:
```bash
# PR验证
python ci_cd_integration.py pr
```

**发布前**:
```bash
# 回归测试
python run_automated_workflow.py regression --baseline v1.0.0

# 发布验证
python ci_cd_integration.py release --version v1.1.0
```

### 2. 测试策略

- **每次提交**: 单元测试 (快速)
- **每个PR**: 单元+集成测试 (中等)
- **每晚**: 全部测试+覆盖率+回归 (完整)
- **每次发布**: 全部测试+回归+性能基准 (彻底)

### 3. 错误处理

- **构建失败**: 使用自动修复，最多3次迭代
- **测试失败**: 查看详细报告，手动调试
- **回归检测**: 对比基线，分析差异原因
- **性能回退**: 使用性能基准测试定位瓶颈

---

## 附录

### A. 命令速查表

| 命令 | 用途 | 耗时 |
|------|------|------|
| `python run_automated_workflow.py quick` | 快速验证 | ~2分钟 |
| `python run_automated_workflow.py comprehensive` | 完整测试 | ~10分钟 |
| `python run_automated_workflow.py regression --baseline v1.0` | 回归测试 | ~8分钟 |
| `python run_automated_workflow.py auto-fix` | 自动修复 | ~5分钟 |
| `python ci_cd_integration.py pr` | PR验证 | ~5分钟 |
| `python ci_cd_integration.py nightly` | 夜间构建 | ~15分钟 |
| `python ci_cd_integration.py release --version v1.0` | 发布验证 | ~12分钟 |

### B. 文件结构

```
PortMaster/
├── autonomous_master_controller.py    # 主控制器
├── run_automated_workflow.py          # 工作流执行器
├── ci_cd_integration.py               # CI/CD集成器
├── automation_examples.py             # 使用示例
├── build_integration.py               # 构建集成
├── autotest_integration.py            # 测试集成
├── static_analyzer.py                 # 静态分析
├── coverage_analyzer.py               # 覆盖率分析
├── fix_strategies.py                  # 修复策略库
├── error_pattern_db.py                # 错误模式数据库
├── automation_reports/                # 自动化报告
│   ├── workflow_*.json
│   └── workflow_*.md
├── ci_artifacts/                      # CI/CD产物
│   ├── pr_validation.json
│   ├── nightly_build.json
│   └── release_validation_*.json
└── AutoTest/                          # 测试框架
    ├── baselines/                     # 回归基线
    ├── bin/Debug/AutoTest.exe         # 测试可执行文件
    └── *.h                            # 测试套件
```

### C. 支持的错误类型

**编译错误**:
- LNK1104, LNK2001, LNK2019 (链接错误)
- C2065, C2079, C2143 (语法错误)
- C4996, C4244, C4267 (警告)

**测试错误**:
- 断言失败
- 超时
- 内存泄漏
- 异常抛出

**回归类型**:
- 功能回归 (测试状态变化)
- 性能回归 (指标下降>10%)
- 时间回归 (执行时间>150%)

---

**文档版本**: 1.0
**更新时间**: 2025-10-02
**维护者**: PortMaster开发团队
