﻿// PortMasterDlg.cpp : 实现文件
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

	// 【修复】不在构造函数中初始化管理器，移到OnInitDialog中
	// 这确保了在控件创建完成后再初始化管理器
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
ON_MESSAGE(WM_USER + 15, &CPortMasterDlg::OnCleanupTransmissionTask)
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

	// 【修复】在控件创建完成后初始化管理器
	// 这确保了所有控件都已创建并可用
	InitializeManagersAfterControlsCreated();

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

		// 【UI响应修复】清除传输状态，让连接状态显示
		if (m_uiStateManager) {
			m_uiStateManager->ClearStatus(UIStateManager::StatusType::Transmission);
			UpdateConnectionStatus(); // 更新连接状态
		}

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

	// 【传输状态管理统一】清理传输状态管理器
	if (m_transmissionStateManager) {
		m_transmissionStateManager->ClearCallbacks();
		g_transmissionStateManager = nullptr;
		m_transmissionStateManager.reset();
	}

	// 【按钮状态控制优化】清理按钮状态管理器
	if (m_buttonStateManager) {
		m_buttonStateManager->ClearCallbacks();
		g_buttonStateManager = nullptr;
		m_buttonStateManager.reset();
	}

	// 【UI更新队列机制】清理线程安全UI更新器
	if (m_threadSafeUIUpdater) {
		m_threadSafeUIUpdater->Stop();
		g_threadSafeUIUpdater = nullptr;
		m_threadSafeUIUpdater.reset();
	}

	// 【进度更新安全化】清理线程安全进度管理器
	if (m_threadSafeProgressManager) {
		m_threadSafeProgressManager->ClearProgressCallback();
		g_threadSafeProgressManager = nullptr;
		m_threadSafeProgressManager.reset();
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

	// 【传输状态管理】切换到已连接状态，允许后续开始传输
	if (m_transmissionStateManager) {
		m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Connected, "连接成功");
		WriteLog("OnBnClickedButtonConnect: TransmissionStateManager切换到Connected状态");
	}

	// 【按钮状态管理】切换到已连接状态，正确更新所有按钮
	if (m_buttonStateManager) {
		m_buttonStateManager->ApplyConnectedState();
		WriteLog("OnBnClickedButtonConnect: ButtonStateManager切换到Connected状态");
	}

	// 更新状态条连接状态
	UpdateConnectionStatus();

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

	// 【按钮状态管理】切换到空闲状态，正确更新所有按钮
	if (m_buttonStateManager) {
		m_buttonStateManager->ApplyIdleState();
		WriteLog("OnBnClickedButtonDisconnect: ButtonStateManager切换到Idle状态");
	}

	// 更新状态条连接状态
	UpdateConnectionStatus();

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

	// 【传输状态管理统一】先转换到初始化状态，再转换到传输中
	if (m_transmissionStateManager) {
		// 步骤1：转换到初始化状态
		if (!m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Initializing, "初始化数据传输")) {
			MessageBox(_T("无法初始化传输，当前状态不允许"), _T("错误"), MB_OK | MB_ICONERROR);
			return;
		}

		// 步骤2：转换到传输中状态
		if (!m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Transmitting, "开始数据传输")) {
			MessageBox(_T("无法开始传输，当前状态不允许"), _T("错误"), MB_OK | MB_ICONERROR);
			return;
		}
	}

	// 重置传输状态
	m_isTransmitting = true;
	m_transmissionPaused = false;
	m_transmissionCancelled = false;

	// 重置进度条（仅在开始新传输时重置）
	m_progress.SetPos(0);

	// 更新按钮文本
	m_btnSend.SetWindowText(_T("中断"));

	// 【UI响应修复】使用状态管理器更新传输状态
	if (m_uiStateManager) {
		m_uiStateManager->UpdateTransmissionStatus("数据传输已开始", UIStateManager::Priority::Normal);
		UpdateUIStatus();
	}

	// 启动异步传输线程，避免UI阻塞
	if (m_transmissionThread.joinable())
	{
		m_transmissionThread.join();
	}

	m_transmissionThread = std::thread([this]()
									   { PerformDataTransmission(); });
}

// 【P1修复】更新传输控制方法 - 使用TransmissionTask实现暂停/恢复
void CPortMasterDlg::PauseTransmission()
{
	// 【传输状态管理统一】请求状态转换到暂停
	if (m_transmissionStateManager) {
		if (!m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Paused, "用户暂停传输")) {
			this->WriteLog("PauseTransmission: 当前状态不允许暂停");
			return;
		}
	}

	// 暂停当前传输任务
	if (m_currentTransmissionTask && m_currentTransmissionTask->IsRunning())
	{
		m_currentTransmissionTask->Pause();
		this->WriteLog("传输任务已暂停");

		// 更新按钮文本
		m_btnSend.SetWindowText(_T("继续"));
	}
	else
	{
		this->WriteLog("PauseTransmission: 没有运行中的传输任务可暂停");
	}

	// 保持与旧代码的兼容性
	m_transmissionPaused = true;
}

void CPortMasterDlg::ResumeTransmission()
{
	// 【传输状态管理统一】请求状态转换到传输中
	if (m_transmissionStateManager) {
		if (!m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Transmitting, "用户恢复传输")) {
			this->WriteLog("ResumeTransmission: 当前状态不允许恢复");
			return;
		}
	}

	// 恢复当前传输任务
	if (m_currentTransmissionTask && m_currentTransmissionTask->IsPaused())
	{
		m_currentTransmissionTask->Resume();
		this->WriteLog("传输任务已恢复");

		// 更新按钮文本
		m_btnSend.SetWindowText(_T("中断"));
	}
	else
	{
		this->WriteLog("ResumeTransmission: 没有暂停的传输任务可恢复");
	}

	// 保持与旧代码的兼容性
	m_transmissionPaused = false;
}

// 【P1修复】重构传输方法 - 使用TransmissionTask实现UI与传输解耦
void CPortMasterDlg::PerformDataTransmission()
{
	try
	{
		// 检查是否已有传输任务在运行
		if (m_currentTransmissionTask && !m_currentTransmissionTask->IsCompleted())
		{
			this->WriteLog("PerformDataTransmission: 传输任务已在运行中，忽略重复请求");
			return;
		}

		// 获取待发送数据
		const std::vector<uint8_t>& data = m_sendDataCache;
		if (data.empty())
		{
			this->WriteLog("PerformDataTransmission: 发送数据为空，无需传输");
			PostMessage(WM_USER + 13, (WPARAM)TransportError::InvalidParameter, 0);
			return;
		}

		this->WriteLog("PerformDataTransmission: 开始创建传输任务，数据大小: " + std::to_string(data.size()) + " 字节");

		// 创建适当的传输任务
		if (m_reliableChannel && m_reliableChannel->IsConnected())
		{
			this->WriteLog("PerformDataTransmission: 使用可靠传输通道");
			m_currentTransmissionTask = std::make_unique<ReliableTransmissionTask>(
				std::shared_ptr<ReliableChannel>(m_reliableChannel.get(), [](ReliableChannel*){}));
		}
		else if (m_transport && m_transport->IsOpen())
		{
			this->WriteLog("PerformDataTransmission: 使用原始传输通道");
			m_currentTransmissionTask = std::make_unique<RawTransmissionTask>(m_transport);
		}
		else
		{
			this->WriteLog("PerformDataTransmission: 错误 - 没有可用的传输通道");
			PostMessage(WM_USER + 13, (WPARAM)TransportError::NotOpen, 0);
			return;
		}

		// 设置回调函数
		m_currentTransmissionTask->SetProgressCallback(
			[this](const TransmissionProgress& progress) {
				this->OnTransmissionProgress(progress);
			});

		m_currentTransmissionTask->SetCompletionCallback(
			[this](const TransmissionResult& result) {
				this->OnTransmissionCompleted(result);
			});

		m_currentTransmissionTask->SetLogCallback(
			[this](const std::string& message) {
				this->OnTransmissionLog(message);
			});

		// 初始化进度条
		PostMessage(WM_USER + 10, 0, 100); // 设置进度条范围
		PostMessage(WM_USER + 11, 0, 0);   // 设置进度条位置

		// 启动传输任务
		bool started = m_currentTransmissionTask->Start(data);
		if (!started)
		{
			this->WriteLog("PerformDataTransmission: 错误 - 传输任务启动失败");
			m_currentTransmissionTask.reset();
			PostMessage(WM_USER + 13, (WPARAM)TransportError::WriteFailed, 0);
			return;
		}

		this->WriteLog("PerformDataTransmission: 传输任务已成功启动，UI线程继续响应");
	}
	catch (const std::exception& e)
	{
		this->WriteLog("PerformDataTransmission异常: " + std::string(e.what()));
		if (m_currentTransmissionTask)
		{
			m_currentTransmissionTask.reset();
		}
		PostMessage(WM_USER + 14, 0, 0); // 通知UI线程处理异常
	}
}

