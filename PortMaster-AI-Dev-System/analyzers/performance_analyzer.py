#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Performance Analyzer

This module provides performance analysis capabilities to identify
potential performance bottlenecks and optimization opportunities.
"""

import os
import re
import json
import logging
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass

from ..utils.file_utils import find_files, read_file
from ..utils.log_utils import setup_logging


@dataclass
class PerformanceIssue:
    """Represents a performance issue"""
    issue_id: str
    title: str
    description: str
    severity: str  # 'critical', 'high', 'medium', 'low'
    category: str  # 'algorithm', 'memory', 'io', 'cpu', 'network'
    file_path: str
    line_number: int
    impact_score: float
    recommendation: str


@dataclass
class PerformanceMetrics:
    """Performance metrics for a function or file"""
    complexity_score: float
    memory_usage: int
    cpu_usage: float
    execution_time: float
    calls_count: int


@dataclass
class PerformanceAnalysisResult:
    """Result of performance analysis"""
    issues: List[PerformanceIssue]
    metrics: Dict[str, PerformanceMetrics]
    bottlenecks: List[str]
    recommendations: List[str]
    summary: Dict[str, Any]
    success: bool
    error_message: Optional[str]


class PerformanceAnalyzer:
    """Performance analyzer"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize performance analyzer

        Args:
            config: Configuration dictionary
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        self.complexity_threshold = config.get('complexity_threshold', 10)
        self.function_size_threshold = config.get('function_size_threshold', 50)

    def analyze(self, project_root: Path) -> PerformanceAnalysisResult:
        """
        Analyze project for performance issues

        Args:
            project_root: Root directory of the project

        Returns:
            Performance analysis result
        """
        self.logger.info(f"Starting performance analysis for project: {project_root}")

        try:
            # Find all source files
            source_files = find_files(project_root, [
                '*.py', '*.java', '*.cpp', '*.c', '*.h', '*.hpp', '*.js', '*.ts'
            ])

            self.logger.info(f"Found {len(source_files)} source files")

            # Analyze each file
            all_issues = []
            all_metrics = {}

            for file_path in source_files:
                file_issues, file_metrics = self._analyze_file(file_path)
                all_issues.extend(file_issues)
                all_metrics[str(file_path.relative_to(project_root))] = file_metrics

            # Identify bottlenecks
            bottlenecks = self._identify_bottlenecks(all_issues, all_metrics)

            # Generate recommendations
            recommendations = self._generate_recommendations(all_issues, all_metrics)

            # Create summary
            summary = self._create_summary(all_issues, all_metrics)

            result = PerformanceAnalysisResult(
                issues=all_issues,
                metrics=all_metrics,
                bottlenecks=bottlenecks,
                recommendations=recommendations,
                summary=summary,
                success=True,
                error_message=None
            )

            self.logger.info(f"Performance analysis completed: {len(all_issues)} issues found")
            return result

        except Exception as e:
            self.logger.error(f"Performance analysis failed: {str(e)}")
            return PerformanceAnalysisResult(
                issues=[],
                metrics={},
                bottlenecks=[],
                recommendations=[],
                summary={},
                success=False,
                error_message=str(e)
            )

    def _analyze_file(self, file_path: Path) -> Tuple[List[PerformanceIssue], PerformanceMetrics]:
        """Analyze a single file for performance issues"""
        content = read_file(file_path)
        if not content:
            return [], PerformanceMetrics(0, 0, 0, 0, 0)

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

        # General performance analysis
        issues.extend(self._analyze_general_performance(file_path, content, lines))

        # Calculate metrics
        metrics = self._calculate_file_metrics(file_path, content, lines)

        return issues, metrics

    def _analyze_python_file(self, file_path: Path, content: str, lines: List[str]) -> List[PerformanceIssue]:
        """Analyze Python file for performance issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common Python performance issues
            if re.search(r'range\s*\(\s*len\s*\(', line):
                issues.append(PerformanceIssue(
                    issue_id="PY_PERF001",
                    title="Inefficient Range Usage",
                    description="Using range(len()) can be inefficient",
                    severity="medium",
                    category="algorithm",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=6.0,
                    recommendation="Use enumerate() or iterate directly over the sequence"
                ))

            if re.search(r'\.append\s*\(\s*\w+\s*\+\s*\w+\s*\)', line):
                issues.append(PerformanceIssue(
                    issue_id="PY_PERF002",
                    title="Inefficient String Concatenation",
                    description="Using + for string concatenation in loop is inefficient",
                    severity="high",
                    category="memory",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=8.0,
                    recommendation="Use ''.join() or list comprehension for string concatenation"
                ))

            if re.search(r'\[.*\s*\+\s*.*\]', line):
                issues.append(PerformanceIssue(
                    issue_id="PY_PERF003",
                    title="Inefficient List Concatenation",
                    description="Using + for list concatenation creates new list objects",
                    severity="medium",
                    category="memory",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=7.0,
                    recommendation="Use extend() or list comprehension for list concatenation"
                ))

            if re.search(r'for\s+\w+\s+in\s+range\s*\(', line):
                # Check if it's a nested loop (simple heuristic)
                for prev_line_num in range(max(0, line_num - 5), line_num - 1):
                    prev_line = lines[prev_line_num]
                    if re.search(r'for\s+\w+\s+in\s+', prev_line):
                        issues.append(PerformanceIssue(
                            issue_id="PY_PERF004",
                            title="Nested Loop",
                            description="Nested loops can cause performance issues",
                            severity="high",
                            category="cpu",
                            file_path=str(file_path),
                            line_number=line_num,
                            impact_score=9.0,
                            recommendation="Consider using more efficient algorithms or data structures"
                        ))
                        break

        return issues

    def _analyze_cpp_file(self, file_path: Path, content: str, lines: List[str]) -> List[PerformanceIssue]:
        """Analyze C++ file for performance issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common C++ performance issues
            if re.search(r'std::endl', line):
                issues.append(PerformanceIssue(
                    issue_id="CPP_PERF001",
                    title="Expensive Stream Flush",
                    description="std::endl flushes the buffer on every call",
                    severity="medium",
                    category="io",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=5.0,
                    recommendation="Use '\\n' instead of std::endl for better performance"
                ))

            if re.search(r'new\s+\w+\s*\[\s*\w+\s*\]', line):
                issues.append(PerformanceIssue(
                    issue_id="CPP_PERF002",
                    title="Array Allocation on Heap",
                    description="Heap allocation can be expensive and fragmented",
                    severity="medium",
                    category="memory",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=6.0,
                    recommendation="Consider using std::vector or stack allocation when possible"
                ))

            if re.search(r'std::string\s+\w+\s*\=\s*.*\+\s*.*', line):
                issues.append(PerformanceIssue(
                    issue_id="CPP_PERF003",
                    title="String Concatenation",
                    description="String concatenation can be inefficient due to reallocation",
                    severity="medium",
                    category="memory",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=7.0,
                    recommendation="Use std::ostringstream or std::string::append for better performance"
                ))

        return issues

    def _analyze_java_file(self, file_path: Path, content: str, lines: List[str]) -> List[PerformanceIssue]:
        """Analyze Java file for performance issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common Java performance issues
            if re.search(r'for\s*\(\s*\w+\s*:\s*\w+\.toArray\(\s*\)\s*\)', line):
                issues.append(PerformanceIssue(
                    issue_id="JAVA_PERF001",
                    title="Array to ArrayList Conversion in Loop",
                    description="Converting array to list in loop is inefficient",
                    severity="medium",
                    category="algorithm",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=6.0,
                    recommendation="Convert array to list once before the loop"
                ))

            if re.search(r'String\s+\w+\s*=\s*new\s+String\s*\(', line):
                issues.append(PerformanceIssue(
                    issue_id="JAVA_PERF002",
                    title="Unnecessary String Constructor",
                    description="Creating new String objects unnecessarily",
                    severity="low",
                    category="memory",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=3.0,
                    recommendation="Use string literals directly instead of creating new String objects"
                ))

            if re.search(r'String\s*\+\s*.*\+.*\w+', line):
                issues.append(PerformanceIssue(
                    issue_id="JAVA_PERF003",
                    title="String Concatenation in Loop",
                    description="String concatenation in loop creates many temporary objects",
                    severity="high",
                    category="memory",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=8.0,
                    recommendation="Use StringBuilder for string concatenation in loops"
                ))

        return issues

    def _analyze_javascript_file(self, file_path: Path, content: str, lines: List[str]) -> List[PerformanceIssue]:
        """Analyze JavaScript/TypeScript file for performance issues"""
        issues = []

        for line_num, line in enumerate(lines, 1):
            # Check for common JavaScript performance issues
            if re.search(r'for\s*\(\s*\w+\s*=\s*0\s*;\s*\w+\s*<.*;\s*\w+\s*\+\+\s*\)', line):
                issues.append(PerformanceIssue(
                    issue_id="JS_PERF001",
                    title="Traditional For Loop",
                    description="Traditional for loops can be slower than array methods",
                    severity="low",
                    category="algorithm",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=4.0,
                    recommendation="Consider using Array.forEach() or for...of loops"
                ))

            if re.search(r'document\.getElementById\s*\(', line):
                # Check if it's in a loop (simple heuristic)
                for prev_line_num in range(max(0, line_num - 5), line_num - 1):
                    prev_line = lines[prev_line_num]
                    if re.search(r'for\s*\(', prev_line):
                        issues.append(PerformanceIssue(
                            issue_id="JS_PERF002",
                            title="DOM Query in Loop",
                            description="DOM queries inside loops are expensive",
                            severity="high",
                            category="io",
                            file_path=str(file_path),
                            line_number=line_num,
                            impact_score=9.0,
                            recommendation="Cache DOM queries outside of loops"
                        ))
                        break

            if re.search(r'setTimeout\s*\(\s*.*,\s*0\s*\)', line):
                issues.append(PerformanceIssue(
                    issue_id="JS_PERF003",
                    title="setTimeout with 0 delay",
                    description="setTimeout with 0ms delay still has minimum delay",
                    severity="medium",
                    category="cpu",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=5.0,
                    recommendation="Use requestAnimationFrame or consider if delay is necessary"
                ))

        return issues

    def _analyze_general_performance(self, file_path: Path, content: str, lines: List[str]) -> List[PerformanceIssue]:
        """Analyze general performance issues"""
        issues = []

        # Check function size
        functions = self._find_functions(content, file_path.suffix)
        for func_name, func_lines in functions:
            if len(func_lines) > self.function_size_threshold:
                issues.append(PerformanceIssue(
                    issue_id="GEN_PERF001",
                    title="Large Function",
                    description=f"Function {func_name} is too large ({len(func_lines)} lines)",
                    severity="medium",
                    category="maintainability",
                    file_path=str(file_path),
                    line_number=func_lines[0] if func_lines else 0,
                    impact_score=5.0,
                    recommendation="Consider breaking down large functions into smaller, more focused functions"
                ))

        # Check for deep nesting
        for line_num, line in enumerate(lines, 1):
            # Count indentation level
            indent_level = len(line) - len(line.lstrip())
            max_indent = indent_level // 4  # Assuming 4 spaces per level

            if max_indent > 5:
                issues.append(PerformanceIssue(
                    issue_id="GEN_PERF002",
                    title="Deep Nesting",
                    description=f"Code is nested {max_indent} levels deep",
                    severity="medium",
                    category="algorithm",
                    file_path=str(file_path),
                    line_number=line_num,
                    impact_score=6.0,
                    recommendation="Consider refactoring to reduce nesting levels using early returns or helper functions"
                ))

        return issues

    def _find_functions(self, content: str, file_extension: str) -> List[Tuple[str, List[int]]]:
        """Find functions in the content and return their names and line numbers"""
        functions = []
        lines = content.split('\n')

        if file_extension == '.py':
            # Python function patterns
            patterns = [
                r'def\s+(\w+)\s*\(',
                r'async\s+def\s+(\w+)\s*\(',
                r'class\s+(\w+)\s*\(',
                r'class\s+(\w+)\s*:',
            ]
        elif file_extension in {'.java', '.cpp', '.c', '.h', '.hpp'}:
            # C/C++/Java function patterns
            patterns = [
                r'(\w+)\s+\w+\s*\([^)]*\)\s*\{',
                r'(\w+)\s*\([^)]*\)\s*\{',
                r'class\s+(\w+)\s*\{',
                r'struct\s+(\w+)\s*\{',
            ]
        elif file_extension in {'.js', '.ts'}:
            # JavaScript/TypeScript function patterns
            patterns = [
                r'function\s+(\w+)\s*\(',
                r'const\s+(\w+)\s*=\s*\([^)]*\)\s*=>',
                r'(\w+)\s*:\s*function\s*\(',
                r'class\s+(\w+)\s*\{',
            ]
        else:
            return functions

        for line_num, line in enumerate(lines, 1):
            for pattern in patterns:
                match = re.search(pattern, line)
                if match:
                    func_name = match.group(1)
                    # Find function end (simplified)
                    func_end = self._find_function_end(lines, line_num - 1, file_extension)
                    func_lines = list(range(line_num, func_end + 1))
                    functions.append((func_name, func_lines))

        return functions

    def _find_function_end(self, lines: List[str], start_idx: int, file_extension: str) -> int:
        """Find the end of a function (simplified implementation)"""
        brace_count = 0
        for i in range(start_idx, len(lines)):
            line = lines[i]
            if '{' in line:
                brace_count += line.count('{')
            if '}' in line:
                brace_count -= line.count('}')
                if brace_count <= 0:
                    return i + 1

        return len(lines)  # Default to end of file

    def _calculate_file_metrics(self, file_path: Path, content: str, lines: List[str]) -> PerformanceMetrics:
        """Calculate performance metrics for a file"""
        # Calculate complexity (simplified)
        complexity_keywords = {'if', 'else', 'elif', 'for', 'while', 'switch', 'case', 'catch', 'try'}
        complexity = sum(
            len(re.findall(r'\b' + keyword + r'\b', line))
            for line in lines
            for keyword in complexity_keywords
        )

        # Estimate memory usage (very rough approximation)
        estimated_memory = len(content.encode('utf-8')) + (complexity * 100)

        # Estimate CPU usage based on complexity
        cpu_usage = min(complexity * 0.1, 10.0)  # Cap at 10.0

        return PerformanceMetrics(
            complexity_score=complexity,
            memory_usage=estimated_memory,
            cpu_usage=cpu_usage,
            execution_time=0.0,  # Would need profiling data
            calls_count=0  # Would need instrumentation
        )

    def _identify_bottlenecks(self, issues: List[PerformanceIssue], metrics: Dict[str, PerformanceMetrics]) -> List[str]:
        """Identify performance bottlenecks"""
        bottlenecks = []

        # Find files with highest complexity
        high_complexity_files = sorted(
            [(file_path, metrics.complexity_score) for file_path, metrics in metrics.items()],
            key=lambda x: x[1],
            reverse=True
        )[:5]

        for file_path, complexity in high_complexity_files:
            if complexity > self.complexity_threshold:
                bottlenecks.append(f"High complexity in {file_path}: {complexity}")

        # Find high-impact issues
        high_impact_issues = sorted(issues, key=lambda x: x.impact_score, reverse=True)[:5]
        for issue in high_impact_issues:
            bottlenecks.append(f"{issue.title} in {issue.file_path} (impact: {issue.impact_score})")

        return bottlenecks

    def _generate_recommendations(self, issues: List[PerformanceIssue], metrics: Dict[str, PerformanceMetrics]) -> List[str]:
        """Generate performance recommendations"""
        recommendations = []

        # General recommendations based on issues
        issue_categories = set(issue.category for issue in issues)

        if 'algorithm' in issue_categories:
            recommendations.append("Review algorithms for time complexity improvements")
            recommendations.append("Consider using more efficient data structures")

        if 'memory' in issue_categories:
            recommendations.append("Profile memory usage to identify allocations")
            recommendations.append("Consider using object pools or caching for frequently allocated objects")

        if 'cpu' in issue_categories:
            recommendations.append("Profile CPU usage to identify bottlenecks")
            recommendations.append("Consider multithreading or asynchronous processing for CPU-intensive tasks")

        if 'io' in issue_categories:
            recommendations.append("Use buffering for I/O operations")
            recommendations.append("Consider asynchronous I/O for better performance")

        # Specific recommendations for high-complexity files
        high_complexity_files = [
            file_path for file_path, metric in metrics.items()
            if metric.complexity_score > self.complexity_threshold
        ]

        if high_complexity_files:
            recommendations.append(f"Refactor high-complexity files: {', '.join(high_complexity_files[:3])}")

        return recommendations

    def _create_summary(self, issues: List[PerformanceIssue], metrics: Dict[str, PerformanceMetrics]) -> Dict[str, Any]:
        """Create summary of performance analysis"""
        summary = {
            'total_issues': len(issues),
            'files_analyzed': len(metrics),
            'severity_distribution': {},
            'category_distribution': {},
            'average_complexity': 0,
            'high_complexity_files': 0,
            'most_impactful_issues': []
        }

        if issues:
            # Calculate distributions
            for issue in issues:
                summary['severity_distribution'][issue.severity] = summary['severity_distribution'].get(issue.severity, 0) + 1
                summary['category_distribution'][issue.category] = summary['category_distribution'].get(issue.category, 0) + 1

        if metrics:
            # Calculate complexity statistics
            total_complexity = sum(metric.complexity_score for metric in metrics.values())
            summary['average_complexity'] = total_complexity / len(metrics)
            summary['high_complexity_files'] = len([
                metric for metric in metrics.values()
                if metric.complexity_score > self.complexity_threshold
            ])

        # Find most impactful issues
        summary['most_impactful_issues'] = [
            {
                'title': issue.title,
                'file': issue.file_path,
                'line': issue.line_number,
                'impact_score': issue.impact_score
            }
            for issue in sorted(issues, key=lambda x: x.impact_score, reverse=True)[:10]
        ]

        return summary

    def profile_execution(self, command: List[str], working_dir: Path, timeout: int = 30) -> Dict[str, Any]:
        """
        Profile execution of a command (simplified implementation)

        Args:
            command: Command to execute
            working_dir: Working directory
            timeout: Timeout in seconds

        Returns:
            Profile results
        """
        self.logger.info(f"Profiling command: {' '.join(command)}")

        try:
            start_time = time.time()
            result = subprocess.run(
                command,
                cwd=working_dir,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            end_time = time.time()

            return {
                'success': result.returncode == 0,
                'execution_time': end_time - start_time,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'return_code': result.returncode
            }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Command timed out',
                'timeout': timeout
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }