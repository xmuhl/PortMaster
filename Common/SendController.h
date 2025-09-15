#pragma once

#include <memory>
#include <vector>
#include <string>

// 馃攽 鏋舵瀯閲嶆瀯锛氫粠PortMasterDlg鎻愬彇鐨勫彂閫佹帶鍒朵笓鑱岀鐞嗗櫒
// SOLID-S: 鍗曚竴鑱岃矗 - 涓撴敞鍙戦€佹搷浣滅殑涓氬姟閫昏緫鍜屾祦绋嬫帶鍒?
// SOLID-O: 寮€闂師鍒?- 鍙墿灞曚笉鍚岀殑鍙戦€佺瓥鐣ュ拰妯″紡
// SOLID-D: 渚濊禆鍊掔疆 - 渚濊禆鎶借薄鎺ュ彛鑰岄潪鍏蜂綋UI瀹炵幇

// 鍓嶇疆澹版槑
class ReliableChannel;
class ITransport;

// 鍙戦€佺粨鏋滅姸鎬佹灇涓?(KISS鍘熷垯锛氱畝鍗曟槑纭殑鐘舵€佸畾涔?
enum class SendResult : int
{
    SUCCESS = 0,        // 鍙戦€佹垚鍔熷惎鍔?
    NO_DATA,            // 鏃犳暟鎹彲鍙戦€?
    NOT_CONNECTED,      // 鏈繛鎺?
    ALREADY_ACTIVE,     // 浼犺緭宸叉縺娲?
    FAILED              // 鍙戦€佸け璐?
};

// 鍙戦€佹帶鍒跺櫒 - 涓撹亴绠＄悊鍙戦€佺浉鍏崇殑鎵€鏈変笟鍔￠€昏緫
class SendController
{
public:
    SendController() = default;
    ~SendController() = default;

    // 绂佺敤鎷疯礉鏋勯€犲拰璧嬪€?(RAII + 鍗曚緥璁捐)
    SendController(const SendController&) = delete;
    SendController& operator=(const SendController&) = delete;

    /**
     * @brief 鎵ц鍙戦€佹搷浣滅殑涓昏鍏ュ彛鐐?
     * @param inputData 杈撳叆鏁版嵁锛堜粠UI杈撳叆妗嗚幏鍙栵級
     * @param transmissionData 鏂囦欢鏁版嵁锛堟嫋鏀炬垨鍔犺浇鐨勬枃浠讹級
     * @param currentFileName 褰撳墠鏂囦欢鍚?
     * @param isConnected 杩炴帴鐘舵€?
     * @param isReliableMode 鏄惁浣跨敤鍙潬浼犺緭妯″紡
     * @param reliableChannel 鍙潬浼犺緭閫氶亾瀹炰緥
     * @return SendResult 鍙戦€佺粨鏋滅姸鎬?
     */
    SendResult ExecuteSend(
        const std::vector<uint8_t>& inputData,
        const std::vector<uint8_t>& transmissionData, 
        const std::wstring& currentFileName,
        bool isConnected,
        bool isReliableMode,
        std::shared_ptr<ReliableChannel> reliableChannel
    );

    /**
     * @brief 妫€鏌ユ槸鍚︽湁鏂偣缁紶鐨勬暟鎹?
     * @return 濡傛灉鏈夊彲鎭㈠鐨勪紶杈撲笂涓嬫枃杩斿洖true
     */
    bool HasResumableTransmission() const;

    /**
     * @brief 澶勭悊鏂偣缁紶閫昏緫
     * @return 濡傛灉鐢ㄦ埛閫夋嫨缁紶骞舵垚鍔熷惎鍔ㄨ繑鍥瀟rue
     */
    bool HandleResumeTransmission();

    /**
     * @brief 娓呴櫎浼犺緭涓婁笅鏂?
     */
    void ClearTransmissionContext();

    /**
     * @brief 楠岃瘉鍙戦€佸墠鐨勬潯浠舵鏌?
     * @param data 瑕佸彂閫佺殑鏁版嵁
     * @param isConnected 杩炴帴鐘舵€?
     * @param isTransmissionActive 浼犺緭鏄惁婵€娲?
     * @return SendResult 楠岃瘉缁撴灉
     */
    static SendResult ValidateSendConditions(
        const std::vector<uint8_t>& data,
        bool isConnected,
        bool isTransmissionActive
    );

    /**
     * @brief 鍚姩鍙潬浼犺緭妯″紡
     * @param data 瑕佸彂閫佺殑鏁版嵁
     * @param fileName 鏂囦欢鍚嶏紙鍙€夛級
     * @param reliableChannel 鍙潬浼犺緭閫氶亾
     * @return 鏄惁鎴愬姛鍚姩
     */
    static bool StartReliableTransmission(
        const std::vector<uint8_t>& data,
        const std::wstring& fileName,
        std::shared_ptr<ReliableChannel> reliableChannel
    );

    /**
     * @brief 鍚姩鏅€氫紶杈撴ā寮?
     * @param data 瑕佸彂閫佺殑鏁版嵁
     * @return 鏄惁鎴愬姛鍚姩
     */
    static bool StartNormalTransmission(
        const std::vector<uint8_t>& data
    );

    /**
     * @brief 鑾峰彇鍙戦€佺粨鏋滅殑鐢ㄦ埛鍙嬪ソ鎻忚堪
     * @param result 鍙戦€佺粨鏋?
     * @return 鎻忚堪瀛楃涓?
     */
    static std::wstring GetResultDescription(SendResult result);

    /**
     * @brief 鏍煎紡鍖栧彂閫佹搷浣滅殑鏃ュ織娑堟伅
     * @param operation 鎿嶄綔鎻忚堪
     * @param dataSize 鏁版嵁澶у皬
     * @param fileName 鏂囦欢鍚嶏紙鍙€夛級
     * @return 鏍煎紡鍖栧悗鐨勬棩蹇楁秷鎭?
     */
    static std::wstring FormatSendLogMessage(
        const std::wstring& operation,
        size_t dataSize,
        const std::wstring& fileName = L""
    );

private:
    // 馃攽 YAGNI鍘熷垯锛氫粎淇濈暀蹇呰鐨勫唴閮ㄧ姸鎬?
    bool m_hasActiveTransmission = false;
    
    // 鍐呴儴杈呭姪鏂规硶 (SOLID-S: 鍗曚竴鑱岃矗鐨勭粏鍒嗗姛鑳?
    
    /**
     * @brief 鍑嗗鍙戦€佹暟鎹紙閫夋嫨杈撳叆鏁版嵁鎴栨枃浠舵暟鎹級
     * @param inputData 杈撳叆妗嗘暟鎹?
     * @param transmissionData 鏂囦欢鏁版嵁
     * @param outData 杈撳嚭鏁版嵁寮曠敤
     * @param outIsFileTransmission 杈撳嚭鏄惁涓烘枃浠朵紶杈撴爣蹇?
     * @return 鏄惁鎴愬姛鍑嗗鏁版嵁
     */
    bool PrepareSendData(
        const std::vector<uint8_t>& inputData,
        const std::vector<uint8_t>& transmissionData,
        std::vector<uint8_t>& outData,
        bool& outIsFileTransmission
    );

    /**
     * @brief 楠岃瘉鍙潬浼犺緭閫氶亾鐘舵€?
     * @param reliableChannel 鍙潬浼犺緭閫氶亾
     * @return 閫氶亾鏄惁鍙敤
     */
    bool ValidateReliableChannel(std::shared_ptr<ReliableChannel> reliableChannel);
};