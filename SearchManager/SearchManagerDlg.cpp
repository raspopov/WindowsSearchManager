// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

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

#include "stdafx.h"
#include "SearchManager.h"
#include "SearchManagerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define COLUMN_SIZE		80

IMPLEMENT_DYNAMIC(CSearchManagerDlg, CDialogExSized)

CSearchManagerDlg::CSearchManagerDlg(CWnd* pParent /*=NULL*/)
	: CDialogExSized		( CSearchManagerDlg::IDD, pParent )
	, m_bRefresh	( true )
	, m_bInUse		( false )
{
	m_hIcon = AfxGetApp()->LoadIcon( IDR_MAINFRAME );
}

void CSearchManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogExSized::DoDataExchange( pDX );

	DDX_Control(pDX, IDC_STATUS, m_wndStatus);
	DDX_Control(pDX, IDC_NAME, m_wndName);
	DDX_Control(pDX, IDC_INDEX, m_wndIndex);
	DDX_Control(pDX, IDC_LIST, m_wndList);
	DDX_Control(pDX, IDC_REINDEX, m_btnReindex);
	DDX_Control(pDX, IDC_ADD, m_btnAdd);
}

BEGIN_MESSAGE_MAP(CSearchManagerDlg, CDialogExSized)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_NOTIFY(NM_CLICK, IDC_SYSINDEX, &CSearchManagerDlg::OnNMClickSysindex)
	ON_NOTIFY(LVN_DELETEITEM, IDC_LIST, &CSearchManagerDlg::OnLvnDeleteitemList)
	ON_BN_CLICKED(IDC_REINDEX, &CSearchManagerDlg::OnBnClickedReindex)
	ON_BN_CLICKED(IDC_ADD, &CSearchManagerDlg::OnBnClickedAdd)
	ON_BN_CLICKED(IDC_DELETE, &CSearchManagerDlg::OnBnClickedDelete)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST, &CSearchManagerDlg::OnLvnItemchangedList)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST, &CSearchManagerDlg::OnNMDblclkList)
	ON_BN_CLICKED(IDC_EDIT, &CSearchManagerDlg::OnBnClickedEdit)
	ON_NOTIFY(NM_RCLICK, IDC_LIST, &CSearchManagerDlg::OnNMRClickList)
	ON_COMMAND(ID_COPY, &CSearchManagerDlg::OnCopy)
	ON_COMMAND(ID_DELETE, &CSearchManagerDlg::OnDelete)
	ON_COMMAND(ID_EDIT, &CSearchManagerDlg::OnEdit)
	ON_UPDATE_COMMAND_UI(ID_EDIT, &CSearchManagerDlg::OnUpdateEdit)
	ON_UPDATE_COMMAND_UI(ID_DELETE, &CSearchManagerDlg::OnUpdateDelete)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST, &CSearchManagerDlg::OnNMCustomdrawList)
	ON_WM_SIZE()
	ON_COMMAND(ID_REINDEX, &CSearchManagerDlg::OnReindex)
	ON_UPDATE_COMMAND_UI(ID_REINDEX, &CSearchManagerDlg::OnUpdateReindex)
END_MESSAGE_MAP()

