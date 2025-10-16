// PortMasterDialogEvents.h - 对话框事件调度模块
#pragma once

#include <shellapi.h>

class CPortMasterDlg;

class PortMasterDialogEvents
{
public:
	explicit PortMasterDialogEvents(CPortMasterDlg& dialog);

	void HandleConnect();
	void HandleDisconnect();
	void HandleSend();
	void HandleStop();
	void HandleSelectFile();
	void HandleClearSend();
	void HandleClearReceive();
	void HandleCopyAll();
	void HandleSaveAll();
	void HandleToggleHex();
	void HandleSelectReliable();
	void HandleSelectDirect();
	void HandleSendDataChanged();
	void HandleDropFiles(HDROP dropInfo);

private:
	CPortMasterDlg& m_dialog;

	CString BuildTimestampPrefix() const;
	void UpdatePortControlsEnabled(bool enabled);
	void ApplySendCacheFromHexDisplay(const CString& currentDisplay);
	void CopyReceiveDataToClipboard();
	void SaveReceiveDataToFile();
	void LoadDataFromSelectedFile(const CString& filePath);
	void ShowBinaryPreviewNotice();
};
