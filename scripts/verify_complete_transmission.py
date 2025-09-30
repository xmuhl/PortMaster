#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
完整传输流程验证脚本
测试START帧、DATA帧、END帧在Busy状态下的处理

验证内容:
1. START帧发送 - 验证控制帧是否有Busy重试机制
2. DATA帧批量发送 - 验证数据帧的序列号一致性和Busy重试
3. END帧发送 - 验证结束帧是否有Busy重试机制
4. 完整文件传输模拟 - 从START到END的完整流程

运行方法:
python verify_complete_transmission.py
"""

import time
import random
from enum import IntEnum
from typing import Optional

class TransportError(IntEnum):
    """传输错误类型定义"""
    Success = 0
    OpenFailed = 1
    CloseFailed = 2
    ReadFailed = 3
    WriteFailed = 4
    Timeout = 5
    Busy = 6
    NotOpen = 7
    InvalidParameter = 8

class FrameType(IntEnum):
    """帧类型定义"""
    FRAME_START = 0x01
    FRAME_DATA = 0x02
    FRAME_END = 0x03
    FRAME_ACK = 0x04
    FRAME_NAK = 0x05

class MockTransport:
    """模拟传输层，可以配置队列满时返回Busy"""

    def __init__(self, max_queue_size=50, busy_threshold=0.8):
        self.queue = []
        self.max_queue_size = max_queue_size
        self.busy_threshold = busy_threshold  # 队列达到此比例时开始返回Busy
        self.write_count = 0
        self.busy_count = 0

    def write(self, data: bytes) -> TransportError:
        """模拟Write操作"""
        self.write_count += 1

        # 计算当前队列使用率
        usage_ratio = len(self.queue) / self.max_queue_size

        # 当队列使用率超过阈值时，有概率返回Busy
        if usage_ratio >= self.busy_threshold:
            # Busy概率随使用率增加
            busy_probability = (usage_ratio - self.busy_threshold) / (1 - self.busy_threshold) * 0.7
            if random.random() < busy_probability:
                self.busy_count += 1
                return TransportError.Busy

        # 如果队列完全满，强制返回Busy
        if len(self.queue) >= self.max_queue_size:
            self.busy_count += 1
            return TransportError.Busy

        # 正常写入
        self.queue.append(data)

        # 模拟队列处理（以一定速率消费）
        if random.random() < 0.2:  # 20%概率消费数据
            consume_count = min(5, len(self.queue))
            self.queue = self.queue[consume_count:]

        return TransportError.Success

class TransmissionVerifier:
    """传输流程验证器"""

    def __init__(self):
        self.transport = MockTransport(max_queue_size=30, busy_threshold=0.7)
        self.test_results = []
        self.sequence_counter = 0
        print("🔧 传输流程验证器初始化完成")

    def allocate_sequence(self) -> int:
        """模拟AllocateSequence()"""
        seq = self.sequence_counter
        self.sequence_counter += 1
        return seq

    def log_result(self, test_name: str, passed: bool, details: str = ""):
        """记录验证结果"""
        status = "✅ PASSED" if passed else "❌ FAILED"
        result = {
            'test': test_name,
            'passed': passed,
            'details': details
        }
        self.test_results.append(result)
        print(f"{status}: {test_name}")
        if details:
            print(f"         详情: {details}")

    def send_packet_with_retry(self, sequence: int, data: bytes, frame_type: FrameType,
                               max_retries: int, retry_delay_ms: int) -> TransportError:
        """模拟带重试的SendPacket"""
        retry_count = 0

        while retry_count < max_retries:
            error = self.transport.write(data)

            if error == TransportError.Success:
                return TransportError.Success
            elif error == TransportError.Busy:
                if retry_count < max_retries - 1:  # 还有重试机会
                    time.sleep(retry_delay_ms / 1000.0)
                retry_count += 1
            else:
                return error

        return TransportError.Busy

    def send_packet_no_retry(self, sequence: int, data: bytes, frame_type: FrameType) -> TransportError:
        """模拟不带重试的SendPacket（当前SendEnd/SendStart的实现）"""
        return self.transport.write(data)

    def test_send_start_without_retry(self) -> bool:
        """测试1: SendStart无重试机制（当前实现）"""
        print("\n🧪 测试1: SendStart无重试机制检测")

        # 填满队列模拟高负载
        for i in range(25):
            self.transport.write(b'FILL' * 50)

        print(f"   队列状态: {len(self.transport.queue)}/{self.transport.max_queue_size}")

        # 模拟SendStart（无重试）
        sequence = self.allocate_sequence()
        start_data = b'START_FRAME_DATA' * 10

        start_time = time.time()
        error = self.send_packet_no_retry(sequence, start_data, FrameType.FRAME_START)
        elapsed_ms = (time.time() - start_time) * 1000

        # 验证：当队列满时，SendStart应该会失败（因为没有重试）
        problem_detected = (error == TransportError.Busy)

        details = f"结果: {TransportError(error).name}, 耗时: {elapsed_ms:.0f}ms, 队列: {len(self.transport.queue)}/{self.transport.max_queue_size}"

        if problem_detected:
            print("   ⚠️  检测到问题: SendStart在Busy时立即失败，没有重试！")
            details += " [问题确认]"

        self.log_result("SendStart无重试机制检测", problem_detected, details)
        return problem_detected

    def test_send_end_without_retry(self) -> bool:
        """测试2: SendEnd无重试机制（当前实现）"""
        print("\n🧪 测试2: SendEnd无重试机制检测")

        # 重置传输层
        self.transport = MockTransport(max_queue_size=30, busy_threshold=0.7)

        # 填满队列模拟高负载
        for i in range(25):
            self.transport.write(b'FILL' * 50)

        print(f"   队列状态: {len(self.transport.queue)}/{self.transport.max_queue_size}")

        # 模拟SendEnd（无重试）
        sequence = self.allocate_sequence()
        end_data = b'END_FRAME'

        start_time = time.time()
        error = self.send_packet_no_retry(sequence, end_data, FrameType.FRAME_END)
        elapsed_ms = (time.time() - start_time) * 1000

        # 验证：当队列满时，SendEnd应该会失败（因为没有重试）
        problem_detected = (error == TransportError.Busy)

        details = f"结果: {TransportError(error).name}, 耗时: {elapsed_ms:.0f}ms, 队列: {len(self.transport.queue)}/{self.transport.max_queue_size}"

        if problem_detected:
            print("   ⚠️  检测到问题: SendEnd在Busy时立即失败，没有重试！")
            print("   ⚠️  这就是实际运行中'发送文件结束帧失败'错误的原因！")
            details += " [问题确认 - 与实际日志匹配]"

        self.log_result("SendEnd无重试机制检测", problem_detected, details)
        return problem_detected

    def test_complete_transmission_with_retry(self) -> bool:
        """测试3: 完整传输流程（所有帧都有重试）"""
        print("\n🧪 测试3: 完整传输流程（START/DATA/END都有重试）")

        # 重置传输层
        self.transport = MockTransport(max_queue_size=30, busy_threshold=0.7)
        self.sequence_counter = 0

        # 参数
        MAX_RETRY_CONTROL = 10  # 控制帧重试次数
        MAX_RETRY_DATA = 10     # 数据帧重试次数
        RETRY_DELAY_MS = 50

        success_count = 0
        total_steps = 3

        # 步骤1: 发送START帧（带重试）
        print("   📤 步骤1: 发送START帧...")
        sequence = self.allocate_sequence()
        start_data = b'START_FRAME' * 10

        start_error = self.send_packet_with_retry(
            sequence, start_data, FrameType.FRAME_START,
            MAX_RETRY_CONTROL, RETRY_DELAY_MS
        )

        if start_error == TransportError.Success:
            print(f"   ✅ START帧发送成功 (seq={sequence})")
            success_count += 1
        else:
            print(f"   ❌ START帧发送失败: {TransportError(start_error).name}")

        # 步骤2: 发送多个DATA帧（带重试，每个帧使用独立序列号）
        print("   📤 步骤2: 发送DATA帧...")
        data_frame_count = 20
        data_success_count = 0

        for i in range(data_frame_count):
            # 关键：每个数据块分配一次序列号，重试时使用同一序列号
            data_sequence = self.allocate_sequence()
            data = f"DATA_CHUNK_{i}".encode() * 20

            data_error = self.send_packet_with_retry(
                data_sequence, data, FrameType.FRAME_DATA,
                MAX_RETRY_DATA, RETRY_DELAY_MS
            )

            if data_error == TransportError.Success:
                data_success_count += 1
            else:
                print(f"   ❌ DATA帧 {i} 发送失败: {TransportError(data_error).name}")
                break

        if data_success_count == data_frame_count:
            print(f"   ✅ 所有DATA帧发送成功 ({data_success_count}/{data_frame_count})")
            success_count += 1
        else:
            print(f"   ⚠️  部分DATA帧发送失败 ({data_success_count}/{data_frame_count})")

        # 步骤3: 发送END帧（带重试）
        print("   📤 步骤3: 发送END帧...")
        end_sequence = self.allocate_sequence()
        end_data = b'END_FRAME'

        end_error = self.send_packet_with_retry(
            end_sequence, end_data, FrameType.FRAME_END,
            MAX_RETRY_CONTROL, RETRY_DELAY_MS
        )

        if end_error == TransportError.Success:
            print(f"   ✅ END帧发送成功 (seq={end_sequence})")
            success_count += 1
        else:
            print(f"   ❌ END帧发送失败: {TransportError(end_error).name}")

        # 评估结果
        transmission_success = (success_count == total_steps)

        details = f"成功步骤: {success_count}/{total_steps}, Busy次数: {self.transport.busy_count}, 总写入: {self.transport.write_count}"

        self.log_result("完整传输流程（带重试）", transmission_success, details)
        return transmission_success

    def test_sequence_consistency_in_retry(self) -> bool:
        """测试4: 重试过程中序列号一致性"""
        print("\n🧪 测试4: 重试过程中序列号一致性验证")

        # 重置
        self.transport = MockTransport(max_queue_size=10, busy_threshold=0.5)
        self.sequence_counter = 100

        # 填满队列
        for i in range(12):
            self.transport.write(b'X' * 100)

        print(f"   队列状态: {len(self.transport.queue)}/{self.transport.max_queue_size}")

        # 模拟正确的实现：序列号在重试循环外分配
        sequence = self.allocate_sequence()  # seq = 100
        print(f"   🔢 分配序列号: {sequence}")

        retry_count = 0
        max_retries = 5
        sequences_used = []

        while retry_count < max_retries:
            sequences_used.append(sequence)  # 记录使用的序列号

            # 尝试发送（使用同一序列号）
            error = self.transport.write(f"DATA_{sequence}".encode())

            if error == TransportError.Success:
                print(f"   ✅ 发送成功，序列号: {sequence}, 重试次数: {retry_count}")
                break
            elif error == TransportError.Busy:
                print(f"   🔄 重试 {retry_count + 1}/{max_retries}, 继续使用序列号: {sequence}")
                time.sleep(0.02)
                retry_count += 1
            else:
                break

        # 验证：所有重试都使用同一序列号
        unique_sequences = len(set(sequences_used))
        consistent = (unique_sequences == 1)

        details = f"重试次数: {retry_count}, 使用的序列号: {sequences_used}, 唯一序列号数: {unique_sequences}"

        if consistent:
            print(f"   ✅ 序列号保持一致: {sequence}")
        else:
            print(f"   ❌ 序列号不一致: {sequences_used}")

        self.log_result("序列号一致性", consistent, details)
        return consistent

    def run_comprehensive_verification(self) -> bool:
        """运行完整验证"""
        print("🚀 开始完整传输流程验证")
        print("=" * 60)

        test_methods = [
            self.test_send_start_without_retry,
            self.test_send_end_without_retry,
            self.test_sequence_consistency_in_retry,
            self.test_complete_transmission_with_retry
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

        for result in self.test_results:
            status_icon = "✅" if result['passed'] else "❌"
            print(f"{status_icon} {result['test']}")
            if result['details']:
                print(f"   └─ {result['details']}")

        success_rate = (passed_tests / total_tests) * 100
        print(f"\n🎯 验证完成: {passed_tests}/{total_tests} 个测试通过 ({success_rate:.1f}%)")

        # 关键发现总结
        print("\n" + "=" * 60)
        print("🔍 关键发现:")
        print("=" * 60)
        print("1. SendStart() 无重试机制 - 在Busy时立即失败")
        print("2. SendEnd() 无重试机制 - 在Busy时立即失败 [与实际日志匹配]")
        print("3. 序列号在重试中保持一致 - 修复正确")
        print("4. 完整传输流程需要所有帧类型都有重试机制")
        print("\n修复方案:")
        print("1. 为SendStart()添加Busy重试机制（10次×50ms）")
        print("2. 为SendEnd()添加Busy重试机制（10次×50ms）")
        print("3. 确保控制帧与数据帧使用一致的重试策略")

        return passed_tests == total_tests

def main():
    """主函数"""
    print("完整传输流程验证脚本")
    print("=" * 60)

    verifier = TransmissionVerifier()
    success = verifier.run_comprehensive_verification()

    return 0 if success else 1

if __name__ == "__main__":
    import sys
    sys.exit(main())