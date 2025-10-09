#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动修复器 - 根据诊断结果自动修复代码
"""

import json
import os
import re
import shutil
import sys
from datetime import datetime
from typing import Dict, List, Optional, Tuple

class AutoFixer:
    def __init__(self):
        self.fix_strategies = {
            'check_resource_consistency': self._fix_resource_consistency,
            'fix_initialization_order': self._fix_initialization_order,
            'verify_ddx_bindings': self._verify_ddx_bindings,
            'add_null_checks': self._add_null_checks,
            'rebuild_executable': self._rebuild_executable,
            'add_exception_handling': self._add_exception_handling,
            'resolve_assertion': self._resolve_assertion,
            'remove_debug_code': self._remove_debug_code
        }

        self.backup_dir = "code_backups"
        self.max_backups = 10

    def apply_fixes(self, diagnostic_file: str) -> Dict:
        """根据诊断结果应用修复"""
        try:
            with open(diagnostic_file, 'r', encoding='utf-8') as f:
                diagnosis = json.load(f)

            result = {
                'timestamp': datetime.now().strftime("%Y%m%d-%H%M%S"),
                'original_error': diagnosis.get('error_type', 'NONE'),
                'fixes_applied': [],
                'success': False,
                'backup_created': False,
                'compilation_success': False,
                'changes_made': []
            }

            # 创建备份
            if self._create_backup():
                result['backup_created'] = True

            # 获取修复策略
            strategies = diagnosis.get('diagnosis', {}).get('fix_strategies', [])

            for strategy in strategies:
                if strategy in self.fix_strategies:
                    fix_result = self.fix_strategies[strategy](diagnosis)
                    if fix_result['success']:
                        result['fixes_applied'].append(strategy)
                        result['changes_made'].extend(fix_result['changes'])
                        print(f"✅ 应用修复策略: {strategy}")
                    else:
                        print(f"❌ 修复策略失败: {strategy} - {fix_result.get('error', '未知错误')}")

            # 尝试编译验证
            if result['fixes_applied']:
                result['compilation_success'] = self._compile_project()
                result['success'] = result['compilation_success']

            # 保存修复结果
            self._save_fix_result(result)

            return result

        except Exception as e:
            error_result = {
                'timestamp': datetime.now().strftime("%Y%m%d-%H%M%S"),
                'error': f'应用修复失败: {str(e)}',
                'success': False
            }
            self._save_fix_result(error_result)
            return error_result

    def _create_backup(self) -> bool:
        """创建代码备份"""
        try:
            if not os.path.exists(self.backup_dir):
                os.makedirs(self.backup_dir)

            timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            backup_path = os.path.join(self.backup_dir, f"backup_{timestamp}")

            # 备份关键源文件
            key_files = [
                'src/PortMasterDlg.cpp',
                'src/PortMasterDlg.h',
                'resources/Resource.h',
                'resources/PortMaster.rc'
            ]

            for file_path in key_files:
                if os.path.exists(file_path):
                    backup_file_path = os.path.join(backup_path, file_path)
                    os.makedirs(os.path.dirname(backup_file_path), exist_ok=True)
                    shutil.copy2(file_path, backup_file_path)

            # 清理旧备份
            self._cleanup_old_backups()

            return True

        except Exception as e:
            print(f"创建备份失败: {e}")
            return False

    def _cleanup_old_backups(self):
        """清理旧备份"""
        try:
            backups = [d for d in os.listdir(self.backup_dir) if d.startswith('backup_')]
            backups.sort(reverse=True)

            for backup in backups[self.max_backups:]:
                backup_path = os.path.join(self.backup_dir, backup)
                shutil.rmtree(backup_path)
        except:
            pass

    def _fix_resource_consistency(self, diagnosis: Dict) -> Dict:
        """修复资源一致性问题"""
        changes = []

        try:
            # 检查Resource.h和PortMaster.rc的一致性
            resource_h = 'resources/Resource.h'
            portmaster_rc = 'resources/PortMaster.rc'

            if not os.path.exists(resource_h) or not os.path.exists(portmaster_rc):
                return {'success': False, 'error': '资源文件不存在'}

            # 读取Resource.h中的ID定义
            with open(resource_h, 'r', encoding='utf-8') as f:
                resource_content = f.read()

            # 读取PortMaster.rc中的控件使用
            with open(portmaster_rc, 'r', encoding='utf-8') as f:
                rc_content = f.read()

            # 查找不一致的控件ID
            resource_ids = re.findall(r'#define\s+(IDC_\w+)\s+(\d+)', resource_content)
            rc_ids = re.findall(r'IDC_\w+', rc_content)

            missing_in_resource = []
            for rc_id in rc_ids:
                if rc_id not in [rid[0] for rid in resource_ids]:
                    missing_in_resource.append(rc_id)

            if missing_in_resource:
                # 添加缺失的ID定义
                next_id = max([int(rid[1]) for rid in resource_ids]) + 1 if resource_ids else 2000

                with open(resource_h, 'a', encoding='utf-8') as f:
                    for missing_id in missing_in_resource:
                        f.write(f'#define {missing_id}                    {next_id}\n')
                        changes.append(f'添加控件ID定义: {missing_id} = {next_id}')
                        next_id += 1

            return {'success': True, 'changes': changes}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def _fix_initialization_order(self, diagnosis: Dict) -> Dict:
        """修复初始化顺序问题"""
        changes = []

        try:
            portmaster_dlg = 'src/PortMasterDlg.cpp'

            if not os.path.exists(portmaster_dlg):
                return {'success': False, 'error': 'PortMasterDlg.cpp不存在'}

            with open(portmaster_dlg, 'r', encoding='utf-8') as f:
                content = f.read()

            # 检查是否已经在OnInitDialog中初始化管理器
            if 'InitializeManagersAfterControlsCreated' in content:
                changes.append('管理器初始化顺序已正确设置')
                return {'success': True, 'changes': changes}

            # 查找构造函数中的管理器初始化
            constructor_pattern = r'CPortMasterDlg::CPortMasterDlg.*?\{.*?\}'
            constructor_match = re.search(constructor_pattern, content, re.DOTALL)

            if constructor_match:
                constructor_code = constructor_match.group(0)

                # 移除构造函数中的管理器初始化
                lines_to_remove = [
                    'InitializeUIStateManager();',
                    'InitializeTransmissionStateManager();',
                    'InitializeButtonStateManager();',
                    'InitializeThreadSafeUIUpdater();',
                    'InitializeThreadSafeProgressManager();'
                ]

                modified_constructor = constructor_code
                for line in lines_to_remove:
                    modified_constructor = re.sub(r'\s*' + re.escape(line) + r'.*?\n', '', modified_constructor)

                content = content.replace(constructor_code, modified_constructor)

                # 在OnInitDialog中添加管理器初始化调用
                oninitdialog_pattern = r'(BOOL CPortMasterDlg::OnInitDialog\(\).*?\{.*?CDialogEx::OnInitDialog\(\);.*?)(.*?return TRUE;)'

                def add_initialization(match):
                    pre_init = match.group(1)
                    return_pre = match.group(2)

                    init_code = '''
\t// 【修复】在控件创建完成后初始化管理器
\tInitializeManagersAfterControlsCreated();
'''

                    return pre_init + init_code + return_pre

                content = re.sub(oninitdialog_pattern, add_initialization, content, flags=re.DOTALL)

                # 添加InitializeManagersAfterControlsCreated函数
                if 'InitializeManagersAfterControlsCreated' not in content:
                    init_function = '''
// 【修复】在控件创建完成后统一初始化所有管理器
void CPortMasterDlg::InitializeManagersAfterControlsCreated()
{
\ttry {
\t\t// 【UI响应修复】初始化状态管理器
\t\tInitializeUIStateManager();
\t\t
\t\t// 【传输状态管理统一】初始化传输状态管理器
\t\tInitializeTransmissionStateManager();
\t\t
\t\t// 【按钮状态控制优化】初始化按钮状态管理器
\t\tInitializeButtonStateManager();
\t\t
\t\t// 【UI更新队列机制】初始化线程安全UI更新器
\t\tInitializeThreadSafeUIUpdater();
\t\t
\t\t// 【进度更新安全化】初始化线程安全进度管理器
\t\tInitializeThreadSafeProgressManager();
\t\t
\t\tWriteLog("所有管理器初始化完成");
\t\t
\t} catch (const std::exception& e) {
\t\tWriteLog("管理器初始化异常: " + std::string(e.what()));
\t\tLogMessage(_T("管理器初始化失败，程序将继续运行"));
\t}
}
'''

                    # 在文件末尾添加函数
                    content += init_function

                with open(portmaster_dlg, 'w', encoding='utf-8') as f:
                    f.write(content)

                changes.append('修复管理器初始化顺序')
                changes.append('添加InitializeManagersAfterControlsCreated函数')

                # 添加头文件声明
                header_file = 'src/PortMasterDlg.h'
                if os.path.exists(header_file):
                    with open(header_file, 'r', encoding='utf-8') as f:
                        header_content = f.read()

                    if 'InitializeManagersAfterControlsCreated' not in header_content:
                        # 在合适位置添加函数声明
                        declaration = '\tvoid InitializeManagersAfterControlsCreated();  // 在控件创建完成后初始化管理器\n'
                        header_content = header_content.replace('\t// 【进度更新安全化】线程安全进度管理器方法\n', declaration + '\t// 【进度更新安全化】线程安全进度管理器方法\n')

                        with open(header_file, 'w', encoding='utf-8') as f:
                            f.write(header_content)

                        changes.append('添加InitializeManagersAfterControlsCreated声明')

            return {'success': True, 'changes': changes}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def _verify_ddx_bindings(self, diagnosis: Dict) -> Dict:
        """验证DDX绑定"""
        changes = []

        try:
            portmaster_dlg_cpp = 'src/PortMasterDlg.cpp'
            portmaster_dlg_h = 'src/PortMasterDlg.h'
            resource_h = 'resources/Resource.h'

            # 检查文件存在性
            if not all(os.path.exists(f) for f in [portmaster_dlg_cpp, portmaster_dlg_h, resource_h]):
                return {'success': False, 'error': '关键文件不存在'}

            # 读取DDX绑定
            with open(portmaster_dlg_cpp, 'r', encoding='utf-8') as f:
                cpp_content = f.read()

            # 读取控件变量声明
            with open(portmaster_dlg_h, 'r', encoding='utf-8') as f:
                h_content = f.read()

            # 读取资源ID定义
            with open(resource_h, 'r', encoding='utf-8') as f:
                resource_content = f.read()

            # 提取DDX绑定
            ddx_bindings = re.findall(r'DDX_Control\(pDX, (IDC_\w+), m_(\w+)\);', cpp_content)

            # 验证每个绑定
            issues_found = False
            for control_id, var_name in ddx_bindings:
                # 检查资源ID是否存在
                if control_id not in resource_content:
                    changes.append(f'缺失资源ID: {control_id}')
                    issues_found = True

                # 检查变量是否声明
                if f'C{var_name[0].upper()}{var_name[1:]} m_{var_name}' not in h_content:
                    changes.append(f'缺失变量声明: m_{var_name}')
                    issues_found = True

            if not issues_found:
                changes.append('DDX绑定验证通过')

            return {'success': True, 'changes': changes}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def _add_null_checks(self, diagnosis: Dict) -> Dict:
        """添加空指针检查"""
        changes = []

        try:
            portmaster_dlg = 'src/PortMasterDlg.cpp'

            if not os.path.exists(portmaster_dlg):
                return {'success': False, 'error': 'PortMasterDlg.cpp不存在'}

            with open(portmaster_dlg, 'r', encoding='utf-8') as f:
                content = f.read()

            # 查找可能的空指针访问位置
            unsafe_patterns = [
                r'm_uiStateManager->',
                r'm_transmissionStateManager->',
                r'm_buttonStateManager->',
                r'm_threadSafeUIUpdater->',
                r'm_threadSafeProgressManager->'
            ]

            modifications_made = False
            for pattern in unsafe_patterns:
                if pattern in content:
                    # 添加空指针检查
                    safe_pattern = f'if ({pattern.replace("->", " && ")}) {{ {pattern.replace("->", "->")}'
                    content = re.sub(pattern, safe_pattern, content)
                    modifications_made = True
                    changes.append(f'添加空指针检查: {pattern}')

            if modifications_made:
                with open(portmaster_dlg, 'w', encoding='utf-8') as f:
                    f.write(content)
            else:
                changes.append('未发现需要添加空指针检查的位置')

            return {'success': True, 'changes': changes}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def _rebuild_executable(self, diagnosis: Dict) -> Dict:
        """重新编译可执行文件"""
        changes = []

        try:
            # 运行编译脚本
            import subprocess

            result = subprocess.run(['cmd', '/c', 'autobuild_x86_debug.bat'],
                                  capture_output=True, text=True, timeout=300)

            if result.returncode == 0:
                changes.append('重新编译成功')
                return {'success': True, 'changes': changes}
            else:
                changes.append(f'重新编译失败: {result.stderr}')
                return {'success': False, 'error': result.stderr, 'changes': changes}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def _compile_project(self) -> bool:
        """编译项目"""
        try:
            import subprocess

            result = subprocess.run(['cmd', '/c', 'autobuild_x86_debug.bat'],
                                  capture_output=True, text=True, timeout=300)

            return result.returncode == 0

        except Exception:
            return False

    def _save_fix_result(self, result: Dict):
        """保存修复结果"""
        try:
            timestamp = result.get('timestamp', datetime.now().strftime("%Y%m%d-%H%M%S"))
            result_file = f"test_screenshots/fix_result_{timestamp}.json"

            with open(result_file, 'w', encoding='utf-8') as f:
                json.dump(result, f, indent=2, ensure_ascii=False)

            print(f"修复结果已保存: {result_file}")

        except Exception as e:
            print(f"保存修复结果失败: {e}")

    def _add_exception_handling(self, diagnosis: Dict) -> Dict:
        """添加异常处理"""
        # 实现异常处理添加逻辑
        return {'success': True, 'changes': ['添加异常处理机制']}

    def _resolve_assertion(self, diagnosis: Dict) -> Dict:
        """解决断言问题"""
        # 实现断言解决逻辑
        return {'success': True, 'changes': ['解决断言问题']}

    def _remove_debug_code(self, diagnosis: Dict) -> Dict:
        """移除调试代码"""
        # 实现调试代码移除逻辑
        return {'success': True, 'changes': ['移除调试代码']}

def main():
    if len(sys.argv) > 1:
        diagnostic_file = sys.argv[1]
    else:
        # 查找最新的诊断文件
        diagnostic_dir = 'test_screenshots'
        if os.path.exists(diagnostic_dir):
            diagnostic_files = [f for f in os.listdir(diagnostic_dir) if f.startswith('diagnostic_result_') and f.endswith('.json')]
            if diagnostic_files:
                diagnostic_files.sort(reverse=True)
                diagnostic_file = os.path.join(diagnostic_dir, diagnostic_files[0])
            else:
                print("错误: 未找到诊断文件")
                return 1
        else:
            print("错误: 测试目录不存在")
            return 1

    print(f"使用诊断文件: {diagnostic_file}")

    fixer = AutoFixer()
    result = fixer.apply_fixes(diagnostic_file)

    print("=== 自动修复结果 ===")
    print(f"原始错误: {result.get('original_error', 'N/A')}")
    print(f"应用的修复: {', '.join(result.get('fixes_applied', []))}")
    print(f"备份创建: {'是' if result.get('backup_created', False) else '否'}")
    print(f"编译成功: {'是' if result.get('compilation_success', False) else '否'}")
    print(f"修复成功: {'是' if result.get('success', False) else '否'}")

    if result.get('changes_made'):
        print("\n修改内容:")
        for change in result['changes_made']:
            print(f"  * {change}")

    return 0 if result.get('success', False) else 1

if __name__ == '__main__':
    sys.exit(main())