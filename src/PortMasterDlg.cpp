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
	: CDialogEx(IDD_PORTMASTER_DIALOG, pParent), m_isConnected(false), m_transmissionState(TransmissionState::IDLE), m_pauseTransmission(false), m_stopTransmission(false), m_bytesSent(0), m_bytesReceived(0), m_sendSpeed(0), m_receiveSpeed(0), m_transport(nullptr), m_reliableChannel(nullptr), m_configStore(ConfigStore::GetInstance())
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

	// 初始化单选按钮
	m_radioReliable.SetCheck(BST_CHECKED);
	m_radioDirect.SetCheck(BST_UNCHECKED);

	// 初始化进度条
	m_progress.SetRange(0, 100);
	m_progress.SetPos(0);

	// 显示初始状态
	LogMessage(_T("程序启动成功"));
	SetDlgItemText(IDC_STATIC_PORT_STATUS, _T("未连接"));
	SetDlgItemText(IDC_STATIC_MODE, _T("可靠"));
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

	// 初始化显示相关的成员变量
	m_sendCacheValid = false;
	m_receiveCacheValid = false;
	m_needUpdateReceiveDisplay = false;
	m_receiveDisplayLines.clear();

	// 初始化传输状态和界面
	UpdateSendButtonState();
	UpdateStatusDisplay();

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
	if (nIDEvent == 1)
	{
		// 定时器1：恢复连接状态显示
		KillTimer(1);
		UpdateConnectionStatus();

		// 重置进度条
		m_progress.SetPos(0);
	}
	else if (nIDEvent == 2)
	{
		// 定时器2：延迟显示100%进度，改善进度条与数据显示的同步
		KillTimer(2);
		m_progress.SetPos(100);
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CPortMasterDlg::PostNcDestroy()
{
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
	TransmissionState currentState = m_transmissionState.load();
	
	switch (currentState)
	{
	case TransmissionState::IDLE:
	case TransmissionState::STOPPED:
		// 开始传输
		{
			// 检查是否已连接
			if (!m_isConnected)
			{
				MessageBox(_T("请先连接端口"), _T("提示"), MB_OK | MB_ICONWARNING);
				return;
			}

			// 检查缓存有效性
			if (!m_sendCacheValid || m_sendDataCache.empty())
			{
				MessageBox(_T("没有可发送的数据，请先输入数据"), _T("提示"), MB_OK | MB_ICONWARNING);
				return;
			}

			// 重置控制标志
			m_pauseTransmission = false;
			m_stopTransmission = false;
			
			// 开始传输
			UpdateTransmissionState(TransmissionState::TRANSMITTING);
			PerformDataTransmission();
		}
		break;
		
	case TransmissionState::TRANSMITTING:
		// 暂停传输
		PauseTransmission();
		break;
		
	case TransmissionState::PAUSED:
		// 恢复传输
		ResumeTransmission();
		break;
		
	case TransmissionState::STOPPING:
		// 正在停止中，忽略按钮点击
		break;
	}
}

void CPortMasterDlg::OnBnClickedButtonStop()
{
	// 检查是否有传输在进行
	if (!IsTransmissionActive())
	{
		MessageBox(_T("当前没有传输正在进行"), _T("提示"), MB_OK | MB_ICONINFORMATION);
		return;
	}

	// 显示确认对话框
	if (ConfirmStopTransmission())
	{
		// 用户确认停止传输
		StopTransmission();
	}
}

void CPortMasterDlg::OnBnClickedButtonFile()
{
	// 文件选择对话框
	CFileDialog dlg(TRUE, _T("txt"), NULL,
					OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
					_T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||"));

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
	// 添加调试日志
	WriteLog("LoadDataFromFile: 开始加载文件 - " + std::string(CT2A(filePath)));
	
	CFile file;
	if (file.Open(filePath, CFile::modeRead))
	{
		ULONGLONG fileLength = file.GetLength();
		WriteLog("LoadDataFromFile: 文件大小=" + std::to_string(fileLength));
		
		if (fileLength > 0)
		{
			BYTE *fileBuffer = new BYTE[(size_t)fileLength + 2];
			file.Read(fileBuffer, (UINT)fileLength);
			fileBuffer[fileLength] = 0;
			fileBuffer[fileLength + 1] = 0;

			// 获取文件扩展名检测二进制文件
			CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
			CString fileExt = fileName.Mid(fileName.ReverseFind('.') + 1);
			
			// 使用扩展名和魔数双重检测文件类型
			bool isBinaryFileByExt = IsBinaryFileByExtension(fileExt);
			bool isBinaryFileByMagic = IsBinaryFileByMagic(fileBuffer, (size_t)fileLength);
			bool isBinaryFile = isBinaryFileByExt || isBinaryFileByMagic;
			
			// 检查是否为大文件（超过1MB）
			bool isLargeFile = fileLength > 1048576; // 1MB = 1024 * 1024
			size_t displayLength = static_cast<size_t>(fileLength);
			bool isPreviewMode = false;
			
			if (isLargeFile && isBinaryFile)
			{
				// 大二进制文件：仅显示前1KB内容
				displayLength = static_cast<size_t>((std::min)(fileLength, static_cast<ULONGLONG>(1024)));
				isPreviewMode = true;
			}
			else if (isLargeFile && !isBinaryFile)
			{
				// 大文本文件：仅显示前1KB内容
				displayLength = static_cast<size_t>((std::min)(fileLength, static_cast<ULONGLONG>(1024)));
				isPreviewMode = true;
			}

			CString fileContent;
			
			if (isBinaryFile)
			{
				// 二进制文件：保持原始字节数据用于16进制显示
				// 不使用fileContent，而是直接使用原始字节数据
			}
			else
			{
				// 文本文件：使用GBK编码转换（仅转换显示部分）
				int wideLen = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)fileBuffer, (int)displayLength, NULL, 0);
				if (wideLen > 0)
				{
					wchar_t *wideStr = new wchar_t[wideLen + 1];
					MultiByteToWideChar(CP_ACP, 0, (LPCSTR)fileBuffer, (int)displayLength, wideStr, wideLen);
					wideStr[wideLen] = 0;
					fileContent = wideStr;
					delete[] wideStr;
				}
				else
				{
					// 编码失败，使用原始字节（仅显示部分）
					for (size_t i = 0; i < displayLength; i++)
					{
						fileContent += (TCHAR)fileBuffer[i];
					}
				}
				
				// 如果是预览模式，添加截断提示
				if (isPreviewMode)
				{
					fileContent += _T("\r\n\r\n[文件过大，仅显示前1KB内容作为预览]");
				}
			}

			// 根据文件类型处理显示
			if (isBinaryFile)
			{
				// 二进制文件：仅显示部分内容（用于预览），但传输时使用完整文件
				CString hexContent = BytesToHex(fileBuffer, displayLength);
				
				// 如果是预览模式，添加提示信息
				if (isPreviewMode)
				{
					CString sizeInfo;
					sizeInfo.Format(_T("\r\n\r\n[大数据文件预览（部分内容） - 文件大小: %I64u 字节, 显示前 %zu 字节]"), 
						fileLength, displayLength);
					hexContent += sizeInfo;
				}
				
				m_editSendData.SetWindowText(hexContent);
				
				// 对于二进制文件，缓存完整文件数据用于传输，不管显示多少
				WriteLog("LoadDataFromFile: 二进制文件，调用UpdateSendCacheFromBytes");
				UpdateSendCacheFromBytes(fileBuffer, (size_t)fileLength);
			}
			else
			{
				// 文本文件：正常处理
				WriteLog("LoadDataFromFile: 文本文件，调用UpdateSendCache");
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
			}

			delete[] fileBuffer;

			// 更新来源显示，包含文件大小和预览状态信息
			CString sourceInfo;
			if (isPreviewMode)
			{
				sourceInfo.Format(_T("来源: 文件 (%s, %I64u字节, 预览模式)"), 
					fileName, fileLength);
			}
			else
			{
				sourceInfo.Format(_T("来源: 文件 (%s, %I64u字节)"), 
					fileName, fileLength);
			}
			m_staticSendSource.SetWindowText(sourceInfo);
		}
		else
		{
			m_editSendData.SetWindowText(_T(""));
			m_staticSendSource.SetWindowText(_T("来源: 文件"));
		}

		file.Close();
		
		// 更新发送按钮状态以反映文件导入后的缓存变化
		UpdateSendButtonState();
	}
	else
	{
		MessageBox(_T("无法打开文件"), _T("错误"), MB_OK | MB_ICONERROR);
	}
}

