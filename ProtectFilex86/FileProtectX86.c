
#ifndef CXX_FILEPROTECTX86_H
#    include "FileProtectX86.h"
#endif

ULONG gC2pKeyCount = 0;
PDRIVER_OBJECT gDriverObject = NULL;

BOOLEAN bOk = FALSE;

ULONG_PTR  IndexOffsetOfFunction = 0;
ULONG_PTR  SSDTDescriptor = 0;
KIRQL Irql;
ULONG_PTR   ulIndex = 0;
ULONG_PTR   ulIndex1 = 0;
ULONG_PTR   ulIndex2 = 0;
pfnNtSetInformationFile Old_NtSetInformationFileWinXP = NULL;
pfnNtDeleteFile Old_NtDeleteFileWinXP = NULL;
//pfnNtCreateFile Old_NtCreateFileWinXP = NULL;
pfnNtWriteFile Old_NtWriteFileWinXP = NULL;
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegisterPath)
{
    ULONG i; 
    NTSTATUS status;

    // ��д���еķַ�������ָ��
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) 
    { 
        DriverObject->MajorFunction[i] = c2pDispatchGeneral; 
    } 

    // ��������дһ��Read�ַ���������ΪҪ�Ĺ��˾��Ƕ�ȡ���İ�����Ϣ
    // �����Ķ�����Ҫ������ַ���������д��
    DriverObject->MajorFunction[IRP_MJ_READ] = c2pDispatchRead; 

    // ��������дһ��IRP_MJ_POWER������������Ϊ���������м�Ҫ����
    // һ��PoCallDriver��һ��PoStartNextPowerIrp���Ƚ����⡣
    DriverObject->MajorFunction [IRP_MJ_POWER] = c2pPower; 

    // ������֪��ʲôʱ��һ�����ǰ󶨹����豸��ж���ˣ�����ӻ�����
    // ���ε��ˣ�������ר��дһ��PNP�����弴�ã��ַ�����
    DriverObject->MajorFunction [IRP_MJ_PNP] = c2pPnP; 

    // ж�غ�����
    DriverObject->DriverUnload = c2pUnload; 
    gDriverObject = DriverObject;
    // �����м����豸
    status =c2pAttachDevices(DriverObject, RegisterPath);

    SSDTDescriptor = (ULONG_PTR)GetFunctionAddressByNameFromNtosExport(L"KeServiceDescriptorTable");
    IndexOffsetOfFunction = 1;

    ulIndex = GetSSDTApiFunctionIndexFromNtdll("NtSetInformationFile");
    ulIndex1 = GetSSDTApiFunctionIndexFromNtdll("NtWriteFile");
    ulIndex2 = GetSSDTApiFunctionIndexFromNtdll("NtDeleteFile");

    HookSSDT(ulIndex);
    HookWrite(ulIndex1);
    HookDelete(ulIndex2);

    return STATUS_SUCCESS;
}



NTSTATUS 
    c2pDevExtInit( 
    IN PC2P_DEV_EXT devExt, 
    IN PDEVICE_OBJECT pFilterDeviceObject, 
    IN PDEVICE_OBJECT pTargetDeviceObject, 
    IN PDEVICE_OBJECT pLowerDeviceObject ) 
{ 
    memset(devExt, 0, sizeof(C2P_DEV_EXT)); 
    devExt->NodeSize = sizeof(C2P_DEV_EXT); 
    devExt->pFilterDeviceObject = pFilterDeviceObject; 
    KeInitializeSpinLock(&(devExt->IoRequestsSpinLock)); 
    KeInitializeEvent(&(devExt->IoInProgressEvent), NotificationEvent, FALSE); 
    devExt->TargetDeviceObject = pTargetDeviceObject; 
    devExt->LowerDeviceObject = pLowerDeviceObject; 
    return( STATUS_SUCCESS ); 
}


