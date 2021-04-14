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

int CALLBACK CSearchManagerDlg::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	auto pThis = reinterpret_cast< const CSearchManagerDlg* >( lParamSort );
	ASSERT_VALID( pThis );
	auto item1 = reinterpret_cast< const CItem* >( pThis->m_wndList.GetItemData( static_cast< int >( lParam1 ) ) );
	ASSERT( item1 );
	auto item2 = reinterpret_cast< const CItem* >( pThis->m_wndList.GetItemData( static_cast< int >( lParam2 ) ) );
	ASSERT( item2 );
    return item1->URL.CompareNoCase( item2->URL );
}

int CSearchManagerDlg::GetGroupId(UINT nID, REFGUID guid)
{
	CString name = LoadString( nID );
	if ( ! IsEqualGUID( guid, GUID() ) )
	{
		name += _T(' ');
		name += StringFromGUID( guid );
	}

	int value;
	if ( m_Groups.Lookup( name, value ) )
	{
		return value;
	}

	LVGROUP grpRoots = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID,
		const_cast< LPTSTR >( static_cast< LPCTSTR >( name ) ), 0, nullptr, 0, static_cast< int >( m_Groups.GetCount() ) };
	VERIFY( m_wndList.InsertGroup( grpRoots.iGroupId, &grpRoots ) != -1 );
	m_Groups.SetAt( name, grpRoots.iGroupId );
	return grpRoots.iGroupId;
}

void CSearchManagerDlg::Clear()
{
	VERIFY( m_wndList.DeleteAllItems() );
	m_List.RemoveAll();

	m_wndList.RemoveAllGroups();
	m_Groups.RemoveAll();
}

void CSearchManagerDlg::Refresh()
{
	HRESULT hr = S_OK;

	CLock locker( &m_bInUse );
	if ( ! locker.Lock() )
	{
		return;
	}

	const bool bRefreshing = m_bRefresh;

	if ( m_bRefresh )
	{
		m_bRefresh = false;

		CWaitCursor wc;

		CString sVersion = _T("Unknown");

		Clear();

		SetIndex( _T("") );

		// Release old connection
		Disconnect();

		// Create a new connection
		SetStatus( IDS_CONNECTING );
		CComPtr< ISearchManager > pManager;
		hr = pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( SUCCEEDED( hr ) )
		{
			// Get version
			LPWSTR szVersion = nullptr;
			hr = pManager->GetIndexerVersionStr( &szVersion );
			if ( SUCCEEDED( hr ) )
			{
				sVersion = szVersion;
				CoTaskMemFree( szVersion );
			}

			// Get catalog
			SetStatus( IDS_GETTING_CATALOG );
			hr = pManager->GetCatalog( CATALOG_NAME, &m_pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				// Get scope
				SetStatus( IDS_ENUMERATING );
				hr = m_pCatalog->GetCrawlScopeManager( &m_pScope );
				if ( SUCCEEDED( hr ) )
				{
					EnumerateRoots( m_pScope );

					EnumerateScopeRules( m_pScope );
				}
			}
		}

		// Enumerate offline roots and scopes
		{
			// Try to get System privileges
			CAutoPtr< CAsProcess >sys( CAsProcess::RunAsSystem() );
			if ( sys )
			{
				EnumerateRegistryRoots();

				EnumerateRegistryDefaultRules();
			}
		}

		RevertToSelf();

		m_wndList.SortItemsEx( &CSearchManagerDlg::SortProc, reinterpret_cast< LPARAM >( this ) );

		SetWindowText( LoadString( AFX_IDS_APP_TITLE ) + _T(" - Indexer version: ") + sVersion );

		if ( SUCCEEDED( hr ) )
		{
			UpdateInterface();
		}
		else
		{
			SleepEx( 500, FALSE );

			const error_t result( hr );
			SetStatus( result.msg + CRLF CRLF + result.error + CRLF CRLF + LoadString( IDS_CLOSED ) );

			Disconnect();
		}
	}

	CString sStatus;

	if ( m_pCatalog )
	{
		LONG numitems = 0;
		hr = m_pCatalog->NumberOfItems( &numitems );
		if ( SUCCEEDED( hr ) )
		{
			sStatus.AppendFormat( _T("Total items      \t\t: %ld") CRLF, numitems );

			DWORD timeout = 0;
			hr = m_pCatalog->get_ConnectTimeout( &timeout );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Connect timeout     \t: %lu s") CRLF, timeout );
			}

			hr = m_pCatalog->get_DataTimeout( &timeout );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Data timeout        \t: %lu s") CRLF, timeout );
			}

			BOOL diacritic = FALSE;
			hr = m_pCatalog->get_DiacriticSensitivity( &diacritic );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Diacritic sensitivity\t: %s") CRLF, ( diacritic ? _T("yes") : _T("no") ) );
			}

			LONG pendingitems = 0, queued = 0, highpri = 0;
			hr = m_pCatalog->NumberOfItemsToIndex( &pendingitems, &queued, &highpri );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat(
					_T("Pending queue      \t: %ld") CRLF
					_T("Notification queue \t: %ld") CRLF
					_T("High priority queue\t: %ld") CRLF,
					pendingitems, queued, highpri );
			}

			CatalogStatus status;
			CatalogPausedReason reason;
			hr = m_pCatalog->GetCatalogStatus( &status, &reason );
			if ( SUCCEEDED( hr ) )
			{
				if ( status >= CATALOG_STATUS_IDLE && status <= CATALOG_STATUS_SHUTTING_DOWN )
				{
					sStatus += CRLF;
					sStatus += LoadString( IDS_STATUS_1 + static_cast< int >( status ) );
				}
				if ( status == CATALOG_STATUS_PAUSED )
				{
					if ( reason > CATALOG_PAUSED_REASON_NONE && reason <= CATALOG_PAUSED_REASON_UPGRADING )
					{
						sStatus += CRLF CRLF;
						sStatus += LoadString( IDS_REASON_1 + static_cast< int >( reason ) - 1 );
					}
				}
			}
		}

		if ( SUCCEEDED( hr ) )
		{
			SetStatus( sStatus );
		}
		else
		{
			const error_t result( hr );
			SetStatus( result.msg + CRLF CRLF + result.error );

			Disconnect();
		}
	}

	if ( m_pCatalog )
	{
		LPWSTR szIndex = nullptr;
		hr = m_pCatalog->URLBeingIndexed( &szIndex );
		if ( SUCCEEDED( hr ) )
		{
			SetIndex( szIndex );
			CoTaskMemFree( szIndex );
		}
		else
		{
			const error_t result( hr );
			SetIndex( result );
		}
	}
}

