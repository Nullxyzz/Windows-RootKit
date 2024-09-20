#include "KernelHookCheck.h"
#include "libdasm.h"
#include "Common.h"
#include "Reload.h"

ULONG IntHookCount;  //��¼Hook����

extern DWORD OriginalKiServiceTable;
extern PSERVICE_DESCRIPTOR_TABLE OriginalServiceDescriptorTable;

extern ULONG_PTR SystemKernelModuleBase;
extern ULONG_PTR SystemKernelModuleSize;
extern ULONG_PTR ImageModuleBase;


BOOLEAN KernelHookCheck(PINLINEHOOKINFO InlineHookInfo)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    PIMAGE_NT_HEADERS       NtHeader;
    PIMAGE_EXPORT_DIRECTORY ExportTable;
    ULONG*  FunctionAddresses;
    ULONG*  FunctionNames;
    USHORT* FunctionIndexs;
    ULONG   ulIndex;
    ULONG   i;
    CHAR*   szFunctionName;
    SIZE_T  ViewSize=0;
    ULONG_PTR ulFunctionAddress;

    BOOL bIsZwFunction = FALSE;

    ULONG ulOldAddress;
    ULONG ulReloadAddress;

    PUCHAR ulTemp;

    __try{
        NtHeader = RtlImageNtHeader((PVOID)ImageModuleBase);
        if (NtHeader && NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress){
            ExportTable =(IMAGE_EXPORT_DIRECTORY*)((ULONG_PTR)ImageModuleBase + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
            FunctionAddresses = (ULONG*)((ULONG_PTR)ImageModuleBase + ExportTable->AddressOfFunctions);
            FunctionNames = (ULONG*)((ULONG_PTR)ImageModuleBase + ExportTable->AddressOfNames);
            FunctionIndexs = (USHORT*)((ULONG_PTR)ImageModuleBase + ExportTable->AddressOfNameOrdinals);
            for(i = 0; i < ExportTable->NumberOfNames; i++)
            {
                szFunctionName = (LPSTR)((ULONG_PTR)ImageModuleBase + FunctionNames[i]);
                
                ulIndex = FunctionIndexs[i]; 
                ulFunctionAddress = (ULONG_PTR)((ULONG_PTR)ImageModuleBase + FunctionAddresses[ulIndex]);
            //    ulIndex=*(ULONG*)(ulFunctionAddress+1); //32 bit 1   64  bit  4  //�����


                //���ڷ�Zwϵ�к���  ƫ�Ƶ�ϵͳ�ĸú�����ַ��
                ulReloadAddress = ulFunctionAddress;
                ulOldAddress = ulReloadAddress - (ULONG)ImageModuleBase + SystemKernelModuleBase; 

                if (!ulOldAddress ||
                    !MmIsAddressValid((PVOID)ulOldAddress) ||
                    !ulReloadAddress ||
                    !MmIsAddressValid((PVOID)ulReloadAddress))
                {
                    continue;
                }
                bIsZwFunction = FALSE;

                //�����һ���һ��call�ĺ�����hook
                if (*szFunctionName == 'Z' &&
                    *(szFunctionName+1) == 'w')
                {
                    bIsZwFunction = TRUE;
                    ulIndex  = *((WORD*)(ulFunctionAddress + 1));  //�õ������

                    if (ulIndex > 0 &&
                        ulIndex <= OriginalServiceDescriptorTable->TableSize)
                    {
                        //����Zwϵ�к���   ���ϵͳNtos�� ��Ӧ��Nt�����ĵ�ַ
                        ulReloadAddress = OriginalServiceDescriptorTable->ServiceTable[ulIndex];
                        ulOldAddress = ulReloadAddress - (ULONG)ImageModuleBase + SystemKernelModuleBase;
                    }
                }
                if (bIsZwFunction)
                {
                    //��� bIsZwFunction == TRUE ����Ч��һ�µ�ַ����Ч��
                    if (!ulOldAddress ||
                        !MmIsAddressValid((PVOID)ulOldAddress) ||
                        !ulReloadAddress ||
                        !MmIsAddressValid((PVOID)ulReloadAddress))
                    {
                        continue;
                    }
                }
                else //��һ�㺯��ֻɨ���Zw��ͷ�ģ�����ֻɨ��δ��������
                {    
                    GetNextFunctionAddress(ImageModuleBase,ulOldAddress,szFunctionName,InlineHookInfo);
                }

                ulTemp = NULL;

                //����Zw�е�Nt���� �� ��������
                //�ж��Ƿ�Ntos ������Hook
                //ulOldAddress �Ǹ������ص�ַ - Base + KernelBase  ���������ĵ�ַ
                ulTemp = (PUCHAR)GetEatHook(ulOldAddress,i,SystemKernelModuleBase,SystemKernelModuleSize); //�Ƚ�EAT Hook
                    
                if(ulTemp)
                {//������Hook��
                    FillInlineHookInfo(ulTemp,InlineHookInfo,szFunctionName,ulOldAddress,1); //EAT Hook 1
                }
                //�Ƿ���InlineHook
                CheckFuncByOpcode((PVOID)ulReloadAddress,InlineHookInfo,szFunctionName,(PVOID)ulOldAddress);

            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return STATUS_SUCCESS;
}
VOID FillInlineHookInfo(PUCHAR ulTemp,PINLINEHOOKINFO InlineHookInfo,CHAR*   szFunctionName,ULONG ulOldAddress,ULONG HookType)
{
    ULONG ulHookModuleBase;
    ULONG ulHookModuleSize;
    char lpszHookModuleImage[256];
    ULONG IntHookCount = InlineHookInfo->ulCount;


    memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));
    if (!IsAddressInSystem(
        (ULONG)ulTemp,
        &ulHookModuleBase,
        &ulHookModuleSize,
        lpszHookModuleImage))
    {
        memset(lpszHookModuleImage,0,sizeof(lpszHookModuleImage));
        strcat(lpszHookModuleImage,"Unknown4");
        ulHookModuleBase = 0;
        ulHookModuleSize = 0;
    }
    InlineHookInfo->InlineHook[IntHookCount].ulMemoryHookBase = (ULONG)ulTemp;
    memset(InlineHookInfo->InlineHook[IntHookCount].lpszFunction,0,sizeof(InlineHookInfo->InlineHook[IntHookCount].lpszFunction));
    memset(InlineHookInfo->InlineHook[IntHookCount].lpszHookModuleImage,0,sizeof(InlineHookInfo->InlineHook[IntHookCount].lpszHookModuleImage));

    memcpy(InlineHookInfo->InlineHook[IntHookCount].lpszFunction,szFunctionName,strlen(szFunctionName));
    memcpy(InlineHookInfo->InlineHook[IntHookCount].lpszHookModuleImage,lpszHookModuleImage,strlen(lpszHookModuleImage));

    InlineHookInfo->InlineHook[IntHookCount].ulMemoryFunctionBase = (ULONG)ulOldAddress;
    InlineHookInfo->InlineHook[IntHookCount].ulHookModuleBase = ulHookModuleBase;
    InlineHookInfo->InlineHook[IntHookCount].ulHookModuleSize = ulHookModuleSize;
    InlineHookInfo->InlineHook[IntHookCount].ulHookType = HookType;  //eat hook 1   Inline Hook 0
    IntHookCount++;
    InlineHookInfo->ulCount++;
}


VOID CheckFuncByOpcode(PVOID ulReloadAddress,PINLINEHOOKINFO InlineHookInfo,CHAR*   szFunctionName,PVOID ulOldAddress)
{
    INSTRUCTION    Inst;
    INSTRUCTION    Instb;
    ULONG ulHookFunctionAddress;
    size_t ulCodeSize;
    PUCHAR p;
    PUCHAR ulTemp;
    int Flagss;
     if (GetFunctionCodeSize(ulOldAddress) == GetFunctionCodeSize(ulReloadAddress) &&
         memcmp(ulReloadAddress,ulOldAddress,GetFunctionCodeSize(ulOldAddress)) != 0)
    {//��Hook��
        //��ʼɨ��hooksss
        ulCodeSize = GetFunctionCodeSize(ulOldAddress);

        for (p = (PUCHAR)ulOldAddress ;(ULONG)p < (ULONG)ulOldAddress+ulCodeSize; p++)
        {
            //�۰�ɨ�裬���ǰ��һ��һ������ʼɨ����һ��
            if (memcmp(ulReloadAddress,ulOldAddress,ulCodeSize/2) == 0)
            {
                ulCodeSize = ulCodeSize + ulCodeSize/2;
                continue;
            }
            if (*p == 0xcc ||
                *p == 0xc2)
            {
                break;
            }
            ulHookFunctionAddress = (*(PULONG)(p + 1) + (ULONG)p + 5);  //�õ�hook�ĵ�ַ
            if (!MmIsAddressValid((PVOID)ulHookFunctionAddress))
            {
                continue;
            }
            ulTemp = NULL;
            get_instruction(&Inst,p,MODE_32);
            switch (Inst.type)
            {
            case INSTRUCTION_TYPE_JMP:
                if(Inst.opcode==0xFF&&Inst.modrm==0x25)
                {
                    //DIRECT_JMP
                    ulTemp = (PUCHAR)Inst.op1.displacement;
                }
                else if (Inst.opcode==0xEB)
                {
                    ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                }
                else if(Inst.opcode==0xE9)
                {
                    //RELATIVE_JMP;
                    ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                }
                break;
            case INSTRUCTION_TYPE_CALL:
                if(Inst.opcode==0xFF&&Inst.modrm==0x15)
                {
                    //DIRECT_CALL
                    ulTemp = (PUCHAR)Inst.op1.displacement;
                }
                else if (Inst.opcode==0x9A)
                {
                    ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                }
                else if(Inst.opcode==0xE8)
                {
                    //RELATIVE_CALL;
                    ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                }
                break;
            case INSTRUCTION_TYPE_PUSH:
                if(!RMmIsAddressValid((PVOID)(p)))
                {
                    break;
                }
                get_instruction(&Instb,(BYTE*)(p),MODE_32);
                if(Instb.type == INSTRUCTION_TYPE_RET)
                {
                    //StartAddress+len-inst.length-instb.length;
                    ulTemp = (PUCHAR)Instb.op1.displacement;
                }
                break;
            }
            if (ulTemp &&
                RMmIsAddressValid(ulTemp) &&
                RMmIsAddressValid(p))   //hook�ĵ�ַҲҪ��Ч�ſ���Ŷ
            {
                if ((ULONG)ulTemp > SystemKernelModuleBase &&
                    (ULONG)ulTemp < SystemKernelModuleBase+SystemKernelModuleSize)   //̫������Ҳ����
                {
                    goto Next;
                }
                //ulTempҲ����С�� SystemKernelModuleBase
                if ((ULONG)ulTemp < SystemKernelModuleBase)
                {
                    goto Next;
                }
                //KdPrint(("%08x-%08x-%08x",p,ulTemp,(SystemKernelModuleBase + SystemKernelModuleSize + 0xfffffff)));

                if (*(ULONG *)ulTemp == 0x00000000 ||
                    *(ULONG *)ulTemp == 0x00000005 ||
                    *(ULONG *)ulTemp == 0xc0000012)
                {
                    goto Next;
                }
                Flagss = 0;
                __asm{
                    mov esi,ulTemp
                    mov ax,word ptr [esi]
                    cmp ax,0x0000
                    jz Cont//��add     byte ptr [eax],al
                    //����
                    mov Flagss,1
Cont:
                }
                if (Flagss != 1)
                    goto Next;

                ulTemp = ulTemp+0x5;
                //�򵥴���һ�¶�����
                if (*ulTemp == 0xe9 ||
                    *ulTemp == 0xe8)
                {
                    ulTemp = (PUCHAR)(*(PULONG)(ulTemp+1)+(ULONG)(ulTemp+5));
                }
                FillInlineHookInfo(ulTemp,InlineHookInfo,szFunctionName,(ULONG)p,0);  //Inline Hook
Next:
                _asm{nop}
            }
        }
    }
}

//��ȡ����������һ��0xe8 call������inlinehookcheck
ULONG GetNextFunctionAddress(ULONG ulNtDllModuleBase,ULONG ulOldAddress,char *functionName,PINLINEHOOKINFO InlineHookInfo)
{
    ULONG ulCodeSize;

    ULONG ulNextFunCodeSize;
    ULONG ulNextFunReloadCodeSize;
    PUCHAR i;

    PUCHAR ulNextFunctionAddress = NULL;
    PUCHAR ulReloadNextFunctionAddress = NULL;
    BOOL bRetOK = FALSE;
    PUCHAR ulTemp;
    ULONG ulHookFunctionAddress;
    PUCHAR p;

    INSTRUCTION    Inst;
    INSTRUCTION    Instb;

    char lpszHookModuleImage[256];
    ULONG ulHookModuleBase;
    ULONG ulHookModuleSize;
    int Flagss;

    if (!MmIsAddressValid((PVOID)ulOldAddress))
    {
        return bRetOK;
    }
    __try
    {
        ulCodeSize = GetFunctionCodeSize((PVOID)ulOldAddress);
        for (i=(PUCHAR)ulOldAddress;i < i+ulCodeSize;i++)
        {
            //ɨ�������ת
            if (*i == 0xe8)
            {
                ulNextFunctionAddress = (PUCHAR)(*(PULONG)(i+1)+(ULONG)(i+5));
                if (MmIsAddressValid((PVOID)ulNextFunctionAddress))
                {
                    //�ж�һ���Ƿ��ǵ�������
                    if (IsFunctionInExportTable(ulNtDllModuleBase,(ULONG)ulNextFunctionAddress))
                    {
                        return 0;
                    }
                    //��hook ɨ��
                    ulReloadNextFunctionAddress = ulNextFunctionAddress - SystemKernelModuleBase + ImageModuleBase;
                    if (MmIsAddressValid(ulReloadNextFunctionAddress) &&
                        MmIsAddressValid(ulNextFunctionAddress))
                    {
                        ulNextFunCodeSize = GetFunctionCodeSize(ulNextFunctionAddress);
                        ulNextFunReloadCodeSize = GetFunctionCodeSize(ulReloadNextFunctionAddress);

                        if (ulNextFunCodeSize == ulNextFunReloadCodeSize &&
                            memcmp(ulReloadNextFunctionAddress,ulNextFunctionAddress,ulNextFunCodeSize) != 0)
                        {
                            //��hook��
                            for (p = (PUCHAR)ulNextFunctionAddress ;(ULONG)p < (ULONG)ulNextFunctionAddress+ulNextFunCodeSize; p++)
                            {
                                //�۰�ɨ�裬���ǰ��һ��һ������ʼɨ����һ��
                                if (memcmp(ulReloadNextFunctionAddress, ulNextFunctionAddress,ulNextFunCodeSize/2) == 0)
                                {
                                    ulNextFunCodeSize = ulNextFunCodeSize + ulNextFunCodeSize/2;
                                    continue;
                                }
                                //�Ƿ������
                                if (*p == 0xcc ||
                                    *p == 0xc2)
                                {
                                    break;
                                }
                                ulHookFunctionAddress = (*(PULONG)(p + 1) + (ULONG)p + 5);  //�õ���ַ
                                if (!RMmIsAddressValid((PVOID)ulHookFunctionAddress))
                                {
                                    continue;
                                }
                                ulTemp = NULL;
                                get_instruction(&Inst,p,MODE_32);
                                switch (Inst.type)
                                {
                                case INSTRUCTION_TYPE_JMP:
                                    if(Inst.opcode==0xFF&&Inst.modrm==0x25)
                                    {
                                        //DIRECT_JMP
                                        ulTemp = (PUCHAR)Inst.op1.displacement;
                                    }
                                    else if (Inst.opcode==0xEB)
                                    {
                                        ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                                    }
                                    else if(Inst.opcode==0xE9)
                                    {
                                        //RELATIVE_JMP;
                                        ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                                    }
                                    break;
                                case INSTRUCTION_TYPE_CALL:
                                    if(Inst.opcode==0xFF&&Inst.modrm==0x15)
                                    {
                                        //DIRECT_CALL
                                        ulTemp = (PUCHAR)Inst.op1.displacement;
                                    }
                                    else if (Inst.opcode==0x9A)
                                    {
                                        ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                                    }
                                    else if(Inst.opcode==0xE8)
                                    {
                                        //RELATIVE_CALL;
                                        ulTemp = (PUCHAR)(p+Inst.op1.immediate);
                                    }
                                    break;
                                case INSTRUCTION_TYPE_PUSH:
                                    if(!RMmIsAddressValid((PVOID)(p)))
                                    {
                                        break;
                                    }
                                    get_instruction(&Instb,(BYTE*)(p),MODE_32);
                                    if(Instb.type == INSTRUCTION_TYPE_RET)
                                    {
                                        //StartAddress+len-inst.length-instb.length;
                                        ulTemp = (PUCHAR)Instb.op1.displacement;
                                    }
                                    break;
                                }
                                if (ulTemp &&
                                    MmIsAddressValid(ulTemp) &&
                                    MmIsAddressValid(p))   //hook�ĵ�ַҲҪ��Ч�ſ���Ŷ
                                {
                                    if ((ULONG)ulTemp > SystemKernelModuleBase &&
                                        (ULONG)ulTemp < SystemKernelModuleBase+SystemKernelModuleSize)   //̫������Ҳ����
                                    {
                                        goto Next;
                                    }
                                    //ulTempҲ����С�� SystemKernelModuleBase
                                    if ((ULONG)ulTemp < SystemKernelModuleBase)
                                    {
                                        goto Next;
                                    }
                                    if (*(ULONG *)ulTemp == 0x00000000 ||
                                        *(ULONG *)ulTemp == 0x00000005)
                                    {
                                        goto Next;
                                    }
                                    Flagss = 0;
                                    __asm{
                                        mov esi,ulTemp
                                            mov ax,word ptr [esi]
                                        cmp ax,0x0000
                                            jz Cont//��add     byte ptr [eax],al
                                            mov Flagss,1
Cont:
                                    }
                                    if (Flagss != 1)
                                        goto Next;

                                    ulTemp = ulTemp+0x5;
                                    //�򵥴���һ�¶�����
                                    if (*ulTemp == 0xe9 ||
                                        *ulTemp == 0xe8)
                                    {
                                        ulTemp = (PUCHAR)(*(PULONG)(ulTemp+1)+(ULONG)(ulTemp+5));
                                    }
                                    FillInlineHookInfo(ulTemp+0x5,InlineHookInfo,functionName,(ULONG)p,2);
Next:
                                    _asm{nop}
                                }
                            }
                        }
                    }
                }
            }
            //������
            if (*i == 0xcc ||
                *i == 0xc2)
            {
                return 0;
            }
        }

    }__except(EXCEPTION_EXECUTE_HANDLER){

    }

    return 0;
}









BOOLEAN IsFunctionInExportTable(ULONG ulModuleBase,ULONG ulFunctionAddress)
{

    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS NtDllHeader;
    IMAGE_OPTIONAL_HEADER opthdr;
    DWORD* arrayOfFunctionAddresses;
    DWORD* arrayOfFunctionNames;
    WORD* arrayOfFunctionOrdinals;
    DWORD functionOrdinal;
    DWORD Base, x, functionAddress,ulOldAddress;
    IMAGE_EXPORT_DIRECTORY *pExportTable;
    char *functionName;


    __try
    {
        pDosHeader=(PIMAGE_DOS_HEADER)ulModuleBase;
        if (pDosHeader->e_magic!=IMAGE_DOS_SIGNATURE)
        {
            KdPrint(("failed to find NtHeader\r\n"));
            return FALSE;
        }
        NtDllHeader=(PIMAGE_NT_HEADERS)(ULONG)((ULONG)pDosHeader+pDosHeader->e_lfanew);
        if (NtDllHeader->Signature!=IMAGE_NT_SIGNATURE)
        {
            KdPrint(("failed to find NtHeader\r\n"));
            return FALSE;
        }
        opthdr = NtDllHeader->OptionalHeader;
        pExportTable =(IMAGE_EXPORT_DIRECTORY*)((BYTE*)ulModuleBase + opthdr.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT]. VirtualAddress); //�õ�������
        arrayOfFunctionAddresses = (DWORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfFunctions);  //��ַ��
        arrayOfFunctionNames = (DWORD*)((BYTE*)ulModuleBase + pExportTable->AddressOfNames);         //��������
        arrayOfFunctionOrdinals = (WORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfNameOrdinals);

        Base = pExportTable->Base;

        for(x = 0; x < pExportTable->NumberOfFunctions; x++) //��������������ɨ��
        {
            //functionName = (char*)((BYTE*)ulModuleBase + arrayOfFunctionNames[x]);
            functionOrdinal = arrayOfFunctionOrdinals[x] + Base - 1; 
            functionAddress = (DWORD)((BYTE*)ulModuleBase + arrayOfFunctionAddresses[functionOrdinal]);
            //KdPrint(("%08x:%s\r\n",functionAddress,functionName));
            //ulOldAddress = GetSystemRoutineAddress(0,functionName);
            ulOldAddress = functionAddress - ulModuleBase + SystemKernelModuleBase;
            if (ulFunctionAddress == ulOldAddress)
            {
                //�ǵ����������˳�
                return TRUE;
            }
        }

    }__except(EXCEPTION_EXECUTE_HANDLER){

    }
    return FALSE;
}


