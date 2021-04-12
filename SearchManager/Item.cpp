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
{
}

CItem::CItem(LPCTSTR url, group_t group)
	: Group	( group )
	, URL	( url )
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
		return User + _T(" ") + Path;
	}
	else
	{
		return Name + _T(" (") + User + _T(") ") + Path;
	}
}

void CItem::ParseURL()
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
			if ( Path.GetLength() >= 42 && Path.GetAt( 3 ) == _T('[') && Path.GetAt( 40 ) == _T(']') )
			{
				Guid = Path.Mid( 4, 36 );
				Path = Path.Left( 3 ) + Path.Mid( 42 );
				URL = Protocol + _T(":///") + Path;
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
					LPOLESTR str_clsid = nullptr;
					if ( SUCCEEDED( StringFromCLSID( clsid, &str_clsid ) ) )
					{
						CString def_key;
						def_key.Format( _T("CLSID\\%s"), str_clsid );

						TCHAR def[ MAX_PATH ] = {};
						DWORD type, def_size = sizeof( def );
						LSTATUS res = RegQueryValueFull( HKEY_CLASSES_ROOT, def_key, _T(""), &type, reinterpret_cast< LPBYTE >( def ), &def_size );
						if ( res == ERROR_SUCCESS )
						{
							Name = def;
						}
						CoTaskMemFree( str_clsid );
					}
				}
			}
		}
	}
}

int CItem::InsertTo(CListCtrl& list, int group_id)
{
	const CString title = GetTitle();

	LVITEM item = { LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM };
	item.iGroupId = group_id;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( Protocol ) );
	item.lParam = reinterpret_cast< LPARAM >( this );
	item.iItem = list.InsertItem( &item );

	item.iSubItem = 1;
	item.mask = LVIF_TEXT;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( title ) );
	list.SetItem( &item );

	return item.iItem;
}

CItem::operator bool() const noexcept
{
	return ! URL.IsEmpty();
}

// CRoot

CRoot::CRoot(BOOL notif, LPCTSTR url, group_t group)
	: CItem					( url, group )
	, IncludedInCrawlScope	( FALSE )
	, IsHierarchical		( FALSE )
	, ProvidesNotifications	( notif )
	, UseNotificationsOnly	( FALSE )
{
}

CRoot::CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot)
	: CItem					( GROUP_ROOTS )
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

		ParseURL();

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

int CRoot::InsertTo(CListCtrl& list, int group_id)
{
	LVITEM item = { LVIF_TEXT };
	item.iItem = CItem::InsertTo( list, group_id );

	item.iSubItem = 2;
	item.pszText = const_cast< LPTSTR >( IncludedInCrawlScope ? _T("In Scope") : _T("") );
	list.SetItem( &item );

	item.iSubItem = 3;
	item.pszText = const_cast< LPTSTR >( IsHierarchical ? _T("Hierarchical") : _T("") );
	list.SetItem( &item );

	item.iSubItem = 4;
	item.pszText = const_cast< LPTSTR >( ProvidesNotifications ? ( UseNotificationsOnly ? _T("Notify Only") : _T("Notify") ): _T("") );
	list.SetItem( &item );

	return item.iItem;
}

HRESULT CRoot::DeleteFrom(ISearchCrawlScopeManager* pScope)
{
	return pScope->RemoveRoot( URL );
}

HRESULT CRoot::AddTo(ISearchCrawlScopeManager* pScope )
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

// COfflineRoot

COfflineRoot::COfflineRoot(BOOL notif, LPCTSTR url, group_t group)
	: CRoot		( notif, url, group )
{
}

// CRule

CRule::CRule(BOOL incl, BOOL def, LPCTSTR url, group_t group)
	: CItem		( url, group )
	, IsInclude	( incl )
	, IsDefault	( def )
	, HasChild	( FALSE )
{
}

CRule::CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule)
	: CItem					( GROUP_RULES )
	, IsInclude				( FALSE )
	, IsDefault				( FALSE )
	, HasChild				( FALSE )
{
	LPWSTR szURL = nullptr;
	HRESULT hr = pRule->get_PatternOrURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		URL = szURL;
		CoTaskMemFree( szURL );

		ParseURL();

		hr = pRule->get_IsIncluded( &IsInclude );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRule->get_IsDefault( &IsDefault );
		ASSERT( SUCCEEDED( hr ) );

		hr = pScope->HasChildScopeRule( URL, &HasChild );
	}
}

int CRule::InsertTo(CListCtrl& list, int group_id)
{
	LVITEM item = { LVIF_TEXT };
	item.iItem = CItem::InsertTo( list, group_id );

	item.iSubItem = 2;
	item.pszText = const_cast< LPTSTR >( IsInclude ? _T("Include") : _T("Exclude") );
	list.SetItem( &item );

	item.iSubItem = 3;
	item.pszText = const_cast< LPTSTR >( IsDefault ? _T("Default") : _T("User") );
	list.SetItem( &item );

	item.iSubItem = 4;
	item.pszText = const_cast< LPTSTR >( HasChild ? _T("Has child") : _T("") );
	list.SetItem( &item );

	return item.iItem;
}

HRESULT CRule::DeleteFrom(ISearchCrawlScopeManager* pScope)
{
	return IsDefault ? pScope->RemoveDefaultScopeRule( URL ) : pScope->RemoveScopeRule( URL );
}

HRESULT CRule::AddTo(ISearchCrawlScopeManager* pScope )
{
	return IsDefault ? pScope->AddDefaultScopeRule( URL, IsInclude, FF_INDEXCOMPLEXURLS ) :
		pScope->AddUserScopeRule( URL, IsInclude, FALSE, FF_INDEXCOMPLEXURLS );
}

// CDefaultRule

CDefaultRule::CDefaultRule(BOOL incl, BOOL def, LPCTSTR url, group_t group)
	: CRule	( incl, def, url, group )
{
}
