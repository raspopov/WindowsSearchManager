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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CItem

CItem::CItem(group_t group) noexcept
	: Group			( group )
	, Guid			()
	, m_ParseGuid	( false )
{
}

CItem::CItem(const CString& url, group_t group) noexcept
	: Group			( group )
	, URL			( url )
	, NormalURL		( url )
	, Guid			()
	, m_ParseGuid	( false )
{
}

CString CItem::GetTitle() const
{
	CString title;

	if ( Protocol.IsEmpty() )
	{
		title = URL;
	}
	else if ( User.IsEmpty() )
	{
		title = Path;
	}
	else if ( Name.IsEmpty() )
	{
		title = _T("(") + User + _T(") ") + Path;
	}
	else
	{
		title = Name + _T(" (") + User + _T(") ") + Path;
	}

	if ( HasError() )
	{
		if ( title.IsEmpty() )
		{
			return m_Error;
		}
		else
		{
			return title + ARROW + m_Error;
		}
	}
	else
	{
		return title;
	}
}

COLORREF CItem::GetColor() const
{
	// Error color (red)
	return HasError() ? RGB( 255, 128, 128 ) : 0;
}

bool CItem::Parse()
{
	NormalURL = URL;

	int begin = URL.Find( _T("://") );
	if ( begin != -1 )
	{
		Protocol = URL.Left( begin );

		begin += 3;

		int end = URL.Find( _T("/"), begin );
		if ( end != -1 )
		{
			const int len = end - begin;
			if ( len > 8 &&
				URL.GetAt( begin ) == _T('{') &&
				( URL.GetAt( begin + 1 ) == _T('S') || URL.GetAt( begin + 1 ) == _T('s') ) &&
				URL.GetAt( begin + 2 ) == _T('-') &&
				URL.GetAt( end - 1 ) == _T('}') )
			{
				User = URL.Mid( begin + 1, len - 2 );
				Path = URL.Mid( end + 1 );

				PSID psid = nullptr;
				if ( ConvertStringSidToSid( User, &psid ) )
				{
					TCHAR user[ MAX_PATH ];
					DWORD user_size = MAX_PATH;
					TCHAR domain[ MAX_PATH ];
					DWORD domain_size = MAX_PATH;
					SID_NAME_USE use;
					if ( LookupAccountSid( nullptr, psid, user, &user_size, domain, &domain_size, &use ) )
					{
						User.Format( _T("%s\\%s"), domain, user );
					}
					LocalFree( psid );
				}
			}
			else
			{
				if ( URL.GetAt( begin ) == _T('/') )
				{
					++ begin;
				}
				Path = URL.Mid( begin );
			}
		}
		else
		{
			Path = URL.Mid( begin );
		}

		if ( Protocol.CompareNoCase( DEFAULT_PROTOCOL ) == 0 )
		{
			// Default root
			const static CString def = LoadString( IDS_DEFAULT_ROOT );
			Name = def;
		}
		else if ( Protocol.CompareNoCase( FILE_PROTOCOL ) == 0 )
		{
			// File
			const TCHAR disk_letter = Path.GetAt( 0 );
			if ( _istalpha( disk_letter ) && Path.GetAt( 1 ) == _T(':') && Path.GetAt( 2 ) == _T('\\') )
			{
				// Check for inner [GUID]
				if ( Path.GetLength() >= 42 && Path.GetAt( 3 ) == _T('[') && Path.GetAt( 40 ) == _T(']') )
				{
					const CString guid = _T("{") + Path.Mid( 4, 36 ) + _T("}");

					// Cut-off the GUID from the Path and URL
					Path = Path.Left( 3 ) + Path.Mid( 42 );
					NormalURL = Protocol + _T(":///") + Path;

					if ( SUCCEEDED( CLSIDFromString( guid, &Guid ) ) && m_ParseGuid )
					{
						// Read disk GUID if any
						const DWORD drives = GetLogicalDrives();
						if ( ( drives & ( 1 << ( disk_letter - 'A' ) ) ) != 0 )
						{
							GUID disk_guid = {};
							HRESULT hr = ReadVolumeGuid( disk_letter, disk_guid );
							if ( SUCCEEDED( hr ) )
							{
								if ( IsEqualGUID( Guid, disk_guid ) )
								{
									// Clear actual GUID
									Guid = GUID();
								}
							}
							else
							{
								SetError( error_t( hr ) );
							}
						}
						else
						{
							TRACE( _T("Disk missed: %c:\\\n"), disk_letter );
						}
					}
				}
			}
		}
		else
		{
			// Protocol handler
			const CString progid = ProgIDFromProtocol( Protocol );
			if ( ! progid.IsEmpty() )
			{
				CLSID clsid = {};
				if ( SUCCEEDED( CLSIDFromProgIDEx( progid, &clsid ) ) )
				{
					const CString str_clsid = StringFromGUID( clsid );
					if ( ! str_clsid.IsEmpty() )
					{
						TCHAR def[ MAX_PATH ] = {};
						DWORD type, def_size = sizeof( def );
						LSTATUS res = RegQueryValueFull( HKEY_CLASSES_ROOT, _T("CLSID\\") + str_clsid, _T(""), &type,
							reinterpret_cast< LPBYTE >( def ), &def_size );
						if ( res == ERROR_SUCCESS )
						{
							Name = def;
						}
					}
				}
			}
		}
	}

	return ! URL.IsEmpty();
}

