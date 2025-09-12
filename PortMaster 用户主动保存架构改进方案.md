🚀 PortMaster 用户主动保存架构改进方案

基于用户反馈和深入的代码分析，本文档提出了一个全新的文件保存架构设计，将自动保存模式改进为用户主动控制的保存模式，提升用户体验和系统安全性。

## 📋 问题分析总结

### 当前自动保存机制的问题

**1. 双重保存路径导致文件重复**
- **位置1**: `DeliverOrderedFrames()` 函数 (ReliableChannel.cpp:974)
- **位置2**: `CompleteReceive()` 函数 (ReliableChannel.cpp:627)
- **结果**: 生成 "文件名.后缀" 和 "文件名(1).后缀" 两个文件

**2. 用户缺乏保存控制权**
- 文件自动保存到固定目录
- 用户无法选择保存位置和文件名
- 无法预览数据内容再决定是否保存
- 大文件传输时占用存储空间

**3. 安全性和可用性问题**
- 敏感数据可能被意外保存
- 临时测试数据污染文件系统
- 无法支持断点续传等高级功能
- 缺乏数据完整性验证机制

## 🎯 用户主动保存架构设计

### 核心设计理念

**用户完全控制**: 只有用户主动点击保存时才真正写入文件系统
**临时缓存**: 接收的数据保存在内存缓冲区或临时缓存中
**安全性优先**: 支持数据预览、验证和选择性保存
**扩展性**: 为断点续传、数据压缩等高级功能预留接口

### 架构组件设计

#### 1. 临时数据管理器 (TempDataManager)

```cpp
class TempDataManager {
public:
    // 数据缓存管理
    bool CacheReceivedData(const std::string& sessionId, 
                          const std::vector<uint8_t>& data,
                          const FileMetadata& metadata);
    
    // 获取缓存数据
    std::shared_ptr<CachedFileData> GetCachedData(const std::string& sessionId);
    
    // 数据预览
    DataPreview GeneratePreview(const std::string& sessionId, 
                               PreviewType type = PreviewType::AUTO);
    
    // 缓存清理
    void ClearCache(const std::string& sessionId);
    void ClearExpiredCache(std::chrono::minutes maxAge = std::chrono::minutes(30));
    
    // 内存管理
    size_t GetCacheSize() const;
    void SetMaxCacheSize(size_t maxSize);
    
private:
    struct CachedFileData {
        std::vector<uint8_t> data;
        FileMetadata metadata;
        std::chrono::steady_clock::time_point timestamp;
        bool isComplete;
        std::string checksum;  // 数据完整性校验
    };
    
    std::unordered_map<std::string, std::shared_ptr<CachedFileData>> m_cache;
    std::mutex m_cacheMutex;
    size_t m_maxCacheSize = 100 * 1024 * 1024; // 100MB默认限制
};
```

#### 2. 用户保存控制器 (UserSaveController)

```cpp
class UserSaveController {
public:
    // 用户保存接口
    bool ShowSaveDialog(const std::string& sessionId);
    bool SaveToFile(const std::string& sessionId, 
                   const std::string& filePath,
                   SaveOptions options = SaveOptions::DEFAULT);
    
    // 数据预览
    bool ShowDataPreview(const std::string& sessionId);
    
    // 批量保存
    bool SaveMultipleFiles(const std::vector<std::string>& sessionIds);
    
    // 保存历史
    std::vector<SaveRecord> GetSaveHistory() const;
    
private:
    TempDataManager* m_tempDataManager;
    std::vector<SaveRecord> m_saveHistory;
};
```

#### 3. 改进的可靠传输通道 (ImprovedReliableChannel)

```cpp
class ImprovedReliableChannel : public ReliableChannel {
public:
    // 设置临时数据管理器
    void SetTempDataManager(std::shared_ptr<TempDataManager> manager);
    
    // 禁用自动保存
    void DisableAutoSave() { m_autoSaveEnabled = false; }
    
    // 数据接收完成回调（不自动保存）
    void OnDataReceiveComplete(const std::string& filename, 
                              const std::vector<uint8_t>& data) override;
    
private:
    std::shared_ptr<TempDataManager> m_tempDataManager;
    bool m_autoSaveEnabled = false;  // 默认禁用自动保存
    std::string m_currentSessionId;
    
    // 移除自动保存相关方法
    // bool SaveReceivedFile();  // 删除或重构为缓存方法
};
```

