/************************************************************************

FileName:	IOControl.cpp

Copyright (c) 2009-2010

Writer:	黄磊

create Date:	2009.04.01

Impact:	设备IO端口控制功能模块

Main Content:
				OpenDevice()
				ReadPort()
				WritePort()
				ClosePort()

UpdateDate:

2009.06.17: 新增自定义消息，当通信错误时，发送消息通知主窗口停止监视并关闭端口

/************************************************************************/

#include "stdafx.h"
#include "IOControl.h"
#include "UDefault.h"

#include "DebugStuff.h"
#include "GetSysVersion.h"
#include "StrTranslate.h"
//extern HANDLE gm_PortHandle;

// 开始查找并打开USB设备
HANDLE  OpenUSBPort(void)
{
	HANDLE hUsbPort = INVALID_HANDLE_VALUE;				// 初始化一个无效句柄

	ULONG NumberDeviceInterfaces, NumSelDeviceInfaces;
	HDEVINFO					hardwareDeviceInfo;								// 设备信息集数据结构
	SP_DEVINFO_DATA				deviceInfoData;								// 指定接口的设备信息数据结构
	SP_INTERFACE_DEVICE_DATA	deviceInterfaceData;		// 指定设备接口数据结构

	NumberDeviceInterfaces = 0;														// 所有连接到本机上的设备USB设备数目
	NumSelDeviceInfaces = 0;																// 指定的设备数目

	GUID ClassGuid = GUID_CLASS_I82930_BULK;
	LPGUID  pGuid = &ClassGuid;

	hardwareDeviceInfo = SetupDiGetClassDevs(pGuid,
		NULL, NULL, (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE));	// 根据指定的USB设备的GUID获取设备信息集

	if (hardwareDeviceInfo == INVALID_HANDLE_VALUE)								// 无法获取指定设备的设备集信息，弹出错误提示框
	{
		//AfxMessageBox(_T("无法获取指定设备信息集!"), MB_ICONERROR);
		// 翻译成英文
		//AfxMessageBox(_T("Unable to get specified device information set!"), MB_ICONERROR);
		 //改为加载字符串资源
		CString strMes = LoadLocalString(IDS_ERR_DEVICE_INFO_SET);
		AfxMessageBox(strMes, MB_ICONERROR);

		SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);

		return	hUsbPort;
	}
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);			// 设定缓冲以容纳被访问接口的设备信息
	deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);	// 设定足够大的缓冲区以容纳完整的接口信息结构体

	// 循环调用枚举设备接口函数SetupDiEnumDeviceInterfaces查找当前连接在主机上的设备接口，
	// 直到函数返回值false,无法查询到更多接口。
	BOOLEAN bGetDevInface = TRUE;

	CString SelDevicePath;	// 指定要操作的设备端口路径
	CString strPortDescription;
	CString strPortNumber;

	do
	{
		bGetDevInface = SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL, pGuid, \
			NumberDeviceInterfaces, &deviceInterfaceData);

		if (bGetDevInface)
		{
			// 如果获得指向设备接口数据结构，则进一步读取设备接口详细信息

			// 第一次调用SetupDiGetInterfaceDeviceDetail,获得用于存放接口详细信息数据结构的缓冲区大小
			PSP_INTERFACE_DEVICE_DETAIL_DATA pDeviceInterfaceDetailData = NULL;
			ULONG predictedLength = 0;
			ULONG requireLength = 0;

			SetupDiGetInterfaceDeviceDetail(hardwareDeviceInfo, &deviceInterfaceData, NULL, \
				0, &requireLength, &deviceInfoData);

			predictedLength = requireLength;

			pDeviceInterfaceDetailData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(predictedLength);
			pDeviceInterfaceDetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

			// 再次调用SetupDiGetInterfaceDeviceDetail，获得接口详细信息
			if (SetupDiGetInterfaceDeviceDetail(hardwareDeviceInfo, \
				& deviceInterfaceData, pDeviceInterfaceDetailData, \
				predictedLength, &requireLength, &deviceInfoData))
			{
				CString strDevicePath = pDeviceInterfaceDetailData->DevicePath;

				// 判断当前接口是否为我们要连接的USB接口；
				// 如果是指定的USB接口，则打开端口，准备用于读写操作；
				// 如果不是指定的usb接口，则继续搜索直到搜索完全部USB端口。

				CString strMessage = _T("当前设备接口的详细信息: ") + strDevicePath;
				DebugMesBox(strMessage);

				//if (strDevicePath.Find(VIDPID) != -1)					// 找到匹配的字符串
				//{	// 1
				NumSelDeviceInfaces++;
				SelDevicePath = pDeviceInterfaceDetailData->DevicePath;

				// 通过注册表查询更多硬件信息
				HKEY hDeviceInfo = NULL;
				CString strHkeyName;
				CString strGuid;
				strGuid = _T("{28d78fad-5a12-11d1-ae5b-0000f803a8c2}");
				strHkeyName = _T("SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\");
				strHkeyName = strHkeyName + strGuid;

				CString strTemp = strDevicePath;
				strTemp.Replace(_T("\\?\\"), _T("##?#"));
				strHkeyName = strHkeyName + strTemp;

				strHkeyName = strHkeyName + _T("\\");
				strHkeyName = strHkeyName + _T("#");
				strHkeyName = strHkeyName + _T("\\");
				strHkeyName = strHkeyName + _T("Device Parameters");

				// 通过注册表内容获取当前设备链接端口号

				LONG lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strHkeyName, 0, KEY_READ, &hDeviceInfo);

				if (lResult == ERROR_SUCCESS)
				{ // 2
					LPBYTE getValue = new BYTE[256];			//得到的键值
					DWORD keyType = REG_SZ;						//定义数据类型
					DWORD DataLen = 256;									//定义数据长度
					CString strUser = _T("Port Description");						//要查询的键名称
					ZeroMemory(getValue, 256);

					long ret1 = RegQueryValueEx(hDeviceInfo, strUser, NULL, &keyType, getValue, &DataLen);
					int i = GetLastError();
					if (ret1 != ERROR_SUCCESS)
					{ // 3
						//AfxMessageBox(_T("错误：无法读取注册表信息"));
						// 翻译成英文
						//AfxMessageBox(_T("Error: Unable to read registry information"));

						// 改为加载字符串资源
						CString strMes = LoadLocalString(IDS_ERR_READ_REGISTRY);
						AfxMessageBox(strMes);

						delete[] getValue;
						getValue = NULL;

						free(pDeviceInterfaceDetailData);

						return FALSE;
					} //3
					else				// 读取注册表键值成功，校验是否为指定的硬件型号信息
					{	//3
						TCHAR print[256] = { 0 };
						wcscpy_s(print, 256, (LPCTSTR)getValue);
						//StartPrintName = print;
						// 获取当前端口信息
						strPortDescription = print;
					}  // 3 //
					delete[]getValue;
					getValue = NULL;

					// 读取“Port Number”键的值（DWORD类型）
					DWORD dwPortNumber = 0;
					DWORD dwPortNumberLen = sizeof(DWORD);
					DWORD dwPortNumberType = REG_DWORD;

					long ret2 = RegQueryValueEx(hDeviceInfo, _T("Port Number"), NULL, &dwPortNumberType, (LPBYTE)&dwPortNumber, &dwPortNumberLen);

					if (ret2 != ERROR_SUCCESS)
					{
						//AfxMessageBox(_T("错误：无法读取注册表信息"));
						// 翻译成英文
						//AfxMessageBox(_T("Error: Unable to read registry information"));
						// 改为加载字符串资源
						CString strMes = LoadLocalString(IDS_ERR_READ_REGISTRY);
						AfxMessageBox(strMes);

						free(pDeviceInterfaceDetailData);

						return FALSE;
					}
					else
					{
						strPortNumber.Format(_T("当前设备链接端口号: USB%.03d"), dwPortNumber);
						//DebugMesBox(strMessage);
					}

					RegCloseKey(hDeviceInfo);  // 关闭注册表键
				}	// 2 //
				else
				{
					//AfxMessageBox(_T("错误：打开指定设备类的注册表项"));
					// 翻译成英文
					//AfxMessageBox(_T("Error: Open the registry entry for the specified device class"));
					// 改为加载字符串资源
					CString strMes = LoadLocalString(IDS_ERR_OPEN_REGISTRY);
					AfxMessageBox(strMes);

					free(pDeviceInterfaceDetailData);
					return FALSE;
				}

				break;  // 退出枚举循环
				//}	// 1 //
			}   // 调用调用SetupDiGetInterfaceDeviceDetail获取当前设备接口详细信息

			free(pDeviceInterfaceDetailData);	// 释放存放当前接口详细信息的缓存区

			NumberDeviceInterfaces++;
		}
	} while (bGetDevInface);

	if (NumSelDeviceInfaces != 0)
	{
		// 以异步方式打开指定的设备端口
		hUsbPort = CreateFile(SelDevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

		if (INVALID_HANDLE_VALUE != hUsbPort)
		{
			// 组织并提示当前端口信息
		/*	CString strMessage = _T("打开指定的设备USB端口成功! \r\n \r\n当前设备名称: ") + strPortDescription;
			strMessage = strMessage + _T("\r\n") + strPortNumber;
			AfxMessageBox(strMessage);*/

			DebugMesBox(_T("打开指定的设备USB端口成功!"));
		}
		else
		{
			DebugMesBox(_T("找到连接主机的USB设备但在打开USB端口时发生错误!"));
		}
	}
	else
	{
		DebugMesBox(_T(" 查询全部USB端口完毕，但无法找到指定的设备! \r\n \r\n请确认是否已将设备连接到主机的USB端口!"));
	}

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);		// 清空枚举后的设备信息列表

	return hUsbPort;
}

