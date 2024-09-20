#include "SSSDT.h"

extern ULONG_PTR  SSSDTDescriptor;

extern PDRIVER_OBJECT   CurrentDriverObject;
extern PVOID            SysModuleBsse;
extern ULONG_PTR        ulSysModuleSize;

//���SSSDT��ַ������*4+SSSDT �����һ����������4λ����ú���ƫ�ơ�����SSSDT���õ� ��Ӧ������ַ
PVOID GetSSSDTFunctionAddress64(ULONG ulIndex)
{
	LONG v1 = 0;
	ULONG_PTR v2 = 0;
	ULONG_PTR ServiceTableBase= 0 ;
	PSYSTEM_SERVICE_TABLE64 SSSDT = (PSYSTEM_SERVICE_TABLE64)SSSDTDescriptor;
	ServiceTableBase=(ULONG_PTR)(SSSDT ->ServiceTableBase);
	v2 = ServiceTableBase + 4 * ulIndex;
	v1 = *(PLONG)v2;
	v1 = v1>>4;
	return (PVOID)(ServiceTableBase + (ULONG_PTR)v1);
}

//SSSDT����ַ+4*Index��������SSSDT��Ӧ�ĺ�����ַ
PVOID GetSSSDTFunctionAddress32(ULONG ulIndex)
{
	ULONG_PTR ServiceTableBase= 0 ;
	PSYSTEM_SERVICE_TABLE32 SSSDT = (PSYSTEM_SERVICE_TABLE32)SSSDTDescriptor;
	ServiceTableBase = (ULONG_PTR)(SSSDT->ServiceTableBase);
	return (PVOID)(*(PULONG_PTR)((ULONG_PTR)ServiceTableBase + 4 * ulIndex));
}

//Ring3����ģ��������DriverObject->DriverSection�ṹ��������  ����������Ƚϣ� һ���򷵻�������ַ
BOOLEAN GetSysModuleByLdrDataTable(WCHAR* wzModuleName)
{
	BOOLEAN bRet = FALSE;
	if (CurrentDriverObject)
	{
		PKLDR_DATA_TABLE_ENTRY ListHead = NULL, ListNext = NULL;

		ListHead = ListNext = (PKLDR_DATA_TABLE_ENTRY)CurrentDriverObject->DriverSection;  //dt _DriverObject
		while((PKLDR_DATA_TABLE_ENTRY)ListNext->InLoadOrderLinks.Flink != ListHead)
		{
			//DbgPrint("%wZ\r\n",&ListNext->BaseDllName);
			if (ListNext->BaseDllName.Buffer&& 														
				wcsstr((WCHAR*)(ListNext->BaseDllName.Buffer),wzModuleName)!=NULL)
			{
				SysModuleBsse = (PVOID)(ListNext->DllBase);
				ulSysModuleSize = ListNext->SizeOfImage;

				//DbgPrint("%x    %x\r\n",ListNext->DllBase,ListNext->EntryPoint);
				//	DbgPrint("ModuleNameSecondGet:%wZ\r\n",&(ListNext->FullDllName));

				bRet = TRUE;
				break;
			}
			ListNext = (PKLDR_DATA_TABLE_ENTRY)ListNext->InLoadOrderLinks.Flink;
		}
	}
	return bRet;
}

//����DriverObject->DriverSection�ṹ�������������в��Һ�������ģ��
BOOLEAN GetSysModuleByLdrDataTable1(PVOID Address,WCHAR* wzModuleName)
{
	BOOLEAN bRet = FALSE;
	ULONG_PTR ulBase;
	ULONG ulSize;

	if (CurrentDriverObject)
	{
		PKLDR_DATA_TABLE_ENTRY ListHead = NULL, ListNext = NULL;

		ListHead = ListNext = (PKLDR_DATA_TABLE_ENTRY)CurrentDriverObject->DriverSection;  //dt _DriverObject
		while((PKLDR_DATA_TABLE_ENTRY)ListNext->InLoadOrderLinks.Flink != ListHead)
		{
			ulBase = (ListNext)->DllBase;
			ulSize = (ListNext)->SizeOfImage;
			if((ULONG_PTR)Address > ulBase && (ULONG_PTR)Address < ulSize+ulBase)
			{
				memcpy(wzModuleName,(WCHAR*)(((ListNext)->FullDllName).Buffer),sizeof(WCHAR)*60);
				bRet = TRUE;
				break;
			} 
			ListNext = (PKLDR_DATA_TABLE_ENTRY)ListNext->InLoadOrderLinks.Flink;
		}
	}
	return bRet;
}

VOID  UnHookSSSDTWin7(ULONG ulIndex, ULONG_PTR OriginalFunctionAddress)
{
	ULONG_PTR v2 = 0;
	ULONG_PTR ServiceTableBase = 0 ;
	ULONG CurrentFunctionOffsetOfSSSDT = 0;
	PSYSTEM_SERVICE_TABLE64 SSSDT = (PSYSTEM_SERVICE_TABLE64)SSSDTDescriptor;
	ServiceTableBase=(ULONG_PTR)(SSSDT ->ServiceTableBase);
	CurrentFunctionOffsetOfSSSDT = (ULONG)((ULONG_PTR)OriginalFunctionAddress - (ULONG_PTR)(SSSDT->ServiceTableBase));
	CurrentFunctionOffsetOfSSSDT = CurrentFunctionOffsetOfSSSDT<<4;

	v2 = ServiceTableBase + 4 * ulIndex;
	WPOFF();
	*(PLONG)v2 = CurrentFunctionOffsetOfSSSDT;
	WPON();
}

VOID UnHookSSSDTWinXP(ULONG ulIndex, ULONG_PTR OriginalFunctionAddress)
{
	ULONG_PTR ServiceTableBase = 0 ;
	ULONG_PTR v2 = 0;
	PSYSTEM_SERVICE_TABLE32 SSSDT = (PSYSTEM_SERVICE_TABLE32)SSSDTDescriptor;
	ServiceTableBase=(ULONG_PTR)(SSSDT->ServiceTableBase);

	v2 = ServiceTableBase + 4 * ulIndex;
	WPOFF();
	*(PLONG)v2 = (ULONG)OriginalFunctionAddress;
	WPON();
}

BOOLEAN ResumeSSSDTInlineHook(ULONG ulIndex,UCHAR* szOriginalFunctionCode)
{
	PVOID  CurrentFunctionAddress = NULL;
#ifdef _WIN64
	CurrentFunctionAddress = GetSSSDTFunctionAddress64(ulIndex);
#else
	CurrentFunctionAddress = GetSSSDTFunctionAddress32(ulIndex);
#endif

	WPOFF();
	SafeCopyMemory(CurrentFunctionAddress,szOriginalFunctionCode,CODE_LENGTH);  
	//memcpy(CurrentFunctionAddress,szOriginalFunctionCode,CODE_LENGTH);
	WPON();

	return TRUE;
}