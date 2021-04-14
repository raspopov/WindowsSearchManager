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
	: Group	( group )
	, Guid	()
{
}

CItem::CItem(const CString& url, group_t group) noexcept
	: Group	( group )
	, URL	( url )
	, Guid	()
{
}

CString CItem::GetTitle() const
{
	if ( Protocol.IsEmpty() )
	{
		return URL;
	}
	else if ( User.IsEmpty() )
	{
		return Path;
	}
	else if ( Name.IsEmpty() )
	{
		return _T("(") + User + _T(") ") + Path;
	}
	else
	{
		return Name + _T(" (") + User + _T(") ") + Path;
	}
}

void CItem::ParseURL(bool bGuid)
{
	int begin = URL.Find( _T("://") );
	if ( begin != -1 )
	{
		Protocol = URL.Left( begin );

		begin += 3;

		int end = URL.Find( _T("/"), begin );
		if ( end != -1 )
		{
			const int len = end - begin;
			if ( len > 8 && URL.GetAt( begin ) == _T('{') && URL.GetAt( end - 1 ) == _T('}') )
			{
				User = URL.Mid( begin + 1, len - 2 );

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

			Path = URL.Mid( end + 1 );
		}
		else
		{
			Path = URL.Mid( begin );
		}

		if ( Protocol.CompareNoCase( DEFAULT_PROTOCOL ) == 0 )
		{
			// Default root
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
					Path = Path.Left( 3 ) + (LPCTSTR)Path.Mid( 42 );
					URL = Protocol + _T(":///") + Path;

					if ( SUCCEEDED( CLSIDFromString( guid, &Guid ) ) && bGuid )
					{
						// Read disk GUID if any
						const DWORD drives = GetLogicalDrives();
						if ( ( drives & ( 1 << ( disk_letter - 'A' ) ) ) != 0 )
						{
							CFile file;
							const CString filename = Path.Left( 3 ) + INDEXER_VOLUME;
							if ( file.Open( filename, CFile::modeRead | CFile::shareDenyNone | CFile::modeNoTruncate ) )
							{
								TCHAR buf[ 38 + 1 ] = {};
								GUID disk_guid = {};
								if ( file.Read( buf, 76 ) == 76 && SUCCEEDED( CLSIDFromString( buf, &disk_guid ) ) )
								{
									// Clear actual GUID
									if ( IsEqualGUID( Guid, disk_guid ) )
									{
										Guid = GUID();
									}
								}
								else
								{
									TRACE( _T("Bad file format: %s\n"), (LPCTSTR)filename );
								}
							}
							else
							{
								TRACE( _T("File read error \"%s\": %s\n"), (LPCTSTR)(CString)error_t(), (LPCTSTR)filename );
							}
						}
						else
						{
							TRACE( _T("Disk missed: %c\n"), disk_letter );
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

		ParseURL( false );
	}
}

int CRoot::InsertTo(CListCtrl& list, int group_id) const
{
	LVITEM item = { LVIF_TEXT };
	item.iItem = CItem::InsertTo( list, group_id );

	item.iSubItem = 2;
	item.pszText = const_cast< LPTSTR >( IncludedInCrawlScope ? _T("In Scope") : _T("") );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 3;
	item.pszText = const_cast< LPTSTR >( IsHierarchical ? _T("Hierarchical") : _T("") );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 4;
	item.pszText = const_cast< LPTSTR >( ProvidesNotifications ? ( UseNotificationsOnly ? _T("Notify Only") : _T("Notify") ): _T("") );
	VERIFY( list.SetItem( &item ) );

	return item.iItem;
}

HRESULT CRoot::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	return pScope->RemoveRoot( URL );
}

HRESULT CRoot::AddTo(ISearchCrawlScopeManager* pScope ) const
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
	return pCatalog->ReindexSearchRoot( URL );
}

// COfflineRoot

COfflineRoot::COfflineRoot(const CString& key, group_t group)
	: CRoot		( group )
	, Key		( key )
{
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

		ParseURL( true );
	}
}

HRESULT COfflineRoot::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	return CRoot::DeleteFrom( pScope );
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

		ParseURL( false );
	}
}

int CRule::InsertTo(CListCtrl& list, int group_id) const
{
	LVITEM item = { LVIF_TEXT };
	item.iItem = CItem::InsertTo( list, group_id );

	item.iSubItem = 2;
	item.pszText = const_cast< LPTSTR >( IsInclude ? _T("Include") : _T("Exclude") );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 3;
	item.pszText = const_cast< LPTSTR >( IsDefault ? _T("Default") : _T("User") );
	VERIFY( list.SetItem( &item ) );

	item.iSubItem = 4;
	item.pszText = const_cast< LPTSTR >( HasChild ? _T("Has child") : _T("") );
	VERIFY( list.SetItem( &item ) );

	return item.iItem;
}

HRESULT CRule::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	return IsDefault ? pScope->RemoveDefaultScopeRule( URL ) : pScope->RemoveScopeRule( URL );
}

HRESULT CRule::AddTo(ISearchCrawlScopeManager* pScope ) const
{
	return IsDefault ? pScope->AddDefaultScopeRule( URL, IsInclude, FF_INDEXCOMPLEXURLS ) :
		pScope->AddUserScopeRule( URL, IsInclude, FALSE, FF_INDEXCOMPLEXURLS );
}

HRESULT CRule::Reindex(ISearchCatalogManager* pCatalog) const
{
	return pCatalog->ReindexMatchingURLs( URL );
}

// CDefaultRule

CDefaultRule::CDefaultRule(const CString& key, group_t group)
	: CRule	( group )
	, Key	( key )
{
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

		ParseURL( true );
	}
}

HRESULT CDefaultRule::DeleteFrom(ISearchCrawlScopeManager* pScope) const
{
	return CRule::DeleteFrom( pScope );
}
