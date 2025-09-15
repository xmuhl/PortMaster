#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

// 馃攽 鏋舵瀯閲嶆瀯锛氫粠PortMasterDlg鎻愬彇鐨勪紶杈撴帶鍒朵笓鑱岀鐞嗗櫒
// SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞浼犺緭杩涘害鎺у埗鍜岀姸鎬佺鐞?
// SOLID-O: 寮€闂師鍒?- 鍙墿灞曚笉鍚岀殑浼犺緭绛栫暐鍜岃繘搴︾畻娉?
// SOLID-D: 渚濊禆鍊掔疆 - 渚濊禆鎶借薄鎺ュ彛鑰岄潪鍏蜂綋UI瀹炵幇

// 鍓嶇疆澹版槑
class ITransport;

// 浼犺緭鐘舵€佹灇涓?(绠€鍖栫増鏈伩鍏嶇紪璇戜緷璧栭棶棰?
enum class TransmissionControllerState : int
{
    IDLE = 0,           // 绌洪棽
    TRANSMITTING = 1,   // 浼犺緭涓?
    PAUSED = 2,         // 鏆傚仠
    COMPLETED = 3,      // 瀹屾垚
    FAILED = 4          // 澶辫触
};

// 浼犺緭鎺у埗鍣?- 涓撹亴绠＄悊浼犺緭杩涘害鍜岀姸鎬佹帶鍒剁殑涓氬姟閫昏緫 (绠€鍖栫増鏈?
class TransmissionController
{
public:
    TransmissionController();
    ~TransmissionController();

    // 绂佺敤鎷疯礉鏋勯€犲拰璧嬪€?(RAII + 鍗曚緥璁捐)
    TransmissionController(const TransmissionController&) = delete;
    TransmissionController& operator=(const TransmissionController&) = delete;

    /**
     * @brief 鍚姩鍒嗗潡浼犺緭
     * @param data 瑕佷紶杈撶殑瀹屾暣鏁版嵁
     * @param chunkSize 姣忓潡澶у皬锛堝瓧鑺傦級
     * @return 鏄惁鎴愬姛鍚姩
     */
    bool StartChunkedTransmission(
        const std::vector<uint8_t>& data, 
        size_t chunkSize = 256
    );

    /**
     * @brief 鍋滄浼犺緭
     * @param completed 鏄惁涓烘甯稿畬鎴愬仠姝?
     */
    void StopTransmission(bool completed = false);

    /**
     * @brief 鏆傚仠浼犺緭
     * @return 鏄惁鎴愬姛鏆傚仠
     */
    bool PauseTransmission();

    /**
     * @brief 鎭㈠浼犺緭
     * @return 鏄惁鎴愬姛鎭㈠
     */
    bool ResumeTransmission();

    /**
     * @brief 鑾峰彇褰撳墠浼犺緭鐘舵€?
     */
    TransmissionControllerState GetCurrentState() const { return m_currentState; }

    /**
     * @brief 妫€鏌ユ槸鍚︽湁娲昏穬鐨勪紶杈?
     */
    bool IsTransmissionActive() const;

    /**
     * @brief 閲嶇疆鎵€鏈夌姸鎬佸拰鏁版嵁
     */
    void Reset();

    // 闈欐€佸伐鍏峰嚱鏁?(SOLID-S: 鍗曚竴鑱岃矗鐨勫伐鍏锋柟娉?

    /**
     * @brief 璁＄畻浼犺緭閫熷害
     * @param bytes 浼犺緭瀛楄妭鏁?
     * @param elapsedMs 鑰楁椂姣鏁?
     * @return 閫熷害 (B/s)
     */
    static double CalculateSpeed(size_t bytes, uint32_t elapsedMs);

    /**
     * @brief 鏍煎紡鍖栭€熷害鏄剧ず
     * @param speedBps 閫熷害 (B/s)
     * @return 鏍煎紡鍖栧悗鐨勯€熷害瀛楃涓?
     */
    static std::wstring FormatSpeed(double speedBps);

    /**
     * @brief 鑾峰彇浼犺緭鐘舵€佺殑鐢ㄦ埛鍙嬪ソ鎻忚堪
     */
    static std::wstring GetStateDescription(TransmissionControllerState state);

    /**
     * @brief 澶勭悊瀹氭椂鍣ㄩ┍鍔ㄧ殑鍒嗗潡浼犺緭 (浠嶱ortMasterDlg杩佺Щ)
     * @param transport 浼犺緭鎺ュ彛鎸囬拡
     * @param progressCallback 杩涘害鏇存柊鍥炶皟鍑芥暟
     * @param dataDisplayCallback 鏁版嵁鏄剧ず鍥炶皟鍑芥暟锛堝洖鐜祴璇曪級
     * @param isLoopbackTest 鏄惁涓哄洖鐜祴璇曟ā寮?
     * @return 浼犺緭澶勭悊缁撴灉锛歵rue=缁х画浼犺緭锛宖alse=浼犺緭瀹屾垚鎴栧け璐?
     */
    bool ProcessChunkedTransmission(
        std::shared_ptr<class ITransport> transport,
        std::function<void()> progressCallback = nullptr,
        std::function<void(const std::vector<uint8_t>&)> dataDisplayCallback = nullptr,
        bool isLoopbackTest = false
    );

    /**
     * @brief 鑾峰彇褰撳墠浼犺緭杩涘害淇℃伅
     * @param outTotalBytes 杈撳嚭鎬诲瓧鑺傛暟
     * @param outTransmittedBytes 杈撳嚭宸蹭紶杈撳瓧鑺傛暟
     * @param outProgress 杈撳嚭杩涘害鐧惧垎姣?
     */
    void GetTransmissionProgress(size_t& outTotalBytes, size_t& outTransmittedBytes, double& outProgress) const;

private:
    // 鏍稿績鐘舵€佸彉閲?(SOLID-S: 鏈€灏忓寲鐘舵€佸鏉傚害)
    TransmissionControllerState m_currentState = TransmissionControllerState::IDLE;
    
    // 浼犺緭鏁版嵁绠＄悊
    std::vector<uint8_t> m_transmissionData;
    size_t m_currentChunkIndex = 0;
    size_t m_chunkSize = 256;

    // 杩涘害璺熻釜 (浠嶱ortMasterDlg杩佺Щ)
    size_t m_totalBytesTransmitted = 0;
    
    /**
     * @brief 鑾峰彇褰撳墠鏃堕棿鎴筹紙姣锛?
     */
    uint32_t GetCurrentTimeMs() const;
};