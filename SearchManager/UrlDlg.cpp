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

#define ID_INSERT_PROTOCOL	100
#define ID_INSERT_USER		200

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

CUrlDialog::CUrlDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx	( CUrlDialog::IDD, pParent)
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
	DDX_Text(pDX, IDC_INFO, m_sInfo);
	DDX_Control(pDX, IDC_INSERT_URL, m_btnInsert);
}

BEGIN_MESSAGE_MAP(CUrlDialog, CDialogEx)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_INSERT_URL, &CUrlDialog::OnBnClickedInsertUrl)
END_MESSAGE_MAP()

// CUrlDialog message handlers

BOOL CUrlDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText( m_sTitle );

	if ( auto menu = theApp.GetContextMenuManager() )
	{
		MENUITEMINFO mi = { sizeof( MENUITEMINFO ), MIIM_SUBMENU };

		menu->AddMenu( _T("INSERT"), IDR_INSERT_MENU );

		m_btnInsert.m_hMenu = GetSubMenu( menu->GetMenuById( IDR_INSERT_MENU ), 0 );

		// Load protocols
		mi.hSubMenu = CreatePopupMenu();
		if ( SetMenuItemInfo( m_btnInsert.m_hMenu, 0, TRUE, &mi ) )
		{
			std::set < CString > items;
			items.insert( DEFAULT_PROTOCOL _T("://") );
			items.insert( FILE_PROTOCOL _T(":///") );

			HKEY hProtocolsKey;
			LSTATUS res = RegOpenKeyFull( HKEY_LOCAL_MACHINE, KEY_PROTOCOLS, KEY_ENUMERATE_SUB_KEYS, &hProtocolsKey );
			if ( res == ERROR_SUCCESS )
			{
				TCHAR name[ MAX_PATH ] = {};
				for ( DWORD i = 0; ; ++i )
				{
					name[ 0 ] = 0;
					DWORD name_size = _countof( name );
					res = SHEnumKeyEx( hProtocolsKey, i, name, &name_size );
					if ( res != ERROR_SUCCESS )
					{
						break;
					}

					CString proto = name;
					proto.MakeLower();
					if ( proto.CompareNoCase( DEFAULT_PROTOCOL ) != 0 && proto.CompareNoCase( FILE_PROTOCOL ) != 0 )
					{
						items.insert( proto + _T("://") );
					}
				}
				RegCloseKey( hProtocolsKey );
			}
			else
			{
				TRACE( _T("RegOpenKeyFull error: %s\n"), GetError() );
			}

			int id = ID_INSERT_PROTOCOL;
			for ( const auto& item : items )
			{
				VERIFY( AppendMenu( mi.hSubMenu, MF_STRING, id++, item ) );
				m_Protocols.push_back( item );
			}
		}

		// Load users
		mi.hSubMenu = CreatePopupMenu();
		if ( SetMenuItemInfo( m_btnInsert.m_hMenu, 1, TRUE, &mi ) )
		{
			std::set < CString > items;

			CString user;
			DWORD user_size = MAX_PATH;
			GetUserNameEx( EXTENDED_NAME_FORMAT::NameSamCompatible, user.GetBuffer( MAX_PATH ), &user_size );
			user.ReleaseBuffer();
			items.insert( user );

			CString computer;
			DWORD computer_size = MAX_PATH;
			GetComputerName( computer.GetBuffer( MAX_PATH ), &computer_size );
			computer.ReleaseBuffer();

			int index = 0;
			NET_API_STATUS res;
			do
			{
				PNET_DISPLAY_USER buf = nullptr;
				DWORD read = 0;
				res = NetQueryDisplayInformation( nullptr, 1, index, 100, MAX_PREFERRED_LENGTH, &read, reinterpret_cast< PVOID* > ( &buf ) );
				if ( ( res == ERROR_SUCCESS || res == ERROR_MORE_DATA ) && buf )
				{
					for ( DWORD i = 0; i < read; ++i )
					{
						if ( ( buf[ i ].usri1_flags & UF_ACCOUNTDISABLE ) == 0 && ( buf[ i ].usri1_flags & UF_NORMAL_ACCOUNT ) != 0 )
						{
							items.insert( computer + _T('\\') + buf[ i ].usri1_name );
						}
						index = buf[ i ].usri1_next_index;
					}
				}

				if ( buf )
				{
					NetApiBufferFree( buf );
				}
			}
			while( res == ERROR_MORE_DATA );

			int id = ID_INSERT_USER;
			for ( const auto& item : items )
			{
				char buf[ MAX_PATH ] = {};
				PSID psid = static_cast< PSID >( buf );
				DWORD size_size = MAX_PATH;
				TCHAR domain[ MAX_PATH ] = {};
				DWORD domain_size = MAX_PATH;
				SID_NAME_USE use;
				if ( LookupAccountName( nullptr, item, psid, &size_size, domain, &domain_size, &use ) )
				{
					LPTSTR pstr_sid = nullptr;
					if ( ConvertSidToStringSid( psid, &pstr_sid ) )
					{
						VERIFY( AppendMenu( mi.hSubMenu, MF_STRING, id++, item ) );
						m_Users.push_back( CString( _T("{") ) + pstr_sid + _T("}") );

						LocalFree( pstr_sid );
					}
					else
					{
						TRACE( _T("ConvertSidToStringSid error: %s\n"), GetError() );
					}
				}
				else
				{
					TRACE( _T("LookupAccountName error: %s\n"), GetError() );
				}
			}
		}
	}

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

