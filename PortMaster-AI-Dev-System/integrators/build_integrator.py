#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - Build Integrator

This module provides integration with various build systems
for automated compilation and build verification.
"""

import os
import subprocess
import logging
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass

from ..utils.file_utils import find_files, read_file, write_file, ensure_directory
from ..utils.log_utils import get_logger


@dataclass
class BuildResult:
    """Build execution result"""
    success: bool
    exit_code: int
    output: str
    error_output: str
    duration: float
    build_artifacts: List[str]
    warnings_count: int
    errors_count: int


class BuildIntegrator:
    """
    Build system integrator supporting multiple build tools
    """

    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None):
        """
        Initialize build integrator

        Args:
            project_root: Project root directory
            config: Build configuration
        """
        self.project_root = project_root
        self.config = config or {}
        self.logger = get_logger(__name__)

        # Supported build systems
        self.build_systems = {
            'make': {
                'files': ['Makefile', 'makefile'],
                'commands': ['make', 'make clean', 'make install'],
                'patterns': ['*.c', '*.cpp', '*.h', '*.hpp']
            },
            'cmake': {
                'files': ['CMakeLists.txt'],
                'commands': [
                    'cmake -B build -S .',
                    'cmake --build build',
                    'cmake --build build --target clean'
                ],
                'patterns': ['*.c', '*.cpp', '*.h', '*.hpp', 'CMakeLists.txt']
            },
            'maven': {
                'files': ['pom.xml'],
                'commands': ['mvn clean compile', 'mvn package', 'mvn test'],
                'patterns': ['*.java', 'pom.xml']
            },
            'gradle': {
                'files': ['build.gradle', 'build.gradle.kts'],
                'commands': ['./gradlew build', './gradlew clean', './gradlew test'],
                'patterns': ['*.java', '*.kt', 'build.gradle', 'build.gradle.kts']
            },
            'npm': {
                'files': ['package.json'],
                'commands': ['npm install', 'npm run build', 'npm test'],
                'patterns': ['*.js', '*.ts', '*.jsx', '*.tsx', 'package.json']
            },
            'python': {
                'files': ['setup.py', 'pyproject.toml', 'requirements.txt'],
                'commands': ['python -m build', 'python -m pip install -e .', 'pytest'],
                'patterns': ['*.py', 'setup.py', 'pyproject.toml', 'requirements.txt']
            },
            'visual_studio': {
                'files': ['*.sln', '*.vcxproj'],
                'commands': [
                    'msbuild solution.sln /p:Configuration=Release',
                    'msbuild solution.sln /p:Configuration=Debug',
                    'msbuild solution.sln /t:Clean'
                ],
                'patterns': ['*.sln', '*.vcxproj', '*.c', '*.cpp', '*.h', '*.hpp']
            }
        }

    def detect_build_system(self) -> Optional[str]:
        """
        Detect the build system used in the project

        Returns:
            Detected build system name or None
        """
        for system_name, system_info in self.build_systems.items():
            for build_file in system_info['files']:
                # Handle glob patterns
                if '*' in build_file:
                    matches = list(self.project_root.glob(build_file))
                    if matches:
                        self.logger.info(f"Detected {system_name} build system (found {build_file})")
                        return system_name
                else:
                    if (self.project_root / build_file).exists():
                        self.logger.info(f"Detected {system_name} build system (found {build_file})")
                        return system_name

        # Try to detect based on source files
        source_extensions = {
            'make': ['.c', '.cpp', '.h'],
            'cmake': ['.c', '.cpp', '.h'],
            'maven': ['.java'],
            'gradle': ['.java', '.kt'],
            'npm': ['.js', '.ts'],
            'python': ['.py'],
            'visual_studio': ['.c', '.cpp', '.h']
        }

        for system_name, extensions in source_extensions.items():
            source_files = []
            for ext in extensions:
                source_files.extend(self.project_root.rglob(f'*{ext}'))

            if source_files and system_name in ['make', 'cmake']:
                # For C/C++ projects, prefer CMake if CMakeLists.txt exists
                if (self.project_root / 'CMakeLists.txt').exists():
                    return 'cmake'
                elif any((self.project_root / f).exists() for f in ['Makefile', 'makefile']):
                    return 'make'
                elif system_name == 'visual_studio' and list(self.project_root.glob('*.sln')):
                    return 'visual_studio'
            elif source_files:
                return system_name

        self.logger.warning("Could not detect build system")
        return None

    def get_build_commands(self, build_system: str, build_type: str = 'build') -> List[str]:
        """
        Get build commands for specific build system and type

        Args:
            build_system: Build system name
            build_type: Build type (build, clean, test, install)

        Returns:
            List of build commands
        """
        if build_system not in self.build_systems:
            raise ValueError(f"Unsupported build system: {build_system}")

        system_info = self.build_systems[build_system]
        commands = system_info['commands']

        # Map build types to commands
        build_type_map = {
            'make': {
                'build': [commands[0]],
                'clean': [commands[1]] if len(commands) > 1 else ['make clean'],
                'install': [commands[2]] if len(commands) > 2 else ['make install']
            },
            'cmake': {
                'build': [commands[0], commands[1]],
                'clean': [commands[2]] if len(commands) > 2 else ['cmake --build build --target clean']
            },
            'maven': {
                'build': [commands[0]],
                'test': [commands[2]] if len(commands) > 2 else ['mvn test'],
                'package': [commands[1]] if len(commands) > 1 else ['mvn package']
            },
            'gradle': {
                'build': [commands[0]],
                'clean': [commands[1]] if len(commands) > 1 else ['./gradlew clean'],
                'test': [commands[2]] if len(commands) > 2 else ['./gradlew test']
            },
            'npm': {
                'build': [commands[1]] if len(commands) > 1 else ['npm run build'],
                'clean': ['npm run clean'] if 'clean' in (read_file(self.project_root / 'package.json') or '') else ['rm -rf node_modules dist'],
                'test': [commands[2]] if len(commands) > 2 else ['npm test']
            },
            'python': {
                'build': [commands[0]] if len(commands) > 0 else ['python -m build'],
                'install': [commands[1]] if len(commands) > 1 else ['python -m pip install -e .'],
                'test': [commands[2]] if len(commands) > 2 else ['pytest']
            },
            'visual_studio': {
                'build': [commands[0]],
                'debug': [commands[1]] if len(commands) > 1 else ['msbuild solution.sln /p:Configuration=Debug'],
                'clean': [commands[2]] if len(commands) > 2 else ['msbuild solution.sln /t:Clean']
            }
        }

        if build_system in build_type_map and build_type in build_type_map[build_system]:
            return build_type_map[build_system][build_type]
        else:
            # Default to first command
            return [commands[0]] if commands else []

    def execute_build(self, build_system: Optional[str] = None, build_type: str = 'build',
                     timeout: int = 300) -> BuildResult:
        """
        Execute build process

        Args:
            build_system: Build system to use (auto-detect if None)
            build_type: Type of build to execute
            timeout: Build timeout in seconds

        Returns:
            Build execution result
        """
        start_time = time.time()

        if build_system is None:
            build_system = self.detect_build_system()
            if build_system is None:
                return BuildResult(
                    success=False,
                    exit_code=-1,
                    output="",
                    error_output="Could not detect build system",
                    duration=0,
                    build_artifacts=[],
                    warnings_count=0,
                    errors_count=1
                )

        try:
            commands = self.get_build_commands(build_system, build_type)
            self.logger.info(f"Executing {len(commands)} build command(s) for {build_system}")

            combined_output = []
            combined_error_output = []
            total_warnings = 0
            total_errors = 0
            final_exit_code = 0

            for i, command in enumerate(commands):
                self.logger.info(f"Build step {i+1}/{len(commands)}: {command}")

                try:
                    # Prepare command for execution
                    if build_system == 'visual_studio' and 'msbuild' in command:
                        # For Visual Studio, use full path if available
                        vs_path = self._find_visual_studio()
                        if vs_path:
                            command = command.replace('msbuild', f'"{vs_path}"')

                    # Execute command
                    result = subprocess.run(
                        command,
                        cwd=self.project_root,
                        shell=True,
                        capture_output=True,
                        text=True,
                        timeout=timeout
                    )

                    # Parse output for warnings and errors
                    warnings, errors = self._parse_build_output(result.stdout + result.stderr, build_system)
                    total_warnings += warnings
                    total_errors += errors

                    combined_output.append(f"=== Command: {command} ===")
                    combined_output.append(result.stdout)
                    combined_error_output.append(f"=== Command: {command} ===")
                    combined_error_output.append(result.stderr)

                    if result.returncode != 0:
                        final_exit_code = result.returncode
                        self.logger.error(f"Build step failed with exit code {result.returncode}")
                        break

                except subprocess.TimeoutExpired:
                    error_msg = f"Build command timed out after {timeout} seconds: {command}"
                    self.logger.error(error_msg)
                    combined_error_output.append(error_msg)
                    total_errors += 1
                    final_exit_code = 124  # Timeout exit code
                    break

            duration = time.time() - start_time
            success = final_exit_code == 0 and total_errors == 0

            # Find build artifacts
            build_artifacts = self._find_build_artifacts(build_system)

            return BuildResult(
                success=success,
                exit_code=final_exit_code,
                output='\n'.join(combined_output),
                error_output='\n'.join(combined_error_output),
                duration=duration,
                build_artifacts=build_artifacts,
                warnings_count=total_warnings,
                errors_count=total_errors
            )

        except Exception as e:
            self.logger.error(f"Build execution failed: {e}")
            duration = time.time() - start_time
            return BuildResult(
                success=False,
                exit_code=-1,
                output="",
                error_output=str(e),
                duration=duration,
                build_artifacts=[],
                warnings_count=0,
                errors_count=1
            )

    def _parse_build_output(self, output: str, build_system: str) -> Tuple[int, int]:
        """
        Parse build output to count warnings and errors

        Args:
            output: Build output text
            build_system: Build system name

        Returns:
            Tuple of (warnings_count, errors_count)
        """
        warnings = 0
        errors = 0

        # Common patterns
        warning_patterns = [
            r'warning:', r'WARNING:', r'Warning:', r'\[WARNING\]',
            r'\[Warn\]', r': warning', r'-W[^\s]+'
        ]

        error_patterns = [
            r'error:', r'ERROR:', r'Error:', r'\[ERROR\]',
            r'\[ERROR\]', r': error', r'failed', r'Failed'
        ]

        # Build system specific patterns
        if build_system == 'make':
            warning_patterns.extend([r':\d+:\s*warning:'])
            error_patterns.extend([r':\d+:\s*error:', r'make.*\*\*\*.*Error'])
        elif build_system == 'cmake':
            warning_patterns.extend([r'CMake Warning'])
            error_patterns.extend([r'CMake Error', r'-- Configuring incomplete, errors occurred'])
        elif build_system == 'maven':
            warning_patterns.extend([r'\[WARNING\]'])
            error_patterns.extend([r'\[ERROR\]', r'BUILD FAILURE'])
        elif build_system == 'gradle':
            warning_patterns.extend([r'warning:'])
            error_patterns.extend([r'FAILED', r'FAILURE:'])
        elif build_system == 'npm':
            warning_patterns.extend([r'npm WARN'])
            error_patterns.extend([r'npm ERR', r'Error:'])
        elif build_system == 'python':
            warning_patterns.extend([r'Warning:', r'warnings.warn'])
            error_patterns.extend([r'Error:', r'Exception:', r'Traceback'])
        elif build_system == 'visual_studio':
            warning_patterns.extend([r': warning [A-Z]\d+:'])
            error_patterns.extend([r': error [A-Z]\d+:', r': fatal error [A-Z]\d+:'])

        import re
        for line in output.split('\n'):
            for pattern in warning_patterns:
                if re.search(pattern, line, re.IGNORECASE):
                    warnings += 1
                    break
            else:
                for pattern in error_patterns:
                    if re.search(pattern, line, re.IGNORECASE):
                        errors += 1
                        break

        return warnings, errors

    def _find_build_artifacts(self, build_system: str) -> List[str]:
        """
        Find build artifacts for the given build system

        Args:
            build_system: Build system name

        Returns:
            List of build artifact paths
        """
        artifacts = []

        # Common artifact patterns by build system
        artifact_patterns = {
            'make': ['*.o', '*.exe', '*.out', '*.so', '*.dll', '*.a', '*.lib'],
            'cmake': ['build/**/*.exe', 'build/**/*.out', 'build/**/*.so', 'build/**/*.dll'],
            'maven': ['target/*.jar', 'target/*.war'],
            'gradle': ['build/libs/*.jar', 'build/distributions/*'],
            'npm': ['dist/**/*', 'build/**/*'],
            'python': ['build/**/*', 'dist/**/*'],
            'visual_studio': ['**/*.exe', '**/*.dll', '**/*.lib']
        }

        if build_system in artifact_patterns:
            for pattern in artifact_patterns[build_system]:
                artifacts.extend([str(p) for p in self.project_root.glob(pattern)])

        return [str(Path(artifact).relative_to(self.project_root)) for artifact in artifacts]

    def _find_visual_studio(self) -> Optional[str]:
        """
        Find Visual Studio MSBuild path

        Returns:
            Path to MSBuild executable or None
        """
        # Common VS installation paths
        vs_paths = [
            r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
            r"C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
            r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        ]

        for path in vs_paths:
            if Path(path).exists():
                return path

        return None

    def clean_build(self, build_system: Optional[str] = None) -> BuildResult:
        """
        Clean build artifacts

        Args:
            build_system: Build system to use (auto-detect if None)

        Returns:
            Clean operation result
        """
        self.logger.info("Starting clean build operation")
        return self.execute_build(build_system, 'clean', timeout=60)

    def verify_build_environment(self, build_system: str) -> Dict[str, Any]:
        """
        Verify build environment for specific build system

        Args:
            build_system: Build system to verify

        Returns:
            Environment verification results
        """
        verification = {
            'build_system': build_system,
            'tools_available': {},
            'dependencies_met': True,
            'issues': []
        }

        try:
            if build_system == 'make':
                # Check for make
                result = subprocess.run(['make', '--version'], capture_output=True, text=True)
                verification['tools_available']['make'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("make is not available")
                    verification['dependencies_met'] = False

            elif build_system == 'cmake':
                # Check for cmake
                result = subprocess.run(['cmake', '--version'], capture_output=True, text=True)
                verification['tools_available']['cmake'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("cmake is not available")
                    verification['dependencies_met'] = False

            elif build_system == 'maven':
                # Check for mvn
                result = subprocess.run(['mvn', '--version'], capture_output=True, text=True)
                verification['tools_available']['maven'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("Maven is not available")
                    verification['dependencies_met'] = False

            elif build_system == 'gradle':
                # Check for gradle or gradlew
                gradlew_path = self.project_root / 'gradlew'
                verification['tools_available']['gradlew'] = gradlew_path.exists() and os.access(gradlew_path, os.X_OK)

                if not verification['tools_available']['gradlew']:
                    result = subprocess.run(['gradle', '--version'], capture_output=True, text=True)
                    verification['tools_available']['gradle'] = result.returncode == 0

                    if result.returncode != 0:
                        verification['issues'].append("Neither gradlew nor gradle is available")
                        verification['dependencies_met'] = False

            elif build_system == 'npm':
                # Check for npm
                result = subprocess.run(['npm', '--version'], capture_output=True, text=True)
                verification['tools_available']['npm'] = result.returncode == 0
                if result.returncode != 0:
                    verification['issues'].append("npm is not available")
                    verification['dependencies_met'] = False

            elif build_system == 'python':
                # Check for python and build tools
                result = subprocess.run(['python', '--version'], capture_output=True, text=True)
                verification['tools_available']['python'] = result.returncode == 0

                # Check for build module
                result = subprocess.run(['python', '-m', 'build', '--help'], capture_output=True, text=True)
                verification['tools_available']['build'] = result.returncode == 0

                if not verification['tools_available']['python']:
                    verification['issues'].append("Python is not available")
                    verification['dependencies_met'] = False
                elif not verification['tools_available']['build']:
                    verification['issues'].append("Python build module is not available")

            elif build_system == 'visual_studio':
                # Check for MSBuild
                msbuild_path = self._find_visual_studio()
                verification['tools_available']['msbuild'] = msbuild_path is not None
                if msbuild_path is None:
                    verification['issues'].append("Visual Studio MSBuild is not available")
                    verification['dependencies_met'] = False

        except Exception as e:
            verification['issues'].append(f"Environment verification failed: {e}")
            verification['dependencies_met'] = False

        return verification

    def get_build_configuration(self) -> Dict[str, Any]:
        """
        Get current build configuration

        Returns:
            Build configuration dictionary
        """
        detected_system = self.detect_build_system()

        config = {
            'detected_build_system': detected_system,
            'supported_systems': list(self.build_systems.keys()),
            'project_root': str(self.project_root),
            'build_files': [],
            'source_files': []
        }

        if detected_system:
            system_info = self.build_systems[detected_system]
            config['build_files'] = [f for f in system_info['files'] if (self.project_root / f).exists()]
            config['available_commands'] = system_info['commands']

        # Find source files
        for extensions in self.build_systems.values():
            for pattern in extensions.get('patterns', []):
                if '*' in pattern:
                    config['source_files'].extend([str(p.relative_to(self.project_root)) for p in self.project_root.glob(pattern)])

        # Remove duplicates
        config['source_files'] = list(set(config['source_files']))

        return config