#include "pch.h"
#include "SendController.h"
#include "../Protocol/ReliableChannel.h"
#include <sstream>
#include <iomanip>

// 馃攽 鏋舵瀯閲嶆瀯锛歋endController涓撹亴绠＄悊鍣ㄥ疄鐜?
// SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鍙戦€佹搷浣滅殑涓氬姟閫昏緫澶勭悊
// SOLID-L: 閲屾皬鏇挎崲 - 鎵€鏈夊彂閫佺瓥鐣ラ兘鑳藉畨鍏ㄦ浛鎹?
// KISS鍘熷垯: 淇濇寔鍙戦€侀€昏緫绠€鍗曠洿瑙?

SendResult SendController::ExecuteSend(
    const std::vector<uint8_t>& inputData,
    const std::vector<uint8_t>& transmissionData,
    const std::wstring& currentFileName,
    bool isConnected,
    bool isReliableMode,
    std::shared_ptr<ReliableChannel> reliableChannel)
{
    // 1. 鍑嗗鍙戦€佹暟鎹?(SOLID-S: 鏁版嵁鍑嗗鍗曚竴鑱岃矗)
    std::vector<uint8_t> dataToSend;
    bool isFileTransmission = false;
    
    if (!PrepareSendData(inputData, transmissionData, dataToSend, isFileTransmission))
    {
        return SendResult::NO_DATA;
    }

    // 2. 楠岃瘉鍙戦€佹潯浠?(DRY鍘熷垯: 缁熶竴鐨勬潯浠堕獙璇侀€昏緫)
    SendResult validationResult = ValidateSendConditions(
        dataToSend, 
        isConnected, 
        m_hasActiveTransmission
    );
    
    if (validationResult != SendResult::SUCCESS)
    {
        return validationResult;
    }

    // 3. 鏍规嵁妯″紡鎵ц鍙戦€?(SOLID-O: 寮€闂師鍒?- 鍙墿灞曞彂閫佺瓥鐣?
    bool transmissionStarted = false;
    
    if (isReliableMode && reliableChannel)
    {
        transmissionStarted = StartReliableTransmission(
            dataToSend, 
            isFileTransmission ? currentFileName : L"", 
            reliableChannel
        );
    }
    else
    {
        transmissionStarted = StartNormalTransmission(dataToSend);
    }

    // 4. 鏇存柊鍐呴儴鐘舵€佸苟杩斿洖缁撴灉
    if (transmissionStarted)
    {
        m_hasActiveTransmission = true;
        return SendResult::SUCCESS;
    }
    else
    {
        return SendResult::FAILED;
    }
}

bool SendController::HasResumableTransmission() const
{
    // YAGNI鍘熷垯锛氱洰鍓嶇畝鍖栧疄鐜帮紝鍚庣画鍙墿灞曚负瀹屾暣鐨勬柇鐐圭画浼犳鏌?
    return false;
}

bool SendController::HandleResumeTransmission()
{
    // YAGNI鍘熷垯锛氱洰鍓嶇畝鍖栧疄鐜帮紝鍚庣画鍙墿灞曚负瀹屾暣鐨勬柇鐐圭画浼犲鐞?
    return false;
}

void SendController::ClearTransmissionContext()
{
    // KISS鍘熷垯锛氱畝鍗曠殑鐘舵€佹竻鐞?
    m_hasActiveTransmission = false;
}

SendResult SendController::ValidateSendConditions(
    const std::vector<uint8_t>& data,
    bool isConnected,
    bool isTransmissionActive)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鏉′欢楠岃瘉閫昏緫
    if (data.empty())
    {
        return SendResult::NO_DATA;
    }

    if (!isConnected)
    {
        return SendResult::NOT_CONNECTED;
    }

    if (isTransmissionActive)
    {
        return SendResult::ALREADY_ACTIVE;
    }

    return SendResult::SUCCESS;
}

