// DialogUiController.cpp - 对话框UI控制器实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "DialogUiController.h"
#include "PortMasterDlg.h"
#include "resource.h"
#include <cassert>

// 构造函数
DialogUiController::DialogUiController()
	: m_receiveDisplayPending(false)
	, m_lastReceiveDisplayUpdate(0)
	, m_lastProgressPercent(0)
{
	// 初始化控件引用为nullptr
	memset(&m_controls, 0, sizeof(m_controls));
}

// 析构函数
DialogUiController::~DialogUiController()
{
	// 停止所有定时器
	StopThrottledDisplayTimer();
}

// 初始化：绑定控件引用并执行初始化设置
void DialogUiController::Initialize(const UIControlRefs& controlRefs)
{
	m_controls = controlRefs;
	ValidateControlRefs();

	// 执行控件初始化
	InitializeControls();
}

// 初始化所有控件的默认值和选项
void DialogUiController::InitializeControls()
{
	InitializePortTypeCombo();
	InitializeSerialParameterCombos();
	InitializeTransmissionModeRadios();
	InitializeProgressBar();
	InitializeStatusDisplays();
}

// 初始化端口类型下拉框
void DialogUiController::InitializePortTypeCombo()
{
	if (!IsControlValid(m_controls.comboPortType))
		return;

	m_controls.comboPortType->ResetContent();
	m_controls.comboPortType->AddString(_T("串口"));
	m_controls.comboPortType->AddString(_T("并口"));
	m_controls.comboPortType->AddString(_T("USB打印"));
	m_controls.comboPortType->AddString(_T("网络打印"));
	m_controls.comboPortType->AddString(_T("回路测试"));

	// 默认选择回路测试
	m_controls.comboPortType->SetCurSel(4);
}

// 初始化串口参数下拉框
void DialogUiController::InitializeSerialParameterCombos()
{
	// 初始化端口列表
	if (IsControlValid(m_controls.comboPort))
	{
		m_controls.comboPort->ResetContent();
		m_controls.comboPort->AddString(_T("COM1"));
		m_controls.comboPort->SetCurSel(0);
	}

	// 初始化波特率列表
	if (IsControlValid(m_controls.comboBaudRate))
	{
		m_controls.comboBaudRate->ResetContent();
		m_controls.comboBaudRate->AddString(_T("9600"));
		m_controls.comboBaudRate->SetCurSel(0);
	}

	// 初始化数据位下拉框
	if (IsControlValid(m_controls.comboDataBits))
	{
		m_controls.comboDataBits->ResetContent();
		m_controls.comboDataBits->AddString(_T("8"));
		m_controls.comboDataBits->SetCurSel(0);
	}

	// 初始化校验位下拉框
	if (IsControlValid(m_controls.comboParity))
	{
		m_controls.comboParity->ResetContent();
		m_controls.comboParity->AddString(_T("None"));
		m_controls.comboParity->SetCurSel(0);
	}

	// 初始化停止位下拉框
	if (IsControlValid(m_controls.comboStopBits))
	{
		m_controls.comboStopBits->ResetContent();
		m_controls.comboStopBits->AddString(_T("1"));
		m_controls.comboStopBits->SetCurSel(0);
	}

	// 初始化流控下拉框
	if (IsControlValid(m_controls.comboFlowControl))
	{
		m_controls.comboFlowControl->ResetContent();
		m_controls.comboFlowControl->AddString(_T("None"));
		m_controls.comboFlowControl->SetCurSel(0);
	}

	// 初始化超时编辑框
	if (IsControlValid(m_controls.parentDialog) && IsControlValid(m_controls.editTimeout))
	{
		m_controls.parentDialog->SetDlgItemText(IDC_EDIT_TIMEOUT, _T("1000"));
	}
}

