#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Workflow Runner

This module provides a flexible workflow execution engine that can run
customized development workflows based on configuration.
"""

import os
import json
import logging
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Callable

from .master_controller import MasterController
from .fix_controller import FixController
from ..utils.log_utils import setup_logging
from ..utils.validation_utils import validate_workflow_config


class WorkflowStep:
    """Represents a single step in a workflow"""

    def __init__(self, step_config: Dict[str, Any]):
        """
        Initialize workflow step

        Args:
            step_config: Step configuration dictionary
        """
        self.name = step_config.get("name", "Unnamed Step")
        self.type = step_config.get("type", "unknown")
        self.config = step_config.get("config", {})
        self.dependencies = step_config.get("dependencies", [])
        self.condition = step_config.get("condition", None)
        self.retry_count = step_config.get("retry_count", 0)
        self.timeout = step_config.get("timeout", 300)
        self.enabled = step_config.get("enabled", True)

    def should_execute(self, context: Dict[str, Any]) -> bool:
        """
        Check if this step should be executed based on conditions

        Args:
            context: Workflow execution context

        Returns:
            True if step should be executed
        """
        if not self.enabled:
            return False

        # Check dependencies
        for dep in self.dependencies:
            if dep not in context or not context[dep].get("success", False):
                return False

        # Check condition
        if self.condition:
            return self._evaluate_condition(context)

        return True

    def _evaluate_condition(self, context: Dict[str, Any]) -> bool:
        """Evaluate step condition (simplified implementation)"""
        # This is a simplified version - real implementation would support
        # more complex condition expressions
        condition_str = self.condition

        # Replace common placeholders
        for key, value in context.items():
            if isinstance(value, dict) and "success" in value:
                condition_str = condition_str.replace(f"${{{key}}}", str(value["success"]))

        try:
            return eval(condition_str)
        except:
            return True  # Default to executing if condition can't be evaluated


class WorkflowRunner:
    """Workflow execution engine"""

    def __init__(self, config_path: Optional[str] = None):
        """
        Initialize workflow runner

        Args:
            config_path: Path to workflow configuration file
        """
        self.config_path = config_path
        self.logger = logging.getLogger(__name__)
        self.execution_history = []

    def load_workflow(self, workflow_path: str) -> Dict[str, Any]:
        """
        Load workflow configuration from file

        Args:
            workflow_path: Path to workflow configuration file

        Returns:
            Workflow configuration dictionary
        """
        try:
            with open(workflow_path, 'r', encoding='utf-8') as f:
                workflow_config = json.load(f)

            # Validate workflow configuration
            if not validate_workflow_config(workflow_config):
                raise ValueError("Invalid workflow configuration")

            return workflow_config

        except Exception as e:
            self.logger.error(f"Failed to load workflow from {workflow_path}: {e}")
            raise

    def run_workflow(self, workflow_config: Dict[str, Any], project_root: Optional[str] = None) -> Dict[str, Any]:
        """
        Execute a complete workflow

        Args:
            workflow_config: Workflow configuration
            project_root: Project root directory

        Returns:
            Workflow execution results
        """
        workflow_name = workflow_config.get("name", "Unnamed Workflow")
        self.logger.info(f"Starting workflow: {workflow_name}")

        # Initialize execution context
        context = {
            "workflow_name": workflow_name,
            "start_time": datetime.now().isoformat(),
            "project_root": project_root or os.getcwd(),
            "steps": {}
        }

        # Create steps from configuration
        steps = []
        for step_config in workflow_config.get("steps", []):
            steps.append(WorkflowStep(step_config))

        # Execute steps
        for step in steps:
            if not step.should_execute(context):
                self.logger.info(f"Skipping step: {step.name} (condition not met)")
                continue

            self.logger.info(f"Executing step: {step.name}")
            step_result = self._execute_step(step, context)
            context["steps"][step.name] = step_result

            # Check if step failed and workflow should stop
            if not step_result.get("success", False) and workflow_config.get("stop_on_failure", True):
                self.logger.error(f"Step {step.name} failed, stopping workflow")
                break

        # Finalize results
        context["end_time"] = datetime.now().isoformat()
        context["duration"] = (
            datetime.fromisoformat(context["end_time"]) -
            datetime.fromisoformat(context["start_time"])
        ).total_seconds()

        # Add to execution history
        self.execution_history.append(context)

        self.logger.info(f"Workflow {workflow_name} completed in {context['duration']:.2f} seconds")
        return context

    def _execute_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute a single workflow step

        Args:
            step: Workflow step to execute
            context: Execution context

        Returns:
            Step execution result
        """
        result = {
            "name": step.name,
            "type": step.type,
            "success": False,
            "start_time": datetime.now().isoformat(),
            "duration": 0,
            "error": None,
            "data": {}
        }

        # Map step types to execution functions
        step_executors = {
            "build": self._execute_build_step,
            "test": self._execute_test_step,
            "analyze": self._execute_analyze_step,
            "fix": self._execute_fix_step,
            "report": self._execute_report_step,
            "custom": self._execute_custom_step
        }

        executor = step_executors.get(step.type)
        if not executor:
            result["error"] = f"Unknown step type: {step.type}"
            return result

        # Execute with retry logic
        for attempt in range(step.retry_count + 1):
            try:
                step_result = executor(step, context)
                result.update(step_result)
                result["success"] = True
                break

            except Exception as e:
                self.logger.warning(f"Step {step.name} failed (attempt {attempt + 1}): {str(e)}")
                if attempt == step.retry_count:
                    result["error"] = str(e)
                else:
                    time.sleep(2 ** attempt)  # Exponential backoff

        result["end_time"] = datetime.now().isoformat()
        result["duration"] = (
            datetime.fromisoformat(result["end_time"]) -
            datetime.fromisoformat(result["start_time"])
        ).total_seconds()

        return result

    def _execute_build_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """Execute build step"""
        project_root = Path(context["project_root"])
        build_config = step.config

        # Initialize build integration (simplified)
        commands = build_config.get("commands", ["make"])
        timeout = build_config.get("timeout", 300)

        for command in commands:
            try:
                result = subprocess.run(
                    command,
                    shell=True,
                    cwd=project_root,
                    capture_output=True,
                    text=True,
                    timeout=timeout
                )

                if result.returncode != 0:
                    return {
                        "success": False,
                        "error": f"Build command failed: {command}",
                        "output": result.stdout,
                        "stderr": result.stderr
                    }

            except subprocess.TimeoutExpired:
                return {
                    "success": False,
                    "error": f"Build command timed out: {command}"
                }

        return {"success": True, "message": "Build completed successfully"}

    def _execute_test_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """Execute test step"""
        project_root = Path(context["project_root"])
        test_config = step.config

        # Run tests (simplified implementation)
        test_commands = test_config.get("commands", ["python -m pytest"])
        timeout = test_config.get("timeout", 600)

        for command in test_commands:
            try:
                result = subprocess.run(
                    command,
                    shell=True,
                    cwd=project_root,
                    capture_output=True,
                    text=True,
                    timeout=timeout
                )

                if result.returncode != 0:
                    return {
                        "success": False,
                        "error": f"Test command failed: {command}",
                        "output": result.stdout,
                        "stderr": result.stderr
                    }

            except subprocess.TimeoutExpired:
                return {
                    "success": False,
                    "error": f"Test command timed out: {command}"
                }

        return {"success": True, "message": "Tests completed successfully"}

    def _execute_analyze_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """Execute analysis step"""
        # This would integrate with static analyzers
        # For now, return a placeholder result
        return {
            "success": True,
            "message": "Analysis completed",
            "data": {
                "issues_found": 0,
                "warnings": 0,
                "errors": 0
            }
        }

    def _execute_fix_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """Execute fix step"""
        project_root = Path(context["project_root"])

        # Initialize fix controller
        fix_config = step.config
        fix_controller = FixController(fix_config)

        # Get issues from context (simplified)
        issues = context.get("steps", {}).get("analyze", {}).get("data", {}).get("issues", [])

        # Apply fixes
        fix_results = fix_controller.apply_fixes(project_root, issues)

        return {
            "success": fix_results["fixes_successful"] > 0,
            "message": f"Applied {fix_results['fixes_successful']} fixes",
            "data": fix_results
        }

    def _execute_report_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """Execute report step"""
        # This would integrate with report generators
        # For now, save context as JSON report
        report_path = Path(context["project_root"]) / "workflow_report.json"

        try:
            with open(report_path, 'w', encoding='utf-8') as f:
                json.dump(context, f, indent=2, default=str)

            return {
                "success": True,
                "message": f"Report saved to {report_path}",
                "data": {"report_path": str(report_path)}
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to save report: {str(e)}"
            }

    def _execute_custom_step(self, step: WorkflowStep, context: Dict[str, Any]) -> Dict[str, Any]:
        """Execute custom step"""
        script_path = step.config.get("script")
        if not script_path:
            return {"success": False, "error": "No script specified for custom step"}

        project_root = Path(context["project_root"])
        script_full_path = project_root / script_path

        if not script_full_path.exists():
            return {"success": False, "error": f"Script not found: {script_full_path}"}

        try:
            result = subprocess.run(
                ["python", str(script_full_path)],
                cwd=project_root,
                capture_output=True,
                text=True,
                timeout=step.timeout
            )

            return {
                "success": result.returncode == 0,
                "output": result.stdout,
                "stderr": result.stderr,
                "return_code": result.returncode
            }

        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Custom script timed out"}
        except Exception as e:
            return {"success": False, "error": f"Failed to execute custom script: {str(e)}"}

    def get_execution_history(self) -> List[Dict[str, Any]]:
        """Get history of workflow executions"""
        return self.execution_history

    def clear_history(self):
        """Clear execution history"""
        self.execution_history = []