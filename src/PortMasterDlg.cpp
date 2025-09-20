﻿// PortMasterDlg.cpp : 实现文件
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
	// 只绑定最基本的控件来测试
	DDX_Control(pDX, IDC_BUTTON_CONNECT, m_btnConnect);
	DDX_Control(pDX, IDC_BUTTON_DISCONNECT, m_btnDisconnect);
	DDX_Control(pDX, IDC_BUTTON_SEND, m_btnSend);
	DDX_Control(pDX, IDC_BUTTON_CLEAR, m_btnClear);
	DDX_Control(pDX, IDC_BUTTON_SAVE, m_btnSave);
	DDX_Control(pDX, IDC_BUTTON_SETTINGS, m_btnSettings);
	DDX_Control(pDX, IDC_BUTTON_ABOUT, m_btnAbout);
	DDX_Control(pDX, IDC_BUTTON_PAUSE, m_btnPause);
	DDX_Control(pDX, IDC_BUTTON_CONTINUE, m_btnContinue);
	DDX_Control(pDX, IDC_BUTTON_STOP, m_btnStop);
	DDX_Control(pDX, IDC_BUTTON_COPY, m_btnCopy);
	DDX_Control(pDX, IDC_BUTTON_FILE, m_btnFile);
	DDX_Control(pDX, IDC_BUTTON_EXPORT_LOG, m_btnExportLog);
	DDX_Control(pDX, IDC_EDIT_SEND_DATA, m_editSendData);
	DDX_Control(pDX, IDC_EDIT_RECEIVE_DATA, m_editReceiveData);
	DDX_Control(pDX, IDC_COMBO_PORT_TYPE, m_comboPortType);
	DDX_Control(pDX, IDC_COMBO_PORT, m_comboPort);
	DDX_Control(pDX, IDC_COMBO_BAUD_RATE, m_comboBaudRate);
	DDX_Control(pDX, IDC_RADIO_RELIABLE, m_radioReliable);
	DDX_Control(pDX, IDC_RADIO_DIRECT, m_radioDirect);
	DDX_Control(pDX, IDC_CHECK_HEX, m_checkHex);
	DDX_Control(pDX, IDC_PROGRESS, m_progress);
}

BEGIN_MESSAGE_MAP(CPortMasterDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CPortMasterDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CPortMasterDlg::OnBnClickedButtonDisconnect)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CPortMasterDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_BUTTON_CLEAR, &CPortMasterDlg::OnBnClickedButtonClear)
	ON_BN_CLICKED(IDC_BUTTON_SAVE, &CPortMasterDlg::OnBnClickedButtonSave)
	ON_BN_CLICKED(IDC_BUTTON_SETTINGS, &CPortMasterDlg::OnBnClickedButtonSettings)
	ON_BN_CLICKED(IDC_BUTTON_ABOUT, &CPortMasterDlg::OnBnClickedButtonAbout)
	ON_BN_CLICKED(IDC_BUTTON_PAUSE, &CPortMasterDlg::OnBnClickedButtonPause)
	ON_BN_CLICKED(IDC_BUTTON_CONTINUE, &CPortMasterDlg::OnBnClickedButtonContinue)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CPortMasterDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_COPY, &CPortMasterDlg::OnBnClickedButtonCopy)
	ON_BN_CLICKED(IDC_BUTTON_FILE, &CPortMasterDlg::OnBnClickedButtonFile)
	ON_BN_CLICKED(IDC_BUTTON_EXPORT_LOG, &CPortMasterDlg::OnBnClickedButtonExportLog)
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
	
	// 基本初始化
	m_comboPortType.AddString(_T("串口"));
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
	
	// 设置默认模式
	m_radioReliable.SetCheck(BST_CHECKED);
	
	// 初始化进度条
	m_progress.SetRange(0, 100);
	m_progress.SetPos(0);
	
	// 显示初始状态
	SetDlgItemText(IDC_STATIC_PORT_STATUS, _T("端口: 就绪"));
	SetDlgItemText(IDC_STATIC_MODE, _T("模式: 可靠"));
	SetDlgItemText(IDC_STATIC_SPEED, _T("速率: 0KB/s"));
	SetDlgItemText(IDC_STATIC_LOOP_COUNT, _T("回路测试轮次: 0"));
	SetDlgItemText(IDC_STATIC_CHECK, _T("校验: 通过"));

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
}


void CPortMasterDlg::OnBnClickedButtonDisconnect()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("断开连接功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}


void CPortMasterDlg::OnBnClickedButtonSend()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("发送数据功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}


void CPortMasterDlg::OnBnClickedButtonClear()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("清除数据功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}


void CPortMasterDlg::OnBnClickedButtonSave()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("保存数据功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}


void CPortMasterDlg::OnBnClickedButtonSettings()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("设置功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}


void CPortMasterDlg::OnBnClickedButtonAbout()
{
	// TODO: 在此添加控件通知处理程序代码
	CAboutDlg dlgAbout;
	dlgAbout.DoModal();
}

void CPortMasterDlg::OnBnClickedButtonPause()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("暂停功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedButtonContinue()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("继续功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedButtonStop()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("停止功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedButtonCopy()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("复制功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedButtonFile()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("文件功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedButtonExportLog()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("导出日志功能"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnCbnSelchangeComboPortType()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("端口类型改变"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedCheckHex()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("十六进制选项改变"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedRadioReliable()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("可靠模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void CPortMasterDlg::OnBnClickedRadioDirect()
{
	// TODO: 在此添加控件通知处理程序代码
	MessageBox(_T("直通模式选择"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}