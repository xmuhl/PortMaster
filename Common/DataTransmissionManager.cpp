#pragma execution_character_set("utf-8")
#include "pch.h"
#include "DataTransmissionManager.h"
#include "../PortMasterDlg.h"
#include "../Transport/ITransport.h"
#include "../Protocol/ReliableChannel.h"
#include <Shlwapi.h>
#include <algorithm>

#pragma comment(lib, "Shlwapi.lib")

extern void WriteDebugLog(const char* message);

DataTransmissionManager::DataTransmissionManager(CPortMasterDlg* dialog)
    : m_dialog(dialog)
    , m_transport(nullptr)
    , m_reliableChannel(nullptr)
    , m_transmissionState(TransmissionState::IDLE)
    , m_transmissionContext(std::make_unique<TransmissionContext>())
    , m_chunkTransmissionIndex(0)
    , m_chunkSize(256)
    , m_transmissionTimer(0)
    , m_transmissionStartTime(0)
    , m_totalBytesTransmitted(0)
    , m_lastSpeedUpdateTime(0)
{
    WriteDebugLog("[DEBUG] DataTransmissionManager::DataTransmissionManager: 数据传输管理器构造完成");
}

DataTransmissionManager::~DataTransmissionManager()
{
    if (m_transmissionTimer != 0 && m_dialog)
    {
        m_dialog->KillTimer(m_transmissionTimer);
        m_transmissionTimer = 0;
    }
    WriteDebugLog("[DEBUG] DataTransmissionManager::~DataTransmissionManager: 数据传输管理器析构完成");
}

bool DataTransmissionManager::ExecuteSend()
{
    WriteDebugLog("[DEBUG] DataTransmissionManager::ExecuteSend: 开始执行发送操作");

    if (!m_dialog)
    {
        WriteDebugLog("[ERROR] DataTransmissionManager::ExecuteSend: 对话框指针为空");
        return false;
    }

    // 断点续传检查 (SOLID-S: 单一职责 - 续传逻辑分离)
    if (CheckResumeCondition())
    {
        int result = ShowResumeDialog();

        if (result == IDYES)
        {
            // 用户选择续传
            if (ResumeTransmission())
            {
                return true; // 续传成功，直接返回
            }
            // 续传失败，继续执行正常发送流程
        }
        else if (result == IDCANCEL)
        {
            return false; // 用户取消操作
        }
        // result == IDNO 时，清除断点并继续正常发送流程
        ClearTransmissionContext();
    }

    // 获取传输数据
    std::vector<uint8_t> dataToSend;
    bool isFileTransmission = false;

    if (!GetTransmissionData(dataToSend, isFileTransmission))
    {
        return false;
    }

    if (dataToSend.empty())
    {
        if (m_dialog)
        {
            m_dialog->AppendLog(L"错误：没有数据可发送");
        }
        return false;
    }

    // 检查传输条件
    if (!m_transport || !m_reliableChannel)
    {
        if (m_dialog)
        {
            m_dialog->AppendLog(L"错误：传输通道未初始化");
        }
        return false;
    }

    // 检查连接状态
    if (!m_dialog->m_bConnected)
    {
        m_dialog->ShowUserMessage(L"连接错误", L"请先连接端口才能发送数据", MB_ICONERROR);
        return false;
    }

    // 传输状态控制 (SOLID-S: 单一职责 - 传输状态控制)
    if (m_dialog->IsTransmissionActive())
    {
        // 正在传输中，提供停止传输选项
        int result = m_dialog->MessageBox(L"当前正在传输数据，是否要停止传输？",
            L"传输控制", MB_YESNO | MB_ICONQUESTION);

        if (result == IDYES) {
            SetTransmissionState(TransmissionState::IDLE);
            StopDataTransmission(false);
            m_dialog->AppendLog(L"用户手动停止传输");
        }
        return false;
    }

    // 根据传输模式选择传输方式
    return ExecuteTransmissionByMode(dataToSend, isFileTransmission);
}

