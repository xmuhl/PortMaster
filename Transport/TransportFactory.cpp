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
	// 简化实现：检测端口名称格式的有效性
	if (portName.empty())
	{
		return false;
	}

	// 检测串口
	if (portName.substr(0, 3) == "COM" && portName.length() >= 4)
	{
		try
		{
			int portNum = std::stoi(portName.substr(3));
			return portNum >= 1 && portNum <= 256; // 有效的串口范围
		}
		catch (...)
		{
			return false;
		}
	}

	// 检测并口
	if (portName.substr(0, 3) == "LPT" && portName.length() >= 4)
	{
		try
		{
			int portNum = std::stoi(portName.substr(3));
			return portNum >= 1 && portNum <= 9; // 有效的并口范围
		}
		catch (...)
		{
			return false;
		}
	}

	// 检测USB端口
	if (portName.substr(0, 3) == "USB")
	{
		return true; // USB端口名称格式正确
	}

	// 检测网络端口（IP:PORT格式）
	if (portName.find(':') != std::string::npos)
	{
		return true; // 简化检测：有冒号就认为是网络地址
	}

	// 检测特殊端口名称
	if (portName == "LOOPBACK" || portName == "TEST" || portName == "网络打印机")
	{
		return true;
	}

	// 未知端口格式
	return false;
}