// 【P1修复】传输任务回调方法实现 - UI与传输解耦

void CPortMasterDlg::OnTransmissionProgress(const TransmissionProgress& progress)
{
	// 使用PostMessage确保线程安全的UI更新
	int progressPercent = progress.progressPercent;
	PostMessage(WM_USER + 11, 0, progressPercent);

	// 更新状态文本
	CString* statusText = new CString();

	// 【回归修复】正确处理UTF-8到Unicode编码转换，解决状态栏乱码问题
	// progress.statusText是UTF-8编码的std::string，需要显式转换为Unicode
	CA2W statusTextW(progress.statusText.c_str(), CP_UTF8);
	statusText->Format(_T("%s: %u/%u 字节 (%d%%)"),
					   (LPCWSTR)statusTextW,
					   (unsigned int)progress.bytesTransmitted,
					   (unsigned int)progress.totalBytes,
					   progressPercent);
	PostMessage(WM_USER + 12, 0, (LPARAM)statusText);

	// 记录详细进度信息（可选，用于调试）
	if (progress.bytesTransmitted % (progress.totalBytes / 10 + 1) == 0 || progress.progressPercent == 100)
	{
		this->WriteLog("传输进度: " + std::to_string(progress.bytesTransmitted) + "/" +
					   std::to_string(progress.totalBytes) + " 字节 (" +
					   std::to_string(progress.progressPercent) + "%)");
	}
}

// 【修复线程自阻塞】重构传输完成回调 - 避免在传输线程中直接析构任务对象
void CPortMasterDlg::OnTransmissionCompleted(const TransmissionResult& result)
{
	this->WriteLog("传输任务完成: 状态=" + std::to_string(static_cast<int>(result.finalState)) +
				   ", 错误码=" + std::to_string(static_cast<int>(result.errorCode)) +
				   ", 传输字节=" + std::to_string(result.bytesTransmitted) +
				   ", 耗时=" + std::to_string(result.duration.count()) + "ms");

	// 【关键修复】不在传输线程中直接析构任务对象，改为PostMessage到UI线程
	// 原因：当前回调在传输线程中执行，直接reset()会导致线程试图join自己，造成死锁

	// 通过PostMessage通知UI线程处理传输结果
	PostMessage(WM_USER + 13, (WPARAM)result.errorCode, 0);

	// 【新增】通过PostMessage安全地清理传输任务对象（在UI线程中执行）
	PostMessage(WM_USER + 15, 0, 0);

	// 根据结果状态显示相应信息
	switch (result.finalState)
	{
	case TransmissionTaskState::Completed:
		this->WriteLog("传输成功完成，总用时: " + std::to_string(result.duration.count()) + "ms");
		break;
	case TransmissionTaskState::Cancelled:
		this->WriteLog("传输被用户取消");
		break;
	case TransmissionTaskState::Failed:
		this->WriteLog("传输失败: " + result.errorMessage);
		break;
	default:
		this->WriteLog("传输结束，状态未知");
		break;
	}
}

