/************************************************************************

FileName:	IOControl.h

Copyright (c) 2009-2010

Writer:	黄磊

create Date:	2009.04.01

Rewriter:

Rewrite Date:

Impact:	设备IO端口控制功能模块预定义头文件及常量

Main Content:   OpenDevice()
				ReadPort()
				WritePort()
				ClosePort()

/************************************************************************/
#pragma  once

#include "Setupapi.h"
#include <WinIoCtl.h>
#include "Resource.h"

// USB 设备GUID Identifier GUID_DEVINTERFACE_USB_DEVICE Class GUID
// {A5DCBF10-6530- 11D2-901F-00C04FB951ED}
#define  USB_GUID \
 { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } }

#define GUID_CLASS_I82930_BULK  {0x28d78fad, 0x5a12, 0x11d1,0xae,0x5b , 0x00, 0x00, 0xf8,0x03 ,0xa8 , 0xc2}

#define USBPRINT_IOCTL_INDEX  0x0000
#define IOCTL_USBPRINT_GET_1284_ID     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USBPRINT_IOCTL_INDEX + 13, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define MS_TIMEOUT		10000			// 等待超时时间改为10秒
#define MS_TIMEOUT2     10000				// 读取数据等待改为10秒
#define REC_BUFFER		1024				// 读取数据缓存大小
#define SLEEP_TIME		2000				// 读取回送数据间隔时间
#define MAX_RECDATA  4096		// 最大监视读取数据缓存大小4K
#define PACKET_SIZE 128				// 数据包大小

// 打开指定打印机设备的USB端口，如果打开成功，则返回该端口句柄，否则返回NULL,通过总线关系读取打印机设备
HANDLE    OpenUSBPort(CString& StartPrintName, LPCTSTR VIDPID);

HANDLE OpenUSBPort(void);
//打开打印机USB端口，只返回句柄
HANDLE  InitPrinter(void);

// 关闭打开的设备端口，不返回值
void	 CloseUSBPort(HANDLE hUsbPort);

//从USB读取回来的ID解析出打印机型号
LPTSTR ResolveUsbPrintId(CString PrintId);
//从USB读取回来的ID解析出仿真方式
LPTSTR ResolveUsbPrintCommand(CString PrintId);
//从USB读取回来的ID解析出产家名称
LPTSTR ResolveUsbPrintProvider(CString Provider);
int ReadPort(HWND hwnd, UCHAR* recBuffer, int iLen, int iWaitTime, HANDLE gm_DeviceHandle);
int TryWriteData(HWND hwnd, HANDLE hDevice, CHAR* buffer, int len, int waitTime, LPOVERLAPPED lpOverlapped);
int WritePort(HWND hwnd, CHAR* sendBuffer, int iLen, int iWaitTime, HANDLE gm_DeviceHandle);
BOOL OutPutDataToPort(HWND hwnd, char* strOutputData, DWORD uDataLength, DWORD dwTimeout, HANDLE gm_DeviceHandle);