bool CPortMasterDlg::IsBinaryFileByMagic(const BYTE* data, size_t length)
{
	if (data == nullptr || length < 4)
		return false;
		
	// 常见文件格式的魔数（文件头）检测
	// ZIP文件: PK (0x50 0x4B)
	if (length >= 2 && data[0] == 0x50 && data[1] == 0x4B)
		return true;
		
	// RAR文件: Rar! (0x52 0x61 0x72 0x21)
	if (length >= 4 && data[0] == 0x52 && data[1] == 0x61 && data[2] == 0x72 && data[3] == 0x21)
		return true;
		
	// 7Z文件: 7z (0x37 0x7A 0xBC 0xAF 0x27 0x1C)
	if (length >= 6 && data[0] == 0x37 && data[1] == 0x7A && data[2] == 0xBC && 
		data[3] == 0xAF && data[4] == 0x27 && data[5] == 0x1C)
		return true;
		
	// PDF文件: %PDF (0x25 0x50 0x44 0x46)
	if (length >= 4 && data[0] == 0x25 && data[1] == 0x50 && data[2] == 0x44 && data[3] == 0x46)
		return true;
		
	// PNG文件: \x89PNG\r\n\x1a\n
	if (length >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
		data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A)
		return true;
		
	// JPEG文件: \xFF\xD8\xFF
	if (length >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
		return true;
		
	// GIF文件: GIF87a 或 GIF89a
	if (length >= 6 && data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46 &&
		((data[3] == 0x38 && data[4] == 0x37 && data[5] == 0x61) ||
		 (data[3] == 0x38 && data[4] == 0x39 && data[5] == 0x61)))
		return true;
		
	// BMP文件: BM (0x42 0x4D)
	if (length >= 2 && data[0] == 0x42 && data[1] == 0x4D)
		return true;
		
	// ICO文件: \x00\x00\x01\x00
	if (length >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00)
		return true;
		
	// EXE/DLL文件: MZ (0x4D 0x5A)
	if (length >= 2 && data[0] == 0x4D && data[1] == 0x5A)
		return true;
		
	// MP3文件: ID3 (0x49 0x44 0x33) 或 \xFF\xFB
	if (length >= 3 && ((data[0] == 0x49 && data[1] == 0x44 && data[2] == 0x33) ||
		(data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)))
		return true;
		
	// WAV文件: RIFF...WAVE
	if (length >= 12 && data[0] == 0x52 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x46 &&
		data[8] == 0x57 && data[9] == 0x41 && data[10] == 0x56 && data[11] == 0x45)
		return true;
		
	// AVI文件: RIFF...AVI
	if (length >= 11 && data[0] == 0x52 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x46 &&
		data[8] == 0x41 && data[9] == 0x56 && data[10] == 0x49)
		return true;
		
	// 检测是否包含大量不可打印字符（通用二进制文件检测）
	size_t nullCount = 0;
	size_t controlCount = 0;
	size_t checkLength = (std::min)(length, (size_t)512); // 检查前512字节
	
	for (size_t i = 0; i < checkLength; i++)
	{
		if (data[i] == 0x00)
			nullCount++;
		else if (data[i] < 0x20 && data[i] != 0x09 && data[i] != 0x0A && data[i] != 0x0D)
			controlCount++;
	}
	
	// 如果空字符或控制字符超过10%，认为是二进制文件
	return (nullCount + controlCount) > (checkLength / 10);
}

bool CPortMasterDlg::IsBinaryFileByExtension(const CString &fileExt)
{
	CString ext = fileExt;
	ext.MakeLower();
	
	return (ext == _T("prn") || ext == _T("bin") || 
			ext == _T("dat") || ext == _T("exe") || 
			ext == _T("dll") || ext == _T("img") ||
			// 压缩文件格式
			ext == _T("zip") || ext == _T("rar") || ext == _T("7z") ||
			ext == _T("tar") || ext == _T("gz") || ext == _T("bz2") ||
			// 图像文件格式
			ext == _T("jpg") || ext == _T("jpeg") || ext == _T("png") ||
			ext == _T("gif") || ext == _T("bmp") || ext == _T("tiff") ||
			ext == _T("webp") || ext == _T("ico") ||
			// 音频文件格式
			ext == _T("mp3") || ext == _T("wav") || ext == _T("flac") ||
			ext == _T("aac") || ext == _T("ogg") || ext == _T("wma") ||
			// 视频文件格式
			ext == _T("mp4") || ext == _T("avi") || ext == _T("mkv") ||
			ext == _T("mov") || ext == _T("wmv") || ext == _T("flv") ||
			// 文档文件格式
			ext == _T("pdf") || ext == _T("doc") || ext == _T("docx") ||
			ext == _T("xls") || ext == _T("xlsx") || ext == _T("ppt") ||
			ext == _T("pptx") ||
			// 其他常见二进制格式
			ext == _T("iso") || ext == _T("dmg") || ext == _T("deb") ||
			ext == _T("rpm") || ext == _T("msi") || ext == _T("app") ||
			ext == _T("apk") || ext == _T("ipa") || ext == _T("cab") ||
			ext == _T("sys") || ext == _T("drv") || ext == _T("com") ||
			ext == _T("scr") || ext == _T("ocx") || ext == _T("cpl"));
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

	// 接收数据显示模式切换：直接从缓存更新显示
	if (m_receiveCacheValid && !m_receiveDataCache.empty())
	{
		// 直接使用UpdateReceiveDisplayFromCache来正确处理16进制格式化
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

CString CPortMasterDlg::BytesToHex(const BYTE* data, size_t length)
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
		// 根据当前显示模式从缓存更新显示
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 十六进制模式：将字节缓存转换为十六进制显示
			// 直接将字节数据转换为十六进制显示，避免字符串转换问题
			CString hexDisplay;
			CString asciiStr;

			for (size_t i = 0; i < m_sendDataCache.size(); i++)
			{
				uint8_t byte = m_sendDataCache[i];

				// 添加偏移地址（每16字节一行）
				if (i % 16 == 0)
				{
					if (i > 0)
					{
						// 添加ASCII显示部分
						hexDisplay += _T("  |");
						hexDisplay += asciiStr;
						hexDisplay += _T("|\r\n");
						asciiStr.Empty();
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

			m_editSendData.SetWindowText(hexDisplay);
		}
		else
		{
			// 文本模式：将字节缓存转换为文本显示
			// 创建包含null终止符的副本，确保字符串安全
			std::vector<char> safeData(m_sendDataCache.begin(), m_sendDataCache.end());
			safeData.push_back('\0');

			// 尝试将数据作为UTF-8字符串解码
			try
			{
				CA2T utf8Text(safeData.data(), CP_UTF8);
				m_editSendData.SetWindowText(CString(utf8Text));
			}
			catch (...)
			{
				// 如果UTF-8解码失败，显示可打印字符
				CString textDisplay;
				for (uint8_t byte : m_sendDataCache)
				{
					if (byte >= 32 && byte <= 126)
					{
						textDisplay += (TCHAR)byte;
					}
					else if (byte == 0)
					{
						textDisplay += _T("[NUL]");
					}
					else
					{
						textDisplay += _T(".");
					}
				}
				m_editSendData.SetWindowText(textDisplay);
			}
		}
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
			m_editSendData.SetWindowText(_T(""));
		}
	}
}

void CPortMasterDlg::UpdateReceiveDisplayFromCache()
{
	// 添加调试输出：记录显示更新过程
	this->WriteLog("=== UpdateReceiveDisplayFromCache 开始 ===");
	this->WriteLog("更新接收显示 - 缓存状态: " + std::string(m_receiveCacheValid ? "有效" : "无效"));
	this->WriteLog("更新接收显示 - 缓存数据大小: " + std::to_string(m_receiveDataCache.size()) + " 字节");
	this->WriteLog("更新接收显示 - 当前显示模式: " + std::string(m_checkHex.GetCheck() == BST_CHECKED ? "十六进制" : "文本"));

	// 如果接收缓存无效，直接返回
	if (!m_receiveCacheValid || m_receiveDataCache.empty())
	{
		this->WriteLog("更新接收显示 - 缓存无效或为空，清空显示");
		m_editReceiveData.SetWindowText(_T(""));
		this->WriteLog("=== UpdateReceiveDisplayFromCache 结束 ===");
		return;
	}

	// 根据当前显示模式从缓存更新显示
	if (m_checkHex.GetCheck() == BST_CHECKED)
	{
		// 十六进制模式：将字节缓存转换为十六进制显示
		// 修复：移除错误的数据格式检测逻辑，直接进行十六进制格式化
		// 直接将字节数据转换为十六进制显示，避免字符串转换问题
		CString hexDisplay;
		CString asciiStr;

		for (size_t i = 0; i < m_receiveDataCache.size(); i++)
		{
			uint8_t byte = m_receiveDataCache[i];

			// 添加偏移地址（每16字节一行）
			if (i % 16 == 0)
			{
				if (i > 0)
				{
					// 添加ASCII显示部分
					hexDisplay += _T("  |");
					hexDisplay += asciiStr;
					hexDisplay += _T("|\r\n");
					asciiStr.Empty();
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
		this->WriteLog("显示更新 - 开始智能编码检测...");
		
		std::vector<char> safeData(m_receiveDataCache.begin(), m_receiveDataCache.end());
		safeData.push_back('\0');
		
		CString displayText;
		bool decoded = false;
		
		// 方案1：尝试UTF-8解码
		try
		{
			CA2T utf8Text(safeData.data(), CP_UTF8);
			CString testResult = CString(utf8Text);
			
			// 修复：移除不合理的长度限制，只要解码结果非空即可接受
			if (!testResult.IsEmpty())
			{
				displayText = testResult;
				decoded = true;
				this->WriteLog("显示更新 - UTF-8解码成功，文本长度: " + std::to_string(displayText.GetLength()));
			}
		}
		catch (...)
		{
			this->WriteLog("显示更新 - UTF-8解码失败");
		}
		
		// 方案2：如果UTF-8失败，尝试GBK解码
		if (!decoded)
		{
			try
			{
				CA2T gbkText(safeData.data(), CP_ACP);
				CString testResult = CString(gbkText);
				
				// 修复：移除不合理的长度限制，只要解码结果非空即可接受
				if (!testResult.IsEmpty())
				{
					displayText = testResult;
					decoded = true;
					this->WriteLog("显示更新 - GBK解码成功，文本长度: " + std::to_string(displayText.GetLength()));
				}
			}
			catch (...)
			{
				this->WriteLog("显示更新 - GBK解码失败");
			}
		}
		
		// 方案3：如果所有编码都失败，显示可打印字符
		if (!decoded)
		{
			this->WriteLog("显示更新 - 使用原始字节显示");
			for (size_t i = 0; i < m_receiveDataCache.size(); i++)
			{
				uint8_t byte = m_receiveDataCache[i];
				displayText += (TCHAR)byte;
			}
		}
		
		m_editReceiveData.SetWindowText(displayText);
	}

	this->WriteLog("=== UpdateReceiveDisplayFromCache 结束 ===");
}

void CPortMasterDlg::UpdateSendCache(const CString &data)
{
	// 添加调试日志
	WriteLog("UpdateSendCache: 输入数据长度=" + std::to_string(data.GetLength()));
	
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

	WriteLog("UpdateSendCache: 转换后缓存大小=" + std::to_string(m_sendDataCache.size()));
	m_sendCacheValid = true;
	WriteLog("UpdateSendCache: 缓存状态设置为有效");
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

void CPortMasterDlg::UpdateSendCacheFromHex(const CString &hexData)
{
	// 添加调试日志
	WriteLog("UpdateSendCacheFromHex: 输入数据长度=" + std::to_string(hexData.GetLength()));
	
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
		WriteLog("UpdateSendCacheFromHex: 无有效十六进制数据");
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

	WriteLog("UpdateSendCacheFromHex: 解析后缓存大小=" + std::to_string(m_sendDataCache.size()));
	m_sendCacheValid = true;
	WriteLog("UpdateSendCacheFromHex: 缓存状态设置为有效");
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

	// 同时写入临时缓存文件（如果已初始化）
	if (m_useTempCacheFile && m_tempCacheFile.is_open())
	{
		this->WriteLog("更新接收缓存 - 写入临时缓存文件...");
		bool writeResult = WriteDataToTempCache(data);
		this->WriteLog("更新接收缓存 - 临时缓存文件写入结果: " + std::string(writeResult ? "成功" : "失败"));
	}
	else
	{
		this->WriteLog("更新接收缓存 - 临时缓存文件未启用或未打开");
	}

	this->WriteLog("=== UpdateReceiveCache 结束 ===");
}

void CPortMasterDlg::AddReceiveDataToDisplayBuffer(const std::vector<uint8_t> &data)
{
	if (data.empty())
		return;
		
	// 将接收的数据按行分割并添加到显示缓冲区
	std::string displayLine;
	
	if (m_checkHex.GetCheck() == BST_CHECKED)
	{
		// 十六进制模式：将字节转换为十六进制字符串
		for (size_t i = 0; i < data.size(); i++)
		{
			char hexByte[4];
			sprintf_s(hexByte, "%02X ", data[i]);
			displayLine += hexByte;
			
			// 每16个字节换行
			if ((i + 1) % 16 == 0)
			{
				// 添加到显示缓冲区
				m_receiveDisplayLines.push_back(displayLine);
				
				// 如果超过最大行数，删除最旧的行
				if (m_receiveDisplayLines.size() > MAX_DISPLAY_LINES)
				{
					m_receiveDisplayLines.pop_front();
				}
				
				displayLine.clear();
			}
		}
		
		// 如果还有剩余数据（不足16字节的行）
		if (!displayLine.empty())
		{
			m_receiveDisplayLines.push_back(displayLine);
			if (m_receiveDisplayLines.size() > MAX_DISPLAY_LINES)
			{
				m_receiveDisplayLines.pop_front();
			}
		}
	}
	else
	{
		// 文本模式：按字符处理
		for (size_t i = 0; i < data.size(); i++)
		{
			char ch = static_cast<char>(data[i]);
			
			if (ch == '\n')
			{
				// 遇到换行符，添加当前行到缓冲区
				m_receiveDisplayLines.push_back(displayLine);
				
				// 如果超过最大行数，删除最旧的行
				if (m_receiveDisplayLines.size() > MAX_DISPLAY_LINES)
				{
					m_receiveDisplayLines.pop_front();
				}
				
				displayLine.clear();
			}
			else if (ch == '\r')
			{
				// 忽略回车符
				continue;
			}
			else if (ch >= 32 && ch <= 126)
			{
				// 可打印字符
				displayLine += ch;
			}
			else
			{
				// 不可打印字符用点表示
				displayLine += '.';
			}
			
			// 如果行太长，强制换行（避免界面卡顿）
			if (displayLine.length() >= 80)
			{
				m_receiveDisplayLines.push_back(displayLine);
				if (m_receiveDisplayLines.size() > MAX_DISPLAY_LINES)
				{
					m_receiveDisplayLines.pop_front();
				}
				displayLine.clear();
			}
		}
		
		// 如果还有剩余数据且不是以换行符结束
		if (!displayLine.empty())
		{
			// 暂存到最后一行（不换行）
			if (!m_receiveDisplayLines.empty())
			{
				// 追加到最后一行
				m_receiveDisplayLines.back() += displayLine;
				
				// 检查最后一行是否过长
				if (m_receiveDisplayLines.back().length() >= 80)
				{
					m_receiveDisplayLines.push_back("");
					if (m_receiveDisplayLines.size() > MAX_DISPLAY_LINES)
					{
						m_receiveDisplayLines.pop_front();
					}
				}
			}
			else
			{
				// 第一行
				m_receiveDisplayLines.push_back(displayLine);
			}
		}
	}
	
	// 标记需要更新显示
	m_needUpdateReceiveDisplay = true;
}

void CPortMasterDlg::UpdateReceiveDisplayFromBuffer()
{
	if (!m_needUpdateReceiveDisplay)
		return;
		
	// 将显示缓冲区的内容合并为一个字符串
	std::string displayText;
	
	for (const auto& line : m_receiveDisplayLines)
	{
		displayText += line;
		displayText += "\r\n";
	}
	
	// 转换为CString并设置到编辑框
	CString displayContent(displayText.c_str());
	m_editReceiveData.SetWindowText(displayContent);
	
	// 滚动到底部
	ScrollToBottomReceiveEdit();
	
	// 清除更新标记
	m_needUpdateReceiveDisplay = false;
}

void CPortMasterDlg::ScrollToBottomReceiveEdit()
{
	// 获取编辑框的行数
	int lineCount = m_editReceiveData.GetLineCount();
	
	// 滚动到最后一行
	if (lineCount > 0)
	{
		m_editReceiveData.LineScroll(lineCount);
		
		// 设置光标到最后
		int textLength = m_editReceiveData.GetWindowTextLength();
		m_editReceiveData.SetSel(textLength, textLength);
	}
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
	// 当用户在发送数据编辑框中输入内容时，更新缓存
	CString currentData;
	m_editSendData.GetWindowText(currentData);

	// 添加调试日志
	WriteLog("OnEnChangeEditSendData: dataLength=" + std::to_string(currentData.GetLength()));

	if (!currentData.IsEmpty())
	{
		// 根据当前模式更新缓存
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 十六进制模式：解析十六进制字符串为字节数据
			WriteLog("OnEnChangeEditSendData: 十六进制模式，更新缓存");
			UpdateSendCacheFromHex(currentData);
		}
		else
		{
			// 文本模式：直接缓存文本数据
			WriteLog("OnEnChangeEditSendData: 文本模式，更新缓存");
			UpdateSendCache(currentData);
		}
	}
	else
	{
		// 如果编辑框为空，清空缓存
		WriteLog("OnEnChangeEditSendData: 编辑框为空，清空缓存");
		m_sendDataCache.clear();
		m_sendCacheValid = false;
	}
	
	// 更新发送按钮状态以反映缓存变化
	UpdateSendButtonState();
}

void CPortMasterDlg::OnBnClickedButtonClearAll()
{
	// 清空发送数据编辑框
	m_editSendData.SetWindowText(_T(""));

	// 清空发送数据缓存（关键修复）
	m_sendDataCache.clear();
	m_sendCacheValid = false;

	// 重置数据源显示
	SetDlgItemText(IDC_STATIC_SEND_SOURCE, _T("来源: 手动输入"));

	// 更新发送按钮状态以反映缓存清空
	UpdateSendButtonState();

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
	
	// 清空显示缓冲区
	m_receiveDisplayLines.clear();
	m_needUpdateReceiveDisplay = false;

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

	// 优先从临时缓存文件读取原始数据（如果可用）
	if (m_useTempCacheFile && !m_tempCacheFilePath.IsEmpty())
	{
		this->WriteLog("尝试从临时缓存文件读取数据...");
		this->WriteLog("临时缓存文件路径: " + std::string(CT2A(m_tempCacheFilePath)));
		this->WriteLog("临时缓存文件是否存在: " + std::string(PathFileExists(m_tempCacheFilePath) ? "是" : "否"));

		std::vector<uint8_t> cachedData = ReadAllDataFromTempCache();
		this->WriteLog("从临时缓存读取数据大小: " + std::to_string(cachedData.size()) + " 字节");
		this->WriteLog("临时缓存文件累计接收字节数: " + std::to_string(m_totalReceivedBytes));

		if (!cachedData.empty())
		{
			this->WriteLog("保存策略：直接保存原始字节数据，避免编码转换问题");
			
			// 无论是十六进制还是文本模式，都直接保存原始二进制数据
			// 这样可以确保数据的100%完整性，避免所有编码转换问题
			binaryData = cachedData;
			isBinaryMode = true;
			
			if (m_checkHex.GetCheck() == BST_CHECKED)
			{
				this->WriteLog("十六进制模式：保存原始二进制数据");
			}
			else
			{
				this->WriteLog("文本模式：保存原始二进制数据（用户可用任何编辑器以正确编码打开）");
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

	// 根据数据类型选择合适的文件对话框和保存方法
	if (isBinaryMode && !binaryData.empty())
	{
		this->WriteLog("准备保存原始字节数据，大小: " + std::to_string(binaryData.size()) + " 字节");
		
		// 提供多种文件格式选择，包括原始编码的文本文件
		CFileDialog dlg(FALSE, _T("txt"), _T("ReceiveData"),
						OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
						_T("原始文本文件 (*.txt)|*.txt|日志文件 (*.log)|*.log|数据文件 (*.dat)|*.dat|二进制文件 (*.bin)|*.bin|所有文件 (*.*)|*.*||"));

		if (dlg.DoModal() == IDOK)
		{
			SaveBinaryDataToFile(dlg.GetPathName(), binaryData);
		}
	}
	else if (!saveData.IsEmpty())
	{
		this->WriteLog("准备保存文本数据，长度: " + std::to_string(saveData.GetLength()) + " 字符");
		// 文本模式：保存为文本文件
		CFileDialog dlg(FALSE, _T("txt"), _T("ReceiveData"),
						OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
						_T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||"));

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
	CString preview = data.Left(100);  // 前100个字符
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

			CString message;
			message.Format(_T("原始数据保存成功 (%zu 字节)"), data.size());
			MessageBox(message, _T("提示"), MB_OK | MB_ICONINFORMATION);

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
														   { OnReliableProgress(static_cast<uint32_t>((current * 100) / total)); });

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
														   { OnReliableProgress(static_cast<uint32_t>((current * 100) / total)); });

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
														   { OnReliableProgress(static_cast<uint32_t>((current * 100) / total)); });

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
														   { OnReliableProgress(static_cast<uint32_t>((current * 100) / total)); });

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
														   { OnReliableProgress(static_cast<uint32_t>((current * 100) / total)); });

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
	UpdateTransmissionState(TransmissionState::IDLE);
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
	// 添加调试输出：记录接收数据处理流程
	this->WriteLog("=== OnTransportDataReceived 开始 ===");
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

	// 在主线程中更新UI
	PostMessage(WM_USER + 1, 0, (LPARAM) new std::vector<uint8_t>(data));

	this->WriteLog("=== OnTransportDataReceived 结束 ===");
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
	UpdateTransmissionState(TransmissionState::IDLE);

	if (success)
	{
		// 传输完成 - 显示传输完成消息
		CString completeStatus;
		completeStatus.Format(_T("传输完成"));
		m_staticPortStatus.SetWindowText(completeStatus);
		
		// 修复：延迟进度条完成显示，给接收数据显示一些时间
		// 先设置95%，1秒后再设置100%，改善用户体验
		m_progress.SetPos(95);
		SetTimer(2, 1000, NULL); // 使用定时器ID=2 延迟1秒显示100%

		// 2秒后恢复连接状态显示并重置进度条
		SetTimer(1, 2000, NULL);
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
}

void CPortMasterDlg::OnReliableStateChanged(bool connected)
{
	// 连接状态变化回调 - 只处理连接状态，不显示传输完成消息
	if (connected)
	{
		// 连接成功 - 更新状态显示和内部连接状态
		CString statusText = _T("已连接");
		m_staticPortStatus.SetWindowText(statusText);
		m_isConnected = true;
		WriteLog("OnReliableStateChanged: 可靠传输连接成功，m_isConnected设置为true");
	}
	else
	{
		// 连接断开 - 更新状态显示和内部连接状态
		CString statusText = _T("连接断开");
		m_staticPortStatus.SetWindowText(statusText);
		m_progress.SetPos(0);
		m_isConnected = false;
		WriteLog("OnReliableStateChanged: 可靠传输连接断开，m_isConnected设置为false");
	}
	
	// 更新发送按钮状态以反映连接状态变化
	UpdateSendButtonState();
}

// 传输控制方法实现

void CPortMasterDlg::UpdateTransmissionState(TransmissionState newState)
{
	TransmissionState oldState = m_transmissionState.exchange(newState);
	
	if (oldState != newState)
	{
		// 状态发生变化，更新界面
		UpdateSendButtonState();
		UpdateStatusDisplay();
		
		// 记录状态变化
		std::string stateNames[] = {"IDLE", "TRANSMITTING", "PAUSED", "STOPPING", "STOPPED"};
		WriteLog("传输状态变化: " + stateNames[static_cast<int>(oldState)] + " -> " + stateNames[static_cast<int>(newState)]);
	}
}

void CPortMasterDlg::UpdateSendButtonState()
{
	TransmissionState currentState = m_transmissionState.load();
	
	// 添加调试日志
	bool connected = m_isConnected;
	bool cacheValid = m_sendCacheValid;
	size_t cacheSize = m_sendDataCache.size();
	
	std::string debugMsg = "UpdateSendButtonState: state=" + std::to_string(static_cast<int>(currentState)) + 
		", connected=" + std::to_string(connected) + 
		", cacheValid=" + std::to_string(cacheValid) + 
		", cacheSize=" + std::to_string(cacheSize);
	WriteLog(debugMsg);
	
	switch (currentState)
	{
	case TransmissionState::IDLE:
	case TransmissionState::STOPPED:
		m_btnSend.SetWindowText(_T("发送"));
		m_btnSend.EnableWindow(connected && cacheValid && (cacheSize > 0));
		break;
		
	case TransmissionState::TRANSMITTING:
		m_btnSend.SetWindowText(_T("中断"));
		m_btnSend.EnableWindow(TRUE);
		break;
		
	case TransmissionState::PAUSED:
		m_btnSend.SetWindowText(_T("继续"));
		m_btnSend.EnableWindow(TRUE);
		break;
		
	case TransmissionState::STOPPING:
		m_btnSend.SetWindowText(_T("停止中..."));
		m_btnSend.EnableWindow(FALSE);
		break;
	}
}

void CPortMasterDlg::UpdateStatusDisplay()
{
	TransmissionState currentState = m_transmissionState.load();
	CString statusText;
	
	switch (currentState)
	{
	case TransmissionState::IDLE:
		statusText = _T("准备就绪");
		break;
		
	case TransmissionState::TRANSMITTING:
		statusText = _T("数据传输已开始");
		break;
		
	case TransmissionState::PAUSED:
		statusText = _T("传输已暂停");
		break;
		
	case TransmissionState::STOPPING:
		statusText = _T("正在停止传输...");
		break;
		
	case TransmissionState::STOPPED:
		statusText = _T("传输已终止");
		break;
	}
	
	m_staticPortStatus.SetWindowText(statusText);
}

bool CPortMasterDlg::ConfirmStopTransmission()
{
	TransmissionState currentState = m_transmissionState.load();
	
	if (currentState == TransmissionState::TRANSMITTING || currentState == TransmissionState::PAUSED)
	{
		int result = MessageBox(
			_T("确认要终止当前传输吗？\n传输中断后无法恢复。"),
			_T("确认终止传输"),
			MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
		);
		
		return (result == IDYES);
	}
	
	return true;
}

void CPortMasterDlg::PauseTransmission()
{
	m_pauseTransmission = true;
	UpdateTransmissionState(TransmissionState::PAUSED);
	WriteLog("传输已暂停");
}

void CPortMasterDlg::ResumeTransmission()
{
	m_pauseTransmission = false;
	UpdateTransmissionState(TransmissionState::TRANSMITTING);
	WriteLog("传输已恢复");
}

void CPortMasterDlg::StopTransmission()
{
	m_stopTransmission = true;
	m_pauseTransmission = false;
	UpdateTransmissionState(TransmissionState::STOPPING);
	WriteLog("正在停止传输");
	
	// 等待传输线程响应停止信号
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	
	// 强制停止状态
	UpdateTransmissionState(TransmissionState::STOPPED);
	
	// 重置进度条
	m_progress.SetPos(0);
}

bool CPortMasterDlg::IsTransmissionActive() const
{
	TransmissionState currentState = m_transmissionState.load();
	return (currentState == TransmissionState::TRANSMITTING || currentState == TransmissionState::PAUSED);
}

void CPortMasterDlg::PerformDataTransmission()
{
	try
	{
		// 直接使用缓存中的原始字节数据
		const std::vector<uint8_t> &data = m_sendDataCache;

		// 添加调试输出：记录发送数据的详细信息
		this->WriteLog("=== 开始数据传输 ===");
		this->WriteLog("传输数据大小: " + std::to_string(data.size()) + " 字节");

		// 传输逻辑
		TransportError error = TransportError::Success;
		bool transmissionCompleted = false;

		if (m_reliableChannel && m_reliableChannel->IsConnected())
		{
			this->WriteLog("使用可靠传输通道");

			// 分块发送以支持暂停/恢复
			const size_t chunkSize = 1024;
			size_t totalSent = 0;

			while (totalSent < data.size() && !m_stopTransmission && m_transmissionState.load() != TransmissionState::STOPPED)
			{
				// 检查暂停状态
				while (m_pauseTransmission && !m_stopTransmission)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				if (m_stopTransmission)
					break;

				size_t currentChunkSize = (std::min)(chunkSize, data.size() - totalSent);
				std::vector<uint8_t> chunk(data.begin() + totalSent, data.begin() + totalSent + currentChunkSize);

				bool success = m_reliableChannel->Send(chunk);
				if (!success)
				{
					this->WriteLog("传输失败于位置: " + std::to_string(totalSent));
					error = TransportError::WriteFailed;
					break;
				}

				totalSent += currentChunkSize;

				// 更新进度条
				int progress = (int)((totalSent * 100) / data.size());
				m_progress.SetPos(progress);

				// 更新统计
				m_bytesSent += currentChunkSize;
				CString sentText;
				sentText.Format(_T("%u"), m_bytesSent);
				SetDlgItemText(IDC_STATIC_SENT, sentText);
			}

			transmissionCompleted = (totalSent == data.size()) && (error == TransportError::Success);
		}
		else if (m_transport && m_transport->IsOpen())
		{
			this->WriteLog("使用原始传输");

			// 原始传输也支持分块和暂停
			const size_t chunkSize = 4096;
			size_t totalSent = 0;

			while (totalSent < data.size() && !m_stopTransmission && m_transmissionState.load() != TransmissionState::STOPPED)
			{
				// 检查暂停状态
				while (m_pauseTransmission && !m_stopTransmission)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				if (m_stopTransmission)
					break;

				size_t currentChunkSize = (std::min)(chunkSize, data.size() - totalSent);
				TransportError chunkError = m_transport->Write(data.data() + totalSent, currentChunkSize);
				
				if (chunkError != TransportError::Success)
				{
					this->WriteLog("传输失败于位置: " + std::to_string(totalSent));
					error = chunkError;
					break;
				}

				totalSent += currentChunkSize;

				// 更新进度条
				int progress = (int)((totalSent * 100) / data.size());
				m_progress.SetPos(progress);

				// 更新统计
				m_bytesSent += currentChunkSize;
				CString sentText;
				sentText.Format(_T("%u"), m_bytesSent);
				SetDlgItemText(IDC_STATIC_SENT, sentText);

				// 小延迟避免过快发送
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			transmissionCompleted = (totalSent == data.size()) && (error == TransportError::Success);
		}

		// 根据传输结果更新状态
		if (m_stopTransmission)
		{
			this->WriteLog("传输被用户终止");
			UpdateTransmissionState(TransmissionState::STOPPED);
			m_progress.SetPos(0);
		}
		else if (error != TransportError::Success)
		{
			this->WriteLog("传输发生错误: " + std::to_string(static_cast<int>(error)));
			UpdateTransmissionState(TransmissionState::STOPPED);
			
			CString errorMsg;
			errorMsg.Format(_T("传输失败: %d"), static_cast<int>(error));
			MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
			m_progress.SetPos(0);
		}
		else if (transmissionCompleted)
		{
			this->WriteLog("传输成功完成");
			UpdateTransmissionState(TransmissionState::IDLE);
			
			// 显示完成进度
			m_progress.SetPos(100);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			m_progress.SetPos(0);

			WriteLog("数据传输成功, 大小: " + std::to_string(data.size()) + " 字节");
		}

		this->WriteLog("=== 传输结束 ===");
	}
	catch (const std::exception &e)
	{
		CString errorMsg;
		errorMsg.Format(_T("传输过程发生异常: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
		UpdateTransmissionState(TransmissionState::STOPPED);
	}
}

// 自定义消息处理实现

LRESULT CPortMasterDlg::OnTransportDataReceivedMessage(WPARAM wParam, LPARAM lParam)
{
	std::vector<uint8_t> *data = reinterpret_cast<std::vector<uint8_t> *>(lParam);
	if (data)
	{
		try
		{
			// 添加调试输出：记录接收数据的详细信息
			this->WriteLog("=== 接收数据调试信息 ===");
			this->WriteLog("接收数据大小: " + std::to_string(data->size()) + " 字节");

			// 记录前32字节的十六进制内容用于调试
			std::string hexPreview;
			size_t previewSize = (std::min)(data->size(), (size_t)32);
			for (size_t i = 0; i < previewSize; i++)
			{
				char hexByte[4];
				sprintf_s(hexByte, "%02X ", (*data)[i]);
				hexPreview += hexByte;
			}
			if (data->size() > 32)
			{
				hexPreview += "...";
			}
			this->WriteLog("接收数据预览(十六进制): " + hexPreview);

			// 记录前32字节的ASCII内容用于调试
			std::string asciiPreview;
			for (size_t i = 0; i < previewSize; i++)
			{
				uint8_t byte = (*data)[i];
				if (byte >= 32 && byte <= 126)
				{
					asciiPreview += (char)byte;
				}
				else
				{
					asciiPreview += '.';
				}
			}
			if (data->size() > 32)
			{
				asciiPreview += "...";
			}
			this->WriteLog("接收数据预览(ASCII): " + asciiPreview);

			// 更新接收统计
			m_bytesReceived += data->size();
			CString receivedText;
			receivedText.Format(_T("%u"), m_bytesReceived);
			SetDlgItemText(IDC_STATIC_RECEIVED, receivedText);

			// 更新接收缓存 - 保存原始字节数据
			UpdateReceiveCache(*data);

			// 使用新的显示缓冲机制，实现实时数据块显示
			AddReceiveDataToDisplayBuffer(*data);
			
			// 更新显示（带行数限制和自动滚动）
			UpdateReceiveDisplayFromBuffer();

			this->WriteLog("=== 接收数据调试信息结束 ===");
		}
		catch (const std::exception &e)
		{
			CString errorMsg;
			errorMsg.Format(_T("处理接收数据失败: %s"), CString(e.what()));
			MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
		}

		delete data;
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

		// 设置可靠模式（串口配置不再支持，使用协议配置）
		if (config.protocol.windowSize > 1)
		{
			m_radioReliable.SetCheck(BST_CHECKED);
			m_radioDirect.SetCheck(BST_UNCHECKED);
		}
		else
		{
			m_radioReliable.SetCheck(BST_UNCHECKED);
			m_radioDirect.SetCheck(BST_CHECKED);
		}

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
	if (!m_tempCacheFile.is_open() || data.empty())
	{
		return false;
	}

	try
	{
		m_tempCacheFile.write(reinterpret_cast<const char *>(data.data()), data.size());
		if (m_tempCacheFile.fail())
		{
			WriteLog("写入临时缓存文件失败");
			return false;
		}

		m_tempCacheFile.flush(); // 确保数据立即写入磁盘
		m_totalReceivedBytes += data.size();
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
		// 确保写入文件被刷新，以便读取到最新数据
		if (m_tempCacheFile.is_open())
		{
			m_tempCacheFile.flush();
		}

		std::ifstream file(m_tempCacheFilePath, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			return result;
		}

		// 检查文件大小
		file.seekg(0, std::ios::end);
		std::streamsize fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		// 验证偏移量
		if (offset >= static_cast<uint64_t>(fileSize))
		{
			return result;
		}

		// 计算实际可读取的长度
		size_t availableLength = static_cast<size_t>(fileSize - offset);
		size_t readLength = (length == 0 || length > availableLength) ? availableLength : length;

		if (readLength > 0)
		{
			result.resize(readLength);
			file.seekg(offset);
			file.read(reinterpret_cast<char *>(result.data()), readLength);

			if (file.fail())
			{
				result.clear();
			}
		}

		file.close();
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