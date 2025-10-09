#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
性能优化分析器
自动分析性能瓶颈并提供优化建议
"""

import json
import re
from pathlib import Path
from typing import Dict, List, Tuple
from collections import defaultdict
from datetime import datetime

class PerformanceOptimizer:
    """性能优化分析器"""

    def __init__(self):
        self.performance_db = Path("performance_data.json")
        self.optimization_history = Path("optimization_history.json")

    def analyze_performance(self, test_results: Dict) -> Dict:
        """
        分析性能测试结果

        Args:
            test_results: 性能测试结果

        Returns:
            性能分析报告
        """
        analysis = {
            "timestamp": datetime.now().isoformat(),
            "bottlenecks": [],
            "recommendations": [],
            "metrics": {},
            "severity": "low"
        }

        # 分析吞吐量
        throughput_analysis = self._analyze_throughput(test_results)
        if throughput_analysis:
            analysis["bottlenecks"].extend(throughput_analysis["bottlenecks"])
            analysis["recommendations"].extend(throughput_analysis["recommendations"])

        # 分析延迟
        latency_analysis = self._analyze_latency(test_results)
        if latency_analysis:
            analysis["bottlenecks"].extend(latency_analysis["bottlenecks"])
            analysis["recommendations"].extend(latency_analysis["recommendations"])

        # 分析内存使用
        memory_analysis = self._analyze_memory(test_results)
        if memory_analysis:
            analysis["bottlenecks"].extend(memory_analysis["bottlenecks"])
            analysis["recommendations"].extend(memory_analysis["recommendations"])

        # 确定严重程度
        analysis["severity"] = self._determine_severity(analysis["bottlenecks"])

        # 保存到历史数据库
        self._save_analysis(analysis)

        return analysis

    def _analyze_throughput(self, test_results: Dict) -> Dict:
        """分析吞吐量性能"""
        analysis = {
            "bottlenecks": [],
            "recommendations": []
        }

        # 查找吞吐量测试结果
        throughput_tests = [
            t for t in test_results.get('tests', [])
            if 'throughput' in t.get('name', '').lower()
        ]

        for test in throughput_tests:
            throughput = test.get('metrics', {}).get('throughput', 0)

            # 吞吐量低于1MB/s视为瓶颈
            if throughput < 1024 * 1024:
                analysis["bottlenecks"].append({
                    "type": "throughput",
                    "test": test['name'],
                    "value": throughput,
                    "threshold": 1024 * 1024,
                    "description": f"吞吐量仅{throughput / 1024:.1f} KB/s，低于1 MB/s目标"
                })

                # 提供优化建议
                analysis["recommendations"].extend([
                    {
                        "priority": "high",
                        "category": "buffer_optimization",
                        "title": "增大缓冲区大小",
                        "description": "当前吞吐量较低，建议增大发送和接收缓冲区",
                        "code_example": """
// 在ReliableChannel.cpp中增大缓冲区
m_sendBuffer.resize(64 * 1024);  // 增大到64KB
m_recvBuffer.resize(64 * 1024);
                        """,
                        "expected_improvement": "提升30-50%吞吐量"
                    },
                    {
                        "priority": "medium",
                        "category": "window_size",
                        "title": "调整滑动窗口大小",
                        "description": "增大滑动窗口以减少等待ACK的时间",
                        "code_example": """
// 在Protocol配置中调整窗口大小
config.window_size = 16;  // 增大到16（当前为1）
                        """,
                        "expected_improvement": "提升50-100%吞吐量"
                    }
                ])

        return analysis

    def _analyze_latency(self, test_results: Dict) -> Dict:
        """分析延迟性能"""
        analysis = {
            "bottlenecks": [],
            "recommendations": []
        }

        # 查找延迟测试结果
        latency_tests = [
            t for t in test_results.get('tests', [])
            if 'latency' in t.get('name', '').lower()
        ]

        for test in latency_tests:
            latency = test.get('metrics', {}).get('average_latency', 0)

            # 平均延迟超过100ms视为瓶颈
            if latency > 100:
                analysis["bottlenecks"].append({
                    "type": "latency",
                    "test": test['name'],
                    "value": latency,
                    "threshold": 100,
                    "description": f"平均延迟{latency:.1f}ms，超过100ms目标"
                })

                # 提供优化建议
                analysis["recommendations"].extend([
                    {
                        "priority": "high",
                        "category": "timeout_tuning",
                        "title": "优化超时参数",
                        "description": "减小重传超时时间以加快错误恢复",
                        "code_example": """
