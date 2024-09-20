#include "precomp.h"

#include "MemLoadDriver.h"

//���ļ����ص��ڴ棬psBufferLength�����ļ����ȣ���������ֵΪ�ļ����ص�ַ
PVOID LoadFileToMemory(PUNICODE_STRING pUstrDllPath, PSIZE_T psBufferLength)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	OBJECT_ATTRIBUTES objectAttributes = { 0 };
	IO_STATUS_BLOCK ioStatusBlock = { 0 };
	HANDLE hFile = NULL;
	PVOID pBuffer = NULL;
	FILE_STANDARD_INFORMATION fsi = { 0 };
	KdPrint(("-->%s %d\n", __FUNCTION__, __LINE__));

	//����У��
	if (pUstrDllPath == NULL || psBufferLength == NULL)
	{
		KdPrint(("%s %d: Parameter error\n", __FUNCTION__, __LINE__));
		goto End;
	}

	//���ļ�
	InitializeObjectAttributes(&objectAttributes, pUstrDllPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, 0);
	ntStatus = ZwCreateFile(
		&hFile,
		GENERIC_READ,
		&objectAttributes,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0
	);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("%s %d: ZwCreateFile failed 0x%x\n", __FUNCTION__, __LINE__, ntStatus));
		goto End;
	}

	//��ȡ�ļ���С*psBufferLength
	ntStatus = ZwQueryInformationFile(
		hFile,
		&ioStatusBlock,
		&fsi,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation
	);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("%s %d: ZwQueryInformationFile failed 0x%x\n", __FUNCTION__, __LINE__, ntStatus));
		goto End;
	}
	*psBufferLength = (SIZE_T)fsi.EndOfFile.QuadPart;

	//�����ڴ�
	pBuffer = ExAllocatePool(PagedPool, *psBufferLength);
	if (pBuffer == NULL)
	{
		KdPrint(("%s %d: ExAllocatePool failed\n", __FUNCTION__, __LINE__));
		goto End;
	}

	//���ļ������ڴ�
	ntStatus = ZwReadFile(
		hFile,
		NULL,
		NULL,
		NULL,
		&ioStatusBlock,
		pBuffer,
		(ULONG)*psBufferLength,
		NULL,
		NULL
	);
	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint(("%s %d: ZwReadFile failed 0x%x\n", __FUNCTION__, __LINE__, ntStatus));
		ExFreePool(pBuffer);
		goto End;
	}

End:
	//�ر��ļ����
	if (hFile != NULL)
	{
		ZwClose(hFile);
	}

	KdPrint(("<--%s %d\n", __FUNCTION__, __LINE__));
	return pBuffer;
}

//�ض�λ
NTSTATUS DoRelocation(PVOID pImageBuffer)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PIMAGE_NT_HEADERS pImageNtHeaders = NULL;
	PIMAGE_BASE_RELOCATION pImageBaseRelocation = NULL;
	ULONG_PTR llDelta = 0;
	ULONG ulTemp = 0;
	ULONG ulSize = 0;
	PUSHORT chains = NULL;
	KdPrint(("-->%s %d\n", __FUNCTION__, __LINE__));

	pImageNtHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)pImageBuffer + ((PIMAGE_DOS_HEADER)pImageBuffer)->e_lfanew);
	if (pImageNtHeaders == NULL)
	{
		KdPrint(("%s %d: RtlImageNtHeader failed\n", __FUNCTION__, __LINE__));
		ntStatus = STATUS_UNSUCCESSFUL;
		goto End;
	}
	llDelta = (ULONG_PTR)pImageBuffer - pImageNtHeaders->OptionalHeader.ImageBase;

	pImageBaseRelocation = (PIMAGE_BASE_RELOCATION)(pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + (ULONG_PTR)pImageBuffer);
	ulSize = pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
	for (ulTemp = 0; ulTemp < ulSize; ulTemp += pImageBaseRelocation->SizeOfBlock, pImageBaseRelocation = (PIMAGE_BASE_RELOCATION)((ULONG_PTR)pImageBaseRelocation + ulTemp))
	{
		for (chains = (PUSHORT)((ULONG_PTR)pImageBaseRelocation + sizeof(IMAGE_BASE_RELOCATION)); chains < (PUSHORT)((ULONG_PTR)pImageBaseRelocation + pImageBaseRelocation->SizeOfBlock); ++chains)
		{
			switch (*chains >> 12)
			{
			case IMAGE_REL_BASED_ABSOLUTE:
			{
				break;
			}
			case IMAGE_REL_BASED_HIGHLOW:
			{
				*(PULONG)CONVERT_RVA(pImageBuffer, pImageBaseRelocation->VirtualAddress + (*chains & 0x0fff)) += (ULONG)llDelta;
				break;
			}
			case IMAGE_REL_BASED_DIR64:
			{
				*(PULONG_PTR)CONVERT_RVA(pImageBuffer, pImageBaseRelocation->VirtualAddress + (*chains & 0x0fff)) += llDelta;
				break;
			}
			default:
			{
				ntStatus = STATUS_NOT_IMPLEMENTED;
				goto End;
			}
			}
		}
	}

	ntStatus = STATUS_SUCCESS;

