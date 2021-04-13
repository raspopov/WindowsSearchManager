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

#pragma once

enum group_t { GROUP_ROOTS, GROUP_OFFLINE_ROOTS, GROUP_RULES, GROUP_DEFAULT_RULES };

class CItem
{
public:
	CItem(group_t group) noexcept;
	CItem(const CString& url, group_t group) noexcept;
	virtual ~CItem() = default;

	virtual int InsertTo(CListCtrl& list, int group_id) const;
	virtual HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const = 0;
	virtual HRESULT AddTo(ISearchCrawlScopeManager* pScope) const = 0;
	virtual HRESULT Reindex(ISearchCatalogManager* pCatalog) const = 0;

	operator bool() const noexcept
	{
		return ! URL.IsEmpty();
	}

	inline bool operator==(const CItem& item) const noexcept
	{
		return IsEqualGUID( item.Guid, Guid ) && ( item.URL == URL );
	}

	virtual CString GetTitle() const;

	group_t	Group;				// Group type: root or rule
	CString URL;				// The pattern or URL for the rule, or the URL of the starting point for search root
	CString Protocol;
	CString Name;
	CString User;
	CString Path;
	GUID Guid;

protected:
	void ParseURL(bool bGuid);
};

class CRoot : public CItem
{
public:
	CRoot(group_t group = GROUP_ROOTS);
	CRoot(BOOL notif, const CString& url, group_t group = GROUP_ROOTS);
	CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot, group_t group = GROUP_ROOTS);

	int InsertTo(CListCtrl& list, int group_id) const override;
	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const override;
	HRESULT AddTo(ISearchCrawlScopeManager* pScope) const override;
	HRESULT Reindex(ISearchCatalogManager* pCatalog) const override;

	BOOL IncludedInCrawlScope;	// An indicator of whether the specified URL is included in the crawl scope
	BOOL IsHierarchical;		// Indicates whether the search is rooted on a hierarchical tree structure
	BOOL ProvidesNotifications;	// Indicates whether the search engine is notified (by protocol handlers or other applications) about changes to the URLs under the search root
	BOOL UseNotificationsOnly;	// Indicates whether this search root should be indexed only by notification and not crawled
};

class COfflineRoot : public CRoot
{
public:
	COfflineRoot(const CString& key, group_t group = GROUP_OFFLINE_ROOTS);
};

class CRule : public CItem
{
public:
	CRule(group_t group = GROUP_RULES);
	CRule(BOOL incl, BOOL def, const CString& url, group_t group = GROUP_RULES);
	CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule, group_t group = GROUP_RULES);

	int InsertTo(CListCtrl& list, int group_id) const override;
	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const override;
	HRESULT AddTo(ISearchCrawlScopeManager* pScope) const override;
	HRESULT Reindex(ISearchCatalogManager* pCatalog) const override;

	BOOL IsInclude;				// A value identifying whether this rule is an inclusion rule
	BOOL IsDefault;				// Identifies whether this is a default rule
	BOOL HasChild;				// Identifies whether a given URL has a child rule in scope
};

class CDefaultRule : public CRule
{
public:
	CDefaultRule(const CString& key, group_t group = GROUP_DEFAULT_RULES);
};
