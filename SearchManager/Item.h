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

enum group_t { GROUP_NONE, GROUP_ROOTS, GROUP_OFFLINE_ROOTS, GROUP_RULES, GROUP_OFFLINE_RULES, GROUP_VOLUMES };

class CItem
{
public:
	CItem(group_t group = GROUP_NONE) noexcept;
	CItem(const CString& url, group_t group = GROUP_NONE) noexcept;
	virtual ~CItem() = default;

	// Get item title for name column of list
	virtual CString GetTitle() const;

	// Get item color for list
	virtual COLORREF GetColor() const;

	virtual bool Parse();

	inline bool HasError() const noexcept
	{
		return ! m_Error.IsEmpty();
	}

	virtual bool operator!=(const CItem& item) const noexcept
	{
		return ! operator==( item );
	}

	virtual bool operator==(const CItem& item) const noexcept
	{
		return IsEqualGUID( item.Guid, Guid ) && ( item.NormalURL == NormalURL );
	}

	virtual bool HasNonIndexDelete() const noexcept
	{
		return false;
	}

	virtual int InsertTo(CListCtrl& list, int group_id) const;

	virtual HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const
	{
		UNUSED_ALWAYS( pScope );
		return E_NOTIMPL;
	}

	virtual HRESULT AddTo(ISearchCrawlScopeManager* pScope) const
	{
		UNUSED_ALWAYS( pScope );
		return E_NOTIMPL;
	}

	virtual HRESULT Reindex(ISearchCatalogManager* pCatalog) const
	{
		UNUSED_ALWAYS( pCatalog );
		return E_NOTIMPL;
	}

	inline void SetError(const CString& error)
	{
		m_Error = error;
	}

	group_t	Group;				// Group type
	CString URL;				// The pattern or URL for the rule, or the URL of the starting point for search root
	CString NormalURL;			// Normalized URL
	CString Protocol;			// Protocol name ("file", "iehistory", etc.)
	CString Name;				// Protocol handler name
	CString User;				// User name (SID) if any
	CString Path;				// Search root/scope path
	GUID Guid;					// Search volume GUID for "file" protocol

	static HRESULT ReadVolumeGuid(TCHAR disk, GUID& guid);

protected:
	bool m_ParseGuid;			// Is GUID parsed?
	CString m_Error;			// Has some errors
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

	bool HasNonIndexDelete() const noexcept override
	{
		return true;
	}

	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const override;

	CString Key;				// Registry key
};

class CRule : public CItem
{
public:
	CRule(group_t group = GROUP_RULES);
	CRule(BOOL incl, BOOL def, const CString& url, group_t group = GROUP_RULES);
	CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule, group_t group = GROUP_RULES);

	COLORREF GetColor() const override;

	int InsertTo(CListCtrl& list, int group_id) const override;
	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const override;
	HRESULT AddTo(ISearchCrawlScopeManager* pScope) const override;
	HRESULT Reindex(ISearchCatalogManager* pCatalog) const override;

	BOOL IsInclude;				// A value identifying whether this rule is an inclusion rule
	BOOL IsDefault;				// Identifies whether this is a default rule
	BOOL HasChild;				// Identifies whether a given URL has a child rule in scope
};

class COfflineRule : public CRule
{
public:
	COfflineRule(const CString& key, group_t group = GROUP_OFFLINE_RULES);

	bool HasNonIndexDelete() const noexcept override
	{
		return true;
	}

	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const override;

	CString Key;				// Registry key
};

class CVolume : public CItem
{
public:
	CVolume(TCHAR disk, group_t group = GROUP_VOLUMES);

	CString GetTitle() const override;

	COLORREF GetColor() const override;

	bool Parse() override;

	bool operator==(const CItem& item) const noexcept override
	{
		return ! IsEqualGUID( Guid, GUID() ) && IsEqualGUID( item.Guid, Guid );
	}

	bool HasNonIndexDelete() const noexcept override
	{
		return true;
	}

	HRESULT DeleteFrom(ISearchCrawlScopeManager* pScope) const override;
	HRESULT Reindex(ISearchCatalogManager* pCatalog) const override;

	BOOL HasDuplicateDUID;		// Duplicate GUID detected
};
