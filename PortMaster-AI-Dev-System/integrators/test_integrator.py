#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Test Integrator

This module provides integration with various test frameworks
for automated test execution and reporting.
"""

import os
import subprocess
import json
import xml.etree.ElementTree as ET
import logging
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass

from ..utils.file_utils import find_files, read_file, write_file, ensure_directory
from ..utils.log_utils import get_logger


@dataclass
class TestResult:
    """Test execution result"""
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
    coverage_info: Optional[Dict[str, Any]] = None


@dataclass
class TestCase:
    """Individual test case result"""
    name: str
    class_name: Optional[str]
    file_path: Optional[str]
    line_number: Optional[int]
    status: str  # passed, failed, skipped, error
    duration: float
    error_message: Optional[str]
    stack_trace: Optional[str]


class TestIntegrator:
    """
    Test framework integrator supporting multiple test tools
    """

    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None):
        """
        Initialize test integrator

        Args:
            project_root: Project root directory
            config: Test configuration
        """
        self.project_root = project_root
        self.config = config or {}
        self.logger = get_logger(__name__)

        # Supported test frameworks
        self.test_frameworks = {
            'pytest': {
                'files': ['test_*.py', '*_test.py', 'tests/test_*.py'],
                'commands': ['pytest', 'pytest --junit-xml=test-results.xml', 'pytest --cov=. --cov-report=json'],
                'result_files': ['test-results.xml', 'coverage.json']
            },
            'unittest': {
                'files': ['test_*.py', '*_test.py'],
                'commands': ['python -m unittest discover', 'python -m unittest discover -v'],
                'result_files': []
            },
            'jest': {
                'files': ['*.test.js', '*.test.ts', '**/__tests__/**/*.js', '**/__tests__/**/*.ts'],
                'commands': ['npm test', 'npm run test', 'jest --coverage'],
                'result_files': ['coverage/coverage-final.json', 'junit.xml']
            },
            'mocha': {
                'files': ['*.test.js', '*_test.js', 'test/**/*.js'],
                'commands': ['npm test', 'mocha', 'mocha --reporter json'],
                'result_files': ['test-results.json']
            },
            'junit': {
                'files': ['**/*Test.java', '**/*Tests.java'],
                'commands': ['mvn test', 'gradle test'],
                'result_files': ['target/surefire-reports/*.xml', 'build/test-results/test/*.xml']
            },
            'googletest': {
                'files': ['**/*_test.cpp', '**/*_test.cc', '**/test/**/*.cpp'],
                'commands': ['make test', 'cmake --build build --target test', 'ctest'],
                'result_files': ['build/test/**/*.xml']
            },
            'catch2': {
                'files': ['**/*_test.cpp', '**/*_test.cc', '**/test/**/*.cpp'],
                'commands': ['make test', 'cmake --build build --target test'],
                'result_files': []
            }
        }

    def detect_test_framework(self) -> Optional[str]:
        """
        Detect the test framework used in the project

        Returns:
            Detected test framework name or None
        """
        for framework_name, framework_info in self.test_frameworks.items():
            for test_pattern in framework_info['files']:
                if list(self.project_root.glob(test_pattern)):
                    self.logger.info(f"Detected {framework_name} test framework (found {test_pattern})")
                    return framework_name

        # Try to detect based on configuration files
        config_files = {
            'pytest': ['pytest.ini', 'pyproject.toml', 'setup.cfg', 'tox.ini'],
            'jest': ['jest.config.js', 'jest.config.json', 'package.json'],
            'mocha': ['mocha.opts', 'package.json'],
            'junit': ['pom.xml', 'build.gradle'],
            'googletest': ['CMakeLists.txt'],
            'catch2': ['CMakeLists.txt']
        }

        for framework_name, files in config_files.items():
            for config_file in files:
                if (self.project_root / config_file).exists():
                    # Verify by checking content
                    content = read_file(self.project_root / config_file)
                    if content and framework_name.lower() in content.lower():
                        self.logger.info(f"Detected {framework_name} test framework (found {config_file})")
                        return framework_name

        self.logger.warning("Could not detect test framework")
        return None

    def get_test_commands(self, framework: str, include_coverage: bool = False) -> List[str]:
        """
        Get test commands for specific framework

        Args:
            framework: Test framework name
            include_coverage: Whether to include coverage collection

        Returns:
            List of test commands
        """
        if framework not in self.test_frameworks:
            raise ValueError(f"Unsupported test framework: {framework}")

        framework_info = self.test_frameworks[framework]
        commands = []

        if framework == 'pytest':
            commands = ['pytest --junit-xml=test-results.xml --tb=short']
            if include_coverage:
                commands[-1] += ' --cov=. --cov-report=json --cov-report=xml'

        elif framework == 'unittest':
            commands = ['python -m unittest discover -v']

        elif framework == 'jest':
            commands = ['npm test -- --coverage --outputFile=coverage.json']

        elif framework == 'mocha':
            commands = ['npm test']

        elif framework == 'junit':
            # Check for Maven or Gradle
            if (self.project_root / 'pom.xml').exists():
                commands = ['mvn test']
            elif (self.project_root / 'build.gradle').exists():
                commands = ['gradle test']

        elif framework == 'googletest':
            commands = ['ctest --output-on-failure']

        elif framework == 'catch2':
            commands = ['ctest --output-on-failure']

        return commands

    def execute_tests(self, framework: Optional[str] = None, include_coverage: bool = False,
                     timeout: int = 300) -> TestResult:
        """
        Execute test suite

        Args:
            framework: Test framework to use (auto-detect if None)
            include_coverage: Whether to collect coverage information
            timeout: Test timeout in seconds

        Returns:
            Test execution result
        """
        start_time = time.time()

        if framework is None:
            framework = self.detect_test_framework()
            if framework is None:
                return TestResult(
                    framework='unknown',
                    total_tests=0,
                    passed_tests=0,
                    failed_tests=0,
                    skipped_tests=0,
                    errors=1,
                    duration=0,
                    output="",
                    error_output="Could not detect test framework",
                    test_cases=[]
                )

        try:
            commands = self.get_test_commands(framework, include_coverage)
            self.logger.info(f"Executing tests with {framework} framework")

            combined_output = []
            combined_error_output = []
            test_cases = []
            coverage_info = None

            for command in commands:
                self.logger.info(f"Running test command: {command}")

                try:
                    result = subprocess.run(
                        command,
                        cwd=self.project_root,
                        shell=True,
                        capture_output=True,
                        text=True,
                        timeout=timeout
                    )

                    combined_output.append(f"=== Command: {command} ===")
                    combined_output.append(result.stdout)
                    combined_error_output.append(f"=== Command: {command} ===")
                    combined_error_output.append(result.stderr)

                    if result.returncode != 0:
                        self.logger.error(f"Test command failed with exit code {result.returncode}")
                        break

                except subprocess.TimeoutExpired:
                    error_msg = f"Test command timed out after {timeout} seconds: {command}"
                    self.logger.error(error_msg)
                    combined_error_output.append(error_msg)
                    break

            duration = time.time() - start_time

            # Parse test results
            total_tests, passed_tests, failed_tests, skipped_tests, errors = self._parse_test_results(
                '\n'.join(combined_output), framework
            )

            # Parse test cases from result files
            test_cases = self._parse_test_case_files(framework)

            # Parse coverage if requested
            if include_coverage:
                coverage_info = self._parse_coverage_results(framework)

            return TestResult(
                framework=framework,
                total_tests=total_tests,
                passed_tests=passed_tests,
                failed_tests=failed_tests,
                skipped_tests=skipped_tests,
                errors=errors,
                duration=duration,
                output='\n'.join(combined_output),
                error_output='\n'.join(combined_error_output),
                test_cases=test_cases,
                coverage_info=coverage_info
            )

        except Exception as e:
            self.logger.error(f"Test execution failed: {e}")
            duration = time.time() - start_time
            return TestResult(
                framework=framework or 'unknown',
                total_tests=0,
                passed_tests=0,
                failed_tests=0,
                skipped_tests=0,
                errors=1,
                duration=duration,
                output="",
                error_output=str(e),
                test_cases=[]
            )

    def _parse_test_results(self, output: str, framework: str) -> Tuple[int, int, int, int, int]:
        """
        Parse test output to count test results

        Args:
            output: Test output text
            framework: Test framework name

        Returns:
            Tuple of (total, passed, failed, skipped, errors)
        """
        total = passed = failed = skipped = errors = 0

        if framework == 'pytest':
            # Parse pytest summary
            import re
            summary_match = re.search(r'(\d+)\s+passed\s+in\s+[\d.]+s', output)
            if summary_match:
                passed = int(summary_match.group(1))
                total = passed

            failed_match = re.search(r'(\d+)\s+failed', output)
            if failed_match:
                failed = int(failed_match.group(1))
                total += failed

            skipped_match = re.search(r'(\d+)\s+skipped', output)
            if skipped_match:
                skipped = int(skipped_match.group(1))
                total += skipped

            error_match = re.search(r'(\d+)\s+error', output)
            if error_match:
                errors = int(error_match.group(1))
                total += errors

        elif framework == 'unittest':
            # Parse unittest output
            import re
            summary_match = re.search(r'Ran\s+(\d+)\s+tests', output)
            if summary_match:
                total = int(summary_match.group(1))

            if 'OK' in output:
                passed = total
            else:
                # Count failures
                failed_match = re.search(r'FAILURES:\s*(\d+)', output)
                if failed_match:
                    failed = int(failed_match.group(1))

                error_match = re.search(r'ERRORS:\s*(\d+)', output)
                if error_match:
                    errors = int(error_match.group(1))

                passed = total - failed - errors

        elif framework in ['jest', 'mocha']:
            # Parse JavaScript test output
            import re
            summary_match = re.search(r'(\d+)\s+passing,\s*(\d+)\s+failing', output)
            if summary_match:
                passed = int(summary_match.group(1))
                failed = int(summary_match.group(2))
                total = passed + failed

        elif framework == 'junit':
            # Parse JUnit/Maven output
            import re
            summary_match = re.search(r'Tests run:\s*(\d+),\s*Failures:\s*(\d+),\s*Errors:\s*(\d+),\s*Skipped:\s*(\d+)', output)
            if summary_match:
                total = int(summary_match.group(1))
                failed = int(summary_match.group(2))
                errors = int(summary_match.group(3))
                skipped = int(summary_match.group(4))
                passed = total - failed - errors - skipped

        elif framework in ['googletest', 'catch2']:
            # Parse C++ test output
            import re
            summary_match = re.search(r'\[\s*OK\s*\]\s*(\d+)\s+tests?', output)
            if summary_match:
                passed = int(summary_match.group(1))
                total = passed

            failed_match = re.search(r'\[\s*FAILED\s*\]\s*(\d+)\s+tests?', output)
            if failed_match:
                failed = int(failed_match.group(1))
                total += failed

        return total, passed, failed, skipped, errors

    def _parse_test_case_files(self, framework: str) -> List[Dict[str, Any]]:
        """
        Parse test case results from result files

        Args:
            framework: Test framework name

        Returns:
            List of test case dictionaries
        """
        test_cases = []

        try:
            if framework == 'pytest':
                # Parse JUnit XML
                junit_file = self.project_root / 'test-results.xml'
                if junit_file.exists():
                    test_cases = self._parse_junit_xml(junit_file)

            elif framework == 'junit':
                # Parse JUnit XML files
                junit_dirs = [
                    self.project_root / 'target/surefire-reports',
                    self.project_root / 'build/test-results/test'
                ]
                for junit_dir in junit_dirs:
                    if junit_dir.exists():
                        for xml_file in junit_dir.glob('*.xml'):
                            test_cases.extend(self._parse_junit_xml(xml_file))

            elif framework == 'jest':
                # Parse Jest JSON output
                json_file = self.project_root / 'test-results.json'
                if json_file.exists():
                    test_cases = self._parse_jest_json(json_file)

            elif framework == 'googletest':
                # Parse GoogleTest XML
                xml_files = list(self.project_root.glob('build/test/**/*.xml'))
                for xml_file in xml_files:
                    test_cases.extend(self._parse_junit_xml(xml_file))

        except Exception as e:
            self.logger.warning(f"Failed to parse test case files: {e}")

        return test_cases

    def _parse_junit_xml(self, xml_file: Path) -> List[Dict[str, Any]]:
        """Parse JUnit XML file"""
        test_cases = []

        try:
            tree = ET.parse(xml_file)
            root = tree.getroot()

            for testcase in root.findall('.//testcase'):
                name = testcase.get('name', '')
                class_name = testcase.get('classname', '')
                time_str = testcase.get('time', '0')
                duration = float(time_str)

                # Determine status
                failure = testcase.find('failure')
                error = testcase.find('error')
                skipped = testcase.find('skipped')

                if failure is not None:
                    status = 'failed'
                    error_message = failure.get('message', '')
                    stack_trace = failure.text or ''
                elif error is not None:
                    status = 'error'
                    error_message = error.get('message', '')
                    stack_trace = error.text or ''
                elif skipped is not None:
                    status = 'skipped'
                    error_message = skipped.get('message', '')
                    stack_trace = ''
                else:
                    status = 'passed'
                    error_message = ''
                    stack_trace = ''

                test_cases.append({
                    'name': name,
                    'class_name': class_name,
                    'status': status,
                    'duration': duration,
                    'error_message': error_message,
                    'stack_trace': stack_trace,
                    'file_path': None,
                    'line_number': None
                })

        except Exception as e:
            self.logger.warning(f"Failed to parse JUnit XML {xml_file}: {e}")

        return test_cases

    def _parse_jest_json(self, json_file: Path) -> List[Dict[str, Any]]:
        """Parse Jest JSON results"""
        test_cases = []

        try:
            with open(json_file, 'r') as f:
                data = json.load(f)

            for test_file in data.get('testResults', []):
                file_path = test_file.get('name', '')
                for assertion in test_file.get('assertionResults', []):
                    name = assertion.get('title', '')
                    status = assertion.get('status', 'unknown')
                    duration = assertion.get('duration', 0) / 1000.0  # Convert ms to seconds

                    error_message = ''
                    stack_trace = ''
                    if status in ['failed', 'error']:
                        failure = assertion.get('failureMessages', [])
                        error_message = failure[0] if failure else ''

                    test_cases.append({
                        'name': name,
                        'class_name': None,
                        'file_path': file_path,
                        'status': status,
                        'duration': duration,
                        'error_message': error_message,
                        'stack_trace': stack_trace,
                        'line_number': None
                    })

        except Exception as e:
            self.logger.warning(f"Failed to parse Jest JSON {json_file}: {e}")

        return test_cases

    def _parse_coverage_results(self, framework: str) -> Optional[Dict[str, Any]]:
        """
        Parse coverage results

        Args:
            framework: Test framework name

        Returns:
            Coverage information dictionary or None
        """
        try:
            if framework == 'pytest':
                # Parse coverage JSON
                coverage_file = self.project_root / 'coverage.json'
                if coverage_file.exists():
                    with open(coverage_file, 'r') as f:
                        coverage_data = json.load(f)

                    totals = coverage_data.get('totals', {})
                    return {
                        'line_coverage': totals.get('covered_lines', 0) / max(totals.get('num_statements', 1), 1) * 100,
                        'branch_coverage': totals.get('covered_branches', 0) / max(totals.get('num_branches', 1), 1) * 100,
                        'function_coverage': totals.get('covered_functions', 0) / max(totals.get('num_functions', 1), 1) * 100,
                        'total_lines': totals.get('num_statements', 0),
                        'covered_lines': totals.get('covered_lines', 0),
                        'files': coverage_data.get('files', {})
                    }

            elif framework == 'jest':
                # Parse Jest coverage
                coverage_file = self.project_root / 'coverage' / 'coverage-final.json'
                if coverage_file.exists():
                    with open(coverage_file, 'r') as f:
                        coverage_data = json.load(f)

                    total_lines = 0
                    covered_lines = 0
                    for file_path, file_data in coverage_data.items():
                        lines = file_data.get('l', {})
                        total_lines += len(lines)
                        covered_lines += sum(1 for hits in lines.values() if hits > 0)

                    return {
                        'line_coverage': covered_lines / max(total_lines, 1) * 100,
                        'total_lines': total_lines,
                        'covered_lines': covered_lines,
                        'files': coverage_data
                    }

        except Exception as e:
            self.logger.warning(f"Failed to parse coverage results: {e}")

        return None

    def get_test_configuration(self) -> Dict[str, Any]:
        """
        Get current test configuration

        Returns:
            Test configuration dictionary
        """
        detected_framework = self.detect_test_framework()

        config = {
            'detected_framework': detected_framework,
            'supported_frameworks': list(self.test_frameworks.keys()),
            'project_root': str(self.project_root),
            'test_files': [],
            'configuration_files': []
        }

        if detected_framework:
            framework_info = self.test_frameworks[detected_framework]

            # Find test files
            for pattern in framework_info['files']:
                config['test_files'].extend([str(p.relative_to(self.project_root)) for p in self.project_root.glob(pattern)])

            # Find configuration files
            config_files_map = {
                'pytest': ['pytest.ini', 'pyproject.toml', 'setup.cfg', 'tox.ini'],
                'jest': ['jest.config.js', 'jest.config.json', 'package.json'],
                'mocha': ['mocha.opts', 'package.json'],
                'junit': ['pom.xml', 'build.gradle'],
                'googletest': ['CMakeLists.txt'],
                'catch2': ['CMakeLists.txt']
            }

            if detected_framework in config_files_map:
                for config_file in config_files_map[detected_framework]:
                    config_path = self.project_root / config_file
                    if config_path.exists():
                        config['configuration_files'].append(str(config_path.relative_to(self.project_root)))

        return config

    def verify_test_environment(self, framework: str) -> Dict[str, Any]:
        """
        Verify test environment for specific framework

        Args:
            framework: Test framework to verify

        Returns:
            Environment verification results
        """
        verification = {
            'framework': framework,
            'tools_available': {},
            'dependencies_met': True,
            'issues': []
        }

        try:
            if framework == 'pytest':
                result = subprocess.run(['pytest', '--version'], capture_output=True, text=True)
                verification['tools_available']['pytest'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("pytest is not available")
                    verification['dependencies_met'] = False

            elif framework == 'unittest':
                # unittest is built into Python
                verification['tools_available']['unittest'] = True

            elif framework in ['jest', 'mocha']:
                result = subprocess.run(['npm', '--version'], capture_output=True, text=True)
                verification['tools_available']['npm'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("npm is not available")
                    verification['dependencies_met'] = False

            elif framework == 'junit':
                # Check for Maven or Gradle
                maven_result = subprocess.run(['mvn', '--version'], capture_output=True, text=True)
                verification['tools_available']['maven'] = maven_result.returncode == 0

                gradle_result = subprocess.run(['gradle', '--version'], capture_output=True, text=True)
                verification['tools_available']['gradle'] = gradle_result.returncode == 0

                if not verification['tools_available']['maven'] and not verification['tools_available']['gradle']:
                    verification['issues'].append("Neither Maven nor Gradle is available")
                    verification['dependencies_met'] = False

            elif framework in ['googletest', 'catch2']:
                # Check for CMake and build tools
                result = subprocess.run(['cmake', '--version'], capture_output=True, text=True)
                verification['tools_available']['cmake'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("cmake is not available")
                    verification['dependencies_met'] = False

                result = subprocess.run(['ctest', '--version'], capture_output=True, text=True)
                verification['tools_available']['ctest'] = result.returncode == 0

        except Exception as e:
            verification['issues'].append(f"Environment verification failed: {e}")
            verification['dependencies_met'] = False

        return verification