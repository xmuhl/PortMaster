#include "stdafx.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "afxdialogex.h"

#pragma execution_character_set("utf-8")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPortMasterDlg 对话框

CPortMasterDlg::CPortMasterDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PORTMASTER_DIALOG, pParent)
	, isConnected_(false)
	, isTransmitting_(false)
	, transmissionStartTime_(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPortMasterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	// TODO: 添加控件DDX绑定
	/*
	DDX_Control(pDX, IDC_COMBO_PORT_TYPE, m_ctrlPortType);
	DDX_Control(pDX, IDC_COMBO_PORT_NAME, m_ctrlPortName);
	DDX_Control(pDX, IDC_EDIT_BAUD_RATE, m_ctrlBaudRate);
	DDX_Control(pDX, IDC_BUTTON_CONNECT, m_ctrlConnect);
	DDX_Control(pDX, IDC_BUTTON_DISCONNECT, m_ctrlDisconnect);
	DDX_Control(pDX, IDC_CHECK_RELIABLE_MODE, m_ctrlReliableMode);
	DDX_Control(pDX, IDC_CHECK_HEX_VIEW, m_ctrlHexView);
	DDX_Control(pDX, IDC_EDIT_SEND_DATA, m_ctrlSendData);
	DDX_Control(pDX, IDC_BUTTON_SEND, m_ctrlSend);
	DDX_Control(pDX, IDC_BUTTON_CLEAR, m_ctrlClear);
	DDX_Control(pDX, IDC_BUTTON_BROWSE, m_ctrlBrowse);
	DDX_Control(pDX, IDC_EDIT_RECEIVE_DATA, m_ctrlReceiveData);
	DDX_Control(pDX, IDC_LIST_LOG, m_ctrlLogList);
	DDX_Control(pDX, IDC_PROGRESS, m_ctrlProgress);
	DDX_Control(pDX, IDC_STATIC_STATUS, m_ctrlStatus);
	*/
}

