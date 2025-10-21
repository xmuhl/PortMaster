// PortMasterDlg.h : 头文件
#pragma execution_character_set("utf-8")
#pragma once
#include "afxwin.h"
#include "../Transport/ITransport.h"
#include "../Protocol/ReliableChannel.h"
#include "../Common/ConfigStore.h"
#include "../Common/DataPresentationService.h"
#include "../Common/ReceiveCacheService.h"
#include "DialogConfigBinder.h"
#include "DialogUiController.h"
#include "PortConfigPresenter.h"
#include "TransmissionCoordinator.h"
#include "PortMasterDialogEvents.h"
#include "StatusDisplayManager.h"
#include "../Protocol/PortSessionController.h"
#include <memory>
#include <thread>
#include <atomic>
#include <fstream>
#include <chrono>

// CPortMasterDlg 对话框
class CPortMasterDlg : public CDialogEx
{
public:
	CPortMasterDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum
	{
		IDD = IDD_PORTMASTER_DIALOG
	};
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

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

	void WriteLog(const std::string& message);

	// 端口类型切换处理
	void UpdatePortParameters();

	// 数据处理函数
	void SendData();
	void ReceiveData();
	void UpdateDataDisplay();
	void SaveDataToFile(const CString& filePath, const CString& data);
	void SaveBinaryDataToFile(const CString& filePath, const std::vector<uint8_t>& data);

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

	// 基于缓存的格式转换函数
	void UpdateSendDisplayFromCache();						   // 从发送缓存更新显示
	void UpdateReceiveDisplayFromCache();					   // 从接收缓存更新显示

	// 进度条管理函数
	void SetProgressPercent(int percent, bool forceReset = false);  // 设置进度条百分比
	void ThrottledUpdateReceiveDisplay();					   // 【UI优化】节流的接收显示更新
	void UpdateSendCache(const CString& data);				   // 更新发送缓存
	void UpdateSendCacheFromBytes(const BYTE* data, size_t length); // 直接从字节数据更新发送缓存（避免编码转换）
	void UpdateSendCacheFromHex(const CString& hexData);	   // 从十六进制字符串更新发送缓存
	void UpdateReceiveCache(const std::vector<uint8_t>& data); // 更新接收缓存

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
	bool m_receiveVerboseLogging;    // 是否启用接收数据详细日志

	// 【阶段1迁移】接收窗口更新节流机制已迁移到DialogUiController
	ULONGLONG m_lastUiLogTick;        // 最近一次输出轻量级UI日志的时间戳

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
	std::atomic<bool> m_requiresReconnect;  // 【阶段C新增】模式切换后需要重新连接标志
	std::atomic<bool> m_shutdownInProgress;  // 【阶段二新增】程序关闭时的幂等保护标志
	std::thread m_receiveThread;
	// 【Stage4迁移】移除传输线程，由TransmissionCoordinator管理
	std::mutex m_reliableSessionMutex;         // 可靠模式进度统计互斥锁
	int64_t m_reliableExpectedBytes;           // 最新可靠传输预期字节数
	int64_t m_reliableReceivedBytes;           // 最新可靠传输已接收字节数

	// 【Stage4迁移】传输任务管理已迁移到TransmissionCoordinator

	// 【阶段D迁移】传输任务协调器
	std::unique_ptr<TransmissionCoordinator> m_transmissionCoordinator;

	// 【阶段E迁移】会话管理控制器
	std::unique_ptr<PortSessionController> m_sessionController;

	// 【阶段1迁移】UI控制器 - 统一管理控件初始化和状态更新
	std::unique_ptr<DialogUiController> m_uiController;

	// 【阶段2迁移】端口配置呈现器 - 管理端口枚举和参数配置
	std::unique_ptr<PortConfigPresenter> m_portConfigPresenter;

	// 【阶段4迁移】状态展示管理器 - 统一管理统计、日志、进度显示
	std::unique_ptr<StatusDisplayManager> m_statusDisplayManager;

	// 配置管理
	ConfigStore& m_configStore;
	std::unique_ptr<DialogConfigBinder> m_configBinder;

	// 接收缓存服务
	std::unique_ptr<ReceiveCacheService> m_receiveCacheService;

	// 传输配置
	TransportConfig m_transportConfig;
	ReliableConfig m_reliableConfig;

	// 统计信息
	uint64_t m_bytesSent;
	uint64_t m_bytesReceived;
	uint32_t m_sendSpeed;
	uint32_t m_receiveSpeed;
	// 【阶段1迁移】m_lastProgressPercent已迁移到DialogUiController

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
	void BuildTransportConfigFromUI();  // 【阶段A修复】从UI构建传输配置
	// 【阶段5迁移】CreateTransport/DestroyTransport/StartReceiveThread/StopReceiveThread已迁移到PortSessionController
	void UpdateConnectionStatus();
	CString GetCurrentPortName();  // 获取当前端口显示名称
	int GetCurrentProgressPercent();  // 获取当前传输进度百分比
	void UpdateStatistics();

	// 【可靠模式按钮管控】保存按钮状态控制
	void UpdateSaveButtonStatus();
	void OnTransportDataReceived(const std::vector<uint8_t>& data);
	void OnTransportError(const std::string& error);
	void OnReliableProgress(int64_t current, int64_t total);
	void OnReliableComplete(bool success);
	void OnReliableStateChanged(bool connected);
	void ConfigureReliableLogging();

	// 【阶段3迁移】临时缓存文件管理方法已迁移到ReceiveCacheService

	void BeginReliableReceiveSession();
	void AppendReliablePayload(const std::vector<uint8_t>& data);
	std::vector<uint8_t> SnapshotReliablePayload() const;
	void CompleteReliableReceiveSession();
	void ResetReliableReceiveSession();

	// 【数据稳定性检测】保存后验证机制
	bool VerifySavedFileSize(const CString& filePath, size_t expectedSize);

	// 【阶段二修复】程序关闭时传输资源清理（幂等设计）
	void ShutdownActiveTransmission();

	// 重写虚函数用于资源清理和窗口关闭处理
	virtual void PostNcDestroy();
	virtual void OnCancel();
	virtual void OnClose();

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

	friend class PortMasterDialogEvents;

private:
	std::unique_ptr<PortMasterDialogEvents> m_eventDispatcher;
};
