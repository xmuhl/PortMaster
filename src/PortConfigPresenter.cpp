// PortConfigPresenter.cpp - 端口配置呈现器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "PortConfigPresenter.h"
#include "resource.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/ParallelTransport.h"
#include "../Transport/UsbPrintTransport.h"
#include "NetworkPrinterConfigDialog.h"
#include "../Common/CommonTypes.h"
#include <cassert>
#include <windows.h>
#include <thread>

// 构造函数
PortConfigPresenter::PortConfigPresenter()
{
	// 初始化控件引用为nullptr
 memset(&m_controls, 0, sizeof(m_controls));

	// 初始化端口扫描器
	m_portScanner = std::make_unique<PortScanner>();
}

// 析构函数
PortConfigPresenter::~PortConfigPresenter()
{
}

// =================== PortScanner类实现 ===================

// 构造函数
PortScanner::PortScanner()
	: m_shouldStop(false)
	, m_isScanning(false)
{
}

// 析构函数
PortScanner::~PortScanner()
{
	StopScan();
}

// 开始异步扫描
void PortScanner::StartScan(PortTypeIndex portType,
	PortScanProgressCallback progressCallback,
	PortScanCompleteCallback completeCallback)
{
	// 如果正在扫描，先停止
	if (m_isScanning)
	{
		StopScan();
	}

	// 重置状态
	m_shouldStop = false;
	m_isScanning = true;
	m_scanResults.clear();

	// 启动后台线程执行扫描
	std::thread scanThread([this, portType, progressCallback, completeCallback]()
		{
			ScanWorker(portType, progressCallback, completeCallback);
		});

	scanThread.detach();
}

// 停止扫描
void PortScanner::StopScan()
{
	m_shouldStop = true;
}

// 检查是否正在扫描
bool PortScanner::IsScanning() const
{
	return m_isScanning;
}

// 扫描工作线程函数
void PortScanner::ScanWorker(PortTypeIndex portType,
	PortScanProgressCallback progressCallback,
	PortScanCompleteCallback completeCallback)
{
	PortScanResult result;
	result.success = true;
	result.error = "";

	try
	{
		// 根据端口类型执行不同的扫描
		switch (portType)
		{
		case PortTypeIndex::Serial:
			ScanSerialPorts(progressCallback);
			break;
		case PortTypeIndex::Parallel:
			ScanParallelPorts(progressCallback);
			break;
		case PortTypeIndex::UsbPrint:
			ScanUsbPorts(progressCallback);
			break;
		case PortTypeIndex::NetworkPrint:
			ScanNetworkPorts(progressCallback);
			break;
		default:
			result.success = false;
			result.error = "未知的端口类型";
			break;
		}
	}
	catch (const std::exception& e)
	{
		result.success = false;
		result.error = e.what();
	}
	catch (...)
	{
		result.success = false;
		result.error = "未知异常";
	}

	// 标记扫描完成
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_isScanning = false;
	}

	// 调用完成回调
	if (completeCallback)
	{
		completeCallback(result);
	}
}

// 扫描串口
void PortScanner::ScanSerialPorts(PortScanProgressCallback progressCallback)
{
	auto portInfos = SerialTransport::EnumerateSerialPortsWithInfo();

	for (size_t i = 0; i < portInfos.size(); ++i)
	{
		if (m_shouldStop) break;

		// 更新进度
		if (progressCallback)
		{
			PortScanProgress progress;
			progress.currentPort = static_cast<int>(i);
			progress.totalPorts = static_cast<int>(portInfos.size());
			progress.status = "扫描串口: " + portInfos[i].portName;
			progressCallback(progress);
		}

		// 等待一小段时间，避免过于频繁的更新
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// 生成显示文本
		std::string displayText = portInfos[i].portName;
		if (!portInfos[i].displayName.empty())
		{
			displayText += " - " + portInfos[i].displayName;
		}
		if (portInfos[i].IsConnected())
		{
			displayText += " (已连接)";
		}
		else if (portInfos[i].status == PortStatus::Available)
		{
			displayText += " (可用)";
		}

		m_scanResults.push_back(displayText);
	}
}

