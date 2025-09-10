#pragma once

#include "afxdialogex.h"
#include "resource.h"
#include "Transport/ITransport.h"

// 测试向导对话框
class CTestWizardDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CTestWizardDialog)

public:
	CTestWizardDialog(CWnd* pParent = nullptr);
	virtual ~CTestWizardDialog();

	enum { IDD = IDD_TEST_WIZARD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedStartTest();
	afx_msg void OnBnClickedStopTest();
	afx_msg void OnCbnSelchangeTestType();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	
private:
	// 控件变量
	CComboBox m_ctrlTestType;
	CComboBox m_ctrlTransportType;
	CEdit m_ctrlTestConfig;
	CEdit m_ctrlTestResults;
	CProgressCtrl m_ctrlTestProgress;
	CButton m_ctrlStartBtn;
	CButton m_ctrlStopBtn;
	CStatic m_ctrlStatusText;

	// 测试状态
	bool m_bTesting;
	int m_nCurrentTest;
	int m_nTotalTests;
	UINT_PTR m_nTimer;
	
	// 测试类型枚举 (仅保留核心测试类型)
	enum TestType
	{
		TEST_LOOPBACK = 0,      // 回环测试
		TEST_PROTOCOL_RELIABLE  // 可靠协议测试
	};

	// 测试结果结构
	struct TestResult
	{
		CString testName;
		bool passed;
		CString details;
		DWORD duration; // 毫秒
	};

	std::vector<TestResult> m_testResults;

	// 初始化函数
	void InitializeControls();
	void UpdateControlStates();
	void UpdateTestProgress();
	
	// 测试执行函数
	void StartTesting();
	void StopTesting();
	void ExecuteNextTest();
	void CompleteTest();
	
	// 具体测试方法 (仅保留核心测试)
	bool ExecuteLoopbackTest();
	bool ExecuteProtocolReliableTest();
	
	// 辅助方法
	void AppendTestResult(const CString& message);
	void LogTestResult(const CString& testName, bool passed, const CString& details, DWORD duration);
	CString FormatTestSummary();
	std::vector<uint8_t> GenerateTestData(size_t size, bool pattern = true);
	bool CompareData(const std::vector<uint8_t>& sent, const std::vector<uint8_t>& received);
	
	// 扩展的回环测试方法
	bool ExecuteLoopbackBasicTest(class LoopbackTransport& transport);
	bool ExecuteLoopbackLargeDataTest(class LoopbackTransport& transport);
	bool ExecuteLoopbackContinuousTest(class LoopbackTransport& transport);
	bool ExecuteLoopbackErrorTest(class LoopbackTransport& transport);
	bool ExecuteLoopbackPerformanceTest(class LoopbackTransport& transport);
	
	// 可靠协议测试方法
	bool ExecuteReliableChannelBasicTest();
	bool ExecuteReliableChannelCrcTest();
	bool ExecuteReliableChannelTimeoutTest();
	bool ExecuteReliableChannelFileTest();
	bool ExecuteReliableChannelConcurrentTest();
	
	// 测试工具方法
	std::string CreateTemporaryTestFile(const std::string& content);
	bool ValidateFileContent(const std::string& filePath, const std::string& expectedContent);
	double MeasureThroughput(size_t dataSize, DWORD timeMs);
	bool WaitForCondition(std::function<bool()> condition, int timeoutMs = 5000);
	
	// 错误处理和验证方法
	bool VerifyControlsExist();
	void LogControlError(const CString& controlName, UINT controlId);
};