void CPortMasterDlg::OnTransmissionLog(const std::string& message)
{
	// 直接转发到现有的日志系统
	this->WriteLog(message);
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

			// 【状态管理修复】使用TransmissionStateManager管理状态转换
			if (m_transmissionStateManager) {
				m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Failed, "用户手动终止传输");
			}

			// 【状态管理修复】使用ButtonStateManager更新按钮状态
			if (m_buttonStateManager) {
				m_buttonStateManager->ApplyConnectedState(); // 终止后回到已连接状态，允许重新发送
			}

			// 【状态管理修复】使用UIStateManager更新状态条
			if (m_uiStateManager) {
				m_uiStateManager->UpdateTransmissionStatus("传输已终止", UIStateManager::Priority::High);
			}

			// 更新进度条
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
			// 【编码修复】使用ATL转换宏安全转换UTF-8到UNICODE
			CA2T portName(port.c_str(), CP_UTF8);
			m_comboPort.AddString(portName);
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
			// 【编码修复】使用ATL转换宏安全转换UTF-8到UNICODE
			CA2T portName(port.c_str(), CP_UTF8);
			m_comboPort.AddString(portName);
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
			// 【编码修复】使用ATL转换宏安全转换UTF-8到UNICODE
			CA2T portName(port.c_str(), CP_UTF8);
			m_comboPort.AddString(portName);
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

	// 2. 【优化】简化状态检查，仅在文件流关闭时恢复
	if (m_useTempCacheFile && !m_tempCacheFile.is_open())
	{
		this->WriteLog("⚠️ 检测到临时文件流关闭，启动自动恢复...");
		if (CheckAndRecoverTempCacheFile())
		{
			this->WriteLog("✅ 临时文件自动恢复成功，继续数据写入");
		}
		else
		{
			this->WriteLog("❌ 临时文件自动恢复失败，数据将仅保存到内存缓存");
		}
	}

	// 3. 立即同步写入临时文件（强制串行化，确保数据完全落盘）
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

			// 【修复】数据写入后进行状态诊断
			LogTempCacheFileStatus("数据写入后状态验证");
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
	// 【P0修复】检查是否正在传输，禁止切换模式
	if (m_isTransmitting)
	{
		MessageBox(_T("传输进行中，无法切换模式\n请先停止传输"), _T("模式切换失败"), MB_OK | MB_ICONWARNING);
		// 恢复到直通模式单选按钮
		m_radioDirect.SetCheck(BST_CHECKED);
		m_radioReliable.SetCheck(BST_UNCHECKED);
		WriteLog("模式切换被阻止：传输进行中");
		return;
	}

	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("可靠模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);

	// 更新状态条模式信息
	m_staticMode.SetWindowText(_T("可靠"));

	// 【P0修复】强制重置传输状态管理器
	if (m_transmissionStateManager)
	{
		// 获取当前状态，如果不是Idle则强制重置
		TransmissionUIState currentState = m_transmissionStateManager->GetCurrentState();
		if (currentState != TransmissionUIState::Idle)
		{
			m_transmissionStateManager->ForceState(TransmissionUIState::Idle, "模式切换：强制重置到空闲状态");
			WriteLog("模式切换：传输状态已重置到Idle");
		}
	}

	// 【P0修复】根据连接状态更新按钮状态管理器
	if (m_buttonStateManager)
	{
		if (m_isConnected)
		{
			m_buttonStateManager->ApplyConnectedState();
			WriteLog("模式切换：按钮状态已恢复到Connected模式");
		}
		else
		{
			m_buttonStateManager->ApplyIdleState();
			WriteLog("模式切换：按钮状态已恢复到Idle模式");
		}
	}

	// 【状态管理修复】模式切换后更新保存按钮状态
	// 可靠模式下保存按钮的启用条件更严格（需要传输完成且有数据）
	UpdateSaveButtonStatus();

	WriteLog("模式切换：可靠模式");
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
	bool tempFileAvailable = false; // 添加变量声明

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

		// 【简化保存流程】采用直接读取+后验证模式，移除复杂的稳定性检测
		this->WriteLog("=== 开始简化保存流程 ===");

		// 【用户体验优化】传输状态检查与用户确认
		if (m_isTransmitting || m_transmissionPaused)
		{
			this->WriteLog("⚠️ 检测到传输仍在进行中");

			// 询问用户是否要等待传输完成
			int userChoice = MessageBox(
				_T("⚠️ 传输正在进行中\n\n")
				_T("当前数据可能不完整，建议等待传输完成后再保存。\n\n")
				_T("选择操作：\n")
				_T("• 是(Y) - 等待传输完成后再保存\n")
				_T("• 否(N) - 强制保存当前已接收的数据\n")
				_T("• 取消 - 取消保存操作"),
				_T("传输状态提醒"),
				MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON1
			);

			if (userChoice == IDYES)
			{
				this->WriteLog("用户选择等待传输完成");
				MessageBox(_T("请等待传输完成后再点击保存按钮"), _T("提示"), MB_OK | MB_ICONINFORMATION);
				return;
			}
			else if (userChoice == IDCANCEL)
			{
				this->WriteLog("用户取消保存操作");
				return;
			}
			else
			{
				this->WriteLog("用户选择强制保存当前数据");
			}
		}

		// 【关键修复】优先使用临时缓存文件，确保大文件传输的数据完整性
		try
		{
			// 【延后读取策略】先检查数据源可用性，但不即时读取数据
			this->WriteLog("策略：检查临时缓存文件可用性");
			
			// 仅检查文件是否存在，不读取内容
			// 【关键修复】移除重复声明，直接使用外层变量避免变量遮蔽
			{
				std::lock_guard<std::mutex> quickLock(m_receiveFileMutex);
				tempFileAvailable = PathFileExists(m_tempCacheFilePath) && (m_totalReceivedBytes > 0);
			}
			
			if (tempFileAvailable)
			{
				this->WriteLog("✅ 临时缓存文件可用，将在用户确认保存后读取最新数据");
				isBinaryMode = true; // 标记为二进制模式，但不读取数据
				// 为了通过后续判断，添加一个占位元素
				binaryData.push_back(0); // 占位数据，将在用户确认后替换为真实数据
			}
			else
			{
				this->WriteLog("⚠️ 临时缓存文件不可用，将使用备选数据源");
				
				// 使用内存缓存作为备选
				if (m_receiveCacheValid && !m_receiveDataCache.empty())
				{
					this->WriteLog("使用内存缓存数据作为备选");
					binaryData = m_receiveDataCache;
					isBinaryMode = true;
				}
			}
		}
		catch (const std::exception& e)
		{
			this->WriteLog("❌ 数据检查失败: " + std::string(e.what()));
			MessageBox(_T("⚠️ 数据检查失败\n\n")
					  _T("无法检查接收数据，请稍后重试。\n")
					  _T("如果问题持续存在，请重新启动传输。"),
					  _T("检查错误"), MB_OK | MB_ICONERROR);
			return;
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
			// 【关键修复】在用户确认保存后立即读取最新数据，确保数据完整性
			if (tempFileAvailable)
			{
				this->WriteLog("用户确认保存，立即读取临时文件最新数据");
				
				// 读取最新的完整数据
				std::vector<uint8_t> latestData;
				try
				{
					std::lock_guard<std::mutex> saveLock(m_receiveFileMutex);
					latestData = ReadAllDataFromTempCacheUnlocked();
					this->WriteLog("最新数据大小: " + std::to_string(latestData.size()) + " 字节");

					// 【数据完整性验证】确保读取到的数据不为空且大小合理
					if (latestData.empty())
					{
						this->WriteLog("❌ 致命错误：从临时文件读取到的数据为空");
						MessageBox(_T("❌ 数据读取失败\n\n")
								  _T("无法从临时文件中读取到任何数据。\n")
								  _T("这可能是由于文件损坏或访问冲突造成的。\n\n")
								  _T("建议：\n")
								  _T("• 重新启动传输\n")
								  _T("• 检查磁盘空间是否充足"),
								  _T("数据读取错误"), MB_OK | MB_ICONERROR);
						return;
					}
					else if (latestData.size() < 100) // 对于大文件传输，数据应该远大于100字节
					{
						this->WriteLog("⚠️ 数据异常：读取到的数据量过小，可能存在问题");
						// 【编码修复】使用CString::Format安全格式化数字
						CString message;
						message.Format(
							_T("⚠️ 数据量异常\n\n")
							_T("检测到数据量异常小（%zu 字节），这可能不是完整的传输数据。\n\n")
							_T("是否仍要继续保存？"),
							latestData.size()
						);
						int userChoice = MessageBox(message, _T("数据量异常"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

						if (userChoice == IDNO)
						{
							this->WriteLog("用户取消保存异常数据");
							return;
						}
					}
				}
				catch (const std::exception& e)
				{
					this->WriteLog("❌ 读取最新数据失败: " + std::string(e.what()));
					MessageBox(_T("❌ 数据读取异常\n\n")
							  _T("无法读取临时文件中的数据：\n") +
							  CString(e.what()) + _T("\n\n")
							  _T("请检查：\n")
							  _T("• 临时文件是否被其他程序占用\n")
							  _T("• 磁盘空间是否充足\n")
							  _T("• 文件权限是否正确"),
							  _T("读取错误"), MB_OK | MB_ICONERROR);
					return; // 读取失败时直接返回，不保存错误数据
				}
				
				SaveBinaryDataToFile(dlg.GetPathName(), latestData);
			}
			else
			{
				// 【安全处理】临时文件不可用时的错误处理
				this->WriteLog("❌ 临时文件不可用，无法保存完整数据");
				MessageBox(_T("❌ 数据源不可用\n\n")
						  _T("临时缓存文件不可用，无法保存完整的接收数据。\n\n")
						  _T("可能的原因：\n")
						  _T("• 数据尚未完全接收\n")
						  _T("• 临时文件已被清理\n")
						  _T("• 文件访问权限问题\n\n")
						  _T("建议：\n")
						  _T("• 等待传输完成后再保存\n")
						  _T("• 重新启动传输\n")
						  _T("• 切换到内存缓存模式"),
						  _T("保存失败"), MB_OK | MB_ICONERROR);
			}
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

			// 【保存后验证机制】验证保存文件的完整性
			bool verificationPassed = VerifySavedFileSize(filePath, utf8Length);
			if (verificationPassed)
			{
				this->WriteLog("✅ 保存后验证通过 - 文件大小与预期一致");

				CString message;
				message.Format(_T("文本数据保存成功 (%d 字节)\n✅ 文件完整性验证通过"), utf8Length);
				MessageBox(message, _T("保存成功"), MB_OK | MB_ICONINFORMATION);
			}
			else
			{
				this->WriteLog("❌ 保存后验证失败 - 文件大小与预期不符");

				CString message;
				message.Format(_T("⚠️ 保存完成但验证失败\n\n")
							  _T("预期大小: %d 字节\n")
							  _T("文件路径: %s\n\n")
							  _T("建议检查：\n")
							  _T("1. 磁盘空间是否充足\n")
							  _T("2. 文件是否被其他程序占用\n")
							  _T("3. 重新保存验证"),
							  utf8Length, filePath.GetString());
				MessageBox(message, _T("保存验证异常"), MB_OK | MB_ICONWARNING);
			}

			// 更新日志
			CTime time = CTime::GetCurrentTime();
			CString timeStr = time.Format(_T("[%H:%M:%S] "));
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			CString logMessage;
			logMessage.Format(_T("文本数据保存至: %s (%d 字节)"), fileName.GetString(), utf8Length);
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
	// 【P0修复】检查是否正在传输，禁止切换模式
	if (m_isTransmitting)
	{
		MessageBox(_T("传输进行中，无法切换模式\n请先停止传输"), _T("模式切换失败"), MB_OK | MB_ICONWARNING);
		// 恢复到可靠模式单选按钮
		m_radioReliable.SetCheck(BST_CHECKED);
		m_radioDirect.SetCheck(BST_UNCHECKED);
		WriteLog("模式切换被阻止：传输进行中");
		return;
	}

	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("直通模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);

	// 更新状态条模式信息
	m_staticMode.SetWindowText(_T("直通"));

	// 【P0修复】强制重置传输状态管理器
	if (m_transmissionStateManager)
	{
		// 获取当前状态，如果不是Idle则强制重置
		TransmissionUIState currentState = m_transmissionStateManager->GetCurrentState();
		if (currentState != TransmissionUIState::Idle)
		{
			m_transmissionStateManager->ForceState(TransmissionUIState::Idle, "模式切换：强制重置到空闲状态");
			WriteLog("模式切换：传输状态已重置到Idle");
		}
	}

	// 【P0修复】根据连接状态更新按钮状态管理器
	if (m_buttonStateManager)
	{
		if (m_isConnected)
		{
			m_buttonStateManager->ApplyConnectedState();
			WriteLog("模式切换：按钮状态已恢复到Connected模式");
		}
		else
		{
			m_buttonStateManager->ApplyIdleState();
			WriteLog("模式切换：按钮状态已恢复到Idle模式");
		}
	}

	// 【状态管理修复】模式切换后更新保存按钮状态
	// 直通模式下保存按钮的启用条件较宽松
	UpdateSaveButtonStatus();

	WriteLog("模式切换：直通模式");
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
                    // 【新增】处理超时和空数据情况，避免高频轮询
                    else
                    {
                        // 当Read超时或返回0字节时，短暂休眠避免忙等
                        // 这样可以将轮询频率从~10次/秒降低到~50次/秒
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
	this->WriteLog("UpdateConnectionStatus: 函数开始执行");

	// 【UI响应修复】使用状态管理器更新连接状态
	if (m_uiStateManager) {
		std::string statusText = m_isConnected ? "已连接" : "未连接";
		this->WriteLog("UpdateConnectionStatus: 更新连接状态为: " + statusText);
		m_uiStateManager->UpdateConnectionStatus(statusText, UIStateManager::Priority::Normal);
		this->WriteLog("UpdateConnectionStatus: 连接状态更新完成");
	}

	// 【可靠模式按钮管控】根据缺陷分析报告实施按钮状态控制
	this->WriteLog("UpdateConnectionStatus: 准备调用UpdateSaveButtonStatus");
	UpdateSaveButtonStatus();
	this->WriteLog("UpdateConnectionStatus: UpdateSaveButtonStatus执行完毕");

	// 【UI响应修复】应用状态更新到UI控件
	this->WriteLog("UpdateConnectionStatus: 准备调用UpdateUIStatus");
	UpdateUIStatus();
	this->WriteLog("UpdateConnectionStatus: UpdateUIStatus执行完毕");

	this->WriteLog("UpdateConnectionStatus: 函数执行完毕");
}

// 【可靠模式按钮管控】更新保存按钮状态的专用函数
void CPortMasterDlg::UpdateSaveButtonStatus()
{
	bool enableSaveButton = true; // 默认启用保存按钮

	// 检查是否处于可靠模式
	bool isReliableMode = (m_radioReliable.GetCheck() == BST_CHECKED);

	if (isReliableMode && m_reliableChannel && m_isConnected)
	{
		// 【关键修复】在可靠模式下，需要更严格的完整性检查
		bool fileTransferActive = m_reliableChannel->IsFileTransferActive();
		
		// 获取传输统计信息进行完整性验证
		auto stats = m_reliableChannel->GetStats();
		bool hasReceivedData = (stats.bytesReceived > 0);
		
		if (fileTransferActive)
		{
			enableSaveButton = false;
			this->WriteLog("可靠模式：文件传输进行中，禁用保存按钮");
		}
		else if (!hasReceivedData)
		{
			enableSaveButton = false;
			this->WriteLog("可靠模式：未接收到数据，禁用保存按钮");
		}
		else
		{
			// 【增强验证】检查是否有足够的数据用于保存
			bool hasValidData = (m_totalReceivedBytes > 0) || (!m_receiveDataCache.empty());
			if (hasValidData)
			{
				this->WriteLog("可靠模式：文件传输已完成且有有效数据，启用保存按钮");
			}
			else
			{
				enableSaveButton = false;
				this->WriteLog("可靠模式：传输完成但无有效数据，禁用保存按钮");
			}
		}
	}
	else if (!isReliableMode)
	{
		// 直通模式：保持原有逻辑，正常启用保存按钮
		this->WriteLog("直通模式：正常启用保存按钮");
	}

	// 更新保存按钮状态
	this->WriteLog("UpdateSaveButtonStatus: 准备更新保存按钮状态，enableSaveButton=" + std::to_string(enableSaveButton));
	m_btnSaveAll.EnableWindow(enableSaveButton ? TRUE : FALSE);
	this->WriteLog("UpdateSaveButtonStatus: 保存按钮状态已更新");
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
	// 【编码修复】使用ATL转换宏安全转换UTF-8到UNICODE
	CA2T errorMsgT(error.c_str(), CP_UTF8);
	CString errorMsg = errorMsgT;
	PostMessage(WM_USER + 2, 0, (LPARAM) new CString(errorMsg));
}

void CPortMasterDlg::OnReliableProgress(uint32_t progress)
{
	// 【进度更新安全化】使用安全的进度更新方式
	UpdateProgressSafe(static_cast<uint64_t>(progress), 100, "传输中...");
}

void CPortMasterDlg::OnReliableComplete(bool success)
{
	m_isTransmitting = false;

	if (success)
	{
		// 【传输状态管理统一】请求状态转换到完成
		if (m_transmissionStateManager) {
			m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Completed, "可靠传输完成");
		}

		// 传输完成 - 更新进度条
		m_progress.SetPos(100);

		// 2秒后恢复连接状态显示，但保持进度条100%状态
		SetTimer(TIMER_ID_CONNECTION_STATUS, 2000, NULL);
	}
	else
	{
		// 【传输状态管理统一】请求状态转换到失败
		if (m_transmissionStateManager) {
			m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Failed, "可靠传输失败");
		}

		// 传输失败 - 更新进度条
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
		// 连接成功 - 更新进度条
		// 【UI响应修复】使用状态管理器更新连接状态
		if (m_uiStateManager) {
			m_uiStateManager->UpdateConnectionStatus("已连接", UIStateManager::Priority::Normal);
			UpdateUIStatus();
		}
	}
	else
	{
		// 连接断开 - 更新进度条
		m_progress.SetPos(0);

		// 【UI响应修复】使用状态管理器更新连接状态
		if (m_uiStateManager) {
			m_uiStateManager->UpdateConnectionStatus("连接断开", UIStateManager::Priority::Normal);
			UpdateUIStatus();
		}
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
		m_progress.SetPos(0);

		// 【UI响应修复】使用状态管理器更新状态
		if (m_uiStateManager) {
			m_uiStateManager->UpdateTransmissionStatus("传输已终止", UIStateManager::Priority::Normal);
			UpdateUIStatus();
		}
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

		// 设置进度条为100%
		m_progress.SetPos(100);

		// 【UI响应修复】使用状态管理器更新传输状态
		if (m_uiStateManager) {
			std::string completeStatus = "传输完成: " + std::to_string(data.size()) + " 字节";
			m_uiStateManager->UpdateTransmissionStatus(completeStatus, UIStateManager::Priority::Normal);
			UpdateUIStatus();
		}

		// 添加传输完成提示对话框
		CString successMsg;
		successMsg.Format(_T("文件传输完成！\n\n已成功发送 %u 字节数据"), data.size());
		MessageBox(successMsg, _T("传输完成"), MB_OK | MB_ICONINFORMATION);

		// 【UI体验修复】发送成功后保持缓存有效，允许用户重新发送相同内容
		// 不清空缓存，保持与当前发送窗口内容的一致性，支持重复发送

		this->WriteLog("发送完成，缓存保持有效以支持重复发送");

		// 【关键修复】可靠模式特殊处理 - 确保协议层正确终止
		bool isReliableMode = (m_radioReliable.GetCheck() == BST_CHECKED);
		if (isReliableMode && m_reliableChannel)
		{
			this->WriteLog("可靠模式传输完成 - 检查协议层状态");

			// 等待协议层完全结束（给一些时间让END帧处理完成）
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// 检查文件传输是否真正完成
			bool transferActive = m_reliableChannel->IsFileTransferActive();
			this->WriteLog("可靠模式传输完成 - 协议层传输状态: " +
			              std::string(transferActive ? "仍活跃" : "已结束"));

			// 强制更新保存按钮状态
			UpdateSaveButtonStatus();
		}

		// 【P0修复】传输成功后更新状态管理器
		if (m_transmissionStateManager)
		{
			m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Completed, "传输成功完成");
			WriteLog("传输完成：TransmissionStateManager状态已转换到Completed");
		}

		// 【P0修复】传输成功后更新按钮状态管理器
		if (m_buttonStateManager)
		{
			m_buttonStateManager->ApplyCompletedState();
			WriteLog("传输完成：ButtonStateManager状态已转换到Completed");
		}

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

// 【回归修复】传输任务清理消息处理
LRESULT CPortMasterDlg::OnCleanupTransmissionTask(WPARAM wParam, LPARAM lParam)
{
	// 【关键修复】在UI线程中安全析构传输任务对象
	// 此方法通过PostMessage调用，确保在UI线程中执行，避免线程自阻塞问题
	if (m_currentTransmissionTask)
	{
		WriteLog("OnCleanupTransmissionTask - 开始安全清理传输任务对象");

		// 在UI线程中安全析构，此时传输线程已完成，可以安全调用join()
		m_currentTransmissionTask.reset();

		WriteLog("OnCleanupTransmissionTask - 传输任务对象已安全清理");
	}

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
		// 【编码修复】使用ATL转换宏安全转换UTF-8到UNICODE
	CA2T portName(serialConfig.portName.c_str(), CP_UTF8);
	SetDlgItemText(IDC_COMBO_PORT, portName);

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

		// 【读写冲突消除】保持写入流打开，仅刷新确保数据落盘
		if (m_tempCacheFile.is_open())
		{
			m_tempCacheFile.flush(); // 确保所有数据写入磁盘，但不关闭文件流
			WriteLog("ReadDataFromTempCache: 刷新写入流，保持文件流打开状态");
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

		WriteLog("ReadDataFromTempCache: 独立读取完成，写入流保持打开状态");

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

// 【简化保存流程】简化的不加锁数据读取版本
std::vector<uint8_t> CPortMasterDlg::ReadDataFromTempCacheUnlocked(uint64_t offset, size_t length)
{
	std::vector<uint8_t> result;

	if (m_tempCacheFilePath.IsEmpty() || !PathFileExists(m_tempCacheFilePath))
	{
		WriteLog("ReadDataFromTempCacheUnlocked: 临时缓存文件不存在");
		return result;
	}

	try
	{
		// 【回归修复】彻底解决竞态条件：使用独立读取流，避免关闭共享写入流
		// 记录读取前的文件大小，用于完整性验证
		uint64_t beforeReadBytes = m_totalReceivedBytes;

		if (m_tempCacheFile.is_open())
		{
			// 【关键修复】仅刷新数据到磁盘，绝不关闭写入流
			m_tempCacheFile.flush(); // 确保所有数据写入磁盘
			WriteLog("ReadDataFromTempCacheUnlocked: 刷新写入流到磁盘，保持流开放状态");
		}

		// 额外等待确保文件系统同步（解决Windows文件缓存问题）
		Sleep(50); // 50ms等待，确保文件系统完全同步

		// 【核心修复】使用独立读取流，避免与写入流冲突
		std::ifstream file(m_tempCacheFilePath, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			WriteLog("ReadDataFromTempCacheUnlocked: 无法打开临时缓存文件");
			return result;
		}

		// 获取文件大小
		file.seekg(0, std::ios::end);
		std::streamsize fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		WriteLog("ReadDataFromTempCacheUnlocked: 临时缓存文件大小 " + std::to_string(fileSize) + " 字节");

		if (fileSize <= 0)
		{
			WriteLog("ReadDataFromTempCacheUnlocked: 文件为空");
			file.close();
			return result;
		}

		// 计算读取长度（0表示读取全部）
		size_t availableLength = static_cast<size_t>(fileSize - offset);
		size_t targetReadLength = (length == 0) ? static_cast<size_t>(fileSize) : (length < availableLength ? length : availableLength);

		if (targetReadLength > 0 && offset < static_cast<uint64_t>(fileSize))
		{
			WriteLog("ReadDataFromTempCacheUnlocked: 开始分块循环读取，目标长度 " + std::to_string(targetReadLength) + " 字节");

			// 【关键修复】实施分块循环读取机制，彻底解决大文件部分读取问题
			try
			{
				// 预分配内存，提高性能
				result.reserve(targetReadLength);

				// 设置64KB块大小进行循环读取
				const size_t CHUNK_SIZE = 65536; // 64KB
				std::vector<char> chunk(CHUNK_SIZE);

				file.seekg(offset);
				size_t totalBytesRead = 0;
				size_t remainingBytes = targetReadLength;

				WriteLog("ReadDataFromTempCacheUnlocked: 使用64KB分块循环读取策略");

				while (remainingBytes > 0 && !file.eof())
				{
					// 计算本次读取大小（不超过剩余字节数和块大小）
					size_t currentChunkSize = (CHUNK_SIZE < remainingBytes) ? CHUNK_SIZE : remainingBytes;

					// 读取数据块
					file.read(chunk.data(), static_cast<std::streamsize>(currentChunkSize));
					std::streamsize actualRead = file.gcount(); // 获取实际读取的字节数

					if (actualRead > 0)
					{
						// 将读取的数据追加到结果向量
						size_t actualReadSize = static_cast<size_t>(actualRead);
						result.insert(result.end(), chunk.begin(), chunk.begin() + actualReadSize);
						totalBytesRead += actualReadSize;
						remainingBytes -= actualReadSize;

						// 【调试日志】记录读取进度（每10MB输出一次进度）
						if (totalBytesRead % (10 * 1024 * 1024) == 0 || remainingBytes == 0)
						{
							WriteLog("ReadDataFromTempCacheUnlocked: 读取进度 " +
									std::to_string(totalBytesRead) + "/" +
									std::to_string(targetReadLength) + " 字节 (" +
									std::to_string((totalBytesRead * 100) / targetReadLength) + "%)");
						}
					}
					else
					{
						// 如果无法再读取数据，检查是否是文件结束
						if (file.eof())
						{
							WriteLog("ReadDataFromTempCacheUnlocked: 到达文件末尾，读取完成");
							break;
						}
						else if (file.fail())
						{
							WriteLog("ReadDataFromTempCacheUnlocked: 读取过程中发生错误");
							break;
						}
					}
				}

				// 【数据完整性验证】检查实际读取量与预期读取量
				if (totalBytesRead == targetReadLength)
				{
					WriteLog("ReadDataFromTempCacheUnlocked: ✅ 数据读取完整，成功读取 " +
							std::to_string(totalBytesRead) + " 字节");
				}
				else
				{
					WriteLog("ReadDataFromTempCacheUnlocked: ⚠️ 数据读取不完整 - 预期: " +
							std::to_string(targetReadLength) + " 字节，实际: " +
							std::to_string(totalBytesRead) + " 字节");

					// 对于不完整读取，仍然返回已读取的数据，但记录警告
					if (totalBytesRead == 0)
					{
						WriteLog("ReadDataFromTempCacheUnlocked: ❌ 未读取到任何数据，清空结果");
						result.clear();
					}
				}
			}
			catch (const std::exception& e)
			{
				WriteLog("ReadDataFromTempCacheUnlocked: ❌ 分块读取过程中发生异常: " + std::string(e.what()));
				result.clear();
			}
		}

		file.close();

		// 【新增】数据完整性验证：检测读取过程中是否有新数据写入
		uint64_t afterReadBytes = m_totalReceivedBytes;
		if (afterReadBytes > beforeReadBytes)
		{
			size_t newDataSize = static_cast<size_t>(afterReadBytes - beforeReadBytes);
			WriteLog("⚠️ 检测到读取过程中有新数据写入: " + std::to_string(newDataSize) + " 字节");
			WriteLog("读取前: " + std::to_string(beforeReadBytes) + " 字节, 读取后: " + std::to_string(afterReadBytes) + " 字节");

			// 这里可以选择重新读取或者提示用户，当前先记录警告
			WriteLog("当前返回的数据可能不完整，建议用户等待传输完成后重新保存");
		}
		else
		{
			WriteLog("✅ 数据完整性验证通过，读取期间无新数据写入");
		}

		return result;
	}
	catch (const std::exception &e)
	{
		WriteLog("ReadDataFromTempCacheUnlocked: 读取异常 - " + std::string(e.what()));
		return result;
	}
}

std::vector<uint8_t> CPortMasterDlg::ReadAllDataFromTempCacheUnlocked()
{
	return ReadDataFromTempCacheUnlocked(0, 0); // 0长度表示读取全部数据
}

// 【简化保存流程】移除复杂的数据稳定性检测，采用直接保存+后验证模式
// 原有的 StabilityTracker::IsDataStable 方法已被移除，不再需要复杂的稳定性检测

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

// 【临时文件状态监控与自动恢复】新增机制实现

// 记录详细的临时文件状态诊断信息
void CPortMasterDlg::LogTempCacheFileStatus(const std::string& context)
{
	this->WriteLog("=== 临时文件状态诊断 [" + context + "] ===");
	this->WriteLog("m_useTempCacheFile: " + std::string(m_useTempCacheFile ? "true" : "false"));
	this->WriteLog("m_tempCacheFile.is_open(): " + std::string(m_tempCacheFile.is_open() ? "true" : "false"));
	this->WriteLog("临时文件路径: " + std::string(CT2A(m_tempCacheFilePath)));

	// 检查文件系统层面的文件状态
	if (!m_tempCacheFilePath.IsEmpty())
	{
		bool fileExists = PathFileExists(m_tempCacheFilePath) ? true : false;
		this->WriteLog("文件系统中文件存在: " + std::string(fileExists ? "true" : "false"));

		if (fileExists)
		{
			// 获取文件大小
			WIN32_FILE_ATTRIBUTE_DATA fileAttr;
			if (GetFileAttributesEx(m_tempCacheFilePath, GetFileExInfoStandard, &fileAttr))
			{
				LARGE_INTEGER fileSize;
				fileSize.LowPart = fileAttr.nFileSizeLow;
				fileSize.HighPart = fileAttr.nFileSizeHigh;
				this->WriteLog("文件系统中文件大小: " + std::to_string(fileSize.QuadPart) + " 字节");
			}
		}
	}

	this->WriteLog("内存缓存大小: " + std::to_string(m_receiveDataCache.size()) + " 字节");
	this->WriteLog("统计接收字节数: " + std::to_string(m_totalReceivedBytes) + " 字节");
	this->WriteLog("=== 状态诊断结束 ===");
}

// 验证临时文件完整性
bool CPortMasterDlg::VerifyTempCacheFileIntegrity()
{
	if (m_tempCacheFilePath.IsEmpty())
	{
		this->WriteLog("验证失败：临时文件路径为空");
		return false;
	}

	if (!PathFileExists(m_tempCacheFilePath))
	{
		this->WriteLog("验证失败：临时文件不存在");
		return false;
	}

	// 获取文件实际大小
	WIN32_FILE_ATTRIBUTE_DATA fileAttr;
	if (!GetFileAttributesEx(m_tempCacheFilePath, GetFileExInfoStandard, &fileAttr))
	{
		this->WriteLog("验证失败：无法获取文件属性");
		return false;
	}

	LARGE_INTEGER fileSize;
	fileSize.LowPart = fileAttr.nFileSizeLow;
	fileSize.HighPart = fileAttr.nFileSizeHigh;

	// 验证文件大小与统计数据一致性
	if (fileSize.QuadPart != m_totalReceivedBytes)
	{
		this->WriteLog("完整性验证：文件大小不匹配");
		this->WriteLog("文件实际大小: " + std::to_string(fileSize.QuadPart) + " 字节");
		this->WriteLog("统计接收字节: " + std::to_string(m_totalReceivedBytes) + " 字节");
		return false;
	}

	this->WriteLog("完整性验证通过：文件大小 " + std::to_string(fileSize.QuadPart) + " 字节");
	return true;
}

// 检查并恢复临时文件状态（核心函数）
bool CPortMasterDlg::CheckAndRecoverTempCacheFile()
{
	// 记录检查开始
	this->WriteLog("开始临时文件状态检查和恢复...");

	// 如果明确禁用了临时文件机制，直接返回
	if (!m_useTempCacheFile)
	{
		this->WriteLog("临时文件机制已禁用，无需恢复");
		return false;
	}

	// 检查文件流状态
	if (m_tempCacheFile.is_open())
	{
		// 验证文件流的健康状态
		if (m_tempCacheFile.good())
		{
			this->WriteLog("临时文件流状态正常，无需恢复");
			return true;
		}
		else
		{
			this->WriteLog("检测到文件流状态异常，开始修复...");
			// 关闭异常的文件流
			m_tempCacheFile.close();
		}
	}

	// 文件流已关闭，尝试重新打开
	this->WriteLog("尝试重新打开临时文件: " + std::string(CT2A(m_tempCacheFilePath)));

	if (m_tempCacheFilePath.IsEmpty())
	{
		this->WriteLog("❌ 恢复失败：临时文件路径为空，尝试重新初始化");
		return InitializeTempCacheFile();
	}

	try
	{
		// 使用追加模式重新打开，避免覆盖已有数据
		m_tempCacheFile.open(m_tempCacheFilePath, std::ios::out | std::ios::binary | std::ios::app);

		if (m_tempCacheFile.is_open())
		{
			this->WriteLog("✅ 临时文件恢复成功");

			// 验证文件完整性
			if (VerifyTempCacheFileIntegrity())
			{
				this->WriteLog("✅ 文件完整性验证通过");
				return true;
			}
			else
			{
				this->WriteLog("⚠️ 文件完整性验证失败，但文件流已恢复");
				// 即使完整性验证失败，文件流已经恢复，可以继续写入
				return true;
			}
		}
		else
		{
			this->WriteLog("❌ 文件流打开失败，尝试重新创建临时文件");
			return InitializeTempCacheFile();
		}
	}
	catch (const std::exception& e)
	{
		this->WriteLog("❌ 文件恢复异常: " + std::string(e.what()));
		this->WriteLog("尝试重新初始化临时文件...");
		return InitializeTempCacheFile();
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

// 【UI响应修复】状态管理器相关方法实现

// 初始化状态管理器
void CPortMasterDlg::InitializeUIStateManager()
{
	// 创建状态管理器实例
	m_uiStateManager = std::make_unique<UIStateManager>();

	// 设置全局指针
	g_uiStateManager = m_uiStateManager.get();

	// 初始化连接状态
	m_uiStateManager->UpdateConnectionStatus("未连接", UIStateManager::Priority::Normal);
}

// 【传输状态管理统一】初始化传输状态管理器
void CPortMasterDlg::InitializeTransmissionStateManager()
{
	// 创建传输状态管理器实例
	m_transmissionStateManager = std::make_unique<TransmissionStateManager>();

	// 设置全局指针
	g_transmissionStateManager = m_transmissionStateManager.get();

	// 设置状态变化回调
	m_transmissionStateManager->SetStateChangeCallback(
		[this](TransmissionUIState oldState, TransmissionUIState newState) {
			OnTransmissionStateChanged(oldState, newState);
		});

	// 初始化为空闲状态
	m_transmissionStateManager->RequestStateTransition(TransmissionUIState::Idle, "初始化传输状态管理器");
}

// 【按钮状态控制优化】初始化按钮状态管理器
void CPortMasterDlg::InitializeButtonStateManager()
{
	// 创建按钮状态管理器实例
	m_buttonStateManager = std::make_unique<ButtonStateManager>();

	// 设置全局指针
	g_buttonStateManager = m_buttonStateManager.get();

	// 设置状态变化回调
	m_buttonStateManager->SetStateChangeCallback(
		[this](ButtonID buttonId, ButtonState newState, const std::string& reason) {
			OnButtonStateChanged(buttonId, newState, reason);
		});

	// 初始化为空闲状态
	m_buttonStateManager->ApplyIdleState();

	// 更新按钮状态显示
	UpdateButtonStates();
}

// 【UI更新队列机制】初始化线程安全UI更新器
void CPortMasterDlg::InitializeThreadSafeUIUpdater()
{
	// 创建线程安全UI更新器实例
	m_threadSafeUIUpdater = std::make_unique<ThreadSafeUIUpdater>();

	// 设置全局指针
	g_threadSafeUIUpdater = m_threadSafeUIUpdater.get();

	// 启动UI更新器
	if (m_threadSafeUIUpdater->Start()) {
		WriteLog("线程安全UI更新器已启动");
	} else {
		WriteLog("线程安全UI更新器启动失败");
	}

	// 注册主要UI控件（这里使用示例控件ID，实际需要根据资源文件调整）
	// 注意：这里注册的是控件指针，需要在控件创建后进行注册
	// 将在OnInitDialog中进行控件注册
}

// 【进度更新安全化】初始化线程安全进度管理器
void CPortMasterDlg::InitializeThreadSafeProgressManager()
{
	// 创建线程安全进度管理器实例
	m_threadSafeProgressManager = std::make_unique<ThreadSafeProgressManager>();

	// 设置全局指针
	g_threadSafeProgressManager = m_threadSafeProgressManager.get();

	// 设置进度变化回调
	m_threadSafeProgressManager->SetProgressCallback(
		[this](const ProgressInfo& progress) {
			OnProgressChanged(progress);
		});

	// 设置最小更新间隔为100ms，避免过于频繁的UI更新
	m_threadSafeProgressManager->SetMinUpdateInterval(std::chrono::milliseconds(100));

	WriteLog("线程安全进度管理器已初始化");
}

// 【修复】在控件创建完成后统一初始化所有管理器
void CPortMasterDlg::InitializeManagersAfterControlsCreated()
{
	WriteLog("开始在控件创建完成后初始化管理器");

	try {
		// 【UI响应修复】初始化状态管理器
		InitializeUIStateManager();

		// 【传输状态管理统一】初始化传输状态管理器
		InitializeTransmissionStateManager();

		// 【按钮状态控制优化】初始化按钮状态管理器
		InitializeButtonStateManager();

		// 【UI更新队列机制】初始化线程安全UI更新器
		InitializeThreadSafeUIUpdater();

		// 【进度更新安全化】初始化线程安全进度管理器
		InitializeThreadSafeProgressManager();

		// 【临时修复】暂时注释掉控件注册，避免可能的断言问题
		// TODO: 调查ThreadSafeUIUpdater.RegisterControl方法的正确用法
		/*
		if (m_threadSafeUIUpdater) {
			// 注册主要状态控件
			m_threadSafeUIUpdater->RegisterControl(IDC_STATIC_PORT_STATUS, &m_staticPortStatus);
			m_threadSafeUIUpdater->RegisterControl(IDC_STATIC_MODE, &m_staticMode);
			m_threadSafeUIUpdater->RegisterControl(IDC_STATIC_SPEED, &m_staticSpeed);
			m_threadSafeUIUpdater->RegisterControl(IDC_PROGRESS, &m_progress);
			WriteLog("UI控件已注册到线程安全更新器");
		}
		*/

		WriteLog("所有管理器初始化完成");

		// 【新增】初始化完成后，将当前UI状态同步到管理器
		if (m_uiStateManager) {
			// 同步当前连接状态到状态管理器
			CString currentStatus;
			m_staticPortStatus.GetWindowText(currentStatus);
			CT2CA statusText(currentStatus);
			std::string statusStr(statusText);
			m_uiStateManager->UpdateConnectionStatus(statusStr, UIStateManager::Priority::Normal);
		}

	} catch (const std::exception& e) {
		WriteLog("管理器初始化异常: " + std::string(e.what()));
		// 如果初始化失败，记录错误但不阻止程序启动
		LogMessage(_T("管理器初始化失败，程序将继续运行"));
	} catch (...) {
		WriteLog("管理器初始化发生未知异常");
		LogMessage(_T("管理器初始化发生未知异常，程序将继续运行"));
	}
}

// 更新UI状态显示
void CPortMasterDlg::UpdateUIStatus()
{
	this->WriteLog("UpdateUIStatus: 函数开始执行");

	if (m_uiStateManager) {
		this->WriteLog("UpdateUIStatus: m_uiStateManager有效，准备应用状态");

		// 应用状态到状态栏控件
		this->WriteLog("UpdateUIStatus: 调用ApplyStatusToControl前");
		bool updated = m_uiStateManager->ApplyStatusToControl(&m_staticPortStatus);
		this->WriteLog("UpdateUIStatus: 调用ApplyStatusToControl后，updated=" + std::to_string(updated));

		// 如果状态有更新，记录日志
		if (updated) {
			this->WriteLog("UpdateUIStatus: 准备获取当前状态文本");
			std::string statusText = m_uiStateManager->GetCurrentStatusText();
			this->WriteLog("UI状态已更新: " + statusText);
		}
	}
	else {
		this->WriteLog("UpdateUIStatus: m_uiStateManager为空指针");
	}

	this->WriteLog("UpdateUIStatus: 函数执行完毕");
}

// 【传输状态管理统一】传输状态变化回调处理
void CPortMasterDlg::OnTransmissionStateChanged(TransmissionUIState oldState, TransmissionUIState newState)
{
	// 根据新状态更新UI显示
	switch (newState) {
	case TransmissionUIState::Idle:
		m_uiStateManager->UpdateTransmissionStatus("准备就绪");
		break;
	case TransmissionUIState::Connecting:
		m_uiStateManager->UpdateTransmissionStatus("连接中...");
		break;
	case TransmissionUIState::Connected:
		m_uiStateManager->UpdateTransmissionStatus("已连接");
		break;
	case TransmissionUIState::Initializing:
		m_uiStateManager->UpdateTransmissionStatus("初始化传输...");
		break;
	case TransmissionUIState::Handshaking:
		m_uiStateManager->UpdateTransmissionStatus("协议握手...");
		break;
	case TransmissionUIState::Transmitting:
		m_uiStateManager->UpdateTransmissionStatus("数据传输中...");
		break;
	case TransmissionUIState::Paused:
		m_uiStateManager->UpdateTransmissionStatus("传输已暂停");
		break;
	case TransmissionUIState::Completing:
		m_uiStateManager->UpdateTransmissionStatus("完成传输...");
		break;
	case TransmissionUIState::Completed:
		m_uiStateManager->UpdateTransmissionStatus("传输完成");
		break;
	case TransmissionUIState::Failed:
		m_uiStateManager->UpdateTransmissionStatus("传输失败", UIStateManager::Priority::High);
		break;
	case TransmissionUIState::Error:
		m_uiStateManager->UpdateTransmissionStatus("传输错误", UIStateManager::Priority::High);
		break;
	}

	// 更新UI状态显示
	UpdateUIStatus();

	// 记录状态变化
	std::string logMsg = "传输状态变化: "
		+ std::string(m_transmissionStateManager->GetStateDescription(oldState))
		+ " -> "
		+ std::string(m_transmissionStateManager->GetStateDescription(newState));
	WriteLog(logMsg);

	// 【按钮状态控制优化】根据传输状态更新按钮状态
	if (m_buttonStateManager) {
		// 检查是否为可靠传输模式
		bool isReliableMode = (m_reliableChannel != nullptr);

		switch (newState) {
		case TransmissionUIState::Idle:
			m_buttonStateManager->ApplyIdleState();
			break;
		case TransmissionUIState::Connecting:
			m_buttonStateManager->ApplyConnectingState();
			break;
		case TransmissionUIState::Connected:
			m_buttonStateManager->ApplyConnectedState();
			break;
		case TransmissionUIState::Transmitting:
			// 根据是否为可靠传输模式应用不同的按钮状态
			if (isReliableMode) {
				m_buttonStateManager->ApplyReliableModeTransmittingState();
			} else {
				m_buttonStateManager->ApplyTransmittingState();
			}
			break;
		case TransmissionUIState::Paused:
			// 根据是否为可靠传输模式应用不同的按钮状态
			if (isReliableMode) {
				m_buttonStateManager->ApplyReliableModePausedState();
			} else {
				m_buttonStateManager->ApplyPausedState();
			}
			break;
		case TransmissionUIState::Completed:
			// 根据是否为可靠传输模式应用不同的按钮状态
			if (isReliableMode) {
				m_buttonStateManager->ApplyReliableModeCompletedState();
			} else {
				m_buttonStateManager->ApplyCompletedState();
			}
			break;
		case TransmissionUIState::Failed:
		case TransmissionUIState::Error:
			m_buttonStateManager->ApplyErrorState();
			break;
		default:
			break;
		}

		// 更新按钮状态显示
		UpdateButtonStates();
	}
}

// 【按钮状态控制优化】按钮状态变化回调处理
void CPortMasterDlg::OnButtonStateChanged(ButtonID buttonId, ButtonState newState, const std::string& reason)
{
	// 根据按钮ID和状态更新对应的控件
	switch (buttonId) {
	case ButtonID::Connect:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnConnect.EnableWindow(TRUE);
		} else {
			m_btnConnect.EnableWindow(FALSE);
		}
		break;

	case ButtonID::Disconnect:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnDisconnect.EnableWindow(TRUE);
		} else {
			m_btnDisconnect.EnableWindow(FALSE);
		}
		break;

	case ButtonID::Send:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnSend.EnableWindow(TRUE);
		} else {
			m_btnSend.EnableWindow(FALSE);
		}
		break;

	case ButtonID::Stop:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnStop.EnableWindow(TRUE);
		} else {
			m_btnStop.EnableWindow(FALSE);
		}
		break;

	case ButtonID::File:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnFile.EnableWindow(TRUE);
		} else {
			m_btnFile.EnableWindow(FALSE);
		}
		break;

	case ButtonID::ClearAll:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnClearAll.EnableWindow(TRUE);
		} else {
			m_btnClearAll.EnableWindow(FALSE);
		}
		break;

	case ButtonID::ClearReceive:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnClearReceive.EnableWindow(TRUE);
		} else {
			m_btnClearReceive.EnableWindow(FALSE);
		}
		break;

	case ButtonID::CopyAll:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnCopyAll.EnableWindow(TRUE);
		} else {
			m_btnCopyAll.EnableWindow(FALSE);
		}
		break;

	case ButtonID::SaveAll:
		if (newState == ButtonState::Enabled || newState == ButtonState::Visible) {
			m_btnSaveAll.EnableWindow(TRUE);
		} else {
			m_btnSaveAll.EnableWindow(FALSE);
		}
		break;

	case ButtonID::PauseResume:
		// 暂停/继续按钮复用发送按钮，这里只记录状态
		break;

	default:
		break;
	}

	// 记录状态变化
	std::string logMsg = "按钮状态变化: ";

	// 获取按钮名称
	switch (buttonId) {
	case ButtonID::Connect: logMsg += "连接"; break;
	case ButtonID::Disconnect: logMsg += "断开"; break;
	case ButtonID::Send: logMsg += "发送"; break;
	case ButtonID::Stop: logMsg += "停止"; break;
	case ButtonID::File: logMsg += "文件"; break;
	case ButtonID::ClearAll: logMsg += "清空全部"; break;
	case ButtonID::ClearReceive: logMsg += "清空接收"; break;
	case ButtonID::CopyAll: logMsg += "复制全部"; break;
	case ButtonID::SaveAll: logMsg += "保存全部"; break;
	case ButtonID::PauseResume: logMsg += "暂停/继续"; break;
	default: logMsg += "未知按钮"; break;
	}

	// 获取状态名称
	switch (newState) {
	case ButtonState::Enabled: logMsg += " -> 启用"; break;
	case ButtonState::Disabled: logMsg += " -> 禁用"; break;
	case ButtonState::Visible: logMsg += " -> 显示"; break;
	case ButtonState::Hidden: logMsg += " -> 隐藏"; break;
	}

	if (!reason.empty()) {
		logMsg += " (" + reason + ")";
	}

	WriteLog(logMsg);
}

