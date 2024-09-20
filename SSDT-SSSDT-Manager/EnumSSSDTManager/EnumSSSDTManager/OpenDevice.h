#pragma once
#include <WinIoCtl.h>
#define INITIALIZE  70

#define SSDTINITIALIZE  30
#define GET_SSSDT_CURRENT_FUNC_ADDR  10
#define GET_SSDT_CURRENT_FUNC_ADDR   40
#define MODULE_LENGTH  30
#define GET_MODULE_NAME 110
#define GET_SSDT_MODULE_NAME 130

#define CODE_LENGTH  23
#define IOCTL_GET_SSSDTSERVERICE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_SSSDT_FUNCTIONADDRESS	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define IOCTL_GET_SSDTSERVERICE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_SDT_FUNCTIONADDRESS	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define CTL_GET_SYS_MODULE_INFOR \
	CTL_CODE(FILE_DEVICE_UNKNOWN,0x830,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define CTL_GET_SSDT_SYS_MODULE_INFOR \
	CTL_CODE(FILE_DEVICE_UNKNOWN,0x832,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_GET_MODULENAME \
	CTL_CODE(FILE_DEVICE_UNKNOWN,0x840,METHOD_BUFFERED,FILE_ANY_ACCESS)



class COpenDevice
{
public:
	COpenDevice(void);
	~COpenDevice(void);
	HANDLE g_hDevice;
	HANDLE OpenDevice(LPCTSTR wzLinkPath)
	{
		HANDLE hDevice = CreateFile(wzLinkPath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (hDevice == INVALID_HANDLE_VALUE)
		{

		}

		return hDevice;

	}
	BOOL SendIoControlCode(ULONG ulIndex,PVOID* FuntionAddress,ULONG_PTR ulControlCode);
};