BOOL CSearchManagerDlg::OnInitDialog()
{
	CDialogExSized::OnInitDialog();

	SetWindowText( LoadString( AFX_IDS_APP_TITLE ) );

	SetIcon( m_hIcon, TRUE );
	SetIcon( m_hIcon, FALSE );

	RestoreWindowPlacement();

	CRect rc;
	m_wndList.GetWindowRect( &rc );

	m_wndList.SetExtendedStyle( m_wndList.GetExtendedStyle() | LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );

	m_wndList.EnableGroupView( TRUE );

	m_wndList.InsertColumn( 0, _T(""), LVCFMT_LEFT, COLUMN_SIZE );
	m_wndList.InsertColumn( 1, _T(""), LVCFMT_LEFT, COLUMN_SIZE );
	m_wndList.InsertColumn( 2, _T(""), LVCFMT_CENTER, COLUMN_SIZE );
	m_wndList.InsertColumn( 3, _T(""), LVCFMT_CENTER, COLUMN_SIZE );
	m_wndList.InsertColumn( 4, _T(""), LVCFMT_CENTER, COLUMN_SIZE );

	if ( auto menu = theApp.GetContextMenuManager() )
	{
		MENUITEMINFO mi = { sizeof( MENUITEMINFO ), MIIM_STATE, 0, MFS_DEFAULT };

		menu->AddMenu( _T("IDR_ADD_MENU"), IDR_ADD_MENU );
		menu->AddMenu( _T("IDR_LIST_MENU"), IDR_LIST_MENU );
		menu->AddMenu( _T("IDR_REINDEX_MENU"), IDR_REINDEX_MENU );

		m_btnReindex.m_bOSMenu = FALSE;
		SetMenuItemInfo( m_btnReindex.m_hMenu = GetSubMenu( menu->GetMenuById( IDR_REINDEX_MENU ), 0 ), 0, TRUE, &mi );

		m_btnAdd.m_bOSMenu = FALSE;
		SetMenuItemInfo( m_btnAdd.m_hMenu = GetSubMenu( menu->GetMenuById( IDR_ADD_MENU ), 0 ), 0, TRUE, &mi );
	}

	ReSize();

	UpdateInterface();

	SetTimer( 1, 200, nullptr );

	return TRUE;
}

void CSearchManagerDlg::OnPaint()
{
	if ( IsIconic() )
	{
		CPaintDC dc( this );
		SendMessage( WM_ICONERASEBKGND, reinterpret_cast< WPARAM >( dc.GetSafeHdc() ), 0 );
		const int cxIcon = GetSystemMetrics( SM_CXICON );
		const int cyIcon = GetSystemMetrics( SM_CYICON );
		CRect rect;
		GetClientRect( &rect );
		const int x = ( rect.Width() - cxIcon + 1 ) / 2;
		const int y = ( rect.Height() - cyIcon + 1 ) / 2;
		dc.DrawIcon( x, y, m_hIcon );
	}
	else
	{
		CDialogExSized::OnPaint();
	}
}

HCURSOR CSearchManagerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CSearchManagerDlg::OnTimer(UINT_PTR nIDEvent)
{
	if ( nIDEvent == 1 )
	{
		Refresh();
	}

	MSG msg;
	while ( ::PeekMessage( &msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE | PM_NOYIELD ) );

	CDialogExSized::OnTimer( nIDEvent );
}

void CSearchManagerDlg::OnDestroy()
{
	KillTimer( 1 );

	m_pScope.Release();
	m_pCatalog.Release();

	CDialogExSized::OnDestroy();
}

void CSearchManagerDlg::Disconnect()
{
	m_pScope.Release();
	m_pCatalog.Release();

	UpdateInterface();

	UpdateWindow();
}

void CSearchManagerDlg::UpdateInterface()
{
	const BOOL bCatalog = static_cast< bool >( m_pCatalog );
	const BOOL bScope = static_cast< bool >( m_pScope );
	const auto count = m_wndList.GetSelectedCount();
	const BOOL bSingle = bScope && count == 1;
	const BOOL bSelected = bScope && count > 0;

	GetDlgItem( IDC_REINDEX )->EnableWindow( bCatalog );
	GetDlgItem( IDC_ADD )->EnableWindow( bScope );
	GetDlgItem( IDC_EDIT )->EnableWindow( bSingle );
	GetDlgItem( IDC_DELETE )->EnableWindow( bSelected );
}

void CSearchManagerDlg::SetStatus(const CString& sStatus)
{
	if ( m_sStatusCache != sStatus )
	{
		m_wndStatus.SetWindowText( m_sStatusCache = sStatus );
		m_wndStatus.UpdateWindow();
	}
}

void CSearchManagerDlg::SetIndex(const CString& sIndex)
{
	if ( m_sIndexCache != sIndex )
	{
		m_wndIndex.SetWindowText( m_sIndexCache = sIndex );
		m_wndIndex.UpdateWindow();
	}
}

