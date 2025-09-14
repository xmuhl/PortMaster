#include "pch.h"
#include "framework.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "Resource.h"
#include "PortConfigDialog.h"
#include "TestWizardDialog.h"
#include "Transport/TcpTransport.h"
#include "Transport/UdpTransport.h"
#include "Common/ConfigManager.h"
#include "Common/RAIIHandle.h"
#include "afxdialogex.h"
#include <fstream>
#include <filesystem>
#include <Shlwapi.h>
#include <algorithm>

#pragma comment(lib, "Shlwapi.lib")

extern void WriteDebugLog(const char* message);

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 用于应用程序"关于"菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

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

// CPortMasterDlg 对话框
CPortMasterDlg::CPortMasterDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PORTMASTER_DIALOG, pParent)
	, m_bConnected(false)
	, m_bReliableMode(false)
	, m_bHexDisplay(false)                      // 关键修复：初始化十六进制显示标志
	, m_bTransmitting(false)
	, m_transmissionState(TransmissionState::IDLE)  // 初始化传输状态为空闲
	, m_transmissionProgress(0)
	, m_transmissionTimer(0)
	, m_transmissionStartTime(0)
	, m_totalBytesTransmitted(0)
	, m_lastSpeedUpdateTime(0)
	, m_currentRetryCount(0)
	, m_maxRetryCount(3)  // 默认最多重试3次
	, m_lastProgressUpdate(std::chrono::steady_clock::now())  // 🔑 P1-4: 初始化回调频率限制时间戳
	, m_tempDataManager(std::make_unique<TempDataManager>())  // 初始化临时数据管理器
	, m_managerIntegration(ManagerIntegrationFactory::Create(this))  // 🔑 架构重构：初始化管理器集成器
{
	WriteDebugLog("[DEBUG] CPortMasterDlg::CPortMasterDlg: 主对话框构造函数开始");
	m_hIcon = AfxGetApp()->LoadIcon(IDI_MAIN_ICON);
	WriteDebugLog("[DEBUG] CPortMasterDlg::CPortMasterDlg: 主对话框构造函数完成");
}

