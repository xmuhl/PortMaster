#pragma execution_character_set("utf-8")
#include "pch.h"
#include "PortMaster.h"
#include "SplashDialog.h"
#include "afxdialogex.h"
#include <gdiplus.h>
#include <fstream>
#pragma comment(lib, "gdiplus.lib")

extern void WriteDebugLog(const char* message);
using namespace Gdiplus;

// CSplashDialog 对话框
IMPLEMENT_DYNAMIC(CSplashDialog, CDialogEx)

CSplashDialog::CSplashDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SPLASH_DIALOG, pParent)
	, m_nProgressTimer(0) // ★ 新增：初始化进度定时器
	, m_bInitializationComplete(false) // ★ 新增：初始化状态
	, m_dwStartTime(0) // ★ 新增：启动时间
	, m_strProgressMessage(L"正在初始化...") // ★ 新增：默认进度消息
	, m_bTextModeEnabled(false) // ⭐ 新增：文本模式标志
{
}

CSplashDialog::~CSplashDialog()
{
	// 确保定时器在窗口销毁前被安全清理
	if (::IsWindow(GetSafeHwnd()))
	{
		if (m_nTimer != 0) {
			KillTimer(m_nTimer);
			m_nTimer = 0;
		}
		// ★ 新增：清理进度定时器
		if (m_nProgressTimer != 0) {
			KillTimer(m_nProgressTimer);
			m_nProgressTimer = 0;
		}
	}
}

void CSplashDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSplashDialog, CDialogEx)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND() // ⭐ 新增：文本模式背景绘制
	ON_MESSAGE(WM_SPLASH_INIT_COMPLETE, &CSplashDialog::OnInitializationComplete) // ★ 新增：初始化完成消息处理
END_MESSAGE_MAP()

// CSplashDialog 消息处理程序

