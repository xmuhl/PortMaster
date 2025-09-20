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
DECLARE_MESSAGE_MAP()

public:
CButton m_btnConnect;
	CButton m_btnDisconnect;
	CButton m_btnSend;
	CButton m_btnClear;
	CButton m_btnSave;
	CButton m_btnSettings;
	CButton m_btnAbout;
	CButton m_btnPause;
	CButton m_btnContinue;
	CButton m_btnStop;
	CButton m_btnCopy;
	CButton m_btnFile;
	CButton m_btnExportLog;
	CEdit m_editSendData;
	CEdit m_editReceiveData;
	CComboBox m_comboPortType;
	CComboBox m_comboPort;
	CComboBox m_comboBaudRate;
	CButton m_radioReliable;
	CButton m_radioDirect;
	CButton m_checkHex;
	CProgressCtrl m_progress;

afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonDisconnect();
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedButtonClear();
	afx_msg void OnBnClickedButtonSave();
	afx_msg void OnBnClickedButtonSettings();
	afx_msg void OnBnClickedButtonAbout();
	afx_msg void OnBnClickedButtonPause();
	afx_msg void OnBnClickedButtonContinue();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonCopy();
	afx_msg void OnBnClickedButtonFile();
	afx_msg void OnBnClickedButtonExportLog();
	afx_msg void OnCbnSelchangeComboPortType();
	afx_msg void OnBnClickedCheckHex();
	afx_msg void OnBnClickedRadioReliable();
	afx_msg void OnBnClickedRadioDirect();
};