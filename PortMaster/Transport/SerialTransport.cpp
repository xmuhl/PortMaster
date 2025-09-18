#pragma execution_character_set("utf-8")

#include "../stdafx.h"
#include "ITransport.h"

namespace PortMaster {

    /// <summary>
    /// 串口传输实现类
    /// </summary>
    class SerialTransport : public ITransport {
    public:
        SerialTransport() : handle_(INVALID_HANDLE_VALUE), status_(TransportStatus::TRANSPORT_CLOSED) {}

        virtual ~SerialTransport() {
            Close();
        }

        bool Open(const PortConfig& cfg, TransportContext& ctx) override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_CLOSED) {
                ctx.lastError = ERROR_ALREADY_EXISTS;
                ctx.errorMessage = _T("端口已经打开");
                return false;
            }

            status_ = TransportStatus::TRANSPORT_OPENING;

            // 构造串口设备名
            CString deviceName = _T("\\\\.\\") + cfg.portName;

            // 打开串口设备
            handle_ = CreateFile(deviceName,
                               GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED, NULL);

            if (handle_ == INVALID_HANDLE_VALUE) {
                status_ = TransportStatus::TRANSPORT_ERROR;
                ctx.lastError = GetLastError();
                ctx.errorMessage = _T("无法打开串口设备");
                return false;
            }

            // 配置串口参数
            DCB dcb = {0};
            dcb.DCBlength = sizeof(DCB);

            if (!GetCommState(handle_, &dcb)) {
                Close();
                status_ = TransportStatus::TRANSPORT_ERROR;
                ctx.lastError = GetLastError();
                ctx.errorMessage = _T("无法获取串口状态");
                return false;
            }

            dcb.BaudRate = cfg.baudRate;
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            dcb.fBinary = TRUE;
            dcb.fParity = FALSE;

            if (!SetCommState(handle_, &dcb)) {
                Close();
                status_ = TransportStatus::TRANSPORT_ERROR;
                ctx.lastError = GetLastError();
                ctx.errorMessage = _T("无法设置串口参数");
                return false;
            }

            // 设置超时
            COMMTIMEOUTS timeouts = {0};
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.ReadTotalTimeoutMultiplier = 0;
            timeouts.ReadTotalTimeoutConstant = cfg.timeout;
            timeouts.WriteTotalTimeoutMultiplier = 0;
            timeouts.WriteTotalTimeoutConstant = cfg.timeout;

            if (!SetCommTimeouts(handle_, &timeouts)) {
                Close();
                status_ = TransportStatus::TRANSPORT_ERROR;
                ctx.lastError = GetLastError();
                ctx.errorMessage = _T("无法设置串口超时");
                return false;
            }

            config_ = cfg;
            status_ = TransportStatus::TRANSPORT_OPEN;
            ctx.lastError = 0;
            ctx.errorMessage = _T("串口打开成功");

            return true;
        }

        void Close() override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (handle_ != INVALID_HANDLE_VALUE) {
                status_ = TransportStatus::TRANSPORT_CLOSING;
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
            }

            status_ = TransportStatus::TRANSPORT_CLOSED;
        }

        bool Send(const BufferView& data) override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_OPEN || data.empty()) {
                return false;
            }

            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (!overlapped.hEvent) {
                return false;
            }

            DWORD bytesWritten = 0;
            BOOL result = WriteFile(handle_, data.data(),
                                  static_cast<DWORD>(data.size()),
                                  &bytesWritten, &overlapped);

            if (!result && GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, config_.timeout);
                if (waitResult == WAIT_OBJECT_0) {
                    result = GetOverlappedResult(handle_, &overlapped, &bytesWritten, FALSE);
                }
            }

            CloseHandle(overlapped.hEvent);
            return result && (bytesWritten == data.size());
        }

        bool Receive(Buffer& out, DWORD timeout) override {
            std::lock_guard<std::mutex> lock(mutex_);

            if (status_ != TransportStatus::TRANSPORT_OPEN) {
                return false;
            }

            const DWORD BUFFER_SIZE = 4096;
            out.resize(BUFFER_SIZE);

            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (!overlapped.hEvent) {
                return false;
            }

            DWORD bytesRead = 0;
            BOOL result = ReadFile(handle_, out.data(), BUFFER_SIZE, &bytesRead, &overlapped);

            if (!result && GetLastError() == ERROR_IO_PENDING) {
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, timeout);
                if (waitResult == WAIT_OBJECT_0) {
                    result = GetOverlappedResult(handle_, &overlapped, &bytesRead, FALSE);
                }
            }

            CloseHandle(overlapped.hEvent);

            if (result && bytesRead > 0) {
                out.resize(bytesRead);
                return true;
            }

            out.clear();
            return false;
        }

        TransportStatus QueryStatus() override {
            std::lock_guard<std::mutex> lock(mutex_);
            return status_;
        }

    private:
        HANDLE handle_;
        PortConfig config_;
        TransportStatus status_;
        mutable std::mutex mutex_;
    };

    // 工厂方法实现
    std::shared_ptr<ITransport> TransportFactory::CreateSerial() {
        return std::make_shared<SerialTransport>();
    }

} // namespace PortMaster



 
