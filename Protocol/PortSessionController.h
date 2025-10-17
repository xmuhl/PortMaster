#pragma once
#pragma execution_character_set("utf-8")

#include "../Transport/ITransport.h"
#include "ReliableChannel.h"
#include "../Common/CommonTypes.h"
#include "../Common/ConfigStore.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <vector>

/**
 * @brief 端口会话控制器
 *
 * 职责：封装传输层连接管理、接收会话管理、ReliableChannel生命周期管理
 * 位置：Protocol/ 目录
 *
 * 功能说明：
 * - 根据配置创建不同类型的Transport（串口、并口、USB打印、网络打印、回路测试）
 * - 管理ReliableChannel的初始化和销毁
 * - 启动和停止接收线程
 * - 通过回调向UI层抛出数据接收和错误事件
 *
 * 线程安全性：
 * - 接收线程与主线程通过回调异步通信
 * - 内部使用atomic变量保证状态同步
 *
 * 使用示例：
 * @code
 * auto controller = std::make_unique<PortSessionController>();
 * controller->SetDataCallback([](const std::vector<uint8_t>& data) {
 *     // 处理接收数据
 * });
 * controller->SetErrorCallback([](const std::string& error) {
 *     // 处理错误
 * });
 *
 * TransportConfig config;
 * config.portType = PortType::PORT_TYPE_SERIAL;
 * config.portName = "COM1";
 * if (controller->Connect(config, true)) {  // true表示使用可靠模式
 *     controller->StartReceiveSession();
 * }
 * @endcode
 */
class PortSessionController
{
public:
	PortSessionController();
	~PortSessionController();

	// 禁止拷贝和赋值
	PortSessionController(const PortSessionController&) = delete;
	PortSessionController& operator=(const PortSessionController&) = delete;

	// ========== 连接管理 ==========

	/**
	 * @brief 建立传输连接
	 * @param config 传输配置（包含端口类型、端口名称、波特率等）
	 * @param useReliableMode 是否启用可靠传输模式
	 * @return 连接是否成功
	 *
	 * 说明：
	 * - 根据config.portType自动选择Transport实现类
	 * - 如果useReliableMode为true，自动创建ReliableChannel
	 * - 内部会调用ConfigureReliableLogging()设置日志级别
	 */
	bool Connect(const TransportConfig& config, bool useReliableMode);

	/**
	 * @brief 断开传输连接
	 *
	 * 说明：
	 * - 先停止接收会话
	 * - 销毁ReliableChannel（如果有）
	 * - 关闭并销毁Transport
	 */
	void Disconnect();

	/**
	 * @brief 查询连接状态
	 * @return 是否已连接
	 */
	bool IsConnected() const;

	// ========== 接收会话管理 ==========

	/**
	 * @brief 启动接收会话（接收线程）
	 *
	 * 说明：
	 * - 创建后台接收线程
	 * - 根据是否使用可靠模式选择不同的接收策略
	 * - 接收到的数据通过DataCallback回调通知
	 */
	void StartReceiveSession();

	/**
	 * @brief 停止接收会话（接收线程）
	 *
	 * 说明：
	 * - 设置停止标志
	 * - 等待接收线程退出
	 */
	void StopReceiveSession();

	// ========== ReliableChannel管理 ==========

	/**
	 * @brief 设置可靠传输配置
	 * @param config 可靠传输配置
	 *
	 * 说明：
	 * - 在Connect之前调用，用于设置超时等参数
	 * - 配置会在创建ReliableChannel时生效
	 */
	void SetReliableConfig(const ReliableConfig& config);

	/**
	 * @brief 获取ReliableChannel实例
	 * @return ReliableChannel共享指针，如果未启用可靠模式返回nullptr
	 *
	 * 说明：
	 * - 用于外部直接访问ReliableChannel进行文件传输等操作
	 */
	std::shared_ptr<ReliableChannel> GetReliableChannel();

	/**
	 * @brief 配置ReliableChannel的日志输出级别
	 * @param verbose 是否启用详细日志
	 *
	 * 说明：
	 * - 根据应用配置动态调整可靠协议的日志输出
	 * - 避免低级别日志造成的日志文件膨胀
	 */
	void ConfigureReliableLogging(bool verbose);

	// ========== 回调接口 ==========

	/**
	 * @brief 数据接收回调类型
	 * @param data 接收到的数据
	 */
	using DataCallback = std::function<void(const std::vector<uint8_t>& data)>;

	/**
	 * @brief 错误回调类型
	 * @param error 错误描述
	 */
	using ErrorCallback = std::function<void(const std::string& error)>;

	/**
	 * @brief 设置数据接收回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 接收线程接收到数据后会调用此回调
	 * - 回调在接收线程中执行，需注意线程安全
	 */
	void SetDataCallback(DataCallback callback);

	/**
	 * @brief 设置错误回调
	 * @param callback 回调函数
	 *
	 * 说明：
	 * - 传输层或协议层发生错误时调用
	 * - 回调在接收线程中执行，需注意线程安全
	 */
	void SetErrorCallback(ErrorCallback callback);

	// ========== 获取底层Transport ==========

	/**
	 * @brief 获取底层Transport实例
	 * @return Transport共享指针，如果未连接返回nullptr
	 *
	 * 说明：
	 * - 用于外部直接访问Transport进行原始读写操作
	 * - 不建议在使用ReliableChannel的情况下直接操作Transport
	 */
	std::shared_ptr<ITransport> GetTransport();

private:
	// ========== 内部方法 ==========

	/**
	 * @brief 创建指定类型的Transport实例
	 * @param config 传输配置
	 * @return Transport实例，失败返回nullptr
	 */
	std::shared_ptr<ITransport> CreateTransportByType(const TransportConfig& config);

	/**
	 * @brief 接收线程主函数
	 *
	 * 说明：
	 * - 根据是否使用可靠模式选择不同的接收策略
	 * - 可靠模式：调用ReliableChannel::Receive()
	 * - 直接模式：调用Transport::Read()
	 */
	void ReceiveThreadProc();

	/**
	 * @brief 处理接收到的数据
	 * @param data 接收到的数据
	 *
	 * 说明：
	 * - 调用DataCallback通知上层
	 */
	void OnDataReceived(const std::vector<uint8_t>& data);

	/**
	 * @brief 处理传输错误
	 * @param error 错误描述
	 *
	 * 说明：
	 * - 调用ErrorCallback通知上层
	 */
	void OnError(const std::string& error);

private:
	// ========== 成员变量 ==========

	// Transport层
	std::shared_ptr<ITransport> m_transport;          // 底层传输对象
	std::unique_ptr<ReliableChannel> m_reliableChannel; // 可靠传输通道

	// 接收线程
	std::thread m_receiveThread;                      // 接收线程
	std::atomic<bool> m_isConnected;                  // 连接状态
	std::atomic<bool> m_useReliableMode;              // 是否使用可靠模式

	// 回调函数
	DataCallback m_dataCallback;                      // 数据接收回调
	ErrorCallback m_errorCallback;                    // 错误回调

	// 配置
	ReliableConfig m_reliableConfig;                  // 可靠传输配置
};