bool DataTransmissionManager::StartDataTransmission(const std::vector<uint8_t>& data, bool isFileTransmission)
{
    WriteDebugLog("[DEBUG] DataTransmissionManager::StartDataTransmission: 开始数据传输");

    if (data.empty()) {
        if (m_dialog)
        {
            m_dialog->AppendLog(L"错误：数据为空，无法启动传输");
        }
        return false;
    }

    // 设置传输状态
    SetTransmissionState(TransmissionState::TRANSMITTING);

    // 初始化传输参数
    m_chunkTransmissionData = data;
    m_chunkTransmissionIndex = 0;
    m_chunkSize = 256; // 每次传输256字节 (KISS: 使用固定大小简化逻辑)

    // 记录传输开始时间
    m_transmissionStartTime = GetTickCount();
    m_totalBytesTransmitted = 0;
    m_lastSpeedUpdateTime = m_transmissionStartTime;

    // 更新UI状态
    if (m_dialog)
    {
        m_dialog->UpdateButtonStatesLegacy();

        // 设置进度条 (SOLID-S: 修复大文件传输时的范围溢出问题)
        CProgressCtrl* progressCtrl = nullptr;
        // 这里需要通过dialog获取进度条控件 - 暂时简化处理

        CString logMsg;
        logMsg.Format(L"开始分块传输 - 总大小: %zu 字节, 块大小: %zu 字节",
            data.size(), m_chunkSize);
        m_dialog->AppendLog(logMsg);
    }

    // 启动传输定时器
    const UINT TRANSMISSION_TIMER_ID = 1001;
    const UINT TRANSMISSION_TIMER_INTERVAL = 50; // 50ms间隔

    if (m_dialog)
    {
        m_transmissionTimer = m_dialog->SetTimer(TRANSMISSION_TIMER_ID, TRANSMISSION_TIMER_INTERVAL, NULL);
    }

    if (m_transmissionTimer == 0) {
        // 定时器启动失败
        SetTransmissionState(TransmissionState::FAILED);
        if (m_dialog)
        {
            m_dialog->AppendLog(L"错误：无法启动传输定时器");
        }
        return false;
    }

    WriteDebugLog("[DEBUG] DataTransmissionManager::StartDataTransmission: 传输定时器已启动，开始分块传输");
    return true;
}

void DataTransmissionManager::StopDataTransmission(bool completed)
{
    WriteDebugLog("[DEBUG] DataTransmissionManager::StopDataTransmission: 停止数据传输");

    // 停止定时器
    if (m_transmissionTimer != 0 && m_dialog)
    {
        m_dialog->KillTimer(m_transmissionTimer);
        m_transmissionTimer = 0;
    }

    // 更新状态
    if (completed)
    {
        SetTransmissionState(TransmissionState::COMPLETED);
    }
    else
    {
        SetTransmissionState(TransmissionState::IDLE);
    }

    // 清理数据
    m_chunkTransmissionData.clear();
    m_chunkTransmissionIndex = 0;

    if (m_dialog)
    {
        m_dialog->UpdateButtonStatesLegacy();

        if (completed)
        {
            m_dialog->AppendLog(L"数据传输完成");
        }
        else
        {
            m_dialog->AppendLog(L"数据传输已停止");
        }
    }
}

void DataTransmissionManager::UpdateTransmissionProgress()
{
    // 这里需要根据实际的传输进度更新UI
    // 暂时简化实现
    if (!m_dialog) return;

    if (m_transmissionState == TransmissionState::TRANSMITTING)
    {
        size_t totalBytes = m_chunkTransmissionData.size();
        if (totalBytes > 0)
        {
            double progress = static_cast<double>(m_chunkTransmissionIndex * m_chunkSize) / totalBytes * 100.0;
            progress = std::min(progress, 100.0);

            CString progressMsg;
            progressMsg.Format(L"传输进度: %.1f%% (%zu/%zu 字节)",
                progress, std::min(m_chunkTransmissionIndex * m_chunkSize, totalBytes), totalBytes);

            WriteDebugLog(CT2A(progressMsg));
        }
    }
}

void DataTransmissionManager::SetTransmissionState(TransmissionState newState)
{
    TransmissionState oldState = m_transmissionState.load();
    if (oldState == newState) return;

    m_transmissionState = newState;

    CString debugMsg;
    debugMsg.Format(L"[DEBUG] DataTransmissionManager::SetTransmissionState: 状态变更 %d->%d",
        static_cast<int>(oldState), static_cast<int>(newState));
    WriteDebugLog(CT2A(debugMsg));

    // 通知UI更新
    if (m_dialog)
    {
        m_dialog->UpdateButtonStatesLegacy();
    }
}

