// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
This file is part of Search Manager - shows Windows Search internals.

Copyright (C) 2012-2020 Nikolay Raspopov <raspopov@cherubicsoft.com>

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
( at your option ) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see < http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "SearchManager.h"
#include "SearchManagerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSearchManagerApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CSearchManagerApp::CSearchManagerApp()
{
}

CSearchManagerApp theApp;

BOOL CSearchManagerApp::InitInstance()
{
	const INITCOMMONCONTROLSEX InitCtrls = { sizeof( INITCOMMONCONTROLSEX ), ICC_WIN95_CLASSES };
	InitCommonControlsEx( &InitCtrls );

	CWinApp::InitInstance();

	SetRegistryKey( _T("Raspopov") );

	HRESULT hr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
	if ( SUCCEEDED( hr ) )
	{
		CSearchManagerDlg dlg;
		m_pMainWnd = &dlg;
		dlg.DoModal();
	}
	CoUninitialize();

	return FALSE;
}
