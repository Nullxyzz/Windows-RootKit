#include "precomp.h"

#include "main.h"

//��ڵ㺯��
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	
	KdPrint(("test.sys loaded!\n"));
	
	return ntStatus;
}