HRESULT CItem::ReadVolumeGuid(TCHAR disk, GUID& guid)
{
	CString filename;
	filename.Format( _T("%c:\\%s"), disk, INDEXER_VOLUME );

	CAtlFile file;
	HRESULT hr = file.Create( filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING );
	if ( SUCCEEDED( hr ) )
	{
		TCHAR buf[ 38 + 1 ] = {};
		DWORD read = 76;
		hr = file.Read( buf, 76, read );
		if ( SUCCEEDED( hr ) )
		{
			if ( read == 76 )
			{
				hr = CLSIDFromString( buf, &guid );
				if ( SUCCEEDED( hr ) )
				{
					return hr;
				}
			}
			else
			{
				hr = ERROR_INVALID_DATA;
			}
			TRACE( _T("Bad file format: %s\n"), (LPCTSTR)filename );
			return hr;
		}
	}
	TRACE( _T("File read error \"%s\": %s\n"), (LPCTSTR)filename, (LPCTSTR)(CString)error_t( hr ) );
	return hr;
}

int CItem::InsertTo(CListCtrl& list, int group_id) const
{
	const CString title = GetTitle();

	LVITEM item = { LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM };
	item.iGroupId = group_id;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( Protocol ) );
	item.lParam = reinterpret_cast< LPARAM >( this );
	item.iItem = list.InsertItem( &item );
	ASSERT( item.iItem != -1 );

	item.iSubItem = 1;
	item.mask = LVIF_TEXT;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( title ) );
	VERIFY( list.SetItem( &item ) );

	return item.iItem;
}

// CRoot

CRoot::CRoot(group_t group)
	: CItem					( group )
	, IncludedInCrawlScope	( FALSE )
	, IsHierarchical		( FALSE )
	, ProvidesNotifications	( FALSE )
	, UseNotificationsOnly	( FALSE )
{
}

CRoot::CRoot(BOOL notif, const CString& url, group_t group)
	: CItem					( url, group )
	, IncludedInCrawlScope	( FALSE )
	, IsHierarchical		( FALSE )
	, ProvidesNotifications	( notif )
	, UseNotificationsOnly	( FALSE )
{
}

CRoot::CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot, group_t group)
	: CItem					( group )
	, IncludedInCrawlScope	( FALSE )
	, IsHierarchical		( FALSE )
	, ProvidesNotifications	( FALSE )
	, UseNotificationsOnly	( FALSE )
{
	LPWSTR szURL = nullptr;
	HRESULT hr = pRoot->get_RootURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		URL = szURL;
		CoTaskMemFree( szURL );

		CLUSION_REASON reason;
		hr = pScope->IncludedInCrawlScopeEx( URL, &IncludedInCrawlScope, &reason );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_IsHierarchical( &IsHierarchical );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_ProvidesNotifications( &ProvidesNotifications );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_UseNotificationsOnly( &UseNotificationsOnly );
		ASSERT( SUCCEEDED( hr ) );
	}
}

