// PortConfigPresenter.h - 端口配置呈现器
// 职责：端口类型枚举、端口参数管理、UI同步、事件响应
#pragma once

#include "afxwin.h"
#include <vector>
#include <string>
#include <functional>

// 端口类型枚举
enum class PortTypeIndex
{
	Serial = 0,        // 串口
	Parallel = 1,      // 并口
	UsbPrint = 2,      // USB打印
	NetworkPrint = 3,  // 网络打印
	Loopback = 4       // 回路测试
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

private:
	PortConfigControlRefs m_controls;                        // 控件引用集合

	// 事件回调
	std::function<void()> m_portTypeChangedCallback;         // 端口类型变更回调
	std::function<void()> m_portChangedCallback;             // 端口号变更回调

	// 内部辅助方法
	void ValidateControlRefs() const;                        // 验证控件引用有效性
	bool IsControlValid(CWnd* control) const;                // 检查单个控件是否有效
	void PopulateComboBox(CComboBox* combo, const std::vector<std::string>& items); // 填充下拉框
	void PopulateComboBox(CComboBox* combo, const std::vector<CString>& items);     // 填充下拉框（CString版本）
	int FindComboBoxItem(CComboBox* combo, const CString& text) const; // 查找下拉框项目索引
};
