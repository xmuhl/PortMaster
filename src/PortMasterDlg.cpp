// PortMasterDlg.cpp : 实现文件
//

#include "pch.h"
#include "framework.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "afxdialogex.h"
#include "../Transport/ITransport.h"
#include "../Transport/SerialTransport.h"
#include "../Transport/ParallelTransport.h"
#include "../Transport/UsbPrintTransport.h"
#include "../Transport/NetworkPrintTransport.h"
#include "../Transport/LoopbackTransport.h"
#include "../Common/CommonTypes.h"
#include "../Common/ConfigStore.h"
#include <chrono>
#include <thread>
#include <shellapi.h>
#include <ctime>
#include <fstream>
#include <algorithm>

// 【UI优化】定时器ID常量定义
#define TIMER_ID_CONNECTION_STATUS  1  // 连接状态恢复定时器
#define TIMER_ID_THROTTLED_DISPLAY   2  // 接收显示节流定时器

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum
	{
		IDD = IDD_ABOUTBOX
	};
#endif

protected:
	virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// 日志写入函数
void CPortMasterDlg::WriteLog(const std::string &message)
{
	try
	{
		// 获取当前时间
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

		std::tm tm;
		localtime_s(&tm, &time_t);

		// 格式化时间字符串
		char timeStr[32];
		sprintf_s(timeStr, sizeof(timeStr), "[%02d:%02d:%02d.%03d] ",
				  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));

		// 打开日志文件（追加模式）
		std::ofstream logFile("PortMaster_debug.log", std::ios::app);
		if (logFile.is_open())
		{
			logFile << timeStr << message << std::endl;
			logFile.close();
		}
	}
	catch (...)
	{
		// 忽略日志写入错误
	}
}

// CPortMasterDlg 对话框

CPortMasterDlg::CPortMasterDlg(CWnd *pParent /*=nullptr*/)
	: CDialogEx(IDD_PORTMASTER_DIALOG, pParent), m_isConnected(false), m_isTransmitting(false), m_transmissionPaused(false), m_transmissionCancelled(false), m_bytesSent(0), m_bytesReceived(0), m_sendSpeed(0), m_receiveSpeed(0), m_transport(nullptr), m_reliableChannel(nullptr), m_configStore(ConfigStore::GetInstance()), m_binaryDataDetected(false), m_updateDisplayInProgress(false), m_receiveDisplayPending(false), m_lastReceiveDisplayUpdate(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPortMasterDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialogEx::DoDataExchange(pDX);

	// 基本按钮控件绑定
	DDX_Control(pDX, IDC_BUTTON_CONNECT, m_btnConnect);
	DDX_Control(pDX, IDC_BUTTON_DISCONNECT, m_btnDisconnect);
	DDX_Control(pDX, IDC_BUTTON_SEND, m_btnSend);
	DDX_Control(pDX, IDC_BUTTON_STOP, m_btnStop);
	DDX_Control(pDX, IDC_BUTTON_FILE, m_btnFile);

	// 新增按钮控件绑定
	DDX_Control(pDX, IDC_BUTTON_CLEAR_ALL, m_btnClearAll);
	DDX_Control(pDX, IDC_BUTTON_CLEAR_RECEIVE, m_btnClearReceive);
	DDX_Control(pDX, IDC_BUTTON_COPY_ALL, m_btnCopyAll);
	DDX_Control(pDX, IDC_BUTTON_SAVE_ALL, m_btnSaveAll);

	// 编辑框控件绑定
	DDX_Control(pDX, IDC_EDIT_SEND_DATA, m_editSendData);
	DDX_Control(pDX, IDC_EDIT_RECEIVE_DATA, m_editReceiveData);
	DDX_Control(pDX, IDC_EDIT_TIMEOUT, m_editTimeout);

	// 下拉框控件绑定
	DDX_Control(pDX, IDC_COMBO_PORT_TYPE, m_comboPortType);
	DDX_Control(pDX, IDC_COMBO_PORT, m_comboPort);
	DDX_Control(pDX, IDC_COMBO_BAUD_RATE, m_comboBaudRate);
	DDX_Control(pDX, IDC_COMBO_DATA_BITS, m_comboDataBits);
	DDX_Control(pDX, IDC_COMBO_PARITY, m_comboParity);
	DDX_Control(pDX, IDC_COMBO_STOP_BITS, m_comboStopBits);
	DDX_Control(pDX, IDC_COMBO_FLOW_CONTROL, m_comboFlowControl);

	// 选项控件绑定
	DDX_Control(pDX, IDC_RADIO_RELIABLE, m_radioReliable);
	DDX_Control(pDX, IDC_RADIO_DIRECT, m_radioDirect);
	DDX_Control(pDX, IDC_CHECK_HEX, m_checkHex);
	DDX_Control(pDX, IDC_CHECK_OCCUPY, m_checkOccupy);

	// 状态显示控件绑定
	DDX_Control(pDX, IDC_PROGRESS, m_progress);
	DDX_Control(pDX, IDC_STATIC_LOG, m_staticLog);
	DDX_Control(pDX, IDC_STATIC_PORT_STATUS, m_staticPortStatus);
	DDX_Control(pDX, IDC_STATIC_MODE, m_staticMode);
	DDX_Control(pDX, IDC_STATIC_SPEED, m_staticSpeed);
	DDX_Control(pDX, IDC_STATIC_SEND_SOURCE, m_staticSendSource);
}

BEGIN_MESSAGE_MAP(CPortMasterDlg, CDialogEx)
ON_WM_SYSCOMMAND()
ON_WM_PAINT()
ON_WM_QUERYDRAGICON()
ON_WM_DROPFILES()
ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CPortMasterDlg::OnBnClickedButtonConnect)
ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CPortMasterDlg::OnBnClickedButtonDisconnect)
ON_BN_CLICKED(IDC_BUTTON_SEND, &CPortMasterDlg::OnBnClickedButtonSend)
ON_BN_CLICKED(IDC_BUTTON_STOP, &CPortMasterDlg::OnBnClickedButtonStop)
ON_BN_CLICKED(IDC_BUTTON_FILE, &CPortMasterDlg::OnBnClickedButtonFile)
ON_BN_CLICKED(IDC_BUTTON_CLEAR_ALL, &CPortMasterDlg::OnBnClickedButtonClearAll)
ON_BN_CLICKED(IDC_BUTTON_CLEAR_RECEIVE, &CPortMasterDlg::OnBnClickedButtonClearReceive)
ON_BN_CLICKED(IDC_BUTTON_COPY_ALL, &CPortMasterDlg::OnBnClickedButtonCopyAll)
ON_BN_CLICKED(IDC_BUTTON_SAVE_ALL, &CPortMasterDlg::OnBnClickedButtonSaveAll)
ON_CBN_SELCHANGE(IDC_COMBO_PORT_TYPE, &CPortMasterDlg::OnCbnSelchangeComboPortType)
ON_BN_CLICKED(IDC_CHECK_HEX, &CPortMasterDlg::OnBnClickedCheckHex)
ON_BN_CLICKED(IDC_RADIO_RELIABLE, &CPortMasterDlg::OnBnClickedRadioReliable)
ON_BN_CLICKED(IDC_RADIO_DIRECT, &CPortMasterDlg::OnBnClickedRadioDirect)
ON_EN_CHANGE(IDC_EDIT_SEND_DATA, &CPortMasterDlg::OnEnChangeEditSendData)
ON_WM_TIMER()
ON_MESSAGE(WM_USER + 1, &CPortMasterDlg::OnTransportDataReceivedMessage)
ON_MESSAGE(WM_USER + 2, &CPortMasterDlg::OnTransportErrorMessage)
ON_MESSAGE(WM_USER + 10, &CPortMasterDlg::OnTransmissionProgressRange)
ON_MESSAGE(WM_USER + 11, &CPortMasterDlg::OnTransmissionProgressUpdate)
ON_MESSAGE(WM_USER + 12, &CPortMasterDlg::OnTransmissionStatusUpdate)
ON_MESSAGE(WM_USER + 13, &CPortMasterDlg::OnTransmissionComplete)
ON_MESSAGE(WM_USER + 14, &CPortMasterDlg::OnTransmissionError)
END_MESSAGE_MAP()

// CPortMasterDlg 消息处理程序

BOOL CPortMasterDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu *pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);	 // 设置大图标
	SetIcon(m_hIcon, FALSE); // 设置小图标

	// TODO: 在此添加额外的初始化代码

	// 初始化端口类型下拉框
	m_comboPortType.AddString(_T("串口"));
	m_comboPortType.AddString(_T("并口"));
	m_comboPortType.AddString(_T("USB打印"));
	m_comboPortType.AddString(_T("网络打印"));
	m_comboPortType.AddString(_T("回路测试"));
	m_comboPortType.SetCurSel(0);

	// 初始化端口列表
	m_comboPort.AddString(_T("COM1"));
	m_comboPort.SetCurSel(0);

	// 初始化波特率列表
	m_comboBaudRate.AddString(_T("9600"));
	m_comboBaudRate.SetCurSel(0);

	// 初始化数据位下拉框
	m_comboDataBits.AddString(_T("8"));
	m_comboDataBits.SetCurSel(0);

	// 初始化校验位下拉框
	m_comboParity.AddString(_T("None"));
	m_comboParity.SetCurSel(0);

	// 初始化停止位下拉框
	m_comboStopBits.AddString(_T("1"));
	m_comboStopBits.SetCurSel(0);

	// 初始化流控下拉框
	m_comboFlowControl.AddString(_T("None"));
	m_comboFlowControl.SetCurSel(0);

	// 初始化超时编辑框
	SetDlgItemText(IDC_EDIT_TIMEOUT, _T("1000"));

	// 初始化单选按钮 - 默认选择直通模式
	m_radioReliable.SetCheck(BST_UNCHECKED);
	m_radioDirect.SetCheck(BST_CHECKED);

	// 初始化占用检测复选框 - 默认勾选直通模式
	m_checkOccupy.SetCheck(BST_CHECKED);

	// 初始化进度条
	m_progress.SetRange(0, 100);
	m_progress.SetPos(0);

	// 显示初始状态
	LogMessage(_T("程序启动成功"));
	SetDlgItemText(IDC_STATIC_PORT_STATUS, _T("未连接"));
	SetDlgItemText(IDC_STATIC_MODE, _T("直通")); // 与默认选择的直通模式保持一致
	SetDlgItemText(IDC_STATIC_SPEED, _T("0KB/s"));
	SetDlgItemText(IDC_STATIC_SEND_SOURCE, _T("手动输入"));

	// 初始化传输配置
	InitializeTransportConfig();

	// 从配置存储加载配置
	LoadConfigurationFromStore();

	// 启用文件拖拽功能
	DragAcceptFiles(TRUE);

	// 初始化临时缓存文件
	InitializeTempCacheFile();

	return TRUE; // 除非将焦点设置到控件，否则返回 TRUE
}

void CPortMasterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CPortMasterDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 当用户拖动最小化窗口时系统调用此函数取得光标
// 显示。
HCURSOR CPortMasterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CPortMasterDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_ID_CONNECTION_STATUS)
	{
		// 定时器1：恢复连接状态显示
		KillTimer(TIMER_ID_CONNECTION_STATUS);
		UpdateConnectionStatus();

		// 不再自动重置进度条，保持传输完成后的100%状态
		// 进度条只在开始新传输时重置为0
	}
	else if (nIDEvent == TIMER_ID_THROTTLED_DISPLAY)
	{
		// 【UI优化】定时器2：节流的接收显示更新
		KillTimer(TIMER_ID_THROTTLED_DISPLAY);

		if (m_receiveDisplayPending)
		{
			// 执行延迟的显示更新
			UpdateReceiveDisplayFromCache();
			m_lastReceiveDisplayUpdate = ::GetTickCount();
			m_receiveDisplayPending = false;
		}
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CPortMasterDlg::PostNcDestroy()
{
	// 确保传输线程正确结束
	if (m_transmissionThread.joinable())
	{
		m_transmissionCancelled = true;
		m_transmissionThread.join();
	}

	// 清理临时缓存文件
	CloseTempCacheFile();

	CDialogEx::PostNcDestroy();
}

void CPortMasterDlg::OnBnClickedButtonConnect()
{
	// TODO: 在此添加控件通知处理程序代码

	// 创建传输对象
	if (!CreateTransport())
	{
		return;
	}

	// 打开传输连接
	WriteLog("OnBnClickedButtonConnect: Starting connection...");

	TransportError error = TransportError::Success;
	if (m_reliableChannel)
	{
		WriteLog("OnBnClickedButtonConnect: Connecting reliable channel...");
		bool connected = m_reliableChannel->Connect();
		WriteLog("OnBnClickedButtonConnect: Reliable channel connect result: " + std::to_string(connected));
		error = connected ? static_cast<TransportError>(TransportError::Success) : static_cast<TransportError>(TransportError::ConnectionClosed);
	}
	else if (m_transport)
	{
		WriteLog("OnBnClickedButtonConnect: Using direct transport...");
		// 传输对象已经在CreateTransport中打开，这里不需要重复打开
		if (!m_transport->IsOpen())
		{
			WriteLog("OnBnClickedButtonConnect: Transport not open");
			error = TransportError::NotOpen;
		}
		else
		{
			WriteLog("OnBnClickedButtonConnect: Transport already open");
		}
	}

	if (error != TransportError::Success)
	{
		CString errorMsg;
		errorMsg.Format(_T("连接失败: %d"), static_cast<int>(error));
		MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
		DestroyTransport();
		return;
	}

	// 标记为已连接
	m_isConnected = true;

	// 启动接收线程
	StartReceiveThread();

	// 更新状态条连接状态
	UpdateConnectionStatus();

	// 启用断开按钮，禁用连接按钮
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);

	// 禁用端口参数控件
	GetDlgItem(IDC_COMBO_PORT)->EnableWindow(FALSE);
	GetDlgItem(IDC_COMBO_BAUD_RATE)->EnableWindow(FALSE);
	GetDlgItem(IDC_COMBO_DATA_BITS)->EnableWindow(FALSE);
	GetDlgItem(IDC_COMBO_PARITY)->EnableWindow(FALSE);
	GetDlgItem(IDC_COMBO_STOP_BITS)->EnableWindow(FALSE);
	GetDlgItem(IDC_COMBO_FLOW_CONTROL)->EnableWindow(FALSE);
}

void CPortMasterDlg::OnBnClickedButtonDisconnect()
{
	// TODO: 在此添加控件通知处理程序代码

	// 销毁传输对象
	DestroyTransport();

	// 清理临时缓存文件（断开连接时清空缓存）
	ClearTempCacheFile();

	// 更新状态条连接状态
	UpdateConnectionStatus();

	// 启用连接按钮，禁用断开按钮
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);

	// 启用端口参数控件
	GetDlgItem(IDC_COMBO_PORT)->EnableWindow(TRUE);
	GetDlgItem(IDC_COMBO_BAUD_RATE)->EnableWindow(TRUE);
	GetDlgItem(IDC_COMBO_DATA_BITS)->EnableWindow(TRUE);
	GetDlgItem(IDC_COMBO_PARITY)->EnableWindow(TRUE);
	GetDlgItem(IDC_COMBO_STOP_BITS)->EnableWindow(TRUE);
	GetDlgItem(IDC_COMBO_FLOW_CONTROL)->EnableWindow(TRUE);
}

