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
#include "Item.h"
#include "UrlDlg.h"

// CBrowseCtrl

IMPLEMENT_DYNAMIC(CBrowseCtrl, CMFCEditBrowseCtrl)

void CBrowseCtrl::OnAfterUpdate()
{
	CString str;
	GetWindowText( str );
	if ( str.Find( FILE_PROTOCOL, 0 ) != 0 )
	{
		SetWindowText( FILE_PROTOCOL _T(":///") + str );
	}
}

BEGIN_MESSAGE_MAP(CBrowseCtrl, CMFCEditBrowseCtrl)
END_MESSAGE_MAP()

// CUrlDialog dialog

IMPLEMENT_DYNAMIC(CUrlDialog, CDialogEx)

CUrlDialog::CUrlDialog(const CString& sTitle, CWnd* pParent /*=nullptr*/)
	: CDialogEx	( CUrlDialog::IDD, pParent)
	, m_sTitle	( sTitle )
{
}

bool CUrlDialog::IsEmpty() const noexcept
{
	return ( m_sURL.IsEmpty() || m_sURL.CompareNoCase( FILE_PROTOCOL _T(":///") ) == 0 );
}

void CUrlDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_BROWSER, m_sURL);
	DDX_Control(pDX, IDC_BROWSER, m_wndUrl);
}

BEGIN_MESSAGE_MAP(CUrlDialog, CDialogEx)
	ON_WM_TIMER()
END_MESSAGE_MAP()

// CUrlDialog message handlers

BOOL CUrlDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText( m_sTitle );

	if ( IsEmpty() )
	{
		// Open Browser dialog
		SetTimer( 1, 100, nullptr );
	}

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CUrlDialog::OnTimer(UINT_PTR nIDEvent)
{
	if ( nIDEvent == 1 )
	{
		KillTimer( 1 );

		// Open Browser dialog
		m_wndUrl.PostMessage( WM_SYSKEYDOWN, VK_DOWN );
	}

	CDialogEx::OnTimer(nIDEvent);
}
