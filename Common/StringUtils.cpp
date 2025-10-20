#pragma execution_character_set("utf-8")

#include "pch.h"
#include "StringUtils.h"
#include <Windows.h>

// ==================== UTF-8编码转换 ====================

std::string StringUtils::Utf8EncodeWide(const std::wstring& wideStr)
{
	// 处理空字符串情况
	if (wideStr.empty())
	{
		return std::string();
	}

	try
	{
		// 计算转换所需的缓冲区大小（包括null终止符）
		int size = WideCharToMultiByte(
			CP_UTF8,                // UTF-8代码页
			0,                      // 默认转换标志
			wideStr.c_str(),        // 源字符串
			-1,                     // 以null终止的字符串
			nullptr,                // 目标缓冲区为空，仅计算大小
			0,                      // 目标缓冲区大小为0
			nullptr,                // 默认字符
			nullptr                 // 默认字符使用标志
		);

		// 检查计算结果
		if (size <= 0)
		{
			// 转换失败，可能是无效的宽字符
			return std::string();
		}

		// 使用vector作为安全的缓冲区，包含null终止符的空间
		std::vector<char> buffer(size);

		// 执行实际转换
		int result = WideCharToMultiByte(
			CP_UTF8,                // UTF-8代码页
			0,                      // 默认转换标志
			wideStr.c_str(),        // 源字符串
			-1,                     // 以null终止的字符串
			buffer.data(),          // 目标缓冲区
			size,                   // 目标缓冲区大小
			nullptr,                // 默认字符
			nullptr                 // 默认字符使用标志
		);

		// 验证转换结果
		if (result <= 0)
		{
			// 转换失败
			return std::string();
		}

		// 返回不包含null终止符的字符串
		// result包含null终止符，所以实际字符长度为result-1
		return std::string(buffer.data(), result - 1);
	}
	catch (...)
	{
		// 异常情况下返回空字符串
		return std::string();
	}
}

std::wstring StringUtils::WideEncodeUtf8(const std::string& utf8Str)
{
	// 处理空字符串情况
	if (utf8Str.empty())
	{
		return std::wstring();
	}

	try
	{
		// 计算转换所需的缓冲区大小（包括null终止符）
		int size = MultiByteToWideChar(
			CP_UTF8,                // UTF-8代码页
			0,                      // 默认转换标志
			utf8Str.c_str(),        // 源字符串
			-1,                     // 以null终止的字符串
			nullptr,                // 目标缓冲区为空，仅计算大小
			0                       // 目标缓冲区大小为0
		);

		// 检查计算结果
		if (size <= 0)
		{
			// 转换失败，可能是无效的UTF-8字符串
			return std::wstring();
		}

		// 使用vector作为安全的缓冲区，包含null终止符的空间
		std::vector<wchar_t> buffer(size);

		// 执行实际转换
		int result = MultiByteToWideChar(
			CP_UTF8,                // UTF-8代码页
			0,                      // 默认转换标志
			utf8Str.c_str(),        // 源字符串
			-1,                     // 以null终止的字符串
			buffer.data(),          // 目标缓冲区
			size                    // 目标缓冲区大小
		);

		// 验证转换结果
		if (result <= 0)
		{
			// 转换失败
			return std::wstring();
		}

		// 返回不包含null终止符的宽字符串
		// result包含null终止符，所以实际字符长度为result-1
		return std::wstring(buffer.data(), result - 1);
	}
	catch (...)
	{
		// 异常情况下返回空字符串
		return std::wstring();
	}
}

// ==================== 辅助工具 ====================

bool StringUtils::IsValidUtf8(const std::string& str)
{
	// 处理空字符串
	if (str.empty())
	{
		return true; // 空字符串被认为是有效的UTF-8
	}

	size_t i = 0;
	size_t len = str.length();

	while (i < len)
	{
		unsigned char c = static_cast<unsigned char>(str[i]);

		// ASCII字符 (0x00-0x7F)
		if (c <= 0x7F)
		{
			i += 1;
			continue;
		}

		// 多字节UTF-8字符
		int expectedBytes = 0;
		if ((c & 0xE0) == 0xC0)          // 110xxxxx (2字节)
		{
			expectedBytes = 2;
		}
		else if ((c & 0xF0) == 0xE0)     // 1110xxxx (3字节)
		{
			expectedBytes = 3;
		}
		else if ((c & 0xF8) == 0xF0)     // 11110xxx (4字节)
		{
			expectedBytes = 4;
		}
		else
		{
			// 无效的UTF-8起始字节
			return false;
		}

		// 检查是否有足够的字节
		if (i + expectedBytes > len)
		{
			return false; // 字符串截断
		}

		// 检查后续字节是否都是10xxxxxx格式
		for (int j = 1; j < expectedBytes; ++j)
		{
			unsigned char nextByte = static_cast<unsigned char>(str[i + j]);
			if ((nextByte & 0xC0) != 0x80)
			{
				return false; // 后续字节格式错误
			}
		}

		i += expectedBytes;
	}

	return true;
}

