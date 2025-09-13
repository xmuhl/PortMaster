#pragma execution_character_set("utf-8")
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <afxwin.h>  // For CString

// 前置声明
class CEdit;
class CProgressCtrl;
class CStatic;

/**
 * @brief 数据显示模式枚举
 * SOLID-O: 开放封闭原则 - 易于扩展新的显示模式
 */
enum class DisplayMode
{
    TEXT = 0,        // 纯文本显示
    HEX,             // 十六进制显示  
    MIXED,           // 智能混合显示（文本+十六进制）
    BINARY           // 二进制显示（未来扩展）
};

/**
 * @brief 数据显示格式化接口
 * SOLID-I: 接口隔离原则 - 单一的格式化职责
 */
class IDataDisplayFormatter
{
public:
    virtual ~IDataDisplayFormatter() = default;
    
    /**
     * @brief 格式化数据为显示字符串
     * @param data 原始数据
     * @param mode 显示模式
     * @return 格式化后的显示字符串
     */
    virtual CString Format(const std::vector<uint8_t>& data, DisplayMode mode) = 0;
};

/**
 * @brief 默认数据显示格式化器实现
 * SOLID-S: 单一职责原则 - 专注于数据格式化
 */
class DefaultDataDisplayFormatter : public IDataDisplayFormatter
{
public:
    CString Format(const std::vector<uint8_t>& data, DisplayMode mode) override;

private:
    // 格式化方法 - KISS原则：简单直观的实现
    CString FormatAsText(const std::vector<uint8_t>& data);
    CString FormatAsHex(const std::vector<uint8_t>& data);
    CString FormatAsMixed(const std::vector<uint8_t>& data);
    
    // UTF-8编码检测辅助方法
    bool IsValidUTF8Sequence(const std::vector<uint8_t>& data, size_t start, size_t& length);
};

/**
 * @brief 统一数据显示管理器
 * SOLID-S: 单一职责原则 - 专门负责数据显示管理
 * SOLID-D: 依赖倒置原则 - 依赖于IDataDisplayFormatter抽象
 */
class DataDisplayManager
{
public:
    /**
     * @brief 构造函数
     * @param formatter 数据格式化器（依赖注入）
     */
    explicit DataDisplayManager(std::unique_ptr<IDataDisplayFormatter> formatter = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~DataDisplayManager() = default;

    // 禁用拷贝构造和拷贝赋值 - RAII原则
    DataDisplayManager(const DataDisplayManager&) = delete;
    DataDisplayManager& operator=(const DataDisplayManager&) = delete;
    
    // 允许移动构造和移动赋值
    DataDisplayManager(DataDisplayManager&&) = default;
    DataDisplayManager& operator=(DataDisplayManager&&) = default;

    /**
     * @brief 设置显示控件
     * @param dataView 数据显示控件
     * @param progressCtrl 进度条控件（可选）
     * @param statusLabel 状态标签控件（可选）
     */
    void SetDisplayControls(CEdit* dataView, CProgressCtrl* progressCtrl = nullptr, CStatic* statusLabel = nullptr);
    
    /**
     * @brief 设置显示模式
     * @param mode 显示模式
     */
    void SetDisplayMode(DisplayMode mode);
    
    /**
     * @brief 获取当前显示模式
     * @return 当前显示模式
     */
    DisplayMode GetDisplayMode() const;
    
    /**
     * @brief 更新显示数据（替换模式）
     * @param data 新的显示数据
     */
    void UpdateDisplay(const std::vector<uint8_t>& data);
    
    /**
     * @brief 追加显示数据（追加模式）
     * @param data 要追加的数据
     */
    void AppendDisplay(const std::vector<uint8_t>& data);
    
    /**
     * @brief 清空显示
     */
    void ClearDisplay();
    
    /**
     * @brief 获取当前显示的数据
     * @return 当前显示数据的副本
     */
    std::vector<uint8_t> GetDisplayedData() const;
    
    /**
     * @brief 获取格式化后的显示文本
     * @return 当前显示的文本内容
     */
    CString GetFormattedText() const;
    
    /**
     * @brief 滚动到底部
     */
    void ScrollToBottom();
    
    /**
     * @brief 设置最大显示字节数限制
     * @param maxBytes 最大字节数（0表示无限制）
     */
    void SetMaxDisplayBytes(size_t maxBytes);
    
    /**
     * @brief 获取当前数据大小
     * @return 当前显示数据的字节数
     */
    size_t GetDataSize() const;

private:
    // 核心数据成员
    std::vector<uint8_t> m_displayedData;           // 当前显示的数据
    DisplayMode m_displayMode;                       // 当前显示模式
    std::unique_ptr<IDataDisplayFormatter> m_formatter; // 数据格式化器
    
    // UI控件指针
    CEdit* m_dataView;                              // 数据显示控件
    CProgressCtrl* m_progressCtrl;                  // 进度条控件
    CStatic* m_statusLabel;                         // 状态标签控件
    
    // 配置参数
    size_t m_maxDisplayBytes;                       // 最大显示字节数限制
    
    // 线程安全保护
    mutable std::mutex m_dataMutex;                 // 数据访问互斥锁
    
    // 内部方法
    void RefreshDisplay();                          // 刷新显示内容
    void UpdateStatusInfo();                        // 更新状态信息
    void ApplyDisplayLimit();                       // 应用显示字节数限制
};

/**
 * @brief 数据显示管理器工厂类
 * SOLID-O: 开放封闭原则 - 支持扩展不同的管理器类型
 */
class DataDisplayManagerFactory
{
public:
    /**
     * @brief 创建默认的数据显示管理器
     * @return 数据显示管理器实例
     */
    static std::unique_ptr<DataDisplayManager> CreateDefault();
    
    /**
     * @brief 创建带自定义格式化器的数据显示管理器
     * @param formatter 自定义格式化器
     * @return 数据显示管理器实例
     */
    static std::unique_ptr<DataDisplayManager> CreateWithFormatter(
        std::unique_ptr<IDataDisplayFormatter> formatter);
};