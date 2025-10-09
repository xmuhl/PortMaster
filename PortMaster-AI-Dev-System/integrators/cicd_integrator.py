#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - CI/CD Integrator

This module provides integration with CI/CD platforms
for automated build, test, and deployment workflows.
"""

import os
import json
import yaml
import logging
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass

from ..utils.file_utils import write_file, ensure_directory, read_file
from ..utils.log_utils import get_logger


@dataclass
class PipelineResult:
    """Pipeline execution result"""
    platform: str
    pipeline_name: str
    status: str  # success, failed, running, cancelled
    duration: float
    stages: List[Dict[str, Any]]
    artifacts: List[str]
    logs: str
    error_message: Optional[str] = None


class CICDIntegrator:
    """
    CI/CD platform integrator supporting multiple platforms
    """

    def __init__(self, project_root: Path, config: Optional[Dict[str, Any]] = None):
        """
        Initialize CI/CD integrator

        Args:
            project_root: Project root directory
            config: CI/CD configuration
        """
        self.project_root = project_root
        self.config = config or {}
        self.logger = get_logger(__name__)

        # Supported CI/CD platforms
        self.platforms = {
            'github_actions': {
                'config_files': ['.github/workflows/*.yml', '.github/workflows/*.yaml'],
                'api_url': 'https://api.github.com',
                'webhook_url': 'https://github.com'
            },
            'gitlab_ci': {
                'config_files': ['.gitlab-ci.yml'],
                'api_url': 'https://gitlab.com/api/v4',
                'webhook_url': 'https://gitlab.com'
            },
            'jenkins': {
                'config_files': ['Jenkinsfile', 'jenkins.yml'],
                'api_url': 'http://localhost:8080',
                'webhook_url': 'http://localhost:8080'
            },
            'azure_pipelines': {
                'config_files': ['azure-pipelines.yml', '.azure/pipelines/*.yml'],
                'api_url': 'https://dev.azure.com',
                'webhook_url': 'https://dev.azure.com'
            },
            'circleci': {
                'config_files': ['.circleci/config.yml'],
                'api_url': 'https://circleci.com/api/v2',
                'webhook_url': 'https://circleci.com'
            },
            'travis_ci': {
                'config_files': ['.travis.yml'],
                'api_url': 'https://api.travis-ci.com',
                'webhook_url': 'https://travis-ci.com'
            }
        }

    def detect_ci_platform(self) -> Optional[str]:
        """
        Detect the CI/CD platform used in the project

        Returns:
            Detected platform name or None
        """
        for platform_name, platform_info in self.platforms.items():
            for config_pattern in platform_info['config_files']:
                if list(self.project_root.glob(config_pattern)):
                    self.logger.info(f"Detected {platform_name} CI/CD platform (found {config_pattern})")
                    return platform_name

        # Check for environment variables (useful when running in CI)
        env_vars = {
            'github_actions': ['GITHUB_ACTIONS', 'GITHUB_RUN_ID'],
            'gitlab_ci': ['GITLAB_CI', 'CI_PROJECT_ID'],
            'jenkins': ['JENKINS_URL', 'BUILD_NUMBER'],
            'azure_pipelines': ['TF_BUILD', 'BUILD_BUILDID'],
            'circleci': ['CIRCLECI', 'CIRCLE_BUILD_NUM'],
            'travis_ci': ['TRAVIS', 'TRAVIS_BUILD_NUMBER']
        }

        for platform_name, vars_list in env_vars.items():
            if any(os.environ.get(var) for var in vars_list):
                self.logger.info(f"Detected {platform_name} CI/CD platform from environment")
                return platform_name

        self.logger.warning("Could not detect CI/CD platform")
        return None

    def create_pipeline_config(self, platform: str, pipeline_config: Dict[str, Any]) -> bool:
        """
        Create pipeline configuration file

        Args:
            platform: CI/CD platform name
            pipeline_config: Pipeline configuration

        Returns:
            True if successful, False otherwise
        """
        try:
            if platform == 'github_actions':
                return self._create_github_actions_config(pipeline_config)
            elif platform == 'gitlab_ci':
                return self._create_gitlab_ci_config(pipeline_config)
            elif platform == 'jenkins':
                return self._create_jenkins_config(pipeline_config)
            elif platform == 'azure_pipelines':
                return self._create_azure_pipelines_config(pipeline_config)
            elif platform == 'circleci':
                return self._create_circleci_config(pipeline_config)
            elif platform == 'travis_ci':
                return self._create_travis_ci_config(pipeline_config)
            else:
                self.logger.error(f"Unsupported platform: {platform}")
                return False

        except Exception as e:
            self.logger.error(f"Failed to create pipeline config: {e}")
            return False

    def _create_github_actions_config(self, config: Dict[str, Any]) -> bool:
        """Create GitHub Actions workflow configuration"""
        workflow_dir = self.project_root / '.github' / 'workflows'
        ensure_directory(workflow_dir)

        workflow_name = config.get('name', 'ci-cd')
        workflow_file = workflow_dir / f'{workflow_name}.yml'

        workflow = {
            'name': config.get('display_name', 'CI/CD Pipeline'),
            'on': {
                'push': {
                    'branches': config.get('branches', ['main', 'master', 'develop'])
                },
                'pull_request': {
                    'branches': config.get('branches', ['main', 'master'])
                }
            },
            'jobs': {}
        }

        # Add build job
        if config.get('build', {}).get('enabled', True):
            workflow['jobs']['build'] = {
                'runs-on': config.get('build', {}).get('runner', 'ubuntu-latest'),
                'steps': [
                    {'uses': 'actions/checkout@v3'},
                    {
                        'name': 'Setup Build Environment',
                        'run': self._generate_build_steps(config.get('build', {}))
                    }
                ]
            }

        # Add test job
        if config.get('test', {}).get('enabled', True):
            workflow['jobs']['test'] = {
                'runs-on': config.get('test', {}).get('runner', 'ubuntu-latest'),
                'needs': ['build'],
                'steps': [
                    {'uses': 'actions/checkout@v3'},
                    {
                        'name': 'Setup Test Environment',
                        'run': self._generate_test_steps(config.get('test', {}))
                    }
                ]
            }

        # Add deploy job
        if config.get('deploy', {}).get('enabled', False):
            workflow['jobs']['deploy'] = {
                'runs-on': config.get('deploy', {}).get('runner', 'ubuntu-latest'),
                'needs': ['test'],
                'if': "github.ref == 'refs/heads/main'",
                'steps': [
                    {'uses': 'actions/checkout@v3'},
                    {
                        'name': 'Deploy',
                        'run': self._generate_deploy_steps(config.get('deploy', {}))
                    }
                ]
            }

        yaml_content = yaml.dump(workflow, default_flow_style=False, indent=2)
        return write_file(workflow_file, yaml_content)

    def _create_gitlab_ci_config(self, config: Dict[str, Any]) -> bool:
        """Create GitLab CI configuration"""
        gitlab_ci_file = self.project_root / '.gitlab-ci.yml'

        gitlab_ci = {
            'stages': [],
            'variables': config.get('variables', {})
        }

        # Add build stage
        if config.get('build', {}).get('enabled', True):
            gitlab_ci['stages'].append('build')
            gitlab_ci['build'] = {
                'stage': 'build',
                'script': self._generate_build_steps(config.get('build', {})),
                'artifacts': {
                    'paths': config.get('build', {}).get('artifacts', ['build/', 'dist/']),
                    'expire_in': '1 week'
                }
            }

        # Add test stage
        if config.get('test', {}).get('enabled', True):
            gitlab_ci['stages'].append('test')
            gitlab_ci['test'] = {
                'stage': 'test',
                'script': self._generate_test_steps(config.get('test', {})),
                'artifacts': {
                    'reports': {
                        'junit': config.get('test', {}).get('reports', ['test-results.xml'])
                    }
                }
            }

        # Add deploy stage
        if config.get('deploy', {}).get('enabled', False):
            gitlab_ci['stages'].append('deploy')
            gitlab_ci['deploy'] = {
                'stage': 'deploy',
                'script': self._generate_deploy_steps(config.get('deploy', {})),
                'only': ['main', 'master'],
                'when': 'manual'
            }

        yaml_content = yaml.dump(gitlab_ci, default_flow_style=False, indent=2)
        return write_file(gitlab_ci_file, yaml_content)

    def _create_jenkins_config(self, config: Dict[str, Any]) -> bool:
        """Create Jenkins pipeline configuration"""
        jenkinsfile = self.project_root / 'Jenkinsfile'

        pipeline_script = "pipeline {\n"
        pipeline_script += f"    agent any\n\n"

        stages = []

        if config.get('build', {}).get('enabled', True):
            stages.append("        stage('Build') {\n            steps {\n")
            stages.append(f"                sh '''\n{self._generate_build_steps(config.get('build', {}))}\n'''")
            stages.append("            }\n        }")

        if config.get('test', {}).get('enabled', True):
            stages.append("        stage('Test') {\n            steps {\n")
            stages.append(f"                sh '''\n{self._generate_test_steps(config.get('test', {}))}\n'''")
            stages.append("            }\n        }")

        if config.get('deploy', {}).get('enabled', False):
            stages.append("        stage('Deploy') {\n            steps {\n")
            stages.append(f"                sh '''\n{self._generate_deploy_steps(config.get('deploy', {}))}\n'''")
            stages.append("            }\n        }")

        pipeline_script += "    stages {\n" + "\n".join(stages) + "\n    }\n"
        pipeline_script += "}\n"

        return write_file(jenkinsfile, pipeline_script)

    def _create_azure_pipelines_config(self, config: Dict[str, Any]) -> bool:
        """Create Azure Pipelines configuration"""
        azure_pipelines_file = self.project_root / 'azure-pipelines.yml'

        pipeline = {
            'trigger': {
                'branches': {
                    'include': config.get('branches', ['main', 'master', 'develop'])
                }
            },
            'pool': {
                'vmImage': config.get('vm_image', 'ubuntu-latest')
            },
            'variables': config.get('variables', {}),
            'stages': []
        }

        # Build stage
        if config.get('build', {}).get('enabled', True):
            build_stage = {
                'stage': 'Build',
                'jobs': [{
                    'job': 'Build',
                    'steps': [
                        {
                            'task': 'UsePythonVersion@0',
                            'inputs': {
                                'versionSpec': config.get('python_version', '3.9')
                            }
                        },
                        {
                            'script': self._generate_build_steps(config.get('build', {})),
                            'displayName': 'Build'
                        }
                    ]
                }]
            }
            pipeline['stages'].append(build_stage)

        # Test stage
        if config.get('test', {}).get('enabled', True):
            test_stage = {
                'stage': 'Test',
                'dependsOn': 'Build',
                'jobs': [{
                    'job': 'Test',
                    'steps': [
                        {
                            'task': 'UsePythonVersion@0',
                            'inputs': {
                                'versionSpec': config.get('python_version', '3.9')
                            }
                        },
                        {
                            'script': self._generate_test_steps(config.get('test', {})),
                            'displayName': 'Test'
                        }
                    ]
                }]
            }
            pipeline['stages'].append(test_stage)

        yaml_content = yaml.dump(pipeline, default_flow_style=False, indent=2)
        return write_file(azure_pipelines_file, yaml_content)

    def _create_circleci_config(self, config: Dict[str, Any]) -> bool:
        """Create CircleCI configuration"""
        circleci_dir = self.project_root / '.circleci'
        ensure_directory(circleci_dir)

        config_file = circleci_dir / 'config.yml'

        circleci_config = {
            'version': 2.1,
            'jobs': {},
            'workflows': {
                'version': 2,
                'build_and_test': {
                    'jobs': []
                }
            }
        }

        # Build job
        if config.get('build', {}).get('enabled', True):
            circleci_config['jobs']['build'] = {
                'docker': [{'image': config.get('docker_image', 'cimg/python:3.9')}],
                'steps': [
                    'checkout',
                    {
                        'run': {
                            'name': 'Build',
                            'command': self._generate_build_steps(config.get('build', {}))
                        }
                    }
                ]
            }
            circleci_config['workflows']['build_and_test']['jobs'].append('build')

        # Test job
        if config.get('test', {}).get('enabled', True):
            circleci_config['jobs']['test'] = {
                'docker': [{'image': config.get('docker_image', 'cimg/python:3.9')}],
                'steps': [
                    'checkout',
                    {
                        'run': {
                            'name': 'Test',
                            'command': self._generate_test_steps(config.get('test', {}))
                        }
                    }
                ]
            }
            circleci_config['workflows']['build_and_test']['jobs'].append('test')

        yaml_content = yaml.dump(circleci_config, default_flow_style=False, indent=2)
        return write_file(config_file, yaml_content)

    def _create_travis_ci_config(self, config: Dict[str, Any]) -> bool:
        """Create Travis CI configuration"""
        travis_file = self.project_root / '.travis.yml'

        travis_config = {
            'language': config.get('language', 'python'),
            'python': config.get('python_version', ['3.9']),
            'script': []
        }

        # Add build steps
        if config.get('build', {}).get('enabled', True):
            build_steps = self._generate_build_steps(config.get('build', {}))
            travis_config['script'].extend(build_steps.split('\n'))

        # Add test steps
        if config.get('test', {}).get('enabled', True):
            test_steps = self._generate_test_steps(config.get('test', {}))
            travis_config['script'].extend(test_steps.split('\n'))

        yaml_content = yaml.dump(travis_config, default_flow_style=False, indent=2)
        return write_file(travis_file, yaml_content)

    def _generate_build_steps(self, build_config: Dict[str, Any]) -> str:
        """Generate build steps for pipeline"""
        steps = []

        # Install dependencies
        if build_config.get('dependencies'):
            for dep in build_config['dependencies']:
                if dep.get('type') == 'apt':
                    steps.append(f"sudo apt-get install -y {dep['package']}")
                elif dep.get('type') == 'pip':
                    steps.append(f"pip install {dep['package']}")
                elif dep.get('type') == 'npm':
                    steps.append(f"npm install {dep['package']}")

        # Setup environment
        if build_config.get('setup_commands'):
            steps.extend(build_config['setup_commands'])

        # Build commands
        if build_config.get('commands'):
            steps.extend(build_config['commands'])

        return '\n'.join(steps) if steps else 'echo "No build steps configured"'

    def _generate_test_steps(self, test_config: Dict[str, Any]) -> str:
        """Generate test steps for pipeline"""
        steps = []

        # Install test dependencies
        if test_config.get('dependencies'):
            for dep in test_config['dependencies']:
                steps.append(f"pip install {dep}")

        # Setup test environment
        if test_config.get('setup_commands'):
            steps.extend(test_config['setup_commands'])

        # Test commands
        if test_config.get('commands'):
            steps.extend(test_config['commands'])
        else:
            # Default test commands based on framework
            framework = test_config.get('framework', 'pytest')
            if framework == 'pytest':
                steps.append('pytest --junit-xml=test-results.xml')
            elif framework == 'unittest':
                steps.append('python -m unittest discover -v')
            elif framework == 'jest':
                steps.append('npm test')

        return '\n'.join(steps) if steps else 'echo "No test steps configured"'

    def _generate_deploy_steps(self, deploy_config: Dict[str, Any]) -> str:
        """Generate deploy steps for pipeline"""
        steps = []

        # Deploy commands
        if deploy_config.get('commands'):
            steps.extend(deploy_config['commands'])
        else:
            steps.append('echo "No deploy steps configured"')

        return '\n'.join(steps)

    def get_pipeline_status(self, platform: str, pipeline_id: Optional[str] = None) -> Dict[str, Any]:
        """
        Get pipeline status from CI/CD platform

        Args:
            platform: CI/CD platform name
            pipeline_id: Pipeline ID (optional)

        Returns:
            Pipeline status information
        """
        # This is a simplified implementation
        # In practice, you would make HTTP requests to the platform's API
        status = {
            'platform': platform,
            'status': 'unknown',
            'pipeline_id': pipeline_id,
            'last_updated': time.time(),
            'stages': [],
            'artifacts': []
        }

        if platform == 'github_actions':
            # Check GitHub Actions environment variables
            if os.environ.get('GITHUB_ACTIONS'):
                status.update({
                    'status': 'running',
                    'run_id': os.environ.get('GITHUB_RUN_ID'),
                    'repository': os.environ.get('GITHUB_REPOSITORY'),
                    'branch': os.environ.get('GITHUB_REF', '').replace('refs/heads/', ''),
                    'workflow': os.environ.get('GITHUB_WORKFLOW')
                })

        elif platform == 'gitlab_ci':
            # Check GitLab CI environment variables
            if os.environ.get('GITLAB_CI'):
                status.update({
                    'status': 'running',
                    'pipeline_id': os.environ.get('CI_PIPELINE_ID'),
                    'project_id': os.environ.get('CI_PROJECT_ID'),
                    'branch': os.environ.get('CI_COMMIT_BRANCH'),
                    'job_name': os.environ.get('CI_JOB_NAME')
                })

        return status

    def trigger_pipeline(self, platform: str, branch: str = 'main',
                        parameters: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        Trigger pipeline execution

        Args:
            platform: CI/CD platform name
            branch: Branch to trigger pipeline for
            parameters: Additional parameters

        Returns:
            Trigger result information
        """
        # This is a simplified implementation
        # In practice, you would make HTTP requests to trigger the pipeline
        result = {
            'platform': platform,
            'status': 'triggered',
            'branch': branch,
            'pipeline_id': f"pipeline_{int(time.time())}",
            'triggered_at': time.time(),
            'parameters': parameters or {}
        }

        self.logger.info(f"Triggered {platform} pipeline for branch {branch}")
        return result

    def get_ci_configuration(self) -> Dict[str, Any]:
        """
        Get current CI/CD configuration

        Returns:
            CI/CD configuration dictionary
        """
        detected_platform = self.detect_ci_platform()

        config = {
            'detected_platform': detected_platform,
            'supported_platforms': list(self.platforms.keys()),
            'project_root': str(self.project_root),
            'configuration_files': [],
            'environment_info': {}
        }

        # Find configuration files
        if detected_platform:
            platform_info = self.platforms[detected_platform]
            for pattern in platform_info['config_files']:
                config['configuration_files'].extend([str(p.relative_to(self.project_root)) for p in self.project_root.glob(pattern)])

        # Get environment information
        env_vars = {
            'github_actions': ['GITHUB_ACTIONS', 'GITHUB_RUN_ID', 'GITHUB_REPOSITORY'],
            'gitlab_ci': ['GITLAB_CI', 'CI_PIPELINE_ID', 'CI_PROJECT_ID'],
            'jenkins': ['JENKINS_URL', 'BUILD_NUMBER'],
            'azure_pipelines': ['TF_BUILD', 'BUILD_BUILDID'],
            'circleci': ['CIRCLECI', 'CIRCLE_BUILD_NUM'],
            'travis_ci': ['TRAVIS', 'TRAVIS_BUILD_NUMBER']
        }

        if detected_platform and detected_platform in env_vars:
            for var in env_vars[detected_platform]:
                value = os.environ.get(var)
                if value:
                    config['environment_info'][var] = value

        return config

    def create_default_pipeline(self, platform: str, project_type: str = 'python') -> bool:
        """
        Create default pipeline configuration for a project type

        Args:
            platform: CI/CD platform name
            project_type: Type of project (python, nodejs, java, cpp)

        Returns:
            True if successful, False otherwise
        """
        default_configs = {
            'python': {
                'name': 'ci-cd',
                'display_name': 'Python CI/CD Pipeline',
                'branches': ['main', 'develop'],
                'variables': {},
                'build': {
                    'enabled': True,
                    'runner': 'ubuntu-latest',
                    'dependencies': [
                        {'type': 'pip', 'package': 'pytest'},
                        {'type': 'pip', 'package': 'pytest-cov'}
                    ],
                    'commands': [
                        'pip install -r requirements.txt',
                        'python -m build'
                    ],
                    'artifacts': ['dist/']
                },
                'test': {
                    'enabled': True,
                    'framework': 'pytest',
                    'runner': 'ubuntu-latest',
                    'commands': [
                        'pytest --junit-xml=test-results.xml --cov=. --cov-report=xml'
                    ],
                    'reports': ['test-results.xml']
                },
                'deploy': {
                    'enabled': False,
                    'commands': []
                }
            },
            'nodejs': {
                'name': 'ci-cd',
                'display_name': 'Node.js CI/CD Pipeline',
                'branches': ['main', 'develop'],
                'variables': {},
                'build': {
                    'enabled': True,
                    'runner': 'ubuntu-latest',
                    'dependencies': [],
                    'commands': [
                        'npm ci',
                        'npm run build'
                    ],
                    'artifacts': ['dist/']
                },
                'test': {
                    'enabled': True,
                    'framework': 'jest',
                    'runner': 'ubuntu-latest',
                    'commands': [
                        'npm test -- --coverage --outputFile=coverage.json'
                    ],
                    'reports': ['coverage.json']
                },
                'deploy': {
                    'enabled': False,
                    'commands': []
                }
            }
        }

        if project_type in default_configs:
            return self.create_pipeline_config(platform, default_configs[project_type])
        else:
            self.logger.error(f"Unsupported project type: {project_type}")
            return False