#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Fix Controller

This module provides automated fixing capabilities for common code issues
detected during the analysis phase.
"""

import os
import re
import logging
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

from ..utils.file_utils import backup_file, restore_file, read_file, write_file
from ..utils.log_utils import setup_logging


class FixController:
    """Controller for automated code fixing"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize the fix controller

        Args:
            config: Configuration dictionary
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        self.fix_history = []

    def apply_fixes(self, project_root: Path, issues: List[Dict]) -> Dict[str, Any]:
        """
        Apply automated fixes to detected issues

        Args:
            project_root: Root directory of the project
            issues: List of detected issues

        Returns:
            Dictionary containing fix results
        """
        self.logger.info(f"Starting automated fix process for {len(issues)} issues")

        results = {
            "total_issues": len(issues),
            "fixes_attempted": 0,
            "fixes_successful": 0,
            "fixes_failed": 0,
            "details": []
        }

        for issue in issues:
            fix_result = self._fix_single_issue(project_root, issue)
            results["details"].append(fix_result)
            results["fixes_attempted"] += 1

            if fix_result["success"]:
                results["fixes_successful"] += 1
                self.logger.info(f"Successfully fixed issue: {issue.get('description', 'Unknown')}")
            else:
                results["fixes_failed"] += 1
                self.logger.warning(f"Failed to fix issue: {issue.get('description', 'Unknown')}")

        self.logger.info(f"Fix process completed: {results['fixes_successful']}/{results['fixes_attempted']} successful")

        return results

    def _fix_single_issue(self, project_root: Path, issue: Dict) -> Dict[str, Any]:
        """
        Fix a single issue

        Args:
            project_root: Root directory of the project
            issue: Issue description

        Returns:
            Fix result dictionary
        """
        issue_type = issue.get("type", "unknown")
        file_path = issue.get("file", "")
        line_number = issue.get("line", 0)

        result = {
            "issue_type": issue_type,
            "file": file_path,
            "line": line_number,
            "success": False,
            "message": "",
            "backup_created": False
        }

        if not file_path:
            result["message"] = "No file path specified for issue"
            return result

        full_path = project_root / file_path
        if not full_path.exists():
            result["message"] = f"File not found: {full_path}"
            return result

        # Create backup before fixing
        if backup_file(full_path):
            result["backup_created"] = True
        else:
            self.logger.warning(f"Failed to create backup for {full_path}")

        try:
            if issue_type == "syntax_error":
                success, message = self._fix_syntax_error(full_path, issue)
            elif issue_type == "import_error":
                success, message = self._fix_import_error(full_path, issue)
            elif issue_type == "undefined_variable":
                success, message = self._fix_undefined_variable(full_path, issue)
            elif issue_type == "complexity_violation":
                success, message = self._fix_complexity_violation(full_path, issue)
            elif issue_type == "unused_variable":
                success, message = self._fix_unused_variable(full_path, issue)
            elif issue_type == "missing_semicolon":
                success, message = self._fix_missing_semicolon(full_path, issue)
            elif issue_type == "missing_bracket":
                success, message = self._fix_missing_bracket(full_path, issue)
            else:
                success, message = False, f"Unsupported issue type: {issue_type}"

            result["success"] = success
            result["message"] = message

        except Exception as e:
            result["success"] = False
            result["message"] = f"Exception during fix: {str(e)}"

            # Restore backup if fix failed
            if result["backup_created"]:
                restore_file(full_path)

        return result

    def _fix_syntax_error(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix syntax errors"""
        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        lines = content.split('\n')
        line_num = issue.get("line", 1) - 1

        if 0 <= line_num < len(lines):
            line = lines[line_num]

            # Common syntax error fixes
            fixes = [
                # Missing semicolon
                (r'([^;{}\s]\s*)$', r'\1;'),
                # Missing closing parenthesis
                (r'([^)]\s*)$', r'\1)'),
                # Missing closing brace
                (r'([^}]\s*)$', r'\1}'),
                # Unclosed string
                (r'\"([^\"]*)$', r'"\1"'),
                (r'\'([^\']*)$', r"'\1'"),
            ]

            for pattern, replacement in fixes:
                if re.search(pattern, line):
                    lines[line_num] = re.sub(pattern, replacement, line)
                    new_content = '\n'.join(lines)
                    write_file(file_path, new_content)
                    return True, f"Fixed syntax error on line {line_num + 1}"

        return False, "Could not fix syntax error"

    def _fix_import_error(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix import errors"""
        missing_module = issue.get("missing_module", "")
        if not missing_module:
            return False, "No missing module specified"

        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        lines = content.split('\n')

        # Find import statements and add missing import
        import_added = False
        for i, line in enumerate(lines):
            if line.strip().startswith(('import ', 'from ')):
                # Insert after existing imports
                if i + 1 < len(lines) and not lines[i + 1].strip().startswith(('import ', 'from ')):
                    lines.insert(i + 1, f"import {missing_module}")
                    import_added = True
                    break

        if not import_added:
            # Add at the beginning of the file
            lines.insert(0, f"import {missing_module}")

        new_content = '\n'.join(lines)
        write_file(file_path, new_content)
        return True, f"Added import for {missing_module}"

    def _fix_undefined_variable(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix undefined variable errors"""
        var_name = issue.get("variable", "")
        if not var_name:
            return False, "No variable name specified"

        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        # Try to guess the correct variable name based on common patterns
        suggestions = [
            var_name.lower(),
            var_name.upper(),
            var_name.replace('_', ''),
            var_name + '_',
            '_' + var_name
        ]

        # Look for similar variable names in the file
        existing_vars = set()
        for line in content.split('\n'):
            # Find variable assignments
            matches = re.findall(r'(\w+)\s*=', line)
            existing_vars.update(matches)

        # Find the best match
        best_match = None
        for suggestion in suggestions:
            if suggestion in existing_vars:
                best_match = suggestion
                break

        if best_match:
            new_content = content.replace(var_name, best_match)
            write_file(file_path, new_content)
            return True, f"Replaced '{var_name}' with '{best_match}'"

        return False, f"Could not find replacement for undefined variable '{var_name}'"

    def _fix_complexity_violation(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix complexity violations by extracting functions"""
        line_num = issue.get("line", 1) - 1

        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        lines = content.split('\n')

        # This is a simplified version - real complexity fixing would require
        # more sophisticated AST analysis
        if 0 <= line_num < len(lines):
            # Add a comment suggesting manual refactoring
            lines.insert(line_num, f"# TODO: Extract complex logic into separate function (complexity: {issue.get('complexity', 'high')})")

            new_content = '\n'.join(lines)
            write_file(file_path, new_content)
            return True, "Added refactoring suggestion for complexity violation"

        return False, "Could not fix complexity violation"

    def _fix_unused_variable(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix unused variable by removing or commenting out"""
        var_name = issue.get("variable", "")
        line_num = issue.get("line", 1) - 1

        if not var_name:
            return False, "No variable name specified"

        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        lines = content.split('\n')

        if 0 <= line_num < len(lines):
            line = lines[line_num]

            # Comment out the line containing the unused variable
            if var_name in line:
                lines[line_num] = f"# {line.strip()}"
                new_content = '\n'.join(lines)
                write_file(file_path, new_content)
                return True, f"Commented out unused variable '{var_name}'"

        return False, "Could not fix unused variable"

    def _fix_missing_semicolon(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix missing semicolon"""
        line_num = issue.get("line", 1) - 1

        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        lines = content.split('\n')

        if 0 <= line_num < len(lines):
            line = lines[line_num].rstrip()
            if not line.endswith(';'):
                lines[line_num] = line + ';'
                new_content = '\n'.join(lines)
                write_file(file_path, new_content)
                return True, f"Added semicolon to line {line_num + 1}"

        return False, "Could not fix missing semicolon"

    def _fix_missing_bracket(self, file_path: Path, issue: Dict) -> Tuple[bool, str]:
        """Fix missing brackets"""
        line_num = issue.get("line", 1) - 1
        bracket_type = issue.get("bracket_type", "}")  # Default to closing brace

        content = read_file(file_path)
        if not content:
            return False, "Could not read file content"

        lines = content.split('\n')

        if 0 <= line_num < len(lines):
            line = lines[line_num].rstrip()
            lines[line_num] = line + bracket_type
            new_content = '\n'.join(lines)
            write_file(file_path, new_content)
            return True, f"Added missing bracket '{bracket_type}' to line {line_num + 1}"

        return False, "Could not fix missing bracket"

    def rollback_fixes(self, project_root: Path, fix_results: Dict) -> Dict[str, Any]:
        """
        Rollback all applied fixes

        Args:
            project_root: Root directory of the project
            fix_results: Results from previous fix operation

        Returns:
            Rollback results
        """
        self.logger.info("Starting rollback process")

        rollback_results = {
            "total_fixes": fix_results.get("fixes_attempted", 0),
            "rollback_attempted": 0,
            "rollback_successful": 0,
            "rollback_failed": 0,
            "details": []
        }

        for detail in fix_results.get("details", []):
            if detail.get("backup_created", False):
                file_path = project_root / detail["file"]

                if restore_file(file_path):
                    rollback_results["rollback_successful"] += 1
                    self.logger.info(f"Successfully rolled back fix for {detail['file']}")
                else:
                    rollback_results["rollback_failed"] += 1
                    self.logger.warning(f"Failed to rollback fix for {detail['file']}")

                rollback_results["rollback_attempted"] += 1

        self.logger.info(f"Rollback completed: {rollback_results['rollback_successful']}/{rollback_results['rollback_attempted']} successful")
        return rollback_results