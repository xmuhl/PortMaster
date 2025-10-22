#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ITransport.h"
#include "SerialTransport.h"
#include "ParallelTransport.h"
#include "UsbPrintTransport.h"
#include "NetworkPrintTransport.h"
#include "LoopbackTransport.h"
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

// TransportFactory 实现文件
// 负责创建不同类型的传输层实例并提供端口枚举功能

// 创建传输实例
std::unique_ptr<ITransport> TransportFactory::CreateTransport(const std::string& type)
{
	// 转换为小写以支持大小写不敏感的类型匹配
	std::string lowerType = type;
	std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
		[](unsigned char c) { return std::tolower(c); });

	if (lowerType == "serial" || lowerType == "com")
	{
		return std::make_unique<SerialTransport>();
	}
	else if (lowerType == "parallel" || lowerType == "lpt")
	{
		return std::make_unique<ParallelTransport>();
	}
	else if (lowerType == "usb" || lowerType == "usbprint")
	{
		return std::make_unique<UsbPrintTransport>();
	}
	else if (lowerType == "network" || lowerType == "tcp" || lowerType == "udp")
	{
		return std::make_unique<NetworkPrintTransport>();
	}
	else if (lowerType == "loopback" || lowerType == "test")
	{
		return std::make_unique<LoopbackTransport>();
	}
	else
	{
		// 未知传输类型，返回空指针
		return nullptr;
	}
}

// 枚举可用端口
std::vector<std::string> TransportFactory::EnumeratePorts(const std::string& type)
{
	std::vector<std::string> ports;

	// 转换为小写以支持大小写不敏感的类型匹配
	std::string lowerType = type;
	std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
		[](unsigned char c) { return std::tolower(c); });

	if (lowerType == "serial" || lowerType == "com")
	{
		// 枚举串口端口 (COM1-COM16)
		for (int i = 1; i <= 16; i++)
		{
			ports.push_back("COM" + std::to_string(i));
		}
	}
	else if (lowerType == "parallel" || lowerType == "lpt")
	{
		// 枚举并口端口 (LPT1-LPT3)
		for (int i = 1; i <= 3; i++)
		{
			ports.push_back("LPT" + std::to_string(i));
		}
	}
	else if (lowerType == "usb" || lowerType == "usbprint")
	{
		// USB打印机端口通常以 "USB" 开头
		// 实际实现中应该枚举系统中的USB打印设备
		ports.push_back("USB001");
		ports.push_back("USB002");
		ports.push_back("USB003");
	}
	else if (lowerType == "network" || lowerType == "tcp" || lowerType == "udp")
	{
		// 网络端口示例
		ports.push_back("192.168.1.100:9100"); // RAW打印端口
		ports.push_back("192.168.1.100:631");  // IPP端口
		ports.push_back("网络打印机");
	}
	else if (lowerType == "loopback" || lowerType == "test")
	{
		// 环回测试端口
		ports.push_back("LOOPBACK");
		ports.push_back("TEST");
	}

	return ports;
}

// 检测端口占用
bool TransportFactory::IsPortAvailable(const std::string& portName)
{
	// 参数验证
	if (portName.empty())
	{
		return false;
	}

	// ===== COM端口（串口）：尝试实际打开来检测占用 =====
	if (portName.substr(0, 3) == "COM" && portName.length() >= 4)
	{
		try
		{
			// 先检测格式有效性
			int portNum = std::stoi(portName.substr(3));
			if (!(portNum >= 1 && portNum <= 256))
			{
				return false; // 端口号范围无效
			}

			// 尝试使用CreateFile打开端口以检测占用状态
			std::string devicePath = "\\\\.\\" + portName;
			HANDLE hPort = CreateFileA(
				devicePath.c_str(),
				GENERIC_READ | GENERIC_WRITE,
				0,  // 不共享（独占）
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			);

			if (hPort != INVALID_HANDLE_VALUE)
			{
				// 端口可以打开，说明可用
				CloseHandle(hPort);
				return true;
			}
			else
			{
				// 无法打开，说明端口被占用或不存在
				DWORD error = GetLastError();
				// ERROR_FILE_NOT_FOUND(2) 或 ERROR_PATH_NOT_FOUND(3) 表示端口不存在
				// ERROR_ACCESS_DENIED(5) 表示端口被占用
				return false;
			}
		}
		catch (...)
		{
			return false;
		}
	}

	// ===== LPT端口（并口）：名称格式检测 =====
	// 注：LPT端口通常通过打印机驱动程序访问，实际检测较复杂
	if (portName.substr(0, 3) == "LPT" && portName.length() >= 4)
	{
		try
		{
			int portNum = std::stoi(portName.substr(3));
			// 检测范围
			if (portNum >= 1 && portNum <= 9)
			{
				// LPT端口存在则认为可用（完整的占用检测需要特殊驱动程序）
				return true;
			}
		}
		catch (...)
		{
			return false;
		}
	}

	// ===== USB端口：名称格式检测 =====
	if (portName.substr(0, 3) == "USB")
	{
		return true; // USB端口名称格式正确，认为可用
	}

	// ===== 网络端口（IP:PORT格式）=====
	if (portName.find(':') != std::string::npos)
	{
		// 简化检测：有冒号就认为是网络地址
		// 完整的网络端口检测需要Socket操作，当前采用名称检查
		return true;
	}

	// ===== 特殊测试端口 =====
	if (portName == "LOOPBACK" || portName == "TEST" || portName == "网络打印机")
	{
		return true;
	}

	// 未知端口格式
	return false;
}