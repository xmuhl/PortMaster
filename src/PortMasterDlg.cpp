﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿﻿// PortMasterDlg.cpp : 实现文件
//
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "framework.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "afxdialogex.h"
#include "PortMasterDialogEvents.h"
#include "../Transport/ITransport.h"
#include "../Common/CommonTypes.h"
#include "../Common/ConfigStore.h"
#include "../Common/StringUtils.h"
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
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// 日志写入函数
void CPortMasterDlg::WriteLog(const std::string& message)
{
	try
	{
		// 【调试日志优化】仅在关键步骤节点输出调试信息
		static const std::vector<std::string> keywords = {
			"开始", "成功", "失败", "错误", "异常", "取消", "暂停", "恢复",
			"连接", "断开", "传输任务", "按钮状态", "保存数据", "清空缓存",
			"OnBnClicked", "析构", "初始化", "退出", "WriteLog", "打开文件", "关闭文件",
			"传输完成", "传输失败", "可靠传输", "直接传输", "协调传输任务"
		};

		// 【排除冗余UI更新信息】过滤掉高频次的UI更新日志
		static const std::vector<std::string> excludeKeywords = {
			"轻量级UI更新完成", "UI更新完成", "界面更新", "进度更新", "显示更新",
			"定时器", "状态栏", "进度条", "刷新", "重绘", "UpdateUI", "ThrottledUpdate",
			"AppendData 开始", "强制同步写入成功", "字节", "追加数据", "接收数据大小",
			"接收到数据大小", "UTF-8转换后字节长度", "原始字节数据大小", "数据大小:"
		};

		// 检查是否应该排除（优先级最高）
		bool shouldExclude = false;
		for (const auto& exclude : excludeKeywords) {
			if (message.find(exclude) != std::string::npos) {
				shouldExclude = true;
				break;
			}
		}

		// 如果没有被排除，检查是否包含关键词
		bool shouldLog = !shouldExclude;
		if (shouldLog) {
			shouldLog = false;
			for (const auto& keyword : keywords) {
				if (message.find(keyword) != std::string::npos) {
					shouldLog = true;
					break;
				}
			}
		}

		// 如果不符合条件，直接返回（不写入日志）
		if (!shouldLog) {
			// 即使不写入日志文件，仍然更新状态栏显示重要信息
			if (m_statusDisplayManager) {
				std::wstring displayMessageW = StringUtils::WideEncodeUtf8(message);
				CString displayMessage(displayMessageW.c_str());
				m_statusDisplayManager->LogMessage(displayMessage);
			}
			return;
		}

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

		// 【阶段B修复】正确处理UTF-8到Unicode编码转换，解决状态栏乱码问题
		if (m_statusDisplayManager)
		{
			// 使用StringUtils替代MFC宏，避免长日志信息截断风险
			std::wstring displayMessageW = StringUtils::WideEncodeUtf8(message);
			CString displayMessage(displayMessageW.c_str());
			m_statusDisplayManager->LogMessage(displayMessage);
		}
	}
	catch (...)
	{
		// 忽略日志写入错误
	}
}

// CPortMasterDlg 对话框

CPortMasterDlg::CPortMasterDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PORTMASTER_DIALOG, pParent),
	m_transport(nullptr),
	m_reliableChannel(nullptr),
	m_isConnected(false),
	m_isTransmitting(false),
	m_transmissionPaused(false),
	m_transmissionCancelled(false),
	m_requiresReconnect(false),  // 【阶段C初始化】模式切换标志初始化为false
	m_shutdownInProgress(false),  // 【阶段二初始化】程序关闭幂等标志初始化为false
	m_receiveThread(),
	// 【Stage4迁移】移除传输线程初始化，由TransmissionCoordinator管理
	m_reliableSessionMutex(),
	m_reliableExpectedBytes(0),
	m_reliableReceivedBytes(0),
	m_bytesSent(0),
	m_bytesReceived(0),
	m_sendSpeed(0),
	m_receiveSpeed(0),
	// 【阶段1迁移】m_lastProgressPercent、m_receiveDisplayPending、m_lastReceiveDisplayUpdate已迁移到DialogUiController
	m_configStore(ConfigStore::GetInstance()),
	m_binaryDataDetected(false),
	m_updateDisplayInProgress(false),
	m_receiveVerboseLogging(false),
	m_lastUiLogTick(0ULL),
	m_totalSentBytes(0),
	m_sendCacheValid(false),
	m_receiveCacheValid(false),
	m_useTempCacheFile(false),
	m_totalReceivedBytes(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	// 初始化DialogConfigBinder
	try
	{
		m_configBinder = std::make_unique<DialogConfigBinder>(this, m_configStore);

		// 设置配置变更回调
		m_configBinder->SetConfigChangedCallback([this]() {
			OnConfigurationChanged();
			});

		// 设置错误回调
		m_configBinder->SetErrorCallback([this](const std::string& errorMessage) {
			CString errorMsg;
			CA2T convertedMsg(errorMessage.c_str());
		errorMsg.Format(_T("配置错误: %s"), (LPCTSTR)convertedMsg);
			MessageBox(errorMsg, _T("配置错误"), MB_OK | MB_ICONWARNING);
			});
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化配置绑定器失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}

	// 初始化ReceiveCacheService
	try
	{
		m_receiveCacheService = std::make_unique<ReceiveCacheService>();

		// 设置日志回调
		m_receiveCacheService->SetLogCallback([this](const std::string& message) {
			this->WriteLog(message);
			});

		// 设置详细日志（开发阶段开启）
		m_receiveCacheService->SetVerboseLogging(true);

		// 初始化临时缓存文件
		if (!m_receiveCacheService->Initialize())
		{
			MessageBox(_T("初始化接收缓存服务失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
		}
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化接收缓存服务失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}

	// 初始化TransmissionCoordinator
	try
	{
		m_transmissionCoordinator = std::make_unique<TransmissionCoordinator>();

		// 设置进度回调
		m_transmissionCoordinator->SetProgressCallback([this](const TransmissionProgress& progress) {
			this->OnTransmissionProgress(progress);
			});

		// 设置完成回调
		m_transmissionCoordinator->SetCompletionCallback([this](const TransmissionResult& result) {
			this->OnTransmissionCompleted(result);
			});

		// 设置日志回调
		m_transmissionCoordinator->SetLogCallback([this](const std::string& message) {
			this->OnTransmissionLog(message);
			});

		// 配置传输参数
		m_transmissionCoordinator->SetChunkSize(1024); // 1KB数据块
		m_transmissionCoordinator->SetRetrySettings(3, 1000); // 最多重试3次，间隔1秒
		m_transmissionCoordinator->SetProgressUpdateInterval(100); // 100ms更新间隔
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化传输协调器失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}

	// 初始化PortSessionController
	try
	{
		m_sessionController = std::make_unique<PortSessionController>();

		// 设置数据接收回调
		m_sessionController->SetDataCallback([this](const std::vector<uint8_t>& data) {
			this->OnTransportDataReceived(data);
			});

		// 设置错误回调
		m_sessionController->SetErrorCallback([this](const std::string& error) {
			this->OnTransportError(error);
			});
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化会话控制器失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}
}

void CPortMasterDlg::DoDataExchange(CDataExchange* pDX)
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

	// 【程序启动时清空日志文件】避免反复测试导致日志文件过长
	try {
		std::ofstream logFile("PortMaster_debug.log", std::ios::out | std::ios::trunc);
		if (logFile.is_open()) {
			logFile.close();
		}
	}
	catch (...) {
		// 忽略清空日志文件的错误，不影响程序启动
	}

	// 将"关于..."菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
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

	// 【阶段1迁移】初始化DialogUiController - 统一管理控件初始化和状态更新
	try
	{
		// 创建DialogUiController实例
		m_uiController = std::make_unique<DialogUiController>();

		// 构建控件引用结构
		UIControlRefs controlRefs;
		controlRefs.btnConnect = &m_btnConnect;
		controlRefs.btnDisconnect = &m_btnDisconnect;
		controlRefs.btnSend = &m_btnSend;
		controlRefs.btnStop = &m_btnStop;
		controlRefs.btnFile = &m_btnFile;
		controlRefs.btnClearAll = &m_btnClearAll;
		controlRefs.btnClearReceive = &m_btnClearReceive;
		controlRefs.btnCopyAll = &m_btnCopyAll;
		controlRefs.btnSaveAll = &m_btnSaveAll;
		controlRefs.editSendData = &m_editSendData;
		controlRefs.editReceiveData = &m_editReceiveData;
		controlRefs.editTimeout = &m_editTimeout;
		controlRefs.comboPortType = &m_comboPortType;
		controlRefs.comboPort = &m_comboPort;
		controlRefs.comboBaudRate = &m_comboBaudRate;
		controlRefs.comboDataBits = &m_comboDataBits;
		controlRefs.comboParity = &m_comboParity;
		controlRefs.comboStopBits = &m_comboStopBits;
		controlRefs.comboFlowControl = &m_comboFlowControl;
		controlRefs.radioReliable = &m_radioReliable;
		controlRefs.radioDirect = &m_radioDirect;
		controlRefs.checkHex = &m_checkHex;
		controlRefs.checkOccupy = &m_checkOccupy;
		controlRefs.progress = &m_progress;
		controlRefs.staticLog = &m_staticLog;
		controlRefs.staticPortStatus = &m_staticPortStatus;
		controlRefs.staticMode = &m_staticMode;
		controlRefs.staticSpeed = &m_staticSpeed;
		controlRefs.staticSendSource = &m_staticSendSource;
		controlRefs.parentDialog = this;

		// 初始化DialogUiController（包含所有控件初始化和状态设置）
		m_uiController->Initialize(controlRefs);

		// 设置节流显示回调
		m_uiController->SetThrottledDisplayCallback([this]() {
			this->UpdateReceiveDisplayFromCache();
			});
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化UI控制器失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}

	// 【阶段2迁移】初始化PortConfigPresenter - 统一管理端口配置和枚举
	try
	{
		// 创建PortConfigPresenter实例
		m_portConfigPresenter = std::make_unique<PortConfigPresenter>();

		// 构建端口配置控件引用结构
		PortConfigControlRefs portControlRefs;
		portControlRefs.comboPortType = &m_comboPortType;
		portControlRefs.comboPort = &m_comboPort;
		portControlRefs.comboBaudRate = &m_comboBaudRate;
		portControlRefs.comboDataBits = &m_comboDataBits;
		portControlRefs.comboParity = &m_comboParity;
		portControlRefs.comboStopBits = &m_comboStopBits;
		portControlRefs.comboFlowControl = &m_comboFlowControl;
		portControlRefs.parentDialog = this;

		// 初始化PortConfigPresenter（包含端口类型初始化）
		m_portConfigPresenter->Initialize(portControlRefs);

		// 设置默认选择回路测试
		m_portConfigPresenter->SetSelectedPortType(PortTypeIndex::Loopback);
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化端口配置呈现器失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}

	// 【阶段4迁移】初始化StatusDisplayManager - 统一管理状态展示和日志
	try
	{
		// 创建StatusDisplayManager实例
		m_statusDisplayManager = std::make_unique<StatusDisplayManager>();

		// 初始化StatusDisplayManager（绑定父对话框）
		m_statusDisplayManager->Initialize(this);

		// 设置节流显示回调（与DialogUiController协同）
		m_statusDisplayManager->SetThrottledDisplayCallback([this]() {
			this->UpdateReceiveDisplayFromCache();
			});
	}
	catch (const std::exception& e)
	{
		MessageBox(_T("初始化状态展示管理器失败: ") + CString(e.what()), _T("初始化错误"), MB_OK | MB_ICONERROR);
	}

	// 初始化事件调度器
	m_eventDispatcher = std::make_unique<PortMasterDialogEvents>(*this);

	// 初始化传输配置
	InitializeTransportConfig();

	// 从配置存储加载配置
	LoadConfigurationFromStore();

	// 启用文件拖拽功能
	DragAcceptFiles(TRUE);

	// ✅ 修复：初始化按钮状态，确保Stop按钮初始禁用，Send按钮初始启用
	if (m_uiController)
	{
		// 【阶段三修复】统一使用状态机驱动UI更新，初始化为Idle状态
		m_uiController->ApplyTransmissionState(TransmissionUiState::Idle);
	}

	// 初始化ReceiveCacheService（临时缓存文件功能已迁移到服务层）
	// ReceiveCacheService已在构造函数初始化部分完成

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
		// 【阶段1迁移】使用DialogUiController管理节流显示
		if (m_uiController)
		{
			m_uiController->StopThrottledDisplayTimer();

			if (m_uiController->IsDisplayUpdatePending())
			{
				// 执行延迟的显示更新
				UpdateReceiveDisplayFromCache();
				m_uiController->RecordDisplayUpdate();
				m_uiController->SetDisplayUpdatePending(false);
			}
		}
	}
	else if (nIDEvent == 3)
	{
		// 【分类6.4优化】异步清空传输速度显示，避免UI卡顿
		KillTimer(3);
		if (m_uiController)
		{
			m_uiController->SetStaticText(IDC_STATIC_SPEED, _T(""));
		}
	}

	CDialogEx::OnTimer(nIDEvent);
}

