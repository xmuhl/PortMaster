// PortConfigPresenter.h - 端口配置呈现器
// 职责：端口类型枚举、端口参数管理、UI同步、事件响应
#pragma once

#include "afxwin.h"
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <future>

// 前向声明
class NetworkPrinterConfigDialog;

// 端口类型枚举
enum class PortTypeIndex
{
	Serial = 0,        // 串口
	Parallel = 1,      // 并口
	UsbPrint = 2,      // USB打印
	NetworkPrint = 3,  // 网络打印
	Loopback = 4       // 回路测试
};

// 端口扫描进度信息
struct PortScanProgress
{
	int currentPort;      // 当前扫描的端口索引
	int totalPorts;       // 总端口数
	std::string status;   // 状态信息
};

// 端口扫描完成信息
struct PortScanResult
{
	bool success;         // 扫描是否成功
	std::string error;    // 错误信息（如果有）
};

// 端口扫描进度回调类型
using PortScanProgressCallback = std::function<void(const PortScanProgress&)>;

// 端口扫描完成回调类型
using PortScanCompleteCallback = std::function<void(const PortScanResult&)>;

// 端口扫描器类 - 实现异步端口扫描
class PortScanner
{
public:
	PortScanner();
	~PortScanner();

	// 开始异步扫描
	void StartScan(PortTypeIndex portType,
		PortScanProgressCallback progressCallback,
		PortScanCompleteCallback completeCallback);

	// 停止扫描
	void StopScan();

	// 检查是否正在扫描
	bool IsScanning() const;

private:
	// 扫描工作线程函数
	void ScanWorker(PortTypeIndex portType,
		PortScanProgressCallback progressCallback,
		PortScanCompleteCallback completeCallback);

	// 各端口类型的扫描实现
	void ScanSerialPorts(PortScanProgressCallback progressCallback);
	void ScanParallelPorts(PortScanProgressCallback progressCallback);
	void ScanUsbPorts(PortScanProgressCallback progressCallback);
	void ScanNetworkPorts(PortScanProgressCallback progressCallback);

	std::atomic<bool> m_shouldStop;   // 停止标志
	std::atomic<bool> m_isScanning;   // 扫描标志
	std::mutex m_mutex;               // 互斥锁

	// 扫描结果存储
	std::vector<std::string> m_scanResults;
};

// 端口配置UI控件引用
struct PortConfigControlRefs
{
	CComboBox* comboPortType;       // 端口类型下拉框
	CComboBox* comboPort;           // 端口号下拉框
	CComboBox* comboBaudRate;       // 波特率下拉框
	CComboBox* comboDataBits;       // 数据位下拉框
	CComboBox* comboParity;         // 校验位下拉框
	CComboBox* comboStopBits;       // 停止位下拉框
	CComboBox* comboFlowControl;    // 流控下拉框
	CWnd* parentDialog;             // 父对话框窗口指针
};

// 端口配置呈现器类
class PortConfigPresenter
{
public:
	PortConfigPresenter();
	~PortConfigPresenter();

	// 初始化：绑定控件引用
	void Initialize(const PortConfigControlRefs& controlRefs);

	// 端口类型管理
	void InitializePortTypeCombo();                          // 初始化端口类型下拉框
	void OnPortTypeChanged();                                // 端口类型切换事件处理
	PortTypeIndex GetSelectedPortType() const;               // 获取当前选择的端口类型
	void SetSelectedPortType(PortTypeIndex type);            // 设置选择的端口类型

	// 端口参数更新
	void UpdatePortParameters();                             // 更新端口参数（根据端口类型）
	void UpdateSerialPortParameters();                       // 更新串口参数
	void UpdateParallelPortParameters();                     // 更新并口参数
	void UpdateUsbPrintPortParameters();                     // 更新USB打印端口参数
	void UpdateNetworkPrintPortParameters();                 // 更新网络打印端口参数
	void UpdateLoopbackPortParameters();                     // 更新回路测试参数

	// 串口参数控件显示/隐藏
	void ShowSerialParameters(bool show);                    // 显示/隐藏串口参数控件

	// 端口枚举
	std::vector<std::string> EnumerateSerialPorts();         // 枚举串口
	std::vector<std::string> EnumerateParallelPorts();       // 枚举并口
	std::vector<std::string> EnumerateUsbPorts();            // 枚举USB端口

	// 当前配置获取
	std::string GetSelectedPort() const;                     // 获取选择的端口名称
	int GetSelectedBaudRate() const;                         // 获取选择的波特率
	int GetSelectedDataBits() const;                         // 获取选择的数据位
	std::string GetSelectedParity() const;                   // 获取选择的校验位
	int GetSelectedStopBits() const;                         // 获取选择的停止位
	std::string GetSelectedFlowControl() const;              // 获取选择的流控

	// 配置设置
	void SetPortSelection(const std::string& port);          // 设置端口选择
	void SetBaudRate(int baudRate);                          // 设置波特率
	void SetDataBits(int dataBits);                          // 设置数据位
	void SetParity(const std::string& parity);               // 设置校验位
	void SetStopBits(int stopBits);                          // 设置停止位
	void SetFlowControl(const std::string& flowControl);     // 设置流控

	// 事件回调注册
	void SetPortTypeChangedCallback(std::function<void()> callback);  // 端口类型变更回调
	void SetPortChangedCallback(std::function<void()> callback);      // 端口号变更回调

	// 网络打印机配置
	void OnNetworkPrinterConfigSelected();                             // 处理网络打印机配置选项选择

	// 异步端口参数更新
	void UpdatePortParametersAsync();                                  // 异步更新端口参数（避免阻塞UI）

	// 智能默认选择
	void SelectDefaultPort();                                          // 根据设备状态智能选择默认端口

	// 轻量级端口状态检测
	bool QuickCheckPortStatus();                                       // 快速检测当前选中的端口状态（500ms超时）

private:
	PortConfigControlRefs m_controls;                        // 控件引用集合

	// 事件回调
	std::function<void()> m_portTypeChangedCallback;         // 端口类型变更回调
	std::function<void()> m_portChangedCallback;             // 端口号变更回调

	// 端口扫描器
	std::unique_ptr<PortScanner> m_portScanner;              // 异步端口扫描器

	// 内部辅助方法
	void ValidateControlRefs() const;                        // 验证控件引用有效性
	bool IsControlValid(CWnd* control) const;                // 检查单个控件是否有效
	void PopulateComboBox(CComboBox* combo, const std::vector<std::string>& items); // 填充下拉框
	void PopulateComboBox(CComboBox* combo, const std::vector<CString>& items);     // 填充下拉框（CString版本）
	int FindComboBoxItem(CComboBox* combo, const CString& text) const; // 查找下拉框项目索引
};
