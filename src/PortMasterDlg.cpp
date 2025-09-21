// PortMasterDlg.cpp : 实现文件
//

#include "pch.h"
#include "framework.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "afxdialogex.h"

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
	m_comboPortType.AddString(_T("LPT"));
	m_comboPortType.AddString(_T("TCP客户端"));
	m_comboPortType.AddString(_T("TCP服务器"));
	m_comboPortType.AddString(_T("UDP"));
	m_comboPortType.SetCurSel(0);
	
	// 初始化端口列表
	m_comboPort.AddString(_T("COM1"));
	m_comboPort.AddString(_T("COM2"));
	m_comboPort.AddString(_T("COM3"));
	m_comboPort.AddString(_T("COM4"));
	m_comboPort.AddString(_T("COM5"));
	m_comboPort.AddString(_T("COM6"));
	m_comboPort.AddString(_T("COM7"));
	m_comboPort.AddString(_T("COM8"));
	m_comboPort.SetCurSel(0);
	
	// 初始化波特率列表
	m_comboBaudRate.AddString(_T("9600"));
	m_comboBaudRate.AddString(_T("19200"));
	m_comboBaudRate.AddString(_T("38400"));
	m_comboBaudRate.AddString(_T("57600"));
	m_comboBaudRate.AddString(_T("115200"));
	m_comboBaudRate.AddString(_T("230400"));
	m_comboBaudRate.AddString(_T("460800"));
	m_comboBaudRate.AddString(_T("921600"));
	m_comboBaudRate.SetCurSel(0);
	
	// 初始化数据位下拉框
	m_comboDataBits.AddString(_T("8"));
	m_comboDataBits.AddString(_T("7"));
	m_comboDataBits.AddString(_T("6"));
	m_comboDataBits.AddString(_T("5"));
	m_comboDataBits.SetCurSel(0);
	
	// 初始化校验位下拉框
	m_comboParity.AddString(_T("None"));
	m_comboParity.AddString(_T("Odd"));
	m_comboParity.AddString(_T("Even"));
	m_comboParity.AddString(_T("Mark"));
	m_comboParity.AddString(_T("Space"));
	m_comboParity.SetCurSel(0);
	
	// 初始化停止位下拉框
	m_comboStopBits.AddString(_T("1"));
	m_comboStopBits.AddString(_T("1.5"));
	m_comboStopBits.AddString(_T("2"));
	m_comboStopBits.SetCurSel(0);
	
	// 初始化流控下拉框
	m_comboFlowControl.AddString(_T("None"));
	m_comboFlowControl.AddString(_T("RTS/CTS"));
	m_comboFlowControl.AddString(_T("XON/XOFF"));
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

	// 初始化状态条信息
	m_staticPortStatus.SetWindowText(_T("未连接"));
	m_staticMode.SetWindowText(_T("可靠"));
	m_staticSpeed.SetWindowText(_T("0KB/s"));
	m_staticSendSource.SetWindowText(_T("来源: 手动输入"));

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
	MessageBox(_T("连接功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	
	// 更新状态条信息
	m_staticPortStatus.SetWindowText(_T("已连接"));
	m_staticSpeed.SetWindowText(_T("9600"));
}


void CPortMasterDlg::OnBnClickedButtonDisconnect()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("断开连接功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	
	// 更新状态条信息
	m_staticPortStatus.SetWindowText(_T("未连接"));
}


void CPortMasterDlg::OnBnClickedButtonSend()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("发送数据功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
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