// 初始化传输模式单选按钮
void DialogUiController::InitializeTransmissionModeRadios()
{
	// 默认选择可靠模式
	if (IsControlValid(m_controls.radioReliable))
	{
		m_controls.radioReliable->SetCheck(BST_CHECKED);
	}

	if (IsControlValid(m_controls.radioDirect))
	{
		m_controls.radioDirect->SetCheck(BST_UNCHECKED);
	}

	// ✅ 修复：同步更新模式显示文本，确保IDC_STATIC_MODE初始化时显示"可靠"
	UpdateTransmissionMode(true);

	// 初始化占用检测复选框 - 默认勾选
	if (IsControlValid(m_controls.checkOccupy))
	{
		m_controls.checkOccupy->SetCheck(BST_CHECKED);
	}
}

// 初始化进度条
void DialogUiController::InitializeProgressBar()
{
	if (!IsControlValid(m_controls.progress))
		return;

	m_controls.progress->SetRange(0, 100);
	SetProgressPercent(0, true);
}

// 初始化状态显示区域
void DialogUiController::InitializeStatusDisplays()
{
	if (IsControlValid(m_controls.parentDialog))
	{
		// 显示初始状态
		if (IsControlValid(m_controls.staticPortStatus))
		{
			m_controls.staticPortStatus->SetWindowText(_T("未连接"));
		}

		if (IsControlValid(m_controls.staticSpeed))
		{
			m_controls.staticSpeed->SetWindowText(_T("0%"));
		}

		if (IsControlValid(m_controls.staticSendSource))
		{
			m_controls.staticSendSource->SetWindowText(_T("手动输入"));
		}

		// ✅ 修复：初始化发送按钮文本为"发送"，确保UI状态正确
		if (IsControlValid(m_controls.btnSend))
		{
			m_controls.btnSend->SetWindowText(_T("发送"));
		}

		// 初始日志消息
		LogMessage(_T("程序启动成功"));
	}
}

// 更新连接/断开按钮状态
void DialogUiController::UpdateConnectionButtons(bool connected)
{
	if (IsControlValid(m_controls.btnConnect))
	{
		m_controls.btnConnect->EnableWindow(connected ? FALSE : TRUE);
	}

	if (IsControlValid(m_controls.btnDisconnect))
	{
		m_controls.btnDisconnect->EnableWindow(connected ? TRUE : FALSE);
	}
}

// 【新增】统一按状态驱动UI更新 - 状态机主入口
void DialogUiController::ApplyTransmissionState(TransmissionUiState state)
{
	m_currentTransmissionState = state;

	// 根据状态设置按钮和文本
	switch (state)
	{
	case TransmissionUiState::Idle:
		// 空闲状态：发送按钮启用、文本"发送"，停止按钮禁用，文件按钮启用
		if (IsControlValid(m_controls.btnSend))
		{
			m_controls.btnSend->EnableWindow(TRUE);
			m_controls.btnSend->SetWindowText(_T("发送"));
		}
		if (IsControlValid(m_controls.btnStop))
		{
			m_controls.btnStop->EnableWindow(FALSE);
		}
		if (IsControlValid(m_controls.btnFile))
		{
			m_controls.btnFile->EnableWindow(TRUE);
		}
		break;

	case TransmissionUiState::Running:
		// 运行状态：发送按钮启用、文本"中断"，停止按钮启用，文件按钮禁用
		if (IsControlValid(m_controls.btnSend))
		{
			m_controls.btnSend->EnableWindow(TRUE);
			m_controls.btnSend->SetWindowText(_T("中断"));
		}
		if (IsControlValid(m_controls.btnStop))
		{
			m_controls.btnStop->EnableWindow(TRUE);
		}
		if (IsControlValid(m_controls.btnFile))
		{
			m_controls.btnFile->EnableWindow(FALSE);
		}
		break;

	case TransmissionUiState::Paused:
		// 暂停状态：发送按钮启用、文本"继续"，停止按钮启用，文件按钮禁用
		if (IsControlValid(m_controls.btnSend))
		{
			m_controls.btnSend->EnableWindow(TRUE);
			m_controls.btnSend->SetWindowText(_T("继续"));
		}
		if (IsControlValid(m_controls.btnStop))
		{
			m_controls.btnStop->EnableWindow(TRUE);
		}
		if (IsControlValid(m_controls.btnFile))
		{
			m_controls.btnFile->EnableWindow(FALSE);
		}
		break;

	case TransmissionUiState::Cancelling:
		// 取消中状态：发送/停止按钮禁用，状态栏提示"正在停止…"
		if (IsControlValid(m_controls.btnSend))
		{
			m_controls.btnSend->EnableWindow(FALSE);
		}
		if (IsControlValid(m_controls.btnStop))
		{
			m_controls.btnStop->EnableWindow(FALSE);
		}
		if (IsControlValid(m_controls.btnFile))
		{
			m_controls.btnFile->EnableWindow(FALSE);
		}
		// 更新状态栏提示
		if (IsControlValid(m_controls.staticPortStatus))
		{
			m_controls.staticPortStatus->SetWindowText(_T("正在停止…"));
		}
		break;

	default:
		// 未知状态，恢复为空闲
		ApplyTransmissionState(TransmissionUiState::Idle);
		break;
	}
}

