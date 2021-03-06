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

	LVGROUP grp =
	{
		sizeof( LVGROUP ),
		LVGF_HEADER| LVGF_GROUPID | LVGF_STATE,
		const_cast< LPTSTR >( static_cast< LPCTSTR >( key ) ),
		0,
		nullptr,
		0,
		static_cast< int >( m_Groups.size() ),
		( IsWindowsVistaOrGreater() ? LVGS_COLLAPSIBLE : 0u ),
		( IsWindowsVistaOrGreater() ? LVGS_COLLAPSIBLE : 0u )
	};
	VERIFY( m_wndList.EnableGroupView( TRUE ) != -1 );
	VERIFY( m_wndList.InsertGroup( grp.iGroupId, &grp ) != -1 );

	m_Groups.emplace( std::make_pair( key, grp.iGroupId ) );

	return grp.iGroupId;
}

void CSearchManagerDlg::Clear()
{
	VERIFY( m_wndList.DeleteAllItems() );
	m_List.clear();

	m_wndList.RemoveAllGroups();
	m_Groups.clear();

	m_sUserAgent.reset();
	m_sVersion.reset();
	m_sEnableFindMyFiles.reset();
	m_sDiacriticSensitivity.reset();
}

void CSearchManagerDlg::Refresh()
{
	HRESULT hr = S_OK;
	CString sStatus;

	CLock locker( &m_bInUse );
	if ( ! locker.Lock() )
	{
		return;
	}

	const bool bRefreshing = m_bRefresh;

	if ( m_bRefresh )
	{
		m_bRefresh = false;
		m_nDrives = GetLogicalDrives();

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
				m_sVersion.emplace( szVersion );
				CoTaskMemFree( szVersion );
			}
			else
			{
				m_sVersion.emplace( error_t( hr ) );
			}

			LPWSTR szUserAgent = nullptr;
			hr = pManager->get_UserAgent( &szUserAgent );
			if ( SUCCEEDED( hr ) )
			{
				m_sUserAgent.emplace( szUserAgent );
				CoTaskMemFree( szUserAgent );
			}
			else
			{
				m_sUserAgent.emplace( error_t( hr ) );
			}

			// Get catalog
			hr = pManager->GetCatalog( CATALOG_NAME, &m_pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				BOOL diacritic = FALSE;
				hr = m_pCatalog->get_DiacriticSensitivity( &diacritic );
				if ( SUCCEEDED( hr ) )
				{
					m_sDiacriticSensitivity.emplace( diacritic ? _T("Yes") : _T("No") );
				}
				else
				{
					m_sDiacriticSensitivity.emplace( error_t( hr ) );
				}

				// Get scope
				hr = m_pCatalog->GetCrawlScopeManager( &m_pScope );
				if ( SUCCEEDED( hr ) )
				{
					EnumerateRoots( m_pScope );

					EnumerateScopeRules( m_pScope );
				}
			}
		}

		// Try to get System privileges
		CAutoPtr< CAsProcess >sys( CAsProcess::RunAsTrustedInstaller() );
		if ( sys )
		{
			EnumerateVolumes();
			EnumerateRegistryRoots();
			EnumerateRegistryDefaultRules();

			sys.Free();

			VERIFY( RevertToSelf() );
		}
		else
		{
			sys.Free();

			VERIFY( RevertToSelf() );

			// Ask for administrative rights once
			static bool check = true;
			if ( check && ! IsProcessElevated() )
			{
				check = false;
				if ( AfxMessageBox( LoadString( IDS_NONELEVATED ), MB_YESNO | MB_ICONWARNING ) == IDYES )
				{
					SHELLEXECUTEINFO sei = { sizeof( SHELLEXECUTEINFO ), SEE_MASK_DEFAULT, GetSafeHwnd(), _T("runas"), theApp.ModulePath,
						nullptr, nullptr, SW_SHOWNORMAL };
					VERIFY( ShellExecuteEx( &sei ) );

					CDialog::OnOK();
					return;
				}
			}

			EnumerateVolumes();
			EnumerateRegistryRoots();
			EnumerateRegistryDefaultRules();
		}

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

		DWORD value = 0;
		DWORD value_size = sizeof( DWORD ), type;
		LRESULT res = RegQueryValueFull( HKEY_LOCAL_MACHINE, KEY_GATHER, _T("EnableFindMyFiles"), &type, reinterpret_cast< LPBYTE >( &value ), &value_size );
		if ( res == ERROR_SUCCESS )
		{
			m_sEnableFindMyFiles.emplace( value ? _T("Yes") : _T("No") );
		}
		else if ( res != ERROR_FILE_NOT_FOUND && res != ERROR_PATH_NOT_FOUND )
		{
			m_sEnableFindMyFiles.emplace( GetErrorEx( static_cast< DWORD >( res ) ) );
		}
	}

	if ( m_sUserAgent )
	{
		sStatus += _T("User agent\t\t: ");
		sStatus += m_sUserAgent.value();
		sStatus += CRLF;
	}

	if ( m_sVersion )
	{
		sStatus += _T("Indexer version\t\t: ");
		sStatus += m_sVersion.value();
		sStatus += CRLF;
	}

	if ( m_sEnableFindMyFiles )
	{
		sStatus += _T("\"Find My Files\"\t\t: ");
		sStatus += m_sEnableFindMyFiles.value();
		sStatus += CRLF;
	}

	if ( m_sDiacriticSensitivity )
	{
		sStatus += _T("Diacritic sensitivity\t: ");
		sStatus += m_sDiacriticSensitivity.value();
		sStatus += CRLF;
	}

	if ( theApp.SearchDatabase )
	{
		WIN32_FILE_ATTRIBUTE_DATA wfad = {};
		if ( GetFileAttributesEx( theApp.SearchDatabase.value(), GetFileExInfoStandard, &wfad ) )
		{
			const ULARGE_INTEGER size = { { wfad.nFileSizeLow, wfad.nFileSizeHigh } };
			TCHAR database_size[ 16 ] = {};
			if ( StrFormatByteSize64( size.QuadPart, database_size, 16 ) )
			{
				sStatus += _T("Database size\t\t: ");
				sStatus += database_size;
				sStatus += CRLF;
			}
		}
	}

	if ( m_pCatalog )
	{
		LONG numitems = 0;
		hr = m_pCatalog->NumberOfItems( &numitems );
		if ( SUCCEEDED( hr ) )
		{
			sStatus.AppendFormat( _T("Total items\t\t: %ld") CRLF, numitems );

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
						sStatus += CRLF;
						sStatus += reasons[ static_cast< int >( reason ) - 1 ];
					}
				}
			}
		}

		LPWSTR szIndex = nullptr;
		HRESULT hrIndex = m_pCatalog->URLBeingIndexed( &szIndex );
		if ( SUCCEEDED( hrIndex ) )
		{
			SetIndex( szIndex );
			CoTaskMemFree( szIndex );
		}
		else
		{
			const error_t result( hrIndex );
			SetIndex( result );
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

			CAutoPtr< CRoot > root( new CRoot );
			hr = root->Load( pScope, pRoot );
			if ( SUCCEEDED( hr ) )
			{
				const static CString group_name = LoadString( IDS_ROOTS );
				VERIFY( root->InsertTo( m_wndList, GetGroupId( group_name, root->Guid ) ) != -1 );
				m_List.push_back( root.Detach() );
			}
			else
			{
				TRACE( _T("Root load error: %s\n"), static_cast< LPCTSTR >( static_cast< CString >( error_t( hr ) ) ) );
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

			CAutoPtr< CRule > rule( new CRule );
			hr = rule->Load( pScope, pRule );
			if ( SUCCEEDED( hr ) )
			{
				const static CString group_name = LoadString( IDS_RULES );
				VERIFY( rule->InsertTo( m_wndList, GetGroupId( group_name, rule->Guid ) ) != -1 );
				m_List.push_back( rule.Detach() );
			}
			else
			{
				TRACE( _T("Rule load error: %s\n"), static_cast< LPCTSTR >( static_cast< CString >( error_t( hr ) ) ) );
			}
		}
	}

	return hr;
}

