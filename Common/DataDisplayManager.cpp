#pragma execution_character_set("utf-8")
#include "pch.h"
#include "DataDisplayManager.h"
#include <afxwin.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

extern void WriteDebugLog(const char* message);

// DefaultDataDisplayFormatter 实现

CString DefaultDataDisplayFormatter::Format(const std::vector<uint8_t>& data, DisplayMode mode)
{
    try {
        switch (mode) {
        case DisplayMode::TEXT:
            return FormatAsText(data);
        case DisplayMode::HEX:
            return FormatAsHex(data);
        case DisplayMode::MIXED:
            return FormatAsMixed(data);
        case DisplayMode::BINARY:
            // 未来扩展：二进制显示
            return FormatAsHex(data); // 暂时使用十六进制显示
        default:
            WriteDebugLog("[ERROR] DefaultDataDisplayFormatter::Format: 未知显示模式");
            return FormatAsText(data);
        }
    }
    catch (const std::exception& e) {
        CString errorMsg;
        errorMsg.Format(L"[ERROR] 数据格式化异常: %s", CA2W(e.what()));
        WriteDebugLog(CW2A(errorMsg));
        return L"[数据格式化错误]";
    }
}

CString DefaultDataDisplayFormatter::FormatAsText(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return L"";
    }
    
    // KISS原则：简单的文本转换
    std::string textData;
    textData.reserve(data.size());
    
    for (uint8_t byte : data) {
        if (byte >= 32 && byte <= 126) {
            // 可打印ASCII字符
            textData += static_cast<char>(byte);
        } else if (byte == '\r' || byte == '\n' || byte == '\t') {
            // 保留常见控制字符
            textData += static_cast<char>(byte);
        } else {
            // 不可打印字符用点号替代
            textData += '.';
        }
    }
    
    return CString(CA2W(textData.c_str(), CP_UTF8));
}

CString DefaultDataDisplayFormatter::FormatAsHex(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return L"";
    }
    
    std::wstringstream hexStream;
    const size_t BYTES_PER_LINE = 16;  // 每行16字节
    
    for (size_t i = 0; i < data.size(); ++i) {
        // 行首显示偏移地址
        if (i % BYTES_PER_LINE == 0) {
            if (i > 0) {
                hexStream << L"\r\n";
            }
            hexStream << std::hex << std::uppercase << std::setfill(L'0') 
                     << std::setw(8) << i << L": ";
        }
        
        // 显示十六进制数据
        hexStream << std::hex << std::uppercase << std::setfill(L'0') 
                 << std::setw(2) << static_cast<int>(data[i]) << L" ";
        
        // 每8字节添加额外空格便于阅读
        if ((i + 1) % 8 == 0 && (i + 1) % BYTES_PER_LINE != 0) {
            hexStream << L" ";
        }
    }
    
    return hexStream.str().c_str();
}

CString DefaultDataDisplayFormatter::FormatAsMixed(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return L"";
    }
    
    std::wstringstream mixedStream;
    const size_t BYTES_PER_LINE = 16;
    
    for (size_t lineStart = 0; lineStart < data.size(); lineStart += BYTES_PER_LINE) {
        size_t lineEnd = std::min(lineStart + BYTES_PER_LINE, data.size());
        
        // 行首偏移地址
        mixedStream << std::hex << std::uppercase << std::setfill(L'0') 
                   << std::setw(8) << lineStart << L": ";
        
        // 十六进制部分
        for (size_t i = lineStart; i < lineEnd; ++i) {
            mixedStream << std::hex << std::uppercase << std::setfill(L'0') 
                       << std::setw(2) << static_cast<int>(data[i]) << L" ";
            
            if ((i + 1 - lineStart) % 8 == 0 && (i + 1) < lineEnd) {
                mixedStream << L" ";
            }
        }
        
        // 填充对齐空格
        size_t bytesInLine = lineEnd - lineStart;
        size_t paddingSpaces = (BYTES_PER_LINE - bytesInLine) * 3;
        if (bytesInLine <= 8) {
            paddingSpaces += 1; // 额外的分组空格
        }
        mixedStream << std::wstring(paddingSpaces, L' ');
        
        // ASCII表示部分
        mixedStream << L" |";
        for (size_t i = lineStart; i < lineEnd; ++i) {
            uint8_t byte = data[i];
            if (byte >= 32 && byte <= 126) {
                mixedStream << static_cast<wchar_t>(byte);
            } else {
                mixedStream << L'.';
            }
        }
        mixedStream << L"|";
        
        if (lineEnd < data.size()) {
            mixedStream << L"\r\n";
        }
    }
    
    return mixedStream.str().c_str();
}

