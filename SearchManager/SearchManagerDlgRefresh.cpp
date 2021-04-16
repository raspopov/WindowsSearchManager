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

int CSearchManagerDlg::GetGroupId(const CString& name, REFGUID guid)
{
	CString key = name;
	if ( ! IsEqualGUID( guid, GUID() ) )
	{
		key += _T(' ');
		key += StringFromGUID( guid );
	}

	const auto group = m_Groups.find( key );
	if ( group != m_Groups.end() )
	{
		return group->second;
	}

	LVGROUP grpRoots = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID,
		const_cast< LPTSTR >( static_cast< LPCTSTR >( key ) ), 0, nullptr, 0, static_cast< int >( m_Groups.size() ) };
	VERIFY( m_wndList.InsertGroup( grpRoots.iGroupId, &grpRoots ) != -1 );

	m_Groups.insert( std::make_pair( key, grpRoots.iGroupId ) );

	return grpRoots.iGroupId;
}

void CSearchManagerDlg::Clear()
{
	VERIFY( m_wndList.DeleteAllItems() );
	m_List.clear();

	m_wndList.RemoveAllGroups();
	m_Groups.clear();

	m_sUserAgent.Empty();
	m_sVersion.Empty();
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

		Clear();

		SetIndex( _T("") );

		// Release old connection
		Disconnect();

		// Create a new connection
		const static CString connecting = LoadString( IDS_CONNECTING );
		SetStatus( connecting );
		CComPtr< ISearchManager > pManager;
		hr = pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( SUCCEEDED( hr ) )
		{
			// Get version
			LPWSTR szVersion = nullptr;
			hr = pManager->GetIndexerVersionStr( &szVersion );
			if ( SUCCEEDED( hr ) )
			{
				m_sVersion = szVersion;
				CoTaskMemFree( szVersion );
			}

			LPWSTR szUserAgent = nullptr;
			hr = pManager->get_UserAgent( &szUserAgent );
			if ( SUCCEEDED( hr ) )
			{
				m_sUserAgent = szUserAgent;
				CoTaskMemFree( szUserAgent );
			}

			// Get catalog
			hr = pManager->GetCatalog( CATALOG_NAME, &m_pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				// Get scope
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
			CAutoPtr< CAsProcess >sys( CAsProcess::RunAsTrustedInstaller() );
			if ( sys )
			{
				EnumerateRegistryRoots();

				EnumerateRegistryDefaultRules();
			}
		}

		VERIFY( RevertToSelf() );

		m_wndList.SortItemsEx( &CSearchManagerDlg::SortProc, reinterpret_cast< LPARAM >( this ) );

		if ( SUCCEEDED( hr ) )
		{
			UpdateInterface();
		}
		else
		{
			SleepEx( 500, FALSE );

			const static CString closed = LoadString( IDS_CLOSED );
			const error_t result( hr );
			SetStatus( result.msg + CRLF CRLF + result.error + CRLF CRLF + closed );

			Disconnect();
		}
	}

	CString sStatus;

	if ( ! m_sUserAgent.IsEmpty() )
	{
		sStatus += _T("User agent\t\t: ") + m_sUserAgent + CRLF;
	}

	if ( ! m_sVersion.IsEmpty() )
	{
		sStatus += _T("Indexer version\t\t: ") + m_sVersion + CRLF;
	}

	if ( m_pCatalog )
	{
		LONG numitems = 0;
		hr = m_pCatalog->NumberOfItems( &numitems );
		if ( SUCCEEDED( hr ) )
		{
			sStatus.AppendFormat( _T("Total items\t\t: %ld") CRLF, numitems );

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
					_T("Pending queue\t\t: %ld") CRLF
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
					const static CString statuses[ CATALOG_STATUS_SHUTTING_DOWN + 1 ] =
					{
						LoadString( IDS_STATUS_1 ),
						LoadString( IDS_STATUS_2 ),
						LoadString( IDS_STATUS_3 ),
						LoadString( IDS_STATUS_4 ),
						LoadString( IDS_STATUS_5 ),
						LoadString( IDS_STATUS_6 ),
						LoadString( IDS_STATUS_7 )
					};
					sStatus += CRLF;
					sStatus += statuses[ static_cast< int >( status ) ];
				}
				if ( status == CATALOG_STATUS_PAUSED )
				{
					if ( reason > CATALOG_PAUSED_REASON_NONE && reason <= CATALOG_PAUSED_REASON_UPGRADING )
					{
						const static CString reasons[ CATALOG_PAUSED_REASON_UPGRADING ] =
						{
							LoadString( IDS_REASON_1 ),
							LoadString( IDS_REASON_2 ),
							LoadString( IDS_REASON_3 ),
							LoadString( IDS_REASON_4 ),
							LoadString( IDS_REASON_5 ),
							LoadString( IDS_REASON_6 ),
							LoadString( IDS_REASON_7 ),
							LoadString( IDS_REASON_8 ),
							LoadString( IDS_REASON_9 ),
							LoadString( IDS_REASON_10 )
						};
						sStatus += CRLF CRLF;
						sStatus += reasons[ static_cast< int >( reason ) - 1 ];
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
				const static CString group_name = LoadString( IDS_ROOTS );
				VERIFY( root->InsertTo( m_wndList, GetGroupId( group_name, root->Guid ) ) != -1 );
				m_List.push_back( root.Detach() );
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
				const static CString group_name = LoadString( IDS_RULES );
				VERIFY( rule->InsertTo( m_wndList, GetGroupId( group_name, rule->Guid ) ) != -1 );
				m_List.push_back( rule.Detach() );
			}
		}
	}

	return hr;
}