// 【阶段二修复】程序关闭时清理传输资源（幂等设计，防止重复调用）
void CPortMasterDlg::ShutdownActiveTransmission()
{
	// 幂等保护：防止重复调用
	if (m_shutdownInProgress.exchange(true))
	{
		return;  // 已在清理过程中，忽略重复调用
	}

	try
	{
		// 1. 停止所有活跃的传输任务（【阶段六修复】改为异步方式）
		if (m_transmissionCoordinator)
		{
			if (m_transmissionCoordinator->IsRunning() || m_transmissionCoordinator->IsPaused())
			{
				WriteLog("程序关闭：正在取消活跃的传输任务...");
				m_transmissionCoordinator->Cancel();

				// 【阶段六修复】改为超时轮询而非阻塞等待，避免窗口卡顿
				// 最多等待3秒，每200ms检查一次
				bool taskStopped = false;
				for (int i = 0; i < 15; ++i)
				{
					if (!m_transmissionCoordinator->IsRunning() && !m_transmissionCoordinator->IsPaused())
					{
						taskStopped = true;
						break;
					}
					Sleep(200);  // 每200ms检查一次（减少CPU占用）
				}

				if (taskStopped)
				{
					WriteLog("程序关闭：传输任务已取消");
				}
				else
				{
					WriteLog("程序关闭：等待传输任务超时，继续关闭程序");
				}

				// 【阶段六修复】清理任务资源
				m_transmissionCoordinator->CleanupTransmissionTask();
			}
		}

		// 2. 停止接收会话
		if (m_sessionController)
		{
			WriteLog("程序关闭：正在停止接收会话...");
			m_sessionController->StopReceiveSession();
			WriteLog("程序关闭：接收会话已停止");
		}

		// 3. 清理ReceiveCacheService（临时缓存文件功能已迁移到服务层）
		if (m_receiveCacheService)
		{
			WriteLog("程序关闭：正在关闭临时缓存服务...");
			m_receiveCacheService->Shutdown();
			WriteLog("程序关闭：临时缓存服务已关闭");
		}

		// 4. 断开传输连接
		if (m_sessionController)
		{
			WriteLog("程序关闭：正在断开传输连接...");
			m_sessionController->Disconnect();
			WriteLog("程序关闭：传输连接已断开");
		}

		WriteLog("程序关闭：所有资源已清理完毕");
	}
	catch (const std::exception& e)
	{
		WriteLog(std::string("程序关闭时发生异常: ") + e.what());
	}
	catch (...)
	{
		WriteLog("程序关闭时发生未知异常");
	}
}

// 【阶段二修复】重载OnCancel方法，在销毁前执行清理
void CPortMasterDlg::OnCancel()
{
	// 窗口句柄仍然有效时执行清理，避免PostMessage到无效窗口
	ShutdownActiveTransmission();

	// 调用基类实现默认的cancel逻辑
	CDialogEx::OnCancel();
}

// 【阶段二修复】重载OnClose方法，在销毁前执行清理
void CPortMasterDlg::OnClose()
{
	// 窗口句柄仍然有效时执行清理，避免PostMessage到无效窗口
	ShutdownActiveTransmission();

	// 调用基类实现默认的close逻辑
	CDialogEx::OnClose();
}

