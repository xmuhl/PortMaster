#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Simple System Check
"""

import sys
import os
from pathlib import Path

def main():
    print("AI Development System - System Integrity Check")
    print("=" * 60)

    current_dir = Path(__file__).parent
    checks_passed = 0
    total_checks = 0

    # Check key files
    key_files = [
        "__init__.py",
        "main.py",
        "requirements.txt",
        "README.md",
        "USER_MANUAL.md",
        "PROJECT_SUMMARY.md"
    ]

    print("\n[INFO] Checking key files:")
    for file_name in key_files:
        file_path = current_dir / file_name
        total_checks += 1
        if file_path.exists():
            print(f"[OK] {file_name}")
            checks_passed += 1
        else:
            print(f"[FAIL] {file_name} - Not found")

    # Check directories
    key_dirs = [
        "core",
        "analyzers",
        "integrators",
        "utils",
        "config",
        "reporters"
    ]

    print("\n[INFO] Checking directories:")
    for dir_name in key_dirs:
        dir_path = current_dir / dir_name
        total_checks += 1
        if dir_path.exists() and dir_path.is_dir():
            print(f"[OK] {dir_name}/")
            checks_passed += 1
        else:
            print(f"[FAIL] {dir_name}/ - Not found")

    # Check module files in core
    print("\n[INFO] Checking core module files:")
    core_files = [
        "core/__init__.py",
        "core/master_controller.py",
        "core/fix_controller.py",
        "core/workflow_runner.py"
    ]

    for file_name in core_files:
        file_path = current_dir / file_name
        total_checks += 1
        if file_path.exists():
            print(f"[OK] {file_name}")
            checks_passed += 1
        else:
            print(f"[FAIL] {file_name} - Not found")

    # Summary
    print("\n" + "=" * 60)
    print(f"Check Results: {checks_passed}/{total_checks} passed")

    if checks_passed == total_checks:
        print("SUCCESS: All checks passed! AI Development System is ready.")
        return 0
    else:
        failed = total_checks - checks_passed
        print(f"WARNING: {failed} checks failed.")
        return 1

if __name__ == "__main__":
    sys.exit(main())