int CRoot::InsertTo(CListCtrl& list, int group_id) const
{
	LVITEM item = { LVIF_TEXT };
	item.iItem = CItem::InsertTo( list, group_id );

	item.iSubItem = 2;
	const static CString in_scope = LoadString( IDS_ROOT_IN_SCOPE );
	item.pszText = const_cast< LPTSTR >( IncludedInCrawlScope ? static_cast< LPCTSTR >( in_scope ) : _T("") );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 3;
	const static CString hier = LoadString( IDS_ROOT_HIER );
	item.pszText = const_cast< LPTSTR >( IsHierarchical ? static_cast< LPCTSTR >( hier ) : _T("") );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 4;
	const static CString notify = LoadString( IDS_ROOT_NOTIFY );
	const static CString notify_only = LoadString( IDS_ROOT_NOTIFY_ONLY );
	item.pszText = const_cast< LPTSTR >( ProvidesNotifications ? ( UseNotificationsOnly ? static_cast< LPCTSTR >( notify_only ) : static_cast< LPCTSTR >( notify ) ): _T("") );
	VERIFY( list.SetItem( &item ) );

	return item.iItem;
}

HRESULT CRoot::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	const HRESULT hr = pScope->RemoveRoot( NormalURL );
	TRACE( _T("Deleting root from scope: \"%s\" : %s\n"),
		static_cast< LPCTSTR >( NormalURL ), static_cast< LPCTSTR >( static_cast< CString >( error_t( hr ) ) ) );
	return hr;
}

HRESULT CRoot::AddTo(ISearchCrawlScopeManager* pScope) const
{
	CComPtr< ISearchRoot > root;
	HRESULT hr = root.CoCreateInstance( __uuidof( CSearchRoot ) );
	if ( SUCCEEDED( hr ) )
	{
		hr = root->put_RootURL( URL );
		if ( SUCCEEDED( hr ) )
		{
			hr = root->put_ProvidesNotifications( ProvidesNotifications );
			if ( SUCCEEDED( hr ) )
			{
				hr = pScope->AddRoot( root );
			}
		}
	}
	return hr;
}

HRESULT CRoot::Reindex(ISearchCatalogManager* pCatalog) const
{
	return pCatalog->ReindexSearchRoot( NormalURL );
}

// COfflineRoot

COfflineRoot::COfflineRoot(const CString& key, group_t group)
	: CRoot		( group )
	, Key		( key.Mid( key.ReverseFind( _T('\\') ) + 1 ) )
{
	m_ParseGuid = true;

	TCHAR url[ MAX_PATH ] = {};
	DWORD type, ulr_size = sizeof( url );
	LSTATUS res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
	if ( res == ERROR_SUCCESS )
	{
		URL = url;

		DWORD notif = 0;
		DWORD notif_size = sizeof( DWORD );
		res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("ProvidesNotifications"), &type, reinterpret_cast< LPBYTE >( &notif ), &notif_size );
		ASSERT( res == ERROR_SUCCESS );
		ProvidesNotifications = ( notif != 0 );
	}
}

HRESULT COfflineRoot::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	if ( pScope )
	{
		// Delete root
		return CRoot::DeleteFrom( pScope );
	}

	ASSERT( ! Key.IsEmpty() );

	TCHAR url[ MAX_PATH ] = {};
	DWORD type, ulr_size;

	// Checking key
	const CString subkey = CString( KEY_SEARCH_ROOTS ) + _T("\\") + Key;
	TRACE( _T("Deleting registry key: %s\n"), static_cast< LPCTSTR >( subkey ) );
	ulr_size = sizeof( url );
	LSTATUS res = RegQueryValueFull( HKEY_LOCAL_MACHINE, subkey, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
	if ( res == ERROR_SUCCESS )
	{
		if ( URL == url )
		{
			// Delete from registry
			res = SHDeleteKey( HKEY_LOCAL_MACHINE, subkey );
		}
		else
		{
			TRACE( _T("Registry key changed!\n") );
		}
	}

	if ( res == ERROR_PATH_NOT_FOUND || res == ERROR_FILE_NOT_FOUND )
	{
		res = ERROR_SUCCESS;
	}
	return HRESULT_FROM_WIN32( res );
}

// CRule

CRule::CRule(group_t group)
	: CItem		( group )
	, IsInclude	( FALSE )
	, IsDefault	( FALSE )
	, HasChild	( FALSE )
{
}

CRule::CRule(BOOL incl, BOOL def, const CString& url, group_t group)
	: CItem		( url, group )
	, IsInclude	( incl )
	, IsDefault	( def )
	, HasChild	( FALSE )
{
}

