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
#include "UrlDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define COLUMN_SIZE		80
#define CRLF			_T("\r\n")
#define CATALOG_NAME	_T("SystemIndex")
#define INDEXER_SERVICE	_T("WSearch")

struct error_t
{
	CString error;
	CString msg;

	error_t(HRESULT hr)
	{
		static LPCTSTR szModules [] =
		{
			_T("tquery.dll"),
			_T("lsasrv.dll"),
			_T("user32.dll"),
			_T("netapi32.dll"),
			_T("netmsg.dll"),
			_T("netevent.dll"),
			_T("xpsp2res.dll"),
			_T("spmsg.dll"),
			_T("wininet.dll"),
			_T("ntdll.dll"),
			_T("ntdsbmsg.dll"),
			_T("mprmsg.dll"),
			_T("IoLogMsg.dll"),
			_T("NTMSEVT.DLL")
		};

		LPTSTR lpszTemp = nullptr;
		if ( ! FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, reinterpret_cast< LPTSTR >( &lpszTemp ), 0, nullptr ) )
		{
			for ( auto & szModule : szModules )
			{
				if ( HMODULE hModule = LoadLibraryEx( szModule, nullptr, LOAD_LIBRARY_AS_DATAFILE ) )
				{
					const BOOL res = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE, hModule, hr, 0, reinterpret_cast< LPTSTR >( &lpszTemp ), 0, nullptr);

					FreeLibrary( hModule );

					if ( res )
					{
						break;
					}
				}
			}
		}

		if ( lpszTemp )
		{
			msg = lpszTemp;
			msg.TrimRight( _T(" \t?!.\r\n") );
			msg += _T(".");
			LocalFree( lpszTemp );

			if ( hr == E_ACCESSDENIED )
			{
				// Administrative privileges required
				msg += _T(" ");
				msg += LoadString( IDS_ADMIN );
			}
		}
		else
		{
			msg = LoadString( IDS_UNKNOWN_ERROR );
		}

		error.Format( IDS_ERROR_CODE, hr );
	}
};

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
	DDX_Control(pDX, IDC_ADD, m_btnAdd);
}

BEGIN_MESSAGE_MAP(CSearchManagerDlg, CDialogExSized)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_NOTIFY(NM_CLICK, IDC_SYSINDEX, &CSearchManagerDlg::OnNMClickSysindex)
	ON_NOTIFY(LVN_DELETEITEM, IDC_LIST, &CSearchManagerDlg::OnLvnDeleteitemList)
	ON_BN_CLICKED(IDC_ADD, &CSearchManagerDlg::OnBnClickedAdd)
	ON_BN_CLICKED(IDC_DELETE, &CSearchManagerDlg::OnBnClickedDelete)
	ON_BN_CLICKED(IDC_REINDEX, &CSearchManagerDlg::OnBnClickedReindex)
	ON_BN_CLICKED(IDC_RESET, &CSearchManagerDlg::OnBnClickedReset)
	ON_BN_CLICKED(IDC_DEFAULT, &CSearchManagerDlg::OnBnClickedDefault)
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

	const CString sRoots = LoadString( IDS_ROOTS );
	LVGROUP grpRoots = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID,
		const_cast< LPTSTR >( static_cast< LPCTSTR >( sRoots ) ), 0, nullptr, 0, GROUP_ROOTS };
	m_wndList.InsertGroup( 0, &grpRoots );
	const CString sRules = LoadString( IDS_RULES );
	LVGROUP grpRules = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID,
		const_cast< LPTSTR >( static_cast< LPCTSTR >( sRules ) ), 0, nullptr, 0, GROUP_RULES };
	m_wndList.InsertGroup( 1, &grpRules );
	m_wndList.EnableGroupView( TRUE );

	m_wndList.InsertColumn( 0, _T(""), LVCFMT_CENTER, COLUMN_SIZE );
	m_wndList.InsertColumn( 1, _T(""), LVCFMT_LEFT, rc.Width() - COLUMN_SIZE * 4- GetSystemMetrics( SM_CXVSCROLL ) - GetSystemMetrics( SM_CXEDGE ) * 2 );
	m_wndList.InsertColumn( 2, _T(""), LVCFMT_CENTER, COLUMN_SIZE );
	m_wndList.InsertColumn( 3, _T(""), LVCFMT_CENTER, COLUMN_SIZE );
	m_wndList.InsertColumn( 4, _T(""), LVCFMT_CENTER, COLUMN_SIZE );

	if ( auto menu = theApp.GetContextMenuManager() )
	{
		menu->AddMenu( _T("IDR_ADD_MENU"), IDR_ADD_MENU );
		menu->AddMenu( _T("IDR_LIST_MENU"), IDR_LIST_MENU );

		m_btnAdd.m_bOSMenu = FALSE;
		m_btnAdd.m_hMenu = GetSubMenu( menu->GetMenuById( IDR_ADD_MENU ), 0 );
	}

	UpdateInterface();

	SetTimer( 1, 200, nullptr );

	return TRUE;
}

