#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
增强版错误窗口捕获系统
专门用于捕获和分析程序启动时的错误对话框
"""

import os
import sys
import time
import subprocess
import ctypes
from ctypes import wintypes
import logging
from datetime import datetime
from typing import List, Dict, Optional, Tuple

# Windows API常量
WH_CBT = 5
HCBT_ACTIVATE = 5
WM_CLOSE = 0x0010
MB_OK = 0x00000000
MB_ICONERROR = 0x00000010
MB_ICONWARNING = 0x00000030

class ErrorWindow:
    """错误窗口信息结构"""
    def __init__(self, hwnd: int, title: str, text: str, window_type: str):
        self.hwnd = hwnd
        self.title = title
        self.text = text
        self.window_type = window_type
        self.timestamp = datetime.now()

class EnhancedErrorCapture:
    """增强版错误窗口捕获系统"""

    def __init__(self):
        self.captured_windows = []
        self.hook_id = None

    def find_error_windows(self) -> List[ErrorWindow]:
        """查找所有错误窗口"""
        error_windows = []

        try:
            # 使用Windows API查找窗口
            user32 = ctypes.windll.user32

            def enum_windows_callback(hwnd, lparam):
                # 窗口回调函数
                if user32.IsWindowVisible(hwnd):
                    title = self.get_window_title(hwnd)
                    if self.is_error_dialog(title):
                        text = self.get_window_text(hwnd)
                        window_type = self.classify_window_type(title, text)
                        error_window = ErrorWindow(hwnd, title, text, window_type)
                        error_windows.append(error_window)
                        logging.info(f"发现错误窗口: {title}")
                return True

            # 设置回调函数
            callback_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int))
            callback = callback_type(enum_windows_callback)

            # 枚举所有窗口
            user32.EnumWindows(callback, 0)

        except Exception as e:
            logging.error(f"查找错误窗口失败: {e}")

        return error_windows

    def get_window_title(self, hwnd: int) -> str:
        """获取窗口标题"""
        try:
            user32 = ctypes.windll.user32
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buffer = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buffer, length + 1)
                return buffer.value
        except:
            pass
        return ""

    def get_window_text(self, hwnd: int) -> str:
        """获取窗口文本内容"""
        try:
            # 尝试获取窗口中的文本内容
            user32 = ctypes.windll.user32

            # 查找子控件（静态文本等）
            def enum_child_callback(child_hwnd, lparam):
                texts = []
                try:
                    # 获取控件类名
                    class_name = ctypes.create_unicode_buffer(256)
                    user32.GetClassNameW(child_hwnd, class_name, 256)

                    # 如果是静态文本控件，获取其文本
                    if "Static" in class_name.value:
                        length = user32.GetWindowTextLengthW(child_hwnd)
                        if length > 0:
                            buffer = ctypes.create_unicode_buffer(length + 1)
                            user32.GetWindowTextW(child_hwnd, buffer, length + 1)
                            if buffer.value.strip():
                                texts.append(buffer.value)
                except:
                    pass

                return True

            callback_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int))
            callback = callback_type(enum_child_callback)
            user32.EnumChildWindows(hwnd, callback, 0)

        except Exception as e:
            logging.error(f"获取窗口文本失败: {e}")

        return ""

    def is_error_dialog(self, title: str) -> bool:
        """判断是否为错误对话框"""
        error_keywords = [
            "error", "错误", "assertion", "断言", "debug", "debug assertion",
            "failed", "失败", "exception", "异常", "runtime error", "运行时错误",
            "debug assertion failed", "microsoft visual c++", "application error"
        ]

        title_lower = title.lower()
        return any(keyword in title_lower for keyword in error_keywords)

    def classify_window_type(self, title: str, text: str) -> str:
        """分类窗口类型"""
        title_lower = title.lower()
        text_lower = text.lower()

        if "debug assertion" in title_lower:
            return "ASSERTION_FAILED"
        elif "microsoft visual c++" in title_lower:
            return "RUNTIME_ERROR"
        elif "application error" in title_lower:
            return "APPLICATION_ERROR"
        elif "failed" in title_lower or "失败" in title_lower:
            return "GENERIC_ERROR"
        else:
            return "UNKNOWN_ERROR"

    def capture_screenshot_with_error_window(self, error_window: ErrorWindow) -> Optional[str]:
        """为特定错误窗口截图"""
        try:
            from PIL import ImageGrab
            import pygetwindow as gw

            # 查找窗口位置
            windows = gw.getWindowsWithTitle(error_window.title)
            if not windows:
                # 尝试通过句柄查找
                title = error_window.title
                windows = gw.getAllWindows()
                for window in windows:
                    if window.title == title:
                        target_window = window
                        break
                else:
                    return self.capture_full_screenshot()
            else:
                target_window = windows[0]

            # 截取特定窗口区域
            bbox = (
                target_window.left,
                target_window.top,
                target_window.left + target_window.width,
                target_window.top + target_window.height
            )

            screenshot = ImageGrab.grab(bbox=bbox)

            # 保存截图
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"auto_screenshots/error_{error_window.window_type}_{timestamp}.png"
            screenshot.save(filename)

            logging.info(f"错误窗口截图已保存: {filename}")
            return filename

        except ImportError:
            return self.capture_full_screenshot()
        except Exception as e:
            logging.error(f"错误窗口截图失败: {e}")
            return self.capture_full_screenshot()

    def capture_full_screenshot(self) -> Optional[str]:
        """截取全屏"""
        try:
            from PIL import ImageGrab

            screenshot = ImageGrab.grab()
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"auto_screenshots/fullscreen_{timestamp}.png"
            screenshot.save(filename)

            logging.info(f"全屏截图已保存: {filename}")
            return filename

        except Exception as e:
            logging.error(f"全屏截图失败: {e}")
            return None

    def close_error_window(self, error_window: ErrorWindow) -> bool:
        """关闭错误窗口"""
        try:
            user32 = ctypes.windll.user32
            result = user32.SendMessageW(error_window.hwnd, WM_CLOSE, 0, 0)

            if result:
                logging.info(f"已关闭错误窗口: {error_window.title}")
                return True
            else:
                logging.warning(f"关闭错误窗口失败: {error_window.title}")
                return False

        except Exception as e:
            logging.error(f"关闭错误窗口异常: {e}")
            return False

    def analyze_error_content(self, error_window: ErrorWindow) -> Dict:
        """分析错误内容"""
        analysis = {
            'window_type': error_window.window_type,
            'title': error_window.title,
            'text': error_window.text,
            'error_keywords': [],
            'likely_cause': '',
            'suggested_fix': '',
            'confidence': 0.0
        }

        # 提取错误关键词
        text_content = (error_window.title + " " + error_window.text).lower()

        keyword_patterns = {
            'Debug Assertion Failed': {
                'keywords': ['debug assertion', 'assertion failed', 'expression:', 'file:', 'line:'],
                'cause': 'MFC调试断言失败，通常由空指针访问或数组越界引起',
                'fix': '检查指针初始化、数组边界和控件访问时机',
                'confidence': 0.9
            },
            'Microsoft Visual C++ Runtime Library': {
                'keywords': ['runtime error', 'microsoft visual c++', 'runtime library'],
                'cause': 'C++运行时错误，可能是内存访问违例或空指针',
                'fix': '检查内存管理、指针操作和对象生命周期',
                'confidence': 0.85
            },
            'Application Error': {
                'keywords': ['application error', 'memory could not be', 'access violation'],
                'cause': '应用程序内存访问错误',
                'fix': '检查内存分配、释放和访问权限',
                'confidence': 0.8
            }
        }

        # 匹配错误模式
        for pattern_name, pattern_info in keyword_patterns.items():
            found_keywords = [kw for kw in pattern_info['keywords'] if kw in text_content]
            if found_keywords:
                analysis['error_keywords'] = found_keywords
                analysis['likely_cause'] = pattern_info['cause']
                analysis['suggested_fix'] = pattern_info['fix']
                analysis['confidence'] = pattern_info['confidence']
                break

        return analysis

def test_error_capture():
    """测试错误捕获功能"""
    logging.basicConfig(level=logging.INFO)

    capturer = EnhancedErrorCapture()
    logging.info("开始捕获错误窗口...")

    # 查找错误窗口
    error_windows = capturer.find_error_windows()

    if error_windows:
        logging.info(f"发现 {len(error_windows)} 个错误窗口")

        for error_window in error_windows:
            logging.info(f"处理错误窗口: {error_window.title}")

            # 分析错误内容
            analysis = capturer.analyze_error_content(error_window)
            logging.info(f"错误分析: {analysis}")

            # 截图
            screenshot_file = capturer.capture_screenshot_with_error_window(error_window)
            if screenshot_file:
                logging.info(f"截图已保存: {screenshot_file}")

            # 关闭错误窗口
            capturer.close_error_window(error_window)

        return True
    else:
        logging.info("未发现错误窗口")
        return False

if __name__ == '__main__':
    test_error_capture()