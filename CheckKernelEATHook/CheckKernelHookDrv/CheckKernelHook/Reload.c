#include "Reload.h"
#include "KernelReload.h"

WCHAR* SystemKernelFilePath = NULL;
ULONG_PTR SystemKernelModuleBase = 0;
ULONG_PTR SystemKernelModuleSize = 0;
ULONG_PTR ImageModuleBase;

PVOID OriginalKiServiceTable;
extern PSERVICE_DESCRIPTOR_TABLE    KeServiceDescriptorTable;
PSERVICE_DESCRIPTOR_TABLE OriginalServiceDescriptorTable;
PSERVICE_DESCRIPTOR_TABLE Safe_ServiceDescriptorTable;


/*
����FuncName  �� ԭ��Ntos��ַ  ���Լ����� Ntos��ַ
//��һ�ζ���ͨ��  ϵͳ��ԭ��ƫ�� + NewBase ��ú�����ַ  
//Ȼ��ͨ���Լ���RMmGetSystemRoutineAddress��� ƫ��+NewBase ��ú�����ַ
�������ҵ������������
*/
ULONG ReLoadNtosCALL(WCHAR *lpwzFuncTion,ULONG ulOldNtosBase,ULONG ulReloadNtosBase)
{
    UNICODE_STRING UnicodeFunctionName;
    ULONG ulOldFunctionAddress;
    PUCHAR ulReloadFunctionAddress = NULL;
    int index=0;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS NtDllHeader;

    IMAGE_OPTIONAL_HEADER opthdr;
    DWORD* arrayOfFunctionAddresses;
    DWORD* arrayOfFunctionNames;
    WORD* arrayOfFunctionOrdinals;
    DWORD functionOrdinal;
    DWORD Base, x, functionAddress,position;
    char* functionName;
    IMAGE_EXPORT_DIRECTORY *pExportTable;
    ULONG ulNtDllModuleBase;

    UNICODE_STRING UnicodeFunction;
    UNICODE_STRING UnicodeExportTableFunction;
    ANSI_STRING ExportTableFunction;
    //��һ�ζ���ͨ��  ϵͳ��ԭ��ƫ�� + NewBase ��ú�����ַ  
    //Ȼ��ͨ���Լ���RMmGetSystemRoutineAddress��� ƫ��+NewBase ��ú�����ַ
    __try
    {
        if (RRtlInitUnicodeString &&
            RRtlCompareUnicodeString &&
            RMmGetSystemRoutineAddress &&
            RMmIsAddressValid)
        {
            RRtlInitUnicodeString(&UnicodeFunctionName,lpwzFuncTion);
            ulOldFunctionAddress = (DWORD)RMmGetSystemRoutineAddress(&UnicodeFunctionName);
            ulReloadFunctionAddress = (PUCHAR)(ulOldFunctionAddress - ulOldNtosBase + ulReloadNtosBase); //������ص�FuncAddr
            if (RMmIsAddressValid(ulReloadFunctionAddress)) //�����Ч�ʹ� ������  ��ȡ��  Ӧ�ò�����Ч
            {
                return (ULONG)ulReloadFunctionAddress;
            }
            //�ӵ��������ȡ
            ulNtDllModuleBase = ulReloadNtosBase;
            pDosHeader = (PIMAGE_DOS_HEADER)ulReloadNtosBase;
            if (pDosHeader->e_magic!=IMAGE_DOS_SIGNATURE)
            {
                KdPrint(("failed to find NtHeader\r\n"));
                return 0;
            }
            NtDllHeader=(PIMAGE_NT_HEADERS)(ULONG)((ULONG)pDosHeader+pDosHeader->e_lfanew);
            if (NtDllHeader->Signature!=IMAGE_NT_SIGNATURE)
            {
                KdPrint(("failed to find NtHeader\r\n"));
                return 0;
            }
            opthdr = NtDllHeader->OptionalHeader;
            pExportTable =(IMAGE_EXPORT_DIRECTORY*)((BYTE*)ulNtDllModuleBase + opthdr.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT]. VirtualAddress); //�õ�������
            arrayOfFunctionAddresses = (DWORD*)( (BYTE*)ulNtDllModuleBase + pExportTable->AddressOfFunctions);  //��ַ��
            arrayOfFunctionNames = (DWORD*)((BYTE*)ulNtDllModuleBase + pExportTable->AddressOfNames);         //��������
            arrayOfFunctionOrdinals = (WORD*)((BYTE*)ulNtDllModuleBase + pExportTable->AddressOfNameOrdinals);

            Base = pExportTable->Base;

            for(x = 0; x < pExportTable->NumberOfFunctions; x++) //��������������ɨ��
            {
                functionName = (char*)( (BYTE*)ulNtDllModuleBase + arrayOfFunctionNames[x]);
                functionOrdinal = arrayOfFunctionOrdinals[x] + Base - 1; 
                functionAddress = (DWORD)((BYTE*)ulNtDllModuleBase + arrayOfFunctionAddresses[functionOrdinal]);
                RtlInitAnsiString(&ExportTableFunction,functionName);
                RtlAnsiStringToUnicodeString(&UnicodeExportTableFunction,&ExportTableFunction,TRUE);

                RRtlInitUnicodeString(&UnicodeFunction,lpwzFuncTion);
                if (RRtlCompareUnicodeString(&UnicodeExportTableFunction,&UnicodeFunction,TRUE) == 0)
                {
                    RtlFreeUnicodeString(&UnicodeExportTableFunction);
                    return functionAddress;
                }
                RtlFreeUnicodeString(&UnicodeExportTableFunction);
            }
            return 0;
        }
        RtlInitUnicodeString(&UnicodeFunctionName,lpwzFuncTion);
        ulOldFunctionAddress = (DWORD)MmGetSystemRoutineAddress(&UnicodeFunctionName);
        ulReloadFunctionAddress = (PUCHAR)(ulOldFunctionAddress - ulOldNtosBase + ulReloadNtosBase);

        //KdPrint(("%ws:%08x:%08x",lpwzFuncTion,ulOldFunctionAddress,ulReloadFunctionAddress));

        if (MmIsAddressValid(ulReloadFunctionAddress))
        {
            return (ULONG)ulReloadFunctionAddress;
        }
        //         

    }__except(EXCEPTION_EXECUTE_HANDLER){
        KdPrint(("EXCEPTION_EXECUTE_HANDLER"));
    }
    return 0;
}


