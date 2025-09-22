// PortMasterDlg.cpp : 实现文件
//

#include "pch.h"
#include "framework.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "afxdialogex.h"
#include "../Transport/SerialTransport.h"
#include "../Common/CommonTypes.h"
#include <chrono>

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
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

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
	, m_isConnected(false)
	, m_isTransmitting(false)
	, m_bytesSent(0)
	, m_bytesReceived(0)
	, m_sendSpeed(0)
	, m_receiveSpeed(0)
	, m_transport(nullptr)
	, m_reliableChannel(nullptr)
	, m_configStore(ConfigStore::GetInstance())
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
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

	// TODO: 在此添加额外的初始化代码
	
	// 初始化端口类型下拉框
	m_comboPortType.AddString(_T("串口"));
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
	SetDlgItemText(IDC_STATIC_LOG, _T("程序启动成功"));
	SetDlgItemText(IDC_STATIC_PORT_STATUS, _T("未连接"));
	SetDlgItemText(IDC_STATIC_MODE, _T("可靠"));
	SetDlgItemText(IDC_STATIC_SPEED, _T("0KB/s"));
	SetDlgItemText(IDC_STATIC_SEND_SOURCE, _T("手动输入"));

	// 初始化传输配置
	InitializeTransportConfig();
	
	// 从配置存储加载配置
	LoadConfigurationFromStore();

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