void CPortMasterDlg::DoDataExchange(CDataExchange* pDX)
{
	WriteDebugLog("[DEBUG] DoDataExchange: 开始数据交换");
	CDialogEx::DoDataExchange(pDX);
	WriteDebugLog("[DEBUG] DoDataExchange: CDialogEx::DoDataExchange 完成");

	// 逐个绑定控件，并记录调试信息
	try {
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_PORT_TYPE");
		DDX_Control(pDX, IDC_PORT_TYPE, m_ctrlPortType);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_PORT_LIST");
		DDX_Control(pDX, IDC_PORT_LIST, m_ctrlPortList);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_BAUD_RATE");
		DDX_Control(pDX, IDC_BAUD_RATE, m_ctrlBaudRate);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_DATA_BITS");
		DDX_Control(pDX, IDC_DATA_BITS, m_ctrlDataBits);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_PARITY");
		DDX_Control(pDX, IDC_PARITY, m_ctrlParity);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_STOP_BITS");
		DDX_Control(pDX, IDC_STOP_BITS, m_ctrlStopBits);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_CONNECT_BUTTON");
		DDX_Control(pDX, IDC_CONNECT_BUTTON, m_ctrlConnectBtn);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_DISCONNECT_BUTTON");
		DDX_Control(pDX, IDC_DISCONNECT_BUTTON, m_ctrlDisconnectBtn);
		
		
		WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_SEND_BUTTON");
        DDX_Control(pDX, IDC_SEND_BUTTON, m_ctrlSendBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_STOP_BUTTON");
        DDX_Control(pDX, IDC_STOP_BUTTON, m_ctrlStopBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_CLEAR_INPUT_BUTTON");
        DDX_Control(pDX, IDC_CLEAR_INPUT_BUTTON, m_ctrlClearInputBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_CLEAR_DISPLAY_BUTTON");
        DDX_Control(pDX, IDC_CLEAR_DISPLAY_BUTTON, m_ctrlClearDisplayBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_LOAD_FILE_BUTTON");
        DDX_Control(pDX, IDC_LOAD_FILE_BUTTON, m_ctrlLoadFileBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_SAVE_FILE_BUTTON");
        DDX_Control(pDX, IDC_SAVE_FILE_BUTTON, m_ctrlSaveFileBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_COPY_BUTTON (统一复制按钮)");
        DDX_Control(pDX, IDC_COPY_BUTTON, m_ctrlCopyBtn);
        
        // IDC_COPY_HEX_BUTTON 和 IDC_COPY_TEXT_BUTTON 在.rc文件中不存在，已注释
        // WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_COPY_HEX_BUTTON");
        // DDX_Control(pDX, IDC_COPY_HEX_BUTTON, m_ctrlCopyHexBtn);
        
        // WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_COPY_TEXT_BUTTON");
        // DDX_Control(pDX, IDC_COPY_TEXT_BUTTON, m_ctrlCopyTextBtn);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_RELIABLE_MODE");
        DDX_Control(pDX, IDC_RELIABLE_MODE, m_ctrlReliableMode);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_INPUT_HEX");
        DDX_Control(pDX, IDC_INPUT_HEX, m_ctrlInputHex);
        
        
        // IDC_HEX_VIEW 和 IDC_TEXT_VIEW 在.rc文件中不存在，改为绑定实际存在的 IDC_DATA_VIEW
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_DATA_VIEW (统一数据显示控件)");
        DDX_Control(pDX, IDC_DATA_VIEW, m_ctrlDataView);
        
        // 可选：为了兼容性，也绑定 IDC_HEX_DISPLAY_CHECK
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_HEX_DISPLAY_CHECK");
        DDX_Control(pDX, IDC_HEX_DISPLAY_CHECK, m_ctrlHexDisplayCheck);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_LOG");
        DDX_Control(pDX, IDC_LOG, m_ctrlLog);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_PROGRESS");
        DDX_Control(pDX, IDC_PROGRESS, m_ctrlProgress);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_CONNECTION_STATUS");
        DDX_Control(pDX, IDC_CONNECTION_STATUS, m_ctrlConnectionStatus);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_PROTOCOL_STATUS");
        DDX_Control(pDX, IDC_PROTOCOL_STATUS, m_ctrlProtocolStatus);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_TRANSFER_STATS");
        DDX_Control(pDX, IDC_TRANSFER_STATS, m_ctrlTransferStatus);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_TRANSFER_SPEED");
        DDX_Control(pDX, IDC_TRANSFER_SPEED, m_ctrlTransferSpeed);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_TRANSFER_PROGRESS");
        DDX_Control(pDX, IDC_TRANSFER_PROGRESS, m_ctrlTransferProgress);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_STATUS");
        DDX_Control(pDX, IDC_STATUS, m_ctrlStatus);
        
        WriteDebugLog("[DEBUG] DoDataExchange: 绑定 IDC_DATA_SOURCE_LABEL");
        DDX_Control(pDX, IDC_DATA_SOURCE_LABEL, m_ctrlDataSourceLabel);
        
        // DDX_Control(pDX, IDC_TIME_REMAINING, m_ctrlTimeRemaining);
        // DDX_Control(pDX, IDC_CLEAR_BUTTON, m_ctrlClearBtn);
		
		WriteDebugLog("[DEBUG] DoDataExchange: 所有控件绑定完成");
	}
	catch (...) {
		WriteDebugLog("[ERROR] DoDataExchange: 控件绑定过程中出现异常");
		throw;
	}
}

BEGIN_MESSAGE_MAP(CPortMasterDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_CONNECT_BUTTON, &CPortMasterDlg::OnBnClickedConnect)
	ON_BN_CLICKED(IDC_DISCONNECT_BUTTON, &CPortMasterDlg::OnBnClickedDisconnect)
	ON_BN_CLICKED(IDC_SEND_BUTTON, &CPortMasterDlg::OnBnClickedSend)
	ON_BN_CLICKED(IDC_STOP_BUTTON, &CPortMasterDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_CLEAR_INPUT_BUTTON, &CPortMasterDlg::OnBnClickedClearInput)
	ON_BN_CLICKED(IDC_CLEAR_DISPLAY_BUTTON, &CPortMasterDlg::OnBnClickedClearDisplay)
	ON_BN_CLICKED(IDC_CLEAR_BUTTON, &CPortMasterDlg::OnBnClickedClear)
	ON_BN_CLICKED(IDC_LOAD_FILE_BUTTON, &CPortMasterDlg::OnBnClickedLoadFile)
	ON_BN_CLICKED(IDC_SAVE_FILE_BUTTON, &CPortMasterDlg::OnBnClickedSaveFile)
	ON_BN_CLICKED(IDC_COPY_BUTTON, &CPortMasterDlg::OnBnClickedCopy)
	// IDC_COPY_HEX_BUTTON 和 IDC_COPY_TEXT_BUTTON 在.rc文件中不存在，已注释
	// ON_BN_CLICKED(IDC_COPY_HEX_BUTTON, &CPortMasterDlg::OnBnClickedCopyHex)
	// ON_BN_CLICKED(IDC_COPY_TEXT_BUTTON, &CPortMasterDlg::OnBnClickedCopyText)
	// 添加实际存在的控件消息映射
	ON_BN_CLICKED(IDC_HEX_DISPLAY_CHECK, &CPortMasterDlg::OnBnClickedHexDisplay)
	ON_CBN_SELCHANGE(IDC_PORT_TYPE, &CPortMasterDlg::OnCbnSelchangePortType)
	ON_BN_CLICKED(IDC_RELIABLE_MODE, &CPortMasterDlg::OnBnClickedReliableMode)
	ON_WM_DROPFILES()
	ON_WM_SIZE()
	ON_WM_TIMER()
	
	// 线程安全UI更新消息映射
	ON_MESSAGE(WM_UPDATE_PROGRESS, &CPortMasterDlg::OnUpdateProgress)
	ON_MESSAGE(WM_UPDATE_COMPLETION, &CPortMasterDlg::OnUpdateCompletion)
	ON_MESSAGE(WM_UPDATE_FILE_RECEIVED, &CPortMasterDlg::OnUpdateFileReceived)
	ON_MESSAGE(WM_DISPLAY_RECEIVED_DATA, &CPortMasterDlg::OnDisplayReceivedDataMsg)
END_MESSAGE_MAP()

// CPortMasterDlg 消息处理程序

BOOL CPortMasterDlg::OnInitDialog()
{
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 开始初始化主对话框");
	
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤1 - 调用CDialogEx::OnInitDialog");
	
	// 现在应该可以安全调用，因为已经修复了DDX绑定和资源定义
	if (!CDialogEx::OnInitDialog()) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: CDialogEx::OnInitDialog失败");
		return FALSE;
	}
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: CDialogEx::OnInitDialog成功完成");
	
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤2 - 开始初始化复杂对象");
	try {
		// 现在在控件已经正确绑定后初始化复杂对象
		InitializeTransportObjects();
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 复杂对象初始化完成");
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 复杂对象初始化失败");
		return FALSE;
	}

	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3 - 开始设置系统菜单");
	// 将"关于..."菜单项添加到系统菜单中。
	// IDM_ABOUTBOX 必须在系统命令范围内。
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.1 - 检查IDM_ABOUTBOX常量");
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.2 - IDM_ABOUTBOX常量检查完成");

	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.3 - 获取系统菜单");
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.4 - 系统菜单获取完成");
	if (pSysMenu != nullptr)
	{
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.5 - 系统菜单有效，开始加载字符串");
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.6 - 字符串加载完成");
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.7 - 添加菜单项");
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
			WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3.8 - 菜单项添加完成");
		}
	}
	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 步骤3 - 系统菜单设置完成");

	try {
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 设置对话框图标");
		// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
		//  执行此操作
		SetIcon(m_hIcon, TRUE); // 设置大图标
		SetIcon(m_hIcon, FALSE); // 设置小图标
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 对话框图标设置完成");
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 设置图标异常");
	}

	try {
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 开始初始化控件");
		// 初始化控件
		InitializeControls();
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 控件初始化完成");
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 初始化控件异常");
		return FALSE;
	}

	// 🔑 架构重构：初始化管理器集成器
	try {
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 开始初始化管理器集成器");
		if (m_managerIntegration && m_managerIntegration->Initialize()) {
			// 设置UI控件到管理器
			m_managerIntegration->SetUIControls(&m_ctrlDataView, &m_ctrlProgress, 
												&m_ctrlStatus, &m_ctrlConnectionStatus);
			WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 管理器集成器初始化完成");
		}
		else {
			WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 管理器集成器初始化失败");
		}
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 管理器集成器初始化异常");
	}

	try {
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 启用拖放功能");
		// 启用拖放
		DragAcceptFiles(TRUE);
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 拖放功能启用完成");
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 启用拖放功能异常");
	}

	try {
		WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 窗口初始化完成");
		// TODO: 实现窗口大小自适应功能
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::OnInitDialog: 窗口初始化异常");
	}

	WriteDebugLog("[DEBUG] PortMasterDlg::OnInitDialog: 主对话框初始化完成");
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
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
//  来绘制该图标。对于文档/视图模型的 MFC 应用程序，
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

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CPortMasterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CPortMasterDlg::InitializeControls()
{
	WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 开始初始化控件");
	
	try {
		// 初始化端口类型下拉框 - 实现所有设计功能
		m_ctrlPortType.AddString(L"串口");
		m_ctrlPortType.AddString(L"并口");
		m_ctrlPortType.AddString(L"USB打印机");
		m_ctrlPortType.AddString(L"TCP客户端");
		m_ctrlPortType.AddString(L"TCP服务器");
		m_ctrlPortType.AddString(L"UDP");
		m_ctrlPortType.AddString(L"回环测试");
		
		m_ctrlPortType.SetCurSel(0); // 默认选择串口
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 端口类型下拉框初始化完成");

		// 初始化波特率下拉框
		m_ctrlBaudRate.AddString(L"9600");
		m_ctrlBaudRate.AddString(L"19200");
		m_ctrlBaudRate.AddString(L"38400");
		m_ctrlBaudRate.AddString(L"57600");
		m_ctrlBaudRate.AddString(L"115200");
		m_ctrlBaudRate.SetCurSel(0);

		// 初始化数据位下拉框
		m_ctrlDataBits.AddString(L"5");
		m_ctrlDataBits.AddString(L"6");
		m_ctrlDataBits.AddString(L"7");
		m_ctrlDataBits.AddString(L"8");
		m_ctrlDataBits.SetCurSel(3);

		// 初始化停止位下拉框
		m_ctrlStopBits.AddString(L"1");
		m_ctrlStopBits.AddString(L"1.5");
		m_ctrlStopBits.AddString(L"2");
		m_ctrlStopBits.SetCurSel(0);

		// 初始化校验位下拉框
		m_ctrlParity.AddString(L"None");
		m_ctrlParity.AddString(L"Odd");
		m_ctrlParity.AddString(L"Even");
		m_ctrlParity.AddString(L"Mark");
		m_ctrlParity.AddString(L"Space");
		m_ctrlParity.SetCurSel(0);
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 下拉框初始化完成");

		// 更新端口列表
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 开始更新端口列表");
		UpdatePortList();
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 端口列表更新完成");
	
		// 更新按钮状态
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 开始更新按钮状态");
		UpdateButtonStates();
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 按钮状态更新完成");

		// 设置文件拖放区域提示文本
		// m_ctrlFileDropZone.SetWindowText(L"拖拽文件到此处加载(不自动传输)");  // 控件不存在

		// 初始化状态显示
		// 📊 使用统一状态管理初始化状态显示
		UpdateStatusDisplay(L"未连接", L"空闲", L"就绪", L"", StatusPriority::NORMAL);
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 设置状态显示完成");
		
		// 设置等宽字体用于十六进制显示
		static CFont monoFont;
		monoFont.CreateFont(
			-12,                    // 字体高度
			0,                      // 字体宽度（自动）
			0,                      // 倾斜角度
			0,                      // 基线倾斜角度
			FW_NORMAL,             // 字体粗细
			FALSE,                 // 是否斜体
			FALSE,                 // 是否下划线
			0,                     // 是否删除线
			ANSI_CHARSET,          // 字符集
			OUT_DEFAULT_PRECIS,    // 输出精度
			CLIP_DEFAULT_PRECIS,   // 裁剪精度
			CLEARTYPE_QUALITY,     // 输出质量
			FIXED_PITCH | FF_MODERN, // 字体间距和系列
			_T("Consolas")         // 字体名称，备选：Courier New
		);
		// ⭐ 修复：改为对实际存在的控件设置字体
		m_ctrlDataView.SetFont(&monoFont);  // 统一数据显示控件
		m_ctrlInputHex.SetFont(&monoFont);  // 十六进制输入框
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 等宽字体设置完成");
		
		// 添加初始化日志
		AppendLog(L"PortMaster 初始化完成");
		AppendLog(L"现代化界面已启用");
		WriteDebugLog("[DEBUG] PortMasterDlg::InitializeControls: 控件初始化完成");
	}
	catch (...) {
		WriteDebugLog("[ERROR] PortMasterDlg::InitializeControls: 控件初始化异常");
		throw;
	}
}

void CPortMasterDlg::InitializeTransportObjects()
{
	WriteDebugLog("[DEBUG] CPortMasterDlg::InitializeTransportObjects: 开始初始化传输对象");
	
	// 初始化传输层和可靠通道
	m_transport = std::make_shared<LoopbackTransport>();
	m_reliableChannel = std::make_shared<ReliableChannel>(m_transport);
	
	// 设置可靠通道回调 - 使用PostMessage实现线程安全UI更新
	m_reliableChannel->SetProgressCallback([this](const TransferStats& stats) {
		// 线程安全的进度更新 - 增强窗口句柄验证
		if (stats.totalBytes > 0 && ::IsWindow(GetSafeHwnd()))
		{
			int progress = static_cast<int>((stats.transferredBytes * 100) / stats.totalBytes);
			CString* statusText = new CString();
			statusText->Format(L"传输中 (%.1f%%)", stats.GetProgress() * 100);
			
			// 🔑 P0-1: 使用SafePostMessage防止MFC断言崩溃
			if (!SafePostMessage(WM_UPDATE_PROGRESS, progress, reinterpret_cast<LPARAM>(statusText)))
			{
				// SafePostMessage失败，清理分配的内存
				delete statusText;
				WriteDebugLog(CT2A(L"[WARNING] 直接传输进度回调SafePostMessage失败"));
			}
			else
			{
				WriteDebugLog(CT2A(L"[DEBUG] 直接传输进度更新成功"));
			}
		}
	});
	
	m_reliableChannel->SetCompletionCallback([this](bool success, const std::string& message) {
		// 🔑 P1-5: 增强完成回调线程安全 - 使用SafePostMessage防止崩溃
		m_bTransmitting = false;  // 首先更新原子状态
		
		// 验证窗口句柄有效性后再进行UI操作
		if (::IsWindow(GetSafeHwnd()))
		{
			CString* msgData = new CString(CA2W(message.c_str(), CP_UTF8));
			
			// 🔑 P0-1: 使用SafePostMessage提升线程安全性
			if (!SafePostMessage(WM_UPDATE_COMPLETION, success ? 1 : 0, reinterpret_cast<LPARAM>(msgData)))
			{
				// SafePostMessage失败，清理分配的内存
				delete msgData;
				WriteDebugLog(CT2A(L"[WARNING] 可靠传输完成回调SafePostMessage失败"));
			}
		}
	});
	
	m_reliableChannel->SetFileReceivedCallback([this](const std::string& filename, const std::vector<uint8_t>& data) {
		// 文件接收完成回调 - 使用PostMessage实现线程安全UI更新
		
		// 验证窗口句柄有效性后再进行UI操作
		if (::IsWindow(GetSafeHwnd()))
		{
			// 创建数据包用于传递到UI线程
			struct FileReceivedData {
				CString filename;
				std::vector<uint8_t> data;
			};
			
			FileReceivedData* receivedData = new FileReceivedData{CA2W(filename.c_str()), data};
			
			// 🔑 P0-1: 使用SafePostMessage防止MFC断言崩溃（直接传输）
			CString debugMsg;
			debugMsg.Format(L"[DEBUG] 直接传输文件接收回调：%s, %zu字节", CA2W(filename.c_str()), data.size());
			WriteDebugLog(CT2A(debugMsg));
			if (!SafePostMessage(WM_UPDATE_FILE_RECEIVED, 0, reinterpret_cast<LPARAM>(receivedData)))
			{
				// SafePostMessage失败，清理分配的内存
				delete receivedData;
				WriteDebugLog(CT2A(L"[ERROR] 直接传输文件接收回调SafePostMessage失败"));
			}
			else
			{
				WriteDebugLog(CT2A(L"[DEBUG] 直接传输文件接收回调SafePostMessage成功"));
			}
		}
	});
	
	WriteDebugLog("[DEBUG] CPortMasterDlg::InitializeTransportObjects: 传输对象初始化完成");
}

void CPortMasterDlg::UpdatePortList()
{
	m_ctrlPortList.ResetContent();
	
	int portType = m_ctrlPortType.GetCurSel();
	switch (portType)
	{
	case 0: // 串口
		// 枚举串口 - 简单示例
		for (int i = 1; i <= 16; i++)
		{
			CString portName;
			portName.Format(L"COM%d", i);
			m_ctrlPortList.AddString(portName);
		}
		break;
	case 1: // 并口
		m_ctrlPortList.AddString(L"LPT1");
		m_ctrlPortList.AddString(L"LPT2");
		break;
	case 2: // USB打印机
		m_ctrlPortList.AddString(L"USB打印机1");
		break;
	case 3: // TCP客户端
		m_ctrlPortList.AddString(L"127.0.0.1:8080");
		break;
	case 4: // TCP服务端
		m_ctrlPortList.AddString(L"监听端口:8080");
		break;
	case 5: // UDP
		m_ctrlPortList.AddString(L"UDP:8080");
		break;
	case 6: // 本地回环
		m_ctrlPortList.AddString(L"本地回环");
		break;
	}
	
	if (m_ctrlPortList.GetCount() > 0)
		m_ctrlPortList.SetCurSel(0);
}

void CPortMasterDlg::UpdateButtonStates()
{
	// 🔑 P2-7: 异常处理和崩溃预防机制 - UI更新函数保护
	try 
	{
		// 添加控件句柄安全检查
		if (!IsWindow(m_ctrlConnectBtn.GetSafeHwnd()) || !IsWindow(m_ctrlDisconnectBtn.GetSafeHwnd()))
		{
			WriteDebugLog("[WARNING] UpdateButtonStates: 控件句柄未初始化，跳过更新");
			return;
		}
	
	// 更新在DoDataExchange中实际绑定的控件
	m_ctrlConnectBtn.EnableWindow(!m_bConnected);
	m_ctrlDisconnectBtn.EnableWindow(m_bConnected);
	
	// 改进的发送按钮状态管理（使用新的状态管理系统）
	bool hasSendableData = HasValidInputData();
	if (IsWindow(m_ctrlSendBtn.GetSafeHwnd()))
	{
		TransmissionState currentState = GetTransmissionState();
		bool enableSend = m_bConnected && hasSendableData;
		m_ctrlSendBtn.EnableWindow(enableSend);
		
		// 根据传输状态设置按钮文本
		switch (currentState)
		{
		case TransmissionState::IDLE:
			m_ctrlSendBtn.SetWindowText(L"发送");
			break;
		case TransmissionState::TRANSMITTING:
			m_ctrlSendBtn.SetWindowText(L"停止");
			break;
		case TransmissionState::PAUSED:
			m_ctrlSendBtn.SetWindowText(L"继续");
			break;
		case TransmissionState::COMPLETED:
			m_ctrlSendBtn.SetWindowText(L"发送");
			break;
		case TransmissionState::FAILED:
			m_ctrlSendBtn.SetWindowText(L"重试");
			break;
		}
	}
	
	// 停止按钮状态管理
	if (IsWindow(m_ctrlStopBtn.GetSafeHwnd()))
	{
		bool enableStop = IsTransmissionActive();
		m_ctrlStopBtn.EnableWindow(enableStop);
		
		// 根据传输状态设置停止按钮文本
		TransmissionState currentState = GetTransmissionState();
		if (currentState == TransmissionState::TRANSMITTING)
			m_ctrlStopBtn.SetWindowText(L"暂停");
		else if (currentState == TransmissionState::PAUSED)
			m_ctrlStopBtn.SetWindowText(L"停止");
		else
			m_ctrlStopBtn.SetWindowText(L"停止");
	}
	
	// 文件操作按钮状态管理
	if (IsWindow(m_ctrlLoadFileBtn.GetSafeHwnd()))
		m_ctrlLoadFileBtn.EnableWindow(!IsTransmissionActive());
	if (IsWindow(m_ctrlSaveFileBtn.GetSafeHwnd()))
	{
		bool hasDisplayData = !m_displayedData.empty();
		m_ctrlSaveFileBtn.EnableWindow(hasDisplayData);
	}
	
	// 🔑 关键修复：添加复制按钮状态管理
	if (IsWindow(m_ctrlCopyBtn.GetSafeHwnd()))
	{
		bool hasDisplayData = !m_displayedData.empty();
		m_ctrlCopyBtn.EnableWindow(hasDisplayData);
	}
	
	// 清除按钮始终可用 - 使用实际绑定的控件
	if (IsWindow(m_ctrlClearInputBtn.GetSafeHwnd()))
		m_ctrlClearInputBtn.EnableWindow(TRUE);
	if (IsWindow(m_ctrlClearDisplayBtn.GetSafeHwnd()))
		m_ctrlClearDisplayBtn.EnableWindow(TRUE);
	
	// 📊 使用统一状态管理更新显示（SOLID-S: 单一职责原则 - 集中状态管理）
	CString connectionStatus, protocolStatus, transferStatus;
	StatusPriority priority = StatusPriority::NORMAL;
	
	// 连接状态（带状态指示器）
	if (m_bTransmitting)
		connectionStatus = L"● 传输中";
	else if (m_bConnected)
		connectionStatus = L"● 已连接";
	else
		connectionStatus = L"○ 未连接";
	
	// 🔑 第八轮修订：增强协议状态检查，特别处理RELIABLE_DONE状态
	if (m_reliableChannel)
	{
		ReliableState state = m_reliableChannel->GetState();
		switch (state)
		{
		case RELIABLE_IDLE: protocolStatus = L"空闲"; break;
		case RELIABLE_STARTING: protocolStatus = L"开始传输"; priority = StatusPriority::HIGH; break;
		case RELIABLE_SENDING: protocolStatus = L"传输中"; priority = StatusPriority::HIGH; break;
		case RELIABLE_ENDING: protocolStatus = L"结束传输"; priority = StatusPriority::HIGH; break;
		case RELIABLE_RECEIVING: protocolStatus = L"接收中"; priority = StatusPriority::HIGH; break;
		case RELIABLE_DONE: 
			// 🔑 关键修复：RELIABLE_DONE状态应立即触发状态重置
			protocolStatus = L"完成"; 
			priority = StatusPriority::HIGH;
			// 检测到RELIABLE_DONE状态时，确保传输状态同步
			if (GetTransmissionState() == TransmissionState::TRANSMITTING) {
				SetTransmissionState(TransmissionState::COMPLETED);
			}
			break;
		case RELIABLE_FAILED: protocolStatus = L"失败"; priority = StatusPriority::CRITICAL; break;
		default: protocolStatus = L"未知"; break;
		}
	}
	
	// 传输状态
	if (m_bTransmitting)
	{
		transferStatus = L"状态: 传输中";
		priority = StatusPriority::HIGH; // 传输中是高优先级状态
	}
	else if (m_bConnected)
	{
		transferStatus = L"状态: 已连接";
		if (m_bReliableMode)
			transferStatus += L" (可靠模式)";
	}
	else
	{
		transferStatus = L"状态: 就绪";
	}
	
	// 📊 统一状态更新 - 避免状态信息混乱
	UpdateStatusDisplay(connectionStatus, protocolStatus, transferStatus, L"", priority);
	}
	catch (const std::exception& e)
	{
		CString errorMsg;
		errorMsg.Format(L"[CRITICAL] UpdateButtonStates异常: %s", CA2W(e.what()));
		WriteDebugLog(CT2A(errorMsg));
	}
	catch (...)
	{
		WriteDebugLog(CT2A(L"[CRITICAL] UpdateButtonStates未知异常"));
	}
}

// =====================================
// Stage 3 新增：综合状态栏更新 (SOLID-S: 单一职责, KISS: 简洁明了)
// =====================================

void CPortMasterDlg::UpdateStatusBar()
{
	// 连接状态详细信息 (DRY: 复用现有状态检查逻辑)
	CString connectionInfo;
	if (IsWindow(m_ctrlConnectionStatus.GetSafeHwnd()))
	{
		if (m_bConnected && m_transport)
		{
			// 获取传输类型信息
			std::string transportType = m_transport->GetTransportType();
			CString portName;
			if (m_ctrlPortList.GetCurSel() >= 0)
			{
				m_ctrlPortList.GetLBText(m_ctrlPortList.GetCurSel(), portName);
			}
			
			connectionInfo.Format(L"● 已连接 [%s: %s]", 
				CA2W(transportType.c_str()),
				portName.IsEmpty() ? L"未知端口" : portName);
		}
		else
		{
			connectionInfo = L"○ 未连接";
		}
	}
	
	// 协议状态详细信息
	CString protocolInfo;
	if (m_bReliableMode && m_reliableChannel)
	{
		ReliableState state = m_reliableChannel->GetState();
		const wchar_t* stateNames[] = { 
			L"空闲", L"开始", L"发送中", L"结束", L"就绪", L"接收中", L"完成", L"失败" 
		};
		
		protocolInfo.Format(L"可靠协议: %s", 
			(state >= 0 && state < 8) ? stateNames[state] : L"未知");
	}
	else
	{
		protocolInfo = L"直接传输模式";
	}
	
	// 传输状态综合信息 (SOLID-O: 开放封闭，易于扩展新状态)
	CString transferInfo;
	StatusPriority priority = StatusPriority::NORMAL;
	TransmissionState currentState = GetTransmissionState();
	
	switch (currentState)
	{
	case TransmissionState::IDLE:
		transferInfo = L"状态: 就绪";
		break;
	case TransmissionState::TRANSMITTING:
		priority = StatusPriority::HIGH; // 传输中是高优先级
		if (!m_transmissionData.empty())
		{
			// 显示传输进度和速度信息
			double progressPercent = (m_transmissionProgress * 100.0) / m_transmissionData.size();
			transferInfo.Format(L"传输中 %.1f%% | 速度: %s", 
				progressPercent, 
				GetCurrentTransferSpeed());
		}
		else
		{
			transferInfo = L"传输中...";
		}
		break;
	case TransmissionState::PAUSED:
		priority = StatusPriority::HIGH;
		if (m_transmissionContext.isValidContext)
		{
			transferInfo.Format(L"已暂停 (%.1f%%) | 可续传", 
				m_transmissionContext.GetProgressPercentage());
		}
		else
		{
			transferInfo = L"已暂停";
		}
		break;
	case TransmissionState::COMPLETED:
		priority = StatusPriority::HIGH;
		transferInfo = L"传输完成 ✓";
		break;
	case TransmissionState::FAILED:
		priority = StatusPriority::CRITICAL;
		transferInfo = L"传输失败 ✗ | 点击重试";
		break;
	default:
		transferInfo = L"状态未知";
		break;
	}
	
	// 📊 使用统一状态管理更新增强连接显示
	UpdateStatusDisplay(connectionInfo, protocolInfo, transferInfo, L"", priority);
}

CString CPortMasterDlg::GetCurrentTransferSpeed()
{
	// 计算当前传输速度 (YAGNI: 简化实现，避免过度设计)
	DWORD currentTime = GetTickCount();
	DWORD elapsedTime = currentTime - m_transmissionStartTime;
	
	if (elapsedTime > 0 && m_totalBytesTransmitted > 0)
	{
		double speed = (double)(m_totalBytesTransmitted * 1000) / elapsedTime;
		CString speedText;
		
		if (speed >= 1024)
		{
			speedText.Format(L"%.1f KB/s", speed / 1024.0);
		}
		else
		{
			speedText.Format(L"%.0f B/s", speed);
		}
		return speedText;
	}
	
	return L"计算中...";
}

void CPortMasterDlg::UpdatePortTypeSpecificControls()
{
	// 添加控件句柄安全检查
	if (!IsWindow(m_ctrlPortType.GetSafeHwnd()))
	{
		WriteDebugLog("[WARNING] UpdatePortTypeSpecificControls: 端口类型控件句柄未初始化，跳过更新");
		return;
	}
	
	// 获取当前选择的端口类型
	int currentPortType = m_ctrlPortType.GetCurSel();
	
	// 判断是否为串口类型（索引0为串口）
	bool isSerialPort = (currentPortType == 0);
	
	// 串口专用控件状态管理 (SOLID-S: 单一职责控制串口特定UI)
	if (IsWindow(m_ctrlBaudRate.GetSafeHwnd()))
		m_ctrlBaudRate.EnableWindow(isSerialPort);
	if (IsWindow(m_ctrlDataBits.GetSafeHwnd()))
		m_ctrlDataBits.EnableWindow(isSerialPort);
	if (IsWindow(m_ctrlStopBits.GetSafeHwnd()))
		m_ctrlStopBits.EnableWindow(isSerialPort);
	if (IsWindow(m_ctrlParity.GetSafeHwnd()))
		m_ctrlParity.EnableWindow(isSerialPort);
	
	// 根据端口类型记录日志 (YAGNI: 仅记录必要的状态信息)
	const wchar_t* portTypeNames[] = {
		L"串口", L"并口", L"USB打印机", L"TCP客户端", L"TCP服务器", L"UDP", L"回环测试"
	};
	
	if (currentPortType >= 0 && currentPortType < 7)
	{
		CString logMessage;
		logMessage.Format(L"端口类型切换到: %s, 串口专用控件%s", 
			portTypeNames[currentPortType], 
			isSerialPort ? L"已启用" : L"已禁用");
		WriteDebugLog(CW2A(logMessage, CP_UTF8));
	}
}

void CPortMasterDlg::AppendLog(const CString& message)
{
	CString timeStamp;
	SYSTEMTIME st;
	GetLocalTime(&st);
	timeStamp.Format(L"[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	
	CString log;
	m_ctrlLog.GetWindowText(log);
	log += timeStamp + message + L"\r\n";
	m_ctrlLog.SetWindowText(log);
	
	// TODO: 实现日志滚动功能（需要CEdit控件支持）
}

void CPortMasterDlg::AppendLogWithDetails(const CString& message, size_t bytes)
{
	CString detailedMessage = message;
	if (bytes > 0)
	{
		CString byteInfo;
		if (bytes >= 1024 * 1024)
		{
			byteInfo.Format(L" (%.2f MB)", bytes / (1024.0 * 1024.0));
		}
		else if (bytes >= 1024)
		{
			byteInfo.Format(L" (%.2f KB)", bytes / 1024.0);
		}
		else
		{
			byteInfo.Format(L" (%zu bytes)", bytes);
		}
		detailedMessage += byteInfo;
	}
	AppendLog(detailedMessage);
}

void CPortMasterDlg::UpdateTransferSpeed(size_t bytesTransferred)
{
	DWORD currentTime = GetTickCount();
	m_totalBytesTransmitted += bytesTransferred;
	
	// 每500ms更新一次速度显示
	if (currentTime - m_lastSpeedUpdateTime >= 500)
	{
		DWORD elapsedTime = currentTime - m_transmissionStartTime;
		if (elapsedTime > 0)
		{
			double speed = (m_totalBytesTransmitted * 1000.0) / elapsedTime;  // bytes/sec
			CString speedText;
			
			if (speed >= 1024 * 1024)
			{
				speedText.Format(L"速度: %.2f MB/s", speed / (1024.0 * 1024.0));
			}
			else if (speed >= 1024)
			{
				speedText.Format(L"速度: %.2f KB/s", speed / 1024.0);
			}
			else
			{
				speedText.Format(L"速度: %.0f B/s", speed);
			}
			
			// 使用统一状态管理更新传输速度
			if (IsWindow(m_ctrlTransferSpeed.GetSafeHwnd()))
			{
				m_ctrlTransferSpeed.SetWindowText(speedText);
			}
			
			// 估计剩余时间
			if (m_transmissionData.size() > m_transmissionProgress && speed > 0)
			{
				size_t remainingBytes = m_transmissionData.size() - m_transmissionProgress;
				double remainingSeconds = remainingBytes / speed;
				
				CString timeText;
				if (remainingSeconds >= 60)
				{
					timeText.Format(L"剩余: %.1f 分钟", remainingSeconds / 60.0);
				}
				else
				{
					timeText.Format(L"剩余: %.0f 秒", remainingSeconds);
				}
				
				// 使用统一状态管理显示剩余时间 (KISS: 利用现有UI控件避免资源修改)
				if (::IsWindow(m_ctrlTransferStatus.m_hWnd)) {
					// 不直接更新状态控件，改为通过传输速度控件显示剩余时间
					if (IsWindow(m_ctrlTransferSpeed.GetSafeHwnd())) {
						CString combinedText = speedText + L" | " + timeText;
						m_ctrlTransferSpeed.SetWindowText(combinedText);
					}
				}
			}
			
			m_lastSpeedUpdateTime = currentTime;
		}
	}
	
	// 更新传输进度显示
	if (IsWindow(m_ctrlTransferProgress.GetSafeHwnd()) && !m_transmissionData.empty())
	{
		CString progressText;
		progressText.Format(L"已传输: %zu/%zu", m_transmissionProgress, m_transmissionData.size());
		// 使用专门的进度控件，不与状态管理冲突
		m_ctrlTransferProgress.SetWindowText(progressText);
	}
}

void CPortMasterDlg::AppendHexData(const BYTE* data, size_t length, bool incoming)
{
	const size_t BYTES_PER_LINE = 8;  // 每行限制8字节
	CString prefix = incoming ? L"<< " : L">> ";
	
	CString currentHex;
	m_ctrlDataView.GetWindowText(currentHex);
	
	for (size_t i = 0; i < length; i += BYTES_PER_LINE)
	{
		CString hexLine;
		size_t lineLength = std::min(BYTES_PER_LINE, length - i);
		
		for (size_t j = 0; j < lineLength; j++)
		{
			CString temp;
			temp.Format(L"%02X ", data[i + j]);
			hexLine += temp;
		}
		
		currentHex += prefix + hexLine + L"\r\n";
	}
	
	m_ctrlDataView.SetWindowText(currentHex);
	
	// 滚动到底部
	m_ctrlDataView.LineScroll(static_cast<int>(m_ctrlDataView.GetLineCount()));
}

void CPortMasterDlg::AppendTextData(const CString& text, bool incoming)
{
	CString prefix = incoming ? L"<< " : L">> ";
	
	CString currentText;
	m_ctrlDataView.GetWindowText(currentText);
	currentText += prefix + text + L"\r\n";
	m_ctrlDataView.SetWindowText(currentText);
	
	// 滚动到底部
	m_ctrlDataView.LineScroll(static_cast<int>(m_ctrlDataView.GetLineCount()));
}

// 按钮事件处理
void CPortMasterDlg::OnBnClickedConnect()
{
	// SOLID-S: 单一职责 - 连接逻辑专门处理连接建立
	
	// 获取当前选择的传输类型
	int transportIndex = m_ctrlPortType.GetCurSel();
	if (transportIndex == CB_ERR)
	{
		AppendLog(L"请选择传输类型");
		return;
	}
	
	// 使用工厂模式创建传输实例 (SOLID-O: 开闭原则)
	std::shared_ptr<ITransport> newTransport = CreateTransportFromUI();
	if (!newTransport)
	{
		AppendLog(L"不支持的传输类型");
		return;
	}
	
	// 获取配置参数 (SOLID-S: 单一职责分离)
	TransportConfig config = GetTransportConfigFromUI();
	
	// 尝试打开传输连接
	if (!newTransport->Open(config))
	{
		std::string error = newTransport->GetLastError();
		CString statusMsg = GetConnectionStatusMessage(TRANSPORT_ERROR, error);
		
		// SOLID-S: 单一职责 - 提供针对性的错误建议
		CString detailedError = GetDetailedErrorSuggestion(transportIndex, error);
		AppendLog(L"连接失败: " + statusMsg);
		if (!detailedError.IsEmpty())
		{
			AppendLog(L"建议: " + detailedError);
		}
		
		// 📊 使用统一状态管理更新连接失败状态
		UpdateStatusDisplay(statusMsg, L"空闲", L"状态: 连接失败", L"", StatusPriority::CRITICAL);
		return;
	}
	
	// 连接成功，更新传输对象和可靠通道
	m_transport = newTransport;
	
	// 🔑 P0-2: 设置直接传输模式的数据接收回调（使用SafePostMessage防止MFC崩溃）
	m_transport->SetDataReceivedCallback([this](const std::vector<uint8_t>& data) {
		// 复制数据到堆内存用于线程间传递
		std::vector<uint8_t>* dataPtr = new std::vector<uint8_t>(data);
		
		// 使用SafePostMessage发送到UI线程处理 - 防止winq.cpp:1113崩溃
		if (!SafePostMessage(WM_DISPLAY_RECEIVED_DATA, 0, reinterpret_cast<LPARAM>(dataPtr)))
		{
			// SafePostMessage失败，清理分配的内存
			delete dataPtr;
			WriteDebugLog(CT2A(L"[WARNING] 直接传输数据接收回调SafePostMessage失败"));
		}
	});
	
	m_reliableChannel = std::make_shared<ReliableChannel>(m_transport);
	
	// SOLID-S: 单一职责 - 配置协议参数 (DRY: 统一配置管理)
	// 🚀 性能优化：本地回路跳过重配置，使用默认值提升连接速度
	if (std::dynamic_pointer_cast<LoopbackTransport>(newTransport)) {
		ConfigureReliableChannelForLoopback();
	} else {
		ConfigureReliableChannelFromConfig();
	}
	
	// 设置回调函数 (保持原有功能) - 使用SafePostMessage实现线程安全UI更新
	m_reliableChannel->SetProgressCallback([this](const TransferStats& stats) {
		// 🔑 P1-4: 频率限制机制 - 防止UI消息队列饱和
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastProgressUpdate);
		
		if (elapsed.count() < MIN_PROGRESS_INTERVAL_MS) {
			return; // 跳过过于频繁的回调
		}
		m_lastProgressUpdate = now;
		
		// 线程安全的进度更新 - 增强窗口句柄验证
		if (stats.totalBytes > 0 && ::IsWindow(GetSafeHwnd()))
		{
			int progress = static_cast<int>((stats.transferredBytes * 100) / stats.totalBytes);
			CString* statusText = new CString();
			statusText->Format(L"状态: 传输中 (%.1f%%, %zu/%zu 字节)", 
				stats.GetProgress() * 100, stats.transferredBytes, stats.totalBytes);
			
			// 🔑 P0-1: 使用SafePostMessage防止MFC断言崩溃
			if (!SafePostMessage(WM_UPDATE_PROGRESS, progress, reinterpret_cast<LPARAM>(statusText)))
			{
				// SafePostMessage失败，清理分配的内存
				delete statusText;
				WriteDebugLog(CT2A(L"[WARNING] 可靠传输进度回调SafePostMessage失败"));
			}
		}
	});
	
	m_reliableChannel->SetCompletionCallback([this](bool success, const std::string& message) {
		// 🔑 P1-5: 增强完成回调线程安全 - 使用SafePostMessage防止崩溃
		m_bTransmitting = false;  // 首先更新原子状态
		
		// 验证窗口句柄有效性后再进行UI操作
		if (::IsWindow(GetSafeHwnd()))
		{
			CString* msgData = new CString(CA2W(message.c_str(), CP_UTF8));
			
			// 🔑 P0-1: 使用SafePostMessage提升线程安全性
			if (!SafePostMessage(WM_UPDATE_COMPLETION, success ? 1 : 0, reinterpret_cast<LPARAM>(msgData)))
			{
				// SafePostMessage失败，清理分配的内存
				delete msgData;
				WriteDebugLog(CT2A(L"[WARNING] 可靠传输完成回调SafePostMessage失败"));
			}
		}
	});
	
	m_reliableChannel->SetFileReceivedCallback([this](const std::string& filename, const std::vector<uint8_t>& data) {
		// 文件接收完成回调 - 使用PostMessage实现线程安全UI更新
		
		// 验证窗口句柄有效性后再进行UI操作
		if (::IsWindow(GetSafeHwnd()))
		{
			// 创建数据包用于传递到UI线程
			struct FileReceivedData {
				CString filename;
				std::vector<uint8_t> data;
			};
			
			FileReceivedData* receivedData = new FileReceivedData{CA2W(filename.c_str()), data};
			
			// 🔑 P0-1: 使用SafePostMessage防止MFC断言崩溃（可靠传输）
			CString debugMsg2;
			debugMsg2.Format(L"[DEBUG] 可靠传输文件接收回调：%s, %zu字节", CA2W(filename.c_str()), data.size());
			WriteDebugLog(CT2A(debugMsg2));
			if (!SafePostMessage(WM_UPDATE_FILE_RECEIVED, 0, reinterpret_cast<LPARAM>(receivedData)))
			{
				// SafePostMessage失败，清理分配的内存
				delete receivedData;
				WriteDebugLog(CT2A(L"[CRITICAL] 可靠传输文件接收回调SafePostMessage失败 - 这是崩溃的主要原因！"));
			}
			else
			{
				WriteDebugLog(CT2A(L"[DEBUG] 可靠传输文件接收回调SafePostMessage成功"));
			}
		}
	});
	
	// 启用接收功能
	m_reliableChannel->EnableReceiving(true);
	
	if (m_reliableChannel && m_reliableChannel->Start())
	{
		m_bConnected = true;
		UpdateButtonStates();
		
		// 获取传输类型和端点信息用于显示 (DRY: 复用格式化函数)
		std::string transportTypeStr = m_transport->GetTransportType();
		std::string endpoint;
		
		// 重新获取配置信息 (KISS: 简化作用域管理)
		TransportConfig currentConfig = GetTransportConfigFromUI();
		
		// 获取端点信息 (SOLID-S: 单一职责 - 端点信息获取)
		if (transportTypeStr == "Serial")
		{
			endpoint = currentConfig.portName;
		}
		else if (transportTypeStr == "TCP" || transportTypeStr == "UDP")
		{
			// 获取实际的网络连接信息
			std::string actualEndpoint = GetNetworkConnectionInfo(transportTypeStr);
			endpoint = actualEndpoint.empty() ? (currentConfig.ipAddress + ":" + std::to_string(currentConfig.port)) : actualEndpoint;
		}
		else if (transportTypeStr == "LPT" || transportTypeStr == "USB")
		{
			endpoint = currentConfig.portName;
		}
		
		// 格式化连接信息
		CString transportInfo = FormatTransportInfo(transportTypeStr, endpoint);
		CString statusMsg = GetConnectionStatusMessage(TRANSPORT_OPEN);
		
		AppendLog(L"连接成功 - " + transportInfo);
		
		// 📊 使用统一状态管理更新连接成功状态
		UpdateStatusDisplay(statusMsg, L"空闲", L"状态: 已连接", L"", StatusPriority::HIGH);
	}
	else
	{
		std::string error = m_reliableChannel ? m_reliableChannel->GetLastError() : "可靠通道启动失败";
		CString statusMsg = GetConnectionStatusMessage(TRANSPORT_ERROR, error);
		AppendLog(L"可靠通道启动失败: " + statusMsg);
		
		// 📊 使用统一状态管理更新可靠通道失败状态
		UpdateStatusDisplay(statusMsg, L"失败", L"状态: 通道启动失败", L"", StatusPriority::CRITICAL);
	}
}

void CPortMasterDlg::OnBnClickedDisconnect()
{
	if (m_reliableChannel)
	{
		m_reliableChannel->Stop();
	}
	
	m_bConnected = false;
	// 🔑 关键修复：断开连接时重置传输状态
	SetTransmissionState(TransmissionState::IDLE);
	AppendLog(L"已断开连接");
	
	// 📊 使用统一状态管理更新断开连接后的状态
	UpdateStatusDisplay(L"○ 未连接", L"空闲", L"状态: 就绪", L"", StatusPriority::NORMAL);
}

void CPortMasterDlg::OnBnClickedSend()
{
	// 断点续传检查 (SOLID-S: 单一职责 - 续传逻辑分离)
	if (GetTransmissionState() == TransmissionState::PAUSED && m_transmissionContext.CanResume())
	{
		// 当前处于暂停状态且可以续传，询问用户是否续传
		CString resumeMsg;
		resumeMsg.Format(L"检测到未完成的传输: %s (进度 %.1f%%)\n是否续传？\n\n点击\"是\"继续传输，点击\"否\"重新开始", 
			PathFindFileName(m_transmissionContext.sourceFilePath),
			m_transmissionContext.GetProgressPercentage());
		
		int result = MessageBox(resumeMsg, L"断点续传", MB_YESNOCANCEL | MB_ICONQUESTION);
		
		if (result == IDYES)
		{
			// 用户选择续传
			if (ResumeTransmission())
			{
				return; // 续传成功，直接返回
			}
			// 续传失败，继续执行正常发送流程
		}
		else if (result == IDCANCEL)
		{
			return; // 用户取消操作
		}
		// result == IDNO 时，清除断点并继续正常发送流程
		ClearTransmissionContext();
	}
	
	// 优先检查是否有文件数据要发送
	std::vector<uint8_t> dataToSend;
	bool isFileTransmission = false;
	
	if (!m_transmissionData.empty())
	{
		// 有文件数据，发送文件
		dataToSend = m_transmissionData;
		isFileTransmission = true;
		AppendLog(L"发送文件数据");
	}
	else
	{
		// 获取输入框数据
		dataToSend = GetInputData();
		AppendLog(L"发送输入数据");
	}
	
	if (dataToSend.empty())
	{
		ShowUserMessage(L"没有数据可发送", L"请在十六进制或文本输入框中输入数据，或拖放文件", MB_ICONWARNING);
		return;
	}
	
	// 检查连接状态
	if (!m_bConnected)
	{
		ShowUserMessage(L"连接错误", L"请先连接端口才能发送数据", MB_ICONERROR);
		return;
	}
	
	// 传输状态控制 (SOLID-S: 单一职责 - 传输状态控制)
	if (IsTransmissionActive())
	{
		// 正在传输中，提供停止传输选项
		int result = MessageBox(L"当前正在传输数据，是否要停止传输？", 
			L"传输控制", MB_YESNO | MB_ICONQUESTION);
		
		if (result == IDYES) {
			SetTransmissionState(TransmissionState::IDLE);
			StopDataTransmission(false);
			AppendLog(L"用户手动停止传输");
		}
		return;
	}
	
	if (m_bReliableMode && m_reliableChannel)
	{
		// 使用可靠传输模式 - 增强错误检查和状态验证
		
		// 1. 验证可靠传输通道是否已激活
		if (!m_reliableChannel->IsActive())
		{
			AppendLog(L"可靠传输通道未启动，尝试启动...");
			if (!m_reliableChannel->Start())
			{
				SetTransmissionState(TransmissionState::FAILED);
				AppendLog(L"无法启动可靠传输通道");
				CString error = CA2W(m_reliableChannel->GetLastError().c_str(), CP_UTF8);
				if (!error.IsEmpty())
				{
					AppendLog(L"启动错误: " + error);
				}
				ShowUserMessage(L"可靠传输启动失败", 
					L"可靠传输通道无法启动，请检查连接状态或切换到普通传输模式", 
					MB_ICONERROR);
				return;
			}
			AppendLog(L"可靠传输通道启动成功");
		}
		
		// 2. 验证可靠传输通道状态
		ReliableState currentState = m_reliableChannel->GetState();
		if (currentState != RELIABLE_IDLE)
		{
			SetTransmissionState(TransmissionState::FAILED);
			CString stateMsg;
			stateMsg.Format(L"可靠传输通道状态异常 (状态码: %d)，请等待当前操作完成或重新连接", static_cast<int>(currentState));
			AppendLog(stateMsg);
			ShowUserMessage(L"可靠传输状态错误", stateMsg, MB_ICONWARNING);
			return;
		}
		
		// 3. 开始传输操作
		SetTransmissionState(TransmissionState::TRANSMITTING);
		bool transmissionStarted = false;
		
		if (isFileTransmission && !m_currentFileName.IsEmpty())
		{
			// 发送文件（带文件名）
			std::string fileNameStr = CT2A(m_currentFileName);
			transmissionStarted = m_reliableChannel->SendFile(fileNameStr, dataToSend);
			if (transmissionStarted)
			{
				AppendLog(L"开始可靠文件传输: " + m_currentFileName);
			}
			else
			{
				AppendLog(L"可靠文件传输启动失败");
			}
		}
		else
		{
			// 发送数据
			transmissionStarted = m_reliableChannel->SendData(dataToSend);
			if (transmissionStarted)
			{
				AppendLog(L"开始可靠传输");
			}
			else
			{
				AppendLog(L"可靠传输启动失败");
			}
		}
		
		// 4. 处理传输启动失败的情况
		if (!transmissionStarted)
		{
			SetTransmissionState(TransmissionState::FAILED);
			CString error = CA2W(m_reliableChannel->GetLastError().c_str(), CP_UTF8);
			if (!error.IsEmpty())
			{
				AppendLog(L"错误详情: " + error);
			}
			
			// 提供用户友好的错误处理建议
			ShowUserMessage(L"可靠传输失败", 
				L"可靠传输启动失败。\n\n建议操作：\n1. 检查连接状态\n2. 重新连接端口\n3. 或切换到普通传输模式", 
				MB_ICONERROR);
		}
	}
	else
	{
		// 使用普通传输模式（模拟）
		StartDataTransmission(dataToSend);
	}
}

void CPortMasterDlg::OnBnClickedClear()
{
	// 🔑 架构重构：使用ManagerIntegration清空显示
	// SOLID-S: 单一职责 - 委托清空操作给专职管理器
	if (m_managerIntegration) {
		m_managerIntegration->ClearDataDisplay();
	}
	
	AppendLog(L"显示区域已清空（通过管理器）");
}



void CPortMasterDlg::OnCbnSelchangePortType()
{
	UpdatePortList();
	UpdatePortTypeSpecificControls(); // SOLID-S: 单一职责 - 更新端口类型相关控件状态
	AppendLog(L"切换端口类型");
}

void CPortMasterDlg::OnBnClickedReliableMode()
{
	m_bReliableMode = (m_ctrlReliableMode.GetCheck() == BST_CHECKED);
	
	// 🔑 关键修复：模式切换时重置可靠传输状态
	if (m_reliableChannel)
	{
		// 重置可靠传输通道状态到IDLE，避免状态残留
		m_reliableChannel->ResetToIdle();
		AppendLog(L"可靠传输通道状态已重置");
	}
	
	// 重置UI传输状态
	SetTransmissionState(TransmissionState::IDLE);
	
	// 🔑 关键修复：切换传输模式后重新配置回调函数
	ConfigureTransportCallback();
	
	UpdateButtonStates();
	AppendLog(m_bReliableMode ? L"启用可靠传输模式" : L"禁用可靠传输模式");
}

void CPortMasterDlg::ConfigureTransportCallback()
{
	WriteDebugLog("[DEBUG] ConfigureTransportCallback: 开始配置Transport回调");
	
	if (!m_transport) {
		WriteDebugLog("[ERROR] ConfigureTransportCallback: Transport未初始化");
		return;
	}
	
	// 根据当前传输模式设置回调
	if (m_bReliableMode) {
		// 可靠传输模式：确保ReliableChannel的回调被正确设置
		if (m_reliableChannel) {
			// 重新配置ReliableChannel的Transport回调，防止被直接传输模式覆盖
			m_reliableChannel->ReconfigureTransportCallback();
			WriteDebugLog("[DEBUG] ConfigureTransportCallback: 可靠传输模式 - ReliableChannel回调已重新配置");
		} else {
			WriteDebugLog("[ERROR] ConfigureTransportCallback: ReliableChannel未初始化");
		}
	} else {
		// 直接传输模式：设置直接传输回调
		WriteDebugLog("[DEBUG] ConfigureTransportCallback: 配置直接传输模式回调");
		
		// 初始化直接传输回调（如果还没有初始化）
		if (!m_directTransportCallback) {
			m_directTransportCallback = [this](const std::vector<uint8_t>& data) {
				WriteDebugLog(("[DIRECT] 接收到直接传输数据，长度: " + std::to_string(data.size())).c_str());
				
				// 确保数据不为空
				if (!data.empty()) {
					// 复制数据到堆内存用于线程间传递
					std::vector<uint8_t>* dataPtr = new std::vector<uint8_t>(data);
					
					// 使用SafePostMessage发送到UI线程处理
					if (!SafePostMessage(WM_DISPLAY_RECEIVED_DATA, 0, reinterpret_cast<LPARAM>(dataPtr)))
					{
						// SafePostMessage失败，清理分配的内存
						delete dataPtr;
						WriteDebugLog("[WARNING] 直接传输数据接收回调SafePostMessage失败");
					}
				}
			};
			WriteDebugLog("[DEBUG] ConfigureTransportCallback: 直接传输回调已初始化");
		}
		
		m_transport->SetDataReceivedCallback(m_directTransportCallback);
	}
	
	WriteDebugLog("[DEBUG] ConfigureTransportCallback: Transport回调配置完成");
}

void CPortMasterDlg::OnDropFiles(HDROP hDropInfo)
{
	WriteDebugLog("[DEBUG] OnDropFiles: 接收到文件拖放事件");
	
	// 🔑 架构重构：委托给FileOperationManager处理文件拖放逻辑
	if (m_managerIntegration && m_managerIntegration->GetFileOperationManager()) {
		std::vector<std::wstring> filePaths;
		
		// 委托给FileOperationManager处理拖放
		if (m_managerIntegration->GetFileOperationManager()->HandleDropFiles(hDropInfo, filePaths)) {
			if (!filePaths.empty()) {
				// 处理第一个文件
				std::wstring firstFile = filePaths[0];
				CString filePath(firstFile.c_str());
				
				// 显示文件信息
				CString fileName = PathFindFileName(filePath);
				CString message;
				message.Format(L"正在处理文件: %s", fileName);
				AppendLog(message);
				
				// 加载文件用于传输
				std::vector<uint8_t> fileData;
				std::wstring displayInfo;
				
				if (m_managerIntegration->GetFileOperationManager()->LoadFileForTransmission(firstFile, fileData, displayInfo)) {
					// 更新传输数据
					m_transmissionData = fileData;
					m_currentFileName = fileName;
					
					// 显示文件信息
					AppendLog(CString(displayInfo.c_str()));
					UpdateDataSourceDisplay(L"文件: " + fileName);
					
					// 更新数据显示
					if (m_managerIntegration->GetDataDisplayManager()) {
						m_managerIntegration->UpdateDataDisplay(fileData, m_bHexDisplay ? DisplayMode::HEX : DisplayMode::TEXT);
					}
					
					ShowUserMessage(L"文件加载成功", 
						L"文件已加载并准备传输。\n可以在下方查看文件内容预览，\n点击发送按钮开始传输。", 
						MB_ICONINFORMATION);
					
					// 更新按钮状态
					UpdateButtonStates();
					WriteDebugLog("[SUCCESS] OnDropFiles: 文件加载成功");
				} else {
					ShowUserMessage(L"文件加载失败", L"无法读取文件内容，请检查文件是否损坏或权限不足", MB_ICONERROR);
					WriteDebugLog("[ERROR] OnDropFiles: 文件加载失败");
				}
			}
		} else {
			ShowUserMessage(L"拖放处理失败", L"文件拖放处理失败，请重试", MB_ICONERROR);
			WriteDebugLog("[ERROR] OnDropFiles: FileOperationManager处理失败");
		}
		
		DragFinish(hDropInfo);
		CDialogEx::OnDropFiles(hDropInfo);
		WriteDebugLog("[DEBUG] OnDropFiles: 文件拖放事件处理完成");
		return;
	}
	
	// 备用方案：如果FileOperationManager未初始化，使用简化逻辑
	WriteDebugLog("[WARNING] FileOperationManager未初始化，使用备用文件拖放处理");
	
	try {
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
		if (fileCount > 0) {
			TCHAR filePath[MAX_PATH];
			if (DragQueryFile(hDropInfo, 0, filePath, MAX_PATH) > 0) {
				CString fileName = PathFindFileName(filePath);
				AppendLog(L"拖放文件: " + fileName);
				
				// 尝试加载文件
				if (LoadFileForTransmission(filePath)) {
					ShowUserMessage(L"文件加载成功", L"文件已加载", MB_ICONINFORMATION);
					UpdateButtonStates();
				} else {
					ShowUserMessage(L"文件加载失败", L"无法加载文件", MB_ICONERROR);
				}
			}
		}
	} catch (...) {
		ShowUserMessage(L"拖放异常", L"文件拖放处理异常", MB_ICONERROR);
		WriteDebugLog("[ERROR] OnDropFiles: 备用处理异常");
	}
	
	DragFinish(hDropInfo);
	CDialogEx::OnDropFiles(hDropInfo);
	WriteDebugLog("[DEBUG] OnDropFiles: 备用文件拖放事件处理完成");
}

// SOLID-S: 单一职责 - 专门负责获取详细错误建议
CString CPortMasterDlg::GetDetailedErrorSuggestion(int transportIndex, const std::string& error)
{
	// SOLID-S: 单一职责 - 使用静态映射避免UI依赖 (YAGNI: 仅实现必要的传输类型)
	static const wchar_t* transportTypes[] = {
		L"串口", L"TCP客户端", L"TCP服务器", L"UDP", L"并口", L"USB打印机", L"回环测试"
	};
	
	CString transportType = L"";
	if (transportIndex >= 0 && transportIndex < _countof(transportTypes))
	{
		transportType = transportTypes[transportIndex];
	}
	
	CString errorMsg = CA2W(error.c_str(), CP_UTF8);
	errorMsg.MakeLower();
	
	// 串口相关错误建议
	if (transportType == L"串口")
	{
		if (errorMsg.Find(L"access") != -1 || errorMsg.Find(L"占用") != -1)
		{
			return L"串口被其他程序占用，请关闭相关程序后重试";
		}
		else if (errorMsg.Find(L"find") != -1 || errorMsg.Find(L"exist") != -1)
		{
			return L"串口不存在，请检查设备连接并刷新端口列表";
		}
		else if (errorMsg.Find(L"parameter") != -1 || errorMsg.Find(L"baud") != -1)
		{
			return L"串口参数配置错误，请检查波特率、数据位等设置";
		}
		return L"请检查串口连接、权限和参数配置";
	}
	// 网络相关错误建议
	else if (transportType == L"TCP客户端" || transportType == L"TCP服务器")
	{
		if (errorMsg.Find(L"connect") != -1 || errorMsg.Find(L"connection") != -1)
		{
			return L"无法建立TCP连接，请检查IP地址、端口号和网络状况";
		}
		else if (errorMsg.Find(L"bind") != -1 || errorMsg.Find(L"address") != -1)
		{
			return L"TCP端口绑定失败，请检查端口是否被占用或更换端口";
		}
		else if (errorMsg.Find(L"timeout") != -1)
		{
			return L"连接超时，请检查网络连通性和防火墙设置";
		}
		return L"请检查网络配置、防火墙设置和目标设备状态";
	}
	else if (transportType == L"UDP")
	{
		if (errorMsg.Find(L"bind") != -1)
		{
			return L"UDP端口绑定失败，请更换端口或检查权限";
		}
		else if (errorMsg.Find(L"address") != -1)
		{
			return L"UDP地址配置错误，请检查IP地址和端口设置";
		}
		return L"请检查UDP端口配置和网络权限";
	}
	// 打印机相关错误建议
	else if (transportType == L"并口" || transportType == L"USB打印机")
	{
		if (errorMsg.Find(L"printer") != -1 || errorMsg.Find(L"打印") != -1)
		{
			return L"打印机不可用，请检查设备连接和驱动安装";
		}
		else if (errorMsg.Find(L"access") != -1 || errorMsg.Find(L"permission") != -1)
		{
			return L"打印机访问权限不足，请以管理员身份运行程序";
		}
		return L"请检查打印机连接、权限和驱动程序";
	}
	// 回环测试
	else if (transportType == L"回环测试")
	{
		return L"回环测试失败，请检查程序配置和系统资源";
	}
	
	// 通用建议
	return L"请检查设备连接、权限设置和配置参数";
}

// SOLID-S: 单一职责 - 专门负责接收目录的设置和创建
void CPortMasterDlg::SetupReceiveDirectory()
{
	if (!m_reliableChannel)
		return;
		
	// 从配置管理器获取接收目录 (DRY: 复用配置管理)
	ConfigManager config;
	ConfigManager::AppConfig appConfig = config.GetAppConfig();
	
	std::string receiveDir = appConfig.receiveDirectory;
	if (receiveDir.empty())
	{
		// 使用默认接收目录
		char localAppData[MAX_PATH];
		if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData) == S_OK)
		{
			receiveDir = std::string(localAppData) + "\\PortIO\\Recv";
		}
		else
		{
			receiveDir = ".\\Recv";
		}
	}
	
	// 确保接收目录存在 (SOLID-S: 单一职责 - 目录管理)
	try
	{
		std::filesystem::path dirPath(receiveDir);
		if (!std::filesystem::exists(dirPath))
		{
			std::filesystem::create_directories(dirPath);
			CString dirMsg = CA2W(receiveDir.c_str(), CP_UTF8);
			AppendLog(L"已创建接收目录: " + dirMsg);
		}
		
		// 设置到可靠通道
		m_reliableChannel->SetReceiveDirectory(receiveDir);
		
		CString dirMsg = CA2W(receiveDir.c_str(), CP_UTF8);
		AppendLog(L"接收目录设置为: " + dirMsg);
	}
	catch (const std::exception& e)
	{
		CString errorMsg = CA2W(e.what(), CP_UTF8);
		AppendLog(L"创建接收目录失败: " + errorMsg);
		
		// 使用当前目录作为备用方案
		m_reliableChannel->SetReceiveDirectory(".");
		AppendLog(L"使用当前目录作为接收目录");
	}
}

// SOLID-S: 单一职责 - 专门负责获取网络连接详细信息
std::string CPortMasterDlg::GetNetworkConnectionInfo(const std::string& transportType)
{
	if (!m_transport)
		return "";
	
	std::string info;
	
	try
	{
		if (transportType == "TCP")
		{
			// 尝试转换为TCP传输获取详细连接信息
			auto tcpTransport = std::dynamic_pointer_cast<TcpTransport>(m_transport);
			if (tcpTransport)
			{
				std::string localEndpoint = tcpTransport->GetLocalEndpoint();
				std::string remoteEndpoint = tcpTransport->GetRemoteEndpoint();
				
				if (tcpTransport->IsServerMode())
				{
					if (!remoteEndpoint.empty())
					{
						info = "服务器(" + localEndpoint + ") ← 客户端(" + remoteEndpoint + ")";
					}
					else
					{
						info = "服务器(" + localEndpoint + ") - 等待连接";
					}
				}
				else
				{
					info = "客户端 → " + remoteEndpoint + " (本地:" + localEndpoint + ")";
				}
			}
		}
		else if (transportType == "UDP")
		{
			// 尝试转换为UDP传输获取详细连接信息
			auto udpTransport = std::dynamic_pointer_cast<UdpTransport>(m_transport);
			if (udpTransport)
			{
				std::string localEndpoint = udpTransport->GetLocalEndpoint();
				std::string remoteEndpoint = udpTransport->GetRemoteEndpoint();
				
				if (!remoteEndpoint.empty())
				{
					info = "UDP(" + localEndpoint + ") ↔ " + remoteEndpoint;
				}
				else
				{
					info = "UDP(" + localEndpoint + ") - 学习模式";
				}
			}
		}
	}
	catch (const std::exception&)
	{
		// 如果转换失败，返回空字符串使用默认显示
		return "";
	}
	
	return info;
}

// SOLID-S: 单一职责 - 传输工厂方法 (SOLID-O: 开闭原则)
std::shared_ptr<ITransport> CPortMasterDlg::CreateTransportFromUI()
{
	int transportIndex = m_ctrlPortType.GetCurSel();
	if (transportIndex == CB_ERR)
		return nullptr;

	// 🔑 架构重构完成：完全委托给TransportManager处理传输工厂逻辑
	// SOLID-S: 单一职责原则 - PortMasterDlg仅负责UI控制，不再承担传输创建职责
	if (m_managerIntegration && m_managerIntegration->GetTransportManager()) {
		return m_managerIntegration->GetTransportManager()->CreateTransportFromUI(transportIndex);
	}
	
	// 架构重构后的错误处理：如果管理器未初始化则报错
	WriteDebugLog("[ERROR] TransportManager未初始化，无法创建传输对象");
	return nullptr;
}

// 🔑 架构重构：委托给TransportManager处理配置获取逻辑
TransportConfig CPortMasterDlg::GetTransportConfigFromUI()
{
	int transportIndex = m_ctrlPortType.GetCurSel();
	if (transportIndex == CB_ERR) {
		return TransportConfig(); // 返回默认配置
	}
	
	// 从UI控件获取参数
	CString portName, baudRateStr, dataBitsStr, endpoint;
	int parityIndex = -1, stopBitsIndex = -1;
	
	// 获取端口名称/端点
	if (m_ctrlPortList.GetCurSel() != CB_ERR) {
		m_ctrlPortList.GetLBText(m_ctrlPortList.GetCurSel(), portName);
		endpoint = portName; // 对于网络传输，端点就是选择的项
	}
	
	// 获取串口参数
	if (m_ctrlBaudRate.GetCurSel() != CB_ERR) {
		m_ctrlBaudRate.GetLBText(m_ctrlBaudRate.GetCurSel(), baudRateStr);
	}
	if (m_ctrlDataBits.GetCurSel() != CB_ERR) {
		m_ctrlDataBits.GetLBText(m_ctrlDataBits.GetCurSel(), dataBitsStr);
	}
	parityIndex = m_ctrlParity.GetCurSel();
	stopBitsIndex = m_ctrlStopBits.GetCurSel();
	
	// 🔑 架构重构完成：完全委托给TransportManager处理配置获取
	// SOLID-S: 单一职责原则 - PortMasterDlg仅负责UI参数收集，配置逻辑由TransportManager处理
	if (m_managerIntegration && m_managerIntegration->GetTransportManager()) {
		return m_managerIntegration->GetTransportManager()->GetTransportConfigFromUI(
			transportIndex,
			std::string(CT2A(portName)),
			std::string(CT2A(baudRateStr)),
			std::string(CT2A(dataBitsStr)),
			parityIndex,
			stopBitsIndex,
			std::string(CT2A(endpoint))
		);
	}
	
	// 架构重构后的错误处理：如果管理器未初始化则返回默认配置
	WriteDebugLog("[ERROR] TransportManager未初始化，返回默认配置");
	return TransportConfig();
}

// SOLID-S: 单一职责 - 从配置管理器设置可靠通道参数 (DRY: 统一配置管理)
void CPortMasterDlg::ConfigureReliableChannelFromConfig()
{
	if (!m_reliableChannel)
		return;
		
	try
	{
		// 从配置管理器获取协议配置 (DRY: 复用配置管理)
		ConfigManager config;
		ConfigManager::AppConfig appConfig = config.GetAppConfig();
		
		// 应用协议参数到可靠通道 (SOLID-S: 单一职责 - 参数配置)
		m_reliableChannel->SetAckTimeout(appConfig.ackTimeoutMs);
		m_reliableChannel->SetMaxRetries(appConfig.maxRetries);
		m_reliableChannel->SetMaxPayloadSize(appConfig.maxPayloadSize);
		
		// 设置接收目录 (KISS: 简化目录管理逻辑)
		std::string receiveDir = appConfig.receiveDirectory;
		if (receiveDir.empty())
		{
			// 使用默认接收目录
			char localAppData[MAX_PATH];
			if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData) == S_OK)
			{
				receiveDir = std::string(localAppData) + "\\PortIO\\Recv";
			}
			else
			{
				receiveDir = ".\\Recv";
			}
		}
		
		// 确保接收目录存在 (SOLID-S: 单一职责 - 目录管理)
		std::filesystem::path dirPath(receiveDir);
		if (!std::filesystem::exists(dirPath))
		{
			std::filesystem::create_directories(dirPath);
			CString dirMsg = CA2W(receiveDir.c_str(), CP_UTF8);
			AppendLog(L"已创建接收目录: " + dirMsg);
		}
		
		// 设置到可靠通道
		m_reliableChannel->SetReceiveDirectory(receiveDir);
		
		// 记录配置应用日志
		CString protocolMsg;
		protocolMsg.Format(L"协议参数已配置: 超时=%dms, 重试=%d次, 负载=%zu字节", 
			appConfig.ackTimeoutMs, appConfig.maxRetries, appConfig.maxPayloadSize);
		AppendLog(protocolMsg);
		
		CString dirMsg = CA2W(receiveDir.c_str(), CP_UTF8);
		AppendLog(L"接收目录: " + dirMsg);
	}
	catch (const std::exception& e)
	{
		CString errorMsg = CA2W(e.what(), CP_UTF8);
		AppendLog(L"配置协议参数失败: " + errorMsg);
		
		// 使用默认参数作为备用方案 (SOLID-I: 接口隔离 - 不依赖外部配置)
		m_reliableChannel->SetAckTimeout(1000);
		m_reliableChannel->SetMaxRetries(3);
		m_reliableChannel->SetMaxPayloadSize(1024);
		m_reliableChannel->SetReceiveDirectory(".");
		
		AppendLog(L"已使用默认协议参数");
	}
}

// 🚀 性能优化：本地回路快速配置函数
void CPortMasterDlg::ConfigureReliableChannelForLoopback()
{
	if (!m_reliableChannel)
		return;
		
	// 本地回路使用优化的默认参数，无需读取配置文件
	m_reliableChannel->SetAckTimeout(100);    // 更短超时，本地回路延迟极低
	m_reliableChannel->SetMaxRetries(1);      // 更少重试，本地回路不丢包
	m_reliableChannel->SetMaxPayloadSize(8192); // 更大负载，本地回路无带宽限制
	m_reliableChannel->SetReceiveDirectory("."); // 简单接收目录
	
	WriteDebugLog("[DEBUG] 本地回路快速配置完成 - 跳过配置文件读取");
}

// 📊 统一状态管理 - 解决状态信息混乱问题
void CPortMasterDlg::UpdateStatusDisplay(const CString& connectionStatus, 
                                         const CString& protocolStatus, 
                                         const CString& transferStatus,
                                         const CString& speedInfo,
                                         StatusPriority priority)
{
	// 🔑 架构重构：委托给StateManager处理状态显示逻辑
	if (m_managerIntegration && m_managerIntegration->GetStateManager()) {
		// 转换StatusPriority到StatePriority
		StatePriority statePriority = StatePriority::NORMAL;
		switch (priority) {
		case StatusPriority::NORMAL: statePriority = StatePriority::NORMAL; break;
		case StatusPriority::HIGH: statePriority = StatePriority::HIGH; break;
		case StatusPriority::CRITICAL: statePriority = StatePriority::CRITICAL; break;
		}
		
		// 委托给StateManager处理
		m_managerIntegration->GetStateManager()->UpdateStatusDisplay(
			std::string(CT2A(connectionStatus)),
			std::string(CT2A(protocolStatus)),
			std::string(CT2A(transferStatus)),
			std::string(CT2A(speedInfo)),
			statePriority
		);
		
		// 更新UI控件（StateManager处理逻辑，UI更新仍在主对话框）
		if (!connectionStatus.IsEmpty() && IsWindow(m_ctrlConnectionStatus.GetSafeHwnd())) {
			m_ctrlConnectionStatus.SetWindowText(connectionStatus);
		}
		if (!protocolStatus.IsEmpty() && IsWindow(m_ctrlProtocolStatus.GetSafeHwnd())) {
			m_ctrlProtocolStatus.SetWindowText(protocolStatus);
		}
		if (!transferStatus.IsEmpty() && IsWindow(m_ctrlTransferStatus.GetSafeHwnd())) {
			m_ctrlTransferStatus.SetWindowText(transferStatus);
		}
		if (!speedInfo.IsEmpty() && IsWindow(m_ctrlTransferSpeed.GetSafeHwnd())) {
			m_ctrlTransferSpeed.SetWindowText(speedInfo);
		}
		return;
	}
	
	// 备用方案：如果StateManager未初始化，使用原有逻辑
	WriteDebugLog("[WARNING] StateManager未初始化，使用备用状态显示");
	
	// 简化的备用状态显示逻辑
	if (!connectionStatus.IsEmpty() && IsWindow(m_ctrlConnectionStatus.GetSafeHwnd())) {
		m_ctrlConnectionStatus.SetWindowText(connectionStatus);
	}
	if (!protocolStatus.IsEmpty() && IsWindow(m_ctrlProtocolStatus.GetSafeHwnd())) {
		m_ctrlProtocolStatus.SetWindowText(protocolStatus);
	}
	if (!transferStatus.IsEmpty() && IsWindow(m_ctrlTransferStatus.GetSafeHwnd())) {
		m_ctrlTransferStatus.SetWindowText(transferStatus);
	}
	if (!speedInfo.IsEmpty() && IsWindow(m_ctrlTransferSpeed.GetSafeHwnd())) {
		m_ctrlTransferSpeed.SetWindowText(speedInfo);
	}
	
	// 记录调试日志
	CString debugMsg;
	debugMsg.Format(L"[DEBUG] 备用状态更新 - 连接:%s 协议:%s 传输:%s 速度:%s", 
		connectionStatus.IsEmpty() ? L"" : connectionStatus,
		protocolStatus.IsEmpty() ? L"" : protocolStatus, 
		transferStatus.IsEmpty() ? L"" : transferStatus,
		speedInfo.IsEmpty() ? L"" : speedInfo);
	WriteDebugLog(CT2A(debugMsg));
}

// SOLID-S: 单一职责 - 用户界面交互方法实现

CPortMasterDlg::~CPortMasterDlg()
{
	// 析构函数 - 清理资源
	if (m_transmissionTimer != 0)
	{
		KillTimer(m_transmissionTimer);
		m_transmissionTimer = 0;
	}
}

void CPortMasterDlg::OnBnClickedClearInput()
{
	m_ctrlInputHex.SetWindowText(L"");
	AppendLog(L"输入区域已清空");
}

void CPortMasterDlg::OnBnClickedClearDisplay()
{
	// 🔑 架构重构：使用ManagerIntegration清空显示
	// SOLID-S: 单一职责 - 委托清空操作给专职管理器
	if (m_managerIntegration) {
		m_managerIntegration->ClearDataDisplay();
	}
	
	// 清空显示数据缓冲区（保持兼容性）
	{
		std::lock_guard<std::mutex> lock(m_displayDataMutex);
		m_displayedData.clear();
	}
	
	AppendLog(L"显示区域已清空（通过管理器）");
	UpdateButtonStates(); // 更新保存按钮状态
}

void CPortMasterDlg::OnBnClickedLoadFile()
{
	CFileDialog fileDlg(TRUE, nullptr, nullptr, 
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		L"所有文件|*.*|文本文件|*.txt|二进制文件|*.bin|数据文件|*.dat||");
		
	if (fileDlg.DoModal() == IDOK)
	{
		CString filePath = fileDlg.GetPathName();
		if (LoadFileForTransmission(filePath))
		{
			ShowUserMessage(L"文件加载成功", L"文件已加载并准备传输", MB_ICONINFORMATION);
			UpdateButtonStates();
		}
		else
		{
			ShowUserMessage(L"文件加载失败", L"无法读取文件内容", MB_ICONERROR);
		}
	}
}

void CPortMasterDlg::OnBnClickedSaveFile()
{
	std::vector<uint8_t> dataToSave;
	{
		std::lock_guard<std::mutex> lock(m_displayDataMutex);
		dataToSave = m_displayedData;
	}
	
	if (dataToSave.empty())
	{
		ShowUserMessage(L"保存失败", L"没有数据可保存", MB_ICONWARNING);
		return;
	}
	
	CFileDialog fileDlg(FALSE, L"dat", L"ReceivedData",
		OFN_OVERWRITEPROMPT,
		L"数据文件|*.dat|二进制文件|*.bin|文本文件|*.txt|所有文件|*.*||");
		
	if (fileDlg.DoModal() == IDOK)
	{
		CString filePath = fileDlg.GetPathName();
		std::ofstream file(CT2A(filePath), std::ios::binary);
		
		if (file.is_open())
		{
			file.write(reinterpret_cast<const char*>(dataToSave.data()), dataToSave.size());
			file.close();
			
			CString msg;
			msg.Format(L"文件保存成功: %s (%zu 字节)", 
				PathFindFileName(filePath), dataToSave.size());
			AppendLog(msg);
			ShowUserMessage(L"保存成功", msg, MB_ICONINFORMATION);
		}
		else
		{
			ShowUserMessage(L"保存失败", L"无法创建文件", MB_ICONERROR);
		}
	}
}

void CPortMasterDlg::OnBnClickedCopy()
{
	WriteDebugLog("[DEBUG] OnBnClickedCopy: 统一复制功能调用");
	
	// 根据当前十六进制显示模式智能选择复制方式
	if (m_bHexDisplay) {
		WriteDebugLog("[DEBUG] OnBnClickedCopy: 当前为十六进制模式，调用十六进制复制");
		OnBnClickedCopyHex();
	} else {
		WriteDebugLog("[DEBUG] OnBnClickedCopy: 当前为文本模式，调用文本复制");
		OnBnClickedCopyText();
	}
}

void CPortMasterDlg::OnBnClickedHexDisplay()
{
	WriteDebugLog("[DEBUG] OnBnClickedHexDisplay: 十六进制显示模式切换");
	
	// 获取复选框当前状态
	m_bHexDisplay = (m_ctrlHexDisplayCheck.GetCheck() == BST_CHECKED);
	
	// 记录状态变化
	CString logMsg;
	logMsg.Format(L"切换到%s显示模式", m_bHexDisplay ? L"十六进制" : L"文本");
	WriteDebugLog(CW2A(logMsg));
	
	// 立即更新显示内容，应用新的显示模式
	UpdateDataDisplay();
	
	WriteDebugLog("[DEBUG] OnBnClickedHexDisplay: 显示模式切换完成");
}

void CPortMasterDlg::OnBnClickedCopyHex()
{
	// 重构：基于原始数据源，确保数据完整性 (SOLID-S: 单一职责)
	std::lock_guard<std::mutex> lock(m_displayDataMutex); // 线程安全访问
	
	if (m_displayedData.empty())
	{
		ShowUserMessage(L"复制失败", L"没有数据可复制", MB_ICONWARNING);
		return;
	}
	
	// 🔑 架构重构：直接调用详细格式化函数，移除冗余包装
	CString hexText = FormatHexDisplay(m_displayedData);
	
	if (!hexText.IsEmpty())
	{
		if (OpenClipboard())
		{
			EmptyClipboard();
			size_t len = (hexText.GetLength() + 1) * sizeof(WCHAR);
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
			
			if (hMem)
			{
				wcscpy_s((WCHAR*)GlobalLock(hMem), hexText.GetLength() + 1, hexText);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
				
				// 详细的成功反馈
				CString logMsg;
				logMsg.Format(L"十六进制数据已复制到剪贴板 (%zu 字节)", m_displayedData.size());
				AppendLog(logMsg);
			}
			CloseClipboard();
		}
		else
		{
			AppendLog(L"剪贴板访问失败");
		}
	}
	else
	{
		AppendLog(L"数据格式化失败");
	}
}

void CPortMasterDlg::OnBnClickedCopyText()
{
	// 重构：基于原始数据源，确保数据完整性 (SOLID-S: 单一职责)
	std::lock_guard<std::mutex> lock(m_displayDataMutex); // 线程安全访问
	
	if (m_displayedData.empty())
	{
		ShowUserMessage(L"复制失败", L"没有数据可复制", MB_ICONWARNING);
		return;
	}
	
	// 🔑 架构重构：直接调用详细格式化函数，移除冗余包装
	CString textData = FormatTextDisplay(m_displayedData);
	
	if (!textData.IsEmpty())
	{
		if (OpenClipboard())
		{
			EmptyClipboard();
			size_t len = (textData.GetLength() + 1) * sizeof(WCHAR);
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
			
			if (hMem)
			{
				wcscpy_s((WCHAR*)GlobalLock(hMem), textData.GetLength() + 1, textData);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
				
				// 详细的成功反馈
				CString logMsg;
				logMsg.Format(L"文本数据已复制到剪贴板 (%zu 字节)", m_displayedData.size());
				AppendLog(logMsg);
			}
			CloseClipboard();
		}
		else
		{
			AppendLog(L"剪贴板访问失败");
		}
	}
	else
	{
		AppendLog(L"数据格式化失败");
	}
}

void CPortMasterDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == AppConstants::TRANSMISSION_TIMER_ID)
	{
		OnChunkTransmissionTimer();
	}
	CDialogEx::OnTimer(nIDEvent);
}