// ��������������졣�ܴ���������Kbdclass��Ȼ���
// ����������е��豸��
NTSTATUS 
    c2pAttachDevices( 
    IN PDRIVER_OBJECT DriverObject, 
    IN PUNICODE_STRING RegistryPath 
    ) 
{ 
    NTSTATUS status = 0; 
    UNICODE_STRING uniNtNameString; 
    PC2P_DEV_EXT devExt; 
    PDEVICE_OBJECT pFilterDeviceObject = NULL; 
    PDEVICE_OBJECT pTargetDeviceObject = NULL; 
    PDEVICE_OBJECT pLowerDeviceObject = NULL; 

    PDRIVER_OBJECT KbdDriverObject = NULL; 

    KdPrint(("MyAttach\n")); 

    // ��ʼ��һ���ַ���������Kdbclass���������֡�
    RtlInitUnicodeString(&uniNtNameString, KBD_DRIVER_NAME); 
    // �����ǰ����豸��������ӡ�ֻ������򿪵�����������
    status = ObReferenceObjectByName ( 
        &uniNtNameString, 
        OBJ_CASE_INSENSITIVE, 
        NULL, 
        0, 
        IoDriverObjectType, 
        KernelMode, 
        NULL, 
        &KbdDriverObject 
        ); 
    // ���ʧ���˾�ֱ�ӷ���
    if(!NT_SUCCESS(status)) 
    { 
        KdPrint(("MyAttach: Couldn't get the MyTest Device Object\n")); 
        return( status ); 
    }
    else
    {
        // �������Ҫ��Ӧ�á�����������֮�����ǡ�
        ObDereferenceObject(DriverObject);
    }

    // �����豸���еĵ�һ���豸    
    pTargetDeviceObject = KbdDriverObject->DeviceObject;
    // ���ڿ�ʼ��������豸��
    while (pTargetDeviceObject) 
    {
        // ����һ�������豸������ǰ�����ѧϰ���ġ������IN���OUT�궼��
        // �պֻ꣬�б�־�����壬�������������һ������������������
        status = IoCreateDevice( 
            IN DriverObject, 
            IN sizeof(C2P_DEV_EXT), 
            IN NULL, 
            IN pTargetDeviceObject->DeviceType, 
            IN pTargetDeviceObject->Characteristics, 
            IN FALSE, 
            OUT &pFilterDeviceObject 
            ); 

        // ���ʧ���˾�ֱ���˳���
        if (!NT_SUCCESS(status)) 
        { 
            KdPrint(("MyAttach: Couldn't create the MyFilter Filter Device Object\n")); 
            return (status); 
        } 

        // �󶨡�pLowerDeviceObject�ǰ�֮��õ�����һ���豸��Ҳ����
        // ǰ�泣��˵����ν��ʵ�豸��
        pLowerDeviceObject = 
            IoAttachDeviceToDeviceStack(pFilterDeviceObject, pTargetDeviceObject); 
        // �����ʧ���ˣ�����֮ǰ�Ĳ������˳���
        if(!pLowerDeviceObject) 
        { 
            KdPrint(("MyAttach: Couldn't attach to MyTest Device Object\n")); 
            IoDeleteDevice(pFilterDeviceObject); 
            pFilterDeviceObject = NULL; 
            return( status ); 
        } 

        // �豸��չ������Ҫ��ϸ�����豸��չ��Ӧ�á�
        devExt = (PC2P_DEV_EXT)(pFilterDeviceObject->DeviceExtension); 
        c2pDevExtInit( 
            devExt, 
            pFilterDeviceObject, 
            pTargetDeviceObject, 
            pLowerDeviceObject ); 

        // ����Ĳ�����ǰ����˴��ڵĲ�������һ�¡����ﲻ�ٽ����ˡ�
        pFilterDeviceObject->DeviceType=pLowerDeviceObject->DeviceType; 
        pFilterDeviceObject->Characteristics=pLowerDeviceObject->Characteristics; 
        pFilterDeviceObject->StackSize=pLowerDeviceObject->StackSize+1; 
        pFilterDeviceObject->Flags |= pLowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE) ; 
        //next device 
        pTargetDeviceObject = pTargetDeviceObject->NextDevice;
    }
    return status; 
} 


