#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
错误模式学习系统
自动学习错误模式并优化修复策略
"""

import json
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from collections import defaultdict, Counter
from datetime import datetime

class ErrorPatternLearner:
    """错误模式学习器"""

    def __init__(self):
        self.pattern_db = Path("error_patterns_learned.json")
        self.fix_history = Path("fix_history.json")
        self.patterns = self._load_patterns()
        self.fix_success_rate = defaultdict(list)

    def _load_patterns(self) -> Dict:
        """加载已学习的错误模式"""
        if self.pattern_db.exists():
            with open(self.pattern_db, 'r', encoding='utf-8') as f:
                return json.load(f)
        return {}

    def learn_from_error(self, error_info: Dict, fix_applied: Dict, fix_success: bool):
        """
        从错误和修复结果中学习

        Args:
            error_info: 错误信息
            fix_applied: 应用的修复策略
            fix_success: 修复是否成功
        """
        error_code = error_info.get('code', '')
        error_message = error_info.get('message', '')

        # 提取错误模式
        pattern = self._extract_pattern(error_code, error_message)

        if pattern not in self.patterns:
            self.patterns[pattern] = {
                "error_code": error_code,
                "pattern_regex": self._generate_regex(error_message),
                "examples": [],
                "fix_strategies": [],
                "success_rate": {}
            }

        # 添加示例
        if len(self.patterns[pattern]["examples"]) < 10:
            self.patterns[pattern]["examples"].append({
                "message": error_message,
                "file": error_info.get('file', ''),
                "line": error_info.get('line', 0),
                "timestamp": datetime.now().isoformat()
            })

        # 记录修复策略效果
        fix_strategy = fix_applied.get('strategy', 'unknown')
        if fix_strategy not in self.patterns[pattern]["fix_strategies"]:
            self.patterns[pattern]["fix_strategies"].append({
                "strategy": fix_strategy,
                "description": fix_applied.get('description', ''),
                "actions": fix_applied.get('actions', []),
                "success_count": 0,
                "failure_count": 0,
                "confidence": 0.0
            })

        # 更新成功率
        for strategy in self.patterns[pattern]["fix_strategies"]:
            if strategy["strategy"] == fix_strategy:
                if fix_success:
                    strategy["success_count"] += 1
                else:
                    strategy["failure_count"] += 1

                total = strategy["success_count"] + strategy["failure_count"]
                strategy["confidence"] = strategy["success_count"] / total if total > 0 else 0.0

        # 保存更新的模式
        self._save_patterns()

        # 记录到修复历史
        self._record_fix_history(error_info, fix_applied, fix_success)

    def _extract_pattern(self, error_code: str, error_message: str) -> str:
        """提取错误模式标识"""
        # 基于错误代码和消息关键词创建模式标识
        if error_code:
            return f"CODE_{error_code}"

        # 提取关键词
        keywords = re.findall(r'\b[A-Z]{2,}\b|\b\w+\(\)', error_message)
        if keywords:
            return f"PATTERN_{'_'.join(keywords[:3])}"

        return "PATTERN_UNKNOWN"

    def _generate_regex(self, error_message: str) -> str:
        """生成错误消息的正则表达式模式"""
        # 将具体值替换为通配符
        pattern = error_message

        # 替换文件路径
        pattern = re.sub(r'[A-Za-z]:\\[^\s]+', r'<FILE_PATH>', pattern)
        pattern = re.sub(r'/[^\s]+', r'<FILE_PATH>', pattern)

        # 替换数字
        pattern = re.sub(r'\d+', r'\\d+', pattern)

        # 替换标识符（变量名、函数名等）
        pattern = re.sub(r'\b[a-zA-Z_][a-zA-Z0-9_]*\b', r'[a-zA-Z_][a-zA-Z0-9_]*', pattern)

        return pattern

    def _save_patterns(self):
        """保存学习的模式"""
        with open(self.pattern_db, 'w', encoding='utf-8') as f:
            json.dump(self.patterns, f, ensure_ascii=False, indent=2)

    def _record_fix_history(self, error_info: Dict, fix_applied: Dict, fix_success: bool):
        """记录修复历史"""
        history = []
        if self.fix_history.exists():
            with open(self.fix_history, 'r', encoding='utf-8') as f:
                history = json.load(f)

        history.append({
            "timestamp": datetime.now().isoformat(),
            "error": error_info,
            "fix": fix_applied,
            "success": fix_success
        })

        # 保留最近100条记录
        if len(history) > 100:
            history = history[-100:]

        with open(self.fix_history, 'w', encoding='utf-8') as f:
            json.dump(history, f, ensure_ascii=False, indent=2)

    def get_best_fix_strategy(self, error_info: Dict) -> Optional[Dict]:
        """
        根据错误信息获取最佳修复策略

        Args:
            error_info: 错误信息

        Returns:
            推荐的修复策略
        """
        error_code = error_info.get('code', '')
        error_message = error_info.get('message', '')

        pattern = self._extract_pattern(error_code, error_message)

        if pattern not in self.patterns:
            return None

        # 获取成功率最高的策略
        strategies = self.patterns[pattern]["fix_strategies"]
        if not strategies:
            return None

        best_strategy = max(strategies, key=lambda s: s["confidence"])

        # 仅返回置信度>=0.5的策略
        if best_strategy["confidence"] >= 0.5:
            return {
                "strategy": best_strategy["strategy"],
                "description": best_strategy["description"],
                "actions": best_strategy["actions"],
                "confidence": best_strategy["confidence"],
                "success_count": best_strategy["success_count"],
                "failure_count": best_strategy["failure_count"]
            }

        return None

    def analyze_error_trends(self) -> Dict:
        """
        分析错误趋势

        Returns:
            错误趋势分析报告
        """
        if not self.fix_history.exists():
            return {"error": "无修复历史数据"}

        with open(self.fix_history, 'r', encoding='utf-8') as f:
            history = json.load(f)

        analysis = {
            "total_errors": len(history),
            "error_types": Counter(),
            "fix_success_rate": {},
            "most_common_errors": [],
            "problematic_files": Counter(),
            "time_analysis": {
                "errors_by_hour": defaultdict(int),
                "errors_by_day": defaultdict(int)
            }
        }

        for record in history:
            error = record["error"]
            error_code = error.get("code", "UNKNOWN")

            # 统计错误类型
            analysis["error_types"][error_code] += 1

            # 统计问题文件
            error_file = error.get("file", "")
            if error_file:
                analysis["problematic_files"][error_file] += 1

            # 时间分析
            if "timestamp" in record:
                dt = datetime.fromisoformat(record["timestamp"])
                analysis["time_analysis"]["errors_by_hour"][dt.hour] += 1
                analysis["time_analysis"]["errors_by_day"][dt.strftime("%Y-%m-%d")] += 1

            # 修复成功率统计
            fix_strategy = record["fix"].get("strategy", "unknown")
            if fix_strategy not in analysis["fix_success_rate"]:
                analysis["fix_success_rate"][fix_strategy] = {"success": 0, "failure": 0}

            if record["success"]:
                analysis["fix_success_rate"][fix_strategy]["success"] += 1
            else:
                analysis["fix_success_rate"][fix_strategy]["failure"] += 1

        # 最常见错误
        analysis["most_common_errors"] = [
            {"error_code": code, "count": count}
            for code, count in analysis["error_types"].most_common(10)
        ]

        # 最多问题文件
        analysis["most_problematic_files"] = [
            {"file": file, "error_count": count}
            for file, count in analysis["problematic_files"].most_common(10)
        ]

        # 计算各策略成功率
        for strategy, stats in analysis["fix_success_rate"].items():
            total = stats["success"] + stats["failure"]
            stats["rate"] = stats["success"] / total if total > 0 else 0.0

        return analysis

    def generate_learning_report(self) -> str:
        """
        生成学习报告

        Returns:
            Markdown格式报告
        """
        analysis = self.analyze_error_trends()

        if "error" in analysis:
            return f"# 错误模式学习报告\n\n{analysis['error']}\n"

        report = f"""# 错误模式学习报告

