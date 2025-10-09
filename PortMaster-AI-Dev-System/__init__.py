"""
AI Development System

A comprehensive, automated AI-driven development system for code analysis,
testing, reporting, and improvement across multiple programming languages
and project types.

Features:
- Multi-language static analysis (Python, Java, C++, JavaScript, etc.)
- Automated build and test integration
- Coverage analysis and reporting
- Performance profiling and optimization suggestions
- Error pattern detection and analysis
- Automated code fixing capabilities
- CI/CD pipeline integration
- Comprehensive reporting (JSON, Markdown, HTML)
- Extensible workflow system
- Project-agnostic design

Quick Start:
    from ai_dev_system import MasterController, ConfigManager

    config = ConfigManager()
    controller = MasterController(Path("/path/to/project"), config)
    results = controller.run_full_workflow()

Command Line Usage:
    python main.py analyze /path/to/project
    python main.py test /path/to/project --coverage
    python main.py report /path/to/project --format html
"""

__version__ = "1.0.0"
__author__ = "AI Development System"
__email__ = "ai-dev-system@example.com"

# Core components
from .core.master_controller import MasterController
from .core.fix_controller import FixController
from .core.workflow_runner import WorkflowRunner

# Analyzers
from .analyzers.static_analyzer import StaticAnalyzer
from .analyzers.coverage_analyzer import CoverageAnalyzer
from .analyzers.error_pattern_analyzer import ErrorPatternAnalyzer
from .analyzers.performance_analyzer import PerformanceAnalyzer

# Integrators
from .integrators.build_integrator import BuildIntegrator
from .integrators.test_integrator import TestIntegrator
from .integrators.cicd_integrator import CICDIntegrator

# Configuration
from .config.config_manager import (
    ConfigManager,
    SystemConfig,
    BuildConfig,
    TestConfig,
    StaticAnalysisConfig,
    ReportingConfig
)

# Utilities
from .utils.file_utils import (
    find_files, read_file, write_file, backup_file, restore_file,
    ensure_directory, save_json, load_json
)
from .utils.log_utils import setup_logging
from .utils.validation_utils import validate_config, validate_workflow_config
from .utils.git_utils import GitManager, is_git_repository, initialize_repository, clone_repository

# Reporters
from .reporters.report_generator import ReportGenerator

__all__ = [
    # Core
    'MasterController',
    'FixController',
    'WorkflowRunner',

    # Analyzers
    'StaticAnalyzer',
    'CoverageAnalyzer',
    'ErrorPatternAnalyzer',
    'PerformanceAnalyzer',

    # Integrators
    'BuildIntegrator',
    'TestIntegrator',
    'CICDIntegrator',

    # Configuration
    'ConfigManager',
    'SystemConfig',
    'BuildConfig',
    'TestConfig',
    'StaticAnalysisConfig',
    'ReportingConfig',

    # Utilities
    'find_files',
    'read_file',
    'write_file',
    'backup_file',
    'restore_file',
    'ensure_directory',
    'save_json',
    'load_json',
    'setup_logging',
    'validate_config',
    'validate_workflow_config',
    'GitManager',
    'is_git_repository',
    'initialize_repository',
    'clone_repository',

    # Reporters
    'ReportGenerator'
]


def get_version() -> str:
    """Get version information"""
    return __version__


def analyze_project(project_path, config_file=None, **kwargs):
    """
    Quick analyze function for simple usage

    Args:
        project_path: Path to project directory
        config_file: Optional configuration file
        **kwargs: Additional configuration options

    Returns:
        Analysis results dictionary
    """
    from pathlib import Path

    project_path = Path(project_path)
    config = ConfigManager(config_file, project_path)

    # Apply kwargs to config
    for key, value in kwargs.items():
        config.set(key, value)

    controller = MasterController(project_path, config)
    return controller.run_full_workflow()


def create_config(project_path, config_file=None, **options):
    """
    Create configuration for a project

    Args:
        project_path: Path to project directory
        config_file: Optional configuration file path
        **options: Configuration options

    Returns:
        ConfigManager instance
    """
    from pathlib import Path

    project_path = Path(project_path)
    config = ConfigManager(config_file, project_path)

    # Apply options to config
    for key, value in options.items():
        config.set(key, value)

    # Save configuration if file specified
    if config_file:
        config.save_config()

    return config


def run_command(project_path, command='analyze', **kwargs):
    """
    Run system command programmatically

    Args:
        project_path: Path to project directory
        command: Command to run (analyze, build, test, fix, report)
        **kwargs: Command-specific arguments

    Returns:
        Command results
    """
    import sys
    from pathlib import Path

    project_path = Path(project_path)

    # Create a mock args object
    class MockArgs:
        def __init__(self):
            self.project_path = project_path
            self.config = kwargs.get('config_file')
            self.output = Path(kwargs.get('output', 'output'))
            self.log_level = kwargs.get('log_level', 'INFO')
            self.debug = kwargs.get('debug', False)

    args = MockArgs()

    # Load configuration
    config = ConfigManager(args.config, args.project_path)

    # Import command handlers
    from main import (
        handle_analyze_command,
        handle_build_command,
        handle_test_command,
        handle_fix_command,
        handle_report_command
    )

    # Setup logging
    from main import setup_system_logging
    setup_system_logging(config, args.debug)

    # Apply command-specific arguments
    for key, value in kwargs.items():
        if key != 'config_file':
            setattr(args, key, value)

    # Run appropriate command
    if command == 'analyze':
        return handle_analyze_command(args, config)
    elif command == 'build':
        return handle_build_command(args, config)
    elif command == 'test':
        return handle_test_command(args, config)
    elif command == 'fix':
        return handle_fix_command(args, config)
    elif command == 'report':
        return handle_report_command(args, config)
    else:
        raise ValueError(f"Unknown command: {command}")