End:
	KdPrint(("<--%s %d\n", __FUNCTION__, __LINE__));
	return ntStatus;
}

//�Ƚ��ַ���
BOOLEAN xstricmp(LPCSTR s1, LPCSTR s2)
{
	ULONG i = 0;

	for (i = 0; 0 == ((s1[i] ^ s2[i]) & 0xDF); ++i)
	{
		if (0 == s1[i])
		{
			return TRUE;
		}
	}

	return FALSE;
}

//��ȡģ���ַ
PVOID GetModuleByName(LPCSTR driverName)
{
	ULONG size = 0;
	PVOID ImageBase = NULL;
	PRTL_MODULE_EXTENDED_INFO pDrivers = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	ULONG i = 0;

	status = fun_RtlQueryModuleInformation(&size, sizeof(RTL_MODULE_EXTENDED_INFO), NULL);
	if NT_SUCCESS(status)
	{
		pDrivers = (PRTL_MODULE_EXTENDED_INFO)ExAllocatePool(PagedPool, size);
		if (pDrivers)
		{
			status = fun_RtlQueryModuleInformation(&size, sizeof(RTL_MODULE_EXTENDED_INFO), pDrivers);
			if(NT_SUCCESS(status))
			{
				for (i = 0; i < size / sizeof(RTL_MODULE_EXTENDED_INFO); ++i)
				{
					if (xstricmp(driverName, &pDrivers[i].FullPathName[pDrivers[i].FileNameOffset]))
					{
						ImageBase = pDrivers[i].ImageBase;
						break;
					}
				}
			}
			ExFreePool(pDrivers);
		}
	}

	return ImageBase;
}

//��ȡ������ַ
PVOID GetRoutineByName(PVOID pImageBuffer, PCHAR cFunctionName)
{
	ULONG dirSize = 0;
	PIMAGE_EXPORT_DIRECTORY pExportDir = NULL;
	PULONG names = NULL;
	PUSHORT ordinals = NULL;
	PULONG functions = NULL;
	ULONG i = 0;
	LPCSTR name = NULL;

	pExportDir = (PIMAGE_EXPORT_DIRECTORY)fun_RtlImageDirectoryEntryToData(pImageBuffer, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &dirSize);
	names = (PULONG)CONVERT_RVA(pImageBuffer, pExportDir->AddressOfNames);
	ordinals = (PUSHORT)CONVERT_RVA(pImageBuffer, pExportDir->AddressOfNameOrdinals);
	functions = (PULONG)CONVERT_RVA(pImageBuffer, pExportDir->AddressOfFunctions);

	for (i = 0; i < pExportDir->NumberOfNames; ++i)
	{
		name = (LPCSTR)CONVERT_RVA(pImageBuffer, names[i]);
		if (0 == strcmp(cFunctionName, name))
		{
			return CONVERT_RVA(pImageBuffer, functions[ordinals[i]]);
		}
	}

	return NULL;
}

