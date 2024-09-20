

#ifndef CXX_PROTECTPROCESSX64_H
#    include "ProtectProcessx64.h"
#endif


PVOID obHandle;//����һ��void*���͵ı�������������ΪObRegisterCallbacks�����ĵ�2��������

NTSTATUS
DriverEntry(IN PDRIVER_OBJECT pDriverObj, IN PUNICODE_STRING pRegistryString)
{
    NTSTATUS status = STATUS_SUCCESS;
    PLDR_DATA_TABLE_ENTRY64 ldr;

    pDriverObj->DriverUnload = DriverUnload;
    // �ƹ�MmVerifyCallbackFunction��
    ldr = (PLDR_DATA_TABLE_ENTRY64)pDriverObj->DriverSection;
    ldr->Flags |= 0x20;

    ProtectProcess(TRUE);

    return STATUS_SUCCESS;
}



NTSTATUS ProtectProcess(BOOLEAN Enable)
{
    OB_CALLBACK_REGISTRATION obReg;
    OB_OPERATION_REGISTRATION opReg;

    memset(&obReg, 0, sizeof(obReg));
    obReg.Version = ObGetFilterVersion();
    obReg.OperationRegistrationCount = 1;
    obReg.RegistrationContext = NULL;
    RtlInitUnicodeString(&obReg.Altitude, L"321000");
    memset(&opReg, 0, sizeof(opReg)); //��ʼ���ṹ�����

    //���� ��ע������ṹ��ĳ�Ա�ֶε�����
    opReg.ObjectType = PsProcessType;
    opReg.Operations = OB_OPERATION_HANDLE_CREATE|OB_OPERATION_HANDLE_DUPLICATE; 
    opReg.PreOperation = (POB_PRE_OPERATION_CALLBACK)&preCall; //������ע��һ���ص�����ָ��
    obReg.OperationRegistration = &opReg; //ע����һ�����

    return ObRegisterCallbacks(&obReg, &obHandle); //������ע��ص�����
}


OB_PREOP_CALLBACK_STATUS 
    preCall(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
    HANDLE pid = PsGetProcessId((PEPROCESS)pOperationInformation->Object);
    char szProcName[16]={0};
    UNREFERENCED_PARAMETER(RegistrationContext);
    strcpy(szProcName,GetProcessImageNameByProcessID((ULONG)pid));
    if( !_stricmp(szProcName,"calc.exe") )
    {
        if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)
        {
            if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_TERMINATE) == PROCESS_TERMINATE)
            {
                //Terminate the process, such as by calling the user-mode TerminateProcess routine..
                pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
            }
            if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_OPERATION) == PROCESS_VM_OPERATION)
            {
                //Modify the address space of the process, such as by calling the user-mode WriteProcessMemory and VirtualProtectEx routines.
                pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_OPERATION;
            }
            if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_READ) == PROCESS_VM_READ)
            {
                //Read to the address space of the process, such as by calling the user-mode ReadProcessMemory routine.
                pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
            }
            if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
            {
                //Write to the address space of the process, such as by calling the user-mode WriteProcessMemory routine.
                pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
            }
        }
    }
    return OB_PREOP_SUCCESS;
}


/*
OpenProcess ��һֱ����ص���  ֱ������
char*
    GetProcessImageNameByProcessID(ULONG ulProcessID)
{
    CLIENT_ID Cid;    
    HANDLE    hProcess;
    NTSTATUS  Status;
    OBJECT_ATTRIBUTES  oa;
    PEPROCESS  EProcess = NULL;

    Cid.UniqueProcess = (HANDLE)ulProcessID;
    Cid.UniqueThread = 0;

    InitializeObjectAttributes(&oa,0,0,0,0);
    Status = ZwOpenProcess(&hProcess,PROCESS_ALL_ACCESS,&oa,&Cid);    //hProcess
    //ǿ�򿪽��̻�þ��
    if (!NT_SUCCESS(Status))
    {
        return FALSE;
    }
    Status = ObReferenceObjectByHandle(hProcess,FILE_READ_DATA,0,
        KernelMode,&EProcess, 0);
    //ͨ�������ȡEProcess
    if (!NT_SUCCESS(Status))
    {
        ZwClose(hProcess);
        return FALSE;
    }
    ObDereferenceObject(EProcess);
    //����ж�
    ZwClose(hProcess);
    //ͨ��EProcess��ý�������
    return (char*)PsGetProcessImageFileName(EProcess);     
    
}
*/

char*
    GetProcessImageNameByProcessID(ULONG ulProcessID)
{
    NTSTATUS  Status;
    PEPROCESS  EProcess = NULL;

    Status = PsLookupProcessByProcessId((HANDLE)ulProcessID,&EProcess);    //hProcess

    //ͨ�������ȡEProcess
    if (!NT_SUCCESS(Status))
    {
        return FALSE;
    }
    ObDereferenceObject(EProcess);
    //ͨ��EProcess��ý�������
    return (char*)PsGetProcessImageFileName(EProcess);
}



VOID
DriverUnload(IN PDRIVER_OBJECT pDriverObj)
{    
    UNREFERENCED_PARAMETER(pDriverObj);
    DbgPrint("driver unloading...\n");

    ObUnRegisterCallbacks(obHandle); //obHandle�����涨��� PVOID obHandle;
}



