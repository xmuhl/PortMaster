// PortConfigPresenter.cpp - 端口配置呈现器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "PortConfigPresenter.h"
#include "resource.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/ParallelTransport.h"
#include "../Transport/UsbPrintTransport.h"
#include <cassert>

// 构造函数
PortConfigPresenter::PortConfigPresenter()
{
	// 初始化控件引用为nullptr
	memset(&m_controls, 0, sizeof(m_controls));
}

// 析构函数
PortConfigPresenter::~PortConfigPresenter()
{
}

// 初始化：绑定控件引用
void PortConfigPresenter::Initialize(const PortConfigControlRefs& controlRefs)
{
	m_controls = controlRefs;
	ValidateControlRefs();

	// 初始化端口类型下拉框
	InitializePortTypeCombo();
}

// 初始化端口类型下拉框
void PortConfigPresenter::InitializePortTypeCombo()
{
	if (!IsControlValid(m_controls.comboPortType))
		return;

	m_controls.comboPortType->ResetContent();
	m_controls.comboPortType->AddString(_T("串口"));
	m_controls.comboPortType->AddString(_T("并口"));
	m_controls.comboPortType->AddString(_T("USB打印"));
	m_controls.comboPortType->AddString(_T("网络打印"));
	m_controls.comboPortType->AddString(_T("回路测试"));
}

// 端口类型切换事件处理
void PortConfigPresenter::OnPortTypeChanged()
{
	// 更新端口参数
	UpdatePortParameters();

	// 触发回调
	if (m_portTypeChangedCallback)
	{
		m_portTypeChangedCallback();
	}
}

// 获取当前选择的端口类型
PortTypeIndex PortConfigPresenter::GetSelectedPortType() const
{
	if (!IsControlValid(m_controls.comboPortType))
		return PortTypeIndex::Loopback;

	int sel = m_controls.comboPortType->GetCurSel();
	return static_cast<PortTypeIndex>(sel);
}

// 设置选择的端口类型
void PortConfigPresenter::SetSelectedPortType(PortTypeIndex type)
{
	if (!IsControlValid(m_controls.comboPortType))
		return;

	m_controls.comboPortType->SetCurSel(static_cast<int>(type));
	UpdatePortParameters();
}

// 更新端口参数（根据端口类型）
void PortConfigPresenter::UpdatePortParameters()
{
	PortTypeIndex portType = GetSelectedPortType();

	// 清空端口列表
	if (IsControlValid(m_controls.comboPort))
	{
		m_controls.comboPort->ResetContent();
	}

	switch (portType)
	{
	case PortTypeIndex::Serial:
		UpdateSerialPortParameters();
		break;
	case PortTypeIndex::Parallel:
		UpdateParallelPortParameters();
		break;
	case PortTypeIndex::UsbPrint:
		UpdateUsbPrintPortParameters();
		break;
	case PortTypeIndex::NetworkPrint:
		UpdateNetworkPrintPortParameters();
		break;
	case PortTypeIndex::Loopback:
		UpdateLoopbackPortParameters();
		break;
	default:
		UpdateLoopbackPortParameters();
		break;
	}

	// 设置默认选择
	if (IsControlValid(m_controls.comboPort) && m_controls.comboPort->GetCount() > 0)
	{
		m_controls.comboPort->SetCurSel(0);
	}
}

// 更新串口参数
void PortConfigPresenter::UpdateSerialPortParameters()
{
	// 枚举串口
	std::vector<std::string> ports = EnumerateSerialPorts();

	// 如果没有找到串口，添加默认选项
	if (ports.empty())
	{
		ports.push_back("COM1");
		ports.push_back("COM2");
		ports.push_back("COM3");
		ports.push_back("COM4");
	}

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, ports);

	// 显示串口相关参数
	ShowSerialParameters(true);
}

