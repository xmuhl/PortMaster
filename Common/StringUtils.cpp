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