// 开始查找并打开指定USB设备
HANDLE  OpenUSBPort(CString& StartPrintName, LPCTSTR VIDPID)//存放实达打印机型号
{
	HANDLE hUsbPort = INVALID_HANDLE_VALUE;				// 初始化一个无效句柄

	ULONG NumberDeviceInterfaces, NumSelDeviceInfaces;
	HDEVINFO					hardwareDeviceInfo;								// 设备信息集数据结构
	SP_DEVINFO_DATA				deviceInfoData;								// 指定接口的设备信息数据结构
	SP_INTERFACE_DEVICE_DATA	deviceInterfaceData;		// 指定设备接口数据结构

	NumberDeviceInterfaces = 0;														// 所有连接到本机上的设备USB设备数目
	NumSelDeviceInfaces = 0;																// 指定的设备数目

	GUID ClassGuid = GUID_CLASS_I82930_BULK;
	LPGUID  pGuid = &ClassGuid;

	hardwareDeviceInfo = SetupDiGetClassDevs(pGuid,
		NULL, NULL, (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE));	// 根据指定的USB设备的GUID获取设备信息集

	if (hardwareDeviceInfo == INVALID_HANDLE_VALUE)								// 无法获取指定设备的设备集信息，弹出错误提示框
	{
		//AfxMessageBox(_T("无法获取指定设备信息集!"), MB_ICONERROR);
		// 翻译成英文
		//AfxMessageBox(_T("Unable to get specified device information set!"), MB_ICONERROR);
		// 改为加载字符串资源
		CString strMes = LoadLocalString(IDS_ERR_DEVICE_INFO_SET);
		AfxMessageBox(strMes, MB_ICONERROR);

		SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);

		return	hUsbPort;
	}
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);			// 设定缓冲以容纳被访问接口的设备信息
	deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);	// 设定足够大的缓冲区以容纳完整的接口信息结构体

	// 循环调用枚举设备接口函数SetupDiEnumDeviceInterfaces查找当前连接在主机上的设备接口，
	// 直到函数返回值false,无法查询到更多接口。
	BOOLEAN bGetDevInface = TRUE;

	CString SelDevicePath;	// 指定要操作的设备端口路径

	do
	{
		bGetDevInface = SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL, pGuid, \
			NumberDeviceInterfaces, &deviceInterfaceData);

		if (bGetDevInface)
		{
			// 如果获得指向设备接口数据结构，则进一步读取设备接口详细信息

			// 第一次调用SetupDiGetInterfaceDeviceDetail,获得用于存放接口详细信息数据结构的缓冲区大小
			PSP_INTERFACE_DEVICE_DETAIL_DATA pDeviceInterfaceDetailData = NULL;
			ULONG predictedLength = 0;
			ULONG requireLength = 0;

			SetupDiGetInterfaceDeviceDetail(hardwareDeviceInfo, &deviceInterfaceData, NULL, \
				0, &requireLength, &deviceInfoData);

			predictedLength = requireLength;

			pDeviceInterfaceDetailData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(predictedLength);
			pDeviceInterfaceDetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

			// 再次调用SetupDiGetInterfaceDeviceDetail，获得接口详细信息
			if (SetupDiGetInterfaceDeviceDetail(hardwareDeviceInfo, \
				& deviceInterfaceData, pDeviceInterfaceDetailData, \
				predictedLength, &requireLength, &deviceInfoData))
			{
				CString strDevicePath = pDeviceInterfaceDetailData->DevicePath;

				// 判断当前接口是否为我们要连接的USB接口；
				// 如果是指定的USB接口，则打开端口，准备用于读写操作；
				// 如果不是指定的usb接口，则继续搜索直到搜索完全部USB端口。

				CString strMessage = _T("当前设备接口的详细信息: ") + strDevicePath;
				DebugMesBox(strMessage);

				if (strDevicePath.Find(VIDPID) != -1)					// 找到匹配的字符串
				{	// 1
					NumSelDeviceInfaces++;
					SelDevicePath = pDeviceInterfaceDetailData->DevicePath;

					// 通过注册表查询更多硬件信息
					HKEY hDeviceInfo = NULL;
					CString strHkeyName;
					CString strGuid;
					strGuid = _T("{28d78fad-5a12-11d1-ae5b-0000f803a8c2}");
					strHkeyName = _T("SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\");
					strHkeyName = strHkeyName + strGuid;

					CString strTemp = strDevicePath;
					strTemp.Replace(_T("\\?\\"), _T("##?#"));
					strHkeyName = strHkeyName + strTemp;

					strHkeyName = strHkeyName + _T("\\");
					strHkeyName = strHkeyName + _T("#");
					strHkeyName = strHkeyName + _T("\\");
					strHkeyName = strHkeyName + _T("Device Parameters");

					LONG lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strHkeyName, 0, KEY_READ, &hDeviceInfo);

					if (lResult == ERROR_SUCCESS)
					{ // 2
						LPBYTE getValue = new BYTE[256];			//得到的键值
						DWORD keyType = REG_SZ;						//定义数据类型
						DWORD DataLen = 256;									//定义数据长度
						CString strUser = _T("Port Description");						//要查询的键名称
						ZeroMemory(getValue, 256);

						long ret1 = RegQueryValueEx(hDeviceInfo, strUser, NULL, &keyType, getValue, &DataLen);
						int i = GetLastError();
						if (ret1 != ERROR_SUCCESS)
						{ // 3
							//AfxMessageBox(_T("错误：无法读取注册表信息"));
							// 翻译成英文
							//AfxMessageBox(_T("Error: Unable to read registry information"));
							// 改为加载字符串资源
							CString strMes = LoadLocalString(IDS_ERR_READ_REGISTRY);
							AfxMessageBox(strMes);

							delete[] getValue;
							getValue = NULL;

							free(pDeviceInterfaceDetailData);

							return FALSE;
						} //3
						else				// 读取注册表键值成功，校验是否为指定的硬件型号信息
						{	//3
							TCHAR print[256] = { 0 };
							wcscpy_s(print, 256, (LPCTSTR)getValue);
							StartPrintName = print;

							//if(strPortDescription.Find(STR_DEVICE_MODEL) != -1)     // 找到指定的硬件型号
							//{ // 4
							/*	NumSelDeviceInfaces++;
								SelDevicePath = pDeviceInterfaceDetailData->DevicePath;*/
								//} // 4 //
						}  // 3 //
						delete[]getValue;
						getValue = NULL;
					}	// 2 //
					else
					{
						//AfxMessageBox(_T("错误：打开指定设备类的注册表项"));
						// 翻译成英文
						//AfxMessageBox(_T("Error: Open the registry entry for the specified device class"));
						// 改为加载字符串资源
						CString strMes = LoadLocalString(IDS_ERR_OPEN_REGISTRY);
						AfxMessageBox(strMes);

						free(pDeviceInterfaceDetailData);
						return FALSE;
					}
				}	// 1 //
			}   // 调用调用SetupDiGetInterfaceDeviceDetail获取当前设备接口详细信息

			free(pDeviceInterfaceDetailData);	// 释放存放当前接口详细信息的缓存区

			NumberDeviceInterfaces++;
		}
	} while (bGetDevInface);

	if (ERROR_NO_MORE_ITEMS != GetLastError())
	{
		//AfxMessageBox(_T("调用SetupDiEnumDeviceInterfaces枚举函数错误，无法获得设备接口详细信息!"), MB_ICONERROR);
		// 翻译成英文
		//AfxMessageBox(_T("Error calling SetupDiEnumDeviceInterfaces enumeration function, unable to get device interface details!"), MB_ICONERROR);
		// 改为加载字符串资源
		CString strMes = LoadLocalString(IDS_ERR_ENUM_DEVICE);
		AfxMessageBox(strMes, MB_ICONERROR);
	}
	else
	{
		if (NumSelDeviceInfaces != 0)
		{
			// 以异步方式打开指定的设备端口
			hUsbPort = CreateFile(SelDevicePath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

			if (INVALID_HANDLE_VALUE != hUsbPort)
			{
				DebugMesBox(_T("打开指定的设备USB端口成功!"));
			}
			else
			{
				DebugMesBox(_T("找到连接主机的USB设备但在打开USB端口时发生错误!"));
			}
		}
		else
		{
			DebugMesBox(_T(" 查询全部USB端口完毕，但无法找到指定的设备! \r\n \r\n请确认是否已将设备连接到主机的USB端口!"));
		}
	}

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);		// 清空枚举后的设备信息列表

	return hUsbPort;
}