VOID 
    c2pDetach(IN PDEVICE_OBJECT pDeviceObject) 
{ 
    PC2P_DEV_EXT devExt; 
    BOOLEAN NoRequestsOutstanding = FALSE; 
    devExt = (PC2P_DEV_EXT)pDeviceObject->DeviceExtension; 
    __try 
    { 
        __try 
        { 
            IoDetachDevice(devExt->TargetDeviceObject);
            devExt->TargetDeviceObject = NULL; 
            IoDeleteDevice(pDeviceObject); 
            devExt->pFilterDeviceObject = NULL; 
            DbgPrint(("Detach Finished\n")); 
        } 
        __except (EXCEPTION_EXECUTE_HANDLER){} 
    } 
    __finally{} 
    return; 
}



VOID 
    c2pUnload(IN PDRIVER_OBJECT DriverObject) 
{ 
    PDEVICE_OBJECT DeviceObject; 
    PDEVICE_OBJECT OldDeviceObject; 
    PC2P_DEV_EXT devExt; 

    LARGE_INTEGER    lDelay;
    PRKTHREAD CurrentThread;
    //delay some time 
    lDelay = RtlConvertLongToLargeInteger(100 * DELAY_ONE_MILLISECOND);
    CurrentThread = KeGetCurrentThread();
    // �ѵ�ǰ�߳�����Ϊ��ʵʱģʽ���Ա����������о�����Ӱ����������
    KeSetPriorityThread(CurrentThread, LOW_REALTIME_PRIORITY);

    UNREFERENCED_PARAMETER(DriverObject); 
    KdPrint(("DriverEntry unLoading...\n")); 

    // ���������豸��һ�ɽ����
    DeviceObject = DriverObject->DeviceObject;
    while (DeviceObject)
    {
        // ����󶨲�ɾ�����е��豸
        c2pDetach(DeviceObject);
        DeviceObject = DeviceObject->NextDevice;
    } 
    ASSERT(NULL == DriverObject->DeviceObject);

    while (gC2pKeyCount)
    {
        KeDelayExecutionThread(KernelMode, FALSE, &lDelay);
    }

    UnHookSSDT(ulIndex);
    UnHookSSDTWrite(ulIndex1);
    UnHookSSDTDelete(ulIndex2);
    KdPrint(("DriverEntry unLoad OK!\n")); 
    //return; 
} 


//�������ǲ����ĵ�����IRP
NTSTATUS c2pDispatchGeneral( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    ) 
{ 
    // �����ķַ�������ֱ��skipȻ����IoCallDriver��IRP���͵���ʵ�豸
    // ���豸���� 
    KdPrint(("Other Diapatch!")); 
    IoSkipCurrentIrpStackLocation(Irp); 
    return IoCallDriver(((PC2P_DEV_EXT)
        DeviceObject->DeviceExtension)->LowerDeviceObject, Irp); 
} 
//ֻ���������ܺ�ΪIRP_MJ_POWER��IRP
NTSTATUS c2pPower( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    ) 
{ 
    PC2P_DEV_EXT devExt;
    devExt =
        (PC2P_DEV_EXT)DeviceObject->DeviceExtension; 

    PoStartNextPowerIrp( Irp ); 
    IoSkipCurrentIrpStackLocation( Irp ); 
    return PoCallDriver(devExt->LowerDeviceObject, Irp ); 
} 
//�豸���γ�ʱ�������󶨣���ɾ�������豸
NTSTATUS c2pPnP( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    ) 
{ 
    PC2P_DEV_EXT devExt; 
    PIO_STACK_LOCATION irpStack; 
    NTSTATUS status = STATUS_SUCCESS; 
    KIRQL oldIrql; 
    KEVENT event; 

    // �����ʵ�豸��
    devExt = (PC2P_DEV_EXT)(DeviceObject->DeviceExtension); 
    irpStack = IoGetCurrentIrpStackLocation(Irp); 

    switch (irpStack->MinorFunction) 
    { 
    case IRP_MN_REMOVE_DEVICE: 
        KdPrint(("IRP_MN_REMOVE_DEVICE\n")); 

        // ���Ȱ�������ȥ
        IoSkipCurrentIrpStackLocation(Irp); 
        IoCallDriver(devExt->LowerDeviceObject, Irp); 
        // Ȼ�����󶨡�
        IoDetachDevice(devExt->LowerDeviceObject); 
        // ɾ�������Լ����ɵ������豸��
        IoDeleteDevice(DeviceObject); 
        status = STATUS_SUCCESS; 
        break; 

    default: 
        // �����������͵�IRP��ȫ����ֱ���·����ɡ� 
        IoSkipCurrentIrpStackLocation(Irp); 
        status = IoCallDriver(devExt->LowerDeviceObject, Irp); 
    } 
    return status; 
}