// 在ReliableChannel中调整超时参数
config.ack_timeout = 50;      // 减小到50ms（当前可能是200ms）
config.retry_interval = 100;  // 调整重试间隔
                        """,
                        "expected_improvement": "减少20-30%延迟"
                    },
                    {
                        "priority": "medium",
                        "category": "processing_optimization",
                        "title": "优化帧处理逻辑",
                        "description": "减少不必要的数据拷贝和处理",
                        "code_example": """
// 使用移动语义避免数据拷贝
void OnFrameReceived(Frame&& frame) {
    m_receiveQueue.push(std::move(frame));  // 移动而非拷贝
}
                        """,
                        "expected_improvement": "减少10-15%延迟"
                    }
                ])

        return analysis

    def _analyze_memory(self, test_results: Dict) -> Dict:
        """分析内存使用"""
        analysis = {
            "bottlenecks": [],
            "recommendations": []
        }

        # 查找内存相关测试结果
        memory_tests = [
            t for t in test_results.get('tests', [])
            if 'memory' in t.get('name', '').lower() or 'stress' in t.get('name', '').lower()
        ]

        for test in memory_tests:
            peak_memory = test.get('metrics', {}).get('peak_memory_mb', 0)

            # 峰值内存超过100MB视为潜在问题
            if peak_memory > 100:
                analysis["bottlenecks"].append({
                    "type": "memory",
                    "test": test['name'],
                    "value": peak_memory,
                    "threshold": 100,
                    "description": f"峰值内存使用{peak_memory:.1f}MB，超过100MB建议值"
                })

                # 提供优化建议
                analysis["recommendations"].extend([
                    {
                        "priority": "medium",
                        "category": "memory_pool",
                        "title": "使用内存池管理",
                        "description": "频繁分配释放的对象使用内存池",
                        "code_example": """
// 创建Frame对象池
class FramePool {
    std::vector<std::unique_ptr<Frame>> m_pool;
    std::mutex m_mutex;

public:
    std::unique_ptr<Frame> acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_pool.empty()) {
            auto frame = std::move(m_pool.back());
            m_pool.pop_back();
            return frame;
        }
        return std::make_unique<Frame>();
    }

    void release(std::unique_ptr<Frame> frame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        frame->reset();
        m_pool.push_back(std::move(frame));
    }
};
                        """,
                        "expected_improvement": "减少30-40%内存分配开销"
                    },
                    {
                        "priority": "low",
                        "category": "buffer_reuse",
                        "title": "复用缓冲区",
                        "description": "避免每次发送都分配新缓冲区",
                        "code_example": """
// 在Transport层复用缓冲区
class Transport {
    std::vector<uint8_t> m_sendBuffer;  // 成员变量，复用

    void Send(const void* data, size_t size) {
        m_sendBuffer.resize(size);  // 调整大小而非重新分配
        memcpy(m_sendBuffer.data(), data, size);
        // ... 发送逻辑
    }
};
                        """,
                        "expected_improvement": "减少20-30%内存分配次数"
                    }
                ])

        return analysis

    def _determine_severity(self, bottlenecks: List[Dict]) -> str:
        """确定性能问题严重程度"""
        if not bottlenecks:
            return "none"

        # 根据瓶颈数量和类型判断
        high_priority_count = sum(1 for b in bottlenecks if b['type'] in ['throughput', 'latency'])

        if high_priority_count >= 3:
            return "critical"
        elif high_priority_count >= 2:
            return "high"
        elif high_priority_count >= 1:
            return "medium"
        else:
            return "low"

    def _save_analysis(self, analysis: Dict):
        """保存分析结果到历史数据库"""
        history = []
        if self.optimization_history.exists():
            with open(self.optimization_history, 'r', encoding='utf-8') as f:
                history = json.load(f)

        history.append(analysis)

        # 保留最近30次分析
        if len(history) > 30:
            history = history[-30:]

        with open(self.optimization_history, 'w', encoding='utf-8') as f:
            json.dump(history, f, ensure_ascii=False, indent=2)

    def generate_optimization_report(self, analysis: Dict) -> str:
        """
        生成优化建议报告

        Args:
            analysis: 性能分析结果

        Returns:
            Markdown格式报告
        """
        report = f"""# 性能优化建议报告

**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
**严重程度**: {self._severity_emoji(analysis['severity'])} {analysis['severity'].upper()}

---

## 📊 性能瓶颈

"""

        if not analysis["bottlenecks"]:
            report += "✅ 未发现明显性能瓶颈\n\n"
        else:
            for idx, bottleneck in enumerate(analysis["bottlenecks"], 1):
                report += f"""### {idx}. {bottleneck['type'].upper()} - {bottleneck['test']}

- **当前值**: {self._format_metric(bottleneck['value'], bottleneck['type'])}
- **目标值**: {self._format_metric(bottleneck['threshold'], bottleneck['type'])}
- **描述**: {bottleneck['description']}