void ReplaceProtocol(CString& sURL, const CString& sProtocol)
{
	int begin = sURL.Find( _T("://") );
	if ( begin != -1 )
	{
		if ( sURL.GetAt( begin + 3 ) == _T('/') )
		{
			++ begin;
		}
		sURL = sProtocol + sURL.Mid( begin + 3 );
	}
	else
	{
		sURL.Insert( 0, sProtocol );
	}
}

void ReplaceSID(CString& sURL, const CString& sSID)
{
	int begin_sid = sURL.Find( _T("{S-") );
	if ( begin_sid == -1 )
	{
		begin_sid = sURL.Find( _T("{s-") );
	}
	if ( begin_sid != -1 )
	{
		const int end_sid = sURL.Find( _T("}"), begin_sid + 2 );
		if ( end_sid != -1 )
		{
			sURL = sURL.Left( begin_sid ) + sSID + sURL.Mid( end_sid + 1 );
		}
		else
		{
			sURL += sSID;
		}
	}
	else
	{
		int begin = sURL.Find( _T("://") );
		if ( begin != -1 )
		{
			sURL.Insert( begin + 3, sSID );
		}
		else
		{
			sURL += sSID;
		}
	}

	if ( sURL.GetAt( sURL.GetLength() - 1 ) != _T('/') )
	{
		sURL += _T('/');
	}
}

void CUrlDialog::OnBnClickedInsertUrl()
{
	UpdateData();

	if ( m_btnInsert.m_nMenuResult >= ID_INSERT_PROTOCOL && m_btnInsert.m_nMenuResult < ID_INSERT_PROTOCOL + static_cast< int >( m_Protocols.size() ) )
	{
		ReplaceProtocol( m_sURL, m_Protocols[ m_btnInsert.m_nMenuResult - ID_INSERT_PROTOCOL ] );
	}
	else if ( m_btnInsert.m_nMenuResult >= ID_INSERT_USER && m_btnInsert.m_nMenuResult < ID_INSERT_USER + static_cast< int >( m_Users.size() ) )
	{
		ReplaceSID( m_sURL, m_Users[ m_btnInsert.m_nMenuResult - ID_INSERT_USER ] );
	}

	UpdateData( FALSE );
}