void CPortMasterDlg::OnBnClickedButtonConnect()
{
	// TODO: 在此添加控件通知处理程序代码
	
	// 创建传输对象
	if (!CreateTransport())
	{
		return;
	}
	
	// 打开传输连接
	TransportError error = TransportError::Success;
	if (m_reliableChannel)
	{
		bool connected = m_reliableChannel->Connect();
		error = connected ? static_cast<TransportError>(TransportError::Success) : static_cast<TransportError>(TransportError::ConnectionClosed);
	}
	else if (m_transport)
	{
		// 传输对象已经在CreateTransport中打开，这里不需要重复打开
		if (!m_transport->IsOpen())
		{
			error = TransportError::NotOpen;
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
	// TODO: 在此添加控件通知处理程序代码
	
	// 检查是否已连接
	if (!m_isConnected)
	{
		MessageBox(_T("请先连接端口"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}
	
	// 获取发送数据
	CString sendData;
	m_editSendData.GetWindowText(sendData);
	
	if (sendData.IsEmpty())
	{
		MessageBox(_T("请输入要发送的数据"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}
	
	try
	{
		std::vector<uint8_t> data;
		
		// 根据显示模式转换数据
		if (m_checkHex.GetCheck() == BST_CHECKED)
		{
			// 十六进制模式
			CString hexData = sendData;
			hexData.Remove(' '); // 移除空格
			hexData.Remove('\r'); // 移除回车
			hexData.Remove('\n'); // 移除换行
			
			// 检查长度是否为偶数
			if (hexData.GetLength() % 2 != 0)
			{
				MessageBox(_T("十六进制数据长度必须为偶数"), _T("错误"), MB_OK | MB_ICONERROR);
				return;
			}
			
			// 转换为字节数组
			for (int i = 0; i < hexData.GetLength(); i += 2)
			{
				CString byteStr = hexData.Mid(i, 2);
				int byteValue;
				if (_stscanf_s(byteStr, _T("%02X"), &byteValue) == 1)
				{
					data.push_back(static_cast<uint8_t>(byteValue));
				}
				else
				{
					MessageBox(_T("十六进制数据格式错误"), _T("错误"), MB_OK | MB_ICONERROR);
					return;
				}
			}
		}
		else
		{
			// 文本模式
			CT2A textData(sendData);
			const char* text = textData;
			data.assign(text, text + strlen(text));
		}
		
		// 发送数据
		TransportError error = TransportError::Success;
		if (m_reliableChannel && m_reliableChannel->IsConnected())
		{
			// 使用可靠传输通道
			bool success = m_reliableChannel->Send(data);
			error = success ? TransportError::Success : TransportError::WriteFailed;
			m_isTransmitting = true;
		}
		else if (m_transport && m_transport->IsOpen())
		{
			// 使用原始传输
			error = m_transport->Write(data.data(), data.size());
		}
		
		if (error != TransportError::Success)
		{
			CString errorMsg;
			errorMsg.Format(_T("发送失败: %d"), static_cast<int>(error));
			MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
		}
		else
		{
			// 更新发送统计
			m_bytesSent += data.size();
			CString sentText;
			sentText.Format(_T("%u"), m_bytesSent);
			SetDlgItemText(IDC_STATIC_SENT, sentText);
			
			// 添加到发送历史
			CString history;
			GetDlgItemText(IDC_EDIT_SEND_HISTORY, history);
			if (!history.IsEmpty())
			{
				history += _T("\r\n");
			}
			history += sendData;
			SetDlgItemText(IDC_EDIT_SEND_HISTORY, history);
			
			// 清空发送编辑框
			SetDlgItemText(IDC_EDIT_SEND, _T(""));
		}
	}
	catch (const std::exception& e)
	{
		CString errorMsg;
		errorMsg.Format(_T("发送数据失败: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
	}
}




void CPortMasterDlg::OnBnClickedButtonStop()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("停止功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
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

void CPortMasterDlg::LoadDataFromFile(const CString& filePath)
{
	CStdioFile file;
	if (file.Open(filePath, CFile::modeRead | CFile::typeText))
	{
		CString fileContent;
		CString line;
		
		while (file.ReadString(line))
		{
			fileContent += line + _T("\r\n");
		}
		
		file.Close();
		
		// 设置到发送数据编辑框
		m_editSendData.SetWindowText(fileContent);
		
		// 如果当前是十六进制模式，转换显示
		if (m_checkHex.GetCheck())
		{
			CString hexContent = StringToHex(fileContent);
			m_editSendData.SetWindowText(hexContent);
		}
		
		// 更新数据源状态
		m_staticSendSource.SetWindowText(_T("来源: 文件"));
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
		m_comboPort.AddString(_T("COM1"));
		m_comboPort.AddString(_T("COM2"));
		m_comboPort.AddString(_T("COM3"));
		m_comboPort.AddString(_T("COM4"));
		m_comboPort.AddString(_T("COM5"));
		m_comboPort.AddString(_T("COM6"));
		m_comboPort.AddString(_T("COM7"));
		m_comboPort.AddString(_T("COM8"));
		// 显示串口相关参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_SHOW);
		break;
		
	case 1: // LPT
		m_comboPort.AddString(_T("LPT1"));
		m_comboPort.AddString(_T("LPT2"));
		m_comboPort.AddString(_T("LPT3"));
		// 隐藏串口参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_HIDE);
		break;
		
	case 2: // TCP客户端
	case 3: // TCP服务器
	case 4: // UDP
		m_comboPort.AddString(_T("127.0.0.1:8080"));
		m_comboPort.AddString(_T("192.168.1.100:8080"));
		m_comboPort.AddString(_T("localhost:9600"));
		// 隐藏串口参数
		GetDlgItem(IDC_COMBO_BAUD_RATE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_DATA_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_PARITY)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_STOP_BITS)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_COMBO_FLOW_CONTROL)->ShowWindow(SW_HIDE);
		break;
	}
	
	m_comboPort.SetCurSel(0);
}

void CPortMasterDlg::OnBnClickedCheckHex()
{
	// 十六进制显示模式切换
	BOOL bHex = m_checkHex.GetCheck();
	
	// 获取当前发送和接收数据
	CString strSendData, strReceiveData;
	m_editSendData.GetWindowText(strSendData);
	m_editReceiveData.GetWindowText(strReceiveData);
	
	if (bHex)
	{
		// 转换为十六进制显示
		if (!strSendData.IsEmpty())
		{
			CString hexSend = StringToHex(strSendData);
			m_editSendData.SetWindowText(hexSend);
		}
		if (!strReceiveData.IsEmpty())
		{
			CString hexReceive = StringToHex(strReceiveData);
			m_editReceiveData.SetWindowText(hexReceive);
		}
	}
	else
	{
		// 转换为文本显示
		if (!strSendData.IsEmpty())
		{
			CString textSend = HexToString(strSendData);
			m_editSendData.SetWindowText(textSend);
		}
		if (!strReceiveData.IsEmpty())
		{
			CString textReceive = HexToString(strReceiveData);
			m_editReceiveData.SetWindowText(textReceive);
		}
	}
}

CString CPortMasterDlg::StringToHex(const CString& str)
{
	CString hexStr;
	for (int i = 0; i < str.GetLength(); i++)
	{
		CString temp;
		temp.Format(_T("%02X "), (BYTE)str[i]);
		hexStr += temp;
	}
	hexStr.TrimRight();
	return hexStr;
}

CString CPortMasterDlg::HexToString(const CString& hex)
{
	CString result;
	CString hexData = hex;
	hexData.Replace(_T(" "), _T(""));
	
	for (int i = 0; i < hexData.GetLength(); i += 2)
	{
		if (i + 1 < hexData.GetLength())
		{
			CString hexByte = hexData.Mid(i, 2);
			BYTE byteValue = (BYTE)_tcstol(hexByte, NULL, 16);
			result += (TCHAR)byteValue;
		}
	}
	return result;
}

void CPortMasterDlg::OnBnClickedRadioReliable()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("可靠模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	
	// 更新状态条模式信息
	m_staticMode.SetWindowText(_T("可靠"));
}

void CPortMasterDlg::OnBnClickedButtonClearAll()
{
	// 清空发送和接收数据
	m_editSendData.SetWindowText(_T(""));
	m_editReceiveData.SetWindowText(_T(""));
	
	// 重置数据源显示
	SetDlgItemText(IDC_STATIC_SEND_SOURCE, _T("来源: 手动输入"));
	
	// 更新日志
	CString logText;
	GetDlgItemText(IDC_STATIC_LOG, logText);
	CTime time = CTime::GetCurrentTime();
	CString timeStr = time.Format(_T("[%H:%M:%S] "));
	logText = timeStr + _T("清空全部数据") + _T(" ") + logText;
	SetDlgItemText(IDC_STATIC_LOG, logText);
}

void CPortMasterDlg::OnBnClickedButtonClearReceive()
{
	// 只清空接收数据
	m_editReceiveData.SetWindowText(_T(""));
	
	// 更新日志
	CString logText;
	GetDlgItemText(IDC_STATIC_LOG, logText);
	CTime time = CTime::GetCurrentTime();
	CString timeStr = time.Format(_T("[%H:%M:%S] "));
	logText = timeStr + _T("清空接收数据") + _T(" ") + logText;
	SetDlgItemText(IDC_STATIC_LOG, logText);
}

void CPortMasterDlg::OnBnClickedButtonCopyAll()
{
	// 复制接收数据到剪贴板
	CString receiveData;
	m_editReceiveData.GetWindowText(receiveData);
	
	if (!receiveData.IsEmpty())
	{
		if (OpenClipboard())
		{
			EmptyClipboard();
			
			HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (receiveData.GetLength() + 1) * sizeof(TCHAR));
			if (hGlobal)
			{
				LPTSTR lpszData = (LPTSTR)GlobalLock(hGlobal);
				_tcscpy_s(lpszData, receiveData.GetLength() + 1, receiveData);
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
	CString receiveData;
	m_editReceiveData.GetWindowText(receiveData);
	
	if (!receiveData.IsEmpty())
	{
		CFileDialog dlg(FALSE, _T("txt"), _T("ReceiveData"), 
			OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
			_T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||"));
		
		if (dlg.DoModal() == IDOK)
		{
			SaveDataToFile(dlg.GetPathName(), receiveData);
		}
	}
	else
	{
		MessageBox(_T("没有数据可保存"), _T("提示"), MB_OK | MB_ICONWARNING);
	}
}

void CPortMasterDlg::SaveDataToFile(const CString& filePath, const CString& data)
{
	CStdioFile file;
	if (file.Open(filePath, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
	{
		file.WriteString(data);
		file.Close();
		
		MessageBox(_T("数据保存成功"), _T("提示"), MB_OK | MB_ICONINFORMATION);
		
		// 更新日志
		CString logText;
		GetDlgItemText(IDC_STATIC_LOG, logText);
		CTime time = CTime::GetCurrentTime();
		CString timeStr = time.Format(_T("[%H:%M:%S] "));
		CString fileName = filePath.Mid(filePath.ReverseFind('\\') + 1);
		logText = timeStr + _T("数据保存至: ") + fileName + _T(" ") + logText;
		SetDlgItemText(IDC_STATIC_LOG, logText);
	}
	else
	{
		MessageBox(_T("保存文件失败"), _T("错误"), MB_OK | MB_ICONERROR);
	}
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
	m_reliableConfig.timeoutBase = 5000;  // 基础超时时间
	m_reliableConfig.maxPayloadSize = 1024;  // 最大负载大小
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
						m_reliableChannel = std::make_unique<ReliableChannel>();
						m_reliableChannel->Initialize(m_transport, m_reliableConfig);
						
						// 设置回调函数
						m_reliableChannel->SetProgressCallback([this](int64_t current, int64_t total) {
							OnReliableProgress(static_cast<uint32_t>((current * 100) / total));
						});
						
						m_reliableChannel->SetStateChangedCallback([this](bool connected) {
							OnReliableComplete(connected);
						});
					}
					
					return true;
				}
			}
			break;
			
		case 1: // LPT
			MessageBox(_T("LPT端口暂不支持"), _T("提示"), MB_OK | MB_ICONINFORMATION);
			return false;
			
		case 2: // TCP客户端
		case 3: // TCP服务器
		case 4: // UDP
			MessageBox(_T("网络端口暂不支持"), _T("提示"), MB_OK | MB_ICONINFORMATION);
			return false;
			
		default:
			return false;
		}
	}
	catch (const std::exception& e)
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
	
	m_receiveThread = std::thread([this]() {
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
		}
	});
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

void CPortMasterDlg::OnTransportDataReceived(const std::vector<uint8_t>& data)
{
	// 在主线程中更新UI
	PostMessage(WM_USER + 1, 0, (LPARAM)new std::vector<uint8_t>(data));
}

void CPortMasterDlg::OnTransportError(const std::string& error)
{
	// 在主线程中显示错误
	CString errorMsg(error.c_str());
	PostMessage(WM_USER + 2, 0, (LPARAM)new CString(errorMsg));
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
		MessageBox(_T("传输完成"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		MessageBox(_T("传输失败"), _T("错误"), MB_OK | MB_ICONERROR);
	}
}

// 自定义消息处理实现

LRESULT CPortMasterDlg::OnTransportDataReceivedMessage(WPARAM wParam, LPARAM lParam)
{
	std::vector<uint8_t>* data = reinterpret_cast<std::vector<uint8_t>*>(lParam);
	if (data)
	{
		try
		{
			// 更新接收统计
			m_bytesReceived += data->size();
			CString receivedText;
			receivedText.Format(_T("%u"), m_bytesReceived);
			SetDlgItemText(IDC_STATIC_RECEIVED, receivedText);
			
			// 转换数据为字符串
			CString dataText;
			if (m_checkHex.GetCheck() == BST_CHECKED)
			{
				// 十六进制显示
				for (uint8_t byte : *data)
				{
					CString byteText;
					byteText.Format(_T("%02X "), byte);
					dataText += byteText;
				}
			}
			else
			{
				// 文本显示
				dataText = CString(reinterpret_cast<const char*>(data->data()), data->size());
			}
			
			// 添加到接收显示
			CString currentText;
			m_editReceiveData.GetWindowText(currentText);
			if (!currentText.IsEmpty())
			{
				currentText += _T("\r\n");
			}
			currentText += dataText;
			m_editReceiveData.SetWindowText(currentText);
			m_editReceiveData.LineScroll(m_editReceiveData.GetLineCount());
		}
		catch (const std::exception& e)
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
	CString* errorMsg = reinterpret_cast<CString*>(lParam);
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
		const PortMasterConfig& config = m_configStore.GetConfig();
		
		// 加载应用程序配置
		if (config.app.enableLogging)
		{
			// 设置日志级别等
		}
		
		// 加载串口配置
		const SerialPortConfig& serialConfig = config.serial;
		
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
		case NOPARITY:   parityText = _T("None"); break;
		case ODDPARITY:  parityText = _T("Odd"); break;
		case EVENPARITY: parityText = _T("Even"); break;
		default:         parityText = _T("None"); break;
		}
		SetDlgItemText(IDC_COMBO_PARITY, parityText);
		
		// 设置停止位
		CString stopBitsText;
		switch (serialConfig.stopBits)
		{
		case ONESTOPBIT:   stopBitsText = _T("1"); break;
		case ONE5STOPBITS: stopBitsText = _T("1.5"); break;
		case TWOSTOPBITS:  stopBitsText = _T("2"); break;
		default:           stopBitsText = _T("1"); break;
		}
		SetDlgItemText(IDC_COMBO_STOP_BITS, stopBitsText);
		
		// 设置流控制
		CString flowControlText = (serialConfig.flowControl == 0) ? _T("None") : _T("Hardware");
		SetDlgItemText(IDC_COMBO_FLOW_CONTROL, flowControlText);
		
		// 设置超时
		CString timeoutText;
		timeoutText.Format(_T("%d"), serialConfig.readTimeout);
		SetDlgItemText(IDC_EDIT_TIMEOUT, timeoutText);
		
		// 设置可靠模式
		if (serialConfig.reliableMode)
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
		const UIConfig& uiConfig = config.ui;
		
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
	catch (const std::exception& e)
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
		SerialPortConfig& serialConfig = config.serial;
		
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
		if (parityText == _T("None"))        serialConfig.parity = NOPARITY;
		else if (parityText == _T("Odd"))    serialConfig.parity = ODDPARITY;
		else if (parityText == _T("Even"))   serialConfig.parity = EVENPARITY;
		else                                 serialConfig.parity = NOPARITY;
		
		// 获取停止位
		CString stopBitsText;
		GetDlgItemText(IDC_COMBO_STOP_BITS, stopBitsText);
		if (stopBitsText == _T("1"))         serialConfig.stopBits = ONESTOPBIT;
		else if (stopBitsText == _T("1.5"))  serialConfig.stopBits = ONE5STOPBITS;
		else if (stopBitsText == _T("2"))    serialConfig.stopBits = TWOSTOPBITS;
		else                                 serialConfig.stopBits = ONESTOPBIT;
		
		// 获取流控制
		CString flowControlText;
		GetDlgItemText(IDC_COMBO_FLOW_CONTROL, flowControlText);
		serialConfig.flowControl = (flowControlText == _T("None")) ? 0 : 1;
		
		// 获取超时
		CString timeoutText;
		GetDlgItemText(IDC_EDIT_TIMEOUT, timeoutText);
		serialConfig.readTimeout = _ttoi(timeoutText);
		serialConfig.writeTimeout = serialConfig.readTimeout;
		
		// 获取可靠模式
		serialConfig.reliableMode = (m_radioReliable.GetCheck() == BST_CHECKED);
		
		// 保存UI配置
		UIConfig& uiConfig = config.ui;
		
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
	catch (const std::exception& e)
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
	catch (const std::exception& e)
	{
		CString errorMsg;
		errorMsg.Format(_T("应用配置变更失败: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("配置错误"), MB_OK | MB_ICONWARNING);
	}
}