void CPortMasterDlg::PostNcDestroy()
{
	// 【阶段二修复】简化PostNcDestroy，所有清理逻辑已移至ShutdownActiveTransmission
	// 此处仅调用基类实现，避免销毁后继续触发回调
	CDialogEx::PostNcDestroy();
}

void CPortMasterDlg::OnBnClickedButtonConnect()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleConnect();
	}
}

void CPortMasterDlg::OnBnClickedButtonDisconnect()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleDisconnect();
	}
}

void CPortMasterDlg::OnBnClickedButtonSend()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleSend();
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
	SetProgressPercent(0, true);

	// 更新按钮文本和状态栏
	if (m_uiController)
	{
		m_uiController->SetStatusText(_T("数据传输已开始"));
		// 【阶段三修复】统一使用状态机驱动UI更新，Running状态自动设置按钮文本为"中断"
		m_uiController->ApplyTransmissionState(TransmissionUiState::Running);
	}

	// 【Stage4迁移】使用TransmissionCoordinator启动实际传输
	PerformDataTransmission();
}

// 【P1修复】更新传输控制方法 - 使用TransmissionTask实现暂停/恢复
void CPortMasterDlg::PauseTransmission()
{
	// 使用TransmissionCoordinator暂停传输任务
	if (m_transmissionCoordinator && m_transmissionCoordinator->IsRunning())
	{
		m_transmissionCoordinator->Pause();
		this->WriteLog("传输协调器已暂停传输任务");

		// 更新按钮文本和状态栏
		if (m_uiController)
		{
			m_uiController->SetStatusText(_T("传输已暂停"));
			// 【阶段三修复】统一使用状态机驱动UI更新，Paused状态自动设置按钮文本为"继续"
			m_uiController->ApplyTransmissionState(TransmissionUiState::Paused);
		}
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
	// 使用TransmissionCoordinator恢复传输任务
	if (m_transmissionCoordinator && m_transmissionCoordinator->IsPaused())
	{
		m_transmissionCoordinator->Resume();
		this->WriteLog("传输协调器已恢复传输任务");

		// 更新按钮文本和状态栏
		if (m_uiController)
		{
			m_uiController->SetStatusText(_T("传输已恢复"));
			// 【阶段三修复】统一使用状态机驱动UI更新，Running状态自动设置按钮文本为"中断"
			m_uiController->ApplyTransmissionState(TransmissionUiState::Running);
		}
	}
	else
	{
		this->WriteLog("ResumeTransmission: 没有暂停的传输任务可恢复");
	}

	// 保持与旧代码的兼容性
	m_transmissionPaused = false;
}

// 【阶段D迁移】使用TransmissionCoordinator实现传输任务协调
void CPortMasterDlg::PerformDataTransmission()
{
	try
	{
		// 检查是否已有传输任务在运行
		if (m_transmissionCoordinator && m_transmissionCoordinator->IsRunning())
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

		this->WriteLog("PerformDataTransmission: 开始协调传输任务，数据大小: " + std::to_string(data.size()) + " 字节");

		// 使用TransmissionCoordinator自动选择传输通道并启动传输
		if (m_transmissionCoordinator)
		{
			// 初始化进度条
			PostMessage(WM_USER + 10, 0, 100); // 设置进度条范围
			PostMessage(WM_USER + 11, 0, 0);   // 设置进度条位置

			// 启动传输任务（TransmissionCoordinator会自动选择可靠/直接传输模式）
			// 【阶段A修复】从PortSessionController获取实际的连接对象
			std::shared_ptr<ReliableChannel> reliableChannel = m_sessionController ?
				m_sessionController->GetReliableChannel() : nullptr;
			std::shared_ptr<ITransport> transport = m_sessionController ?
				m_sessionController->GetTransport() : nullptr;

			// 【第二轮阶段A修复】根据模式化通道判定逻辑
			bool isReliableMode = m_uiController && m_uiController->IsReliableModeSelected();

			if (isReliableMode)
			{
				// 可靠模式：要求reliableChannel非空且IsConnected()为真
				if (!reliableChannel)
				{
					this->WriteLog("PerformDataTransmission: 错误 - 可靠模式下获取可靠通道失败");
					PostMessage(WM_USER + 13, (WPARAM)TransportError::NotOpen, 0);
					return;
				}

				if (!reliableChannel->IsConnected())
				{
					this->WriteLog("PerformDataTransmission: 错误 - 可靠通道未连接");
					PostMessage(WM_USER + 13, (WPARAM)TransportError::NotOpen, 0);
					return;
				}

				this->WriteLog("PerformDataTransmission: 可靠模式验证通过，可靠通道已连接");
			}
			else
			{
				// 直通模式：仅要求transport非空且IsOpen()为真
				if (!transport)
				{
					this->WriteLog("PerformDataTransmission: 错误 - 直通模式下获取传输通道失败");
					PostMessage(WM_USER + 13, (WPARAM)TransportError::NotOpen, 0);
					return;
				}

				if (!transport->IsOpen())
				{
					this->WriteLog("PerformDataTransmission: 错误 - 传输通道未打开");
					PostMessage(WM_USER + 13, (WPARAM)TransportError::NotOpen, 0);
					return;
				}

				this->WriteLog("PerformDataTransmission: 直通模式验证通过，传输通道已打开");
			}

			bool started = m_transmissionCoordinator->Start(
				data,
				reliableChannel,
				transport
			);

			if (!started)
			{
				this->WriteLog("PerformDataTransmission: 错误 - 传输协调器启动失败");
				PostMessage(WM_USER + 13, (WPARAM)TransportError::WriteFailed, 0);
				return;
			}

			this->WriteLog("PerformDataTransmission: 传输协调器已成功启动，UI线程继续响应");
		}
		else
		{
			this->WriteLog("PerformDataTransmission: 错误 - 传输协调器未初始化");
			PostMessage(WM_USER + 13, (WPARAM)TransportError::NotOpen, 0);
		}
	}
	catch (const std::exception& e)
	{
		this->WriteLog("PerformDataTransmission异常: " + std::string(e.what()));
		PostMessage(WM_USER + 14, 0, 0); // 通知UI线程处理异常
	}
}

// 【P1修复】传输任务回调方法实现 - UI与传输解耦

void CPortMasterDlg::OnTransmissionProgress(const TransmissionProgress& progress)
{
	// 【第二轮修复】检查窗口句柄有效性，避免PostMessage到已销毁窗口导致宕机
	if (!IsWindow(GetSafeHwnd()))
	{
		// 窗口已销毁，直接返回，避免继续处理
		return;
	}

	// 使用PostMessage确保线程安全的UI更新
	int progressPercent = progress.progressPercent;
	PostMessage(WM_USER + 11, 0, static_cast<LPARAM>(progressPercent));

	// 更新状态文本
	CString* statusText = new CString();

	// 【修复状态文本重复】直接使用后台传递的完整状态文本，不再追加重复信息
	// 后台(TransmissionTask)已生成完整状态："正在传输: X/Y 字节 (P%)"
	// 【回归修复】正确处理UTF-8到Unicode编码转换，解决状态栏乱码问题
	// 使用StringUtils替代MFC宏，消除内存风险
	std::wstring statusTextW = StringUtils::WideEncodeUtf8(progress.statusText);
	*statusText = statusTextW.c_str();  // 直接赋值，不再Format追加

	// 【第二轮修复】再次检查窗口有效性
	if (IsWindow(GetSafeHwnd()))
	{
		PostMessage(WM_USER + 12, 0, reinterpret_cast<LPARAM>(statusText));
	}
	else
	{
		// 窗口已销毁，释放内存并返回
		delete statusText;
		return;
	}

	// ✅ 删除进度日志记录，保持日志清洁
	// 不再记录每个数据块的传输进度，以减少日志文件体积
}

// 【修复线程自阻塞】重构传输完成回调 - 避免在传输线程中直接析构任务对象
void CPortMasterDlg::OnTransmissionCompleted(const TransmissionResult& result)
{
	// 【调试】OnTransmissionCompleted 开始
	this->WriteLog("=== OnTransmissionCompleted 开始 ===");
	this->WriteLog("传输任务完成: 状态=" + std::to_string(static_cast<int>(result.finalState)) +
		", 错误码=" + std::to_string(static_cast<int>(result.errorCode)) +
		", 传输字节=" + std::to_string(result.bytesTransmitted) +
		", 耗时=" + std::to_string(result.duration.count()) + "ms");

	// 【关键修复】不在传输线程中直接析构任务对象，改为PostMessage到UI线程
	// 原因：当前回调在传输线程中执行，直接reset()会导致线程试图join自己，造成死锁

	// 通过PostMessage通知UI线程处理传输结果
	this->WriteLog("OnTransmissionCompleted: 准备发送 WM_USER + 13 消息");
	PostMessage(WM_USER + 13, (WPARAM)result.errorCode, 0);
	this->WriteLog("OnTransmissionCompleted: WM_USER + 13 消息已发送");

	// 【新增】通过PostMessage安全地清理传输任务对象（在UI线程中执行）
	this->WriteLog("OnTransmissionCompleted: 准备发送 WM_USER + 15 消息");
	PostMessage(WM_USER + 15, 0, 0);
	this->WriteLog("OnTransmissionCompleted: WM_USER + 15 消息已发送");

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

	this->WriteLog("=== OnTransmissionCompleted 结束 ===");
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
		if (m_uiController)
		{
			m_uiController->SetReceiveDataText(_T(""));
		}

		// 【阶段3迁移】清除接收缓存
		if (m_receiveCacheService)
		{
			m_receiveCacheService->Shutdown();
			m_totalReceivedBytes = 0;
			if (!m_receiveCacheService->Initialize())
			{
				WriteLog("清空接收数据后重新初始化接收缓存失败");
			}
		}

		// 重置接收统计信息
		m_bytesReceived = 0;
		if (m_uiController)
		{
			m_uiController->SetStaticText(IDC_STATIC_RECEIVED, _T("0"));
		}

		// 注意：发送窗口数据和发送统计信息保持不变
		// 发送窗口数据仅能通过用户主动点击"清空发送框"按钮进行清除

		this->WriteLog("已清除接收缓存数据，发送窗口数据保持不变");
	}
	catch (const std::exception& e)
	{
		this->WriteLog("清除缓存数据时发生异常: " + std::string(e.what()));
	}
}

