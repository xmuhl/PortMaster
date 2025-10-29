#pragma execution_character_set("utf-8")

#include "pch.h"
#include "DataPresentationService.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

// ==================== 十六进制转换 ====================

std::string DataPresentationService::BytesToHex(const uint8_t* data, size_t length)
{
	std::ostringstream hexStream;
	std::ostringstream asciiStream;

	for (size_t i = 0; i < length; i++)
	{
		uint8_t byte = data[i];

		// 添加偏移地址(每16字节一行)
		if (i % 16 == 0)
		{
			if (i > 0)
			{
				// 添加ASCII显示部分
				hexStream << "  |" << asciiStream.str() << "|\r\n";
				asciiStream.str("");
				asciiStream.clear();
			}

			// 输出地址偏移
			hexStream << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
				<< static_cast<unsigned int>(i) << ": ";
		}

		// 添加十六进制值
		hexStream << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<int>(byte) << " ";

		// 添加可打印字符到ASCII部分
		if (byte >= 32 && byte <= 126)
		{
			asciiStream << static_cast<char>(byte);
		}
		else
		{
			asciiStream << ".";
		}
	}

	// 处理最后一行
	if (length % 16 != 0)
	{
		// 补齐空格
		size_t remain = 16 - (length % 16);
		for (size_t i = 0; i < remain; i++)
		{
			hexStream << "   ";
		}
	}

	// 添加ASCII显示部分
	hexStream << "  |" << asciiStream.str() << "|";

	return hexStream.str();
}

std::string DataPresentationService::BytesToHex(const std::vector<uint8_t>& data)
{
	return BytesToHex(data.data(), data.size());
}

std::vector<uint8_t> DataPresentationService::HexToBytes(const std::string& hex)
{
	std::vector<uint8_t> result;
	std::string cleanHex;
	bool inHexSection = false;

	// 从格式化的十六进制文本中提取纯十六进制字节对
	for (size_t i = 0; i < hex.length(); i++)
	{
		char ch = hex[i];

		// 检测十六进制数据区域(在冒号之后)
		if (ch == ':')
		{
			inHexSection = true;
			continue;
		}

		// 检测ASCII部分开始(在'|'字符处结束十六进制部分)
		if (ch == '|')
		{
			inHexSection = false;
			continue;
		}

		// 只在十六进制数据区域内收集字符
		if (inHexSection)
		{
			// 只保留有效的十六进制字符
			if (std::isxdigit(ch))
			{
				cleanHex += ch;
			}
		}
	}

	// 确保字节对数为偶数
	if (cleanHex.length() % 2 != 0)
	{
		cleanHex = cleanHex.substr(0, cleanHex.length() - 1);
	}

	// 两个字符一组转换
	for (size_t i = 0; i + 1 < cleanHex.size(); i += 2)
	{
		uint8_t high, low;
		if (HexCharToValue(cleanHex[i], high) && HexCharToValue(cleanHex[i + 1], low))
		{
			result.push_back((high << 4) | low);
		}
	}

	return result;
}

// ==================== 文本转换 ====================

