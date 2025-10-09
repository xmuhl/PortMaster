#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - File Utilities

This module provides utility functions for file operations,
including reading, writing, backup, and validation.
"""

import os
import json
import shutil
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any, Union


def find_files(
    directory: Path,
    patterns: Optional[List[str]] = None,
    exclude_patterns: Optional[List[str]] = None,
    recursive: bool = True
) -> List[Path]:
    """
    Find files matching patterns in a directory

    Args:
        directory: Directory to search
        patterns: List of file patterns (e.g., ['*.py', '*.java'])
        exclude_patterns: Patterns to exclude (e.g., ['*.pyc', '__pycache__'])
        recursive: Whether to search recursively

    Returns:
        List of matching file paths
    """
    if not directory.exists():
        return []

    files = []
    search_pattern = "**/*" if recursive else "*"

    for pattern in patterns or ["*"]:
        for file_path in directory.glob(search_pattern):
            if file_path.is_file():
                if file_path.match(pattern):
                    # Check exclude patterns
                    should_exclude = False
                    for exclude in (exclude_patterns or []):
                        if file_path.match(exclude):
                            should_exclude = True
                            break

                    if not should_exclude:
                        files.append(file_path)

    return files


def read_file(file_path: Path, encoding: str = 'utf-8') -> Optional[str]:
    """
    Read file content

    Args:
        file_path: Path to the file
        encoding: File encoding

    Returns:
        File content or None if failed
    """
    try:
        with open(file_path, 'r', encoding=encoding) as f:
            return f.read()
    except Exception as e:
        logging.warning(f"Failed to read file {file_path}: {e}")
        return None


def write_file(
    file_path: Path,
    content: str,
    encoding: str = 'utf-8',
    create_dirs: bool = True
) -> bool:
    """
    Write content to file

    Args:
        file_path: Path to the file
        content: Content to write
        encoding: File encoding
        create_dirs: Whether to create parent directories

    Returns:
        True if successful, False otherwise
    """
    try:
        if create_dirs:
            ensure_directory(file_path.parent)

        with open(file_path, 'w', encoding=encoding) as f:
            f.write(content)
        return True
    except Exception as e:
        logging.error(f"Failed to write file {file_path}: {e}")
        return False


def backup_file(file_path: Path, backup_suffix: str = '.backup') -> bool:
    """
    Create a backup of a file

    Args:
        file_path: Path to the file to backup
        backup_suffix: Suffix for backup file

    Returns:
        True if successful, False otherwise
    """
    if not file_path.exists():
        return False

    backup_path = file_path.with_suffix(file_path.suffix + backup_suffix)

    try:
        shutil.copy2(file_path, backup_path)
        logging.info(f"Created backup: {backup_path}")
        return True
    except Exception as e:
        logging.error(f"Failed to create backup for {file_path}: {e}")
        return False


def restore_file(file_path: Path, backup_suffix: str = '.backup') -> bool:
    """
    Restore a file from backup

    Args:
        file_path: Path to the file to restore
        backup_suffix: Suffix for backup file

    Returns:
        True if successful, False otherwise
    """
    backup_path = file_path.with_suffix(file_path.suffix + backup_suffix)

    if not backup_path.exists():
        return False

    try:
        shutil.copy2(backup_path, file_path)
        logging.info(f"Restored file from backup: {backup_path}")
        return True
    except Exception as e:
        logging.error(f"Failed to restore file from backup: {e}")
        return False


def ensure_directory(directory: Path) -> bool:
    """
    Ensure directory exists, create if necessary

    Args:
        directory: Directory path

    Returns:
        True if successful or already exists, False otherwise
    """
    try:
        directory.mkdir(parents=True, exist_ok=True)
        return True
    except Exception as e:
        logging.error(f"Failed to create directory {directory}: {e}")
        return False


def save_json(data: Dict[str, Any], file_path: Path, indent: int = 2) -> bool:
    """
    Save data to JSON file

    Args:
        data: Data to save
        file_path: Output file path
        indent: JSON indentation

    Returns:
        True if successful, False otherwise
    """
    try:
        ensure_directory(file_path.parent)

        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=indent, default=str)
        return True
    except Exception as e:
        logging.error(f"Failed to save JSON to {file_path}: {e}")
        return False


def load_json(file_path: Path) -> Optional[Dict[str, Any]]:
    """
    Load data from JSON file

    Args:
        file_path: Input file path

    Returns:
        Loaded data or None if failed
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception as e:
        logging.warning(f"Failed to load JSON from {file_path}: {e}")
        return None


def get_file_size(file_path: Path) -> int:
    """
    Get file size in bytes

    Args:
        file_path: Path to the file

    Returns:
        File size in bytes
    """
    try:
        return file_path.stat().st_size
    except OSError:
        return 0


