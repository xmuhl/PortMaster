#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "ITransport.h"

namespace PortMaster {

    /// <summary>
    /// 回路测试传输实现类
    /// 用于内部回路测试，发送的数据会被回送
    /// </summary>
    class LoopbackTransport : public ITransport {
    public:
        LoopbackTransport() : status_(TransportStatus::TRANSPORT_CLOSED) {}

        virtual ~LoopbackTransport() {
            Close();
        }

        bool Open(const PortConfig& cfg, TransportContext& ctx) override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_CLOSED) {
                ctx.lastError = ERROR_ALREADY_EXISTS;
                ctx.errorMessage = _T("回路端口已经打开");
                return false;
            }

            status_ = TransportStatus::TRANSPORT_OPENING;

            // 清空内部缓冲区
            while (!loopbackQueue_.empty()) {
                loopbackQueue_.pop();
            }

            config_ = cfg;
            status_ = TransportStatus::TRANSPORT_OPEN;
            ctx.lastError = 0;
            ctx.errorMessage = _T("回路测试端口打开成功");

            return true;
        }

        void Close() override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_CLOSED) {
                status_ = TransportStatus::TRANSPORT_CLOSING;

                // 清空队列
                while (!loopbackQueue_.empty()) {
                    loopbackQueue_.pop();
                }

                status_ = TransportStatus::TRANSPORT_CLOSED;
            }
        }

        bool Send(const BufferView& data) override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_OPEN || data.empty()) {
                return false;
            }

            // 将发送的数据放入回路队列
            Buffer buffer(data.size());
            memcpy(buffer.data(), data.data(), data.size());
            loopbackQueue_.push(std::move(buffer));

            // 模拟发送延迟
            if (config_.timeout > 0) {
                Sleep(1); // 最小延迟1ms
            }

            return true;
        }

        bool Receive(Buffer& out, DWORD timeout) override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_OPEN) {
                return false;
            }

            // 检查是否有回路数据
            if (loopbackQueue_.empty()) {
                // 模拟接收等待
                if (timeout > 0) {
                    mutex_.unlock();
                    Sleep(min(timeout, 10)); // 最多等待10ms
                    mutex_.lock();
                }

                if (loopbackQueue_.empty()) {
                    out.clear();
                    return false;
                }
            }

            // 返回队列中的第一个数据包
            out = std::move(loopbackQueue_.front());
            loopbackQueue_.pop();

            return true;
        }

        TransportStatus QueryStatus() override {
            std::lock_guard<std::mutex> lock(mutex_);
            return status_;
        }

        /// <summary>
        /// 获取回路队列中的数据包数量（测试用）
        /// </summary>
        size_t GetQueueSize() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return loopbackQueue_.size();
        }

    private:
        PortConfig config_;
        TransportStatus status_;
        mutable std::mutex mutex_;
        std::queue<Buffer> loopbackQueue_;
    };

    // 工厂方法实现
    std::shared_ptr<ITransport> TransportFactory::CreateLoopback() {
        return std::make_shared<LoopbackTransport>();
    }

    // 临时占位实现（稍后完善）
    std::shared_ptr<ITransport> TransportFactory::CreateParallel() {
        // TODO: 实现并口传输
        return nullptr;
    }

    std::shared_ptr<ITransport> TransportFactory::CreateUsbPrint() {
        // TODO: 实现USB打印传输
        return nullptr;
    }

    std::shared_ptr<ITransport> TransportFactory::CreateNetworkPrint() {
        // TODO: 实现网络打印传输
        return nullptr;
    }

} // namespace PortMaster




 
