#pragma execution_character_set("utf-8")
#include "pch.h"
#include "framework.h"
#include "PortMaster.h"
#include "PortMasterDlg.h"
#include "SplashDialog.h"
#include "Common/ConfigManager.h"
#include <fstream>
#include <shlobj.h>
#include <memory>

std::string GetLogFilePath() {
    static std::string cachedPath;
    if (!cachedPath.empty()) {
        return cachedPath;
    }
    
    // 首先尝试从ConfigManager获取日志目录
    static std::unique_ptr<ConfigManager> configManager;
    if (!configManager) {
        configManager = std::make_unique<ConfigManager>();
    }
    
    std::string logDir = configManager->GetString("Log", "logDirectory", "");
    
    if (logDir.empty()) {
        // 如果配置中没有指定，使用程序目录
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string path = exePath;
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            logDir = path.substr(0, pos);
        } else {
            logDir = ".";
        }
        
        // 如果程序目录不可写，使用用户LOCALAPPDATA
        std::string testFile = logDir + "\\test_write.tmp";
        std::ofstream test(testFile);
        if (!test.is_open()) {
            char appDataPath[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
                logDir = std::string(appDataPath) + "\\PortMaster\\Logs";
                CreateDirectoryA((std::string(appDataPath) + "\\PortMaster").c_str(), NULL);
                CreateDirectoryA(logDir.c_str(), NULL);
            }
        } else {
            test.close();
            DeleteFileA(testFile.c_str());
        }
        
        // 更新配置
        configManager->SetValue("Log", "logDirectory", logDir);
    }
    
    // 确保目录存在
    CreateDirectoryA(logDir.c_str(), NULL);
    
    // 获取当前日期作为日志文件名
    SYSTEMTIME st;
    GetLocalTime(&st);
    char dateStr[32];
    sprintf_s(dateStr, sizeof(dateStr), "%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
    
    cachedPath = logDir + "\\PortMaster_" + dateStr + ".log";
    return cachedPath;
}

