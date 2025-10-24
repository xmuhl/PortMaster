#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Static Analyzer

This module provides static code analysis capabilities for detecting
code quality issues, security vulnerabilities, and maintainability problems.
"""

import os
import re
import subprocess
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any, Set
from dataclasses import dataclass

from ..utils.file_utils import find_files, read_file
from ..utils.log_utils import setup_logging


@dataclass
class CodeIssue:
    """Represents a code issue found during analysis"""
    file_path: str
    line_number: int
    column_number: int
    severity: str  # 'error', 'warning', 'info'
    message: str
    rule_id: str
    category: str  # 'style', 'security', 'performance', 'maintainability'


@dataclass
class ComplexityMetrics:
    """Code complexity metrics"""
    cyclomatic_complexity: int
    cognitive_complexity: int
    lines_of_code: int
    maintainability_index: float


@dataclass
class AnalysisResult:
    """Result of static analysis"""
    issues: List[CodeIssue]
    metrics: Dict[str, Any]
    summary: Dict[str, int]
    success: bool
    error_message: Optional[str]


class StaticAnalyzer:
    """Static code analyzer"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize static analyzer

        Args:
            config: Configuration dictionary
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        self.supported_extensions = {'.py', '.java', '.cpp', '.c', '.h', '.hpp', '.js', '.ts'}
        self.issues = []

    def analyze(self, project_root: Path) -> AnalysisResult:
        """
        Perform static analysis on the project

        Args:
            project_root: Root directory of the project

        Returns:
            Analysis results
        """
        self.logger.info(f"Starting static analysis for project: {project_root}")

        try:
            # Find all source files
            source_files = self._find_source_files(project_root)
            self.logger.info(f"Found {len(source_files)} source files")

            # Analyze each file
            all_issues = []
            metrics = {}

            for file_path in source_files:
                file_issues, file_metrics = self._analyze_file(file_path)
                all_issues.extend(file_issues)
                metrics[str(file_path.relative_to(project_root))] = file_metrics

            # Calculate summary
            summary = self._calculate_summary(all_issues)

            result = AnalysisResult(
                issues=all_issues,
                metrics=metrics,
                summary=summary,
                success=True,
                error_message=None
            )

            self.logger.info(f"Static analysis completed: {summary['total_issues']} issues found")
            return result

        except Exception as e:
            self.logger.error(f"Static analysis failed: {str(e)}")
            return AnalysisResult(
                issues=[],
                metrics={},
                summary={},
                success=False,
                error_message=str(e)
            )

    def _find_source_files(self, project_root: Path) -> List[Path]:
        """Find all source files in the project"""
        source_files = []

        for ext in self.supported_extensions:
            pattern = f"**/*{ext}"
            files = list(project_root.glob(pattern))
            source_files.extend(files)

        # Filter out common directories to ignore
        ignore_dirs = {'__pycache__', '.git', 'node_modules', '.vs', 'build', 'dist'}
        source_files = [
            f for f in source_files
            if not any(ignore_dir in f.parts for ignore_dir in ignore_dirs)
        ]

        return source_files

    def _analyze_file(self, file_path: Path) -> Tuple[List[CodeIssue], Dict[str, Any]]:
        """
        Analyze a single file

        Args:
            file_path: Path to the file to analyze

        Returns:
            Tuple of (issues, metrics)
        """
        content = read_file(file_path)
        if not content:
            return [], {}

        issues = []
        lines = content.split('\n')

        # Language-specific analysis
        if file_path.suffix == '.py':
            issues.extend(self._analyze_python_file(file_path, content, lines))
        elif file_path.suffix in {'.cpp', '.c', '.h', '.hpp'}:
            issues.extend(self._analyze_cpp_file(file_path, content, lines))
        elif file_path.suffix in {'.java'}:
            issues.extend(self._analyze_java_file(file_path, content, lines))
        elif file_path.suffix in {'.js', '.ts'}:
            issues.extend(self._analyze_javascript_file(file_path, content, lines))

        # General analysis
        issues.extend(self._analyze_general_issues(file_path, content, lines))

        # Calculate metrics
        metrics = self._calculate_file_metrics(file_path, content, lines)

        return issues, metrics

    def _analyze_python_file(self, file_path: Path, content: str, lines: List[str]) -> List[CodeIssue]:
        """Analyze Python file for issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common Python issues
            if re.search(r'^\s*import \*', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Avoid using star imports',
                    rule_id='PY001',
                    category='style'
                ))

            if re.search(r'^\s*eval\s*\(', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='error',
                    message='Avoid using eval()',
                    rule_id='PY002',
                    category='security'
                ))

            if re.search(r'^\s*exec\s*\(', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='error',
                    message='Avoid using exec()',
                    rule_id='PY003',
                    category='security'
                ))

            # Check for long lines
            if len(line) > 120:
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=120,
                    severity='info',
                    message='Line too long (>120 characters)',
                    rule_id='PY004',
                    category='style'
                ))

        return issues

    def _analyze_cpp_file(self, file_path: Path, content: str, lines: List[str]) -> List[CodeIssue]:
        """Analyze C++ file for issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common C++ issues
            if re.search(r'using\s+namespace\s+std;', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Avoid using namespace std in header files',
                    rule_id='CPP001',
                    category='style'
                ))

            if re.search(r'#define\s+\w+\s*[^\\]', line) and not line.strip().endswith('\\'):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Consider using const or enum instead of #define',
                    rule_id='CPP002',
                    category='maintainability'
                ))

            # Check for memory management issues
            if re.search(r'new\s+\w+', line) and 'delete' not in line:
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Memory allocation without corresponding delete found',
                    rule_id='CPP003',
                    category='memory'
                ))

        return issues

    def _analyze_java_file(self, file_path: Path, content: str, lines: List[str]) -> List[CodeIssue]:
        """Analyze Java file for issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common Java issues
            if re.search(r'public\s+class\s+\w+\s*\{', line) and 'final' not in line:
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='info',
                    message='Consider making class final',
                    rule_id='JAVA001',
                    category='performance'
                ))

            if re.search(r'String\s+\w+\s*=\s*new\s+String\s*\(', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Avoid unnecessary String constructor',
                    rule_id='JAVA002',
                    category='performance'
                ))

        return issues

    def _analyze_javascript_file(self, file_path: Path, content: str, lines: List[str]) -> List[CodeIssue]:
        """Analyze JavaScript/TypeScript file for issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common JavaScript issues
            if re.search(r'var\s+\w+', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Prefer let or const over var',
                    rule_id='JS001',
                    category='style'
                ))

            if re.search(r'==\s*[^=]', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='warning',
                    message='Use === instead of ==',
                    rule_id='JS002',
                    category='style'
                ))

            if re.search(r'eval\s*\(', line):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='error',
                    message='Avoid using eval()',
                    rule_id='JS003',
                    category='security'
                ))

        return issues

    def _analyze_general_issues(self, file_path: Path, content: str, lines: List[str]) -> List[CodeIssue]:
        """Analyze general issues applicable to all languages"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for TODO/FIXME comments
            if re.search(r'\b(TODO|FIXME|HACK)\b', line, re.IGNORECASE):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='info',
                    message='Found TODO/FIXME comment',
                    rule_id='GEN001',
                    category='maintainability'
                ))

            # Check for hardcoded passwords
            if re.search(r'(password|pwd|pass)\s*=\s*[\'"][^\'"]+[\'"]', line, re.IGNORECASE):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=0,
                    severity='error',
                    message='Hardcoded password detected',
                    rule_id='GEN002',
                    category='security'
                ))

            # Check for trailing whitespace
            if line.endswith(' ') or line.endswith('\t'):
                issues.append(CodeIssue(
                    file_path=str(file_path),
                    line_number=line_num,
                    column_number=len(line.rstrip()),
                    severity='info',
                    message='Trailing whitespace',
                    rule_id='GEN003',
                    category='style'
                ))

        return issues

    def _calculate_file_metrics(self, file_path: Path, content: str, lines: List[str]) -> Dict[str, Any]:
        """Calculate metrics for a single file"""
        non_empty_lines = [line for line in lines if line.strip()]
        comment_lines = [line for line in lines if line.strip().startswith('#') or
                        line.strip().startswith('//') or line.strip().startswith('/*')]

        # Simple complexity calculation (count control structures)
        complexity_keywords = {'if', 'else', 'elif', 'for', 'while', 'switch', 'case', 'catch', 'try'}
        complexity = sum(
            len(re.findall(r'\b' + keyword + r'\b', line))
            for line in lines
            for keyword in complexity_keywords
        )

        return {
            'lines_of_code': len(non_empty_lines),
            'total_lines': len(lines),
            'comment_lines': len(comment_lines),
            'blank_lines': len(lines) - len(non_empty_lines),
            'comment_ratio': len(comment_lines) / len(lines) if lines else 0,
            'cyclomatic_complexity': complexity + 1,  # Base complexity is 1
            'file_size_bytes': len(content.encode('utf-8'))
        }

    def _calculate_summary(self, issues: List[CodeIssue]) -> Dict[str, int]:
        """Calculate summary statistics from issues"""
        summary = {
            'total_issues': len(issues),
            'error_issues': len([i for i in issues if i.severity == 'error']),
            'warning_issues': len([i for i in issues if i.severity == 'warning']),
            'info_issues': len([i for i in issues if i.severity == 'info'])
        }

        # Count by category
        for issue in issues:
            category = issue.category
            summary[f'{category}_issues'] = summary.get(f'{category}_issues', 0) + 1

        return summary

    def run_external_tool(self, tool_name: str, project_root: Path) -> List[CodeIssue]:
        """
        Run external static analysis tools

        Args:
            tool_name: Name of the tool to run
            project_root: Project root directory

        Returns:
            List of issues found by the tool
        """
        issues = []

        tool_commands = {
            'pylint': ['pylint', '--output-format=json', '.'],
            'flake8': ['flake8', '--format=json', '.'],
            'eslint': ['eslint', '--format=json', '.'],
            'cppcheck': ['cppcheck', '--xml', '.']
        }

        if tool_name not in tool_commands:
            self.logger.warning(f"Unsupported tool: {tool_name}")
            return issues

        try:
            command = tool_commands[tool_name]
            result = subprocess.run(
                command,
                cwd=project_root,
                capture_output=True,
                text=True,
                timeout=300
            )

            if result.stdout:
                # Parse tool output (simplified)
                issues.extend(self._parse_tool_output(tool_name, result.stdout))

        except subprocess.TimeoutExpired:
            self.logger.warning(f"Tool {tool_name} timed out")
        except Exception as e:
            self.logger.warning(f"Failed to run tool {tool_name}: {str(e)}")

        return issues

    def _parse_tool_output(self, tool_name: str, output: str) -> List[CodeIssue]:
        """Parse output from external tools (simplified implementation)"""
        issues = []

        # This is a simplified parser - real implementation would handle
        # different output formats from each tool
        lines = output.split('\n')
        for line in lines:
            if line.strip():
                # Generic parsing - would be tool-specific in real implementation
                parts = line.split(':')
                if len(parts) >= 3:
                    try:
                        file_path = parts[0]
                        line_num = int(parts[1])
                        message = ':'.join(parts[2:]).strip()

                        issues.append(CodeIssue(
                            file_path=file_path,
                            line_number=line_num,
                            column_number=0,
                            severity='warning',
                            message=message,
                            rule_id=f'{tool_name.upper()}001',
                            category='external'
                        ))
                    except (ValueError, IndexError):
                        continue

        return issues