#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Main Entry Point

This is the main entry point for the AI Development System.
It provides a command-line interface for running various analysis
and automation tasks on software projects.
"""

import sys
import argparse
import logging
from pathlib import Path
from typing import Dict, Any, Optional

# Add the current directory to Python path for imports
sys.path.insert(0, str(Path(__file__).parent))

from core.master_controller import MasterController
from core.fix_controller import FixController
from core.workflow_runner import WorkflowRunner
from config.config_manager import ConfigManager
from utils.log_utils import setup_logging
from reporters.report_generator import ReportGenerator


def setup_argument_parser() -> argparse.ArgumentParser:
    """Setup command line argument parser"""
    parser = argparse.ArgumentParser(
        description="AI Development System - Automated Code Analysis and Improvement",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s analyze /path/to/project                    # Run full analysis
  %(prog)s build /path/to/project                      # Build project only
  %(prog)s test /path/to/project --coverage           # Run tests with coverage
  %(prog)s fix /path/to/project --auto-fix            # Auto-fix detected issues
  %(prog)s workflow /path/to/project --config custom.yaml  # Run custom workflow
  %(prog)s report /path/to/project --format html      # Generate HTML report
        """
    )

    # Global arguments
    parser.add_argument(
        'project_path',
        type=Path,
        help='Path to the project directory'
    )

    parser.add_argument(
        '--config', '-c',
        type=Path,
        help='Path to configuration file'
    )

    parser.add_argument(
        '--output', '-o',
        type=Path,
        default='output',
        help='Output directory for results (default: output)'
    )

    parser.add_argument(
        '--log-level',
        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
        default='INFO',
        help='Logging level (default: INFO)'
    )

    parser.add_argument(
        '--debug', '-d',
        action='store_true',
        help='Enable debug mode'
    )

    # Subcommands
    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Analyze command
    analyze_parser = subparsers.add_parser('analyze', help='Run complete analysis')
    analyze_parser.add_argument(
        '--skip-build',
        action='store_true',
        help='Skip build verification'
    )
    analyze_parser.add_argument(
        '--skip-tests',
        action='store_true',
        help='Skip test execution'
    )
    analyze_parser.add_argument(
        '--skip-coverage',
        action='store_true',
        help='Skip coverage analysis'
    )

    # Build command
    build_parser = subparsers.add_parser('build', help='Build project')
    build_parser.add_argument(
        '--type',
        choices=['debug', 'release', 'clean'],
        default='debug',
        help='Build type (default: debug)'
    )

    # Test command
    test_parser = subparsers.add_parser('test', help='Run tests')
    test_parser.add_argument(
        '--coverage',
        action='store_true',
        help='Collect coverage information'
    )
    test_parser.add_argument(
        '--framework',
        help='Specify test framework'
    )

    # Fix command
    fix_parser = subparsers.add_parser('fix', help='Fix detected issues')
    fix_parser.add_argument(
        '--auto-fix',
        action='store_true',
        help='Automatically apply fixes'
    )
    fix_parser.add_argument(
        '--backup',
        action='store_true',
        default=True,
        help='Create backup before fixing (default: True)'
    )
    fix_parser.add_argument(
        '--issue-types',
        nargs='+',
        help='Types of issues to fix'
    )

    # Workflow command
    workflow_parser = subparsers.add_parser('workflow', help='Run custom workflow')
    workflow_parser.add_argument(
        '--workflow-file',
        type=Path,
        help='Path to workflow configuration file'
    )
    workflow_parser.add_argument(
        '--workflow-name',
        help='Name of workflow to run'
    )

    # Report command
    report_parser = subparsers.add_parser('report', help='Generate reports')
    report_parser.add_argument(
        '--format',
        choices=['json', 'markdown', 'html'],
        default='html',
        help='Report format (default: html)'
    )
    report_parser.add_argument(
        '--template',
        help='Custom report template'
    )
    report_parser.add_argument(
        '--data-file',
        type=Path,
        help='Path to analysis data file'
    )

    # Config command
    config_parser = subparsers.add_parser('config', help='Configuration management')
    config_subparsers = config_parser.add_subparsers(dest='config_action', help='Config actions')

    config_subparsers.add_parser('show', help='Show current configuration')
    config_subparsers.add_parser('validate', help='Validate configuration')

    init_parser = config_subparsers.add_parser('init', help='Initialize configuration')
    init_parser.add_argument(
        '--force',
        action='store_true',
        help='Overwrite existing configuration'
    )

    return parser


def setup_system_logging(config: ConfigManager, debug: bool = False) -> None:
    """Setup system logging"""
    log_level = config.get('logging.level', 'INFO')
    if debug:
        log_level = 'DEBUG'

    log_config = {
        'level': log_level,
        'file': config.get('logging.file', 'ai_dev_system.log'),
        'format': config.get('logging.format', '%(asctime)s - %(name)s - %(levelname)s - %(message)s'),
        'max_file_size': config.get('logging.max_file_size', 10485760),
        'console_output': config.get('logging.console_output', True)
    }

    setup_logging(log_config)