void CPortMasterDlg::OnBnClickedButtonStop()
{
	// 【调试】停止按钮点击开始
	this->WriteLog("=== OnBnClickedButtonStop 开始 ===");

	if (m_eventDispatcher)
	{
		this->WriteLog("OnBnClickedButtonStop: 调用 m_eventDispatcher->HandleStop()");
		m_eventDispatcher->HandleStop();
		this->WriteLog("OnBnClickedButtonStop: m_eventDispatcher->HandleStop() 完成");
	}
	else
	{
		this->WriteLog("OnBnClickedButtonStop: m_eventDispatcher 为空");
	}

	this->WriteLog("=== OnBnClickedButtonStop 结束 ===");
}

void CPortMasterDlg::OnBnClickedButtonFile()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleSelectFile();
	}
}

void CPortMasterDlg::OnCbnSelchangeComboPortType()
{
	// 【阶段2迁移】使用PortConfigPresenter处理端口类型切换
	if (m_portConfigPresenter)
	{
		m_portConfigPresenter->OnPortTypeChanged();
	}
}

// 【阶段2迁移】UpdatePortParameters已迁移到PortConfigPresenter

void CPortMasterDlg::OnBnClickedCheckHex()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleToggleHex();
	}
}

// 基于缓存的格式转换函数实现

void CPortMasterDlg::UpdateSendDisplayFromCache()
{
	// 使用DataPresentationService准备显示数据
	if (m_sendCacheValid && !m_sendDataCache.empty())
	{
		bool hexMode = m_uiController->IsHexDisplayEnabled();
		DataPresentationService::DisplayUpdate update =
			DataPresentationService::PrepareDisplay(m_sendDataCache, hexMode);

		// 转换为CString并设置到编辑框
		CString displayText(update.content.c_str(), static_cast<int>(update.content.length()));
		if (m_uiController)
		{
			m_uiController->SetSendDataText(displayText);
		}
	}
	else
	{
		// 缓存无效或为空，清空显示
		if (m_uiController)
		{
			m_uiController->SetSendDataText(_T(""));
		}
	}
}
void CPortMasterDlg::UpdateReceiveDisplayFromCache()
{
	// 使用DataPresentationService准备显示数据
	if (m_receiveCacheValid && !m_receiveDataCache.empty())
	{
		bool hexMode = m_uiController->IsHexDisplayEnabled();
		DataPresentationService::DisplayUpdate update =
			DataPresentationService::PrepareDisplay(m_receiveDataCache, hexMode);

		// 转换为CString并设置到编辑框
		CString displayText(update.content.c_str(), static_cast<int>(update.content.length()));
		if (m_uiController)
		{
			m_uiController->SetReceiveDataText(displayText);
		}
	}
	else
	{
		// 缓存无效或为空，清空显示
		if (m_uiController)
		{
			m_uiController->SetReceiveDataText(_T(""));
		}
	}
}
void CPortMasterDlg::ThrottledUpdateReceiveDisplay()
{
	// 【阶段1迁移】使用DialogUiController管理节流机制
	if (!m_uiController)
		return;

	// 检查是否可以立即更新显示
	if (!m_uiController->CanUpdateDisplay())
	{
		// 如果在节流间隔内，设置待处理标志并启动定时器
		if (!m_uiController->IsDisplayUpdatePending())
		{
			m_uiController->SetDisplayUpdatePending(true);
			m_uiController->StartThrottledDisplayTimer();
		}
		// 如果已经有待处理的更新，不需要重复设置定时器
	}
	else
	{
		// 超过节流间隔，立即更新显示
		UpdateReceiveDisplayFromCache();
		m_uiController->RecordDisplayUpdate();
		m_uiController->SetDisplayUpdatePending(false);

		// 确保定时器被取消（以防万一）
		m_uiController->StopThrottledDisplayTimer();
	}
}

void CPortMasterDlg::UpdateSendCache(const CString& data)
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
			// 对于非ASCII字符，使用StringUtils进行安全转换（统一代码风格）
			std::wstring singleCharStr(1, ch);
			std::string utf8Str = StringUtils::Utf8EncodeWide(singleCharStr);

			for (size_t j = 0; j < utf8Str.length(); j++)
			{
				m_sendDataCache.push_back(static_cast<uint8_t>(utf8Str[j]));
			}
		}
	}

	this->WriteLog("=== UpdateSendCache 结束 ===");
	m_sendCacheValid = true;
}

void CPortMasterDlg::UpdateSendCacheFromBytes(const BYTE* data, size_t length)
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

void CPortMasterDlg::UpdateSendCacheFromHex(const CString& hexData)
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

void CPortMasterDlg::UpdateReceiveCache(const std::vector<uint8_t>& data)
{
	// 使用ReceiveCacheService替代手动缓存管理
	if (m_receiveCacheService)
	{
		try
		{
			m_receiveCacheService->AppendData(data);

			if (m_receiveVerboseLogging)
			{
				WriteLog("UpdateReceiveCache通过ReceiveCacheService追加: " + std::to_string(data.size()) + " 字节");
			}
		}
		catch (const std::exception& e)
		{
			WriteLog("UpdateReceiveCache失败: " + std::string(e.what()));
		}
	}
}

void CPortMasterDlg::OnBnClickedRadioReliable()
{
	// 【分类6修复】模式切换时自动执行"断开→重连"并刷新UI
	if (m_isConnected)
	{
		WriteLog("OnBnClickedRadioReliable: 检测到已连接，执行自动断开→重连");

		// 自动断开当前连接
		if (m_eventDispatcher)
		{
			m_eventDispatcher->HandleDisconnect();
			WriteLog("OnBnClickedRadioReliable: 已执行断开操作");
		}

		// 短暂延迟以确保断开完成
		Sleep(500);

		// 自动重新连接
		if (m_eventDispatcher)
		{
			m_eventDispatcher->HandleConnect();
			WriteLog("OnBnClickedRadioReliable: 已执行重新连接");
		}
	}
	else
	{
		// 未连接时，仅更新模式提示
		WriteLog("OnBnClickedRadioReliable: 未连接状态，仅更新模式");
	}

	// 更新状态条模式信息并刷新UI
	if (m_uiController)
	{
		m_uiController->SetModeText(_T("可靠"));
	}
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
	if (m_uiController)
	{
		currentData = m_uiController->GetSendDataText();
	}
	else
	{
		currentData = _T("");
	}

	if (!currentData.IsEmpty())
	{
		// 根据当前模式更新缓存
		if (m_uiController->IsHexDisplayEnabled())
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
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleClearSend();
	}
}

