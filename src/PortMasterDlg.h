// PortMasterDlg.h : 头文件
#pragma once
#include "afxwin.h"
#include "../Transport/ITransport.h"
#include "../Protocol/ReliableChannel.h"
#include "../Common/ConfigStore.h"
#include "TransmissionTask.h"
#include "UIStateManager.h"
#include "TransmissionStateManager.h"
#include "ButtonStateManager.h"
#include "ThreadSafeUIUpdater.h"
#include "ThreadSafeProgressManager.h"
#include <memory>
#include <thread>
#include <atomic>
#include <fstream>
#include <chrono>

// CPortMasterDlg 对话框
class CPortMasterDlg : public CDialogEx
{
public:
	CPortMasterDlg(CWnd *pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum
	{
		IDD = IDD_PORTMASTER_DIALOG
	};
#endif

protected:
	virtual void DoDataExchange(CDataExchange *pDX);

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	// 新增事件处理函数
	afx_msg void OnBnClickedButtonClearAll();
	afx_msg void OnBnClickedButtonClearReceive();
	afx_msg void OnBnClickedButtonCopyAll();
	afx_msg void OnBnClickedButtonSaveAll();
	afx_msg void OnEnChangeEditTimeout();

	// Logging functionality
	void LogMessage(const CString &message);
	void WriteLog(const std::string &message);

	// 端口类型切换处理
	void UpdatePortParameters();

	// 数据处理函数
	void SendData();
	void ReceiveData();
	void UpdateDataDisplay();
	void LoadDataFromFile(const CString &filePath);
	void SaveDataToFile(const CString &filePath, const CString &data);
	void SaveBinaryDataToFile(const CString &filePath, const std::vector<uint8_t> &data);

	// 传输控制函数
	void StartTransmission();
	void PauseTransmission();
	void ResumeTransmission();
	void PerformDataTransmission();
	void ClearAllCacheData();  // 新增：清除所有缓存数据

	// 【P1修复】传输任务回调处理 - UI与传输解耦
	void OnTransmissionProgress(const TransmissionProgress& progress);
	void OnTransmissionCompleted(const TransmissionResult& result);
	void OnTransmissionLog(const std::string& message);

	// 十六进制转换函数
	CString StringToHex(const CString &str);
	CString BytesToHex(const BYTE* data, size_t length); // 新增：处理原始字节数据
	CString HexToString(const CString &hex);
	CString ExtractHexAsciiText(const CString &hex);

	// 基于缓存的格式转换函数
	void UpdateSendDisplayFromCache();						   // 从发送缓存更新显示
	void UpdateReceiveDisplayFromCache();					   // 从接收缓存更新显示
	void ThrottledUpdateReceiveDisplay();					   // 【UI优化】节流的接收显示更新
	void UpdateSendCache(const CString &data);				   // 更新发送缓存
	void UpdateSendCacheFromBytes(const BYTE* data, size_t length); // 直接从字节数据更新发送缓存（避免编码转换）
	void UpdateSendCacheFromHex(const CString &hexData);	   // 从十六进制字符串更新发送缓存
	void UpdateReceiveCache(const std::vector<uint8_t> &data); // 更新接收缓存

	// 【深层修复】线程安全的接收数据直接落盘函数
	void ThreadSafeAppendReceiveData(const std::vector<uint8_t> &data); // 接收线程直接落盘，避免消息队列异步延迟

	// 【深层修复】轻量级UI更新信息结构
	struct UIUpdateInfo {
		size_t dataSize;
		std::string hexPreview;
		std::string asciiPreview;
	};

	// 【简化保存流程】移除复杂的数据稳定性检测，采用直接保存+后验证模式

private:
	// 二进制数据显示状态管理
	bool m_binaryDataDetected;  // 是否检测到二进制数据
	CString m_binaryDataPreview;  // 二进制数据预览内容（静态）
	bool m_updateDisplayInProgress;  // 标志：正在更新显示，防止事件递归

	// 【UI优化】接收窗口更新节流机制
	bool m_receiveDisplayPending;    // 标志：是否有待处理的接收显示更新
	DWORD m_lastReceiveDisplayUpdate; // 最后一次更新接收显示的时间戳
	const DWORD RECEIVE_DISPLAY_THROTTLE_MS = 200; // 接收显示更新节流间隔(ms)

	DECLARE_MESSAGE_MAP()

public:
	// 基本控件
	CButton m_btnConnect;
	CButton m_btnDisconnect;
	CButton m_btnSend;
	CButton m_btnStop;
	CButton m_btnFile;

	// 操作按钮
	CButton m_btnClearAll;
	CButton m_btnClearReceive;
	CButton m_btnCopyAll;
	CButton m_btnSaveAll;

	// 编辑框控件
	CEdit m_editSendData;
	CEdit m_editReceiveData;
	CEdit m_editTimeout;

	// 下拉框控件
	CComboBox m_comboPortType;
	CComboBox m_comboPort;
	CComboBox m_comboBaudRate;
	CComboBox m_comboDataBits;
	CComboBox m_comboParity;
	CComboBox m_comboStopBits;
	CComboBox m_comboFlowControl;

	// 选项控件
	CButton m_radioReliable;
	CButton m_radioDirect;
	CButton m_checkHex;
	CButton m_checkOccupy;

	// 状态显示控件
	CProgressCtrl m_progress;
	CStatic m_staticLog;
	CStatic m_staticPortStatus;
	CStatic m_staticMode;
	CStatic m_staticSpeed;
	CStatic m_staticSendSource;

	// Transport层集成
private:
	std::shared_ptr<ITransport> m_transport;
	std::unique_ptr<ReliableChannel> m_reliableChannel;
	std::atomic<bool> m_isConnected;
	std::atomic<bool> m_isTransmitting;
	std::atomic<bool> m_transmissionPaused;  // 新增：传输暂停状态
	std::atomic<bool> m_transmissionCancelled;  // 新增：传输取消状态
	std::thread m_receiveThread;
	std::thread m_transmissionThread;  // 新增：传输线程

	// 【P1修复】传输任务管理 - 实现UI与传输解耦
	std::unique_ptr<TransmissionTask> m_currentTransmissionTask;

	// 【UI响应修复】状态管理器 - 解决状态栏重复显示问题
	std::unique_ptr<UIStateManager> m_uiStateManager;

	// 【传输状态管理统一】传输状态管理器 - 统一状态管理机制
	std::unique_ptr<TransmissionStateManager> m_transmissionStateManager;

	// 【按钮状态控制优化】按钮状态管理器 - 统一按钮控制机制
	std::unique_ptr<ButtonStateManager> m_buttonStateManager;

	// 【UI更新队列机制】线程安全UI更新器 - 队列化UI更新机制
	std::unique_ptr<ThreadSafeUIUpdater> m_threadSafeUIUpdater;

	// 【进度更新安全化】线程安全进度管理器 - 安全化进度更新机制
	std::unique_ptr<ThreadSafeProgressManager> m_threadSafeProgressManager;

	// 配置管理
	ConfigStore &m_configStore;

	// 传输配置
	TransportConfig m_transportConfig;
	ReliableConfig m_reliableConfig;

	// 统计信息
	uint64_t m_bytesSent;
	uint64_t m_bytesReceived;
	uint32_t m_sendSpeed;
	uint32_t m_receiveSpeed;

	// 源数据缓存 - 用于确保显示模式切换时数据一致性
	std::vector<uint8_t> m_sendDataCache;	 // 发送数据的原始字节缓存
	std::vector<uint8_t> m_receiveDataCache; // 接收数据的原始字节缓存
	bool m_sendCacheValid;					 // 发送缓存是否有效
	bool m_receiveCacheValid;				 // 接收缓存是否有效

	// 临时缓存文件管理
	CString m_tempCacheFilePath;			 // 临时缓存文件路径
	std::ofstream m_tempCacheFile;			 // 临时缓存文件流
	bool m_useTempCacheFile;				 // 是否使用临时缓存文件
	uint64_t m_totalReceivedBytes;			 // 总接收字节数
	uint64_t m_totalSentBytes;				 // 总发送字节数

	// 【并发安全修复】临时缓存文件访问同步
	mutable std::mutex m_tempCacheMutex;	 // 临时缓存文件访问互斥锁
	std::queue<std::vector<uint8_t>> m_pendingWrites; // 待写入数据队列（读取时暂存）

	// 【深层修复】接收数据落盘与保存操作专用同步
	mutable std::mutex m_receiveFileMutex;   // 接收文件访问专用互斥锁（与保存操作完全互斥）

	// 内部方法
	void InitializeTransportConfig();
	bool CreateTransport();
	void DestroyTransport();
	void StartReceiveThread();
	void StopReceiveThread();
	void UpdateConnectionStatus();
	void UpdateStatistics();

	// 【UI响应修复】状态管理器方法
	void UpdateUIStatus();  // 更新UI状态显示
	void InitializeUIStateManager();  // 初始化状态管理器

	// 【传输状态管理统一】传输状态管理器方法
	void InitializeTransmissionStateManager();  // 初始化传输状态管理器
	void OnTransmissionStateChanged(TransmissionUIState oldState, TransmissionUIState newState);  // 传输状态变化回调

	// 【按钮状态控制优化】按钮状态管理器方法
	void InitializeButtonStateManager();  // 初始化按钮状态管理器
	void OnButtonStateChanged(ButtonID buttonId, ButtonState newState, const std::string& reason);  // 按钮状态变化回调
	void UpdateButtonStates();  // 更新按钮状态显示

	// 【UI更新队列机制】线程安全UI更新器方法
	void InitializeThreadSafeUIUpdater();  // 初始化线程安全UI更新器
	void UpdateUIStatusThreadSafe(const std::string& status);  // 线程安全的状态更新
	void UpdateProgressThreadSafe(int progress);  // 线程安全的进度更新

	// 【进度更新安全化】线程安全进度管理器方法
	void InitializeThreadSafeProgressManager();  // 初始化线程安全进度管理器
	void OnProgressChanged(const ProgressInfo& progress);  // 进度变化回调
	void UpdateProgressSafe(uint64_t current, uint64_t total, const std::string& status = "");  // 安全的进度更新

	// 【可靠模式按钮管控】保存按钮状态控制
	void UpdateSaveButtonStatus();
	void OnTransportDataReceived(const std::vector<uint8_t> &data);
	void OnTransportError(const std::string &error);
	void OnReliableProgress(uint32_t progress);
	void OnReliableComplete(bool success);
	void OnReliableStateChanged(bool connected);

	// 临时缓存文件管理方法
	bool InitializeTempCacheFile();
	void CloseTempCacheFile();
	bool WriteDataToTempCache(const std::vector<uint8_t>& data);
	std::vector<uint8_t> ReadDataFromTempCache(uint64_t offset, size_t length);
	std::vector<uint8_t> ReadAllDataFromTempCache();
	void ClearTempCacheFile();

	// 【临时文件状态监控与自动恢复】新增机制
	bool CheckAndRecoverTempCacheFile();
	void LogTempCacheFileStatus(const std::string& context);
	bool VerifyTempCacheFileIntegrity();

	// 【保存完整性误报修复】不加锁的读取版本，用于已持锁的保存流程
	std::vector<uint8_t> ReadDataFromTempCacheUnlocked(uint64_t offset, size_t length);
	std::vector<uint8_t> ReadAllDataFromTempCacheUnlocked();

	// 【数据稳定性检测】保存后验证机制
	bool VerifySavedFileSize(const CString& filePath, size_t expectedSize);

	// 重写虚函数用于资源清理
	virtual void PostNcDestroy();

	// 配置管理方法
	void LoadConfigurationFromStore();
	void SaveConfigurationToStore();
	void OnConfigurationChanged();

	// 消息处理函数声明
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonDisconnect();
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonFile();

	// 控件事件处理
	afx_msg void OnCbnSelchangeComboPortType();
	afx_msg void OnBnClickedCheckHex();
	afx_msg void OnBnClickedRadioReliable();
	afx_msg void OnBnClickedRadioDirect();
	afx_msg void OnEnChangeEditSendData();
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	// 文件拖拽处理
	afx_msg void OnDropFiles(HDROP hDropInfo);

	// 消息处理函数
	afx_msg LRESULT OnTransportDataReceivedMessage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransportErrorMessage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransmissionProgressRange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransmissionProgressUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransmissionStatusUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransmissionComplete(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransmissionError(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCleanupTransmissionTask(WPARAM wParam, LPARAM lParam);
};