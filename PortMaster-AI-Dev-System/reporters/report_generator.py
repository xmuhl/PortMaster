#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Report Generator

This module provides comprehensive report generation capabilities
for creating detailed analysis reports in multiple formats.
"""

import os
import json
import logging
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any
from jinja2 import Template, Environment, FileSystemLoader

from ..utils.file_utils import ensure_directory, save_json
from ..utils.log_utils import setup_logging


class ReportGenerator:
    """Comprehensive report generator"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize report generator

        Args:
            config: Configuration dictionary
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        self.output_dir = Path(config.get('output_dir', 'reports'))
        self.formats = config.get('formats', ['json', 'markdown', 'html'])
        self.include_metrics = config.get('include_metrics', True)

        # Setup Jinja2 environment
        template_dir = Path(__file__).parent / 'templates'
        self.jinja_env = Environment(
            loader=FileSystemLoader(template_dir),
            autoescape=True
        )

    def generate_report(self, results: Dict[str, Any], format_type: str = 'markdown') -> str:
        """
        Generate a comprehensive report from analysis results

        Args:
            results: Analysis results from the system
            format_type: Output format ('json', 'markdown', 'html')

        Returns:
            Generated report as string
        """
        self.logger.info(f"Generating {format_type} report")

        try:
            if format_type == 'json':
                return self._generate_json_report(results)
            elif format_type == 'markdown':
                return self._generate_markdown_report(results)
            elif format_type == 'html':
                return self._generate_html_report(results)
            else:
                raise ValueError(f"Unsupported format: {format_type}")

        except Exception as e:
            self.logger.error(f"Failed to generate {format_type} report: {str(e)}")
            return f"Error generating report: {str(e)}"

    def _generate_json_report(self, results: Dict[str, Any]) -> str:
        """Generate JSON format report"""
        # Add metadata
        report_data = {
            'metadata': {
                'generated_at': datetime.now().isoformat(),
                'generator': 'AI Development System v1.0',
                'format': 'json'
            },
            'results': results
        }

        # Calculate summary statistics
        if 'steps' in results:
            report_data['summary'] = self._calculate_summary(results['steps'])

        return json.dumps(report_data, indent=2, default=str)

    def _generate_markdown_report(self, results: Dict[str, Any]) -> str:
        """Generate Markdown format report"""
        template_str = self._get_markdown_template()
        template = Template(template_str)

        # Prepare template variables
        template_vars = {
            'generated_at': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            'results': results,
            'summary': self._calculate_summary(results.get('steps', {})),
            'steps': results.get('steps', {}),
            'include_metrics': self.include_metrics
        }

        return template.render(**template_vars)

    def _generate_html_report(self, results: Dict[str, Any]) -> str:
        """Generate HTML format report"""
        template_str = self._get_html_template()
        template = Template(template_str)

        # Prepare template variables
        template_vars = {
            'generated_at': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            'results': results,
            'summary': self._calculate_summary(results.get('steps', {})),
            'steps': results.get('steps', {}),
            'include_metrics': self.include_metrics
        }

        return template.render(**template_vars)

    def _calculate_summary(self, steps: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate summary statistics from step results"""
        summary = {
            'total_steps': len(steps),
            'successful_steps': 0,
            'failed_steps': 0,
            'total_duration': 0,
            'issues_found': 0,
            'coverage_percentage': 0,
            'build_status': 'unknown'
        }

        for step_name, step_result in steps.items():
            if step_result.get('success', False):
                summary['successful_steps'] += 1
            else:
                summary['failed_steps'] += 1

            # Accumulate duration
            duration = step_result.get('duration', 0)
            if isinstance(duration, (int, float)):
                summary['total_duration'] += duration

            # Collect metrics
            if step_name == 'build_verification':
                summary['build_status'] = 'success' if step_result.get('success', False) else 'failed'

            elif step_name == 'test_execution':
                total_tests = step_result.get('total_tests', 0)
                passed_tests = step_result.get('passed_tests', 0)
                summary['tests_total'] = total_tests
                summary['tests_passed'] = passed_tests
                summary['tests_failed'] = total_tests - passed_tests

                coverage = step_result.get('coverage', 0)
                if isinstance(coverage, (int, float)):
                    summary['coverage_percentage'] = coverage

            elif step_name == 'static_analysis':
                issues = step_result.get('issues_found', 0)
                if isinstance(issues, int):
                    summary['issues_found'] += issues

            elif step_name == 'error_detection':
                patterns = step_result.get('patterns_found', 0)
                if isinstance(patterns, int):
                    summary['issues_found'] += patterns

        # Calculate success rate
        if summary['total_steps'] > 0:
            summary['success_rate'] = (summary['successful_steps'] / summary['total_steps']) * 100
        else:
            summary['success_rate'] = 0

        return summary

    def _get_markdown_template(self) -> str:
        """Get Markdown report template"""
        return '''# AI Development System Analysis Report

## Executive Summary

**Generated:** {{ generated_at }}
**Total Steps:** {{ summary.total_steps }}
**Success Rate:** {{ "%.1f"|format(summary.success_rate) }}%
**Duration:** {{ "%.2f"|format(summary.total_duration) }} seconds

### Quick Stats
{%- if summary.build_status != 'unknown' %}
- **Build Status:** {{ "✅ Success" if summary.build_status == 'success' else "❌ Failed" }}
{%- endif %}
{%- if summary.tests_total %}
- **Tests:** {{ summary.tests_passed }}/{{ summary.tests_total }} passed
{%- endif %}
{%- if summary.coverage_percentage > 0 %}
- **Coverage:** {{ "%.1f"|format(summary.coverage_percentage) }}%
{%- endif %}
{%- if summary.issues_found > 0 %}
- **Issues Found:** {{ summary.issues_found }}
{%- endif %}

## Detailed Results

{% for step_name, step_result in steps.items() %}
### {{ step_name.replace('_', ' ').title() }}

**Status:** {{ "✅ Success" if step_result.success else "❌ Failed" }}
**Duration:** {{ "%.2f"|format(step_result.duration) }} seconds

{% if not step_result.success and step_result.error %}
**Error:** `{{ step_result.error }}`
{% endif %}

{% if step_result.success and include_metrics %}
{% if step_name == 'build_verification' %}
- **Warnings:** {{ step_result.warnings | length }}
- **Errors:** {{ step_result.errors | length }}
{%- endif %}

{% if step_name == 'test_execution' %}
- **Total Tests:** {{ step_result.total_tests }}
- **Passed:** {{ step_result.passed_tests }}
- **Failed:** {{ step_result.failed_tests }}
- **Coverage:** {{ "%.1f"|format(step_result.coverage) }}%
{%- endif %}

{% if step_name == 'static_analysis' %}
- **Issues Found:** {{ step_result.issues_found }}
- **Complexity Violations:** {{ step_result.complexity_violations }}
{%- endif %}

{% if step_name == 'coverage_analysis' %}
- **Overall Coverage:** {{ "%.1f"|format(step_result.overall_coverage) }}%
- **Line Coverage:** {{ step_result.line_coverage }}%
- **Branch Coverage:** {{ step_result.branch_coverage }}%
{%- endif %}

{% if step_name == 'error_detection' %}
- **Patterns Found:** {{ step_result.patterns_found }}
- **Critical Issues:** {{ step_result.critical_issues | length }}
{%- endif %}

{% if step_name == 'report_generation' %}
- **Reports Generated:** {{ step_result.reports_generated | length }}
- **Report Paths:**
{% for format, path in step_result.report_paths.items() %}
  - {{ format }}: `{{ path }}`
{% endfor %}
{%- endif %}
{% endif %}

{% endfor %}

## Recommendations

Based on the analysis results, here are the key recommendations:

{% if summary.build_status == 'failed' %}
### 🔴 Build Issues
- Fix build errors before proceeding with further development
- Review compilation warnings for potential issues
{% endif %}

{% if summary.tests_total and summary.tests_failed > 0 %}
### 🧪 Test Coverage
- Address {{ summary.tests_failed }} failing tests
- Improve test coverage (current: {{ "%.1f"|format(summary.coverage_percentage) }}%)
{% endif %}

{% if summary.issues_found > 0 %}
### 🔍 Code Quality
- Fix {{ summary.issues_found }} identified code issues
- Focus on critical issues first
{% endif %}

## Generated Files

All reports and artifacts have been generated in the reports directory.

---

*This report was generated by the AI Development System v1.0*
'''

    def _get_html_template(self) -> str:
        """Get HTML report template"""
        return '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI Development System Analysis Report</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .header {
            border-bottom: 2px solid #e9ecef;
            padding-bottom: 20px;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #2c3e50;
            margin: 0;
        }
        .meta {
            color: #6c757d;
            font-size: 0.9em;
        }
        .summary {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .summary-card {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 6px;
            border-left: 4px solid #007bff;
        }
        .summary-card h3 {
            margin: 0 0 10px 0;
            color: #495057;
        }
        .summary-card .value {
            font-size: 1.5em;
            font-weight: bold;
            color: #007bff;
        }
        .success { border-left-color: #28a745; }
        .success .value { color: #28a745; }
        .warning { border-left-color: #ffc107; }
        .warning .value { color: #ffc107; }
        .danger { border-left-color: #dc3545; }
        .danger .value { color: #dc3545; }
        .step {
            margin-bottom: 30px;
            border: 1px solid #e9ecef;
            border-radius: 6px;
            overflow: hidden;
        }
        .step-header {
            background: #f8f9fa;
            padding: 15px 20px;
            border-bottom: 1px solid #e9ecef;
        }
        .step-header h3 {
            margin: 0;
            color: #495057;
        }
        .step-content {
            padding: 20px;
        }
        .status-success { color: #28a745; }
        .status-failed { color: #dc3545; }
        .metrics {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-top: 15px;
        }
        .metric {
            background: #f8f9fa;
            padding: 10px;
            border-radius: 4px;
        }
        .metric-label {
            font-size: 0.9em;
            color: #6c757d;
        }
        .metric-value {
            font-weight: bold;
            color: #495057;
        }
        .error {
            background: #f8d7da;
            color: #721c24;
            padding: 10px;
            border-radius: 4px;
            font-family: monospace;
            margin-top: 10px;
        }
        .recommendations {
            background: #d1ecf1;
            border: 1px solid #bee5eb;
            border-radius: 6px;
            padding: 20px;
            margin-top: 30px;
        }
        .recommendations h3 {
            margin-top: 0;
            color: #0c5460;
        }
        .footer {
            text-align: center;
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid #e9ecef;
            color: #6c757d;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>AI Development System Analysis Report</h1>
            <div class="meta">Generated: {{ generated_at }}</div>
        </div>

        <div class="summary">
            <div class="summary-card">
                <h3>Total Steps</h3>
                <div class="value">{{ summary.total_steps }}</div>
            </div>
            <div class="summary-card {{ 'success' if summary.success_rate == 100 else 'warning' if summary.success_rate >= 80 else 'danger' }}">
                <h3>Success Rate</h3>
                <div class="value">{{ "%.1f"|format(summary.success_rate) }}%</div>
            </div>
            <div class="summary-card">
                <h3>Duration</h3>
                <div class="value">{{ "%.2f"|format(summary.total_duration) }}s</div>
            </div>
            {% if summary.tests_total %}
            <div class="summary-card {{ 'success' if summary.tests_failed == 0 else 'warning' }}">
                <h3>Tests</h3>
                <div class="value">{{ summary.tests_passed }}/{{ summary.tests_total }}</div>
            </div>
            {% endif %}
            {% if summary.coverage_percentage > 0 %}
            <div class="summary-card {{ 'success' if summary.coverage_percentage >= 80 else 'warning' }}">
                <h3>Coverage</h3>
                <div class="value">{{ "%.1f"|format(summary.coverage_percentage) }}%</div>
            </div>
            {% endif %}
            {% if summary.issues_found > 0 %}
            <div class="summary-card {{ 'warning' if summary.issues_found < 10 else 'danger' }}">
                <h3>Issues</h3>
                <div class="value">{{ summary.issues_found }}</div>
            </div>
            {% endif %}
        </div>

        {% for step_name, step_result in steps.items() %}
        <div class="step">
            <div class="step-header">
                <h3>{{ step_name.replace('_', ' ').title() }}</h3>
                <div class="status-{{ 'success' if step_result.success else 'failed' }}">
                    {{ "✅ Success" if step_result.success else "❌ Failed" }}
                </div>
            </div>
            <div class="step-content">
                <div><strong>Duration:</strong> {{ "%.2f"|format(step_result.duration) }} seconds</div>

                {% if not step_result.success and step_result.error %}
                <div class="error"><strong>Error:</strong> {{ step_result.error }}</div>
                {% endif %}

                {% if step_result.success and include_metrics %}
                {% if step_name == 'build_verification' %}
                <div class="metrics">
                    <div class="metric">
                        <div class="metric-label">Warnings</div>
                        <div class="metric-value">{{ step_result.warnings | length }}</div>
                    </div>
                    <div class="metric">
                        <div class="metric-label">Errors</div>
                        <div class="metric-value">{{ step_result.errors | length }}</div>
                    </div>
                </div>
                {% endif %}

                {% if step_name == 'test_execution' %}
                <div class="metrics">
                    <div class="metric">
                        <div class="metric-label">Total Tests</div>
                        <div class="metric-value">{{ step_result.total_tests }}</div>
                    </div>
                    <div class="metric">
                        <div class="metric-label">Passed</div>
                        <div class="metric-value">{{ step_result.passed_tests }}</div>
                    </div>
                    <div class="metric">
                        <div class="metric-label">Coverage</div>
                        <div class="metric-value">{{ "%.1f"|format(step_result.coverage) }}%</div>
                    </div>
                </div>
                {% endif %}

                {% if step_name == 'static_analysis' %}
                <div class="metrics">
                    <div class="metric">
                        <div class="metric-label">Issues Found</div>
                        <div class="metric-value">{{ step_result.issues_found }}</div>
                    </div>
                    <div class="metric">
                        <div class="metric-label">Complexity Violations</div>
                        <div class="metric-value">{{ step_result.complexity_violations }}</div>
                    </div>
                </div>
                {% endif %}
                {% endif %}
            </div>
        </div>
        {% endfor %}

        <div class="recommendations">
            <h3>Recommendations</h3>
            <ul>
                {% if summary.build_status == 'failed' %}
                <li>🔴 Fix build errors before proceeding with further development</li>
                <li>Review compilation warnings for potential issues</li>
                {% endif %}
                {% if summary.tests_total and summary.tests_failed > 0 %}
                <li>🧪 Address {{ summary.tests_failed }} failing tests</li>
                <li>Improve test coverage (current: {{ "%.1f"|format(summary.coverage_percentage) }}%)</li>
                {% endif %}
                {% if summary.issues_found > 0 %}
                <li>🔍 Fix {{ summary.issues_found }} identified code issues</li>
                <li>Focus on critical issues first</li>
                {% endif %}
            </ul>
        </div>

        <div class="footer">
            <p>Generated by AI Development System v1.0</p>
        </div>
    </div>
</body>
</html>'''

    def save_reports(self, results: Dict[str, Any], output_dir: Optional[Path] = None) -> Dict[str, str]:
        """
        Save reports in multiple formats

        Args:
            results: Analysis results
            output_dir: Output directory (uses config default if None)

        Returns:
            Dictionary mapping format to file paths
        """
        if output_dir is None:
            output_dir = self.output_dir

        ensure_directory(output_dir)
        report_files = {}

        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')

        for format_type in self.formats:
            try:
                report_content = self.generate_report(results, format_type)

                if format_type == 'json':
                    filename = f"ai_dev_report_{timestamp}.json"
                else:
                    filename = f"ai_dev_report.{format_type}"

                file_path = output_dir / filename
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(report_content)

                report_files[format_type] = str(file_path)
                self.logger.info(f"Saved {format_type} report to {file_path}")

            except Exception as e:
                self.logger.error(f"Failed to save {format_type} report: {str(e)}")

        return report_files

    def generate_dashboard_data(self, results: Dict[str, Any]) -> Dict[str, Any]:
        """
        Generate data for dashboard visualization

        Args:
            results: Analysis results

        Returns:
            Dashboard data dictionary
        """
        summary = self._calculate_summary(results.get('steps', {}))

        dashboard_data = {
            'overview': {
                'total_steps': summary['total_steps'],
                'successful_steps': summary['successful_steps'],
                'success_rate': summary['success_rate'],
                'total_duration': summary['total_duration']
            },
            'metrics': {
                'build_status': summary.get('build_status', 'unknown'),
                'tests': {
                    'total': summary.get('tests_total', 0),
                    'passed': summary.get('tests_passed', 0),
                    'failed': summary.get('tests_failed', 0)
                },
                'coverage': summary.get('coverage_percentage', 0),
                'issues': summary.get('issues_found', 0)
            },
            'timeline': self._generate_timeline_data(results.get('steps', {})),
            'trends': self._generate_trends_data(results.get('steps', {}))
        }

        return dashboard_data

    def _generate_timeline_data(self, steps: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Generate timeline data for visualization"""
        timeline = []
        current_time = 0

        for step_name, step_result in steps.items():
            timeline.append({
                'step': step_name,
                'status': 'success' if step_result.get('success', False) else 'failed',
                'duration': step_result.get('duration', 0),
                'start_time': current_time,
                'end_time': current_time + step_result.get('duration', 0)
            })
            current_time += step_result.get('duration', 0)

        return timeline

    def _generate_trends_data(self, steps: Dict[str, Any]) -> Dict[str, List[float]]:
        """Generate trends data for visualization"""
        trends = {
            'success_rates': [],
            'durations': [],
            'coverage': [],
            'issues': []
        }

        # This would require historical data - simplified implementation
        success_count = 0
        total_count = 0
        for step_result in steps.values():
            total_count += 1
            if step_result.get('success', False):
                success_count += 1
            trends['success_rates'].append((success_count / total_count) * 100 if total_count > 0 else 0)
            trends['durations'].append(step_result.get('duration', 0))

        return trends