// StatusDisplayManager.cpp - 状态展示与日志统一管理模块实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "StatusDisplayManager.h"
#include "PortMasterDlg.h"
#include "resource.h"
#include <cassert>
#include <cstdarg>

// 构造函数
StatusDisplayManager::StatusDisplayManager()
	: m_parentDialog(nullptr)
	, m_displayUpdatePending(false)
	, m_lastDisplayUpdate(0)
	, m_lastProgressPercent(0)
	, m_progressInitialized(false)
{
}

// 析构函数
StatusDisplayManager::~StatusDisplayManager()
{
	StopThrottledDisplayTimer();
}

// 初始化：绑定对话框实例
void StatusDisplayManager::Initialize(CPortMasterDlg* parentDialog)
{
	m_parentDialog = parentDialog;
	ValidateParentDialog();
	InitializeProgress();
}

// 验证父对话框有效性
void StatusDisplayManager::ValidateParentDialog() const
{
	assert(m_parentDialog != nullptr && "父对话框指针无效");
}

// 初始化进度条
void StatusDisplayManager::InitializeProgress()
{
	if (!m_parentDialog)
		return;

	// 初始化进度条范围和初始值
	m_parentDialog->SetDlgItemText(IDC_STATIC_SPEED, _T("0%"));
	SetProgressPercent(0, true);
	m_progressInitialized = true;
}

// 更新传输进度显示
void StatusDisplayManager::UpdateProgressDisplay(int progressPercent)
{
	if (!m_parentDialog)
		return;

	// 边界检查
	if (progressPercent < 0)
		progressPercent = 0;
	if (progressPercent > 100)
		progressPercent = 100;

	CString progressText;
	progressText.Format(_T("%d%%"), progressPercent);
	m_parentDialog->SetDlgItemText(IDC_STATIC_SPEED, progressText);
}

// 更新发送统计
void StatusDisplayManager::UpdateSendStatistics(uint64_t bytesSent)
{
	if (!m_parentDialog)
		return;

	CString sentText;
	sentText.Format(_T("%llu"), bytesSent);
	m_parentDialog->SetDlgItemText(IDC_STATIC_SENT, sentText);
}

// 更新接收统计
void StatusDisplayManager::UpdateReceiveStatistics(uint64_t bytesReceived)
{
	if (!m_parentDialog)
		return;

	CString receivedText;
	receivedText.Format(_T("%llu"), bytesReceived);
	m_parentDialog->SetDlgItemText(IDC_STATIC_RECEIVED, receivedText);
}

// 更新所有统计信息
void StatusDisplayManager::UpdateAllStatistics(uint64_t bytesSent, uint64_t bytesReceived,
	uint32_t sendSpeed, uint32_t receiveSpeed)
{
	UpdateSendStatistics(bytesSent);
	UpdateReceiveStatistics(bytesReceived);
	// 注释掉速度显示更新，现在使用进度显示
	// UpdateSpeedDisplay(sendSpeed, receiveSpeed);
}

// 输出日志消息
void StatusDisplayManager::LogMessage(const CString& message)
{
	if (!m_parentDialog)
		return;

	CString formattedMessage = FormatLogMessage(message);
	m_parentDialog->SetDlgItemText(IDC_STATIC_LOG, formattedMessage);
}

// 格式化日志消息输出
void StatusDisplayManager::LogFormattedMessage(CString format, ...)
{
	if (!m_parentDialog)
		return;

	CString formattedMessage;
	va_list args;
	va_start(args, format);
	formattedMessage.FormatV(format, args);
	va_end(args);

	LogMessage(formattedMessage);
}

// 清空日志
void StatusDisplayManager::ClearLog()
{
	if (!m_parentDialog)
		return;

	m_parentDialog->SetDlgItemText(IDC_STATIC_LOG, _T(""));
}