// ����һ��IRP��ɻص�������ԭ��
NTSTATUS c2pReadComplete( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp, 
    IN PVOID Context 
    ) 
{
    POBJECT_NAME_INFORMATION ObjetNameInfor;  
    ULONG* ulProcessNameLen;
    PIO_STACK_LOCATION IrpSp;
    ULONG buf_len = 0;
    PUCHAR buf = NULL;
    size_t i;
    ULONG numKeys = 0;
    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //  �����������ǳɹ��ġ�����Ȼ���������ʧ���ˣ���ô��ȡ
    //   ��һ������Ϣ��û����ġ�
    if( NT_SUCCESS( Irp->IoStatus.Status ) ) 
    { 
        PKEYBOARD_INPUT_DATA pKeyData;
        // ��ö�������ɺ�����Ļ�����
        buf = Irp->AssociatedIrp.SystemBuffer;
        pKeyData = Irp->AssociatedIrp.SystemBuffer;

        // �������������ĳ��ȡ�һ���˵����ֵ�ж೤��������
        // Information�С�

        buf_len = Irp->IoStatus.Information;
        numKeys = Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

        __try
        {
            if (NT_SUCCESS(IoQueryFileDosDeviceName((PFILE_OBJECT)IrpSp->FileObject, &ObjetNameInfor)))  
            {  
                if(wcsstr(ObjetNameInfor->Name.Buffer,L"Shine.txt")!=0)
                {
                    DbgPrint("aaaaaaa");
                }
            }  
        }
        __except(1)
        {
            DbgPrint("Exception:%x",GetExceptionCode());
        }


        //ͨ��Process��ý�������
        for(i = 0; i < numKeys; i++) 
        {
            //    DbgPrint("%02X %d\n",pKeyData[i].MakeCode,pKeyData[i].Flags);

            if(pKeyData[i].MakeCode == 0x1d && pKeyData[i].Flags == KEY_MAKE)
            {
                //��Ctrl
                bOk = TRUE;
            }

            if(pKeyData[i].MakeCode == 0x2e && pKeyData[i].Flags == KEY_MAKE && bOk == TRUE ) //����
            {
                pKeyData[i].MakeCode = 0x20;
                bOk = FALSE;
                DbgPrint("aaaaaaaaaaaaaa");
            }
        }
        //�� �����������һ���Ĵ���������ܼ򵥵Ĵ�ӡ�����е�ɨ
        // ���롣

        //    for(i=0;i<buf_len;++i)
        //    {
        //DbgPrint("ctrl2cap: %2x\r\n", buf[i]);
        //        if(buf[i]==0x3a)
        //        {
        //            DbgPrint("SSSSSS");
        //        }
        //    }

    }
    gC2pKeyCount--;

    if( Irp->PendingReturned )
    { 
        IoMarkIrpPending( Irp ); 
    } 
    return Irp->IoStatus.Status;
}