//�޸ĵ����
NTSTATUS FindImports(PVOID pImageBuffer)
{
	PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor = NULL;
	PIMAGE_NT_HEADERS pImageNtHeaders = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	ULONG ulTemp = 0;
	ULONG ulSize = 0;
	LPSTR libName = NULL;
	PVOID hModule = NULL;
	PIMAGE_THUNK_DATA pNames = NULL;
	PIMAGE_THUNK_DATA pFuncP = NULL;
	PIMAGE_IMPORT_BY_NAME pIName = NULL;
	PVOID func = NULL;
	KdPrint(("-->%s %d\n", __FUNCTION__, __LINE__));

	//�õ�������׵�ַpImportDescriptor����СulSize
	pImageNtHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)pImageBuffer + ((PIMAGE_DOS_HEADER)pImageBuffer)->e_lfanew);
	if (pImageNtHeaders == NULL)
	{
		KdPrint(("%s %d: RtlImageNtHeader failed\n", __FUNCTION__, __LINE__));
		ntStatus = STATUS_UNSUCCESSFUL;
		goto End;
	}
	pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + (ULONG_PTR)pImageBuffer);
	ulSize = pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

	for (; pImportDescriptor->Name; pImportDescriptor++)
	{
		libName = (PCHAR)((ULONG_PTR)pImageBuffer + pImportDescriptor->Name);
		hModule = GetModuleByName(libName);
		if (hModule)
		{
			pNames = (PIMAGE_THUNK_DATA)CONVERT_RVA(pImageBuffer, pImportDescriptor->OriginalFirstThunk);
			pFuncP = (PIMAGE_THUNK_DATA)CONVERT_RVA(pImageBuffer, pImportDescriptor->FirstThunk);

			for (; pNames->u1.ForwarderString; ++pNames, ++pFuncP)
			{
				pIName = (PIMAGE_IMPORT_BY_NAME)CONVERT_RVA(pImageBuffer, pNames->u1.AddressOfData);
				func = GetRoutineByName(hModule, pIName->Name);
				if (func)
				{
					pFuncP->u1.Function = (ULONG_PTR)func;
				}
				else
				{
					ntStatus = STATUS_PROCEDURE_NOT_FOUND;
					goto End;
				}
			}
		}
		else
		{
			ntStatus = STATUS_DRIVER_UNABLE_TO_LOAD;
			goto End;
		}
	}

	ntStatus = STATUS_SUCCESS;

End:
	KdPrint(("<--%s %d\n", __FUNCTION__, __LINE__));
	return ntStatus;
}

