#pragma execution_character_set("utf-8")
#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <afxwin.h>  // For CString

// 前置声明
class ITransport;
class ReliableChannel;
struct TransportConfig;
class CPortMasterDlg;

/**
 * @brief 连接管理器
 * SOLID-S: 单一职责 - 专门负责传输连接的建立和管理
 * SOLID-O: 开放封闭原则 - 易于扩展新的连接类型和协议
 */
class ConnectionManager
{
public:
    /**
     * @brief 构造函数
     * @param dialog 主对话框指针，用于UI更新和回调
     */
    explicit ConnectionManager(CPortMasterDlg* dialog);

    /**
     * @brief 析构函数
     */
    virtual ~ConnectionManager() = default;

    /**
     * @brief 建立连接
     * SOLID-S: 单一职责 - 专门处理连接建立逻辑
     * @param transportIndex 传输类型索引
     * @param config 传输配置参数
     * @return 连接成功返回true，失败返回false
     */
    bool EstablishConnection(int transportIndex, const TransportConfig& config);

    /**
     * @brief 断开连接
     * SOLID-S: 单一职责 - 专门处理断开连接逻辑
     * @return 断开成功返回true，失败返回false
     */
    bool DisconnectTransport();

    /**
     * @brief 获取当前传输对象
     * @return 当前传输对象的智能指针
     */
    std::shared_ptr<ITransport> GetCurrentTransport() const { return m_transport; }

    /**
     * @brief 获取当前可靠通道
     * @return 当前可靠通道的智能指针
     */
    std::shared_ptr<ReliableChannel> GetReliableChannel() const { return m_reliableChannel; }

    /**
     * @brief 检查连接状态
     * @return 已连接返回true，未连接返回false
     */
    bool IsConnected() const { return m_bConnected; }

    /**
     * @brief 设置可靠模式状态
     * @param reliable 是否启用可靠模式
     */
    void SetReliableMode(bool reliable) { m_bReliableMode = reliable; }

    /**
     * @brief 获取可靠模式状态
     * @return 是否启用可靠模式
     */
    bool IsReliableMode() const { return m_bReliableMode; }

private:
    /**
     * @brief 创建传输对象
     * SOLID-O: 开放封闭原则 - 使用工厂模式支持新传输类型
     * @param transportIndex 传输类型索引
     * @param config 传输配置
     * @return 创建的传输对象智能指针
     */
    std::shared_ptr<ITransport> CreateTransportFromIndex(int transportIndex, const TransportConfig& config);

    /**
     * @brief 配置可靠通道参数
     * SOLID-S: 单一职责 - 专门处理可靠通道配置
     * @param transport 传输对象
     */
    void ConfigureReliableChannel(std::shared_ptr<ITransport> transport);

    /**
     * @brief 设置传输回调函数
     * SOLID-S: 单一职责 - 专门处理回调配置
     */
    void SetupTransportCallbacks();

    /**
     * @brief 更新连接状态显示
     * @param connected 是否已连接
     * @param transportType 传输类型
     * @param endpoint 连接端点信息
     */
    void UpdateConnectionDisplay(bool connected, const std::string& transportType, const std::string& endpoint);

    /**
     * @brief 获取网络连接信息
     * @param transportType 传输类型
     * @return 连接信息字符串
     */
    std::string GetNetworkConnectionInfo(const std::string& transportType);

    /**
     * @brief 获取连接端点信息
     * @param config 传输配置
     * @param transportType 传输类型
     * @return 端点信息字符串
     */
    std::string GetConnectionEndpoint(const TransportConfig& config, const std::string& transportType);

private:
    CPortMasterDlg* m_dialog;                           ///< 主对话框指针
    std::shared_ptr<ITransport> m_transport;           ///< 当前传输对象
    std::shared_ptr<ReliableChannel> m_reliableChannel; ///< 可靠通道对象
    bool m_bConnected;                                  ///< 连接状态
    bool m_bReliableMode;                              ///< 可靠模式状态
};