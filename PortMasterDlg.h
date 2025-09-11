#pragma execution_character_set("utf-8")
#pragma once
#include <afxrich.h>

#include "afxdialogex.h"
#include "Common/DataFormatter.h"
#include "Common/DeviceManager.h"
#include "Protocol/ReliableChannel.h"
#include "Protocol/ProtocolManager.h"
#include "Transport/LoopbackTransport.h"
#include "Transport/SerialTransport.h"
#include "Transport/LptSpoolerTransport.h" 
#include "Transport/UsbPrinterTransport.h"
#include "Transport/TcpTransport.h"
#include "Transport/UdpTransport.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <string>
#include <vector>

// 自定义消息定义（线程安全UI更新）
#define WM_UPDATE_PROGRESS      (WM_USER + 1001)
#define WM_UPDATE_COMPLETION    (WM_USER + 1002)
#define WM_UPDATE_FILE_RECEIVED (WM_USER + 1003)
#define WM_DISPLAY_RECEIVED_DATA (WM_USER + 1004)

// 前置声明

// 传输状态枚举
enum class TransmissionState
{
	IDLE = 0,        // 空闲状态
	TRANSMITTING,    // 传输中
	PAUSED,          // 暂停状态
	COMPLETED,       // 传输完成
	FAILED           // 传输失败
};

// 传输上下文结构体 - 支持断点续传功能 (SOLID-S: 单一职责)
struct TransmissionContext
{
	CString sourceFilePath;          // 源文件路径
	CString targetIdentifier;        // 目标标识符（端口/网络地址等）
	size_t totalBytes;               // 文件总大小（字节）
	size_t transmittedBytes;         // 已传输字节数
	size_t lastChunkSize;            // 最后一个数据块大小
	DWORD startTimestamp;            // 传输开始时间戳
	DWORD lastUpdateTimestamp;       // 最后更新时间戳
	double averageSpeed;             // 平均传输速度 (bytes/sec)
	bool isValidContext;             // 上下文是否有效
	
	// 构造函数：初始化为无效上下文
	TransmissionContext()
		: totalBytes(0)
		, transmittedBytes(0)
		, lastChunkSize(0)
		, startTimestamp(0)
		, lastUpdateTimestamp(0)
		, averageSpeed(0.0)
		, isValidContext(false)
	{
	}
	
	// 重置上下文 (KISS原则：简单清晰的状态管理)
	void Reset()
	{
		sourceFilePath.Empty();
		targetIdentifier.Empty();
		totalBytes = 0;
		transmittedBytes = 0;
		lastChunkSize = 0;
		startTimestamp = 0;
		lastUpdateTimestamp = 0;
		averageSpeed = 0.0;
		isValidContext = false;
	}
	
	// 计算传输进度百分比 (0-100)
	double GetProgressPercentage() const
	{
		return (totalBytes > 0) ? (static_cast<double>(transmittedBytes) / totalBytes * 100.0) : 0.0;
	}
	
	// 检查是否可以续传
	bool CanResume() const
	{
		return isValidContext && (transmittedBytes < totalBytes) && (transmittedBytes > 0);
	}
};

// CPortMasterDlg 对话框
class CPortMasterDlg : public CDialogEx
{
// 构造
public:
	CPortMasterDlg(CWnd* pParent = nullptr);	// 标准构造函数
	virtual ~CPortMasterDlg();					// 析构函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PORTMASTER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedConnect();
	afx_msg void OnBnClickedDisconnect();
	afx_msg void OnBnClickedSend();
	afx_msg void OnBnClickedStop();				// 停止传输按钮事件处理
	afx_msg void OnBnClickedClearInput();
	afx_msg void OnBnClickedClearDisplay();
	afx_msg void OnBnClickedClear();
	afx_msg void OnCbnSelchangePortType();
	afx_msg void OnBnClickedRefreshPorts();
	afx_msg void OnBnClickedReliableMode();
	afx_msg void OnBnClickedLoadFile();
	afx_msg void OnBnClickedSaveFile();
	afx_msg void OnBnClickedCopy();
	afx_msg void OnBnClickedTestWizard();
	afx_msg void OnBnClickedLogWindow();
	afx_msg void OnBnClickedCopyHex();
	afx_msg void OnBnClickedCopyText();
	afx_msg void OnBnClickedHexDisplay();
	afx_msg void OnBnClickedRefreshDevices(); // 刷新设备列表
	afx_msg BOOL OnDeviceChange(UINT nEventType, DWORD_PTR dwData);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	// OnSize 已移除 - 固定窗口大小，无需响应式布局
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);		// 最小窗口尺寸限制
	afx_msg void OnEnChangeInputHex();						// 输入框内容变化事件处理
	afx_msg LRESULT OnDisplayReceivedData(WPARAM wParam, LPARAM lParam);  // 线程安全数据显示
	afx_msg LRESULT OnDelayedScrollToBottom(WPARAM wParam, LPARAM lParam); // 延迟滚动处理器
	
	// 线程安全UI更新消息处理函数
	afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);        // 进度更新
	afx_msg LRESULT OnUpdateCompletion(WPARAM wParam, LPARAM lParam);      // 完成状态更新
	afx_msg LRESULT OnUpdateFileReceived(WPARAM wParam, LPARAM lParam);    // 文件接收更新
	afx_msg LRESULT OnDisplayReceivedDataMsg(WPARAM wParam, LPARAM lParam); // 🔑 数据显示更新

	DECLARE_MESSAGE_MAP()