// 【按钮状态控制优化】更新按钮状态显示
void CPortMasterDlg::UpdateButtonStates()
{
	if (!m_buttonStateManager) {
		return;
	}

	// 更新所有按钮的显示状态
	OnButtonStateChanged(ButtonID::Connect, m_buttonStateManager->GetButtonState(ButtonID::Connect), "更新显示");
	OnButtonStateChanged(ButtonID::Disconnect, m_buttonStateManager->GetButtonState(ButtonID::Disconnect), "更新显示");
	OnButtonStateChanged(ButtonID::Send, m_buttonStateManager->GetButtonState(ButtonID::Send), "更新显示");
	OnButtonStateChanged(ButtonID::Stop, m_buttonStateManager->GetButtonState(ButtonID::Stop), "更新显示");
	OnButtonStateChanged(ButtonID::File, m_buttonStateManager->GetButtonState(ButtonID::File), "更新显示");
	OnButtonStateChanged(ButtonID::ClearAll, m_buttonStateManager->GetButtonState(ButtonID::ClearAll), "更新显示");
	OnButtonStateChanged(ButtonID::ClearReceive, m_buttonStateManager->GetButtonState(ButtonID::ClearReceive), "更新显示");
	OnButtonStateChanged(ButtonID::CopyAll, m_buttonStateManager->GetButtonState(ButtonID::CopyAll), "更新显示");
	OnButtonStateChanged(ButtonID::SaveAll, m_buttonStateManager->GetButtonState(ButtonID::SaveAll), "更新显示");
	OnButtonStateChanged(ButtonID::PauseResume, m_buttonStateManager->GetButtonState(ButtonID::PauseResume), "更新显示");
}

