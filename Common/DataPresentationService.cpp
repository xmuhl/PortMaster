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
	size_t displaySize = std::min(cache.size(), maxDisplaySize);

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
