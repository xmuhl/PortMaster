/************************************************************************

FileName:	LPTCtrl.h

Copyright (c) 2009-2012

Writer:	黄磊

create Date:	202.07.10

Rewriter:

Rewrite Date:

Impact:	LPT端口控制功能模块预定义头文件及常量

Main Content:
GetLPTDeviceID();			// 获取并口设备ID
OpenLPT();						// 打开并口
WriteLPTPort();					// 往并口写入数据
CloseLPTPort();				// 关闭并口句柄

/************************************************************************/

#include "stdafx.h"

#include "LPTCtrl.h"
#include <setupapi.h>
#include "DebugStuff.h"

#include <WinIoCtl.h>
#include "ntddpar.h"
//#include "uConstDef.h"
#include <cfgmgr32.h>
#include "StrTranslate.h"
#pragma comment (lib,"setupapi.lib")

#define GUID_DEVINTERFACE_PARALLEL {0x97F76EF0, 0xF883, 0x11D0, {0xAF, 0x1F, 0x00, 0x00, 0xF8, 0x00, 0x84, 0x5C}}
#define  LOOP_COUNTS	3				// 循环判断次数
#define  SLEEP_TIME		0			// 硬件扫描间隔时间

// 打开并口
HANDLE OpenLPTPort(CString PortNum)
{
	HANDLE hLPTPort = INVALID_HANDLE_VALUE;				// 初始化一个无效句柄

	CString Port = QueryLPTPort(PortNum);

	CString strPort = _T("\\\\.\\");
	strPort += Port;

	hLPTPort = CreateFile(strPort,			 // interfaceDetail->DevicePath
		GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	//hLPTPort = CreateFile(_T("\\\\.\\LPT1"),			 // interfaceDetail->DevicePath
	//	GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	return hLPTPort;
}
//枚举设备管理器,返回打开的并口，如果有设备ID返回可能为1.4...
CString QueryLPTPort(CString PortNum)
{
	CString strReturn = _T("");
	// 获取当前并口端口列表
	TCHAR GUID[20] = { 0 };
	DWORD dwGUIDsize;
	DWORD dwsize = 0;
	BOOL bResult;
	int iLPTID = 0;

	bResult = SetupDiClassGuidsFromName(_T("Ports"), (LPGUID)GUID, 20, &dwGUIDsize);

	if (bResult)
	{
		//Get device info set for device class (SetupDiGetClassDevsA function)
		HDEVINFO					hardwareDeviceInfo;		// 设备信息集数据结构

		hardwareDeviceInfo = SetupDiGetClassDevs((LPGUID)GUID,
			NULL, NULL, (DIGCF_PRESENT)); // 根据指定的USB设备的GUID获取设备信息集

		if (hardwareDeviceInfo == INVALID_HANDLE_VALUE)			// 无法获取指定设备的设备集信息，弹出错误提示框
		{
			GetErrAndLog(_T("无法获取指定设备信息集!"));
		}
		else
		{
			//Get device info data for every device (SetupDiGetClassDevsA function, second parameter for this function is sequential device index in the device class, so call this function in circle with device index = 0, 1, etc.).
			SP_DEVINFO_DATA				deviceInfoData;			// 指定接口的设备信息数据结构
			deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			DWORD dwPortID = 0;
			BOOL bGetPortInfo = FALSE;
			do
			{
				bGetPortInfo = SetupDiEnumDeviceInfo(hardwareDeviceInfo, dwPortID, &deviceInfoData);

				if (bGetPortInfo)
				{
					DWORD dwProperty = 0;
					DWORD dwRequireSize = 0;
					BYTE Propbuf[MAX_PATH] = { 0 };
					CString strPortname;

					BOOL bGetRegInfo = SetupDiGetDeviceRegistryProperty(hardwareDeviceInfo, &deviceInfoData,
						SPDRP_FRIENDLYNAME, &dwProperty, Propbuf, MAX_PATH, &dwRequireSize);

					if (bGetRegInfo)
					{
						CString strDevicename = (LPCTSTR)Propbuf;

						if (strDevicename.Find(PortNum) != -1)
						{
							strPortname = strDevicename.Mid(strDevicename.Find(_T("(LPT")) + 1, 4);

							TCHAR* szMsg = new TCHAR[100];
							//其参数为CString字符串的长度
							szMsg = strPortname.GetBuffer(strPortname.GetLength());

							strPortname.ReleaseBuffer();

							//调用函数判断该端口是否连接的是实达打印机,返回打印机型号,没有则为空
							//IsSTDeviceID(szMsg, strReturn);

							strReturn = ResolveLPTPort(szMsg);
							iLPTID++;

							break;
						}
					}
				}
				dwPortID++;
			} while (bGetPortInfo);
		}
		SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	}
	else
	{
		GetErrAndLog(_T("获取端口GUID失败！"));
	}

	return strReturn;
}
//从LPT读取回来的ID解析出打印机并口号
CString ResolveLPTPort(CString PrintId)
{
	CString strReturn = _T("");
	int iPos = PrintId.ReverseFind('&');
	int iBufSize = PrintId.GetLength();
	if ((iPos != -1) && (iPos <= iBufSize))
	{
		strReturn = PrintId.Mid(iPos + 1, iBufSize - iPos);
	}
	return strReturn;
}
BOOL   ScanForHardwareChanges(TCHAR* pDeviceID)
{
	DEVINST           devInst;
	CONFIGRET       status;

	//
	//   Get   the   root   devnode.
	//

	status = CM_Locate_DevNode(&devInst, pDeviceID, CM_LOCATE_DEVNODE_NORMAL);

	if (status != CR_SUCCESS)
	{
		//GetErrAndLog(_T("CM_Locate_DevNode   failed:   %x\n ") + status, TRUE);
		CString strError;
		strError.Format(_T("CM_Locate_DevNode failed: %x\n"), status);
		GetErrAndLog(strError);

		return   FALSE;
	}

	status = CM_Reenumerate_DevNode(devInst, 0);

	if (status != CR_SUCCESS)
	{
		CString strError;
		strError.Format(_T("CM_Reenumerate_DevNode failed: %x\n"), status);
		GetErrAndLog(strError);
		//GetErrAndLog(_T("CM_Reenumerate_DevNode   failed:   %x\n ") + status, TRUE);
		return   FALSE;
	}

	return   TRUE;
}
// 获取并口设备ID
BOOL IsSTDeviceID(TCHAR* lpLPTName, CString& PrintModel)
{
	BOOL bIsSTDevice = FALSE;

	// 通过并口GUID开始检索设备接口详细信息
	GUID ClassGuid = GUID_DEVINTERFACE_PARALLEL;
	LPGUID  pGuid = &ClassGuid;

	HDEVINFO devs;
	devs = SetupDiGetClassDevs(pGuid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	SP_DEVINFO_DATA devInfo;
	SP_DEVICE_INTERFACE_DATA devInterface;
	DWORD size;

	PSP_DEVICE_INTERFACE_DETAIL_DATA interfaceDetail = NULL;

	if (devs == INVALID_HANDLE_VALUE)
	{
		GetErrAndLog(_T("SetupDiGetClassDevs!"));
		return 0;
	}

	DWORD devCount = 0;
	devInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	while (SetupDiEnumDeviceInterfaces(devs, 0, pGuid, devCount, &devInterface))
	{ //1
		devCount++;
		size = 0;

		SetupDiGetDeviceInterfaceDetail(devs, &devInterface, 0, 0, &size, 0);

		devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
		interfaceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)calloc(1, size);

		if (interfaceDetail)
		{ //2
			interfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

			if (!SetupDiGetDeviceInterfaceDetail(devs, &devInterface, interfaceDetail, size, 0, &devInfo))
			{  //3
				free(interfaceDetail);
				SetupDiDestroyDeviceInfoList(devs);
				GetErrAndLog(_T("SetupDiGetDeviceInterfaceDetail"));
				return 0;
			} // 3//

			// 判断是否是指定的并口,可能有多个并口
			TCHAR  driverkey[MAX_PATH] = { 0 };
			DWORD dataType;
			if (!SetupDiGetDeviceRegistryProperty(devs, &devInfo, SPDRP_FRIENDLYNAME,
				&dataType, (LPBYTE)driverkey, size, 0))
			{// 3
				free(interfaceDetail);
				SetupDiDestroyDeviceInfoList(devs);

				GetErrAndLog(_T("SetupDiGetDeviceRegistryProperty"));
				return FALSE;
			}
			else
			{
				_wcsupr_s(driverkey);
				//DebugMesBox(driverkey);

				if (!wcsstr(driverkey, lpLPTName))
				{ // 4
					continue;
				}	//4
			}//3

			for (int i = 0; i < LOOP_COUNTS; i++)		//多次扫描硬件
			{//3
				// 扫描硬件改动
				// Get device ID string length
				ULONG ulSize = 0;
				if (CM_Get_Device_ID_Size(&ulSize, devInfo.DevInst, 0) == CR_SUCCESS)
				{ // 4
					// Get first child device DeviceID
					TCHAR* strDeviceID = new TCHAR[ulSize + 1];
					if (CM_Get_Device_ID(devInfo.DevInst, strDeviceID, ulSize + 1, 0) == CR_SUCCESS)
					{ //5
						//DebugMesBox(strDeviceID);
						// 扫描硬件改动
						if (ScanForHardwareChanges(strDeviceID) != TRUE)
						{ //6
							delete[] strDeviceID;
							strDeviceID = NULL;

							free(interfaceDetail);
							SetupDiDestroyDeviceInfoList(devs);
							GetErrAndLog(_T("ScanForHardwareChanges Failure"));
							return FALSE;
						}	// 6//
						delete[] strDeviceID;
						strDeviceID = NULL;
					} // 5 //
				}//4//

				//获取指定并口总线句柄，开始枚举当前并口总线连接的子设备中是否有指定型号
				ULONG hdevChild;
				// Get Child device handle
				if (CM_Get_Child(&hdevChild, devInfo.DevInst, 0) == CR_SUCCESS)
				{//4
					if (CM_Get_Device_ID_Size(&ulSize, hdevChild, 0) == CR_SUCCESS)
					{//5
						bIsSTDevice = FALSE;      // 重新开始扫描，标识位重置

						// Get first child device DeviceID
						TCHAR* strDeviceID = new TCHAR[ulSize + 1];
						if (CM_Get_Device_ID(hdevChild, strDeviceID, ulSize + 1, 0) == CR_SUCCESS)
						{//6
							//DebugMesLog(strDeviceID);
							delete[] strDeviceID;
							strDeviceID = NULL;

							//开始枚举后续子设备
							CONFIGRET cr;
							do
							{//7
								cr = CM_Get_Sibling(&hdevChild, hdevChild, 0);
								if (cr == CR_SUCCESS)
								{//8
									if (CM_Get_Device_ID_Size(&ulSize, hdevChild, 0) == CR_SUCCESS)
									{//9
										TCHAR* strDeviceID = new TCHAR[ulSize + 1];

										if (CM_Get_Device_ID(hdevChild, strDeviceID, ulSize + 1, 0) == CR_SUCCESS)
										{//10
											if (wcsstr(strDeviceID, STR_DEVICE_ID))
											{//11
												PrintModel = strDeviceID;
												bIsSTDevice = TRUE;					// 找到指定设备ID

												delete[] strDeviceID;
												strDeviceID = NULL;

												break;
											}//11//
										}//10//
										delete[] strDeviceID;
										strDeviceID = NULL;
									}//9//
								}//8//
							} while (cr == CR_SUCCESS);//7//
						}//6//
					}//5//
				}//4//

				if (bIsSTDevice)	// 找到指定设备ID，退出循环 查询
				{//4
					break;
				}
				else				// 否则停止3秒钟，等待打印机复位，再次查询
				{
					Sleep(SLEEP_TIME);
				}//4
			}	// 3 //多次硬件扫描结束
		} //2

		free(interfaceDetail);
	}// 1

	//free(interfaceDetail);
	SetupDiDestroyDeviceInfoList(devs);
	return bIsSTDevice;
}

//从USB读取回来的ID解析出打印机型号
LPTSTR ResolvePrintId(CString PrintId)
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
LPTSTR ResolvePrintCommand(CString PrintId, CString print)
{
	CString strReturn = _T("");
	LPTSTR strReturn1 = _T("");
	int iBufSize = PrintId.GetLength();
	if (iBufSize > 0)
	{
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
	}
	return strReturn1;
}
//从USB读取回来的ID解析出产家名称
LPTSTR ResolvePrintProvider(CString Provider)
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
				LPTSTR strReturn1 = _T("");
				strReturn1 = strReturn.AllocSysString();
			}
		}
	}
	return strReturn1;
}