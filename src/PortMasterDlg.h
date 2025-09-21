// PortMasterDlg.h : 头文件
#pragma once
#include "afxwin.h"

// CPortMasterDlg 对话框
class CPortMasterDlg : public CDialogEx
{
public:
CPortMasterDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
enum { IDD = IDD_PORTMASTER_DIALOG };
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
	
	// 端口类型切换处理
	void UpdatePortParameters();
	
	// 数据处理函数
	void SendData();
	void ReceiveData();
	void UpdateDataDisplay();
	void LoadDataFromFile(const CString& filePath);
	void SaveDataToFile(const CString& filePath, const CString& data);
	
	// 十六进制转换函数
	CString StringToHex(const CString& str);
	CString HexToString(const CString& hex);
	
	DECLARE_MESSAGE_MAP()

public:
	// 基本控件
	CButton m_btnConnect;
	CButton m_btnDisconnect;
	CButton m_btnSend;
	CButton m_btnStop;
	CButton m_btnFile;
	CButton m_btnExportLog;
	
	// 新增操作按钮
	CButton m_btnClearAll;
	CButton m_btnClearReceive;
	CButton m_btnCopyAll;
	CButton m_btnSaveAll;
	
	// 编辑框控件
	CEdit m_editSendData;
	CEdit m_editReceiveData;
	CEdit m_editLogDetail;
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
	CStatic m_staticCheck;
	CStatic m_staticLoopCount;
	CStatic m_staticSendSource;

// 消息处理函数声明
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonDisconnect();
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonFile();
	afx_msg void OnBnClickedButtonExportLog();
	
	// 控件事件处理
	afx_msg void OnCbnSelchangeComboPortType();
	afx_msg void OnBnClickedCheckHex();
	afx_msg void OnBnClickedRadioReliable();
	afx_msg void OnBnClickedRadioDirect();
	afx_msg void OnBnClickedCheckOccupy();
};