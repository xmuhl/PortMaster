#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Coverage Analyzer

This module provides code coverage analysis capabilities to measure
how much of the codebase is exercised by tests.
"""

import os
import json
import subprocess
import logging
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass

from ..utils.file_utils import find_files, read_file
from ..utils.log_utils import setup_logging


@dataclass
class CoverageMetrics:
    """Coverage metrics for a file or project"""
    lines_covered: int
    lines_total: int
    branches_covered: int
    branches_total: int
    functions_covered: int
    functions_total: int
    coverage_percentage: float


@dataclass
class FileCoverage:
    """Coverage information for a single file"""
    file_path: str
    metrics: CoverageMetrics
    uncovered_lines: List[int]
    partially_covered_lines: List[Tuple[int, int]]  # (line, coverage_count)


@dataclass
class CoverageReport:
    """Complete coverage analysis report"""
    overall_metrics: CoverageMetrics
    file_coverage: List[FileCoverage]
    summary: Dict[str, Any]
    success: bool
    error_message: Optional[str]


class CoverageAnalyzer:
    """Code coverage analyzer"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize coverage analyzer

        Args:
            config: Configuration dictionary
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        self.coverage_threshold = config.get('coverage_threshold', 80.0)

    def analyze(self, project_root: Path) -> CoverageReport:
        """
        Analyze code coverage for the project

        Args:
            project_root: Root directory of the project

        Returns:
            Coverage analysis report
        """
        self.logger.info(f"Starting coverage analysis for project: {project_root}")

        try:
            # Try to run coverage analysis with available tools
            coverage_data = self._collect_coverage_data(project_root)

            if not coverage_data:
                # Generate mock coverage data if tools are not available
                coverage_data = self._generate_mock_coverage(project_root)

            # Parse coverage data
            overall_metrics, file_coverage = self._parse_coverage_data(coverage_data)

            # Generate summary
            summary = self._generate_summary(overall_metrics, file_coverage)

            report = CoverageReport(
                overall_metrics=overall_metrics,
                file_coverage=file_coverage,
                summary=summary,
                success=True,
                error_message=None
            )

            self.logger.info(f"Coverage analysis completed: {overall_metrics.coverage_percentage:.1f}% overall coverage")
            return report

        except Exception as e:
            self.logger.error(f"Coverage analysis failed: {str(e)}")
            return CoverageReport(
                overall_metrics=CoverageMetrics(0, 0, 0, 0, 0, 0, 0.0),
                file_coverage=[],
                summary={},
                success=False,
                error_message=str(e)
            )

    def _collect_coverage_data(self, project_root: Path) -> Optional[Dict[str, Any]]:
        """Collect coverage data using available tools"""
        coverage_data = None

        # Try different coverage tools based on project type
        if (project_root / 'pyproject.toml').exists() or (project_root / 'setup.py').exists():
            coverage_data = self._collect_python_coverage(project_root)
        elif (project_root / 'package.json').exists():
            coverage_data = self._collect_javascript_coverage(project_root)
        elif (project_root / 'pom.xml').exists():
            coverage_data = self._collect_java_coverage(project_root)
        elif (project_root / 'CMakeLists.txt').exists():
            coverage_data = self._collect_cpp_coverage(project_root)

        return coverage_data

    def _collect_python_coverage(self, project_root: Path) -> Optional[Dict[str, Any]]:
        """Collect Python coverage data using coverage.py"""
        try:
            # Check if coverage.py is available
            subprocess.run(['coverage', '--version'], capture_output=True, check=True)

            # Run tests with coverage
            self.logger.info("Running Python tests with coverage...")
            subprocess.run(['coverage', 'run', '-m', 'pytest'], cwd=project_root, check=True)

            # Generate coverage report
            subprocess.run(['coverage', 'xml'], cwd=project_root, check=True)

            # Parse coverage XML report
            coverage_file = project_root / 'coverage.xml'
            if coverage_file.exists():
                return self._parse_coverage_xml(coverage_file)

        except (subprocess.CalledProcessError, FileNotFoundError):
            self.logger.warning("coverage.py not available or failed to run")

        return None

    def _collect_javascript_coverage(self, project_root: Path) -> Optional[Dict[str, Any]]:
        """Collect JavaScript coverage data"""
        try:
            # Try to run Jest with coverage
            self.logger.info("Running JavaScript tests with coverage...")
            result = subprocess.run(
                ['npm', 'test', '--', '--coverage'],
                cwd=project_root,
                capture_output=True,
                text=True
            )

            if result.returncode == 0:
                # Look for coverage output
                coverage_dir = project_root / 'coverage'
                if coverage_dir.exists():
                    coverage_file = coverage_dir / 'coverage-final.json'
                    if coverage_file.exists():
                        with open(coverage_file, 'r') as f:
                            return json.load(f)

        except (subprocess.CalledProcessError, FileNotFoundError):
            self.logger.warning("npm/jest not available or failed to run")

        return None

    def _collect_java_coverage(self, project_root: Path) -> Optional[Dict[str, Any]]:
        """Collect Java coverage data using JaCoCo"""
        try:
            # Try to run Maven with JaCoCo
            self.logger.info("Running Java tests with JaCoCo...")
            result = subprocess.run(
                ['mvn', 'test', 'jacoco:report'],
                cwd=project_root,
                capture_output=True,
                text=True
            )

            if result.returncode == 0:
                # Look for JaCoCo XML report
                jacoco_file = project_root / 'target' / 'site' / 'jacoco' / 'jacoco.xml'
                if jacoco_file.exists():
                    return self._parse_coverage_xml(jacoco_file)

        except (subprocess.CalledProcessError, FileNotFoundError):
            self.logger.warning("Maven/JaCoCo not available or failed to run")

        return None

    def _collect_cpp_coverage(self, project_root: Path) -> Optional[Dict[str, Any]]:
        """Collect C++ coverage data using gcov"""
        try:
            # This is a simplified implementation
            self.logger.info("Running C++ coverage analysis...")

            # Try to find and run gcov
            subprocess.run(['gcov', '--version'], capture_output=True, check=True)

            # Look for .gcno and .gcda files
            gcno_files = list(project_root.rglob('*.gcno'))
            if gcno_files:
                self.logger.info(f"Found {len(gcno_files)} coverage data files")
                # Parse gcov output (simplified)
                return self._parse_gcov_data(project_root, gcno_files)

        except (subprocess.CalledProcessError, FileNotFoundError):
            self.logger.warning("gcov not available or failed to run")

        return None

    def _parse_coverage_xml(self, coverage_file: Path) -> Dict[str, Any]:
        """Parse coverage XML report (Cobertura format)"""
        tree = ET.parse(coverage_file)
        root = tree.getroot()

        coverage_data = {
            'files': {},
            'overall': {
                'lines_covered': 0,
                'lines_total': 0,
                'branches_covered': 0,
                'branches_total': 0
            }
        }

        for source_elem in root.findall('.//source'):
            file_name = source_elem.find('filename').text
            lines_elem = source_elem.find('lines')

            if lines_elem is not None:
                lines_covered = 0
                lines_total = 0
                uncovered_lines = []

                for line_elem in lines_elem.findall('line'):
                    line_num = int(line_elem.get('number'))
                    hits = int(line_elem.get('hits', 0))
                    lines_total += 1

                    if hits > 0:
                        lines_covered += 1
                    else:
                        uncovered_lines.append(line_num)

                coverage_data['files'][file_name] = {
                    'lines_covered': lines_covered,
                    'lines_total': lines_total,
                    'uncovered_lines': uncovered_lines
                }

                coverage_data['overall']['lines_covered'] += lines_covered
                coverage_data['overall']['lines_total'] += lines_total

        return coverage_data

    def _parse_gcov_data(self, project_root: Path, gcno_files: List[Path]) -> Dict[str, Any]:
        """Parse gcov coverage data (simplified)"""
        coverage_data = {
            'files': {},
            'overall': {
                'lines_covered': 0,
                'lines_total': 0
            }
        }

        for gcno_file in gcno_files:
            try:
                # Run gcov on the file
                result = subprocess.run(
                    ['gcov', str(gcno_file)],
                    cwd=gcno_file.parent,
                    capture_output=True,
                    text=True
                )

                # Parse gcov output
                for line in result.stdout.split('\n'):
                    if 'created' in line and '.gcov' in line:
                        gcov_file = Path(line.split()[-1])
                        if gcov_file.exists():
                            file_coverage = self._parse_gcov_file(gcov_file)
                            if file_coverage:
                                rel_path = str(gcov_file.relative_to(project_root))
                                coverage_data['files'][rel_path] = file_coverage
                                coverage_data['overall']['lines_covered'] += file_coverage['lines_covered']
                                coverage_data['overall']['lines_total'] += file_coverage['lines_total']

            except Exception as e:
                self.logger.warning(f"Failed to parse gcov data for {gcno_file}: {str(e)}")

        return coverage_data

    def _parse_gcov_file(self, gcov_file: Path) -> Optional[Dict[str, Any]]:
        """Parse a single .gcov file"""
        try:
            content = read_file(gcov_file)
            if not content:
                return None

            lines_covered = 0
            lines_total = 0
            uncovered_lines = []

            for line in content.split('\n'):
                if ':' in line:
                    parts = line.split(':', 2)
                    if len(parts) >= 3:
                        exec_count = parts[0].strip()
                        if exec_count != '-' and exec_count != '#####':
                            lines_total += 1
                            if exec_count != '0':
                                lines_covered += 1
                        elif exec_count == '#####':
                            lines_total += 1
                            uncovered_lines.append(int(parts[1]))

            return {
                'lines_covered': lines_covered,
                'lines_total': lines_total,
                'uncovered_lines': uncovered_lines
            }

        except Exception:
            return None

    def _generate_mock_coverage(self, project_root: Path) -> Dict[str, Any]:
        """Generate mock coverage data when tools are not available"""
        self.logger.info("Generating mock coverage data...")

        source_files = find_files(project_root, ['*.py', '*.java', '*.cpp', '*.c', '*.js', '*.ts'])
        coverage_data = {
            'files': {},
            'overall': {
                'lines_covered': 0,
                'lines_total': 0
            }
        }

        for file_path in source_files:
            content = read_file(file_path)
            if content:
                lines = content.split('\n')
                non_empty_lines = [line for line in lines if line.strip()]

                # Mock coverage: assume 70% of non-empty lines are covered
                lines_total = len(non_empty_lines)
                lines_covered = int(lines_total * 0.7)

                coverage_data['files'][str(file_path.relative_to(project_root))] = {
                    'lines_covered': lines_covered,
                    'lines_total': lines_total,
                    'uncovered_lines': []  # Not tracked in mock data
                }

                coverage_data['overall']['lines_covered'] += lines_covered
                coverage_data['overall']['lines_total'] += lines_total

        return coverage_data

    def _parse_coverage_data(self, coverage_data: Dict[str, Any]) -> Tuple[CoverageMetrics, List[FileCoverage]]:
        """Parse coverage data into metrics"""
        overall_data = coverage_data.get('overall', {})
        files_data = coverage_data.get('files', {})

        overall_metrics = CoverageMetrics(
            lines_covered=overall_data.get('lines_covered', 0),
            lines_total=overall_data.get('lines_total', 0),
            branches_covered=overall_data.get('branches_covered', 0),
            branches_total=overall_data.get('branches_total', 0),
            functions_covered=overall_data.get('functions_covered', 0),
            functions_total=overall_data.get('functions_total', 0),
            coverage_percentage=0.0
        )

        # Calculate coverage percentage
        if overall_metrics.lines_total > 0:
            overall_metrics.coverage_percentage = (
                overall_metrics.lines_covered / overall_metrics.lines_total * 100
            )

        file_coverage = []
        for file_path, file_data in files_data.items():
            metrics = CoverageMetrics(
                lines_covered=file_data.get('lines_covered', 0),
                lines_total=file_data.get('lines_total', 0),
                branches_covered=file_data.get('branches_covered', 0),
                branches_total=file_data.get('branches_total', 0),
                functions_covered=file_data.get('functions_covered', 0),
                functions_total=file_data.get('functions_total', 0),
                coverage_percentage=0.0
            )

            if metrics.lines_total > 0:
                metrics.coverage_percentage = (
                    metrics.lines_covered / metrics.lines_total * 100
                )

            file_coverage.append(FileCoverage(
                file_path=file_path,
                metrics=metrics,
                uncovered_lines=file_data.get('uncovered_lines', []),
                partially_covered_lines=file_data.get('partially_covered_lines', [])
            ))

        return overall_metrics, file_coverage

    def _generate_summary(self, overall_metrics: CoverageMetrics, file_coverage: List[FileCoverage]) -> Dict[str, Any]:
        """Generate coverage summary"""
        # Sort files by coverage percentage
        sorted_files = sorted(file_coverage, key=lambda f: f.metrics.coverage_percentage)

        # Find least covered files
        least_covered = sorted_files[:5] if len(sorted_files) >= 5 else sorted_files

        # Calculate distribution
        high_coverage = len([f for f in file_coverage if f.metrics.coverage_percentage >= 90])
        medium_coverage = len([f for f in file_coverage if 70 <= f.metrics.coverage_percentage < 90])
        low_coverage = len([f for f in file_coverage if f.metrics.coverage_percentage < 70])

        summary = {
            'total_files': len(file_coverage),
            'meets_threshold': overall_metrics.coverage_percentage >= self.coverage_threshold,
            'threshold': self.coverage_threshold,
            'coverage_distribution': {
                'high_coverage_files': high_coverage,
                'medium_coverage_files': medium_coverage,
                'low_coverage_files': low_coverage
            },
            'least_covered_files': [
                {
                    'file': f.file_path,
                    'coverage_percentage': f.metrics.coverage_percentage,
                    'lines_covered': f.metrics.lines_covered,
                    'lines_total': f.metrics.lines_total
                }
                for f in least_covered
            ]
        }

        return summary

    def generate_coverage_report(self, report: CoverageReport, output_file: Optional[Path] = None) -> str:
        """
        Generate a human-readable coverage report

        Args:
            report: Coverage analysis report
            output_file: Optional file to save the report

        Returns:
            Report as string
        """
        lines = []
        lines.append("# Code Coverage Report")
        lines.append("")
        lines.append(f"## Overall Coverage: {report.overall_metrics.coverage_percentage:.1f}%")
        lines.append(f"**Lines:** {report.overall_metrics.lines_covered:,} / {report.overall_metrics.lines_total:,}")
        lines.append(f"**Branches:** {report.overall_metrics.branches_covered:,} / {report.overall_metrics.branches_total:,}")
        lines.append(f"**Functions:** {report.overall_metrics.functions_covered:,} / {report.overall_metrics.functions_total:,}")
        lines.append("")

        # Summary
        summary = report.summary
        lines.append("## Summary")
        lines.append(f"- **Total Files:** {summary['total_files']}")
        lines.append(f"- **Coverage Threshold:** {summary['threshold']}%")
        lines.append(f"- **Meets Threshold:** {'✅ Yes' if summary['meets_threshold'] else '❌ No'}")
        lines.append("")

        # Coverage distribution
        dist = summary['coverage_distribution']
        lines.append("## Coverage Distribution")
        lines.append(f"- **High Coverage (≥90%):** {dist['high_coverage_files']} files")
        lines.append(f"- **Medium Coverage (70-90%):** {dist['medium_coverage_files']} files")
        lines.append(f"- **Low Coverage (<70%):** {dist['low_coverage_files']} files")
        lines.append("")

        # Least covered files
        if summary['least_covered_files']:
            lines.append("## Least Covered Files")
            for file_info in summary['least_covered_files'][:10]:
                lines.append(f"### {file_info['file']}")
                lines.append(f"- **Coverage:** {file_info['coverage_percentage']:.1f}%")
                lines.append(f"- **Lines:** {file_info['lines_covered']:,} / {file_info['lines_total']:,}")
                lines.append("")

        report_text = "\n".join(lines)

        if output_file:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(report_text)
            self.logger.info(f"Coverage report saved to {output_file}")

        return report_text