NTSTATUS c2pDispatchRead( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp ) 
{ 
    NTSTATUS status = STATUS_SUCCESS; 
    PC2P_DEV_EXT devExt; 
    PIO_STACK_LOCATION currentIrpStack; 
    KEVENT waitEvent;
    KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

    if (Irp->CurrentLocation == 1) 
    { 
        ULONG ReturnedInformation = 0; 
        KdPrint(("Dispatch encountered bogus current location\n")); 
        status = STATUS_INVALID_DEVICE_REQUEST; 
        Irp->IoStatus.Status = status; 
        Irp->IoStatus.Information = ReturnedInformation; 
        IoCompleteRequest(Irp, IO_NO_INCREMENT); 
        return(status); 
    } 

    // ȫ�ֱ�������������1
    gC2pKeyCount++;

    // �õ��豸��չ��Ŀ����֮��Ϊ�˻����һ���豸��ָ�롣
    devExt =
        (PC2P_DEV_EXT)DeviceObject->DeviceExtension;

    // ���ûص���������IRP������ȥ�� ֮����Ĵ���Ҳ�ͽ����ˡ�
    // ʣ�µ�������Ҫ�ȴ���������ɡ�
    currentIrpStack = IoGetCurrentIrpStackLocation(Irp); 
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine( Irp, c2pReadComplete, 
        DeviceObject, TRUE, TRUE, TRUE ); 
    return  IoCallDriver( devExt->LowerDeviceObject, Irp );     
}

VOID  HookSSDT(ULONG_PTR ulIndex)
{
    PULONG32  ServiceTableBase = NULL;
    ServiceTableBase = (PULONG32)((PSYSTEM_SERVICE_TABLE32)SSDTDescriptor)->ServiceTableBase;    //�����׵�ַ
    Old_NtSetInformationFileWinXP  = (pfnNtSetInformationFile)ServiceTableBase[ulIndex];      //�ȱ���ԭ�ȵĺ�����ַ

    WPOFF();  
    ServiceTableBase[ulIndex] = (ULONG32)Fake_NtSetInformationFileWinXP;  //��KeBugCheckEx������ƫ�Ƶ�ַ����SSDT����
    WPON();    
}

VOID HookWrite(ULONG_PTR ulIndex)
{
    PULONG32  ServiceTableBase = NULL;
    ServiceTableBase = (PULONG32)((PSYSTEM_SERVICE_TABLE32)SSDTDescriptor)->ServiceTableBase;    //�����׵�ַ
    Old_NtWriteFileWinXP  = (pfnNtWriteFile)ServiceTableBase[ulIndex];      //�ȱ���ԭ�ȵĺ�����ַ

    WPOFF();  
    ServiceTableBase[ulIndex] = (ULONG32)Fake_NtWriteFileWinXP;  //��KeBugCheckEx������ƫ�Ƶ�ַ����SSDT����
    WPON();
}

VOID HookDelete(ULONG_PTR ulIndex)
{
    PULONG32  ServiceTableBase = NULL;
    ServiceTableBase = (PULONG32)((PSYSTEM_SERVICE_TABLE32)SSDTDescriptor)->ServiceTableBase;    //�����׵�ַ
    Old_NtDeleteFileWinXP  = (pfnNtDeleteFile)ServiceTableBase[ulIndex];      //�ȱ���ԭ�ȵĺ�����ַ

    WPOFF();  
    ServiceTableBase[ulIndex] = (ULONG32)Fake_NtDeleteFileWinXP;  //��KeBugCheckEx������ƫ�Ƶ�ַ����SSDT����
    WPON();
}


VOID
    UnHookSSDT(ULONG_PTR ulIndex)
{
    PULONG32  ServiceTableBase = NULL;
    ServiceTableBase=(PULONG32)((PSYSTEM_SERVICE_TABLE32)SSDTDescriptor)->ServiceTableBase;

    WPOFF();
    ServiceTableBase[ulIndex] = (ULONG32)Old_NtSetInformationFileWinXP;
    WPON();
}

VOID
    UnHookSSDTWrite(ULONG_PTR ulIndex)
{

    PULONG32  ServiceTableBase = NULL;
    ServiceTableBase=(PULONG32)((PSYSTEM_SERVICE_TABLE32)SSDTDescriptor)->ServiceTableBase;

    WPOFF();
    ServiceTableBase[ulIndex] = (ULONG32)Old_NtWriteFileWinXP;
    WPON();

}

VOID
    UnHookSSDTDelete(ULONG_PTR ulIndex)
{
    PULONG32  ServiceTableBase = NULL;
    ServiceTableBase=(PULONG32)((PSYSTEM_SERVICE_TABLE32)SSDTDescriptor)->ServiceTableBase;

    WPOFF();
    ServiceTableBase[ulIndex] = (ULONG32)Old_NtDeleteFileWinXP;
    WPON();
}


