
// CheckKernelHook.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CCheckKernelHookApp:
// �йش����ʵ�֣������ CheckKernelHook.cpp
//

class CCheckKernelHookApp : public CWinApp
{
public:
	CCheckKernelHookApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CCheckKernelHookApp theApp;