std::string DataPresentationService::BytesToText(const std::vector<uint8_t>& data)
{
	if (data.empty()) return "";

	// ✅ 修复：智能编码检测和转换
	// 1. 首先尝试UTF-8解码
	bool isValidUtf8 = IsValidUtf8(data.data(), data.size());

	// 2. 如果不是UTF-8，尝试系统本地编码（通常是GBK/GB2312）
	if (!isValidUtf8)
	{
		try
		{
			// 将本地编码转换为UTF-8用于显示
			std::string localEncodedData(data.begin(), data.end());

			// 尝试使用CP_ACP（系统默认编码，通常是GBK）
			int wideCharCount = MultiByteToWideChar(CP_ACP, 0, localEncodedData.c_str(),
				static_cast<int>(localEncodedData.length()), nullptr, 0);

			if (wideCharCount > 0)
			{
				// 分配缓冲区并转换
				std::wstring wideText(wideCharCount, L'\0');
				int actualConverted = MultiByteToWideChar(CP_ACP, 0, localEncodedData.c_str(),
					static_cast<int>(localEncodedData.length()), &wideText[0], wideCharCount);

				if (actualConverted > 0)
				{
					// 验证转换结果是否包含有效字符（不是全问号）
					bool hasValidChars = false;
					for (int j = 0; j < actualConverted; ++j)
					{
						if (wideText[j] != L'?' && wideText[j] != 0)
						{
							hasValidChars = true;
							break;
						}
					}

					if (hasValidChars)
					{
						// 转换回UTF-8
						int utf8ByteCount = WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(),
							actualConverted, nullptr, 0, nullptr, nullptr);

						if (utf8ByteCount > 0)
						{
							std::string utf8Result(utf8ByteCount, '\0');
							WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), actualConverted,
								&utf8Result[0], utf8ByteCount, nullptr, nullptr);
							return utf8Result;
						}
					}
				}
			}

			// 如果CP_ACP失败，尝试使用CP_GB2312（简体中文）
			wideCharCount = MultiByteToWideChar(936, 0, localEncodedData.c_str(),  // 936 = GB2312
				static_cast<int>(localEncodedData.length()), nullptr, 0);

			if (wideCharCount > 0)
			{
				std::wstring wideText(wideCharCount, L'\0');
				int actualConverted = MultiByteToWideChar(936, 0, localEncodedData.c_str(),
					static_cast<int>(localEncodedData.length()), &wideText[0], wideCharCount);

				if (actualConverted > 0)
				{
					// 验证转换结果是否包含有效字符（不是全问号）
					bool hasValidChars = false;
					for (int j = 0; j < actualConverted; ++j)
					{
						if (wideText[j] != L'?' && wideText[j] != 0)
						{
							hasValidChars = true;
							break;
						}
					}

					if (hasValidChars)
					{
						int utf8ByteCount = WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(),
							actualConverted, nullptr, 0, nullptr, nullptr);

						if (utf8ByteCount > 0)
						{
							std::string utf8Result(utf8ByteCount, '\0');
							WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), actualConverted,
								&utf8Result[0], utf8ByteCount, nullptr, nullptr);
							return utf8Result;
						}
					}
				}
			}
		}
		catch (...)
		{
			// 转换失败，继续尝试其他方法
		}
	}
	else
	{
		// 确认是有效的UTF-8，直接返回
		return std::string(data.begin(), data.end());
	}

	// 3. 如果所有编码转换都失败，返回原始字节（可能显示为乱码，但保持数据完整性）
	return std::string(data.begin(), data.end());
}

std::vector<uint8_t> DataPresentationService::TextToBytes(const std::string& text)
{
	return std::vector<uint8_t>(text.begin(), text.end());
}

// ==================== 二进制检测 ====================

bool DataPresentationService::IsBinaryData(const uint8_t* data, size_t length, double threshold)
{
	if (length == 0) return false;

	size_t unprintableCount = 0;
	for (size_t i = 0; i < length; ++i)
	{
		if (!IsPrintable(data[i]))
		{
			unprintableCount++;
		}
	}

	double ratio = static_cast<double>(unprintableCount) / length;
	return ratio > threshold;
}

// ==================== 混合显示 ====================

std::string DataPresentationService::FormatHexAscii(const uint8_t* data, size_t length, size_t bytesPerLine)
{
	std::ostringstream hexStream;
	std::ostringstream asciiStream;

	for (size_t i = 0; i < length; i++)
	{
		uint8_t byte = data[i];

		// 添加偏移地址(每bytesPerLine字节一行)
		if (i % bytesPerLine == 0)
		{
			if (i > 0)
			{
				// 添加ASCII显示部分
				hexStream << "  |" << asciiStream.str() << "|\r\n";
				asciiStream.str("");
				asciiStream.clear();
			}

			// 输出地址偏移
			hexStream << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
				<< static_cast<unsigned int>(i) << ": ";
		}

		// 添加十六进制值
		hexStream << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<int>(byte) << " ";

		// 添加可打印字符到ASCII部分
		if (IsPrintable(byte))
		{
			asciiStream << static_cast<char>(byte);
		}
		else
		{
			asciiStream << ".";
		}
	}

	// 处理最后一行
	if (length % bytesPerLine != 0)
	{
		// 补齐空格
		size_t remain = bytesPerLine - (length % bytesPerLine);
		for (size_t i = 0; i < remain; i++)
		{
			hexStream << "   ";
		}
	}

	// 添加ASCII显示部分
	hexStream << "  |" << asciiStream.str() << "|";

	return hexStream.str();
}

// ==================== 显示更新准备 ====================