void CSearchManagerDlg::OnNMClickSysindex(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	CWaitCursor wc;

	CString sSys;
	GetSystemWindowsDirectory( sSys.GetBuffer( MAX_PATH ), MAX_PATH );
	sSys.ReleaseBuffer();
	sSys += _T("\\system32\\");

	const CString sControl = sSys + _T("rundll32.exe");
	SHELLEXECUTEINFO sei = { sizeof( SHELLEXECUTEINFO ), SEE_MASK_DEFAULT, GetSafeHwnd(), nullptr, sControl, _T("shell32.dll,Control_RunDLL srchadmin.dll"), sSys, SW_SHOWDEFAULT };
	VERIFY( ShellExecuteEx( &sei ) );

	SleepEx( 500, FALSE );

	*pResult = 0;
}

void CSearchManagerDlg::OnOK()
{
	m_bRefresh = true;
}

BOOL CSearchManagerDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	UNUSED_ALWAYS( nRepCnt );
	UNUSED_ALWAYS( nFlags );

	const bool bControl = GetKeyState( VK_CONTROL ) < 0;
	const bool bShift = GetKeyState( VK_SHIFT ) < 0;

	switch ( nChar )
	{
	case 'C':
		if ( GetFocus() == static_cast< CWnd*>( &m_wndList ) )
		{
			if ( bControl )
			{
				// Ctrl + C
				OnCopy();
				return TRUE;
			}
		}
		break;

	case VK_INSERT:
		if ( GetFocus() == static_cast< CWnd*>( &m_wndList ) )
		{
			if ( bControl )
			{
				// Ctrl + Insert
				OnCopy();
				return TRUE;
			}
			else if ( bShift )
			{
				// Shift + Insert
			}
			else
			{
				// Insert
				AddRule();
				return TRUE;
			}
		}
		break;

	case VK_DELETE:
		if ( GetFocus() == static_cast< CWnd*>( &m_wndList ) )
		{
			if ( ! bControl && ! bShift )
			{
				// Delete
				Delete();
				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}

void CSearchManagerDlg::OnLvnDeleteitemList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast< LPNMLISTVIEW >( pNMHDR );
	*pResult = 0;

	if ( CItem* item = reinterpret_cast< CItem* >( m_wndList.GetItemData( pNMLV->iItem ) ) )
	{
		m_wndList.SetItemData( pNMLV->iItem, 0 );

		delete item;
	}
}

void CSearchManagerDlg::OnBnClickedDelete()
{
	Delete();
}

void CSearchManagerDlg::OnBnClickedReindex()
{
	switch ( m_btnReindex.m_nMenuResult )
	{
	case ID_REINDEX_ALL:
	default:
		ReindexAll();
		break;

	case ID_RESET:
		Reset();
		break;

	case ID_DEFAULT:
		Default();
		break;

	case ID_EXPLORE:
		Explore();
		break;
	}
}

void CSearchManagerDlg::OnBnClickedAdd()
{
	switch ( m_btnAdd.m_nMenuResult )
	{
	case ID_INCLUDE_USER_SCOPE:
		AddRule( TRUE, FALSE );
		break;

	case ID_EXCLUDE_USER_SCOPE:
		AddRule( FALSE, FALSE );
		break;

	case ID_INCLUDE_DEFAULT_SCOPE:
	default:
		AddRule();
		break;

	case ID_EXCLUDE_DEFAULT_SCOPE:
		AddRule( FALSE );
		break;

	case ID_SEARCHROOT:
		AddRoot();
		break;
	}
}

void CSearchManagerDlg::OnLvnItemchangedList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast< LPNMLISTVIEW >( pNMHDR );
	*pResult = 0;

	UNUSED_ALWAYS( pNMLV );

	UpdateInterface();
}

void CSearchManagerDlg::OnNMDblclkList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>( pNMHDR );
	*pResult = 0;

	if ( pNMItemActivate->iItem >= 0 )
	{
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( pNMItemActivate->iItem ) ) )
		{
			OnEdit( item );
		}
	}
}

