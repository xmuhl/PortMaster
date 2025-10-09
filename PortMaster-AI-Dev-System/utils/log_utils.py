#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Logging Utilities

This module provides logging setup and utility functions
for consistent logging across the system.
"""

import logging
import logging.handlers
import sys
from pathlib import Path
from typing import Dict, Any, Optional


def setup_logging(
    config: Optional[Dict[str, Any]] = None,
    log_file: Optional[str] = None,
    log_level: str = "INFO",
    log_format: Optional[str] = None
) -> logging.Logger:
    """
    Setup logging configuration

    Args:
        config: Logging configuration dictionary
        log_file: Log file path (optional)
        log_level: Logging level
        log_format: Log format string

    Returns:
        Configured logger instance
    """
    # Use config if provided, otherwise use individual parameters
    if config:
        log_level = config.get('level', log_level)
        log_file = config.get('file', log_file)
        log_format = config.get('format', log_format)

    # Default log format
    if not log_format:
        log_format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

    # Convert string log level to logging constant
    numeric_level = getattr(logging, log_level.upper(), logging.INFO)

    # Create root logger
    logger = logging.getLogger()
    logger.setLevel(numeric_level)

    # Clear existing handlers
    logger.handlers.clear()

    # Create formatter
    formatter = logging.Formatter(log_format)

    # Console handler
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(numeric_level)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)

    # File handler
    if log_file:
        try:
            # Create directory if it doesn't exist
            log_path = Path(log_file)
            log_path.parent.mkdir(parents=True, exist_ok=True)

            # Create rotating file handler
            file_handler = logging.handlers.RotatingFileHandler(
                log_file,
                maxBytes=10 * 1024 * 1024,  # 10MB
                backupCount=5,
                encoding='utf-8'
            )
            file_handler.setLevel(logging.DEBUG)  # Always log DEBUG to file
            file_handler.setFormatter(formatter)
            logger.addHandler(file_handler)

        except Exception as e:
            print(f"Warning: Failed to set up file logging: {e}")

    return logger


def get_logger(name: str) -> logging.Logger:
    """
    Get a logger instance with the specified name

    Args:
        name: Logger name

    Returns:
        Logger instance
    """
    return logging.getLogger(name)


def set_log_level(logger: logging.Logger, level: str) -> None:
    """
    Set logging level for a logger

    Args:
        logger: Logger instance
        level: Logging level name
    """
    numeric_level = getattr(logging, level.upper(), logging.INFO)
    logger.setLevel(numeric_level)

    # Update handler levels
    for handler in logger.handlers:
        handler.setLevel(numeric_level)


def add_log_file(
    logger: logging.Logger,
    log_file: str,
    level: str = "DEBUG",
    max_bytes: int = 10 * 1024 * 1024,
    backup_count: int = 5
) -> bool:
    """
    Add a file handler to a logger

    Args:
        logger: Logger instance
        log_file: Log file path
        level: Logging level for file
        max_bytes: Maximum file size before rotation
        backup_count: Number of backup files to keep

    Returns:
        True if successful, False otherwise
    """
    try:
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)

        file_handler = logging.handlers.RotatingFileHandler(
            log_file,
            maxBytes=max_bytes,
            backupCount=backup_count,
            encoding='utf-8'
        )

        numeric_level = getattr(logging, level.upper(), logging.DEBUG)
        file_handler.setLevel(numeric_level)

        # Use same formatter as console
        if logger.handlers:
            console_handler = logger.handlers[0]
            file_handler.setFormatter(console_handler.formatter)
        else:
            formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
            file_handler.setFormatter(formatter)

        logger.addHandler(file_handler)
        return True

    except Exception as e:
        logger.error(f"Failed to add file handler: {e}")
        return False


def create_context_logger(name: str, context_data: Dict[str, Any]) -> logging.Logger:
    """
    Create a logger with additional context information

    Args:
        name: Logger name
        context_data: Context data to include in log messages

    Returns:
        Context-aware logger
    """
    logger = get_logger(name)

    # Create custom formatter that includes context
    class ContextFormatter(logging.Formatter):
        def format(self, record):
            # Add context data to record
            for key, value in context_data.items():
                setattr(record, f"ctx_{key}", value)
            return super().format(record)

    # Apply context formatter to all handlers
    for handler in logger.handlers:
        handler.setFormatter(ContextFormatter(handler.formatter._fmt))

    return logger


def log_function_call(func_name: str, args: tuple, kwargs: dict, result: Any) -> None:
    """
    Log a function call with its arguments and result

    Args:
        func_name: Function name
        args: Function arguments
        kwargs: Function keyword arguments
        result: Function return value
    """
    logger = get_logger("function_calls")

    args_str = ", ".join(str(arg) for arg in args)
    kwargs_str = ", ".join(f"{k}={v}" for k, v in kwargs.items())

    if kwargs_str:
        args_str += f", {kwargs_str}" if args_str else f"kwargs={kwargs_str}"

    logger.debug(f"Function call: {func_name}({args_str}) -> {result}")


def log_performance(
    operation: str,
    duration: float,
    details: Optional[Dict[str, Any]] = None
) -> None:
    """
    Log performance information

    Args:
        operation: Operation name
        duration: Duration in seconds
        details: Additional details
    """
    logger = get_logger("performance")

    message = f"Performance: {operation} took {duration:.3f}s"

    if details:
        details_str = ", ".join(f"{k}={v}" for k, v in details.items())
        message += f" ({details_str})"

    logger.info(message)


def log_error_with_context(
    error: Exception,
    context: Optional[Dict[str, Any]] = None,
    logger: Optional[logging.Logger] = None
) -> None:
    """
    Log an error with optional context information

    Args:
        error: Exception instance
        context: Context information
        logger: Logger instance (optional)
    """
    if logger is None:
        logger = get_logger("errors")

    message = f"Error: {type(error).__name__}: {str(error)}"

    if context:
        context_str = ", ".join(f"{k}={v}" for k, v in context.items())
        message += f" (Context: {context_str})"

    logger.error(message, exc_info=True)


def setup_debug_logging() -> logging.Logger:
    """
    Setup debug logging with verbose output

    Returns:
        Debug logger instance
    """
    return setup_logging(
        config={
            'level': 'DEBUG',
            'format': '%(asctime)s - %(name)s - %(levelname)s - %(filename)s:%(lineno)d - %(funcName)s() - %(message)s'
        }
    )


def setup_production_logging(log_file: str) -> logging.Logger:
    """
    Setup production logging with appropriate levels

    Args:
        log_file: Log file path

    Returns:
        Production logger instance
    """
    return setup_logging(
        config={
            'level': 'INFO',
            'file': log_file,
            'format': '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        }
    )


def log_system_info() -> None:
    """Log system information"""
    logger = get_logger("system")
    logger.info("System logging initialized")


def log_project_info(project_root: Path, config: Dict[str, Any]) -> None:
    """
    Log project information

    Args:
        project_root: Project root directory
        config: Configuration data
    """
    logger = get_logger("project")
    logger.info(f"Project root: {project_root}")
    logger.info(f"Configuration keys: {list(config.keys())}")


class LogCapture:
    """Context manager to capture log output"""

    def __init__(self, logger: Optional[logging.Logger] = None):
        """
        Initialize log capture

        Args:
            logger: Logger to capture (uses root logger if None)
        """
        self.logger = logger or logging.getLogger()
        self.handler = None
        self.records = []

    def __enter__(self):
        # Create custom handler to capture logs
        self.handler = logging.Handler()
        self.handler.emit = self._capture_record
        self.logger.addHandler(self.handler)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.handler:
            self.logger.removeHandler(self.handler)

    def _capture_record(self, record: logging.LogRecord):
        """Capture log record"""
        self.records.append(record)

    def get_logs(self, level: Optional[str] = None) -> List[str]:
        """
        Get captured logs

        Args:
            level: Minimum log level to include

        Returns:
            List of formatted log messages
        """
        if level:
            numeric_level = getattr(logging, level.upper(), logging.DEBUG)
        else:
            numeric_level = logging.DEBUG

        formatted_logs = []
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )

        for record in self.records:
            if record.levelno >= numeric_level:
                formatted_logs.append(formatter.format(record))

        return formatted_logs

    def clear_logs(self) -> None:
        """Clear captured logs"""
        self.records.clear()