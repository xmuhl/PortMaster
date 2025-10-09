#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Master Controller

This module provides the main controller for the AI-driven development system.
It orchestrates all components and manages the complete automated development workflow.
"""

import os
import sys
import json
import logging
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

# Import local modules
from ..utils.log_utils import setup_logging
from ..utils.file_utils import ensure_directory, save_json, load_json
from ..integrators.autotest_integration import AutoTestIntegration
from ..integrators.build_integration import BuildIntegration
from ..analyzers.static_analyzer import StaticAnalyzer
from ..analyzers.coverage_analyzer import CoverageAnalyzer
from ..analyzers.error_pattern_analyzer import ErrorPatternAnalyzer
from ..reporters.report_generator import ReportGenerator


class MasterController:
    """Main controller for AI-driven development system"""

    def __init__(self, config_path: Optional[str] = None, project_root: Optional[str] = None):
        """
        Initialize the master controller

        Args:
            config_path: Path to configuration file
            project_root: Root directory of the project to analyze
        """
        self.project_root = Path(project_root) if project_root else Path.cwd()
        self.config = self._load_config(config_path)
        self.results = {}
        self.session_start = datetime.now()

        # Setup logging
        setup_logging(self.config.get('logging', {}))
        self.logger = logging.getLogger(__name__)

        # Initialize components
        self._initialize_components()

    def _load_config(self, config_path: Optional[str]) -> Dict:
        """Load configuration from file or use defaults"""
        default_config = {
            "build": {
                "commands": ["make", "cmake --build .", "mvn compile", "gradle build"],
                "timeout": 300,
                "clean_command": None
            },
            "test": {
                "frameworks": ["pytest", "unittest", "jest", "maven test", "gradle test"],
                "timeout": 600,
                "coverage_threshold": 80.0
            },
            "analysis": {
                "static_analysis_tools": ["cppcheck", "pylint", "eslint"],
                "complexity_threshold": 10,
                "duplicate_threshold": 5
            },
            "reporting": {
                "output_dir": "reports",
                "formats": ["json", "html", "markdown"],
                "include_metrics": True
            },
            "logging": {
                "level": "INFO",
                "file": "ai_dev_system.log",
                "format": "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
            }
        }

        if config_path and os.path.exists(config_path):
            try:
                with open(config_path, 'r', encoding='utf-8') as f:
                    user_config = json.load(f)
                # Merge with defaults
                return self._merge_configs(default_config, user_config)
            except Exception as e:
                logging.warning(f"Failed to load config from {config_path}: {e}")

        return default_config

    def _merge_configs(self, default: Dict, user: Dict) -> Dict:
        """Recursively merge user config with defaults"""
        result = default.copy()
        for key, value in user.items():
            if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                result[key] = self._merge_configs(result[key], value)
            else:
                result[key] = value
        return result

    def _initialize_components(self):
        """Initialize all system components"""
        self.logger.info("Initializing AI Development System components...")

        # Initialize integrators
        self.build_integration = BuildIntegration(self.config['build'])
        self.test_integration = AutoTestIntegration(self.config['test'])

        # Initialize analyzers
        self.static_analyzer = StaticAnalyzer(self.config['analysis'])
        self.coverage_analyzer = CoverageAnalyzer(self.config['analysis'])
        self.error_analyzer = ErrorPatternAnalyzer(self.config['analysis'])

        # Initialize reporter
        self.reporter = ReportGenerator(self.config['reporting'])

        self.logger.info("Components initialized successfully")

    def run_full_workflow(self) -> Dict[str, Any]:
        """
        Run the complete automated development workflow

        Returns:
            Dictionary containing all results and metrics
        """
        self.logger.info("Starting full automated development workflow")

        workflow_steps = [
            ("build_verification", self._verify_build),
            ("static_analysis", self._run_static_analysis),
            ("test_execution", self._run_tests),
            ("coverage_analysis", self._analyze_coverage),
            ("error_detection", self._detect_errors),
            ("report_generation", self._generate_reports)
        ]

        results = {
            "session_info": {
                "start_time": self.session_start.isoformat(),
                "project_root": str(self.project_root),
                "config": self.config
            },
            "steps": {}
        }

        for step_name, step_func in workflow_steps:
            self.logger.info(f"Executing step: {step_name}")
            try:
                step_result = step_func()
                results["steps"][step_name] = step_result

                # Check if step failed critically
                if not step_result.get("success", True):
                    self.logger.warning(f"Step {step_name} completed with warnings")

            except Exception as e:
                self.logger.error(f"Step {step_name} failed: {str(e)}")
                results["steps"][step_name] = {
                    "success": False,
                    "error": str(e),
                    "timestamp": datetime.now().isoformat()
                }

        # Add session summary
        results["session_info"]["end_time"] = datetime.now().isoformat()
        results["session_info"]["duration"] = (
            datetime.now() - self.session_start
        ).total_seconds()

        self.results = results
        return results

    def _verify_build(self) -> Dict[str, Any]:
        """Verify project build"""
        self.logger.info("Verifying project build...")

        try:
            build_result = self.build_integration.build(self.project_root)

            return {
                "success": build_result.success,
                "duration": build_result.duration,
                "output": build_result.output,
                "errors": build_result.errors,
                "warnings": build_result.warnings,
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def _run_static_analysis(self) -> Dict[str, Any]:
        """Run static code analysis"""
        self.logger.info("Running static analysis...")

        try:
            analysis_result = self.static_analyzer.analyze(self.project_root)

            return {
                "success": True,
                "issues_found": len(analysis_result.issues),
                "complexity_violations": analysis_result.complexity_violations,
                "metrics": analysis_result.metrics,
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def _run_tests(self) -> Dict[str, Any]:
        """Execute test suite"""
        self.logger.info("Running test suite...")

        try:
            test_result = self.test_integration.run_tests(self.project_root)

            return {
                "success": test_result.success,
                "total_tests": test_result.total_tests,
                "passed_tests": test_result.passed_tests,
                "failed_tests": test_result.failed_tests,
                "coverage": test_result.coverage,
                "duration": test_result.duration,
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def _analyze_coverage(self) -> Dict[str, Any]:
        """Analyze code coverage"""
        self.logger.info("Analyzing code coverage...")

        try:
            coverage_result = self.coverage_analyzer.analyze(self.project_root)

            return {
                "success": True,
                "overall_coverage": coverage_result.overall_coverage,
                "line_coverage": coverage_result.line_coverage,
                "branch_coverage": coverage_result.branch_coverage,
                "uncovered_files": coverage_result.uncovered_files,
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def _detect_errors(self) -> Dict[str, Any]:
        """Detect and analyze error patterns"""
        self.logger.info("Detecting error patterns...")

        try:
            error_result = self.error_analyzer.analyze(self.project_root)

            return {
                "success": True,
                "patterns_found": len(error_result.patterns),
                "critical_issues": error_result.critical_issues,
                "recommendations": error_result.recommendations,
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def _generate_reports(self) -> Dict[str, Any]:
        """Generate comprehensive reports"""
        self.logger.info("Generating reports...")

        try:
            # Ensure output directory exists
            output_dir = Path(self.config['reporting']['output_dir'])
            ensure_directory(output_dir)

            # Generate reports in different formats
            reports = {}
            for format_type in self.config['reporting']['formats']:
                report_path = output_dir / f"ai_dev_report.{format_type}"
                report_content = self.reporter.generate_report(
                    self.results,
                    format_type
                )

                with open(report_path, 'w', encoding='utf-8') as f:
                    f.write(report_content)

                reports[format_type] = str(report_path)

            # Save raw results as JSON
            json_path = output_dir / f"ai_dev_results_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            save_json(self.results, json_path)

            return {
                "success": True,
                "reports_generated": list(reports.keys()),
                "report_paths": reports,
                "raw_results": str(json_path),
                "timestamp": datetime.now().isoformat()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def get_summary(self) -> Dict[str, Any]:
        """Get a summary of the last run"""
        if not self.results:
            return {"status": "No results available"}

        summary = {
            "session_duration": self.results.get("session_info", {}).get("duration", 0),
            "steps_completed": len(self.results.get("steps", {})),
            "overall_success": all(
                step.get("success", False)
                for step in self.results.get("steps", {}).values()
            )
        }

        # Add key metrics from different steps
        steps = self.results.get("steps", {})

        if "build_verification" in steps:
            summary["build_status"] = steps["build_verification"].get("success", False)

        if "test_execution" in steps:
            test_step = steps["test_execution"]
            summary["test_results"] = {
                "total": test_step.get("total_tests", 0),
                "passed": test_step.get("passed_tests", 0),
                "coverage": test_step.get("coverage", 0)
            }

        if "static_analysis" in steps:
            summary["code_quality"] = {
                "issues_found": steps["static_analysis"].get("issues_found", 0),
                "complexity_violations": steps["static_analysis"].get("complexity_violations", 0)
            }

        return summary