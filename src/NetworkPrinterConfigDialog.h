// NetworkPrinterConfigDialog.h - 网络打印机配置对话框
#pragma once
#pragma execution_character_set("utf-8")

#include "afxwin.h"
#include "..\Transport\NetworkPrintTransport.h"

// NetworkPrinterConfigDialog 对话框

class NetworkPrinterConfigDialog : public CDialogEx
{
	DECLARE_DYNAMIC(NetworkPrinterConfigDialog)

public:
	NetworkPrinterConfigDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~NetworkPrinterConfigDialog();

	// 对话框数据
	enum { IDD = 1051 };

	// 配置获取和设置
	void SetNetworkConfig(const NetworkPrintConfig& config);
	NetworkPrintConfig GetNetworkConfig() const;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	virtual BOOL OnInitDialog();

	// 控件事件
	afx_msg void OnBnClickedButtonTestConnection();
	afx_msg void OnEnChangeEditIpAddress();
	afx_msg void OnEnChangeEditPort();

	// 连接测试
	bool TestConnection(const NetworkPrintConfig& config, std::string& errorMessage);

	DECLARE_MESSAGE_MAP()

private:
	// 控件变量
	CEdit m_editIpAddress;
	CEdit m_editPort;
	CComboBox m_comboProtocol;
	CButton m_buttonTestConnection;

	// 当前配置
	NetworkPrintConfig m_currentConfig;

	// 验证输入
	bool ValidateInputs(std::string& errorMessage);

	// 解析IP地址
	bool IsValidIpAddress(const std::string& ip) const;

	// 解析端口号
	bool IsValidPort(int port) const;
};