void CPortMasterDlg::OnBnClickedButtonClearReceive()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleClearReceive();
	}
}

void CPortMasterDlg::OnBnClickedButtonCopyAll()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleCopyAll();
	}
}

void CPortMasterDlg::OnBnClickedButtonSaveAll()
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleSaveAll();
	}
}

void CPortMasterDlg::SaveDataToFile(const CString& filePath, const CString& data)
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

			// 【阶段4迁移】使用StatusDisplayManager更新日志
			CTime time = CTime::GetCurrentTime();
			CString timeStr = time.Format(_T("[%H:%M:%S] "));
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			CString logMessage;
			logMessage.Format(_T("文本数据保存至: %s (%d 字节)"), fileName.GetString(), utf8Length);
			if (m_statusDisplayManager)
			{
				m_statusDisplayManager->LogMessage(timeStr + logMessage);
			}
		}
		else
		{
			this->WriteLog("文件打开失败");
			MessageBox(_T("保存文件失败"), _T("错误"), MB_OK | MB_ICONERROR);
		}
	}
	catch (const std::exception& e)
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

void CPortMasterDlg::SaveBinaryDataToFile(const CString& filePath, const std::vector<uint8_t>& data)
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

			// 【阶段4迁移】使用StatusDisplayManager更新日志
			CTime time = CTime::GetCurrentTime();
			CString timeStr = time.Format(_T("[%H:%M:%S] "));
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			CString logMessage;
			logMessage.Format(_T("原始数据保存至: %s (%zu 字节)"), fileName.GetString(), data.size());
			if (m_statusDisplayManager)
			{
				m_statusDisplayManager->LogMessage(timeStr + logMessage);
			}
		}
		else
		{
			this->WriteLog("文件打开失败");
			MessageBox(_T("保存文件失败"), _T("错误"), MB_OK | MB_ICONERROR);
		}
	}
	catch (const std::exception& e)
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
	// 【分类6修复】模式切换时自动执行"断开→重连"并刷新UI
	if (m_isConnected)
	{
		WriteLog("OnBnClickedRadioDirect: 检测到已连接，执行自动断开→重连");

		// 自动断开当前连接
		if (m_eventDispatcher)
		{
			m_eventDispatcher->HandleDisconnect();
			WriteLog("OnBnClickedRadioDirect: 已执行断开操作");
		}

		// 短暂延迟以确保断开完成
		Sleep(500);

		// 自动重新连接
		if (m_eventDispatcher)
		{
			m_eventDispatcher->HandleConnect();
			WriteLog("OnBnClickedRadioDirect: 已执行重新连接");
		}
	}
	else
	{
		// 未连接时，仅更新模式提示
		WriteLog("OnBnClickedRadioDirect: 未连接状态，仅更新模式");
	}

	// 更新状态条模式信息并刷新UI
	if (m_uiController)
	{
		m_uiController->SetModeText(_T("直通"));
	}
}

// Transport层集成实现

void CPortMasterDlg::InitializeTransportConfig()
{
	// 初始化串口配置
	m_transportConfig.portName = "COM1";
	m_transportConfig.readTimeout = 1000;
	m_transportConfig.writeTimeout = 1000;
	m_transportConfig.bufferSize = 4096;

	// 【第二轮阶段B修复】扩展超时窗口，避免心跳线程误判连接超时
	m_reliableConfig.maxRetries = 3;
	m_reliableConfig.timeoutBase = 5000;	// 基础超时时间
	m_reliableConfig.timeoutMax = 15000;	// 【修复】最大超时时间扩展至15秒
	m_reliableConfig.maxPayloadSize = 1024; // 最大负载大小
	m_reliableConfig.windowSize = 32;		// 窗口大小 - 增加窗口大小以支持大量数据传输
	m_reliableConfig.heartbeatInterval = 1000; // 心跳间隔保持1秒
}

// 【阶段A修复】从UI构建传输配置
void CPortMasterDlg::BuildTransportConfigFromUI()
{
	WriteLog("BuildTransportConfigFromUI: 开始从UI读取配置");

	// 1. 调用DialogConfigBinder保存UI配置到ConfigStore
	if (m_configBinder)
	{
		if (m_configBinder->SaveFromUI())
		{
			WriteLog("BuildTransportConfigFromUI: DialogConfigBinder::SaveFromUI() 成功");
		}
		else
		{
			WriteLog("BuildTransportConfigFromUI: 警告 - DialogConfigBinder::SaveFromUI() 失败");
		}
	}

	// 2. 获取UI选择的端口类型
	if (!m_portConfigPresenter)
	{
		WriteLog("BuildTransportConfigFromUI: 错误 - PortConfigPresenter未初始化");
		// 回退到默认串口配置
		InitializeTransportConfig();
		return;
	}

	PortTypeIndex portTypeIndex = m_portConfigPresenter->GetSelectedPortType();
	WriteLog("BuildTransportConfigFromUI: UI选择的端口类型索引 = " + std::to_string(static_cast<int>(portTypeIndex)));

	// 3. 根据端口类型填充TransportConfig
	switch (portTypeIndex)
	{
	case PortTypeIndex::Serial:
	{
		m_transportConfig.portType = PortType::PORT_TYPE_SERIAL;
		m_transportConfig.portName = m_portConfigPresenter->GetSelectedPort();
		// 从ConfigStore读取串口配置
		SerialConfig serialCfg = m_configStore.GetSerialConfig();
		m_transportConfig.baudRate = serialCfg.baudRate;
		m_transportConfig.dataBits = serialCfg.dataBits;
		m_transportConfig.parity = serialCfg.parity;
		m_transportConfig.stopBits = serialCfg.stopBits;

		WriteLog("BuildTransportConfigFromUI: 串口配置 - 端口=" + m_transportConfig.portName +
			", 波特率=" + std::to_string(serialCfg.baudRate));
		break;
	}
	case PortTypeIndex::Parallel:
	{
		m_transportConfig.portType = PortType::PORT_TYPE_PARALLEL;
		m_transportConfig.portName = m_portConfigPresenter->GetSelectedPort();
		WriteLog("BuildTransportConfigFromUI: 并口配置 - 端口=" + m_transportConfig.portName);
		break;
	}
	case PortTypeIndex::UsbPrint:
	{
		m_transportConfig.portType = PortType::PORT_TYPE_USB_PRINT;
		m_transportConfig.portName = m_portConfigPresenter->GetSelectedPort();
		WriteLog("BuildTransportConfigFromUI: USB打印配置 - 端口=" + m_transportConfig.portName);
		break;
	}
	case PortTypeIndex::NetworkPrint:
	{
		m_transportConfig.portType = PortType::PORT_TYPE_NETWORK_PRINT;
		// 网络打印配置从ConfigStore读取
		NetworkPrintConfig networkCfg = m_configStore.GetNetworkConfig();

		// 【分类7修复】验证hostname:port格式
		if (m_configBinder)
		{
			std::string errorMessage;
			if (!m_configBinder->ValidateNetworkConfig(networkCfg.hostname, std::to_string(networkCfg.port), errorMessage))
			{
				WriteLog("BuildTransportConfigFromUI: 网络配置验证失败 - " + errorMessage);
				// 触发错误通知（如果已设置错误回调）
				std::wstring wideError = StringUtils::WideEncodeUtf8(errorMessage);
				MessageBox((_T("网络配置无效: ") + wideError).c_str(), _T("配置错误"), MB_ICONERROR);
				// 恢复默认配置
				InitializeTransportConfig();
				return;
			}
		}

		m_transportConfig.portName = networkCfg.hostname + ":" + std::to_string(networkCfg.port);
		WriteLog("BuildTransportConfigFromUI: 网络打印配置 - 地址=" + m_transportConfig.portName);
		break;
	}
	case PortTypeIndex::Loopback:
	{
		m_transportConfig.portType = PortType::PORT_TYPE_LOOPBACK;
		m_transportConfig.portName = "Loopback";
		// 【分类6修复】从ConfigStore读取Loopback配置参数并完整传递
		LoopbackTestConfig loopbackCfg = m_configStore.GetLoopbackConfig();

		// 初始化m_currentLoopbackConfig，将LoopbackTestConfig参数映射到LoopbackConfig
		m_currentLoopbackConfig.portType = PortType::PORT_TYPE_LOOPBACK;
		m_currentLoopbackConfig.portName = "Loopback";
		m_currentLoopbackConfig.readTimeout = 1000;
		m_currentLoopbackConfig.writeTimeout = 1000;
		m_currentLoopbackConfig.bufferSize = 4096;
		m_currentLoopbackConfig.asyncMode = true;

		// 传递Loopback特定参数
		m_currentLoopbackConfig.delayMs = loopbackCfg.delayMs;
		m_currentLoopbackConfig.errorRate = static_cast<uint32_t>(loopbackCfg.errorRate);
		m_currentLoopbackConfig.packetLossRate = static_cast<uint32_t>(loopbackCfg.packetLossRate);
		m_currentLoopbackConfig.enableJitter = loopbackCfg.enableJitter;
		m_currentLoopbackConfig.jitterMaxMs = static_cast<uint32_t>(loopbackCfg.jitterMaxMs);
		m_currentLoopbackConfig.maxQueueSize = static_cast<uint32_t>(loopbackCfg.maxQueueSize);
		m_currentLoopbackConfig.enableLogging = true;

		WriteLog("BuildTransportConfigFromUI: 回路测试配置完整传递 - maxQueueSize=" + std::to_string(m_currentLoopbackConfig.maxQueueSize) +
			", delayMs=" + std::to_string(m_currentLoopbackConfig.delayMs) + "ms, errorRate=" + std::to_string(m_currentLoopbackConfig.errorRate) + "%");
		break;
	}
	default:
	{
		WriteLog("BuildTransportConfigFromUI: 警告 - 未知端口类型，使用默认串口配置");
		InitializeTransportConfig();
		return;
	}
	}

	// 4. 设置通用配置参数
	m_transportConfig.readTimeout = 1000;
	m_transportConfig.writeTimeout = 1000;
	m_transportConfig.bufferSize = 4096;

	// 5. 同步可靠传输配置（保持第二轮修订的配置）
	m_reliableConfig.maxRetries = 3;
	m_reliableConfig.timeoutBase = 5000;
	m_reliableConfig.timeoutMax = 15000;
	m_reliableConfig.maxPayloadSize = 1024;
	m_reliableConfig.windowSize = 32;
	m_reliableConfig.heartbeatInterval = 1000;

	WriteLog("BuildTransportConfigFromUI: 配置构建完成 - portType=" + std::to_string(static_cast<int>(m_transportConfig.portType)) +
		", portName=" + m_transportConfig.portName);
}