bool DefaultDataDisplayFormatter::IsValidUTF8Sequence(const std::vector<uint8_t>& data, size_t start, size_t& length)
{
    if (start >= data.size()) {
        return false;
    }
    
    uint8_t firstByte = data[start];
    
    // ASCII字符 (0xxxxxxx)
    if ((firstByte & 0x80) == 0) {
        length = 1;
        return true;
    }
    
    // 多字节UTF-8序列
    length = 0;
    if ((firstByte & 0xE0) == 0xC0) {
        length = 2;  // 110xxxxx 10xxxxxx
    } else if ((firstByte & 0xF0) == 0xE0) {
        length = 3;  // 1110xxxx 10xxxxxx 10xxxxxx
    } else if ((firstByte & 0xF8) == 0xF0) {
        length = 4;  // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    } else {
        return false; // 无效的UTF-8起始字节
    }
    
    // 检查是否有足够的字节
    if (start + length > data.size()) {
        return false;
    }
    
    // 验证后续字节都是10xxxxxx格式
    for (size_t i = 1; i < length; ++i) {
        if ((data[start + i] & 0xC0) != 0x80) {
            return false;
        }
    }
    
    return true;
}

// DataDisplayManager 实现

DataDisplayManager::DataDisplayManager(std::unique_ptr<IDataDisplayFormatter> formatter)
    : m_displayMode(DisplayMode::TEXT)
    , m_formatter(formatter ? std::move(formatter) : std::make_unique<DefaultDataDisplayFormatter>())
    , m_dataView(nullptr)
    , m_progressCtrl(nullptr)
    , m_statusLabel(nullptr)
    , m_maxDisplayBytes(1024 * 1024)  // 默认1MB限制
{
    WriteDebugLog("[DEBUG] DataDisplayManager构造完成");
}

void DataDisplayManager::SetDisplayControls(CEdit* dataView, CProgressCtrl* progressCtrl, CStatic* statusLabel)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    m_dataView = dataView;
    m_progressCtrl = progressCtrl;
    m_statusLabel = statusLabel;
    
    WriteDebugLog("[DEBUG] DataDisplayManager::SetDisplayControls: 控件设置完成");
}

void DataDisplayManager::SetDisplayMode(DisplayMode mode)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (m_displayMode != mode) {
        m_displayMode = mode;
        WriteDebugLog("[DEBUG] DataDisplayManager::SetDisplayMode: 显示模式已更新");
        
        // 模式改变时刷新显示
        RefreshDisplay();
    }
}

DisplayMode DataDisplayManager::GetDisplayMode() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_displayMode;
}

void DataDisplayManager::UpdateDisplay(const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    // 替换当前数据
    m_displayedData = data;
    
    // 应用显示限制
    ApplyDisplayLimit();
    
    // 刷新显示
    RefreshDisplay();
    
    WriteDebugLog("[DEBUG] DataDisplayManager::UpdateDisplay: 显示数据已更新");
}

void DataDisplayManager::AppendDisplay(const std::vector<uint8_t>& data)
{
    if (data.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    // 追加新数据
    m_displayedData.insert(m_displayedData.end(), data.begin(), data.end());
    
    // 应用显示限制
    ApplyDisplayLimit();
    
    // 刷新显示
    RefreshDisplay();
    
    WriteDebugLog("[DEBUG] DataDisplayManager::AppendDisplay: 数据已追加");
}

void DataDisplayManager::ClearDisplay()
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    m_displayedData.clear();
    
    if (m_dataView && IsWindow(m_dataView->GetSafeHwnd())) {
        m_dataView->SetWindowText(L"");
    }
    
    UpdateStatusInfo();
    
    WriteDebugLog("[DEBUG] DataDisplayManager::ClearDisplay: 显示已清空");
}