void CPortMasterDlg::DisplayReceivedData(const std::vector<uint8_t>& data)
{
	if (data.empty())
		return;
	
	// 🔑 架构重构：使用ManagerIntegration处理数据显示
	// SOLID-S: 单一职责 - 委托数据管理给专职管理器
	if (!m_managerIntegration) {
		WriteDebugLog("[ERROR] DisplayReceivedData: ManagerIntegration未初始化");
		return;
	}
	
	try {
		// 线程安全地更新显示数据缓冲区（保持兼容性）
		{
			std::lock_guard<std::mutex> lock(m_displayDataMutex);
			m_displayedData = data; // 替换而不是追加
		}
		
		// 🔑 架构优势：使用管理器的统一显示接口
		// SOLID-D: 依赖抽象而非具体实现
		DisplayMode mode = m_bHexDisplay ? DisplayMode::MIXED : DisplayMode::TEXT;
		m_managerIntegration->UpdateDataDisplay(data, mode);
		
		// 🔑 架构优势：滚动由DataDisplayManager内部处理
		// 更新按钮状态
		UpdateButtonStates();
		
		WriteDebugLog("[INFO] DisplayReceivedData: 数据显示已更新（通过管理器）");
		
	} catch (const std::exception& e) {
		CString errorMsg;
		errorMsg.Format(L"[ERROR] DisplayReceivedData异常: %s", CA2W(e.what()));
		WriteDebugLog(CW2A(errorMsg));
	}
}