void CPortMasterDlg::OnBnClickedButtonSend()
{
	// 检查是否已连接
	if (!m_isConnected)
	{
		MessageBox(_T("请先连接端口"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	// 根据当前传输状态决定按钮行为
	if (!m_isTransmitting)
	{
		// 初始状态：开始发送
		StartTransmission();
	}
	else if (m_transmissionPaused)
	{
		// 暂停状态：继续传输
		ResumeTransmission();
	}
	else
	{
		// 传输中状态：暂停传输
		PauseTransmission();
	}
}

void CPortMasterDlg::StartTransmission()
{
	// 基于缓存的架构：检查缓存有效性
	if (!m_sendCacheValid || m_sendDataCache.empty())
	{
		MessageBox(_T("没有可发送的数据，请先输入数据"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	// 重置传输状态
	m_isTransmitting = true;
	m_transmissionPaused = false;
	m_transmissionCancelled = false;

	// 重置进度条（仅在开始新传输时重置）
	m_progress.SetPos(0);

	// 更新按钮文本和状态栏
	m_btnSend.SetWindowText(_T("中断"));
	m_staticPortStatus.SetWindowText(_T("数据传输已开始"));

	// 启动异步传输线程，避免UI阻塞
	if (m_transmissionThread.joinable())
	{
		m_transmissionThread.join();
	}

	m_transmissionThread = std::thread([this]()
									   { PerformDataTransmission(); });
}

void CPortMasterDlg::PauseTransmission()
{
	// 直接暂停传输，不弹出提示对话框
	m_transmissionPaused = true;

	// 更新按钮文本和状态栏
	m_btnSend.SetWindowText(_T("继续"));
	m_staticPortStatus.SetWindowText(_T("传输已暂停"));

	// 记录日志
	this->WriteLog("传输已暂停");
}

void CPortMasterDlg::ResumeTransmission()
{
	m_transmissionPaused = false;

	// 更新按钮文本和状态栏
	m_btnSend.SetWindowText(_T("中断"));
	m_staticPortStatus.SetWindowText(_T("传输已恢复"));
}

void CPortMasterDlg::PerformDataTransmission()
{
	try
	{
		// 直接使用缓存中的原始字节数据，无需任何解析转换
		const std::vector<uint8_t> &data = m_sendDataCache;

		// 添加调试输出：记录发送数据的详细信息
		this->WriteLog("发送数据: " + std::to_string(data.size()) + " 字节");

		// 发送数据
		TransportError error = TransportError::Success;

		// 初始化进度条（使用PostMessage确保线程安全）
		PostMessage(WM_USER + 10, 0, 100); // 设置进度条范围
		PostMessage(WM_USER + 11, 0, 0);   // 设置进度条位置

		// 添加调试输出：检查传输通道状态
		if (m_reliableChannel && m_reliableChannel->IsConnected())
		{
			this->WriteLog("使用可靠传输通道");

			// 使用可靠传输通道
			bool success = false;

			// 对于较大的数据（>1KB），模拟进度更新
			if (data.size() > 1024)
			{
				this->WriteLog("PerformDataTransmission: Large data detected, size=" + std::to_string(data.size()) + ", using chunked send");

				// 分块发送以显示进度
				const size_t chunkSize = 1024;
				size_t totalSent = 0;

				while (totalSent < data.size() && !m_transmissionCancelled)
				{
					// 检查是否暂停
					while (m_transmissionPaused && !m_transmissionCancelled)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}

					if (m_transmissionCancelled)
						break;

					size_t currentChunkSize = min(chunkSize, data.size() - totalSent);
					std::vector<uint8_t> chunk(data.begin() + totalSent, data.begin() + totalSent + currentChunkSize);

					this->WriteLog("PerformDataTransmission: Sending chunk " + std::to_string(totalSent / chunkSize + 1) +
								   "/" + std::to_string((data.size() + chunkSize - 1) / chunkSize) +
								   ", size=" + std::to_string(currentChunkSize));

					success = m_reliableChannel->Send(chunk);
					if (!success)
					{
						this->WriteLog("PerformDataTransmission: Chunk send failed at " + std::to_string(totalSent) +
									   "/" + std::to_string(data.size()));
						break;
					}

					totalSent += currentChunkSize;

					// 更新进度条（使用PostMessage确保线程安全）
					int progress = (int)((totalSent * 100) / data.size());
					PostMessage(WM_USER + 11, 0, progress);

					// 更新状态（使用PostMessage确保线程安全）
					if (!m_transmissionPaused)
					{
						CString *progressStatus = new CString();
						progressStatus->Format(_T("正在传输: %u/%u 字节 (%d%%)"), totalSent, data.size(), progress);
						PostMessage(WM_USER + 12, 0, (LPARAM)progressStatus);
					}

					// 添加小延迟，避免过快发送
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

				// 检查传输结果
				if (m_transmissionCancelled)
				{
					this->WriteLog("PerformDataTransmission: Transmission cancelled, " + std::to_string(totalSent) +
								   "/" + std::to_string(data.size()) + " bytes sent");
					error = TransportError::WriteFailed;
				}
				else if (totalSent == data.size())
				{
					this->WriteLog("PerformDataTransmission: Chunked transmission completed successfully");
					error = success ? TransportError::Success : TransportError::WriteFailed;
				}
				else
				{
					error = TransportError::WriteFailed;
				}
			}
			else
			{
				this->WriteLog("PerformDataTransmission: Small data, size=" + std::to_string(data.size()) + ", sending directly");
				// 小数据直接发送
				success = m_reliableChannel->Send(data);
				this->WriteLog("PerformDataTransmission: Direct send result: " + std::to_string(success));
				error = success ? TransportError::Success : TransportError::WriteFailed;
			}
		}
		else if (m_transport && m_transport->IsOpen())
		{
			this->WriteLog("PerformDataTransmission: Using raw transport");

			// 使用原始传输 - 添加分块发送支持以确保大数据完整传输
			if (data.size() > 4096) // 对于大于4KB的数据进行分块发送
			{
				this->WriteLog("PerformDataTransmission: Large data for raw transport, size=" + std::to_string(data.size()) + ", using chunked send");

				const size_t chunkSize = 4096; // 原始传输使用较大的块大小
				size_t totalSent = 0;

				while (totalSent < data.size() && !m_transmissionCancelled)
				{
					// 检查是否暂停
					while (m_transmissionPaused && !m_transmissionCancelled)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}

					if (m_transmissionCancelled)
						break;

					size_t currentChunkSize = (std::min)(chunkSize, data.size() - totalSent);

					this->WriteLog("PerformDataTransmission: Sending raw chunk " + std::to_string(totalSent / chunkSize + 1) +
								   "/" + std::to_string((data.size() + chunkSize - 1) / chunkSize) +
								   ", size=" + std::to_string(currentChunkSize));

					TransportError chunkError = TransportError::Success;
					int retryCount = 0;
					const int maxRetries = 3;
					const int retryDelayMs = 50;

					// 智能重试机制：对Busy错误进行有限重试
					do
					{
						chunkError = m_transport->Write(data.data() + totalSent, currentChunkSize);

						if (chunkError == TransportError::Success)
						{
							break; // 发送成功，退出重试循环
						}
						else if (chunkError == TransportError::Busy && retryCount < maxRetries)
						{
							retryCount++;
							this->WriteLog("PerformDataTransmission: Raw chunk send busy, retry " + std::to_string(retryCount) +
										   "/" + std::to_string(maxRetries) + " in " + std::to_string(retryDelayMs) + "ms");
							std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
						}
						else
						{
							// 非Busy错误或重试次数用尽，记录错误并退出
							this->WriteLog("PerformDataTransmission: Raw chunk send failed at " + std::to_string(totalSent) +
										   "/" + std::to_string(data.size()) + ", error=" + std::to_string(static_cast<int>(chunkError)) +
										   (retryCount > 0 ? " (after " + std::to_string(retryCount) + " retries)" : ""));
							break;
						}
					} while (chunkError == TransportError::Busy && retryCount <= maxRetries);

					if (chunkError != TransportError::Success)
					{
						error = chunkError;
						break;
					}

					totalSent += currentChunkSize;

					// 更新进度条（使用PostMessage确保线程安全）
					int progress = (int)((totalSent * 100) / data.size());
					PostMessage(WM_USER + 11, 0, progress);

					// 更新状态（使用PostMessage确保线程安全）
					if (!m_transmissionPaused)
					{
						CString *progressStatus = new CString();
						progressStatus->Format(_T("原始传输: %u/%u 字节 (%d%%)"), totalSent, data.size(), progress);
						PostMessage(WM_USER + 12, 0, (LPARAM)progressStatus);
					}

					// 添加小延迟，避免过快发送导致接收端处理不及时
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

				if (totalSent == data.size() && error == TransportError::Success && !m_transmissionCancelled)
				{
					this->WriteLog("PerformDataTransmission: Raw chunked transmission completed successfully");
				}
			}
			else
			{
				this->WriteLog("PerformDataTransmission: Small data for raw transport, size=" + std::to_string(data.size()) + ", sending directly");
				error = m_transport->Write(data.data(), data.size());
				this->WriteLog("PerformDataTransmission: Raw direct send result: " + std::to_string(static_cast<int>(error)));
			}
		}

		// 通过PostMessage通知UI线程处理传输结果
		PostMessage(WM_USER + 13, (WPARAM)error, 0);

		this->WriteLog("=== 发送数据调试信息结束 ===");
	}
	catch (const std::exception &e)
	{
		this->WriteLog("PerformDataTransmission异常: " + std::string(e.what()));
		PostMessage(WM_USER + 14, 0, 0); // 通知UI线程处理异常
	}
}

// 新增：清除所有缓存数据函数
void CPortMasterDlg::ClearAllCacheData()
{
	try
	{
		// 仅清除接收数据缓存，不影响发送窗口数据
		m_receiveDataCache.clear();
		m_receiveCacheValid = false;

		// 重置二进制数据检测状态，允许重新检测
		m_binaryDataDetected = false;
		m_binaryDataPreview.Empty();

		// 仅清空接收数据显示框，保持发送窗口数据不变
		m_editReceiveData.SetWindowText(_T(""));

		// 清除临时缓存文件
		ClearTempCacheFile();

		// 重置接收统计信息
		m_bytesReceived = 0;
		SetDlgItemText(IDC_STATIC_RECEIVED, _T("0"));

		// 注意：发送窗口数据和发送统计信息保持不变
		// 发送窗口数据仅能通过用户主动点击"清空发送框"按钮进行清除

		this->WriteLog("已清除接收缓存数据，发送窗口数据保持不变");
	}
	catch (const std::exception &e)
	{
		this->WriteLog("清除缓存数据时发生异常: " + std::string(e.what()));
	}
}

void CPortMasterDlg::OnBnClickedButtonStop()
{
	if (m_isTransmitting)
	{
		// 如果正在传输，弹出确认对话框
		int result = MessageBox(_T("确认终止传输？"), _T("确认终止传输"), MB_YESNO | MB_ICONQUESTION);
		if (result == IDYES)
		{
			// 用户确认终止传输
			m_transmissionCancelled = true;
			m_isTransmitting = false;
			m_transmissionPaused = false;

			// 立即终止传输进程并清除所有已接收的缓存数据
			ClearAllCacheData();

			// 更新界面状态
			m_btnSend.SetWindowText(_T("发送"));
			m_staticPortStatus.SetWindowText(_T("传输已终止"));
			m_progress.SetPos(0);

			// 记录日志
			this->WriteLog("用户手动终止传输，已清除所有缓存数据");
		}
	}
	else
	{
		// 如果没有传输，显示提示信息
		MessageBox(_T("当前没有进行中的传输"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	}
}

void CPortMasterDlg::OnBnClickedButtonFile()
{
	// 统一的文件过滤器配置：默认显示所有文件，包含所有文件类型
	CString filter;
	CString defaultExt;

	// 无论是否勾选16进制模式，都使用相同的过滤器配置
	filter = _T("所有文件 (*.*)|*.*|")
			 _T("文本文件 (*.txt;*.log;*.ini;*.cfg;*.conf)|*.txt;*.log;*.ini;*.cfg;*.conf|")
			 _T("二进制文件 (*.bin;*.dat;*.exe)|*.bin;*.dat;*.exe|")
			 _T("图像文件 (*.jpg;*.png;*.bmp;*.gif;*.tiff)|*.jpg;*.png;*.bmp;*.gif;*.tiff|")
			 _T("压缩文件 (*.zip;*.rar;*.7z;*.tar;*.gz)|*.zip;*.rar;*.7z;*.tar;*.gz|")
			 _T("文档文件 (*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx)|*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx|")
			 _T("脚本文件 (*.bat;*.cmd;*.ps1;*.sh;*.py)|*.bat;*.cmd;*.ps1;*.sh;*.py|")
			 _T("源代码 (*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css)|*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css||");
	defaultExt = _T("");

	// 文件选择对话框
	CFileDialog dlg(TRUE, defaultExt, NULL,
					OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
					filter);

	if (dlg.DoModal() == IDOK)
	{
		CString filePath = dlg.GetPathName();
		LoadDataFromFile(filePath);

		// 更新数据源显示
		CString fileName = dlg.GetFileName();
		CString sourceText;
		sourceText.Format(_T("来源: %s"), fileName);
		SetDlgItemText(IDC_STATIC_SEND_SOURCE, sourceText);
	}
}

void CPortMasterDlg::LoadDataFromFile(const CString &filePath)
{
	CFile file;
	if (file.Open(filePath, CFile::modeRead))
	{
		ULONGLONG fileLength = file.GetLength();
		if (fileLength > 0)
		{
			BYTE *fileBuffer = new BYTE[(size_t)fileLength + 2];
			file.Read(fileBuffer, (UINT)fileLength);
			fileBuffer[fileLength] = 0;
			fileBuffer[fileLength + 1] = 0;

			// 获取文件扩展名检测二进制文件
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			CString fileExt = fileName.Mid(fileName.ReverseFind('.') + 1);
			fileExt.MakeLower();

			bool isBinaryFile = (fileExt == _T("prn") || fileExt == _T("bin") ||
								 fileExt == _T("dat") || fileExt == _T("exe") ||
								 fileExt == _T("dll") || fileExt == _T("img") ||
								 fileExt == _T("zip") || fileExt == _T("rar") ||
								 fileExt == _T("7z") || fileExt == _T("tar") ||
								 fileExt == _T("gz") || fileExt == _T("bz2") ||
								 fileExt == _T("xz") || fileExt == _T("iso") ||
								 fileExt == _T("pdf") || fileExt == _T("doc") ||
								 fileExt == _T("docx") || fileExt == _T("xls") ||
								 fileExt == _T("xlsx") || fileExt == _T("ppt") ||
								 fileExt == _T("pptx") || fileExt == _T("jpg") ||
								 fileExt == _T("jpeg") || fileExt == _T("png") ||
								 fileExt == _T("gif") || fileExt == _T("bmp") ||
								 fileExt == _T("tiff") || fileExt == _T("mp3") ||
								 fileExt == _T("mp4") || fileExt == _T("avi") ||
								 fileExt == _T("mkv") || fileExt == _T("wmv"));

			CString fileContent;

			// 实现大文件预览功能 - 发送端仅显示前1KB内容
			const size_t PREVIEW_SIZE = 1024; // 1KB预览
			bool isLargeFile = (fileLength > PREVIEW_SIZE);
			size_t displaySize = isLargeFile ? PREVIEW_SIZE : (size_t)fileLength;

			if (isBinaryFile)
			{
				// 二进制文件：保持原始字节数据用于16进制显示
				// 不使用fileContent，而是直接使用原始字节数据
			}
			else
			{
				// 文本文件：使用GBK编码转换
				int wideLen = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)fileBuffer, (int)displaySize, NULL, 0);
				if (wideLen > 0)
				{
					wchar_t *wideStr = new wchar_t[wideLen + 1];
					MultiByteToWideChar(CP_ACP, 0, (LPCSTR)fileBuffer, (int)displaySize, wideStr, wideLen);
					wideStr[wideLen] = 0;
					fileContent = wideStr;
					delete[] wideStr;
				}
				else
				{
					// 编码失败，使用原始字节
					for (size_t i = 0; i < displaySize; i++)
					{
						fileContent += (TCHAR)fileBuffer[i];
					}
				}
			}

			// 根据文件类型处理显示
			if (isBinaryFile)
			{
				// 二进制文件：缓存完整文件内容，使用统一显示逻辑
				UpdateSendCacheFromBytes(fileBuffer, (size_t)fileLength);

				// 使用统一的显示逻辑，而不是直接设置十六进制内容
				UpdateSendDisplayFromCache();
			}
			else
			{
				// 文本文件：显示预览内容
				UpdateSendCache(fileContent);

				// 设置到发送数据编辑框
				if (m_checkHex.GetCheck())
				{
					CString hexContent = StringToHex(fileContent);
					m_editSendData.SetWindowText(hexContent);
				}
				else
				{
					m_editSendData.SetWindowText(fileContent);
				}

				// 对于文本文件，如果是大文件，需要重新读取完整内容到缓存
				if (isLargeFile)
				{
					// 重新读取完整文件内容到缓存
					CString fullContent;
					int fullWideLen = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)fileBuffer, (int)fileLength, NULL, 0);
					if (fullWideLen > 0)
					{
						wchar_t *fullWideStr = new wchar_t[fullWideLen + 1];
						MultiByteToWideChar(CP_ACP, 0, (LPCSTR)fileBuffer, (int)fileLength, fullWideStr, fullWideLen);
						fullWideStr[fullWideLen] = 0;
						fullContent = fullWideStr;
						delete[] fullWideStr;
						UpdateSendCache(fullContent);
					}
				}
			}

			delete[] fileBuffer;

			// 更新数据源显示，包含大文件提示
			if (isLargeFile)
			{
				CString sizeInfo;
				if (fileLength < 1024 * 1024)
				{
					sizeInfo.Format(_T("来源: 文件 (%.1fKB, 大数据文件预览-部分内容)"), fileLength / 1024.0);
				}
				else
				{
					sizeInfo.Format(_T("来源: 文件 (%.1fMB, 大数据文件预览-部分内容)"), fileLength / (1024.0 * 1024.0));
				}
				m_staticSendSource.SetWindowText(sizeInfo);
			}
			else
			{
				m_staticSendSource.SetWindowText(_T("来源: 文件"));
			}
		}
		else
		{
			m_editSendData.SetWindowText(_T(""));
			m_staticSendSource.SetWindowText(_T("来源: 文件"));
		}

		file.Close();
	}
	else
	{
		MessageBox(_T("无法打开文件"), _T("错误"), MB_OK | MB_ICONERROR);
	}
}

void CPortMasterDlg::OnCbnSelchangeComboPortType()
{
	// 根据端口类型切换参数配置
	UpdatePortParameters();
}

void CPortMasterDlg::UpdatePortParameters()
{
	int nSel = m_comboPortType.GetCurSel();

	// 清空端口列表
	m_comboPort.ResetContent();

	switch (nSel)
	{
	case 0: // 串口
	{
		// 使用SetupAPI枚举实际串口
		std::vector<std::string> ports = SerialTransport::EnumerateSerialPorts();
		for (const auto &port : ports)
		{
			m_comboPort.AddString(CString(port.c_str()));
		}
		// 如果没有找到串口，添加默认选项
		if (ports.empty())
		{
			m_comboPort.AddString(_T("COM1"));
			m_comboPort.AddString(_T("COM2"));
			m_comboPort.AddString(_T("COM3"));
			m_comboPort.AddString(_T("COM4"));
		}
		// 显示串口相关参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_SHOW);
	}
	break;

	case 1: // 并口
	{
		// 枚举并口
		std::vector<std::string> ports = ParallelTransport::EnumerateParallelPorts();
		for (const auto &port : ports)
		{
			m_comboPort.AddString(CString(port.c_str()));
		}
		// 如果没有找到并口，添加默认选项
		if (ports.empty())
		{
			m_comboPort.AddString(_T("LPT1"));
			m_comboPort.AddString(_T("LPT2"));
			m_comboPort.AddString(_T("LPT3"));
		}
		// 隐藏串口参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_HIDE);
	}
	break;

	case 2: // USB打印
	{
		// 枚举USB打印端口
		std::vector<std::string> ports = UsbPrintTransport::EnumerateUsbPorts();
		for (const auto &port : ports)
		{
			m_comboPort.AddString(CString(port.c_str()));
		}
		// 如果没有找到USB打印端口，添加默认选项
		if (ports.empty())
		{
			m_comboPort.AddString(_T("USB001"));
			m_comboPort.AddString(_T("USB002"));
		}
		// 隐藏串口参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_HIDE);
	}
	break;

	case 3: // 网络打印
	{
		// 添加网络打印地址选项
		m_comboPort.AddString(_T("127.0.0.1:9100"));
		m_comboPort.AddString(_T("192.168.1.100:9100"));
		m_comboPort.AddString(_T("printer.local:9100"));
		// 隐藏串口参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_HIDE);
	}
	break;

	case 4: // 回路测试
	{
		// 添加回路测试选项
		m_comboPort.AddString(_T("Loopback"));
		// 隐藏串口参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_HIDE);
	}
	break;
	}

	m_comboPort.SetCurSel(0);
}

void CPortMasterDlg::OnBnClickedCheckHex()
{
	// 十六进制显示模式切换
	// 读取切换后的勾选状态，GetCheck()此时已经反映最新结果
	BOOL isHexMode = (m_checkHex.GetCheck() == BST_CHECKED);

	// 获取当前编辑框中的内容
	CString currentSendData;
	m_editSendData.GetWindowText(currentSendData);

	// 如果有内容，重新解析数据
	if (!currentSendData.IsEmpty())
	{
		if (isHexMode)
		{
			// 切换到十六进制模式：解析当前显示的内容
			// 如果当前显示的是文本，需要转换为十六进制
			if (!m_sendCacheValid)
			{
				// 缓存无效，直接缓存当前文本数据
				UpdateSendCache(currentSendData);
			}
			// 缓存有效，从缓存更新显示
		}
		else
		{
			// 切换到文本模式：如果缓存有效，直接使用缓存
			if (!m_sendCacheValid)
			{
				// 缓存无效，尝试从当前显示的十六进制数据中提取
				CString textData = HexToString(currentSendData);
				if (!textData.IsEmpty())
				{
					UpdateSendCache(textData);
				}
				else
				{
					// 如果无法提取，假设当前显示的就是文本
					UpdateSendCache(currentSendData);
				}
			}
		}
	}
	// 如果编辑框为空，清空缓存
	else if (m_sendCacheValid)
	{
		m_sendDataCache.clear();
		m_sendCacheValid = false;
	}

	// 根据缓存更新显示（这会基于原始数据进行正确的格式转换）
	UpdateSendDisplayFromCache();

	// 只有在接收缓存有效时才更新接收显示，避免模式切换时错误填充接收框
	if (m_receiveCacheValid && !m_receiveDataCache.empty())
	{
		UpdateReceiveDisplayFromCache();
	}
}

CString CPortMasterDlg::StringToHex(const CString &str)
{
	CString hexStr;
	CString asciiStr;

	// 将CString转换为UTF-8字节序列以正确处理Unicode字符
	CT2A utf8Str(str, CP_UTF8);
	const char *pUtf8 = utf8Str;
	int utf8Len = strlen(pUtf8);

	for (int i = 0; i < utf8Len; i++)
	{
		uint8_t byte = static_cast<uint8_t>(pUtf8[i]);

		// 添加偏移地址（每16字节一行）
		if (i % 16 == 0)
		{
			if (i > 0)
			{
				// 添加ASCII显示部分
				hexStr += _T("  |");
				hexStr += asciiStr;
				hexStr += _T("|\r\n");
				asciiStr.Empty();
			}

			CString offset;
			offset.Format(_T("%08X: "), i);
			hexStr += offset;
		}

		// 添加16进制值
		CString hexByte;
		hexByte.Format(_T("%02X "), byte);
		hexStr += hexByte;

		// 添加可打印字符到ASCII部分
		if (byte >= 32 && byte <= 126)
		{
			asciiStr += (TCHAR)byte;
		}
		else
		{
			asciiStr += _T(".");
		}
	}

	// 处理最后一行
	if (utf8Len % 16 != 0)
	{
		// 补齐空格
		int remain = 16 - (utf8Len % 16);
		for (int i = 0; i < remain; i++)
		{
			hexStr += _T("   ");
		}
	}

	// 添加ASCII显示部分
	hexStr += _T("  |");
	hexStr += asciiStr;
	hexStr += _T("|");

	return hexStr;
}

CString CPortMasterDlg::BytesToHex(const BYTE *data, size_t length)
{
	CString hexStr;
	CString asciiStr;

	for (size_t i = 0; i < length; i++)
	{
		uint8_t byte = data[i];

		// 添加偏移地址（每16字节一行）
		if (i % 16 == 0)
		{
			if (i > 0)
			{
				// 添加ASCII显示部分
				hexStr += _T("  |");
				hexStr += asciiStr;
				hexStr += _T("|\r\n");
				asciiStr.Empty();
			}

			CString offset;
			offset.Format(_T("%08X: "), (unsigned int)i);
			hexStr += offset;
		}

		// 添加16进制值
		CString hexByte;
		hexByte.Format(_T("%02X "), byte);
		hexStr += hexByte;

		// 添加可打印字符到ASCII部分
		if (byte >= 32 && byte <= 126)
		{
			asciiStr += (TCHAR)byte;
		}
		else
		{
			asciiStr += _T(".");
		}
	}

	// 处理最后一行
	if (length % 16 != 0)
	{
		// 补齐空格
		int remain = 16 - (length % 16);
		for (int i = 0; i < remain; i++)
		{
			hexStr += _T("   ");
		}
	}

	// 添加ASCII显示部分
	hexStr += _T("  |");
	hexStr += asciiStr;
	hexStr += _T("|");

	return hexStr;
}

CString CPortMasterDlg::HexToString(const CString &hex)
{
	// 从格式化的十六进制文本中提取纯十六进制字节对
	CString cleanHex;
	int len = hex.GetLength();
	bool inHexSection = false;

	for (int i = 0; i < len; i++)
	{
		TCHAR ch = hex[i];

		// 检测十六进制数据区域（在冒号之后）
		if (ch == _T(':'))
		{
			inHexSection = true;
			continue;
		}

		// 检测ASCII部分开始（在'|'字符处结束十六进制部分）
		if (ch == _T('|'))
		{
			inHexSection = false;
			continue;
		}

		// 只在十六进制数据区域内收集字符
		if (inHexSection)
		{
			// 只保留有效的十六进制字符
			if ((ch >= _T('0') && ch <= _T('9')) ||
				(ch >= _T('A') && ch <= _T('F')) ||
				(ch >= _T('a') && ch <= _T('f')))
			{
				cleanHex += ch;
			}
			// 跳过空格、换行符等其他字符
		}
	}

	// 确保字节对数为偶数
	if (cleanHex.GetLength() % 2 != 0)
	{
		// 如果长度为奇数，移除最后一个字符
		cleanHex = cleanHex.Left(cleanHex.GetLength() - 1);
	}

	// 转换十六进制字节对为字节数组
	std::vector<uint8_t> bytes;
	for (int i = 0; i < cleanHex.GetLength(); i += 2)
	{
		CString hexByte = cleanHex.Mid(i, 2);
		uint8_t byteValue = static_cast<uint8_t>(_tcstoul(hexByte, NULL, 16));
		bytes.push_back(byteValue);
	}

	// 将字节数组转换为UTF-8编码的CString
	if (!bytes.empty())
	{
		// 添加null终止符确保字符串安全
		bytes.push_back(0);
		CA2T utf8Result(reinterpret_cast<const char *>(bytes.data()), CP_UTF8);
		return CString(utf8Result);
	}

	// 如果没有有效的十六进制数据，返回空字符串
	return CString();
}

CString CPortMasterDlg::ExtractHexAsciiText(const CString &hex)
{
	// 从格式化的十六进制文本中提取ASCII部分
	CString asciiText;
	int len = hex.GetLength();
	bool inAsciiSection = false;

	for (int i = 0; i < len; i++)
	{
		TCHAR ch = hex[i];

		// 检测ASCII部分开始（在'|'字符处开始）
		if (ch == _T('|'))
		{
			inAsciiSection = true;
			continue;
		}

		// 检测ASCII部分结束（在第二个'|'字符处结束）
		if (inAsciiSection && ch == _T('|'))
		{
			break;
		}

		// 收集ASCII部分的字符
		if (inAsciiSection)
		{
			asciiText += ch;
		}
	}

	// 移除可能的换行符和回车符
	asciiText.Remove(_T('\r'));
	asciiText.Remove(_T('\n'));

	return asciiText;
}

// 基于缓存的格式转换函数实现

void CPortMasterDlg::UpdateSendDisplayFromCache()
{
	// 如果发送缓存有效且不为空，直接使用缓存显示
	if (m_sendCacheValid && !m_sendDataCache.empty())
	{
		// 实现100行显示限制（与接收窗口保持一致）
		const int MAX_DISPLAY_LINES = 100;

		// 根据当前显示模式从缓存更新显示
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 十六进制模式：每16字节一行
			CString hexDisplay;
			CString asciiStr;
			int lineCount = 0;

			// 计算需要显示的数据范围（最后100行）
			size_t totalLines = (m_sendDataCache.size() + 15) / 16; // 向上取整
			size_t startLine = (totalLines > MAX_DISPLAY_LINES) ? (totalLines - MAX_DISPLAY_LINES) : 0;
			size_t startByte = startLine * 16;

			// 如果数据被截断，添加提示
			if (startLine > 0)
			{
				hexDisplay += _T("... (显示最后100行数据) ...\r\n");
			}

			for (size_t i = startByte; i < m_sendDataCache.size(); i++)
			{
				uint8_t byte = m_sendDataCache[i];

				// 添加偏移地址（每16字节一行）
				if (i % 16 == 0)
				{
					if (i > startByte)
					{
						// 添加ASCII显示部分
						hexDisplay += _T("  |");
						hexDisplay += asciiStr;
						hexDisplay += _T("|\r\n");
						asciiStr.Empty();
						lineCount++;
					}

					CString offset;
					offset.Format(_T("%08X: "), (int)i);
					hexDisplay += offset;
				}

				// 添加16进制值
				CString hexByte;
				hexByte.Format(_T("%02X "), byte);
				hexDisplay += hexByte;

				// 添加可打印字符到ASCII部分
				if (byte >= 32 && byte <= 126)
				{
					asciiStr += (TCHAR)byte;
				}
				else
				{
					asciiStr += _T(".");
				}
			}

			// 处理最后一行
			if (m_sendDataCache.size() % 16 != 0)
			{
				// 补齐空格
				size_t remain = 16 - (m_sendDataCache.size() % 16);
				for (size_t i = 0; i < remain; i++)
				{
					hexDisplay += _T("   ");
				}
			}

			// 添加ASCII显示部分
			hexDisplay += _T("  |");
			hexDisplay += asciiStr;
			hexDisplay += _T("|");

			// 设置标志防止OnEnChangeEditSendData事件触发
			m_updateDisplayInProgress = true;
			m_editSendData.SetWindowText(hexDisplay);
			m_updateDisplayInProgress = false;
		}
		else
		{
			// 文本模式：智能编码检测和显示（与接收窗口完全一致）
			std::vector<char> safeData(m_sendDataCache.begin(), m_sendDataCache.end());
			safeData.push_back('\0');

			CString displayText;
			bool decoded = false;

			// 检测是否为二进制数据（包含大量不可打印字符）
			size_t nonPrintableCount = 0;
			size_t nullByteCount = 0;
			size_t sampleSize = min(m_sendDataCache.size(), (size_t)1024); // 检查前1KB

			for (size_t i = 0; i < sampleSize; i++)
			{
				uint8_t byte = m_sendDataCache[i];
				if (byte == 0)
				{
					nullByteCount++;
				}
				else if (byte < 32 && byte != '\r' && byte != '\n' && byte != '\t')
				{
					nonPrintableCount++;
				}
			}

			// 改进的二进制检测逻辑：
			// 1. 如果包含空字节，很可能是二进制文件
			// 2. 如果不可打印字符超过20%，认为是二进制数据
			bool isBinaryData = false;
			if (sampleSize > 0)
			{
				if (nullByteCount > 0 || (nonPrintableCount * 100 / sampleSize) > 20)
				{
					isBinaryData = true;
				}
			}

			if (isBinaryData)
			{
				displayText = _T("=== 检测到二进制数据 ===\r\n");
				displayText += _T("建议切换到十六进制模式查看完整内容\r\n");

				CString sizeInfo;
				sizeInfo.Format(_T("数据大小: %u 字节\r\n"), (unsigned int)m_sendDataCache.size());
				displayText += sizeInfo;

				CString charInfo;
				charInfo.Format(_T("不可打印字符: %u (%.1f%%)\r\n"),
								(unsigned int)nonPrintableCount,
								sampleSize > 0 ? (nonPrintableCount * 100.0 / sampleSize) : 0);
				displayText += charInfo;

				if (nullByteCount > 0)
				{
					CString nullInfo;
					nullInfo.Format(_T("空字节数量: %u\r\n"), (unsigned int)nullByteCount);
					displayText += nullInfo;
				}
				displayText += _T("\r\n--- 数据预览（仅显示可打印字符）---\r\n");

				// 显示前512字节的可打印字符作为预览，按行格式化
				size_t previewSize = min(m_sendDataCache.size(), (size_t)512);
				CString previewLine;
				int lineLength = 0;

				for (size_t i = 0; i < previewSize; i++)
				{
					uint8_t byte = m_sendDataCache[i];
					if (byte >= 32 && byte <= 126)
					{
						previewLine += (TCHAR)byte;
						lineLength++;
					}
					else if (byte == '\r')
					{
						previewLine += _T("\\r");
						lineLength += 2;
					}
					else if (byte == '\n')
					{
						previewLine += _T("\\n\r\n");
						displayText += previewLine;
						previewLine.Empty();
						lineLength = 0;
					}
					else if (byte == '\t')
					{
						previewLine += _T("\\t");
						lineLength += 2;
					}
					else
					{
						previewLine += _T(".");
						lineLength++;
					}

					// 每80个字符换行
					if (lineLength >= 80)
					{
						previewLine += _T("\r\n");
						displayText += previewLine;
						previewLine.Empty();
						lineLength = 0;
					}
				}

				// 添加剩余的预览内容
				if (!previewLine.IsEmpty())
				{
					displayText += previewLine;
				}

				if (m_sendDataCache.size() > 512)
				{
					displayText += _T("\r\n\r\n... (更多内容请切换到十六进制模式查看) ...");
				}

				decoded = true;
			}
			else
			{
				// 尝试UTF-8解码
				try
				{
					CA2T utf8Text(safeData.data(), CP_UTF8);
					CString testResult = CString(utf8Text);

					if (!testResult.IsEmpty())
					{
						displayText = testResult;
						decoded = true;
					}
				}
				catch (...)
				{
					// UTF-8解码失败，继续尝试其他编码
				}

				// 如果UTF-8失败，尝试GBK解码
				if (!decoded)
				{
					try
					{
						CA2T gbkText(safeData.data(), CP_ACP);
						CString testResult = CString(gbkText);

						if (!testResult.IsEmpty())
						{
							displayText = testResult;
							decoded = true;
						}
					}
					catch (...)
					{
						// GBK解码失败
					}
				}

				// 如果所有编码都失败，显示原始字节
				if (!decoded)
				{
					for (size_t i = 0; i < m_sendDataCache.size(); i++)
					{
						uint8_t byte = m_sendDataCache[i];
						displayText += (TCHAR)byte;
					}
				}
			}

			// 实现100行限制（与接收窗口保持一致）
			CString lines[MAX_DISPLAY_LINES];
			int totalLines = 0;
			int currentPos = 0;

			// 分割文本为行
			while (currentPos < displayText.GetLength() && totalLines < MAX_DISPLAY_LINES * 2) // 预留缓冲
			{
				int nextPos = displayText.Find(_T('\n'), currentPos);
				if (nextPos == -1)
				{
					if (currentPos < displayText.GetLength())
					{
						lines[totalLines % MAX_DISPLAY_LINES] = displayText.Mid(currentPos);
						totalLines++;
					}
					break;
				}
				else
				{
					lines[totalLines % MAX_DISPLAY_LINES] = displayText.Mid(currentPos, nextPos - currentPos + 1);
					totalLines++;
					currentPos = nextPos + 1;
				}
			}

			// 构建最终显示文本（最后100行）
			CString finalText;
			int startLineIndex = (totalLines > MAX_DISPLAY_LINES) ? (totalLines - MAX_DISPLAY_LINES) : 0;

			if (startLineIndex > 0)
			{
				finalText += _T("... (显示最后100行数据) ...\r\n");
			}

			for (int i = startLineIndex; i < totalLines; i++)
			{
				finalText += lines[i % MAX_DISPLAY_LINES];
			}

			// 设置标志防止OnEnChangeEditSendData事件触发
			m_updateDisplayInProgress = true;
			m_editSendData.SetWindowText(finalText);
			m_updateDisplayInProgress = false;
		}

		// 自动滚动到最新数据（与接收窗口保持一致）
		m_editSendData.SetSel(-1, -1);							  // 移动光标到末尾
		m_editSendData.LineScroll(m_editSendData.GetLineCount()); // 滚动到底部
	}
	else
	{
		// 缓存无效或为空，尝试从当前显示内容获取数据
		CString currentData;
		m_editSendData.GetWindowText(currentData);

		if (!currentData.IsEmpty())
		{
			// 如果当前是十六进制模式且内容看起来是十六进制格式
			if (m_checkHex.GetCheck() == BST_CHECKED)
			{
				// 检查是否为十六进制格式（包含冒号和十六进制字符）
				if (currentData.Find(_T(':')) >= 0)
				{
					// 尝试从十六进制显示中提取原始数据
					CString textData = HexToString(currentData);
					if (!textData.IsEmpty())
					{
						// 更新缓存
						UpdateSendCache(textData);
						// 重新调用此函数以正确显示
						UpdateSendDisplayFromCache();
						return;
					}
				}
				else
				{
					// 直接缓存当前的十六进制文本数据
					UpdateSendCache(currentData);
					// 重新调用此函数以正确显示
					UpdateSendDisplayFromCache();
					return;
				}
			}
			else
			{
				// 文本模式，直接缓存当前文本数据
				UpdateSendCache(currentData);
				// 重新调用此函数以正确显示
				UpdateSendDisplayFromCache();
				return;
			}
		}
		else
		{
			// 没有数据，清空显示
			m_updateDisplayInProgress = true;
			m_editSendData.SetWindowText(_T(""));
			m_updateDisplayInProgress = false;
		}
	}
}

void CPortMasterDlg::UpdateReceiveDisplayFromCache()
{
	// 如果接收缓存无效，直接返回
	if (!m_receiveCacheValid || m_receiveDataCache.empty())
	{
		m_editReceiveData.SetWindowText(_T(""));
		return;
	}

	// 实现100行显示限制
	const int MAX_DISPLAY_LINES = 100;

	// 根据当前显示模式从缓存更新显示
	if (m_checkHex.GetCheck() == BST_CHECKED)
	{
		// 十六进制模式：每16字节一行
		CString hexDisplay;
		CString asciiStr;
		int lineCount = 0;

		// 计算需要显示的数据范围（最后100行）
		size_t totalLines = (m_receiveDataCache.size() + 15) / 16; // 向上取整
		size_t startLine = (totalLines > MAX_DISPLAY_LINES) ? (totalLines - MAX_DISPLAY_LINES) : 0;
		size_t startByte = startLine * 16;

		// 如果数据被截断，添加提示
		if (startLine > 0)
		{
			hexDisplay += _T("... (显示最后100行数据) ...\r\n");
		}

		for (size_t i = startByte; i < m_receiveDataCache.size(); i++)
		{
			uint8_t byte = m_receiveDataCache[i];

			// 添加偏移地址（每16字节一行）
			if (i % 16 == 0)
			{
				if (i > startByte)
				{
					// 添加ASCII显示部分
					hexDisplay += _T("  |");
					hexDisplay += asciiStr;
					hexDisplay += _T("|\r\n");
					asciiStr.Empty();
					lineCount++;
				}

				CString offset;
				offset.Format(_T("%08X: "), (int)i);
				hexDisplay += offset;
			}

			// 添加16进制值
			CString hexByte;
			hexByte.Format(_T("%02X "), byte);
			hexDisplay += hexByte;

			// 添加可打印字符到ASCII部分
			if (byte >= 32 && byte <= 126)
			{
				asciiStr += (TCHAR)byte;
			}
			else
			{
				asciiStr += _T(".");
			}
		}

		// 处理最后一行
		if (m_receiveDataCache.size() % 16 != 0)
		{
			// 补齐空格
			size_t remain = 16 - (m_receiveDataCache.size() % 16);
			for (size_t i = 0; i < remain; i++)
			{
				hexDisplay += _T("   ");
			}
		}

		// 添加ASCII显示部分
		hexDisplay += _T("  |");
		hexDisplay += asciiStr;
		hexDisplay += _T("|");

		m_editReceiveData.SetWindowText(hexDisplay);
	}
	else
	{
		// 文本模式：智能编码检测和显示
		std::vector<char> safeData(m_receiveDataCache.begin(), m_receiveDataCache.end());
		safeData.push_back('\0');

		CString displayText;
		bool decoded = false;

		// 检测是否为二进制数据（包含大量不可打印字符）
		size_t nonPrintableCount = 0;
		size_t nullByteCount = 0;
		size_t sampleSize = min(m_receiveDataCache.size(), (size_t)1024); // 检查前1KB

		for (size_t i = 0; i < sampleSize; i++)
		{
			uint8_t byte = m_receiveDataCache[i];
			if (byte == 0)
			{
				nullByteCount++;
			}
			else if (byte < 32 && byte != '\r' && byte != '\n' && byte != '\t')
			{
				nonPrintableCount++;
			}
		}

		// 改进的二进制检测逻辑：
		// 1. 如果包含空字节，很可能是二进制文件
		// 2. 如果不可打印字符超过20%，认为是二进制数据
		bool isBinaryData = false;
		if (sampleSize > 0)
		{
			if (nullByteCount > 0 || (nonPrintableCount * 100 / sampleSize) > 20)
			{
				isBinaryData = true;
			}
		}

		if (isBinaryData)
		{
			// 如果是首次检测到二进制数据，生成静态预览内容
			if (!m_binaryDataDetected)
			{
				m_binaryDataDetected = true;

				// 生成简化的静态提示内容
				m_binaryDataPreview = _T("=== 检测到二进制数据 ===\r\n");
				m_binaryDataPreview += _T("建议切换到十六进制模式查看完整内容\r\n");
				m_binaryDataPreview += _T("\r\n--- 数据预览（仅显示可打印字符）---\r\n");

				// 显示前512字节的可打印字符作为预览，按行格式化
				size_t previewSize = min(m_receiveDataCache.size(), (size_t)512);
				CString previewLine;
				int lineLength = 0;

				for (size_t i = 0; i < previewSize; i++)
				{
					uint8_t byte = m_receiveDataCache[i];
					if (byte >= 32 && byte <= 126)
					{
						previewLine += (TCHAR)byte;
						lineLength++;
					}
					else if (byte == '\r')
					{
						previewLine += _T("\\r");
						lineLength += 2;
					}
					else if (byte == '\n')
					{
						previewLine += _T("\\n\r\n");
						m_binaryDataPreview += previewLine;
						previewLine.Empty();
						lineLength = 0;
					}
					else if (byte == '\t')
					{
						previewLine += _T("\\t");
						lineLength += 2;
					}
					else
					{
						previewLine += _T(".");
						lineLength++;
					}

					// 每80个字符换行
					if (lineLength >= 80)
					{
						previewLine += _T("\r\n");
						m_binaryDataPreview += previewLine;
						previewLine.Empty();
						lineLength = 0;
					}
				}

				// 添加剩余的预览内容
				if (!previewLine.IsEmpty())
				{
					m_binaryDataPreview += previewLine;
				}

				if (m_receiveDataCache.size() > 512)
				{
					m_binaryDataPreview += _T("\r\n\r\n... (更多内容请切换到十六进制模式查看) ...");
				}
			}

			// 使用静态预览内容，避免重复刷新
			displayText = m_binaryDataPreview;
			decoded = true;
		}
		else
		{
			// 尝试UTF-8解码
			try
			{
				CA2T utf8Text(safeData.data(), CP_UTF8);
				CString testResult = CString(utf8Text);

				if (!testResult.IsEmpty())
				{
					displayText = testResult;
					decoded = true;
				}
			}
			catch (...)
			{
				// UTF-8解码失败，继续尝试其他编码
			}

			// 如果UTF-8失败，尝试GBK解码
			if (!decoded)
			{
				try
				{
					CA2T gbkText(safeData.data(), CP_ACP);
					CString testResult = CString(gbkText);

					if (!testResult.IsEmpty())
					{
						displayText = testResult;
						decoded = true;
					}
				}
				catch (...)
				{
					// GBK解码失败
				}
			}

			// 如果所有编码都失败，显示原始字节
			if (!decoded)
			{
				for (size_t i = 0; i < m_receiveDataCache.size(); i++)
				{
					uint8_t byte = m_receiveDataCache[i];
					displayText += (TCHAR)byte;
				}
			}
		}

		// 实现100行限制
		CString lines[MAX_DISPLAY_LINES];
		int totalLines = 0;
		int currentPos = 0;

		// 分割文本为行
		while (currentPos < displayText.GetLength() && totalLines < MAX_DISPLAY_LINES * 2) // 预留缓冲
		{
			int nextPos = displayText.Find(_T('\n'), currentPos);
			if (nextPos == -1)
			{
				if (currentPos < displayText.GetLength())
				{
					lines[totalLines % MAX_DISPLAY_LINES] = displayText.Mid(currentPos);
					totalLines++;
				}
				break;
			}
			else
			{
				lines[totalLines % MAX_DISPLAY_LINES] = displayText.Mid(currentPos, nextPos - currentPos + 1);
				totalLines++;
				currentPos = nextPos + 1;
			}
		}

		// 构建最终显示文本（最后100行）
		CString finalText;
		int startLineIndex = (totalLines > MAX_DISPLAY_LINES) ? (totalLines - MAX_DISPLAY_LINES) : 0;

		if (startLineIndex > 0)
		{
			finalText += _T("... (显示最后100行数据) ...\r\n");
		}

		for (int i = startLineIndex; i < totalLines; i++)
		{
			finalText += lines[i % MAX_DISPLAY_LINES];
		}

		m_editReceiveData.SetWindowText(finalText);
	}

	// 自动滚动到最新数据
	m_editReceiveData.SetSel(-1, -1);								// 移动光标到末尾
	m_editReceiveData.LineScroll(m_editReceiveData.GetLineCount()); // 滚动到底部
}

// 【UI优化】节流的接收显示更新 - 避免大文件传输时频繁UI刷新导致的窗口闪烁
void CPortMasterDlg::ThrottledUpdateReceiveDisplay()
{
	DWORD currentTime = ::GetTickCount();

	// 检查是否在节流间隔内
	if (currentTime - m_lastReceiveDisplayUpdate < RECEIVE_DISPLAY_THROTTLE_MS)
	{
		// 如果在节流间隔内，设置待处理标志并启动定时器
		if (!m_receiveDisplayPending)
		{
			m_receiveDisplayPending = true;
			// 启动延时定时器，延时时间为剩余的节流间隔
			DWORD remainingTime = RECEIVE_DISPLAY_THROTTLE_MS - (currentTime - m_lastReceiveDisplayUpdate);
			SetTimer(TIMER_ID_THROTTLED_DISPLAY, remainingTime, nullptr);
		}
		// 如果已经有待处理的更新，不需要重复设置定时器
	}
	else
	{
		// 超过节流间隔，立即更新显示
		UpdateReceiveDisplayFromCache();
		m_lastReceiveDisplayUpdate = currentTime;
		m_receiveDisplayPending = false;

		// 确保定时器被取消（以防万一）
		KillTimer(TIMER_ID_THROTTLED_DISPLAY);
	}
}

void CPortMasterDlg::UpdateSendCache(const CString &data)
{
	// 将CString转换为字节序列并缓存
	m_sendDataCache.clear();

	// 获取字符串长度（字符数）
	int len = data.GetLength();

	// 为每个字符分配2字节（Unicode转换为UTF-8可能需要多字节）
	m_sendDataCache.reserve(len * 3); // 预留足够空间

	// 遍历每个字符并转换为UTF-8
	for (int i = 0; i < len; i++)
	{
		TCHAR ch = data[i];

		// 简单处理：ASCII字符直接转换
		if (ch < 128)
		{
			m_sendDataCache.push_back(static_cast<uint8_t>(ch));
		}
		else
		{
			// 对于非ASCII字符，使用WideCharToMultiByte进行转换
			wchar_t wch = ch;
			char utf8Buf[4] = {0};
			int utf8Len = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utf8Buf, 4, NULL, NULL);

			for (int j = 0; j < utf8Len; j++)
			{
				m_sendDataCache.push_back(static_cast<uint8_t>(utf8Buf[j]));
			}
		}
	}

	this->WriteLog("=== UpdateSendCache 结束 ===");
	m_sendCacheValid = true;
}

void CPortMasterDlg::UpdateSendCacheFromBytes(const BYTE *data, size_t length)
{
	// 直接从字节数据更新缓存，避免任何编码转换
	m_sendDataCache.clear();
	m_sendDataCache.reserve(length);

	// 直接复制字节数据，保持原始值不变
	for (size_t i = 0; i < length; i++)
	{
		m_sendDataCache.push_back(data[i]);
	}

	this->WriteLog("=== UpdateSendCacheFromBytes 结束 ===");
	m_sendCacheValid = true;
}

void CPortMasterDlg::UpdateSendCacheFromHex(const CString &hexData)
{
	// 在十六进制模式下，将十六进制字符串解析为字节数据并缓存
	m_sendDataCache.clear();

	// 提取有效的十六进制字符
	CString cleanHex;
	int len = hexData.GetLength();

	for (int i = 0; i < len; i++)
	{
		TCHAR ch = hexData[i];
		// 只保留有效的十六进制字符
		if ((ch >= _T('0') && ch <= _T('9')) ||
			(ch >= _T('A') && ch <= _T('F')) ||
			(ch >= _T('a') && ch <= _T('f')))
		{
			cleanHex += ch;
		}
		// 自动跳过空格、换行符、偏移地址("00000000:")、ASCII分隔符("|...|")等
	}

	// 检查是否有有效的十六进制数据
	if (cleanHex.IsEmpty())
	{
		m_sendCacheValid = false;
		return;
	}

	// 确保字节对数为偶数
	if (cleanHex.GetLength() % 2 != 0)
	{
		// 如果长度为奇数，移除最后一个字符
		cleanHex = cleanHex.Left(cleanHex.GetLength() - 1);
	}

	// 转换为字节数组
	for (int i = 0; i < cleanHex.GetLength(); i += 2)
	{
		CString byteStr = cleanHex.Mid(i, 2);
		uint8_t byteValue = static_cast<uint8_t>(_tcstoul(byteStr, NULL, 16));
		m_sendDataCache.push_back(byteValue);
	}

	m_sendCacheValid = true;
}

void CPortMasterDlg::UpdateReceiveCache(const std::vector<uint8_t> &data)
{
	// 添加调试输出：记录缓存更新过程
	this->WriteLog("=== UpdateReceiveCache 开始 ===");
	this->WriteLog("更新接收缓存 - 输入数据大小: " + std::to_string(data.size()) + " 字节");
	this->WriteLog("更新接收缓存 - 当前缓存状态: " + std::string(m_receiveCacheValid ? "有效" : "无效"));
	this->WriteLog("更新接收缓存 - 当前缓存数据大小: " + std::to_string(m_receiveDataCache.size()) + " 字节");

	// 修复：追加接收到的字节数据，而不是覆盖
	if (!m_receiveCacheValid || m_receiveDataCache.empty())
	{
		// 如果缓存无效或为空，直接设置新数据
		this->WriteLog("更新接收缓存 - 缓存无效或为空，直接设置新数据");
		m_receiveDataCache = data;
	}
	else
	{
		// 如果缓存有效且不为空，追加新数据
		this->WriteLog("更新接收缓存 - 追加新数据到现有缓存");
		size_t oldSize = m_receiveDataCache.size();
		m_receiveDataCache.insert(m_receiveDataCache.end(), data.begin(), data.end());
		this->WriteLog("更新接收缓存 - 数据追加完成，从 " + std::to_string(oldSize) + " 字节增加到 " + std::to_string(m_receiveDataCache.size()) + " 字节");
	}
	m_receiveCacheValid = true;

	this->WriteLog("更新接收缓存 - 缓存更新完成，新的缓存数据大小: " + std::to_string(m_receiveDataCache.size()) + " 字节");

	// 【深层修复】不再调用WriteDataToTempCache，避免与ThreadSafeAppendReceiveData重复写入
	// UpdateReceiveCache现在仅负责内存缓存管理，临时文件由ThreadSafeAppendReceiveData统一处理
	this->WriteLog("更新接收缓存 - 仅更新内存缓存，临时文件由直接落盘函数统一处理");

	this->WriteLog("=== UpdateReceiveCache 结束 ===");
}

// 【深层修复】线程安全的接收数据直接落盘函数
// 根据最新检查报告最优解方案：接收线程直接落盘，避免消息队列异步延迟导致的数据丢失
void CPortMasterDlg::ThreadSafeAppendReceiveData(const std::vector<uint8_t> &data)
{
	// 使用接收文件专用互斥锁，确保与保存操作完全互斥
	std::lock_guard<std::mutex> lock(m_receiveFileMutex);

	// 增强日志：记录详细的直接落盘过程
	this->WriteLog("=== ThreadSafeAppendReceiveData 开始（接收线程直接落盘）===");
	this->WriteLog("接收数据大小: " + std::to_string(data.size()) + " 字节");
	this->WriteLog("当前接收缓存大小: " + std::to_string(m_receiveDataCache.size()) + " 字节");
	this->WriteLog("总接收字节数: " + std::to_string(m_totalReceivedBytes) + " 字节");

	// 1. 立即更新内存缓存（内存缓存作为备份，保证数据完整性）
	if (!m_receiveCacheValid || m_receiveDataCache.empty())
	{
		this->WriteLog("初始化接收缓存（首次接收）");
		m_receiveDataCache = data;
	}
	else
	{
		this->WriteLog("追加数据到接收缓存");
		size_t oldSize = m_receiveDataCache.size();
		m_receiveDataCache.insert(m_receiveDataCache.end(), data.begin(), data.end());
		this->WriteLog("缓存追加完成: " + std::to_string(oldSize) + " → " + std::to_string(m_receiveDataCache.size()) + " 字节");
	}
	m_receiveCacheValid = true;

	// 2. 立即同步写入临时文件（强制串行化，确保数据完全落盘）
	if (m_useTempCacheFile && m_tempCacheFile.is_open())
	{
		this->WriteLog("执行强制同步写入临时缓存文件...");
		try
		{
			// 写入数据到文件
			m_tempCacheFile.write(reinterpret_cast<const char*>(data.data()), data.size());

			// 强制刷新缓冲区到磁盘，确保数据真正落盘
			m_tempCacheFile.flush();

			// 更新总字节数统计
			m_totalReceivedBytes += data.size();

			this->WriteLog("强制同步写入成功: " + std::to_string(data.size()) + " 字节");
			this->WriteLog("更新后总接收字节数: " + std::to_string(m_totalReceivedBytes) + " 字节");
		}
		catch (const std::exception& e)
		{
			this->WriteLog("临时缓存文件写入异常: " + std::string(e.what()));
			this->WriteLog("数据已保存到内存缓存，文件写入失败但不影响数据完整性");
		}
	}
	else
	{
		this->WriteLog("临时缓存文件未启用或未打开，仅更新内存缓存");
		// 即使没有临时文件，也要更新总字节数
		m_totalReceivedBytes += data.size();
		this->WriteLog("更新总接收字节数（仅内存）: " + std::to_string(m_totalReceivedBytes) + " 字节");
	}

	this->WriteLog("=== ThreadSafeAppendReceiveData 结束（数据已强制落盘）===");
}

void CPortMasterDlg::LogMessage(const CString &message)
{
	// 实现日志消息显示，支持自动换行且仅显示最新一条
	CString formattedMessage = message;

	// 确保消息适合控件宽度，实现自动换行效果
	CWnd *pLogWnd = GetDlgItem(IDC_STATIC_LOG);
	if (pLogWnd != nullptr)
	{
		CRect rect;
		pLogWnd->GetClientRect(&rect);

		CDC *pDC = pLogWnd->GetDC();
		if (pDC != nullptr)
		{
			CFont *pOldFont = pDC->SelectObject(pLogWnd->GetFont());

			// 计算可用宽度
			int maxWidth = rect.Width() - 10; // 留一些边距

			// 如果消息太长，进行简单换行处理
			CString wrappedMessage;
			int startPos = 0;
			while (startPos < formattedMessage.GetLength())
			{
				int chunkEnd = startPos;
				CSize textSize;

				// 找到适合当前行的最大字符数
				while (chunkEnd < formattedMessage.GetLength())
				{
					CString chunk = formattedMessage.Mid(startPos, chunkEnd - startPos + 1);
					textSize = pDC->GetTextExtent(chunk);
					if (textSize.cx > maxWidth)
					{
						if (chunkEnd > startPos)
							chunkEnd--; // 回退一个字符
						break;
					}
					chunkEnd++;
				}

				if (chunkEnd <= startPos)
					chunkEnd = startPos + 1; // 至少一个字符

				if (!wrappedMessage.IsEmpty())
					wrappedMessage += _T(" "); // 用空格连接行
				wrappedMessage += formattedMessage.Mid(startPos, chunkEnd - startPos);

				startPos = chunkEnd;
			}

			pDC->SelectObject(pOldFont);
			pLogWnd->ReleaseDC(pDC);

			formattedMessage = wrappedMessage;
		}
	}

	// 设置日志文本（仅显示最新一条）
	SetDlgItemText(IDC_STATIC_LOG, formattedMessage);
}

void CPortMasterDlg::OnBnClickedRadioReliable()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("可靠模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);

	// 更新状态条模式信息
	m_staticMode.SetWindowText(_T("可靠"));
}

void CPortMasterDlg::OnEnChangeEditSendData()
{
	// 如果正在程序内部更新显示，忽略此事件，防止递归调用
	if (m_updateDisplayInProgress)
	{
		return;
	}

	// 当用户在发送数据编辑框中输入内容时，更新缓存
	CString currentData;
	m_editSendData.GetWindowText(currentData);

	if (!currentData.IsEmpty())
	{
		// 根据当前模式更新缓存
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 十六进制模式：解析十六进制字符串为字节数据
			UpdateSendCacheFromHex(currentData);
		}
		else
		{
			// 文本模式：直接缓存文本数据
			UpdateSendCache(currentData);
		}
	}
	else
	{
		// 如果编辑框为空，清空缓存
		m_sendDataCache.clear();
		m_sendCacheValid = false;
	}
}

void CPortMasterDlg::OnBnClickedButtonClearAll()
{
	// 只清空发送数据编辑框（重命名为"清空发送框"后的新功能）
	m_editSendData.SetWindowText(_T(""));

	// 重置数据源显示
	SetDlgItemText(IDC_STATIC_SEND_SOURCE, _T("来源: 手动输入"));

	// 更新日志
	CTime time = CTime::GetCurrentTime();
	CString timeStr = time.Format(_T("[%H:%M:%S] "));
	LogMessage(timeStr + _T("清空发送框"));
}

void CPortMasterDlg::OnBnClickedButtonClearReceive()
{
	// 清空接收数据编辑框
	m_editReceiveData.SetWindowText(_T(""));

	// 清空内存中的接收缓存（关键修复）
	m_receiveDataCache.clear();
	m_receiveCacheValid = false;

	// 重置二进制数据检测状态，允许重新检测
	m_binaryDataDetected = false;
	m_binaryDataPreview.Empty();

	// 清空临时缓存文件
	ClearTempCacheFile();

	// 更新日志
	CTime time = CTime::GetCurrentTime();
	CString timeStr = time.Format(_T("[%H:%M:%S] "));
	LogMessage(timeStr + _T("清空接收框"));
}

void CPortMasterDlg::OnBnClickedButtonCopyAll()
{
	// 复制接收数据到剪贴板
	CString copyData;

	// 优先从临时缓存文件读取原始数据（如果可用）
	if (m_useTempCacheFile && !m_tempCacheFilePath.IsEmpty())
	{
		std::vector<uint8_t> cachedData = ReadAllDataFromTempCache();
		if (!cachedData.empty())
		{
			if (m_checkHex.GetCheck() == BST_CHECKED)
			{
				// 十六进制模式：复制格式化的十六进制文本（避免null字符截断问题）
				copyData = BytesToHex(cachedData.data(), cachedData.size());
			}
			else
			{
				// 文本模式：将原始数据转换为文本显示
				try
				{
					std::vector<char> safeData(cachedData.begin(), cachedData.end());
					safeData.push_back('\0');
					CA2T utf8Text(safeData.data(), CP_UTF8);
					copyData = CString(utf8Text);
				}
				catch (...)
				{
					// UTF-8解码失败，显示可打印字符
					for (uint8_t byte : cachedData)
					{
						if (byte >= 32 && byte <= 126)
						{
							copyData += (TCHAR)byte;
						}
						else if (byte == 0)
						{
							copyData += _T("[NUL]");
						}
						else
						{
							copyData += _T(".");
						}
					}
				}
			}
		}
	}

	// 如果临时缓存不可用，使用原有逻辑
	if (copyData.IsEmpty())
	{
		// 如果处于十六进制模式，复制当前显示的完整格式化内容
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 直接复制当前显示的完整16进制格式内容
			m_editReceiveData.GetWindowText(copyData);
		}
		else
		{
			// 文本模式：直接复制当前显示的内容
			m_editReceiveData.GetWindowText(copyData);
		}
	}

	if (!copyData.IsEmpty())
	{
		if (OpenClipboard())
		{
			EmptyClipboard();

			HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (copyData.GetLength() + 1) * sizeof(TCHAR));
			if (hGlobal)
			{
				LPTSTR lpszData = (LPTSTR)GlobalLock(hGlobal);
				_tcscpy_s(lpszData, copyData.GetLength() + 1, copyData);
				GlobalUnlock(hGlobal);

#ifdef UNICODE
				SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
				SetClipboardData(CF_TEXT, hGlobal);
#endif
			}
			CloseClipboard();

			MessageBox(_T("数据已复制到剪贴板"), _T("提示"), MB_OK | MB_ICONINFORMATION);
		}
	}
	else
	{
		MessageBox(_T("没有数据可复制"), _T("提示"), MB_OK | MB_ICONWARNING);
	}
}

