#pragma execution_character_set("utf-8")
#include "pch.h"
#include "FileOperationManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shellapi.h>

extern void WriteDebugLog(const char* message);

// =====================================================================================
// FileOperationManager 实现 - 文件操作管理器
// =====================================================================================

bool FileOperationManager::LoadFile(const std::wstring& filePath, std::vector<uint8_t>& data)
{
    try
    {
        if (!ValidatePath(filePath))
            return false;
        
        size_t fileSize = GetFileSize(filePath);
        if (fileSize == 0 || fileSize > MAX_FILE_SIZE)
            return false;
        
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
            return false;
        
        data.resize(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        file.close();
        
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool FileOperationManager::SaveFile(const std::wstring& filePath, const std::vector<uint8_t>& data)
{
    try
    {
        if (!ValidatePath(filePath))
            return false;
        
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open())
            return false;
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool FileOperationManager::ValidatePath(const std::wstring& filePath)
{
    if (filePath.empty() || filePath.length() > 260)
        return false;
    
    return true;
}

size_t FileOperationManager::GetFileSize(const std::wstring& filePath)
{
    try
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return 0;
        
        size_t size = static_cast<size_t>(file.tellg());
        file.close();
        return size;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

// =====================================================================================
// 文件拖放处理逻辑 (从PortMasterDlg转移)
// =====================================================================================

bool FileOperationManager::HandleDropFiles(void* hDropInfo, std::vector<std::wstring>& filePaths)
{
    HDROP hDrop = static_cast<HDROP>(hDropInfo);
    if (!hDrop)
        return false;
    
    try
    {
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        if (fileCount == 0)
            return false;
        
        filePaths.clear();
        filePaths.reserve(fileCount);
        
        for (UINT i = 0; i < fileCount; ++i)
        {
            UINT pathLength = DragQueryFileW(hDrop, i, nullptr, 0);
            if (pathLength > 0)
            {
                std::wstring filePath(pathLength, L'\0');
                DragQueryFileW(hDrop, i, &filePath[0], pathLength + 1);
                filePath.resize(pathLength); // 移除多余的null字符
                
                if (!filePath.empty())
                {
                    filePaths.push_back(filePath);
                }
            }
        }
        
        return !filePaths.empty();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool FileOperationManager::LoadFileForTransmission(const std::wstring& filePath, std::vector<uint8_t>& data, std::wstring& displayInfo)
{
    try
    {
        // 验证文件
        std::wstring errorMessage;
        if (!IsFileValidForTransmission(filePath, errorMessage))
        {
            displayInfo = L"文件验证失败: " + errorMessage;
            return false;
        }
        
        // 加载文件数据
        if (!LoadFile(filePath, data))
        {
            displayInfo = L"文件加载失败";
            return false;
        }
        
        // 生成显示信息
        size_t fileSize = data.size();
        std::wstring fileName = filePath.substr(filePath.find_last_of(L"\\") + 1);
        std::wstring sizeStr = FormatFileSize(fileSize);
        
        displayInfo = fileName + L" (" + sizeStr + L")";
        
        // 转换为UTF-8字符串用于日志记录
        std::string fileNameUtf8;
        std::string sizeStrUtf8;
        
        // 简单的宽字符到多字节字符转换
        fileNameUtf8.reserve(fileName.length() * 2);
        for (wchar_t wc : fileName) {
            if (wc < 128) {
                fileNameUtf8.push_back(static_cast<char>(wc));
            } else {
                fileNameUtf8.push_back('?'); // 非ASCII字符用?替代
            }
        }
        
        sizeStrUtf8.reserve(sizeStr.length() * 2);
        for (wchar_t wc : sizeStr) {
            if (wc < 128) {
                sizeStrUtf8.push_back(static_cast<char>(wc));
            } else {
                sizeStrUtf8.push_back('?'); // 非ASCII字符用?替代
            }
        }
        
        WriteDebugLog(("文件加载成功: " + fileNameUtf8 + ", 大小: " + sizeStrUtf8).c_str());
        
        return true;
    }
    catch (const std::exception&)
    {
        displayInfo = L"文件处理异常";
        return false;
    }
}

bool FileOperationManager::IsFileValidForTransmission(const std::wstring& filePath, std::wstring& errorMessage)
{
    // 基本路径验证
    if (!ValidatePath(filePath))
    {
        errorMessage = L"文件路径无效";
        return false;
    }
    
    // 检查文件是否存在
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        errorMessage = L"文件不存在或无法访问";
        return false;
    }
    file.close();
    
    // 检查文件大小
    size_t fileSize = GetFileSize(filePath);
    if (fileSize == 0)
    {
        errorMessage = L"文件为空";
        return false;
    }
    
    if (fileSize > MAX_FILE_SIZE)
    {
        errorMessage = L"文件过大 (最大支持" + FormatFileSize(MAX_FILE_SIZE) + L")";
        return false;
    }
    
    // 检查文件类型
    std::wstring extension = GetFileExtension(filePath);
    if (!IsSupportedFileType(extension))
    {
        errorMessage = L"不支持的文件类型: " + extension;
        return false;
    }
    
    return true;
}

// =====================================================================================
// 内部辅助方法实现
// =====================================================================================

std::wstring FileOperationManager::FormatFileSize(size_t bytes)
{
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB" };
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 3)
    {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(unitIndex == 0 ? 0 : 2) << size << units[unitIndex];
    return oss.str();
}

std::wstring FileOperationManager::GetFileExtension(const std::wstring& filePath)
{
    size_t dotPos = filePath.find_last_of(L'.');
    if (dotPos == std::wstring::npos || dotPos == filePath.length() - 1)
        return L"";
    
    std::wstring extension = filePath.substr(dotPos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    return extension;
}

bool FileOperationManager::IsSupportedFileType(const std::wstring& extension)
{
    // 支持的文件类型列表
    static const std::vector<std::wstring> supportedTypes = {
        L"txt", L"bin", L"dat", L"hex", L"log",
        L"cfg", L"ini", L"xml", L"json", L"csv",
        L"bmp", L"jpg", L"jpeg", L"png", L"gif",
        L"doc", L"docx", L"pdf", L"xls", L"xlsx",
        L"zip", L"rar", L"7z", L"tar", L"gz"
    };
    
    return std::find(supportedTypes.begin(), supportedTypes.end(), extension) != supportedTypes.end();
}