void CSearchManagerDlg::OnBnClickedEdit()
{
	OnEdit();
}

void CSearchManagerDlg::OnEdit(const CItem* item)
{
	switch ( item->Group )
	{
	case GROUP_ROOTS:
	case GROUP_OFFLINE_ROOTS:
		AddRoot( item->URL );
		break;

	case GROUP_RULES:
	case GROUP_DEFAULT_RULES:
		AddRule( static_cast< const CRule* >( item )->IsInclude, static_cast< const CRule* >( item )->IsDefault, item->URL );
		break;
	}
}

void CSearchManagerDlg::OnNMRClickList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>( pNMHDR );
	*pResult = 0;

	if ( pNMItemActivate->iItem >= 0 )
	{
		m_wndList.SetFocus();
		m_wndList.SetSelectionMark( pNMItemActivate->iItem );

		CPoint cursor;
		GetCursorPos( &cursor );

		if ( auto menu = theApp.GetContextMenuManager() )
		{
			menu->ShowPopupMenu( IDR_LIST_MENU, cursor.x, cursor.y, this, TRUE );
		}
	}
}

void CSearchManagerDlg::OnCopy()
{
	CString values;

	// Gather text string
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			if ( ! values.IsEmpty() )
			{
				values += CRLF;
			}
			values += item->URL;
		}
	}

	// Publish to the clipboard
	if ( OpenClipboard() )
	{
		if ( EmptyClipboard() )
		{
			const size_t nLen = ( static_cast< size_t >( values.GetLength() ) + 1 ) * sizeof( TCHAR );
			if ( HGLOBAL hGlob = GlobalAlloc( GMEM_FIXED, nLen ) )
			{
				memcpy_s( hGlob, nLen, static_cast< LPCTSTR >( values ), nLen );

				if ( SetClipboardData( CF_UNICODETEXT, hGlob ) == nullptr )
				{
					GlobalFree( hGlob );
				}
			}
		}
		CloseClipboard();
	}
}

void CSearchManagerDlg::OnDelete()
{
	Delete();
}

void CSearchManagerDlg::OnUpdateDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( m_pScope && m_wndList.GetSelectedCount() > 0 );
}

void CSearchManagerDlg::OnEdit()
{
	if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			OnEdit( item );
		}
	}
}

void CSearchManagerDlg::OnUpdateEdit(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( m_pScope && m_wndList.GetSelectedCount() == 1 );
}

void CSearchManagerDlg::OnReindex()
{
	Reindex();
}

void CSearchManagerDlg::OnUpdateReindex(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( m_pCatalog && m_wndList.GetSelectedCount() > 0 );
}

void CSearchManagerDlg::OnNMCustomdrawList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVCUSTOMDRAW pNMCD = reinterpret_cast< LPNMLVCUSTOMDRAW >( pNMHDR );

	*pResult = CDRF_DODEFAULT;

	switch ( pNMCD->nmcd.dwDrawStage )
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;

	case CDDS_ITEMPREPAINT:
		if ( auto item = reinterpret_cast< const CItem* >( pNMCD->nmcd.lItemlParam ) )
		{
			if ( item->Group == GROUP_RULES || item->Group == GROUP_DEFAULT_RULES )
			{
				pNMCD->clrTextBk = static_cast< const CRule* >( item )->IsInclude ? RGB( 192, 255, 192 ) : RGB( 255, 192, 192 );
			}
		}
		break;
	}
}

void CSearchManagerDlg::ReSize()
{
	CRect rc;
	m_wndList.GetWindowRect( &rc );

	m_wndList.SetColumnWidth( 1, rc.Width() - COLUMN_SIZE * 4- GetSystemMetrics( SM_CXVSCROLL ) - GetSystemMetrics( SM_CXEDGE ) * 2 );
}

void CSearchManagerDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogExSized::OnSize(nType, cx, cy);

	if ( m_wndList.m_hWnd )
	{
		ReSize();
	}
}