HRESULT CSearchManagerDlg::EnumerateRegistryRoots()
{
	const static CString group_name = LoadString( IDS_DEFAULT_ROOTS );

	HRESULT hr = EnumerateRegistry( HKEY_LOCAL_MACHINE, KEY_SEARCH_ROOTS, GROUP_ROOTS, group_name );
	if ( FAILED( hr ) )
	{
		CAutoPtr< CItem > item( new CItem );
		item->SetError(  LoadString( IDS_REGISTRY_ERROR ) + KEY_SEARCH_ROOTS + ARROW + error_t( hr ) );
		VERIFY( item->InsertTo( m_wndList, GetGroupId( group_name ) ) != -1 );
		m_List.push_back( item.Detach() );
	}
	return hr;
}

HRESULT CSearchManagerDlg::EnumerateRegistryDefaultRules()
{
	const static CString group_name = LoadString( IDS_DEFAULT_RULES );

	HRESULT hr = EnumerateRegistry( HKEY_LOCAL_MACHINE, KEY_DEFAULT_RULES, GROUP_RULES, group_name );
	if ( FAILED( hr ) )
	{
		CAutoPtr< CItem > item( new CItem );
		item->SetError( LoadString( IDS_REGISTRY_ERROR ) + KEY_DEFAULT_RULES + ARROW + error_t( hr ) );
		VERIFY( item->InsertTo( m_wndList, GetGroupId( group_name ) ) != -1 );
		m_List.push_back( item.Detach() );
	}

	hr = EnumerateRegistry( HKEY_LOCAL_MACHINE, KEY_WORKING_RULES, GROUP_RULES, group_name );
	if ( FAILED( hr ) )
	{
		CAutoPtr< CItem > item( new CItem );
		item->SetError( LoadString( IDS_REGISTRY_ERROR ) + KEY_WORKING_RULES + ARROW + error_t( hr ) );
		VERIFY( item->InsertTo( m_wndList, GetGroupId( group_name ) ) != -1 );
		m_List.push_back( item.Detach() );
	}

	return hr;
}