def handle_analyze_command(args, config: ConfigManager) -> int:
    """Handle analyze command"""
    try:
        print("🔍 Starting project analysis...")

        controller = MasterController(args.project_path, config)

        # Update config based on command line arguments
        if args.skip_build:
            config.set('build.enabled', False)
        if args.skip_tests:
            config.set('test.enabled', False)
        if args.skip_coverage:
            config.set('test.collect_coverage', False)

        # Run full workflow
        results = controller.run_full_workflow()

        # Print summary
        print("\n📊 Analysis Results:")
        print(f"   Build Status: {'✅ Success' if results.get('build', {}).get('success') else '❌ Failed'}")
        print(f"   Tests: {results.get('test', {}).get('passed_tests', 0)} passed, {results.get('test', {}).get('failed_tests', 0)} failed")
        print(f"   Coverage: {results.get('coverage', {}).get('line_coverage', 0):.1f}%")
        print(f"   Issues Found: {len(results.get('static_analysis', {}).get('issues', []))}")
        print(f"   Performance Issues: {len(results.get('performance', {}).get('issues', []))}")

        # Generate report
        if results:
            generator = ReportGenerator(args.output / 'reports', config)
            report_path = generator.generate_report(results, format='html')
            if report_path:
                print(f"\n📄 Report generated: {report_path}")

        return 0 if results.get('build', {}).get('success') else 1

    except Exception as e:
        print(f"❌ Analysis failed: {e}")
        return 1


def handle_build_command(args, config: ConfigManager) -> int:
    """Handle build command"""
    try:
        print(f"🔨 Building project ({args.type})...")

        from integrators.build_integrator import BuildIntegrator
        integrator = BuildIntegrator(args.project_path, config)

        result = integrator.execute_build(build_type=args.type)

        if result.success:
            print("✅ Build completed successfully")
            print(f"   Duration: {result.duration:.2f}s")
            print(f"   Warnings: {result.warnings_count}")
            print(f"   Artifacts: {len(result.build_artifacts)}")
        else:
            print("❌ Build failed")
            print(f"   Exit Code: {result.exit_code}")
            print(f"   Errors: {result.errors_count}")
            if result.error_output:
                print(f"   Error Output:\n{result.error_output}")

        return 0 if result.success else 1

    except Exception as e:
        print(f"❌ Build failed: {e}")
        return 1


def handle_test_command(args, config: ConfigManager) -> int:
    """Handle test command"""
    try:
        print("🧪 Running tests...")

        from integrators.test_integrator import TestIntegrator
        integrator = TestIntegrator(args.project_path, config)

        if args.framework:
            config.set('test.framework', args.framework)

        result = integrator.execute_tests(include_coverage=args.coverage)

        print(f"\n📊 Test Results:")
        print(f"   Total Tests: {result.total_tests}")
        print(f"   Passed: {result.passed_tests}")
        print(f"   Failed: {result.failed_tests}")
        print(f"   Skipped: {result.skipped_tests}")
        print(f"   Errors: {result.errors}")
        print(f"   Duration: {result.duration:.2f}s")

        if result.coverage_info:
            print(f"   Coverage: {result.coverage_info.get('line_coverage', 0):.1f}%")

        return 0 if result.failed_tests == 0 and result.errors == 0 else 1

    except Exception as e:
        print(f"❌ Test execution failed: {e}")
        return 1


def handle_fix_command(args, config: ConfigManager) -> int:
    """Handle fix command"""
    try:
        print("🔧 Analyzing and fixing issues...")

        # First run analysis to find issues
        controller = MasterController(args.project_path, config)
        analysis_results = controller.run_full_workflow()

        issues = []

        # Collect issues from different analyzers
        static_issues = analysis_results.get('static_analysis', {}).get('issues', [])
        performance_issues = analysis_results.get('performance', {}).get('issues', [])
        error_patterns = analysis_results.get('error_patterns', {}).get('matches', [])

        issues.extend(static_issues)
        issues.extend(performance_issues)

        # Convert error patterns to issue format
        for pattern in error_patterns:
            issues.append({
                'type': 'error_pattern',
                'severity': pattern.get('severity', 'medium'),
                'file': pattern.get('file_path'),
                'line': pattern.get('line_number'),
                'message': pattern.get('matched_text'),
                'description': pattern.get('pattern', {}).get('description', '')
            })

        if not issues:
            print("✅ No issues found to fix")
            return 0

        print(f"   Found {len(issues)} issues to potentially fix")

        if not args.auto_fix:
            print("   Use --auto-fix to automatically apply fixes")
            return 0

        # Apply fixes
        fix_controller = FixController(args.project_path, config)
        fix_results = fix_controller.apply_fixes(issues, backup=args.backup)

        print(f"\n🔧 Fix Results:")
        print(f"   Attempted Fixes: {fix_results.get('attempted_fixes', 0)}")
        print(f"   Successful Fixes: {fix_results.get('successful_fixes', 0)}")
        print(f"   Failed Fixes: {fix_results.get('failed_fixes', 0)}")

        if fix_results.get('backup_created'):
            print(f"   Backup created: {fix_results.get('backup_path')}")

        return 0

    except Exception as e:
        print(f"❌ Fix operation failed: {e}")
        return 1


