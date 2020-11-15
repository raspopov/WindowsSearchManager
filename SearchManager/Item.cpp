// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
This file is part of Search Manager - shows Windows Search internals.

Copyright (C) 2012-2020 Nikolay Raspopov <raspopov@cherubicsoft.com>

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
#include "Item.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CRoot

CRoot::CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot)
	: IncludedInCrawlScope	( FALSE )
	, IsHierarchical		( FALSE )
	, ProvidesNotifications	( FALSE )
{
	LPWSTR szURL = nullptr;
	HRESULT hr = pRoot->get_RootURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		RootURL = szURL;

		hr = pScope->IncludedInCrawlScope( szURL, &IncludedInCrawlScope );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_IsHierarchical( &IsHierarchical );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_ProvidesNotifications( &ProvidesNotifications );
		ASSERT( SUCCEEDED( hr ) );

		CoTaskMemFree( szURL );
	}
}

void CRoot::InsertTo(CListCtrl& list)
{
	LVITEM item = { LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM };
	item.iItem = list.GetItemCount();
	item.iGroupId = GROUP_ID_ROOTS;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( RootURL ) );
	item.lParam = reinterpret_cast< LPARAM >( static_cast< CItem* >( this ) );
	item.iItem = list.InsertItem( &item );
	item.mask = LVIF_TEXT;

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IncludedInCrawlScope ? _T("In Scope") : _T("") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IsHierarchical ? _T("Hierarchical") : _T("") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( ProvidesNotifications ? _T("Notify") : _T("") );
	list.SetItem( &item );
}

bool CRoot::Delete(ISearchCrawlScopeManager* pScope)
{
	HRESULT hr = pScope->RemoveRoot( RootURL );
	return  SUCCEEDED( hr );
}

// CRule

CRule::CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule)
	: IsIncluded			( FALSE )
	, IsDefault				( FALSE )
{
	UNUSED_ALWAYS( pScope );

	LPWSTR szURL = nullptr;
	HRESULT hr = pRule->get_PatternOrURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		PatternOrURL = szURL;

		hr = pRule->get_IsIncluded( &IsIncluded );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRule->get_IsDefault( &IsDefault );
		ASSERT( SUCCEEDED( hr ) );

		CoTaskMemFree( szURL );
	}
}

void CRule::InsertTo(CListCtrl& list)
{
	LVITEM item = { LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM };
	item.iItem = list.GetItemCount();
	item.iGroupId = GROUP_ID_RULES;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( PatternOrURL ) );
	item.lParam = reinterpret_cast< LPARAM >( static_cast< CItem* >( this ) );
	item.iItem = list.InsertItem( &item );
	item.mask = LVIF_TEXT;

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IsIncluded ? _T("Included") : _T("Excluded") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IsDefault ? _T("Default") : _T("") );
	list.SetItem( &item );
}

bool CRule::Delete(ISearchCrawlScopeManager* pScope)
{
	const HRESULT hr = IsDefault ? pScope->RemoveDefaultScopeRule( PatternOrURL ) : pScope->RemoveScopeRule( PatternOrURL );
	return SUCCEEDED( hr );
}
