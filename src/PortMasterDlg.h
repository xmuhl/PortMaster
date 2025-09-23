// PortMasterDlg.h : 头文件
#pragma once
#include "afxwin.h"
#include "../Transport/ITransport.h"
#include "../Protocol/ReliableChannel.h"
#include "../Common/ConfigStore.h"
#include <memory>
#include <thread>
#include <atomic>

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

	// 十六进制转换函数
	CString StringToHex(const CString &str);
	CString HexToString(const CString &hex);

	// 基于缓存的格式转换函数
	void UpdateSendDisplayFromCache();						   // 从发送缓存更新显示
	void UpdateReceiveDisplayFromCache();					   // 从接收缓存更新显示
	void UpdateSendCache(const CString &data);				   // 更新发送缓存
	void UpdateReceiveCache(const std::vector<uint8_t> &data); // 更新接收缓存

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
	std::thread m_receiveThread;

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

	// 内部方法
	void InitializeTransportConfig();
	bool CreateTransport();
	void DestroyTransport();
	void StartReceiveThread();
	void StopReceiveThread();
	void UpdateConnectionStatus();
	void UpdateStatistics();
	void OnTransportDataReceived(const std::vector<uint8_t> &data);
	void OnTransportError(const std::string &error);
	void OnReliableProgress(uint32_t progress);
	void OnReliableComplete(bool success);
	void OnReliableStateChanged(bool connected);

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

	// 自定义消息处理
	afx_msg LRESULT OnTransportDataReceivedMessage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTransportErrorMessage(WPARAM wParam, LPARAM lParam);
};