def handle_workflow_command(args, config: ConfigManager) -> int:
    """Handle workflow command"""
    try:
        print("⚙️ Running custom workflow...")

        runner = WorkflowRunner(args.project_path, config)

        if args.workflow_file:
            workflow = runner.load_workflow(args.workflow_file)
        elif args.workflow_name:
            workflow = runner.get_workflow(args.workflow_name)
        else:
            print("❌ Either --workflow-file or --workflow-name is required")
            return 1

        if not workflow:
            print("❌ Could not load workflow")
            return 1

        results = runner.run_workflow(workflow)

        print(f"\n📊 Workflow Results:")
        print(f"   Total Steps: {len(workflow.get('steps', []))}")
        print(f"   Completed Steps: {len([r for r in results if r.get('success')])}")
        print(f"   Failed Steps: {len([r for r in results if not r.get('success')])}")
        print(f"   Duration: {sum(r.get('duration', 0) for r in results):.2f}s")

        return 0 if all(r.get('success') for r in results) else 1

    except Exception as e:
        print(f"❌ Workflow execution failed: {e}")
        return 1


def handle_report_command(args, config: ConfigManager) -> int:
    """Handle report command"""
    try:
        print("📄 Generating report...")

        # Load analysis data if provided
        data = None
        if args.data_file and args.data_file.exists():
            import json
            with open(args.data_file, 'r') as f:
                data = json.load(f)
        else:
            # Run analysis to get data
            controller = MasterController(args.project_path, config)
            data = controller.run_full_workflow()

        if not data:
            print("❌ No data available for report generation")
            return 1

        generator = ReportGenerator(args.output, config)
        report_path = generator.generate_report(data, format=args.format, template=args.template)

        if report_path:
            print(f"✅ Report generated: {report_path}")
            return 0
        else:
            print("❌ Report generation failed")
            return 1

    except Exception as e:
        print(f"❌ Report generation failed: {e}")
        return 1


def handle_config_command(args, config: ConfigManager) -> int:
    """Handle config command"""
    try:
        if args.config_action == 'show':
            print("📋 Current Configuration:")
            print(config.export_config(format='yaml'))

        elif args.config_action == 'validate':
            print("🔍 Validating configuration...")
            errors = config.validate_config()
            if errors:
                print("❌ Configuration validation failed:")
                for error in errors:
                    print(f"   - {error}")
                return 1
            else:
                print("✅ Configuration is valid")

        elif args.config_action == 'init':
            print("📝 Initializing configuration...")
            if config.config_file.exists() and not args.force:
                print(f"❌ Configuration file already exists: {config.config_file}")
                print("   Use --force to overwrite")
                return 1

            success = config.create_user_config()
            if success:
                print(f"✅ Configuration created: {config.config_file}")
                return 0
            else:
                print("❌ Failed to create configuration")
                return 1

        return 0

    except Exception as e:
        print(f"❌ Config operation failed: {e}")
        return 1


def main() -> int:
    """Main entry point"""
    parser = setup_argument_parser()
    args = parser.parse_args()

    # Validate project path
    if not args.project_path.exists():
        print(f"❌ Project path does not exist: {args.project_path}")
        return 1

    # Load configuration
    try:
        config = ConfigManager(args.config, args.project_path)
    except Exception as e:
        print(f"❌ Failed to load configuration: {e}")
        return 1

    # Setup logging
    setup_system_logging(config, args.debug)

    # Override config with command line arguments
    if args.debug:
        config.set('system.debug_mode', True)
    config.set('system.log_level', args.log_level)
    config.set('system.output_directory', str(args.output))

    # Ensure output directory exists
    args.output.mkdir(parents=True, exist_ok=True)

    # Handle commands
    if args.command == 'analyze':
        return handle_analyze_command(args, config)
    elif args.command == 'build':
        return handle_build_command(args, config)
    elif args.command == 'test':
        return handle_test_command(args, config)
    elif args.command == 'fix':
        return handle_fix_command(args, config)
    elif args.command == 'workflow':
        return handle_workflow_command(args, config)
    elif args.command == 'report':
        return handle_report_command(args, config)
    elif args.command == 'config':
        return handle_config_command(args, config)
    else:
        # Default to analyze if no command specified
        return handle_analyze_command(args, config)


if __name__ == '__main__':
    sys.exit(main())