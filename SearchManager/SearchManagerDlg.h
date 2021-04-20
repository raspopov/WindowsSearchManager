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
	CStatic			m_wndLastIndexed;
	CListCtrl		m_wndList;
	CMFCMenuButton	m_btnReindex;
	CMFCMenuButton	m_btnService;
	CMFCMenuButton	m_btnAdd;

	CString			m_sStatusCache;
	CString			m_sIndexCache;
	bool			m_bInUse;
	bool			m_bRefresh;
	CString			m_sUserAgent;
	CString			m_sVersion;

	CComPtr< ISearchCatalogManager >	m_pCatalog;
	CComPtr< ISearchCrawlScopeManager >	m_pScope;

	std::list< CItem* >	m_List;
	std::map< CString, int > m_Groups;
	DWORD			m_nDrives;
	CString			m_sModulePath;

	// Get list group by name and GUID creating a new one if missed
	int GetGroupId(const CString& name, REFGUID guid = GUID(), const CString& info = CString());

	// Update interface items
	void UpdateInterface();

	BOOL IsAddEnabled() const;
	BOOL IsEditEnabled() const;
	BOOL IsDeleteEnabled() const;
	BOOL IsReindexEnabled() const;

	// Resize interface items including list columns sizes
	void ReSize();

	void Disconnect();

	// Clear all loaded data
	void Clear();

	// Refresh all data
	void Refresh();

	// List sorting callback function
	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

	// Enumerate search roots from the Windows Search
	HRESULT EnumerateRoots(ISearchCrawlScopeManager* pScope);

	// Enumerate search scope rules from the Windows Search
	HRESULT EnumerateScopeRules(ISearchCrawlScopeManager* pScope);

	// Enumerate search roots from the registry
	HRESULT EnumerateRegistryRoots();

	// Enumerate search scope rules from the registry
	HRESULT EnumerateRegistryDefaultRules();
	HRESULT EnumerateRegistry(HKEY hKey, LPCTSTR szSubkey, group_t nGroup, const CString& sGroupName);

	// Enumerate search volumes
	void EnumerateVolumes();

	void SetStatus(const CString& sStatus);

	void SetIndex(const CString& sIndex);

	// Add new or edit existing search root
	void AddRoot(const CString& sURL = CString());

	// Add new or edit existing search scope rule
	void AddRule(BOOL bInclude = TRUE, BOOL bDefault = TRUE, const CString& sURL = CString());

	// Delete selected item(s)
	void Delete();

	// Re-index selected item(s)
	void Reindex();

	// Re-index all item(s)
	void ReindexAll();

	// Reset whole index
	void Reset();

	// Rebuild whole index (removing folder)
	void Rebuild();

	// Reset to defaults (remove user scopes)
	void Default(bool bInteractive);

	// Index defragmentation
	void Defrag();

	// Index checking
	void Check();

	void DatabaseOperation(const CString& prompt, const CString& status, const CString& options);

	// Run Explorer for index folder
	void Explore();

	bool StopWindowsSearch(bool& bWasStarted);
	bool StartWindowsSearch();

	void Edit(const CItem* item);

	BOOL OnInitDialog() override;
	void DoDataExchange(CDataExchange* pDX) override;
	void OnOK() override;

	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnNMClickSysindex(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnDeleteitemList( NMHDR *pNMHDR, LRESULT *pResult );
	afx_msg void OnBnClickedIndex();
	afx_msg void OnBnClickedService();
	afx_msg void OnBnClickedAdd();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnLvnItemchangedList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMDblclkList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedEdit();
	afx_msg void OnNMRClickList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnCopy();
	afx_msg void OnDelete();
	afx_msg void OnUpdateDelete(CCmdUI *pCmdUI);
	afx_msg void OnEdit();
	afx_msg void OnUpdateEdit(CCmdUI *pCmdUI);
	afx_msg void OnReindex();
	afx_msg void OnUpdateReindex(CCmdUI *pCmdUI);
	afx_msg void OnNMCustomdrawList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnDeviceChange(UINT nEventType, DWORD_PTR dwData);
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);

	DECLARE_MESSAGE_MAP()
};