void CSearchManagerDlg::EnumerateRegistryRoots()
{
	const static CString group_name = LoadString( IDS_DEFAULT_ROOTS );
	EnumerateRegistry( HKEY_LOCAL_MACHINE, KEY_SEARCH_ROOTS, GROUP_ROOTS, group_name );
}

void CSearchManagerDlg::EnumerateRegistryDefaultRules()
{
	const static CString group_name = LoadString( IDS_DEFAULT_RULES );
	EnumerateRegistry( HKEY_LOCAL_MACHINE, KEY_DEFAULT_RULES, GROUP_RULES, group_name );
	EnumerateRegistry( HKEY_LOCAL_MACHINE, KEY_WORKING_RULES, GROUP_RULES, group_name );
}

void CSearchManagerDlg::EnumerateRegistry(HKEY hKey, LPCTSTR szSubkey, group_t nGroup, const CString& sGroupName)
{
	TRACE( _T("EnumerateRegistry: %s\n"), szSubkey );

	HKEY hRootsKey;
	LSTATUS res = RegOpenKeyFull( hKey, szSubkey, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hRootsKey );
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

			CString url = CString( szSubkey ) + _T('\\') + name;

			CAutoPtr< CItem > rule( ( nGroup == GROUP_RULES ) ?
				static_cast< CItem* >( new COfflineRule( url ) ) :
				static_cast< CItem* >( new COfflineRoot( url ) ) );
			if ( *rule )
			{
				bool found = false;
				for ( const auto item : m_List )
				{
					if ( ( item->Group == nGroup || ( ( nGroup == GROUP_RULES ) ?
						 ( item->Group == GROUP_OFFLINE_RULES ) : ( item->Group == GROUP_OFFLINE_ROOTS ) ) ) &&
						*item == *rule )
					{
						found = true;
						break;
					}
				}

				if ( ! found )
				{
					VERIFY( rule->InsertTo( m_wndList, GetGroupId( sGroupName, rule->Guid ) ) != -1 );
					m_List.push_back( rule.Detach() );
				}
			}
		}
		RegCloseKey( hRootsKey );
	}
}
