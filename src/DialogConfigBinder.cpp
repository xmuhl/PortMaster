#pragma execution_character_set("utf-8")

#include "pch.h"
#include "DialogConfigBinder.h"
#include "resource.h"
#include <afxwin.h>

// ==================== 构造与析构 ====================

DialogConfigBinder::DialogConfigBinder(CDialog* dialog, ConfigStore& configStore)
	: m_dialog(dialog)
	, m_configStore(configStore)
{
	if (m_dialog == nullptr)
	{
		throw std::invalid_argument("Dialog pointer cannot be null");
	}
}

DialogConfigBinder::~DialogConfigBinder()
{
}

// ==================== 双向绑定接口 ====================

bool DialogConfigBinder::LoadToUI()
{
	// 从ConfigStore加载配置到UI控件
	try
	{
		const PortMasterConfig& config = m_configStore.GetConfig();

		// 加载串口配置
		LoadSerialConfigToUI(config.serial);

		// 加载UI配置
		LoadUIConfigToDialog(config.ui);

		// 加载协议配置
		LoadProtocolConfigToUI(config.protocol);

		return true;
	}
	catch (const std::exception& e)
	{
		NotifyError("加载配置到UI失败: " + std::string(e.what()));
		return false;
	}
}

bool DialogConfigBinder::SaveFromUI()
{
	// 从UI控件读取配置并保存到ConfigStore
	try
	{
		PortMasterConfig config = m_configStore.GetConfig();

		// 读取串口配置
		if (!ReadSerialConfigFromUI(config.serial))
		{
			NotifyError("读取串口配置失败");
			return false;
		}

		// 读取UI配置
		if (!ReadUIConfigFromDialog(config.ui))
		{
			NotifyError("读取UI配置失败");
			return false;
		}

		// 读取协议配置
		if (!ReadProtocolConfigFromUI(config.protocol))
		{
			NotifyError("读取协议配置失败");
			return false;
		}

		// 验证配置有效性
		std::string errorMessage;
		if (!ValidateSerialConfig(config.serial, errorMessage))
		{
			NotifyError("串口配置无效: " + errorMessage);
			return false;
		}

		// 更新配置到存储
		m_configStore.SetConfig(config);

		// 触发配置变更回调
		NotifyConfigChanged();

		return true;
	}
	catch (const std::exception& e)
	{
		NotifyError("保存配置失败: " + std::string(e.what()));
		return false;
	}
}

// ==================== 配置访问接口 ====================

SerialConfig DialogConfigBinder::GetSerialConfig() const
{
	return m_configStore.GetConfig().serial;
}

UIConfig DialogConfigBinder::GetUIConfig() const
{
	return m_configStore.GetConfig().ui;
}

ProtocolConfig DialogConfigBinder::GetProtocolConfig() const
{
	return m_configStore.GetConfig().protocol;
}

PortMasterConfig DialogConfigBinder::GetConfig() const
{
	return m_configStore.GetConfig();
}