bool CPortMasterDlg::HasValidInputData()
{
	CString inputText;
	m_ctrlInputHex.GetWindowText(inputText);
	
	// 检查输入框是否有数据或者文件传输数据是否存在
	return (!inputText.IsEmpty() || !m_transmissionData.empty());
}

CString CPortMasterDlg::GetConnectionStatusMessage(TransportState state, const std::string& error)
{
	switch (state)
	{
	case TRANSPORT_CLOSED:
		return L"未连接";
	case TRANSPORT_OPENING:
		return L"连接中...";
	case TRANSPORT_OPEN:
		return L"已连接";
	case TRANSPORT_CLOSING:
		return L"断开中...";
	case TRANSPORT_ERROR:
		{
			if (error.empty())
				return L"连接错误";
			CString errorMsg = CA2W(error.c_str(), CP_UTF8);
			return L"错误: " + errorMsg;
		}
	default:
		return L"未知状态";
	}
}

CString CPortMasterDlg::FormatTransportInfo(const std::string& transportType, const std::string& endpoint)
{
	CString typeMsg = CA2W(transportType.c_str(), CP_UTF8);
	
	if (endpoint.empty())
	{
		return typeMsg + L" 连接";
	}
	else
	{
		CString endpointMsg = CA2W(endpoint.c_str(), CP_UTF8);
		return typeMsg + L" (" + endpointMsg + L")";
	}
}

