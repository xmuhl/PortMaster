// ButtonStateManager.cpp - 按钮状态管理器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "ButtonStateManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// 全局按钮状态管理器实例
ButtonStateManager* g_buttonStateManager = nullptr;

// 按钮名称映射表
static const char* s_buttonNames[] = {
	"连接",
	"断开",
	"发送",
	"停止",
	"文件",
	"清空全部",
	"清空接收",
	"复制全部",
	"保存全部",
	"暂停/继续",
	"未知按钮"
};

// 状态名称映射表
static const char* s_stateNames[] = {
	"启用",
	"禁用",
	"显示",
	"隐藏"
};

ButtonStateManager::ButtonStateManager()
	: m_stateChangeCallback(nullptr)
{
	// 初始化按钮到控件ID的映射
	m_buttonToControlMap = {
		{ButtonID::Connect, 1001},      // IDC_BUTTON_CONNECT
		{ButtonID::Disconnect, 1002},   // IDC_BUTTON_DISCONNECT
		{ButtonID::Send, 1003},         // IDC_BUTTON_SEND
		{ButtonID::Stop, 1004},         // IDC_BUTTON_STOP
		{ButtonID::File, 1005},         // IDC_BUTTON_FILE
		{ButtonID::ClearAll, 1006},     // IDC_BUTTON_CLEAR_ALL
		{ButtonID::ClearReceive, 1007}, // IDC_BUTTON_CLEAR_RECEIVE
		{ButtonID::CopyAll, 1008},      // IDC_BUTTON_COPY_ALL
		{ButtonID::SaveAll, 1009},      // IDC_BUTTON_SAVE_ALL
		{ButtonID::PauseResume, 1010}   // 暂停/继续按钮（复用发送按钮）
	};

	// 初始化默认状态
	ResetToDefault();
}

ButtonStateManager::~ButtonStateManager()
{
	// 清理全局指针
	if (g_buttonStateManager == this) {
		g_buttonStateManager = nullptr;
	}
}

void ButtonStateManager::SetStateChangeCallback(ButtonStateChangeCallback callback)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stateChangeCallback = callback;
}

bool ButtonStateManager::SetButtonState(ButtonID buttonId, ButtonState newState, const std::string& reason)
{
	if (!IsValidButtonID(buttonId)) {
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	ButtonState oldState = m_buttonStates[buttonId];
	if (oldState == newState) {
		return true; // 状态没有变化
	}

	m_buttonStates[buttonId] = newState;

	// 通知状态变化
	NotifyStateChange(buttonId, newState, reason);

	return true;
}

ButtonState ButtonStateManager::GetButtonState(ButtonID buttonId) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_buttonStates.find(buttonId);
	if (it != m_buttonStates.end()) {
		return it->second;
	}

	return ButtonState::Disabled; // 默认为禁用
}

void ButtonStateManager::SetButtonStates(const std::unordered_map<ButtonID, ButtonState>& states, const std::string& reason)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	for (const auto& pair : states) {
		if (IsValidButtonID(pair.first)) {
			ButtonState oldState = m_buttonStates[pair.first];
			if (oldState != pair.second) {
				m_buttonStates[pair.first] = pair.second;
				NotifyStateChange(pair.first, pair.second, reason);
			}
		}
	}
}

bool ButtonStateManager::IsButtonEnabled(ButtonID buttonId) const
{
	ButtonState state = GetButtonState(buttonId);
	return (state == ButtonState::Enabled || state == ButtonState::Visible);
}

bool ButtonStateManager::IsButtonVisible(ButtonID buttonId) const
{
	ButtonState state = GetButtonState(buttonId);
	return (state == ButtonState::Enabled || state == ButtonState::Disabled);
}

void ButtonStateManager::ApplyIdleState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Enabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Disabled},
		{ButtonID::Stop, ButtonState::Disabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},
		{ButtonID::PauseResume, ButtonState::Disabled}
	};

	SetButtonStates(states, "切换到空闲状态");
}

void ButtonStateManager::ApplyConnectingState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Enabled},
		{ButtonID::Send, ButtonState::Disabled},
		{ButtonID::Stop, ButtonState::Enabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},
		{ButtonID::PauseResume, ButtonState::Disabled}
	};

	SetButtonStates(states, "切换到连接中状态");
}

void ButtonStateManager::ApplyConnectedState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Enabled},
		{ButtonID::Send, ButtonState::Enabled},
		{ButtonID::Stop, ButtonState::Disabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},
		{ButtonID::PauseResume, ButtonState::Disabled}
	};

	SetButtonStates(states, "切换到已连接状态");
}

void ButtonStateManager::ApplyTransmittingState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Enabled},  // 变为中断/暂停按钮
		{ButtonID::Stop, ButtonState::Enabled},
		{ButtonID::File, ButtonState::Disabled},
		{ButtonID::ClearAll, ButtonState::Disabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Disabled}, // 传输中禁用保存
		{ButtonID::PauseResume, ButtonState::Enabled}
	};

	SetButtonStates(states, "切换到传输中状态");
}

void ButtonStateManager::ApplyPausedState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Enabled},  // 变为继续按钮
		{ButtonID::Stop, ButtonState::Enabled},
		{ButtonID::File, ButtonState::Disabled},
		{ButtonID::ClearAll, ButtonState::Disabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Disabled}, // 暂停时禁用保存
		{ButtonID::PauseResume, ButtonState::Enabled}
	};

	SetButtonStates(states, "切换到暂停状态");
}

