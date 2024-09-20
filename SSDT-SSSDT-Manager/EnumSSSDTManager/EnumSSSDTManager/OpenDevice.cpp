#include "StdAfx.h"
#include "OpenDevice.h"


COpenDevice::COpenDevice(void)
{
	g_hDevice = NULL;
}


COpenDevice::~COpenDevice(void)
{
}



BOOL COpenDevice::SendIoControlCode(ULONG ulIndex,PVOID* FuntionAddress,ULONG_PTR ulControlCode)
{
	BOOL bRet = FALSE;
	DWORD ulReturnSize = 0;

	if (ulControlCode==INITIALIZE)//������Ϣ���Ring0����Ring0���SSSDT���ַ
	{
		bRet = DeviceIoControl(g_hDevice,IOCTL_GET_SSSDTSERVERICE,
			NULL,
			NULL,
			NULL,
			NULL,
			&ulReturnSize,
			NULL);

		if (bRet==FALSE)
		{
			return FALSE;
		}


	}

	if (ulControlCode==GET_SSSDT_CURRENT_FUNC_ADDR)
	{

		bRet = DeviceIoControl(g_hDevice,IOCTL_GET_SSSDT_FUNCTIONADDRESS,
			&ulIndex,
			sizeof(ULONG),
			FuntionAddress,
			sizeof(PVOID),
			&ulReturnSize,
			NULL);

		if (bRet==FALSE)
		{
			return FALSE;
		}
	}

	if (ulControlCode==SSDTINITIALIZE)
	{
		bRet = DeviceIoControl(g_hDevice,IOCTL_GET_SSDTSERVERICE,
			NULL,
			NULL,
			NULL,
			NULL,
			&ulReturnSize,
			NULL);

		if (bRet==FALSE)
		{
			return FALSE;
		}


	}

	if (ulControlCode==GET_SSDT_CURRENT_FUNC_ADDR)
	{

		bRet = DeviceIoControl(g_hDevice,IOCTL_GET_SDT_FUNCTIONADDRESS,
			&ulIndex,
			sizeof(ULONG),
			FuntionAddress,
			sizeof(PVOID),
			&ulReturnSize,
			NULL);

		if (bRet==FALSE)
		{
			return FALSE;
		}
	}
	return TRUE;
}