**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
**总错误数**: {analysis['total_errors']}

---

## 📊 错误类型分布

"""

        for error in analysis["most_common_errors"]:
            percentage = error["count"] / analysis["total_errors"] * 100
            report += f"- **{error['error_code']}**: {error['count']} 次 ({percentage:.1f}%)\n"

        report += """
---

## 🔧 修复策略效果

"""

        for strategy, stats in sorted(analysis["fix_success_rate"].items(), key=lambda x: x[1]["rate"], reverse=True):
            success_rate = stats["rate"] * 100
            emoji = "✅" if success_rate >= 80 else "⚠️" if success_rate >= 50 else "❌"

            report += f"""### {emoji} {strategy}

- 成功次数: {stats["success"]}
- 失败次数: {stats["failure"]}
- 成功率: {success_rate:.1f}%

"""

        report += """---

## 📁 问题文件统计

"""

        for file_info in analysis.get("most_problematic_files", [])[:5]:
            report += f"- **{file_info['file']}**: {file_info['error_count']} 个错误\n"

        report += """
---

## 📈 时间分布

### 按小时分布

"""

        # 生成简单的时间分布图
        hours_data = analysis["time_analysis"]["errors_by_hour"]
        max_count = max(hours_data.values()) if hours_data else 1

        for hour in range(24):
            count = hours_data.get(hour, 0)
            bar_length = int(count / max_count * 40) if max_count > 0 else 0
            bar = "█" * bar_length
            report += f"{hour:02d}:00 | {bar} {count}\n"

        report += """
