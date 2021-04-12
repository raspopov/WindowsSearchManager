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

void CSearchManagerDlg::AddRoot(LPCTSTR szURL)
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	CUrlDialog dlg( this );
	dlg.m_sTitle = LoadString( IDS_ADD_ROOT );
	if ( szURL )
	{
		dlg.m_sURL = szURL;
	}
	dlg.m_sInfo = LoadString( IDS_ADD_ROOT_INFO );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() && ( ! szURL || dlg.m_sURL.CompareNoCase( szURL ) != 0 ) )
	{
		CWaitCursor wc;

		// Delete old root if specified
		HRESULT hr = S_OK;
		if ( szURL )
		{
			CRoot root( TRUE, szURL );
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

void CSearchManagerDlg::AddRule(BOOL bInclude, BOOL bDefault, LPCTSTR szURL)
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
	if ( szURL )
	{
		dlg.m_sURL = szURL;
	}
	dlg.m_sInfo = LoadString( IDS_ADD_SCOPE_INFO );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() && ( ! szURL || dlg.m_sURL.CompareNoCase( szURL ) != 0 ) )
	{
		CWaitCursor wc;

		// Delete old scope rule if specified
		HRESULT hr = S_OK;
		if ( szURL )
		{
			CRule rule( bInclude, bDefault, szURL );
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
	CList< CItem* > to_delete;
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< CItem* >( m_wndList.GetItemData( index ) ) )
		{
			if ( IsDeletable( item->Group ) )
			{
				to_delete.AddTail( item );
			}
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
				CItem* item = to_delete.GetNext( pos );

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
