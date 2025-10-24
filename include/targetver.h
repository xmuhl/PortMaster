#pragma once

// 包括 SDKDDKVer.h 将定义可用的最高版本的 Windows 平台。
// 如果要为以前的 Windows 平台生成应用程序，请包括 WinSDKVer.h，
// 并在包括 SDKDDKVer.h 之前将 _WIN32_WINNT 宏设置为要支持的平台。

// Windows 7 及更高版本兼容性
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7
#endif

#include <SDKDDKVer.h>