std::vector<uint8_t> DataDisplayManager::GetDisplayedData() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_displayedData;  // 返回副本
}

CString DataDisplayManager::GetFormattedText() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (!m_formatter) {
        return L"";
    }
    
    return m_formatter->Format(m_displayedData, m_displayMode);
}

void DataDisplayManager::ScrollToBottom()
{
    if (!m_dataView || !IsWindow(m_dataView->GetSafeHwnd())) {
        return;
    }
    
    // 滚动到底部
    int lineCount = m_dataView->GetLineCount();
    if (lineCount > 0) {
        m_dataView->LineScroll(lineCount);
    }
}

void DataDisplayManager::SetMaxDisplayBytes(size_t maxBytes)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    m_maxDisplayBytes = maxBytes;
    
    // 立即应用新的限制
    if (maxBytes > 0 && m_displayedData.size() > maxBytes) {
        ApplyDisplayLimit();
        RefreshDisplay();
    }
    
    WriteDebugLog("[DEBUG] DataDisplayManager::SetMaxDisplayBytes: 显示字节限制已更新");
}

size_t DataDisplayManager::GetDataSize() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_displayedData.size();
}

void DataDisplayManager::RefreshDisplay()
{
    // 注意：此方法假设调用者已持有互斥锁
    
    if (!m_dataView || !IsWindow(m_dataView->GetSafeHwnd())) {
        return;
    }
    
    if (!m_formatter) {
        WriteDebugLog("[ERROR] DataDisplayManager::RefreshDisplay: 格式化器未设置");
        return;
    }
    
    try {
        // 暂停重绘以减少闪烁
        m_dataView->SetRedraw(FALSE);
        
        // 格式化并设置显示内容
        CString formattedText = m_formatter->Format(m_displayedData, m_displayMode);
        m_dataView->SetWindowText(formattedText);
        
        // 恢复重绘
        m_dataView->SetRedraw(TRUE);
        m_dataView->Invalidate();
        
        // 更新状态信息
        UpdateStatusInfo();
        
        WriteDebugLog("[DEBUG] DataDisplayManager::RefreshDisplay: 显示刷新完成");
    }
    catch (const std::exception& e) {
        m_dataView->SetRedraw(TRUE);  // 确保重绘状态恢复
        
        CString errorMsg;
        errorMsg.Format(L"[ERROR] 刷新显示异常: %s", CA2W(e.what()));
        WriteDebugLog(CW2A(errorMsg));
    }
}

void DataDisplayManager::UpdateStatusInfo()
{
    // 注意：此方法假设调用者已持有互斥锁
    
    if (!m_statusLabel || !IsWindow(m_statusLabel->GetSafeHwnd())) {
        return;
    }
    
    CString statusText;
    statusText.Format(L"数据大小: %zu 字节", m_displayedData.size());
    
    if (m_maxDisplayBytes > 0 && m_displayedData.size() > m_maxDisplayBytes) {
        statusText += L" (已限制显示)";
    }
    
    m_statusLabel->SetWindowText(statusText);
}

void DataDisplayManager::ApplyDisplayLimit()
{
    // 注意：此方法假设调用者已持有互斥锁
    
    if (m_maxDisplayBytes == 0 || m_displayedData.size() <= m_maxDisplayBytes) {
        return;  // 无需限制
    }
    
    // 保留最新的数据
    std::vector<uint8_t> limitedData;
    limitedData.assign(
        m_displayedData.end() - m_maxDisplayBytes, 
        m_displayedData.end()
    );
    
    m_displayedData = std::move(limitedData);
    
    WriteDebugLog("[DEBUG] DataDisplayManager::ApplyDisplayLimit: 应用了显示字节限制");
}

// DataDisplayManagerFactory 实现

std::unique_ptr<DataDisplayManager> DataDisplayManagerFactory::CreateDefault()
{
    return std::make_unique<DataDisplayManager>();
}

std::unique_ptr<DataDisplayManager> DataDisplayManagerFactory::CreateWithFormatter(
    std::unique_ptr<IDataDisplayFormatter> formatter)
{
    return std::make_unique<DataDisplayManager>(std::move(formatter));
}