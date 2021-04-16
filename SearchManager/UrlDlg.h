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

// CBrowseCtrl

class CBrowseCtrl : public CMFCEditBrowseCtrl
{
	DECLARE_DYNAMIC(CBrowseCtrl)

public:
	virtual ~CBrowseCtrl() = default;

protected:
	void OnAfterUpdate() override;

	DECLARE_MESSAGE_MAP()
};

// CUrlDialog dialog

class CUrlDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CUrlDialog)

public:
	CUrlDialog(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CUrlDialog() = default;

	enum { IDD = IDD_URL_DIALOG };

	CString	m_sTitle;
	CString m_sURL;
	CString m_sInfo;

	// Check if URL has no data
	bool IsEmpty() const noexcept;

protected:
	CBrowseCtrl m_wndUrl;
	CMFCMenuButton m_btnInsert;

	std::vector< CString > m_Protocols;
	std::vector< CString > m_Users;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedInsertUrl();

	DECLARE_MESSAGE_MAP()
};
