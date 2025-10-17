// StatusDisplayManager.h - 状态展示与日志统一管理模块
#pragma once

#include <afxwin.h>
#include <string>
#include <memory>
#include <functional>

// 前向声明
class CPortMasterDlg;

// 状态展示与日志管理器 - 负责统计信息、日志显示、进度条等UI状态管理
class StatusDisplayManager
{
public:
	StatusDisplayManager();
	~StatusDisplayManager();

	// 初始化：绑定对话框实例
	void Initialize(CPortMasterDlg* parentDialog);

	// 统计信息更新方法
	void UpdateSpeedDisplay(uint32_t sendSpeed, uint32_t receiveSpeed);
	void UpdateSendStatistics(uint64_t bytesSent);
	void UpdateReceiveStatistics(uint64_t bytesReceived);
	void UpdateAllStatistics(uint64_t bytesSent, uint64_t bytesReceived, uint32_t sendSpeed, uint32_t receiveSpeed);

	// 日志显示方法
	void LogMessage(const CString& message);
	void LogFormattedMessage(CString format, ...);
	void ClearLog();

	// 进度条控制方法
	void SetProgressPercent(int percent, bool forceReset = false);
	void ResetProgress();
	void SetProgressRange(int min, int max);

	// 节流显示管理
	void StartThrottledDisplayTimer();
	void StopThrottledDisplayTimer();
	bool IsDisplayUpdatePending() const;
	void SetDisplayUpdatePending(bool pending);
	bool CanUpdateDisplay() const;
	void RecordDisplayUpdate();
	void SetThrottledDisplayCallback(std::function<void()> callback);

	// 状态栏更新方法
	void UpdateConnectionStatus(bool connected);
	void UpdateTransmissionMode(bool reliable);
	void UpdateSendSourceDisplay(const CString& source);
	void UpdateStatusBarText(int controlId, const CString& text);

	// UI控件直接操作封装
	void SetStaticText(int controlId, const CString& text);
	void SetButtonText(int controlId, const CString& text);
	void EnableControl(int controlId, bool enabled);
	CString GetControlText(int controlId) const;

	// 日志格式化辅助方法
	CString BuildTimestampPrefix() const;
	CString FormatDataSize(uint64_t bytes) const;
	CString FormatSpeed(uint32_t bytesPerSecond) const;

private:
	CPortMasterDlg* m_parentDialog;  // 父对话框指针

	// 节流显示管理
	bool m_displayUpdatePending;
	ULONGLONG m_lastDisplayUpdate;
	const DWORD DISPLAY_THROTTLE_MS = 200;  // 显示更新节流间隔(ms)

	// 进度条状态
	int m_lastProgressPercent;
	bool m_progressInitialized;

	// 定时器回调
	std::function<void()> m_throttledDisplayCallback;

	// 内部辅助方法
	void ValidateParentDialog() const;
	void InitializeProgress();
	CString FormatLogMessage(const CString& message) const;
};