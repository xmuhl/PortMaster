# 自动修复系统归档目录

## 📁 目录结构说明

### `deprecated_tools/` - 已弃用的开发工具
这些是系统开发过程中使用的早期版本工具，已被更新的系统替代：

- `error_diagnostic.py` - 早期错误诊断工具
- `auto_fix.py` - 第一版自动修复工具
- `iterative_fix.py` - 迭代修复原型
- `verify_fixes.py` - 修复验证工具

**保留原因**：用于开发历史追溯和技术演进分析。

### `screenshots_archive/` - 历史错误窗口截图
包含系统运行过程中捕获的所有错误窗口截图：

- 文件命名格式：`error_ERROR_TYPE_YYYYMMDD_HHMMSS.png`
- 主要错误类型：`RUNTIME_ERROR`、`ASSERTION_FAILED`等
- 用途：错误分析和系统训练数据

**统计信息**：
- 总截图数量：24个
- 时间范围：2025-09-30 23:12 - 23:19
- 主要错误类型：Microsoft Visual C++ Runtime Library

### `logs_archive/` - 历史运行日志
包含系统运行产生的详细日志文件：

- 文件命名格式：`quick_test_YYYYMMDD_HHMMSS.log`
- 内容：错误检测、修复过程、系统状态等详细信息
- 用途：问题排查、性能分析、系统优化

### `development_versions/` - 开发版本备份
（预留目录）用于存放重要的开发版本备份。

## 🔍 归档原则

### 文件归档标准
1. **开发工具归档**：功能被完全替代的工具
2. **数据文件归档**：超过30天的历史数据
3. **日志文件归档**：已完成分析周期的日志
4. **版本备份**：重要的里程碑版本

### 保留期限
- **开发工具**：永久保留（历史价值）
- **截图文件**：保留6个月
- **日志文件**：保留3个月
- **版本备份**：永久保留

## 📊 归档统计

### 文件统计
- **开发工具**：4个文件
- **截图文件**：24个文件
- **日志文件**：1个文件
- **总计**：29个文件

### 空间占用
- `deprecated_tools/`：约50KB
- `screenshots_archive/`：约350KB
- `logs_archive/`：约2KB
- **总计**：约402KB

## 🔄 访问指南

### 查看历史截图
```bash
# 查看所有截图
ls archive/screenshots_archive/

# 按日期查看截图
ls archive/screenshots_archive/*20250930*

# 查看特定类型错误截图
ls archive/screenshots_archive/*RUNTIME_ERROR*
```

### 查看历史日志
```bash
# 查看所有日志
ls archive/logs_archive/

# 查看最新日志
cat archive/logs_archive/*.log

# 搜索特定错误
grep "ERROR" archive/logs_archive/*.log
```

### 使用废弃工具
```bash
# 运行早期诊断工具
python archive/deprecated_tools/error_diagnostic.py

# 查看工具帮助
python archive/deprecated_tools/auto_fix.py --help
```

## 🗑️ 清理策略

### 自动清理脚本
可以创建定期清理脚本：

```bash
#!/bin/bash
# cleanup_archive.sh

# 清理超过6个月的截图
find archive/screenshots_archive -name "*.png" -mtime +180 -delete

# 清理超过3个月的日志
find archive/logs_archive -name "*.log" -mtime +90 -delete

# 压缩旧的截图文件
find archive/screenshots_archive -name "*.png" -mtime +30 -exec gzip {} \;
```

### 手动清理建议
1. **每月检查**：检查目录大小和文件数量
2. **季度清理**：清理过期的日志和截图
3. **年度备份**：将重要归档备份到外部存储

## 📝 维护记录

- **2025-09-30**：初始归档建立，移动开发工具和历史数据
- **待更新**：后续维护记录

---

**注意**：此目录下的文件主要用于历史追溯和分析，不建议在生产环境中直接使用。如需使用相关功能，请使用项目根目录下的最新版本。