// 设置进度条百分比
void StatusDisplayManager::SetProgressPercent(int percent, bool forceReset)
{
	if (!m_parentDialog || !m_progressInitialized)
		return;

	// 单调性保护：防止进度条倒退（除非强制重置）
	if (!forceReset && percent < m_lastProgressPercent)
	{
		return; // 忽略倒退的进度值
	}

	// 边界检查
	if (percent < 0)
		percent = 0;
	if (percent > 100)
		percent = 100;

	CProgressCtrl* progressCtrl = (CProgressCtrl*)m_parentDialog->GetDlgItem(IDC_PROGRESS);
	if (progressCtrl && progressCtrl->GetSafeHwnd())
	{
		progressCtrl->SetPos(percent);
		m_lastProgressPercent = percent;
	}
}

// 重置进度条
void StatusDisplayManager::ResetProgress()
{
	SetProgressPercent(0, true);
}

// 设置进度条范围
void StatusDisplayManager::SetProgressRange(int min, int max)
{
	if (!m_parentDialog)
		return;

	CProgressCtrl* progressCtrl = (CProgressCtrl*)m_parentDialog->GetDlgItem(IDC_PROGRESS);
	if (progressCtrl && progressCtrl->GetSafeHwnd())
	{
		progressCtrl->SetRange(min, max);
		m_progressInitialized = true;
	}
}

// 启动节流显示定时器
void StatusDisplayManager::StartThrottledDisplayTimer()
{
	if (!m_parentDialog)
		return;

	m_parentDialog->SetTimer(TIMER_ID_THROTTLED_DISPLAY, DISPLAY_THROTTLE_MS, nullptr);
}

// 停止节流显示定时器
void StatusDisplayManager::StopThrottledDisplayTimer()
{
	if (!m_parentDialog)
		return;

	m_parentDialog->KillTimer(TIMER_ID_THROTTLED_DISPLAY);
}

// 查询是否有待处理的显示更新
bool StatusDisplayManager::IsDisplayUpdatePending() const
{
	return m_displayUpdatePending;
}

// 设置显示更新待处理标志
void StatusDisplayManager::SetDisplayUpdatePending(bool pending)
{
	m_displayUpdatePending = pending;
}

// 判断是否可以立即更新显示（节流控制）
bool StatusDisplayManager::CanUpdateDisplay() const
{
	ULONGLONG currentTick = ::GetTickCount64();
	ULONGLONG elapsed = currentTick - m_lastDisplayUpdate;
	return elapsed >= DISPLAY_THROTTLE_MS;
}

// 记录本次显示更新时间戳
void StatusDisplayManager::RecordDisplayUpdate()
{
	m_lastDisplayUpdate = ::GetTickCount64();
}

// 设置节流显示回调函数
void StatusDisplayManager::SetThrottledDisplayCallback(std::function<void()> callback)
{
	m_throttledDisplayCallback = callback;
}

// 更新连接状态
void StatusDisplayManager::UpdateConnectionStatus(bool connected)
{
	if (!m_parentDialog)
		return;

	CString statusText = connected ? _T("已连接") : _T("未连接");
	m_parentDialog->SetDlgItemText(IDC_STATIC_PORT_STATUS, statusText);
}

// 更新传输模式
void StatusDisplayManager::UpdateTransmissionMode(bool reliable)
{
	if (!m_parentDialog)
		return;

	CString modeText = reliable ? _T("可靠") : _T("直通");
	m_parentDialog->SetDlgItemText(IDC_STATIC_MODE, modeText);
}

// 更新发送源显示
void StatusDisplayManager::UpdateSendSourceDisplay(const CString& source)
{
	if (!m_parentDialog)
		return;

	m_parentDialog->SetDlgItemText(IDC_STATIC_SEND_SOURCE, source);
}

// 更新状态栏文本
void StatusDisplayManager::UpdateStatusBarText(int controlId, const CString& text)
{
	SetStaticText(controlId, text);
}

// 设置静态控件文本
void StatusDisplayManager::SetStaticText(int controlId, const CString& text)
{
	if (!m_parentDialog)
		return;

	m_parentDialog->SetDlgItemText(controlId, text);
}

// 设置按钮文本
void StatusDisplayManager::SetButtonText(int controlId, const CString& text)
{
	if (!m_parentDialog)
		return;

	CButton* button = (CButton*)m_parentDialog->GetDlgItem(controlId);
	if (button && button->GetSafeHwnd())
	{
		button->SetWindowText(text);
	}
}