void CSearchManagerDlg::OnPaint()
{
	if ( IsIconic() )
	{
		CPaintDC dc(this);

		SendMessage( WM_ICONERASEBKGND, reinterpret_cast<WPARAM>( dc.GetSafeHdc() ), 0 );

		int cxIcon = GetSystemMetrics( SM_CXICON );
		int cyIcon = GetSystemMetrics( SM_CYICON );
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
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
		Update();
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
	bool bSingleRule = false;
	bool bSelected = false;
	if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			bSingleRule = ( item->Group == GROUP_RULES ) && ! pos;
			bSelected = true;
		}
	}

	GetDlgItem( IDC_REINDEX )->EnableWindow( m_pCatalog != false );
	GetDlgItem( IDC_RESET )->EnableWindow( m_pCatalog != false );
	GetDlgItem( IDC_DEFAULT )->EnableWindow( m_pScope != false );

	GetDlgItem( IDC_ADD )->EnableWindow( m_pScope != false );
	GetDlgItem( IDC_EDIT )->EnableWindow( m_pScope != false && bSingleRule );
	GetDlgItem( IDC_DELETE )->EnableWindow( m_pScope != false && bSelected );
}

void CSearchManagerDlg::Update()
{
	HRESULT hr = S_OK;

	CLock locker( &m_bInUse );
	if ( ! locker.Lock() )
	{
		return;
	}

	const bool bRefreshing = m_bRefresh;

	if ( m_bRefresh )
	{
		m_bRefresh = false;

		CWaitCursor wc;

		CString sVersion = _T("Unknown");

		m_wndList.DeleteAllItems();

		// Release old connection
		if ( m_pCatalog )
		{
			SetStatus( IDS_DISCONNECTING );
			SetIndex( _T("") );

			Disconnect();

			SleepEx( 500, FALSE );
		}

		// Create a new connection
		SetStatus( IDS_CONNECTING );
		CComPtr< ISearchManager > pManager;
		hr = pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( SUCCEEDED( hr ) )
		{
			// Get version
			LPWSTR szVersion = nullptr;
			hr = pManager->GetIndexerVersionStr( &szVersion );
			if ( SUCCEEDED( hr ) )
			{
				sVersion = szVersion;
				CoTaskMemFree( szVersion );
			}

			// Get catalog
			SetStatus( IDS_GETTING_CATALOG );
			hr = pManager->GetCatalog( CATALOG_NAME, &m_pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				// Get scope
				SetStatus( IDS_ENUMERATING );
				hr = m_pCatalog->GetCrawlScopeManager( &m_pScope );
				if ( SUCCEEDED( hr ) )
				{
					// Enumerate scope
					EnumerateRoots( m_pScope );
					EnumerateScopeRules( m_pScope );
				}
			}
		}

		SetWindowText( LoadString( AFX_IDS_APP_TITLE ) + _T(" - Indexer version: ") + sVersion );

		if ( SUCCEEDED( hr ) )
		{
			UpdateInterface();

			UpdateWindow();
		}
		else
		{
			SleepEx( 500, FALSE );

			const error_t result( hr );
			SetStatus( result.msg + CRLF CRLF + result.error + CRLF CRLF + LoadString( IDS_CLOSED ) );

			Disconnect();
		}
	}

	CString sStatus;

	if ( m_pCatalog )
	{
		LONG numitems = 0;
		hr = m_pCatalog->NumberOfItems( &numitems );
		if ( SUCCEEDED( hr ) )
		{
			sStatus.AppendFormat( _T("Total items      \t\t: %ld") CRLF, numitems );

			DWORD timeout = 0;
			hr = m_pCatalog->get_ConnectTimeout( &timeout );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Connect timeout     \t: %lu s") CRLF, timeout );
			}

			hr = m_pCatalog->get_DataTimeout( &timeout );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Data timeout        \t: %lu s") CRLF, timeout );
			}

			BOOL diacritic = FALSE;
			hr = m_pCatalog->get_DiacriticSensitivity( &diacritic );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Diacritic sensitivity\t: %s") CRLF, ( diacritic ? _T("yes") : _T("no") ) );
			}

			LONG pendingitems = 0, queued = 0, highpri = 0;
			hr = m_pCatalog->NumberOfItemsToIndex( &pendingitems, &queued, &highpri );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat(
					_T("Pending queue      \t: %ld") CRLF
					_T("Notification queue \t: %ld") CRLF
					_T("High priority queue\t: %ld") CRLF,
					pendingitems, queued, highpri );
			}

			CatalogStatus status;
			CatalogPausedReason reason;
			hr = m_pCatalog->GetCatalogStatus( &status, &reason );
			if ( SUCCEEDED( hr ) )
			{
				if ( status >= CATALOG_STATUS_IDLE && status <= CATALOG_STATUS_SHUTTING_DOWN )
				{
					sStatus += CRLF;
					sStatus += LoadString( IDS_STATUS_1 + static_cast< int >( status ) );
				}
				if ( status == CATALOG_STATUS_PAUSED )
				{
					if ( reason > CATALOG_PAUSED_REASON_NONE && reason <= CATALOG_PAUSED_REASON_UPGRADING )
					{
						sStatus += CRLF CRLF;
						sStatus += LoadString( IDS_REASON_1 + static_cast< int >( reason ) - 1 );
					}
				}
			}
		}

		if ( SUCCEEDED( hr ) )
		{
			SetStatus( sStatus );
		}
		else
		{
			const error_t result( hr );
			SetStatus( result.msg + CRLF CRLF + result.error );

			if ( FAILED( hr ) )
			{
				Disconnect();
			}
		}
	}

	if ( m_pCatalog )
	{
		LPWSTR szIndex = nullptr;
		hr = m_pCatalog->URLBeingIndexed( &szIndex );
		if ( SUCCEEDED( hr ) )
		{
			SetIndex( szIndex );
			CoTaskMemFree( szIndex );
		}
		else
		{
			const error_t result( hr );
			SetIndex( result.msg + _T(" ") + result.error );
		}
	}
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