bool SendController::StartReliableTransmission(
    const std::vector<uint8_t>& data,
    const std::wstring& fileName,
    std::shared_ptr<ReliableChannel> reliableChannel)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鍙潬浼犺緭鍚姩閫昏緫
    if (!reliableChannel)
    {
        return false;
    }

    // 楠岃瘉鍙潬浼犺緭閫氶亾鐘舵€?
    if (!reliableChannel->IsActive())
    {
        if (!reliableChannel->Start())
        {
            return false;
        }
    }

    // 妫€鏌ラ€氶亾鐘舵€?
    ReliableState currentState = reliableChannel->GetState();
    if (currentState != RELIABLE_IDLE)
    {
        return false;
    }

    // 鎵ц鍙戦€佹搷浣?
    bool result = false;
    if (!fileName.empty())
    {
        // 鍙戦€佹枃浠讹紙甯︽枃浠跺悕锛?
        std::string fileNameStr(fileName.begin(), fileName.end());
        result = reliableChannel->SendFile(fileNameStr, data);
    }
    else
    {
        // 鍙戦€佹暟鎹?
        result = reliableChannel->SendData(data);
    }

    return result;
}

bool SendController::StartNormalTransmission(const std::vector<uint8_t>& data)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鏅€氫紶杈撳惎鍔ㄩ€昏緫
    // YAGNI鍘熷垯锛氱洰鍓嶇畝鍖栧疄鐜帮紝鍚庣画鍙墿灞曚负瀹屾暣鐨勬櫘閫氫紶杈撳鐞?
    
    if (data.empty())
    {
        return false;
    }
    
    // 妯℃嫙鍙戦€佹垚鍔燂紙瀹為檯搴旇繛鎺ュ埌ITransport灞傦級
    return true;
}

std::wstring SendController::GetResultDescription(SendResult result)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞缁撴灉鎻忚堪鏍煎紡鍖?
    switch (result)
    {
        case SendResult::SUCCESS:
            return L"Send operation started successfully";
        case SendResult::NO_DATA:
            return L"No data to send";
        case SendResult::NOT_CONNECTED:
            return L"Not connected to port";
        case SendResult::ALREADY_ACTIVE:
            return L"Transmission already active";
        case SendResult::FAILED:
            return L"Send operation failed";
        default:
            return L"Unknown status";
    }
}

std::wstring SendController::FormatSendLogMessage(
    const std::wstring& operation,
    size_t dataSize,
    const std::wstring& fileName)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鏃ュ織娑堟伅鏍煎紡鍖?
    // KISS鍘熷垯锛氱畝鍗曠洿瑙傜殑鏃ュ織鏍煎紡
    std::wostringstream oss;
    oss << operation;
    
    if (!fileName.empty())
    {
        oss << L" [File: " << fileName << L"]";
    }
    
    oss << L" (Size: " << dataSize << L" bytes)";
    
    return oss.str();
}

// 绉佹湁杈呭姪鏂规硶瀹炵幇

bool SendController::PrepareSendData(
    const std::vector<uint8_t>& inputData,
    const std::vector<uint8_t>& transmissionData,
    std::vector<uint8_t>& outData,
    bool& outIsFileTransmission)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鏁版嵁鍑嗗閫昏緫
    // DRY鍘熷垯锛氱粺涓€鐨勬暟鎹紭鍏堢骇閫夋嫨閫昏緫
    
    if (!transmissionData.empty())
    {
        // 浼樺厛浣跨敤鏂囦欢鏁版嵁
        outData = transmissionData;
        outIsFileTransmission = true;
        return true;
    }
    else if (!inputData.empty())
    {
        // 浣跨敤杈撳叆妗嗘暟鎹?
        outData = inputData;
        outIsFileTransmission = false;
        return true;
    }
    
    // 鏃犲彲鐢ㄦ暟鎹?
    outData.clear();
    outIsFileTransmission = false;
    return false;
}

bool SendController::ValidateReliableChannel(std::shared_ptr<ReliableChannel> reliableChannel)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鍙潬浼犺緭閫氶亾楠岃瘉
    if (!reliableChannel)
    {
        return false;
    }

    if (!reliableChannel->IsActive())
    {
        // 灏濊瘯鍚姩閫氶亾
        return reliableChannel->Start();
    }

    // 妫€鏌ラ€氶亾鐘舵€?
    ReliableState currentState = reliableChannel->GetState();
    return (currentState == RELIABLE_IDLE);
}