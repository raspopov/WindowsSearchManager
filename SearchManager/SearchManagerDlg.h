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

#include "Item.h"

class CLock
{
public:
	inline CLock(bool* lock) noexcept
		: m_p		( lock )
		, m_locked	( false )
	{
	}

	inline bool Lock() noexcept
	{
		if ( *m_p )
		{
			return false;
		}
		else
		{
			*m_p = m_locked = true;
			return true;
		}
	}

	inline ~CLock() noexcept
	{
		if ( m_locked )
		{
			*m_p = false;
		}
	}

private:
	bool* m_p;
	bool  m_locked;
};


class CSearchManagerDlg : public CDialog
{
public:
	CSearchManagerDlg(CWnd* pParent = nullptr);

	enum { IDD = IDD_SEARCHMANAGER_DIALOG };

	// Pass key down event from application to dialog
	BOOL OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

protected:
	HICON m_hIcon;
	CStatic m_wndStatus;
	CStatic m_wndName;
	CStatic m_wndIndex;
	CListCtrl m_wndList;

	CString m_sStatusCache;
	CString m_sIndexCache;
	bool m_bInUse;
	bool m_bRefresh;

	CComPtr< ISearchManager > m_pManager;
	CComPtr< ISearchCatalogManager > m_pCatalog;
	CComPtr< ISearchCrawlScopeManager > m_pScope;

	void Disconnect();
	HRESULT EnumerateRoots(ISearchCrawlScopeManager* pScope);
	HRESULT EnumerateScopeRules(ISearchCrawlScopeManager* pScope);

	void SetStatus(UINT nStatus);
	void SetStatus(const CString& sStatus);
	void SetIndex(UINT nIndex);
	void SetIndex(const CString& sIndex);

	// Update interface
	void OnUpdate();

	// Delete key pressed
	void OnDelete();

	BOOL OnInitDialog() override;
	void DoDataExchange(CDataExchange* pDX) override;
	void OnOK() override;

	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnNMClickSysindex(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnDeleteitemList( NMHDR *pNMHDR, LRESULT *pResult );

	DECLARE_MESSAGE_MAP()
};
