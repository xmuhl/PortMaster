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
	// 先断开已有连接
	if (m_isConnected)
	{
		Disconnect();
	}

	// 创建Transport
	m_transport = CreateTransportByType(config);
	if (!m_transport || !m_transport->IsOpen())
	{
		OnError("创建或打开传输通道失败");
		return false;
	}

	// 如果启用可靠模式,创建ReliableChannel
	m_useReliableMode = useReliableMode;
	if (useReliableMode)
	{
		m_reliableChannel = std::make_unique<ReliableChannel>();
		if (!m_reliableChannel->Initialize(m_transport, m_reliableConfig))
		{
			OnError("可靠传输通道初始化失败");
			m_reliableChannel.reset();
			m_transport->Close();
			m_transport.reset();
			return false;
		}

		// 连接ReliableChannel
		if (!m_reliableChannel->Connect())
		{
			OnError("可靠传输通道连接失败");
			m_reliableChannel->Shutdown();
			m_reliableChannel.reset();
			m_transport->Close();
			m_transport.reset();
			return false;
		}

		// 配置日志级别
		ConfigureReliableLogging(false);
	}

	m_isConnected = true;
	return true;
}

void PortSessionController::Disconnect()
{
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
	if (m_receiveThread.joinable())
	{
		return;
	}

	m_receiveThread = std::thread(&PortSessionController::ReceiveThreadProc, this);
}

void PortSessionController::StopReceiveSession()
{
	m_isConnected = false;

	if (m_receiveThread.joinable())
	{
		m_receiveThread.join();
	}
}

// ==================== ReliableChannel管理 ====================

void PortSessionController::SetReliableConfig(const ReliableConfig& config)
{
	// 保存配置，在Connect时使用
	m_reliableConfig = config;
}

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
	std::shared_ptr<ITransport> transport = nullptr;

	switch (config.portType)
	{
	case PortType::PORT_TYPE_SERIAL:
	{
		// 创建串口传输对象
		auto serialTransport = std::make_shared<SerialTransport>();
		SerialConfig serialConfig;
		serialConfig.portName = config.portName;
		serialConfig.baudRate = config.baudRate;
		serialConfig.dataBits = config.dataBits;
		serialConfig.parity = config.parity;
		serialConfig.stopBits = config.stopBits;
		serialConfig.flowControl = config.flowControl;
		serialConfig.readTimeout = config.readTimeout;
		serialConfig.writeTimeout = config.writeTimeout;

		if (serialTransport->Open(serialConfig) == TransportError::Success)
		{
			transport = serialTransport;
		}
		break;
	}

	case PortType::PORT_TYPE_PARALLEL:
	{
		// 创建并口传输对象
		auto parallelTransport = std::make_shared<ParallelTransport>();
		ParallelPortConfig parallelConfig;
		parallelConfig.deviceName = config.portName;
		parallelConfig.readTimeout = config.readTimeout;
		parallelConfig.writeTimeout = config.writeTimeout;

		if (parallelTransport->Open(parallelConfig) == TransportError::Success)
		{
			transport = parallelTransport;
		}
		break;
	}

	case PortType::PORT_TYPE_USB_PRINT:
	{
		// 创建USB打印传输对象
		auto usbTransport = std::make_shared<UsbPrintTransport>();
		UsbPrintConfig usbConfig;
		usbConfig.deviceName = config.portName;
		usbConfig.readTimeout = config.readTimeout;
		usbConfig.writeTimeout = config.writeTimeout;

		if (usbTransport->Open(usbConfig) == TransportError::Success)
		{
			transport = usbTransport;
		}
		break;
	}

	case PortType::PORT_TYPE_NETWORK_PRINT:
	{
		// 创建网络打印传输对象
		auto networkTransport = std::make_shared<NetworkPrintTransport>();
		NetworkPrintConfig networkConfig;

		// 解析地址和端口
		size_t colonPos = config.portName.find(':');
		if (colonPos != std::string::npos)
		{
			networkConfig.hostname = config.portName.substr(0, colonPos);
			networkConfig.port = static_cast<WORD>(std::stoi(config.portName.substr(colonPos + 1)));
		}
		else
		{
			networkConfig.hostname = config.portName;
			networkConfig.port = 9100; // 默认网络打印端口
		}
		networkConfig.connectTimeout = 5000;
		networkConfig.sendTimeout = config.writeTimeout;
		networkConfig.receiveTimeout = config.readTimeout;

		if (networkTransport->Open(networkConfig) == TransportError::Success)
		{
			transport = networkTransport;
		}
		break;
	}

	case PortType::PORT_TYPE_LOOPBACK:
	{
		// 创建回路测试传输对象
		auto loopbackTransport = std::make_shared<LoopbackTransport>();
		if (loopbackTransport->Open(config) == TransportError::Success)
		{
			transport = loopbackTransport;
		}
		break;
	}

	default:
		OnError("不支持的端口类型");
		break;
	}

	return transport;
}

void PortSessionController::ReceiveThreadProc()
{
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
