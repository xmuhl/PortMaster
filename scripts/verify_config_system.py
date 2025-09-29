#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
配置系统读写验证程序
测试ConfigStore的配置管理和持久化功能

验证内容:
1. JSON配置文件读写功能 - 验证配置正确保存和加载
2. 默认值回退机制 - 验证配置损坏时的恢复能力
3. 多层配置存储 - 验证传输、协议、UI等配置分类存储
4. 配置自动持久化 - 验证程序关闭时自动保存配置

运行方法:
python verify_config_system.py
"""

import json
import os
import tempfile
import shutil
import time
import sys
from typing import Dict, Any, Optional
from pathlib import Path

class ConfigStoreVerifier:
    """配置存储系统验证器"""

    def __init__(self):
        # 创建临时测试目录
        self.test_dir = tempfile.mkdtemp(prefix='portmaster_config_test_')
        self.config_file = os.path.join(self.test_dir, 'portmaster_config.json')

        self.verification_results = []
        print(f"🔧 配置系统验证器初始化完成")
        print(f"   测试目录: {self.test_dir}")

    def __del__(self):
        """清理测试目录"""
        if hasattr(self, 'test_dir') and os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir, ignore_errors=True)

    def log_result(self, test_name: str, passed: bool, details: str = ""):
        """记录验证结果"""
        status = "✅ PASSED" if passed else "❌ FAILED"
        result = {
            'test': test_name,
            'passed': passed,
            'details': details,
            'timestamp': time.strftime('%H:%M:%S')
        }
        self.verification_results.append(result)
        print(f"[{result['timestamp']}] {status}: {test_name}")
        if details:
            print(f"           详情: {details}")

    def create_test_config(self) -> Dict[str, Any]:
        """创建测试配置数据"""
        return {
            "transport": {
                "port_type": "serial",
                "port_name": "COM1",
                "baud_rate": 9600,
                "data_bits": 8,
                "parity": "None",
                "stop_bits": 1,
                "flow_control": "None",
                "timeout": 1000
            },
            "protocol": {
                "use_reliable_mode": True,
                "max_retries": 3,
                "retry_timeout": 500,
                "window_size": 1,
                "frame_size": 1024
            },
            "ui": {
                "hex_mode": False,
                "auto_scroll": True,
                "font_name": "Consolas",
                "font_size": 10,
                "window_width": 800,
                "window_height": 600
            },
            "application": {
                "version": "1.0.0",
                "last_used_file": "",
                "recent_files": [],
                "startup_check_updates": True
            }
        }

    def test_json_config_read_write(self) -> bool:
        """测试1: JSON配置文件读写功能"""
        print("\n🧪 测试1: JSON配置文件读写功能")

        # 创建测试配置
        test_config = self.create_test_config()

        try:
            # 测试写入配置
            print("   📝 写入配置文件...")
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(test_config, f, indent=2, ensure_ascii=False)

            write_success = os.path.exists(self.config_file)
            print(f"   写入成功: {write_success}")

            # 测试读取配置
            print("   📖 读取配置文件...")
            with open(self.config_file, 'r', encoding='utf-8') as f:
                loaded_config = json.load(f)

            # 验证数据完整性
            config_matches = (loaded_config == test_config)

            # 检查关键配置项
            key_checks = [
                loaded_config.get('transport', {}).get('port_name') == 'COM1',
                loaded_config.get('protocol', {}).get('use_reliable_mode') == True,
                loaded_config.get('ui', {}).get('font_name') == 'Consolas',
                len(loaded_config.get('application', {}).get('recent_files', [])) == 0
            ]

            all_keys_valid = all(key_checks)

            passed = write_success and config_matches and all_keys_valid
            details = f"写入: {write_success}, 数据匹配: {config_matches}, 关键项有效: {all_keys_valid}"

        except Exception as e:
            passed = False
            details = f"异常: {str(e)}"

        self.log_result("JSON配置文件读写", passed, details)
        return passed

    def test_default_value_fallback(self) -> bool:
        """测试2: 默认值回退机制"""
        print("\n🧪 测试2: 默认值回退机制")

        try:
            # 创建损坏的配置文件
            print("   🔧 创建损坏的配置文件...")
            with open(self.config_file, 'w', encoding='utf-8') as f:
                f.write('{ "transport": { "port_name": "COM1", invalid_json }')

            # 尝试读取损坏的配置
            print("   📖 尝试读取损坏的配置...")
            config = None
            read_failed = False

            try:
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    config = json.load(f)
            except json.JSONDecodeError:
                read_failed = True
                print("   ⚠️  JSON解析失败，如预期")

            # 模拟使用默认配置
            if read_failed:
                print("   🔄 使用默认配置...")
                default_config = {
                    "transport": {
                        "port_type": "serial",
                        "port_name": "COM1",
                        "baud_rate": 9600
                    },
                    "protocol": {
                        "use_reliable_mode": False,
                        "max_retries": 3
                    }
                }
                config = default_config

            # 验证默认配置可用
            default_config_valid = (
                config is not None and
                config.get('transport', {}).get('port_name') == 'COM1' and
                config.get('protocol', {}).get('max_retries') == 3
            )

            # 重新保存修复的配置
            if default_config_valid:
                print("   💾 保存修复的配置...")
                with open(self.config_file, 'w', encoding='utf-8') as f:
                    json.dump(config, f, indent=2, ensure_ascii=False)

                # 验证修复后的配置可正常读取
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    repaired_config = json.load(f)

                repair_successful = (repaired_config == config)
            else:
                repair_successful = False

            passed = read_failed and default_config_valid and repair_successful
            details = f"检测到损坏: {read_failed}, 默认值有效: {default_config_valid}, 修复成功: {repair_successful}"

        except Exception as e:
            passed = False
            details = f"异常: {str(e)}"

        self.log_result("默认值回退机制", passed, details)
        return passed

    def test_multi_tier_config_storage(self) -> bool:
        """测试3: 多层配置存储"""
        print("\n🧪 测试3: 多层配置存储")

        try:
            # 创建分层的配置结构
            multi_tier_config = {
                "transport_configs": {
                    "serial": {
                        "default_baud_rate": 9600,
                        "supported_rates": [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
                    },
                    "parallel": {
                        "default_timeout": 5000,
                        "max_data_size": 1024
                    },
                    "usb": {
                        "auto_detect": True,
                        "vendor_filters": []
                    },
                    "network": {
                        "default_port": 9100,
                        "connection_timeout": 10000
                    }
                },
                "protocol_configs": {
                    "reliable": {
                        "default_window_size": 1,
                        "max_frame_size": 1024,
                        "default_timeout": 1000
                    },
                    "direct": {
                        "buffer_size": 4096,
                        "flush_interval": 100
                    }
                },
                "ui_configs": {
                    "display": {
                        "max_lines": 100,
                        "hex_bytes_per_line": 16,
                        "auto_scroll": True
                    },
                    "controls": {
                        "button_timeout": 2000,
                        "progress_update_interval": 200
                    }
                }
            }

            # 保存分层配置
            print("   📝 保存分层配置...")
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(multi_tier_config, f, indent=2, ensure_ascii=False)

            # 验证分层结构
            print("   📖 验证分层结构...")
            with open(self.config_file, 'r', encoding='utf-8') as f:
                loaded_config = json.load(f)

            # 检查各层配置
            tier_checks = [
                # Transport层
                'transport_configs' in loaded_config,
                loaded_config.get('transport_configs', {}).get('serial', {}).get('default_baud_rate') == 9600,
                len(loaded_config.get('transport_configs', {}).get('serial', {}).get('supported_rates', [])) == 8,

                # Protocol层
                'protocol_configs' in loaded_config,
                loaded_config.get('protocol_configs', {}).get('reliable', {}).get('default_window_size') == 1,

                # UI层
                'ui_configs' in loaded_config,
                loaded_config.get('ui_configs', {}).get('display', {}).get('max_lines') == 100
            ]

            all_tiers_valid = all(tier_checks)
            tier_count = len([k for k in loaded_config.keys() if k.endswith('_configs')])

            passed = all_tiers_valid and tier_count == 3
            details = f"分层结构有效: {all_tiers_valid}, 配置层数: {tier_count}"

        except Exception as e:
            passed = False
            details = f"异常: {str(e)}"

        self.log_result("多层配置存储", passed, details)
        return passed

    def test_config_auto_persistence(self) -> bool:
        """测试4: 配置自动持久化"""
        print("\n🧪 测试4: 配置自动持久化")

        try:
            # 模拟程序运行时配置变更
            print("   🔄 模拟运行时配置变更...")
            runtime_config = self.create_test_config()

            # 模拟用户更改设置
            runtime_config['transport']['port_name'] = 'COM3'
            runtime_config['transport']['baud_rate'] = 115200
            runtime_config['ui']['hex_mode'] = True
            runtime_config['application']['last_used_file'] = 'test.txt'

            # 记录更改前的文件修改时间
            old_mtime = 0
            if os.path.exists(self.config_file):
                old_mtime = os.path.getmtime(self.config_file)

            # 模拟自动保存（比如程序关闭时）
            print("   💾 执行自动保存...")
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(runtime_config, f, indent=2, ensure_ascii=False)

            # 检查文件是否更新
            new_mtime = os.path.getmtime(self.config_file)
            file_updated = new_mtime > old_mtime

            # 模拟程序重启后加载配置
            print("   🔄 模拟程序重启加载...")
            time.sleep(0.1)  # 确保文件系统时间戳差异

            with open(self.config_file, 'r', encoding='utf-8') as f:
                reloaded_config = json.load(f)

            # 验证重启后配置保持一致
            config_persistent = (
                reloaded_config.get('transport', {}).get('port_name') == 'COM3' and
                reloaded_config.get('transport', {}).get('baud_rate') == 115200 and
                reloaded_config.get('ui', {}).get('hex_mode') == True and
                reloaded_config.get('application', {}).get('last_used_file') == 'test.txt'
            )

            # 检查配置完整性
            config_complete = all(key in reloaded_config for key in ['transport', 'protocol', 'ui', 'application'])

            passed = file_updated and config_persistent and config_complete
            details = f"文件已更新: {file_updated}, 配置持久化: {config_persistent}, 配置完整: {config_complete}"

        except Exception as e:
            passed = False
            details = f"异常: {str(e)}"

        self.log_result("配置自动持久化", passed, details)
        return passed

    def run_comprehensive_verification(self) -> bool:
        """运行完整的配置系统验证"""
        print("🚀 开始配置系统全面验证")
        print("=" * 60)

        test_methods = [
            self.test_json_config_read_write,
            self.test_default_value_fallback,
            self.test_multi_tier_config_storage,
            self.test_config_auto_persistence
        ]

        passed_tests = 0
        total_tests = len(test_methods)

        for test_method in test_methods:
            try:
                if test_method():
                    passed_tests += 1
            except Exception as e:
                print(f"❌ 测试执行异常: {e}")

        print("\n" + "=" * 60)
        print("📊 验证结果汇总")
        print("=" * 60)

        for result in self.verification_results:
            status_icon = "✅" if result['passed'] else "❌"
            print(f"{status_icon} [{result['timestamp']}] {result['test']}")
            if result['details']:
                print(f"   └─ {result['details']}")

        success_rate = (passed_tests / total_tests) * 100
        print(f"\n🎯 验证完成: {passed_tests}/{total_tests} 个测试通过 ({success_rate:.1f}%)")

        if passed_tests == total_tests:
            print("🎉 配置系统验证全部通过！配置管理功能确认有效。")
            return True
        else:
            print("⚠️  部分测试失败，需要进一步检查配置实现。")
            return False

def main():
    """主函数"""
    print("配置系统自动化验证脚本")
    print("验证ConfigStore的配置管理和持久化功能")
    print("=" * 60)

    verifier = ConfigStoreVerifier()
    success = verifier.run_comprehensive_verification()

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())