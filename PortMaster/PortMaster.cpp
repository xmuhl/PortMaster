#include "stdafx.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "Common/LogCenter.h"

#pragma execution_character_set("utf-8")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPortMasterApp

BEGIN_MESSAGE_MAP(CPortMasterApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, &CPortMasterApp::OnAppAbout)
END_MESSAGE_MAP()

// CPortMasterApp 构造

CPortMasterApp::CPortMasterApp() : m_hMutex(NULL)
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码,
	// 将所有重要的初始化放置在 InitInstance 中
}

// 唯一的 CPortMasterApp 对象
CPortMasterApp theApp;

// CPortMasterApp 初始化

BOOL CPortMasterApp::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式,
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活"Windows Native"视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 单实例检查
	if (!CheckSingleInstance()) {
		return FALSE;
	}

	try {
		// 初始化日志系统
		using namespace PortMaster;
		CString logDir = _T(".\\Logs");
		LogCenter::Instance().Initialize(logDir, LogLevel::LOG_INFO, true);

		PortMaster::LogCenter::Instance().Info(_T("App"), _T("PortMaster 应用程序启动"));

		// 显示启动画面
		ShowSplashScreen();

		// 创建主对话框
		CPortMasterDlg dlg;
		m_pMainWnd = &dlg;

		// 隐藏启动画面
		HideSplashScreen();

		// 显示主对话框
		INT_PTR nResponse = dlg.DoModal();
		if (nResponse == IDOK) {
			// TODO: 在此放置处理何时用
			//  "确定"来关闭对话框的代码
		}
		else if (nResponse == IDCANCEL) {
			// TODO: 在此放置处理何时用
			//  "取消"来关闭对话框的代码
		}
		else if (nResponse == -1) {
			TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
			TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
		}

		PortMaster::LogCenter::Instance().Info(_T("App"), _T("PortMaster 应用程序正常退出"));

	} catch (const std::exception& e) {
		// 异常处理
		CStringA errorMsg(e.what());
		CString wideError(errorMsg);
		PortMaster::LogCenter::Instance().Error(_T("App"), _T("应用程序异常: ") + wideError);

		AfxMessageBox(_T("应用程序发生严重错误，即将退出。"), MB_ICONERROR);
	} catch (...) {
		PortMaster::LogCenter::Instance().Error(_T("App"), _T("应用程序发生未知异常"));
		AfxMessageBox(_T("应用程序发生未知错误，即将退出。"), MB_ICONERROR);
	}

	// 删除上面创建的 shell 管理器。
	if (pShellManager != NULL) {
		delete pShellManager;
	}

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

int CPortMasterApp::ExitInstance()
{
	// 关闭日志系统
	PortMaster::LogCenter::Instance().Shutdown();

	// 释放单实例互斥量
	ReleaseSingleInstance();

	return CWinApp::ExitInstance();
}

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

// App 命令
void CPortMasterApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// 单实例检查实现
bool CPortMasterApp::CheckSingleInstance()
{
	// 创建命名互斥量
	m_hMutex = CreateMutex(NULL, TRUE, _T("PortMaster_SingleInstance_Mutex"));
	if (m_hMutex == NULL) {
		return false;
	}

	// 检查是否已经有实例在运行
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		ReleaseSingleInstance();

		// 尝试找到并激活已有实例的窗口
		HWND hExistingWnd = FindWindow(NULL, _T("PortMaster"));
		if (hExistingWnd) {
			// 恢复窗口并将其置于前台
			if (IsIconic(hExistingWnd)) {
				ShowWindow(hExistingWnd, SW_RESTORE);
			}
			SetForegroundWindow(hExistingWnd);
		}

		return false;
	}

	return true;
}

void CPortMasterApp::ReleaseSingleInstance()
{
	if (m_hMutex) {
		CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}
}

void CPortMasterApp::ShowSplashScreen()
{
	// TODO: 实现启动画面显示
	// 可以创建一个简单的对话框显示PortMaster logo和版本信息
}

void CPortMasterApp::HideSplashScreen()
{
	// TODO: 实现启动画面隐藏
}
