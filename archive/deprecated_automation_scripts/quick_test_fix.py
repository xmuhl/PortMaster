#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
简化版自主修复系统 - 快速测试错误检测和修复流程
"""

import os
import sys
import time
import subprocess
import logging
from datetime import datetime

# 设置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(f'fix_results/quick_test_{datetime.now().strftime("%Y%m%d_%H%M%S")}.log', encoding='utf-8'),
        logging.StreamHandler()
    ]
)

def test_error_detection_and_fix():
    """测试错误检测和修复的完整流程"""
    logging.info("=== 开始快速测试错误检测和修复流程 ===")

    # 1. 启动程序
    logging.info("步骤1: 启动程序...")
    try:
        # 清理可能存在的进程
        subprocess.run(['taskkill', '/F', '/IM', 'PortMaster.exe'], capture_output=True)
        time.sleep(2)

        # 启动程序
        process = subprocess.Popen(
            ["build\\Debug\\PortMaster.exe"],
            cwd=os.getcwd(),
            shell=True
        )

        logging.info(f"程序已启动，PID: {process.pid}")
        time.sleep(5)  # 等待错误窗口出现

    except Exception as e:
        logging.error(f"启动程序失败: {e}")
        return False

    # 2. 检测错误窗口
    logging.info("步骤2: 检测错误窗口...")
    try:
        from enhanced_error_capture import EnhancedErrorCapture

        capturer = EnhancedErrorCapture()
        error_windows = capturer.find_error_windows()

        if error_windows:
            logging.info(f"✅ 成功检测到 {len(error_windows)} 个错误窗口")

            for error_window in error_windows:
                logging.info(f"错误窗口标题: {error_window.title}")
                logging.info(f"错误窗口类型: {error_window.window_type}")

                # 分析错误内容
                analysis = capturer.analyze_error_content(error_window)
                logging.info(f"错误分析: {analysis['likely_cause']}")
                logging.info(f"建议修复: {analysis['suggested_fix']}")

                # 截图
                screenshot_path = capturer.capture_screenshot_with_error_window(error_window)
                if screenshot_path:
                    logging.info(f"✅ 错误窗口截图已保存: {screenshot_path}")

                # 关闭错误窗口
                capturer.close_error_window(error_window)

            return True
        else:
            logging.warning("未检测到错误窗口")
            return False

    except Exception as e:
        logging.error(f"错误窗口检测失败: {e}")
        return False

    # 3. 应用修复
    logging.info("步骤3: 应用修复...")
    try:
        # 这里可以添加具体的修复逻辑
        logging.info("模拟应用修复...")

        # 重新编译（如果需要）
        logging.info("尝试重新编译...")
        result = subprocess.run(
            ['cmd.exe', '/c', os.path.join(os.getcwd(), 'autobuild_x86_debug.bat')],
            capture_output=True,
            text=True,
            timeout=120,
            cwd=os.getcwd()
        )

        if result.returncode == 0:
            logging.info("✅ 编译成功")
            return True
        else:
            logging.error(f"编译失败: {result.stderr}")
            return False

    except Exception as e:
        logging.error(f"修复应用失败: {e}")
        return False

    # 4. 清理
    logging.info("步骤4: 清理进程...")
    try:
        subprocess.run(['taskkill', '/F', '/IM', 'PortMaster.exe'], capture_output=True)
        logging.info("✅ 清理完成")
        return True
    except:
        pass

def main():
    """主函数"""
    logging.info("快速测试开始")

    # 创建必要的目录
    os.makedirs('auto_screenshots', exist_ok=True)
    os.makedirs('code_backups', exist_ok=True)
    os.makedirs('fix_results', exist_ok=True)

    # 运行测试
    success = test_error_detection_and_fix()

    if success:
        logging.info("✅ 快速测试成功完成！")
        return 0
    else:
        logging.error("❌ 快速测试失败")
        return 1

if __name__ == '__main__':
    sys.exit(main())