// 更新并口参数
void PortConfigPresenter::UpdateParallelPortParameters()
{
	// 枚举并口
	std::vector<std::string> ports = EnumerateParallelPorts();

	// 如果没有找到并口，添加默认选项
	if (ports.empty())
	{
		ports.push_back("LPT1");
		ports.push_back("LPT2");
		ports.push_back("LPT3");
	}

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, ports);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 更新USB打印端口参数
void PortConfigPresenter::UpdateUsbPrintPortParameters()
{
	// 枚举USB打印端口
	std::vector<std::string> ports = EnumerateUsbPorts();

	// 如果没有找到USB打印端口，添加默认选项
	if (ports.empty())
	{
		ports.push_back("USB001");
		ports.push_back("USB002");
	}

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, ports);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 更新网络打印端口参数
void PortConfigPresenter::UpdateNetworkPrintPortParameters()
{
	// 添加网络打印地址选项
	std::vector<std::string> addresses;
	addresses.push_back("127.0.0.1:9100");
	addresses.push_back("192.168.1.100:9100");
	addresses.push_back("printer.local:9100");

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, addresses);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 更新回路测试参数
void PortConfigPresenter::UpdateLoopbackPortParameters()
{
	// 添加回路测试选项
	std::vector<std::string> options;
	options.push_back("Loopback");

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, options);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 显示/隐藏串口参数控件
void PortConfigPresenter::ShowSerialParameters(bool show)
{
	if (!IsControlValid(m_controls.parentDialog))
		return;

	int showCmd = show ? SW_SHOW : SW_HIDE;

	// 显示/隐藏串口参数控件
	CWnd* pBaudRate = m_controls.parentDialog->GetDlgItem(IDC_COMBO_BAUD_RATE);
	if (pBaudRate)
		pBaudRate->ShowWindow(showCmd);

	CWnd* pDataBits = m_controls.parentDialog->GetDlgItem(IDC_COMBO_DATA_BITS);
	if (pDataBits)
		pDataBits->ShowWindow(showCmd);

	CWnd* pParity = m_controls.parentDialog->GetDlgItem(IDC_COMBO_PARITY);
	if (pParity)
		pParity->ShowWindow(showCmd);

	CWnd* pStopBits = m_controls.parentDialog->GetDlgItem(IDC_COMBO_STOP_BITS);
	if (pStopBits)
		pStopBits->ShowWindow(showCmd);

	CWnd* pFlowControl = m_controls.parentDialog->GetDlgItem(IDC_COMBO_FLOW_CONTROL);
	if (pFlowControl)
		pFlowControl->ShowWindow(showCmd);
}

// 枚举串口
std::vector<std::string> PortConfigPresenter::EnumerateSerialPorts()
{
	return SerialTransport::EnumerateSerialPorts();
}

// 枚举并口
std::vector<std::string> PortConfigPresenter::EnumerateParallelPorts()
{
	return ParallelTransport::EnumerateParallelPorts();
}

// 枚举USB端口
std::vector<std::string> PortConfigPresenter::EnumerateUsbPorts()
{
	return UsbPrintTransport::EnumerateUsbPorts();
}

// 获取选择的端口名称
std::string PortConfigPresenter::GetSelectedPort() const
{
	if (!IsControlValid(m_controls.comboPort))
		return "";

	CString portText;
	m_controls.comboPort->GetWindowText(portText);

	CT2A utf8Port(portText, CP_UTF8);
	return std::string(utf8Port);
}

// 获取选择的波特率
int PortConfigPresenter::GetSelectedBaudRate() const
{
	if (!IsControlValid(m_controls.comboBaudRate))
		return 9600;

	CString text;
	m_controls.comboBaudRate->GetWindowText(text);
	return _ttoi(text);
}

// 获取选择的数据位
int PortConfigPresenter::GetSelectedDataBits() const
{
	if (!IsControlValid(m_controls.comboDataBits))
		return 8;

	CString text;
	m_controls.comboDataBits->GetWindowText(text);
	return _ttoi(text);
}

// 获取选择的校验位
std::string PortConfigPresenter::GetSelectedParity() const
{
	if (!IsControlValid(m_controls.comboParity))
		return "None";

	CString text;
	m_controls.comboParity->GetWindowText(text);

	CT2A utf8Text(text, CP_UTF8);
	return std::string(utf8Text);
}

// 获取选择的停止位
int PortConfigPresenter::GetSelectedStopBits() const
{
	if (!IsControlValid(m_controls.comboStopBits))
		return 1;

	CString text;
	m_controls.comboStopBits->GetWindowText(text);
	return _ttoi(text);
}

// 获取选择的流控
std::string PortConfigPresenter::GetSelectedFlowControl() const
{
	if (!IsControlValid(m_controls.comboFlowControl))
		return "None";

	CString text;
	m_controls.comboFlowControl->GetWindowText(text);

	CT2A utf8Text(text, CP_UTF8);
	return std::string(utf8Text);
}