/*打开端口函数*/
HANDLE  InitPrinter(void)
{
	HANDLE hUsbPort = INVALID_HANDLE_VALUE;				// 初始化一个无效句柄

	ULONG NumberDeviceInterfaces, NumSelDeviceInfaces;
	HDEVINFO					hardwareDeviceInfo;								// 设备信息集数据结构
	SP_DEVINFO_DATA				deviceInfoData;								// 指定接口的设备信息数据结构
	SP_INTERFACE_DEVICE_DATA	deviceInterfaceData;		// 指定设备接口数据结构

	NumberDeviceInterfaces = 0;														// 所有连接到本机上的设备USB设备数目
	NumSelDeviceInfaces = 0;																// 指定的设备数目

	GUID ClassGuid = GUID_CLASS_I82930_BULK;
	LPGUID  pGuid = &ClassGuid;

	hardwareDeviceInfo = SetupDiGetClassDevs(pGuid,
		NULL, NULL, (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE));	// 根据指定的USB设备的GUID获取设备信息集

	if (hardwareDeviceInfo == INVALID_HANDLE_VALUE)								// 无法获取指定设备的设备集信息，弹出错误提示框
	{
		DebugMesBox(_T("无法获取指定设备信息集!"));

		SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);

		return	hUsbPort;
	}
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);			// 设定缓冲以容纳被访问接口的设备信息
	deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);	// 设定足够大的缓冲区以容纳完整的接口信息结构体

	// 循环调用枚举设备接口函数SetupDiEnumDeviceInterfaces查找当前连接在主机上的设备接口，
	// 直到函数返回值false,无法查询到更多接口。
	BOOLEAN bGetDevInface = TRUE;

	CString SelDevicePath;	// 指定要操作的设备端口路径

	do
	{
		bGetDevInface = SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL, pGuid, \
			NumberDeviceInterfaces, &deviceInterfaceData);

		if (bGetDevInface)
		{
			// 如果获得指向设备接口数据结构，则进一步读取设备接口详细信息

			// 第一次调用SetupDiGetInterfaceDeviceDetail,获得用于存放接口详细信息数据结构的缓冲区大小
			PSP_INTERFACE_DEVICE_DETAIL_DATA pDeviceInterfaceDetailData = NULL;
			ULONG predictedLength = 0;
			ULONG requireLength = 0;

			SetupDiGetInterfaceDeviceDetail(hardwareDeviceInfo, &deviceInterfaceData, NULL, \
				0, &requireLength, &deviceInfoData);

			predictedLength = requireLength;

			pDeviceInterfaceDetailData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(predictedLength);
			pDeviceInterfaceDetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

			// 再次调用SetupDiGetInterfaceDeviceDetail，获得接口详细信息
			if (SetupDiGetInterfaceDeviceDetail(hardwareDeviceInfo, \
				& deviceInterfaceData, pDeviceInterfaceDetailData, \
				predictedLength, &requireLength, &deviceInfoData))
			{
				CString strDevicePath = pDeviceInterfaceDetailData->DevicePath;

				// 判断当前接口是否为我们要连接的USB接口；
				// 如果是指定的USB接口，则打开端口，准备用于读写操作；
				// 如果不是指定的usb接口，则继续搜索直到搜索完全部USB端口。

				CString strMessage = _T("当前设备接口的详细信息: ") + strDevicePath;
				//DebugMesBox(strMessage);

				//if (strDevicePath.Find(VIDPID) != -1)					// 找到匹配的字符串
				//{	// 1
				NumSelDeviceInfaces++;
				SelDevicePath = pDeviceInterfaceDetailData->DevicePath;
				//}	// 1 //
			}   // 调用调用SetupDiGetInterfaceDeviceDetail获取当前设备接口详细信息

			free(pDeviceInterfaceDetailData);	// 释放存放当前接口详细信息的缓存区

			NumberDeviceInterfaces++;
		}
	} while (bGetDevInface);

	if (ERROR_NO_MORE_ITEMS != GetLastError())
	{
		DebugMesBox(_T("调用SetupDiEnumDeviceInterfaces枚举函数错误，无法获得设备接口详细信息!"));
	}
	else
	{
		if (NumSelDeviceInfaces != 0)
		{
			// 以异步方式打开指定的设备端口
			hUsbPort = CreateFile(SelDevicePath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		}
		else
		{
			//DebugMesBox(_T(" 查询全部USB端口完毕，但无法找到指定的设备! \r\n \r\n请确认是否已将设备连接到主机的USB端口!"));
		}
	}

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);		// 清空枚举后的设备信息列表
	return hUsbPort;
}

