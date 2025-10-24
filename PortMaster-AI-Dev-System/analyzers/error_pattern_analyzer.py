#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Error Pattern Analyzer

This module provides error pattern detection and analysis capabilities
to identify recurring issues and suggest solutions.
"""

import os
import re
import json
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any, Set, Tuple
from dataclasses import dataclass
from collections import defaultdict

from ..utils.file_utils import find_files, read_file
from ..utils.log_utils import setup_logging


@dataclass
class ErrorPattern:
    """Represents an error pattern"""
    pattern_id: str
    name: str
    description: str
    severity: str  # 'critical', 'high', 'medium', 'low'
    category: str  # 'syntax', 'logic', 'performance', 'security', 'maintainability'
    regex_pattern: str
    suggestion: str
    tags: List[str]


@dataclass
class ErrorMatch:
    """Represents a match of an error pattern"""
    pattern: ErrorPattern
    file_path: str
    line_number: int
    column_number: int
    matched_text: str
    context: str


@dataclass
class ErrorAnalysisResult:
    """Result of error pattern analysis"""
    patterns_found: List[ErrorMatch]
    critical_issues: List[ErrorMatch]
    recommendations: List[str]
    summary: Dict[str, Any]
    success: bool
    error_message: Optional[str]


class ErrorPatternAnalyzer:
    """Error pattern analyzer"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize error pattern analyzer

        Args:
            config: Configuration dictionary
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        self.patterns = self._load_error_patterns()

    def _load_error_patterns(self) -> List[ErrorPattern]:
        """Load error patterns from configuration or use defaults"""
        default_patterns = [
            # Syntax errors
            ErrorPattern(
                pattern_id="SYNTAX001",
                name="Missing Semicolon",
                description="Missing semicolon at end of statement",
                severity="high",
                category="syntax",
                regex_pattern=r"[^;{}\s]\s*$",
                suggestion="Add a semicolon at the end of the statement",
                tags=["syntax", "semicolon"]
            ),
            ErrorPattern(
                pattern_id="SYNTAX002",
                name="Unmatched Parentheses",
                description="Unmatched opening or closing parentheses",
                severity="critical",
                category="syntax",
                regex_pattern=r"\([^)]*$|^[^)]*\)",
                suggestion="Ensure all parentheses are properly matched",
                tags=["syntax", "parentheses"]
            ),
            ErrorPattern(
                pattern_id="SYNTAX003",
                name="Unmatched Brackets",
                description="Unmatched opening or closing brackets",
                severity="critical",
                category="syntax",
                regex_pattern=r"\[[^\]]*$|^[^\]]*\]",
                suggestion="Ensure all brackets are properly matched",
                tags=["syntax", "brackets"]
            ),
            ErrorPattern(
                pattern_id="SYNTAX004",
                name="Unmatched Braces",
                description="Unmatched opening or closing braces",
                severity="critical",
                category="syntax",
                regex_pattern=r"\{[^}]*$|^[^}]*\}",
                suggestion="Ensure all braces are properly matched",
                tags=["syntax", "braces"]
            ),

            # Logic errors
            ErrorPattern(
                pattern_id="LOGIC001",
                name="Potential Null Dereference",
                description="Dereferencing potentially null pointer",
                severity="high",
                category="logic",
                regex_pattern=r"(\w+)\.\w+\s*(?!.*null\s*check|.*!=\s*null)",
                suggestion="Add null check before dereferencing",
                tags=["logic", "null", "safety"]
            ),
            ErrorPattern(
                pattern_id="LOGIC002",
                name="Comparison Assignment",
                description="Using assignment operator in comparison",
                severity="high",
                category="logic",
                regex_pattern=r"==\s*=",
                suggestion="Use proper comparison operator (== instead of =)",
                tags=["logic", "comparison", "assignment"]
            ),
            ErrorPattern(
                pattern_id="LOGIC003",
                name="Empty Block",
                description="Empty block statement that might indicate incomplete code",
                severity="medium",
                category="logic",
                regex_pattern=r"\{\s*\}",
                suggestion="Remove empty block or add implementation",
                tags=["logic", "empty", "incomplete"]
            ),

            # Performance issues
            ErrorPattern(
                pattern_id="PERF001",
                name="String Concatenation in Loop",
                description="String concatenation inside loop can be inefficient",
                severity="medium",
                category="performance",
                regex_pattern=r"(for|while).*\+.*[\w]+",
                suggestion="Use StringBuilder or join() for string concatenation in loops",
                tags=["performance", "string", "loop"]
            ),
            ErrorPattern(
                pattern_id="PERF002",
                name="Inefficient Regular Expression",
                description="Regular expression that could cause performance issues",
                severity="medium",
                category="performance",
                regex_pattern=r".*\.\*.*\.\*",
                suggestion="Consider using more specific regex patterns",
                tags=["performance", "regex"]
            ),

            # Security issues
            ErrorPattern(
                pattern_id="SEC001",
                name="Hardcoded Password",
                description="Hardcoded password or sensitive information",
                severity="critical",
                category="security",
                regex_pattern=r"(password|pwd|pass|secret|key)\s*=\s*[\"'][^\"']+[\"']",
                suggestion="Use environment variables or secure configuration",
                tags=["security", "password", "sensitive"]
            ),
            ErrorPattern(
                pattern_id="SEC002",
                name="SQL Injection Risk",
                description="Potential SQL injection vulnerability",
                severity="critical",
                category="security",
                regex_pattern=r"(execute|query|sql).*\+.*[\"']",
                suggestion="Use parameterized queries to prevent SQL injection",
                tags=["security", "sql", "injection"]
            ),
            ErrorPattern(
                pattern_id="SEC003",
                name="Use of eval()",
                description="Use of eval() function can be dangerous",
                severity="high",
                category="security",
                regex_pattern=r"eval\s*\(",
                suggestion="Avoid using eval(), consider safer alternatives",
                tags=["security", "eval", "dynamic"]
            ),

            # Maintainability issues
            ErrorPattern(
                pattern_id="MAINT001",
                name="Magic Number",
                description="Magic number used without explanation",
                severity="low",
                category="maintainability",
                regex_pattern=r"\b(?!0|1)[2-9]\d*\b",
                suggestion="Replace magic numbers with named constants",
                tags=["maintainability", "magic-number", "constant"]
            ),
            ErrorPattern(
                pattern_id="MAINT002",
                name="Long Line",
                description="Line exceeds recommended length",
                severity="low",
                category="maintainability",
                regex_pattern=r".{80,}",
                suggestion="Break long lines into multiple lines",
                tags=["maintainability", "long-line"]
            ),
            ErrorPattern(
                pattern_id="MAINT003",
                name="TODO/FIXME Comment",
                description="TODO or FIXME comment indicating incomplete work",
                severity="medium",
                category="maintainability",
                regex_pattern=r"(TODO|FIXME|HACK|XXX)",
                suggestion="Address TODO/FIXME items or track them properly",
                tags=["maintainability", "todo", "incomplete"]
            ),
            ErrorPattern(
                pattern_id="MAINT004",
                name="Deep Nesting",
                description="Code with deep nesting levels can be hard to read",
                severity="medium",
                category="maintainability",
                regex_pattern=r"(\t| {4}){5,}",
                suggestion="Consider refactoring to reduce nesting levels",
                tags=["maintainability", "nesting", "readability"]
            ),

            # Common language-specific issues
            ErrorPattern(
                pattern_id="LANG001",
                name="Python Import Star",
                description="Using star imports can cause namespace pollution",
                severity="medium",
                category="maintainability",
                regex_pattern=r"from\s+\w+\s+import\s+\*",
                suggestion="Import specific modules instead of using star imports",
                tags=["python", "import", "namespace"]
            ),
            ErrorPattern(
                pattern_id="LANG002",
                name="C++ Using Namespace Std",
                description="Using namespace std in header files",
                severity="medium",
                category="maintainability",
                regex_pattern=r"using\s+namespace\s+std\s*;",
                suggestion="Avoid using namespace std in header files",
                tags=["cpp", "namespace", "header"]
            ),
            ErrorPattern(
                pattern_id="LANG003",
                name="JavaScript Var",
                description="Using var instead of let/const",
                severity="low",
                category="maintainability",
                regex_pattern=r"\bvar\s+\w+\s*=",
                suggestion="Use let or const instead of var",
                tags=["javascript", "var", "let-const"]
            )
        ]

        # Try to load custom patterns from config
        custom_patterns = self.config.get('custom_patterns', [])
        if custom_patterns:
            for pattern_data in custom_patterns:
                try:
                    pattern = ErrorPattern(**pattern_data)
                    default_patterns.append(pattern)
                except Exception as e:
                    self.logger.warning(f"Failed to load custom pattern: {e}")

        return default_patterns

    def analyze(self, project_root: Path) -> ErrorAnalysisResult:
        """
        Analyze project for error patterns

        Args:
            project_root: Root directory of the project

        Returns:
            Error analysis result
        """
        self.logger.info(f"Starting error pattern analysis for project: {project_root}")

        try:
            # Find all source files
            source_files = find_files(project_root, [
                '*.py', '*.java', '*.cpp', '*.c', '*.h', '*.hpp',
                '*.js', '*.ts', '*.php', '*.rb', '*.go', '*.rs'
            ])

            self.logger.info(f"Found {len(source_files)} source files")

            # Analyze each file for error patterns
            all_matches = []
            for file_path in source_files:
                matches = self._analyze_file(file_path)
                all_matches.extend(matches)

            # Filter critical issues
            critical_issues = [
                match for match in all_matches
                if match.pattern.severity in ['critical', 'high']
            ]

            # Generate recommendations
            recommendations = self._generate_recommendations(all_matches)

            # Create summary
            summary = self._create_summary(all_matches)

            result = ErrorAnalysisResult(
                patterns_found=all_matches,
                critical_issues=critical_issues,
                recommendations=recommendations,
                summary=summary,
                success=True,
                error_message=None
            )

            self.logger.info(f"Error pattern analysis completed: {len(all_matches)} patterns found")
            return result

        except Exception as e:
            self.logger.error(f"Error pattern analysis failed: {str(e)}")
            return ErrorAnalysisResult(
                patterns_found=[],
                critical_issues=[],
                recommendations=[],
                summary={},
                success=False,
                error_message=str(e)
            )

    def _analyze_file(self, file_path: Path) -> List[ErrorMatch]:
        """Analyze a single file for error patterns"""
        content = read_file(file_path)
        if not content:
            return []

        lines = content.split('\n')
        matches = []

        for line_num, line in enumerate(lines, 1):
            for pattern in self.patterns:
                try:
                    for match in re.finditer(pattern.regex_pattern, line):
                        if match:
                            # Get context (3 lines before and after)
                            context_start = max(0, line_num - 4)
                            context_end = min(len(lines), line_num + 3)
                            context = '\n'.join(lines[context_start:context_end])

                            matches.append(ErrorMatch(
                                pattern=pattern,
                                file_path=str(file_path.relative_to(file_path.parent.parent)),
                                line_number=line_num,
                                column_number=match.start() + 1,
                                matched_text=match.group(),
                                context=context
                            ))
                except re.error as e:
                    self.logger.warning(f"Invalid regex pattern {pattern.pattern_id}: {e}")

        return matches

    def _generate_recommendations(self, matches: List[ErrorMatch]) -> List[str]:
        """Generate recommendations based on found patterns"""
        recommendations = []
        seen_suggestions = set()

        # Group patterns by category
        patterns_by_category = defaultdict(list)
        for match in matches:
            patterns_by_category[match.pattern.category].append(match.pattern)

        # Generate recommendations for each category
        for category, patterns in patterns_by_category.items():
            category_recommendations = self._get_category_recommendations(category, patterns)
            for rec in category_recommendations:
                if rec not in seen_suggestions:
                    recommendations.append(rec)
                    seen_suggestions.add(rec)

        # Add specific recommendations for critical issues
        critical_patterns = {match.pattern for match in matches if match.pattern.severity == 'critical'}
        for pattern in critical_patterns:
            if pattern.suggestion not in seen_suggestions:
                recommendations.append(f"Critical: {pattern.suggestion}")
                seen_suggestions.add(pattern.suggestion)

        return recommendations

    def _get_category_recommendations(self, category: str, patterns: List[ErrorPattern]) -> List[str]:
        """Get recommendations for a specific category"""
        recommendations = []

        if category == "syntax":
            recommendations.append("Syntax errors should be fixed immediately as they prevent code execution.")
            recommendations.append("Consider using IDE syntax highlighting and real-time error checking.")

        elif category == "logic":
            recommendations.append("Review logic errors carefully as they can cause runtime failures.")
            recommendations.append("Add unit tests to catch logic errors early in development.")

        elif category == "performance":
            recommendations.append("Performance issues should be addressed in performance-critical paths.")
            recommendations.append("Profile your code to identify actual performance bottlenecks.")

        elif category == "security":
            recommendations.append("Security issues should be treated with highest priority.")
            recommendations.append("Conduct regular security reviews and use security scanning tools.")

        elif category == "maintainability":
            recommendations.append("Maintainability issues should be addressed to improve code readability.")
            recommendations.append("Consider refactoring complex code and adding better documentation.")

        return recommendations

    def _create_summary(self, matches: List[ErrorMatch]) -> Dict[str, Any]:
        """Create summary of analysis results"""
        summary = {
            'total_matches': len(matches),
            'unique_patterns': len(set(match.pattern.pattern_id for match in matches)),
            'files_affected': len(set(match.file_path for match in matches)),
            'severity_distribution': defaultdict(int),
            'category_distribution': defaultdict(int),
            'most_common_patterns': [],
            'files_with_most_issues': []
        }

        # Calculate distributions
        for match in matches:
            summary['severity_distribution'][match.pattern.severity] += 1
            summary['category_distribution'][match.pattern.category] += 1

        # Find most common patterns
        pattern_counts = defaultdict(int)
        for match in matches:
            pattern_counts[match.pattern.pattern_id] += 1

        summary['most_common_patterns'] = [
            {'pattern_id': pid, 'count': count, 'name': next(
                (p.name for p in self.patterns if p.pattern_id == pid), 'Unknown'
            )}
            for pid, count in sorted(pattern_counts.items(), key=lambda x: x[1], reverse=True)[:10]
        ]

        # Find files with most issues
        file_counts = defaultdict(int)
        for match in matches:
            file_counts[match.file_path] += 1

        summary['files_with_most_issues'] = [
            {'file': file_path, 'count': count}
            for file_path, count in sorted(file_counts.items(), key=lambda x: x[1], reverse=True)[:10]
        ]

        return dict(summary)

    def export_patterns(self, output_file: Path) -> bool:
        """Export all error patterns to JSON file"""
        try:
            patterns_data = []
            for pattern in self.patterns:
                patterns_data.append({
                    'pattern_id': pattern.pattern_id,
                    'name': pattern.name,
                    'description': pattern.description,
                    'severity': pattern.severity,
                    'category': pattern.category,
                    'regex_pattern': pattern.regex_pattern,
                    'suggestion': pattern.suggestion,
                    'tags': pattern.tags
                })

            with open(output_file, 'w', encoding='utf-8') as f:
                json.dump(patterns_data, f, indent=2)

            self.logger.info(f"Error patterns exported to {output_file}")
            return True

        except Exception as e:
            self.logger.error(f"Failed to export patterns: {str(e)}")
            return False

    def import_patterns(self, pattern_file: Path) -> bool:
        """Import custom error patterns from JSON file"""
        try:
            with open(pattern_file, 'r', encoding='utf-8') as f:
                patterns_data = json.load(f)

            for pattern_data in patterns_data:
                try:
                    pattern = ErrorPattern(**pattern_data)
                    self.patterns.append(pattern)
                except Exception as e:
                    self.logger.warning(f"Failed to import pattern {pattern_data.get('pattern_id', 'unknown')}: {e}")

            self.logger.info(f"Imported {len(patterns_data)} custom patterns from {pattern_file}")
            return True

        except Exception as e:
            self.logger.error(f"Failed to import patterns: {str(e)}")
            return False