### UI改进设计

#### 1. 新增控件定义

```cpp
// Resource.h 新增控件ID
#define IDC_SAVE_RECEIVED_BUTTON    1079  // 保存接收数据按钮
#define IDC_PREVIEW_DATA_BUTTON     1080  // 预览数据按钮
#define IDC_CLEAR_CACHE_BUTTON      1081  // 清理缓存按钮
#define IDC_CACHE_STATUS_LABEL      1082  // 缓存状态标签
#define IDC_RECEIVED_FILES_LIST     1083  // 接收文件列表
```

#### 2. UI布局改进

**接收数据管理区域**:
- 接收文件列表控件：显示当前缓存的文件
- 预览按钮：预览选中文件的内容
- 保存按钮：保存选中文件到指定位置
- 清理缓存按钮：清理临时缓存
- 缓存状态标签：显示当前缓存使用情况

#### 3. 用户交互流程

```
文件传输完成 → 数据缓存到内存 → 通知用户接收完成
                     ↓
用户查看文件列表 → 选择文件预览 → 决定是否保存
                     ↓
点击保存按钮 → 选择保存位置 → 确认保存 → 文件写入磁盘
```

## 🔧 具体实施方案

### 阶段一：核心架构重构

#### 1.1 修改ReliableChannel

**移除自动保存逻辑**:
```cpp
// 在 DeliverOrderedFrames() 中移除自动保存
void ReliableChannel::DeliverOrderedFrames() {
    // ... 现有逻辑 ...
    
    if (!entry.isData) {
        // END帧：完成接收，但不自动保存
        SetState(RELIABLE_DONE);
        
        // 🔧 新增：缓存数据而非直接保存
        if (m_tempDataManager) {
            FileMetadata metadata;
            metadata.filename = m_receivedFilename;
            metadata.fileSize = m_receivedData.size();
            metadata.timestamp = std::chrono::system_clock::now();
            
            m_tempDataManager->CacheReceivedData(
                m_currentSessionId, m_receivedData, metadata);
        }
        
        // 通知UI数据接收完成（不是文件保存完成）
        if (m_dataReceivedCallback) {
            m_dataReceivedCallback(m_currentSessionId, m_receivedFilename, m_receivedData);
        }
        
        // 移除原有的 SaveReceivedFile() 调用
    }
}
```

**移除CompleteReceive中的保存逻辑**:
```cpp
void ReliableChannel::CompleteReceive() {
    // 验证数据完整性
    if (m_receivedData.size() != static_cast<size_t>(m_receiveMetadata.fileSize)) {
        AbortReceive("接收数据大小不匹配");
        return;
    }
    
    // 🔧 修改：缓存数据而非保存文件
    if (m_tempDataManager) {
        FileMetadata metadata;
        metadata.filename = m_receivedFilename;
        metadata.fileSize = m_receivedData.size();
        metadata.checksum = CalculateChecksum(m_receivedData);
        
        if (m_tempDataManager->CacheReceivedData(
                m_currentSessionId, m_receivedData, metadata)) {
            SetState(RELIABLE_DONE);
            NotifyCompletion(true, "数据接收完成，已缓存: " + m_receivedFilename);
        } else {
            AbortReceive("数据缓存失败");
        }
    } else {
        AbortReceive("临时数据管理器未初始化");
    }
}
```

#### 1.2 实现TempDataManager

```cpp
// Common/TempDataManager.h
#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <string>

struct FileMetadata {
    std::string filename;
    size_t fileSize;
    std::chrono::system_clock::time_point timestamp;
    std::string checksum;
    std::string mimeType;
};

struct DataPreview {
    std::string textPreview;     // 文本预览（前1KB）
    std::string hexPreview;      // 十六进制预览
    bool isBinary;
    bool isImage;
    bool isText;
};

class TempDataManager {
    // ... 如前所述的完整实现
};
```

#### 1.3 UI控件集成

