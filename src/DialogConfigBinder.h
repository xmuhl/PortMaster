#pragma once
#pragma execution_character_set("utf-8")

#include "../Common/ConfigStore.h"
#include <functional>
#include <string>

// 前置声明
class CDialog;
class CWnd;

/**
 * @brief 对话框配置绑定器
 *
 * 职责：负责 UI 控件与 ConfigStore 间的数据绑定与验证；生成概要配置结构体供外部使用
 * 位置：src/ 目录（UI辅助服务）
 *
 * 功能说明：
 * - 双向绑定对话框控件状态与配置数据（ConfigStore）
 * - LoadFromUI(): 从UI控件读取配置并更新到ConfigStore
 * - SaveToUI(): 从ConfigStore读取配置并更新到UI控件
 * - 自动化UI↔配置同步，减少重复代码
 * - 配置变更通知机制（回调函数）
 * - 配置数据验证与错误处理
 *
 * 线程安全性：
 * - 所有方法应在UI线程调用（涉及MFC控件访问）
 * - ConfigStore访问通过引用传递，外部确保线程安全
 *
 * 使用示例：
 * @code
 * // 在对话框初始化时创建绑定器
 * DialogConfigBinder binder(this, m_configStore);
 *
 * // 设置配置变更回调
 * binder.SetConfigChangedCallback([this]() {
 *     // 配置变更后的处理逻辑
 *     UpdateUIState();
 * });
 *
 * // 从ConfigStore加载配置到UI控件
 * binder.LoadToUI();
 *
 * // 从UI控件读取配置并保存到ConfigStore
 * if (binder.SaveFromUI()) {
 *     // 保存成功
 * }
 *
 * // 获取特定配置项
 * SerialConfig serialConfig = binder.GetSerialConfig();
 * @endcode
 */
class DialogConfigBinder
{
public:
	/**
	 * @brief 构造函数
	 * @param dialog 对话框指针（用于访问UI控件）
	 * @param configStore ConfigStore引用（配置存储管理器）
	 *
	 * 说明：
	 * - dialog不能为nullptr
	 * - configStore引用必须在对象生命周期内有效
	 */
	DialogConfigBinder(CDialog* dialog, ConfigStore& configStore);
	~DialogConfigBinder();

	// 禁止拷贝和赋值
	DialogConfigBinder(const DialogConfigBinder&) = delete;
	DialogConfigBinder& operator=(const DialogConfigBinder&) = delete;

	// ========== 双向绑定接口 ==========

	/**
	 * @brief 从ConfigStore加载配置到UI控件
	 * @return 加载是否成功
	 *
	 * 说明：
	 * - 读取ConfigStore中的SerialConfig、UIConfig、ProtocolConfig
	 * - 将配置值设置到对应的UI控件（ComboBox、Edit、RadioButton等）
	 * - 包含串口参数、传输模式、十六进制显示、窗口位置等
	 */
	bool LoadToUI();

	/**
	 * @brief 从UI控件读取配置并保存到ConfigStore
	 * @return 保存是否成功
	 *
	 * 说明：
	 * - 从UI控件读取当前值（端口名、波特率、数据位等）
	 * - 验证配置数据有效性（波特率范围、停止位枚举等）
	 * - 更新到ConfigStore并触发配置变更回调
	 */
	bool SaveFromUI();

	// ========== 配置访问接口 ==========

	/**
	 * @brief 获取串口配置
	 * @return 串口配置结构
	 *
	 * 说明：
	 * - 从当前ConfigStore读取SerialConfig
	 * - 包含端口名、波特率、数据位、校验位、停止位、流控制、超时等
	 */
	SerialConfig GetSerialConfig() const;

	/**
	 * @brief 获取UI配置
	 * @return UI配置结构
	 *
	 * 说明：
	 * - 从当前ConfigStore读取UIConfig
	 * - 包含十六进制显示、窗口位置/尺寸等
	 */
	UIConfig GetUIConfig() const;

	/**
	 * @brief 获取协议配置
	 * @return 协议配置结构
	 *
	 * 说明：
	 * - 从当前ConfigStore读取ProtocolConfig
	 * - 包含可靠传输窗口大小、最大重试次数等
	 */
	ProtocolConfig GetProtocolConfig() const;

