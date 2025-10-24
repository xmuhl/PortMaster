#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Validation Utilities

This module provides validation functions for configuration
data, workflow definitions, and other system inputs.
"""

import logging
from typing import Dict, List, Any, Optional, Union
from pathlib import Path

from .file_utils import validate_file_path
from .log_utils import get_logger


def validate_config(config: Dict[str, Any]) -> Dict[str, Any]:
    """
    Validate system configuration

    Args:
        config: Configuration dictionary

    Returns:
        Validated configuration
    """
    logger = get_logger(__name__)
    validated_config = {}

    # Required configuration sections
    required_sections = ['build', 'test', 'analysis', 'reporting']
    for section in required_sections:
        if section not in config:
            logger.warning(f"Missing required config section: {section}")
            validated_config[section] = _get_default_config_section(section)
        else:
            validated_config[section] = _validate_config_section(section, config[section])

    # Optional configuration sections
    optional_sections = ['logging', 'performance', 'security']
    for section in optional_sections:
        if section in config:
            validated_config[section] = _validate_config_section(section, config[section])

    return validated_config


def _get_default_config_section(section: str) -> Dict[str, Any]:
    """Get default configuration for a section"""
    defaults = {
        'build': {
            'commands': ['make', 'cmake --build .', 'mvn compile'],
            'timeout': 300,
            'clean_command': None,
            'parallel_jobs': 1
        },
        'test': {
            'frameworks': ['pytest', 'unittest', 'jest', 'maven test'],
            'timeout': 600,
            'coverage_threshold': 80.0,
            'parallel_jobs': 1
        },
        'analysis': {
            'static_analysis_tools': ['pylint', 'eslint', 'cppcheck'],
            'complexity_threshold': 10,
            'duplicate_threshold': 5,
            'security_scan': True
        },
        'reporting': {
            'output_dir': 'reports',
            'formats': ['json', 'markdown', 'html'],
            'include_metrics': True,
            'include_charts': False
        },
        'logging': {
            'level': 'INFO',
            'file': 'ai_dev_system.log',
            'format': '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            'max_file_size': 10485760  # 10MB
        },
        'performance': {
            'enable_profiling': False,
            'memory_limit': '1GB',
            'timeout_multiplier': 1.5
        },
        'security': {
            'scan_secrets': True,
            'scan_dependencies': True,
            'scan_credentials': True
        }
    }
    return defaults.get(section, {})


def _validate_config_section(section: str, section_config: Dict[str, Any]) -> Dict[str, Any]:
    """Validate a specific configuration section"""
    logger = get_logger(__name__)
    validated_config = {}

    # Build configuration validation
    if section == 'build':
        validated_config['commands'] = _validate_list_field(
            section_config, 'commands', str, required=True
        )
        validated_config['timeout'] = _validate_int_field(
            section_config, 'timeout', min_value=1, default=300
        )
        validated_config['parallel_jobs'] = _validate_int_field(
            section_config, 'parallel_jobs', min_value=1, default=1
        )

    # Test configuration validation
    elif section == 'test':
        validated_config['frameworks'] = _validate_list_field(
            section_config, 'frameworks', str, required=True
        )
        validated_config['timeout'] = _validate_int_field(
            section_config, 'timeout', min_value=1, default=600
        )
        validated_config['coverage_threshold'] = _validate_float_field(
            section_config, 'coverage_threshold', min_value=0, max_value=100, default=80.0
        )
        validated_config['parallel_jobs'] = _validate_int_field(
            section_config, 'parallel_jobs', min_value=1, default=1
        )

    # Analysis configuration validation
    elif section == 'analysis':
        validated_config['static_analysis_tools'] = _validate_list_field(
            section_config, 'static_analysis_tools', str
        )
        validated_config['complexity_threshold'] = _validate_int_field(
            section_config, 'complexity_threshold', min_value=1, default=10
        )
        validated_config['duplicate_threshold'] = _validate_int_field(
            section_config, 'duplicate_threshold', min_value=1, default=5
        )

    # Reporting configuration validation
    elif section == 'reporting':
        validated_config['output_dir'] = _validate_string_field(
            section_config, 'output_dir', default='reports'
        )
        validated_config['formats'] = _validate_list_field(
            section_config, 'formats', str, allowed_values=['json', 'markdown', 'html', 'pdf']
        )
        validated_config['include_metrics'] = _validate_bool_field(
            section_config, 'include_metrics', default=True
        )

    # Logging configuration validation
    elif section == 'logging':
        validated_config['level'] = _validate_log_level(section_config.get('level'))
        validated_config['file'] = _validate_string_field(
            section_config, 'file', default='ai_dev_system.log'
        )
        validated_config['format'] = _validate_string_field(
            section_config, 'format',
            default='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        validated_config['max_file_size'] = _validate_int_field(
            section_config, 'max_file_size', min_value=1024, default=10485760
        )

    # Copy other fields
    for key, value in section_config.items():
        if key not in validated_config:
            validated_config[key] = value

    return validated_config


def validate_workflow_config(workflow_config: Dict[str, Any]) -> bool:
    """
    Validate workflow configuration

    Args:
        workflow_config: Workflow configuration dictionary

    Returns:
        True if valid, False otherwise
    """
    logger = get_logger(__name__)

    # Check required fields
    required_fields = ['name', 'steps']
    for field in required_fields:
        if field not in workflow_config:
            logger.error(f"Workflow missing required field: {field}")
            return False

    # Validate steps
    if not isinstance(workflow_config.get('steps'), list):
        logger.error("Workflow steps must be a list")
        return False

    for i, step in enumerate(workflow_config['steps']):
        if not validate_step_config(step, f"step {i+1}"):
            return False

    # Validate optional fields
    if 'description' in workflow_config:
        if not isinstance(workflow_config['description'], str):
            logger.error("Workflow description must be a string")
            return False

    if 'version' in workflow_config:
        if not isinstance(workflow_config['version'], str):
            logger.error("Workflow version must be a string")
            return False

    return True


def validate_step_config(step_config: Dict[str, Any], step_name: str = "step") -> bool:
    """
    Validate a single workflow step configuration

    Args:
        step_config: Step configuration dictionary
        step_name: Step name for error reporting

    Returns:
        True if valid, False otherwise
    """
    logger = get_logger(__name__)

    # Check required fields
    required_fields = ['type']
    for field in required_fields:
        if field not in step_config:
            logger.error(f"{step_name} missing required field: {field}")
            return False

    # Validate step type
    step_type = step_config['type']
    valid_types = ['build', 'test', 'analyze', 'fix', 'report', 'custom']
    if step_type not in valid_types:
        logger.error(f"{step_name} has invalid type: {step_type}")
        return False

    # Validate optional fields
    if 'name' in step_config and not isinstance(step_config['name'], str):
        logger.error(f"{step_name} name must be a string")
        return False

    if 'description' in step_config and not isinstance(step_config['description'], str):
        logger.error(f"{step_name} description must be a string")
        return False

    if 'timeout' in step_config:
        if not isinstance(step_config['timeout'], (int, float)):
            logger.error(f"{step_name} timeout must be a number")
            return False
        if step_config['timeout'] <= 0:
            logger.error(f"{step_name} timeout must be positive")
            return False

    if 'dependencies' in step_config:
        if not isinstance(step_config['dependencies'], list):
            logger.error(f"{step_name} dependencies must be a list")
            return False

    if 'condition' in step_config:
        if not isinstance(step_config['condition'], str):
            logger.error(f"{step_name} condition must be a string")
            return False

    if 'retry_count' in step_config:
        if not isinstance(step_config['retry_count'], int):
            logger.error(f"{step_name} retry_count must be an integer")
            return False
        if step_config['retry_count'] < 0:
            logger.error(f"{step_name} retry_count must be non-negative")
            return False

    if 'enabled' in step_config:
        if not isinstance(step_config['enabled'], bool):
            logger.error(f"{step_name} enabled must be a boolean")
            return False

    return True


def validate_file_path_list(file_paths: List[Union[str, Path]], base_path: Optional[Path] = None) -> List[Path]:
    """
    Validate a list of file paths

    Args:
        file_paths: List of file paths
        base_path: Base path for relative paths

    Returns:
        List of validated Path objects
    """
    logger = get_logger(__name__)
    validated_paths = []

    for file_path in file_paths:
        if isinstance(file_path, str):
            file_path = Path(file_path)

        if base_path and not file_path.is_absolute():
            file_path = base_path / file_path

        if validate_file_path(file_path):
            validated_paths.append(file_path)
        else:
            logger.warning(f"Skipping invalid file path: {file_path}")

    return validated_paths


def validate_string_field(
    config: Dict[str, Any],
    field_name: str,
    default: Optional[str] = None
) -> str:
    """Validate and extract string field from config"""
    value = config.get(field_name, default)
    if value is None:
        return "" if default is None else default
    if not isinstance(value, str):
        return str(value)
    return value


def validate_bool_field(
    config: Dict[str, Any],
    field_name: str,
    default: bool = False
) -> bool:
    """Validate and extract boolean field from config"""
    value = config.get(field_name, default)
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.lower() in ('true', '1', 'yes', 'on')
    return bool(value)


def validate_int_field(
    config: Dict[str, Any],
    field_name: str,
    min_value: Optional[int] = None,
    max_value: Optional[int] = None,
    default: int = 0
) -> int:
    """Validate and extract integer field from config"""
    value = config.get(field_name, default)
    try:
        int_value = int(value)
        if min_value is not None and int_value < min_value:
            return min_value
        if max_value is not None and int_value > max_value:
            return max_value
        return int_value
    except (ValueError, TypeError):
        return default


def validate_float_field(
    config: Dict[str, Any],
    field_name: str,
    min_value: Optional[float] = None,
    max_value: Optional[float] = None,
    default: float = 0.0
) -> float:
    """Validate and extract float field from config"""
    value = config.get(field_name, default)
    try:
        float_value = float(value)
        if min_value is not None and float_value < min_value:
            return min_value
        if max_value is not None and float_value > max_value:
            return max_value
        return float_value
    except (ValueError, TypeError):
        return default


def validate_list_field(
    config: Dict[str, Any],
    field_name: str,
    item_type: type,
    required: bool = False,
    allowed_values: Optional[List[str]] = None,
    default: Optional[List[Any]] = None
) -> List[Any]:
    """Validate and extract list field from config"""
    value = config.get(field_name, default)

    if required and value is None:
        return []

    if not isinstance(value, list):
        return []

    validated_list = []
    for item in value:
        if isinstance(item, item_type):
            if allowed_values and item not in allowed_values:
                continue
            validated_list.append(item)

    return validated_list


def validate_log_level(level: str) -> str:
    """Validate log level string"""
    valid_levels = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
    if level.upper() in valid_levels:
        return level.upper()
    return 'INFO'


def validate_project_structure(project_root: Path) -> Dict[str, Any]:
    """
    Validate project structure

    Args:
        project_root: Project root directory

    Returns:
        Validation results
    """
    logger = get_logger(__name__)
    validation_results = {
        'valid': True,
        'issues': [],
        'recommendations': []
    }

    # Check if directory exists
    if not project_root.exists():
        validation_results['valid'] = False
        validation_results['issues'].append(f"Project root directory does not exist: {project_root}")
        return validation_results

    # Check for common project files
    project_indicators = [
        ('README.md', 'Project documentation'),
        ('.git', 'Git repository'),
        ('requirements.txt', 'Python dependencies'),
        ('package.json', 'Node.js dependencies'),
        ('pom.xml', 'Maven project'),
        ('CMakeLists.txt', 'CMake project'),
        ('Makefile', 'Make build system')
    ]

    found_indicators = []
    for indicator, description in project_indicators:
        if (project_root / indicator).exists():
            found_indicators.append(description)

    if not found_indicators:
        validation_results['recommendations'].append(
            "Consider adding project documentation (README.md)"
        )

    # Check for source code
    source_extensions = ['.py', '.java', '.cpp', '.c', '.js', '.ts', '.go', '.rs']
    source_files = []
    for ext in source_extensions:
        source_files.extend(list(project_root.rglob(f"*{ext}")))

    if not source_files:
        validation_results['recommendations'].append(
            "No source code files found in project"
        )

    validation_results['source_files'] = len(source_files)
    validation_results['found_indicators'] = found_indicators

    return validation_results


def validate_system_requirements() -> Dict[str, Any]:
    """Validate system requirements"""
    import sys
    import subprocess

    requirements = {
        'python_version': {
            'required': (3, 7),
            'found': sys.version_info[:2],
            'valid': sys.version_info[:2] >= (3, 7)
        },
        'tools': {}
    }

    # Check for common tools
    tools_to_check = {
        'git': ['git', '--version'],
        'python': ['python', '--version'],
        'node': ['node', '--version'],
        'npm': ['npm', '--version'],
        'maven': ['mvn', '--version'],
        'cmake': ['cmake', '--version'],
        'pytest': ['pytest', '--version'],
        'eslint': ['eslint', '--version']
    }

    for tool_name, command in tools_to_check.items():
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                timeout=5
            )
            requirements['tools'][tool_name] = {
                'available': result.returncode == 0,
                'version': result.stdout.strip() if result.stdout else 'Unknown'
            }
        except (subprocess.TimeoutExpired, FileNotFoundError):
            requirements['tools'][tool_name] = {
                'available': False,
                'version': 'Not found'
            }

    return requirements


def check_dependencies(dependencies: List[str]) -> Dict[str, Any]:
    """
    Check if required dependencies are available

    Args:
        dependencies: List of dependency names

    Returns:
        Dependency availability results
    """
    import import importlib

    results = {}
    for dep in dependencies:
        try:
            module = importlib.import_module(dep)
            results[dep] = {
                'available': True,
                'version': getattr(module, '__version__', 'Unknown')
            }
        except ImportError:
            results[dep] = {
                'available': False,
                'version': 'Not installed'
            }

    return results