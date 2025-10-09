# AI Development System - 使用示例

## 目录

1. [快速开始](#快速开始)
2. [基本用法示例](#基本用法示例)
3. [高级用法示例](#高级用法示例)
4. [集成示例](#集成示例)
5. [配置示例](#配置示例)
6. [工作流示例](#工作流示例)
7. [插件开发示例](#插件开发示例)

## 快速开始

### 1. 安装系统

```bash
# 克隆仓库
git clone https://github.com/your-repo/PortMaster-AI-Dev-System.git
cd PortMaster-AI-Dev-System

# 安装依赖
pip install -r requirements.txt

# 验证安装
python main.py --help
```

### 2. 第一次分析

```bash
# 分析您的项目
python main.py analyze /path/to/your/project

# 查看HTML报告
open output/reports/index.html
```

### 3. 编程接口

```python
from ai_dev_system import MasterController, ConfigManager
from pathlib import Path

# 创建配置
config = ConfigManager()

# 运行分析
controller = MasterController(Path("/path/to/project"), config)
results = controller.run_full_workflow()

print(f"构建状态: {'成功' if results['build']['success'] else '失败'}")
```

## 基本用法示例

### 1. 项目分析

#### 完整分析

```bash
# 分析项目（包括构建、测试、覆盖率等）
python main.py analyze ./my-python-project

# 只分析代码质量，跳过构建
python main.py analyze ./my-project --skip-build

# 跳过测试执行
python main.py analyze ./my-project --skip-tests
```

#### 单独分析

```bash
# 只构建项目
python main.py build ./my-project --type debug

# 只运行测试
python main.py test ./my-project --coverage

# 生成报告
python main.py report ./my-project --format html
```

### 2. 自动修复

```python
from ai_dev_system import MasterController, FixController, ConfigManager
from pathlib import Path

# 配置
config = ConfigManager()
project_path = Path("./my-project")

# 分析项目
controller = MasterController(project_path, config)
analysis_results = controller.run_full_workflow()

# 收集所有问题
issues = []
issues.extend(analysis_results.get('static_analysis', {}).get('issues', []))
issues.extend(analysis_results.get('performance', {}).get('issues', []))

print(f"发现 {len(issues)} 个问题")

# 预览修复（不实际修改文件）
fix_controller = FixController(project_path, config)
preview = fix_controller.preview_fixes(issues)

for fix in preview[:5]:  # 显示前5个修复预览
    print(f"文件: {fix['file']}")
    print(f"问题: {fix['issue']}")
    print(f"建议修复: {fix['suggested_fix']}")
    print("---")

# 应用自动修复
if input("是否应用自动修复? (y/n): ").lower() == 'y':
    fix_results = fix_controller.apply_fixes(issues, backup=True)
    print(f"成功修复 {fix_results['successful_fixes']} 个问题")
    print(f"失败 {fix_results['failed_fixes']} 个问题")
```

### 3. 测试和覆盖率

```python
from ai_dev_system import TestIntegrator, ConfigManager
from pathlib import Path

config = ConfigManager()
project_path = Path("./my-project")

# 创建测试集成器
test_integrator = TestIntegrator(project_path, config)

# 运行测试并收集覆盖率
result = test_integrator.execute_tests(include_coverage=True)

print(f"测试结果:")
print(f"  总测试数: {result.total_tests}")
print(f"  通过: {result.passed_tests}")
print(f"  失败: {result.failed_tests}")
print(f"  跳过: {result.skipped_tests}")
print(f"  耗时: {result.duration:.2f}s")

if result.coverage_info:
    coverage = result.coverage_info
    print(f"\n覆盖率信息:")
    print(f"  行覆盖率: {coverage['line_coverage']:.1f}%")
    print(f"  分支覆盖率: {coverage['branch_coverage']:.1f}%")
    print(f"  函数覆盖率: {coverage['function_coverage']:.1f}%")

    # 找出覆盖率低的文件
    low_coverage_files = []
    for file_path, file_coverage in coverage.get('files', {}).items():
        if file_coverage.get('line_coverage', 0) < 50:
            low_coverage_files.append((file_path, file_coverage['line_coverage']))

    if low_coverage_files:
        print(f"\n覆盖率较低的文件:")
        for file_path, cov in sorted(low_coverage_files, key=lambda x: x[1]):
            print(f"  {file_path}: {cov:.1f}%")
```

## 高级用法示例

### 1. 自定义分析规则

```python
from ai_dev_system import StaticAnalyzer, ConfigManager
from pathlib import Path

config = ConfigManager()
project_path = Path("./my-project")

# 创建静态分析器
analyzer = StaticAnalyzer(project_path, config)

# 添加自定义规则
custom_rules = [
    {
        'id': 'custom_naming_convention',
        'name': '函数命名规范',
        'pattern': r'def\s+([a-z]+_[a-z_]+)',
        'message': '函数名应使用驼峰命名法',
        'severity': 'medium',
        'suggestion': '建议使用驼峰命名法，如：getUserName',
        'language': 'python'
    },
    {
        'id': 'custom_docstring_required',
        'name': '缺少文档字符串',
        'pattern': r'def\s+\w+\([^)]*\):(?!\s*"""|\s*\'\'\')',
        'message': '公共函数缺少文档字符串',
        'severity': 'low',
        'suggestion': '为函数添加详细的文档字符串',
        'language': 'python'
    }
]

# 应用自定义规则
for rule in custom_rules:
    analyzer.add_custom_rule(rule)

# 运行分析
result = analyzer.analyze()

# 查看自定义规则的结果
custom_issues = [issue for issue in result.issues
                if issue.get('rule_id') in ['custom_naming_convention', 'custom_docstring_required']]

print(f"自定义规则发现 {len(custom_issues)} 个问题:")
for issue in custom_issues:
    print(f"  {issue['file_path']}:{issue['line_number']} - {issue['message']}")
```

### 2. 批量项目分析

```python
from ai_dev_system import analyze_project, ConfigManager
from pathlib import Path
import concurrent.futures
import json
from datetime import datetime

def analyze_single_project(project_path, output_dir):
    """分析单个项目"""
    try:
        print(f"开始分析项目: {project_path.name}")

        # 创建项目特定的输出目录
        project_output_dir = output_dir / project_path.name
        project_output_dir.mkdir(exist_ok=True)

        # 配置分析
        config = ConfigManager()
        config.set('reporting.output_directory', str(project_output_dir / 'reports'))

        # 运行分析
        results = analyze_project(project_path, config)

        # 保存结果
        result_file = project_output_dir / 'analysis_results.json'
        with open(result_file, 'w', encoding='utf-8') as f:
            json.dump(results, f, indent=2, ensure_ascii=False, default=str)

        return {
            'project': str(project_path),
            'status': 'success',
            'results': results,
            'output_file': str(result_file)
        }

    except Exception as e:
        return {
            'project': str(project_path),
            'status': 'error',
            'error': str(e)
        }

# 批量分析配置
projects_dir = Path("/path/to/projects")
output_dir = Path("/path/to/batch-analysis-results")
output_dir.mkdir(exist_ok=True)

# 发现所有项目
projects = [p for p in projects_dir.iterdir() if p.is_dir()]
print(f"发现 {len(projects)} 个项目")

# 并行分析（最多4个并发）
max_workers = 4
all_results = []

with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
    # 提交所有分析任务
    future_to_project = {
        executor.submit(analyze_single_project, project, output_dir): project
        for project in projects
    }

    # 收集结果
    for future in concurrent.futures.as_completed(future_to_project):
        project = future_to_project[future]
        try:
            result = future.result()
            all_results.append(result)

            if result['status'] == 'success':
                print(f"✅ {project.name} 分析完成")
            else:
                print(f"❌ {project.name} 分析失败: {result['error']}")

        except Exception as e:
            print(f"❌ {project.name} 分析异常: {e}")
            all_results.append({
                'project': str(project),
                'status': 'error',
                'error': str(e)
            })

# 生成汇总报告
summary = {
    'timestamp': datetime.now().isoformat(),
    'total_projects': len(projects),
    'successful_analyses': len([r for r in all_results if r['status'] == 'success']),
    'failed_analyses': len([r for r in all_results if r['status'] == 'error']),
    'results': all_results
}

summary_file = output_dir / 'batch_analysis_summary.json'
with open(summary_file, 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, ensure_ascii=False, default=str)

print(f"\n分析完成!")
print(f"成功: {summary['successful_analyses']}")
print(f"失败: {summary['failed_analyses']}")
print(f"汇总报告: {summary_file}")
```

### 3. 性能基准测试

```python
from ai_dev_system import PerformanceAnalyzer, MasterController, ConfigManager
from pathlib import Path
import time
import statistics

class PerformanceBenchmark:
    def __init__(self, project_path):
        self.project_path = project_path
        self.config = ConfigManager()
        self.analyzer = PerformanceAnalyzer(project_path, self.config)
        self.controller = MasterController(project_path, self.config)

    def benchmark_full_analysis(self, iterations=3):
        """基准测试完整分析流程"""
        durations = []
        memory_usage = []

        print(f"开始完整分析基准测试 ({iterations} 次迭代)...")

        for i in range(iterations):
            print(f"第 {i+1} 次迭代...")

            start_time = time.time()
            start_memory = self._get_memory_usage()

            # 运行完整分析
            results = self.controller.run_full_workflow()

            end_time = time.time()
            end_memory = self._get_memory_usage()

            duration = end_time - start_time
            memory_delta = end_memory - start_memory

            durations.append(duration)
            memory_usage.append(memory_delta)

            print(f"  耗时: {duration:.2f}s, 内存增量: {memory_delta:.1f}MB")

        # 计算统计信息
        avg_duration = statistics.mean(durations)
        std_duration = statistics.stdev(durations) if len(durations) > 1 else 0
        avg_memory = statistics.mean(memory_usage)

        return {
            'iterations': iterations,
            'avg_duration': avg_duration,
            'std_duration': std_duration,
            'min_duration': min(durations),
            'max_duration': max(durations),
            'avg_memory_usage': avg_memory,
            'durations': durations,
            'memory_usage': memory_usage
        }

    def benchmark_individual_components(self):
        """基准测试各个组件"""
        results = {}

        # 测试静态分析
        print("基准测试静态分析...")
        start_time = time.time()
        static_result = self.analyzer.analyze_performance()
        static_duration = time.time() - start_time
        results['static_analysis'] = {
            'duration': static_duration,
            'issues_found': len(static_result)
        }

        # 测试构建（如果启用）
        if self.config.get('build.enabled', True):
            print("基准测试构建...")
            from ai_dev_system import BuildIntegrator
            build_integrator = BuildIntegrator(self.project_path, self.config)
            start_time = time.time()
            build_result = build_integrator.execute_build()
            build_duration = time.time() - start_time
            results['build'] = {
                'duration': build_duration,
                'success': build_result.success,
                'warnings': build_result.warnings_count,
                'errors': build_result.errors_count
            }

        # 测试测试执行（如果启用）
        if self.config.get('test.enabled', True):
            print("基准测试测试执行...")
            from ai_dev_system import TestIntegrator
            test_integrator = TestIntegrator(self.project_path, self.config)
            start_time = time.time()
            test_result = test_integrator.execute_tests()
            test_duration = time.time() - start_time
            results['test'] = {
                'duration': test_duration,
                'total_tests': test_result.total_tests,
                'passed_tests': test_result.passed_tests,
                'failed_tests': test_result.failed_tests
            }

        return results

    def _get_memory_usage(self):
        """获取当前内存使用量（MB）"""
        try:
            import psutil
            process = psutil.Process()
            return process.memory_info().rss / 1024 / 1024
        except ImportError:
            return 0

# 使用示例
if __name__ == "__main__":
    project_path = Path("./my-project")
    benchmark = PerformanceBenchmark(project_path)

    # 完整分析基准测试
    full_analysis_results = benchmark.benchmark_full_analysis(iterations=5)
    print("\n完整分析基准测试结果:")
    print(f"  平均耗时: {full_analysis_results['avg_duration']:.2f}s ± {full_analysis_results['std_duration']:.2f}s")
    print(f"  最短耗时: {full_analysis_results['min_duration']:.2f}s")
    print(f"  最长耗时: {full_analysis_results['max_duration']:.2f}s")
    print(f"  平均内存使用: {full_analysis_results['avg_memory_usage']:.1f}MB")

    # 组件基准测试
    component_results = benchmark.benchmark_individual_components()
    print("\n组件基准测试结果:")
    for component, result in component_results.items():
        print(f"  {component}: {result['duration']:.2f}s")
        for key, value in result.items():
            if key != 'duration':
                print(f"    {key}: {value}")
```

## 集成示例

### 1. Flask Web应用集成

```python
from flask import Flask, request, jsonify, render_template
from ai_dev_system import MasterController, ConfigManager, ReportGenerator
from pathlib import Path
import tempfile
import shutil
import json

app = Flask(__name__)

# 配置
UPLOAD_FOLDER = 'uploads'
OUTPUT_FOLDER = 'outputs'
ALLOWED_EXTENSIONS = {'zip', 'tar', 'gz'}

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['OUTPUT_FOLDER'] = OUTPUT_FOLDER

# 确保目录存在
Path(UPLOAD_FOLDER).mkdir(exist_ok=True)
Path(OUTPUT_FOLDER).mkdir(exist_ok=True)

def allowed_file(filename):
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/analyze', methods=['POST'])
def analyze_project():
    """分析上传的项目"""
    try:
        # 检查文件
        if 'file' not in request.files:
            return jsonify({'error': '没有上传文件'}), 400

        file = request.files['file']
        if file.filename == '':
            return jsonify({'error': '没有选择文件'}), 400

        if not allowed_file(file.filename):
            return jsonify({'error': '不支持的文件格式'}), 400

        # 创建临时目录
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # 保存上传的文件
            file_path = temp_path / file.filename
            file.save(file_path)

            # 解压项目
            project_dir = temp_path / 'project'
            if file.filename.endswith('.zip'):
                import zipfile
                with zipfile.ZipFile(file_path, 'r') as zip_ref:
                    zip_ref.extractall(project_dir)
            else:
                import tarfile
                with tarfile.open(file_path, 'r:*') as tar_ref:
                    tar_ref.extractall(project_dir)

            # 运行分析
            config = ConfigManager()
            config.set('reporting.output_formats', ['json'])

            controller = MasterController(project_dir, config)
            results = controller.run_full_workflow()

            # 生成报告
            output_dir = Path(app.config['OUTPUT_FOLDER']) / f"analysis_{int(time.time())}"
            output_dir.mkdir(exist_ok=True)

            generator = ReportGenerator(output_dir, config)
            html_report = generator.generate_report(results, format='html')

            # 保存JSON结果
            json_file = output_dir / 'results.json'
            with open(json_file, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False, default=str)

            # 返回结果摘要
            return jsonify({
                'status': 'success',
                'project_name': file.filename,
                'analysis_id': output_dir.name,
                'summary': {
                    'build_success': results['build']['success'],
                    'total_tests': results['test']['total_tests'],
                    'passed_tests': results['test']['passed_tests'],
                    'coverage': results.get('coverage', {}).get('line_coverage', 0),
                    'total_issues': len(results.get('static_analysis', {}).get('issues', [])),
                    'performance_issues': len(results.get('performance', {}).get('issues', []))
                },
                'report_url': f"/report/{output_dir.name}",
                'download_url': f"/download/{output_dir.name}/results.json"
            })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/report/<analysis_id>')
def view_report(analysis_id):
    """查看分析报告"""
    report_path = Path(app.config['OUTPUT_FOLDER']) / analysis_id / 'index.html'
    if report_path.exists():
        return send_file(report_path)
    else:
        return "报告不存在", 404

@app.route('/download/<analysis_id>/<filename>')
def download_file(analysis_id, filename):
    """下载分析文件"""
    file_path = Path(app.config['OUTPUT_FOLDER']) / analysis_id / filename
    if file_path.exists():
        return send_file(file_path, as_attachment=True)
    else:
        return "文件不存在", 404

@app.route('/api/health')
def health_check():
    """健康检查"""
    return jsonify({
        'status': 'healthy',
        'timestamp': time.time(),
        'version': '1.0.0'
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
```

### 2. 与Git集成

```python
from ai_dev_system import MasterController, ConfigManager, GitManager
from pathlib import Path
import argparse
import sys

def analyze_git_diff(repo_path, commit_hash=None):
    """分析Git差异"""
    try:
        # 初始化Git管理器
        git_manager = GitManager(repo_path)

        # 获取差异
        diff_content = git_manager.get_diff(commit_hash)
        if not diff_content:
            print("没有发现差异")
            return

        print(f"分析Git差异...")
        if commit_hash:
            print(f"与提交 {commit_hash} 的差异")
        else:
            print("工作目录的差异")

        # 提取修改的文件
        status = git_manager.get_status()
        modified_files = status['modified_files'] + status['staged_files']

        if not modified_files:
            print("没有修改的文件")
            return

        print(f"发现 {len(modified_files)} 个修改的文件:")
        for file_path in modified_files:
            print(f"  - {file_path}")

        # 运行分析
        config = ConfigManager()
        config.set('static_analysis.include_files', modified_files)

        controller = MasterController(repo_path, config)

        # 只运行静态分析
        results = controller.run_static_analysis()

        # 显示结果
        issues = results.get('issues', [])
        if issues:
            print(f"\n发现 {len(issues)} 个问题:")
            for issue in issues:
                if issue['file_path'] in modified_files:
                    print(f"  {issue['file_path']}:{issue.get('line_number', '?')} - {issue['message']}")
                    print(f"    严重程度: {issue['severity']}")
                    if issue.get('suggestion'):
                        print(f"    建议: {issue['suggestion']}")
                    print()
        else:
            print("没有发现问题")

        # 可选：自动修复
        if issues and input("是否尝试自动修复? (y/n): ").lower() == 'y':
            from ai_dev_system import FixController
            fix_controller = FixController(repo_path, config)
            fix_results = fix_controller.apply_fixes(issues, backup=True)
            print(f"修复了 {fix_results['successful_fixes']} 个问题")

    except Exception as e:
        print(f"分析失败: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='分析Git差异')
    parser.add_argument('repo_path', type=Path, help='Git仓库路径')
    parser.add_argument('--commit', help='指定提交哈希')

    args = parser.parse_args()

    if not args.repo_path.exists():
        print(f"路径不存在: {args.repo_path}")
        sys.exit(1)

    analyze_git_diff(args.repo_path, args.commit)

if __name__ == '__main__':
    main()
```

### 3. CI/CD集成脚本

```bash
#!/bin/bash
# ci-integration.sh - CI/CD集成脚本

set -e

# 配置
PROJECT_PATH="${1:-.}"
OUTPUT_DIR="${2:-./ci-output}"
CONFIG_FILE="${3:-./ci-config.yaml}"

echo "开始CI/CD分析..."
echo "项目路径: $PROJECT_PATH"
echo "输出目录: $OUTPUT_DIR"

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 检查Python环境
if ! command -v python &> /dev/null; then
    echo "错误: Python未安装"
    exit 1
fi

# 安装系统（如果需要）
if [ ! -d "PortMaster-AI-Dev-System" ]; then
    echo "克隆AI开发系统..."
    git clone https://github.com/your-repo/PortMaster-AI-Dev-System.git
    cd PortMaster-AI-Dev-System
    pip install -r requirements.txt
    cd ..
else
    cd PortMaster-AI-Dev-System
    git pull
    pip install -r requirements.txt
    cd ..
fi

# 设置Python路径
export PYTHONPATH="${PYTHONPATH}:$(pwd)/PortMaster-AI-Dev-System"

# 创建CI配置
cat > "$CONFIG_FILE" << EOF
system:
  log_level: "INFO"
  output_directory: "$OUTPUT_DIR"

build:
  enabled: true
  timeout: 300

test:
  enabled: true
  timeout: 600
  coverage_threshold: 80.0
  collect_coverage: true

static_analysis:
  enabled: true
  complexity_threshold: 10
  security_scan: true

reporting:
  output_formats:
    - "json"
    - "markdown"
  include_charts: true

performance:
  enable_profiling: false

logging:
  level: "INFO"
  file: "$OUTPUT_DIR/ci-analysis.log"
EOF

echo "运行分析..."

# 运行分析
cd PortMaster-AI-Dev-System
python main.py analyze "$PROJECT_PATH" --config "$CONFIG_FILE" --output "$OUTPUT_DIR"
cd ..

# 检查结果
if [ -f "$OUTPUT_DIR/reports/analysis_results.json" ]; then
    echo "分析完成，生成报告..."

    # 提取关键指标
    python3 << EOF
import json
import sys

with open("$OUTPUT_DIR/reports/analysis_results.json", 'r') as f:
    results = json.load(f)

# 构建状态
build_success = results.get('build', {}).get('success', False)
print(f"构建状态: {'成功' if build_success else '失败'}")

if not build_success:
    print("构建失败，CI检查未通过")
    sys.exit(1)

# 测试结果
test_results = results.get('test', {})
total_tests = test_results.get('total_tests', 0)
passed_tests = test_results.get('passed_tests', 0)
failed_tests = test_results.get('failed_tests', 0)

print(f"测试结果: {passed_tests}/{total_tests} 通过")

if failed_tests > 0:
    print(f"存在 {failed_tests} 个失败的测试")
    sys.exit(1)

# 覆盖率
coverage = results.get('coverage', {}).get('line_coverage', 0)
print(f"代码覆盖率: {coverage:.1f}%")

if coverage < 80.0:
    print(f"覆盖率 {coverage:.1f}% 低于要求的 80%")
    sys.exit(1)

# 代码质量
issues = results.get('static_analysis', {}).get('issues', [])
critical_issues = [i for i in issues if i.get('severity') == 'critical']
high_issues = [i for i in issues if i.get('severity') == 'high']

print(f"代码质量: 发现 {len(issues)} 个问题")
print(f"  严重问题: {len(critical_issues)}")
print(f"  高优先级问题: {len(high_issues)}")

if critical_issues:
    print("存在严重代码质量问题")
    sys.exit(1)

print("所有检查通过！")
EOF

    echo "CI/CD分析完成！"
    echo "详细报告: $OUTPUT_DIR/reports/"

    # 如果在CI环境中，上传报告
    if [ -n "$CI" ]; then
        echo "上传报告到CI系统..."

        # GitHub Actions
        if [ "$CI_SYSTEM" = "github-actions" ]; then
            echo "::set-output name=report-path::$OUTPUT_DIR/reports/"

            # 创建PR评论（如果在PR中）
            if [ -n "$GITHUB_EVENT_NAME" ] && [ "$GITHUB_EVENT_NAME" = "pull_request" ]; then
                python3 << EOF
import json
import os
import requests

with open("$OUTPUT_DIR/reports/analysis_results.json", 'r') as f:
    results = json.load(f)

# 构建评论内容
comment = f"""
## 🤖 AI开发系统分析结果

### ✅ 通过项目
- ✅ 构建状态: {'成功' if results.get('build', {}).get('success', False) else '失败'}
- ✅ 测试通过率: {results.get('test', {}).get('passed_tests', 0)}/{results.get('test', {}).get('total_tests', 0)}
- ✅ 代码覆盖率: {results.get('coverage', {}).get('line_coverage', 0):.1f}%

### 📊 质量指标
- 🔍 发现问题: {len(results.get('static_analysis', {}).get('issues', []))}
- 🐛 性能问题: {len(results.get('performance', {}).get('issues', []))}
- 📈 复杂度问题: {len([i for i in results.get('static_analysis', {}).get('issues', []) if i.get('type') == 'complexity'])}

[查看详细报告](${os.environ.get('GITHUB_SERVER_URL')}/${os.environ.get('GITHUB_REPOSITORY')}/actions/runs/${os.environ.get('GITHUB_RUN_ID')})
"""

# 这里应该调用GitHub API创建评论，需要token
print("PR评论内容:")
print(comment)
EOF
            fi
        fi
    else
        echo "不在CI环境中，跳过上传"
    fi

else
    echo "错误: 分析结果文件不存在"
    exit 1
fi
```

## 配置示例

### 1. Python项目配置

```yaml
# python-project-config.yaml
project:
  type: python
  language: python
  build_system: setuptools
  test_framework: pytest
  source_directories:
    - "src/"
    - "lib/"
  test_directories:
    - "tests/"
    - "unit_tests/"
    - "integration_tests/"

build:
  enabled: true
  timeout: 300
  commands:
    - "pip install -e ."
    - "python setup.py build"
  artifacts:
    - "dist/"
    - "build/"
    - "*.egg-info/"

test:
  enabled: true
  framework: pytest
  timeout: 600
  coverage_threshold: 85.0
  collect_coverage: true
  commands:
    - "pytest tests/ --cov=src --cov-report=xml --cov-report=html"
    - "pytest integration_tests/ --cov=src --cov-append"
  coverage_formats:
    - "xml"
    - "html"
    - "json"

static_analysis:
  enabled: true
  timeout: 300
  tools:
    python:
      - "pylint --score=no"
      - "flake8"
      - "mypy --strict"
      - "bandit -r"
  complexity_threshold: 8
  security_scan: true

reporting:
  enabled: true
  output_formats:
    - "html"
    - "json"
    - "markdown"
  include_charts: true
  dashboard: true

performance:
  enabled: true
  profile_memory: true
  complexity_analysis: true
  benchmarks:
    enabled: true
    iterations: 10

# 自定义规则
custom_rules:
  enabled: true
  rules_file: "custom_rules.yaml"

# 环境特定配置
environments:
  development:
    log_level: "DEBUG"
    skip_coverage: false

  ci:
    log_level: "INFO"
    parallel_jobs: 2
    fail_fast: true

  production:
    log_level: "WARNING"
    skip_build: false
    coverage_threshold: 90.0
```

### 2. 多语言项目配置

```yaml
# multi-lang-config.yaml
project:
  type: mixed
  languages: ["python", "javascript", "java"]
  build_system: maven  # 主要构建系统
  test_framework: junit  # 主要测试框架

# 语言特定配置
language_configs:
  python:
    source_directories: ["python/src/", "python/lib/"]
    test_directories: ["python/tests/"]
    test_framework: pytest
    static_analysis_tools: ["pylint", "mypy"]

  javascript:
    source_directories: ["frontend/src/", "frontend/lib/"]
    test_directories: ["frontend/tests/", "frontend/__tests__/"]
    test_framework: jest
    static_analysis_tools: ["eslint", "jshint"]

  java:
    source_directories: ["backend/src/main/java/"]
    test_directories: ["backend/src/test/java/"]
    test_framework: junit
    static_analysis_tools: ["spotbugs", "checkstyle"]

# 构建配置
build:
  enabled: true
  timeout: 600
  parallel_jobs: 2
  steps:
    - name: "Python依赖安装"
      command: "pip install -r python/requirements.txt"
      condition: "exists python/requirements.txt"

    - name: "Node.js依赖安装"
      command: "cd frontend && npm install"
      condition: "exists frontend/package.json"

    - name: "Java编译"
      command: "mvn compile"
      condition: "exists pom.xml"

    - name: "前端构建"
      command: "cd frontend && npm run build"
      condition: "exists frontend/package.json"

# 测试配置
test:
  enabled: true
  timeout: 900
  parallel: true
  test_suites:
    - name: "Python单元测试"
      command: "pytest python/tests/ --cov=python/src"
      working_directory: "python/"

    - name: "JavaScript测试"
      command: "npm test -- --coverage"
      working_directory: "frontend/"

    - name: "Java测试"
      command: "mvn test"
      working_directory: "backend/"

# 静态分析配置
static_analysis:
  enabled: true
  timeout: 600
  parallel: true
  analyze_by_language: true
  language_specific_rules:
    python:
      complexity_threshold: 7
      line_length: 88
    javascript:
      complexity_threshold: 10
      esversion: 2020
    java:
      complexity_threshold: 15
      checkstyle_file: "checkstyle.xml"

# 报告配置
reporting:
  enabled: true
  output_formats:
    - "html"
    - "json"
  group_by_language: true
  comparative_analysis: true
  trend_analysis: true
```

## 工作流示例

### 1. 快速检查工作流

```yaml
# quick-check-workflow.yaml
name: "快速代码质量检查"
description: "5分钟内完成代码质量检查"

variables:
  timeout: 300
  parallel: true

steps:
  - name: "环境准备"
    type: "custom"
    command: "echo '开始快速检查'"
    timeout: 30

  - name: "语法检查"
    type: "analyze"
    config:
      analyzer: "syntax"
      timeout: 120
    dependencies: ["环境准备"]

  - name: "快速静态分析"
    type: "analyze"
    config:
      analyzer: "static"
      timeout: 180
      skip_expensive: true
    dependencies: ["语法检查"]

  - name: "格式检查"
    type: "custom"
    command: |
      # Python格式检查
      find . -name "*.py" -exec black --check {} \;
      find . -name "*.py" -exec isort --check-only {} \;

      # JavaScript格式检查
      find . -name "*.js" -exec eslint --no-eslintrc --config .eslintrc.quick {} \;
    timeout: 60
    dependencies: ["环境准备"]

  - name: "生成快速报告"
    type: "report"
    config:
      format: "markdown"
      template: "quick-report"
      include_summary_only: true
    dependencies: ["语法检查", "快速静态分析", "格式检查"]

# 失败处理
on_failure:
  - name: "通知失败"
    type: "custom"
    command: "echo '快速检查失败，请查看报告'"
    always_run: true
```

### 2. 完整CI/CD工作流

```yaml
# complete-cicd-workflow.yaml
name: "完整CI/CD流水线"
description: "包含构建、测试、部署的完整流水线"

variables:
  build_parallel: true
  test_parallel: true
  deploy_approval: true

steps:
  # 阶段1: 构建准备
  - name: "代码检出"
    type: "custom"
    command: "echo '代码已检出'"
    timeout: 60

  - name: "依赖安装"
    type: "custom"
    command: |
      # 安装Python依赖
      if [ -f requirements.txt ]; then
        pip install -r requirements.txt
      fi

      # 安装Node.js依赖
      if [ -f package.json ]; then
        npm ci
      fi

      # 安装Java依赖
      if [ -f pom.xml ]; then
        mvn dependency:resolve
      fi
    timeout: 300
    dependencies: ["代码检出"]

  # 阶段2: 构建
  - name: "项目构建"
    type: "build"
    config:
      parallel_jobs: 4
      clean: false
    timeout: 600
    dependencies: ["依赖安装"]

  - name: "构建验证"
    type: "custom"
    command: |
      # 验证构建产物
      if [ ! -d "dist" ] && [ ! -d "build" ] && [ ! -d "target" ]; then
        echo "错误: 未找到构建产物"
        exit 1
      fi

      echo "构建验证通过"
    dependencies: ["项目构建"]
    timeout: 60

  # 阶段3: 测试
  - name: "单元测试"
    type: "test"
    config:
      test_type: "unit"
      coverage: true
      parallel: true
    timeout: 900
    dependencies: ["构建验证"]

  - name: "集成测试"
    type: "test"
    config:
      test_type: "integration"
      coverage: false
      parallel: false
    timeout: 1200
    dependencies: ["构建验证"]

  - name: "性能测试"
    type: "custom"
    command: |
      # 运行性能基准测试
      python scripts/benchmark.py --output performance-results.json

      # 检查性能回归
      python scripts/check-performance-regression.py --baseline baseline.json --current performance-results.json
    timeout: 600
    dependencies: ["单元测试"]

  # 阶段4: 质量检查
  - name: "静态代码分析"
    type: "analyze"
    config:
      include_security: true
      include_performance: true
      include_complexity: true
    timeout: 600
    dependencies: ["单元测试"]

  - name: "安全扫描"
    type: "custom"
    command: |
      # 依赖安全扫描
      safety check
      npm audit --audit-level moderate

      # 代码安全扫描
      bandit -r . -f json -o security-report.json
    timeout: 300
    dependencies: ["依赖安装"]

  - name: "许可证检查"
    type: "custom"
    command: |
      # 检查依赖许可证
      pip-licenses --from=mixed --format=json --output-file=licenses.json

      # 验证许可证合规性
      python scripts/check-licenses.py --licenses-file licenses.json
    timeout: 180
    dependencies: ["依赖安装"]

  # 阶段5: 报告生成
  - name: "生成分析报告"
    type: "report"
    config:
      formats: ["html", "json", "markdown"]
      include_charts: true
      include_trends: true
    timeout: 300
    dependencies: ["静态代码分析", "安全扫描", "单元测试", "集成测试"]

  - name: "生成测试报告"
    type: "custom"
    command: |
      # 合并测试覆盖率报告
      coverage combine
      coverage html -d test-reports/html
      coverage xml -o test-reports/coverage.xml

      # 生成测试趋势报告
      python scripts/generate-test-trends.py --output test-trends.json
    timeout: 180
    dependencies: ["单元测试", "集成测试"]

  # 阶段6: 部署准备
  - name: "构建部署包"
    type: "custom"
    command: |
      # 创建部署包
      mkdir -p deployment-package

      # 复制构建产物
      cp -r dist/* deployment-package/ 2>/dev/null || true
      cp -r build/* deployment-package/ 2>/dev/null || true
      cp -r target/* deployment-package/ 2>/dev/null || true

      # 复制配置文件
      cp -r config/* deployment-package/ 2>/dev/null || true

      # 创建部署元数据
      echo "{
        \"version\": \"$BUILD_VERSION\",
        \"build_time\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",
        \"git_commit\": \"$GIT_COMMIT\",
        \"branch\": \"$GIT_BRANCH\"
      }" > deployment-package/deployment-metadata.json

      # 打包
      tar -czf deployment-package.tar.gz deployment-package/
    timeout: 300
    dependencies: ["生成分析报告", "生成测试报告"]

  # 阶段7: 部署（需要审批）
  - name: "部署到测试环境"
    type: "custom"
    command: |
      echo "等待部署审批..."

      # 部署脚本
      ./scripts/deploy.sh --environment=staging --package=deployment-package.tar.gz
    timeout: 600
    dependencies: ["构建部署包"]
    approval_required: true

  - name: "部署后验证"
    type: "custom"
    command: |
      # 健康检查
      curl -f http://staging.example.com/health || exit 1

      # 冒烟测试
      python scripts/smoke-tests.py --environment=staging
    timeout: 300
    dependencies: ["部署到测试环境"]

  # 阶段8: 通知和清理
  - name: "发送通知"
    type: "custom"
    command: |
      # 发送成功通知
      curl -X POST "$WEBHOOK_URL" \
        -H "Content-Type: application/json" \
        -d "{
          \"text\": \"✅ CI/CD流水线成功完成\",
          \"build_number\": \"$BUILD_NUMBER\",
          \"report_url\": \"$REPORT_URL\"
        }"
    timeout: 60
    dependencies: ["部署后验证"]
    always_run: true

  - name: "清理工作空间"
    type: "custom"
    command: |
      # 清理临时文件
      rm -rf temp/*
      rm -f *.log

      # 清理Docker镜像（如果使用）
      docker system prune -f
    timeout: 120
    dependencies: ["发送通知"]
    always_run: true

# 失败处理
on_failure:
  - name: "失败通知"
    type: "custom"
    command: |
      curl -X POST "$WEBHOOK_URL" \
        -H "Content-Type: application/json" \
        -d "{
          \"text\": \"❌ CI/CD流水线失败\",
          \"build_number\": \"$BUILD_NUMBER\",
          \"failed_step\": \"$FAILED_STEP\"
        }"
    always_run: true

  - name: "收集日志"
    type: "custom"
    command: |
      # 收集所有日志文件
      mkdir -p failure-logs
      cp *.log failure-logs/ 2>/dev/null || true

      # 收集系统信息
      uname -a > failure-logs/system-info.txt
      docker version > failure-logs/docker-info.txt 2>/dev/null || true

      # 打包日志
      tar -czf failure-logs.tar.gz failure-logs/
    always_run: true
    timeout: 60

# 环境特定配置
environments:
  development:
    skip_security_scan: true
    skip_performance_test: true
    deploy_approval: false

  ci:
    parallel_jobs: 4
    fail_fast: true
    timeout_multiplier: 1.0

  production:
    security_scan_level: "strict"
    deploy_approval: true
    rollback_on_failure: true
```

## 插件开发示例

### 1. 自定义分析器插件

```python
# custom_analyzer_plugin.py
from ai_dev_system.analyzers.static_analyzer import StaticAnalyzer, CodeIssue
from pathlib import Path
import re
from typing import List

class CustomSecurityAnalyzer:
    """自定义安全分析器插件"""

    def __init__(self, config=None):
        self.name = "custom_security_analyzer"
        self.config = config or {}
        self.rules = self._load_security_rules()

    def _load_security_rules(self):
        """加载安全规则"""
        return [
            {
                'id': 'hardcoded_password',
                'name': '硬编码密码',
                'pattern': r'(password\s*=\s*["\'][^"\']+["\'])',
                'severity': 'critical',
                'message': '检测到硬编码密码',
                'suggestion': '使用环境变量或配置文件存储敏感信息',
                'languages': ['python', 'javascript', 'java']
            },
            {
                'id': 'sql_injection_risk',
                'name': 'SQL注入风险',
                'pattern': r'(execute\s*\(\s*["\'].*\+.*["\']|cursor\.execute\s*\(\s*["\'].*%\s*.*["\'])',
                'severity': 'high',
                'message': '可能的SQL注入风险',
                'suggestion': '使用参数化查询或ORM',
                'languages': ['python', 'java']
            },
            {
                'id': 'insecure_deserialization',
                'name': '不安全的反序列化',
                'pattern': r'(pickle\.loads|yaml\.load\s*\([^)]*\)(?!\s*Loader\))',
                'severity': 'high',
                'message': '不安全的反序列化操作',
                'suggestion': '使用安全的序列化格式或验证输入',
                'languages': ['python']
            }
        ]

    def analyze(self, project_path: Path) -> List[CodeIssue]:
        """分析项目安全问题"""
        issues = []

        # 获取项目中的代码文件
        code_files = self._get_code_files(project_path)

        for file_path in code_files:
            file_issues = self._analyze_file(file_path)
            issues.extend(file_issues)

        return issues

    def _get_code_files(self, project_path: Path) -> List[Path]:
        """获取需要分析的代码文件"""
        extensions = ['.py', '.js', '.java', '.ts']
        code_files = []

        for ext in extensions:
            code_files.extend(project_path.rglob(f'*{ext}'))

        # 排除一些目录
        exclude_dirs = {'node_modules', '.git', '__pycache__', 'venv', 'env'}
        code_files = [f for f in code_files
                     if not any(exclude_dir in str(f).split('/') for exclude_dir in exclude_dirs)]

        return code_files

    def _analyze_file(self, file_path: Path) -> List[CodeIssue]:
        """分析单个文件"""
        issues = []

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()

            # 检测文件语言
            language = self._detect_language(file_path)

            # 应用安全规则
            for rule in self.rules:
                if language in rule['languages']:
                    matches = re.finditer(rule['pattern'], content, re.IGNORECASE | re.MULTILINE)

                    for match in matches:
                        # 计算行号
                        line_number = content[:match.start()].count('\n') + 1

                        issue = CodeIssue(
                            type='security',
                            severity=rule['severity'],
                            file_path=str(file_path.relative_to(file_path.parents[-2])),
                            line_number=line_number,
                            column_number=match.start() - content.rfind('\n', 0, match.start()),
                            message=rule['message'],
                            description=f"安全规则: {rule['name']}",
                            suggestion=rule['suggestion'],
                            rule_id=rule['id'],
                            category='security'
                        )
                        issues.append(issue)

        except Exception as e:
            print(f"分析文件 {file_path} 时出错: {e}")

        return issues

    def _detect_language(self, file_path: Path) -> str:
        """检测文件语言"""
        suffix = file_path.suffix.lower()

        language_map = {
            '.py': 'python',
            '.js': 'javascript',
            '.ts': 'typescript',
            '.java': 'java',
            '.cpp': 'cpp',
            '.c': 'c',
            '.cs': 'csharp'
        }

        return language_map.get(suffix, 'unknown')

# 插件注册函数
def register_plugin(plugin_manager):
    """注册插件到插件管理器"""
    plugin_manager.register_analyzer('custom_security', CustomSecurityAnalyzer)

# 插件配置示例
PLUGIN_CONFIG = {
    'custom_security_analyzer': {
        'enabled': True,
        'severity_threshold': 'medium',  # 只报告medium及以上级别的问题
        'exclude_patterns': [
            'test_*.py',  # 排除测试文件
            '*_test.py',
            'tests/*'
        ],
        'custom_rules_file': 'custom_security_rules.yaml'  # 自定义规则文件
    }
}
```

### 2. 自定义报告插件

```python
# custom_report_plugin.py
from ai_dev_system.reporters.report_generator import ReportGenerator
from pathlib import Path
import json
import jinja2
from datetime import datetime

class CustomMarkdownReporter:
    """自定义Markdown报告生成器"""

    def __init__(self, config=None):
        self.name = "custom_markdown_reporter"
        self.config = config or {}
        self.template_dir = Path(__file__).parent / 'templates'

    def generate_report(self, data: dict, output_path: Path) -> Path:
        """生成Markdown报告"""
        # 加载模板
        template = self._load_template('custom_report.md')

        # 准备模板数据
        template_data = self._prepare_template_data(data)

        # 渲染模板
        content = template.render(**template_data)

        # 保存报告
        report_file = output_path / 'custom_report.md'
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(content)

        return report_file

    def _load_template(self, template_name: str) -> jinja2.Template:
        """加载Jinja2模板"""
        template_path = self.template_dir / template_name

        if not template_path.exists():
            # 使用内置模板
            template_content = self._get_builtin_template(template_name)
        else:
            with open(template_path, 'r', encoding='utf-8') as f:
                template_content = f.read()

        return jinja2.Template(template_content)

    def _get_builtin_template(self, template_name: str) -> str:
        """获取内置模板"""
        if template_name == 'custom_report.md':
            return """
# {{ project_name }} - AI开发系统分析报告

**生成时间**: {{ timestamp }}
**分析版本**: {{ version }}

## 📊 执行摘要

{% if build_success %}
- ✅ **构建状态**: 成功
{% else %}
- ❌ **构建状态**: 失败
{% endif %}

- 🧪 **测试通过率**: {{ test_pass_rate }}%
- 📈 **代码覆盖率**: {{ coverage_percentage }}%
- 🔍 **发现问题总数**: {{ total_issues }}
- 🚨 **严重问题**: {{ critical_issues }}

## 🏗️ 构建结果

{% if build_success %}
构建成功完成，耗时 {{ build_duration }}秒。
{% else %}
构建失败，错误信息：
```
{{ build_error }}
```
{% endif %}

## 🧪 测试结果

- **总测试数**: {{ total_tests }}
- **通过**: {{ passed_tests }}
- **失败**: {{ failed_tests }}
- **跳过**: {{ skipped_tests }}

{% if coverage_percentage > 0 %}
### 覆盖率详情
- **行覆盖率**: {{ coverage_percentage }}%
- **分支覆盖率**: {{ branch_coverage }}%
- **函数覆盖率**: {{ function_coverage }}%
{% endif %}

## 🔍 代码质量分析

### 问题统计
| 严重程度 | 数量 |
|---------|------|
| 🚨 严重 | {{ critical_issues }} |
| ⚠️  高 | {{ high_issues }} |
| 💡 中等 | {{ medium_issues }} |
| ℹ️  低 | {{ low_issues }} |

### 问题详情

{% for issue in top_issues %}
#### {{ issue.severity }} - {{ issue.message }}
- **文件**: `{{ issue.file_path }}:{{ issue.line_number }}`
- **建议**: {{ issue.suggestion }}

{% endfor %}

## 📈 趋势分析

{% if trend_data %}
{% for metric in trend_data %}
### {{ metric.name }}
- 当前: {{ metric.current }}
- 上次: {{ metric.previous }}
- 变化: {{ metric.change }} ({{ metric.trend }})

{% endfor %}
{% else %}
暂无历史数据用于趋势分析。
{% endif %}

## 🔧 建议修复

{% for fix in suggested_fixes %}
1. **{{ fix.title }}**
   - 优先级: {{ fix.priority }}
   - 预估工作量: {{ fix.effort }}
   - 描述: {{ fix.description }}

{% endfor %}

---

*此报告由AI开发系统自动生成*
            """
        else:
            return "# 模板未找到"

    def _prepare_template_data(self, data: dict) -> dict:
        """准备模板数据"""
        project_info = data.get('project', {})
        build_result = data.get('build', {})
        test_result = data.get('test', {})
        coverage_data = data.get('coverage', {})
        static_analysis = data.get('static_analysis', {})

        # 计算统计信息
        total_tests = test_result.get('total_tests', 0)
        passed_tests = test_result.get('passed_tests', 0)
        test_pass_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0

        coverage_percentage = coverage_data.get('line_coverage', 0)

        issues = static_analysis.get('issues', [])
        issues_by_severity = {}
        for issue in issues:
            severity = issue.get('severity', 'low')
            issues_by_severity[severity] = issues_by_severity.get(severity, 0) + 1

        # 获取前10个问题
        top_issues = sorted(issues, key=lambda x: self._severity_weight(x.get('severity', 'low')), reverse=True)[:10]

        return {
            'project_name': project_info.get('name', 'Unknown'),
            'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            'version': data.get('version', '1.0.0'),

            'build_success': build_result.get('success', False),
            'build_duration': build_result.get('duration', 0),
            'build_error': build_result.get('error_output', '')[:500],

            'total_tests': total_tests,
            'passed_tests': passed_tests,
            'failed_tests': test_result.get('failed_tests', 0),
            'skipped_tests': test_result.get('skipped_tests', 0),
            'test_pass_rate': f"{test_pass_rate:.1f}",

            'coverage_percentage': f"{coverage_percentage:.1f}",
            'branch_coverage': f"{coverage_data.get('branch_coverage', 0):.1f}",
            'function_coverage': f"{coverage_data.get('function_coverage', 0):.1f}",

            'total_issues': len(issues),
            'critical_issues': issues_by_severity.get('critical', 0),
            'high_issues': issues_by_severity.get('high', 0),
            'medium_issues': issues_by_severity.get('medium', 0),
            'low_issues': issues_by_severity.get('low', 0),

            'top_issues': top_issues,

            'trend_data': data.get('trends', []),
            'suggested_fixes': self._generate_suggested_fixes(issues)
        }

    def _severity_weight(self, severity: str) -> int:
        """获取严重程度权重"""
        weights = {
            'critical': 4,
            'high': 3,
            'medium': 2,
            'low': 1
        }
        return weights.get(severity, 0)

    def _generate_suggested_fixes(self, issues: list) -> list:
        """生成建议修复列表"""
        fixes = []

        # 按严重程度和类型分组问题
        issues_by_type = {}
        for issue in issues:
            issue_type = issue.get('type', 'other')
            if issue_type not in issues_by_type:
                issues_by_type[issue_type] = []
            issues_by_type[issue_type].append(issue)

        # 为每种类型生成修复建议
        for issue_type, type_issues in issues_by_type.items():
            if issue_type == 'security':
                fixes.append({
                    'title': '修复安全问题',
                    'priority': '高',
                    'effort': '2-4小时',
                    'description': f'发现 {len(type_issues)} 个安全问题，建议立即修复硬编码密码、SQL注入等安全漏洞。'
                })
            elif issue_type == 'complexity':
                fixes.append({
                    'title': '降低代码复杂度',
                    'priority': '中',
                    'effort': '4-8小时',
                    'description': f'发现 {len(type_issues)} 个复杂度过高的函数，建议重构为更小的函数。'
                })
            elif issue_type == 'style':
                fixes.append({
                    'title': '改进代码风格',
                    'priority': '低',
                    'effort': '1-2小时',
                    'description': f'发现 {len(type_issues)} 个代码风格问题，可以使用自动格式化工具修复。'
                })

        return fixes

# 插件注册函数
def register_plugin(plugin_manager):
    """注册插件到插件管理器"""
    plugin_manager.register_reporter('custom_markdown', CustomMarkdownReporter)
```

### 3. 插件使用示例

```python
# plugin_usage_example.py
from ai_dev_system import MasterController, ConfigManager
from pathlib import Path

# 加载自定义插件
from custom_analyzer_plugin import CustomSecurityAnalyzer
from custom_report_plugin import CustomMarkdownReporter

class PluginManager:
    """插件管理器"""
    def __init__(self):
        self.analyzers = {}
        self.reporters = {}

    def register_analyzer(self, name, analyzer_class):
        """注册分析器插件"""
        self.analyzers[name] = analyzer_class

    def register_reporter(self, name, reporter_class):
        """注册报告器插件"""
        self.reporters[name] = reporter_class

    def get_analyzer(self, name, config=None):
        """获取分析器实例"""
        if name in self.analyzers:
            return self.analyzers[name](config)
        return None

    def get_reporter(self, name, config=None):
        """获取报告器实例"""
        if name in self.reporters:
            return self.reporters[name](config)
        return None

# 使用自定义插件
def main():
    # 创建插件管理器
    plugin_manager = PluginManager()

    # 注册插件
    plugin_manager.register_analyzer('custom_security', CustomSecurityAnalyzer)
    plugin_manager.register_reporter('custom_markdown', CustomMarkdownReporter)

    # 创建配置
    config = ConfigManager()

    # 获取自定义分析器
    security_analyzer = plugin_manager.get_analyzer('custom_security')
    if security_analyzer:
        project_path = Path("./my-project")
        security_issues = security_analyzer.analyze(project_path)
        print(f"发现 {len(security_issues)} 个安全问题")

    # 获取自定义报告器
    markdown_reporter = plugin_manager.get_reporter('custom_markdown')
    if markdown_reporter:
        # 模拟分析数据
        analysis_data = {
            'project': {'name': 'My Project'},
            'build': {'success': True, 'duration': 120},
            'test': {'total_tests': 100, 'passed_tests': 95, 'failed_tests': 3, 'skipped_tests': 2},
            'coverage': {'line_coverage': 85.5, 'branch_coverage': 78.2, 'function_coverage': 92.1},
            'static_analysis': {'issues': security_issues}
        }

        output_path = Path("./custom-reports")
        output_path.mkdir(exist_ok=True)

        report_file = markdown_reporter.generate_report(analysis_data, output_path)
        print(f"自定义报告已生成: {report_file}")

if __name__ == '__main__':
    main()
```

---

这些示例展示了AI Development System的各种使用方式，从基础的命令行操作到高级的插件开发。根据您的具体需求，选择合适的示例并进行相应的调整。