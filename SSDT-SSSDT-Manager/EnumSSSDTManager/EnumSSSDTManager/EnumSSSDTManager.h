
// EnumSSSDTManager.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CEnumSSSDTManagerApp:
// �йش����ʵ�֣������ EnumSSSDTManager.cpp
//

class CEnumSSSDTManagerApp : public CWinApp
{
public:
	CEnumSSSDTManagerApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CEnumSSSDTManagerApp theApp;