std::vector<uint8_t> CPortMasterDlg::GetInputData()
{
	CString inputText;
	m_ctrlInputHex.GetWindowText(inputText);
	
	if (inputText.IsEmpty())
		return std::vector<uint8_t>();
		
	// 检查是否为十六进制格式
	if (IsHexFormatInput(inputText))
	{
		return ProcessHexInput(inputText);
	}
	else
	{
		return ProcessTextInput(inputText);
	}
}

void CPortMasterDlg::ShowUserMessage(const CString& title, const CString& message, UINT type)
{
	MessageBox(message, title, type);
}

// =====================================
// Stage 3 新增：增强错误处理机制 (SOLID-S: 单一职责, DRY: 统一错误处理)
// =====================================

void CPortMasterDlg::ShowDetailedErrorMessage(const CString& operation, const CString& error, const CString& suggestion)
{
	CString detailedMsg;
	detailedMsg.Format(L"操作: %s\n\n错误详情: %s", operation, error);
	
	if (!suggestion.IsEmpty())
	{
		detailedMsg += L"\n\n建议解决方案:\n" + suggestion;
	}
	
	// 根据错误类型提供通用建议 (SOLID-O: 开放封闭，易于扩展)
	if (error.Find(L"连接") >= 0 || error.Find(L"端口") >= 0)
	{
		if (suggestion.IsEmpty())
		{
			detailedMsg += L"\n\n建议解决方案:\n• 检查设备连接是否正常\n• 确认端口参数设置正确\n• 尝试重新连接端口";
		}
	}
	else if (error.Find(L"传输") >= 0 || error.Find(L"发送") >= 0)
	{
		if (suggestion.IsEmpty())
		{
			detailedMsg += L"\n\n建议解决方案:\n• 检查网络连接状态\n• 确认目标设备是否在线\n• 尝试减小传输数据大小";
		}
	}
	else if (error.Find(L"文件") >= 0)
	{
		if (suggestion.IsEmpty())
		{
			detailedMsg += L"\n\n建议解决方案:\n• 检查文件是否存在且可读\n• 确认文件权限设置\n• 尝试选择其他文件";
		}
	}
	
	MessageBox(detailedMsg, L"详细错误信息", MB_ICONERROR | MB_OK);
	
	// 记录详细错误日志
	CString logEntry;
	logEntry.Format(L"[ERROR] %s: %s", operation, error);
	AppendLog(logEntry);
}

void CPortMasterDlg::HandleTransmissionErrorWithSuggestion(const CString& errorMsg, bool canRetry)
{
	// 更新传输状态为失败
	SetTransmissionState(TransmissionState::FAILED);
	
	// 分析错误类型并提供针对性建议 (KISS: 简化错误分类逻辑)
	CString suggestion;
	
	if (errorMsg.Find(L"超时") >= 0 || errorMsg.Find(L"timeout") >= 0)
	{
		suggestion = L"• 检查网络连接稳定性\n• 尝试增加超时设置\n• 确认目标设备响应正常";
	}
	else if (errorMsg.Find(L"拒绝") >= 0 || errorMsg.Find(L"refused") >= 0)
	{
		suggestion = L"• 检查目标端口是否开放\n• 确认防火墙设置\n• 验证连接参数";
	}
	else if (errorMsg.Find(L"数据") >= 0 || errorMsg.Find(L"CRC") >= 0)
	{
		suggestion = L"• 检查传输线缆连接\n• 降低传输速率\n• 检查数据完整性";
	}
	else
	{
		suggestion = L"• 检查设备连接状态\n• 确认传输参数设置\n• 尝试重新启动传输";
	}
	
	if (canRetry)
	{
		suggestion += L"\n• 点击\"重试\"按钮重新尝试传输";
	}
	
	// 显示详细错误信息
	ShowDetailedErrorMessage(L"数据传输", errorMsg, suggestion);
	
	// 更新状态栏显示
	UpdateStatusBar();
}

