#pragma execution_character_set("utf-8")
#include "pch.h"
#include "ManagerIntegration.h"
#include "../PortMasterDlg.h"
#include <afxwin.h>

extern void WriteDebugLog(const char* message);

// PortMasterUIStateUpdater 实现

PortMasterUIStateUpdater::PortMasterUIStateUpdater(CPortMasterDlg* dialog)
    : m_dialog(dialog)
{
    WriteDebugLog("[DEBUG] PortMasterUIStateUpdater构造完成");
}

void PortMasterUIStateUpdater::UpdateConnectionStatus(bool connected, const std::string& info)
{
    if (!m_dialog) return;
    
    try {
        // 将状态更新传递给主对话框 - 这里预留接口，待主对话框重构后实现
        WriteDebugLog(connected ? "[UI] 连接状态: 已连接" : "[UI] 连接状态: 已断开");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterUIStateUpdater::UpdateConnectionStatus异常");
    }
}

void PortMasterUIStateUpdater::UpdateTransmissionStatus(ApplicationState state, double progress)
{
    if (!m_dialog) return;
    
    try {
        // 将传输状态更新传递给主对话框
        WriteDebugLog("[UI] 传输状态更新");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterUIStateUpdater::UpdateTransmissionStatus异常");
    }
}

void PortMasterUIStateUpdater::UpdateButtonStates(ApplicationState state)
{
    if (!m_dialog) return;
    
    try {
        // 将按钮状态更新传递给主对话框
        WriteDebugLog("[UI] 按钮状态更新");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterUIStateUpdater::UpdateButtonStates异常");
    }
}

void PortMasterUIStateUpdater::UpdateStatusBar(const std::string& message, StatePriority priority)
{
    if (!m_dialog) return;
    
    try {
        // 将状态栏更新传递给主对话框
        WriteDebugLog("[UI] 状态栏更新");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterUIStateUpdater::UpdateStatusBar异常");
    }
}

void PortMasterUIStateUpdater::ShowErrorMessage(const std::string& title, const std::string& message)
{
    if (!m_dialog) return;
    
    try {
        // 显示错误消息框
        CString titleW = CA2W(title.c_str(), CP_UTF8);
        CString messageW = CA2W(message.c_str(), CP_UTF8);
        
        WriteDebugLog("[UI] 显示错误消息");
        // m_dialog->MessageBox(messageW, titleW, MB_ICONERROR);  // 预留，待重构后实现
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterUIStateUpdater::ShowErrorMessage异常");
    }
}

// PortMasterStateCallback 实现

PortMasterStateCallback::PortMasterStateCallback(CPortMasterDlg* dialog)
    : m_dialog(dialog)
{
    WriteDebugLog("[DEBUG] PortMasterStateCallback构造完成");
}

void PortMasterStateCallback::OnStateChanged(const StateInfo& oldState, const StateInfo& newState)
{
    if (!m_dialog) return;
    
    try {
        WriteDebugLog("[DEBUG] 状态变化回调触发");
        // 这里可以记录状态变化日志或执行其他回调逻辑
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterStateCallback::OnStateChanged异常");
    }
}

void PortMasterStateCallback::OnStateUpdate(const StateInfo& stateInfo)
{
    if (!m_dialog) return;
    
    try {
        WriteDebugLog("[DEBUG] 状态更新回调触发");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterStateCallback::OnStateUpdate异常");
    }
}

void PortMasterStateCallback::OnErrorState(const StateInfo& errorState)
{
    if (!m_dialog) return;
    
    try {
        WriteDebugLog("[ERROR] 错误状态回调触发");
        // 处理错误状态的特殊逻辑
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] PortMasterStateCallback::OnErrorState异常");
    }
}

// ManagerIntegration 实现

ManagerIntegration::ManagerIntegration(CPortMasterDlg* dialog)
    : m_dialog(dialog)
    , m_initialized(false)
{
    WriteDebugLog("[DEBUG] ManagerIntegration构造开始");
    
    try {
        // 创建管理器实例
        m_dataDisplayManager = DataDisplayManagerFactory::CreateDefault();
        m_stateManager = StateManagerFactory::CreateDefault();
        
        // 创建回调实现
        m_uiUpdater = std::make_shared<PortMasterUIStateUpdater>(dialog);
        m_stateCallback = std::make_shared<PortMasterStateCallback>(dialog);
        
        WriteDebugLog("[DEBUG] ManagerIntegration构造完成");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration构造异常");
        m_dataDisplayManager.reset();
        m_stateManager.reset();
        m_uiUpdater.reset();
        m_stateCallback.reset();
    }
}

