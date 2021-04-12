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

#define DEFAULT_PROTOCOL	_T("defaultroot")
#define FILE_PROTOCOL		_T("file")

enum group_t { GROUP_ROOTS, GROUP_OFFLINE_ROOTS, GROUP_RULES, GROUP_DEFAULT_RULES };

inline bool IsDeletable(group_t group)
{
	return ( group == GROUP_ROOTS || group == GROUP_OFFLINE_ROOTS || group == GROUP_RULES || group == GROUP_DEFAULT_RULES );
}

class CItem
{
public:
	CItem(group_t group) noexcept;
	CItem(LPCTSTR url, group_t group);
	virtual ~CItem() = default;

	virtual void ParseURL();
	virtual int InsertTo(CListCtrl& list, int group_id);
	virtual HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) = 0;
	virtual HRESULT AddTo(ISearchCrawlScopeManager* pScope) = 0;

	operator bool() const noexcept;

	virtual CString GetTitle() const;

	group_t	Group;				// Group type: root or rule
	CString URL;				// The pattern or URL for the rule, or the URL of the starting point for search root
	CString Protocol;
	CString Name;
	CString User;
	CString Path;
	CString Guid;
};

class CRoot : public CItem
{
public:
	CRoot(BOOL notif, LPCTSTR url, group_t group = GROUP_ROOTS);
	CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot);

	int InsertTo(CListCtrl& list, int group_id) override;
	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) override;
	HRESULT AddTo(ISearchCrawlScopeManager* pScope) override;

	BOOL IncludedInCrawlScope;	// An indicator of whether the specified URL is included in the crawl scope
	BOOL IsHierarchical;		// Indicates whether the search is rooted on a hierarchical tree structure
	BOOL ProvidesNotifications;	// Indicates whether the search engine is notified (by protocol handlers or other applications) about changes to the URLs under the search root
	BOOL UseNotificationsOnly;	// Indicates whether this search root should be indexed only by notification and not crawled
};

class COfflineRoot : public CRoot
{
public:
	COfflineRoot(BOOL notif, LPCTSTR url, group_t group = GROUP_OFFLINE_ROOTS);
};

class CRule : public CItem
{
public:
	CRule(BOOL incl, BOOL def, LPCTSTR url, group_t group = GROUP_RULES);
	CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule);

	int InsertTo(CListCtrl& list, int group_id) override;
	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) override;
	HRESULT AddTo(ISearchCrawlScopeManager* pScope) override;

	BOOL IsInclude;				// A value identifying whether this rule is an inclusion rule
	BOOL IsDefault;				// Identifies whether this is a default rule
	BOOL HasChild;				// Identifies whether a given URL has a child rule in scope
};

class CDefaultRule : public CRule
{
public:
	CDefaultRule(BOOL incl, BOOL def, LPCTSTR url, group_t group = GROUP_DEFAULT_RULES);
};
