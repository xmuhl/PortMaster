#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Configuration Manager

This module provides configuration management functionality
for the AI development system.
"""

import os
import yaml
import json
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any, Union
from dataclasses import dataclass, asdict

from ..utils.file_utils import read_file, write_file, ensure_directory, save_json, load_json
from ..utils.log_utils import get_logger
from ..utils.validation_utils import validate_config


@dataclass
class SystemConfig:
    """System configuration data class"""
    name: str = "AI Development System"
    version: str = "1.0.0"
    debug_mode: bool = False
    log_level: str = "INFO"
    temp_directory: str = "temp"
    output_directory: str = "output"


@dataclass
class BuildConfig:
    """Build configuration data class"""
    enabled: bool = True
    timeout: int = 300
    parallel_jobs: int = 1
    clean_command: Optional[str] = None
    artifacts: List[str] = None

    def __post_init__(self):
        if self.artifacts is None:
            self.artifacts = ["build/", "dist/", "target/", "bin/", "*.exe", "*.jar"]


@dataclass
class TestConfig:
    """Test configuration data class"""
    enabled: bool = True
    timeout: int = 600
    parallel_jobs: int = 1
    coverage_threshold: float = 80.0
    collect_coverage: bool = True
    coverage_formats: List[str] = None
    test_patterns: Dict[str, List[str]] = None

    def __post_init__(self):
        if self.coverage_formats is None:
            self.coverage_formats = ["json", "xml", "html"]
        if self.test_patterns is None:
            self.test_patterns = {
                "python": ["test_*.py", "*_test.py", "tests/test_*.py"],
                "javascript": ["*.test.js", "*.test.ts", "**/__tests__/**/*.js"],
                "java": ["**/*Test.java", "**/*Tests.java"],
                "cpp": ["**/*_test.cpp", "**/*_test.cc"]
            }


@dataclass
class StaticAnalysisConfig:
    """Static analysis configuration data class"""
    enabled: bool = True
    timeout: int = 300
    tools: Dict[str, List[str]] = None
    complexity_threshold: int = 10
    duplicate_threshold: int = 5
    security_scan: bool = True

    def __post_init__(self):
        if self.tools is None:
            self.tools = {
                "python": ["pylint", "flake8", "mypy"],
                "javascript": ["eslint", "jshint"],
                "java": ["spotbugs", "checkstyle"],
                "cpp": ["cppcheck", "clang-tidy"]
            }


@dataclass
class ReportingConfig:
    """Reporting configuration data class"""
    enabled: bool = True
    output_formats: List[str] = None
    output_directory: str = "reports"
    include_metrics: bool = True
    include_charts: bool = True
    dashboard: bool = True
    templates_directory: str = "templates"

    def __post_init__(self):
        if self.output_formats is None:
            self.output_formats = ["json", "markdown", "html"]


class ConfigManager:
    """
    Configuration manager for the AI development system
    """

    def __init__(self, config_file: Optional[Path] = None, project_root: Optional[Path] = None):
        """
        Initialize configuration manager

        Args:
            config_file: Path to configuration file
            project_root: Project root directory
        """
        self.project_root = project_root or Path.cwd()
        self.config_file = config_file or self._find_config_file()
        self.logger = get_logger(__name__)

        # Default configuration
        self.config = {}

        # Load configuration
        self.load_config()

    def _find_config_file(self) -> Path:
        """Find configuration file in project directory"""
        config_names = [
            'ai_dev_config.yaml',
            'ai_dev_config.yml',
            'ai_dev_config.json',
            '.ai_dev_config.yaml',
            '.ai_dev_config.yml',
            '.ai_dev_config.json'
        ]

        for config_name in config_names:
            config_path = self.project_root / config_name
            if config_path.exists():
                return config_path

        # Return default config file path
        return self.project_root / 'ai_dev_config.yaml'

    def load_config(self) -> Dict[str, Any]:
        """
        Load configuration from file

        Returns:
            Loaded configuration dictionary
        """
        try:
            # Load default configuration
            default_config_file = Path(__file__).parent / 'default_config.yaml'
            default_config = self._load_yaml_file(default_config_file)
            if default_config:
                self.config = default_config
            else:
                self.logger.warning("Could not load default configuration, using built-in defaults")
                self.config = self._get_builtin_defaults()

            # Override with user configuration if file exists
            if self.config_file.exists():
                user_config = self._load_user_config()
                if user_config:
                    self.config = self._merge_configs(self.config, user_config)
                    self.logger.info(f"Loaded configuration from {self.config_file}")

            # Validate configuration
            self.config = validate_config(self.config)

            # Expand environment variables
            self.config = self._expand_env_vars(self.config)

            return self.config

        except Exception as e:
            self.logger.error(f"Failed to load configuration: {e}")
            self.config = self._get_builtin_defaults()
            return self.config

    def _load_yaml_file(self, file_path: Path) -> Optional[Dict[str, Any]]:
        """Load YAML file"""
        try:
            content = read_file(file_path)
            if content:
                return yaml.safe_load(content)
        except Exception as e:
            self.logger.error(f"Failed to load YAML file {file_path}: {e}")
        return None

    def _load_json_file(self, file_path: Path) -> Optional[Dict[str, Any]]:
        """Load JSON file"""
        try:
            return load_json(file_path)
        except Exception as e:
            self.logger.error(f"Failed to load JSON file {file_path}: {e}")
        return None

    def _load_user_config(self) -> Optional[Dict[str, Any]]:
        """Load user configuration file"""
        if self.config_file.suffix.lower() in ['.yaml', '.yml']:
            return self._load_yaml_file(self.config_file)
        elif self.config_file.suffix.lower() == '.json':
            return self._load_json_file(self.config_file)
        else:
            self.logger.error(f"Unsupported configuration file format: {self.config_file.suffix}")
            return None

    def _get_builtin_defaults(self) -> Dict[str, Any]:
        """Get built-in default configuration"""
        return {
            'system': asdict(SystemConfig()),
            'build': asdict(BuildConfig()),
            'test': asdict(TestConfig()),
            'static_analysis': asdict(StaticAnalysisConfig()),
            'reporting': asdict(ReportingConfig()),
            'performance': {
                'enabled': True,
                'timeout': 300,
                'profile_memory': True,
                'profile_cpu': True,
                'complexity_analysis': True
            },
            'error_patterns': {
                'enabled': True,
                'custom_patterns_file': None,
                'max_issues_per_file': 100,
                'severity_levels': ['critical', 'high', 'medium', 'low', 'info']
            },
            'logging': {
                'level': 'INFO',
                'file': 'ai_dev_system.log',
                'format': '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                'max_file_size': 10485760,
                'backup_count': 5,
                'console_output': True
            },
            'project': {
                'type': None,
                'language': None,
                'build_system': None,
                'test_framework': None,
                'source_directories': ['src/', 'lib/', 'app/'],
                'test_directories': ['tests/', 'test/', '__tests__/'],
                'exclude_patterns': ['.git/', 'node_modules/', '__pycache__/', '*.pyc']
            }
        }

    def _merge_configs(self, base_config: Dict[str, Any], override_config: Dict[str, Any]) -> Dict[str, Any]:
        """
        Merge two configuration dictionaries

        Args:
            base_config: Base configuration
            override_config: Override configuration

        Returns:
            Merged configuration
        """
        merged = base_config.copy()

        for key, value in override_config.items():
            if key in merged and isinstance(merged[key], dict) and isinstance(value, dict):
                merged[key] = self._merge_configs(merged[key], value)
            else:
                merged[key] = value

        return merged

    def _expand_env_vars(self, config: Any) -> Any:
        """
        Expand environment variables in configuration values

        Args:
            config: Configuration value or dictionary

        Returns:
            Configuration with expanded environment variables
        """
        if isinstance(config, dict):
            return {key: self._expand_env_vars(value) for key, value in config.items()}
        elif isinstance(config, list):
            return [self._expand_env_vars(item) for item in config]
        elif isinstance(config, str) and config.startswith('${') and config.endswith('}'):
            env_var = config[2:-1]
            default_value = None
            if ':' in env_var:
                env_var, default_value = env_var.split(':', 1)
            return os.environ.get(env_var, default_value or config)
        else:
            return config

    def save_config(self, config_file: Optional[Path] = None) -> bool:
        """
        Save configuration to file

        Args:
            config_file: Path to save configuration (uses default if None)

        Returns:
            True if successful, False otherwise
        """
        target_file = config_file or self.config_file

        try:
            ensure_directory(target_file.parent)

            if target_file.suffix.lower() in ['.yaml', '.yml']:
                content = yaml.dump(self.config, default_flow_style=False, indent=2)
                return write_file(target_file, content)
            elif target_file.suffix.lower() == '.json':
                return save_json(self.config, target_file)
            else:
                self.logger.error(f"Unsupported configuration file format: {target_file.suffix}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to save configuration: {e}")
            return False

    def get(self, key: str, default: Any = None) -> Any:
        """
        Get configuration value by key

        Args:
            key: Configuration key (supports dot notation)
            default: Default value if key not found

        Returns:
            Configuration value
        """
        keys = key.split('.')
        value = self.config

        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default

        return value

    def set(self, key: str, value: Any) -> None:
        """
        Set configuration value by key

        Args:
            key: Configuration key (supports dot notation)
            value: Value to set
        """
        keys = key.split('.')
        config = self.config

        for k in keys[:-1]:
            if k not in config:
                config[k] = {}
            config = config[k]

        config[keys[-1]] = value

    def update(self, updates: Dict[str, Any]) -> None:
        """
        Update configuration with new values

        Args:
            updates: Dictionary of updates
        """
        self.config = self._merge_configs(self.config, updates)

    def reset_to_defaults(self) -> None:
        """Reset configuration to defaults"""
        default_config_file = Path(__file__).parent / 'default_config.yaml'
        if default_config_file.exists():
            self.config = self._load_yaml_file(default_config_file) or self._get_builtin_defaults()
        else:
            self.config = self._get_builtin_defaults()

    def create_user_config(self, config_file: Optional[Path] = None) -> bool:
        """
        Create user configuration file from current configuration

        Args:
            config_file: Path for user configuration file

        Returns:
            True if successful, False otherwise
        """
        target_file = config_file or self.config_file

        try:
            # Create a minimal user config with only non-default values
            default_config = self._get_builtin_defaults()
            user_config = self._get_user_config_diff(self.config, default_config)

            if user_config:
                ensure_directory(target_file.parent)
                content = yaml.dump(user_config, default_flow_style=False, indent=2)
                result = write_file(target_file, content)
                if result:
                    self.logger.info(f"Created user configuration file: {target_file}")
                return result
            else:
                self.logger.info("No custom configuration to save")
                return True

        except Exception as e:
            self.logger.error(f"Failed to create user configuration: {e}")
            return False

    def _get_user_config_diff(self, config: Dict[str, Any], defaults: Dict[str, Any]) -> Dict[str, Any]:
        """
        Get differences between current config and defaults

        Args:
            config: Current configuration
            defaults: Default configuration

        Returns:
            Dictionary with only non-default values
        """
        diff = {}

        for key, value in config.items():
            if key not in defaults:
                diff[key] = value
            elif isinstance(value, dict) and isinstance(defaults.get(key), dict):
                nested_diff = self._get_user_config_diff(value, defaults[key])
                if nested_diff:
                    diff[key] = nested_diff
            elif value != defaults.get(key):
                diff[key] = value

        return diff

    def validate_config(self) -> List[str]:
        """
        Validate current configuration

        Returns:
            List of validation errors
        """
        errors = []

        # Validate system configuration
        if 'system' in self.config:
            system_config = self.config['system']
            if 'name' not in system_config or not system_config['name']:
                errors.append("System name is required")
            if 'version' not in system_config or not system_config['version']:
                errors.append("System version is required")

        # Validate build configuration
        if 'build' in self.config:
            build_config = self.config['build']
            if 'timeout' in build_config and (not isinstance(build_config['timeout'], int) or build_config['timeout'] <= 0):
                errors.append("Build timeout must be a positive integer")
            if 'parallel_jobs' in build_config and (not isinstance(build_config['parallel_jobs'], int) or build_config['parallel_jobs'] <= 0):
                errors.append("Build parallel_jobs must be a positive integer")

        # Validate test configuration
        if 'test' in self.config:
            test_config = self.config['test']
            if 'timeout' in test_config and (not isinstance(test_config['timeout'], int) or test_config['timeout'] <= 0):
                errors.append("Test timeout must be a positive integer")
            if 'coverage_threshold' in test_config and (not isinstance(test_config['coverage_threshold'], (int, float)) or not (0 <= test_config['coverage_threshold'] <= 100)):
                errors.append("Coverage threshold must be between 0 and 100")

        # Validate reporting configuration
        if 'reporting' in self.config:
            reporting_config = self.config['reporting']
            if 'output_formats' in reporting_config:
                valid_formats = ['json', 'markdown', 'html', 'pdf']
                for fmt in reporting_config['output_formats']:
                    if fmt not in valid_formats:
                        errors.append(f"Invalid output format: {fmt}")

        return errors

    def get_project_config(self) -> Dict[str, Any]:
        """
        Get project-specific configuration

        Returns:
            Project configuration dictionary
        """
        project_config = self.get('project', {})

        # Auto-detect project settings if not specified
        if not project_config.get('language'):
            project_config['language'] = self._detect_project_language()

        if not project_config.get('build_system'):
            project_config['build_system'] = self._detect_build_system()

        if not project_config.get('test_framework'):
            project_config['test_framework'] = self._detect_test_framework()

        return project_config

    def _detect_project_language(self) -> Optional[str]:
        """Detect project programming language"""
        language_patterns = {
            'python': ['*.py', 'requirements.txt', 'setup.py', 'pyproject.toml'],
            'javascript': ['*.js', '*.ts', 'package.json', 'webpack.config.js'],
            'java': ['*.java', 'pom.xml', 'build.gradle', 'src/main/java/'],
            'cpp': ['*.cpp', '*.c', '*.h', '*.hpp', 'CMakeLists.txt', 'Makefile'],
            'csharp': ['*.cs', '*.csx', '*.sln', '*.csproj'],
            'go': ['*.go', 'go.mod', 'go.sum'],
            'rust': ['*.rs', 'Cargo.toml'],
            'php': ['*.php', 'composer.json'],
            'ruby': ['*.rb', 'Gemfile', 'Rakefile']
        }

        for language, patterns in language_patterns.items():
            for pattern in patterns:
                if list(self.project_root.glob(pattern)):
                    return language

        return None

    def _detect_build_system(self) -> Optional[str]:
        """Detect build system"""
        build_files = {
            'make': ['Makefile', 'makefile'],
            'cmake': ['CMakeLists.txt'],
            'maven': ['pom.xml'],
            'gradle': ['build.gradle', 'build.gradle.kts'],
            'npm': ['package.json'],
            'webpack': ['webpack.config.js'],
            'msbuild': ['*.sln', '*.vcxproj'],
            'xcodebuild': ['*.xcodeproj', '*.xcworkspace']
        }

        for system, files in build_files.items():
            for file_pattern in files:
                if list(self.project_root.glob(file_pattern)):
                    return system

        return None

    def _detect_test_framework(self) -> Optional[str]:
        """Detect test framework"""
        test_patterns = {
            'pytest': ['test_*.py', '*_test.py', 'pytest.ini', 'pyproject.toml'],
            'unittest': ['test_*.py', '*_test.py'],
            'jest': ['*.test.js', '*.test.ts', 'jest.config.js', 'package.json'],
            'mocha': ['*.test.js', '*_test.js', 'mocha.opts'],
            'junit': ['**/*Test.java', '**/*Tests.java', 'pom.xml', 'build.gradle'],
            'googletest': ['**/*_test.cpp', '**/*_test.cc', 'CMakeLists.txt']
        }

        for framework, patterns in test_patterns.items():
            for pattern in patterns:
                if list(self.project_root.glob(pattern)):
                    return framework

        return None

    def export_config(self, format: str = 'yaml') -> Optional[str]:
        """
        Export configuration to string

        Args:
            format: Export format ('yaml' or 'json')

        Returns:
            Configuration string or None
        """
        try:
            if format.lower() == 'yaml':
                return yaml.dump(self.config, default_flow_style=False, indent=2)
            elif format.lower() == 'json':
                return json.dumps(self.config, indent=2, default=str)
            else:
                self.logger.error(f"Unsupported export format: {format}")
                return None
        except Exception as e:
            self.logger.error(f"Failed to export configuration: {e}")
            return None

    def import_config(self, config_str: str, format: str = 'yaml') -> bool:
        """
        Import configuration from string

        Args:
            config_str: Configuration string
            format: Import format ('yaml' or 'json')

        Returns:
            True if successful, False otherwise
        """
        try:
            if format.lower() == 'yaml':
                imported_config = yaml.safe_load(config_str)
            elif format.lower() == 'json':
                imported_config = json.loads(config_str)
            else:
                self.logger.error(f"Unsupported import format: {format}")
                return False

            if imported_config and isinstance(imported_config, dict):
                self.config = validate_config(imported_config)
                return True
            else:
                self.logger.error("Invalid configuration format")
                return False

        except Exception as e:
            self.logger.error(f"Failed to import configuration: {e}")
            return False