// 扫描并口
void PortScanner::ScanParallelPorts(PortScanProgressCallback progressCallback)
{
	auto portInfos = ParallelTransport::EnumerateParallelPortsWithInfo();

	for (size_t i = 0; i < portInfos.size(); ++i)
	{
		if (m_shouldStop) break;

		// 更新进度
		if (progressCallback)
		{
			PortScanProgress progress;
			progress.currentPort = static_cast<int>(i);
			progress.totalPorts = static_cast<int>(portInfos.size());
			progress.status = "扫描并口: " + portInfos[i].portName;
			progressCallback(progress);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// 生成显示文本
		std::string displayText = portInfos[i].portName;
		if (!portInfos[i].displayName.empty())
		{
			displayText += " - " + portInfos[i].displayName;
		}
		if (portInfos[i].IsConnected())
		{
			displayText += " (已连接)";
		}
		else if (portInfos[i].status == PortStatus::Available)
		{
			displayText += " (可用)";
		}

		m_scanResults.push_back(displayText);
	}
}

// 扫描USB端口
void PortScanner::ScanUsbPorts(PortScanProgressCallback progressCallback)
{
	auto portInfos = UsbPrintTransport::EnumerateUsbPortsWithInfo();

	for (size_t i = 0; i < portInfos.size(); ++i)
	{
		if (m_shouldStop) break;

		// 更新进度
		if (progressCallback)
		{
			PortScanProgress progress;
			progress.currentPort = static_cast<int>(i);
			progress.totalPorts = static_cast<int>(portInfos.size());
			progress.status = "扫描USB: " + portInfos[i].portName;
			progressCallback(progress);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// 生成显示文本
		std::string displayText = portInfos[i].portName;
		if (!portInfos[i].displayName.empty())
		{
			displayText += " - " + portInfos[i].displayName;
		}
		if (!portInfos[i].statusText.empty())
		{
			displayText += " (" + portInfos[i].statusText + ")";
		}

		m_scanResults.push_back(displayText);
	}
}

// 扫描网络端口
void PortScanner::ScanNetworkPorts(PortScanProgressCallback progressCallback)
{
	// 网络端口使用预定义列表
	m_scanResults.push_back("127.0.0.1:9100 (未检测)");
	m_scanResults.push_back("192.168.1.100:9100 (未检测)");
	m_scanResults.push_back("printer.local:9100 (未检测)");
	m_scanResults.push_back("[配置网络打印机...]");

	if (progressCallback)
	{
		PortScanProgress progress;
		progress.currentPort = 1;
		progress.totalPorts = 1;
		progress.status = "网络端口扫描完成";
		progressCallback(progress);
	}
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

	// 智能设置默认选择
	SelectDefaultPort();
}

// 更新串口参数
void PortConfigPresenter::UpdateSerialPortParameters()
{
	// 使用增强版枚举（获取设备描述）
	auto portInfos = SerialTransport::EnumerateSerialPortsWithInfo();

	// 转换为显示字符串列表
	std::vector<std::string> portList;
	for (const auto& info : portInfos)
	{
		// 格式：COM3 - CH340 (已连接)
		std::string displayText = info.portName;
		if (!info.displayName.empty())
		{
			displayText += " - " + info.displayName;
		}
		if (info.IsConnected())
		{
			displayText += " (已连接)";
		}
		else if (info.status == PortStatus::Available)
		{
			displayText += " (可用)";
		}
		portList.push_back(displayText);
	}

	// 如果没有找到串口，添加默认选项
	if (portList.empty())
	{
		portList.push_back("COM1 (默认)");
		portList.push_back("COM2 (默认)");
		portList.push_back("COM3 (默认)");
		portList.push_back("COM4 (默认)");
	}

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, portList);

	// 显示串口相关参数
	ShowSerialParameters(true);
}

// 更新并口参数
void PortConfigPresenter::UpdateParallelPortParameters()
{
	// 使用增强版枚举（获取设备描述）
	auto portInfos = ParallelTransport::EnumerateParallelPortsWithInfo();

	// 转换为显示字符串列表
	std::vector<std::string> portList;
	for (const auto& info : portInfos)
	{
		// 格式：LPT1 - EPSON L3150 (已连接)
		std::string displayText = info.portName;
		if (!info.displayName.empty())
		{
			displayText += " - " + info.displayName;
		}
		if (info.IsConnected())
		{
			displayText += " (已连接)";
		}
		else if (info.status == PortStatus::Available)
		{
			displayText += " (可用)";
		}
		portList.push_back(displayText);
	}

	// 如果没有找到并口，添加默认选项
	if (portList.empty())
	{
		portList.push_back("LPT1 (默认)");
		portList.push_back("LPT2 (默认)");
		portList.push_back("LPT3 (默认)");
	}

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, portList);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 更新USB打印端口参数
void PortConfigPresenter::UpdateUsbPrintPortParameters()
{
	// 使用增强版枚举（获取设备描述）
	auto portInfos = UsbPrintTransport::EnumerateUsbPortsWithInfo();

	// 转换为显示字符串列表
	std::vector<std::string> portList;
	for (const auto& info : portInfos)
	{
		// 格式：USB001 - Canon iP7200 (就绪)
		std::string displayText = info.portName;
		if (!info.displayName.empty())
		{
			displayText += " - " + info.displayName;
		}
		if (!info.statusText.empty())
		{
			displayText += " (" + info.statusText + ")";
		}
		portList.push_back(displayText);
	}

	// 如果没有找到USB打印端口，添加默认选项
	if (portList.empty())
	{
		portList.push_back("USB001 (默认)");
		portList.push_back("USB002 (默认)");
	}

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, portList);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 更新网络打印端口参数
void PortConfigPresenter::UpdateNetworkPrintPortParameters()
{
	// 添加网络打印地址选项
	std::vector<std::string> addresses;
	addresses.push_back("127.0.0.1:9100 (未检测)");
	addresses.push_back("192.168.1.100:9100 (未检测)");
	addresses.push_back("printer.local:9100 (未检测)");
	addresses.push_back("[配置网络打印机...]");

	// 填充端口列表
	PopulateComboBox(m_controls.comboPort, addresses);

	// 隐藏串口参数
	ShowSerialParameters(false);
}

// 处理网络打印机配置选项选择
void PortConfigPresenter::OnNetworkPrinterConfigSelected()
{
	// 弹出网络打印机配置对话框
	NetworkPrinterConfigDialog dialog;
	if (dialog.DoModal() == IDOK) {
		// 这里可以添加自定义网络打印机到配置中的逻辑
		// 目前先显示一个消息框
		// 实际应用中需要将新配置添加到端口列表中
	}

	// 重新选择之前的选项或默认选项
	if (m_controls.comboPort && m_controls.comboPort->GetCount() > 0) {
		m_controls.comboPort->SetCurSel(0);
	}
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

// 异步更新端口参数（避免阻塞UI）
void PortConfigPresenter::UpdatePortParametersAsync()
{
	PortTypeIndex portType = GetSelectedPortType();

	// 清空端口列表并显示"扫描中..."提示
	if (IsControlValid(m_controls.comboPort))
	{
		m_controls.comboPort->ResetContent();
		m_controls.comboPort->AddString(_T("正在扫描端口..."));
		m_controls.comboPort->SetCurSel(0);
	}

	// 开始异步扫描
	if (m_portScanner)
	{
		m_portScanner->StartScan(portType,
			// 进度回调
			[this](const PortScanProgress& progress)
			{
				// 在这里可以更新UI显示扫描进度（如果需要）
				// 注意：由于这是在后台线程中，不应该直接更新UI
				// 可以通过消息机制将进度转发到UI线程
			},
			// 完成回调
			[this, portType](const PortScanResult& result)
			{
				// 扫描完成，更新UI
				if (result.success)
				{
					// 使用消息机制在UI线程中更新列表
					if (IsControlValid(m_controls.parentDialog))
					{
						m_controls.parentDialog->PostMessage(WM_USER + 100, 0, 0);
					}
				}
				else
				{
					// 扫描失败，显示错误
					if (IsControlValid(m_controls.comboPort))
					{
						m_controls.comboPort->ResetContent();
						CString errorText = _T("扫描失败: ");
						errorText += CA2T(result.error.c_str(), CP_UTF8);
						m_controls.comboPort->AddString(errorText);
						m_controls.comboPort->SetCurSel(0);
					}
				}
			});
	}
}

// 智能选择默认端口
void PortConfigPresenter::SelectDefaultPort()
{
	if (!IsControlValid(m_controls.comboPort) || m_controls.comboPort->GetCount() == 0)
	{
		return;
	}

	int selectedIndex = 0;  // 默认选择第一个

	// 遍历所有端口，寻找最佳选择
	for (int i = 0; i < m_controls.comboPort->GetCount(); ++i)
	{
		CString portText;
		m_controls.comboPort->GetLBText(i, portText);

		// 第一优先级：已连接设备
		if (portText.Find(_T("(已连接)")) != -1)
		{
			selectedIndex = i;
			break;  // 找到已连接设备，立即选择
		}
	}

	// 第二优先级：可用设备（如果没找到已连接设备）
	if (selectedIndex == 0)
	{
		for (int i = 0; i < m_controls.comboPort->GetCount(); ++i)
		{
			CString portText;
			m_controls.comboPort->GetLBText(i, portText);

			// 查找"可用"、"就绪"等状态
			if (portText.Find(_T("(可用)")) != -1 ||
				portText.Find(_T("(就绪)")) != -1)
			{
				selectedIndex = i;
				break;  // 找到可用设备，立即选择
			}
		}
	}

	// 应用选择
	m_controls.comboPort->SetCurSel(selectedIndex);
}

// 快速检测当前选中的端口状态（500ms超时）
bool PortConfigPresenter::QuickCheckPortStatus()
{
	// 获取当前选择的端口
	if (!IsControlValid(m_controls.comboPort))
	{
		return false;
	}

	CString portText;
	m_controls.comboPort->GetWindowText(portText);

	PortTypeIndex portType = GetSelectedPortType();

	// 轻量级检测逻辑
	switch (portType)
	{
	case PortTypeIndex::Serial:
	{
		// 串口：检查端口名称格式是否正确
		if (portText.Find(_T("COM")) != -1)
		{
			// 提取端口号进行简单验证
			int comIndex = portText.Find(_T("COM"));
			if (comIndex != -1 && comIndex + 4 < portText.GetLength())
			{
				CString portNum = portText.Mid(comIndex + 3, portText.GetLength() - comIndex - 3);
				// 移除可能的描述部分
				int dashIndex = portNum.Find(_T(" -"));
				if (dashIndex != -1)
				{
					portNum = portNum.Left(dashIndex);
				}
				// 检查端口号是否为数字
				for (int i = 0; i < portNum.GetLength(); ++i)
				{
					if (!_istdigit(portNum.GetAt(i)))
					{
						return false;
					}
				}
				return true;  // 格式正确
			}
		}
		return false;
	}

	case PortTypeIndex::Parallel:
	{
		// 并口：检查端口名称格式
		if (portText.Find(_T("LPT")) != -1)
		{
			return true;
		}
		return false;
	}

	case PortTypeIndex::UsbPrint:
	{
		// USB打印：检查端口名称格式
		if (portText.Find(_T("USB")) != -1)
		{
			return true;
		}
		return false;
	}

	case PortTypeIndex::NetworkPrint:
	{
		// 网络打印：检查IP地址格式（简单检查）
		if (portText.Find(_T(":")) != -1)  // 包含端口号
		{
			// 可以进一步解析IP地址格式
			return true;
		}
		return false;
	}

	case PortTypeIndex::Loopback:
	{
		// 回路测试：始终可用
		return true;
	}

	default:
		return false;
	}
}