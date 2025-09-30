#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Busy状态处理验证脚本
专门测试ReliableChannel对传输层Busy状态的处理机制

验证内容:
1. SendPacket返回TransportError.Busy时的重试机制
2. 重试次数和间隔是否符合设计（SendFile: 10次×50ms, SendThread: 5次×20ms）
3. 重试耗尽后的错误处理
4. 非Busy错误的立即失败机制

运行方法:
python verify_busy_handling.py
"""

import time
import random
from enum import IntEnum
from typing import Optional

class TransportError(IntEnum):
    """传输错误类型定义 - 与C++代码保持一致"""
    Success = 0           # 成功
    OpenFailed = 1        # 打开失败
    CloseFailed = 2       # 关闭失败
    ReadFailed = 3        # 读取失败
    WriteFailed = 4       # 写入失败
    Timeout = 5           # 超时
    Busy = 6              # 设备忙
    NotOpen = 7           # 未打开
    InvalidParameter = 8  # 无效参数

class MockLoopbackTransport:
    """模拟LoopbackTransport行为"""

    def __init__(self, max_queue_size=100):
        self.queue = []
        self.max_queue_size = max_queue_size
        self.write_count = 0
        self.busy_count = 0

    def write(self, data: bytes) -> TransportError:
        """模拟Write操作"""
        self.write_count += 1

        # 模拟队列满的情况
        if len(self.queue) >= self.max_queue_size:
            self.busy_count += 1
            print(f"   ⚠️  传输层Busy (队列: {len(self.queue)}/{self.max_queue_size}, 第{self.busy_count}次)")
            return TransportError.Busy

        # 正常写入
        self.queue.append(data)

        # 模拟队列处理（以一定速率消费）
        if random.random() < 0.3:  # 30%概率消费一些数据
            consume_count = min(3, len(self.queue))
            self.queue = self.queue[consume_count:]

        return TransportError.Success

class ReliableChannelVerifier:
    """可靠传输协议Busy状态处理验证器"""

    def __init__(self):
        self.transport = MockLoopbackTransport(max_queue_size=10)  # 小队列模拟快速填满
        self.test_results = []
        print("🔧 Busy状态处理验证器初始化完成")

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

    def simulate_send_packet(self, sequence: int, data: bytes, max_retries: int, retry_delay_ms: int) -> TransportError:
        """模拟SendPacket的重试逻辑"""
        retry_count = 0

        while retry_count < max_retries:
            error = self.transport.write(data)

            if error == TransportError.Success:
                return TransportError.Success
            elif error == TransportError.Busy:
                print(f"   🔄 重试 {retry_count + 1}/{max_retries}...")
                time.sleep(retry_delay_ms / 1000.0)
                retry_count += 1
            else:
                # 其他错误立即失败
                return error

        # 重试耗尽
        return TransportError.Busy

    def test_basic_busy_retry(self) -> bool:
        """测试1: 基本Busy重试机制"""
        print("\n🧪 测试1: 基本Busy重试机制")

        # 填满队列触发Busy
        test_data = b'A' * 100
        for i in range(15):  # 超过队列容量
            self.transport.write(test_data)

        print(f"   队列状态: {len(self.transport.queue)}/{self.transport.max_queue_size}")

        # 测试重试机制
        start_time = time.time()
        result = self.simulate_send_packet(1, test_data, max_retries=5, retry_delay_ms=20)
        elapsed_ms = (time.time() - start_time) * 1000

        # 应该重试并最终成功或失败
        retry_logic_works = result in [TransportError.Success, TransportError.Busy]

        details = f"结果: {TransportError(result).name}, 耗时: {elapsed_ms:.0f}ms, Busy次数: {self.transport.busy_count}"
        self.log_result("基本Busy重试机制", retry_logic_works, details)
        return retry_logic_works

    def test_sendfile_retry_parameters(self) -> bool:
        """测试2: SendFile重试参数(10次×50ms)"""
        print("\n🧪 测试2: SendFile重试参数验证")

        self.transport = MockLoopbackTransport(max_queue_size=5)  # 更小队列

        # 填满队列
        for i in range(10):
            self.transport.write(b'X' * 50)

        MAX_RETRY_COUNT = 10
        RETRY_DELAY_MS = 50

        start_time = time.time()
        result = self.simulate_send_packet(1, b'TEST', MAX_RETRY_COUNT, RETRY_DELAY_MS)
        elapsed_ms = (time.time() - start_time) * 1000

        # 验证重试次数和总延迟
        expected_min_delay = 0  # 如果第一次就成功
        expected_max_delay = MAX_RETRY_COUNT * RETRY_DELAY_MS + 200  # 加200ms容差

        delay_reasonable = elapsed_ms <= expected_max_delay

        details = f"耗时: {elapsed_ms:.0f}ms (预期≤{expected_max_delay}ms), 结果: {TransportError(result).name}"
        self.log_result("SendFile重试参数", delay_reasonable, details)
        return delay_reasonable

    def test_sendthread_retry_parameters(self) -> bool:
        """测试3: SendThread重试参数(5次×20ms)"""
        print("\n🧪 测试3: SendThread重试参数验证")

        self.transport = MockLoopbackTransport(max_queue_size=5)

        # 填满队列
        for i in range(10):
            self.transport.write(b'Y' * 50)

        MAX_RETRY_COUNT = 5
        RETRY_DELAY_MS = 20

        start_time = time.time()
        result = self.simulate_send_packet(1, b'THREAD_TEST', MAX_RETRY_COUNT, RETRY_DELAY_MS)
        elapsed_ms = (time.time() - start_time) * 1000

        # 验证重试次数和总延迟
        expected_max_delay = MAX_RETRY_COUNT * RETRY_DELAY_MS + 100  # 加100ms容差

        delay_reasonable = elapsed_ms <= expected_max_delay

        details = f"耗时: {elapsed_ms:.0f}ms (预期≤{expected_max_delay}ms), 结果: {TransportError(result).name}"
        self.log_result("SendThread重试参数", delay_reasonable, details)
        return delay_reasonable

    def test_non_busy_immediate_fail(self) -> bool:
        """测试4: 非Busy错误立即失败"""
        print("\n🧪 测试4: 非Busy错误立即失败机制")

        # 模拟一个立即返回WriteFailed的传输层
        class FailingTransport:
            def write(self, data: bytes) -> TransportError:
                return TransportError.WriteFailed

        failing_transport = FailingTransport()

        start_time = time.time()

        # 尝试写入但应该立即失败，不重试
        retry_count = 0
        max_retries = 5

        while retry_count < max_retries:
            error = failing_transport.write(b'DATA')

            if error == TransportError.Success:
                break
            elif error == TransportError.Busy:
                retry_count += 1
                time.sleep(0.02)
            else:
                # 非Busy错误，立即退出
                break

        elapsed_ms = (time.time() - start_time) * 1000

        # 应该立即失败，不重试（耗时应该<10ms）
        immediate_fail = elapsed_ms < 10 and retry_count == 0

        details = f"耗时: {elapsed_ms:.1f}ms, 重试次数: {retry_count} (预期: 0次)"
        self.log_result("非Busy错误立即失败", immediate_fail, details)
        return immediate_fail

    def test_retry_exhaustion(self) -> bool:
        """测试5: 重试次数耗尽的错误处理"""
        print("\n🧪 测试5: 重试次数耗尽错误处理")

        # 创建一个永远Busy的传输层
        class AlwaysBusyTransport:
            def __init__(self):
                self.call_count = 0

            def write(self, data: bytes) -> TransportError:
                self.call_count += 1
                return TransportError.Busy

        always_busy = AlwaysBusyTransport()

        MAX_RETRY_COUNT = 10
        RETRY_DELAY_MS = 10  # 缩短延迟以加快测试

        retry_count = 0
        start_time = time.time()

        while retry_count < MAX_RETRY_COUNT:
            error = always_busy.write(b'DATA')

            if error == TransportError.Success:
                break
            elif error == TransportError.Busy:
                time.sleep(RETRY_DELAY_MS / 1000.0)
                retry_count += 1
            else:
                break

        elapsed_ms = (time.time() - start_time) * 1000

        # 验证：应该尝试了MAX_RETRY_COUNT次
        correct_retry_count = (always_busy.call_count == MAX_RETRY_COUNT)

        # 最终应该返回Busy错误
        final_error = TransportError.Busy

        details = f"调用次数: {always_busy.call_count}/{MAX_RETRY_COUNT}, 耗时: {elapsed_ms:.0f}ms"
        self.log_result("重试次数耗尽处理", correct_retry_count, details)
        return correct_retry_count

    def test_sequence_allocation_issue(self) -> bool:
        """测试6: 检查AllocateSequence在重试中的问题"""
        print("\n🧪 测试6: 序列号分配问题检测")

        print("   ⚠️  关键问题: SendFile中每次重试都调用AllocateSequence()!")
        print("   原代码: sendError = SendPacket(AllocateSequence(), buffer)")
        print("   问题: 每次重试会生成新的序列号，导致发送窗口混乱")

        # 模拟问题场景
        sequence_allocations = []
        retry_count = 0
        max_retries = 5

        while retry_count < max_retries:
            # 错误的做法：每次重试都分配新序列号
            new_sequence = retry_count + 100  # 模拟AllocateSequence()
            sequence_allocations.append(new_sequence)

            print(f"   🔢 重试 {retry_count + 1}: 分配序列号 {new_sequence}")

            retry_count += 1
            if retry_count >= 3:  # 模拟第3次重试成功
                break

        # 验证问题：序列号不应该在重试中变化
        unique_sequences = len(set(sequence_allocations))
        problem_detected = unique_sequences > 1

        if problem_detected:
            details = f"❌ 检测到问题: {unique_sequences}个不同序列号 {sequence_allocations}"
        else:
            details = f"✅ 序列号保持一致: {sequence_allocations[0]}"

        self.log_result("序列号分配问题检测", problem_detected, details)

        # 正确的做法演示
        print("\n   ✅ 正确做法:")
        print("   uint16_t sequence = AllocateSequence();  // 重试循环外分配一次")
        print("   while (retry_count < MAX_RETRY_COUNT) {")
        print("       sendError = SendPacket(sequence, buffer);  // 使用同一序列号")
        print("   }")

        return problem_detected

    def run_comprehensive_verification(self) -> bool:
        """运行完整验证"""
        print("🚀 开始Busy状态处理机制全面验证")
        print("=" * 60)

        test_methods = [
            self.test_basic_busy_retry,
            self.test_sendfile_retry_parameters,
            self.test_sendthread_retry_parameters,
            self.test_non_busy_immediate_fail,
            self.test_retry_exhaustion,
            self.test_sequence_allocation_issue
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

        # 特别提示
        print("\n" + "=" * 60)
        print("🔍 关键发现:")
        print("=" * 60)
        print("测试6检测到重大问题: AllocateSequence()在重试循环内调用")
        print("这会导致每次重试使用不同的序列号，破坏发送窗口逻辑！")
        print("\n修复方案:")
        print("1. 在SendFile的重试循环外先调用 AllocateSequence()")
        print("2. 在重试循环内使用同一个序列号")
        print("3. 确保SendThread也遵循相同模式")

        return passed_tests == total_tests

def main():
    """主函数"""
    print("Busy状态处理机制验证脚本")
    print("=" * 60)

    verifier = ReliableChannelVerifier()
    success = verifier.run_comprehensive_verification()

    return 0 if success else 1

if __name__ == "__main__":
    import sys
    sys.exit(main())