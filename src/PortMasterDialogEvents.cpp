// PortMasterDialogEvents.cpp - 对话框事件调度模块实现
#pragma execution_character_set("utf-8")

#include "pch.h"
#include "resource.h"
#include "PortMasterDialogEvents.h"
#include "PortMasterDlg.h"
#include "DialogUiController.h"
#include "PortConfigPresenter.h"
#include "../Common/DataPresentationService.h"
#include "../Common/ReceiveCacheService.h"
#include "../Common/StringUtils.h"
#include "../Protocol/PortSessionController.h"
#include "TransmissionCoordinator.h"
#include <fstream>

PortMasterDialogEvents::PortMasterDialogEvents(CPortMasterDlg& dialog)
	: m_dialog(dialog)
{
}

CString PortMasterDialogEvents::BuildTimestampPrefix() const
{
	CTime time = CTime::GetCurrentTime();
	return time.Format(_T("[%H:%M:%S] "));
}

void PortMasterDialogEvents::UpdatePortControlsEnabled(bool enabled)
{
	m_dialog.GetDlgItem(IDC_COMBO_PORT)->EnableWindow(enabled ? TRUE : FALSE);
	m_dialog.GetDlgItem(IDC_COMBO_BAUD_RATE)->EnableWindow(enabled ? TRUE : FALSE);
	m_dialog.GetDlgItem(IDC_COMBO_DATA_BITS)->EnableWindow(enabled ? TRUE : FALSE);
	m_dialog.GetDlgItem(IDC_COMBO_PARITY)->EnableWindow(enabled ? TRUE : FALSE);
	m_dialog.GetDlgItem(IDC_COMBO_STOP_BITS)->EnableWindow(enabled ? TRUE : FALSE);
	m_dialog.GetDlgItem(IDC_COMBO_FLOW_CONTROL)->EnableWindow(enabled ? TRUE : FALSE);
}