HRESULT CSearchManagerDlg::EnumerateRoots(ISearchCrawlScopeManager* pScope)
{
	TRACE( _T("EnumerateRoots\n") );

	HRESULT hr = S_OK;

	CComPtr< IEnumSearchRoots > pRoots;
	hr = pScope->EnumerateRoots( &pRoots );
	if ( SUCCEEDED( hr ) )
	{
		for (;;)
		{
			CComPtr< ISearchRoot > pRoot;
			hr = pRoots->Next( 1, &pRoot, nullptr );
			if ( hr != S_OK )
			{
				break;
			}

			CAutoPtr< CRoot > root( new CRoot( pScope, pRoot ) );
			if ( *root )
			{
				VERIFY( root->InsertTo( m_wndList, GetGroupId( IDS_ROOTS, root->Guid ) ) != -1 );
				m_List.AddTail( root.Detach() );
			}
		}
	}

	return hr;
}

HRESULT CSearchManagerDlg::EnumerateScopeRules(ISearchCrawlScopeManager* pScope)
{
	TRACE( _T("EnumerateScopeRules\n") );

	HRESULT hr = S_OK;

	CComPtr< IEnumSearchScopeRules > pRules;
	hr = pScope->EnumerateScopeRules( &pRules );
	if ( SUCCEEDED( hr ) )
	{
		for (;;)
		{
			CComPtr< ISearchScopeRule > pRule;
			hr = pRules->Next( 1, &pRule, nullptr );
			if ( hr != S_OK )
			{
				break;
			}

			CAutoPtr< CRule > rule( new CRule( pScope, pRule ) );
			if ( *rule )
			{
				VERIFY( rule->InsertTo( m_wndList, GetGroupId( IDS_RULES, rule->Guid ) ) != -1 );
				m_List.AddTail( rule.Detach() );
			}
		}
	}

	return hr;
}

void CSearchManagerDlg::EnumerateRegistryRoots()
{
	TRACE( _T("EnumerateRegistryRoots\n") );

	HKEY hRootsKey;
	LSTATUS res = RegOpenKeyFull( HKEY_LOCAL_MACHINE, KEY_ROOTS, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hRootsKey );
	if ( res == ERROR_SUCCESS )
	{
		TCHAR name[ MAX_PATH ] = {};
		for ( DWORD i = 0; ; ++i )
		{
			name[ 0 ] = 0;
			DWORD name_size = _countof( name );
			res = SHEnumKeyEx( hRootsKey, i, name, &name_size );
			if ( res != ERROR_SUCCESS )
			{
				break;
			}

			CAutoPtr< COfflineRoot > root( new COfflineRoot( CString( KEY_ROOTS ) + _T('\\') + name ) );
			if ( *root )
			{
				bool found = false;
				for ( POSITION pos = m_List.GetHeadPosition(); pos; )
				{
					auto item = m_List.GetNext( pos );
					if ( item->Group == GROUP_ROOTS && *item == *root )
					{
						found = true;
						break;
					}
				}

				if ( ! found )
				{
					VERIFY( root->InsertTo( m_wndList, GetGroupId( IDS_DEFAULT_ROOTS, root->Guid ) ) != -1 );
					m_List.AddTail( root.Detach() );
				}
			}
		}
		RegCloseKey( hRootsKey );
	}
}

void CSearchManagerDlg::EnumerateRegistryDefaultRules()
{
	TRACE( _T("EnumerateRegistryDefaultRules\n") );

	HKEY hRootsKey;
	LSTATUS res = RegOpenKeyFull( HKEY_LOCAL_MACHINE, KEY_DEFAULT_RULES, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hRootsKey );
	if ( res == ERROR_SUCCESS )
	{
		TCHAR name[ MAX_PATH ] = {};
		for ( DWORD i = 0; ; ++i )
		{
			name[ 0 ] = 0;
			DWORD name_size = _countof( name );
			res = SHEnumKeyEx( hRootsKey, i, name, &name_size );
			if ( res != ERROR_SUCCESS )
			{
				break;
			}

			CAutoPtr< CDefaultRule > rule( new CDefaultRule( CString( KEY_DEFAULT_RULES ) + _T('\\') + name ) );
			if ( *rule )
			{
				bool found = false;
				for ( POSITION pos = m_List.GetHeadPosition(); pos; )
				{
					auto item = m_List.GetNext( pos );
					if ( item->Group == GROUP_RULES && *item == *rule )
					{
						found = true;
						break;
					}
				}

				if ( ! found )
				{
					VERIFY( rule->InsertTo( m_wndList, GetGroupId( IDS_DEFAULT_RULES, rule->Guid ) ) != -1 );
					m_List.AddTail( rule.Detach() );
				}
			}
		}
		RegCloseKey( hRootsKey );
	}
}