// 关闭打开的设备端口
void  CloseUSBPort(HANDLE hUsbPort)
{
	if (INVALID_HANDLE_VALUE != hUsbPort)
	{
		CloseHandle(hUsbPort);
		hUsbPort = INVALID_HANDLE_VALUE;
	}
}

//从USB读取回来的ID解析出打印机型号
LPTSTR ResolveUsbPrintId(CString PrintId)
{
	CString strReturn = _T("");
	LPTSTR strReturn1 = _T("");
	int iBufSize = PrintId.GetLength();
	//查找MDL字符串
	int iPos = PrintId.Find(_T("MDL:"));
	if (iPos != -1)
	{
		//查找MDL后面的第一个;号
		int iPos1 = PrintId.Find(_T(";"), iPos);
		if ((iPos1 != -1) && (iPos1 > iPos) && (iPos1 <= iBufSize))
		{
			int iCount = iPos1 - iPos - 4;//-4是为了屏蔽MDL:
			if (iCount > 0)
			{
				strReturn = PrintId.Mid(iPos + 4, iCount);  //+4是为了跳过MDL:
				strReturn.TrimLeft();//剔除空格
				strReturn1 = strReturn.AllocSysString();
			}
		}
	}
	return strReturn1;
}
//从USB读取回来的ID解析出仿真方式
LPTSTR ResolveUsbPrintCommand(CString PrintId)
{
	CString strReturn = _T("");
	LPTSTR strReturn1 = _T("");

	int iBufSize = PrintId.GetLength();
	//查找MDL字符串
	int iPos = PrintId.Find(_T("CMD:"));
	if (iPos != -1)
	{
		//查找MDL后面的第一个;号
		int iPos1 = PrintId.Find(_T(";"), iPos);
		if ((iPos1 != -1) && (iPos1 > iPos) && (iPos1 <= iBufSize))
		{
			int iCount = iPos1 - iPos - 4;//-4是为了屏蔽CMD:
			if (iCount > 0)
			{
				strReturn = PrintId.Mid(iPos + 4, iCount);  //+4是为了跳过MDL:
				strReturn.TrimLeft();//剔除空格
				strReturn1 = strReturn.AllocSysString();
			}
		}
	}
	return strReturn1;
}
//从USB读取回来的ID解析出产家名称
LPTSTR ResolveUsbPrintProvider(CString Provider)
{
	CString strReturn = _T("");
	LPTSTR strReturn1 = _T("");
	int iBufSize = Provider.GetLength();
	//查找MDL字符串
	int iPos = Provider.Find(_T("MFG:"));
	if (iPos != -1)
	{
		//查找MDL后面的第一个;号
		int iPos1 = Provider.Find(_T(";"), iPos);
		if ((iPos1 != -1) && (iPos1 > iPos) && (iPos1 <= iBufSize))
		{
			int iCount = iPos1 - iPos - 4;//-4是为了屏蔽MFG:
			if (iCount > 0)
			{
				strReturn = Provider.Mid(iPos + 4, iCount);  //+4是为了跳过MFG:
				strReturn.TrimLeft();//剔除空格
				strReturn1 = strReturn.AllocSysString();
			}
		}
	}
	return strReturn1;
}

