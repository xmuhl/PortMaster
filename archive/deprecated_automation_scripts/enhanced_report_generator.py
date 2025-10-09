#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
增强的报告生成系统
支持HTML、Markdown、JSON多种格式
提供丰富的可视化和趋势分析
"""

import json
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional
from collections import defaultdict

class EnhancedReportGenerator:
    """增强的报告生成器"""

    def __init__(self, reports_dir: str = "automation_reports"):
        self.reports_dir = Path(reports_dir)
        self.reports_dir.mkdir(exist_ok=True)

    def generate_comprehensive_report(self, workflow_result: Dict, test_results: Dict = None,
                                     coverage_results: Dict = None, static_analysis: Dict = None) -> str:
        """
        生成综合HTML报告

        Args:
            workflow_result: 工作流执行结果
            test_results: 测试执行结果
            coverage_results: 代码覆盖率结果
            static_analysis: 静态分析结果

        Returns:
            报告文件路径
        """
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        report_file = self.reports_dir / f"comprehensive_report_{timestamp}.html"

        html_content = self._generate_html_report(
            workflow_result, test_results, coverage_results, static_analysis
        )

        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(html_content)

        return str(report_file)

    def _generate_html_report(self, workflow_result: Dict, test_results: Dict = None,
                             coverage_results: Dict = None, static_analysis: Dict = None) -> str:
        """生成HTML格式报告"""

        html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PortMaster 自动化测试报告</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif;
            line-height: 1.6;
            color: #333;
            background: #f5f5f5;
            padding: 20px;
        }}

        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            overflow: hidden;
        }}

        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
        }}

        .header h1 {{
            font-size: 28px;
            margin-bottom: 10px;
        }}

        .header .meta {{
            opacity: 0.9;
            font-size: 14px;
        }}

        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            padding: 30px;
            background: #f8f9fa;
        }}

        .stat-card {{
            background: white;
            padding: 20px;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }}

        .stat-card .label {{
            font-size: 14px;
            color: #666;
            margin-bottom: 8px;
        }}

        .stat-card .value {{
            font-size: 32px;
            font-weight: bold;
            color: #333;
        }}

        .stat-card.success {{
            border-left-color: #28a745;
        }}

        .stat-card.warning {{
            border-left-color: #ffc107;
        }}

        .stat-card.danger {{
            border-left-color: #dc3545;
        }}

        .section {{
            padding: 30px;
            border-bottom: 1px solid #e9ecef;
        }}

        .section h2 {{
            font-size: 20px;
            margin-bottom: 20px;
            color: #667eea;
        }}

        .step-list {{
            list-style: none;
        }}

        .step-item {{
            background: #f8f9fa;
            padding: 15px;
            margin-bottom: 10px;
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }}

        .step-item.success {{
            background: #d4edda;
            border-left: 4px solid #28a745;
        }}

        .step-item.failure {{
            background: #f8d7da;
            border-left: 4px solid #dc3545;
        }}

        .step-item.skipped {{
            background: #fff3cd;
            border-left: 4px solid #ffc107;
        }}

        .step-name {{
            font-weight: 500;
        }}

        .step-status {{
            display: inline-block;
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 12px;
            font-weight: 600;
        }}

        .step-status.success {{
            background: #28a745;
            color: white;
        }}

        .step-status.failure {{
            background: #dc3545;
            color: white;
        }}

        .step-status.skipped {{
            background: #ffc107;
            color: #333;
        }}

        .test-table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 15px;
        }}

        .test-table th {{
            background: #667eea;
            color: white;
            padding: 12px;
            text-align: left;
            font-weight: 500;
        }}

        .test-table td {{
            padding: 12px;
            border-bottom: 1px solid #e9ecef;
        }}

        .test-table tr:hover {{
            background: #f8f9fa;
        }}

        .badge {{
            display: inline-block;
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: 600;
        }}

        .badge.success {{
            background: #28a745;
            color: white;
        }}

        .badge.failure {{
            background: #dc3545;
            color: white;
        }}

        .progress-bar {{
            width: 100%;
            height: 24px;
            background: #e9ecef;
            border-radius: 12px;
            overflow: hidden;
            margin-top: 10px;
        }}

        .progress-fill {{
            height: 100%;
            background: linear-gradient(90deg, #28a745 0%, #20c997 100%);
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-size: 12px;
            font-weight: 600;
        }}

        .chart-container {{
            margin-top: 20px;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 8px;
        }}

        .footer {{
            padding: 20px 30px;
            background: #f8f9fa;
            text-align: center;
            color: #666;
            font-size: 14px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🤖 PortMaster 自动化测试报告</h1>
            <div class="meta">
                <p>工作流: {workflow_result.get('workflow_name', 'Unknown')}</p>
                <p>生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
            </div>
        </div>

        <div class="summary">
            <div class="stat-card {self._get_status_class(workflow_result.get('overall_success', False))}">
                <div class="label">总体状态</div>
                <div class="value">{'✅ 成功' if workflow_result.get('overall_success', False) else '❌ 失败'}</div>
            </div>
            <div class="stat-card">
                <div class="label">总步骤数</div>
                <div class="value">{workflow_result.get('total_steps', 0)}</div>
            </div>
            <div class="stat-card success">
                <div class="label">成功步骤</div>
                <div class="value">{workflow_result.get('successful_steps', 0)}</div>
            </div>
            <div class="stat-card danger">
                <div class="label">失败步骤</div>
                <div class="value">{workflow_result.get('failed_steps', 0)}</div>
            </div>
            <div class="stat-card">
                <div class="label">总耗时</div>
                <div class="value">{workflow_result.get('total_duration', 0):.1f}s</div>
            </div>
        </div>

        <div class="section">
            <h2>📊 执行步骤</h2>
            <ul class="step-list">
"""

        # 添加步骤详情
        for step in workflow_result.get('step_results', []):
            status_class = 'success' if step['success'] else 'failure'
            status_text = '✅ 成功' if step['success'] else '❌ 失败'

            html += f"""
                <li class="step-item {status_class}">
                    <div>
                        <div class="step-name">{step['step_name']}</div>
                        <div style="font-size: 12px; color: #666; margin-top: 4px;">
                            耗时: {step['duration']:.2f}秒
                        </div>
                    </div>
                    <span class="step-status {status_class}">{status_text}</span>
                </li>
"""

        html += """
            </ul>
        </div>
"""

        # 添加测试结果部分（如果有）
        if test_results:
            html += self._generate_test_results_section(test_results)

        # 添加代码覆盖率部分（如果有）
        if coverage_results:
            html += self._generate_coverage_section(coverage_results)

        # 添加静态分析部分（如果有）
        if static_analysis:
            html += self._generate_static_analysis_section(static_analysis)

        html += """
        <div class="footer">
            <p>PortMaster 全自动化AI驱动开发系统</p>
            <p>报告生成时间: """ + datetime.now().strftime('%Y-%m-%d %H:%M:%S') + """</p>
        </div>
    </div>
</body>
</html>
"""

        return html

    def _generate_test_results_section(self, test_results: Dict) -> str:
        """生成测试结果部分"""
        total = test_results.get('total_tests', 0)
        passed = test_results.get('passed_tests', 0)
        failed = test_results.get('failed_tests', 0)

        pass_rate = (passed / total * 100) if total > 0 else 0

        html = f"""
        <div class="section">
            <h2>🧪 测试结果</h2>
            <div class="progress-bar">
                <div class="progress-fill" style="width: {pass_rate}%">
                    {pass_rate:.1f}% 通过率
                </div>
            </div>

            <table class="test-table">
                <thead>
                    <tr>
                        <th>测试套件</th>
                        <th>测试数</th>
                        <th>通过</th>
                        <th>失败</th>
                        <th>状态</th>
                    </tr>
                </thead>
                <tbody>
"""

        for suite in test_results.get('suites', []):
            status_badge = 'success' if suite['passed'] == suite['total'] else 'failure'
            html += f"""
                    <tr>
                        <td>{suite['name']}</td>
                        <td>{suite['total']}</td>
                        <td>{suite['passed']}</td>
                        <td>{suite['failed']}</td>
                        <td><span class="badge {status_badge}">
                            {'✅ 通过' if suite['passed'] == suite['total'] else '❌ 失败'}
                        </span></td>
                    </tr>
"""

        html += """
                </tbody>
            </table>
        </div>
"""
        return html

    def _generate_coverage_section(self, coverage_results: Dict) -> str:
        """生成代码覆盖率部分"""
        line_cov = coverage_results.get('line_coverage', 0)
        branch_cov = coverage_results.get('branch_coverage', 0)

        html = f"""
        <div class="section">
            <h2>📈 代码覆盖率</h2>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-top: 15px;">
                <div>
                    <div style="font-size: 14px; color: #666; margin-bottom: 10px;">行覆盖率</div>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: {line_cov}%; background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);">
                            {line_cov:.1f}%
                        </div>
                    </div>
                </div>
                <div>
                    <div style="font-size: 14px; color: #666; margin-bottom: 10px;">分支覆盖率</div>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: {branch_cov}%; background: linear-gradient(90deg, #f093fb 0%, #f5576c 100%);">
                            {branch_cov:.1f}%
                        </div>
                    </div>
                </div>
            </div>
        </div>
"""
        return html

    def _generate_static_analysis_section(self, static_analysis: Dict) -> str:
        """生成静态分析部分"""
        total_issues = static_analysis.get('total_issues', 0)
        errors = static_analysis.get('errors', 0)
        warnings = static_analysis.get('warnings', 0)

        html = f"""
        <div class="section">
            <h2>🔍 静态分析</h2>
            <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 20px; margin-top: 15px;">
                <div class="stat-card">
                    <div class="label">总问题数</div>
                    <div class="value">{total_issues}</div>
                </div>
                <div class="stat-card danger">
                    <div class="label">错误</div>
                    <div class="value">{errors}</div>
                </div>
                <div class="stat-card warning">
                    <div class="label">警告</div>
                    <div class="value">{warnings}</div>
                </div>
            </div>
        </div>
"""
        return html

    def _get_status_class(self, success: bool) -> str:
        """获取状态CSS类名"""
        return 'success' if success else 'danger'

    def generate_trend_report(self, history_file: str = "test_history.json") -> str:
        """
        生成趋势分析报告

        Args:
            history_file: 历史数据文件路径

        Returns:
            报告文件路径
        """
        history_path = Path(history_file)
        if not history_path.exists():
            return None

        with open(history_path, 'r', encoding='utf-8') as f:
            history = json.load(f)

        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        report_file = self.reports_dir / f"trend_report_{timestamp}.html"

        # 生成趋势图HTML（这里简化，实际可以使用Chart.js等库）
        html = self._generate_trend_html(history)

        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(html)

        return str(report_file)

    def _generate_trend_html(self, history: List[Dict]) -> str:
        """生成趋势分析HTML"""
        # 简化版本，实际可以集成Chart.js或其他图表库
        html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>测试趋势报告</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            padding: 20px;
            background: #f5f5f5;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 30px;
            border-radius: 8px;
        }}
        .chart-container {{
            margin: 30px 0;
            height: 400px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 测试趋势报告</h1>

        <div class="chart-container">
            <canvas id="passRateChart"></canvas>
        </div>

        <div class="chart-container">
            <canvas id="coverageChart"></canvas>
        </div>

        <script>
            // 通过率趋势图
            const passRateCtx = document.getElementById('passRateChart').getContext('2d');
            new Chart(passRateCtx, {{
                type: 'line',
                data: {{
                    labels: {json.dumps([h.get('date', '') for h in history])},
                    datasets: [{{
                        label: '通过率 (%)',
                        data: {json.dumps([h.get('pass_rate', 0) for h in history])},
                        borderColor: '#28a745',
                        tension: 0.1
                    }}]
                }},
                options: {{
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {{
                        title: {{
                            display: true,
                            text: '测试通过率趋势'
                        }}
                    }},
                    scales: {{
                        y: {{
                            beginAtZero: true,
                            max: 100
                        }}
                    }}
                }}
            }});

            // 代码覆盖率趋势图
            const coverageCtx = document.getElementById('coverageChart').getContext('2d');
            new Chart(coverageCtx, {{
                type: 'line',
                data: {{
                    labels: {json.dumps([h.get('date', '') for h in history])},
                    datasets: [{{
                        label: '行覆盖率 (%)',
                        data: {json.dumps([h.get('line_coverage', 0) for h in history])},
                        borderColor: '#667eea',
                        tension: 0.1
                    }}, {{
                        label: '分支覆盖率 (%)',
                        data: {json.dumps([h.get('branch_coverage', 0) for h in history])},
                        borderColor: '#f093fb',
                        tension: 0.1
                    }}]
                }},
                options: {{
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {{
                        title: {{
                            display: true,
                            text: '代码覆盖率趋势'
                        }}
                    }},
                    scales: {{
                        y: {{
                            beginAtZero: true,
                            max: 100
                        }}
                    }}
                }}
            }});
        </script>
    </div>
</body>
</html>
"""
        return html

    def save_test_history(self, test_result: Dict, history_file: str = "test_history.json"):
        """保存测试历史数据"""
        history_path = Path(history_file)

        history = []
        if history_path.exists():
            with open(history_path, 'r', encoding='utf-8') as f:
                history = json.load(f)

        # 添加当前测试结果
        entry = {
            "date": datetime.now().strftime('%Y-%m-%d'),
            "pass_rate": test_result.get('pass_rate', 0),
            "line_coverage": test_result.get('line_coverage', 0),
            "branch_coverage": test_result.get('branch_coverage', 0),
            "total_tests": test_result.get('total_tests', 0),
            "execution_time": test_result.get('execution_time', 0)
        }

        history.append(entry)

        # 保留最近30天的数据
        if len(history) > 30:
            history = history[-30:]

        with open(history_path, 'w', encoding='utf-8') as f:
            json.dump(history, f, ensure_ascii=False, indent=2)

def main():
    """示例使用"""
    generator = EnhancedReportGenerator()

    # 示例工作流结果
    workflow_result = {
        "workflow_name": "综合自动化测试流程",
        "overall_success": True,
        "total_steps": 8,
        "successful_steps": 7,
        "failed_steps": 1,
        "total_duration": 120.5,
        "step_results": [
            {"step_name": "静态分析", "success": True, "duration": 10.2},
            {"step_name": "清理构建", "success": True, "duration": 25.8},
            {"step_name": "单元测试", "success": True, "duration": 15.3},
            {"step_name": "集成测试", "success": False, "duration": 30.1},
            {"step_name": "性能测试", "success": True, "duration": 20.7},
            {"step_name": "压力测试", "success": True, "duration": 12.4},
            {"step_name": "回归测试", "success": True, "duration": 6.0}
        ]
    }

    # 生成综合报告
    report_path = generator.generate_comprehensive_report(workflow_result)
    print(f"报告已生成: {report_path}")

if __name__ == "__main__":
    main()