BOOL CSplashDialog::OnInitDialog()
{
	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 开始初始化启动画面对话框");
	CDialogEx::OnInitDialog();
	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: CDialogEx::OnInitDialog 完成");

	// ⭐ 增强的PNG资源加载 - 添加完整性验证和降级处理
	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 开始加载PNG资源");
	m_bTextModeEnabled = false; // 默认尝试PNG模式
	
	// 首先验证PNG资源完整性
	if (!ValidatePNGResource(IDB_SPLASH))
	{
		WriteDebugLog("[WARNING] SplashDialog::OnInitDialog: PNG资源验证失败，启用文本模式");
		EnableTextMode();
	}
	else if (!LoadPNGResource())
	{
		WriteDebugLog("[WARNING] SplashDialog::OnInitDialog: PNG加载失败，启用文本模式");
		EnableTextMode();
	}
	else
	{
		WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: PNG资源加载成功");
	}
	// ⭐ 旧代码已移除，使用新的分层加载机制

	// 居中显示
	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 开始居中显示");
	CenterWindow();
	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 居中显示完成");

	// ⭐ 修改：设置消息驱动的关闭逻辑
	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 开始设置定时器");
	m_dwStartTime = GetTickCount(); // 记录启动时间
	
	// 设置最大显示时间定时器（防止死锁）
	m_nTimer = SetTimer(SPLASH_TIMER_ID, MAX_DISPLAY_TIME, NULL);
	if (m_nTimer != 0)
	{
		WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 最大显示时间定时器设置成功");
	}
	else
	{
		WriteDebugLog("[ERROR] SplashDialog::OnInitDialog: 最大显示时间定时器设置失败");
	}
	
	// ⭐ 新增：设置进度更新定时器（用于重绘进度信息）
	m_nProgressTimer = SetTimer(PROGRESS_TIMER_ID, 200, NULL); // 每200ms更新一次
	if (m_nProgressTimer != 0)
	{
		WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 进度更新定时器设置成功");
	}
	else
	{
		WriteDebugLog("[ERROR] SplashDialog::OnInitDialog: 进度更新定时器设置失败");
	}

	WriteDebugLog("[DEBUG] SplashDialog::OnInitDialog: 启动画面对话框初始化完成");
	return TRUE;
}
void CSplashDialog::OnPaint()
{
	WriteDebugLog("[DEBUG] SplashDialog::OnPaint: 开始绘制启动画面");
	CPaintDC dc(this);

	if (m_splashBitmap.GetSafeHandle() != NULL)
	{
		WriteDebugLog("[DEBUG] SplashDialog::OnPaint: 使用位图绘制");
		// 创建兼容DC
		CDC memDC;
		memDC.CreateCompatibleDC(&dc);
		CBitmap* pOldBitmap = memDC.SelectObject(&m_splashBitmap);

		// 获取位图尺寸
		BITMAP bmpInfo;
		m_splashBitmap.GetBitmap(&bmpInfo);

		// 获取客户区尺寸
		CRect clientRect;
		GetClientRect(&clientRect);

		// 绘制位图
		dc.BitBlt(0, 0, clientRect.Width(), clientRect.Height(), 
			&memDC, 0, 0, SRCCOPY);

		// 恢复原有位图
		memDC.SelectObject(pOldBitmap);
		
		// ⭐ 新增：在位图上绘制进度信息
		DrawProgressInfo(dc, clientRect);
		WriteDebugLog("[DEBUG] SplashDialog::OnPaint: 位图绘制完成");
	}
	else
	{
		WriteDebugLog("[DEBUG] SplashDialog::OnPaint: 位图不存在，使用文本绘制");
		// 如果没有位图，显示文本
		CRect clientRect;
		GetClientRect(&clientRect);
		
		// 设置背景色
		dc.FillSolidRect(&clientRect, RGB(70, 130, 180)); // 钢蓝色
		
		// 绘制文本
		dc.SetTextColor(RGB(255, 255, 255));
		dc.SetBkMode(TRANSPARENT);
		
		CFont font;
		font.CreatePointFont(240, L"微软雅黑");
		CFont* pOldFont = dc.SelectObject(&font);
		
		dc.DrawText(L"PortMaster\n端口大师", &clientRect, 
			DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		
		dc.SelectObject(pOldFont);
		
		// ⭐ 新增：在文本模式下也绘制进度信息
		DrawProgressInfo(dc, clientRect);
		WriteDebugLog("[DEBUG] SplashDialog::OnPaint: 文本绘制完成");
	}
	WriteDebugLog("[DEBUG] SplashDialog::OnPaint: 绘制完成");
}

// 新增：外部控制的关闭方法
void CSplashDialog::CloseSplash()
{
	if (::IsWindow(GetSafeHwnd()))
	{
		WriteDebugLog("[DEBUG] SplashDialog::CloseSplash: 外部请求关闭启动画面");
		
		// 清理所有定时器
		if (m_nTimer != 0) {
			WriteDebugLog("[DEBUG] SplashDialog::CloseSplash: 清理主定时器");
			KillTimer(m_nTimer);
			m_nTimer = 0;
		}
		if (m_nProgressTimer != 0) {
			WriteDebugLog("[DEBUG] SplashDialog::CloseSplash: 清理进度定时器");
			KillTimer(m_nProgressTimer);
			m_nProgressTimer = 0;
		}
		
		// 关闭对话框
		WriteDebugLog("[DEBUG] SplashDialog::CloseSplash: 销毁对话框窗口");
		DestroyWindow();
	}
}

// ⭐ 新增：初始化完成消息处理器 - 线程安全版本
LRESULT CSplashDialog::OnInitializationComplete(WPARAM wParam, LPARAM lParam)
{
	WriteDebugLog("[DEBUG] SplashDialog::OnInitializationComplete: 收到初始化完成通知");
	SetInitializationCompleteState(true); // ⭐ 使用线程安全的设置方法
	SetProgressMessage(L"初始化完成"); // ⭐ 使用线程安全的设置方法
	
	// KISS原则：初始化完成后立即关闭，提供响应式用户体验
	WriteDebugLog("[DEBUG] SplashDialog::OnInitializationComplete: 初始化完成，立即关闭Splash");
	DestroyWindow();
	
	return 0;
}

// ⭐ 新增：外部通知初始化完成
void CSplashDialog::NotifyInitializationComplete()
{
	if (::IsWindow(GetSafeHwnd())) {
		PostMessage(WM_SPLASH_INIT_COMPLETE);
	}
}

// ⭐ 新增：设置初始化进度信息 - 线程安全版本
void CSplashDialog::SetInitializationProgress(const CString& message)
{
	if (::IsWindow(GetSafeHwnd())) {
		SetProgressMessage(message); // ⭐ 使用线程安全的设置方法
		Invalidate(FALSE); // 触发重绘
	}
}

// ⭐ 新增：绘制进度信息的辅助方法
void CSplashDialog::DrawProgressInfo(CDC& dc, const CRect& clientRect)
{
	// 计算经过的时间
	DWORD dwElapsed = GetTickCount() - m_dwStartTime;
	
	// 创建半透明背景
	CRect progressRect = clientRect;
	progressRect.top = progressRect.bottom - 60; // 底部60像素
	
	CBrush bgBrush(RGB(0, 0, 0));
	CBrush* pOldBrush = dc.SelectObject(&bgBrush);
	CPen transparentPen(PS_NULL, 0, RGB(0, 0, 0));
	CPen* pOldPen = dc.SelectObject(&transparentPen);
	
	// 绘制半透明背景矩形
	dc.SetBkMode(TRANSPARENT);
	dc.Rectangle(&progressRect);
	
	// 设置文字样式
	dc.SetTextColor(RGB(255, 255, 255)); // 白色文字
	CFont progressFont;
	progressFont.CreatePointFont(90, L"微软雅黑");
	CFont* pOldFont = dc.SelectObject(&progressFont);
	
	// 绘制状态信息（线程安全版本）
	CString statusText;
	bool bComplete = GetInitializationCompleteState(); // ⭐ 使用线程安全的获取方法
	CString progressMsg = GetProgressMessage(); // ⭐ 使用线程安全的获取方法
	
	if (bComplete) {
		statusText = L"✓ " + progressMsg;
	} else {
		statusText = L"⚡ " + progressMsg;
	}
	
	CRect textRect = progressRect;
	textRect.DeflateRect(10, 5);
	dc.DrawText(statusText, &textRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
	
	// 绘制时间信息
	CString timeText;
	timeText.Format(L"已用时: %.1f秒", dwElapsed / 1000.0);
	textRect.OffsetRect(0, 20);
	dc.DrawText(timeText, &textRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
	
	// 恢复原有对象
	dc.SelectObject(pOldFont);
	dc.SelectObject(pOldPen);
	dc.SelectObject(pOldBrush);
}

// ⭐ 新增：PNG资源完整性验证
bool CSplashDialog::ValidatePNGResource(UINT resourceId)
{
	WriteDebugLog("[DEBUG] SplashDialog::ValidatePNGResource: 开始验证PNG资源完整性");
	
	HRSRC hResource = FindResource(AfxGetInstanceHandle(), MAKEINTRESOURCE(resourceId), L"RCDATA");
	if (hResource == NULL)
	{
		WriteDebugLog("[ERROR] SplashDialog::ValidatePNGResource: 找不到PNG资源");
		return false;
	}
	
	DWORD dwSize = SizeofResource(AfxGetInstanceHandle(), hResource);
	if (dwSize < 8) // PNG文件最小8字节头部
	{
		WriteDebugLog("[ERROR] SplashDialog::ValidatePNGResource: PNG资源大小异常");
		return false;
	}
	
	HGLOBAL hGlobal = LoadResource(AfxGetInstanceHandle(), hResource);
	if (hGlobal == NULL)
	{
		WriteDebugLog("[ERROR] SplashDialog::ValidatePNGResource: 加载PNG资源失败");
		return false;
	}
	
	LPVOID pData = LockResource(hGlobal);
	if (pData == NULL)
	{
		WriteDebugLog("[ERROR] SplashDialog::ValidatePNGResource: 锁定PNG资源失败");
		return false;
	}
	
	// 验证PNG文件头部签名 (0x89504E470D0A1A0A)
	BYTE* pBytes = (BYTE*)pData;
	if (pBytes[0] != 0x89 || pBytes[1] != 0x50 || pBytes[2] != 0x4E || pBytes[3] != 0x47 ||
		pBytes[4] != 0x0D || pBytes[5] != 0x0A || pBytes[6] != 0x1A || pBytes[7] != 0x0A)
	{
		WriteDebugLog("[ERROR] SplashDialog::ValidatePNGResource: PNG文件头部签名验证失败");
		return false;
	}
	
	WriteDebugLog("[DEBUG] SplashDialog::ValidatePNGResource: PNG资源完整性验证成功");
	return true;
}

// ⭐ 新增：加载PNG资源
bool CSplashDialog::LoadPNGResource()
{
	WriteDebugLog("[DEBUG] SplashDialog::LoadPNGResource: 开始加载PNG资源");
	
	// ⭐ RAII资源管理类
	class GdiplusRAII {
	private:
		ULONG_PTR m_token;
		bool m_initialized;
	public:
		GdiplusRAII() : m_token(0), m_initialized(false) {
			GdiplusStartupInput input;
			m_initialized = (GdiplusStartup(&m_token, &input, NULL) == Ok);
		}
		~GdiplusRAII() {
			if (m_initialized) {
				GdiplusShutdown(m_token);
			}
		}
		bool IsInitialized() const { return m_initialized; }
	};
	
	class StreamRAII {
	private:
		IStream* m_pStream;
	public:
		StreamRAII() : m_pStream(nullptr) {}
		~StreamRAII() {
			if (m_pStream) {
				m_pStream->Release();
			}
		}
		IStream** GetAddress() { return &m_pStream; }
		IStream* Get() { return m_pStream; }
		bool IsValid() const { return m_pStream != nullptr; }
	};
	
	class ImageRAII {
	private:
		Image* m_pImage;
	public:
		ImageRAII(Image* pImage) : m_pImage(pImage) {}
		~ImageRAII() {
			if (m_pImage) {
				delete m_pImage;
			}
		}
		Image* Get() { return m_pImage; }
		Image* Release() { 
			Image* temp = m_pImage; 
			m_pImage = nullptr; 
			return temp; 
		}
		bool IsValid() const { return m_pImage != nullptr && m_pImage->GetLastStatus() == Ok; }
	};
	
	try
	{
		HRSRC hResource = FindResource(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_SPLASH), L"RCDATA");
		if (hResource == NULL)
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: 找不到PNG资源");
			return false;
		}
		
		HGLOBAL hGlobal = LoadResource(AfxGetInstanceHandle(), hResource);
		if (hGlobal == NULL)
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: 加载资源失败");
			return false;
		}
		
		DWORD dwSize = SizeofResource(AfxGetInstanceHandle(), hResource);
		LPVOID pData = LockResource(hGlobal);
		
		if (pData == NULL || dwSize == 0)
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: 资源数据为空");
			return false;
		}
		
		// 创建内存流
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dwSize);
		if (hMem == NULL)
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: 内存分配失败");
			return false;
		}
		
		LPVOID pMemData = GlobalLock(hMem);
		memcpy(pMemData, pData, dwSize);
		GlobalUnlock(hMem);
		
		StreamRAII streamManager;
		if (CreateStreamOnHGlobal(hMem, TRUE, streamManager.GetAddress()) != S_OK)
		{
			GlobalFree(hMem);
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: 创建内存流失败");
			return false;
		}
		
		// ⭐ 使用RAII管理GDI+生命周期
		GdiplusRAII gdiplusManager;
		if (!gdiplusManager.IsInitialized())
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: GDI+初始化失败");
			return false;
		}
		
		// ⭐ 使用RAII管理Image生命周期
		ImageRAII imageManager(Image::FromStream(streamManager.Get()));
		if (!imageManager.IsValid())
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: GDI+图像加载失败");
			return false;
		}
		
		// 转换为HBITMAP
		HBITMAP hBitmap;
		Color bgColor(255, 255, 255); // 白色背景
		if (((Bitmap*)imageManager.Get())->GetHBITMAP(bgColor, &hBitmap) != Ok)
		{
			WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: 位图转换失败");
			return false;
		}
		
		m_splashBitmap.Attach(hBitmap);
		
		// 调整对话框大小
		SetWindowPos(NULL, 0, 0, imageManager.Get()->GetWidth(), imageManager.Get()->GetHeight(), 
			SWP_NOMOVE | SWP_NOZORDER);
		
		// ⭐ 所有资源将通过RAII自动清理
		WriteDebugLog("[DEBUG] SplashDialog::LoadPNGResource: PNG资源加载成功");
		return true;
	}
	catch (const std::exception& e)
	{
		WriteDebugLog(("[ERROR] SplashDialog::LoadPNGResource: PNG加载异常: " + std::string(e.what())).c_str());
		return false;
	}
	catch (...)
	{
		WriteDebugLog("[ERROR] SplashDialog::LoadPNGResource: PNG加载未知异常");
		return false;
	}
}