NTSTATUS Fake_NtSetInformationFileWinXP(
    __in HANDLE FileHandle,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __in_bcount(Length) PVOID FileInformation,
    __in ULONG Length,
    __in FILE_INFORMATION_CLASS FileInformationClass
    )
{
    NTSTATUS Status;
    PFILE_OBJECT hObject;
    POBJECT_NAME_INFORMATION ObjetNameInfor;  

    Status = ObReferenceObjectByHandle(FileHandle,FILE_READ_DATA,0,KernelMode,&hObject, 0);
    //ͨ�����̾����ȡEProcess����

    if (NT_SUCCESS(IoQueryFileDosDeviceName((PFILE_OBJECT)hObject, &ObjetNameInfor)))  
    {  
        if(wcsstr((ObjetNameInfor->Name).Buffer,L"D:\\Shine.txt"))
        {
            if(FileInformationClass == FileRenameInformation)
            {
                return STATUS_ACCESS_DENIED;
            }
        }
    }  

    return Old_NtSetInformationFileWinXP(FileHandle,IoStatusBlock,FileInformation,Length,FileInformationClass);
}

NTSTATUS
    Fake_NtWriteFileWinXP (
    __in HANDLE FileHandle,
    __in_opt HANDLE Event,
    __in_opt PIO_APC_ROUTINE ApcRoutine,
    __in_opt PVOID ApcContext,
    __out PIO_STATUS_BLOCK IoStatusBlock,
    __in_bcount(Length) PVOID Buffer,
    __in ULONG Length,
    __in_opt PLARGE_INTEGER ByteOffset,
    __in_opt PULONG Key
    )
{
    NTSTATUS Status;
    PFILE_OBJECT hObject;
    POBJECT_NAME_INFORMATION ObjetNameInfor;  

    Status = ObReferenceObjectByHandle(FileHandle,FILE_READ_DATA,0,KernelMode,&hObject, 0);
    //ͨ�����̾����ȡEProcess����

    if (NT_SUCCESS(IoQueryFileDosDeviceName((PFILE_OBJECT)hObject, &ObjetNameInfor)))  
    {  
        if(wcsstr((ObjetNameInfor->Name).Buffer,L"D:\\Shine.txt"))
        {
            return STATUS_ACCESS_DENIED;
        }
    }  

    return Old_NtWriteFileWinXP(FileHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,Buffer,Length,ByteOffset,Key);
}


NTSTATUS Fake_NtDeleteFileWinXP(
    __in POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    if(wcsstr((ObjectAttributes->ObjectName)->Buffer,L"D:\\Shine.txt"))
    {
        return STATUS_ACCESS_DENIED;
    }
    return Old_NtDeleteFileWinXP(ObjectAttributes);
}



PVOID 
    GetFunctionAddressByNameFromNtosExport(WCHAR *wzFunctionName)
{
    UNICODE_STRING uniFunctionName;  
    PVOID FunctionAddress = NULL;

    if (wzFunctionName && wcslen(wzFunctionName) > 0)
    {
        RtlInitUnicodeString(&uniFunctionName, wzFunctionName);      
        FunctionAddress = MmGetSystemRoutineAddress(&uniFunctionName);  
    }

    return FunctionAddress;
}