HRESULT CSearchManagerDlg::EnumerateRoots(ISearchCrawlScopeManager* pScope)
{
	HRESULT hr = S_OK;

	CComPtr< IEnumSearchRoots > pRoots;
	hr = pScope->EnumerateRoots( &pRoots );
	if ( SUCCEEDED( hr ) )
	{
		for (;;)
		{
			CComPtr< ISearchRoot > pRoot;
			hr = pRoots->Next( 1, &pRoot, nullptr );
			if ( hr != S_OK )
			{
				break;
			}

			CAutoPtr< CRoot > root( new CRoot( pScope, pRoot ) );
			if ( *root )
			{
				root->InsertTo( m_wndList );
				root.Detach();
			}
		}
	}

	return hr;
}

HRESULT CSearchManagerDlg::EnumerateScopeRules(ISearchCrawlScopeManager* pScope)
{
	HRESULT hr = S_OK;

	CComPtr< IEnumSearchScopeRules > pRules;
	hr = pScope->EnumerateScopeRules( &pRules );
	if ( SUCCEEDED( hr ) )
	{
		for (;;)
		{
			CComPtr< ISearchScopeRule > pRule;
			hr = pRules->Next( 1, &pRule, nullptr );
			if ( hr != S_OK )
			{
				break;
			}

			CAutoPtr< CRule > rule( new CRule( pScope, pRule ) );
			if ( *rule )
			{
				rule->InsertTo( m_wndList );
				rule.Detach();
			}
		}
	}

	return hr;
}

