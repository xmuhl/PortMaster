"""
AI Development System - Configuration Module

This module contains configuration management functionality
for the AI development system.
"""

__version__ = "1.0.0"

from .config_manager import ConfigManager, SystemConfig, BuildConfig, TestConfig, StaticAnalysisConfig, ReportingConfig

__all__ = [
    'ConfigManager',
    'SystemConfig',
    'BuildConfig',
    'TestConfig',
    'StaticAnalysisConfig',
    'ReportingConfig'
]