// 更新发送/停止按钮状态
void DialogUiController::UpdateTransmissionButtons(bool transmitting, bool paused)
{
	if (IsControlValid(m_controls.btnSend))
	{
		// 发送按钮：未传输时启用，传输中禁用
		m_controls.btnSend->EnableWindow(transmitting ? FALSE : TRUE);
	}

	if (IsControlValid(m_controls.btnStop))
	{
		// 停止按钮：传输中启用，未传输时禁用
		m_controls.btnStop->EnableWindow(transmitting ? TRUE : FALSE);
	}

	if (IsControlValid(m_controls.btnFile))
	{
		// 文件按钮：未传输时启用，传输中禁用
		m_controls.btnFile->EnableWindow(transmitting ? FALSE : TRUE);
	}
}

// 更新保存按钮状态
void DialogUiController::UpdateSaveButton(bool enabled)
{
	if (IsControlValid(m_controls.btnSaveAll))
	{
		m_controls.btnSaveAll->EnableWindow(enabled ? TRUE : FALSE);
	}
}

// 统一更新所有按钮状态
void DialogUiController::UpdateAllButtonStates(bool connected, bool transmitting, bool paused, bool hasSaveData)
{
	UpdateConnectionButtons(connected);
	UpdateTransmissionButtons(transmitting, paused);
	UpdateSaveButton(hasSaveData);
}

// 更新连接状态文本（端口名+连接状态）
void DialogUiController::UpdateConnectionStatus(const CString& portName, bool connected, const CString& statusExtInfo)
{
	if (!IsControlValid(m_controls.staticPortStatus))
		return;

	CString statusText;
	if (connected)
	{
		statusText = _T("已连接");
	}
	else if (!statusExtInfo.IsEmpty())
	{
		statusText = statusExtInfo;  // 如"占用"、"错误"等
	}
	else
	{
		statusText = _T("未连接");
	}

	// 组合显示：端口名 + 连接状态
	CString displayText;
	displayText.Format(_T("%s (%s)"), portName.GetString(), statusText.GetString());
	m_controls.staticPortStatus->SetWindowText(displayText);
}

// 更新传输模式文本
void DialogUiController::UpdateTransmissionMode(bool reliable)
{
	if (!IsControlValid(m_controls.staticMode))
		return;

	CString modeText = reliable ? _T("可靠") : _T("直通");
	m_controls.staticMode->SetWindowText(modeText);
}

// 更新速度显示
void DialogUiController::UpdateProgressDisplay(int progressPercent)
{
	if (!IsControlValid(m_controls.staticSpeed))
		return;

	// 边界检查
	if (progressPercent < 0)
		progressPercent = 0;
	if (progressPercent > 100)
		progressPercent = 100;

	CString progressText;
	progressText.Format(_T("%d%%"), progressPercent);
	m_controls.staticSpeed->SetWindowText(progressText);
}