int ReadPort(HWND hwnd, UCHAR* recBuffer, int iLen, int iWaitTime, HANDLE gm_DeviceHandle)
{
	// 函数成功，返回读入的数据长度；通道没有数据返回，返回-2；其他错误返回-1
	int iReadData = -1;

	// 创建人工重叠事件对象，默认为无信号状态
	OVERLAPPED ovInternal;
	memset(&ovInternal, 0, sizeof(OVERLAPPED));
	ovInternal.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (ovInternal.hEvent != NULL)
	{
		LPOVERLAPPED lpOverlapped = &ovInternal;

		DWORD dwReadedByte = 0;

		if (!ReadFile(gm_DeviceHandle, recBuffer, iLen, &dwReadedByte, lpOverlapped))
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				switch (WaitForSingleObject(lpOverlapped->hEvent, iWaitTime))
				{
				case WAIT_OBJECT_0:
					SetLastError(ERROR_SUCCESS);
					if (!GetOverlappedResult(gm_DeviceHandle, lpOverlapped, &dwReadedByte, FALSE))
					{
						if (GetLastError() == ERROR_OPERATION_ABORTED)
						{
							DebugMesBox(_T("Read aborted\r\n"));
						}
						else
						{
							DebugMesBox(_T("GetOverlappedResult(in Read)\n"));
						}
					}
					else
					{
						iReadData = static_cast<int>(dwReadedByte);
					}
					break;

				case WAIT_TIMEOUT:
					// 限定时间已到，重叠事件对象还是无信号状态，说明没有读回任何数据
					iReadData = 0; // 特殊标志，通道没有数据返回
					break;

				case WAIT_FAILED:
				default:
					DebugMesBox(_T("WaitForSingleObject(in Read)返回其他错误\n"));
					CancelIo(gm_DeviceHandle);
					//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
					break;
				}
			}
			else
			{
				DebugMesBox(_T("读取数据失败，且当前错误原因不是因为I/O操作延时！\n"));
				CancelIo(gm_DeviceHandle);
				//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
			}
		}
		else
		{
			iReadData = static_cast<int>(dwReadedByte);
		}

		CloseHandle(lpOverlapped->hEvent);
	}

	return iReadData;
}