/*����Ntos*/
NTSTATUS ReLoadNtos(PDRIVER_OBJECT   DriverObject,DWORD RetAddress)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG ulKeAddSystemServiceTable;
    PULONG p;


    if (!GetSystemKernelModuleInfo(
        &SystemKernelFilePath,
        &SystemKernelModuleBase,
        &SystemKernelModuleSize
        ))
    {
        KdPrint(("Get System Kernel Module failed"));
        return status;
    }


    if (InitSafeOperationModule(
        DriverObject,
        SystemKernelFilePath,
        SystemKernelModuleBase
        ))
    {
        KdPrint(("Init Ntos module success\r\n"));


        RRtlInitUnicodeString = NULL;
        RMmGetSystemRoutineAddress = NULL;
        RMmIsAddressValid = NULL;
        RRtlCompareUnicodeString = NULL;
        RPsGetCurrentProcess = NULL;
    
        status = STATUS_UNSUCCESSFUL;
    
        //��һ�ζ���ͨ��  ϵͳ��ԭ��ƫ�� + NewBase ��ú�����ַ  
        //Ȼ��ͨ���Լ���RMmGetSystemRoutineAddress��� ƫ��+NewBase ��ú�����ַ
        RRtlInitUnicodeString = (ReloadRtlInitUnicodeString)ReLoadNtosCALL(L"RtlInitUnicodeString",SystemKernelModuleBase,ImageModuleBase);
        RRtlCompareUnicodeString = (ReloadRtlCompareUnicodeString)ReLoadNtosCALL(L"RtlCompareUnicodeString",SystemKernelModuleBase,ImageModuleBase);
        RMmGetSystemRoutineAddress = (ReloadMmGetSystemRoutineAddress)ReLoadNtosCALL(L"MmGetSystemRoutineAddress",SystemKernelModuleBase,ImageModuleBase);
        RMmIsAddressValid = (ReloadMmIsAddressValid)ReLoadNtosCALL(L"MmIsAddressValid",SystemKernelModuleBase,ImageModuleBase);
        RPsGetCurrentProcess = (ReloadPsGetCurrentProcess)ReLoadNtosCALL(L"PsGetCurrentProcess",SystemKernelModuleBase,ImageModuleBase);
        if (!RRtlInitUnicodeString ||
            !RRtlCompareUnicodeString ||
            !RMmGetSystemRoutineAddress ||
            !RMmIsAddressValid ||
            !RPsGetCurrentProcess)
        {
            KdPrint(("Init NtosCALL failed"));
            return status;
        }
    }
    return status;
}




