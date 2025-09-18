#pragma once

#pragma execution_character_set("utf-8")

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'stdafx.h' 以生成 PCH"
#endif

#include "resource.h"       // 主符号

/// <summary>
/// PortMaster应用程序类
/// </summary>
class CPortMasterApp : public CWinApp
{
public:
	CPortMasterApp();

// 重写
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 实现
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()

private:
	// 单实例检查
	HANDLE m_hMutex;
	bool CheckSingleInstance();
	void ReleaseSingleInstance();

	// 启动画面
	void ShowSplashScreen();
	void HideSplashScreen();
};