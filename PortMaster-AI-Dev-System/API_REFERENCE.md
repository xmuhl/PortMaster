# API 参考文档

## 目录

1. [核心模块](#核心模块)
2. [分析器模块](#分析器模块)
3. [集成器模块](#集成器模块)
4. [配置模块](#配置模块)
5. [工具模块](#工具模块)
6. [报告模块](#报告模块)

## 核心模块

### MasterController

主要的控制器类，协调所有分析组件。

```python
class MasterController:
    def __init__(self, project_root: Path, config: Optional[ConfigManager] = None)
    def run_full_workflow(self) -> Dict[str, Any]
    def run_build_verification(self) -> Dict[str, Any]
    def run_static_analysis(self) -> Dict[str, Any]
    def run_test_execution(self) -> Dict[str, Any]
    def run_coverage_analysis(self) -> Dict[str, Any]
    def run_error_pattern_analysis(self) -> Dict[str, Any]
    def run_performance_analysis(self) -> Dict[str, Any]
    def generate_report(self, format: str = 'html') -> Optional[Path]
```

#### 构造函数

**`MasterController(project_root, config=None)`**

初始化主控制器。

**参数:**
- `project_root` (Path): 项目根目录路径
- `config` (ConfigManager, optional): 配置管理器实例

**示例:**
```python
from ai_dev_system import MasterController, ConfigManager
from pathlib import Path

config = ConfigManager()
controller = MasterController(Path("/path/to/project"), config)
```

#### 方法

**`run_full_workflow() -> Dict[str, Any]`**

运行完整的分析工作流，包括构建、测试、静态分析等所有步骤。

**返回值:**
- `Dict[str, Any]`: 包含所有分析结果的字典

**返回结构:**
```python
{
    'project': {
        'name': str,
        'path': str,
        'type': str
    },
    'timestamp': str,
    'build': Dict[str, Any],
    'test': Dict[str, Any],
    'static_analysis': Dict[str, Any],
    'coverage': Dict[str, Any],
    'error_patterns': Dict[str, Any],
    'performance': Dict[str, Any]
}
```

**示例:**
```python
results = controller.run_full_workflow()
print(f"构建状态: {results['build']['success']}")
print(f"测试通过率: {results['test']['passed_tests'] / results['test']['total_tests'] * 100:.1f}%")
```

### FixController

自动修复控制器，用于修复检测到的代码问题。

```python
class FixController:
    def __init__(self, project_root: Path, config: Optional[ConfigManager] = None)
    def apply_fixes(self, issues: List[Dict[str, Any]], backup: bool = True) -> Dict[str, Any]
    def rollback_fixes(self, backup_path: Path) -> bool
    def preview_fixes(self, issues: List[Dict[str, Any]]) -> List[Dict[str, Any]]
```

#### 方法

**`apply_fixes(issues, backup=True) -> Dict[str, Any]`**

应用自动修复到指定的问题列表。

**参数:**
- `issues` (List[Dict[str, Any]]): 问题列表
- `backup` (bool): 是否在修复前创建备份

**返回值:**
- `Dict[str, Any]`: 修复结果信息

**返回结构:**
```python
{
    'attempted_fixes': int,
    'successful_fixes': int,
    'failed_fixes': int,
    'backup_created': bool,
    'backup_path': Optional[str],
    'fixes': List[Dict[str, Any]]
}
```

**示例:**
```python
issues = [
    {
        'type': 'syntax_error',
        'file': 'src/module.py',
        'line': 10,
        'message': '语法错误: 缺少冒号'
    }
]

fix_controller = FixController(project_path, config)
results = fix_controller.apply_fixes(issues, backup=True)
print(f"成功修复 {results['successful_fixes']} 个问题")
```

### WorkflowRunner

工作流运行器，用于执行自定义工作流。

```python
class WorkflowRunner:
    def __init__(self, project_root: Path, config: Optional[ConfigManager] = None)
    def load_workflow(self, workflow_file: Path) -> Dict[str, Any]
    def run_workflow(self, workflow: Dict[str, Any]) -> List[Dict[str, Any]]
    def run_workflow_file(self, workflow_file: Path) -> List[Dict[str, Any]]
    def validate_workflow(self, workflow: Dict[str, Any]) -> List[str]
```

#### 方法

**`run_workflow(workflow) -> List[Dict[str, Any]]`**

执行指定的工作流。

**参数:**
- `workflow` (Dict[str, Any]): 工作流配置字典

**返回值:**
- `List[Dict[str, Any]]`: 每个步骤的执行结果列表

**示例:**
```python
workflow = {
    'name': '测试工作流',
    'steps': [
        {
            'name': '静态分析',
            'type': 'analyze',
            'config': {'timeout': 300}
        },
        {
            'name': '运行测试',
            'type': 'test',
            'dependencies': ['静态分析']
        }
    ]
}

runner = WorkflowRunner(project_path, config)
results = runner.run_workflow(workflow)
for step_result in results:
    print(f"步骤 {step_result['name']}: {'成功' if step_result['success'] else '失败'}")
```

## 分析器模块

### StaticAnalyzer

静态代码分析器，检测代码质量问题。

```python
class StaticAnalyzer:
    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None)
    def analyze(self) -> AnalysisResult
    def analyze_file(self, file_path: Path) -> List[CodeIssue]
    def add_custom_rule(self, rule: Dict[str, Any]) -> None
    def get_supported_languages(self) -> List[str]
```

#### 方法

**`analyze() -> AnalysisResult`**

对整个项目进行静态分析。

**返回值:**
- `AnalysisResult`: 分析结果对象

**AnalysisResult 结构:**
```python
@dataclass
class AnalysisResult:
    total_issues: int
    issues_by_severity: Dict[str, int]
    issues_by_type: Dict[str, int]
    file_results: Dict[str, List[CodeIssue]]
    metrics: Dict[str, Any]
    duration: float
```

**示例:**
```python
analyzer = StaticAnalyzer(project_path)
result = analyzer.analyze()

print(f"发现 {result.total_issues} 个问题")
print(f"严重问题: {result.issues_by_severity.get('critical', 0)}")
print(f"警告: {result.issues_by_severity.get('warning', 0)}")
```

### CoverageAnalyzer

代码覆盖率分析器。

```python
class CoverageAnalyzer:
    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None)
    def analyze_coverage(self) -> CoverageMetrics
    def generate_coverage_report(self, format: str = 'html') -> Path
    def get_uncovered_lines(self, file_path: Path) -> List[int]
```

#### 方法

**`analyze_coverage() -> CoverageMetrics`**

分析代码覆盖率。

**返回值:**
- `CoverageMetrics`: 覆盖率指标对象

**CoverageMetrics 结构:**
```python
@dataclass
class CoverageMetrics:
    line_coverage: float
    branch_coverage: float
    function_coverage: float
    total_lines: int
    covered_lines: int
    total_branches: int
    covered_branches: int
    total_functions: int
    covered_functions: int
    file_coverage: Dict[str, FileCoverage]
```

**示例:**
```python
analyzer = CoverageAnalyzer(project_path)
metrics = analyzer.analyze_coverage()

print(f"行覆盖率: {metrics.line_coverage:.1f}%")
print(f"分支覆盖率: {metrics.branch_coverage:.1f}%")
print(f"函数覆盖率: {metrics.function_coverage:.1f}%")
```

### ErrorPatternAnalyzer

错误模式分析器，检测预定义的错误模式。

```python
class ErrorPatternAnalyzer:
    def __init__(self, config: Optional[Dict[str, Any]] = None)
    def analyze(self, project_root: Path) -> List[ErrorMatch]
    def add_pattern(self, pattern: ErrorPattern) -> None
    def load_patterns_from_file(self, file_path: Path) -> None
    def get_available_patterns(self) -> List[ErrorPattern]
```

#### 方法

**`analyze(project_root) -> List[ErrorMatch]`**

在项目中分析错误模式。

**参数:**
- `project_root` (Path): 项目根目录

**返回值:**
- `List[ErrorMatch]`: 匹配到的错误模式列表

**ErrorMatch 结构:**
```python
@dataclass
class ErrorMatch:
    pattern: ErrorPattern
    file_path: str
    line_number: int
    matched_text: str
    context: str
    severity: str
```

**示例:**
```python
analyzer = ErrorPatternAnalyzer()
matches = analyzer.analyze(project_path)

for match in matches:
    print(f"在 {match.file_path}:{match.line_number} 发现 {match.pattern.name}")
    print(f"严重程度: {match.severity}")
    print(f"匹配内容: {match.matched_text}")
```

### PerformanceAnalyzer

性能分析器，识别性能瓶颈。

```python
class PerformanceAnalyzer:
    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None)
    def analyze_performance(self) -> List[PerformanceIssue]
    def profile_code(self, file_path: Path) -> Dict[str, Any]
    def benchmark_functions(self, functions: List[str]) -> Dict[str, float]
```

#### 方法

**`analyze_performance() -> List[PerformanceIssue]`**

分析项目性能问题。

**返回值:**
- `List[PerformanceIssue]`: 性能问题列表

**PerformanceIssue 结构:**
```python
@dataclass
class PerformanceIssue:
    type: str
    severity: str
    file_path: str
    line_number: Optional[int]
    function_name: Optional[str]
    description: str
    suggestion: str
    impact: str
```

**示例:**
```python
analyzer = PerformanceAnalyzer(project_path)
issues = analyzer.analyze_performance()

for issue in issues:
    print(f"性能问题: {issue.description}")
    print(f"影响: {issue.impact}")
    print(f"建议: {issue.suggestion}")
```

## 集成器模块

### BuildIntegrator

构建系统集成器，支持多种构建系统。

```python
class BuildIntegrator:
    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None)
    def detect_build_system(self) -> Optional[str]
    def execute_build(self, build_system: Optional[str] = None, build_type: str = 'build') -> BuildResult
    def clean_build(self, build_system: Optional[str] = None) -> BuildResult
    def verify_build_environment(self, build_system: str) -> Dict[str, Any]
```

#### 方法

**`execute_build(build_system=None, build_type='build') -> BuildResult`**

执行项目构建。

**参数:**
- `build_system` (str, optional): 构建系统名称，自动检测如果为None
- `build_type` (str): 构建类型 ('build', 'clean', 'test', 'install')

**返回值:**
- `BuildResult`: 构建结果对象

**BuildResult 结构:**
```python
@dataclass
class BuildResult:
    success: bool
    exit_code: int
    output: str
    error_output: str
    duration: float
    build_artifacts: List[str]
    warnings_count: int
    errors_count: int
```

**示例:**
```python
integrator = BuildIntegrator(project_path)
result = integrator.execute_build()

if result.success:
    print(f"构建成功，耗时 {result.duration:.2f}s")
    print(f"构建产物: {result.build_artifacts}")
else:
    print(f"构建失败: {result.error_output}")
```

### TestIntegrator

测试框架集成器，支持多种测试框架。

```python
class TestIntegrator:
    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None)
    def detect_test_framework(self) -> Optional[str]
    def execute_tests(self, framework: Optional[str] = None, include_coverage: bool = False) -> TestResult
    def get_test_configuration(self) -> Dict[str, Any]
    def verify_test_environment(self, framework: str) -> Dict[str, Any]
```

#### 方法

**`execute_tests(framework=None, include_coverage=False) -> TestResult`**

执行测试。

**参数:**
- `framework` (str, optional): 测试框架名称，自动检测如果为None
- `include_coverage` (bool): 是否收集覆盖率信息

**返回值:**
- `TestResult`: 测试结果对象

**TestResult 结构:**
```python
@dataclass
class TestResult:
    framework: str
    total_tests: int
    passed_tests: int
    failed_tests: int
    skipped_tests: int
    errors: int
    duration: float
    output: str
    error_output: str
    test_cases: List[Dict[str, Any]]
    coverage_info: Optional[Dict[str, Any]]
```

**示例:**
```python
integrator = TestIntegrator(project_path)
result = integrator.execute_tests(include_coverage=True)

print(f"测试框架: {result.framework}")
print(f"通过率: {result.passed_tests / result.total_tests * 100:.1f}%")
if result.coverage_info:
    print(f"覆盖率: {result.coverage_info['line_coverage']:.1f}%")
```

### CICDIntegrator

CI/CD平台集成器。

```python
class CICDIntegrator:
    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None)
    def detect_ci_platform(self) -> Optional[str]
    def create_pipeline_config(self, platform: str, pipeline_config: Dict[str, Any]) -> bool
    def trigger_pipeline(self, platform: str, branch: str = 'main') -> Dict[str, Any]
    def get_pipeline_status(self, platform: str, pipeline_id: Optional[str] = None) -> Dict[str, Any]
```

#### 方法

**`create_pipeline_config(platform, pipeline_config) -> bool`**

为指定平台创建流水线配置文件。

**参数:**
- `platform` (str): CI/CD平台名称
- `pipeline_config` (Dict[str, Any]): 流水线配置

**返回值:**
- `bool`: 创建成功返回True

**示例:**
```python
integrator = CICDIntegrator(project_path)

pipeline_config = {
    'name': 'CI/CD Pipeline',
    'branches': ['main', 'develop'],
    'build': {'enabled': True},
    'test': {'enabled': True, 'coverage': True}
}

success = integrator.create_pipeline_config('github_actions', pipeline_config)
if success:
    print("GitHub Actions 工作流配置已创建")
```

## 配置模块

### ConfigManager

配置管理器，处理系统配置。

```python
class ConfigManager:
    def __init__(self, config_file: Optional[Path] = None, project_root: Optional[Path] = None)
    def load_config(self) -> Dict[str, Any]
    def save_config(self, config_file: Optional[Path] = None) -> bool
    def get(self, key: str, default: Any = None) -> Any
    def set(self, key: str, value: Any) -> None
    def update(self, updates: Dict[str, Any]) -> None
    def validate_config(self) -> List[str]
    def get_project_config(self) -> Dict[str, Any]
```

#### 方法

**`get(key, default=None) -> Any`**

获取配置值，支持点号分隔的嵌套键。

**参数:**
- `key` (str): 配置键，支持嵌套（如 'build.timeout'）
- `default` (Any): 默认值

**返回值:**
- `Any`: 配置值

**示例:**
```python
config = ConfigManager()

# 获取顶层配置
timeout = config.get('build.timeout', 300)

# 获取嵌套配置
test_framework = config.get('test.framework', 'pytest')

# 获取列表配置
output_formats = config.get('reporting.output_formats', ['json'])
```

**`set(key, value) -> None`**

设置配置值。

**参数:**
- `key` (str): 配置键
- `value` (Any): 配置值

**示例:**
```python
config.set('build.timeout', 600)
config.set('test.coverage_threshold', 90.0)
config.set('reporting.output_formats', ['html', 'json'])
```

**`update(updates) -> None`**

批量更新配置。

**参数:**
- `updates` (Dict[str, Any]): 更新字典

**示例:**
```python
config.update({
    'build': {
        'timeout': 600,
        'parallel_jobs': 4
    },
    'test': {
        'coverage_threshold': 85.0
    }
})
```

## 工具模块

### GitManager

Git仓库管理器。

```python
class GitManager:
    def __init__(self, repo_path: Optional[Path] = None)
    def get_status(self) -> Dict[str, Any]
    def get_commit_history(self, limit: int = 10) -> List[Dict[str, Any]]
    def add_files(self, file_paths: List[Path]) -> bool
    def commit_changes(self, message: str, allow_empty: bool = False) -> bool
    def push_to_remote(self, remote_name: str = 'origin') -> bool
    def create_tag(self, tag_name: str, message: Optional[str] = None) -> bool
```

#### 方法

**`get_status() -> Dict[str, Any]`**

获取Git仓库状态。

**返回值:**
- `Dict[str, Any]`: 仓库状态信息

**示例:**
```python
git_manager = GitManager(project_path)
status = git_manager.get_status()

print(f"当前分支: {status['current_branch']}")
print(f"是否有未提交的更改: {not status['is_clean']}")
print(f"修改的文件: {status['modified_files']}")
```

### 文件操作工具

```python
def find_files(directory: Path, patterns: Optional[List[str]] = None) -> List[Path]
def read_file(file_path: Path, encoding: str = 'utf-8') -> Optional[str]
def write_file(file_path: Path, content: str, encoding: str = 'utf-8') -> bool
def backup_file(file_path: Path, backup_suffix: str = '.backup') -> bool
def save_json(data: Dict[str, Any], file_path: Path) -> bool
def load_json(file_path: Path) -> Optional[Dict[str, Any]]
```

#### 示例

```python
from ai_dev_system.utils import find_files, read_file, write_file, save_json

# 查找Python文件
python_files = find_files(project_path, patterns=['*.py'])

# 读取文件内容
content = read_file(project_path / 'README.md')

# 写入文件
write_file(project_path / 'output.txt', 'Hello World')

# 保存JSON数据
data = {'name': 'project', 'version': '1.0.0'}
save_json(data, project_path / 'project_info.json')
```

## 报告模块

### ReportGenerator

报告生成器，生成多格式的分析报告。

```python
class ReportGenerator:
    def __init__(self, output_dir: Path, config: Optional[ConfigManager] = None)
    def generate_report(self, data: Dict[str, Any], format: str = 'html', template: Optional[str] = None) -> Optional[Path]
    def generate_dashboard_data(self, data: Dict[str, Any]) -> Dict[str, Any]
    def create_custom_template(self, template_name: str, template_content: str) -> bool
```

#### 方法

**`generate_report(data, format='html', template=None) -> Optional[Path]`**

生成分析报告。

**参数:**
- `data` (Dict[str, Any]): 分析数据
- `format` (str): 报告格式 ('html', 'json', 'markdown')
- `template` (str, optional): 自定义模板名称

**返回值:**
- `Optional[Path]`: 生成的报告文件路径

**示例:**
```python
generator = ReportGenerator(Path('output'), config)

# 生成HTML报告
html_report = generator.generate_report(analysis_data, format='html')
print(f"HTML报告: {html_report}")

# 生成JSON报告
json_report = generator.generate_report(analysis_data, format='json')
print(f"JSON报告: {json_report}")

# 使用自定义模板
custom_report = generator.generate_report(
    analysis_data,
    format='html',
    template='my_template'
)
```

## 数据类型

### CodeIssue

代码问题数据结构。

```python
@dataclass
class CodeIssue:
    type: str
    severity: str
    file_path: str
    line_number: Optional[int]
    column_number: Optional[int]
    message: str
    description: Optional[str]
    suggestion: Optional[str]
    rule_id: Optional[str]
    category: Optional[str]
```

### ErrorPattern

错误模式定义。

```python
@dataclass
class ErrorPattern:
    name: str
    description: str
    pattern: str
    severity: str
    category: str
    language: Optional[str]
    suggestion: Optional[str]
```

### WorkflowStep

工作流步骤定义。

```python
@dataclass
class WorkflowStep:
    name: str
    type: str
    description: Optional[str]
    command: Optional[str]
    dependencies: List[str]
    condition: Optional[str]
    timeout: Optional[int]
    retry_count: int
    enabled: bool
    config: Dict[str, Any]
```

## 异常类

### 系统异常

```python
class AISystemError(Exception):
    """系统基础异常类"""
    pass

class ConfigurationError(AISystemError):
    """配置错误"""
    pass

class AnalysisError(AISystemError):
    """分析错误"""
    pass

class BuildError(AISystemError):
    """构建错误"""
    pass

class TestError(AISystemError):
    """测试错误"""
    pass

class ReportGenerationError(AISystemError):
    """报告生成错误"""
    pass
```

## 常量

### 严重程度级别

```python
SEVERITY_CRITICAL = "critical"
SEVERITY_HIGH = "high"
SEVERITY_MEDIUM = "medium"
SEVERITY_LOW = "low"
SEVERITY_INFO = "info"
```

### 问题类型

```python
ISSUE_TYPE_SYNTAX = "syntax"
ISSUE_TYPE_SECURITY = "security"
ISSUE_TYPE_PERFORMANCE = "performance"
ISSUE_TYPE_MAINTAINABILITY = "maintainability"
ISSUE_TYPE_STYLE = "style"
```

### 支持的语言

```python
LANGUAGES = [
    "python", "java", "cpp", "c", "javascript",
    "typescript", "go", "rust", "csharp", "php"
]
```

---

此API参考文档涵盖了AI Development System的主要接口和数据结构。如需更详细的信息，请参考源代码中的文档字符串和类型注解。