BEGIN_MESSAGE_MAP(CPortMasterDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_SIZE()
	/*
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CPortMasterDlg::OnBnClickedConnect)
	ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CPortMasterDlg::OnBnClickedDisconnect)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CPortMasterDlg::OnBnClickedSend)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, &CPortMasterDlg::OnBnClickedClear)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, &CPortMasterDlg::OnBnClickedBrowse)
	ON_CBN_SELCHANGE(IDC_COMBO_PORT_TYPE, &CPortMasterDlg::OnCbnSelchangePortType)
	ON_BN_CLICKED(IDC_CHECK_RELIABLE_MODE, &CPortMasterDlg::OnBnClickedReliableMode)
	ON_BN_CLICKED(IDC_CHECK_HEX_VIEW, &CPortMasterDlg::OnBnClickedHexView)
	*/
END_MESSAGE_MAP()

// CPortMasterDlg 消息处理程序

BOOL CPortMasterDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 添加"关于..."菜单项到系统菜单中。

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
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	try {
		// 初始化控件
		InitializeControls();

		// 加载配置
		LoadConfiguration();

		// 设置定时器
		SetTimer(TIMER_UPDATE_UI, 100, NULL);    // UI更新定时器（100ms）
		SetTimer(TIMER_UPDATE_LOG, 1000, NULL);  // 日志更新定时器（1s）

		// 设置窗口标题
		SetWindowText(_T("PortMaster - 端口大师"));

		// 记录初始化完成
		using namespace PortMaster;
		LOG_INFO(_T("MainDlg"), _T("主界面初始化完成"));

	} catch (const std::exception& e) {
		CStringA errorMsg(e.what());
		CString wideError(errorMsg);
		PortMaster::LOG_ERROR(_T("MainDlg"), _T("初始化异常: ") + wideError);
		ShowMessage(_T("主界面初始化失败: ") + wideError, MB_ICONERROR);
	}

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CPortMasterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		// TODO: 显示关于对话框
		ShowMessage(_T("PortMaster v1.0\n端口通信测试工具\n\n支持串口、并口、USB打印、网络打印和回路测试"));
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

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CPortMasterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CPortMasterDlg::OnDestroy()
{
	// 停止定时器
	KillTimer(TIMER_UPDATE_UI);
	KillTimer(TIMER_UPDATE_LOG);

	// 断开连接
	DisconnectPort();

	// 保存配置
	SaveConfiguration();

	// 记录关闭
	PortMaster::LOG_INFO(_T("MainDlg"), _T("主界面关闭"));

	CDialogEx::OnDestroy();
}

void CPortMasterDlg::OnTimer(UINT_PTR nIDEvent)
{
	switch (nIDEvent) {
	case TIMER_UPDATE_UI:
		UpdateConnectionStatus();
		UpdateControlStates();
		break;

	case TIMER_UPDATE_LOG:
		UpdateLogDisplay();
		break;
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CPortMasterDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: 实现控件自适应布局
}

// 控件事件处理（骨架实现）
void CPortMasterDlg::OnBnClickedConnect()
{
	if (!isConnected_) {
		if (ConnectPort()) {
			OnTransportConnected();
		}
	}
}

void CPortMasterDlg::OnBnClickedDisconnect()
{
	if (isConnected_) {
		DisconnectPort();
		OnTransportDisconnected();
	}
}

void CPortMasterDlg::OnBnClickedSend()
{
	if (isConnected_ && !isTransmitting_) {
		SendData();
	}
}

void CPortMasterDlg::OnBnClickedClear()
{
	ClearReceiveArea();
}

void CPortMasterDlg::OnBnClickedBrowse()
{
	// TODO: 实现文件选择对话框
	CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("所有文件 (*.*)|*.*||"));
	if (dlg.DoModal() == IDOK) {
		SendFile(dlg.GetPathName());
	}
}

void CPortMasterDlg::OnCbnSelchangePortType()
{
	RefreshPortList();
	UpdatePortParameters();
}

void CPortMasterDlg::OnBnClickedReliableMode()
{
	// TODO: 切换可靠传输模式
	config_.ui.enableReliableMode = !config_.ui.enableReliableMode;
}

void CPortMasterDlg::OnBnClickedHexView()
{
	// TODO: 切换十六进制显示模式
	config_.ui.hexViewEnabled = !config_.ui.hexViewEnabled;
}

// 私有方法实现（骨架）
void CPortMasterDlg::InitializeControls()
{
	// TODO: 初始化所有控件的状态和数据
	// 设置端口类型组合框
	// 设置波特率默认值
	// 初始化日志列表控件
}

void CPortMasterDlg::LoadConfiguration()
{
	if (configStore_.Load(config_)) {
		PortMaster::LOG_INFO(_T("MainDlg"), _T("配置加载成功"));
	} else {
		PortMaster::LOG_WARNING(_T("MainDlg"), _T("配置加载失败，使用默认配置"));
		configStore_.SetDefaults(config_);
	}
}

void CPortMasterDlg::SaveConfiguration()
{
	if (configStore_.Save(config_)) {
		PortMaster::LOG_INFO(_T("MainDlg"), _T("配置保存成功"));
	} else {
		PortMaster::LOG_ERROR(_T("MainDlg"), _T("配置保存失败"));
	}
}

void CPortMasterDlg::SetupLogList()
{
	// TODO: 设置日志列表控件的列和格式
}

void CPortMasterDlg::RefreshPortList()
{
	// TODO: 根据选择的端口类型刷新端口列表
}

void CPortMasterDlg::UpdatePortParameters()
{
	// TODO: 根据端口类型更新相关参数控件的可用性
}

bool CPortMasterDlg::ConnectPort()
{
	// TODO: 根据当前选择创建传输实例并连接
	try {
		CString portType = GetSelectedPortType();
		if (portType == _T("串口")) {
			transport_ = PortMaster::TransportFactory::CreateSerial();
		} else if (portType == _T("回路测试")) {
			transport_ = PortMaster::TransportFactory::CreateLoopback();
		} else {
			ShowMessage(_T("暂不支持的端口类型: ") + portType, MB_ICONWARNING);
			return false;
		}

		if (!transport_) {
			ShowMessage(_T("创建传输实例失败"), MB_ICONERROR);
			return false;
		}

		PortMaster::PortConfig cfg;
		cfg.portName = GetSelectedPortName();
		cfg.baudRate = GetBaudRate();
		cfg.timeout = 5000;

		PortMaster::TransportContext ctx;
		if (transport_->Open(cfg, ctx)) {
			isConnected_ = true;
			PortMaster::LOG_INFO(_T("MainDlg"), _T("端口连接成功: ") + cfg.portName);
			return true;
		} else {
			PortMaster::LOG_ERROR(_T("MainDlg"), _T("端口连接失败: ") + ctx.errorMessage);
			ShowMessage(_T("连接失败: ") + ctx.errorMessage, MB_ICONERROR);
			return false;
		}

	} catch (const std::exception& e) {
		CStringA errorMsg(e.what());
		CString wideError(errorMsg);
		PortMaster::LOG_ERROR(_T("MainDlg"), _T("连接异常: ") + wideError);
		ShowMessage(_T("连接异常: ") + wideError, MB_ICONERROR);
		return false;
	}
}

void CPortMasterDlg::DisconnectPort()
{
	if (transport_) {
		transport_->Close();
		transport_.reset();
	}

	if (reliableChannel_) {
		reliableChannel_->Reset();
		reliableChannel_.reset();
	}

	isConnected_ = false;
	isTransmitting_ = false;
	PortMaster::LOG_INFO(_T("MainDlg"), _T("端口已断开"));
}

void CPortMasterDlg::SendData()
{
	// TODO: 实现数据发送
}

void CPortMasterDlg::SendFile(const CString& filePath)
{
	// TODO: 实现文件发送
	UNREFERENCED_PARAMETER(filePath);
}

void CPortMasterDlg::ReceiveData()
{
	// TODO: 实现数据接收
}

void CPortMasterDlg::ClearReceiveArea()
{
	// TODO: 清空接收区域
}

void CPortMasterDlg::UpdateConnectionStatus()
{
	// TODO: 更新连接状态显示
}

void CPortMasterDlg::UpdateProgress(DWORD current, DWORD total)
{
	// TODO: 更新进度条
	UNREFERENCED_PARAMETER(current);
	UNREFERENCED_PARAMETER(total);
}

void CPortMasterDlg::UpdateLogDisplay()
{
	// TODO: 更新日志显示
}

void CPortMasterDlg::UpdateControlStates()
{
	// TODO: 根据当前状态更新控件的启用/禁用状态
}

void CPortMasterDlg::OnTransportConnected()
{
	ShowMessage(_T("端口连接成功"));
}

void CPortMasterDlg::OnTransportDisconnected()
{
	ShowMessage(_T("端口已断开"));
}

void CPortMasterDlg::OnTransportError(const CString& error)
{
	ShowMessage(_T("传输错误: ") + error, MB_ICONERROR);
}

void CPortMasterDlg::OnDataReceived(const PortMaster::BufferView& data)
{
	// TODO: 处理接收到的数据
	UNREFERENCED_PARAMETER(data);
}

void CPortMasterDlg::OnTransmissionProgress(DWORD current, DWORD total)
{
	UpdateProgress(current, total);
}

CString CPortMasterDlg::FormatDataForDisplay(const PortMaster::BufferView& data, bool hexMode)
{
	// TODO: 格式化数据以供显示
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(hexMode);
	return _T("");
}

CString CPortMasterDlg::GetSelectedPortType()
{
	// TODO: 获取选择的端口类型
	return _T("回路测试"); // 临时返回
}

CString CPortMasterDlg::GetSelectedPortName()
{
	// TODO: 获取选择的端口名称
	return _T("LOOPBACK"); // 临时返回
}

DWORD CPortMasterDlg::GetBaudRate()
{
	// TODO: 获取设置的波特率
	return 9600; // 临时返回
}

void CPortMasterDlg::ShowMessage(const CString& message, UINT type)
{
	AfxMessageBox(message, type);
}
