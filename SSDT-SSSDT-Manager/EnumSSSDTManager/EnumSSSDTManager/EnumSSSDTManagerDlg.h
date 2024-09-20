
// EnumSSSDTManagerDlg.h : ͷ�ļ�
//

#pragma once
#include "SSDT.h"
#include "SSSDT.h"

// CEnumSSSDTManagerDlg �Ի���
class CEnumSSSDTManagerDlg : public CDialogEx
{
// ����
public:
	CEnumSSSDTManagerDlg(CWnd* pParent = NULL);	// ��׼���캯��

	CSSSDT SSSDTDlg;
	CSSDT  SSDTDlg;
	ULONG       m_CurSelTab;
	CDialog*    m_Dlg[5];
	VOID  CEnumSSSDTManagerDlg::InitTab();

// �Ի�������
	enum { IDD = IDD_ENUMSSSDTMANAGER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


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
	CTabCtrl m_Tab;
	afx_msg void OnSelchangeTabSssdt(NMHDR *pNMHDR, LRESULT *pResult);
};
