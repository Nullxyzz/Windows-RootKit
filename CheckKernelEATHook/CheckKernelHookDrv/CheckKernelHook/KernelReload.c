
#include "KernelReload.h"
#include "FileSystem.h"
#include "FixRelocation.h"


/*ZwQuerySystemInformation�� ö��ģ����Ϣ  ��õ�һģ��  Ntos..*/
BOOLEAN GetSystemKernelModuleInfo(WCHAR **SystemKernelModulePath,PDWORD SystemKernelModuleBase,PDWORD SystemKernelModuleSize)
{
    NTSTATUS status;
    ULONG ulSize,i;
    PMODULES pModuleList;
    char *lpszKernelName=NULL;
    ANSI_STRING AnsiKernelModule;
    UNICODE_STRING UnicodeKernelModule;
    BOOLEAN bRet=TRUE;

    __try
    {
        status=ZwQuerySystemInformation(
            SystemModuleInformation,
            NULL,
            0,
            &ulSize
            );
        if (status != STATUS_INFO_LENGTH_MISMATCH)
        {
            return FALSE;
        }
        pModuleList=(PMODULES)ExAllocatePool(NonPagedPool,ulSize);
        if (pModuleList)
        {
            status=ZwQuerySystemInformation(
                SystemModuleInformation,
                pModuleList,
                ulSize,
                &ulSize
                );
            if (!NT_SUCCESS(status))
            {
                bRet = FALSE;
            }
        }
        if (!bRet)
        {
            if (pModuleList)
                ExFreePool(pModuleList);
            return FALSE;
        }
        *SystemKernelModulePath=ExAllocatePool(NonPagedPool,260*2);
        if (*SystemKernelModulePath==NULL)
        {
            *SystemKernelModuleBase=0;
            *SystemKernelModuleSize=0;
            return FALSE;
        }

        lpszKernelName = pModuleList->smi[0].ModuleNameOffset+pModuleList->smi[0].ImageName;  //��һģ������
        RtlInitAnsiString(&AnsiKernelModule,lpszKernelName);
        RtlAnsiStringToUnicodeString(&UnicodeKernelModule,&AnsiKernelModule,TRUE);

        RtlZeroMemory(*SystemKernelModulePath,260*2);
        wcscat(*SystemKernelModulePath,L"\\SystemRoot\\system32\\");

        memcpy(
            *SystemKernelModulePath+wcslen(L"\\SystemRoot\\system32\\"),    //��һģ��·��
            UnicodeKernelModule.Buffer,
            UnicodeKernelModule.Length
            );

        *SystemKernelModuleBase=(DWORD)pModuleList->smi[0].Base;   //��õ�һģ���ַ
        *SystemKernelModuleSize=(DWORD)pModuleList->smi[0].Size;   //��õ�һģ���С
        ExFreePool(pModuleList);
        RtlFreeUnicodeString(&UnicodeKernelModule);

    }__except(EXCEPTION_EXECUTE_HANDLER){

    }
    return TRUE;
}


/*����ļ�������DeviceObject��RealDevice*/
BOOLEAN IoGetFileSystemVpbInfo(IN PFILE_OBJECT FileObject,PDEVICE_OBJECT *DeviceObject,PDEVICE_OBJECT *RealDevice)
{
    //PDEVICE_OBJECT deviceObject;
    // If the file object has a mounted Vpb, use its DeviceObject.
    if(FileObject->Vpb != NULL && FileObject->Vpb->DeviceObject != NULL)
    {
        *DeviceObject = FileObject->Vpb->DeviceObject;
        *RealDevice= FileObject->Vpb->RealDevice;

        // Otherwise, if the real device has a VPB that indicates that it is mounted,
        // then use the file system device object associated with the VPB.
    }
    else if
        (
        !(FileObject->Flags & FO_DIRECT_DEVICE_OPEN)
        &&
        FileObject->DeviceObject->Vpb != NULL
        &&
        FileObject->DeviceObject->Vpb->DeviceObject != NULL
        )
    {
        *DeviceObject = FileObject->DeviceObject->Vpb->DeviceObject;
        *RealDevice = FileObject->DeviceObject->Vpb->RealDevice;
        // Otherwise, just return the real device object.
    }
    else
    {
        *DeviceObject = FileObject->DeviceObject;
        *RealDevice=NULL;
    }
    if (*RealDevice==NULL||*DeviceObject==NULL)
    {
        return FALSE;
    }
    // Simply return the resultant file object.
    return TRUE;
}



