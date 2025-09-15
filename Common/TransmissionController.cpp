#include "pch.h"
#include "TransmissionController.h"
#include "../Transport/ITransport.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <memory>

// Windows-specific includes for time functions
#ifdef _WIN32
#include <windows.h>
#endif

// 馃攽 鏋舵瀯閲嶆瀯锛歍ransmissionController涓撹亴绠＄悊鍣ㄥ疄鐜?(绠€鍖栫増鏈?
// SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞浼犺緭鎺у埗鍜岃繘搴︾鐞嗕笟鍔￠€昏緫澶勭悊
// KISS鍘熷垯: 淇濇寔浼犺緭鎺у埗閫昏緫绠€鍗曠洿瑙?

TransmissionController::TransmissionController()
{
    Reset();
}

TransmissionController::~TransmissionController()
{
    // 纭繚鍋滄浼犺緭
    if (IsTransmissionActive())
    {
        StopTransmission(false);
    }
}

bool TransmissionController::StartChunkedTransmission(
    const std::vector<uint8_t>& data, 
    size_t chunkSize)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鍚姩浼犺緭鐨勬潯浠舵鏌ュ拰鍒濆鍖?
    if (data.empty())
    {
        return false;
    }

    if (IsTransmissionActive())
    {
        return false;
    }

    // 鍒濆鍖栦紶杈撴暟鎹拰鍙傛暟
    m_transmissionData = data;
    m_currentChunkIndex = 0;
    m_chunkSize = std::max(size_t(1), chunkSize); // 纭繚鍧楀ぇ灏忚嚦灏戜负1

    // 璁剧疆浼犺緭鐘舵€?
    m_currentState = TransmissionControllerState::TRANSMITTING;

    return true;
}

void TransmissionController::StopTransmission(bool completed)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞浼犺緭鍋滄鐨勭姸鎬佺鐞嗗拰娓呯悊
    
    // 璁剧疆鏈€缁堢姸鎬?
    m_currentState = completed ? 
        TransmissionControllerState::COMPLETED : TransmissionControllerState::IDLE;

    // 娓呯悊浼犺緭鏁版嵁 (YAGNI: 鍙婃椂閲婃斁涓嶉渶瑕佺殑璧勬簮)
    m_transmissionData.clear();
    m_currentChunkIndex = 0;
}

bool TransmissionController::PauseTransmission()
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鏆傚仠閫昏緫澶勭悊
    if (m_currentState != TransmissionControllerState::TRANSMITTING)
    {
        return false; // 鍙湁浼犺緭涓姸鎬佹墠鑳芥殏鍋?
    }

    m_currentState = TransmissionControllerState::PAUSED;
    return true;
}

bool TransmissionController::ResumeTransmission()
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鎭㈠閫昏緫澶勭悊
    if (m_currentState != TransmissionControllerState::PAUSED)
    {
        return false; // 鍙湁鏆傚仠鐘舵€佹墠鑳芥仮澶?
    }

    m_currentState = TransmissionControllerState::TRANSMITTING;
    return true;
}

bool TransmissionController::IsTransmissionActive() const
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞娲昏穬鐘舵€佸垽鏂€昏緫
    return m_currentState == TransmissionControllerState::TRANSMITTING || 
           m_currentState == TransmissionControllerState::PAUSED;
}

void TransmissionController::Reset()
{
    // KISS鍘熷垯锛氱畝鍗曠殑鐘舵€侀噸缃?
    m_currentState = TransmissionControllerState::IDLE;
    m_transmissionData.clear();
    m_currentChunkIndex = 0;
    m_chunkSize = 256;
    m_totalBytesTransmitted = 0;
}

// 闈欐€佸伐鍏峰嚱鏁板疄鐜?(SOLID-S: 鍗曚竴鑱岃矗鐨勫伐鍏锋柟娉?

double TransmissionController::CalculateSpeed(size_t bytes, uint32_t elapsedMs)
{
    if (elapsedMs == 0) return 0.0;
    return (double(bytes) * 1000.0) / elapsedMs; // B/s
}

std::wstring TransmissionController::FormatSpeed(double speedBps)
{
    // KISS鍘熷垯锛氱畝鍗曠洿瑙傜殑閫熷害鏍煎紡鍖?
    std::wostringstream oss;
    
    if (speedBps >= 1024 * 1024) // MB/s
    {
        oss << std::fixed << std::setprecision(1) 
            << (speedBps / (1024 * 1024)) << L" MB/s";
    }
    else if (speedBps >= 1024) // KB/s
    {
        oss << std::fixed << std::setprecision(1) 
            << (speedBps / 1024) << L" KB/s";
    }
    else // B/s
    {
        oss << std::fixed << std::setprecision(0) 
            << speedBps << L" B/s";
    }
    
    return oss.str();
}

