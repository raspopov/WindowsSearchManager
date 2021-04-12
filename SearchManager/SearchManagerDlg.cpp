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

int CSearchManagerDlg::GetGroupId(UINT nID, const CString& sPostfix)
{
	CString name = LoadString( nID );
	if ( ! sPostfix.IsEmpty() )
	{
		name += _T(" @ ");
		name += sPostfix;
	}

	void* value;
	if ( m_Groups.Lookup( name, value ) )
	{
		return reinterpret_cast< int >( value );
	}

	LVGROUP grpRoots = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID,
		const_cast< LPTSTR >( static_cast< LPCTSTR >( name ) ), 0, nullptr, 0, m_wndList.GetGroupCount() };
	m_wndList.InsertGroup( grpRoots.iGroupId, &grpRoots );
	m_Groups.SetAt( name, reinterpret_cast< void* >( grpRoots.iGroupId ) );
	return grpRoots.iGroupId;
}

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
		menu->AddMenu( _T("IDR_ADD_MENU"), IDR_ADD_MENU );
		menu->AddMenu( _T("IDR_LIST_MENU"), IDR_LIST_MENU );

		m_btnAdd.m_bOSMenu = FALSE;
		m_btnAdd.m_hMenu = GetSubMenu( menu->GetMenuById( IDR_ADD_MENU ), 0 );
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
	const BOOL bCatalog = ( m_pCatalog != false );
	const BOOL bScope = ( m_pScope != false );
	BOOL bSingle = FALSE;
	BOOL bSelected = FALSE;
	if ( bScope )
	{
		if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
		{
			const int index = m_wndList.GetNextSelectedItem( pos );
			if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
			{
				bSelected = IsDeletable( item->Group );
				bSingle = bSelected && ! pos;
			}
		}
	}
	GetDlgItem( IDC_REINDEX )->EnableWindow( bCatalog );
	GetDlgItem( IDC_RESET )->EnableWindow( bCatalog );
	GetDlgItem( IDC_DEFAULT )->EnableWindow( bScope );
	GetDlgItem( IDC_ADD )->EnableWindow( bScope );
	GetDlgItem( IDC_EDIT )->EnableWindow( bSingle );
	GetDlgItem( IDC_DELETE )->EnableWindow( bSelected );
}

int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    UNREFERENCED_PARAMETER( lParamSort );
    return reinterpret_cast< const CItem* >( lParam1 )->URL.CompareNoCase( reinterpret_cast< const CItem* >( lParam2 )->URL );
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
		m_List.RemoveAll();

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
					EnumerateRoots( m_pScope );
					EnumerateScopeRules( m_pScope );
				}
			}
		}

		EnumerateRegistryRoots();
		EnumerateRegistryDefaultRules();

		m_wndList.SortItems( &SortProc, 0 );

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
				VERIFY( root->InsertTo( m_wndList, GetGroupId( IDS_ROOTS, _T("") ) ) != -1 );
				m_List.AddTail( root.Detach() );
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
				VERIFY( rule->InsertTo( m_wndList, GetGroupId( IDS_RULES, _T("") ) ) != -1 );
				m_List.AddTail( rule.Detach() );
			}
		}
	}

	return hr;
}

void CSearchManagerDlg::EnumerateRegistryRoots()
{
	HKEY hRootsKey;
	LSTATUS res = RegOpenKeyFull( HKEY_LOCAL_MACHINE, KEY_ROOTS, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hRootsKey );
	if ( res == ERROR_SUCCESS )
	{
		TCHAR name[ MAX_PATH ] = {};
		TCHAR url[ MAX_PATH ] = {};

		for ( DWORD i = 0; ; ++i )
		{
			name[ 0 ] = 0;
			DWORD name_size = _countof( name );
			res = SHEnumKeyEx( hRootsKey, i, name, &name_size );
			if ( res != ERROR_SUCCESS )
			{
				break;
			}

			CString key;
			key.Format( _T("%s\\%s"), KEY_ROOTS, name );
			DWORD type, ulr_size = sizeof( url );
			res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
			if ( res == ERROR_SUCCESS )
			{
				bool found = false;
				for ( POSITION pos = m_List.GetHeadPosition(); pos; )
				{
					auto item = m_List.GetNext( pos );
					if ( item->Group == GROUP_ROOTS && item->URL == url )
					{
						found = true;
						break;
					}
				}

				if ( ! found )
				{
					DWORD notif = FALSE;
					DWORD notif_size = sizeof( DWORD );
					res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("ProvidesNotifications"), &type, reinterpret_cast< LPBYTE >( &notif ), &notif_size );
					ASSERT( res == ERROR_SUCCESS );

					CAutoPtr< COfflineRoot > root( new COfflineRoot( notif, url ) );
					root->ParseURL();

					VERIFY( root->InsertTo( m_wndList, GetGroupId( IDS_DEFAULT_ROOTS, root->Guid ) ) != -1 );
					m_List.AddTail( root.Detach() );
				}
			}
		}
		RegCloseKey( hRootsKey );
	}
}