bool DialogConfigBinder::SetSerialConfig(const SerialConfig& config)
{
	try
	{
		PortMasterConfig fullConfig = m_configStore.GetConfig();
		fullConfig.serial = config;
		m_configStore.SetConfig(fullConfig);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool DialogConfigBinder::SetUIConfig(const UIConfig& config)
{
	try
	{
		PortMasterConfig fullConfig = m_configStore.GetConfig();
		fullConfig.ui = config;
		m_configStore.SetConfig(fullConfig);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool DialogConfigBinder::SetProtocolConfig(const ProtocolConfig& config)
{
	try
	{
		PortMasterConfig fullConfig = m_configStore.GetConfig();
		fullConfig.protocol = config;
		m_configStore.SetConfig(fullConfig);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

// ==================== 单项配置绑定（高频操作优化）====================

void DialogConfigBinder::BindPortName(const std::string& portName)
{
	SetControlText(IDC_COMBO_PORT, std::wstring(portName.begin(), portName.end()));
}

std::string DialogConfigBinder::ReadPortName() const
{
	std::wstring portName = GetControlText(IDC_COMBO_PORT);
	return std::string(portName.begin(), portName.end());
}

void DialogConfigBinder::BindBaudRate(int baudRate)
{
	wchar_t buffer[32];
	swprintf(buffer, 32, L"%d", baudRate);
	SetControlText(IDC_COMBO_BAUD_RATE, buffer);
}

int DialogConfigBinder::ReadBaudRate() const
{
	std::wstring baudRateText = GetControlText(IDC_COMBO_BAUD_RATE);
	return _wtoi(baudRateText.c_str());
}

void DialogConfigBinder::BindTransmissionMode(bool useReliableMode)
{
	CWnd* radioReliable = GetControl(IDC_RADIO_RELIABLE);
	CWnd* radioDirect = GetControl(IDC_RADIO_DIRECT);

	if (radioReliable && radioDirect)
	{
		((CButton*)radioReliable)->SetCheck(useReliableMode ? BST_CHECKED : BST_UNCHECKED);
		((CButton*)radioDirect)->SetCheck(useReliableMode ? BST_UNCHECKED : BST_CHECKED);
	}
}

bool DialogConfigBinder::ReadTransmissionMode() const
{
	CWnd* radioReliable = GetControl(IDC_RADIO_RELIABLE);
	if (radioReliable)
	{
		return ((CButton*)radioReliable)->GetCheck() == BST_CHECKED;
	}
	return false;
}

void DialogConfigBinder::BindHexDisplayMode(bool hexDisplay)
{
	CWnd* checkHex = GetControl(IDC_CHECK_HEX);
	if (checkHex)
	{
		((CButton*)checkHex)->SetCheck(hexDisplay ? BST_CHECKED : BST_UNCHECKED);
	}
}

bool DialogConfigBinder::ReadHexDisplayMode() const
{
	CWnd* checkHex = GetControl(IDC_CHECK_HEX);
	if (checkHex)
	{
		return ((CButton*)checkHex)->GetCheck() == BST_CHECKED;
	}
	return false;
}

// ==================== 配置验证接口 ====================

bool DialogConfigBinder::ValidateSerialConfig(const SerialConfig& config, std::string& errorMessage) const
{
	// 检查端口名非空
	if (config.portName.empty())
	{
		errorMessage = "端口名不能为空";
		return false;
	}

	// 检查波特率在有效范围
	if (config.baudRate < 300 || config.baudRate > 921600)
	{
		errorMessage = "波特率必须在300~921600范围内";
		return false;
	}

	// 检查数据位有效值
	if (config.dataBits < 5 || config.dataBits > 8)
	{
		errorMessage = "数据位必须是5/6/7/8";
		return false;
	}

	// 检查停止位枚举值
	if (config.stopBits != ONESTOPBIT &&
		config.stopBits != ONE5STOPBITS &&
		config.stopBits != TWOSTOPBITS)
	{
		errorMessage = "停止位枚举值无效";
		return false;
	}

	// 检查校验位枚举值
	if (config.parity != NOPARITY &&
		config.parity != ODDPARITY &&
		config.parity != EVENPARITY &&
		config.parity != MARKPARITY &&
		config.parity != SPACEPARITY)
	{
		errorMessage = "校验位枚举值无效";
		return false;
	}

	return true;
}

bool DialogConfigBinder::ValidateUIConfig(const UIConfig& config, std::string& errorMessage) const
{
	// 检查窗口尺寸有效性
	if (config.windowWidth < 0 || config.windowHeight < 0)
	{
		errorMessage = "窗口尺寸不能为负数";
		return false;
	}

	return true;
}

// ==================== 回调接口 ====================

void DialogConfigBinder::SetConfigChangedCallback(ConfigChangedCallback callback)
{
	m_configChangedCallback = callback;
}

void DialogConfigBinder::SetErrorCallback(ErrorCallback callback)
{
	m_errorCallback = callback;
}

// ==================== 工具方法 ====================

void DialogConfigBinder::ApplyWindowPosition(int windowX, int windowY, int windowWidth, int windowHeight)
{
	if (m_dialog && windowWidth > 0 && windowHeight > 0 &&
		windowX != CW_USEDEFAULT && windowY != CW_USEDEFAULT)
	{
		m_dialog->SetWindowPos(nullptr,
			windowX, windowY,
			windowWidth, windowHeight,
			SWP_NOZORDER);
	}
}

void DialogConfigBinder::CaptureWindowPosition(int& windowX, int& windowY, int& windowWidth, int& windowHeight) const
{
	if (m_dialog)
	{
		CRect windowRect;
		m_dialog->GetWindowRect(&windowRect);
		windowX = windowRect.left;
		windowY = windowRect.top;
		windowWidth = windowRect.Width();
		windowHeight = windowRect.Height();
	}
}

// ==================== 内部方法 ====================

void DialogConfigBinder::LoadSerialConfigToUI(const SerialConfig& config)
{
	// 设置端口名
	BindPortName(config.portName);

	// 设置波特率
	BindBaudRate(config.baudRate);

	// 设置数据位
	wchar_t dataBitsBuffer[16];
	swprintf(dataBitsBuffer, 16, L"%d", config.dataBits);
	SetControlText(IDC_COMBO_DATA_BITS, dataBitsBuffer);

	// 设置校验位
	SetControlText(IDC_COMBO_PARITY, ParityToString(config.parity));

	// 设置停止位
	SetControlText(IDC_COMBO_STOP_BITS, StopBitsToString(config.stopBits));

	// 设置流控制
	std::wstring flowControl = (config.flowControl == 0) ? L"None" : L"Hardware";
	SetControlText(IDC_COMBO_FLOW_CONTROL, flowControl);

	// 设置超时
	wchar_t timeoutBuffer[16];
	swprintf(timeoutBuffer, 16, L"%d", config.readTimeout);
	SetControlText(IDC_EDIT_TIMEOUT, timeoutBuffer);
}

bool DialogConfigBinder::ReadSerialConfigFromUI(SerialConfig& config)
{
	// 获取端口名
	config.portName = ReadPortName();

	// 获取波特率
	config.baudRate = ReadBaudRate();

	// 获取数据位
	std::wstring dataBitsText = GetControlText(IDC_COMBO_DATA_BITS);
	config.dataBits = (BYTE)_wtoi(dataBitsText.c_str());

	// 获取校验位
	std::wstring parityText = GetControlText(IDC_COMBO_PARITY);
	config.parity = StringToParity(parityText);

	// 获取停止位
	std::wstring stopBitsText = GetControlText(IDC_COMBO_STOP_BITS);
	config.stopBits = StringToStopBits(stopBitsText);

	// 获取流控制
	std::wstring flowControlText = GetControlText(IDC_COMBO_FLOW_CONTROL);
	config.flowControl = (flowControlText == L"None") ? 0 : 1;

	// 获取超时
	std::wstring timeoutText = GetControlText(IDC_EDIT_TIMEOUT);
	config.readTimeout = _wtoi(timeoutText.c_str());
	config.writeTimeout = config.readTimeout;

	return true;
}

void DialogConfigBinder::LoadUIConfigToDialog(const UIConfig& config)
{
	// 设置十六进制显示选项
	BindHexDisplayMode(config.hexDisplay);

	// 更新窗口位置（如果有保存）
	ApplyWindowPosition(config.windowX, config.windowY, config.windowWidth, config.windowHeight);
}

bool DialogConfigBinder::ReadUIConfigFromDialog(UIConfig& config)
{
	// 保存十六进制显示选项
	config.hexDisplay = ReadHexDisplayMode();

	// 保存窗口位置
	CaptureWindowPosition(config.windowX, config.windowY, config.windowWidth, config.windowHeight);

	return true;
}

void DialogConfigBinder::LoadProtocolConfigToUI(const ProtocolConfig& config)
{
	// 根据协议配置设置传输模式
	bool useReliableMode = (config.windowSize > 1);
	BindTransmissionMode(useReliableMode);
}

bool DialogConfigBinder::ReadProtocolConfigFromUI(ProtocolConfig& config)
{
	// 获取可靠模式（更新协议配置）
	if (ReadTransmissionMode())
	{
		config.windowSize = 4; // 启用可靠模式
		config.maxRetries = 3;
	}
	else
	{
		config.windowSize = 1; // 禁用可靠模式
		config.maxRetries = 0;
	}

	return true;
}

std::wstring DialogConfigBinder::ParityToString(BYTE parity) const
{
	switch (parity)
	{
	case NOPARITY:   return L"None";
	case ODDPARITY:  return L"Odd";
	case EVENPARITY: return L"Even";
	case MARKPARITY: return L"Mark";
	case SPACEPARITY: return L"Space";
	default:         return L"None";
	}
}

BYTE DialogConfigBinder::StringToParity(const std::wstring& parityText) const
{
	if (parityText == L"None")        return NOPARITY;
	else if (parityText == L"Odd")    return ODDPARITY;
	else if (parityText == L"Even")   return EVENPARITY;
	else if (parityText == L"Mark")   return MARKPARITY;
	else if (parityText == L"Space")  return SPACEPARITY;
	else                              return NOPARITY;
}

std::wstring DialogConfigBinder::StopBitsToString(BYTE stopBits) const
{
	switch (stopBits)
	{
	case ONESTOPBIT:   return L"1";
	case ONE5STOPBITS: return L"1.5";
	case TWOSTOPBITS:  return L"2";
	default:           return L"1";
	}
}

BYTE DialogConfigBinder::StringToStopBits(const std::wstring& stopBitsText) const
{
	if (stopBitsText == L"1")         return ONESTOPBIT;
	else if (stopBitsText == L"1.5")  return ONE5STOPBITS;
	else if (stopBitsText == L"2")    return TWOSTOPBITS;
	else                              return ONESTOPBIT;
}

CWnd* DialogConfigBinder::GetControl(int controlID) const
{
	if (m_dialog)
	{
		return m_dialog->GetDlgItem(controlID);
	}
	return nullptr;
}

bool DialogConfigBinder::SetControlText(int controlID, const std::wstring& text)
{
	if (m_dialog)
	{
		m_dialog->SetDlgItemText(controlID, CString(text.c_str()));
		return true;
	}
	return false;
}

std::wstring DialogConfigBinder::GetControlText(int controlID) const
{
	if (m_dialog)
	{
		CString text;
		m_dialog->GetDlgItemText(controlID, text);
		return std::wstring(text.GetString());
	}
	return L"";
}

void DialogConfigBinder::NotifyConfigChanged()
{
	if (m_configChangedCallback)
	{
		m_configChangedCallback();
	}
}

void DialogConfigBinder::NotifyError(const std::string& errorMessage)
{
	if (m_errorCallback)
	{
		m_errorCallback(errorMessage);
	}
}
