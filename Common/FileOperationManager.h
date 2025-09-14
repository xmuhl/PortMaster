#pragma execution_character_set("utf-8")
#pragma once

#include <vector>
#include <string>

/**
 * 文件操作管理器 - 专职处理文件相关操作
 * SOLID-S: 单一职责 - 专注文件操作管理
 */
class FileOperationManager
{
public:
    FileOperationManager() = default;
    ~FileOperationManager() = default;

    /**
     * 加载文件数据
     */
    bool LoadFile(const std::wstring& filePath, std::vector<uint8_t>& data);
    
    /**
     * 保存文件数据
     */
    bool SaveFile(const std::wstring& filePath, const std::vector<uint8_t>& data);
    
    /**
     * 验证文件路径
     */
    bool ValidatePath(const std::wstring& filePath);
    
    /**
     * 获取文件大小
     */
    size_t GetFileSize(const std::wstring& filePath);
    
    /**
     * 处理文件拖放操作 (从PortMasterDlg转移)
     */
    bool HandleDropFiles(void* hDropInfo, std::vector<std::wstring>& filePaths);
    
    /**
     * 加载文件用于传输 (从PortMasterDlg转移)
     */
    bool LoadFileForTransmission(const std::wstring& filePath, std::vector<uint8_t>& data, std::wstring& displayInfo);
    
    /**
     * 验证文件是否适合传输
     */
    bool IsFileValidForTransmission(const std::wstring& filePath, std::wstring& errorMessage);

private:
    static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024; // 100MB
    
    // 内部辅助方法
    std::wstring FormatFileSize(size_t bytes);
    std::wstring GetFileExtension(const std::wstring& filePath);
    bool IsSupportedFileType(const std::wstring& extension);
};