private:
	// 控件变量
	CComboBox m_ctrlPortType;
	CComboBox m_ctrlPortList;
	CComboBox m_ctrlBaudRate;
	CComboBox m_ctrlDataBits;
	CComboBox m_ctrlStopBits;
	CComboBox m_ctrlParity;
	CButton m_ctrlConnectBtn;
	CButton m_ctrlDisconnectBtn;
	CButton m_ctrlSendBtn;
	CButton m_ctrlStopBtn;				// 停止传输按钮
	CButton m_ctrlClearInputBtn;
	CButton m_ctrlClearDisplayBtn;
	CButton m_ctrlLoadFileBtn;
	CButton m_ctrlSaveFileBtn;
	CButton m_ctrlCopyBtn;				// 统一的复制按钮（替代HEX和TEXT分别复制）
	CButton m_ctrlCopyHexBtn;			// 十六进制复制按钮
	CButton m_ctrlCopyTextBtn;			// 文本复制按钮
	CButton m_ctrlReliableMode;
	CButton m_ctrlHexDisplayCheck;		// 十六进制显示复选框
	CEdit m_ctrlInputHex;

	CEdit m_ctrlDataView;			// 统一的数据显示控件（替代HexView和TextView）
	CStatic m_ctrlLog;
	CProgressCtrl m_ctrlProgress;
	CStatic m_ctrlProtocolStatus;
	CStatic m_ctrlTransferSpeed;
	CStatic m_ctrlTransferProgress;
	CStatic m_ctrlStatus;
	CStatic m_ctrlConnectionStatus;		// 连接状态显示
	CStatic m_ctrlTransferStatus;		// 传输状态显示
	CStatic m_ctrlDataSourceLabel;		// 数据源显示标签
	// CStatic m_ctrlFileDropZone;		// 已删除：与IDC_INPUT_HEX重叠，拖放功能集成到输入框

	// 新增成员变量
	bool m_bHexDisplay;					// 十六进制显示模式标志


	// 状态变量
	bool m_bConnected;
	bool m_bReliableMode;
	
	// 设备和协议管理器
	std::shared_ptr<DeviceManager> m_deviceManager;
	std::shared_ptr<ProtocolManager> m_protocolManager;
	
	// 可靠传输
	std::shared_ptr<ITransport> m_transport;
	std::shared_ptr<ReliableChannel> m_reliableChannel;
	std::function<void(const std::vector<uint8_t>&)> m_directTransportCallback;  // 直接传输回调
	
	// 传输状态变量（线程安全保护）
	std::atomic<bool> m_bTransmitting;
	TransmissionState m_transmissionState;  // 传输状态管理
	TransmissionContext m_transmissionContext; // 断点续传上下文信息
	std::vector<uint8_t> m_transmissionData;  // 发送数据缓冲区（文件拖放数据）
	std::vector<uint8_t> m_displayedData;    // 当前显示在hex/text view中的数据（用于保存功能）
	mutable std::mutex m_displayDataMutex;   // 显示数据互斥锁
	// 已移除 m_pendingDisplayData，直接传输现在使用立即显示机制
	CString m_currentFileName;               // 当前加载的文件名
	size_t m_transmissionProgress;
	UINT_PTR m_transmissionTimer;
	int m_transmissionCounter;               // 传输次数计数器
	
	// 分片传输模拟
	std::vector<uint8_t> m_chunkTransmissionData;
	size_t m_chunkTransmissionIndex;
	size_t m_chunkSize;
	
	// 文件传输相关成员变量（与实现代码一致）
	size_t m_transmissionIndex;               // 当前传输索引
	
	// 传输速度统计
	DWORD m_transmissionStartTime;           // 传输开始时间
	size_t m_totalBytesTransmitted;          // 已传输字节数
	DWORD m_lastSpeedUpdateTime;             // 上次速度更新时间
	
	// Stage 3 新增：自动重试机制变量
	int m_currentRetryCount;                 // 当前重试次数
	int m_maxRetryCount;                     // 最大重试次数
	CString m_lastFailedOperation;           // 最后失败的操作
	
	// 初始化函数
	void InitializeControls();
	void InitializeDeviceManager();
	void InitializeProtocolManager();
	void InitializeTransportObjects();
	
	// 传输工厂方法 (SOLID-S: 单一职责, SOLID-O: 开闭原则)
	std::shared_ptr<ITransport> CreateTransportFromUI();
	TransportConfig GetTransportConfigFromUI();
	
	// 状态反馈优化 (SOLID-S: 单一职责)
	CString GetConnectionStatusMessage(TransportState state, const std::string& error = "");
	CString FormatTransportInfo(const std::string& transportType, const std::string& endpoint = "");
	CString GetDetailedErrorSuggestion(int transportIndex, const std::string& error);
	void SetupReceiveDirectory();
	void ConfigureReliableChannelFromConfig(); // 新增：从配置管理器设置可靠通道参数 (SOLID-S: 单一职责)
	std::string GetNetworkConnectionInfo(const std::string& transportType);
	void InitializeTransportObjects(int portType); // 新增：支持指定端口类型的初始化
	void SetupTransportCallbacks(); // 新增：设置传输对象回调
	void ConfigureTransportCallback();
	void UpdatePortList();
	void UpdatePortListFromDeviceManager(); // 使用DeviceManager更新端口列表
	void RefreshDeviceList(); // SOLID-S: 专门负责刷新设备列表
	void ShowDeviceDetails(const DeviceInfo& device); // 显示设备详细信息
	void ShowDeviceHistory(); // 显示设备历史记录
	void ShowFavoriteDevices(); // 显示收藏设备
	void AddCurrentDeviceToFavorites(); // 将当前设备添加到收藏
	void LoadDeviceFromHistory(const DeviceInfo& device); // 从历史记录加载设备
	bool ProcessDeviceCommand(const CString& command); // 处理设备管理命令
	void UpdateProtocolStatus(); // 更新协议状态显示
	void ShowProtocolConfiguration(); // 显示协议配置信息
    void UpdateButtonStates();
    void UpdateStatusBar();                    // Stage 3 新增：综合状态栏信息更新 (SOLID-S: 单一职责)
    CString GetCurrentTransferSpeed();         // Stage 3 新增：获取当前传输速度信息
    void EnsureInputEditorVisible();
	void UpdateEnhancedProtocolStatus();	// 增强的协议状态可视化显示
	
	// 新增：端口类型相关控制方法 (SOLID-S: 单一职责)
	void UpdatePortTypeSpecificControls();
	bool ValidateConnectionStateConsistency();
	bool CheckConnectionStateOnPortChange();
	void AppendLog(const CString& message);
	void AppendLogWithDetails(const CString& message, size_t bytes = 0);
	void UpdateTransferSpeed(size_t bytesTransferred);
	void AppendHexData(const BYTE* data, size_t length, bool incoming);
	void AppendTextData(const CString& text, bool incoming);
	
	// 统一输入处理方法
	bool IsHexFormatInput(const CString& input);
	std::vector<uint8_t> ProcessHexInput(const CString& hexInput);
	std::vector<uint8_t> ProcessTextInput(const CString& textInput);
	bool ValidateHexInput(const CString& hexText);
	std::vector<uint8_t> GetInputData();
	void DisplayReceivedData(const std::vector<uint8_t>& data);
	void DisplaySendData(const std::vector<uint8_t>& data);  // 新增：显示待发送数据到输入区域
	
	// 数据格式化方法 (SOLID-S: 单一职责 - 数据格式转换)
	CString FormatDataAsHex(const std::vector<uint8_t>& data);    // 格式化为十六进制字符串
	CString FormatDataAsText(const std::vector<uint8_t>& data);   // 格式化为文本字符串
	
	// 统一显示管理方法 (SOLID-S: 单一职责 - 显示逻辑统一)
	void UpdateDataDisplay();                                     // 统一的数据显示更新逻辑
	void RefreshDataView();                                       // 刷新数据视图控件
	
	// 文件拖放辅助方法
	bool LoadFileForTransmission(const CString& filePath);
	void ShowFileLoadProgress(const CString& filename, size_t totalSize, size_t loaded);
	void ShowFileTransmissionProgress(const CString& filename, size_t totalSize, size_t transmitted);
	
	// 传输数据结构
	struct TransmissionData {
		std::vector<uint8_t> data;
		bool isFileTransmission = false;
		CString displayName;
	};
	
	// 数据传输方法（重构版）
	bool ValidateConnectionState();
	bool PrepareTransmissionData(TransmissionData& transmissionData);
	void ExecuteReliableTransmission(const TransmissionData& transmissionData);
	void ExecuteDirectTransmission(const TransmissionData& transmissionData);
	void HandleDirectTransmissionSuccess(const TransmissionData& transmissionData);
	void SimulateChunkTransmission(const TransmissionData& transmissionData);
	void OnChunkTransmissionTimer();
	void StopDataTransmission(bool completed);                      // 第四阶段新增：停止传输
	void UpdateTransmissionProgress();                              // 第四阶段新增：更新传输进度
	
	// 传输状态管理方法 (SOLID-S: 单一职责)
	void SetTransmissionState(TransmissionState newState);          // 设置传输状态
	TransmissionState GetTransmissionState() const;                 // 获取当前传输状态  
	bool IsTransmissionActive() const;                              // 检查传输是否活跃
	
	// 断点续传功能方法 (SOLID-S: 单一职责, KISS: 简洁的续传逻辑)
	void SaveTransmissionContext(const CString& filePath, size_t totalBytes, size_t transmittedBytes); // 保存传输断点
	bool LoadTransmissionContext();                                 // 加载传输断点
	void ClearTransmissionContext();                               // 清除传输断点
	CString GetTransmissionContextFilePath() const;                // 获取断点文件路径
	bool ResumeTransmission();                                      // 续传功能实现
	bool ShouldEchoTransmittedData() const;                        // 第四阶段新增：回显策略判断
	void DisplayReceivedDataChunk(const std::vector<uint8_t>& chunk); // 第四阶段新增：分块数据显示
	void HandleTransmissionError(const CString& operation, const std::string& error);
	void ShowDetailedErrorMessage(const CString& operation, const CString& error, const CString& suggestion = L""); // Stage 3 新增：详细错误信息显示
	void HandleTransmissionErrorWithSuggestion(const CString& errorMsg, bool canRetry = true); // Stage 3 新增：传输错误处理建议
	bool AttemptAutoRetry(const CString& operation, int maxRetries = 3); // Stage 3 新增：自动重试机制
	void DisplayReceivedDataImmediate(const std::vector<uint8_t>& data, const CString& sourceName);
	CString FormatHexDisplay(const std::vector<uint8_t>& data);
	CString FormatHexDisplaySimple(const std::vector<uint8_t>& data);  // 用于复制的简化格式
	CString FormatTextDisplay(const std::vector<uint8_t>& data);
	CString FormatMixedDisplay(const std::vector<uint8_t>& data);      // 🔑 智能混合显示
	bool IsValidUTF8Sequence(const std::vector<uint8_t>& data, size_t start, size_t& length); // 🔑 UTF-8序列验证
	void ScrollToBottom();
	void DisplayDataAppendMode(const std::vector<uint8_t>& data);
	
	// 数据传输方法（原有，保留兼容性）
	void StartDataTransmission(const std::vector<uint8_t>& data);
	void OnTransmissionTimer();
	
	// 窗口管理方法
	void SetFixedWindowSize();
	
	// 数据源状态显示
	void UpdateDataSourceDisplay(const CString& source);
	bool HasValidInputData();
	
	// 数据显示格式控制 - 简化版本
	void RefreshDataDisplay();
	void SetDataDisplayFormat(bool hexMode);
	
	// 用户反馈机制
	void ShowUserMessage(const CString& title, const CString& message, UINT type = MB_ICONINFORMATION);
	
	// 新增：连接状态管理方法 (SOLID-O: 开放封闭)
	void CleanupTransportObjects();
	void SynchronizeUIWithTransportState();
	
	// 响应式布局方法
	void ResizeControls(int cx, int cy);
	void InitializeLayoutConstants();
	CRect CalculateControlRect(int left, int top, int width, int height, int cx, int cy);
	
	// 布局常量
	struct LayoutConstants {
		static const int LEFT_PANEL_WIDTH = 142;
		static const int RIGHT_PANEL_WIDTH = 100; 
		static const int MIN_WINDOW_WIDTH = 704;
		static const int MIN_WINDOW_HEIGHT = 495;
		static const int MARGIN = 8;
		static const int GROUP_SPACING = 8;
	};

	// 应用程序常量
	struct AppConstants {
		// 文件和数据处理常量
		static const size_t BYTES_IN_MEGABYTE = 1024 * 1024;		// 1MB
		static const size_t MAX_FILE_SIZE = BYTES_IN_MEGABYTE;		// 文件大小限制
		static const size_t BYTES_PER_LINE = 8;					// 每行显示字节数
		
		// 定时器常量
		static const UINT TRANSMISSION_TIMER_ID = 1001;			// 传输定时器ID
		static const UINT TRANSMISSION_TIMER_INTERVAL = 100;		// 传输定时器间隔(ms)
		static const UINT TIMER_ID_CHUNK_TRANSMISSION = 1002;		// 分块传输定时器ID
		
		// 数据转换常量（使用内联函数避免静态double初始化问题）
		static double GetBytesToMegabytes() { return 1.0 / (1024.0 * 1024.0); }
		static double GetBytesToKilobytes() { return 1.0 / 1024.0; }
	};

public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