std::string StringUtils::SafeTruncateUtf8(const std::string& str, size_t maxLength)
{
	// 处理空字符串或无需截断的情况
	if (str.empty() || str.length() <= maxLength)
	{
		return str;
	}

	// 计算字符数（非字节数）
	size_t charCount = 0;
	size_t bytePos = 0;
	size_t len = str.length();

	while (bytePos < len && charCount < maxLength)
	{
		unsigned char c = static_cast<unsigned char>(str[bytePos]);

		// ASCII字符 (0x00-0x7F)
		if (c <= 0x7F)
		{
			bytePos += 1;
		}
		else
		{
			// 多字节UTF-8字符
			int expectedBytes = 0;
			if ((c & 0xE0) == 0xC0)          // 110xxxxx (2字节)
			{
				expectedBytes = 2;
			}
			else if ((c & 0xF0) == 0xE0)     // 1110xxxx (3字节)
			{
				expectedBytes = 3;
			}
			else if ((c & 0xF8) == 0xF0)     // 11110xxx (4字节)
			{
				expectedBytes = 4;
			}
			else
			{
				// 遇到无效的UTF-8字符，按单字节处理
				expectedBytes = 1;
			}

			// 确保不会超出字符串长度
			if (bytePos + expectedBytes > len)
			{
				break; // 字符串截断，停止在安全位置
			}

			bytePos += expectedBytes;
		}

		charCount++;
	}

	// 返回截断后的字符串
	return str.substr(0, bytePos);
}

// ==================== 通用安全转换（支持指定代码页） ====================

std::wstring StringUtils::SafeMultiByteToWideChar(const std::string& input, UINT codePage)
{
	// 处理空字符串情况
	if (input.empty())
	{
		return std::wstring();
	}

	try
	{
		// 检查字符串长度是否安全（防止大文件内存分配失败）
		if (!IsStringLengthSafe(input))
		{
			// 字符串过长，拒绝转换
			return std::wstring();
		}

		// 计算转换所需的缓冲区大小（包括null终止符）
		int size = MultiByteToWideChar(
			codePage,               // 指定的代码页
			0,                      // 默认转换标志
			input.c_str(),          // 源字符串
			-1,                     // 以null终止的字符串
			nullptr,                // 目标缓冲区为空，仅计算大小
			0                       // 目标缓冲区大小为0
		);

		// 检查计算结果（严格验证返回值）
		if (size <= 0)
		{
			// 转换失败，可能是无效的多字节字符串或不支持的代码页
			return std::wstring();
		}

		// 使用vector作为安全的缓冲区，包含null终止符的空间
		// 自动管理内存，避免手动new/delete导致的内存泄漏
		std::vector<wchar_t> buffer(size);

		// 执行实际转换
		int result = MultiByteToWideChar(
			codePage,               // 指定的代码页
			0,                      // 默认转换标志
			input.c_str(),          // 源字符串
			-1,                     // 以null终止的字符串
			buffer.data(),          // 目标缓冲区
			size                    // 目标缓冲区大小
		);

		// 验证转换结果（二次检查，确保转换成功）
		if (result <= 0)
		{
			// 转换失败（不应该发生，但仍需检查）
			return std::wstring();
		}

		// 返回不包含null终止符的宽字符串
		// result包含null终止符，所以实际字符长度为result-1
		return std::wstring(buffer.data(), result - 1);
	}
	catch (const std::bad_alloc&)
	{
		// 内存分配失败（大文件情况）
		return std::wstring();
	}
	catch (...)
	{
		// 其他异常情况下返回空字符串
		return std::wstring();
	}
}

std::string StringUtils::SafeWideCharToMultiByte(const std::wstring& input, UINT codePage)
{
	// 处理空字符串情况
	if (input.empty())
	{
		return std::string();
	}

	try
	{
		// 检查字符串长度是否安全（防止大文件内存分配失败）
		if (!IsStringLengthSafe(input))
		{
			// 字符串过长，拒绝转换
			return std::string();
		}

		// 计算转换所需的缓冲区大小（包括null终止符）
		int size = WideCharToMultiByte(
			codePage,               // 指定的代码页
			0,                      // 默认转换标志
			input.c_str(),          // 源字符串
			-1,                     // 以null终止的字符串
			nullptr,                // 目标缓冲区为空，仅计算大小
			0,                      // 目标缓冲区大小为0
			nullptr,                // 默认字符
			nullptr                 // 默认字符使用标志
		);

		// 检查计算结果（严格验证返回值）
		if (size <= 0)
		{
			// 转换失败，可能是无效的宽字符串或不支持的代码页
			return std::string();
		}

		// 使用vector作为安全的缓冲区，包含null终止符的空间
		// 自动管理内存，避免手动new/delete导致的内存泄漏
		std::vector<char> buffer(size);

		// 执行实际转换
		int result = WideCharToMultiByte(
			codePage,               // 指定的代码页
			0,                      // 默认转换标志
			input.c_str(),          // 源字符串
			-1,                     // 以null终止的字符串
			buffer.data(),          // 目标缓冲区
			size,                   // 目标缓冲区大小
			nullptr,                // 默认字符
			nullptr                 // 默认字符使用标志
		);

		// 验证转换结果（二次检查，确保转换成功）
		if (result <= 0)
		{
			// 转换失败（不应该发生，但仍需检查）
			return std::string();
		}

		// 返回不包含null终止符的字符串
		// result包含null终止符，所以实际字符长度为result-1
		return std::string(buffer.data(), result - 1);
	}
	catch (const std::bad_alloc&)
	{
		// 内存分配失败（大文件情况）
		return std::string();
	}
	catch (...)
	{
		// 其他异常情况下返回空字符串
		return std::string();
	}
}

// ==================== 安全性检查 ====================

bool StringUtils::IsStringLengthSafe(const std::string& str, size_t maxLength)
{
	// 检查字符串字节长度是否在安全范围内
	return str.length() <= maxLength;
}

bool StringUtils::IsStringLengthSafe(const std::wstring& str, size_t maxLength)
{
	// 检查宽字符串字符数是否在安全范围内
	return str.length() <= maxLength;
}