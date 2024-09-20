
// CheckKernelHookDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "resource.h"
#include <WinIoCtl.h>


typedef struct _INLINEHOOKINFO_INFORMATION {          //INLINEHOOKINFO_INFORMATION
	ULONG ulHookType;
	ULONG ulMemoryFunctionBase;    //ԭʼ��ַ
	ULONG ulMemoryHookBase;        //HOOK ��ַ
	CHAR lpszFunction[256];
	CHAR lpszHookModuleImage[256];
	ULONG ulHookModuleBase;
	ULONG ulHookModuleSize;

} INLINEHOOKINFO_INFORMATION, *PINLINEHOOKINFO_INFORMATION;

typedef struct _INLINEHOOKINFO {          //InlineHook
	ULONG ulCount;
	INLINEHOOKINFO_INFORMATION InlineHook[1];
} INLINEHOOKINFO, *PINLINEHOOKINFO;




#define CTL_CHECKKERNELMODULE \
	CTL_CODE(FILE_DEVICE_UNKNOWN,0x830,METHOD_NEITHER,FILE_ANY_ACCESS)

// CCheckKernelHookDlg �Ի���
class CCheckKernelHookDlg : public CDialogEx
{
// ����
public:
	CCheckKernelHookDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_CHECKKERNELHOOK_DIALOG };

	VOID CheckKernelHook();
	VOID InsertDataToList(PINLINEHOOKINFO PInlineHookInfo);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

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


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_List;
};