CRule::CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule, group_t group)
	: CItem		( group )
	, IsInclude	( FALSE )
	, IsDefault	( FALSE )
	, HasChild	( FALSE )
{
	LPWSTR szURL = nullptr;
	HRESULT hr = pRule->get_PatternOrURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		URL = szURL;
		CoTaskMemFree( szURL );

		hr = pRule->get_IsIncluded( &IsInclude );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRule->get_IsDefault( &IsDefault );
		ASSERT( SUCCEEDED( hr ) );

		hr = pScope->HasChildScopeRule( URL, &HasChild );
	}
}

COLORREF CRule::GetColor() const
{
	if ( COLORREF color = CItem::GetColor() )
	{
		return color;
	}

	// Include color (green) and exclude one (yellow)
	return IsInclude ? RGB( 192, 255, 192 ) : RGB( 255, 255, 192 );
}

int CRule::InsertTo(CListCtrl& list, int group_id) const
{
	LVITEM item = { LVIF_TEXT };
	item.iItem = CItem::InsertTo( list, group_id );

	item.iSubItem = 2;
	const static CString incl = LoadString( IDS_RULE_INCLUDE );
	const static CString excl = LoadString( IDS_RULE_EXCLUDE );
	item.pszText = const_cast< LPTSTR >( IsInclude ? static_cast< LPCTSTR >( incl ) : static_cast< LPCTSTR >( excl ) );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 3;
	const static CString def = LoadString( IDS_RULE_DEFAULT );
	const static CString user = LoadString( IDS_RULE_USER );
	item.pszText = const_cast< LPTSTR >( IsDefault ? static_cast< LPCTSTR >( def ) : static_cast< LPCTSTR >( user ) );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 4;
	const static CString child = LoadString( IDS_RULE_HAS_CHILD );
	item.pszText = const_cast< LPTSTR >( HasChild ? static_cast< LPCTSTR >( child ) : _T("") );
	VERIFY( list.SetItem( &item ) );

	return item.iItem;
}

HRESULT CRule::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	const HRESULT hr1 = pScope->RemoveDefaultScopeRule( NormalURL );
	const HRESULT hr2 = pScope->RemoveScopeRule( NormalURL );
	const HRESULT hr = SUCCEEDED( hr1 ) ? hr1 : hr2;
	TRACE( _T("Deleting %s rule from scope: \"%s\" : %s\n"), ( IsDefault ? _T("Default") : _T("User") ),
		static_cast< LPCTSTR >( NormalURL ), static_cast< LPCTSTR >( static_cast< CString >( error_t( hr ) ) ) );
	return hr;
}

HRESULT CRule::AddTo(ISearchCrawlScopeManager* pScope ) const
{
	return IsDefault ? pScope->AddDefaultScopeRule( URL, IsInclude, FF_INDEXCOMPLEXURLS ) :
		pScope->AddUserScopeRule( URL, IsInclude, FALSE, FF_INDEXCOMPLEXURLS );
}

HRESULT CRule::Reindex(ISearchCatalogManager* pCatalog) const
{
	return pCatalog->ReindexMatchingURLs( NormalURL );
}

// COfflineRule

COfflineRule::COfflineRule(const CString& key, group_t group)
	: CRule	( group )
	, Key	( key.Mid( key.ReverseFind( _T('\\') ) + 1 ) )
{
	m_ParseGuid = true;

	TCHAR url[ MAX_PATH ] = {};
	DWORD type, ulr_size = sizeof( url );
	LSTATUS res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
	if ( res == ERROR_SUCCESS )
	{
		URL = url;

		DWORD incl = 0;
		DWORD incl_size = sizeof( DWORD );
		res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("Include"), &type, reinterpret_cast< LPBYTE >( &incl ), &incl_size );
		ASSERT( res == ERROR_SUCCESS );
		IsInclude = ( incl != 0 );

		DWORD def = 0;
		DWORD def_size = sizeof( DWORD );
		res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("Default"), &type, reinterpret_cast< LPBYTE >( &def ), &def_size );
		ASSERT( res == ERROR_SUCCESS );
		IsDefault = ( def != 0 );
	}
}

