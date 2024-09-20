
// EnumSSSDTManagerDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "EnumSSSDTManager.h"
#include "EnumSSSDTManagerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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


// CEnumSSSDTManagerDlg �Ի���




CEnumSSSDTManagerDlg::CEnumSSSDTManagerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CEnumSSSDTManagerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	for (int i=0;i<5;i++)
	{
		m_Dlg[i] = NULL;
	}
}

void CEnumSSSDTManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_SSSDT, m_Tab);
}

BEGIN_MESSAGE_MAP(CEnumSSSDTManagerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_SSSDT, &CEnumSSSDTManagerDlg::OnSelchangeTabSssdt)
END_MESSAGE_MAP()


// CEnumSSSDTManagerDlg ��Ϣ�������

BOOL CEnumSSSDTManagerDlg::OnInitDialog()
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

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
	InitTab();



	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}


VOID  CEnumSSSDTManagerDlg::InitTab()
{
	m_Tab.InsertItem(0,_T("SSSDT"));
	m_Tab.InsertItem(1,_T("SSDT"));


//	SSSDTDlg.Create(IDD_DIALOG_SSSDT,&m_Tab);
//	SSDTDlg.Create(IDD_DIALOG_SSDT,&m_Tab);

	SSSDTDlg.Create(IDD_DIALOG_SSSDT,GetDlgItem(IDC_TAB_SSSDT));
	SSDTDlg.Create(IDD_DIALOG_SSDT,GetDlgItem(IDC_TAB_SSSDT));

	m_Dlg[0] = &SSSDTDlg;
	m_Dlg[1] = &SSDTDlg;



	CRect rc;
	m_Tab.GetClientRect(rc);
	rc.top +=20;
	rc.bottom -= 4;
	rc.left += 4;
	rc.right -= 4;
	SSSDTDlg.MoveWindow(rc);
	SSDTDlg.MoveWindow(rc);

	m_Tab.SetCurSel(0);
	SSSDTDlg.ShowWindow(TRUE);
}


void CEnumSSSDTManagerDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CEnumSSSDTManagerDlg::OnPaint()
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
HCURSOR CEnumSSSDTManagerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}





void CEnumSSSDTManagerDlg::OnSelchangeTabSssdt(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������

	for(int i=0;i<2;i++)
	{
		if (m_Dlg[i]!=NULL)
		{
			m_Dlg[i]->ShowWindow(SW_HIDE);
		}

	}
	m_CurSelTab = m_Tab.GetCurSel();


	if (m_Dlg[m_CurSelTab]!=NULL)
	{
		m_Dlg[m_CurSelTab]->ShowWindow(SW_SHOW);
	}
	*pResult = 0;
}