//������ʼ��ַΪpFileBuffer��СΪsBufferLength����ΪwDriverName�������ļ�����������ֵΪ����ģ�����ַ
PVOID MemLoadDriverByFileBuffer(PVOID pFileBuffer, SIZE_T sBufferLength, PWCHAR wDriverName)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PIMAGE_NT_HEADERS pImageNtHeaders = NULL;
	PIMAGE_SECTION_HEADER pImageSectionHeader = NULL;
	ULONG ulTemp = 0;
	PVOID pImageBuffer = NULL;
	PDRIVER_INITIALIZE pDriverInitialize = NULL;
	UNICODE_STRING ustrDriverName = { 0 };
	KdPrint(("-->%s %d\n", __FUNCTION__, __LINE__));

	//����У��
	if (pFileBuffer == NULL || sBufferLength == 0 || wDriverName == NULL)
	{
		KdPrint(("%s %d: Parameter error\n", __FUNCTION__, __LINE__));
		goto End;
	}
	
	{
		//����ģ��ӳ���ڴ�pImageBuffer
		pImageNtHeaders = (PIMAGE_NT_HEADERS)((ULONG_PTR)pFileBuffer + ((PIMAGE_DOS_HEADER)pFileBuffer)->e_lfanew);
		if (pImageNtHeaders == NULL)
		{
			KdPrint(("%s %d: RtlImageNtHeader failed\n", __FUNCTION__, __LINE__));
			ntStatus = STATUS_UNSUCCESSFUL;
			goto End;
		}
		pImageBuffer = ExAllocatePool(NonPagedPool, pImageNtHeaders->OptionalHeader.SizeOfImage);
		if (pImageBuffer == NULL)
		{
			KdPrint(("%s %d: ExAllocatePool failed\n", __FUNCTION__, __LINE__));
			goto End;
		}

		//����NTͷ
		RtlCopyMemory(pImageBuffer, pFileBuffer, pImageNtHeaders->OptionalHeader.SizeOfHeaders);

		//��������Section
		pImageSectionHeader = (PIMAGE_SECTION_HEADER)(((PIMAGE_DOS_HEADER)pFileBuffer)->e_lfanew + sizeof(IMAGE_NT_HEADERS) + (ULONG_PTR)pFileBuffer);
		for (ulTemp = 0; ulTemp < pImageNtHeaders->FileHeader.NumberOfSections; ++ulTemp)
		{
			RtlCopyMemory((PCHAR)pImageBuffer + pImageSectionHeader[ulTemp].VirtualAddress, (PCHAR)pFileBuffer + pImageSectionHeader[ulTemp].PointerToRawData, pImageSectionHeader[ulTemp].SizeOfRawData);
		}

		//�����ض�λ
		ntStatus = DoRelocation(pImageBuffer);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("%s %d: DoRelocation failed\n", __FUNCTION__, __LINE__));
			ExFreePool(pImageBuffer);
			pImageBuffer = NULL;
			goto End;
		}

		//�޸������
		ntStatus = FindImports(pImageBuffer);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("%s %d: FindImports failed\n", __FUNCTION__, __LINE__));
			ExFreePool(pImageBuffer);
			pImageBuffer = NULL;
			goto End;
		}

		//������������
		ustrDriverName.Length = wcslen(wDriverName) * sizeof(WCHAR);
		ustrDriverName.MaximumLength = (wcslen(wDriverName) + 1) * sizeof(WCHAR);
		ustrDriverName.Buffer = wDriverName;
		pDriverInitialize = (PDRIVER_INITIALIZE)CONVERT_RVA(pImageBuffer, pImageNtHeaders->OptionalHeader.AddressOfEntryPoint);
		ntStatus = fun_IoCreateDriver(&ustrDriverName, pDriverInitialize);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("%s %d: IoCreateDriver failed\n", __FUNCTION__, __LINE__));
			ExFreePool(pImageBuffer);
			pImageBuffer = NULL;
			goto End;
		}
	}

End:
	KdPrint(("<--%s %d\n", __FUNCTION__, __LINE__));
	return pImageBuffer;
}

//�����ļ�·��ΪwDriverPath����ΪwDriverName���������򣬺�������ֵΪ����ģ�����ַ
PVOID MemLoadDriverByFilePath(PWCHAR wDriverPath, PWCHAR wDriverName)
{
	UNICODE_STRING ustrDriverPath = { 0 };
	SIZE_T sBufferLength = 0;
	PVOID pFileBuffer = NULL;
	PVOID pImageBuffer = NULL;
	KdPrint(("-->%s %d\n", __FUNCTION__, __LINE__));

	//����У��
	if (wDriverPath == NULL || wDriverName == NULL)
	{
		KdPrint(("%s %d: Parameter error!\n", __FUNCTION__, __LINE__));
		goto End;
	}

	//�ڴ���������ļ�
	ustrDriverPath.Length = wcslen(wDriverPath) * sizeof(WCHAR);
	ustrDriverPath.MaximumLength = (wcslen(wDriverPath) + 1) * sizeof(WCHAR);
	ustrDriverPath.Buffer = wDriverPath;
	pFileBuffer = LoadFileToMemory(&ustrDriverPath, &sBufferLength);
	if (pFileBuffer == NULL)
	{
		KdPrint(("%s %d: LoadFileToMemory failed!\n", __FUNCTION__, __LINE__));
		goto End;
	}
	KdPrint(("FileBuffer: 0x%p Length: %d bytes\n", pFileBuffer, sBufferLength));

	//��������
	pImageBuffer = MemLoadDriverByFileBuffer(pFileBuffer, sBufferLength, wDriverName);
	if (pImageBuffer == NULL)
	{
		KdPrint(("%s %d: MemLoadDriver failed!\n", __FUNCTION__, __LINE__));
		goto End;
	}
	KdPrint(("ImageBuffer: 0x%p\n", pImageBuffer));

End:
	if (pFileBuffer)
	{
		ExFreePool(pFileBuffer);
	}

	KdPrint(("<--%s %d\n", __FUNCTION__, __LINE__));
	return pImageBuffer;
}