DataPresentationService::DisplayUpdate DataPresentationService::PrepareDisplay(
	const std::vector<uint8_t>& cache,
	bool hexMode,
	size_t maxDisplaySize)
{
	DisplayUpdate update;
	update.dataSize = cache.size();

	// 限制显示大小
	size_t displaySize = (std::min)(cache.size(), maxDisplaySize);

	if (hexMode)
	{
		// 十六进制模式：使用完整的格式化显示
		update.content = BytesToHex(cache.data(), displaySize);
		update.isBinary = false;
	}
	else
	{
		// 文本模式：先检测是否二进制
		update.isBinary = IsBinaryData(cache.data(), displaySize);
		if (update.isBinary)
		{
			// 二进制数据使用混合显示(十六进制+ASCII侧栏)
			update.content = FormatHexAscii(cache.data(), displaySize);
		}
		else
		{
			// 文本数据直接显示
			update.content = BytesToText(std::vector<uint8_t>(cache.begin(), cache.begin() + displaySize));
		}
	}

	return update;
}

// ==================== 编码检测 ====================

bool DataPresentationService::IsValidUtf8(const uint8_t* data, size_t length)
{
	if (length == 0) return true;

	size_t i = 0;
	while (i < length)
	{
		uint8_t byte = data[i];

		// 1字节字符 (0xxxxxxx)
		if ((byte & 0x80) == 0x00)
		{
			i++;
			continue;
		}

		// 2字节字符 (110xxxxx 10xxxxxx)
		if ((byte & 0xE0) == 0xC0)
		{
			if (i + 1 >= length) return false;
			if ((data[i + 1] & 0xC0) != 0x80) return false;
			// 额外检查：确保字节值在合理范围内
			if ((byte & 0xFC) == 0xC0) return false; // 过编码检查
			i += 2;
			continue;
		}

		// 3字节字符 (1110xxxx 10xxxxxx 10xxxxxx)
		if ((byte & 0xF0) == 0xE0)
		{
			if (i + 2 >= length) return false;
			if ((data[i + 1] & 0xC0) != 0x80) return false;
			if ((data[i + 2] & 0xC0) != 0x80) return false;

			// 额外检查：确保字节值在合理范围内
			if (byte == 0xE0 && (data[i + 1] & 0xE0) == 0x80) return false; // 过编码检查
			if (byte == 0xED && (data[i + 1] & 0xE0) == 0xA0) return false; // UTF-16代理项检查

			i += 3;
			continue;
		}

		// 4字节字符 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
		if ((byte & 0xF8) == 0xF0)
		{
			if (i + 3 >= length) return false;
			if ((data[i + 1] & 0xC0) != 0x80) return false;
			if ((data[i + 2] & 0xC0) != 0x80) return false;
			if ((data[i + 3] & 0xC0) != 0x80) return false;

			// 额外检查：确保字节值在合理范围内
			if (byte == 0xF0 && (data[i + 1] & 0xF0) == 0x80) return false; // 过编码检查
			if (byte > 0xF4) return false; // Unicode范围检查

			i += 4;
			continue;
		}

		// 无效的UTF-8起始字节
		return false;
	}

	// 额外检查：如果包含太多连续的连续字节，可能是错误编码
	size_t continuationBytes = 0;
	for (size_t j = 0; j < length; ++j)
	{
		if ((data[j] & 0xC0) == 0x80)
		{
			continuationBytes++;
			if (continuationBytes > length / 2)
			{
				return false; // 太多连续字节，可能是错误编码
			}
		}
	}

	return true;
}

// ==================== 辅助工具 ====================

std::string DataPresentationService::ByteToHexString(uint8_t byte)
{
	std::ostringstream oss;
	oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
		<< static_cast<int>(byte);
	return oss.str();
}

bool DataPresentationService::HexCharToValue(char c, uint8_t& value)
{
	if (c >= '0' && c <= '9')
	{
		value = c - '0';
		return true;
	}
	else if (c >= 'A' && c <= 'F')
	{
		value = c - 'A' + 10;
		return true;
	}
	else if (c >= 'a' && c <= 'f')
	{
		value = c - 'a' + 10;
		return true;
	}
	return false;
}

bool DataPresentationService::IsPrintable(uint8_t byte)
{
	// 可打印字符：0x20-0x7E
	// 特殊可打印：\t(0x09) \r(0x0D) \n(0x0A)
	return (byte >= 0x20 && byte <= 0x7E) ||
		byte == 0x09 || byte == 0x0D || byte == 0x0A;
}