void CPortMasterDlg::UpdateConnectionStatus()
{
	// 【阶段C修复】若模式切换标志为真，保持"模式已切换，请重新连接"状态
	if (m_requiresReconnect)
	{
		if (m_uiController)
		{
			m_uiController->SetStatusText(_T("模式已切换，请重新连接"));
			// 禁用保存按钮，避免错用旧连接
			m_uiController->UpdateSaveButton(false);
		}
		return;
	}

	// 【阶段1迁移】使用DialogUiController更新连接状态
	if (m_uiController)
	{
		// 获取当前端口名称
		CString portName = GetCurrentPortName();

		// 传递端口名称和连接状态
		m_uiController->UpdateConnectionStatus(portName, m_isConnected);
		m_uiController->UpdateConnectionButtons(m_isConnected);
	}

	// 【可靠模式按钮管控】根据缺陷分析报告实施按钮状态控制
	UpdateSaveButtonStatus();
}

// 获取当前端口显示名称
CString CPortMasterDlg::GetCurrentPortName()
{
	// 优先从PortSessionController获取
	if (m_sessionController)
	{
		std::string portName = m_sessionController->GetCurrentPortName();
		if (!portName.empty())
		{
			return CString(portName.c_str());
		}

		// 如果端口名为空，尝试获取Transport类型名称
		std::string transportTypeName = m_sessionController->GetTransportTypeName();
		if (!transportTypeName.empty())
		{
			return CString(transportTypeName.c_str());
		}
	}

	return _T("未知端口");
}

// 【可靠模式按钮管控】更新保存按钮状态的专用函数
void CPortMasterDlg::UpdateSaveButtonStatus()
{
	bool enableSaveButton = true; // 默认启用保存按钮

	// 【阶段B修复】首先检查传输状态，传输进行中时强制禁用保存按钮
	if (m_isTransmitting)
	{
		enableSaveButton = false;
		this->WriteLog("传输进行中，禁用保存按钮");
	}
	else
	{
		// 【阶段B修复】优先依据ReceiveCacheService和内存缓存判断是否有数据可保存
		uint64_t receivedBytes = m_receiveCacheService ? m_receiveCacheService->GetTotalReceivedBytes() : 0;
		size_t memoryCacheSize = m_receiveDataCache.size();

		bool hasDataToSave = (receivedBytes > 0) || (memoryCacheSize > 0);

		if (hasDataToSave)
		{
			enableSaveButton = true;
			std::string dataSource = "检测到可保存数据，来源[";
			if (receivedBytes > 0)
			{
				dataSource += "接收服务:" + std::to_string(receivedBytes) + "B ";
			}
			if (memoryCacheSize > 0)
			{
				dataSource += "内存缓存:" + std::to_string(memoryCacheSize) + "B ";
			}
			dataSource += "]";
			this->WriteLog(dataSource);
		}
		else
		{
			enableSaveButton = false;
			this->WriteLog("无可保存数据，禁用保存按钮");
		}
	}

	if (m_uiController)
	{
		m_uiController->UpdateSaveButton(enableSaveButton);
	}
}

void CPortMasterDlg::UpdateStatistics()
{
	// 【阶段4迁移】使用StatusDisplayManager更新统计信息
	if (m_statusDisplayManager)
	{
		// 获取当前传输进度百分比
		int progressPercent = GetCurrentProgressPercent();
		m_statusDisplayManager->UpdateProgressDisplay(progressPercent);

		// 仍然更新其他统计信息（可选）
		// m_statusDisplayManager->UpdateAllStatistics(m_bytesSent, m_bytesReceived, m_sendSpeed, m_receiveSpeed);
	}
}

// 获取当前传输进度百分比
int CPortMasterDlg::GetCurrentProgressPercent()
{
	// 方式1：从进度条控件获取
	CProgressCtrl* progressCtrl = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS);
	if (progressCtrl && progressCtrl->GetSafeHwnd())
	{
		return progressCtrl->GetPos();
	}

	return 0;
}

void CPortMasterDlg::OnTransportDataReceived(const std::vector<uint8_t>& data)
{
	// 【深层修复】根据最新检查报告最优解方案：接收线程直接落盘，避免消息队列异步延迟
	auto logReceiveDetail = [&](const std::string& message)
		{
			if (m_receiveVerboseLogging)
			{
				this->WriteLog(message);
			}
		};

	logReceiveDetail("=== OnTransportDataReceived 开始（直接落盘模式）===");
	logReceiveDetail("接收到数据大小: " + std::to_string(data.size()) + " 字节");

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
	logReceiveDetail("接收数据预览(十六进制): " + hexPreview);

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
	logReceiveDetail("接收数据预览(ASCII): " + asciiPreview);

	// 【阶段3迁移】使用ReceiveCacheService替代旧缓存函数
	if (m_receiveCacheService)
	{
		try
		{
			m_receiveCacheService->AppendData(data);
			m_totalReceivedBytes = m_receiveCacheService->GetTotalReceivedBytes();

			// 【第七轮修复】删除内存快照同步 - m_memoryCache已删除，仅依赖文件缓存
			// m_receiveDataCache = m_receiveCacheService->GetMemoryCache();  // 已删除
			// m_receiveCacheValid = !m_receiveDataCache.empty();  // 已删除
			// UI预览仍显示前32KB，统计字节数直接使用 GetTotalReceivedBytes()
			m_bytesReceived = m_totalReceivedBytes;

			if (m_receiveVerboseLogging)
			{
				WriteLog("ReceiveCacheService追加数据: " + std::to_string(data.size()) + " 字节，总计: " + std::to_string(m_totalReceivedBytes) + " 字节");
				// 日志信息已简化 - 不再输出内存快照信息
			}
		}
		catch (const std::exception& e)
		{
			WriteLog("ReceiveCacheService追加数据失败: " + std::string(e.what()));
		}
	}

	// 【轻量化UI更新】仅发送统计信息，不传输实际数据，避免消息队列堆积
	struct UIUpdateInfo {
		size_t dataSize;
		std::string hexPreview;
		std::string asciiPreview;
	};
	UIUpdateInfo* updateInfo = new UIUpdateInfo{ data.size(), hexPreview, asciiPreview };
	PostMessage(WM_USER + 1, 0, (LPARAM)updateInfo);

	logReceiveDetail("=== OnTransportDataReceived 结束（数据已直接落盘）===");
}