void CPortMasterDlg::OnBnClickedButtonSaveAll()
{
	// 保存接收数据到文件
	std::vector<uint8_t> binaryData;
	CString saveData;
	bool isBinaryMode = false;

	// 添加调试输出：记录保存操作的详细信息
	this->WriteLog("=== 保存数据调试信息 ===");
	this->WriteLog("当前十六进制模式: " + std::to_string(m_checkHex.GetCheck() == BST_CHECKED));
	this->WriteLog("使用临时缓存文件: " + std::to_string(m_useTempCacheFile));
	this->WriteLog("临时缓存文件路径: " + std::string(CT2A(m_tempCacheFilePath)));

	// 【可靠模式缺陷修复】根据分析报告实施完整性自愈循环和重新读取机制
	if (m_useTempCacheFile && !m_tempCacheFilePath.IsEmpty())
	{
		this->WriteLog("=== 可靠模式保存流程开始 ===");
		this->WriteLog("临时缓存文件路径: " + std::string(CT2A(m_tempCacheFilePath)));
		this->WriteLog("临时缓存文件是否存在: " + std::string(PathFileExists(m_tempCacheFilePath) ? "是" : "否"));

		std::vector<uint8_t> finalCachedData;
		bool dataStabilized = false;

		// 【数据稳定性检测机制】替代原有的完整性自愈循环
		const int MAX_STABILITY_CHECKS = 10;
		const int STABILITY_CHECK_INTERVAL_MS = 300;

		// 可靠模式额外延迟，给握手和重传更多时间
		bool isReliableMode = m_radioReliable.GetCheck() == BST_CHECKED;
		if (isReliableMode)
		{
			this->WriteLog("可靠模式检测，添加额外500ms延迟等待握手完成...");
			Sleep(500);
		}

		StabilityTracker stabilityTracker;
		this->WriteLog("=== 开始数据稳定性检测 ===");

		// 【增强传输状态感知】在稳定性检测前检查传输状态
		if (m_isTransmitting || m_transmissionPaused)
		{
			this->WriteLog("⚠️ 检测到传输仍在进行中 (isTransmitting=" + std::string(m_isTransmitting ? "true" : "false") +
			               ", isPaused=" + std::string(m_transmissionPaused ? "true" : "false") + ")");
			this->WriteLog("建议等待传输完全结束后再保存数据");
		}

		// 【优化】检查可靠传输协议状态
		bool reliableTransferActive = false;
		if (m_reliableChannel && m_reliableChannel->IsConnected())
		{
			reliableTransferActive = m_reliableChannel->IsFileTransferActive();
			if (reliableTransferActive)
			{
				this->WriteLog("⚠️ 检测到可靠传输协议仍处于活跃状态");
			}
		}

		// 【智能检测策略】如果传输明确已完成，缩短检测次数
		int maxChecks = MAX_STABILITY_CHECKS;
		if (!m_isTransmitting && !m_transmissionPaused && !reliableTransferActive)
		{
			maxChecks = 3; // 传输已完成，仅需快速验证3次
			this->WriteLog("传输已完成状态，使用快速检测模式（" + std::to_string(maxChecks) + "次）");
		}
		else
		{
			this->WriteLog("传输活跃状态，使用标准检测模式（" + std::to_string(maxChecks) + "次）");
		}

		for (int checkCount = 0; checkCount < maxChecks; checkCount++)
		{
			this->WriteLog("稳定性检测第" + std::to_string(checkCount + 1) + "/" + std::to_string(maxChecks) + "次，等待数据稳定...");

			bool readSuccess = false;
			bool lockError = false;
			std::vector<uint8_t> currentData;

			// 【保存完整性误报修复】使用不加锁版本避免死锁
			try
			{
				std::lock_guard<std::mutex> saveLock(m_receiveFileMutex);

				// 使用不加锁版本，因为外层已持锁
				currentData = ReadAllDataFromTempCacheUnlocked();
				readSuccess = true;

				this->WriteLog("当前读取数据大小: " + std::to_string(currentData.size()) + " 字节");
				this->WriteLog("文件写入统计: " + std::to_string(m_totalReceivedBytes) + " 字节");
				this->WriteLog("UI显示统计: " + std::to_string(m_bytesReceived) + " 字节");

			}
			catch (const std::system_error& e)
			{
				// 【区分异常类型处理】捕获锁相关异常
				this->WriteLog("❌ 锁操作异常: " + std::string(e.what()));
				lockError = true;
			}
			catch (const std::exception& e)
			{
				// 【区分异常类型处理】捕获其他异常
				this->WriteLog("❌ 文件读取异常: " + std::string(e.what()));
				readSuccess = false;
			}

			// 【完善错误提示逻辑】区分不同类型的错误
			if (lockError)
			{
				this->WriteLog("❌ 检测到锁冲突，终止保存操作");
				MessageBox(_T("⚠️ 临时缓存读取冲突\n\n")
						  _T("当前有其他操作正在访问接收缓存，请稍后再试。\n")
						  _T("这通常发生在数据仍在传输或处理中。"),
						  _T("保存暂时不可用"), MB_OK | MB_ICONWARNING);
				return;
			}

			if (!readSuccess)
			{
				this->WriteLog("❌ 文件读取失败，终止保存操作");
				MessageBox(_T("⚠️ 临时缓存文件读取失败\n\n")
						  _T("无法读取接收数据缓存文件，请检查：\n")
						  _T("1. 磁盘空间是否充足\n")
						  _T("2. 临时文件是否被其他程序占用\n")
						  _T("3. 稍后重新尝试保存"),
						  _T("文件读取错误"), MB_OK | MB_ICONERROR);
				return;
			}

			// 【核心稳定性检测】检查数据是否已稳定
			if (readSuccess && !currentData.empty())
			{
				if (stabilityTracker.IsDataStable(currentData, m_totalReceivedBytes, m_bytesReceived))
				{
					this->WriteLog("✅ 数据稳定性检测成功 - 数据已停止变化且满足稳定条件");
					finalCachedData = currentData;
					dataStabilized = true;
					break;
				}
				else
				{
					this->WriteLog("数据仍在变化，连续匹配次数: " + std::to_string(stabilityTracker.consecutiveMatches) + "/2");
				}
			}
			else if (readSuccess && currentData.empty())
			{
				this->WriteLog("⚠️ 读取到空数据，可能传输尚未开始或已清空");
			}

			// 等待间隔时间，让数据有机会稳定
			if (checkCount < MAX_STABILITY_CHECKS - 1)
			{
				Sleep(STABILITY_CHECK_INTERVAL_MS);
			}
		}

		// 处理稳定性检测结果
		if (!dataStabilized)
		{
			// 【优化超时处理】根据传输状态提供更准确的诊断信息
			CString warningMsg;
			CString transferStatusInfo;

			// 分析传输状态提供具体建议
			if (m_isTransmitting || m_transmissionPaused)
			{
				transferStatusInfo = _T("检测到传输仍在进行中，这是正常现象");
			}
			else if (reliableTransferActive)
			{
				transferStatusInfo = _T("可靠传输协议握手/重传仍在进行");
			}
			else if (m_totalReceivedBytes != finalCachedData.size())
			{
				transferStatusInfo = _T("文件数据与统计不一致，可能存在延迟写入");
			}
			else
			{
				transferStatusInfo = _T("传输已完成，数据检测可能过于敏感");
			}

			warningMsg.Format(_T("⚠️ 数据稳定性检测超时\n\n")
							 _T("经过%d次检测（共%.1f秒），检测状态：\n")
							 _T("• 文件实际大小: %u 字节\n")
							 _T("• 文件写入统计: %u 字节\n")
							 _T("• 传输状态: %s\n\n")
							 _T("可能原因：\n")
							 _T("• %s\n")
							 _T("• 系统I/O延迟或负载过高\n")
							 _T("• 检测参数过于严格\n\n")
							 _T("建议操作：\n")
							 _T("1. 点击'否'，等待片刻后重试\n")
							 _T("2. 点击'是'保存当前数据（通常是完整的）\n\n")
							 _T("是否要保存当前数据？"),
							 maxChecks,
							 (maxChecks * STABILITY_CHECK_INTERVAL_MS) / 1000.0,
							 (unsigned)finalCachedData.size(),
							 (unsigned)m_totalReceivedBytes,
							 transferStatusInfo,
							 transferStatusInfo);

			int result = MessageBox(warningMsg, _T("数据稳定性检测超时"), MB_YESNO | MB_ICONWARNING);
			if (result == IDNO)
			{
				this->WriteLog("用户选择等待数据稳定，取消当前保存操作");
				this->WriteLog("建议用户等待传输彻底结束后再次点击保存");
				return;
			}

			this->WriteLog("⚠️ 用户确认保存未稳定的数据快照");

			// 【最后读取】用户确认后最后一次读取最新数据
			this->WriteLog("执行最后一次数据读取以获取最新状态...");
			try
			{
				std::lock_guard<std::mutex> saveLock(m_receiveFileMutex);
				finalCachedData = ReadAllDataFromTempCacheUnlocked();
				this->WriteLog("最终快照数据大小: " + std::to_string(finalCachedData.size()) + " 字节");
			}
			catch (const std::exception& e)
			{
				this->WriteLog("❌ 最后读取失败: " + std::string(e.what()));
				MessageBox(_T("最终数据读取失败，无法保存"), _T("保存失败"), MB_OK | MB_ICONERROR);
				return;
			}
		}

		if (!finalCachedData.empty())
		{
			this->WriteLog("保存策略：使用最新读取的原始字节数据");

			// 使用最新读取的数据，而不是早期缓存
			binaryData = finalCachedData;
			isBinaryMode = true;

			if (m_checkHex.GetCheck() == BST_CHECKED)
			{
				this->WriteLog("十六进制模式：保存最新原始二进制数据");
			}
			else
			{
				this->WriteLog("文本模式：保存最新原始二进制数据");
			}
		}
	}

	// 如果临时缓存不可用，使用原有逻辑
	if (saveData.IsEmpty() && !isBinaryMode)
	{
		// 如果处于十六进制模式，从当前显示内容中提取十六进制数据并转换为二进制
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 获取当前显示内容
			CString displayContent;
			m_editReceiveData.GetWindowText(displayContent);

			if (!displayContent.IsEmpty())
			{
				// 提取纯十六进制字符（去除偏移地址、空格、ASCII部分等格式）
				CString cleanHex;
				int len = displayContent.GetLength();

				for (int i = 0; i < len; i++)
				{
					TCHAR ch = displayContent[i];
					// 只保留有效的十六进制字符
					if ((ch >= _T('0') && ch <= _T('9')) ||
						(ch >= _T('A') && ch <= _T('F')) ||
						(ch >= _T('a') && ch <= _T('f')))
					{
						cleanHex += ch;
					}
				}

				// 将十六进制字符串转换为二进制数据
				if (!cleanHex.IsEmpty() && cleanHex.GetLength() % 2 == 0)
				{
					for (int i = 0; i < cleanHex.GetLength(); i += 2)
					{
						CString hexByte = cleanHex.Mid(i, 2);
						uint8_t byte = (uint8_t)_tcstol(hexByte, nullptr, 16);
						binaryData.push_back(byte);
					}
					isBinaryMode = true;
				}
			}
		}
		else
		{
			// 文本模式：直接保存当前显示的内容
			m_editReceiveData.GetWindowText(saveData);
		}
	}

	// 根据数据类型和十六进制模式状态选择合适的文件对话框和保存方法
	if (isBinaryMode && !binaryData.empty())
	{
		this->WriteLog("准备保存原始字节数据，大小: " + std::to_string(binaryData.size()) + " 字节");

		// 统一的文件过滤器配置：默认显示所有文件，包含所有文件类型
		CString filter;
		CString defaultExt;

		// 无论是否勾选16进制模式，都使用相同的过滤器配置
		filter = _T("所有文件 (*.*)|*.*|")
				 _T("文本文件 (*.txt;*.log;*.ini;*.cfg;*.conf)|*.txt;*.log;*.ini;*.cfg;*.conf|")
				 _T("二进制文件 (*.bin;*.dat;*.exe)|*.bin;*.dat;*.exe|")
				 _T("图像文件 (*.jpg;*.png;*.bmp;*.gif;*.tiff)|*.jpg;*.png;*.bmp;*.gif;*.tiff|")
				 _T("压缩文件 (*.zip;*.rar;*.7z;*.tar;*.gz)|*.zip;*.rar;*.7z;*.tar;*.gz|")
				 _T("文档文件 (*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx)|*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx|")
				 _T("脚本文件 (*.bat;*.cmd;*.ps1;*.sh;*.py)|*.bat;*.cmd;*.ps1;*.sh;*.py|")
				 _T("源代码 (*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css)|*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css||");
		defaultExt = _T("");

		// 提供多种文件格式选择，包括原始编码的文本文件
		CFileDialog dlg(FALSE, defaultExt, _T("ReceiveData"),
						OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
						filter);

		if (dlg.DoModal() == IDOK)
		{
			SaveBinaryDataToFile(dlg.GetPathName(), binaryData);
		}
	}
	else if (!saveData.IsEmpty())
	{
		this->WriteLog("准备保存文本数据，长度: " + std::to_string(saveData.GetLength()) + " 字符");

		// 统一的文件过滤器配置：默认显示所有文件，包含所有文件类型
		CString filter;
		CString defaultExt;

		// 无论是否勾选16进制模式，都使用相同的过滤器配置
		filter = _T("所有文件 (*.*)|*.*|")
				 _T("文本文件 (*.txt;*.log;*.ini;*.cfg;*.conf)|*.txt;*.log;*.ini;*.cfg;*.conf|")
				 _T("二进制文件 (*.bin;*.dat;*.exe)|*.bin;*.dat;*.exe|")
				 _T("图像文件 (*.jpg;*.png;*.bmp;*.gif;*.tiff)|*.jpg;*.png;*.bmp;*.gif;*.tiff|")
				 _T("压缩文件 (*.zip;*.rar;*.7z;*.tar;*.gz)|*.zip;*.rar;*.7z;*.tar;*.gz|")
				 _T("文档文件 (*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx)|*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx|")
				 _T("脚本文件 (*.bat;*.cmd;*.ps1;*.sh;*.py)|*.bat;*.cmd;*.ps1;*.sh;*.py|")
				 _T("源代码 (*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css)|*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css||");
		defaultExt = _T("");

		// 文本模式：保存为文本文件
		CFileDialog dlg(FALSE, defaultExt, _T("ReceiveData"),
						OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
						filter);

		if (dlg.DoModal() == IDOK)
		{
			SaveDataToFile(dlg.GetPathName(), saveData);
		}
	}
	else
	{
		this->WriteLog("没有数据可保存");
		MessageBox(_T("没有数据可保存"), _T("提示"), MB_OK | MB_ICONWARNING);
	}

	this->WriteLog("=== 保存数据调试信息结束 ===");
}

