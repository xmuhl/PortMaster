#pragma once

#include "afxdialogex.h"
#include "Transport/ITransport.h"

// CPortConfigDialog 对话框
class CPortConfigDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CPortConfigDialog)

public:
	CPortConfigDialog(const std::string& transportType, const TransportConfig& config, CWnd* pParent = nullptr);
	virtual ~CPortConfigDialog();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PORT_CONFIG_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnCbnSelchangeTransportType();

private:
	std::string m_transportType;
	TransportConfig m_config;
	
	// 控件变量
	CComboBox m_ctrlTransportType;
	CEdit m_ctrlBaudRate;
	CEdit m_ctrlDataBits;
	CComboBox m_ctrlParity;
	CComboBox m_ctrlStopBits;
	CEdit m_ctrlIpAddress;
	CEdit m_ctrlPort;
	CButton m_ctrlIsServer;
	CEdit m_ctrlConnectTimeout;
	CEdit m_ctrlReadTimeout;
	CEdit m_ctrlWriteTimeout;
	CEdit m_ctrlRxBufferSize;
	CEdit m_ctrlTxBufferSize;
	
	// 组框控件
	CStatic m_ctrlSerialGroup;
	CStatic m_ctrlNetworkGroup;
	CStatic m_ctrlTimeoutGroup;
	CStatic m_ctrlBufferGroup;
	
	void UpdateControlVisibility();
	void LoadConfigToControls();
	void SaveControlsToConfig();
	
public:
	TransportConfig GetConfig() const { return m_config; }
	std::string GetTransportType() const { return m_transportType; }
};