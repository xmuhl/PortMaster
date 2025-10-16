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
	// 【阶段二实现】字节转十六进制
	// TODO: 阶段二迁移BytesToHex()完整逻辑
	std::ostringstream oss;
	for (size_t i = 0; i < length; ++i)
	{
		if (i > 0) oss << " ";
		oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<int>(data[i]);
	}
	return oss.str();
}

std::string DataPresentationService::BytesToHex(const std::vector<uint8_t>& data)
{
	return BytesToHex(data.data(), data.size());
}

std::vector<uint8_t> DataPresentationService::HexToBytes(const std::string& hex)
{
	// 【阶段二实现】十六进制转字节
	// TODO: 阶段二迁移HexToString()完整逻辑
	std::vector<uint8_t> result;
	std::string cleaned;

	// 提取十六进制字符
	for (char c : hex)
	{
		if (std::isxdigit(c))
		{
			cleaned += c;
		}
	}

	// 两个字符一组转换
	for (size_t i = 0; i + 1 < cleaned.size(); i += 2)
	{
		uint8_t high, low;
		if (HexCharToValue(cleaned[i], high) && HexCharToValue(cleaned[i + 1], low))
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
	// 【阶段二实现】二进制数据检测
	// TODO: 阶段二迁移二进制检测逻辑
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
	// 【阶段二实现】十六进制+ASCII混合显示
	// TODO: 阶段二迁移ExtractHexAsciiText()逻辑
	std::ostringstream oss;

	for (size_t i = 0; i < length; i += bytesPerLine)
	{
		// 十六进制部分
		size_t lineLength = std::min(bytesPerLine, length - i);
		for (size_t j = 0; j < lineLength; ++j)
		{
			oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<int>(data[i + j]) << " ";
		}

		// 填充空白（如果最后一行不足）
		for (size_t j = lineLength; j < bytesPerLine; ++j)
		{
			oss << "   ";
		}

		// 分隔符
		oss << "| ";

		// ASCII部分
		for (size_t j = 0; j < lineLength; ++j)
		{
			uint8_t byte = data[i + j];
			oss << (IsPrintable(byte) ? static_cast<char>(byte) : '.');
		}

		oss << "\n";
	}

	return oss.str();
}

// ==================== 显示更新准备 ====================

DataPresentationService::DisplayUpdate DataPresentationService::PrepareDisplay(
	const std::vector<uint8_t>& cache,
	bool hexMode,
	size_t maxDisplaySize)
{
	// 【阶段二实现】准备显示更新
	// TODO: 阶段二迁移UpdateSendDisplayFromCache/UpdateReceiveDisplayFromCache逻辑
	DisplayUpdate update;
	update.dataSize = cache.size();

	// 限制显示大小
	size_t displaySize = std::min(cache.size(), maxDisplaySize);

	if (hexMode)
	{
		// 十六进制模式
		update.content = BytesToHex(cache.data(), displaySize);
		update.isBinary = false;
	}
	else
	{
		// 文本模式：先检测是否二进制
		update.isBinary = IsBinaryData(cache.data(), displaySize);
		if (update.isBinary)
		{
			// 二进制数据使用混合显示
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
