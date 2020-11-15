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

#pragma once

enum { GROUP_ID_ROOTS, GROUP_ID_RULES };

class CItem
{
public:
	virtual ~CItem() {}
	virtual operator bool() const noexcept = 0;
	virtual void InsertTo(CListCtrl& list) = 0;
	virtual bool Delete(ISearchCrawlScopeManager* pScope) = 0;
};

class CRoot : public CItem
{
public:
	CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot);

	operator bool() const noexcept override
	{
		return ( RootURL.IsEmpty() == FALSE );
	}

	void InsertTo(CListCtrl& list) override;
	bool Delete(ISearchCrawlScopeManager* pScope) override;

	CString RootURL;			// The URL of the starting point for this search root
	BOOL IncludedInCrawlScope;	// An indicator of whether the specified URL is included in the crawl scope
	BOOL IsHierarchical;		// Indicates whether the search is rooted on a hierarchical tree structure
	BOOL ProvidesNotifications;	// Indicates whether the search engine is notified (by protocol handlers or other applications) about changes to the URLs under the search root
};

class CRule : public CItem
{
public:
	CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule);

	operator bool() const noexcept override
	{
		return ( PatternOrURL.IsEmpty() == FALSE );
	}

	void InsertTo(CListCtrl& list) override;
	bool Delete(ISearchCrawlScopeManager* pScope) override;

	CString PatternOrURL;		// The pattern or URL for the rule
	BOOL IsIncluded;			// A value identifying whether this rule is an inclusion rule
	BOOL IsDefault;				// Identifies whether this is a default rule
};