void CPortMasterDlg::SaveDataToFile(const CString &filePath, const CString &data)
{
	this->WriteLog("=== SaveDataToFile 开始 ===");
	this->WriteLog("保存文件路径: " + std::string(CT2A(filePath)));
	this->WriteLog("输入数据长度: " + std::to_string(data.GetLength()) + " 字符");

	// 添加数据内容预览用于调试
	CString preview = data.Left(100); // 前100个字符
	this->WriteLog("数据预览: " + std::string(CT2A(preview)));

	try
	{
		// 改用更可靠的文件写入方式
		// 方法1：使用CFile二进制模式 + UTF-8编码
		CFile file;
		if (file.Open(filePath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
		{
			// 将CString转换为UTF-8字节序列
			CT2A utf8Data(data, CP_UTF8);
			int utf8Length = strlen(utf8Data);

			this->WriteLog("UTF-8转换后字节长度: " + std::to_string(utf8Length));

			// 写入UTF-8字节序列
			file.Write(utf8Data, utf8Length);
			file.Close();

			this->WriteLog("文件写入成功");

			MessageBox(_T("数据保存成功"), _T("提示"), MB_OK | MB_ICONINFORMATION);

			// 更新日志
			CTime time = CTime::GetCurrentTime();
			CString timeStr = time.Format(_T("[%H:%M:%S] "));
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			LogMessage(timeStr + _T("数据保存至: ") + fileName);
		}
		else
		{
			this->WriteLog("文件打开失败");
			MessageBox(_T("保存文件失败"), _T("错误"), MB_OK | MB_ICONERROR);
		}
	}
	catch (const std::exception &e)
	{
		this->WriteLog("保存过程异常: " + std::string(e.what()));
		MessageBox(_T("保存文件异常"), _T("错误"), MB_OK | MB_ICONERROR);
	}
	catch (...)
	{
		this->WriteLog("保存过程发生未知异常");
		MessageBox(_T("保存文件发生未知异常"), _T("错误"), MB_OK | MB_ICONERROR);
	}

	this->WriteLog("=== SaveDataToFile 结束 ===");
}

void CPortMasterDlg::SaveBinaryDataToFile(const CString &filePath, const std::vector<uint8_t> &data)
{
	this->WriteLog("=== SaveBinaryDataToFile 开始 ===");
	this->WriteLog("保存文件路径: " + std::string(CT2A(filePath)));
	this->WriteLog("原始字节数据大小: " + std::to_string(data.size()) + " 字节");

	// 添加数据内容预览
	std::string hexPreview;
	size_t previewSize = (std::min)(data.size(), (size_t)32);
	for (size_t i = 0; i < previewSize; i++)
	{
		char hexByte[4];
		sprintf_s(hexByte, "%02X ", data[i]);
		hexPreview += hexByte;
	}
	if (data.size() > 32)
	{
		hexPreview += "...";
	}
	this->WriteLog("数据预览(十六进制): " + hexPreview);

	try
	{
		CFile file;
		if (file.Open(filePath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
		{
			file.Write(data.data(), (UINT)data.size());
			file.Close();

			this->WriteLog("原始字节数据写入成功");

			// 【保存后验证机制】验证保存文件的完整性
			bool verificationPassed = VerifySavedFileSize(filePath, data.size());
			if (verificationPassed)
			{
				this->WriteLog("✅ 保存后验证通过 - 文件大小与预期一致");

				CString message;
				message.Format(_T("原始数据保存成功 (%zu 字节)\n✅ 文件完整性验证通过"), data.size());
				MessageBox(message, _T("保存成功"), MB_OK | MB_ICONINFORMATION);
			}
			else
			{
				this->WriteLog("❌ 保存后验证失败 - 文件大小与预期不符");

				CString message;
				message.Format(_T("⚠️ 保存完成但验证失败\n\n")
							  _T("预期大小: %zu 字节\n")
							  _T("文件路径: %s\n\n")
							  _T("建议检查：\n")
							  _T("1. 磁盘空间是否充足\n")
							  _T("2. 文件是否被其他程序占用\n")
							  _T("3. 重新保存验证"),
							  data.size(), filePath.GetString());
				MessageBox(message, _T("保存验证异常"), MB_OK | MB_ICONWARNING);
			}

			// 更新日志
			CTime time = CTime::GetCurrentTime();
			CString timeStr = time.Format(_T("[%H:%M:%S] "));
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			CString logMessage;
			logMessage.Format(_T("原始数据保存至: %s (%zu 字节)"), fileName.GetString(), data.size());
			LogMessage(timeStr + logMessage);
		}
		else
		{
			this->WriteLog("文件打开失败");
			MessageBox(_T("保存文件失败"), _T("错误"), MB_OK | MB_ICONERROR);
		}
	}
	catch (const std::exception &e)
	{
		this->WriteLog("保存过程异常: " + std::string(e.what()));
		MessageBox(_T("保存文件异常"), _T("错误"), MB_OK | MB_ICONERROR);
	}
	catch (...)
	{
		this->WriteLog("保存过程发生未知异常");
		MessageBox(_T("保存文件发生未知异常"), _T("错误"), MB_OK | MB_ICONERROR);
	}

	this->WriteLog("=== SaveBinaryDataToFile 结束 ===");
}

void CPortMasterDlg::OnBnClickedRadioDirect()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("直通模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);

	// 更新状态条模式信息
	m_staticMode.SetWindowText(_T("直通"));
}

// Transport层集成实现

void CPortMasterDlg::InitializeTransportConfig()
{
	// 初始化串口配置
	m_transportConfig.portName = "COM1";
	m_transportConfig.readTimeout = 1000;
	m_transportConfig.writeTimeout = 1000;
	m_transportConfig.bufferSize = 4096;

	// 初始化可靠传输配置
	m_reliableConfig.maxRetries = 3;
	m_reliableConfig.timeoutBase = 5000;	// 基础超时时间
	m_reliableConfig.maxPayloadSize = 1024; // 最大负载大小
	m_reliableConfig.windowSize = 32;		// 窗口大小 - 增加窗口大小以支持大量数据传输
}

bool CPortMasterDlg::CreateTransport()
{
	try
	{
		// 根据端口类型创建相应的传输对象
		int portType = m_comboPortType.GetCurSel();

		switch (portType)
		{
		case 0: // 串口
		{
			// 获取串口参数
			CString portName;
			m_comboPort.GetWindowText(portName);

			CString baudRateStr;
			m_comboBaudRate.GetWindowText(baudRateStr);
			int baudRate = _ttoi(baudRateStr);

			// 创建串口传输对象
			auto transport = std::make_shared<SerialTransport>();

			// 配置串口参数
			SerialConfig config;
			config.portName = CT2A(portName);
			config.baudRate = baudRate;
			config.dataBits = 8;
			config.parity = NOPARITY;
			config.stopBits = ONESTOPBIT;

			// 打开串口
			if (transport->Open(config) == TransportError::Success)
			{
				m_transport = transport;

				// 创建可靠传输通道（如果启用）
				if (m_radioReliable.GetCheck() == BST_CHECKED)
				{
					this->WriteLog("CreateTransport: Creating reliable channel...");
					m_reliableChannel = std::make_unique<ReliableChannel>();
					this->WriteLog("CreateTransport: Reliable channel created, initializing...");
					bool initResult = m_reliableChannel->Initialize(m_transport, m_reliableConfig);
					this->WriteLog("CreateTransport: Reliable channel initialization result: " + std::to_string(initResult));

					if (!initResult)
					{
						this->WriteLog("CreateTransport: ERROR - Reliable channel initialization failed");
						m_reliableChannel.reset();
						return false;
					}

					// 设置回调函数
					this->WriteLog("CreateTransport: Setting callbacks...");
					m_reliableChannel->SetProgressCallback([this](int64_t current, int64_t total)
														   { OnReliableProgress(static_cast<uint32_t>(total > 0 ? (current * 100) / total : 0)); });

					m_reliableChannel->SetStateChangedCallback([this](bool connected)
															   { OnReliableStateChanged(connected); });
					this->WriteLog("CreateTransport: Callbacks set successfully");
				}

				return true;
			}
		}
		break;

		case 1: // 并口
		{
			// 获取并口参数
			CString portName;
			m_comboPort.GetWindowText(portName);

			// 创建并口传输对象
			auto transport = std::make_shared<ParallelTransport>();

			// 配置并口参数
			ParallelPortConfig config;
			config.deviceName = CT2A(portName);
			config.readTimeout = 1000;
			config.writeTimeout = 2000;

			// 打开并口
			if (transport->Open(config) == TransportError::Success)
			{
				m_transport = transport;

				// 创建可靠传输通道（如果启用）
				if (m_radioReliable.GetCheck() == BST_CHECKED)
				{
					m_reliableChannel = std::make_unique<ReliableChannel>();
					m_reliableChannel->Initialize(m_transport, m_reliableConfig);

					// 设置回调函数
					m_reliableChannel->SetProgressCallback([this](int64_t current, int64_t total)
														   { OnReliableProgress(static_cast<uint32_t>(total > 0 ? (current * 100) / total : 0)); });

					m_reliableChannel->SetStateChangedCallback([this](bool connected)
															   { OnReliableStateChanged(connected); });
				}

				return true;
			}
		}
		break;

		case 2: // USB打印
		{
			// 获取USB打印端口参数
			CString portName;
			m_comboPort.GetWindowText(portName);

			// 创建USB打印传输对象
			auto transport = std::make_shared<UsbPrintTransport>();

			// 配置USB打印参数
			UsbPrintConfig config;
			config.deviceName = CT2A(portName);
			config.readTimeout = 1000;
			config.writeTimeout = 2000;

			// 打开USB打印端口
			if (transport->Open(config) == TransportError::Success)
			{
				m_transport = transport;

				// 创建可靠传输通道（如果启用）
				if (m_radioReliable.GetCheck() == BST_CHECKED)
				{
					m_reliableChannel = std::make_unique<ReliableChannel>();
					m_reliableChannel->Initialize(m_transport, m_reliableConfig);

					// 设置回调函数
					m_reliableChannel->SetProgressCallback([this](int64_t current, int64_t total)
														   { OnReliableProgress(static_cast<uint32_t>(total > 0 ? (current * 100) / total : 0)); });

					m_reliableChannel->SetStateChangedCallback([this](bool connected)
															   { OnReliableStateChanged(connected); });
				}

				return true;
			}
		}
		break;

		case 3: // 网络打印
		{
			// 获取网络地址参数
			CString address;
			m_comboPort.GetWindowText(address);

			// 创建网络打印传输对象
			auto transport = std::make_shared<NetworkPrintTransport>();

			// 配置网络打印参数
			NetworkPrintConfig config;
			// 解析地址和端口
			CStringA addressA(address);
			std::string addrStr(addressA);
			size_t colonPos = addrStr.find(':');
			if (colonPos != std::string::npos)
			{
				config.hostname = addrStr.substr(0, colonPos);
				config.port = static_cast<WORD>(std::stoi(addrStr.substr(colonPos + 1)));
			}
			else
			{
				config.hostname = addrStr;
				config.port = 9100; // 默认网络打印端口
			}
			config.connectTimeout = 5000;
			config.sendTimeout = 10000;
			config.receiveTimeout = 10000;

			// 打开网络连接
			if (transport->Open(config) == TransportError::Success)
			{
				m_transport = transport;

				// 创建可靠传输通道（如果启用）
				if (m_radioReliable.GetCheck() == BST_CHECKED)
				{
					m_reliableChannel = std::make_unique<ReliableChannel>();
					m_reliableChannel->Initialize(m_transport, m_reliableConfig);

					// 设置回调函数
					m_reliableChannel->SetProgressCallback([this](int64_t current, int64_t total)
														   { OnReliableProgress(static_cast<uint32_t>(total > 0 ? (current * 100) / total : 0)); });

					m_reliableChannel->SetStateChangedCallback([this](bool connected)
															   { OnReliableStateChanged(connected); });
				}

				return true;
			}
		}
		break;

		case 4: // 回路测试
		{
			// 创建回路测试传输对象
			auto transport = std::make_shared<LoopbackTransport>();

			// 配置回路测试参数
			LoopbackConfig config;
			config.delayMs = 10;
			config.errorRate = 0;
			config.packetLossRate = 0;
			config.enableJitter = false;
			config.jitterMaxMs = 5;
			config.maxQueueSize = 1000;

			// 打开回路测试
			if (transport->Open(config) == TransportError::Success)
			{
				m_transport = transport;

				// 创建可靠传输通道（如果启用）
				if (m_radioReliable.GetCheck() == BST_CHECKED)
				{
					m_reliableChannel = std::make_unique<ReliableChannel>();
					m_reliableChannel->Initialize(m_transport, m_reliableConfig);

					// 设置回调函数
					m_reliableChannel->SetProgressCallback([this](int64_t current, int64_t total)
														   { OnReliableProgress(static_cast<uint32_t>(total > 0 ? (current * 100) / total : 0)); });

					m_reliableChannel->SetStateChangedCallback([this](bool connected)
															   { OnReliableStateChanged(connected); });
				}

				return true;
			}
		}
		break;

		default:
			return false;
		}
	}
	catch (const std::exception &e)
	{
		CString errorMsg;
		errorMsg.Format(_T("创建传输对象失败: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
	}

	return false;
}

void CPortMasterDlg::DestroyTransport()
{
	// 停止接收线程
	StopReceiveThread();

	// 销毁可靠传输通道
	if (m_reliableChannel)
	{
		m_reliableChannel->Shutdown();
		m_reliableChannel.reset();
	}

	// 销毁传输对象
	if (m_transport)
	{
		m_transport->Close();
		m_transport.reset();
	}

	m_isConnected = false;
	m_isTransmitting = false;
}

void CPortMasterDlg::StartReceiveThread()
{
	if (m_receiveThread.joinable())
	{
		return;
	}

	m_receiveThread = std::thread([this]()
								  {
        std::vector<uint8_t> buffer(4096);

        while (m_isConnected)
        {
            try
            {
                if (m_reliableChannel && m_reliableChannel->IsConnected())
                {
                    // 使用可靠传输通道接收数据
                    std::vector<uint8_t> data;
                    if (m_reliableChannel->Receive(data, 100)) // 100ms超时
                    {
                        OnTransportDataReceived(data);
                    }
                }
                else if (m_transport && m_transport->IsOpen())
                {
                    // 使用原始传输接收数据
                    // 每次读取前清零缓冲区，避免旧数据残留
                    std::fill(buffer.begin(), buffer.end(), 0);

                    size_t bytesRead = 0;
                    auto result = m_transport->Read(buffer.data(), buffer.size(), &bytesRead, 100);
                    if (result == TransportError::Success && bytesRead > 0)
                    {
                        std::vector<uint8_t> data(buffer.begin(), buffer.begin() + bytesRead);
                        OnTransportDataReceived(data);
                    }
                }
            }
            catch (const std::exception& e)
            {
                OnTransportError(e.what());
            }
        } });
}

void CPortMasterDlg::StopReceiveThread()
{
	m_isConnected = false;

	if (m_receiveThread.joinable())
	{
		m_receiveThread.join();
	}
}

void CPortMasterDlg::UpdateConnectionStatus()
{
	CString statusText;
	if (m_isConnected)
	{
		statusText = _T("已连接");
	}
	else
	{
		statusText = _T("未连接");
	}

	m_staticPortStatus.SetWindowText(statusText);

	// 【可靠模式按钮管控】根据缺陷分析报告实施按钮状态控制
	UpdateSaveButtonStatus();
}

// 【可靠模式按钮管控】更新保存按钮状态的专用函数
void CPortMasterDlg::UpdateSaveButtonStatus()
{
	bool enableSaveButton = true; // 默认启用保存按钮

	// 检查是否处于可靠模式
	bool isReliableMode = (m_radioReliable.GetCheck() == BST_CHECKED);

	if (isReliableMode && m_reliableChannel && m_isConnected)
	{
		// 在可靠模式下，只有文件传输彻底完成才允许保存
		bool fileTransferActive = m_reliableChannel->IsFileTransferActive();

		if (fileTransferActive)
		{
			enableSaveButton = false;
			this->WriteLog("可靠模式：文件传输进行中，禁用保存按钮");
		}
		else
		{
			this->WriteLog("可靠模式：文件传输已完成，启用保存按钮");
		}
	}
	else if (!isReliableMode)
	{
		// 直通模式：保持原有逻辑，正常启用保存按钮
		this->WriteLog("直通模式：正常启用保存按钮");
	}

	// 更新保存按钮状态
	m_btnSaveAll.EnableWindow(enableSaveButton ? TRUE : FALSE);
}

void CPortMasterDlg::UpdateStatistics()
{
	// 更新速度显示
	CString speedText;
	speedText.Format(_T("%uKB/s"), (m_sendSpeed + m_receiveSpeed) / 1024);
	m_staticSpeed.SetWindowText(speedText);
}

void CPortMasterDlg::OnTransportDataReceived(const std::vector<uint8_t> &data)
{
	// 【深层修复】根据最新检查报告最优解方案：接收线程直接落盘，避免消息队列异步延迟
	this->WriteLog("=== OnTransportDataReceived 开始（直接落盘模式）===");
	this->WriteLog("接收到数据大小: " + std::to_string(data.size()) + " 字节");

	// 记录前64字节的十六进制内容用于调试
	std::string hexPreview;
	size_t previewSize = (std::min)(data.size(), (size_t)64);
	for (size_t i = 0; i < previewSize; i++)
	{
		char hexByte[4];
		sprintf_s(hexByte, "%02X ", data[i]);
		hexPreview += hexByte;
	}
	if (data.size() > 64)
	{
		hexPreview += "...";
	}
	this->WriteLog("接收数据预览(十六进制): " + hexPreview);

	// 记录前64字节的ASCII内容用于调试
	std::string asciiPreview;
	for (size_t i = 0; i < previewSize; i++)
	{
		uint8_t byte = data[i];
		if (byte >= 32 && byte <= 126)
		{
			asciiPreview += (char)byte;
		}
		else
		{
			asciiPreview += '.';
		}
	}
	if (data.size() > 64)
	{
		asciiPreview += "...";
	}
	this->WriteLog("接收数据预览(ASCII): " + asciiPreview);

	// 【关键修复】直接调用线程安全落盘函数，立即写入缓存和文件
	ThreadSafeAppendReceiveData(data);

	// 【轻量化UI更新】仅发送统计信息，不传输实际数据，避免消息队列堆积
	struct UIUpdateInfo {
		size_t dataSize;
		std::string hexPreview;
		std::string asciiPreview;
	};
	UIUpdateInfo* updateInfo = new UIUpdateInfo{data.size(), hexPreview, asciiPreview};
	PostMessage(WM_USER + 1, 0, (LPARAM)updateInfo);

	this->WriteLog("=== OnTransportDataReceived 结束（数据已直接落盘）===");
}

void CPortMasterDlg::OnTransportError(const std::string &error)
{
	// 在主线程中显示错误
	CString errorMsg(error.c_str());
	PostMessage(WM_USER + 2, 0, (LPARAM) new CString(errorMsg));
}

void CPortMasterDlg::OnReliableProgress(uint32_t progress)
{
	// 更新进度条
	m_progress.SetPos(progress);
}

void CPortMasterDlg::OnReliableComplete(bool success)
{
	m_isTransmitting = false;

	if (success)
	{
		// 传输完成 - 显示传输完成消息
		CString completeStatus;
		completeStatus.Format(_T("传输完成"));
		m_staticPortStatus.SetWindowText(completeStatus);
		m_progress.SetPos(100);

		// 2秒后恢复连接状态显示，但保持进度条100%状态
		SetTimer(TIMER_ID_CONNECTION_STATUS, 2000, NULL);
	}
	else
	{
		// 传输失败 - 显示错误消息
		CString errorStatus;
		errorStatus.Format(_T("传输失败"));
		m_staticPortStatus.SetWindowText(errorStatus);
		m_progress.SetPos(0);

		MessageBox(_T("传输失败"), _T("错误"), MB_OK | MB_ICONERROR);
	}

	// 【可靠模式按钮管控】传输完成后立即更新保存按钮状态
	// 确保在文件传输结束后及时启用保存按钮
	UpdateSaveButtonStatus();
}

void CPortMasterDlg::OnReliableStateChanged(bool connected)
{
	// 连接状态变化回调 - 只处理连接状态，不显示传输完成消息
	if (connected)
	{
		// 连接成功 - 更新状态显示
		CString statusText = _T("已连接");
		m_staticPortStatus.SetWindowText(statusText);
	}
	else
	{
		// 连接断开 - 更新状态显示
		CString statusText = _T("连接断开");
		m_staticPortStatus.SetWindowText(statusText);
		m_progress.SetPos(0);
	}
}

// 自定义消息处理实现

LRESULT CPortMasterDlg::OnTransportDataReceivedMessage(WPARAM wParam, LPARAM lParam)
{
	// 【深层修复】处理新的轻量级UI更新信息结构
	UIUpdateInfo *updateInfo = reinterpret_cast<UIUpdateInfo *>(lParam);
	if (updateInfo)
	{
		try
		{
			// 轻量级UI更新：仅处理统计信息和显示预览
			this->WriteLog("=== 轻量级UI更新信息 ===");
			this->WriteLog("接收数据大小: " + std::to_string(updateInfo->dataSize) + " 字节");
			this->WriteLog("十六进制预览: " + updateInfo->hexPreview);
			this->WriteLog("ASCII预览: " + updateInfo->asciiPreview);

			// 更新UI显示的接收统计（与直接落盘的m_totalReceivedBytes区分）
			m_bytesReceived += updateInfo->dataSize;
			CString receivedText;
			receivedText.Format(_T("%u"), m_bytesReceived);
			SetDlgItemText(IDC_STATIC_RECEIVED, receivedText);

			// 【UI优化】使用节流机制更新显示，避免大文件传输时频繁重绘
			// 注意：数据已经在ThreadSafeAppendReceiveData中直接落盘和更新内存缓存
			ThrottledUpdateReceiveDisplay();

			this->WriteLog("=== 轻量级UI更新完成 ===");
		}
		catch (const std::exception &e)
		{
			CString errorMsg;
			errorMsg.Format(_T("处理轻量级UI更新失败: %s"), CString(e.what()));
			MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
		}

		delete updateInfo;
	}

	return 0;
}

LRESULT CPortMasterDlg::OnTransportErrorMessage(WPARAM wParam, LPARAM lParam)
{
	CString *errorMsg = reinterpret_cast<CString *>(lParam);
	if (errorMsg)
	{
		MessageBox(*errorMsg, _T("传输错误"), MB_OK | MB_ICONERROR);
		delete errorMsg;
	}

	return 0;
}

// 新增：传输进度条范围设置消息处理
LRESULT CPortMasterDlg::OnTransmissionProgressRange(WPARAM wParam, LPARAM lParam)
{
	m_progress.SetRange(0, (int)lParam);
	return 0;
}

// 新增：传输进度条更新消息处理
LRESULT CPortMasterDlg::OnTransmissionProgressUpdate(WPARAM wParam, LPARAM lParam)
{
	m_progress.SetPos((int)lParam);
	return 0;
}

// 新增：传输状态更新消息处理
LRESULT CPortMasterDlg::OnTransmissionStatusUpdate(WPARAM wParam, LPARAM lParam)
{
	CString *statusText = reinterpret_cast<CString *>(lParam);
	if (statusText)
	{
		m_staticPortStatus.SetWindowText(*statusText);
		delete statusText;
	}
	return 0;
}

// 新增：传输完成消息处理
LRESULT CPortMasterDlg::OnTransmissionComplete(WPARAM wParam, LPARAM lParam)
{
	TransportError error = static_cast<TransportError>(wParam);

	if (m_transmissionCancelled)
	{
		// 传输被取消
		m_isTransmitting = false;
		m_transmissionPaused = false;
		m_btnSend.SetWindowText(_T("发送"));
		m_staticPortStatus.SetWindowText(_T("传输已终止"));
		m_progress.SetPos(0);
	}
	else if (error != TransportError::Success)
	{
		// 传输失败
		m_isTransmitting = false;
		m_transmissionPaused = false;
		m_btnSend.SetWindowText(_T("发送"));

		CString errorMsg;
		CString errorTitle;

		// 根据错误类型提供用户友好的错误提示
		switch (error)
		{
		case TransportError::Busy:
			errorMsg = _T("传输队列繁忙，大文件传输时可能出现此问题。\n\n建议解决方案：\n1. 稍后重试\n2. 尝试将文件分割成较小的块\n3. 使用可靠传输模式");
			errorTitle = _T("传输队列繁忙");
			break;
		case TransportError::Timeout:
			errorMsg = _T("传输超时，请检查连接状态或增加超时时间");
			errorTitle = _T("传输超时");
			break;
		case TransportError::NotOpen:
			errorMsg = _T("传输通道未打开，请先建立连接");
			errorTitle = _T("连接错误");
			break;
		case TransportError::WriteFailed:
			errorMsg = _T("数据写入失败，请检查目标设备状态");
			errorTitle = _T("写入错误");
			break;
		default:
			errorMsg.Format(_T("发送失败，错误代码: %d\n\n请查看调试日志获取详细信息"), static_cast<int>(error));
			errorTitle = _T("传输错误");
			break;
		}

		MessageBox(errorMsg, errorTitle, MB_OK | MB_ICONERROR);

		// 重置进度条
		m_progress.SetPos(0);

		// 恢复连接状态显示
		UpdateConnectionStatus();
	}
	else
	{
		// 传输成功
		m_isTransmitting = false;
		m_transmissionPaused = false;
		m_btnSend.SetWindowText(_T("发送"));

		// 更新发送统计
		const std::vector<uint8_t> &data = m_sendDataCache;
		m_bytesSent += data.size();
		CString sentText;
		sentText.Format(_T("%u"), m_bytesSent);
		SetDlgItemText(IDC_STATIC_SENT, sentText);

		// 显示传输完成状态并设置进度条为100%
		CString completeStatus;
		completeStatus.Format(_T("传输完成: %u 字节"), data.size());
		m_staticPortStatus.SetWindowText(completeStatus);
		m_progress.SetPos(100);

		// 添加传输完成提示对话框
		CString successMsg;
		successMsg.Format(_T("文件传输完成！\n\n已成功发送 %u 字节数据"), data.size());
		MessageBox(successMsg, _T("传输完成"), MB_OK | MB_ICONINFORMATION);

		// 【UI体验修复】发送成功后保持缓存有效，允许用户重新发送相同内容
		// 不清空缓存，保持与当前发送窗口内容的一致性，支持重复发送

		this->WriteLog("发送完成，缓存保持有效以支持重复发送");

		// 2秒后恢复连接状态显示并重置进度条
		SetTimer(TIMER_ID_CONNECTION_STATUS, 2000, NULL);
	}

	return 0;
}

// 新增：传输异常消息处理
LRESULT CPortMasterDlg::OnTransmissionError(WPARAM wParam, LPARAM lParam)
{
	m_isTransmitting = false;
	m_transmissionPaused = false;
	m_btnSend.SetWindowText(_T("发送"));

	MessageBox(_T("传输过程中发生异常"), _T("错误"), MB_OK | MB_ICONERROR);
	return 0;
}

// 配置管理方法实现
void CPortMasterDlg::LoadConfigurationFromStore()
{
	try
	{
		const PortMasterConfig &config = m_configStore.GetConfig();

		// 加载应用程序配置
		if (config.app.enableLogging)
		{
			// 设置日志级别等
		}

		// 加载串口配置
		const SerialConfig &serialConfig = config.serial;

		// 设置端口名
		SetDlgItemText(IDC_COMBO_PORT, CString(serialConfig.portName.c_str()));

		// 设置波特率
		CString baudRateText;
		baudRateText.Format(_T("%d"), serialConfig.baudRate);
		SetDlgItemText(IDC_COMBO_BAUD_RATE, baudRateText);

		// 设置数据位
		CString dataBitsText;
		dataBitsText.Format(_T("%d"), serialConfig.dataBits);
		SetDlgItemText(IDC_COMBO_DATA_BITS, dataBitsText);

		// 设置校验位
		CString parityText;
		switch (serialConfig.parity)
		{
		case NOPARITY:
			parityText = _T("None");
			break;
		case ODDPARITY:
			parityText = _T("Odd");
			break;
		case EVENPARITY:
			parityText = _T("Even");
			break;
		default:
			parityText = _T("None");
			break;
		}
		SetDlgItemText(IDC_COMBO_PARITY, parityText);

		// 设置停止位
		CString stopBitsText;
		switch (serialConfig.stopBits)
		{
		case ONESTOPBIT:
			stopBitsText = _T("1");
			break;
		case ONE5STOPBITS:
			stopBitsText = _T("1.5");
			break;
		case TWOSTOPBITS:
			stopBitsText = _T("2");
			break;
		default:
			stopBitsText = _T("1");
			break;
		}
		SetDlgItemText(IDC_COMBO_STOP_BITS, stopBitsText);

		// 设置流控制
		CString flowControlText = (serialConfig.flowControl == 0) ? _T("None") : _T("Hardware");
		SetDlgItemText(IDC_COMBO_FLOW_CONTROL, flowControlText);

		// 设置超时
		CString timeoutText;
		timeoutText.Format(_T("%d"), serialConfig.readTimeout);
		SetDlgItemText(IDC_EDIT_TIMEOUT, timeoutText);

		// 设置传输模式 - 始终默认选择直通模式，不受配置文件影响
		m_radioReliable.SetCheck(BST_UNCHECKED);
		m_radioDirect.SetCheck(BST_CHECKED);

		// 更新状态栏显示为直通模式
		SetDlgItemText(IDC_STATIC_MODE, _T("直通"));

		// 加载UI配置
		const UIConfig &uiConfig = config.ui;

		// 设置十六进制显示选项
		m_checkHex.SetCheck(uiConfig.hexDisplay ? BST_CHECKED : BST_UNCHECKED);

		// 更新窗口位置（如果有保存）
		if (uiConfig.windowWidth > 0 && uiConfig.windowHeight > 0 &&
			uiConfig.windowX != CW_USEDEFAULT && uiConfig.windowY != CW_USEDEFAULT)
		{
			SetWindowPos(nullptr,
						 uiConfig.windowX, uiConfig.windowY,
						 uiConfig.windowWidth, uiConfig.windowHeight,
						 SWP_NOZORDER);
		}
	}
	catch (const std::exception &e)
	{
		CString errorMsg;
		errorMsg.Format(_T("加载配置失败: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("配置错误"), MB_OK | MB_ICONWARNING);
	}
}

void CPortMasterDlg::SaveConfigurationToStore()
{
	try
	{
		PortMasterConfig config = m_configStore.GetConfig();

		// 保存串口配置
		SerialConfig &serialConfig = config.serial;

		// 获取端口名
		CString portName;
		GetDlgItemText(IDC_COMBO_PORT, portName);
		serialConfig.portName = CStringA(portName).GetString();

		// 获取波特率
		CString baudRateText;
		GetDlgItemText(IDC_COMBO_BAUD_RATE, baudRateText);
		serialConfig.baudRate = _ttoi(baudRateText);

		// 获取数据位
		CString dataBitsText;
		GetDlgItemText(IDC_COMBO_DATA_BITS, dataBitsText);
		serialConfig.dataBits = (BYTE)_ttoi(dataBitsText);

		// 获取校验位
		CString parityText;
		GetDlgItemText(IDC_COMBO_PARITY, parityText);
		if (parityText == _T("None"))
			serialConfig.parity = NOPARITY;
		else if (parityText == _T("Odd"))
			serialConfig.parity = ODDPARITY;
		else if (parityText == _T("Even"))
			serialConfig.parity = EVENPARITY;
		else
			serialConfig.parity = NOPARITY;

		// 获取停止位
		CString stopBitsText;
		GetDlgItemText(IDC_COMBO_STOP_BITS, stopBitsText);
		if (stopBitsText == _T("1"))
			serialConfig.stopBits = ONESTOPBIT;
		else if (stopBitsText == _T("1.5"))
			serialConfig.stopBits = ONE5STOPBITS;
		else if (stopBitsText == _T("2"))
			serialConfig.stopBits = TWOSTOPBITS;
		else
			serialConfig.stopBits = ONESTOPBIT;

		// 获取流控制
		CString flowControlText;
		GetDlgItemText(IDC_COMBO_FLOW_CONTROL, flowControlText);
		serialConfig.flowControl = (flowControlText == _T("None")) ? 0 : 1;

		// 获取超时
		CString timeoutText;
		GetDlgItemText(IDC_EDIT_TIMEOUT, timeoutText);
		serialConfig.readTimeout = _ttoi(timeoutText);
		serialConfig.writeTimeout = serialConfig.readTimeout;

		// 获取可靠模式（更新协议配置）
		if (m_radioReliable.GetCheck() == BST_CHECKED)
		{
			config.protocol.windowSize = 4; // 启用可靠模式
			config.protocol.maxRetries = 3;
		}
		else
		{
			config.protocol.windowSize = 1; // 禁用可靠模式
			config.protocol.maxRetries = 0;
		}

		// 保存UI配置
		UIConfig &uiConfig = config.ui;

		// 保存十六进制显示选项
		uiConfig.hexDisplay = (m_checkHex.GetCheck() == BST_CHECKED);

		// 保存窗口位置
		CRect windowRect;
		GetWindowRect(&windowRect);
		uiConfig.windowX = windowRect.left;
		uiConfig.windowY = windowRect.top;
		uiConfig.windowWidth = windowRect.Width();
		uiConfig.windowHeight = windowRect.Height();

		// 更新配置到存储
		m_configStore.SetConfig(config);

		// 触发配置变更回调
		OnConfigurationChanged();
	}
	catch (const std::exception &e)
	{
		CString errorMsg;
		errorMsg.Format(_T("保存配置失败: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("配置错误"), MB_OK | MB_ICONERROR);
	}
}

void CPortMasterDlg::OnConfigurationChanged()
{
	// 配置变更时的处理逻辑
	// 可以在这里通知其他组件配置已更改
	// 例如：重新应用Transport配置、更新UI状态等

	try
	{
		// 更新传输配置
		InitializeTransportConfig();

		// 如果当前已连接，可能需要重新连接以应用新配置
		if (m_isConnected.load())
		{
			// 这里可以添加重新连接的逻辑（可选）
			// 为了安全起见，通常建议用户手动重新连接
		}
	}
	catch (const std::exception &e)
	{
		CString errorMsg;
		errorMsg.Format(_T("应用配置变更失败: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("配置错误"), MB_OK | MB_ICONWARNING);
	}
}

// 临时缓存文件管理方法实现
bool CPortMasterDlg::InitializeTempCacheFile()
{
	try
	{
		// 生成临时文件名
		TCHAR tempPath[MAX_PATH];
		TCHAR tempFileName[MAX_PATH];

		if (GetTempPath(MAX_PATH, tempPath) == 0)
		{
			return false;
		}

		if (GetTempFileName(tempPath, _T("PM_"), 0, tempFileName) == 0)
		{
			return false;
		}

		m_tempCacheFilePath = tempFileName;

		// 打开临时文件用于写入（二进制模式）
		m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!m_tempCacheFile.is_open())
		{
			return false;
		}

		m_useTempCacheFile = true;
		m_totalReceivedBytes = 0;
		m_totalSentBytes = 0;

		WriteLog("临时缓存文件已创建: " + std::string(CStringA(m_tempCacheFilePath)));
		return true;
	}
	catch (const std::exception &e)
	{
		WriteLog("创建临时缓存文件失败: " + std::string(e.what()));
		return false;
	}
}

void CPortMasterDlg::CloseTempCacheFile()
{
	if (m_tempCacheFile.is_open())
	{
		m_tempCacheFile.close();
	}

	// 删除临时文件
	if (!m_tempCacheFilePath.IsEmpty() && PathFileExists(m_tempCacheFilePath))
	{
		DeleteFile(m_tempCacheFilePath);
		WriteLog("临时缓存文件已删除");
	}

	m_tempCacheFilePath.Empty();
	m_useTempCacheFile = false;
	m_totalReceivedBytes = 0;
	m_totalSentBytes = 0;
}

bool CPortMasterDlg::WriteDataToTempCache(const std::vector<uint8_t> &data)
{
	if (data.empty())
	{
		return false;
	}

	// 【深层修复】使用接收文件专用互斥锁，确保与接收线程和保存操作完全互斥
	std::lock_guard<std::mutex> lock(m_receiveFileMutex);

	// 如果文件流被关闭（如正在读取），将数据加入待写入队列
	if (!m_tempCacheFile.is_open())
	{
		m_pendingWrites.push(data);
		WriteLog("临时缓存文件流已关闭，数据加入待写入队列，大小: " + std::to_string(data.size()) + " 字节");
		return true; // 返回成功，数据将在文件重新打开时写入
	}

	try
	{
		// 先处理队列中的待写入数据
		while (!m_pendingWrites.empty())
		{
			const auto& pendingData = m_pendingWrites.front();
			m_tempCacheFile.write(reinterpret_cast<const char*>(pendingData.data()), pendingData.size());
			if (m_tempCacheFile.fail())
			{
				WriteLog("写入待处理数据失败，队列大小: " + std::to_string(m_pendingWrites.size()));
				return false;
			}
			m_totalReceivedBytes += pendingData.size();
			m_pendingWrites.pop();
		}

		// 写入当前数据
		m_tempCacheFile.write(reinterpret_cast<const char*>(data.data()), data.size());
		if (m_tempCacheFile.fail())
		{
			WriteLog("写入当前数据到临时缓存文件失败，大小: " + std::to_string(data.size()) + " 字节");
			return false;
		}

		m_tempCacheFile.flush(); // 确保数据立即写入磁盘
		m_totalReceivedBytes += data.size();

		WriteLog("成功写入临时缓存文件，大小: " + std::to_string(data.size()) + " 字节，总计: " + std::to_string(m_totalReceivedBytes) + " 字节");
		return true;
	}
	catch (const std::exception &e)
	{
		WriteLog("写入临时缓存文件异常: " + std::string(e.what()));
		return false;
	}
}

std::vector<uint8_t> CPortMasterDlg::ReadDataFromTempCache(uint64_t offset, size_t length)
{
	std::vector<uint8_t> result;

	if (m_tempCacheFilePath.IsEmpty() || !PathFileExists(m_tempCacheFilePath))
	{
		return result;
	}

	try
	{
		// 【深层修复】使用接收文件专用互斥锁，与接收线程直接落盘完全互斥
		std::lock_guard<std::mutex> lock(m_receiveFileMutex);

		// 安全关闭写入流并确保数据完整性
		bool needReopenWrite = false;
		if (m_tempCacheFile.is_open())
		{
			m_tempCacheFile.flush(); // 确保所有数据写入磁盘
			m_tempCacheFile.close(); // 关闭写入流避免冲突
			needReopenWrite = true;
			WriteLog("ReadDataFromTempCache: 安全关闭写入流，待写入队列大小: " + std::to_string(m_pendingWrites.size()));
		}

		std::ifstream file(m_tempCacheFilePath, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			WriteLog("ReadDataFromTempCache: 无法打开临时缓存文件进行读取");
			return result;
		}

		// 检查文件大小
		file.seekg(0, std::ios::end);
		std::streamsize fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		WriteLog("ReadDataFromTempCache: 临时缓存文件大小 " + std::to_string(fileSize) + " 字节");

		// 验证偏移量
		if (offset >= static_cast<uint64_t>(fileSize))
		{
			WriteLog("ReadDataFromTempCache: 偏移量超出文件大小");
			file.close();
			// 重新打开写入流
			if (needReopenWrite)
			{
				m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::app);
			}
			return result;
		}

		// 计算实际可读取的长度
		size_t availableLength = static_cast<size_t>(fileSize - offset);
		size_t readLength = (length == 0 || length > availableLength) ? availableLength : length;
		WriteLog("ReadDataFromTempCache: 计划读取 " + std::to_string(readLength) + " 字节，从偏移量 " + std::to_string(offset));

		if (readLength > 0)
		{
			result.resize(readLength);
			file.seekg(offset);
			file.read(reinterpret_cast<char *>(result.data()), readLength);

			if (file.fail())
			{
				WriteLog("ReadDataFromTempCache: 文件读取失败");
				result.clear();
			}
			else
			{
				WriteLog("ReadDataFromTempCache: 成功读取 " + std::to_string(result.size()) + " 字节");
			}
		}

		file.close();

		// 【并发安全修复】重新打开写入流并处理待写入队列
		if (needReopenWrite)
		{
			m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::app);
			if (m_tempCacheFile.is_open())
			{
				WriteLog("ReadDataFromTempCache: 重新打开写入流成功");

				// 处理在读取期间积累的待写入数据
				size_t processedCount = 0;
				while (!m_pendingWrites.empty())
				{
					const auto& pendingData = m_pendingWrites.front();
					m_tempCacheFile.write(reinterpret_cast<const char*>(pendingData.data()), pendingData.size());
					if (m_tempCacheFile.fail())
					{
						WriteLog("处理待写入队列失败，剩余 " + std::to_string(m_pendingWrites.size()) + " 项");
						break;
					}
					m_totalReceivedBytes += pendingData.size();
					m_pendingWrites.pop();
					processedCount++;
				}

				if (processedCount > 0)
				{
					m_tempCacheFile.flush();
					WriteLog("已处理 " + std::to_string(processedCount) + " 个待写入数据项");
				}
			}
			else
			{
				WriteLog("ReadDataFromTempCache: 重新打开写入流失败，待写入队列大小: " + std::to_string(m_pendingWrites.size()));
			}
		}

		return result;
	}
	catch (const std::exception &e)
	{
		WriteLog("从临时缓存文件读取数据异常: " + std::string(e.what()));
		return result;
	}
}

std::vector<uint8_t> CPortMasterDlg::ReadAllDataFromTempCache()
{
	return ReadDataFromTempCache(0, 0); // 0长度表示读取全部数据
}

// 【保存完整性误报修复】不加锁的读取版本，避免与外层锁冲突
std::vector<uint8_t> CPortMasterDlg::ReadDataFromTempCacheUnlocked(uint64_t offset, size_t length)
{
	std::vector<uint8_t> result;

	if (m_tempCacheFilePath.IsEmpty() || !PathFileExists(m_tempCacheFilePath))
	{
		return result;
	}

	try
	{
		// 【关键修复】不加锁版本，假设调用方已持锁

		// 安全关闭写入流并确保数据完整性
		bool needReopenWrite = false;
		if (m_tempCacheFile.is_open())
		{
			m_tempCacheFile.flush(); // 确保所有数据写入磁盘
			m_tempCacheFile.close(); // 关闭写入流避免冲突
			needReopenWrite = true;
			WriteLog("ReadDataFromTempCacheUnlocked: 安全关闭写入流，待写入队列大小: " + std::to_string(m_pendingWrites.size()));
		}

		std::ifstream file(m_tempCacheFilePath, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			WriteLog("ReadDataFromTempCacheUnlocked: 无法打开临时缓存文件进行读取");
			return result;
		}

		// 检查文件大小
		file.seekg(0, std::ios::end);
		std::streamsize fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		WriteLog("ReadDataFromTempCacheUnlocked: 临时缓存文件大小 " + std::to_string(fileSize) + " 字节");

		// 验证偏移量
		if (offset >= static_cast<uint64_t>(fileSize))
		{
			WriteLog("ReadDataFromTempCacheUnlocked: 偏移量超出文件大小");
			file.close();
			// 重新打开写入流
			if (needReopenWrite)
			{
				m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::app);
			}
			return result;
		}

		// 计算实际可读取的长度
		size_t availableLength = static_cast<size_t>(fileSize - offset);
		size_t readLength = (length == 0 || length > availableLength) ? availableLength : length;
		WriteLog("ReadDataFromTempCacheUnlocked: 计划读取 " + std::to_string(readLength) + " 字节，从偏移量 " + std::to_string(offset));

		if (readLength > 0)
		{
			result.resize(readLength);
			file.seekg(offset);
			file.read(reinterpret_cast<char *>(result.data()), readLength);

			if (file.fail())
			{
				WriteLog("ReadDataFromTempCacheUnlocked: 文件读取失败");
				result.clear();
			}
			else
			{
				WriteLog("ReadDataFromTempCacheUnlocked: 成功读取 " + std::to_string(result.size()) + " 字节");
			}
		}

		file.close();

		// 【并发安全修复】重新打开写入流并处理待写入队列
		if (needReopenWrite)
		{
			m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::app);
			if (m_tempCacheFile.is_open())
			{
				WriteLog("ReadDataFromTempCacheUnlocked: 重新打开写入流成功");

				// 处理在读取期间积累的待写入数据
				size_t processedCount = 0;
				while (!m_pendingWrites.empty())
				{
					const auto& pendingData = m_pendingWrites.front();
					m_tempCacheFile.write(reinterpret_cast<const char*>(pendingData.data()), pendingData.size());
					if (m_tempCacheFile.fail())
					{
						WriteLog("处理待写入队列失败，剩余 " + std::to_string(m_pendingWrites.size()) + " 项");
						break;
					}
					m_totalReceivedBytes += pendingData.size();
					m_pendingWrites.pop();
					processedCount++;
				}

				if (processedCount > 0)
				{
					m_tempCacheFile.flush();
					WriteLog("已处理 " + std::to_string(processedCount) + " 个待写入数据项");
				}
			}
			else
			{
				WriteLog("ReadDataFromTempCacheUnlocked: 重新打开写入流失败，待写入队列大小: " + std::to_string(m_pendingWrites.size()));
			}
		}

		return result;
	}
	catch (const std::exception &e)
	{
		WriteLog("从临时缓存文件读取数据异常(Unlocked): " + std::string(e.what()));
		return result;
	}
}

std::vector<uint8_t> CPortMasterDlg::ReadAllDataFromTempCacheUnlocked()
{
	return ReadDataFromTempCacheUnlocked(0, 0); // 0长度表示读取全部数据
}

// 【数据稳定性检测】检查数据是否稳定
bool CPortMasterDlg::StabilityTracker::IsDataStable(const std::vector<uint8_t>& currentData,
                                                   uint64_t currentTotal, uint64_t currentBytes)
{
	// 【修复误报】统一数据源检测策略：
	// 仅基于实际文件大小(currentData.size())和文件统计(currentTotal)进行检测
	// 移除对UI统计(currentBytes)的依赖，避免异步更新导致的误报

	// 检查文件数据是否发生变化
	bool fileDataChanged = (currentData.size() != lastReadData.size()) ||
	                       (currentTotal != lastTotalReceived);

	// 【调试日志】记录检测状态，便于问题排查
	// 注意：仅在Debug版本中启用详细日志，避免Release版本的性能影响
	#ifdef _DEBUG
	static int debugCheckCount = 0;
	debugCheckCount++;
	if (debugCheckCount <= 10) { // 仅记录前10次检测，避免日志过多
		// 这里可以添加调试输出（在实际项目中可以通过WriteLog）
		// 但为了保持函数的轻量性，暂时注释掉详细日志
	}
	#endif

	if (fileDataChanged) {
		// 文件数据发生变化，重置计数器和时间
		lastReadData = currentData;
		lastTotalReceived = currentTotal;
		lastBytesReceived = currentBytes; // 保留用于记录，但不用于检测
		lastChangeTime = std::chrono::steady_clock::now();
		consecutiveMatches = 0;
		return false;
	} else {
		// 文件数据未变化，增加连续匹配计数
		consecutiveMatches++;
	}

	// 【优化】检查稳定性条件：
	// 1. 连续至少2次相同读取（确保数据确实稳定）
	// 2. 距离最后变化超过300ms（从500ms缩短，提高响应速度）
	auto quietDuration = std::chrono::steady_clock::now() - lastChangeTime;
	bool timeStable = (quietDuration > std::chrono::milliseconds(300));
	bool countStable = (consecutiveMatches >= 2);

	return timeStable && countStable;
}

// 【数据稳定性检测】保存后验证文件大小
bool CPortMasterDlg::VerifySavedFileSize(const CString& filePath, size_t expectedSize)
{
	HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ,
	                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		this->WriteLog("❌ 保存验证失败：无法打开文件 " + std::string(CT2A(filePath)));
		return false;
	}

	LARGE_INTEGER fileSize;
	bool success = false;
	if (GetFileSizeEx(hFile, &fileSize)) {
		size_t actualSize = static_cast<size_t>(fileSize.QuadPart);

		if (actualSize == expectedSize) {
			this->WriteLog("✅ 保存验证成功：" + std::to_string(actualSize) + " 字节");
			success = true;
		} else {
			this->WriteLog("❌ 保存验证失败：期望 " + std::to_string(expectedSize) +
			              " 字节，实际 " + std::to_string(actualSize) + " 字节");
			success = false;
		}
	} else {
		this->WriteLog("❌ 保存验证失败：无法获取文件大小");
		success = false;
	}

	CloseHandle(hFile);
	return success;
}

void CPortMasterDlg::ClearTempCacheFile()
{
	if (m_tempCacheFile.is_open())
	{
		m_tempCacheFile.close();
		m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (m_tempCacheFile.is_open())
		{
			m_totalReceivedBytes = 0;
			WriteLog("临时缓存文件已清空");
		}
	}
}

// 文件拖拽处理函数
void CPortMasterDlg::OnDropFiles(HDROP hDropInfo)
{
	// 获取拖拽的文件数量
	UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);

	if (fileCount > 0)
	{
		// 只处理第一个文件
		TCHAR filePath[MAX_PATH];
		if (DragQueryFile(hDropInfo, 0, filePath, MAX_PATH) > 0)
		{
			// 检查文件扩展名
			CString fileName(filePath);
			fileName.MakeLower();

			// 支持常见的文本文件格式
			if (fileName.Right(4) == _T(".txt") ||
				fileName.Right(4) == _T(".log") ||
				fileName.Right(5) == _T(".data") ||
				fileName.Right(3) == _T(".in") ||
				fileName.Right(4) == _T(".out") ||
				fileName.Right(4) == _T(".hex") ||
				fileName.Right(4) == _T(".bin"))
			{
				// 加载文件内容到发送窗口
				LoadDataFromFile(filePath);

				// 更新状态栏提示
				CString statusMsg;
				statusMsg.Format(_T("已加载文件: %s"), PathFindFileName(filePath));
				LogMessage(statusMsg);
			}
			else
			{
				MessageBox(_T("不支持该文件格式，请拖拽文本文件或二进制文件"), _T("提示"), MB_OK | MB_ICONWARNING);
			}
		}
	}

	// 释放拖拽信息结构
	DragFinish(hDropInfo);

	// 调用基类处理
	CDialogEx::OnDropFiles(hDropInfo);
}