void CPortMasterDlg::OnTransportError(const std::string& error)
{
	// 【阶段B修复】正确处理UTF-8到Unicode编码转换，解决状态栏乱码问题
	// 使用StringUtils替代MFC宏，消除内存风险
	std::wstring errorMsgW = StringUtils::WideEncodeUtf8(error);
	CString errorMsg(errorMsgW.c_str());
	PostMessage(WM_USER + 2, 0, (LPARAM) new CString(errorMsg));
}

void CPortMasterDlg::ConfigureReliableLogging()
{
	if (!m_reliableChannel)
	{
		return;
	}

	const AppConfig& appConfig = m_configStore.GetAppConfig();
	bool enableVerbose = (appConfig.logLevel >= 3);
	m_reliableChannel->SetVerboseLoggingEnabled(enableVerbose);

	this->WriteLog(std::string("ReliableChannel 日志级别调整：") +
		(enableVerbose ? "开启 Verbose 输出" : "关闭 Verbose 输出"));
}

void CPortMasterDlg::OnReliableProgress(int64_t current, int64_t total)
{
	uint32_t percent = 0;
	if (total > 0)
	{
		percent = static_cast<uint32_t>((current * 100) / total);
		if (percent > 100)
		{
			percent = 100;
		}
	}

	// 【修复进度条闪烁】改用消息队列投递进度更新，确保在UI线程执行
	// 避免非UI线程直接操作控件导致的绘制竞争
	PostMessage(WM_USER + 11, 0, static_cast<LPARAM>(percent));

	// 【新增】更新进度百分比显示
	if (m_statusDisplayManager)
	{
		m_statusDisplayManager->UpdateProgressDisplay(static_cast<int>(percent));
	}

	std::lock_guard<std::mutex> lock(m_reliableSessionMutex);
	m_reliableExpectedBytes = total;
	m_reliableReceivedBytes = current;
}

void CPortMasterDlg::OnReliableComplete(bool success)
{
	m_isTransmitting = false;

	// 【修复】传输完成时强制文件同步，确保所有缓冲数据写入磁盘
	// 解决文件系统多层缓存（C++流缓冲 → OS缓冲 → 页面缓存 → 磁盘）导致的保存不完整问题
	{
		std::lock_guard<std::mutex> lock(m_receiveFileMutex);
		if (m_tempCacheFile.is_open())
		{
			// 刷新C++流缓冲到操作系统缓冲区
			m_tempCacheFile.flush();
			WriteLog("OnReliableComplete: 强制文件同步完成，刷新C++流缓冲到OS缓冲区");
		}
	}

	if (success)
	{
		// 传输完成 - 显示传输完成消息
		CString completeStatus;
		completeStatus.Format(_T("传输完成"));
		if (m_uiController)
		{
			m_uiController->SetStatusText(completeStatus);
			// 【第二轮修复】禁用停止按钮，启用发送和文件按钮
			m_uiController->UpdateTransmissionButtons(false, false);
		}
		SetProgressPercent(100);

		// 2秒后恢复连接状态显示，但保持进度条100%状态
		SetTimer(TIMER_ID_CONNECTION_STATUS, 2000, NULL);

		// 【阶段3迁移】立即检查接收数据大小，快速启用保存按钮
		uint64_t tempFileSize = m_receiveCacheService ? m_receiveCacheService->GetTotalReceivedBytes() : 0;
		if (tempFileSize > 0)
		{
			WriteLog("OnReliableComplete: 检测到临时文件有数据(" +
				std::to_string(tempFileSize) + "字节)，立即启用保存按钮");
			if (m_uiController)
			{
				m_uiController->UpdateSaveButton(true);
			}
		}
		else
		{
			// 文件为空，可能数据尚未落盘，先更新状态
			WriteLog("OnReliableComplete: 临时文件为空，调用UpdateSaveButtonStatus()");
			UpdateSaveButtonStatus();
		}
	}
	else
	{
		// 传输失败 - 显示错误消息
		CString errorStatus;
		errorStatus.Format(_T("传输失败"));
		if (m_uiController)
		{
			m_uiController->SetStatusText(errorStatus);
			// 【第二轮修复】禁用停止按钮，启用发送和文件按钮
			m_uiController->UpdateTransmissionButtons(false, false);
		}
		SetProgressPercent(0, true);

		MessageBox(_T("传输失败"), _T("错误"), MB_OK | MB_ICONERROR);

		// 失败时也更新保存按钮状态
		UpdateSaveButtonStatus();
	}
}

void CPortMasterDlg::OnReliableStateChanged(bool connected)
{
	// 连接状态变化回调 - 只处理连接状态，不显示传输完成消息
	if (connected)
	{
		// 连接成功 - 更新状态显示
		if (m_uiController)
		{
			m_uiController->SetStatusText(_T("已连接"));
		}
	}
	else
	{
		// 连接断开 - 更新状态显示
		if (m_uiController)
		{
			m_uiController->SetStatusText(_T("连接断开"));
		}
		SetProgressPercent(0, true);
	}
}

// 自定义消息处理实现

LRESULT CPortMasterDlg::OnTransportDataReceivedMessage(WPARAM wParam, LPARAM lParam)
{
	// 【深层修复】处理新的轻量级UI更新信息结构
	UIUpdateInfo* updateInfo = reinterpret_cast<UIUpdateInfo*>(lParam);
	if (updateInfo)
	{
		try
		{
			const DWORD kUiLogThrottleMs = 1000;  // UI日志默认一秒输出一次
			ULONGLONG nowTick = ::GetTickCount64();
			bool shouldLog = m_receiveVerboseLogging;

			if (!shouldLog)
			{
				// 数据量大或超过节流间隔时输出一次，避免日志持续膨胀
				if (updateInfo->dataSize >= 64 * 1024)
				{
					shouldLog = true;
				}
				else if (nowTick - m_lastUiLogTick >= kUiLogThrottleMs)
				{
					shouldLog = true;
				}
			}

			if (shouldLog)
			{
				m_lastUiLogTick = nowTick;
				this->WriteLog("=== 轻量级UI更新信息 ===");
				this->WriteLog("接收数据大小: " + std::to_string(updateInfo->dataSize) + " 字节");
				this->WriteLog("十六进制预览: " + updateInfo->hexPreview);
				this->WriteLog("ASCII预览: " + updateInfo->asciiPreview);
			}

			// 【阶段A修复】直接使用OnTransportDataReceived中已更新的m_bytesReceived快照值
			// 移除重复累加逻辑，避免显示与统计滞后
			CString receivedText;
			receivedText.Format(_T("%llu"), static_cast<unsigned long long>(m_bytesReceived));
			if (m_uiController)
			{
				m_uiController->SetStaticText(IDC_STATIC_RECEIVED, receivedText);
			}

			// 【UI优化】使用节流机制更新显示，避免大文件传输时频繁重绘
			// 注意：数据已经由ReceiveCacheService直接落盘和更新内存缓存
			ThrottledUpdateReceiveDisplay();

			if (shouldLog)
			{
				this->WriteLog("=== 轻量级UI更新完成 ===");
			}
		}
		catch (const std::exception& e)
		{
			CString errorMsg;
			CA2T convertedMsg(e.what());
		errorMsg.Format(_T("处理轻量级UI更新失败: %s"), (LPCTSTR)convertedMsg);
			MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
		}

		delete updateInfo;
	}

	return 0;
}

LRESULT CPortMasterDlg::OnTransportErrorMessage(WPARAM wParam, LPARAM lParam)
{
	CString* errorMsg = reinterpret_cast<CString*>(lParam);
	if (errorMsg)
	{
		MessageBox(*errorMsg, _T("传输错误"), MB_OK | MB_ICONERROR);
		delete errorMsg;
	}

	return 0;
}