void PortMasterDialogEvents::HandleConnect()
{
	m_dialog.WriteLog("OnBnClickedButtonConnect: 开始连接...");

	// 【阶段A修复】从UI构建传输配置,替代硬编码初始化
	m_dialog.BuildTransportConfigFromUI();

	bool useReliableMode = (m_dialog.m_uiController && m_dialog.m_uiController->IsReliableModeSelected());
	m_dialog.WriteLog(std::string("OnBnClickedButtonConnect: 使用") + (useReliableMode ? "可靠" : "直通") + "模式");

	// 【第二轮阶段B修复】在连接前设置可靠传输配置
	if (useReliableMode && m_dialog.m_sessionController)
	{
		m_dialog.m_sessionController->SetReliableConfig(m_dialog.m_reliableConfig);
		m_dialog.WriteLog("OnBnClickedButtonConnect: 可靠传输配置已设置，timeoutMax=" + std::to_string(m_dialog.m_reliableConfig.timeoutMax) + "ms");
	}

	// 【分类6修复】选择合适的config对象：Loopback模式使用m_currentLoopbackConfig，其他模式使用m_transportConfig
	const TransportConfig& selectedConfig = (m_dialog.m_transportConfig.portType == PortType::PORT_TYPE_LOOPBACK)
		? static_cast<const TransportConfig&>(m_dialog.m_currentLoopbackConfig)
		: m_dialog.m_transportConfig;

	if (!m_dialog.m_sessionController || !m_dialog.m_sessionController->Connect(selectedConfig, useReliableMode))
	{
		m_dialog.WriteLog("OnBnClickedButtonConnect: 连接失败");
		m_dialog.MessageBox(_T("连接失败"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	m_dialog.m_sessionController->StartReceiveSession();
	m_dialog.WriteLog("OnBnClickedButtonConnect: 接收会话已启动");

	m_dialog.m_isConnected = true;

	// 【阶段C修复】连接成功后清除重新连接标志，恢复发送按钮
	m_dialog.m_requiresReconnect = false;

	m_dialog.UpdateConnectionStatus();

	if (m_dialog.m_uiController)
	{
		m_dialog.m_uiController->UpdateConnectionButtons(true);
		// 【阶段B修复】连接成功后默认启用"文件"按钮，仅在实际传输中禁用
		// 修复逻辑：m_isTransmitting初始为false，直接传入false表示非传输状态
		m_dialog.m_uiController->UpdateTransmissionButtons(false, false);
	}

	UpdatePortControlsEnabled(false);

	m_dialog.WriteLog("OnBnClickedButtonConnect: 连接成功");
}

void PortMasterDialogEvents::HandleDisconnect()
{
	m_dialog.WriteLog("OnBnClickedButtonDisconnect: 开始断开连接...");

	if (m_dialog.m_sessionController)
	{
		m_dialog.m_sessionController->Disconnect();
	}

	m_dialog.m_isConnected = false;

	// 【分类4.1修复】缓存初始化失败时提示用户并禁用保存
	if (m_dialog.m_receiveCacheService)
	{
		m_dialog.m_receiveCacheService->Shutdown();
		m_dialog.m_totalReceivedBytes = 0;
		if (!m_dialog.m_receiveCacheService->Initialize())
		{
			// 缓存初始化失败，提示用户并禁用保存
			m_dialog.WriteLog("断开连接后重新初始化接收缓存失败");
			m_dialog.MessageBox(
				_T("缓存初始化失败，可能导致数据无法正常接收和保存。\n请检查磁盘空间和权限设置。"),
				_T("缓存错误"),
				MB_OK | MB_ICONERROR
			);
			// 禁用保存按钮
			m_dialog.m_btnSaveAll.EnableWindow(FALSE);
			m_dialog.WriteLog("已禁用保存按钮以防止数据丢失");
		}
		else
		{
			// 缓存初始化成功，恢复保存按钮（如果需要）
			m_dialog.m_btnSaveAll.EnableWindow(TRUE);
		}
	}

	m_dialog.UpdateConnectionStatus();

	if (m_dialog.m_uiController)
	{
		m_dialog.m_uiController->UpdateConnectionButtons(false);
	}

	UpdatePortControlsEnabled(true);

	m_dialog.WriteLog("OnBnClickedButtonDisconnect: 断开连接完成");
}

void PortMasterDialogEvents::HandleSend()
{
	if (!m_dialog.m_isConnected)
	{
		m_dialog.MessageBox(_T("请先连接端口"), _T("提示"), MB_OK | MB_ICONWARNING);
		return;
	}

	// 【阶段C修复】检查模式切换标志，若为真则提示用户重新连接
	if (m_dialog.m_requiresReconnect)
	{
		m_dialog.MessageBox(_T("传输模式已切换，当前连接使用的是之前的模式。\n\n请先断开连接，然后重新连接以应用新模式。"),
			_T("需要重新连接"), MB_OK | MB_ICONWARNING);
		return;
	}

	if (!m_dialog.m_isTransmitting)
	{
		m_dialog.StartTransmission();
	}
	else if (m_dialog.m_transmissionPaused)
	{
		m_dialog.ResumeTransmission();
	}
	else
	{
		m_dialog.PauseTransmission();
	}
}

void PortMasterDialogEvents::HandleStop()
{
	if (m_dialog.m_transmissionCoordinator &&
		(m_dialog.m_transmissionCoordinator->IsRunning() || m_dialog.m_transmissionCoordinator->IsPaused()))
	{
		int result = m_dialog.MessageBox(_T("确认终止传输？"), _T("确认终止传输"), MB_YESNO | MB_ICONQUESTION);
		if (result == IDYES)
		{
			m_dialog.m_transmissionCoordinator->Cancel();
			m_dialog.m_transmissionCancelled = true;
			m_dialog.m_isTransmitting = false;
			m_dialog.m_transmissionPaused = false;

			// 【分类4.3修复】停止时保留接收缓存，允许用户保存已接收数据
			// 缓存清理应在：新文件加载、新连接建立、用户明确清除时执行
			// m_dialog.ClearAllCacheData();  // 已移除

			m_dialog.m_btnSend.SetWindowText(_T("发送"));
			m_dialog.m_staticPortStatus.SetWindowText(_T("传输已终止"));
			m_dialog.SetProgressPercent(0, true);

			if (m_dialog.m_uiController)
			{
				m_dialog.m_uiController->UpdateTransmissionButtons(false, false);
			}

			m_dialog.WriteLog("传输已被用户终止，接收缓存已保留，允许用户保存已接收数据");
		}
	}
	else
	{
		m_dialog.MessageBox(_T("没有正在进行的传输"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	}
}

void PortMasterDialogEvents::HandleSelectFile()
{
	CString filter;
	filter = _T("所有文件 (*.*)|*.*|")
		_T("文本文件 (*.txt;*.log;*.ini;*.cfg;*.conf)|*.txt;*.log;*.ini;*.cfg;*.conf|")
		_T("二进制文件 (*.bin;*.dat;*.exe)|*.bin;*.dat;*.exe|")
		_T("图像文件 (*.jpg;*.png;*.bmp;*.gif;*.tiff)|*.jpg;*.png;*.bmp;*.gif;*.tiff|")
		_T("压缩文件 (*.zip;*.rar;*.7z;*.tar;*.gz)|*.zip;*.rar;*.7z;*.tar;*.gz|")
		_T("文档文件 (*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx)|*.pdf;*.doc;*.docx;*.xls;*.xlsx;*.ppt;*.pptx|")
		_T("脚本文件 (*.bat;*.cmd;*.ps1;*.sh;*.py)|*.bat;*.cmd;*.ps1;*.sh;*.py|")
		_T("源代码 (*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css)|*.cpp;*.h;*.c;*.cs;*.java;*.js;*.html;*.css||");

	CFileDialog fileDlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST, filter);
	if (fileDlg.DoModal() != IDOK)
	{
		return;
	}

	LoadDataFromSelectedFile(fileDlg.GetPathName());
}

void PortMasterDialogEvents::HandleClearSend()
{
	m_dialog.m_editSendData.SetWindowText(_T(""));
	if (m_dialog.m_uiController)
	{
		m_dialog.m_uiController->UpdateSendSourceDisplay(_T("来源: 手动输入"));
	}
	// 【阶段4迁移】使用StatusDisplayManager显示日志
	if (m_dialog.m_statusDisplayManager)
	{
		m_dialog.m_statusDisplayManager->LogMessage(BuildTimestampPrefix() + _T("清空发送框"));
	}
}

void PortMasterDialogEvents::HandleClearReceive()
{
	m_dialog.m_editReceiveData.SetWindowText(_T(""));
	m_dialog.m_receiveDataCache.clear();
	m_dialog.m_receiveCacheValid = false;
	m_dialog.m_binaryDataDetected = false;
	m_dialog.m_binaryDataPreview.Empty();

	if (m_dialog.m_receiveCacheService)
	{
		m_dialog.m_receiveCacheService->Shutdown();
		m_dialog.m_totalReceivedBytes = 0;
		if (!m_dialog.m_receiveCacheService->Initialize())
		{
			m_dialog.WriteLog("清空接收缓存后重新初始化失败");
		}
	}

	// 【阶段4迁移】使用StatusDisplayManager显示日志
	if (m_dialog.m_statusDisplayManager)
	{
		m_dialog.m_statusDisplayManager->LogMessage(BuildTimestampPrefix() + _T("清空接收框"));
	}
}

void PortMasterDialogEvents::HandleCopyAll()
{
	CopyReceiveDataToClipboard();
}

void PortMasterDialogEvents::HandleSaveAll()
{
	SaveReceiveDataToFile();
}

void PortMasterDialogEvents::HandleToggleHex()
{
	// 十六进制显示模式切换
	// 读取切换后的勾选状态，通过UI控制器获取
	BOOL isHexMode = m_dialog.m_uiController->IsHexDisplayEnabled();

	// 获取当前编辑框中的内容
	CString currentSendData;
	if (m_dialog.m_uiController)
	{
		currentSendData = m_dialog.m_uiController->GetSendDataText();
	}
	else
	{
		currentSendData = _T("");
	}

	// 如果有内容，重新解析数据
	if (!currentSendData.IsEmpty())
	{
		if (isHexMode)
		{
			// 切换到十六进制模式：解析当前显示的内容
			// 如果当前显示的是文本，需要转换为十六进制
			if (!m_dialog.m_sendCacheValid)
			{
				// 缓存无效，直接缓存当前文本数据
				m_dialog.UpdateSendCache(currentSendData);
			}
			// 缓存有效，从缓存更新显示
		}
		else
		{
			// 切换到文本模式：如果缓存有效，直接使用缓存
			if (!m_dialog.m_sendCacheValid)
			{
				// 缓存无效，尝试从当前显示的十六进制数据中提取
				// 使用StringUtils替代MFC宏，避免缓冲区限制
				std::string hexStdString = StringUtils::Utf8EncodeWide(std::wstring(currentSendData));

				// 直接使用DataPresentationService
				std::vector<uint8_t> bytes = DataPresentationService::HexToBytes(hexStdString);

				// 将字节数组转换为UTF-8编码的CString
				CString textData;
				if (!bytes.empty())
				{
					// 使用StringUtils安全转换，避免MFC宏的缓冲区限制
					std::string bytesStr(reinterpret_cast<const char*>(bytes.data()), bytes.size());
					std::wstring utf8Result = StringUtils::WideEncodeUtf8(bytesStr);
					textData = CString(utf8Result.c_str());
				}
				if (!textData.IsEmpty())
				{
					m_dialog.UpdateSendCache(textData);
				}
				else
				{
					// 如果无法提取，假设当前显示的就是文本
					m_dialog.UpdateSendCache(currentSendData);
				}
			}
		}
	}
	// 如果编辑框为空，清空缓存
	else if (m_dialog.m_sendCacheValid)
	{
		m_dialog.m_sendDataCache.clear();
		m_dialog.m_sendCacheValid = false;
	}

	// 根据缓存更新显示（这会基于原始数据进行正确的格式转换）
	m_dialog.UpdateSendDisplayFromCache();

	// 只有在接收缓存有效时才更新接收显示，避免模式切换时错误填充接收框
	if (m_dialog.m_receiveCacheValid && !m_dialog.m_receiveDataCache.empty())
	{
		m_dialog.UpdateReceiveDisplayFromCache();
	}
}

void PortMasterDialogEvents::HandleDropFiles(HDROP hDropInfo)
{
	UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
	if (fileCount == 0)
	{
		DragFinish(hDropInfo);
		return;
	}

	TCHAR filePath[MAX_PATH] = { 0 };
	DragQueryFile(hDropInfo, 0, filePath, MAX_PATH);
	LoadDataFromSelectedFile(filePath);
	DragFinish(hDropInfo);
}

void PortMasterDialogEvents::ApplySendCacheFromHexDisplay(const CString& currentDisplay)
{
	// 使用StringUtils替代MFC宏，避免缓冲区限制
	std::string hexStdString = StringUtils::Utf8EncodeWide(std::wstring(currentDisplay));
	std::vector<uint8_t> bytes = DataPresentationService::HexToBytes(hexStdString);

	if (!bytes.empty())
	{
		// 使用StringUtils安全转换，消除内存风险
		std::string bytesStr(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		std::wstring utf8Result = StringUtils::WideEncodeUtf8(bytesStr);
		CString textData(utf8Result.c_str());
		if (!textData.IsEmpty())
		{
			m_dialog.UpdateSendCache(textData);
			return;
		}
	}

	m_dialog.UpdateSendCache(currentDisplay);
}

void PortMasterDialogEvents::CopyReceiveDataToClipboard()
{
	CString copyData;

	if (m_dialog.m_receiveCacheService && m_dialog.m_receiveCacheService->IsInitialized())
	{
		std::vector<uint8_t> cachedData = m_dialog.m_receiveCacheService->ReadAllData();
		if (!cachedData.empty())
		{
			if (m_dialog.m_checkHex.GetCheck() == BST_CHECKED)
			{
				std::string hexResult = DataPresentationService::BytesToHex(cachedData.data(), cachedData.size());
				copyData = CString(hexResult.c_str(), static_cast<int>(hexResult.length()));
			}
			else
			{
				try
				{
					// 使用StringUtils替代MFC宏，确保大数据量转换安全
					std::string utf8Data(cachedData.begin(), cachedData.end());
					std::wstring utf8Text = StringUtils::WideEncodeUtf8(utf8Data);
					copyData = CString(utf8Text.c_str());
				}
				catch (...)
				{
					for (uint8_t byte : cachedData)
					{
						if (byte >= 32 && byte <= 126)
						{
							copyData += (TCHAR)byte;
						}
						else if (byte == 0)
						{
							copyData += _T("[NUL]");
						}
						else
						{
							copyData += _T(".");
						}
					}
				}
			}
		}
	}
	else
	{
		m_dialog.m_editReceiveData.GetWindowText(copyData);
	}

	if (copyData.IsEmpty())
	{
		// 【分类4.2修复】错误时同步状态栏
		m_dialog.MessageBox(_T("没有可复制的数据"), _T("提示"), MB_OK | MB_ICONINFORMATION);
		m_dialog.m_staticPortStatus.SetWindowText(_T("剪贴板复制失败：无数据"));
		m_dialog.WriteLog("CopyReceiveDataToClipboard: 无可复制数据");
		return;
	}

	if (!OpenClipboard(m_dialog.GetSafeHwnd()))
	{
		// 【分类4.2修复】错误时同步状态栏
		m_dialog.MessageBox(_T("无法打开剪贴板"), _T("错误"), MB_OK | MB_ICONERROR);
		m_dialog.m_staticPortStatus.SetWindowText(_T("剪贴板操作失败"));
		m_dialog.WriteLog("CopyReceiveDataToClipboard: 无法打开剪贴板");
		return;
	}
	EmptyClipboard();

	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (copyData.GetLength() + 1) * sizeof(TCHAR));
	if (hGlobal)
	{
		LPTSTR pGlobal = static_cast<LPTSTR>(GlobalLock(hGlobal));
		if (pGlobal)
		{
			memcpy(pGlobal, copyData.GetString(), copyData.GetLength() * sizeof(TCHAR));
			pGlobal[copyData.GetLength()] = 0;
			GlobalUnlock(hGlobal);

#ifdef _UNICODE
			SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
			SetClipboardData(CF_TEXT, hGlobal);
#endif
		}
		else
		{
			GlobalFree(hGlobal);
		}
	}
	CloseClipboard();

	// 【分类4.2修复】成功时同步状态栏
	m_dialog.MessageBox(_T("接收数据已复制到剪贴板"), _T("提示"), MB_OK | MB_ICONINFORMATION);
	m_dialog.m_staticPortStatus.SetWindowText(_T("数据已复制到剪贴板"));
	m_dialog.WriteLog("CopyReceiveDataToClipboard: 数据复制成功");
}

void PortMasterDialogEvents::SaveReceiveDataToFile()
{
	CFileDialog saveDlg(FALSE, _T("txt"), _T("接收数据.txt"), OFN_OVERWRITEPROMPT, _T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||"));
	if (saveDlg.DoModal() != IDOK)
	{
		return;
	}

	CString filePath = saveDlg.GetPathName();
	if (filePath.IsEmpty())
	{
		return;
	}

	// 【阶段B修复】优先使用流式复制，避免大内存拷贝导致崩溃
	if (m_dialog.m_receiveCacheService && m_dialog.m_receiveCacheService->IsInitialized())
	{
		try
		{
			m_dialog.WriteLog("SaveReceiveDataToFile: 开始流式保存接收数据");

			size_t bytesWritten = 0;
			bool copySuccess = m_dialog.m_receiveCacheService->CopyToFile(
				std::wstring(filePath.GetString()),
				bytesWritten
			);

			if (copySuccess)
			{
				// 流式复制成功
				m_dialog.WriteLog("SaveReceiveDataToFile: 流式保存成功，字节数: " + std::to_string(bytesWritten));

				CString msg;
				if (bytesWritten < 1024)
				{
					msg.Format(_T("接收数据已保存到文件: %s (%zu 字节)"),
						static_cast<LPCTSTR>(filePath), bytesWritten);
				}
				else if (bytesWritten < 1024 * 1024)
				{
					msg.Format(_T("接收数据已保存到文件: %s (%.1f KB)"),
						static_cast<LPCTSTR>(filePath), bytesWritten / 1024.0);
				}
				else
				{
					msg.Format(_T("接收数据已保存到文件: %s (%.2f MB)"),
						static_cast<LPCTSTR>(filePath), bytesWritten / (1024.0 * 1024.0));
				}
				m_dialog.m_staticPortStatus.SetWindowText(msg);

				// 更新保存按钮状态（保存成功后启用按钮）
				if (m_dialog.m_uiController)
				{
					m_dialog.m_uiController->UpdateSaveButton(true);
				}

				m_dialog.MessageBox(_T("文件保存成功"), _T("提示"), MB_OK | MB_ICONINFORMATION);
				return;
			}
			else
			{
				// 【第七轮修复】流式复制失败时，直接提示错误而不使用编辑框回退
				m_dialog.WriteLog("❌ SaveReceiveDataToFile: 流式保存失败");
				m_dialog.MessageBox(_T("保存失败。请重新接收后再尝试。"), _T("错误"), MB_OK | MB_ICONERROR);
				if (m_dialog.m_uiController) m_dialog.m_uiController->UpdateSaveButton(false);
				return;
			}
		}
		catch (const std::exception& e)
		{
			// 【第七轮修复】异常处理 - 直接提示错误
			m_dialog.WriteLog("❌ SaveReceiveDataToFile: 异常 - " + std::string(e.what()));
			m_dialog.MessageBox(_T("保存失败。请重新接收后再尝试。"), _T("错误"), MB_OK | MB_ICONERROR);
			if (m_dialog.m_uiController) m_dialog.m_uiController->UpdateSaveButton(false);
			return;
		}
		catch (...)
		{
			// 【第七轮修复】未知异常处理
			m_dialog.WriteLog("❌ SaveReceiveDataToFile: 未知异常");
			m_dialog.MessageBox(_T("保存失败。请重新接收后再尝试。"), _T("错误"), MB_OK | MB_ICONERROR);
			if (m_dialog.m_uiController) m_dialog.m_uiController->UpdateSaveButton(false);
			return;
		}
	}


}

void PortMasterDialogEvents::LoadDataFromSelectedFile(const CString& filePath)
{
	CFile file;
	if (!file.Open(filePath, CFile::modeRead | CFile::shareDenyWrite))
	{
		m_dialog.MessageBox(_T("无法打开文件"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	// 【分类3.5修复】使用ULONGLONG支持超过4GB的文件
	ULONGLONG fileLength = file.GetLength();
	if (fileLength == 0)
	{
		file.Close();
		m_dialog.m_editSendData.SetWindowText(_T(""));
		m_dialog.m_staticSendSource.SetWindowText(_T("来源: 文件(空文件)"));
		return;
	}

	// 【分类3.5优化 + 第七轮修复】对超大文件的处理改进
	// 删除虚假的512MB限制声称，尝试加载整个文件（内存允许范围内）
	const size_t PREVIEW_SIZE = 32 * 1024;  // 预览大小32KB（UI显示）

	bool isLargeFile = fileLength > PREVIEW_SIZE;

	// 【第七轮修复】删除512MB人为限制，直接使用文件长度
	// 如果文件过大导致内存分配失败，将在 try/catch 中捕获并提示用户
	size_t readSize = static_cast<size_t>(fileLength);

	// 为了安全起见，仍添加一个合理的内存上限（2GB），超过此限制提示用户
	const size_t ABSOLUTE_LIMIT = 2ULL * 1024 * 1024 * 1024;  // 2GB 绝对上限
	if (fileLength > ABSOLUTE_LIMIT)
	{
		m_dialog.MessageBox(
			_T("文件过于巨大（>2GB），无法完整加载到内存。\n请选择较小的文件。"),
			_T("文件过大"), MB_OK | MB_ICONERROR);
		file.Close();
		return;
	}

	std::unique_ptr<char[]> fileBuffer;
	try
	{
		fileBuffer = std::make_unique<char[]>(readSize);
	}
	catch (const std::bad_alloc&)
	{
		m_dialog.MessageBox(
			_T("内存不足，无法加载此文件。\n请选择较小的文件或增加系统可用内存。"),
			_T("内存不足"), MB_OK | MB_ICONERROR);
		m_dialog.WriteLog("LoadDataFromSelectedFile: 内存分配失败，文件大小超过可用内存");
		file.Close();
		return;
	}
	UINT bytesRead = file.Read(fileBuffer.get(), static_cast<UINT>(readSize));
	file.Close();

	if (bytesRead == 0)
	{
		m_dialog.MessageBox(_T("无法读取文件内容"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

	// 二进制数据检测：检查是否包含大量不可打印字符
	const size_t SAMPLE_SIZE = min(static_cast<size_t>(bytesRead), static_cast<size_t>(4096));
	size_t nonPrintableCount = 0;
	size_t nullByteCount = 0;
	for (size_t i = 0; i < SAMPLE_SIZE; ++i)
	{
		uint8_t byte = static_cast<uint8_t>(fileBuffer[i]);
		if (byte == 0)
		{
			nullByteCount++;
		}
		else if (byte < 32 && byte != '\r' && byte != '\n' && byte != '\t')
		{
			nonPrintableCount++;
		}
	}
	bool isBinaryFile = (nullByteCount > 0) || ((nonPrintableCount * 100 / SAMPLE_SIZE) > 20);

	// 当数据较大时启用预览模式，只显示前32KB内容
	size_t displaySize = isLargeFile ? PREVIEW_SIZE : static_cast<size_t>(bytesRead);

	if (isBinaryFile)
	{
		// 二进制文件：直接缓存完整原始字节数据
		// 【分类3.5修复】传入实际读取的字节数，而非原始文件长度（大文件仅预览）
		m_dialog.UpdateSendCacheFromBytes(reinterpret_cast<const BYTE*>(fileBuffer.get()), (size_t)bytesRead);
		m_dialog.UpdateSendDisplayFromCache();
	}
	else
	{
		// 文本文件：使用UTF-8编码解析（项目统一编码标准）
		try
		{
			// 使用安全的字符串转换方法，避免内存分配失败和编码错误
			std::string fileContent(fileBuffer.get(), displaySize);

			// 检查字符串长度是否安全
			if (!StringUtils::IsStringLengthSafe(fileContent))
			{
				m_dialog.MessageBox(_T("文件内容过大，无法加载"), _T("错误"), MB_OK | MB_ICONERROR);
				m_dialog.WriteLog("OnBnClickedButtonLoadFile: 文件内容超过安全长度限制");
				return;
			}

			// 使用UTF-8编码转换为宽字符串
			std::wstring wideContent = StringUtils::SafeMultiByteToWideChar(fileContent, CP_UTF8);

			// 如果UTF-8转换失败，尝试使用系统本地编码（兼容性考虑）
			if (wideContent.empty() && !fileContent.empty())
			{
				m_dialog.WriteLog("OnBnClickedButtonLoadFile: UTF-8转换失败，尝试系统本地编码");
				wideContent = StringUtils::SafeMultiByteToWideChar(fileContent, CP_ACP);
			}

			// 转换失败则使用回退方案：直接字符拷贝
			CString previewContent;
			if (!wideContent.empty())
			{
				previewContent = wideContent.c_str();
			}
			else
			{
				m_dialog.WriteLog("OnBnClickedButtonLoadFile: 编码转换失败，使用ASCII回退方案");
				for (size_t i = 0; i < displaySize; ++i)
				{
					previewContent += static_cast<TCHAR>(fileBuffer[i]);
				}
			}

			m_dialog.UpdateSendCache(previewContent);
			if (m_dialog.m_checkHex.GetCheck() == BST_CHECKED)
			{
				// 使用StringUtils替代MFC宏，避免缓冲区限制
				std::string utf8Str = StringUtils::Utf8EncodeWide(std::wstring(previewContent));
				std::string hexResult = DataPresentationService::BytesToHex(
					reinterpret_cast<const uint8_t*>(utf8Str.c_str()),
					utf8Str.length());
				m_dialog.m_editSendData.SetWindowText(CString(hexResult.c_str(), static_cast<int>(hexResult.length())));
			}
			else
			{
				m_dialog.m_editSendData.SetWindowText(previewContent);
			}

			// 大文件需要缓存完整内容
			if (isLargeFile)
			{
				// 【分类3.5修复】使用实际读取的字节数而非文件长度
				std::string fullFileContent(fileBuffer.get(), bytesRead);

				// 检查完整文件长度是否安全
				if (!StringUtils::IsStringLengthSafe(fullFileContent))
				{
					m_dialog.WriteLog("OnBnClickedButtonLoadFile: 完整文件内容超过安全长度限制，仅缓存预览部分");
				}
				else
				{
					// 使用UTF-8编码转换完整内容
					std::wstring fullWideContent = StringUtils::SafeMultiByteToWideChar(fullFileContent, CP_UTF8);

					// UTF-8转换失败则尝试系统本地编码
					if (fullWideContent.empty() && !fullFileContent.empty())
					{
						fullWideContent = StringUtils::SafeMultiByteToWideChar(fullFileContent, CP_ACP);
					}

					if (!fullWideContent.empty())
					{
						m_dialog.UpdateSendCache(fullWideContent.c_str());
						m_dialog.WriteLog("OnBnClickedButtonLoadFile: 大文件完整内容已缓存");
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			// 异常保护：确保程序不会因编码转换失败而崩溃
			CString errorMsg;
			errorMsg.Format(_T("文件编码转换失败: %s"), CString(e.what()));
			m_dialog.MessageBox(errorMsg, _T("错误"), MB_OK | MB_ICONERROR);
			m_dialog.WriteLog("OnBnClickedButtonLoadFile: 编码转换异常 - " + std::string(e.what()));
			return;
		}
		catch (...)
		{
			m_dialog.MessageBox(_T("文件编码转换时发生未知错误"), _T("错误"), MB_OK | MB_ICONERROR);
			m_dialog.WriteLog("OnBnClickedButtonLoadFile: 编码转换未知异常");
			return;
		}
	}

	if (m_dialog.m_uiController)
	{
		CString sizeInfo;
		if (isLargeFile)
		{
			if (fileLength < 1024 * 1024)
			{
				sizeInfo.Format(_T("来源: 文件 (%.1fKB, 大数据文件预览-部分内容)"), fileLength / 1024.0);
			}
			else
			{
				sizeInfo.Format(_T("来源: 文件 (%.1fMB, 大数据文件预览-部分内容)"), fileLength / (1024.0 * 1024.0));
			}
		}
		else
		{
			sizeInfo = _T("来源: 文件");
		}
		m_dialog.m_uiController->UpdateSendSourceDisplay(sizeInfo);
	}

	// 【阶段4迁移】使用StatusDisplayManager显示日志
	if (m_dialog.m_statusDisplayManager)
	{
		if (isLargeFile)
		{
			m_dialog.m_statusDisplayManager->LogMessage(BuildTimestampPrefix() + _T("大文件加载完成，显示前32KB内容预览"));
		}

		if (!isBinaryFile && m_dialog.m_checkHex.GetCheck() == BST_UNCHECKED)
		{
			m_dialog.m_statusDisplayManager->LogMessage(_T("提示: 当前为文本数据，建议在文本模式下查看"));
		}
	}
}

void PortMasterDialogEvents::ShowBinaryPreviewNotice()
{
	// 【阶段4迁移】使用StatusDisplayManager显示日志
	if (m_dialog.m_binaryDataDetected && m_dialog.m_statusDisplayManager)
	{
		m_dialog.m_statusDisplayManager->LogMessage(_T("警告: 当前数据包含二进制内容，建议在十六进制模式查看"));
	}
}