// 设置端口选择
void PortConfigPresenter::SetPortSelection(const std::string& port)
{
	if (!IsControlValid(m_controls.comboPort))
		return;

	CA2T widePort(port.c_str(), CP_UTF8);
	CString portText(widePort);

	int index = FindComboBoxItem(m_controls.comboPort, portText);
	if (index != CB_ERR)
	{
		m_controls.comboPort->SetCurSel(index);
	}
	else
	{
		// 如果没有找到，尝试设置文本
		m_controls.comboPort->SetWindowText(portText);
	}
}

// 设置波特率
void PortConfigPresenter::SetBaudRate(int baudRate)
{
	if (!IsControlValid(m_controls.comboBaudRate))
		return;

	CString text;
	text.Format(_T("%d"), baudRate);

	int index = FindComboBoxItem(m_controls.comboBaudRate, text);
	if (index != CB_ERR)
	{
		m_controls.comboBaudRate->SetCurSel(index);
	}
}

// 设置数据位
void PortConfigPresenter::SetDataBits(int dataBits)
{
	if (!IsControlValid(m_controls.comboDataBits))
		return;

	CString text;
	text.Format(_T("%d"), dataBits);

	int index = FindComboBoxItem(m_controls.comboDataBits, text);
	if (index != CB_ERR)
	{
		m_controls.comboDataBits->SetCurSel(index);
	}
}

// 设置校验位
void PortConfigPresenter::SetParity(const std::string& parity)
{
	if (!IsControlValid(m_controls.comboParity))
		return;

	CA2T wideParity(parity.c_str(), CP_UTF8);
	CString text(wideParity);

	int index = FindComboBoxItem(m_controls.comboParity, text);
	if (index != CB_ERR)
	{
		m_controls.comboParity->SetCurSel(index);
	}
}

// 设置停止位
void PortConfigPresenter::SetStopBits(int stopBits)
{
	if (!IsControlValid(m_controls.comboStopBits))
		return;

	CString text;
	text.Format(_T("%d"), stopBits);

	int index = FindComboBoxItem(m_controls.comboStopBits, text);
	if (index != CB_ERR)
	{
		m_controls.comboStopBits->SetCurSel(index);
	}
}

// 设置流控
void PortConfigPresenter::SetFlowControl(const std::string& flowControl)
{
	if (!IsControlValid(m_controls.comboFlowControl))
		return;

	CA2T wideFlowControl(flowControl.c_str(), CP_UTF8);
	CString text(wideFlowControl);

	int index = FindComboBoxItem(m_controls.comboFlowControl, text);
	if (index != CB_ERR)
	{
		m_controls.comboFlowControl->SetCurSel(index);
	}
}

// 设置端口类型变更回调
void PortConfigPresenter::SetPortTypeChangedCallback(std::function<void()> callback)
{
	m_portTypeChangedCallback = callback;
}

// 设置端口号变更回调
void PortConfigPresenter::SetPortChangedCallback(std::function<void()> callback)
{
	m_portChangedCallback = callback;
}

// 验证控件引用有效性
void PortConfigPresenter::ValidateControlRefs() const
{
	// 验证关键控件
	assert(IsControlValid(m_controls.parentDialog) && "父对话框指针无效");
	assert(IsControlValid(m_controls.comboPortType) && "端口类型下拉框指针无效");
	assert(IsControlValid(m_controls.comboPort) && "端口号下拉框指针无效");
}

// 检查单个控件是否有效
bool PortConfigPresenter::IsControlValid(CWnd* control) const
{
	return (control != nullptr && control->GetSafeHwnd() != nullptr);
}

// 填充下拉框（std::string版本）
void PortConfigPresenter::PopulateComboBox(CComboBox* combo, const std::vector<std::string>& items)
{
	if (!IsControlValid(combo))
		return;

	combo->ResetContent();
	for (const auto& item : items)
	{
		CA2T wideItem(item.c_str(), CP_UTF8);
		combo->AddString(CString(wideItem));
	}
}

// 填充下拉框（CString版本）
void PortConfigPresenter::PopulateComboBox(CComboBox* combo, const std::vector<CString>& items)
{
	if (!IsControlValid(combo))
		return;

	combo->ResetContent();
	for (const auto& item : items)
	{
		combo->AddString(item);
	}
}

// 查找下拉框项目索引
int PortConfigPresenter::FindComboBoxItem(CComboBox* combo, const CString& text) const
{
	if (!IsControlValid(combo))
		return CB_ERR;

	return combo->FindStringExact(0, text);
}