---

## 🎓 学习的错误模式

"""

        for pattern, data in list(self.patterns.items())[:10]:
            report += f"""### {pattern}

- **错误代码**: {data.get('error_code', 'N/A')}
- **示例数**: {len(data.get('examples', []))}
- **修复策略数**: {len(data.get('fix_strategies', []))}

"""

            # 显示最佳修复策略
            strategies = data.get('fix_strategies', [])
            if strategies:
                best = max(strategies, key=lambda s: s.get('confidence', 0))
                if best.get('confidence', 0) > 0:
                    report += f"""**最佳修复策略**: {best.get('strategy', 'unknown')}
- 描述: {best.get('description', 'N/A')}
- 置信度: {best.get('confidence', 0):.1%}
- 成功/失败: {best.get('success_count', 0)}/{best.get('failure_count', 0)}

"""

        report += """---

## 💡 改进建议

"""

        # 根据分析结果生成改进建议
        if analysis["total_errors"] > 50:
            report += "1. **错误频率过高**: 建议加强代码审查和测试覆盖\n"

        # 检查低成功率策略
        low_success_strategies = [
            s for s, stats in analysis["fix_success_rate"].items()
            if stats["rate"] < 0.5 and stats["success"] + stats["failure"] >= 3
        ]

        if low_success_strategies:
            report += f"2. **修复策略优化**: 以下策略成功率较低，需要改进:\n"
            for strategy in low_success_strategies:
                report += f"   - {strategy}\n"

        # 检查问题集中的文件
        if analysis.get("most_problematic_files"):
            top_file = analysis["most_problematic_files"][0]
            if top_file["error_count"] > 5:
                report += f"3. **重点关注文件**: `{top_file['file']}` 出现 {top_file['error_count']} 个错误，建议重点重构\n"

        report += f"""
---

**学习数据库**: `{self.pattern_db}`
**修复历史**: `{self.fix_history}`
"""

        return report

    def suggest_proactive_fixes(self) -> List[Dict]:
        """
        基于历史模式建议主动修复

        Returns:
            建议的主动修复列表
        """
        suggestions = []

        analysis = self.analyze_error_trends()

        if "error" in analysis:
            return []

        # 针对高频错误提供建议
        for error in analysis["most_common_errors"][:3]:
            error_code = error["error_code"]
            pattern = f"CODE_{error_code}"

            if pattern in self.patterns:
                best_fix = self.get_best_fix_strategy({"code": error_code, "message": ""})
                if best_fix:
                    suggestions.append({
                        "error_code": error_code,
                        "frequency": error["count"],
                        "recommended_fix": best_fix,
                        "urgency": "high" if error["count"] > 10 else "medium"
                    })

        # 针对问题文件提供建议
        for file_info in analysis.get("most_problematic_files", [])[:3]:
            if file_info["error_count"] >= 5:
                suggestions.append({
                    "type": "file_refactoring",
                    "file": file_info["file"],
                    "error_count": file_info["error_count"],
                    "recommendation": "建议重构此文件以减少错误",
                    "urgency": "high"
                })

        return suggestions

def main():
    """示例使用"""
    learner = ErrorPatternLearner()

    # 模拟学习过程
    errors_and_fixes = [
        (
            {"code": "LNK1104", "message": "无法打开文件'PortMaster.exe'", "file": "PortMaster.vcxproj", "line": 0},
            {"strategy": "kill_process", "description": "关闭占用进程", "actions": ["taskkill /F /IM PortMaster.exe"]},
            True
        ),
        (
            {"code": "LNK1104", "message": "无法打开文件'PortMaster.exe'", "file": "PortMaster.vcxproj", "line": 0},
            {"strategy": "kill_process", "description": "关闭占用进程", "actions": ["taskkill /F /IM PortMaster.exe"]},
            True
        ),
        (
            {"code": "C2065", "message": "未声明的标识符 'std::vector'", "file": "Transport.cpp", "line": 42},
            {"strategy": "add_include", "description": "添加#include <vector>", "actions": ["#include <vector>"]},
            True
        )
    ]

    for error_info, fix_applied, success in errors_and_fixes:
        learner.learn_from_error(error_info, fix_applied, success)

    # 生成学习报告
    report = learner.generate_learning_report()
    print(report)

    # 保存报告
    report_file = Path("error_pattern_learning_report.md")
    report_file.write_text(report, encoding='utf-8')
    print(f"\n报告已保存到: {report_file}")

    # 获取主动修复建议
    suggestions = learner.suggest_proactive_fixes()
    print(f"\n主动修复建议: {json.dumps(suggestions, ensure_ascii=False, indent=2)}")

if __name__ == "__main__":
    main()
