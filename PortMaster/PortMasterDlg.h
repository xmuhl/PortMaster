#pragma once

#pragma execution_character_set("utf-8")

#include "Common/CommonTypes.h"
#include "Common/ConfigStore.h"
#include "Common/LogCenter.h"
#include "Transport/ITransport.h"
#include "Protocol/ReliableChannel.h"

/// <summary>
/// PortMaster主对话框类
/// </summary>
class CPortMasterDlg : public CDialogEx
{
// 构造
public:
	CPortMasterDlg(CWnd* pParent = nullptr);	// 标准构造函数

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
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	// 控件事件处理
	afx_msg void OnBnClickedConnect();
	afx_msg void OnBnClickedDisconnect();
	afx_msg void OnBnClickedSend();
	afx_msg void OnBnClickedClear();
	afx_msg void OnBnClickedBrowse();
	afx_msg void OnCbnSelchangePortType();
	afx_msg void OnBnClickedReliableMode();
	afx_msg void OnBnClickedHexView();

	DECLARE_MESSAGE_MAP()

private:
	// UI控件
	CComboBox m_ctrlPortType;
	CComboBox m_ctrlPortName;
	CEdit m_ctrlBaudRate;
	CButton m_ctrlConnect;
	CButton m_ctrlDisconnect;
	CButton m_ctrlReliableMode;
	CButton m_ctrlHexView;
	CEdit m_ctrlSendData;
	CButton m_ctrlSend;
	CButton m_ctrlClear;
	CButton m_ctrlBrowse;
	CEdit m_ctrlReceiveData;
	CListCtrl m_ctrlLogList;
	CProgressCtrl m_ctrlProgress;
	CStatic m_ctrlStatus;

	// 核心组件
	PortMaster::ConfigStore configStore_;
	PortMaster::AppConfig config_;
	std::shared_ptr<PortMaster::ITransport> transport_;
	std::unique_ptr<PortMaster::ReliableChannel> reliableChannel_;

	// 状态管理
	bool isConnected_;
	bool isTransmitting_;
	DWORD transmissionStartTime_;

	// UI更新
	static const UINT_PTR TIMER_UPDATE_UI = 1;
	static const UINT_PTR TIMER_UPDATE_LOG = 2;

	// 初始化方法
	void InitializeControls();
	void LoadConfiguration();
	void SaveConfiguration();
	void SetupLogList();

	// 端口管理
	void RefreshPortList();
	void UpdatePortParameters();
	bool ConnectPort();
	void DisconnectPort();

	// 数据传输
	void SendData();
	void SendFile(const CString& filePath);
	void ReceiveData();
	void ClearReceiveArea();

	// UI更新
	void UpdateConnectionStatus();
	void UpdateProgress(DWORD current, DWORD total);
	void UpdateLogDisplay();
	void UpdateControlStates();

	// 事件处理
	void OnTransportConnected();
	void OnTransportDisconnected();
	void OnTransportError(const CString& error);
	void OnDataReceived(const PortMaster::BufferView& data);
	void OnTransmissionProgress(DWORD current, DWORD total);

	// 工具方法
	CString FormatDataForDisplay(const PortMaster::BufferView& data, bool hexMode);
	CString GetSelectedPortType();
	CString GetSelectedPortName();
	DWORD GetBaudRate();
	void ShowMessage(const CString& message, UINT type = MB_ICONINFORMATION);
};