void WriteDebugLog(const char* message) {
    std::string logPath = GetLogFilePath();
    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeStr[64];
        sprintf_s(timeStr, sizeof(timeStr), "[%04d-%02d-%02d %02d:%02d:%02d.%03d]", 
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        logFile << timeStr << " " << message << std::endl;
        logFile.close();
    }
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPortMasterApp

BEGIN_MESSAGE_MAP(CPortMasterApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

// CPortMasterApp 构造
CPortMasterApp::CPortMasterApp()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}

// 唯一的 CPortMasterApp 对象

CPortMasterApp theApp;

// CPortMasterApp 初始化
BOOL CPortMasterApp::InitInstance()
{
	WriteDebugLog("[DEBUG] InitInstance: 开始应用程序初始化");
	
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);
	WriteDebugLog("[DEBUG] InitInstance: 公共控件初始化完成");

	CWinApp::InitInstance();
	WriteDebugLog("[DEBUG] InitInstance: CWinApp::InitInstance 完成");

	// 初始化 RichEdit 控件
	if (!AfxInitRichEdit2())
	{
		WriteDebugLog("[ERROR] InitInstance: RichEdit 控件初始化失败");
		AfxMessageBox(L"无法初始化 RichEdit 控件！");
		return FALSE;
	}
	WriteDebugLog("[DEBUG] InitInstance: RichEdit 控件初始化成功");

	// 初始化 OLE 库
	WriteDebugLog("[DEBUG] InitInstance: 开始初始化 OLE 库");
	if (!AfxOleInit())
	{
		WriteDebugLog("[ERROR] InitInstance: OLE 库初始化失败");
		AfxMessageBox(L"初始化 OLE 库失败！");
		return FALSE;
	}
	WriteDebugLog("[DEBUG] InitInstance: OLE 库初始化成功");

	AfxEnableControlContainer();
	WriteDebugLog("[DEBUG] InitInstance: 控件容器启用完成");

	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	WriteDebugLog("[DEBUG] InitInstance: 开始创建 Shell 管理器");
	CShellManager *pShellManager = new CShellManager;
	WriteDebugLog("[DEBUG] InitInstance: Shell 管理器创建完成");

	// 激活"Windows Native"视觉管理器，以便在 MFC 控件中启用主题
	WriteDebugLog("[DEBUG] InitInstance: 开始设置视觉管理器");
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
	WriteDebugLog("[DEBUG] InitInstance: 视觉管理器设置完成");

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	WriteDebugLog("[DEBUG] InitInstance: 开始设置注册表项");
	SetRegistryKey(L"端口大师");
	WriteDebugLog("[DEBUG] InitInstance: 注册表项设置完成");

	// ⭐ 修改：创建启动画面并在后台初始化应用程序（添加异常保护）
	WriteDebugLog("[DEBUG] InitInstance: 开始创建启动画面");
	CSplashDialog* pSplash = new CSplashDialog();
	INT_PTR nResponse = IDCANCEL; // ⭐ 声明在更高的作用域
	bool bSplashSuccess = false;
	
	// ⭐ 新增：异常保护机制
	try {
		// 创建启动画面但不阻塞
		if (pSplash->Create(IDD_SPLASH_DIALOG)) {
		WriteDebugLog("[DEBUG] InitInstance: 启动画面窗口创建成功");
		pSplash->ShowWindow(SW_SHOW);
		pSplash->UpdateWindow();
		
		// ⭐ 新增：在后台进行应用程序初始化
		WriteDebugLog("[DEBUG] InitInstance: 开始后台初始化");
		pSplash->SetInitializationProgress(L"正在加载应用程序组件...");
		
		// 让启动画面有机会显示
		MSG msg;
		for (int i = 0; i < 5; ++i) {
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			Sleep(50); // 短暂延迟
		}
		
		WriteDebugLog("[DEBUG] InitInstance: 开始创建主对话框");
		pSplash->SetInitializationProgress(L"正在初始化主界面...");
		
		// 主对话框初始化
		CPortMasterDlg dlg;
		WriteDebugLog("[DEBUG] InitInstance: 主对话框创建完成");
		
		// 通知启动画面初始化完成
		pSplash->SetInitializationProgress(L"加载完成，正在启动...");
		pSplash->NotifyInitializationComplete();
		
		// ⭐ 新增：立即检查启动画面是否响应
		Sleep(500); // 给启动画面500ms时间响应
		if (pSplash && pSplash->GetSafeHwnd() && IsWindow(pSplash->GetSafeHwnd())) {
			// 发送测试消息检查响应性
			DWORD_PTR dwResult = 0;
			LRESULT result = ::SendMessageTimeoutW(pSplash->GetSafeHwnd(), WM_NULL, 0, 0, SMTO_NORMAL, 1000, &dwResult);
			if (result == 0) {
				WriteDebugLog("[WARNING] InitInstance: 启动画面无响应，直接关闭");
				try {
					pSplash->DestroyWindow();
				} catch (...) {
					WriteDebugLog("[ERROR] InitInstance: 销毁无响应启动画面时异常");
				}
			}
		}
		
		// ⭐ 修复：等待启动画面自动关闭 - 增强超时机制防止死锁
		WriteDebugLog("[DEBUG] InitInstance: 等待启动画面关闭");
		DWORD dwWaitStart = GetTickCount();
		const DWORD MAX_WAIT_TIME = 2500; // 🔴 优化：从3000ms缩短到2500ms，配合启动画面2秒定时器
		int waitCount = 0;
		
		while (pSplash && pSplash->GetSafeHwnd() && IsWindow(pSplash->GetSafeHwnd())) {
			// 处理消息队列
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			
			// 检查超时
			DWORD dwElapsed = GetTickCount() - dwWaitStart;
			if (dwElapsed > MAX_WAIT_TIME) {
				WriteDebugLog("[WARNING] InitInstance: 启动画面等待超时，强制关闭");
				try {
					if (pSplash && pSplash->GetSafeHwnd()) {
						pSplash->SendMessage(WM_CLOSE); // 先尝试正常关闭
						Sleep(100);
						if (IsWindow(pSplash->GetSafeHwnd())) {
							pSplash->DestroyWindow(); // 强制关闭
						}
					}
				} catch (...) {
					WriteDebugLog("[ERROR] InitInstance: 强制关闭启动画面时发生异常");
				}
				break;
			}
			
			// 每100次循环输出一次调试信息
			if (++waitCount % 100 == 0) {
				CString debugMsg;
				debugMsg.Format(L"[DEBUG] InitInstance: 等待启动画面关闭中... (%d ms)", dwElapsed);
				WriteDebugLog(CW2A(debugMsg));
			}
			
			Sleep(10);
		}
		
		// 清理启动画面
		if (pSplash) {
			delete pSplash;
			pSplash = nullptr;
		}
		
		WriteDebugLog("[DEBUG] InitInstance: 启动画面已关闭，开始显示主对话框");
		WriteDebugLog("[DEBUG] InitInstance: 调用DoModal前");
		
		// 🔴 主窗口显示优化：确保窗口正确显示和获得焦点
		// 注意：对于模态对话框，不应在DoModal前设置m_pMainWnd
		// m_pMainWnd会在DoModal内部自动设置
		
		// 确保主窗口能够正确显示
		::SetForegroundWindow(::GetDesktopWindow()); // 重置前台窗口
		
		nResponse = dlg.DoModal(); // ⭐ 修改：使用赋值而不是重新声明
		
		// 🔴 新增：主窗口显示后的状态验证
		if (nResponse == IDOK || nResponse == IDCANCEL) {
			WriteDebugLog("[DEBUG] InitInstance: 主对话框正常关闭");
		} else {
			CString responseMsg;
			responseMsg.Format(L"[WARNING] InitInstance: 主对话框异常关闭，返回值: %d", (int)nResponse);
			WriteDebugLog(CW2A(responseMsg));
		}
		
		WriteDebugLog("[DEBUG] InitInstance: 主对话框显示完成");
		bSplashSuccess = true; // ⭐ 标记启动画面流程成功
	} else {
		// ⭐ 新增：启动画面创建失败的处理
		WriteDebugLog("[ERROR] InitInstance: 启动画面创建失败，直接显示主对话框");
		if (pSplash) {
			delete pSplash;
			pSplash = nullptr;
		}
		
		// 直接创建并显示主对话框
		WriteDebugLog("[DEBUG] InitInstance: 开始创建主对话框（备用模式）");
		CPortMasterDlg dlg;
		WriteDebugLog("[DEBUG] InitInstance: 主对话框创建完成，开始显示（备用模式）");
		nResponse = dlg.DoModal(); // ⭐ 修改：使用赋值而不是重新声明
		CString responseMsg;
		responseMsg.Format(L"[DEBUG] InitInstance: DoModal返回值（备用模式）: %d", (int)nResponse);
		WriteDebugLog(CW2A(responseMsg));
		WriteDebugLog("[DEBUG] InitInstance: 主对话框显示完成（备用模式）");
	}
	} catch (const std::exception& e) {
		// ⭐ 新增：异常处理 - 确保程序不会崩溃
		WriteDebugLog(("[EXCEPTION] InitInstance: 启动画面异常: " + std::string(e.what())).c_str());
		
		// 清理启动画面资源
		if (pSplash) {
			try {
				if (pSplash->GetSafeHwnd()) {
					pSplash->DestroyWindow();
				}
				delete pSplash;
				pSplash = nullptr;
			} catch (...) {
				// 忽略清理过程中的异常
			}
		}
		
		// 直接显示主对话框
		WriteDebugLog("[DEBUG] InitInstance: 异常恢复 - 直接显示主对话框");
		CPortMasterDlg dlg;
		nResponse = dlg.DoModal();
		CString responseMsg;
		responseMsg.Format(L"[DEBUG] InitInstance: DoModal返回值（异常恢复）: %d", (int)nResponse);
		WriteDebugLog(CW2A(responseMsg));
	} catch (...) {
		// ⭐ 新增：捕获所有其他异常
		WriteDebugLog("[EXCEPTION] InitInstance: 启动画面未知异常");
		
		// 清理启动画面资源
		if (pSplash) {
			try {
				delete pSplash;
				pSplash = nullptr;
			} catch (...) {
				// 忽略清理过程中的异常
			}
		}
		
		// 直接显示主对话框
		WriteDebugLog("[DEBUG] InitInstance: 异常恢复 - 直接显示主对话框");
		CPortMasterDlg dlg;
		nResponse = dlg.DoModal();
	}

	// ⭐ 统一的主对话框返回值处理（无论启动画面成功与否）
	
	// ⭐ 修正：最终保险机制 - 仅在对话框创建失败时触发
	if (nResponse == -1) {
		// 仅在DoModal返回-1（对话框创建失败）时启动保险机制
		WriteDebugLog("[WARNING] InitInstance: 主对话框创建失败，启动保险机制");
		try {
			CPortMasterDlg fallbackDlg;
			nResponse = fallbackDlg.DoModal();
			WriteDebugLog("[DEBUG] InitInstance: 保险机制主对话框显示完成");
		} catch (...) {
			WriteDebugLog("[ERROR] InitInstance: 保险机制也失败，程序即将退出");
		}
	}

	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  "确定"来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  "取消"来关闭对话框的代码
	}
	else if (nResponse == -1)
	{
		
		
	}

	// 删除上面创建的 shell 管理器。
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}










