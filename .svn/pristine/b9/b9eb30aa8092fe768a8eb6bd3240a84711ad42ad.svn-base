#pragma once

#include <vector>
#include <string>

// 数据格式化工具类
class DataFormatter
{
public:
    // 字节数组转崄鍏繘鍒跺瓧绗︿覆
    static std::string BytesToHex(const std::vector<uint8_t>& data, bool addSpaces = true);
    static std::string BytesToHex(const uint8_t* data, size_t length, bool addSpaces = true);
    
    // 十六进制字符串转字节数组
    static std::vector<uint8_t> HexToBytes(const std::string& hex);
    
    // 字节数组转可显示文本锛堥潪鎵撳嵃字符鐢ㄧ偣鍙蜂唬鏇匡級
    static std::string BytesToText(const std::vector<uint8_t>& data);
    static std::string BytesToText(const uint8_t* data, size_t length);
    
    // 文本转瓧鑺傛暟缁勶紙UTF-8缂栫爜（
    static std::vector<uint8_t> TextToBytes(const std::string& text);
    
    // 楠岃瘉十六进制字符涓叉牸寮?
    static bool IsValidHex(const std::string& hex);
    
    // 鏍煎紡鍖栧崄鍏繘鍒舵樉绀猴紙甯﹀湴鍧€鍜孉SCII鍒楋級
    static std::string FormatHexDump(const std::vector<uint8_t>& data, int bytesPerLine = 16);
    
    // 创建专业十六进制编辑器头部
    static std::string CreateHexEditorHeader();
    
    // 娓呯悊十六进制杈撳叆锛堢Щ闄ょ┖鏍笺€佹崲琛岀瓑（
    static std::string CleanHexInput(const std::string& input);
    
    // 字符鏄惁涓哄彲鎵撳嵃字符
    static bool IsPrintable(uint8_t c);
    
    // 十六进制字符转暟鍊?
    static int HexCharToValue(char c);
    
    // 鏁板€艰浆十六进制字符
    static char ValueToHexChar(int value);

    
    // 智能数据类型检测
    static bool IsBinaryData(const std::vector<uint8_t>& data);
    static bool IsBinaryData(const uint8_t* data, size_t length);
    
    // 智能文本显示（根据数据类型显示不同内容）
    static std::string SmartTextDisplay(const std::vector<uint8_t>& data);
    static std::string SmartTextDisplay(const uint8_t* data, size_t length);

private:
    static const char* HEX_CHARS;
    
    // UTF-8字符序列验证
    static bool IsValidUtf8Sequence(const uint8_t* data, size_t maxLength);
};