bool CPortMasterDlg::AttemptAutoRetry(const CString& operation, int maxRetries)
{
	// 检查是否应该进行自动重试 (YAGNI: 避免过度复杂的重试策略)
	if (m_currentRetryCount >= maxRetries)
	{
		// 已达到最大重试次数
		CString msg;
		msg.Format(L"操作 \"%s\" 重试 %d 次后仍然失败", operation, maxRetries);
		AppendLog(msg);
		
		// 重置重试计数器
		m_currentRetryCount = 0;
		m_lastFailedOperation.Empty();
		
		return false;
	}
	
	// 增加重试计数
	m_currentRetryCount++;
	m_lastFailedOperation = operation;
	
	CString retryMsg;
	retryMsg.Format(L"正在进行第 %d 次重试: %s", m_currentRetryCount, operation);
	AppendLog(retryMsg);
	
	// 简单的退避延迟策略 (KISS: 简化重试间隔逻辑)
	Sleep(1000 * m_currentRetryCount);  // 1秒, 2秒, 3秒...
	
	// 根据操作类型执行相应的重试逻辑
	if (operation.Find(L"连接") >= 0)
	{
		// 重试连接操作
		OnBnClickedConnect();
		return m_bConnected; // 返回连接是否成功
	}
	else if (operation.Find(L"传输") >= 0 || operation.Find(L"发送") >= 0)
	{
		// 重试传输操作 (SOLID-D: 依赖抽象而不是具体实现)
		if (!m_transmissionData.empty())
		{
			// 有传输数据，重新发送
			OnBnClickedSend();
			return true;
		}
	}
	
	return false;
}

void CPortMasterDlg::StartDataTransmission(const std::vector<uint8_t>& data)
{
	// 第四阶段重构：实现真实的数据分块传输 (SOLID-S: 单一职责 - 分块传输管理)
	if (data.empty()) {
		AppendLog(L"错误：数据为空，无法启动传输");
		return;
	}
	
	// 🔑 关键修复：使用统一的状态管理系统
	SetTransmissionState(TransmissionState::TRANSMITTING);
	
	// 初始化传输参数
	m_chunkTransmissionData = data;
	m_chunkTransmissionIndex = 0;
	m_chunkSize = 256; // 每次传输256字节 (KISS: 使用固定大小简化逻辑)
	
	// 记录传输开始时间
	m_transmissionStartTime = GetTickCount();
	m_totalBytesTransmitted = 0;
	m_lastSpeedUpdateTime = m_transmissionStartTime;
	
	// 更新UI状态 (SOLID-S: 单一职责 - UI状态管理)
	UpdateButtonStates();
	
	// 设置进度条 (SOLID-S: 修复大文件传输时的范围溢出问题)
	if (::IsWindow(m_ctrlProgress.m_hWnd)) {
		m_ctrlProgress.SetRange32(0, static_cast<int>(data.size()));  // 使用SetRange32支持更大范围
		m_ctrlProgress.SetPos(0);
	}
	
	// 日志记录
	CString logMsg;
	logMsg.Format(L"开始分块传输 - 总大小: %zu 字节, 块大小: %zu 字节", 
		data.size(), m_chunkSize);
	AppendLog(logMsg);
	
	// 启动传输定时器 (使用现有常量)
	m_transmissionTimer = SetTimer(AppConstants::TRANSMISSION_TIMER_ID, 
		AppConstants::TRANSMISSION_TIMER_INTERVAL, NULL);
	
	if (m_transmissionTimer == 0) {
		// 定时器启动失败
		SetTransmissionState(TransmissionState::FAILED);
		AppendLog(L"错误：无法启动传输定时器");
		return;
	}
	
	AppendLog(L"传输定时器已启动，开始分块传输");
}

void CPortMasterDlg::UpdateDataSourceDisplay(const CString& source)
{
	// 更新数据源显示标签（如果存在）
	if (IsWindow(m_ctrlDataSourceLabel.GetSafeHwnd()))
	{
		m_ctrlDataSourceLabel.SetWindowText(L"数据源: " + source);
	}
	
	AppendLog(L"数据源: " + source);
}

bool CPortMasterDlg::LoadFileForTransmission(const CString& filePath)
{
	try
	{
		std::ifstream file(CT2A(filePath), std::ios::binary | std::ios::ate);
		if (!file.is_open())
			return false;
			
		size_t fileSize = static_cast<size_t>(file.tellg());
		if (fileSize == 0)
		{
			file.close();
			return false;
		}
		
		// 检查文件大小限制 (SOLID-S: 单一职责 - 资源管理)
		if (fileSize > AppConstants::MAX_FILE_SIZE)
		{
			file.close();
			CString sizeMsg;
			sizeMsg.Format(L"文件过大 (%.2f MB)，最大支持 %.2f MB", 
				fileSize * AppConstants::GetBytesToMegabytes(),
				AppConstants::MAX_FILE_SIZE * AppConstants::GetBytesToMegabytes());
			ShowUserMessage(L"文件过大", sizeMsg, MB_ICONWARNING);
			return false;
		}
		
		file.seekg(0, std::ios::beg);
		
		m_transmissionData.resize(static_cast<size_t>(fileSize));
		file.read(reinterpret_cast<char*>(m_transmissionData.data()), fileSize);
		file.close();
		
		// 设置文件名
		m_currentFileName = PathFindFileName(filePath);
		
		// 显示文件内容到输入框（实现真正共用设计）
		// TODO: 待后续优化 - 通过DataDisplayManager统一格式化
		if (m_bHexDisplay) {
			CString hexDisplay = FormatHexDisplay(m_transmissionData);
			m_ctrlInputHex.SetWindowText(hexDisplay);
		} else {
			CString textDisplay = FormatTextDisplay(m_transmissionData);
			m_ctrlInputHex.SetWindowText(textDisplay);
		}
		
		CString msg;
		msg.Format(L"已加载文件: %s (%zu 字节)", 
			PathFindFileName(filePath), fileSize);
		AppendLog(msg);
		UpdateDataSourceDisplay(L"文件: " + CString(PathFindFileName(filePath)));
		
		return true;
	}
	catch (...) 
	{
		return false;
	}
}

// SOLID-S: 单一职责 - 数据格式处理辅助方法

bool CPortMasterDlg::IsHexFormatInput(const CString& input)
{
	// 检查是否包含十六进制字符
	for (int i = 0; i < input.GetLength(); i++)
	{
		WCHAR ch = input[i];
		if (!iswspace(ch) && !iswxdigit(ch))
			return false;
	}
	return true;
}

std::vector<uint8_t> CPortMasterDlg::ProcessHexInput(const CString& hexInput)
{
	std::vector<uint8_t> data;
	CString cleanHex = hexInput;
	cleanHex.Replace(L" ", L"");
	cleanHex.Replace(L"\t", L"");
	cleanHex.Replace(L"\r", L"");
	cleanHex.Replace(L"\n", L"");
	
	for (int i = 0; i < cleanHex.GetLength(); i += 2)
	{
		if (i + 1 < cleanHex.GetLength())
		{
			CString byteStr = cleanHex.Mid(i, 2);
			uint8_t byte = (uint8_t)wcstoul(byteStr, nullptr, 16);
			data.push_back(byte);
		}
	}
	
	return data;
}

std::vector<uint8_t> CPortMasterDlg::ProcessTextInput(const CString& textInput)
{
	std::string utf8Text = CT2A(textInput, CP_UTF8);
	return std::vector<uint8_t>(utf8Text.begin(), utf8Text.end());
}

// 🔑 统一传输架构核心：十六进制显示格式化
// 优化性能和可读性
CString CPortMasterDlg::FormatHexDisplay(const std::vector<uint8_t>& data)
{
	if (data.empty()) {
		return L"[空数据]";
	}
	
	const size_t BYTES_PER_LINE = 16;
	const size_t MAX_LINES = 10000; // 限制最大行数防止内存溢出
	
	// 🔑 优化1：预分配内存提升性能
	size_t totalLines = (data.size() + BYTES_PER_LINE - 1) / BYTES_PER_LINE;
	if (totalLines > MAX_LINES) {
		totalLines = MAX_LINES;
	}
	
	CString result;
	result.Preallocate(static_cast<int>(totalLines * 80)); // 预估每行80字符
	
	try {
		size_t processedLines = 0;
		
		for (size_t i = 0; i < data.size() && processedLines < MAX_LINES; i += BYTES_PER_LINE, ++processedLines)
		{
			// 🔑 优化2：使用StringBuilder模式减少字符串拼接
			CString line;
			line.Preallocate(80); // 预分配行缓冲区
			
			// 地址部分 - 8位十六进制
			line.Format(L"%08X: ", static_cast<unsigned int>(i));
			
			// 🔑 优化3：十六进制数据部分 - 批量处理
			CString hexPart;
			hexPart.Preallocate(48); // 16字节 * 3字符/字节
			
			size_t actualBytes = std::min(BYTES_PER_LINE, data.size() - i);
			
			for (size_t j = 0; j < actualBytes; j++)
			{
				CString byteStr;
				byteStr.Format(L"%02X ", data[i + j]);
				hexPart += byteStr;
			}
			
			// 🔑 优化4：智能对齐填充
			for (size_t j = actualBytes; j < BYTES_PER_LINE; j++)
			{
				hexPart += L"   ";
			}
			
			line += hexPart + L" |";
			
			// 🔑 优化5：ASCII字符部分 - 增强字符处理
			CString asciiPart;
			asciiPart.Preallocate(16);
			
			for (size_t j = 0; j < actualBytes; j++)
			{
				uint8_t byte = data[i + j];
				
				// 扩展可显示字符范围
				if (byte >= 32 && byte <= 126) {
					// 标准ASCII可打印字符
					asciiPart += static_cast<WCHAR>(byte);
				} else if (byte == 9) {
					// Tab字符显示为特殊符号
					asciiPart += L'→';
				} else if (byte == 10 || byte == 13) {
					// 换行符显示为特殊符号
					asciiPart += L'↵';
				} else if (byte == 0) {
					// NULL字符
					asciiPart += L'∅';
				} else {
					// 其他不可显示字符
					asciiPart += L'·';
				}
			}
			
			// 🔑 优化6：ASCII部分对齐填充
			for (size_t j = actualBytes; j < BYTES_PER_LINE; j++)
			{
				asciiPart += L' ';
			}
			
			line += asciiPart + L"|\r\n";
			result += line;
		}
		
		// 🔑 优化7：数据截断提示
		if (data.size() > MAX_LINES * BYTES_PER_LINE) {
			CString truncateMsg;
			truncateMsg.Format(L"\r\n[数据已截断] 显示前%zu行，总计%zu字节\r\n", 
				MAX_LINES, data.size());
			result += truncateMsg;
		}
		
	} catch (const std::exception& e) {
		// 🔑 优化8：异常处理
		CString errorMsg;
		errorMsg.Format(L"[格式化错误] FormatHexDisplay异常: %s\r\n", CA2W(e.what()));
		return errorMsg;
		
	} catch (...) {
		return L"[格式化错误] FormatHexDisplay发生未知异常\r\n";
	}
	
	return result;
}

// 🔑 UTF-8序列验证辅助函数
bool CPortMasterDlg::IsValidUTF8Sequence(const std::vector<uint8_t>& data, size_t start, size_t& length)
{
	if (start >= data.size()) return false;
	
	uint8_t firstByte = data[start];
	length = 1;
	
	// ASCII字符 (0xxxxxxx)
	if ((firstByte & 0x80) == 0) {
		return true;
	}
	
	// 多字节UTF-8序列
	if ((firstByte & 0xE0) == 0xC0) {
		// 2字节序列 (110xxxxx 10xxxxxx)
		length = 2;
	} else if ((firstByte & 0xF0) == 0xE0) {
		// 3字节序列 (1110xxxx 10xxxxxx 10xxxxxx)
		length = 3;
	} else if ((firstByte & 0xF8) == 0xF0) {
		// 4字节序列 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
		length = 4;
	} else {
		return false; // 无效的起始字节
	}
	
	// 检查是否有足够的字节
	if (start + length > data.size()) {
		return false;
	}
	
	// 验证后续字节格式 (10xxxxxx)
	for (size_t i = 1; i < length; ++i) {
		if ((data[start + i] & 0xC0) != 0x80) {
			return false;
		}
	}
	
	return true;
}

// 🔑 统一传输架构核心：文本显示格式化
// 优化编码检测和文本处理
CString CPortMasterDlg::FormatTextDisplay(const std::vector<uint8_t>& data)
{
	if (data.empty()) {
		return L"[空数据]";
	}
	
	try {
		// 🔑 优化1：数据大小限制和预处理
		const size_t MAX_TEXT_SIZE = 512 * 1024; // 512KB文本显示限制
		std::vector<uint8_t> processData;
		
		if (data.size() > MAX_TEXT_SIZE) {
			// 显示最新数据
			processData.assign(data.end() - MAX_TEXT_SIZE, data.end());
			WriteDebugLog("[INFO] FormatTextDisplay: 大数据量优化，显示最新512KB");
		} else {
			processData = data;
		}
		
		// 🔑 优化2：增强的UTF-8检测和解码
		bool hasValidUTF8 = true;
		size_t utf8ErrorCount = 0;
		size_t i = 0;
		
		while (i < processData.size()) {
			size_t seqLength;
			if (!IsValidUTF8Sequence(processData, i, seqLength)) {
				utf8ErrorCount++;
				// 允许少量UTF-8错误（可能是混合编码）
				if (utf8ErrorCount > processData.size() / 10) {
					hasValidUTF8 = false;
					break;
				}
				i++; // 跳过无效字节
			} else {
				i += seqLength;
			}
		}
		
		// 🔑 优化3：UTF-8解码策略
		if (hasValidUTF8 && utf8ErrorCount == 0) {
			std::string utf8Str(processData.begin(), processData.end());
			int wideStrLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, 
				utf8Str.c_str(), static_cast<int>(utf8Str.length()), nullptr, 0);
			
			if (wideStrLen > 0) {
				std::vector<wchar_t> wideStr(wideStrLen + 1);
				int actualLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
					utf8Str.c_str(), static_cast<int>(utf8Str.length()),
					wideStr.data(), wideStrLen);
					
				if (actualLen > 0) {
					wideStr[actualLen] = L'\0';
					CString result(wideStr.data());
					
					// 🔑 优化4：结果验证
					if (!result.IsEmpty() && result.Find(L'\uFFFD') == -1) {
						WriteDebugLog("[INFO] UTF-8解码成功");
						return result;
					}
				}
			}
		}
		
		// 🔑 优化5：GBK/GB2312解码策略（支持简体中文）
		std::string gbkStr(processData.begin(), processData.end());
		int wideStrLen = MultiByteToWideChar(CP_ACP, 0,
			gbkStr.c_str(), static_cast<int>(gbkStr.length()), nullptr, 0);
		
		if (wideStrLen > 0) {
			std::vector<wchar_t> wideStr(wideStrLen + 1);
			int actualLen = MultiByteToWideChar(CP_ACP, 0,
				gbkStr.c_str(), static_cast<int>(gbkStr.length()),
				wideStr.data(), wideStrLen);
				
			if (actualLen > 0) {
				wideStr[actualLen] = L'\0';
				CString result(wideStr.data());
				
				// 🔑 优化6：GBK解码结果验证
				if (!result.IsEmpty() && result != L"?" && 
					result.Find(L'\uFFFD') == -1) {
					WriteDebugLog("[INFO] GBK/GB2312解码成功");
					return result;
				}
			}
		}
		
		// 🔑 关键修复：FormatTextDisplay应始终返回纯文本格式，不受m_bHexDisplay影响
		// 显示模式的选择由更高层的UpdateDataDisplay函数处理
		WriteDebugLog("[INFO] FormatTextDisplay：返回纯文本显示格式");
		return FormatPlainTextDisplay(processData);
		
	} catch (const std::exception& e) {
		// 🔑 优化8：异常处理
		CString errorMsg;
		errorMsg.Format(L"[格式化错误] FormatTextDisplay异常: %s\r\n", CA2W(e.what()));
		WriteDebugLog(CW2A(errorMsg));
		return errorMsg;
		
	} catch (...) {
		WriteDebugLog("[ERROR] FormatTextDisplay发生未知异常");
		return L"[格式化错误] FormatTextDisplay发生未知异常\r\n";
	}
}

// 🔑 智能混合显示策略（SOLID-S: 单一职责 - 混合格式显示）
// 统一传输架构优化：增强编码检测和字符处理逻辑
CString CPortMasterDlg::FormatMixedDisplay(const std::vector<uint8_t>& data)
{
	CString result;
	result.Preallocate(static_cast<int>(data.size() * 3)); // 增加预分配空间
	
	for (size_t i = 0; i < data.size(); ) {
		uint8_t byte = data[i];
		
		// 🔑 优化1：增强UTF-8序列检测和处理
		size_t utf8Length;
		if (IsValidUTF8Sequence(data, i, utf8Length) && utf8Length > 1) {
			// 尝试解码UTF-8序列
			std::vector<uint8_t> utf8Bytes(data.begin() + i, data.begin() + i + utf8Length);
			std::string utf8Str(utf8Bytes.begin(), utf8Bytes.end());
			
			int wideStrLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, 
				utf8Str.c_str(), static_cast<int>(utf8Str.length()), nullptr, 0);
			
			if (wideStrLen > 0) {
				std::vector<wchar_t> wideStr(wideStrLen + 1);
				int actualLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
					utf8Str.c_str(), static_cast<int>(utf8Str.length()),
					wideStr.data(), wideStrLen);
					
				if (actualLen > 0) {
					wideStr[actualLen] = L'\0';
					CString decodedChar(wideStr.data());
					// 🔑 优化2：更严格的有效字符验证
					if (!decodedChar.IsEmpty() && decodedChar != L"?" && 
						decodedChar.GetLength() > 0 && decodedChar[0] != L'\uFFFD') {
						result += decodedChar;
						i += utf8Length;
						continue;
					}
				}
			}
		}
		
		// 🔑 优化3：增强单字节处理逻辑
		if (byte >= 32 && byte <= 126) {
			// 可打印ASCII字符
			result += static_cast<wchar_t>(byte);
		} else if (byte >= 0xA0 && byte <= 0xFF) {
			// 🔑 优化4：尝试GBK/GB2312单字节扩展字符解码
			char singleByte[2] = { static_cast<char>(byte), '\0' };
			int wideLen = MultiByteToWideChar(CP_ACP, 0, singleByte, 1, nullptr, 0);
			if (wideLen > 0) {
				wchar_t wideChar;
				if (MultiByteToWideChar(CP_ACP, 0, singleByte, 1, &wideChar, 1) > 0) {
					result += wideChar;
					i++;
					continue;
				}
			}
			// 如果解码失败，显示为十六进制
			CString hexByte;
			hexByte.Format(L"[%02X]", byte);
			result += hexByte;
		} else if (byte >= 0x80 && byte < 0xA0) {
			// 非标准高位字节，显示为十六进制
			CString hexByte;
			hexByte.Format(L"[%02X]", byte);
			result += hexByte;
		} else {
			// 🔑 优化5：改进控制字符显示
			switch (byte) {
			case 0x0A: result += L"\r\n"; break;    // 换行符直接换行
			case 0x0D: break;                      // 回车符忽略（避免重复换行）
			case 0x09: result += L"    "; break;    // 制表符转为4个空格
			case 0x00: result += L"[NULL]"; break;  // 空字符明确标识
			case 0x1B: result += L"[ESC]"; break;   // ESC字符
			default:
				CString ctrlChar;
				ctrlChar.Format(L"[%02X]", byte);
				result += ctrlChar;
				break;
			}
		}
		
		i++;
	}
	
	return result;
}

