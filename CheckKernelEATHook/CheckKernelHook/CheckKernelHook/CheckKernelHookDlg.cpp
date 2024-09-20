
// CheckKernelHookDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "CheckKernelHook.h"
#include "CheckKernelHookDlg.h"
#include "afxdialogex.h"
#include "AddService.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


HANDLE g_hDevice = NULL;

typedef struct
{
	WCHAR*     szTitle;           //�б������
	int		  nWidth;            //�б�Ŀ��

}COLUMNSTRUCT;
COLUMNSTRUCT g_Column_Data_Online[] = 
{
	{L"ԭʼ��ַ",			    148	},
	{L"��������",			150	},
	{L"Hook��ַ",	160	},
	{L"ģ������",		300	},
	{L"ģ���ַ",			    80	},
	{L"ģ���С",		    81	},
	{L"����",			81	}
};

int g_Column_Count_Online = 7; //�б�ĸ���
int g_Column_Online_Width = 0; 


// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CCheckKernelHookDlg �Ի���




CCheckKernelHookDlg::CCheckKernelHookDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CCheckKernelHookDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCheckKernelHookDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST, m_List);
}

BEGIN_MESSAGE_MAP(CCheckKernelHookDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CCheckKernelHookDlg ��Ϣ�������

BOOL CCheckKernelHookDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	m_List.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	for (int i = 0; i < g_Column_Count_Online; i++)
	{
		m_List.InsertColumn(i, g_Column_Data_Online[i].szTitle,LVCFMT_CENTER,g_Column_Data_Online[i].nWidth);

		g_Column_Online_Width+=g_Column_Data_Online[i].nWidth;  
	}


	//LoadDrv(L"CheckKernelHook");

	g_hDevice = OpenDevice(L"\\\\.\\CheckKernelHookLinkName");
	if (g_hDevice==(HANDLE)-1)
	{
		MessageBox(L"���豸ʧ��");
		return TRUE;
	}

	


	CheckKernelHook();
	
	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

VOID CCheckKernelHookDlg::CheckKernelHook()
{
	ULONG_PTR ulCount = 0x1000;
	PINLINEHOOKINFO PInlineHookInfo = NULL;
	BOOL bRet = FALSE;
	DWORD ulReturnSize = 0;
	do 
	{
		ULONG_PTR ulSize = 0;
		if (PInlineHookInfo)
		{
			free(PInlineHookInfo);
			PInlineHookInfo = NULL;
		}
		ulSize = sizeof(INLINEHOOKINFO) + ulCount * sizeof(INLINEHOOKINFO_INFORMATION);
		PInlineHookInfo = (PINLINEHOOKINFO)malloc(ulSize);
		if (!PInlineHookInfo)
		{
			break;
		}
		memset(PInlineHookInfo,0,ulSize);
		bRet = DeviceIoControl(g_hDevice,CTL_CHECKKERNELMODULE,
			NULL,
			0,
			PInlineHookInfo,
			ulSize,
			&ulReturnSize,
			NULL);
		ulCount = PInlineHookInfo->ulCount + 1000;
	} while (bRet == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

	if(PInlineHookInfo->ulCount==0)
	{
		MessageBox(L"��ǰ�ں˰�ȫ",L"");
	}
	else
	{
		InsertDataToList(PInlineHookInfo);
	}
	if (PInlineHookInfo)
	{
		free(PInlineHookInfo);
		PInlineHookInfo = NULL;
	}


}

VOID CCheckKernelHookDlg::InsertDataToList(PINLINEHOOKINFO PInlineHookInfo)
{
	CString OrgAddress,CurAddress,ModuleBase,ModuleSize;
	for(int i=0;i<PInlineHookInfo->ulCount;i++)
	{
		OrgAddress.Format(L"0x%p",PInlineHookInfo->InlineHook[i].ulMemoryFunctionBase);
		CurAddress.Format(L"0x%p",PInlineHookInfo->InlineHook[i].ulMemoryHookBase);
		ModuleBase.Format(L"0x%p",PInlineHookInfo->InlineHook[i].ulHookModuleBase);
		ModuleSize.Format(L"%d",PInlineHookInfo->InlineHook[i].ulHookModuleSize);
		int n = m_List.InsertItem(m_List.GetItemCount(),OrgAddress,0);   //ע�������i ����Icon �������λ��
		CString szFunc=L"";
		CString ModuleName = L"";
		szFunc +=PInlineHookInfo->InlineHook[i].lpszFunction;
		ModuleName += PInlineHookInfo->InlineHook[i].lpszHookModuleImage;
		m_List.SetItemText(n,1,szFunc);
		m_List.SetItemText(n,2,CurAddress);
		m_List.SetItemText(n,3,ModuleName);
		m_List.SetItemText(n,4,ModuleBase);
		m_List.SetItemText(n,5,ModuleSize);
		CString Type= L"";
		if(PInlineHookInfo->InlineHook[i].ulHookType==1)
		{
			Type +=L"SSDT Hook";
		}
		else if(PInlineHookInfo->InlineHook[i].ulHookType==2)
		{
			Type +=L"Next Call Hook";
		}
		else if(PInlineHookInfo->InlineHook[i].ulHookType==0)
		{
			Type +=L"Inline Hook";
		}
		m_List.SetItemText(n,6,Type);
		
	}
	UpdateData(TRUE);
}
void CCheckKernelHookDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CCheckKernelHookDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CCheckKernelHookDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

