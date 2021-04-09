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

#include "Item.h"
#include "DialogExSized.h"

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


class CSearchManagerDlg : public CDialogExSized
{
	DECLARE_DYNAMIC(CSearchManagerDlg)

public:
	CSearchManagerDlg(CWnd* pParent = nullptr);
	virtual ~CSearchManagerDlg() = default;

	enum { IDD = IDD_SEARCHMANAGER_DIALOG };

	// Pass key down event from application to dialog
	BOOL OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

protected:
	HICON			m_hIcon;
	CStatic			m_wndStatus;
	CStatic			m_wndName;
	CStatic			m_wndIndex;
	CListCtrl		m_wndList;
	CMFCMenuButton	m_btnAdd;

	CString			m_sStatusCache;
	CString			m_sIndexCache;
	bool			m_bInUse;
	bool			m_bRefresh;

	CComPtr< ISearchCatalogManager >	m_pCatalog;
	CComPtr< ISearchCrawlScopeManager >	m_pScope;

	// Update interface items
	void UpdateInterface();

	void Disconnect();

	HRESULT EnumerateRoots(ISearchCrawlScopeManager* pScope);
	HRESULT EnumerateScopeRules(ISearchCrawlScopeManager* pScope);

	inline void SetStatus(UINT nStatus)
	{
		SetStatus( LoadString( nStatus ) );
	}
	void SetStatus(const CString& sStatus);

	inline void SetIndex(UINT nIndex)
	{
		SetIndex( LoadString( nIndex ) );
	}
	void SetIndex(const CString& sIndex);

	// Update interface
	void Update();

	// Add new item
	void Add(BOOL bInclude, BOOL bDefault);

	// Delete selected item(s)
	void Delete();

	// Edit selected item
	void Edit(CRule* item);

	BOOL OnInitDialog() override;
	void DoDataExchange(CDataExchange* pDX) override;
	void OnOK() override;

	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnNMClickSysindex(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnDeleteitemList( NMHDR *pNMHDR, LRESULT *pResult );
	afx_msg void OnBnClickedAdd();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedReindex();
	afx_msg void OnBnClickedReset();
	afx_msg void OnBnClickedDefault();
	afx_msg void OnLvnItemchangedList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMDblclkList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedEdit();
	afx_msg void OnNMRClickList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnCopy();
	afx_msg void OnDelete();
	afx_msg void OnUpdateDelete(CCmdUI *pCmdUI);
	afx_msg void OnEdit();
	afx_msg void OnUpdateEdit(CCmdUI *pCmdUI);
	afx_msg void OnNMCustomdrawList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
};