void CSearchManagerDlg::OnOK()
{
	m_bRefresh = true;
}

BOOL CSearchManagerDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	UNUSED_ALWAYS( nRepCnt );
	UNUSED_ALWAYS( nFlags );

	switch ( nChar )
	{
	case VK_INSERT:
		if ( GetFocus() == static_cast< CWnd*>( &m_wndList ) )
		{
			Add( TRUE, FALSE );
			return TRUE;
		}
		break;

	case VK_DELETE:
		if ( GetFocus() == static_cast< CWnd*>( &m_wndList ) )
		{
			Delete();
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void CSearchManagerDlg::Add(BOOL bInclude, BOOL bDefault)
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	CString hint;
	hint.Format( LoadString( IDS_ADD_ROOT ),
		( bInclude ? _T("Include") : _T("Exclude") ),
		( bDefault ? _T("Default") : _T("User") ) );
	CUrlDialog dlg( hint, this );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() )
	{
		CWaitCursor wc;

		HRESULT hr = bDefault ? m_pScope->AddDefaultScopeRule( dlg.m_sURL, bInclude, FF_INDEXCOMPLEXURLS ) :
			m_pScope->AddUserScopeRule( dlg.m_sURL, bInclude, FALSE, FF_INDEXCOMPLEXURLS );
		if ( SUCCEEDED( hr ) )
		{
			const HRESULT hr_save = m_pScope->SaveAll();
			if ( hr == S_OK || FAILED( hr_save ) )
			{
				hr = hr_save;
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Delete()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	// Enumerate items to delete
	CList< CItem* > to_delete;
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( CItem* item = reinterpret_cast< CItem* >( m_wndList.GetItemData( index ) ) )
		{
			to_delete.AddTail( item );
		}
	}

	// Delete items
	if ( ! to_delete.IsEmpty() )
	{
		CString msg;
		msg.Format( IDS_DELETE_CONFIRM, to_delete.GetCount() );
		if ( AfxMessageBox( msg, MB_YESNO | MB_ICONQUESTION ) == IDYES )
		{
			SetStatus( IDS_DELETING );

			for ( POSITION pos = to_delete.GetHeadPosition(); pos; )
			{
				const POSITION prev = pos;
				CItem* item = to_delete.GetNext( pos );

				CWaitCursor wc;

				// Delete from WS
				HRESULT hr = item->DeleteFrom( m_pScope );
				if ( SUCCEEDED( hr ) )
				{
					const HRESULT hr_save = m_pScope->SaveAll();
					if ( hr == S_OK || FAILED( hr_save ) )
					{
						hr = hr_save;
					}
				}

				if ( hr == S_OK )
				{
					m_bRefresh = true;
				}
				else
				{
					const error_t result( hr );
					const int res = AfxMessageBox( result.msg + _T("\n\n") + result.error + _T("\n\n") + item->URL,
						MB_ABORTRETRYIGNORE | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONEXCLAMATION ) );
					if ( res == IDRETRY )
					{
						// Retry deletion
						pos = prev;
						continue;
					}
					else if ( res == IDIGNORE )
					{
						// Ignore error and continue deletion
						continue;
					}
					else
					{
						// Cancel deletion
						break;
					}
				}
			}
		}
	}
}

void CSearchManagerDlg::Edit(CRule* item)
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	CString hint;
	hint.Format( LoadString( IDS_EDIT_ROOT ),
		( item->IsInclude ? _T("Include") : _T("Exclude") ),
		( item->IsDefault ? _T("Default") : _T("User") ) );
	CUrlDialog dlg( hint, this );
	dlg.m_sURL = item->URL;
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() )
	{
		CWaitCursor wc;

		// Delete from WS
		HRESULT hr = item->DeleteFrom( m_pScope );
		if ( SUCCEEDED( hr ) )
		{
			item->URL = dlg.m_sURL;

			// Add to WS
			hr = item->AddTo( m_pScope );
			if ( SUCCEEDED( hr ) )
			{
				const HRESULT hr_save = m_pScope->SaveAll();
				if ( hr == S_OK || FAILED( hr_save ) )
				{
					hr = hr_save;
				}
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
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

void CSearchManagerDlg::OnBnClickedAdd()
{
	BOOL bInclude = TRUE, bDefault = TRUE;

	switch ( m_btnAdd.m_nMenuResult )
	{
	case ID_INCLUDE_USER_SCOPE:
		bDefault = FALSE;
		break;

	case ID_EXCLUDE_USER_SCOPE:
		bInclude = FALSE;
		bDefault = FALSE;
		break;

	case ID_INCLUDE_DEFAULT_SCOPE:
		break;

	case ID_EXCLUDE_DEFAULT_SCOPE:
		bInclude = FALSE;
		break;
	}

	Add( bInclude, bDefault );
}

void CSearchManagerDlg::OnBnClickedReindex()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pCatalog )
	{
		return;
	}

	if ( AfxMessageBox( IDS_REINDEX_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		const HRESULT hr = m_pCatalog->Reindex();

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::OnBnClickedReset()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pCatalog )
	{
		return;
	}

	if ( AfxMessageBox( IDS_RESET_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		const HRESULT hr = m_pCatalog->Reset();

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::OnBnClickedDefault()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() || ! m_pScope )
	{
		return;
	}

	if ( AfxMessageBox( IDS_DEFAULT_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		HRESULT hr = m_pScope->RevertToDefaultScopes();
		if ( SUCCEEDED( hr ) )
		{
			const HRESULT hr_save = m_pScope->SaveAll();
			if ( hr == S_OK || FAILED( hr_save ) )
			{
				hr = hr_save;
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
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
		if ( CItem* item = reinterpret_cast< CItem* >( m_wndList.GetItemData( pNMItemActivate->iItem ) ) )
		{
			if ( item->Group == GROUP_RULES )
			{
				Edit( static_cast< CRule* >( item ) );
			}
		}
	}
}

void CSearchManagerDlg::OnBnClickedEdit()
{
	if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< CItem* >( m_wndList.GetItemData( index ) ) )
		{
			if ( item->Group == GROUP_RULES )
			{
				Edit( static_cast< CRule* >( item ) );
			}
		}
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
			const size_t nLen = ( values.GetLength() + 1 ) * sizeof( TCHAR );
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
	OnBnClickedDelete();
}

void CSearchManagerDlg::OnUpdateDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( m_pScope && m_wndList.GetFirstSelectedItemPosition() );
}

void CSearchManagerDlg::OnEdit()
{
	OnBnClickedEdit();
}

void CSearchManagerDlg::OnUpdateEdit(CCmdUI *pCmdUI)
{
	BOOL bEnable = FALSE;

	if ( m_pScope )
	{
		if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
		{
			const int index = m_wndList.GetNextSelectedItem( pos );
			if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
			{
				bEnable = ( item->Group == GROUP_RULES ) && ! pos;
			}
		}
	}

	pCmdUI->Enable( bEnable );
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
			if ( item->Group == GROUP_RULES )
			{
				pNMCD->clrTextBk = static_cast< const CRule* >( item )->IsInclude ? RGB( 192, 255, 192 ) : RGB( 255, 192, 192 );
			}
		}
		break;
	}
}

void CSearchManagerDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogExSized::OnSize(nType, cx, cy);

	if ( m_wndList.m_hWnd )
	{
		CRect rc;
		m_wndList.GetWindowRect( &rc );

		m_wndList.SetColumnWidth( 1, rc.Width() - COLUMN_SIZE * 4- GetSystemMetrics( SM_CXVSCROLL ) - GetSystemMetrics( SM_CXEDGE ) * 2 );
	}
}