LONG GetSSDTApiFunctionIndexFromNtdll(char* szFindFunctionName)
{

    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PVOID    MapBase = NULL;
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
    WCHAR wzNtdll[] = L"\\SystemRoot\\System32\\ntdll.dll";

    Status = MapFileInUserSpace(wzNtdll, NtCurrentProcess(), &MapBase, &ViewSize);
    if (!NT_SUCCESS(Status))
    {
        return STATUS_UNSUCCESSFUL;
    }
    else
    {
        __try{
            NtHeader = RtlImageNtHeader(MapBase);
            if (NtHeader && NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress){
                ExportTable =(IMAGE_EXPORT_DIRECTORY*)((ULONG_PTR)MapBase + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
                FunctionAddresses = (ULONG*)((ULONG_PTR)MapBase + ExportTable->AddressOfFunctions);
                FunctionNames = (ULONG*)((ULONG_PTR)MapBase + ExportTable->AddressOfNames);
                FunctionIndexs = (USHORT*)((ULONG_PTR)MapBase + ExportTable->AddressOfNameOrdinals);
                for(i = 0; i < ExportTable->NumberOfNames; i++)
                {
                    szFunctionName = (LPSTR)((ULONG_PTR)MapBase + FunctionNames[i]);
                    if (_stricmp(szFunctionName, szFindFunctionName) == 0) 
                    {
                        ulIndex = FunctionIndexs[i]; 
                        ulFunctionAddress = (ULONG_PTR)((ULONG_PTR)MapBase + FunctionAddresses[ulIndex]);
                        ulIndex=*(ULONG*)(ulFunctionAddress+IndexOffsetOfFunction);
                        break;
                    }
                }
            }
        }__except(EXCEPTION_EXECUTE_HANDLER)
        {

        }
    }

    if (ulIndex == -1)
    {
        DbgPrint("%s Get Index Error\n", szFindFunctionName);
    }

    ZwUnmapViewOfSection(NtCurrentProcess(), MapBase);
    return ulIndex;
}

NTSTATUS 
    MapFileInUserSpace(WCHAR* wzFilePath,IN HANDLE hProcess OPTIONAL,
    OUT PVOID *BaseAddress,
    OUT PSIZE_T ViewSize OPTIONAL)
{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    HANDLE   hFile = NULL;
    HANDLE   hSection = NULL;
    OBJECT_ATTRIBUTES oa;
    SIZE_T MapViewSize = 0;
    IO_STATUS_BLOCK Iosb;
    UNICODE_STRING uniFilePath;

    if (!wzFilePath || !BaseAddress){
        return Status;
    }

    RtlInitUnicodeString(&uniFilePath, wzFilePath);
    InitializeObjectAttributes(&oa,
        &uniFilePath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
        );

    Status = IoCreateFile(&hFile,
        GENERIC_READ | SYNCHRONIZE,
        &oa,
        &Iosb,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0,
        CreateFileTypeNone,
        NULL,
        IO_NO_PARAMETER_CHECKING
        );

    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    oa.ObjectName = NULL;
    Status = ZwCreateSection(&hSection,
        SECTION_QUERY | SECTION_MAP_READ,
        &oa,
        NULL,
        PAGE_WRITECOPY,
        SEC_IMAGE,
        hFile
        );
    ZwClose(hFile);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    if (!hProcess){
        hProcess = NtCurrentProcess();
    }

    Status = ZwMapViewOfSection(hSection, 
        hProcess, 
        BaseAddress, 
        0, 
        0, 
        0, 
        ViewSize ? ViewSize : &MapViewSize, 
        ViewUnmap, 
        0, 
        PAGE_WRITECOPY
        );
    ZwClose(hSection);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return Status;
}

ULONG_PTR GetFunctionAddressByIndexFromSSDT32(ULONG_PTR ulIndex,ULONG_PTR SSDTDescriptor)
{
    ULONG_PTR ServiceTableBase= 0 ;
    PSYSTEM_SERVICE_TABLE32 SSDT = (PSYSTEM_SERVICE_TABLE32)SSDTDescriptor;

    ServiceTableBase=(ULONG)(SSDT ->ServiceTableBase);

    return (*(PULONG_PTR)(ServiceTableBase + 4 * ulIndex));
}

VOID WPOFF()
{
    ULONG_PTR cr0 = 0;
    Irql = KeRaiseIrqlToDpcLevel();
    cr0 =__readcr0();
    cr0 &= 0xfffffffffffeffff;
    __writecr0(cr0);
    //_disable();
}

VOID WPON()
{
    ULONG_PTR cr0=__readcr0();
    cr0 |= 0x10000;
    //_enable();
    __writecr0(cr0);
    KeLowerIrql(Irql);
}