// 🔑 新增：纯文本显示策略（SOLID-S: 单一职责 - 纯文本格式显示）
// 直接传输模式专用：提供用户友好的纯文本显示
CString CPortMasterDlg::FormatPlainTextDisplay(const std::vector<uint8_t>& data)
{
	if (data.empty()) {
		return L"[空数据]";
	}
	
	try {
		CString result;
		result.Preallocate(static_cast<int>(data.size()));
		
		// 🔑 策略1：优先尝试UTF-8解码
		std::string utf8Data(reinterpret_cast<const char*>(data.data()), data.size());
		int wideStrLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, 
			utf8Data.c_str(), static_cast<int>(utf8Data.length()), nullptr, 0);
		
		if (wideStrLen > 0) {
			// UTF-8解码成功
			std::vector<wchar_t> wideStr(wideStrLen + 1);
			int actualLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
				utf8Data.c_str(), static_cast<int>(utf8Data.length()),
				wideStr.data(), wideStrLen);
			
			if (actualLen > 0) {
				wideStr[actualLen] = L'\0';
				result = CString(wideStr.data());
				WriteDebugLog("[INFO] UTF-8解码成功，使用UTF-8显示");
				return result;
			}
		}
		
		// 🔑 策略2：UTF-8失败，尝试GBK/GB2312解码
		wideStrLen = MultiByteToWideChar(CP_ACP, 0, 
			reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), nullptr, 0);
		
		if (wideStrLen > 0) {
			std::vector<wchar_t> wideStr(wideStrLen + 1);
			int actualLen = MultiByteToWideChar(CP_ACP, 0,
				reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()),
				wideStr.data(), wideStrLen);
			
			if (actualLen > 0) {
				wideStr[actualLen] = L'\0';
				result = CString(wideStr.data());
				WriteDebugLog("[INFO] GBK解码成功，使用GBK显示");
				return result;
			}
		}
		
		// 🔑 策略3：所有解码失败，按ASCII处理（过滤控制字符）
		for (size_t i = 0; i < data.size(); ++i) {
			uint8_t byte = data[i];
			
			if (byte >= 32 && byte <= 126) {
				// 可打印ASCII字符
				result += static_cast<wchar_t>(byte);
			} else if (byte == 0x0A) {
				// 保留换行符
				result += L"\r\n";
			} else if (byte == 0x09) {
				// 制表符转为空格
				result += L"    ";
			} else if (byte == 0x0D) {
				// 跳过回车符（避免重复换行）
				continue;
			}
			// 其他控制字符直接忽略（不显示）
		}
		
		WriteDebugLog("[INFO] 使用ASCII过滤显示");
		return result;
		
	} catch (const std::exception& e) {
		CString errorMsg;
		errorMsg.Format(L"[格式化错误] FormatPlainTextDisplay异常: %s\r\n", CA2W(e.what()));
		WriteDebugLog(CW2A(errorMsg));
		return errorMsg;
		
	} catch (...) {
		WriteDebugLog("[ERROR] FormatPlainTextDisplay发生未知异常");
		return L"[格式化错误] FormatPlainTextDisplay发生未知异常\r\n";
	}
}

void CPortMasterDlg::ScrollToBottom()
{
	// 滚动十六进制视图到底部
	int hexLines = m_ctrlDataView.GetLineCount();
	if (hexLines > 0)
	{
		m_ctrlDataView.LineScroll(hexLines);
	}
	
	// 滚动文本视图到底部
	int textLines = m_ctrlDataView.GetLineCount();
	if (textLines > 0)
	{
		m_ctrlDataView.LineScroll(textLines);
	}
}


// 第四阶段核心：分块传输定时器处理 (SOLID-S: 单一职责 - 分块数据传输)
void CPortMasterDlg::OnChunkTransmissionTimer()
{
	// 🔑 Task 3.3 增强：定时器有效性验证（防止重复调用）
	if (m_transmissionTimer == 0) {
		// 定时器已被外部停止，立即退出
		return;
	}
	
	// 🔑 Task 3.3 增强：线程安全的状态检查（添加中断信号检查）
	TransmissionState currentState = GetTransmissionState();
	
	// 🔑 Task 3.3 增强：优先检查中断信号
	if (currentState == TransmissionState::IDLE || 
		currentState == TransmissionState::COMPLETED || 
		currentState == TransmissionState::FAILED) {
		// 检测到中断信号，安全停止传输
		AppendLog(L"检测到中断信号，停止传输定时器");
		if (m_transmissionTimer != 0) {
			KillTimer(m_transmissionTimer);
			m_transmissionTimer = 0;
		}
		return;
	}
	
	// 🔑 Task 3.3 增强：数据有效性检查（安全的传输状态转换）
	if (m_chunkTransmissionData.empty()) {
		AppendLog(L"传输数据为空，安全停止传输");
		SetTransmissionState(TransmissionState::FAILED);
		StopDataTransmission(false);
		return;
	}
	
	// 🔑 Task 3.3 增强：暂停状态的智能处理
	if (currentState == TransmissionState::PAUSED) {
		// 暂停状态下保持定时器运行但不执行传输，等待恢复信号
		return;
	}
	
	// 🔑 Task 3.3 增强：仅在TRANSMITTING状态下执行传输
	if (currentState != TransmissionState::TRANSMITTING) {
		// 未知状态，安全转换为失败状态
		CString statusMsg;
		statusMsg.Format(L"传输状态异常 (%d)，停止传输", static_cast<int>(currentState));
		AppendLog(statusMsg);
		SetTransmissionState(TransmissionState::FAILED);
		StopDataTransmission(false);
		return;
	}
	
	// 🔑 Task 3.3 增强：安全的数据块计算
	if (m_chunkTransmissionIndex >= m_chunkTransmissionData.size()) {
		// 传输已完成，执行安全的状态转换
		AppendLog(L"数据传输完成，执行完成状态转换");
		StopDataTransmission(true);
		return;
	}
	
	// 计算当前块的大小
	size_t remainingBytes = m_chunkTransmissionData.size() - m_chunkTransmissionIndex;
	size_t currentChunkSize = std::min(m_chunkSize, remainingBytes);
	
	if (currentChunkSize == 0) {
		// 传输已完成
		AppendLog(L"当前数据块大小为0，传输完成");
		StopDataTransmission(true);
		return;
	}
	
	// 🔑 Task 3.3 增强：传输前的最终中断检查
	currentState = GetTransmissionState();
	if (currentState != TransmissionState::TRANSMITTING) {
		AppendLog(L"传输前检测到状态变更，取消当前传输");
		return;
	}
	
	// 提取当前数据块
	std::vector<uint8_t> currentChunk(
		m_chunkTransmissionData.begin() + m_chunkTransmissionIndex,
		m_chunkTransmissionData.begin() + m_chunkTransmissionIndex + currentChunkSize
	);
	
	// 🔑 Task 3.3 增强：安全的数据传输执行 (SOLID-D: 依赖抽象 - 使用传输接口)
	bool transmissionSuccess = false;
	if (m_transport && m_transport->IsOpen()) {
		try {
			size_t written = m_transport->Write(currentChunk);
			transmissionSuccess = (written == currentChunk.size());
			
			// 🔑 Task 3.3 增强：传输后立即检查中断信号
			currentState = GetTransmissionState();
			if (currentState != TransmissionState::TRANSMITTING) {
				AppendLog(L"传输后检测到中断信号，停止后续处理");
				return;
			}
			
			if (transmissionSuccess) {
				// 更新传输进度
				m_chunkTransmissionIndex += currentChunkSize;
				m_totalBytesTransmitted += currentChunkSize;
				
				// 实时UI状态更新 (SOLID-S: 单一职责 - UI状态反馈)
				UpdateTransmissionProgress();
				
				// 显示传输的数据块到接收区域（用于回环测试模式）
				if (ShouldEchoTransmittedData()) {
					DisplayReceivedDataChunk(currentChunk);
				}
				
				// 调试日志
				CString debugMsg;
				debugMsg.Format(L"已发送数据块: %zu 字节, 进度: %.1f%%", 
					currentChunkSize, 
					(double)(m_chunkTransmissionIndex * 100) / m_chunkTransmissionData.size());
				AppendLog(debugMsg);
			}
			else {
				// 写入失败 - 安全的错误状态转换
				CString errorMsg;
				errorMsg.Format(L"数据块传输失败: 预期 %zu 字节, 实际 %zu 字节", 
					currentChunkSize, written);
				AppendLog(errorMsg);
				SetTransmissionState(TransmissionState::FAILED);
				StopDataTransmission(false);
			}
		}
		catch (const std::exception& e) {
			// 🔑 Task 3.3 增强：异常处理中的安全状态转换
			CString errorMsg = CA2W(e.what(), CP_UTF8);
			AppendLog(L"传输异常: " + errorMsg);
			SetTransmissionState(TransmissionState::FAILED);
			StopDataTransmission(false);
		}
	}
	else {
		// 🔑 Task 3.3 增强：传输通道错误的安全处理
		AppendLog(L"错误：传输通道未开启，执行安全停止");
		SetTransmissionState(TransmissionState::FAILED);
		StopDataTransmission(false);
	}
}

// 第四阶段新增：停止数据传输 (SOLID-S: 单一职责 - 传输状态管理)
void CPortMasterDlg::StopDataTransmission(bool completed)
{
	// 停止定时器
	if (m_transmissionTimer != 0) {
		KillTimer(m_transmissionTimer);
		m_transmissionTimer = 0;
	}
	
	// 🔑 关键修复：使用统一的状态管理
	if (completed) {
		SetTransmissionState(TransmissionState::COMPLETED);
	} else {
		SetTransmissionState(TransmissionState::IDLE);
	}
	
	// 更新UI状态
	UpdateButtonStates();
	
	// 完成进度条
	if (::IsWindow(m_ctrlProgress.m_hWnd) && completed) {
		m_ctrlProgress.SetPos(static_cast<int>(m_chunkTransmissionData.size()));
	}
	
	// 记录传输结果
	if (completed) {
		DWORD elapsedTime = GetTickCount() - m_transmissionStartTime;
		double speed = (elapsedTime > 0) ? (double)(m_totalBytesTransmitted * 1000) / elapsedTime : 0;
		
		CString completionMsg;
		completionMsg.Format(L"分块传输完成 - 总计: %zu 字节, 耗时: %lu ms, 平均速度: %.1f B/s", 
			m_totalBytesTransmitted, elapsedTime, speed);
		AppendLog(completionMsg);
	}
	else {
		AppendLog(L"分块传输中断");
	}
	
	// 清理传输数据 (YAGNI: 及时释放不需要的资源)
	m_chunkTransmissionData.clear();
	m_chunkTransmissionIndex = 0;
}

// 第四阶段新增：实时UI进度更新 (SOLID-S: 单一职责 - 进度显示)
void CPortMasterDlg::UpdateTransmissionProgress()
{
	// 更新进度条
	if (::IsWindow(m_ctrlProgress.m_hWnd)) {
		m_ctrlProgress.SetPos(static_cast<int>(m_chunkTransmissionIndex));
	}
	
	// 更新传输速度显示
	DWORD currentTime = GetTickCount();
	if (currentTime > m_lastSpeedUpdateTime + 500) { // 每500ms更新一次速度显示
		DWORD elapsedTime = currentTime - m_transmissionStartTime;
		if (elapsedTime > 0) {
			double speed = (double)(m_totalBytesTransmitted * 1000) / elapsedTime;
			
			CString speedText;
			if (speed >= 1024) {
				speedText.Format(L"%.1f KB/s", speed / 1024.0);
			} else {
				speedText.Format(L"%.0f B/s", speed);
			}
			
			// 使用统一状态管理更新传输速度
			if (::IsWindow(m_ctrlTransferSpeed.m_hWnd)) {
				m_ctrlTransferSpeed.SetWindowText(speedText);
			}
		}
		m_lastSpeedUpdateTime = currentTime;
	}
	
	// 更新进度百分比显示
	double progressPercent = (double)(m_chunkTransmissionIndex * 100) / m_chunkTransmissionData.size();
	CString progressText;
	progressText.Format(L"%.1f%% (%zu/%zu)", progressPercent, 
		m_chunkTransmissionIndex, m_chunkTransmissionData.size());
	
	// 使用专门的进度控件，不与状态管理冲突
	if (::IsWindow(m_ctrlTransferProgress.m_hWnd)) {
		m_ctrlTransferProgress.SetWindowText(progressText);
	}
}

// 第四阶段新增：判断是否应回显传输数据 (SOLID-S: 单一职责 - 回显策略)
bool CPortMasterDlg::ShouldEchoTransmittedData() const
{
	// 只有在回环测试模式下才回显数据
	int portType = m_ctrlPortType.GetCurSel();
	return (portType == 6); // 6 = 回环测试
}

// 第四阶段新增：显示传输数据块 (SOLID-S: 单一职责 - 分块数据显示)
void CPortMasterDlg::DisplayReceivedDataChunk(const std::vector<uint8_t>& chunk)
{
	if (chunk.empty()) return;
	
	// 🔑 架构重构：使用ManagerIntegration处理数据分块显示
	// SOLID-S: 单一职责 - 委托数据管理给专职管理器
	if (!m_managerIntegration) {
		WriteDebugLog("[ERROR] DisplayReceivedDataChunk: ManagerIntegration未初始化");
		return;
	}
	
	try {
		// 更新显示数据缓冲区（保持兼容性）
		{
			std::lock_guard<std::mutex> lock(m_displayDataMutex);
			m_displayedData.insert(m_displayedData.end(), chunk.begin(), chunk.end());
		}
		
		// 🔑 架构优势：使用管理器的追加显示接口
		// SOLID-D: 依赖抽象而非具体实现
		m_managerIntegration->AppendDataDisplay(chunk);
		
		// 🔑 架构优势：格式化和滚动由DataDisplayManager内部处理
		// 更新按钮状态
		UpdateButtonStates();
		
		WriteDebugLog("[INFO] DisplayReceivedDataChunk: 数据块显示已追加（通过管理器）");
		
	} catch (const std::exception& e) {
		CString errorMsg;
		errorMsg.Format(L"[ERROR] DisplayReceivedDataChunk异常: %s", CA2W(e.what()));
		WriteDebugLog(CW2A(errorMsg));
	}
}

// SOLID-S: 单一职责 - 线程安全的UI更新消息处理函数
LRESULT CPortMasterDlg::OnUpdateProgress(WPARAM wParam, LPARAM lParam)
{
	// wParam包含进度百分比，lParam包含状态文本指针
	int progress = static_cast<int>(wParam);
	CString* statusText = reinterpret_cast<CString*>(lParam);
	
	// 线程安全的UI更新
	if (IsWindow(m_ctrlProgress.GetSafeHwnd())) {
		m_ctrlProgress.SetPos(progress);
	}
	
	if (statusText && IsWindow(m_ctrlTransferStatus.GetSafeHwnd())) {
		// 使用统一状态管理更新传输状态
		UpdateStatusDisplay(L"", L"", *statusText, L"", StatusPriority::NORMAL);
		delete statusText; // 释放动态分配的内存
	}
	
	return 0;
}

LRESULT CPortMasterDlg::OnUpdateCompletion(WPARAM wParam, LPARAM lParam)
{
	// wParam表示成功(1)或失败(0)，lParam包含消息文本指针
	BOOL success = static_cast<BOOL>(wParam);
	CString* message = reinterpret_cast<CString*>(lParam);
	
	// 线程安全的UI更新
	if (message) {
		AppendLog(*message);
		delete message; // 释放动态分配的内存
	}
	
	// 🔑 第八轮修订：优化状态同步时序，确保ReliableChannel状态重置在UI更新前完成
	// 先处理可靠传输状态重置，避免时序竞争
	if (success && m_reliableChannel && m_reliableChannel->GetState() == RELIABLE_DONE) {
		// 立即重置ReliableChannel状态，确保后续UI更新能获取到正确状态
		m_reliableChannel->ResetToIdle();
	}
	
	// 📊 使用统一状态管理更新传输完成状态
	if (success) {
		if (IsWindow(m_ctrlProgress.GetSafeHwnd())) {
			m_ctrlProgress.SetPos(0);
		}
		// 传输完成后更新所有状态
		UpdateStatusDisplay(L"● 已连接", L"完成", L"传输完成", L"", StatusPriority::HIGH);
		SetTransmissionState(TransmissionState::COMPLETED);
	} else {
		// 传输失败后更新所有状态
		UpdateStatusDisplay(L"● 已连接", L"失败", L"传输失败", L"", StatusPriority::CRITICAL);
		SetTransmissionState(TransmissionState::FAILED);
	}
	
	// 🔑 关键修复：传输完成后必须更新按钮状态
	// 确保"发送"按钮从"停止"状态恢复到正常的"发送"状态
	// 此时ReliableChannel状态已重置，UpdateButtonStates能获取到正确状态
	UpdateButtonStates();
	
	return 0;
}

LRESULT CPortMasterDlg::OnUpdateFileReceived(WPARAM wParam, LPARAM lParam)
{
	// wParam未使用，lParam包含文件信息结构体指针
	struct FileReceivedData {
		CString filename;
		std::vector<uint8_t> data;
	};
	
	FileReceivedData* info = reinterpret_cast<FileReceivedData*>(lParam);
	
	if (info) {
		// 线程安全的UI更新 - 显示接收到的文件数据
		DisplayReceivedData(info->data);
		
		// 记录日志
		CString logMsg;
		logMsg.Format(L"接收到文件: %s (%zu 字节)", info->filename, info->data.size());
		AppendLog(logMsg);
		
		delete info; // 释放动态分配的内存
	}
	
	return 0;
}

