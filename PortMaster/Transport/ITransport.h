#pragma once

#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "../Common/CommonTypes.h"

namespace PortMaster {

    /// <summary>
    /// 传输层统一接口定义
    /// 提供对串口、并口、USB打印、网络打印、回路测试的统一抽象
    /// </summary>
    class ITransport {
    public:
        virtual ~ITransport() = default;

        /// <summary>
        /// 打开端口连接
        /// </summary>
        /// <param name="cfg">端口配置参数</param>
        /// <param name="ctx">传输上下文，用于返回状态信息</param>
        /// <returns>成功返回true，失败返回false</returns>
        virtual bool Open(const PortConfig& cfg, TransportContext& ctx) = 0;

        /// <summary>
        /// 关闭端口连接
        /// </summary>
        virtual void Close() = 0;

        /// <summary>
        /// 发送数据
        /// </summary>
        /// <param name="data">要发送的数据</param>
        /// <returns>成功返回true，失败返回false</returns>
        virtual bool Send(const BufferView& data) = 0;

        /// <summary>
        /// 接收数据
        /// </summary>
        /// <param name="out">输出缓冲区</param>
        /// <param name="timeout">超时时间（毫秒）</param>
        /// <returns>成功返回true，失败返回false</returns>
        virtual bool Receive(Buffer& out, DWORD timeout) = 0;

        /// <summary>
        /// 查询传输状态
        /// </summary>
        /// <returns>当前传输状态</returns>
        virtual TransportStatus QueryStatus() = 0;

    protected:
        // 受保护的构造函数，防止直接实例化
        ITransport() = default;
    };

    /// <summary>
    /// 传输工厂类
    /// 用于创建不同类型的传输实例
    /// </summary>
    class TransportFactory {
    public:
        /// <summary>
        /// 创建串口传输实例
        /// </summary>
        /// <returns>串口传输指针</returns>
        static std::shared_ptr<ITransport> CreateSerial();

        /// <summary>
        /// 创建并口传输实例
        /// </summary>
        /// <returns>并口传输指针</returns>
        static std::shared_ptr<ITransport> CreateParallel();

        /// <summary>
        /// 创建USB打印传输实例
        /// </summary>
        /// <returns>USB打印传输指针</returns>
        static std::shared_ptr<ITransport> CreateUsbPrint();

        /// <summary>
        /// 创建网络打印传输实例
        /// </summary>
        /// <returns>网络打印传输指针</returns>
        static std::shared_ptr<ITransport> CreateNetworkPrint();

        /// <summary>
        /// 创建回路测试传输实例
        /// </summary>
        /// <returns>回路测试传输指针</returns>
        static std::shared_ptr<ITransport> CreateLoopback();
    };

} // namespace PortMaster