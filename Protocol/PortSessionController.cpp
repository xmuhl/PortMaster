#pragma execution_character_set("utf-8")

#include "pch.h"
#include "PortSessionController.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/ParallelTransport.h"
#include "../Transport/UsbPrintTransport.h"
#include "../Transport/NetworkPrintTransport.h"
#include "../Transport/LoopbackTransport.h"
#include <chrono>

// ==================== 构造与析构 ====================

PortSessionController::PortSessionController()
	: m_isConnected(false)
	, m_useReliableMode(false)
{
	// 初始化默认可靠传输配置
	m_reliableConfig.version = RELIABLE_PROTOCOL_VERSION;
	m_reliableConfig.windowSize = RELIABLE_WINDOW_SIZE;
	m_reliableConfig.maxRetries = RELIABLE_MAX_RETRIES;
	m_reliableConfig.timeoutBase = RELIABLE_TIMEOUT_BASE;
	m_reliableConfig.timeoutMax = RELIABLE_TIMEOUT_MAX;
	m_reliableConfig.maxPayloadSize = RELIABLE_MAX_PAYLOAD_SIZE;
}

PortSessionController::~PortSessionController()
{
	Disconnect();
}

// ==================== 连接管理 ====================

bool PortSessionController::Connect(const TransportConfig& config, bool useReliableMode)
{
	// 【阶段二实现】根据config创建Transport，可选创建ReliableChannel
	// TODO: 阶段二迁移CreateTransport()完整逻辑
	return false;
}

void PortSessionController::Disconnect()
{
	// 【阶段二实现】停止接收、销毁ReliableChannel、关闭Transport
	// TODO: 阶段二迁移DestroyTransport()完整逻辑
	StopReceiveSession();

	if (m_reliableChannel)
	{
		m_reliableChannel->Shutdown();
		m_reliableChannel.reset();
	}

	if (m_transport)
	{
		m_transport->Close();
		m_transport.reset();
	}

	m_isConnected = false;
}

bool PortSessionController::IsConnected() const
{
	return m_isConnected;
}

// ==================== 接收会话管理 ====================

void PortSessionController::StartReceiveSession()
{
	// 【阶段二实现】启动接收线程
	// TODO: 阶段二迁移StartReceiveThread()完整逻辑
	if (m_receiveThread.joinable())
	{
		return;
	}

	m_receiveThread = std::thread(&PortSessionController::ReceiveThreadProc, this);
}

void PortSessionController::StopReceiveSession()
{
	// 【阶段二实现】停止接收线程
	// TODO: 阶段二迁移StopReceiveThread()完整逻辑
	m_isConnected = false;

	if (m_receiveThread.joinable())
	{
		m_receiveThread.join();
	}
}

// ==================== ReliableChannel管理 ====================

std::shared_ptr<ReliableChannel> PortSessionController::GetReliableChannel()
{
	// 注意：ReliableChannel是unique_ptr，但为了兼容现有代码返回shared_ptr
	// 实际上可以考虑将m_reliableChannel改为shared_ptr
	return std::shared_ptr<ReliableChannel>(m_reliableChannel.get(), [](ReliableChannel*) {
		// 空删除器，不实际删除（由unique_ptr管理）
	});
}

void PortSessionController::ConfigureReliableLogging(bool verbose)
{
	// 【阶段二实现】配置ReliableChannel日志级别
	// TODO: 阶段二迁移ConfigureReliableLogging()逻辑
	if (m_reliableChannel)
	{
		m_reliableChannel->SetVerboseLoggingEnabled(verbose);
	}
}

// ==================== 回调接口 ====================

void PortSessionController::SetDataCallback(DataCallback callback)
{
	m_dataCallback = callback;
}

void PortSessionController::SetErrorCallback(ErrorCallback callback)
{
	m_errorCallback = callback;
}

// ==================== 获取底层Transport ====================

std::shared_ptr<ITransport> PortSessionController::GetTransport()
{
	return m_transport;
}

// ==================== 内部方法 ====================

std::shared_ptr<ITransport> PortSessionController::CreateTransportByType(const TransportConfig& config)
{
	// 【阶段二实现】根据配置创建不同类型的Transport
	// TODO: 阶段二迁移CreateTransport()中的switch逻辑
	return nullptr;
}

void PortSessionController::ReceiveThreadProc()
{
	// 【阶段二实现】接收线程主循环
	// TODO: 阶段二迁移StartReceiveThread()中的lambda逻辑
	std::vector<uint8_t> buffer(4096);

	while (m_isConnected)
	{
		try
		{
			if (m_useReliableMode && m_reliableChannel && m_reliableChannel->IsConnected())
			{
				// 可靠模式接收
				std::vector<uint8_t> data;
				if (m_reliableChannel->Receive(data, 100))
				{
					OnDataReceived(data);
				}
			}
			else if (m_transport && m_transport->IsOpen())
			{
				// 直接模式接收
				std::fill(buffer.begin(), buffer.end(), 0);
				size_t bytesRead = 0;
				auto result = m_transport->Read(buffer.data(), buffer.size(), &bytesRead, 100);
				if (result == TransportError::Success && bytesRead > 0)
				{
					std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
					OnDataReceived(data);
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
				}
			}
		}
		catch (const std::exception& e)
		{
			OnError(e.what());
		}
	}
}

void PortSessionController::OnDataReceived(const std::vector<uint8_t>& data)
{
	if (m_dataCallback)
	{
		m_dataCallback(data);
	}
}

void PortSessionController::OnError(const std::string& error)
{
	if (m_errorCallback)
	{
		m_errorCallback(error);
	}
}
