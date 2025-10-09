# AI Development System - 快速开始

## 🚀 5分钟快速上手

### 1. 安装系统

```bash
# 克隆项目
git clone https://github.com/your-repo/PortMaster-AI-Dev-System.git
cd PortMaster-AI-Dev-System

# 安装依赖
pip install -r requirements.txt

# 验证安装
python main.py --help
```

### 2. 分析您的第一个项目

```bash
# 分析您的项目
python main.py analyze /path/to/your/project

# 查看HTML报告
open output/reports/index.html
```

### 3. 命令行快速参考

```bash
# 完整分析
python main.py analyze ./my-project

# 只运行测试
python main.py test ./my-project --coverage

# 自动修复问题
python main.py fix ./my-project --auto-fix

# 生成HTML报告
python main.py report ./my-project --format html
```

## 💻 编程接口示例

### 基本使用

```python
from ai_dev_system import MasterController, ConfigManager
from pathlib import Path

# 创建配置
config = ConfigManager()

# 分析项目
controller = MasterController(Path("/path/to/project"), config)
results = controller.run_full_workflow()

# 查看结果
print(f"构建成功: {results['build']['success']}")
print(f"测试通过率: {results['test']['passed_tests']}/{results['test']['total_tests']}")
```

### 高级用法

```python
# 自定义配置
config = ConfigManager()
config.set('test.coverage_threshold', 90.0)
config.set('reporting.output_formats', ['html', 'json'])

# 运行分析
controller = MasterController(project_path, config)
results = controller.run_full_workflow()

# 自动修复
if results['static_analysis']['issues']:
    from ai_dev_system import FixController
    fix_controller = FixController(project_path, config)
    fix_results = fix_controller.apply_fixes(results['static_analysis']['issues'])
    print(f"修复了 {fix_results['successful_fixes']} 个问题")
```

## 📋 支持的项目类型

| 语言 | 构建系统 | 测试框架 | 状态 |
|------|----------|----------|------|
| Python | setuptools, poetry | pytest, unittest | ✅ 完全支持 |
| Java | Maven, Gradle | JUnit, TestNG | ✅ 完全支持 |
| JavaScript | npm, yarn | Jest, Mocha | ✅ 完全支持 |
| C++ | CMake, Make | GoogleTest | ✅ 完全支持 |
| 更多... | ... | ... | 🚧 开发中 |

## 📖 更多资源

- 📚 [用户手册](USER_MANUAL.md) - 详细的使用指南
- 🔧 [API参考](API_REFERENCE.md) - 完整的API文档
- 🚀 [部署指南](DEPLOYMENT_GUIDE.md) - 生产环境部署
- 💡 [使用示例](EXAMPLES.md) - 丰富的使用示例
- ❓ [故障排除](docs/TROUBLESHOOTING.md) - 常见问题解决

## 🆘 需要帮助？

- 🐛 [报告问题](https://github.com/your-repo/ai-dev-system/issues)
- 💬 [讨论区](https://github.com/your-repo/ai-dev-system/discussions)
- 📧 [邮件支持](mailto:support@example.com)

---

**开始您的AI驱动开发之旅！** 🎉