#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Git Utilities

This module provides Git operations utilities for version control,
branch management, and repository analysis.
"""

import os
import subprocess
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from datetime import datetime


class GitManager:
    """
    Git repository manager with common operations
    """

    def __init__(self, repo_path: Optional[Path] = None):
        """
        Initialize Git manager

        Args:
            repo_path: Path to Git repository (defaults to current directory)
        """
        self.repo_path = repo_path or Path.cwd()
        self.logger = get_logger(__name__)

        # Verify it's a Git repository
        if not self._is_git_repository():
            raise ValueError(f"Not a Git repository: {self.repo_path}")

    def _is_git_repository(self) -> bool:
        """Check if directory is a Git repository"""
        git_dir = self.repo_path / '.git'
        return git_dir.exists() and git_dir.is_dir()

    def _run_git_command(self, command: List[str], capture_output: bool = True) -> subprocess.CompletedProcess:
        """
        Run Git command

        Args:
            command: Git command and arguments
            capture_output: Whether to capture output

        Returns:
            subprocess.CompletedProcess
        """
        try:
            env = os.environ.copy()
            env['GIT_DIR'] = str(self.repo_path / '.git')
            env['GIT_WORK_TREE'] = str(self.repo_path)

            result = subprocess.run(
                ['git'] + command,
                cwd=self.repo_path,
                capture_output=capture_output,
                text=True,
                env=env,
                timeout=30
            )

            if result.returncode != 0 and capture_output:
                self.logger.warning(f"Git command failed: {' '.join(command)} - {result.stderr}")

            return result

        except subprocess.TimeoutExpired:
            self.logger.error(f"Git command timeout: {' '.join(command)}")
            raise
        except Exception as e:
            self.logger.error(f"Failed to run git command: {e}")
            raise

    def get_status(self) -> Dict[str, Any]:
        """
        Get repository status

        Returns:
            Repository status information
        """
        try:
            # Get git status
            result = self._run_git_command(['status', '--porcelain'])
            status_lines = result.stdout.strip().split('\n') if result.stdout.strip() else []

            # Parse status
            staged_files = []
            modified_files = []
            untracked_files = []
            deleted_files = []

            for line in status_lines:
                if not line:
                    continue

                status_code = line[:2]
                file_path = line[3:]

                if status_code[0] in ['A', 'M', 'D']:
                    staged_files.append(file_path)
                if status_code[1] in ['M', 'D']:
                    modified_files.append(file_path)
                if status_code == '??':
                    untracked_files.append(file_path)
                if status_code[1] == 'D':
                    deleted_files.append(file_path)

            # Get current branch
            branch_result = self._run_git_command(['branch', '--show-current'])
            current_branch = branch_result.stdout.strip() if branch_result.returncode == 0 else 'unknown'

            # Get remote information
            remote_result = self._run_git_command(['remote', '-v'])
            remotes = {}
            if remote_result.returncode == 0:
                for line in remote_result.stdout.strip().split('\n'):
                    if line:
                        parts = line.split()
                        if len(parts) >= 2:
                            remote_name = parts[0]
                            remote_url = parts[1]
                            if remote_name not in remotes:
                                remotes[remote_name] = []
                            remotes[remote_name].append(remote_url)

            return {
                'current_branch': current_branch,
                'staged_files': staged_files,
                'modified_files': modified_files,
                'untracked_files': untracked_files,
                'deleted_files': deleted_files,
                'remotes': remotes,
                'is_clean': len(staged_files) == 0 and len(modified_files) == 0 and len(untracked_files) == 0
            }

        except Exception as e:
            self.logger.error(f"Failed to get git status: {e}")
            return {
                'current_branch': 'unknown',
                'staged_files': [],
                'modified_files': [],
                'untracked_files': [],
                'deleted_files': [],
                'remotes': {},
                'is_clean': False,
                'error': str(e)
            }

    def get_commit_history(self, limit: int = 10) -> List[Dict[str, Any]]:
        """
        Get commit history

        Args:
            limit: Maximum number of commits to retrieve

        Returns:
            List of commit information
        """
        try:
            format_string = '%H|%an|%ad|%s|%b'
            result = self._run_git_command([
                'log',
                f'--pretty=format:{format_string}',
                f'--max-count={limit}',
                '--date=iso'
            ])

            if result.returncode != 0:
                return []

            commits = []
            for line in result.stdout.strip().split('\n\n'):
                if not line:
                    continue

                parts = line.split('\n')
                if not parts[0]:
                    continue

                # Parse commit header
                header_parts = parts[0].split('|')
                if len(header_parts) >= 4:
                    commit_info = {
                        'hash': header_parts[0],
                        'author': header_parts[1],
                        'date': header_parts[2],
                        'subject': header_parts[3],
                        'body': '\n'.join(parts[1:]) if len(parts) > 1 else ''
                    }
                    commits.append(commit_info)

            return commits

        except Exception as e:
            self.logger.error(f"Failed to get commit history: {e}")
            return []

    def get_diff(self, commit_hash: Optional[str] = None, file_path: Optional[Path] = None) -> str:
        """
        Get git diff

        Args:
            commit_hash: Specific commit hash (None for working directory changes)
            file_path: Specific file to diff (None for all files)

        Returns:
            Diff output
        """
        try:
            command = ['diff']

            if commit_hash:
                command.append(commit_hash)

            if file_path:
                command.append('--')
                command.append(str(file_path))

            result = self._run_git_command(command)

            if result.returncode == 0:
                return result.stdout
            else:
                self.logger.warning(f"Git diff failed: {result.stderr}")
                return ""

        except Exception as e:
            self.logger.error(f"Failed to get git diff: {e}")
            return ""

    def add_files(self, file_paths: List[Path]) -> bool:
        """
        Add files to staging area

        Args:
            file_paths: List of files to add

        Returns:
            True if successful, False otherwise
        """
        try:
            for file_path in file_paths:
                result = self._run_git_command(['add', str(file_path)])
                if result.returncode != 0:
                    self.logger.error(f"Failed to add file {file_path}: {result.stderr}")
                    return False

            self.logger.info(f"Added {len(file_paths)} files to staging area")
            return True

        except Exception as e:
            self.logger.error(f"Failed to add files: {e}")
            return False

    def commit_changes(self, message: str, allow_empty: bool = False) -> bool:
        """
        Commit staged changes

        Args:
            message: Commit message
            allow_empty: Allow empty commits

        Returns:
            True if successful, False otherwise
        """
        try:
            command = ['commit', '-m', message]
            if allow_empty:
                command.append('--allow-empty')

            result = self._run_git_command(command)

            if result.returncode == 0:
                self.logger.info(f"Successfully committed changes: {message}")
                return True
            else:
                self.logger.error(f"Failed to commit changes: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to commit changes: {e}")
            return False

    def create_branch(self, branch_name: str, checkout: bool = True) -> bool:
        """
        Create new branch

        Args:
            branch_name: Name of new branch
            checkout: Whether to checkout the new branch

        Returns:
            True if successful, False otherwise
        """
        try:
            command = ['branch', branch_name]
            if checkout:
                command = ['checkout', '-b', branch_name]

            result = self._run_git_command(command)

            if result.returncode == 0:
                action = "created and checked out" if checkout else "created"
                self.logger.info(f"Successfully {action} branch: {branch_name}")
                return True
            else:
                self.logger.error(f"Failed to create branch {branch_name}: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to create branch: {e}")
            return False

    def switch_branch(self, branch_name: str) -> bool:
        """
        Switch to branch

        Args:
            branch_name: Name of branch to switch to

        Returns:
            True if successful, False otherwise
        """
        try:
            result = self._run_git_command(['checkout', branch_name])

            if result.returncode == 0:
                self.logger.info(f"Successfully switched to branch: {branch_name}")
                return True
            else:
                self.logger.error(f"Failed to switch to branch {branch_name}: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to switch branch: {e}")
            return False

    def merge_branch(self, branch_name: str, no_commit: bool = False) -> bool:
        """
        Merge branch into current branch

        Args:
            branch_name: Name of branch to merge
            no_commit: Whether to create a merge commit

        Returns:
            True if successful, False otherwise
        """
        try:
            command = ['merge']
            if no_commit:
                command.append('--no-commit')
            command.append(branch_name)

            result = self._run_git_command(command)

            if result.returncode == 0:
                self.logger.info(f"Successfully merged branch: {branch_name}")
                return True
            else:
                self.logger.error(f"Failed to merge branch {branch_name}: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to merge branch: {e}")
            return False

    def push_to_remote(self, remote_name: str = 'origin', branch_name: Optional[str] = None) -> bool:
        """
        Push commits to remote repository

        Args:
            remote_name: Name of remote repository
            branch_name: Name of branch to push (None for current branch)

        Returns:
            True if successful, False otherwise
        """
        try:
            if branch_name is None:
                status = self.get_status()
                branch_name = status['current_branch']

            command = ['push', remote_name, branch_name]
            result = self._run_git_command(command)

            if result.returncode == 0:
                self.logger.info(f"Successfully pushed to {remote_name}/{branch_name}")
                return True
            else:
                self.logger.error(f"Failed to push to {remote_name}/{branch_name}: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to push to remote: {e}")
            return False

    def pull_from_remote(self, remote_name: str = 'origin', branch_name: Optional[str] = None) -> bool:
        """
        Pull changes from remote repository

        Args:
            remote_name: Name of remote repository
            branch_name: Name of branch to pull (None for current branch)

        Returns:
            True if successful, False otherwise
        """
        try:
            if branch_name is None:
                status = self.get_status()
                branch_name = status['current_branch']

            command = ['pull', remote_name, branch_name]
            result = self._run_git_command(command)

            if result.returncode == 0:
                self.logger.info(f"Successfully pulled from {remote_name}/{branch_name}")
                return True
            else:
                self.logger.error(f"Failed to pull from {remote_name}/{branch_name}: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to pull from remote: {e}")
            return False

    def create_tag(self, tag_name: str, message: Optional[str] = None) -> bool:
        """
        Create tag

        Args:
            tag_name: Name of tag
            message: Tag message (None for lightweight tag)

        Returns:
            True if successful, False otherwise
        """
        try:
            command = ['tag', tag_name]
            if message:
                command.extend(['-m', message])

            result = self._run_git_command(command)

            if result.returncode == 0:
                self.logger.info(f"Successfully created tag: {tag_name}")
                return True
            else:
                self.logger.error(f"Failed to create tag {tag_name}: {result.stderr}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to create tag: {e}")
            return False

    def get_repository_info(self) -> Dict[str, Any]:
        """
        Get comprehensive repository information

        Returns:
            Repository information dictionary
        """
        try:
            info = {
                'path': str(self.repo_path),
                'is_git_repo': self._is_git_repository()
            }

            if not info['is_git_repo']:
                return info

            # Get basic info
            status = self.get_status()
            info.update(status)

            # Get commit count
            result = self._run_git_command(['rev-list', '--count', 'HEAD'])
            if result.returncode == 0:
                info['total_commits'] = int(result.stdout.strip())

            # Get latest commit
            latest = self.get_commit_history(limit=1)
            if latest:
                info['latest_commit'] = latest[0]

            # Get branch list
            branch_result = self._run_git_command(['branch', '--format=%(refname:short)'])
            if branch_result.returncode == 0:
                info['branches'] = [b.strip() for b in branch_result.stdout.strip().split('\n') if b.strip()]

            # Get tag list
            tag_result = self._run_git_command(['tag', '--sort=-version:refname'])
            if tag_result.returncode == 0:
                info['tags'] = [t.strip() for t in tag_result.stdout.strip().split('\n') if t.strip()]

            return info

        except Exception as e:
            self.logger.error(f"Failed to get repository info: {e}")
            return {'path': str(self.repo_path), 'error': str(e)}

    def backup_repository(self, backup_dir: Path) -> bool:
        """
        Create backup of repository

        Args:
            backup_dir: Directory to store backup

        Returns:
            True if successful, False otherwise
        """
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            repo_name = self.repo_path.name
            backup_path = backup_dir / f"{repo_name}_backup_{timestamp}"

            # Create backup directory
            backup_path.mkdir(parents=True, exist_ok=True)

            # Copy repository files
            import shutil
            for item in self.repo_path.iterdir():
                if item.name != '.git':  # Don't copy .git directory
                    if item.is_file():
                        shutil.copy2(item, backup_path / item.name)
                    else:
                        shutil.copytree(item, backup_path / item.name, dirs_exist_ok=True)

            self.logger.info(f"Repository backed up to: {backup_path}")
            return True

        except Exception as e:
            self.logger.error(f"Failed to backup repository: {e}")
            return False


def get_logger(name: str) -> logging.Logger:
    """Get logger instance"""
    return logging.getLogger(name)


def is_git_repository(path: Path) -> bool:
    """
    Check if directory is a Git repository

    Args:
        path: Directory path to check

    Returns:
        True if Git repository, False otherwise
    """
    git_dir = path / '.git'
    return git_dir.exists() and git_dir.is_dir()


def initialize_repository(path: Path, init_config: Optional[Dict[str, str]] = None) -> bool:
    """
    Initialize new Git repository

    Args:
        path: Directory path to initialize
        init_config: Initial configuration (user.name, user.email)

    Returns:
        True if successful, False otherwise
    """
    try:
        env = os.environ.copy()
        if init_config:
            if 'user.name' in init_config:
                env['GIT_AUTHOR_NAME'] = init_config['user.name']
                env['GIT_COMMITTER_NAME'] = init_config['user.name']
            if 'user.email' in init_config:
                env['GIT_AUTHOR_EMAIL'] = init_config['user.email']
                env['GIT_COMMITTER_EMAIL'] = init_config['user.email']

        result = subprocess.run(
            ['git', 'init'],
            cwd=path,
            capture_output=True,
            text=True,
            env=env,
            timeout=30
        )

        if result.returncode == 0:
            logging.info(f"Successfully initialized Git repository: {path}")
            return True
        else:
            logging.error(f"Failed to initialize repository: {result.stderr}")
            return False

    except Exception as e:
        logging.error(f"Failed to initialize repository: {e}")
        return False


def clone_repository(url: str, target_path: Path, branch: Optional[str] = None) -> bool:
    """
    Clone Git repository

    Args:
        url: Repository URL
        target_path: Target directory path
        branch: Specific branch to clone

    Returns:
        True if successful, False otherwise
    """
    try:
        command = ['clone', url, str(target_path)]
        if branch:
            command.extend(['--branch', branch])

        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=300  # 5 minutes timeout
        )

        if result.returncode == 0:
            logging.info(f"Successfully cloned repository to: {target_path}")
            return True
        else:
            logging.error(f"Failed to clone repository: {result.stderr}")
            return False

    except Exception as e:
        logging.error(f"Failed to clone repository: {e}")
        return False