//���FileObject�е�RealDevice��DeviceObject
BOOLEAN GetDeviceObjectFromFileFullName(WCHAR *FileFullName,PDEVICE_OBJECT *RealDevice, PDEVICE_OBJECT *DeviceObject)
{
    WCHAR wRootName[32]={0};
    UNICODE_STRING RootName;
    OBJECT_ATTRIBUTES ObjectAttributes={0};
    NTSTATUS status;
    HANDLE hFile;
    IO_STATUS_BLOCK IoStatus;
    PFILE_OBJECT FileObject;
    if (FileFullName[0]==0x005C)
    {//in   \Windows\system32\ntkrnlpa.exe
        wcscpy(wRootName,L"\\SystemRoot");
    }
    else
    {
        wcscpy(wRootName,L"\\DosDevices\\*:\\");
        wRootName[12]=FileFullName[0];
    }
    RtlInitUnicodeString(&RootName,wRootName);

    InitializeObjectAttributes(&ObjectAttributes, &RootName,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    //RootName.Buffer = "\SystemRoot"
    status = IoCreateFile(
        &hFile,
        SYNCHRONIZE,
        &ObjectAttributes,
        &IoStatus,
        0,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        FILE_OPEN,
        FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0,
        0,
        NULL,
        IO_NO_PARAMETER_CHECKING);

    if (!NT_SUCCESS(status))
    {

        return FALSE;
    }
    status=ObReferenceObjectByHandle(hFile,1,*IoFileObjectType,KernelMode,&FileObject,NULL);
    if (!NT_SUCCESS(status))
    {
        ZwClose(hFile);
        return FALSE;
    }
    if(!IoGetFileSystemVpbInfo(FileObject,DeviceObject,RealDevice))  //���FileObject�е�deviceObject��RealDevice
    {
        ObfDereferenceObject(FileObject);
        ZwClose(hFile);
        return FALSE;

    }
    ObfDereferenceObject(FileObject);
    ZwClose(hFile);

    return TRUE;

}

/*���ϵͳĿ¼*/
BOOLEAN GetWindowsRootName(WCHAR *WindowsRootName)
{
    UNICODE_STRING RootName,ObjectName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE hLink;
    NTSTATUS status;
    WCHAR *SystemRootName=(WCHAR*)0x7FFE0030;
    WCHAR *ObjectNameBuffer=(WCHAR*)ExAllocatePool(NonPagedPool,260*2);
    if (ObjectNameBuffer==NULL)
    {
        return FALSE;
    }
    RtlZeroMemory(ObjectNameBuffer,260*2);
    RtlInitUnicodeString(&RootName,L"\\SystemRoot");
    InitializeObjectAttributes(&ObjectAttributes,&RootName,OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    status=ZwOpenSymbolicLinkObject(&hLink,1,&ObjectAttributes);
    if (NT_SUCCESS(status))
    {
        ObjectName.Buffer=ObjectNameBuffer;
        ObjectName.Length=0;
        ObjectName.MaximumLength=260*2;
        status=ZwQuerySymbolicLinkObject(hLink,&ObjectName,NULL);
        //ObjectNameBuffer   \Device\Harddisk0\Partition1\Windows
        if (NT_SUCCESS(status))
        {
            int ObjectNameLength=ObjectName.Length/2;
            int Index;
            for (Index=ObjectNameLength-1;Index>0;Index--)
            {
                if (ObjectNameBuffer[Index]==0x005C)
                {
                    if (!MmIsAddressValid(&WindowsRootName[ObjectNameLength-Index]))
                    {
                        break;

                    }
                    //\Windows  WindowsRootName
                    RtlCopyMemory(WindowsRootName,&ObjectNameBuffer[Index],(ObjectNameLength-Index)*2);
                    ExFreePool(ObjectNameBuffer);
                    return TRUE;
                }

            }
        }

    }
    ExFreePool(ObjectNameBuffer);
    if (!MmIsAddressValid(SystemRootName))
    {
        return FALSE;
    }
    if (SystemRootName[1]!=0x003A||SystemRootName[2]!=0x005C)
    {
        return FALSE;
    }
    wcscpy(WindowsRootName,&SystemRootName[2]);

    return TRUE;


}


/*
�Լ������ļ����󣬹���FileObject->IrpList  �������ļ����
*/
//\SystemRoot\system32\ntkrnlpa.exe
NTSTATUS  KernelOpenFile(wchar_t *FileFullName, 
    PHANDLE FileHandle, 
    ACCESS_MASK DesiredAccess, 
    ULONG FileAttributes, 
    ULONG ShareAccess, 
    ULONG CreateDisposition, 
    ULONG CreateOptions)
{
    WCHAR SystemRootName[32]=L"\\SystemRoot";
    WCHAR *FileNodeName=NULL;
    UNICODE_STRING FilePath;
    PDEVICE_OBJECT RealDevice,DeviceObject;
    NTSTATUS status=STATUS_UNSUCCESSFUL;
    PFILE_OBJECT FileObject;

    FileNodeName=ExAllocatePool(NonPagedPool,260*2);
    if (FileNodeName==NULL)
    {
        return status;
    }
    RtlZeroMemory(FileNodeName,260*2);

    if (_wcsnicmp(FileFullName,SystemRootName,wcslen(SystemRootName))==0) //������ȷ���ʲô��  ����Ӧ���ǲ�����·��  ���������޸�
    {
        //in 
        int Len;
        if(!GetWindowsRootName(FileNodeName))  //  \Windows
        {
            ExFreePool(FileNodeName);
            return status;
        }
        Len=wcslen(SystemRootName);
        wcscat(FileNodeName,&FileFullName[Len]);
        //FileNodeName ==  \Windows\system32\ntkrnlpa.exe
        //FileFullName ==  \SystemRoot\system32\ntkrnlpa.exe
    }
    else
    {
        if (FileFullName[1]!=0x003A||FileFullName[2]!=0x005C)
        {
            return status;

        }
        wcscpy(FileNodeName,&FileFullName[2]);
    }

    if(!GetDeviceObjectFromFileFullName(FileFullName,&RealDevice,&DeviceObject)) //���FileObject�е�DeviceObject��RealDevice
    {
        ExFreePool(FileNodeName);
        return status;
    }
    //FileNodeName ==  \Windows\system32\ntkrnlpa.exe
    RtlInitUnicodeString(&FilePath,FileNodeName);

    status=IrpCreateFile(&FilePath,DesiredAccess,FileAttributes,ShareAccess,CreateDisposition,CreateOptions,DeviceObject,RealDevice,&FileObject);
    //�����ļ�����   ����FileObject->IrpList��  
    if (!NT_SUCCESS(status))
    {
        ExFreePool(FileNodeName);
        return status;
    }

    //�����ļ����󣬻���ļ����
    status=ObOpenObjectByPointer(
        FileObject,
        OBJ_KERNEL_HANDLE,    //verifier�²���Ҫָ��OBJ_KERNEL_HANDLE
        0,
        DesiredAccess|0x100000,
        *IoFileObjectType,
        0,
        FileHandle);

    ObfDereferenceObject(FileObject);


    return status;

}




//��ѯirp��Ϣ������filesize
NTSTATUS  KernelGetFileSize(HANDLE hFile, PLARGE_INTEGER FileSize)
{
    NTSTATUS status;
    PFILE_OBJECT FileObject;
    PDEVICE_OBJECT DeviceObject,RealDevice;
    FILE_STANDARD_INFORMATION FileInformation;

    status=ObReferenceObjectByHandle(hFile, 0, *IoFileObjectType, KernelMode, &FileObject, 0);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if(!IoGetFileSystemVpbInfo(FileObject,&DeviceObject,&RealDevice))
    {
        ObDereferenceObject(FileObject);
        return STATUS_UNSUCCESSFUL;
    }
    //��ѯirp��ջ��Ϣ������FileObject
    status=IrpQueryInformationFile(FileObject,DeviceObject,&FileInformation,sizeof(FILE_STANDARD_INFORMATION),FileStandardInformation);
    if (!NT_SUCCESS(status))
    {
        ObDereferenceObject(FileObject);
        return status;
    }
    FileSize->HighPart=FileInformation.EndOfFile.HighPart;
    FileSize->LowPart=FileInformation.EndOfFile.LowPart;
    ObDereferenceObject(FileObject);
    return status;
}




/*
�����ļ�������ļ���С��ȡ�ļ����ڴ���
*/
NTSTATUS KernelReadFile(HANDLE hFile, PLARGE_INTEGER ByteOffset, ULONG Length, PVOID FileBuffer, PIO_STATUS_BLOCK IoStatusBlock)
{
    NTSTATUS status;
    PFILE_OBJECT FileObject;
    PDEVICE_OBJECT DeviceObject,RealDevice;
    FILE_STANDARD_INFORMATION FileInformation;
    status=ObReferenceObjectByHandle(hFile, 0, *IoFileObjectType, KernelMode, &FileObject, 0);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if(!IoGetFileSystemVpbInfo(FileObject,&DeviceObject,&RealDevice))
    {
        ObDereferenceObject(FileObject);
        return STATUS_UNSUCCESSFUL;
    }
    status=IrpReadFile(FileObject,DeviceObject,IoStatusBlock,FileBuffer,Length,ByteOffset);  //Irp���󣬽��ļ����뻺������
    ObDereferenceObject(FileObject);
    return status;

}



/*
�޸�FileBuffer�е�ƫ��  ����VirtualAglin  ����  
filebuffer Ϊ��ȡ���ڴ�  ��ImageModuleBaseΪϵͳ�е�ģ���ַ
*/
BOOLEAN ImageFile(BYTE *FileBuffer,BYTE **ImageModuleBase)
{
    PIMAGE_DOS_HEADER ImageDosHeader;
    PIMAGE_NT_HEADERS ImageNtHeaders;
    PIMAGE_SECTION_HEADER ImageSectionHeader;
    DWORD FileAlignment,SectionAlignment,NumberOfSections,SizeOfImage,SizeOfHeaders;
    DWORD Index;
    BYTE *ImageBase;
    DWORD SizeOfNtHeaders;
    ImageDosHeader=(PIMAGE_DOS_HEADER)FileBuffer;
    if (ImageDosHeader->e_magic!=IMAGE_DOS_SIGNATURE)
    {
        return FALSE;
    }
    ImageNtHeaders=(PIMAGE_NT_HEADERS)(FileBuffer+ImageDosHeader->e_lfanew);
    if (ImageNtHeaders->Signature!=IMAGE_NT_SIGNATURE)
    {
        return FALSE;
    }
    FileAlignment=ImageNtHeaders->OptionalHeader.FileAlignment;//0x200
    SectionAlignment=ImageNtHeaders->OptionalHeader.SectionAlignment;//0x1000
    NumberOfSections=ImageNtHeaders->FileHeader.NumberOfSections;//0x16
    SizeOfImage=ImageNtHeaders->OptionalHeader.SizeOfImage;//0x412000
    SizeOfHeaders=ImageNtHeaders->OptionalHeader.SizeOfHeaders;//0x800

    SizeOfImage=AlignSize(SizeOfImage,SectionAlignment);//0x412000

    ImageBase=ExAllocatePool(NonPagedPool,SizeOfImage);
    if (ImageBase==NULL)
    {
        return FALSE;
    }
    RtlZeroMemory(ImageBase,SizeOfImage);
    //0xf8
    SizeOfNtHeaders=sizeof(ImageNtHeaders->FileHeader) + sizeof(ImageNtHeaders->Signature)+ImageNtHeaders->FileHeader.SizeOfOptionalHeader;
    ImageSectionHeader=(PIMAGE_SECTION_HEADER)((DWORD)ImageNtHeaders+SizeOfNtHeaders);
    for (Index=0;Index<NumberOfSections;Index++)
    {
        ImageSectionHeader[Index].SizeOfRawData=AlignSize(ImageSectionHeader[Index].SizeOfRawData,FileAlignment);
        ImageSectionHeader[Index].Misc.VirtualSize=AlignSize(ImageSectionHeader[Index].Misc.VirtualSize,SectionAlignment);
    }
    if (ImageSectionHeader[NumberOfSections-1].VirtualAddress+ImageSectionHeader[NumberOfSections-1].SizeOfRawData>SizeOfImage)
    {//no in
        ImageSectionHeader[NumberOfSections-1].SizeOfRawData = SizeOfImage-ImageSectionHeader[NumberOfSections-1].VirtualAddress;
    }
    RtlCopyMemory(ImageBase,FileBuffer,SizeOfHeaders);

    for (Index=0;Index<NumberOfSections;Index++)
    {
        DWORD FileOffset=ImageSectionHeader[Index].PointerToRawData;
        DWORD Length=ImageSectionHeader[Index].SizeOfRawData;
        DWORD ImageOffset=ImageSectionHeader[Index].VirtualAddress;
        RtlCopyMemory(&ImageBase[ImageOffset],&FileBuffer[FileOffset],Length);
    }
    *ImageModuleBase=ImageBase;

    return TRUE;


}

ULONG AlignSize(ULONG nSize, ULONG nAlign)
{
    return ((nSize + nAlign - 1) / nAlign * nAlign);
}



/*
ͨ��DriverObject->DriverSection ����  �ں�ģ��  
*/
PVOID GetKernelModuleBase(PDRIVER_OBJECT DriverObject,char *KernelModuleName)
{
    PLDR_DATA_TABLE_ENTRY DriverSection,LdrEntry;
    ANSI_STRING AnsiKernelModuleName;
    UNICODE_STRING UniKernelModuleName;
    UNICODE_STRING ModuleName;
    WCHAR *Buffer;
    int Lentgh,Index;
    RtlInitAnsiString(&AnsiKernelModuleName,KernelModuleName);
    RtlAnsiStringToUnicodeString(&UniKernelModuleName,&AnsiKernelModuleName,TRUE);
    Buffer=ExAllocatePool(NonPagedPool,260*2);
    if (Buffer==NULL)
    {
        return NULL;
    }
    RtlZeroMemory(Buffer,206*2);
    DriverSection=DriverObject->DriverSection;
    LdrEntry=(PLDR_DATA_TABLE_ENTRY)DriverSection->InLoadOrderLinks.Flink;
    while (LdrEntry&&DriverSection!=LdrEntry)
    {
        //(DWORD)LdrEntry->DllBase>=*(DWORD*)MmSystemRangeStart&&
        if (LdrEntry->FullDllName.Length>0&&
            LdrEntry->FullDllName.Buffer!=NULL)
        {

            if (MmIsAddressValid(&LdrEntry->FullDllName.Buffer[LdrEntry->FullDllName.Length/2-1]))
            {
                Lentgh=LdrEntry->FullDllName.Length/2;
                for (Index=Lentgh-1;Index>0;Index--)
                {
                    if (LdrEntry->FullDllName.Buffer[Index]==0x005C)
                    {
                        break;
                    }
                }
                if (LdrEntry->FullDllName.Buffer[Index]==0x005C)
                {
                    RtlCopyMemory(Buffer,&(LdrEntry->FullDllName.Buffer[Index+1]),(Lentgh-Index-1)*2);
                    ModuleName.Buffer=Buffer;
                    ModuleName.Length=(Lentgh-Index-1)*2;
                    ModuleName.MaximumLength=260*2;
                }
                else
                {
                    RtlCopyMemory(Buffer,LdrEntry->FullDllName.Buffer,Lentgh*2);
                    ModuleName.Buffer=Buffer;
                    ModuleName.Length=Lentgh*2;
                    ModuleName.MaximumLength=260*2;

                }

                if (RtlEqualUnicodeString(&ModuleName,&UniKernelModuleName,TRUE))
                {
                    ExFreePool(Buffer);
                    return LdrEntry->DllBase;
                }

            }

        }    
        LdrEntry=(PLDR_DATA_TABLE_ENTRY)LdrEntry->InLoadOrderLinks.Flink;
    }
    ExFreePool(Buffer);
    return NULL;
}


/*
ͨ���������ú�����ַ
*/
PVOID
    MiFindExportedRoutine (
    IN PVOID DllBase,
    BOOLEAN ByName,
    IN char *RoutineName,
    DWORD Ordinal
    )
{
    USHORT OrdinalNumber;
    PULONG NameTableBase;
    PUSHORT NameOrdinalTableBase;
    PULONG AddressTableBase;
    PULONG Addr;
    LONG High;
    LONG Low;
    LONG Middle;
    LONG Result;
    ULONG ExportSize;
    PVOID FunctionAddress;
    PIMAGE_EXPORT_DIRECTORY ExportDirectory;

    PAGED_CODE();

    //��õ�����
    ExportDirectory = (PIMAGE_EXPORT_DIRECTORY) RtlImageDirectoryEntryToData (
        DllBase,
        TRUE,
        IMAGE_DIRECTORY_ENTRY_EXPORT,
        &ExportSize);

    if (ExportDirectory == NULL) {
        return NULL;
    }

    NameTableBase = (PULONG)((PCHAR)DllBase + (ULONG)ExportDirectory->AddressOfNames);
    NameOrdinalTableBase = (PUSHORT)((PCHAR)DllBase + (ULONG)ExportDirectory->AddressOfNameOrdinals);
    AddressTableBase=(PULONG)((PCHAR)DllBase + (ULONG)ExportDirectory->AddressOfFunctions);

    if (!ByName)
    {
        return (PVOID)AddressTableBase[Ordinal];
    }


    Low = 0;
    Middle = 0;
    High = ExportDirectory->NumberOfNames - 1;

    while (High >= Low) {
        Middle = (Low + High) >> 1;

        Result = strcmp (RoutineName,
            (PCHAR)DllBase + NameTableBase[Middle]);

        if (Result < 0) {
            High = Middle - 1;
        }
        else if (Result > 0) {
            Low = Middle + 1;
        }
        else {
            break;
        }
    }

    if (High < Low) {
        return NULL;
    }

    OrdinalNumber = NameOrdinalTableBase[Middle];
    if ((ULONG)OrdinalNumber >= ExportDirectory->NumberOfFunctions) {
        return NULL;
    }

    Addr = (PULONG)((PCHAR)DllBase + (ULONG)ExportDirectory->AddressOfFunctions);

    FunctionAddress = (PVOID)((PCHAR)DllBase + Addr[OrdinalNumber]);

    //
    // Forwarders are not used by the kernel and HAL to each other.
    //

    ASSERT ((FunctionAddress <= (PVOID)ExportDirectory) ||
        (FunctionAddress >= (PVOID)((PCHAR)ExportDirectory + ExportSize)));

    return FunctionAddress;
}




BOOLEAN InsertOriginalFirstThunk(DWORD ImageBase,DWORD ExistImageBase,PIMAGE_THUNK_DATA FirstThunk)
{
    DWORD Offset;
    PIMAGE_THUNK_DATA OriginalFirstThunk;
    Offset=(DWORD)FirstThunk-ImageBase;
    OriginalFirstThunk=(PIMAGE_THUNK_DATA)(ExistImageBase+Offset);
    while (OriginalFirstThunk->u1.Function)
    {
        FirstThunk->u1.Function=OriginalFirstThunk->u1.Function;
        OriginalFirstThunk++;
        FirstThunk++;
    }
    return TRUE;

}






//�޸������
BOOLEAN FixImportTable(BYTE *ImageBase,DWORD ExistImageBase,PDRIVER_OBJECT DriverObject)
{
    PIMAGE_IMPORT_DESCRIPTOR ImageImportDescriptor=NULL;
    PIMAGE_THUNK_DATA ImageThunkData,FirstThunk;
    PIMAGE_IMPORT_BY_NAME ImortByName;
    DWORD ImportSize;
    PVOID ModuleBase;
    char ModuleName[260];
    DWORD FunctionAddress;
    //�õ�������ַ
    ImageImportDescriptor=(PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(ImageBase,TRUE,IMAGE_DIRECTORY_ENTRY_IMPORT,&ImportSize);
    if (ImageImportDescriptor==NULL)
    {
        return FALSE;
    }
    while (ImageImportDescriptor->OriginalFirstThunk&&ImageImportDescriptor->Name)
    {
        strcpy(ModuleName,(char*)(ImageBase+ImageImportDescriptor->Name));  //������Ϣ����

        //ntoskrnl.exe(NTKRNLPA.exe��ntkrnlmp.exe��ntkrpamp.exe)��
        if (_stricmp(ModuleName,"ntkrnlpa.exe")==0||
            _stricmp(ModuleName,"ntoskrnl.exe")==0||
            _stricmp(ModuleName,"ntkrnlmp.exe")==0||
            _stricmp(ModuleName,"ntkrpamp.exe")==0)
        {//no in
            ModuleBase=GetKernelModuleBase(DriverObject,"ntkrnlpa.exe");  //ͨ��DriverObject->DriverSection �����ں�ģ��
            if (ModuleBase==NULL)
            {
                ModuleBase=GetKernelModuleBase(DriverObject,"ntoskrnl.exe");
                if (ModuleBase==NULL)
                {
                    ModuleBase=GetKernelModuleBase(DriverObject,"ntkrnlmp.exe");
                    if (ModuleBase==NULL)
                    {
                        ModuleBase=GetKernelModuleBase(DriverObject,"ntkrpamp.exe");

                    }

                }
            }

        }
        else
        {
            ModuleBase=GetKernelModuleBase(DriverObject,ModuleName);

        }
        if (ModuleBase==NULL)
        {
            FirstThunk=(PIMAGE_THUNK_DATA)(ImageBase+ImageImportDescriptor->FirstThunk);
            InsertOriginalFirstThunk((DWORD)ImageBase,ExistImageBase,FirstThunk);
            ImageImportDescriptor++;
            continue;
        }
        //PSHED.dll
        ImageThunkData=(PIMAGE_THUNK_DATA)(ImageBase+ImageImportDescriptor->OriginalFirstThunk);
        FirstThunk=(PIMAGE_THUNK_DATA)(ImageBase+ImageImportDescriptor->FirstThunk);
        while(ImageThunkData->u1.Ordinal)
        {
            //��ŵ���
            if(IMAGE_SNAP_BY_ORDINAL32(ImageThunkData->u1.Ordinal))
            {
                //ͨ��ϵͳ�ں˵ĵ�����   ����- ��� ������ַ
                FunctionAddress=(DWORD)MiFindExportedRoutine(ModuleBase,FALSE,NULL,ImageThunkData->u1.Ordinal & ~IMAGE_ORDINAL_FLAG32);
                if (FunctionAddress==0)
                {
                    return FALSE;
                }
                FirstThunk->u1.Function=FunctionAddress;
            }
            //����������
            else
            {
                //
                ImortByName=(PIMAGE_IMPORT_BY_NAME)(ImageBase+ImageThunkData->u1.AddressOfData);
                FunctionAddress=(DWORD)MiFindExportedRoutine(ModuleBase,TRUE,ImortByName->Name,0);
                if (FunctionAddress==0)
                {
                    return FALSE;
                }
                FirstThunk->u1.Function=FunctionAddress;
            }
            FirstThunk++;
            ImageThunkData++;
        }
        ImageImportDescriptor++;
    }
    return TRUE;
}


/*
system32//NtosKrnl.exe .. 
*/
BOOLEAN PeLoad(
    WCHAR *FileFullPath,
    BYTE **ImageModeleBase,
    PDRIVER_OBJECT DeviceObject,
    DWORD ExistImageBase
    )
{
    NTSTATUS Status;
    HANDLE hFile;
    LARGE_INTEGER FileSize;
    DWORD Length;
    BYTE *FileBuffer;
    BYTE *ImageBase;
    IO_STATUS_BLOCK IoStatus;
    //\SystemRoot\system32\ntkrnlpa.exe
    Status=KernelOpenFile(FileFullPath,&hFile,0x100020,0x80,1,1,0x20);  //�Լ������ļ����󣬹���FileObject->IrpList  �������ļ����
    if (!NT_SUCCESS(Status))
    {
        return FALSE;
    }

    Status=KernelGetFileSize(hFile,&FileSize);  //��ȡirp��Ϣ������filesize
    if (!NT_SUCCESS(Status))
    {
        ZwClose(hFile);
        return FALSE;
    }
    Length=FileSize.LowPart;
    FileBuffer=ExAllocatePool(PagedPool,Length);
    if (FileBuffer==NULL)
    {
        ZwClose(hFile);
        return FALSE;
    }

    Status=KernelReadFile(hFile,NULL,Length,FileBuffer,&IoStatus); //�����ļ�������ļ���С ͨ��irp���󣬶�ȡ�ļ����ڴ���
    if (!NT_SUCCESS(Status))
    {
        ZwClose(hFile);
        ExFreePool(FileBuffer);
        return FALSE;
    }
    ZwClose(hFile);


    if(!ImageFile(FileBuffer,&ImageBase))   //�޸�FileBuffer�е�ƫ��  ����VirtualAglin  ����    �õ�ȫ��ImageModuleBase
    {
        ExFreePool(FileBuffer);
        return FALSE;
    }
    ExFreePool(FileBuffer);

    //2k3��MiFindExportedRoutine����ʧ��
    if(!FixImportTable(ImageBase,ExistImageBase,DeviceObject)) //�޸������
    {
        ExFreePool(ImageBase);
        return FALSE;
    }
    if(!FixBaseRelocTable(ImageBase,ExistImageBase))  //�޸��ض�λ��
    {
        ExFreePool(ImageBase);
        return FALSE;
    }

    *ImageModeleBase=ImageBase; //�õ����Ļ���ַ   ���� �� ԭ���ڴ��и�ʽһ���� һ��ntos

    return TRUE;
}