void DataTransmissionManager::OnChunkTransmissionTimer()
{
    if (m_transmissionState != TransmissionState::TRANSMITTING)
    {
        return;
    }

    if (m_chunkTransmissionData.empty())
    {
        StopDataTransmission(true);
        return;
    }

    size_t dataSize = m_chunkTransmissionData.size();
    size_t currentPos = m_chunkTransmissionIndex * m_chunkSize;

    if (currentPos >= dataSize)
    {
        // 传输完成
        SetTransmissionState(TransmissionState::COMPLETED);
        StopDataTransmission(true);
        return;
    }

    // 计算当前块大小
    size_t actualChunkSize = std::min(m_chunkSize, dataSize - currentPos);

    // 发送当前块数据
    std::vector<uint8_t> chunkData(
        m_chunkTransmissionData.begin() + currentPos,
        m_chunkTransmissionData.begin() + currentPos + actualChunkSize);

    if (m_transport && chunkData.size() > 0)
    {
        if (m_transport->Write(chunkData) > 0)
        {
            m_chunkTransmissionIndex++;
            m_totalBytesTransmitted += actualChunkSize;
            UpdateTransmissionProgress();
        }
        else
        {
            // 发送失败
            SetTransmissionState(TransmissionState::FAILED);
            StopDataTransmission(false);

            if (m_dialog)
            {
                m_dialog->AppendLog(L"数据发送失败");
            }
        }
    }
}

bool DataTransmissionManager::CheckResumeCondition()
{
    return (GetTransmissionState() == TransmissionState::PAUSED && m_transmissionContext && m_transmissionContext->CanResume());
}

int DataTransmissionManager::ShowResumeDialog()
{
    if (!m_dialog) return IDCANCEL;

    CString resumeMsg;
    resumeMsg.Format(L"检测到未完成的传输: %s (进度 %.1f%%)\n是否续传？\n\n点击\"是\"继续传输，点击\"否\"重新开始",
        PathFindFileName(m_transmissionContext->sourceFilePath),
        m_transmissionContext->GetProgressPercentage());

    return m_dialog->MessageBox(resumeMsg, L"断点续传", MB_YESNOCANCEL | MB_ICONQUESTION);
}

std::vector<uint8_t> DataTransmissionManager::GetInputData()
{
    if (!m_dialog)
    {
        return std::vector<uint8_t>();
    }

    // 委托给dialog的GetInputData方法
    return m_dialog->GetInputData();
}

bool DataTransmissionManager::GetTransmissionData(std::vector<uint8_t>& dataToSend, bool& isFileTransmission)
{
    if (!m_dialog) return false;

    dataToSend.clear();
    isFileTransmission = false;

    // 检查是否有文件数据要发送
    if (!m_dialog->m_transmissionData.empty())
    {
        // 有文件数据，发送文件
        dataToSend = m_dialog->m_transmissionData;
        isFileTransmission = true;
        m_dialog->AppendLog(L"发送文件数据");
    }
    else
    {
        // 获取输入框数据
        dataToSend = m_dialog->GetInputData();
        m_dialog->AppendLog(L"发送输入数据");
    }

    return !dataToSend.empty();
}

bool DataTransmissionManager::PauseTransmission()
{
    if (m_transmissionState == TransmissionState::TRANSMITTING)
    {
        SetTransmissionState(TransmissionState::PAUSED);
        return true;
    }
    return false;
}

bool DataTransmissionManager::ResumeTransmission()
{
    if (m_transmissionState == TransmissionState::PAUSED && m_transmissionContext && m_transmissionContext->CanResume())
    {
        SetTransmissionState(TransmissionState::TRANSMITTING);
        return true;
    }
    return false;
}

void DataTransmissionManager::SaveTransmissionContext(const CString& filePath, size_t totalBytes, size_t transmittedBytes)
{
    if (!m_transmissionContext) {
        m_transmissionContext = std::make_unique<TransmissionContext>();
    }

    m_transmissionContext->sourceFilePath = filePath;
    m_transmissionContext->totalBytes = totalBytes;
    m_transmissionContext->transmittedBytes = transmittedBytes;
    // m_transmissionContext->startTime = std::chrono::steady_clock::now(); // 如果有这个成员的话

    CString contextMsg;
    contextMsg.Format(L"[DEBUG] DataTransmissionManager::SaveTransmissionContext: 保存传输上下文 %s %zu/%zu",
        filePath, transmittedBytes, totalBytes);
    WriteDebugLog(CT2A(contextMsg));
}

void DataTransmissionManager::ClearTransmissionContext()
{
    m_transmissionContext = std::make_unique<TransmissionContext>();
    WriteDebugLog("[DEBUG] DataTransmissionManager::ClearTransmissionContext: 清除传输上下文");
}