def is_text_file(file_path: Path, sample_size: int = 1024) -> bool:
    """
    Check if file is likely a text file

    Args:
        file_path: Path to the file
        sample_size: Number of bytes to sample

    Returns:
        True if likely text file, False otherwise
    """
    try:
        with open(file_path, 'rb') as f:
            sample = f.read(sample_size)

        # Check for null bytes (binary files)
        if b'\x00' in sample:
            return False

        # Try to decode as text
        try:
            sample.decode('utf-8')
            return True
        except UnicodeDecodeError:
            return False
    except (OSError, IOError):
        return False


def get_relative_path(file_path: Path, base_path: Path) -> Path:
    """
    Get relative path from base path

    Args:
        file_path: File path
        base_path: Base path

    Returns:
        Relative path
    """
    try:
        return file_path.relative_to(base_path)
    except ValueError:
        return file_path


def copy_files(
    source_dir: Path,
    dest_dir: Path,
    patterns: Optional[List[str]] = None,
    preserve_structure: bool = True
) -> bool:
    """
    Copy files matching patterns from source to destination

    Args:
        source_dir: Source directory
        dest_dir: Destination directory
        patterns: File patterns to match
        preserve_structure: Whether to preserve directory structure

    Returns:
        True if successful, False otherwise
    """
    try:
        if not source_dir.exists():
            logging.error(f"Source directory does not exist: {source_dir}")
            return False

        ensure_directory(dest_dir)

        files = find_files(source_dir, patterns)

        for file_path in files:
            if preserve_structure:
                rel_path = get_relative_path(file_path, source_dir)
                dest_file = dest_dir / rel_path
            else:
                dest_file = dest_dir / file_path.name

            ensure_directory(dest_file.parent)
            shutil.copy2(file_path, dest_file)

        logging.info(f"Copied {len(files)} files from {source_dir} to {dest_dir}")
        return True

    except Exception as e:
        logging.error(f"Failed to copy files: {e}")
        return False


def clean_directory(
    directory: Path,
    patterns: Optional[List[str]] = None,
    recursive: bool = True,
    dry_run: bool = False
) -> List[Path]:
    """
    Clean directory by removing files matching patterns

    Args:
        directory: Directory to clean
        patterns: File patterns to remove
        recursive: Whether to clean recursively
        dry_run: Whether to only simulate removal

    Returns:
        List of removed file paths
    """
    if not directory.exists():
        return []

    files = find_files(directory, patterns, recursive=recursive)
    removed_files = []

    for file_path in files:
        try:
            if not dry_run:
                file_path.unlink()
            logging.info(f"{'Would remove' if dry_run else 'Removed'}: {file_path}")
            removed_files.append(file_path)
        except Exception as e:
            logging.warning(f"Failed to remove {file_path}: {e}")

    return removed_files


def calculate_directory_size(directory: Path) -> int:
    """
    Calculate total size of directory recursively

    Args:
        directory: Directory path

    Returns:
        Total size in bytes
    """
    total_size = 0

    try:
        for item in directory.rglob('*'):
            if item.is_file():
                total_size += item.stat().st_size
    except OSError:
        pass

    return total_size


def format_file_size(size_bytes: int) -> str:
    """
    Format file size in human-readable format

    Args:
        size_bytes: Size in bytes

    Returns:
        Formatted size string
    """
    if size_bytes == 0:
        return "0 B"

    size_names = ["B", "KB", "MB", "GB", "TB"]
    i = 0

    while size_bytes >= 1024 and i < len(size_names) - 1:
        size_bytes /= 1024.0
        i += 1

    return f"{size_bytes:.1f} {size_names[i]}"


def validate_file_path(file_path: Path, must_exist: bool = True) -> bool:
    """
    Validate file path

    Args:
        file_path: File path to validate
        must_exist: Whether file must exist

    Returns:
        True if valid, False otherwise
    """
    if must_exist and not file_path.exists():
        return False

    try:
        # Check if path is valid
        file_path.resolve()
        return True
    except (OSError, ValueError):
        return False


def get_file_hash(file_path: Path, algorithm: str = 'md5') -> Optional[str]:
    """
    Calculate file hash

    Args:
        file_path: Path to file
        algorithm: Hash algorithm ('md5', 'sha1', 'sha256')

    Returns:
        File hash as hex string or None if failed
    """
    import hashlib

    try:
        hash_obj = hashlib.new(algorithm)

        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_obj.update(chunk)

        return hash_obj.hexdigest()
    except Exception as e:
        logging.error(f"Failed to calculate hash for {file_path}: {e}")
        return None