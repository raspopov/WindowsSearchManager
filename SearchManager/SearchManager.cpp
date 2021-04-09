// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
This file is part of Search Manager - shows Windows Search internals.

Copyright (C) 2012-2021 Nikolay Raspopov <raspopov@cherubicsoft.com>

https://github.com/raspopov/WindowsSearchManager

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

BEGIN_MESSAGE_MAP(CSearchManagerApp, CWinAppEx)
	ON_COMMAND(ID_HELP, &CWinAppEx::OnHelp)
END_MESSAGE_MAP()

CSearchManagerApp::CSearchManagerApp()
{
	EnableHtmlHelp();

	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}

CSearchManagerApp theApp;

BOOL CSearchManagerApp::InitInstance()
{
	const INITCOMMONCONTROLSEX InitCtrls = { sizeof( INITCOMMONCONTROLSEX ), ICC_WIN95_CLASSES | ICC_USEREX_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS };
	InitCommonControlsEx( &InitCtrls );

	SetAppID( AfxGetAppName() );

	CWinAppEx::InitInstance();

	CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

	SetRegistryKey( AFX_IDS_COMPANY_NAME );

	EnableTaskbarInteraction();

	InitContextMenuManager();

	CMFCVisualManager::SetDefaultManager( RUNTIME_CLASS( CMFCVisualManagerWindows ) );

	{
		CSearchManagerDlg dlg;
		m_pMainWnd = &dlg;
		dlg.DoModal();
	}

	CoUninitialize();

	return FALSE;
}

BOOL CSearchManagerApp::ProcessMessageFilter(int code, LPMSG lpMsg)
{
	CSearchManagerDlg* pMainWnd = static_cast< CSearchManagerDlg* >( m_pMainWnd );
	if ( pMainWnd && ( lpMsg->hwnd == pMainWnd->m_hWnd || ::IsChild( pMainWnd->m_hWnd, lpMsg->hwnd ) ) )
	{
		if ( lpMsg->message == WM_KEYDOWN )
		{
			// Emulate key down message for dialog
			if ( pMainWnd->OnKeyDown( (UINT)lpMsg->wParam, ( lpMsg->lParam & 0xffff ), ( ( lpMsg->lParam >> 16 ) & 0xffff ) ) )
			{
				return TRUE;
			}
		}
	}

	return CWinAppEx::ProcessMessageFilter( code, lpMsg );
}