// 🔑 线程安全的数据显示更新消息处理函数
// 🔑 统一传输架构核心：数据显示消息处理
// 优化线程安全性和数据处理逻辑
LRESULT CPortMasterDlg::OnDisplayReceivedDataMsg(WPARAM wParam, LPARAM lParam)
{
	// 🔑 优化1：增强参数验证
	if (lParam == 0) {
		WriteDebugLog("[WARNING] OnDisplayReceivedDataMsg: 接收到空数据指针");
		return -1;
	}
	
	std::vector<uint8_t>* dataPtr = reinterpret_cast<std::vector<uint8_t>*>(lParam);
	
	// 🔑 优化2：数据有效性检查
	if (!dataPtr || dataPtr->empty()) {
		WriteDebugLog("[WARNING] OnDisplayReceivedDataMsg: 数据向量为空或无效");
		if (dataPtr) {
			delete dataPtr; // 确保内存清理
		}
		return -1;
	}
	
	try {
		// 🔑 优化3：增强线程安全的数据追加
		{
			std::lock_guard<std::mutex> lock(m_displayDataMutex);
			
			// 检查数据大小限制（防止内存溢出）
			const size_t MAX_DISPLAY_SIZE = 10 * 1024 * 1024; // 10MB限制
			if (m_displayedData.size() + dataPtr->size() > MAX_DISPLAY_SIZE) {
				// 保留最新的数据，删除旧数据
				size_t keepSize = MAX_DISPLAY_SIZE / 2;
				if (m_displayedData.size() > keepSize) {
					m_displayedData.erase(m_displayedData.begin(), 
						m_displayedData.end() - keepSize);
					WriteDebugLog("[INFO] 显示数据缓冲区已清理，保留最新数据");
				}
			}
			
			// 追加新数据
			m_displayedData.insert(m_displayedData.end(), 
								 dataPtr->begin(), dataPtr->end());
			
			// 🔑 优化8: 缓存接收到的数据到临时数据管理器
			if (m_tempDataManager) {
				std::string identifier = "received_data_" + std::to_string(
					std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now().time_since_epoch()).count());
				m_tempDataManager->CacheData(*dataPtr, identifier);
			}
			
			// 记录数据接收日志
			CString logMsg;
			logMsg.Format(L"[INFO] 接收数据 %zu 字节，总计 %zu 字节", 
				dataPtr->size(), m_displayedData.size());
			WriteDebugLog(CW2A(logMsg));
		}
		
		// 🔑 优化4：统一显示更新流程
		UpdateDataDisplay();
		ScrollToBottom();
		
		// 🔑 优化5：状态同步更新
		UpdateButtonStates();
		UpdateStatusBar(); // 确保状态栏显示最新信息
		
		// 🔑 优化6：安全内存清理
		delete dataPtr;
		dataPtr = nullptr;
		
	} catch (const std::exception& e) {
		// 🔑 优化7：异常处理
		CString errorMsg;
		errorMsg.Format(L"[ERROR] OnDisplayReceivedDataMsg异常: %s", CA2W(e.what()));
		WriteDebugLog(CW2A(errorMsg));
		
		// 确保内存清理
		if (dataPtr) {
			delete dataPtr;
			dataPtr = nullptr;
		}
		return -1;
	} catch (...) {
		// 处理未知异常
		WriteDebugLog("[ERROR] OnDisplayReceivedDataMsg发生未知异常");
		
		// 确保内存清理
		if (dataPtr) {
			delete dataPtr;
			dataPtr = nullptr;
		}
		return -1;
	}
	
	return 0;
}

// =====================================
// 数据格式化方法实现 (SOLID-S: 单一职责)
// =====================================

// 🔑 架构重构完成：已删除FormatDataAsHex和FormatDataAsText包装函数
// 直接调用FormatHexDisplay和FormatTextDisplay以减少代码冗余

// =====================================
// 统一显示管理方法实现 (SOLID-S: 单一职责)
// =====================================

// 🔑 统一传输架构核心：数据显示更新
// 优化性能和稳定性
void CPortMasterDlg::UpdateDataDisplay()
{
	// 🔑 架构重构：使用ManagerIntegration的DataDisplayManager
	// SOLID-S: 单一职责 - 委托显示管理给专职管理器
	if (!m_managerIntegration) {
		WriteDebugLog("[ERROR] UpdateDataDisplay: ManagerIntegration未初始化");
		return;
	}
	
	try {
		// 🔑 优化：线程安全的数据访问
		std::lock_guard<std::mutex> lock(m_displayDataMutex);
		
		if (m_displayedData.empty()) {
			// 使用管理器清空显示
			m_managerIntegration->ClearDataDisplay();
			WriteDebugLog("[INFO] 数据显示已清空（通过管理器）");
			return;
		}
		
		// 🔑 架构优势：性能优化由DataDisplayManager内部处理
		// 设置显示模式
		m_managerIntegration->SetDisplayMode(m_bHexDisplay);
		
		// 更新显示数据 - SOLID-D: 依赖抽象而非具体实现
		DisplayMode mode = m_bHexDisplay ? DisplayMode::MIXED : DisplayMode::TEXT;
		m_managerIntegration->UpdateDataDisplay(m_displayedData, mode);
		
		// 🔑 架构优势：滚动和重绘由DataDisplayManager内部处理
		WriteDebugLog("[INFO] 数据显示已更新（通过管理器）");
		
	} catch (const std::exception& e) {
		// 🔑 架构重构：异常处理简化
		CString errorMsg;
		errorMsg.Format(L"[ERROR] UpdateDataDisplay异常: %s", CA2W(e.what()));
		WriteDebugLog(CW2A(errorMsg));
		
		// 通过管理器显示错误信息
		if (m_managerIntegration) {
			m_managerIntegration->ClearDataDisplay();
		}
		
	} catch (...) {
		// 处理未知异常
		WriteDebugLog("[ERROR] UpdateDataDisplay发生未知异常");
		if (m_managerIntegration) {
			m_managerIntegration->ClearDataDisplay();
		}
	}
}

void CPortMasterDlg::RefreshDataView()
{
	// 刷新数据视图控件状态
	if (!IsWindow(m_ctrlDataView.GetSafeHwnd())) {
		WriteDebugLog("[WARNING] RefreshDataView: 数据视图控件无效");
		return;
	}
	
	// 强制控件重绘
	m_ctrlDataView.Invalidate();
	m_ctrlDataView.UpdateWindow();
	
	WriteDebugLog("[DEBUG] RefreshDataView: 数据视图控件已刷新");
}

// =====================================
// 传输状态管理方法实现 (SOLID-S: 单一职责)
// =====================================

void CPortMasterDlg::SetTransmissionState(TransmissionState newState)
{
	// 状态转换逻辑验证
	TransmissionState oldState = m_transmissionState;
	
	// 记录状态转换
	CString stateNames[] = { L"空闲", L"传输中", L"暂停", L"完成", L"失败" };
	CString logMsg;
	logMsg.Format(L"传输状态转换: %s -> %s", 
		stateNames[static_cast<int>(oldState)], 
		stateNames[static_cast<int>(newState)]);
	AppendLog(logMsg);
	
	// 断点续传逻辑处理 (SOLID-S: 单一职责)
	if (oldState == TransmissionState::TRANSMITTING && newState == TransmissionState::PAUSED)
	{
		// 传输被暂停，保存断点信息
		if (!m_currentFileName.IsEmpty() && !m_transmissionData.empty())
		{
			// 计算已传输字节数（这里简化处理，实际应根据传输进度计算）
			size_t transmittedBytes = static_cast<size_t>(m_transmissionProgress * m_transmissionData.size() / 100.0);
			SaveTransmissionContext(m_currentFileName, m_transmissionData.size(), transmittedBytes);
		}
	}
	else if (newState == TransmissionState::IDLE || newState == TransmissionState::COMPLETED)
	{
		// 传输结束，清除断点信息
		if (m_transmissionContext.isValidContext)
		{
			ClearTransmissionContext();
		}
	}
	
	// 设置新状态
	m_transmissionState = newState;
	
	// 根据状态更新UI
	UpdateButtonStates();
	
	// 同步旧的原子变量状态 (向后兼容)
	m_bTransmitting = (newState == TransmissionState::TRANSMITTING);
}

TransmissionState CPortMasterDlg::GetTransmissionState() const
{
	return m_transmissionState;
}

bool CPortMasterDlg::IsTransmissionActive() const
{
	// 🔑 第八轮修复：强化边界条件处理和状态判断准确性
	bool uiActive = (m_transmissionState == TransmissionState::TRANSMITTING || 
					 m_transmissionState == TransmissionState::PAUSED);
	
	// 在可靠传输模式下，需要精确检查ReliableChannel的状态
	if (m_bReliableMode)
	{
		// 🔑 边界条件检查：确保ReliableChannel指针有效
		if (!m_reliableChannel)
		{
			// 可靠模式但通道无效，仅依赖UI状态
			return uiActive;
		}
		
		ReliableState reliableState = m_reliableChannel->GetState();
		
		// 🔑 关键修复：RELIABLE_DONE和RELIABLE_FAILED应被视为非活跃状态
		// 只有真正在传输过程中的状态才被视为活跃
		bool reliableActive = (reliableState == RELIABLE_STARTING || 
							   reliableState == RELIABLE_SENDING || 
							   reliableState == RELIABLE_ENDING ||
							   reliableState == RELIABLE_RECEIVING);
		
		// ⚠️ 特殊处理：RELIABLE_DONE状态表示刚完成，应立即视为非活跃
		// 这解决了"传输完成后仍显示停止按钮"的问题
		if (reliableState == RELIABLE_DONE || reliableState == RELIABLE_FAILED)
		{
			// 可靠传输已完成或失败，强制设为非活跃
			// 让UI有时间处理完成通知并更新按钮状态
			return false;
		}
		
		// 🔑 状态一致性验证：避免UI状态与可靠传输状态不一致
		// 如果UI显示空闲但可靠传输活跃，优先信任可靠传输状态
		if (!uiActive && reliableActive)
		{
			// 检测到状态不一致，记录调试信息并信任可靠传输状态
			// 这种情况可能发生在状态同步延迟时
			return true;
		}
		
		// 可靠传输模式下，任一层面活跃即认为传输活跃
		return uiActive || reliableActive;
	}
	
	// 非可靠传输模式，只检查UI状态
	return uiActive;
}

// =====================================
// 断点续传功能实现 (SOLID-S: 单一职责, KISS: 简洁的续传逻辑)
// =====================================

void CPortMasterDlg::SaveTransmissionContext(const CString& filePath, size_t totalBytes, size_t transmittedBytes)
{
	// 更新传输上下文信息
	m_transmissionContext.sourceFilePath = filePath;
	m_transmissionContext.totalBytes = totalBytes;
	m_transmissionContext.transmittedBytes = transmittedBytes;
	m_transmissionContext.startTimestamp = GetTickCount();
	m_transmissionContext.lastUpdateTimestamp = GetTickCount();
	m_transmissionContext.isValidContext = true;
	
	// 保存当前目标标识符（端口或网络地址）
	CString targetInfo;
	CString portName;
	if (m_ctrlPortList.GetCurSel() >= 0)
	{
		m_ctrlPortList.GetLBText(m_ctrlPortList.GetCurSel(), portName);
	}
	targetInfo.Format(L"%s:%s", 
		m_ctrlPortType.GetCurSel() == 0 ? L"Serial" : L"Network",
		portName.IsEmpty() ? L"Unknown" : portName);
	m_transmissionContext.targetIdentifier = targetInfo;
	
	// 记录断点保存
	CString logMsg;
	logMsg.Format(L"保存传输断点: %s [%zu/%zu 字节 %.1f%%]", 
		filePath, transmittedBytes, totalBytes, 
		m_transmissionContext.GetProgressPercentage());
	AppendLog(logMsg);
}

bool CPortMasterDlg::LoadTransmissionContext()
{
	// 检查是否有有效的传输上下文
	if (!m_transmissionContext.isValidContext || !m_transmissionContext.CanResume())
	{
		return false;
	}
	
	// 验证源文件是否仍然存在
	if (!PathFileExists(m_transmissionContext.sourceFilePath))
	{
		ClearTransmissionContext();
		AppendLog(L"断点续传失败: 源文件不存在");
		return false;
	}
	
	// 记录断点加载
	CString logMsg;
	logMsg.Format(L"加载传输断点: %s [从 %zu 字节继续，进度 %.1f%%]", 
		m_transmissionContext.sourceFilePath,
		m_transmissionContext.transmittedBytes,
		m_transmissionContext.GetProgressPercentage());
	AppendLog(logMsg);
	
	return true;
}

void CPortMasterDlg::ClearTransmissionContext()
{
	// 重置传输上下文
	m_transmissionContext.Reset();
	
	AppendLog(L"清除传输断点信息");
}

CString CPortMasterDlg::GetTransmissionContextFilePath() const
{
	// 返回传输上下文文件路径，如果无效返回空字符串
	return m_transmissionContext.isValidContext ? m_transmissionContext.sourceFilePath : CString(L"");
}

bool CPortMasterDlg::ResumeTransmission()
{
	// 检查是否可以续传
	if (!LoadTransmissionContext())
	{
		return false;
	}
	
	// 检查连接状态
	if (!m_bConnected)
	{
		AppendLog(L"续传失败: 请先连接端口");
		return false;
	}
	
	// 从断点位置读取剩余文件数据 (SOLID-S: 单一职责, DRY: 避免重复读取)
	CFile file;
	if (!file.Open(m_transmissionContext.sourceFilePath, CFile::modeRead | CFile::typeBinary))
	{
		AppendLog(L"续传失败: 无法打开源文件");
		ClearTransmissionContext();
		return false;
	}
	
	// 移动到断点位置
	try
	{
		file.Seek(static_cast<LONGLONG>(m_transmissionContext.transmittedBytes), CFile::begin);
		
		// 读取剩余数据
		size_t remainingBytes = m_transmissionContext.totalBytes - m_transmissionContext.transmittedBytes;
		m_transmissionData.resize(remainingBytes);
		
		UINT bytesRead = file.Read(m_transmissionData.data(), static_cast<UINT>(remainingBytes));
		if (bytesRead != remainingBytes)
		{
			AppendLog(L"续传失败: 文件读取不完整");
			file.Close();
			ClearTransmissionContext();
			return false;
		}
		
		file.Close();
		
		// 更新当前文件名
		m_currentFileName = PathFindFileName(m_transmissionContext.sourceFilePath);
		
		// 开始续传
		SetTransmissionState(TransmissionState::TRANSMITTING);
		
		// 根据传输模式选择传输方式 (YAGNI: 只实现必需功能)
		if (m_bReliableMode && m_reliableChannel)
		{
			std::string fileNameStr = CT2A(m_currentFileName);
			if (m_reliableChannel->SendFile(fileNameStr, m_transmissionData))
			{
				CString resumeMsg;
				resumeMsg.Format(L"续传开始: %s [从%.1f%%继续]", 
					m_currentFileName, m_transmissionContext.GetProgressPercentage());
				AppendLog(resumeMsg);
				return true;
			}
			else
			{
				SetTransmissionState(TransmissionState::FAILED);
				AppendLog(L"续传失败: 可靠传输启动失败");
				ClearTransmissionContext();
				return false;
			}
		}
		else
		{
			// 普通传输模式
			StartDataTransmission(m_transmissionData);
			CString resumeMsg;
			resumeMsg.Format(L"续传开始: %s [从%.1f%%继续]", 
				m_currentFileName, m_transmissionContext.GetProgressPercentage());
			AppendLog(resumeMsg);
			return true;
		}
	}
	catch (CFileException* e)
	{
		AppendLog(L"续传失败: 文件操作异常");
		e->Delete();
		file.Close();
		ClearTransmissionContext();
		return false;
	}
}

// =====================================
// 停止按钮事件处理方法实现 (SOLID-S: 单一职责)
// =====================================

void CPortMasterDlg::OnBnClickedStop()
{
	// 🔑 关键修复：强制停止任何活跃的传输，绕过状态检查
	TransmissionState currentState = GetTransmissionState();
	
	// 立即停止定时器（优先级最高，确保传输立即停止）
	if (m_transmissionTimer != 0) {
		KillTimer(m_transmissionTimer);
		m_transmissionTimer = 0;
		AppendLog(L"传输定时器已强制停止");
	}
	
	// 根据当前状态执行相应操作
	if (currentState == TransmissionState::TRANSMITTING)
	{
		// 正在传输 -> 直接停止
		StopDataTransmission(false);
		AppendLog(L"传输已立即停止");
	}
	else if (currentState == TransmissionState::PAUSED)
	{
		// 暂停状态 -> 强制停止
		StopDataTransmission(false);
		AppendLog(L"传输已强制停止");
	}
	else if (IsTransmissionActive())
	{
		// 其他活跃状态 -> 强制停止
		StopDataTransmission(false);
		AppendLog(L"传输已强制停止");
	}
	else
	{
		// 非活跃状态也允许操作，重置状态
		SetTransmissionState(TransmissionState::IDLE);
		AppendLog(L"传输状态已重置为空闲");
	}
	
	// 立即更新按钮状态
	UpdateButtonStates();
}

// 🔑 P0-1: 安全的PostMessage封装函数 - 防止MFC断言崩溃
bool CPortMasterDlg::SafePostMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// 🔑 P2-7: 异常处理和崩溃预防机制
	try 
	{
		// 多重安全检查 - 防止winq.cpp:1113断言失败
		if (!::IsWindow(GetSafeHwnd()))
		{
			WriteDebugLog(CT2A(L"[WARNING] SafePostMessage: 窗口句柄无效"));
			return false;
		}
		
		// 检查窗口是否属于当前线程 - 防止跨线程访问问题
		DWORD windowThreadId = ::GetWindowThreadProcessId(GetSafeHwnd(), NULL);
		DWORD currentThreadId = ::GetCurrentThreadId();
		if (windowThreadId != currentThreadId)
		{
			WriteDebugLog(CT2A(L"[WARNING] SafePostMessage: 跨线程访问检测"));
			// 对于跨线程情况，仍然尝试发送，但记录警告
		}
		
		// 双重句柄验证，防止竞态条件
		HWND hWnd = GetSafeHwnd();
		if (!::IsWindow(hWnd))
		{
			WriteDebugLog(CT2A(L"[WARNING] SafePostMessage: 窗口句柄在使用前失效"));
			return false;
		}
		
		// 安全地发送消息
		BOOL result = ::PostMessage(hWnd, message, wParam, lParam);
		if (!result)
		{
			DWORD error = ::GetLastError();
			CString errorMsg;
			errorMsg.Format(L"[ERROR] SafePostMessage失败: 错误码=%lu, 消息=0x%X", error, message);
			WriteDebugLog(CT2A(errorMsg));
			return false;
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		CString errorMsg;
		errorMsg.Format(L"[CRITICAL] SafePostMessage异常: %s", CA2W(e.what()));
		WriteDebugLog(CT2A(errorMsg));
		return false;
	}
	catch (...)
	{
		WriteDebugLog(CT2A(L"[CRITICAL] SafePostMessage未知异常"));
		return false;
	}
}