void ButtonStateManager::ApplyCompletedState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Enabled},
		{ButtonID::Send, ButtonState::Enabled},  // 恢复为发送按钮
		{ButtonID::Stop, ButtonState::Disabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},  // 完成后启用保存
		{ButtonID::PauseResume, ButtonState::Disabled}
	};

	SetButtonStates(states, "切换到完成状态");
}

void ButtonStateManager::ApplyErrorState()
{
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Enabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Enabled},  // 恢复为发送按钮
		{ButtonID::Stop, ButtonState::Disabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},  // 错误时启用保存
		{ButtonID::PauseResume, ButtonState::Disabled}
	};

	SetButtonStates(states, "切换到错误状态");
}

void ButtonStateManager::ApplyReliableModeIdleState()
{
	// 可靠模式空闲状态 - 与普通空闲状态类似，但可能有特殊设置
	ApplyIdleState();
	SetButtonState(ButtonID::SaveAll, ButtonState::Disabled, "可靠模式空闲时禁用保存");
}

void ButtonStateManager::ApplyReliableModeTransmittingState()
{
	// 可靠模式传输中状态 - 禁用更多操作以确保传输安全
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Enabled},  // 变为中断按钮
		{ButtonID::Stop, ButtonState::Enabled},
		{ButtonID::File, ButtonState::Disabled},
		{ButtonID::ClearAll, ButtonState::Disabled},
		{ButtonID::ClearReceive, ButtonState::Disabled}, // 可靠模式传输中禁用清空
		{ButtonID::CopyAll, ButtonState::Disabled},     // 可靠模式传输中禁用复制
		{ButtonID::SaveAll, ButtonState::Disabled},     // 可靠模式传输中禁用保存
		{ButtonID::PauseResume, ButtonState::Enabled}
	};

	SetButtonStates(states, "切换到可靠模式传输中状态");
}

void ButtonStateManager::ApplyReliableModePausedState()
{
	// 可靠模式暂停状态 - 允许恢复和中断
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Enabled},  // 变为继续按钮
		{ButtonID::Stop, ButtonState::Enabled},
		{ButtonID::File, ButtonState::Disabled},
		{ButtonID::ClearAll, ButtonState::Disabled},
		{ButtonID::ClearReceive, ButtonState::Disabled},
		{ButtonID::CopyAll, ButtonState::Enabled}, // 暂停时允许复制
		{ButtonID::SaveAll, ButtonState::Disabled}, // 可靠模式暂停时禁用保存
		{ButtonID::PauseResume, ButtonState::Enabled}
	};

	SetButtonStates(states, "切换到可靠模式暂停状态");
}

void ButtonStateManager::ApplyReliableModeCompletedState()
{
	// 可靠模式完成状态 - 启用所有操作
	std::unordered_map<ButtonID, ButtonState> states = {
		{ButtonID::Connect, ButtonState::Disabled},
		{ButtonID::Disconnect, ButtonState::Enabled},
		{ButtonID::Send, ButtonState::Enabled},  // 恢复为发送按钮
		{ButtonID::Stop, ButtonState::Disabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},  // 完成后启用保存
		{ButtonID::PauseResume, ButtonState::Disabled}
	};

	SetButtonStates(states, "切换到可靠模式完成状态");
}

void ButtonStateManager::DumpButtonStates() const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	std::cout << "=== 按钮状态管理器调试信息 ===" << std::endl;

	for (const auto& pair : m_buttonStates) {
		std::cout << GetButtonName(pair.first) << ": "
			<< GetStateName(pair.second) << std::endl;
	}

	std::cout << "===============================" << std::endl;
}

void ButtonStateManager::ResetToDefault()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 重置所有按钮到默认状态
	m_buttonStates = {
		{ButtonID::Connect, ButtonState::Enabled},
		{ButtonID::Disconnect, ButtonState::Disabled},
		{ButtonID::Send, ButtonState::Disabled},
		{ButtonID::Stop, ButtonState::Disabled},
		{ButtonID::File, ButtonState::Enabled},
		{ButtonID::ClearAll, ButtonState::Enabled},
		{ButtonID::ClearReceive, ButtonState::Enabled},
		{ButtonID::CopyAll, ButtonState::Enabled},
		{ButtonID::SaveAll, ButtonState::Enabled},
		{ButtonID::PauseResume, ButtonState::Disabled}
	};
}

void ButtonStateManager::ClearCallbacks()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stateChangeCallback = nullptr;
}

// 私有方法实现

bool ButtonStateManager::IsValidButtonID(ButtonID buttonId) const
{
	return (buttonId >= ButtonID::Connect && buttonId <= ButtonID::PauseResume);
}

const char* ButtonStateManager::GetButtonName(ButtonID buttonId) const
{
	int index = static_cast<int>(buttonId);
	if (index >= 0 && index < sizeof(s_buttonNames) / sizeof(s_buttonNames[0])) {
		return s_buttonNames[index];
	}
	return s_buttonNames[10]; // "未知按钮"
}

const char* ButtonStateManager::GetStateName(ButtonState state) const
{
	int index = static_cast<int>(state);
	if (index >= 0 && index < sizeof(s_stateNames) / sizeof(s_stateNames[0])) {
		return s_stateNames[index];
	}
	return "未知状态";
}

void ButtonStateManager::NotifyStateChange(ButtonID buttonId, ButtonState newState, const std::string& reason)
{
	// 记录日志
	std::ostringstream log;
	log << "按钮状态变化: " << GetButtonName(buttonId)
		<< " -> " << GetStateName(newState);

	if (!reason.empty()) {
		log << " (" << reason << ")";
	}

	std::cout << log.str() << std::endl;

	// 调用回调函数
	if (m_stateChangeCallback) {
		m_stateChangeCallback(buttonId, newState, reason);
	}
}