HRESULT CSearchManagerDlg::EnumerateRegistry(HKEY hKey, LPCTSTR szSubkey, group_t nGroup, const CString& sGroupName)
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
				// No more data
				if ( res == ERROR_NO_MORE_ITEMS )
				{
					res = ERROR_SUCCESS;
				}
				break;
			}

			const CString key = CString( szSubkey ) + _T('\\') + name;

			CAutoPtr< CItem > rule( ( nGroup == GROUP_RULES ) ?
				static_cast< CItem* >( new COfflineRule() ) :
				static_cast< CItem* >( new COfflineRoot() ) );
			HRESULT hr = rule->Load( key );
			if ( SUCCEEDED( hr ) )
			{
				bool found = false;
				for ( const auto item : m_List )
				{
					if ( ( item->Group == nGroup || ( ( nGroup == GROUP_RULES ) ?
						 ( item->Group == GROUP_OFFLINE_RULES ) : ( item->Group == GROUP_OFFLINE_ROOTS ) ) ) &&
						*item == *rule )
					{
						//TRACE( _T("Excluded same item %d: %s\n"), rule->Group, static_cast< LPCTSTR >( rule->URL ) );
						//TRACE( _T("   because of item %d: %s\n"), item->Group, static_cast< LPCTSTR >( item->URL ) );
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
			else
			{
				TRACE( _T("Registry load error: %s\n"), static_cast< LPCTSTR >( static_cast< CString >( error_t( hr ) ) ) );
			}
		}
		RegCloseKey( hRootsKey );
	}

	return HRESULT_FROM_WIN32( res );
}

void CSearchManagerDlg::EnumerateVolumes()
{
	const static CString group_name = LoadString( IDS_SEARCH_VOLUMES );

	for ( TCHAR disk = 'A'; disk <= 'Z'; ++disk )
	{
		CAutoPtr< CVolume > volume( new CVolume( disk ) );
		if ( volume && volume->Parse() )
		{
			// Looking for duplicates
			for ( const auto item : m_List )
			{
				if ( item->Group == GROUP_VOLUMES && *item == *volume )
				{
					volume->HasDuplicateDUID = TRUE;
					break;
				}
			}

			VERIFY( volume->InsertTo( m_wndList, GetGroupId( group_name ) ) != -1 );
			m_List.push_back( volume.Detach() );
		}
	}
}