	/**
	 * @brief 获取完整配置
	 * @return 完整配置结构
	 */
	PortMasterConfig GetConfig() const;

	/**
	 * @brief 设置串口配置
	 * @param config 串口配置结构
	 * @return 设置是否成功
	 *
	 * 说明：
	 * - 更新ConfigStore中的SerialConfig
	 * - 不自动更新UI控件（需手动调用LoadToUI）
	 */
	bool SetSerialConfig(const SerialConfig& config);

	/**
	 * @brief 设置UI配置
	 * @param config UI配置结构
	 * @return 设置是否成功
	 */
	bool SetUIConfig(const UIConfig& config);

	/**
	 * @brief 设置协议配置
	 * @param config 协议配置结构
	 * @return 设置是否成功
	 */
	bool SetProtocolConfig(const ProtocolConfig& config);

	// ========== 单项配置绑定（高频操作优化）==========

	/**
	 * @brief 绑定端口名到UI
	 * @param portName 端口名（如"COM1"）
	 */
	void BindPortName(const std::string& portName);

	/**
	 * @brief 从UI读取端口名
	 * @return 端口名字符串
	 */
	std::string ReadPortName() const;

	/**
	 * @brief 绑定波特率到UI
	 * @param baudRate 波特率（如9600, 115200）
	 */
	void BindBaudRate(int baudRate);

	/**
	 * @brief 从UI读取波特率
	 * @return 波特率值
	 */
	int ReadBaudRate() const;

	/**
	 * @brief 绑定传输模式到UI
	 * @param useReliableMode 是否使用可靠模式
	 */
	void BindTransmissionMode(bool useReliableMode);

	/**
	 * @brief 从UI读取传输模式
	 * @return 是否使用可靠模式
	 */
	bool ReadTransmissionMode() const;

	/**
	 * @brief 绑定十六进制显示模式到UI
	 * @param hexDisplay 是否十六进制显示
	 */
	void BindHexDisplayMode(bool hexDisplay);

	/**
	 * @brief 从UI读取十六进制显示模式
	 * @return 是否十六进制显示
	 */
	bool ReadHexDisplayMode() const;

	// ========== 配置验证接口 ==========

	/**
	 * @brief 验证串口配置有效性
	 * @param config 待验证的串口配置
	 * @param errorMessage 输出错误消息
	 * @return 验证是否通过
	 *
	 * 说明：
	 * - 检查端口名非空
	 * - 检查波特率在有效范围（300~921600）
	 * - 检查数据位有效值（5/6/7/8）
	 * - 检查停止位枚举值有效
	 * - 检查校验位枚举值有效
	 */
	bool ValidateSerialConfig(const SerialConfig& config, std::string& errorMessage) const;

	/**
	 * @brief 验证UI配置有效性
	 * @param config 待验证的UI配置
	 * @param errorMessage 输出错误消息
	 * @return 验证是否通过
	 */
	bool ValidateUIConfig(const UIConfig& config, std::string& errorMessage) const;

	// ========== 回调接口 ==========

	/**
	 * @brief 配置变更回调类型
	 */
	using ConfigChangedCallback = std::function<void()>;

	/**
	 * @brief 设置配置变更回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 当SaveFromUI成功后触发此回调
	 * - 回调在UI线程执行
	 * - 用于通知外部配置已更新，需要刷新相关状态
	 */
	void SetConfigChangedCallback(ConfigChangedCallback callback);

	/**
	 * @brief 错误回调类型
	 * @param errorMessage 错误消息
	 */
	using ErrorCallback = std::function<void(const std::string&)>;

	/**
	 * @brief 设置错误回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 当配置验证失败或读写异常时触发
	 * - 可用于显示错误提示框或记录日志
	 */
	void SetErrorCallback(ErrorCallback callback);

	// ========== 工具方法 ==========

	/**
	 * @brief 应用窗口位置配置
	 * @param windowX 窗口左上角X坐标
	 * @param windowY 窗口左上角Y坐标
	 * @param windowWidth 窗口宽度
	 * @param windowHeight 窗口高度
	 *
	 * 说明：
	 * - 调用SetWindowPos设置窗口位置和尺寸
	 * - 仅当参数有效时（宽高>0，坐标!=CW_USEDEFAULT）才应用
	 */
	void ApplyWindowPosition(int windowX, int windowY, int windowWidth, int windowHeight);