BOOLEAN ReSetEatHook(CHAR *lpszFunction,ULONG ulReloadKernelModule,ULONG ulKernelModule)
{
    ULONG ulModuleBase;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS NtDllHeader;
    IMAGE_OPTIONAL_HEADER opthdr;
    DWORD* arrayOfFunctionAddresses;
    DWORD* arrayOfFunctionNames;
    WORD* arrayOfFunctionOrdinals;
    DWORD functionOrdinal;
    DWORD Base,x,functionAddress;
    IMAGE_EXPORT_DIRECTORY *pExportTable;
    char *functionName = NULL;
    BOOL bIsEatHooked = FALSE;
    int position;
    ULONG ulFunctionOrdinal;

    //�ָ���ʱ�� ��reload��ImageModuleBase
    ulModuleBase = ulReloadKernelModule;
    pDosHeader = (PIMAGE_DOS_HEADER)ulModuleBase;
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
    pExportTable =(IMAGE_EXPORT_DIRECTORY*)((BYTE*)ulModuleBase + opthdr.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT]. VirtualAddress); //�õ�������
    arrayOfFunctionAddresses = (DWORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfFunctions);  //��ַ��
    arrayOfFunctionNames = (DWORD*)((BYTE*)ulModuleBase + pExportTable->AddressOfNames);         //��������
    arrayOfFunctionOrdinals = (WORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfNameOrdinals);

    Base = pExportTable->Base;

    for(x = 0; x < pExportTable->NumberOfFunctions; x++) //��������������ɨ��
    {
        functionName = (char*)((BYTE*)ulModuleBase + arrayOfFunctionNames[x]);
        ulFunctionOrdinal = arrayOfFunctionOrdinals[x] + Base - 1; 
        ulFunctionOrdinal = arrayOfFunctionAddresses[ulFunctionOrdinal];

        functionAddress = (DWORD)((BYTE*)ulModuleBase + ulFunctionOrdinal);

        if (_stricmp(lpszFunction,functionName) == 0)
        {
            KdPrint(("reload ulFunctionOrdinal:%08x:%s",ulFunctionOrdinal,functionName));

            //��ʼ�ָ�
            ulModuleBase = ulKernelModule;
            pDosHeader = (PIMAGE_DOS_HEADER)ulModuleBase;
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
            pExportTable =(IMAGE_EXPORT_DIRECTORY*)((BYTE*)ulModuleBase + opthdr.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT]. VirtualAddress); //�õ�������
            arrayOfFunctionAddresses = (DWORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfFunctions);  //��ַ��
            arrayOfFunctionNames = (DWORD*)((BYTE*)ulModuleBase + pExportTable->AddressOfNames);         //��������
            arrayOfFunctionOrdinals = (WORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfNameOrdinals);

            Base = pExportTable->Base;

            _asm
            {
                CLI                    
                    MOV    EAX, CR0        
                    AND EAX, NOT 10000H 
                    MOV    CR0, EAX        
            }    
            arrayOfFunctionAddresses[arrayOfFunctionOrdinals[x] + Base - 1] = ulFunctionOrdinal;
            _asm 
            {
                MOV    EAX, CR0        
                    OR    EAX, 10000H            
                    MOV    CR0, EAX            
                    STI                    
            }
            break;
        }
    }

    return TRUE;
}
ULONG GetEatHook(ULONG ulOldAddress,int x,ULONG ulSystemKernelModuleBase,ULONG ulSystemKernelModuleSize)
{
    ULONG ulModuleBase;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS NtDllHeader;
    IMAGE_OPTIONAL_HEADER opthdr;
    DWORD* arrayOfFunctionAddresses;
    DWORD* arrayOfFunctionNames;
    WORD* arrayOfFunctionOrdinals;
    DWORD functionOrdinal;
    DWORD Base,functionAddress;
    IMAGE_EXPORT_DIRECTORY *pExportTable;
    char *functionName = NULL;
    BOOL bIsEatHooked = FALSE;
    ULONG position = 0;
    ULONG ulFunctionOrdinal;

    ulModuleBase = ulSystemKernelModuleBase;
    pDosHeader = (PIMAGE_DOS_HEADER)ulModuleBase;
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
    pExportTable =(IMAGE_EXPORT_DIRECTORY*)((BYTE*)ulModuleBase + opthdr.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT]. VirtualAddress); //�õ�������
    arrayOfFunctionAddresses = (DWORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfFunctions);  //��ַ��
    arrayOfFunctionNames = (DWORD*)((BYTE*)ulModuleBase + pExportTable->AddressOfNames);         //��������
    arrayOfFunctionOrdinals = (WORD*)( (BYTE*)ulModuleBase + pExportTable->AddressOfNameOrdinals);

    Base = pExportTable->Base;

    functionName = (char*)((BYTE*)ulModuleBase + arrayOfFunctionNames[x]);
    ulFunctionOrdinal = arrayOfFunctionOrdinals[x] + Base - 1; 
    functionAddress = (DWORD)((BYTE*)ulModuleBase + arrayOfFunctionAddresses[ulFunctionOrdinal]);

    if (*functionName == 'Z' &&
        *(functionName+1) == 'w')
    {
        position  = *((WORD*)(functionAddress + 1));  //�õ������
        if (position > 0 &&
            position <= OriginalServiceDescriptorTable->TableSize)
        {
            //�õ�ԭʼ��ַ
            functionAddress = OriginalServiceDescriptorTable->ServiceTable[position] - (ULONG)ImageModuleBase + SystemKernelModuleBase;
        }
    }
    if (ulOldAddress != functionAddress)
    {
        KdPrint(("EAT HOOK %08x:%s\r\n",functionAddress,functionName));
        return functionAddress;
    }
    return 0;
}