bool ManagerIntegration::Initialize()
{
    if (m_initialized) {
        return true;
    }
    
    try {
        if (!m_dataDisplayManager || !m_stateManager) {
            WriteDebugLog("[ERROR] ManagerIntegration::Initialize: 管理器实例未创建");
            return false;
        }
        
        // 设置状态管理器的回调
        if (m_uiUpdater) {
            m_stateManager->SetUIStateUpdater(m_uiUpdater);
        }
        
        if (m_stateCallback) {
            m_stateManager->SetStateChangeCallback(m_stateCallback);
        }
        
        // 设置初始状态
        m_stateManager->SetApplicationState(ApplicationState::READY, "系统初始化完成", StatePriority::NORMAL, "ManagerIntegration");
        
        m_initialized = true;
        WriteDebugLog("[DEBUG] ManagerIntegration::Initialize: 初始化成功");
        return true;
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::Initialize: 初始化异常");
        return false;
    }
}

void ManagerIntegration::SetUIControls(CEdit* dataView, CProgressCtrl* progressCtrl, 
                                      CStatic* statusLabel, CStatic* connectionLabel)
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetUIControls: DataDisplayManager未创建");
        return;
    }
    
    try {
        m_dataDisplayManager->SetDisplayControls(dataView, progressCtrl, statusLabel);
        WriteDebugLog("[DEBUG] ManagerIntegration::SetUIControls: UI控件设置完成");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetUIControls: 设置异常");
    }
}

DataDisplayManager* ManagerIntegration::GetDataDisplayManager() const
{
    return m_dataDisplayManager.get();
}

StateManager* ManagerIntegration::GetStateManager() const
{
    return m_stateManager.get();
}

void ManagerIntegration::SetApplicationState(ApplicationState state, const std::string& message, const std::string& source)
{
    if (!m_stateManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetApplicationState: StateManager未创建");
        return;
    }
    
    try {
        m_stateManager->SetApplicationState(state, message, StatePriority::NORMAL, source);
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetApplicationState: 设置异常");
    }
}

void ManagerIntegration::UpdateDataDisplay(const std::vector<uint8_t>& data, DisplayMode mode)
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::UpdateDataDisplay: DataDisplayManager未创建");
        return;
    }
    
    try {
        m_dataDisplayManager->SetDisplayMode(mode);
        m_dataDisplayManager->UpdateDisplay(data);
        WriteDebugLog("[DEBUG] ManagerIntegration::UpdateDataDisplay: 数据显示更新完成");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::UpdateDataDisplay: 更新异常");
    }
}

void ManagerIntegration::AppendDataDisplay(const std::vector<uint8_t>& data)
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::AppendDataDisplay: DataDisplayManager未创建");
        return;
    }
    
    try {
        m_dataDisplayManager->AppendDisplay(data);
        WriteDebugLog("[DEBUG] ManagerIntegration::AppendDataDisplay: 数据追加完成");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::AppendDataDisplay: 追加异常");
    }
}

void ManagerIntegration::ClearDataDisplay()
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::ClearDataDisplay: DataDisplayManager未创建");
        return;
    }
    
    try {
        m_dataDisplayManager->ClearDisplay();
        WriteDebugLog("[DEBUG] ManagerIntegration::ClearDataDisplay: 显示清空完成");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::ClearDataDisplay: 清空异常");
    }
}

void ManagerIntegration::SetDisplayMode(bool hexMode)
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetDisplayMode: DataDisplayManager未创建");
        return;
    }
    
    try {
        DisplayMode mode = hexMode ? DisplayMode::MIXED : DisplayMode::TEXT;
        m_dataDisplayManager->SetDisplayMode(mode);
        WriteDebugLog(hexMode ? "[DEBUG] 显示模式: 混合显示" : "[DEBUG] 显示模式: 文本显示");
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetDisplayMode: 设置异常");
    }
}

std::vector<uint8_t> ManagerIntegration::GetDisplayedData() const
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::GetDisplayedData: DataDisplayManager未创建");
        return std::vector<uint8_t>();
    }
    
    try {
        return m_dataDisplayManager->GetDisplayedData();
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::GetDisplayedData: 获取异常");
        return std::vector<uint8_t>();
    }
}

CString ManagerIntegration::GetFormattedText() const
{
    if (!m_dataDisplayManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::GetFormattedText: DataDisplayManager未创建");
        return L"";
    }
    
    try {
        return m_dataDisplayManager->GetFormattedText();
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::GetFormattedText: 获取异常");
        return L"";
    }
}

bool ManagerIntegration::IsInState(ApplicationState state) const
{
    if (!m_stateManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::IsInState: StateManager未创建");
        return false;
    }
    
    try {
        return m_stateManager->IsInState(state);
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::IsInState: 检查异常");
        return false;
    }
}

void ManagerIntegration::SetErrorState(const std::string& errorMessage, const std::string& source)
{
    if (!m_stateManager) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetErrorState: StateManager未创建");
        return;
    }
    
    try {
        m_stateManager->SetErrorState(errorMessage, source);
    }
    catch (const std::exception&) {
        WriteDebugLog("[ERROR] ManagerIntegration::SetErrorState: 设置异常");
    }
}

// ManagerIntegrationFactory 实现

std::unique_ptr<ManagerIntegration> ManagerIntegrationFactory::Create(CPortMasterDlg* dialog)
{
    return std::make_unique<ManagerIntegration>(dialog);
}