HRESULT COfflineRule::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	if ( pScope )
	{
		// Delete from scope
		return CRule::DeleteFrom( pScope );
	}

	ASSERT( ! Key.IsEmpty() );

	TCHAR url[ MAX_PATH ] = {};
	DWORD type, ulr_size;

	// Checking key
	CString subkey = CString( KEY_DEFAULT_RULES ) + _T("\\") + Key;
	TRACE( _T("Deleting registry key: %s\n"), static_cast< LPCTSTR >( subkey ) );
	ulr_size = sizeof( url );
	LSTATUS res = RegQueryValueFull( HKEY_LOCAL_MACHINE, subkey, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
	if ( res == ERROR_SUCCESS )
	{
		if ( URL == url )
		{
			// Delete from registry
			res = SHDeleteKey( HKEY_LOCAL_MACHINE, subkey );
		}
		else
		{
			TRACE( _T("Key changed!\n") );
		}
	}

	// Checking key
	subkey = CString( KEY_WORKING_RULES ) + _T("\\") + Key;
	TRACE( _T("Deleting registry key: %s\n"), static_cast< LPCTSTR >( subkey ) );
	ulr_size = sizeof( url );
	res = RegQueryValueFull( HKEY_LOCAL_MACHINE, subkey, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
	if ( res == ERROR_SUCCESS )
	{
		if ( URL == url )
		{
			// Delete from registry
			res = SHDeleteKey( HKEY_LOCAL_MACHINE, subkey );
		}
		else
		{
			TRACE( _T("Registry key changed!\n") );
		}
	}

	if ( res == ERROR_PATH_NOT_FOUND || res == ERROR_FILE_NOT_FOUND )
	{
		res = ERROR_SUCCESS;
	}
	return HRESULT_FROM_WIN32( res );
}

// CVolume

CVolume::CVolume(TCHAR disk, group_t group)
	: CItem				( group )
	, HasDuplicateDUID	( FALSE )
{
	Path.Format( _T("%c:\\"), disk );
}

CString CVolume::GetTitle() const
{
	if ( HasDuplicateDUID )
	{
		const static CString dup = LoadString( IDS_DUPLICATE_VOLUME );

		return URL + ARROW + dup;
	}
	else
	{
		return CItem::GetTitle();
	}
}

COLORREF CVolume::GetColor() const
{
	if ( COLORREF color = CItem::GetColor() )
	{
		return color;
	}

	// Duplicate color (red)
	return HasDuplicateDUID ? RGB( 255, 128, 128 ) : 0;
}

bool CVolume::Parse()
{
	// Check drive existence
	const DWORD drives = GetLogicalDrives();
	if ( ( drives & ( 1 << ( Path.GetAt( 0 ) - 'A' ) ) ) != 0 )
	{
		// Check drive type
		const UINT type = GetDriveType( Path );
		if ( type == DRIVE_FIXED || type == DRIVE_REMOVABLE )
		{
			URL = Path + _T(' ');

			// Read GUID
			HRESULT hr = ReadVolumeGuid( Path.GetAt( 0 ), Guid );
			if ( SUCCEEDED( hr ) )
			{
				URL += StringFromGUID( Guid );
				return true;
			}
			else if ( hr == HRESULT_FROM_WIN32( ERROR_PATH_NOT_FOUND ) || hr == HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND ) )
			{
				// No file
			}
			else
			{
				SetError( error_t( hr ) );
				return true;
			}
		}
	}

	return false;
}

HRESULT CVolume::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	if ( pScope )
	{
		return E_NOTIMPL;
	}

	if ( ! HasDuplicateDUID )
	{
		const static CString format = LoadString( IDS_CONFIRM_VOLUME );
		CString msg;
		msg.Format( format, static_cast< LPCTSTR >( Path ) );
		if ( AfxMessageBox( msg, MB_YESNO | MB_ICONEXCLAMATION ) != IDYES )
		{
			return S_FALSE;
		}
	}

	GUID disk_guid = {};
	HRESULT hr = ReadVolumeGuid( Path.GetAt( 0 ), disk_guid );
	if ( SUCCEEDED( hr ) )
	{
		if ( IsEqualGUID( Guid, disk_guid ) )
		{
			// Delete GUID file from disk
			const CString filename = Path + INDEXER_VOLUME;
			if ( DeleteFile( filename ) )
			{
				return S_OK;
			}
			else
			{
				hr = HRESULT_FROM_WIN32( GetLastError() );
			}
		}
		else
		{
			TRACE( _T("Volume GUID changed!\n") );
		}
	}

	return hr;
}

HRESULT CVolume::Reindex(ISearchCatalogManager* pCatalog) const
{
	const CString url = FILE_PROTOCOL _T(":///") + Path;

	return pCatalog->ReindexSearchRoot( url );
}