**PortMasterDlg.h 新增成员**:
```cpp
private:
    // 新增：用户主动保存相关控件
    CListCtrl m_ctrlReceivedFilesList;    // 接收文件列表
    CButton m_ctrlSaveReceivedBtn;        // 保存接收数据按钮
    CButton m_ctrlPreviewDataBtn;         // 预览数据按钮
    CButton m_ctrlClearCacheBtn;          // 清理缓存按钮
    CStatic m_ctrlCacheStatusLabel;       // 缓存状态标签
    
    // 新增：数据管理组件
    std::shared_ptr<TempDataManager> m_tempDataManager;
    std::unique_ptr<UserSaveController> m_saveController;
    
    // 新增：消息处理函数
    afx_msg void OnBnClickedSaveReceived();
    afx_msg void OnBnClickedPreviewData();
    afx_msg void OnBnClickedClearCache();
    afx_msg void OnLvnItemchangedReceivedFilesList(NMHDR *pNMHDR, LRESULT *pResult);
```

### 阶段二：高级功能实现

#### 2.1 数据预览功能

```cpp
void CPortMasterDlg::OnBnClickedPreviewData() {
    int selectedIndex = m_ctrlReceivedFilesList.GetNextItem(-1, LVNI_SELECTED);
    if (selectedIndex == -1) {
        ShowUserMessage(L"请选择要预览的文件", L"预览提示", MB_ICONINFORMATION);
        return;
    }
    
    CString sessionId = m_ctrlReceivedFilesList.GetItemText(selectedIndex, 0);
    
    auto cachedData = m_tempDataManager->GetCachedData(CT2A(sessionId));
    if (!cachedData) {
        ShowUserMessage(L"数据已过期或不存在", L"预览错误", MB_ICONERROR);
        return;
    }
    
    // 显示预览对话框
    DataPreviewDialog previewDlg(cachedData, this);
    previewDlg.DoModal();
}
```

#### 2.2 智能保存对话框

```cpp
void CPortMasterDlg::OnBnClickedSaveReceived() {
    int selectedIndex = m_ctrlReceivedFilesList.GetNextItem(-1, LVNI_SELECTED);
    if (selectedIndex == -1) {
        ShowUserMessage(L"请选择要保存的文件", L"保存提示", MB_ICONINFORMATION);
        return;
    }
    
    CString sessionId = m_ctrlReceivedFilesList.GetItemText(selectedIndex, 0);
    auto cachedData = m_tempDataManager->GetCachedData(CT2A(sessionId));
    
    if (!cachedData) {
        ShowUserMessage(L"数据已过期或不存在", L"保存错误", MB_ICONERROR);
        return;
    }
    
    // 智能文件保存对话框
    CString defaultName = CA2W(cachedData->metadata.filename.c_str());
    CString filter = GenerateFileFilter(cachedData->metadata.mimeType);
    
    CFileDialog saveDlg(FALSE, nullptr, defaultName,
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        filter);
    
    if (saveDlg.DoModal() == IDOK) {
        CString filePath = saveDlg.GetPathName();
        
        if (m_saveController->SaveToFile(CT2A(sessionId), CT2A(filePath))) {
            // 保存成功，从缓存中移除
            m_tempDataManager->ClearCache(CT2A(sessionId));
            UpdateReceivedFilesList();
            
            CString msg;
            msg.Format(L"文件保存成功: %s", filePath);
            AppendLog(msg);
            ShowUserMessage(L"保存成功", msg, MB_ICONINFORMATION);
        } else {
            ShowUserMessage(L"保存失败", L"无法写入文件", MB_ICONERROR);
        }
    }
}
```

### 阶段三：安全性和性能优化

#### 3.1 内存管理优化

```cpp
// 大文件支持：分块缓存
class ChunkedTempDataManager : public TempDataManager {
public:
    bool CacheReceivedDataChunked(const std::string& sessionId,
                                 const std::vector<uint8_t>& chunk,
                                 size_t chunkIndex,
                                 size_t totalChunks);
    
    // 临时文件支持（超大文件）
    bool EnableTempFileMode(size_t thresholdSize = 50 * 1024 * 1024); // 50MB阈值
    
private:
    std::string m_tempDirectory;
    size_t m_tempFileThreshold;
};
```

#### 3.2 数据完整性验证

