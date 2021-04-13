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
#include "UrlDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

void CSearchManagerDlg::AddRoot(const CString& sURL)
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	CUrlDialog dlg( this );
	dlg.m_sTitle = LoadString( IDS_ADD_ROOT );
	if ( ! sURL.IsEmpty() )
	{
		dlg.m_sURL = sURL;
	}
	dlg.m_sInfo = LoadString( IDS_ADD_ROOT_INFO );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() && ( sURL.IsEmpty() || dlg.m_sURL.CompareNoCase( sURL ) != 0 ) )
	{
		CWaitCursor wc;

		// Delete old root if specified
		HRESULT hr = S_OK;
		if ( ! sURL.IsEmpty() )
		{
			CRoot root( TRUE, sURL );
			hr = root.DeleteFrom( m_pScope );
		}

		// Add new root
		if ( SUCCEEDED( hr ) )
		{
			CRoot root( TRUE, dlg.m_sURL );
			hr = root.AddTo( m_pScope );
			if ( SUCCEEDED( hr ) )
			{
				const HRESULT hr_save = m_pScope->SaveAll();
				if ( hr == S_OK || FAILED( hr_save ) )
				{
					hr = hr_save;
				}
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::AddRule(BOOL bInclude, BOOL bDefault, const CString& sURL)
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	CUrlDialog dlg( this );
	dlg.m_sTitle.Format( LoadString( IDS_ADD_SCOPE ),
		( bInclude ? _T("Include") : _T("Exclude") ),
		( bDefault ? _T("Default") : _T("User") ) );
	if ( ! sURL.IsEmpty() )
	{
		dlg.m_sURL = sURL;
	}
	dlg.m_sInfo = LoadString( IDS_ADD_SCOPE_INFO );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() && ( sURL.IsEmpty() || dlg.m_sURL.CompareNoCase( sURL ) != 0 ) )
	{
		CWaitCursor wc;

		// Delete old scope rule if specified
		HRESULT hr = S_OK;
		if ( ! sURL.IsEmpty() )
		{
			CRule rule( bInclude, bDefault, sURL );
			hr = rule.DeleteFrom( m_pScope );
		}

		// Add new scope rule
		if ( SUCCEEDED( hr ) )
		{
			CRule rule( bInclude, bDefault, dlg.m_sURL );
			hr = rule.AddTo( m_pScope );
			if ( SUCCEEDED( hr ) )
			{
				const HRESULT hr_save = m_pScope->SaveAll();
				if ( hr == S_OK || FAILED( hr_save ) )
				{
					hr = hr_save;
				}
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Delete()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	// Enumerate items to delete
	CList< const CItem* > to_delete;
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			to_delete.AddTail( item );
		}
	}

	// Delete items
	if ( ! to_delete.IsEmpty() )
	{
		CString msg;
		msg.Format( IDS_DELETE_CONFIRM, to_delete.GetCount() );
		if ( AfxMessageBox( msg, MB_YESNO | MB_ICONQUESTION ) == IDYES )
		{
			SetStatus( IDS_DELETING );

			for ( POSITION pos = to_delete.GetHeadPosition(); pos; )
			{
				const POSITION prev = pos;
				auto item = to_delete.GetNext( pos );

				CWaitCursor wc;

				// Delete from WS
				HRESULT hr = item->DeleteFrom( m_pScope );
				if ( SUCCEEDED( hr ) )
				{
					const HRESULT hr_save = m_pScope->SaveAll();
					if ( hr == S_OK || FAILED( hr_save ) )
					{
						hr = hr_save;
					}
				}

				if ( hr == S_OK )
				{
					m_bRefresh = true;
				}
				else
				{
					const error_t result( hr );
					const int res = AfxMessageBox( result.msg + _T("\n\n") + result.error + _T("\n\n") + item->URL,
						MB_ABORTRETRYIGNORE | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONEXCLAMATION ) );
					if ( res == IDRETRY )
					{
						// Retry deletion
						pos = prev;
						continue;
					}
					else if ( res == IDIGNORE )
					{
						// Ignore error and continue deletion
						continue;
					}
					else
					{
						// Cancel deletion
						break;
					}
				}
			}
		}
	}
}

void CSearchManagerDlg::Reindex()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pCatalog )
	{
		return;
	}

	// Enumerate items to delete
	CList< const CItem* > to_delete;
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const POSITION prev = pos;
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			HRESULT hr = item->Reindex( m_pCatalog );
			if ( hr != S_OK )
			{
				const error_t result( hr );
				const int res = AfxMessageBox( result.msg + _T("\n\n") + result.error + _T("\n\n") + item->URL,
					MB_ABORTRETRYIGNORE | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONEXCLAMATION ) );
				if ( res == IDRETRY )
				{
					// Retry deletion
					pos = prev;
					continue;
				}
				else if ( res == IDIGNORE )
				{
					// Ignore error and continue deletion
					continue;
				}
				else
				{
					// Cancel deletion
					break;
				}
			}
		}
	}
}

void CSearchManagerDlg::ReindexAll()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pCatalog )
	{
		return;
	}

	if ( AfxMessageBox( IDS_REINDEX_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		const HRESULT hr = m_pCatalog->Reindex();

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Reset()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pCatalog )
	{
		return;
	}

	if ( AfxMessageBox( IDS_RESET_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		const HRESULT hr = m_pCatalog->Reset();

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Default()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	if ( AfxMessageBox( IDS_DEFAULT_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		HRESULT hr = m_pScope->RevertToDefaultScopes();
		if ( SUCCEEDED( hr ) )
		{
			const HRESULT hr_save = m_pScope->SaveAll();
			if ( hr == S_OK || FAILED( hr_save ) )
			{
				hr = hr_save;
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Explore()
{
	CWaitCursor wc;

	TCHAR folder[ MAX_PATH ] = {};
	DWORD type, folder_size = sizeof( folder );
	LSTATUS res = RegQueryValueFull( HKEY_LOCAL_MACHINE, KEY_SEARCH, _T("DataDirectory"), &type, reinterpret_cast< LPBYTE >( folder ), &folder_size );
	if ( res == ERROR_SUCCESS )
	{
		const CString params = CString( _T("/k cd /d \"") ) + folder + _T("Applications\\Windows\\\" && dir");
		SHELLEXECUTEINFO sei = { sizeof( SHELLEXECUTEINFO ), SEE_MASK_DEFAULT, GetSafeHwnd(), nullptr, _T("cmd.exe"), params, nullptr, SW_SHOWDEFAULT };
		VERIFY( ShellExecuteEx( &sei ) );

		SleepEx( 500, FALSE );
	}
}