std::wstring TransmissionController::GetStateDescription(TransmissionControllerState state)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鐘舵€佹弿杩版牸寮忓寲
    switch (state)
    {
        case TransmissionControllerState::IDLE:
            return L"Idle";
        case TransmissionControllerState::TRANSMITTING:
            return L"Transmitting";
        case TransmissionControllerState::PAUSED:
            return L"Paused";
        case TransmissionControllerState::COMPLETED:
            return L"Completed";
        case TransmissionControllerState::FAILED:
            return L"Failed";
        default:
            return L"Unknown";
    }
}

uint32_t TransmissionController::GetCurrentTimeMs() const
{
    // 璺ㄥ钩鍙版椂闂磋幏鍙?(SOLID-S: 鍗曚竴鑱岃矗鐨勬椂闂存娊璞?
#ifdef _WIN32
    return GetTickCount();
#else
    return 0; // 绠€鍖栧疄鐜?
#endif
}

// 杩佺Щ鐨勬牳蹇冨垎鍧椾紶杈撳鐞嗘柟娉?(浠嶱ortMasterDlg::OnChunkTransmissionTimer杩佺Щ)
bool TransmissionController::ProcessChunkedTransmission(
    std::shared_ptr<class ITransport> transport,
    std::function<void()> progressCallback,
    std::function<void(const std::vector<uint8_t>&)> dataDisplayCallback,
    bool isLoopbackTest)
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鍒嗗潡浼犺緭閫昏緫澶勭悊

    // 1. 鍩虹鐘舵€侀獙璇?
    if (m_currentState != TransmissionControllerState::TRANSMITTING &&
        m_currentState != TransmissionControllerState::PAUSED) {
        return false; // 闈炰紶杈撶姸鎬侊紝鍋滄澶勭悊
    }

    // 2. 鏁版嵁鏈夋晥鎬ф鏌?
    if (m_transmissionData.empty()) {
        m_currentState = TransmissionControllerState::FAILED;
        return false;
    }

    // 3. 鏆傚仠鐘舵€佺殑鏅鸿兘澶勭悊
    if (m_currentState == TransmissionControllerState::PAUSED) {
        return true; // 鏆傚仠鐘舵€佷笅淇濇寔瀹氭椂鍣ㄨ繍琛屼絾涓嶆墽琛屼紶杈?
    }

    // 4. 浼犺緭瀹屾垚妫€鏌?
    if (m_currentChunkIndex >= m_transmissionData.size()) {
        m_currentState = TransmissionControllerState::COMPLETED;
        return false; // 浼犺緭瀹屾垚
    }

    // 5. 璁＄畻褰撳墠鍧楃殑澶у皬
    size_t remainingBytes = m_transmissionData.size() - m_currentChunkIndex;
    size_t currentChunkSize = std::min(m_chunkSize, remainingBytes);

    if (currentChunkSize == 0) {
        m_currentState = TransmissionControllerState::COMPLETED;
        return false; // 浼犺緭瀹屾垚
    }

    // 6. 鎻愬彇褰撳墠鏁版嵁鍧?
    std::vector<uint8_t> currentChunk(
        m_transmissionData.begin() + m_currentChunkIndex,
        m_transmissionData.begin() + m_currentChunkIndex + currentChunkSize
    );

    // 7. 鎵ц鏁版嵁浼犺緭 (SOLID-D: 渚濊禆鎶借薄 - 浣跨敤浼犺緭鎺ュ彛)
    bool transmissionSuccess = false;
    if (transport && transport->IsOpen()) {
        try {
            size_t written = transport->Write(currentChunk);
            transmissionSuccess = (written == currentChunk.size());

            if (transmissionSuccess) {
                // 鏇存柊浼犺緭杩涘害
                m_currentChunkIndex += currentChunkSize;
                m_totalBytesTransmitted += currentChunkSize;

                // 璋冪敤杩涘害鏇存柊鍥炶皟
                if (progressCallback) {
                    progressCallback();
                }

                // 鍥炵幆娴嬭瘯妯″紡鐨勬暟鎹樉绀?
                if (isLoopbackTest && dataDisplayCallback) {
                    dataDisplayCallback(currentChunk);
                }
            } else {
                // 鍐欏叆澶辫触 - 璁剧疆澶辫触鐘舵€?
                m_currentState = TransmissionControllerState::FAILED;
                return false;
            }
        }
        catch (const std::exception&) {
            // 寮傚父澶勭悊 - 璁剧疆澶辫触鐘舵€?
            m_currentState = TransmissionControllerState::FAILED;
            return false;
        }
    } else {
        // 浼犺緭閫氶亾閿欒 - 璁剧疆澶辫触鐘舵€?
        m_currentState = TransmissionControllerState::FAILED;
        return false;
    }

    return true; // 缁х画浼犺緭
}

void TransmissionController::GetTransmissionProgress(
    size_t& outTotalBytes,
    size_t& outTransmittedBytes,
    double& outProgress) const
{
    // SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞杩涘害淇℃伅鎻愪緵
    outTotalBytes = m_transmissionData.size();
    outTransmittedBytes = m_totalBytesTransmitted;

    if (outTotalBytes > 0) {
        outProgress = (double)(m_currentChunkIndex * 100) / outTotalBytes;
    } else {
        outProgress = 0.0;
    }
}