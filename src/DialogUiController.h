// DialogUiController.h - 对话框UI控制器
// 职责：控件指针缓存、初始值设置、节流定时器管理、按钮状态更新、状态栏文本更新
#pragma once

#include "afxwin.h"
#include <memory>
#include <functional>

// 前向声明
class CPortMasterDlg;

// 定时器ID枚举
enum UITimerID
{
	TIMER_ID_CONNECTION_STATUS = 1,  // 连接状态恢复定时器
	TIMER_ID_THROTTLED_DISPLAY = 2   // 节流显示更新定时器
};

// UI控件引用结构体 - 集中管理所有控件指针
struct UIControlRefs
{
	// 基本按钮控件
	CButton* btnConnect;
	CButton* btnDisconnect;
	CButton* btnSend;
	CButton* btnStop;
	CButton* btnFile;

	// 操作按钮
	CButton* btnClearAll;
	CButton* btnClearReceive;
	CButton* btnCopyAll;
	CButton* btnSaveAll;

	// 编辑框控件
	CEdit* editSendData;
	CEdit* editReceiveData;
	CEdit* editTimeout;

	// 下拉框控件
	CComboBox* comboPortType;
	CComboBox* comboPort;
	CComboBox* comboBaudRate;
	CComboBox* comboDataBits;
	CComboBox* comboParity;
	CComboBox* comboStopBits;
	CComboBox* comboFlowControl;

	// 选项控件
	CButton* radioReliable;
	CButton* radioDirect;
	CButton* checkHex;
	CButton* checkOccupy;

	// 状态显示控件
	CProgressCtrl* progress;
	CStatic* staticLog;
	CStatic* staticPortStatus;
	CStatic* staticMode;
	CStatic* staticSpeed;
	CStatic* staticSendSource;

	// 父对话框窗口指针
	CWnd* parentDialog;
};

// DialogUiController - UI控制器类
class DialogUiController
{
public:
	DialogUiController();
	~DialogUiController();

	// 初始化：绑定控件引用并执行初始化设置
	void Initialize(const UIControlRefs& controlRefs);

	// 控件初始化方法
	void InitializeControls();              // 初始化所有控件的默认值和选项
	void InitializePortTypeCombo();         // 初始化端口类型下拉框
	void InitializeSerialParameterCombos(); // 初始化串口参数下拉框（波特率、数据位等）
	void InitializeTransmissionModeRadios(); // 初始化传输模式单选按钮
	void InitializeProgressBar();           // 初始化进度条
	void InitializeStatusDisplays();        // 初始化状态显示区域

	// 按钮状态更新方法
	void UpdateConnectionButtons(bool connected);  // 更新连接/断开按钮状态
	void UpdateTransmissionButtons(bool transmitting, bool paused); // 更新发送/停止按钮状态
	void UpdateSaveButton(bool enabled);           // 更新保存按钮状态
	void UpdateAllButtonStates(bool connected, bool transmitting, bool paused, bool hasSaveData); // 统一更新所有按钮状态

	// 状态文本更新方法
	void UpdateConnectionStatus(bool connected);   // 更新连接状态文本
	void UpdateTransmissionMode(bool reliable);    // 更新传输模式文本
	void UpdateSpeedDisplay(uint32_t sendSpeed, uint32_t receiveSpeed); // 更新速度显示
	void UpdateSendSourceDisplay(const CString& source); // 更新发送源显示
	void LogMessage(const CString& message);       // 输出日志消息

	// 直接控件文本设置方法
	void SetSendButtonText(const CString& text);    // 设置发送按钮文本
	void SetStatusText(const CString& text);        // 设置状态栏文本
	void SetModeText(const CString& text);          // 设置模式文本
	void SetReceiveDataText(const CString& text);  // 设置接收编辑框文本
	void SetSendDataText(const CString& text);     // 设置发送编辑框文本
	void SetStaticText(int controlId, const CString& text); // 设置静态控件文本

	// 进度条控制方法
	void SetProgressPercent(int percent, bool forceReset = false); // 设置进度条百分比
	void ResetProgress();                          // 重置进度条

	// 节流定时器管理
	void StartThrottledDisplayTimer();             // 启动节流显示定时器
	void StopThrottledDisplayTimer();              // 停止节流显示定时器
	bool IsDisplayUpdatePending() const;           // 查询是否有待处理的显示更新
	void SetDisplayUpdatePending(bool pending);    // 设置显示更新待处理标志
	bool CanUpdateDisplay() const;                 // 判断是否可以立即更新显示（节流控制）
	void RecordDisplayUpdate();                    // 记录本次显示更新时间戳

	// 定时器回调注册
	void SetThrottledDisplayCallback(std::function<void()> callback); // 设置节流显示回调函数

	// 获取控件状态（只读访问）
	bool IsReliableModeSelected() const;           // 是否选择可靠模式
	bool IsHexDisplayEnabled() const;              // 是否启用十六进制显示
	CString GetTimeoutValue() const;               // 获取超时值
	int GetSelectedPortType() const;               // 获取选择的端口类型索引
	CString GetSendDataText() const;               // 获取发送编辑框文本
	CString GetReceiveDataText() const;            // 获取接收编辑框文本

private:
	UIControlRefs m_controls;                      // 控件引用集合

	// 节流状态管理
	bool m_receiveDisplayPending;                  // 是否有待处理的接收显示更新
	ULONGLONG m_lastReceiveDisplayUpdate;          // 最后一次更新接收显示的时间戳
	const DWORD RECEIVE_DISPLAY_THROTTLE_MS = 200; // 接收显示更新节流间隔(ms)

	// 进度条状态
	uint32_t m_lastProgressPercent;                // 最近一次成功更新的进度百分比（单调性保护）

	// 定时器回调
	std::function<void()> m_throttledDisplayCallback; // 节流显示回调函数

	// 内部辅助方法
	void ValidateControlRefs() const;              // 验证控件引用有效性
	bool IsControlValid(CWnd* control) const;      // 检查单个控件是否有效
};
