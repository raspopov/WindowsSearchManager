//
// SearchManagerDlg.h
//
// Search Manager - shows Windows Search internals.
// Copyright (c) Nikolay Raspopov, 2012-2015.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#pragma once

class CSearchManagerDlg : public CDialog
{
public:
	CSearchManagerDlg(CWnd* pParent = NULL);

	enum { IDD = IDD_SEARCHMANAGER_DIALOG };

protected:
	HICON m_hIcon;
	CStatic m_wndStatus;
	CStatic m_wndName;
	CStatic m_wndIndex;
	CListBox m_wndRoots;
	CListBox m_wndRules;
	CString m_sNameCache;
	CString m_sStatusCache;
	CString m_sURLCache;
	CList< CString > m_pRootURLsCache;
	CList< CString > m_pRuleURLsCache;
	CComPtr< ISearchManager > m_pManager;

	void ConnectToWDS();

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnNMClickSysindex(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()
};
