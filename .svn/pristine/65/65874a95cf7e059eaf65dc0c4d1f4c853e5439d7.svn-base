#pragma once

#include "ITransport.h"
#include <windows.h>

class LptSpoolerTransport : public ITransport
{
public:
    LptSpoolerTransport();
    virtual ~LptSpoolerTransport();

    // ITransport 鎺ュ彛瀹炵幇
    virtual bool Open(const TransportConfig& config) override;
    virtual void Close() override;
    virtual bool IsOpen() const override;
    virtual TransportState GetState() const override;

    virtual bool Configure(const TransportConfig& config) override;
    virtual TransportConfig GetConfiguration() const override;

    virtual size_t Write(const std::vector<uint8_t>& data) override;
    virtual size_t Write(const uint8_t* data, size_t length) override;
    virtual size_t Read(std::vector<uint8_t>& data, size_t maxLength = 0) override;
    virtual size_t Available() const override;

    virtual std::string GetLastError() const override;
    virtual std::string GetPortName() const override;
    virtual std::string GetTransportType() const override;

    virtual void SetDataReceivedCallback(DataReceivedCallback callback) override;
    virtual void SetStateChangedCallback(StateChangedCallback callback) override;

    virtual bool Flush() override;
    virtual bool ClearBuffers() override;

    // 辅助函数实现
    virtual void NotifyDataReceived(const std::vector<uint8_t>& data) override;
    virtual void NotifyStateChanged(TransportState state, const std::string& message = "") override;
    virtual void SetLastError(const std::string& error) override;

    // LPT 特有功能
    static std::vector<std::string> EnumeratePrinters();
    void SetPrinterName(const std::string& printerName);
    bool GetPrinterStatus(std::string& status);

protected:

private:
    HANDLE m_hPrinter;
    std::string m_printerName;
    DWORD m_jobId;
    
    bool StartRawPrintJob();
    bool EndPrintJob();
    std::string GetPrinterErrorString(DWORD error) const;
};