```cpp
// 数据校验和计算
std::string TempDataManager::CalculateChecksum(const std::vector<uint8_t>& data) {
    // 使用CRC32或SHA256计算校验和
    CRC32 crc;
    crc.Update(data.data(), data.size());
    return crc.GetHexString();
}

// 保存时验证数据完整性
bool UserSaveController::SaveToFile(const std::string& sessionId, 
                                   const std::string& filePath,
                                   SaveOptions options) {
    auto cachedData = m_tempDataManager->GetCachedData(sessionId);
    if (!cachedData) return false;
    
    // 验证数据完整性
    std::string currentChecksum = m_tempDataManager->CalculateChecksum(cachedData->data);
    if (currentChecksum != cachedData->metadata.checksum) {
        // 数据损坏警告
        if (options.verifyIntegrity) {
            return false;
        }
    }
    
    // 写入文件
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.write(reinterpret_cast<const char*>(cachedData->data.data()), 
               cachedData->data.size());
    
    return file.good();
}
```

## 📈 预期效果和优势

### 用户体验改进

✅ **完全的保存控制权**: 用户决定何时、何地、如何保存文件
✅ **数据预览功能**: 保存前可以预览文件内容
✅ **智能文件命名**: 根据文件类型自动建议合适的扩展名
✅ **批量操作支持**: 可以同时管理多个接收的文件
✅ **保存历史记录**: 跟踪保存操作，便于管理

### 系统安全性提升

🔒 **敏感数据保护**: 临时数据不会意外泄露到文件系统
🔒 **存储空间控制**: 避免自动生成不需要的文件
🔒 **数据完整性验证**: 保存前验证数据完整性
🔒 **访问权限控制**: 用户明确授权才能写入文件系统

### 技术架构优势

🏗️ **模块化设计**: 清晰的职责分离，易于维护和扩展
🏗️ **内存效率**: 智能缓存管理，支持大文件处理
🏗️ **扩展性**: 为断点续传、压缩等功能预留接口
🏗️ **错误处理**: 完善的异常处理和恢复机制

### 高级功能支持

🚀 **断点续传**: 基于缓存的续传机制
🚀 **数据压缩**: 可选的数据压缩存储
🚀 **多格式支持**: 智能识别文件类型
🚀 **云存储集成**: 预留云存储API接口

## 🚀 实施计划和时间安排

### 第一阶段：核心重构 (1-2周)

**Week 1**:
- [ ] 实现TempDataManager基础功能
- [ ] 修改ReliableChannel移除自动保存
- [ ] 基础UI控件集成
- [ ] 单元测试编写

**Week 2**:
- [ ] 用户保存对话框实现
- [ ] 数据预览功能开发
- [ ] 缓存管理功能完善
- [ ] 集成测试和调试

### 第二阶段：功能完善 (1-2周)

**Week 3**:
- [ ] 数据完整性验证
- [ ] 大文件支持优化
- [ ] 错误处理完善
- [ ] 性能优化

**Week 4**:
- [ ] 高级功能实现（批量保存、历史记录）
- [ ] 用户界面优化
- [ ] 全面测试和文档更新
- [ ] 部署和用户培训

### 第三阶段：高级特性 (可选)

- [ ] 断点续传功能
- [ ] 数据压缩支持
- [ ] 云存储集成
- [ ] 多语言支持

## ⚠️ 风险评估和缓解策略

### 技术风险

**风险1**: 内存使用量增加
- **缓解**: 实现智能缓存清理和临时文件支持
- **监控**: 添加内存使用监控和告警

**风险2**: 数据丢失风险
- **缓解**: 实现自动备份和恢复机制
- **监控**: 数据完整性校验和日志记录

**风险3**: 用户操作复杂度增加
- **缓解**: 设计直观的用户界面和操作指南
- **培训**: 提供用户培训和帮助文档

### 兼容性风险

**风险4**: 现有功能回归
- **缓解**: 保持向后兼容，提供传统模式选项
- **测试**: 全面的回归测试覆盖

**风险5**: 第三方集成问题
- **缓解**: 保持现有API接口不变
- **验证**: 与现有系统集成测试

## 📝 总结

本改进方案将PortMaster从自动保存模式升级为用户主动控制的保存模式，不仅解决了当前的文件重复生成问题，更重要的是提升了用户体验、系统安全性和功能扩展性。

通过引入临时数据管理、用户保存控制和智能预览功能，用户将获得完全的文件保存控制权，同时系统架构也变得更加模块化和可维护。

这个方案不仅是对当前问题的修复，更是对整个文件管理架构的重新设计，为未来的功能扩展奠定了坚实的基础。

---

**文档版本**: v1.0  
**创建日期**: 2025年1月12日  
**最后更新**: 2025年1月12日  
**作者**: Claude AI Assistant  
**审核状态**: 待审核