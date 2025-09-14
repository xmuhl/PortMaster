#pragma execution_character_set("utf-8")
#pragma once

#include "DataDisplayManager.h"
#include "StateManager.h"
#include "TransportManager.h"
#include "FileOperationManager.h"
#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <afxwin.h>  // For CString and MFC classes

// 前置声明
class CEdit;
class CProgressCtrl;
class CStatic;
class CPortMasterDlg;

/**
 * @brief 主对话框UI状态更新实现
 * 实现StateManager的IUIStateUpdater接口
 */
class PortMasterUIStateUpdater : public IUIStateUpdater
{
public:
    explicit PortMasterUIStateUpdater(CPortMasterDlg* dialog);
    virtual ~PortMasterUIStateUpdater() = default;
    
    // IUIStateUpdater接口实现
    void UpdateConnectionStatus(bool connected, const std::string& info = "") override;
    void UpdateTransmissionStatus(ApplicationState state, double progress = 0.0) override;
    void UpdateButtonStates(ApplicationState state) override;
    void UpdateStatusBar(const std::string& message, StatePriority priority = StatePriority::NORMAL) override;
    void ShowErrorMessage(const std::string& title, const std::string& message) override;

private:
    CPortMasterDlg* m_dialog;  // 主对话框指针
};

/**
 * @brief 状态变化回调实现
 * 实现StateManager的IStateChangeCallback接口
 */
class PortMasterStateCallback : public IStateChangeCallback
{
public:
    explicit PortMasterStateCallback(CPortMasterDlg* dialog);
    virtual ~PortMasterStateCallback() = default;
    
    // IStateChangeCallback接口实现
    void OnStateChanged(const StateInfo& oldState, const StateInfo& newState) override;
    void OnStateUpdate(const StateInfo& stateInfo) override;
    void OnErrorState(const StateInfo& errorState) override;

private:
    CPortMasterDlg* m_dialog;  // 主对话框指针
};

/**
 * @brief 管理器集成器
 * 统一管理所有专职管理器的集成接口
 * SOLID-S: 单一职责 - 专门负责管理器的创建和集成
 */
class ManagerIntegration
{
public:
    /**
     * @brief 构造函数
     * @param dialog 主对话框指针
     */
    explicit ManagerIntegration(CPortMasterDlg* dialog);
    
    /**
     * @brief 析构函数
     */
    ~ManagerIntegration() = default;

    // 禁用拷贝构造和拷贝赋值 - RAII原则
    ManagerIntegration(const ManagerIntegration&) = delete;
    ManagerIntegration& operator=(const ManagerIntegration&) = delete;
    
    /**
     * @brief 初始化所有管理器
     * @return 初始化是否成功
     */
    bool Initialize();
    
    /**
     * @brief 设置UI控件
     * @param dataView 数据显示控件
     * @param progressCtrl 进度条控件
     * @param statusLabel 状态标签控件
     * @param connectionLabel 连接状态标签控件
     */
    void SetUIControls(CEdit* dataView, CProgressCtrl* progressCtrl = nullptr, 
                      CStatic* statusLabel = nullptr, CStatic* connectionLabel = nullptr);
    
    /**
     * @brief 获取数据显示管理器
     * @return 数据显示管理器指针
     */
    DataDisplayManager* GetDataDisplayManager() const;
    
    /**
     * @brief 获取状态管理器
     * @return 状态管理器指针
     */
    StateManager* GetStateManager() const;
    
    /**
     * @brief 获取传输管理器
     * @return 传输管理器指针
     */
    TransportManager* GetTransportManager() const;
    
    /**
     * @brief 获取文件操作管理器
     * @return 文件操作管理器指针
     */
    FileOperationManager* GetFileOperationManager() const;
    
    /**
     * @brief 设置应用状态
     * @param state 新状态
     * @param message 状态消息
     * @param source 状态来源
     */
    void SetApplicationState(ApplicationState state, const std::string& message = "", const std::string& source = "");
    
    /**
     * @brief 更新数据显示
     * @param data 显示数据
     * @param mode 显示模式
     */
    void UpdateDataDisplay(const std::vector<uint8_t>& data, DisplayMode mode);
    
    /**
     * @brief 追加数据显示
     * @param data 追加数据
     */
    void AppendDataDisplay(const std::vector<uint8_t>& data);
    
    /**
     * @brief 清空数据显示
     */
    void ClearDataDisplay();
    
    /**
     * @brief 设置显示模式
     * @param hexMode true为十六进制模式，false为文本模式
     */
    void SetDisplayMode(bool hexMode);
    
    /**
     * @brief 获取当前显示的数据
     * @return 当前显示数据
     */
    std::vector<uint8_t> GetDisplayedData() const;
    
    /**
     * @brief 获取格式化的显示文本
     * @return 当前显示的文本内容
     */
    CString GetFormattedText() const;
    
    /**
     * @brief 检查当前应用状态
     * @param state 要检查的状态
     * @return 是否处于该状态
     */
    bool IsInState(ApplicationState state) const;
    
    /**
     * @brief 设置错误状态
     * @param errorMessage 错误消息
     * @param source 错误来源
     */
    void SetErrorState(const std::string& errorMessage, const std::string& source = "");

private:
    // 核心管理器实例
    std::unique_ptr<DataDisplayManager> m_dataDisplayManager;  // 数据显示管理器
    std::unique_ptr<StateManager> m_stateManager;             // 状态管理器
    std::unique_ptr<TransportManager> m_transportManager;     // 传输管理器
    std::unique_ptr<FileOperationManager> m_fileOperationManager; // 文件操作管理器
    
    // 回调实现
    std::shared_ptr<PortMasterUIStateUpdater> m_uiUpdater;    // UI状态更新器
    std::shared_ptr<PortMasterStateCallback> m_stateCallback; // 状态变化回调
    
    // 主对话框指针
    CPortMasterDlg* m_dialog;
    
    // 初始化标志
    bool m_initialized;
};

/**
 * @brief 管理器集成器工厂类
 * SOLID-O: 开放封闭原则 - 支持扩展不同的集成器类型
 */
class ManagerIntegrationFactory
{
public:
    /**
     * @brief 创建管理器集成器
     * @param dialog 主对话框指针
     * @return 管理器集成器实例
     */
    static std::unique_ptr<ManagerIntegration> Create(CPortMasterDlg* dialog);
};