// 更新发送源显示
void DialogUiController::UpdateSendSourceDisplay(const CString& source)
{
	if (!IsControlValid(m_controls.staticSendSource))
		return;

	m_controls.staticSendSource->SetWindowText(source);
}

// 输出日志消息
void DialogUiController::LogMessage(const CString& message)
{
	if (!IsControlValid(m_controls.staticLog))
	{
		return;
	}

	CString formattedMessage = message;

	// 自动换行处理，确保日志文本完整显示
	CRect rect;
	m_controls.staticLog->GetClientRect(&rect);

	CDC* pDC = m_controls.staticLog->GetDC();
	if (pDC != nullptr)
	{
		CFont* pOldFont = pDC->SelectObject(m_controls.staticLog->GetFont());
		const int maxWidth = rect.Width() - 10; // 预留边距

		CString wrappedMessage;
		int startPos = 0;
		while (startPos < formattedMessage.GetLength())
		{
			int chunkEnd = startPos;
			CSize textSize;

			// 找到本行可以容纳的最大字符数
			while (chunkEnd < formattedMessage.GetLength())
			{
				CString chunk = formattedMessage.Mid(startPos, chunkEnd - startPos + 1);
				textSize = pDC->GetTextExtent(chunk);
				if (textSize.cx > maxWidth)
				{
					if (chunkEnd > startPos)
					{
						chunkEnd--;
					}
					break;
				}
				chunkEnd++;
			}

			if (chunkEnd <= startPos)
			{
				chunkEnd = startPos + 1; // 至少显示一个字符
			}

			if (!wrappedMessage.IsEmpty())
			{
				wrappedMessage += _T(" ");
			}
			wrappedMessage += formattedMessage.Mid(startPos, chunkEnd - startPos);

			startPos = chunkEnd;
		}

		pDC->SelectObject(pOldFont);
		m_controls.staticLog->ReleaseDC(pDC);

		formattedMessage = wrappedMessage;
	}

	m_controls.staticLog->SetWindowText(formattedMessage);
}

// 设置进度条百分比
void DialogUiController::SetProgressPercent(int percent, bool forceReset)
{
	if (!IsControlValid(m_controls.progress))
		return;

	// 单调性保护：防止进度条倒退（除非强制重置）
	if (!forceReset && percent < (int)m_lastProgressPercent)
	{
		return; // 忽略倒退的进度值
	}

	// 边界检查
	if (percent < 0)
		percent = 0;
	if (percent > 100)
		percent = 100;

	m_controls.progress->SetPos(percent);
	m_lastProgressPercent = percent;
}

// 重置进度条
void DialogUiController::ResetProgress()
{
	SetProgressPercent(0, true);
}

// 启动节流显示定时器
void DialogUiController::StartThrottledDisplayTimer()
{
	if (!IsControlValid(m_controls.parentDialog))
		return;

	m_controls.parentDialog->SetTimer(TIMER_ID_THROTTLED_DISPLAY, RECEIVE_DISPLAY_THROTTLE_MS, nullptr);
}

// 停止节流显示定时器
void DialogUiController::StopThrottledDisplayTimer()
{
	if (!IsControlValid(m_controls.parentDialog))
		return;

	// 【修复】在析构过程中避免调用KillTimer，防止调试错误
	// 双重检查：既有IsControlValid检查，也有窗口句柄有效性检查
	if (::IsWindow(m_controls.parentDialog->GetSafeHwnd()))
	{
		m_controls.parentDialog->KillTimer(TIMER_ID_THROTTLED_DISPLAY);
	}
}

// 查询是否有待处理的显示更新
bool DialogUiController::IsDisplayUpdatePending() const
{
	return m_receiveDisplayPending;
}

// 设置显示更新待处理标志
void DialogUiController::SetDisplayUpdatePending(bool pending)
{
	m_receiveDisplayPending = pending;
}