BOOLEAN InitSafeOperationModule(PDRIVER_OBJECT pDriverObject,WCHAR *SystemModulePath,ULONG KernelModuleBase)
{
    UNICODE_STRING FileName;
    HANDLE hSection;
    PDWORD FixdOriginalKiServiceTable;
    PDWORD CsRootkitOriginalKiServiceTable;
    ULONG i = 0;


    //�Լ�peload һ��ntos*�������ͽ���˸�������ȫ����ĳ�ͻ��~
    if (!PeLoad(SystemModulePath,(BYTE**)&ImageModuleBase,pDriverObject,KernelModuleBase))
    {
        return FALSE;
    }

    OriginalKiServiceTable = ExAllocatePool(NonPagedPool,KeServiceDescriptorTable->TableSize*sizeof(DWORD));
    if (!OriginalKiServiceTable)
    {
        return FALSE;
    }
    //���SSDT��ַ��ͨ���ض�λ��Ƚϵõ�
    if(!GetOriginalKiServiceTable((BYTE*)ImageModuleBase,KernelModuleBase,(DWORD*)&OriginalKiServiceTable))
    {
        ExFreePool(OriginalKiServiceTable);

        return FALSE;
    }

    //�޸�SSDT������ַ  �����Լ�Reload�ĺ�����ַ  �ɾ���
    FixOriginalKiServiceTable((PDWORD)OriginalKiServiceTable,(DWORD)ImageModuleBase,KernelModuleBase);

    OriginalServiceDescriptorTable = (PSERVICE_DESCRIPTOR_TABLE)ExAllocatePool(NonPagedPool,sizeof(SERVICE_DESCRIPTOR_TABLE)*4);
    if (OriginalServiceDescriptorTable == NULL)
    {
        ExFreePool(OriginalKiServiceTable);
        return FALSE;
    }
    RtlZeroMemory(OriginalServiceDescriptorTable,sizeof(SERVICE_DESCRIPTOR_TABLE)*4);

    //�޸�SERVICE_DESCRIPTOR_TABLE �ṹ  
    OriginalServiceDescriptorTable->ServiceTable = (PDWORD)OriginalKiServiceTable;
    OriginalServiceDescriptorTable->CounterTable = KeServiceDescriptorTable->CounterTable;
    OriginalServiceDescriptorTable->TableSize    = KeServiceDescriptorTable->TableSize;
    OriginalServiceDescriptorTable->ArgumentTable = KeServiceDescriptorTable->ArgumentTable;

    CsRootkitOriginalKiServiceTable = (PDWORD)ExAllocatePool(NonPagedPool,KeServiceDescriptorTable->TableSize*sizeof(DWORD));
    if (CsRootkitOriginalKiServiceTable==NULL)
    {
        ExFreePool(OriginalServiceDescriptorTable);
        ExFreePool(OriginalKiServiceTable);
        return FALSE;
    }
    RtlZeroMemory(CsRootkitOriginalKiServiceTable,KeServiceDescriptorTable->TableSize*sizeof(DWORD));

    Safe_ServiceDescriptorTable = (PSERVICE_DESCRIPTOR_TABLE)ExAllocatePool(NonPagedPool,sizeof(SERVICE_DESCRIPTOR_TABLE)*4);
    if (Safe_ServiceDescriptorTable == NULL)
    {
        ExFreePool(OriginalServiceDescriptorTable);
        ExFreePool(CsRootkitOriginalKiServiceTable);
        ExFreePool(OriginalKiServiceTable);
        return FALSE;
    }
    //����һ���ɾ���ԭʼ��ÿ����������Ӧ��SSDT�����ĵ�ַ����ԭʼ����
    RtlZeroMemory(Safe_ServiceDescriptorTable,sizeof(SERVICE_DESCRIPTOR_TABLE)*4);

    //���ԭʼ������ַ
    for (i = 0; i < KeServiceDescriptorTable->TableSize; i++)
    {
        CsRootkitOriginalKiServiceTable[i] = OriginalServiceDescriptorTable->ServiceTable[i];
    }
    Safe_ServiceDescriptorTable->ServiceTable = (PDWORD)CsRootkitOriginalKiServiceTable;
    Safe_ServiceDescriptorTable->CounterTable = KeServiceDescriptorTable->CounterTable;
    Safe_ServiceDescriptorTable->TableSize = KeServiceDescriptorTable->TableSize;
    Safe_ServiceDescriptorTable->ArgumentTable = KeServiceDescriptorTable->ArgumentTable;

    //�ͷžͻ�bsod
    //ExFreePool(OriginalKiServiceTable);
    
    return TRUE;
}