void DataTransmissionManager::SetTransportObjects(std::shared_ptr<ITransport> transport, std::shared_ptr<ReliableChannel> reliableChannel)
{
    m_transport = transport;
    m_reliableChannel = reliableChannel;
    WriteDebugLog("[DEBUG] DataTransmissionManager::SetTransportObjects: 设置传输对象完成");
}

TransmissionState DataTransmissionManager::GetTransmissionState() const
{
    return m_transmissionState.load();
}

const TransmissionContext& DataTransmissionManager::GetTransmissionContext() const
{
    return *m_transmissionContext;
}

bool DataTransmissionManager::ExecuteTransmissionByMode(const std::vector<uint8_t>& dataToSend, bool isFileTransmission)
{
    if (!m_dialog) return false;

    if (m_dialog->m_bReliableMode && m_reliableChannel)
    {
        return ExecuteReliableTransmission(dataToSend, isFileTransmission);
    }
    else
    {
        // 使用普通传输模式
        return StartDataTransmission(dataToSend, isFileTransmission);
    }
}

bool DataTransmissionManager::ExecuteReliableTransmission(const std::vector<uint8_t>& dataToSend, bool isFileTransmission)
{
    if (!m_dialog || !m_reliableChannel) return false;

    // 1. 验证可靠传输通道是否已激活
    if (!m_reliableChannel->IsActive())
    {
        m_dialog->AppendLog(L"可靠传输通道未启动，尝试启动...");
        if (!m_reliableChannel->Start())
        {
            SetTransmissionState(TransmissionState::FAILED);
            m_dialog->AppendLog(L"无法启动可靠传输通道");
            CString error = CA2W(m_reliableChannel->GetLastError().c_str(), CP_UTF8);
            if (!error.IsEmpty())
            {
                m_dialog->AppendLog(L"启动错误: " + error);
            }
            m_dialog->ShowUserMessage(L"可靠传输启动失败",
                L"可靠传输通道无法启动，请检查连接状态或切换到普通传输模式",
                MB_ICONERROR);
            return false;
        }
        m_dialog->AppendLog(L"可靠传输通道启动成功");
    }

    // 2. 验证可靠传输通道状态
    ReliableState currentState = m_reliableChannel->GetState();
    if (currentState != RELIABLE_IDLE)
    {
        SetTransmissionState(TransmissionState::FAILED);
        CString stateMsg;
        stateMsg.Format(L"可靠传输通道状态异常 (状态码: %d)，请等待当前操作完成或重新连接", static_cast<int>(currentState));
        m_dialog->AppendLog(stateMsg);
        m_dialog->ShowUserMessage(L"可靠传输状态错误", stateMsg, MB_ICONWARNING);
        return false;
    }

    // 3. 开始传输操作
    SetTransmissionState(TransmissionState::TRANSMITTING);
    bool transmissionStarted = false;

    if (isFileTransmission && !m_dialog->m_currentFileName.IsEmpty())
    {
        // 发送文件（带文件名）
        std::string fileNameStr = CT2A(m_dialog->m_currentFileName);
        transmissionStarted = m_reliableChannel->SendFile(fileNameStr, dataToSend);
        if (transmissionStarted)
        {
            m_dialog->AppendLog(L"开始可靠文件传输: " + m_dialog->m_currentFileName);
        }
        else
        {
            m_dialog->AppendLog(L"可靠文件传输启动失败");
        }
    }
    else
    {
        // 发送数据
        transmissionStarted = m_reliableChannel->SendData(dataToSend);
        if (transmissionStarted)
        {
            m_dialog->AppendLog(L"开始可靠传输");
        }
        else
        {
            m_dialog->AppendLog(L"可靠传输启动失败");
        }
    }

    // 4. 处理传输启动失败的情况
    if (!transmissionStarted)
    {
        SetTransmissionState(TransmissionState::FAILED);
        CString error = CA2W(m_reliableChannel->GetLastError().c_str(), CP_UTF8);
        if (!error.IsEmpty())
        {
            m_dialog->AppendLog(L"错误详情: " + error);
        }

        // 提供用户友好的错误处理建议
        m_dialog->ShowUserMessage(L"可靠传输失败",
            L"可靠传输启动失败。\n\n建议操作：\n1. 检查连接状态\n2. 重新连接端口\n3. 或切换到普通传输模式",
            MB_ICONERROR);
        return false;
    }

    return true;
}