/**
 * @brief 尝试写入数据到指定的设备，并处理可能的异步操作。
 *
 * @param hwnd 窗口句柄，用于发送消息以通知相关窗口发生的错误。
 * @param hDevice 设备句柄，表示要写入数据的设备。
 * @param buffer 指向要写入的数据的指针。
 * @param len 要写入的数据长度。
 * @param waitTime 异步操作的最大等待时间（毫秒）。
 * @param lpOverlapped 指向OVERLAPPED结构的指针，用于异步I/O操作。
 * @return 成功时返回写入的字节数，失败时返回-1。
 */
int TryWriteData(HWND hwnd, HANDLE hDevice, CHAR* buffer, int len, int waitTime, LPOVERLAPPED lpOverlapped) {
	DWORD bytesWritten = 0;
	BOOL writeResult = WriteFile(hDevice, buffer, len, &bytesWritten, lpOverlapped);

	// 判断WriteFile函数调用是否成功
	if (!writeResult) {
		// 判断是否为异步I/O操作的延迟
		if (GetLastError() == ERROR_IO_PENDING) {
			// 等待异步操作完成或超时
			DWORD waitResult = WaitForSingleObject(lpOverlapped->hEvent, waitTime);
			switch (waitResult) {
				// 异步操作完成
			case WAIT_OBJECT_0:
				// 获取异步操作的结果
				if (!GetOverlappedResult(hDevice, lpOverlapped, &bytesWritten, FALSE)) {
					// 异步操作失败，发送错误消息
					//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
					return -1;
				}
				return (int)bytesWritten;

				// 操作超时
			case WAIT_TIMEOUT:
				// 取消IO操作并发送错误消息
				CancelIo(hDevice);
				//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
				return -1;

				// 其他错误
			default:
				// 取消IO操作并发送错误消息
				CancelIo(hDevice);
				//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
				return -1;
			}
		}
		else {
			// 直接失败的情况，发送错误消息
			//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
			return -1;
		}
	}

	return (int)bytesWritten;
}