VOID FixOriginalKiServiceTable(PDWORD OriginalKiServiceTable,DWORD ModuleBase,DWORD ExistImageBase)
{
    DWORD FuctionCount;
    DWORD Index;
    FuctionCount=KeServiceDescriptorTable->TableSize; //��������
    
    KdPrint(("ssdt funcion count:%X---KiServiceTable:%X\n",FuctionCount,KeServiceDescriptorTable->ServiceTable));    
    for (Index=0;Index<FuctionCount;Index++)
    {
        OriginalKiServiceTable[Index]=OriginalKiServiceTable[Index]-ExistImageBase+ModuleBase; //�޸�SSDT������ַ
    }
}

//ͨ��KeServiceDescriptorTable��RVA���ض�λ��������ĵ�ַRVA�Ƚϣ�һ����ȡ�����е�SSDT���ַ
BOOLEAN GetOriginalKiServiceTable(BYTE *NewImageBase,DWORD ExistImageBase,DWORD *NewKiServiceTable)
{
    PIMAGE_DOS_HEADER ImageDosHeader;
    PIMAGE_NT_HEADERS ImageNtHeaders;
    DWORD KeServiceDescriptorTableRva;
    PIMAGE_BASE_RELOCATION ImageBaseReloc=NULL;
    DWORD RelocSize;
    int ItemCount,Index;
    int Type;
    PDWORD RelocAddress;
    DWORD RvaData;
    DWORD count=0;
    WORD *TypeOffset;


    ImageDosHeader=(PIMAGE_DOS_HEADER)NewImageBase;
    if (ImageDosHeader->e_magic!=IMAGE_DOS_SIGNATURE)
    {
        return FALSE;
    }
    ImageNtHeaders=(PIMAGE_NT_HEADERS)(NewImageBase+ImageDosHeader->e_lfanew);
    if (ImageNtHeaders->Signature!=IMAGE_NT_SIGNATURE)
    {
        return FALSE;
    }
    KeServiceDescriptorTableRva=(DWORD)MiFindExportedRoutine(NewImageBase,TRUE,"KeServiceDescriptorTable",0);
    if (KeServiceDescriptorTableRva==0)
    {
        return FALSE;
    }

    KeServiceDescriptorTableRva=KeServiceDescriptorTableRva-(DWORD)NewImageBase;
    ImageBaseReloc=RtlImageDirectoryEntryToData(NewImageBase,TRUE,IMAGE_DIRECTORY_ENTRY_BASERELOC,&RelocSize);
    if (ImageBaseReloc==NULL)
    {
        return FALSE;
    }

    while (ImageBaseReloc->SizeOfBlock)
    {  
        count++;
        ItemCount=(ImageBaseReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION))/2;
        TypeOffset=(WORD*)((DWORD)ImageBaseReloc+sizeof(IMAGE_BASE_RELOCATION));
        for (Index=0;Index<ItemCount;Index++)
        {
            Type=TypeOffset[Index]>>12;  //��4λ������   ��12λλҳ��ƫ�� 4k  
            if (Type==3)
            {
                //Base + Virtual ��λ��ҳ   + ��12λ  = RelocAddress ��Ҫ�޸��ĵ�ַ
                RelocAddress=(PDWORD)((DWORD)(TypeOffset[Index]&0x0fff)+ImageBaseReloc->VirtualAddress+(DWORD)NewImageBase);
                RvaData=*RelocAddress-ExistImageBase;
                
                if (RvaData==KeServiceDescriptorTableRva)  //�ض�λ���е�rva �� KeServiceDescriptorTable �����
                {
                    if(*(USHORT*)((DWORD)RelocAddress-2)==0x05c7)
                    {
                        /*
                    1: kd> dd 0x89651c12   RelocAddress - 2
                    89651c12       79c005c7 bd9c83f8 

                    1: kd> dd KeServiceDescriptorTable           
                    83f879c0       83e9bd9c 00000000 00000191 83e9c3e4
                    83f879d0       00000000 00000000 00000000 00000000
                
                    1: kd> dd 0x89651c14        RelocAddress
                    89651c14       83f879c0 83e9bd9c 79c41589 c8a383f8
                    89651c24       c783f879 f879cc05 e9c3e483 d8158983
                        */
                        //RelocAddress �������� KeServiceDesriptorTable��ַ  
                        //RelocAddress + 4 ����� KeServiceDesriptorTable��һ��ԱҲ����SSDT��ַ
                        *NewKiServiceTable=*(DWORD*)((DWORD)RelocAddress+4)-ExistImageBase+(DWORD)NewImageBase;
                        return TRUE;
                    }
                }

            }

        }
        ImageBaseReloc=(PIMAGE_BASE_RELOCATION)((DWORD)ImageBaseReloc+ImageBaseReloc->SizeOfBlock);
    }

    return FALSE;
}