	/**
	 * @brief 捕获当前窗口位置
	 * @param windowX 输出窗口左上角X坐标
	 * @param windowY 输出窗口左上角Y坐标
	 * @param windowWidth 输出窗口宽度
	 * @param windowHeight 输出窗口高度
	 *
	 * 说明：
	 * - 调用GetWindowRect获取当前窗口位置和尺寸
	 * - 用于SaveFromUI时保存窗口状态
	 */
	void CaptureWindowPosition(int& windowX, int& windowY, int& windowWidth, int& windowHeight) const;

private:
	// ========== 内部方法 ==========

	/**
	 * @brief 从ConfigStore加载串口配置到UI
	 * @param config 串口配置
	 */
	void LoadSerialConfigToUI(const SerialConfig& config);

	/**
	 * @brief 从UI读取串口配置
	 * @param config 输出串口配置
	 * @return 读取是否成功
	 */
	bool ReadSerialConfigFromUI(SerialConfig& config);

	/**
	 * @brief 从ConfigStore加载UI配置到对话框控件
	 * @param config UI配置
	 */
	void LoadUIConfigToDialog(const UIConfig& config);

	/**
	 * @brief 从对话框控件读取UI配置
	 * @param config 输出UI配置
	 * @return 读取是否成功
	 */
	bool ReadUIConfigFromDialog(UIConfig& config);

	/**
	 * @brief 从ConfigStore加载协议配置到UI
	 * @param config 协议配置
	 */
	void LoadProtocolConfigToUI(const ProtocolConfig& config);

	/**
	 * @brief 从UI读取协议配置
	 * @param config 输出协议配置
	 * @return 读取是否成功
	 */
	bool ReadProtocolConfigFromUI(ProtocolConfig& config);

	/**
	 * @brief 校验位枚举转字符串
	 * @param parity 校验位枚举值（NOPARITY/ODDPARITY/EVENPARITY等）
	 * @return 校验位字符串（"None"/"Odd"/"Even"等）
	 */
	std::wstring ParityToString(BYTE parity) const;

	/**
	 * @brief 字符串转校验位枚举
	 * @param parityText 校验位字符串
	 * @return 校验位枚举值
	 */
	BYTE StringToParity(const std::wstring& parityText) const;

	/**
	 * @brief 停止位枚举转字符串
	 * @param stopBits 停止位枚举值（ONESTOPBIT/ONE5STOPBITS/TWOSTOPBITS）
	 * @return 停止位字符串（"1"/"1.5"/"2"）
	 */
	std::wstring StopBitsToString(BYTE stopBits) const;

	/**
	 * @brief 字符串转停止位枚举
	 * @param stopBitsText 停止位字符串
	 * @return 停止位枚举值
	 */
	BYTE StringToStopBits(const std::wstring& stopBitsText) const;

	/**
	 * @brief 获取对话框控件窗口
	 * @param controlID 控件资源ID
	 * @return 控件窗口指针，失败返回nullptr
	 */
	CWnd* GetControl(int controlID) const;

	/**
	 * @brief 设置对话框控件文本
	 * @param controlID 控件资源ID
	 * @param text 文本内容
	 * @return 设置是否成功
	 */
	bool SetControlText(int controlID, const std::wstring& text);

	/**
	 * @brief 获取对话框控件文本
	 * @param controlID 控件资源ID
	 * @return 文本内容，失败返回空字符串
	 */
	std::wstring GetControlText(int controlID) const;

	/**
	 * @brief 触发配置变更回调
	 */
	void NotifyConfigChanged();

	/**
	 * @brief 触发错误回调
	 * @param errorMessage 错误消息
	 */
	void NotifyError(const std::string& errorMessage);

private:
	// ========== 成员变量 ==========

	CDialog* m_dialog;                      // 对话框指针（用于访问UI控件）
	ConfigStore& m_configStore;             // 配置存储管理器引用

	// 回调函数
	ConfigChangedCallback m_configChangedCallback;  // 配置变更回调
	ErrorCallback m_errorCallback;                  // 错误回调
};