// 启用/禁用控件
void StatusDisplayManager::EnableControl(int controlId, bool enabled)
{
	if (!m_parentDialog)
		return;

	CWnd* control = m_parentDialog->GetDlgItem(controlId);
	if (control && control->GetSafeHwnd())
	{
		control->EnableWindow(enabled ? TRUE : FALSE);
	}
}

// 获取控件文本
CString StatusDisplayManager::GetControlText(int controlId) const
{
	if (!m_parentDialog)
		return _T("");

	CString text;
	m_parentDialog->GetDlgItemText(controlId, text);
	return text;
}

// 构建时间戳前缀
CString StatusDisplayManager::BuildTimestampPrefix() const
{
	CTime time = CTime::GetCurrentTime();
	return time.Format(_T("[%H:%M:%S] "));
}

// 格式化数据大小
CString StatusDisplayManager::FormatDataSize(uint64_t bytes) const
{
	CString sizeText;
	if (bytes < 1024)
	{
		sizeText.Format(_T("%llu B"), bytes);
	}
	else if (bytes < 1024 * 1024)
	{
		sizeText.Format(_T("%.1f KB"), bytes / 1024.0);
	}
	else if (bytes < 1024 * 1024 * 1024)
	{
		sizeText.Format(_T("%.1f MB"), bytes / (1024.0 * 1024.0));
	}
	else
	{
		sizeText.Format(_T("%.1f GB"), bytes / (1024.0 * 1024.0 * 1024.0));
	}
	return sizeText;
}

// 格式化速度
CString StatusDisplayManager::FormatSpeed(uint32_t bytesPerSecond) const
{
	CString speedText;
	if (bytesPerSecond < 1024)
	{
		speedText.Format(_T("%u B/s"), bytesPerSecond);
	}
	else if (bytesPerSecond < 1024 * 1024)
	{
		speedText.Format(_T("%.1f KB/s"), bytesPerSecond / 1024.0);
	}
	else
	{
		speedText.Format(_T("%.1f MB/s"), bytesPerSecond / (1024.0 * 1024.0));
	}
	return speedText;
}

// 格式化日志消息
CString StatusDisplayManager::FormatLogMessage(const CString& message) const
{
	CString formattedMessage = message;

	// 自动换行处理，确保日志文本完整显示
	CRect rect;
	CStatic* logStatic = (CStatic*)m_parentDialog->GetDlgItem(IDC_STATIC_LOG);
	if (logStatic && logStatic->GetSafeHwnd())
	{
		logStatic->GetClientRect(&rect);

		CDC* pDC = logStatic->GetDC();
		if (pDC != nullptr)
		{
			CFont* pOldFont = pDC->SelectObject(logStatic->GetFont());
			const int maxWidth = rect.Width() - 10; // 预留边距

			CString wrappedMessage;
			int startPos = 0;
			while (startPos < formattedMessage.GetLength())
			{
				int chunkEnd = startPos;
				CSize textSize;

				// 找到本行可以容纳的最大字符数
				while (chunkEnd < formattedMessage.GetLength())
				{
					CString chunk = formattedMessage.Mid(startPos, chunkEnd - startPos + 1);
					textSize = pDC->GetTextExtent(chunk);
					if (textSize.cx > maxWidth)
					{
						if (chunkEnd > startPos)
						{
							chunkEnd--;
						}
						break;
					}
					chunkEnd++;
				}

				if (chunkEnd <= startPos)
				{
					chunkEnd = startPos + 1; // 至少显示一个字符
				}

				if (!wrappedMessage.IsEmpty())
				{
					wrappedMessage += _T(" ");
				}
				wrappedMessage += formattedMessage.Mid(startPos, chunkEnd - startPos);

				startPos = chunkEnd;
			}

			pDC->SelectObject(pOldFont);
			logStatic->ReleaseDC(pDC);

			formattedMessage = wrappedMessage;
		}
	}

	return formattedMessage;
}