"""

        report += """---

## 💡 优化建议

"""

        if not analysis["recommendations"]:
            report += "暂无优化建议\n\n"
        else:
            # 按优先级分组
            by_priority = defaultdict(list)
            for rec in analysis["recommendations"]:
                by_priority[rec['priority']].append(rec)

            for priority in ['high', 'medium', 'low']:
                recs = by_priority.get(priority, [])
                if not recs:
                    continue

                priority_emoji = {'high': '🔴', 'medium': '🟡', 'low': '🟢'}
                report += f"### {priority_emoji[priority]} {priority.upper()}优先级\n\n"

                for idx, rec in enumerate(recs, 1):
                    report += f"""#### {idx}. {rec['title']}

**类别**: {rec['category']}

**描述**: {rec['description']}

**代码示例**:
```cpp
{rec['code_example'].strip()}
```

**预期效果**: {rec['expected_improvement']}

---

"""

        report += f"""## 📈 历史趋势

查看完整历史数据: `{self.optimization_history}`

"""

        return report

    def _severity_emoji(self, severity: str) -> str:
        """获取严重程度emoji"""
        return {
            'none': '✅',
            'low': '🟢',
            'medium': '🟡',
            'high': '🟠',
            'critical': '🔴'
        }.get(severity, '❓')

    def _format_metric(self, value: float, metric_type: str) -> str:
        """格式化性能指标"""
        if metric_type == 'throughput':
            if value >= 1024 * 1024:
                return f"{value / (1024 * 1024):.2f} MB/s"
            elif value >= 1024:
                return f"{value / 1024:.2f} KB/s"
            else:
                return f"{value:.2f} B/s"
        elif metric_type == 'latency':
            return f"{value:.2f} ms"
        elif metric_type == 'memory':
            return f"{value:.2f} MB"
        else:
            return f"{value:.2f}"

    def compare_with_baseline(self, current_results: Dict, baseline_version: str) -> Dict:
        """
        与基线版本对比性能

        Args:
            current_results: 当前测试结果
            baseline_version: 基线版本号

        Returns:
            对比结果
        """
        baseline_path = Path(f"AutoTest/baselines/baseline_{baseline_version}.json")
        if not baseline_path.exists():
            return {"error": f"基线版本 {baseline_version} 不存在"}

        with open(baseline_path, 'r', encoding='utf-8') as f:
            baseline = json.load(f)

        comparison = {
            "baseline_version": baseline_version,
            "improvements": [],
            "regressions": [],
            "unchanged": []
        }

        # 对比性能指标
        for current_test in current_results.get('tests', []):
            test_name = current_test['name']
            baseline_test = next((t for t in baseline.get('tests', []) if t['name'] == test_name), None)

            if not baseline_test:
                continue

            current_metrics = current_test.get('metrics', {})
            baseline_metrics = baseline_test.get('metrics', {})

            # 对比吞吐量
            if 'throughput' in current_metrics and 'throughput' in baseline_metrics:
                current_val = current_metrics['throughput']
                baseline_val = baseline_metrics['throughput']
                change = (current_val - baseline_val) / baseline_val * 100

                if abs(change) < 5:  # 变化小于5%视为无变化
                    comparison["unchanged"].append({
                        "test": test_name,
                        "metric": "throughput",
                        "change": change
                    })
                elif change > 0:
                    comparison["improvements"].append({
                        "test": test_name,
                        "metric": "throughput",
                        "current": current_val,
                        "baseline": baseline_val,
                        "improvement": change
                    })
                else:
                    comparison["regressions"].append({
                        "test": test_name,
                        "metric": "throughput",
                        "current": current_val,
                        "baseline": baseline_val,
                        "regression": abs(change)
                    })

        return comparison

def main():
    """示例使用"""
    optimizer = PerformanceOptimizer()

    # 示例测试结果
    test_results = {
        "tests": [
            {
                "name": "ThroughputTest",
                "metrics": {
                    "throughput": 500 * 1024  # 500 KB/s (低于1MB/s目标)
                }
            },
            {
                "name": "LatencyTest",
                "metrics": {
                    "average_latency": 150  # 150ms (超过100ms目标)
                }
            },
            {
                "name": "StressTest",
                "metrics": {
                    "peak_memory_mb": 120  # 120MB (超过100MB建议值)
                }
            }
        ]
    }

    # 分析性能
    analysis = optimizer.analyze_performance(test_results)

    # 生成报告
    report = optimizer.generate_optimization_report(analysis)
    print(report)

    # 保存报告
    report_file = Path("performance_optimization_report.md")
    report_file.write_text(report, encoding='utf-8')
    print(f"\n报告已保存到: {report_file}")

if __name__ == "__main__":
    main()