// ⭐ 新增：启用文本模式
void CSplashDialog::EnableTextMode()
{
	WriteDebugLog("[DEBUG] SplashDialog::EnableTextMode: 启用文本模式降级处理");
	
	m_bTextModeEnabled = true;
	
	// 清理可能存在的位图资源
	if (m_splashBitmap.GetSafeHandle() != NULL)
	{
		m_splashBitmap.DeleteObject();
	}
	
	// 设置文本模式的对话框大小
	SetWindowPos(NULL, 0, 0, 400, 200, SWP_NOMOVE | SWP_NOZORDER);
	
	// 更新进度消息（线程安全版本）
	SetProgressMessage(L"PNG加载失败，使用文本模式启动"); // ⭐ 使用线程安全的设置方法
	
	WriteDebugLog("[DEBUG] SplashDialog::EnableTextMode: 文本模式设置完成");
}

// ⭐ 新增：文本模式背景绘制
BOOL CSplashDialog::OnEraseBkgnd(CDC* pDC)
{
	if (m_bTextModeEnabled)
	{
		// 文本模式下使用自定义背景
		CRect clientRect;
		GetClientRect(&clientRect);
		pDC->FillSolidRect(&clientRect, RGB(70, 130, 180)); // 钢蓝色背景
		return TRUE;
	}
	else
	{
		// 图形模式使用默认背景处理
		return CDialogEx::OnEraseBkgnd(pDC);
	}
}

// ⭐ 新增：线程安全的状态管理方法
bool CSplashDialog::GetInitializationCompleteState() const
{
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_bInitializationComplete;
}

void CSplashDialog::SetInitializationCompleteState(bool bComplete)
{
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_bInitializationComplete = bComplete;
	WriteDebugLog(bComplete ? "[DEBUG] SplashDialog: 初始化状态设置为完成" : "[DEBUG] SplashDialog: 初始化状态设置为未完成");
}

CString CSplashDialog::GetProgressMessage() const
{
	std::lock_guard<std::mutex> lock(m_progressMutex);
	return m_strProgressMessage;
}

void CSplashDialog::SetProgressMessage(const CString& message)
{
	std::lock_guard<std::mutex> lock(m_progressMutex);
	m_strProgressMessage = message;
	WriteDebugLog(("[DEBUG] SplashDialog: 进度消息更新为: " + std::string(CT2A(message))).c_str());
}