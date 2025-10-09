#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试数据生成器
用于生成各种类型和大小的测试文件，支持传输测试
"""

import os
import random
import argparse
import hashlib
import struct
from datetime import datetime
from pathlib import Path


class TestDataGenerator:
    """测试数据生成器类"""

    def __init__(self, output_dir="test_data"):
        """
        初始化生成器

        Args:
            output_dir: 输出目录
        """
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)

    def generate_random_binary(self, size, filename=None):
        """
        生成随机二进制数据

        Args:
            size: 文件大小（字节）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        if filename is None:
            filename = f"random_{size}bytes_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"

        filepath = self.output_dir / filename

        # 分块生成，避免内存溢出
        chunk_size = 1024 * 1024  # 1MB chunks
        with open(filepath, 'wb') as f:
            remaining = size
            while remaining > 0:
                chunk = min(chunk_size, remaining)
                data = os.urandom(chunk)
                f.write(data)
                remaining -= chunk

        print(f"✓ 生成随机二进制文件: {filepath} ({size} bytes)")
        return filepath

    def generate_pattern_binary(self, size, pattern=b'\xAA\x55', filename=None):
        """
        生成重复模式的二进制数据

        Args:
            size: 文件大小（字节）
            pattern: 重复模式（字节序列）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        if filename is None:
            filename = f"pattern_{size}bytes_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"

        filepath = self.output_dir / filename

        with open(filepath, 'wb') as f:
            pattern_len = len(pattern)
            full_repeats = size // pattern_len
            remainder = size % pattern_len

            # 写入完整重复
            f.write(pattern * full_repeats)
            # 写入剩余部分
            if remainder > 0:
                f.write(pattern[:remainder])

        print(f"✓ 生成模式二进制文件: {filepath} ({size} bytes, pattern: {pattern.hex()})")
        return filepath

    def generate_text_file(self, size, filename=None):
        """
        生成文本文件

        Args:
            size: 文件大小（字节）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        if filename is None:
            filename = f"text_{size}bytes_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"

        filepath = self.output_dir / filename

        # 生成可读文本内容
        lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "

        with open(filepath, 'w', encoding='utf-8') as f:
            written = 0
            line_num = 1
            while written < size:
                line = f"Line {line_num}: {lorem}"
                if written + len(line) > size:
                    line = line[:size - written]
                f.write(line + '\n')
                written += len(line) + 1
                line_num += 1

        print(f"✓ 生成文本文件: {filepath} ({size} bytes)")
        return filepath

    def generate_zero_filled(self, size, filename=None):
        """
        生成全零填充文件

        Args:
            size: 文件大小（字节）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        if filename is None:
            filename = f"zeros_{size}bytes_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"

        filepath = self.output_dir / filename

        # 使用seek+write技巧快速创建稀疏文件（在支持的文件系统上）
        with open(filepath, 'wb') as f:
            f.seek(size - 1)
            f.write(b'\0')

        print(f"✓ 生成零填充文件: {filepath} ({size} bytes)")
        return filepath

    def generate_sequential(self, size, filename=None):
        """
        生成顺序递增数据

        Args:
            size: 文件大小（字节）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        if filename is None:
            filename = f"sequential_{size}bytes_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"

        filepath = self.output_dir / filename

        with open(filepath, 'wb') as f:
            for i in range(size):
                f.write(bytes([i % 256]))

        print(f"✓ 生成顺序递增文件: {filepath} ({size} bytes)")
        return filepath

    def generate_compressible(self, size, filename=None):
        """
        生成高度可压缩的数据（大量重复）

        Args:
            size: 文件大小（字节）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        if filename is None:
            filename = f"compressible_{size}bytes_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"

        filepath = self.output_dir / filename

        # 使用简单的重复模式（90%重复，10%随机）
        pattern = b'A' * 100

        with open(filepath, 'wb') as f:
            written = 0
            while written < size:
                if random.random() < 0.9:
                    # 90%概率写入重复模式
                    chunk = pattern[:min(len(pattern), size - written)]
                else:
                    # 10%概率写入随机数据
                    chunk = os.urandom(min(10, size - written))
                f.write(chunk)
                written += len(chunk)

        print(f"✓ 生成可压缩文件: {filepath} ({size} bytes)")
        return filepath

    def generate_incompressible(self, size, filename=None):
        """
        生成不可压缩的数据（纯随机）

        Args:
            size: 文件大小（字节）
            filename: 文件名（可选）

        Returns:
            生成的文件路径
        """
        # 与generate_random_binary相同
        return self.generate_random_binary(size, filename)

    def generate_boundary_test(self, filename=None):
        """
        生成边界条件测试文件
        包含：空文件、1字节、255字节、256字节、1KB-1、1KB、1KB+1等

        Args:
            filename: 文件名前缀（可选）

        Returns:
            生成的文件路径列表
        """
        boundary_sizes = [
            0,      # 空文件
            1,      # 1字节
            255,    # 1字节以下最大值
            256,    # 1字节边界
            1023,   # 1KB - 1
            1024,   # 1KB
            1025,   # 1KB + 1
            4095,   # 4KB - 1
            4096,   # 4KB (常见页大小)
            4097,   # 4KB + 1
            65535,  # 64KB - 1
            65536,  # 64KB
            65537,  # 64KB + 1
        ]

        files = []
        for size in boundary_sizes:
            name = f"boundary_{size}bytes.bin" if filename is None else f"{filename}_{size}bytes.bin"
            filepath = self.generate_random_binary(size, name)
            files.append(filepath)

        print(f"✓ 生成边界测试文件集: {len(files)} 个文件")
        return files

    def generate_test_suite(self):
        """
        生成完整测试套件
        包含各种类型和大小的测试文件

        Returns:
            生成的文件路径字典
        """
        files = {}

        print("\n=== 生成测试套件 ===\n")

        # 小文件测试
        print("--- 小文件测试 (< 10KB) ---")
        files['small_random_1kb'] = self.generate_random_binary(1024, "small_random_1kb.bin")
        files['small_random_5kb'] = self.generate_random_binary(5 * 1024, "small_random_5kb.bin")
        files['small_text_2kb'] = self.generate_text_file(2 * 1024, "small_text_2kb.txt")

        # 中等文件测试
        print("\n--- 中等文件测试 (100KB - 1MB) ---")
        files['medium_random_100kb'] = self.generate_random_binary(100 * 1024, "medium_random_100kb.bin")
        files['medium_random_512kb'] = self.generate_random_binary(512 * 1024, "medium_random_512kb.bin")
        files['medium_random_1mb'] = self.generate_random_binary(1024 * 1024, "medium_random_1mb.bin")

        # 大文件测试
        print("\n--- 大文件测试 (> 1MB) ---")
        files['large_random_5mb'] = self.generate_random_binary(5 * 1024 * 1024, "large_random_5mb.bin")
        files['large_random_10mb'] = self.generate_random_binary(10 * 1024 * 1024, "large_random_10mb.bin")

        # 特殊模式测试
        print("\n--- 特殊模式测试 ---")
        files['pattern_aa55_1mb'] = self.generate_pattern_binary(1024 * 1024, b'\xAA\x55', "pattern_aa55_1mb.bin")
        files['pattern_0xff_1mb'] = self.generate_pattern_binary(1024 * 1024, b'\xFF', "pattern_0xff_1mb.bin")
        files['zeros_1mb'] = self.generate_zero_filled(1024 * 1024, "zeros_1mb.bin")
        files['sequential_1mb'] = self.generate_sequential(1024 * 1024, "sequential_1mb.bin")

        # 压缩性测试
        print("\n--- 压缩性测试 ---")
        files['compressible_1mb'] = self.generate_compressible(1024 * 1024, "compressible_1mb.bin")
        files['incompressible_1mb'] = self.generate_incompressible(1024 * 1024, "incompressible_1mb.bin")

        # 边界条件测试
        print("\n--- 边界条件测试 ---")
        files['boundary_files'] = self.generate_boundary_test("boundary")

        print(f"\n=== 测试套件生成完成 ===")
        print(f"输出目录: {self.output_dir.absolute()}")
        print(f"文件总数: {len(files) + len(files.get('boundary_files', []))}")

        return files

    def calculate_checksum(self, filepath):
        """
        计算文件校验和

        Args:
            filepath: 文件路径

        Returns:
            MD5和SHA256校验和
        """
        md5 = hashlib.md5()
        sha256 = hashlib.sha256()

        with open(filepath, 'rb') as f:
            while True:
                data = f.read(65536)  # 64KB chunks
                if not data:
                    break
                md5.update(data)
                sha256.update(data)

        return {
            'md5': md5.hexdigest(),
            'sha256': sha256.hexdigest()
        }


def parse_size(size_str):
    """
    解析大小字符串（支持K/M/G后缀）

    Args:
        size_str: 大小字符串（如 "10K", "5M", "1G"）

    Returns:
        字节数
    """
    size_str = size_str.upper().strip()

    multipliers = {
        'K': 1024,
        'M': 1024 * 1024,
        'G': 1024 * 1024 * 1024,
    }

    if size_str[-1] in multipliers:
        return int(size_str[:-1]) * multipliers[size_str[-1]]
    else:
        return int(size_str)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="测试数据生成器 - 为传输测试生成各种类型的文件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 生成完整测试套件
  python test_data_generator.py --suite

  # 生成10MB随机文件
  python test_data_generator.py --type random --size 10M --output mytest.bin

  # 生成1KB模式文件
  python test_data_generator.py --type pattern --size 1K --pattern AA55

  # 生成边界测试文件
  python test_data_generator.py --boundary
        """
    )

    parser.add_argument('--output-dir', '-o', default='test_data',
                        help='输出目录 (默认: test_data)')
    parser.add_argument('--suite', action='store_true',
                        help='生成完整测试套件')
    parser.add_argument('--boundary', action='store_true',
                        help='生成边界条件测试文件')
    parser.add_argument('--type', '-t', choices=[
                        'random', 'pattern', 'text', 'zeros', 'sequential',
                        'compressible', 'incompressible'
                        ], help='数据类型')
    parser.add_argument('--size', '-s', help='文件大小 (支持K/M/G后缀, 如: 10M)')
    parser.add_argument('--output', help='输出文件名')
    parser.add_argument('--pattern', default='AA55',
                        help='模式数据的十六进制字符串 (默认: AA55)')
    parser.add_argument('--checksum', action='store_true',
                        help='计算并显示生成文件的校验和')

    args = parser.parse_args()

    # 创建生成器
    generator = TestDataGenerator(args.output_dir)

    generated_files = []

    # 生成完整测试套件
    if args.suite:
        files = generator.generate_test_suite()
        for key, value in files.items():
            if isinstance(value, list):
                generated_files.extend(value)
            else:
                generated_files.append(value)

    # 生成边界测试文件
    elif args.boundary:
        generated_files = generator.generate_boundary_test()

    # 生成单个文件
    elif args.type and args.size:
        size = parse_size(args.size)

        if args.type == 'random':
            filepath = generator.generate_random_binary(size, args.output)
        elif args.type == 'pattern':
            pattern = bytes.fromhex(args.pattern)
            filepath = generator.generate_pattern_binary(size, pattern, args.output)
        elif args.type == 'text':
            filepath = generator.generate_text_file(size, args.output)
        elif args.type == 'zeros':
            filepath = generator.generate_zero_filled(size, args.output)
        elif args.type == 'sequential':
            filepath = generator.generate_sequential(size, args.output)
        elif args.type == 'compressible':
            filepath = generator.generate_compressible(size, args.output)
        elif args.type == 'incompressible':
            filepath = generator.generate_incompressible(size, args.output)

        generated_files.append(filepath)

    else:
        parser.print_help()
        return

    # 计算校验和
    if args.checksum and generated_files:
        print("\n=== 文件校验和 ===")
        for filepath in generated_files:
            if os.path.exists(filepath):
                checksums = generator.calculate_checksum(filepath)
                print(f"\n文件: {filepath}")
                print(f"  MD5:    {checksums['md5']}")
                print(f"  SHA256: {checksums['sha256']}")


if __name__ == '__main__':
    main()