// 判断是否可以立即更新显示（节流控制）
bool DialogUiController::CanUpdateDisplay() const
{
	ULONGLONG currentTick = ::GetTickCount64();
	ULONGLONG elapsed = currentTick - m_lastReceiveDisplayUpdate;
	return elapsed >= RECEIVE_DISPLAY_THROTTLE_MS;
}

// 记录本次显示更新时间戳
void DialogUiController::RecordDisplayUpdate()
{
	m_lastReceiveDisplayUpdate = ::GetTickCount64();
}

// 设置节流显示回调函数
void DialogUiController::SetThrottledDisplayCallback(std::function<void()> callback)
{
	m_throttledDisplayCallback = callback;
}

// 是否选择可靠模式
bool DialogUiController::IsReliableModeSelected() const
{
	if (!IsControlValid(m_controls.radioReliable))
		return false;

	return m_controls.radioReliable->GetCheck() == BST_CHECKED;
}

// 是否启用十六进制显示
bool DialogUiController::IsHexDisplayEnabled() const
{
	if (!IsControlValid(m_controls.checkHex))
		return false;

	return m_controls.checkHex->GetCheck() == BST_CHECKED;
}

// 获取超时值
CString DialogUiController::GetTimeoutValue() const
{
	if (!IsControlValid(m_controls.editTimeout))
		return _T("1000");

	CString timeoutText;
	m_controls.editTimeout->GetWindowText(timeoutText);
	return timeoutText;
}

// 获取选择的端口类型索引
int DialogUiController::GetSelectedPortType() const
{
	if (!IsControlValid(m_controls.comboPortType))
		return -1;

	return m_controls.comboPortType->GetCurSel();
}

// 获取发送编辑框文本
CString DialogUiController::GetSendDataText() const
{
	if (!IsControlValid(m_controls.editSendData))
		return _T("");

	CString text;
	m_controls.editSendData->GetWindowText(text);
	return text;
}

// 获取接收编辑框文本
CString DialogUiController::GetReceiveDataText() const
{
	if (!IsControlValid(m_controls.editReceiveData))
		return _T("");

	CString text;
	m_controls.editReceiveData->GetWindowText(text);
	return text;
}

// 验证控件引用有效性
void DialogUiController::ValidateControlRefs() const
{
	// 验证关键控件
	assert(IsControlValid(m_controls.parentDialog) && "父对话框指针无效");

	// 可以根据需要添加更多验证
	// 注意：部分控件可能在某些配置下不存在，因此不强制验证所有控件
}

// 检查单个控件是否有效
bool DialogUiController::IsControlValid(CWnd* control) const
{
	return (control != nullptr && control->GetSafeHwnd() != nullptr);
}

// 直接控件文本设置方法
void DialogUiController::SetSendButtonText(const CString& text)
{
	if (IsControlValid(m_controls.btnSend))
	{
		m_controls.btnSend->SetWindowText(text);
	}
}

void DialogUiController::SetStatusText(const CString& text)
{
	if (IsControlValid(m_controls.staticPortStatus))
	{
		m_controls.staticPortStatus->SetWindowText(text);
	}
}

void DialogUiController::SetModeText(const CString& text)
{
	if (IsControlValid(m_controls.staticMode))
	{
		m_controls.staticMode->SetWindowText(text);
	}
}

void DialogUiController::SetReceiveDataText(const CString& text)
{
	if (IsControlValid(m_controls.editReceiveData))
	{
		m_controls.editReceiveData->SetWindowText(text);
	}
}

void DialogUiController::SetSendDataText(const CString& text)
{
	if (IsControlValid(m_controls.editSendData))
	{
		m_controls.editSendData->SetWindowText(text);
	}
}

void DialogUiController::SetStaticText(int controlId, const CString& text)
{
	if (IsControlValid(m_controls.parentDialog))
	{
		m_controls.parentDialog->SetDlgItemText(controlId, text);
	}
}