void CSearchManagerDlg::EnumerateRegistryDefaultRules()
{
	HKEY hRootsKey;
	LSTATUS res = RegOpenKeyFull( HKEY_LOCAL_MACHINE, KEY_DEFAULT_RULES, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hRootsKey );
	if ( res == ERROR_SUCCESS )
	{
		TCHAR name[ MAX_PATH ] = {};
		TCHAR url[ MAX_PATH ] = {};

		for ( DWORD i = 0; ; ++i )
		{
			name[ 0 ] = 0;
			DWORD name_size = _countof( name );
			res = SHEnumKeyEx( hRootsKey, i, name, &name_size );
			if ( res != ERROR_SUCCESS )
			{
				break;
			}

			CString key;
			key.Format( _T("%s\\%s"), KEY_DEFAULT_RULES, name );
			DWORD type, ulr_size = sizeof( url );
			res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("URL"), &type, reinterpret_cast< LPBYTE >( url ), &ulr_size );
			if ( res == ERROR_SUCCESS )
			{
				bool found = false;
				for ( POSITION pos = m_List.GetHeadPosition(); pos; )
				{
					auto item = m_List.GetNext( pos );
					if ( item->Group == GROUP_RULES && item->URL == url )
					{
						found = true;
						break;
					}
				}

				if ( ! found )
				{
					DWORD incl = FALSE;
					DWORD incl_size = sizeof( DWORD );
					res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("Include"), &type, reinterpret_cast< LPBYTE >( &incl ), &incl_size );
					ASSERT( res == ERROR_SUCCESS );

					DWORD def = FALSE;
					DWORD def_size = sizeof( DWORD );
					res = RegQueryValueFull( HKEY_LOCAL_MACHINE, key, _T("Default"), &type, reinterpret_cast< LPBYTE >( &def ), &def_size );
					ASSERT( res == ERROR_SUCCESS );

					CAutoPtr< CDefaultRule > rule( new CDefaultRule( incl, def, url ) );
					rule->ParseURL();

					VERIFY( rule->InsertTo( m_wndList, GetGroupId( IDS_DEFAULT_RULES, rule->Guid ) ) != -1 );
					m_List.AddTail( rule.Detach() );
				}
			}
		}
		RegCloseKey( hRootsKey );
	}
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
				AddRule( TRUE, TRUE );
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
		AddRule( TRUE, TRUE );
		break;

	case ID_EXCLUDE_DEFAULT_SCOPE:
		AddRule( FALSE, TRUE );
		break;

	case ID_SEARCHROOT:
		AddRoot();
		break;
	}
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
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( pNMItemActivate->iItem ) ) )
		{
			OnEdit( item );
		}
	}
}

void CSearchManagerDlg::OnBnClickedEdit()
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
	Delete();
}

void CSearchManagerDlg::OnUpdateDelete(CCmdUI *pCmdUI)
{
	BOOL enable = FALSE;
	if ( m_pScope )
	{
		if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
		{
			const int index = m_wndList.GetNextSelectedItem( pos );
			if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
			{
				enable = IsDeletable( item->Group );
			}
		}
	}
	pCmdUI->Enable( enable );
}

void CSearchManagerDlg::OnEdit()
{
	OnBnClickedEdit();
}

void CSearchManagerDlg::OnUpdateEdit(CCmdUI *pCmdUI)
{
	BOOL enable = FALSE;
	if ( m_pScope )
	{
		if ( POSITION pos = m_wndList.GetFirstSelectedItemPosition() )
		{
			const int index = m_wndList.GetNextSelectedItem( pos );
			if ( ! pos )
			{
				if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
				{
					enable = IsDeletable( item->Group );
				}
			}
		}
	}
	pCmdUI->Enable( enable );
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