/**
 * @brief 向设备写入数据，使用异步方式处理I/O操作。
 *
 * @param hwnd 窗口句柄，用于错误处理时发送消息。
 * @param sendBuffer 指向要写入的数据的指针。
 * @param iLen 要写入的数据长度。
 * @param iWaitTime 异步I/O操作的最大等待时间（毫秒）。
 * @param gm_DeviceHandle 设备句柄，表示要写入数据的设备。
 * @return 成功时返回写入的字节数，失败时返回-1。
 */
int WritePort(HWND hwnd, CHAR* sendBuffer, int iLen, int iWaitTime, HANDLE gm_DeviceHandle)
{
	// 验证输入参数的有效性
	if (sendBuffer == NULL || iLen <= 0) {
		// 发送错误消息并返回
		//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
		return -1;
	}

	// 初始化OVERLAPPED结构
	OVERLAPPED ovInternal = { 0 };
	ovInternal.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	// 检查事件对象是否成功创建
	if (ovInternal.hEvent == NULL) {
		// 创建失败，发送错误消息并返回
		//PostMessage(hwnd, UM_COMMErrExit, 0, 0);
		return -1;
	}

	// 调用辅助函数尝试写入数据
	int iWriteData = TryWriteData(hwnd, gm_DeviceHandle, sendBuffer, iLen, iWaitTime, &ovInternal);

	// 清理创建的事件对象
	CloseHandle(ovInternal.hEvent);

	// 返回写入的字节数或错误标识
	return iWriteData;
}

/**
 * @brief 向指定的设备端口发送数据，并在CProgressDlg对话框的进度条和文本控件上显示写入进度和状态信息。
 *
 * @param hwnd 窗口句柄，用于错误处理时发送消息。
 * @param strOutputData 指向要发送的数据的指针。
 * @param uDataLength 要发送的数据长度（字节）。
 * @param dwTimeout 每次写操作的超时时间（毫秒）。
 * @param gm_DeviceHandle 设备句柄，表示要写入数据的设备。
 * @param pProgressDlg 指向CProgressDlg类型的对话框对象，用于更新进度条和状态信息。
 * @return 如果成功发送所有数据包，则返回TRUE；否则返回FALSE。
 */