void CPortMasterDlg::SetProgressPercent(int percent, bool forceReset)
{
	// 【阶段1迁移】使用DialogUiController管理进度条
	if (m_uiController)
	{
		m_uiController->SetProgressPercent(percent, forceReset);
	}
}

// 新增：传输进度条范围设置消息处理
LRESULT CPortMasterDlg::OnTransmissionProgressRange(WPARAM wParam, LPARAM lParam)
{
	if (m_statusDisplayManager)
	{
		m_statusDisplayManager->SetProgressRange(0, (int)lParam);
	}
	return 0;
}

// 新增：传输进度条更新消息处理
LRESULT CPortMasterDlg::OnTransmissionProgressUpdate(WPARAM wParam, LPARAM lParam)
{
	SetProgressPercent(static_cast<int>(lParam));
	return 0;
}

// 新增：传输状态更新消息处理
LRESULT CPortMasterDlg::OnTransmissionStatusUpdate(WPARAM wParam, LPARAM lParam)
{
	CString* statusText = reinterpret_cast<CString*>(lParam);
	if (statusText)
	{
		// ✅ 修复：将传输进度信息显示到传输进度栏（IDC_STATIC_SPEED）而非端口栏
		if (m_isTransmitting)
		{
			if (m_uiController)
			{
				m_uiController->SetStaticText(IDC_STATIC_SPEED, *statusText);
			}
		}
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
		if (m_uiController)
		{
			m_uiController->SetStatusText(_T("传输已终止"));
			// 【阶段三修复】统一使用状态机驱动UI更新，Idle状态自动设置按钮文本为"发送"
			m_uiController->ApplyTransmissionState(TransmissionUiState::Idle);
		}
		SetProgressPercent(0, true);

		// ✅ 重置传输进度显示
		if (m_uiController)
		{
			m_uiController->SetStaticText(IDC_STATIC_SPEED, _T(""));
		}
	}
	else if (error != TransportError::Success)
	{
		// 传输失败
		m_isTransmitting = false;
		m_transmissionPaused = false;
		if (m_uiController)
		{
			// 【阶段三修复】统一使用状态机驱动UI更新，Idle状态自动设置按钮文本为"发送"
			m_uiController->ApplyTransmissionState(TransmissionUiState::Idle);
		}

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
		SetProgressPercent(0, true);

		// ✅ 重置传输进度显示
		if (m_uiController)
		{
			m_uiController->SetStaticText(IDC_STATIC_SPEED, _T(""));
		}

		// 恢复连接状态显示
		UpdateConnectionStatus();
	}
	else
	{
		// 传输成功
		m_isTransmitting = false;
		m_transmissionPaused = false;
		if (m_uiController)
		{
			// 【阶段三修复】统一使用状态机驱动UI更新，Idle状态自动设置按钮文本为"发送"
			m_uiController->ApplyTransmissionState(TransmissionUiState::Idle);
		}

		// 【阶段B修复】传输完成后立即更新保存按钮状态
		UpdateSaveButtonStatus();

		// 更新发送统计
		const std::vector<uint8_t>& data = m_sendDataCache;
		m_bytesSent += data.size();
		CString sentText;
		sentText.Format(_T("%llu"), static_cast<unsigned long long>(m_bytesSent));
		if (m_uiController)
		{
			m_uiController->SetStaticText(IDC_STATIC_SENT, sentText);
		}

		// 显示传输完成状态并设置进度条为100%
		CString completeStatus;
		completeStatus.Format(_T("传输完成: %u 字节"), data.size());
		if (m_uiController)
		{
			m_uiController->SetStatusText(completeStatus);
		}
		SetProgressPercent(100);

		// ✅ 传输成功后，延时2秒后清空传输进度显示
		// 先显示100%让用户看到完成状态，再清空
		// 【分类6.4优化】改用SetTimer异步处理，避免UI线程阻塞（Sleep会卡顿）
		SetTimer(3, 2000, nullptr);  // ID=3: 清空传输速度显示定时器

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

	// 【激进修复】暂时禁用CleanupTransmissionTask，避免程序闪退
	// 让TransmissionTask自然析构，但避免在UI线程中显式调用
	if (m_transmissionCoordinator)
	{
		this->WriteLog("=== 暂时跳过 CleanupTransmissionTask，避免程序闪退 ===");
		// m_transmissionCoordinator->CleanupTransmissionTask(); // 暂时注释掉
		this->WriteLog("跳过 CleanupTransmissionTask 调用");
	}

	this->WriteLog("=== OnTransmissionComplete 即将 return 0 ===");
	return 0;
}

// 新增：传输异常消息处理
LRESULT CPortMasterDlg::OnTransmissionError(WPARAM wParam, LPARAM lParam)
{
	m_isTransmitting = false;
	m_transmissionPaused = false;
	if (m_uiController)
	{
		m_uiController->SetSendButtonText(_T("发送"));
		// 【第二轮修复】禁用停止按钮，启用发送和文件按钮
		m_uiController->UpdateTransmissionButtons(false, false);
	}

	MessageBox(_T("传输过程中发生异常"), _T("错误"), MB_OK | MB_ICONERROR);
	return 0;
}

// 【Stage4迁移】传输任务清理消息处理 - 已由TransmissionCoordinator自动管理
LRESULT CPortMasterDlg::OnCleanupTransmissionTask(WPARAM wParam, LPARAM lParam)
{
	// 【Stage4迁移】TransmissionCoordinator自动管理任务生命周期，无需手动清理
	WriteLog("OnCleanupTransmissionTask - 传输任务由TransmissionCoordinator自动管理");

	return 0;
}

// 配置管理方法实现
void CPortMasterDlg::LoadConfigurationFromStore()
{
	// 使用DialogConfigBinder加载配置到UI控件
	if (m_configBinder && m_configBinder->LoadToUI())
	{
		// ✅ 修复：配置加载成功后同步模式显示文本，确保IDC_STATIC_MODE显示正确
		bool useReliableMode = m_configBinder->ReadTransmissionMode();
		if (m_uiController)
		{
			m_uiController->UpdateTransmissionMode(useReliableMode);
		}

		// 【阶段4迁移】配置加载成功，使用StatusDisplayManager显示日志
		if (m_statusDisplayManager)
		{
			m_statusDisplayManager->LogMessage(_T("配置加载成功"));
		}
	}
	else
	{
		// 配置加载失败
		MessageBox(_T("加载配置失败"), _T("配置错误"), MB_OK | MB_ICONWARNING);
	}
}

void CPortMasterDlg::SaveConfigurationToStore()
{
	// 使用DialogConfigBinder从UI控件保存配置
	if (m_configBinder && m_configBinder->SaveFromUI())
	{
		// 配置保存成功，触发配置变更回调
		OnConfigurationChanged();
		// 【阶段4迁移】配置保存成功，使用StatusDisplayManager显示日志
		if (m_statusDisplayManager)
		{
			m_statusDisplayManager->LogMessage(_T("配置保存成功"));
		}
	}
	else
	{
		// 配置保存失败
		MessageBox(_T("保存配置失败"), _T("配置错误"), MB_OK | MB_ICONWARNING);
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
	catch (const std::exception& e)
	{
		CString errorMsg;
		CA2T convertedMsg(e.what());
		errorMsg.Format(_T("应用配置变更失败: %s"), (LPCTSTR)convertedMsg);
		MessageBox(errorMsg, _T("配置错误"), MB_OK | MB_ICONWARNING);
	}
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
		}
		else {
			this->WriteLog("❌ 保存验证失败：期望 " + std::to_string(expectedSize) +
				" 字节，实际 " + std::to_string(actualSize) + " 字节");
			success = false;
		}
	}
	else {
		this->WriteLog("❌ 保存验证失败：无法获取文件大小");
		success = false;
	}

	CloseHandle(hFile);
	return success;
}

// 【阶段3迁移】ClearTempCacheFile和GetTempCacheFileSize已删除，使用ReceiveCacheService替代

// 【临时文件状态监控与自动恢复】新增机制实现

// 文件拖拽处理函数
void CPortMasterDlg::OnDropFiles(HDROP hDropInfo)
{
	if (m_eventDispatcher)
	{
		m_eventDispatcher->HandleDropFiles(hDropInfo);
	}

	CDialogEx::OnDropFiles(hDropInfo);
}