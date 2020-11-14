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

class CRoot
{
public:
	inline CRoot(LPCWSTR szRootURL)
		: RootURL				( szRootURL )
		, IncludedInCrawlScope	( FALSE )
		, IsHierarchical		( FALSE )
		, ProvidesNotifications	( FALSE )
	{
	}

	void InsertTo(CListCtrl& list);

	CString RootURL;			// The URL of the starting point for this search root
	BOOL IncludedInCrawlScope;	// An indicator of whether the specified URL is included in the crawl scope
	BOOL IsHierarchical;		// Indicates whether the search is rooted on a hierarchical tree structure
	BOOL ProvidesNotifications;	// Indicates whether the search engine is notified (by protocol handlers or other applications) about changes to the URLs under the search root
};

class CRule
{
public:
	inline CRule(LPCWSTR szPatternOrURL)
		: PatternOrURL			( szPatternOrURL )
		, IsIncluded			( FALSE )
		, IsDefault				( FALSE )
	{
	}

	void InsertTo(CListCtrl& list);

	CString PatternOrURL;		// The pattern or URL for the rule
	BOOL IsIncluded;			// A value identifying whether this rule is an inclusion rule
	BOOL IsDefault;				// Identifies whether this is a default rule
};

class CSearchManagerDlg : public CDialog
{
public:
	CSearchManagerDlg(CWnd* pParent = nullptr);

	enum { IDD = IDD_SEARCHMANAGER_DIALOG };

protected:
	HICON m_hIcon;
	CStatic m_wndStatus;
	CStatic m_wndName;
	CStatic m_wndIndex;
	CListCtrl m_wndList;

	CString m_sStatusCache;
	CString m_sURLCache;
	bool m_bRefresh;
	CComPtr< ISearchManager > m_pManager;

	void ConnectToWDS();
	HRESULT EnumerateRoots(ISearchCrawlScopeManager* pScope);
	HRESULT EnumerateScopeRules(ISearchCrawlScopeManager* pScope);

	BOOL OnInitDialog() override;
	void DoDataExchange(CDataExchange* pDX) override;
	void OnOK() override;

	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnNMClickSysindex(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()
};