// 【UI更新队列机制】线程安全的状态更新
void CPortMasterDlg::UpdateUIStatusThreadSafe(const std::string& status)
{
	if (m_threadSafeUIUpdater) {
		// 将状态更新操作排队到UI线程执行
		m_threadSafeUIUpdater->QueueUpdate([this, status]() {
			if (m_uiStateManager) {
				m_uiStateManager->UpdateTransmissionStatus(status);
				UpdateUIStatus();
			}
		}, "线程安全状态更新");
	}
}

// 【UI更新队列机制】线程安全的进度更新
void CPortMasterDlg::UpdateProgressThreadSafe(int progress)
{
	if (m_threadSafeUIUpdater) {
		// 将进度更新操作排队到UI线程执行
		m_threadSafeUIUpdater->QueueUpdate([this, progress]() {
			m_progress.SetPos(progress);
		}, "线程安全进度更新");
	}
}

// 【进度更新安全化】进度变化回调处理
void CPortMasterDlg::OnProgressChanged(const ProgressInfo& progress)
{
	// 使用线程安全的方式更新UI进度条
	UpdateProgressThreadSafe(progress.percentage);

	// 如果有状态文本，也更新状态显示
	if (!progress.statusText.empty()) {
		UpdateUIStatusThreadSafe(progress.statusText);
	}

	// 记录进度信息（可选）
	std::ostringstream log;
	log << "进度更新: " << progress.current << "/" << progress.total
		<< " (" << progress.percentage << "%)";

	if (!progress.statusText.empty()) {
		log << " - " << progress.statusText;
	}

	WriteLog(log.str());
}

// 【进度更新安全化】安全的进度更新
void CPortMasterDlg::UpdateProgressSafe(uint64_t current, uint64_t total, const std::string& status)
{
	if (m_threadSafeProgressManager) {
		m_threadSafeProgressManager->SetProgress(current, total, status);
	}
}