BOOL OutPutDataToPort(HWND hwnd, char* strOutputData, DWORD uDataLength, DWORD dwTimeout, HANDLE gm_DeviceHandle)
{
	if (strOutputData == NULL || uDataLength == 0) {
		// 输入参数验证失败
		return FALSE;
	}

	BOOL bOutSucceeded = TRUE; // 标示写入操作是否成功
	UINT uPacketSize = PACKET_SIZE; // 定义每个数据包的大小
	DWORD dwPacket = uDataLength / uPacketSize; // 计算需要发送的完整包数量
	DWORD dwOutputBytes = 0; // 输出数据大小
	DWORD dwTotalBytesWritten = 0; // 已写入的总数据量

	// 如果数据量超过一个包的大小，则需要分包发送
	if (dwPacket != 0)
	{
		LPSTR lpPoint = strOutputData;
		int uLastPacket = uDataLength % uPacketSize;

		// 发送完整的数据包
		for (DWORD dwSendedPackets = 0; dwSendedPackets < dwPacket; dwSendedPackets++)
		{
			int nWriteResult = WritePort(hwnd, lpPoint, uPacketSize, dwTimeout, gm_DeviceHandle);
			if (nWriteResult < 1)
			{
				// 输出数据失败
				bOutSucceeded = FALSE;
				break;
			}
			dwOutputBytes = (DWORD)nWriteResult;
			//#endif
			lpPoint += uPacketSize; // 移动指针到下一个数据包的开始位置
			dwTotalBytesWritten += dwOutputBytes;

			//// 更新进度条和状态信息
			//pProgressDlg->m_DownloadProgress.SetPos((int)dwTotalBytesWritten);
			//CString strInfo;
			//strInfo.Format(_T("已写入 %d / %d 字节"), dwTotalBytesWritten, uDataLength);
			//pProgressDlg->m_StaticText.SetWindowText(strInfo);

			// 调试版本，将输出数据写入日志文件
#ifdef _DEBUG
			CString strInfo;
			// 将输出的16进制数据转换为字符串，注意可能数据中包含0x00字符，因此不能直接使用CString
			char szHexData[PACKET_SIZE * 3 + 1] = { 0 };
			for (UINT i = 0; i < uPacketSize; i++)
			{
				sprintf_s(szHexData + i * 3, 4, "%02X ", (BYTE)lpPoint[i]);
			}
			// 将数据大小和内容写入日志文件
			strInfo.Format(_T("Data Size: %d, Data Content: %s"), dwOutputBytes, szHexData);
			WriteLogFile(strInfo, FALSE);
#endif
		}

		// 发送最后一个不完整的数据包（如果有）
		if (bOutSucceeded && uLastPacket != 0)
		{
			int nWriteResult = WritePort(hwnd, lpPoint, uLastPacket, dwTimeout, gm_DeviceHandle);
			if (nWriteResult < 1)
			{
				// 输出数据失败
				bOutSucceeded = FALSE;
				//pProgressDlg->m_StaticText.SetWindowText(_T("写入数据失败"));
			}
			else
			{
				dwOutputBytes = (DWORD)nWriteResult;
				dwTotalBytesWritten += dwOutputBytes;

				// 调试版本，将输出数据写入日志文件
#ifdef _DEBUG
				CString strInfo;
				// 将输出的16进制数据转换为字符串，注意可能数据中包含0x00字符，因此不能直接使用CString
				char szHexData[PACKET_SIZE * 3 + 1] = { 0 };
				for (int i = 0; i < (int)uLastPacket; i++)
				{
					sprintf_s(szHexData + i * 3, 4, "%02X ", (BYTE)lpPoint[i]);
				}
				// 将数据大小和内容写入日志文件
				strInfo.Format(_T("Data Size: %d, Data Content: %s"), dwOutputBytes, szHexData);
				WriteLogFile(strInfo, FALSE);
#endif
			}
			//#endif
		}
	}
	else // 如果数据量小于或等于一个包的大小，直接发送
	{
		int nWriteResult = WritePort(hwnd, strOutputData, uDataLength, dwTimeout, gm_DeviceHandle);
		if (nWriteResult < 1)
		{
			// 输出数据失败
			bOutSucceeded = FALSE;
		}
		else
		{
			dwOutputBytes = (DWORD)nWriteResult;
			// 调试版本，将输出数据写入日志文件
#ifdef _DEBUG
			CString strInfo;
			// 将输出的16进制数据转换为字符串，注意可能数据中包含0x00字符，因此不能直接使用CString
			char szHexData[PACKET_SIZE * 3 + 1] = { 0 };
			for (DWORD i = 0; i < uDataLength; i++)
			{
				sprintf_s(szHexData + i * 3, 4, "%02X ", (BYTE)strOutputData[i]);
			}
			// 将数据大小和内容写入日志文件
			strInfo.Format(_T("Data Size: %d, Data Content: %s"), dwOutputBytes, szHexData);
			WriteLogFile(strInfo, FALSE);
#endif
		}
	}

	//if (bOutSucceeded)
	//{
	//}

	return bOutSucceeded;
}