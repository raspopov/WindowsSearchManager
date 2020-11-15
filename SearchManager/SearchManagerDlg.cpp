// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

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

#include "stdafx.h"
#include "SearchManager.h"
#include "SearchManagerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

const LPCTSTR szStatus[] =
{
	_T( "Index is current; no indexing needed. Queries can be processed." ),
	_T( "Indexer is paused. This can happen either because the user paused indexing or the indexer back-off criteria have been met. Queries can be processed." ),
	_T( "Index is recovering; queries and indexing are processed while in this state." ),
	_T( "Indexer is currently executing a full crawl and will index everything it is configured to index. Queries can be processed while indexing." ),
	_T( "Indexer is preforming a crawl to see if anything has changed or requires indexing. Queries can be processed while indexing." ),
	_T( "Indexer is processing the notification queue. This is done before resuming any crawl." ),
	_T( "Indexer is shutting down and is not indexing. Indexer can't be queried." )
};

const LPCTSTR szPausedReason[] =
{
	_T( "" ),
	_T( "Paused due to high I/O." ),
	_T( "Paused due to high CPU usage." ),
	_T( "Paused due to high NTF rate." ),
	_T( "Paused due to low battery." ),
	_T( "Paused due to low memory." ),
	_T( "Paused due to low disk space." ),
	_T( "Paused due to need for delayed recovery." ),
	_T( "Paused due to user activity." ),
	_T( "Paused by external request." ),
	_T( "Paused by upgrading." ),
};

CSearchManagerDlg::CSearchManagerDlg(CWnd* pParent /*=NULL*/)
	: CDialog		( CSearchManagerDlg::IDD, pParent )
	, m_bRefresh	( true )
	, m_bInUse		( false )
{
	m_hIcon = AfxGetApp()->LoadIcon( IDR_MAINFRAME );
}

void CSearchManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange( pDX );

	DDX_Control( pDX, IDC_STATUS, m_wndStatus );
	DDX_Control( pDX, IDC_NAME, m_wndName );
	DDX_Control( pDX, IDC_INDEX, m_wndIndex );
	DDX_Control( pDX, IDC_LIST, m_wndList );
}

BEGIN_MESSAGE_MAP(CSearchManagerDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_NOTIFY( NM_CLICK, IDC_SYSINDEX, &CSearchManagerDlg::OnNMClickSysindex )
	ON_NOTIFY( LVN_DELETEITEM, IDC_LIST, &CSearchManagerDlg::OnLvnDeleteitemList )
END_MESSAGE_MAP()

BOOL CSearchManagerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString sTitle;
	sTitle.LoadString( AFX_IDS_APP_TITLE );
	SetWindowText( sTitle );

	SetIcon( m_hIcon, TRUE );
	SetIcon( m_hIcon, FALSE );

	CRect rc;
	m_wndList.GetWindowRect( &rc );

	m_wndList.SetExtendedStyle( m_wndList.GetExtendedStyle() | LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );

	LVGROUP grpRoots = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID, _T("Search roots"), 0, nullptr, 0, GROUP_ID_ROOTS };
	m_wndList.InsertGroup( 0, &grpRoots );
	LVGROUP grpRules = { sizeof( LVGROUP ), LVGF_HEADER | LVGF_GROUPID, _T("Scope rules"), 0, nullptr, 0, GROUP_ID_RULES };
	m_wndList.InsertGroup( 1, &grpRules );
	m_wndList.EnableGroupView( TRUE );

	m_wndList.InsertColumn( 0, _T("Name"), LVCFMT_LEFT, rc.Width() - 80 * 3- GetSystemMetrics( SM_CXVSCROLL ) - GetSystemMetrics( SM_CXEDGE ) * 2 );
	m_wndList.InsertColumn( 1, _T(""), LVCFMT_CENTER, 80 );
	m_wndList.InsertColumn( 2, _T(""), LVCFMT_CENTER, 80 );
	m_wndList.InsertColumn( 3, _T(""), LVCFMT_CENTER, 80 );

	SetTimer( 1, 1000, nullptr );

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
		CDialog::OnPaint();
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
		OnUpdate();
	}

	MSG msg;
	while ( ::PeekMessage( &msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE | PM_NOYIELD ) );

	CDialog::OnTimer( nIDEvent );
}

void CSearchManagerDlg::OnDestroy()
{
	KillTimer( 1 );

	m_pManager.Release();

	CDialog::OnDestroy();
}

void CSearchManagerDlg::Disconnect()
{
	m_pScope.Release();
	m_pCatalog.Release();
	m_pManager.Release();
}

void CSearchManagerDlg::OnUpdate()
{
	HRESULT hr = S_OK;

	CLock locker( &m_bInUse );
	if ( ! locker.Lock() )
	{
		return;
	}

	bool bRefreshing = m_bRefresh;
	if ( m_bRefresh )
	{
		m_bRefresh = false;

		CWaitCursor wc;

		// Release old connection
		if ( m_pManager )
		{
			SetStatus( _T("Disconnecting...") );
			SetIndex( _T("") );

			m_wndList.DeleteAllItems();

			Disconnect();

			Sleep( 1000 );
		}

		// Create a new connection
		SetStatus( IDS_CONNECTING );
		hr = m_pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( FAILED( hr ) )
		{
			SetStatus( _T("Can't connect to Windows Search\x2122.") );
			SetIndex( _T("") );
			return;
		}

		// Get version string
		LPWSTR szVersion;
		hr = m_pManager->GetIndexerVersionStr( &szVersion );
		if ( FAILED( hr ) )
		{
			m_pManager.Release();
			SetStatus( _T("Can't get Windows Search\x2122 version.") );
			SetIndex( _T("") );
			return;
		}

		CString sTitle;
		sTitle.LoadString( AFX_IDS_APP_TITLE );
		SetWindowText( sTitle + _T(" - Indexer version: ") + szVersion );

		CoTaskMemFree( szVersion );
	}

	CString sStatus, sIndex;
	if ( m_pManager )
	{
		if ( ! m_pCatalog )
		{
			SetStatus( _T("Getting catalog...") );

			hr = m_pManager->GetCatalog( L"SystemIndex", &m_pCatalog );
		}

		if ( m_pCatalog )
		{
			LPWSTR szName;
			hr = m_pCatalog->get_Name( &szName );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Name             \t\t: %s\r\n"), szName );
				CoTaskMemFree( szName );
			}

			LONG numitems;
			hr = m_pCatalog->NumberOfItems( &numitems );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Total items      \t\t: %ld\r\n"), numitems );
			}

			DWORD timeout;
			hr = m_pCatalog->get_ConnectTimeout( &timeout );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Connect timeout     \t: %lu s\r\n"), timeout );
			}

			hr = m_pCatalog->get_DataTimeout( &timeout );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Data timeout        \t: %lu s\r\n"), timeout );
			}

			BOOL diacritic;
			hr = m_pCatalog->get_DiacriticSensitivity( &diacritic );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat( _T("Diacritic sensitivity\t: %s\r\n"), ( diacritic ? _T("yes") : _T("no") ) );
			}

			if ( bRefreshing )
			{
				SetStatus( _T("Getting number of items to index...") );
			}
			LONG pendingitems;
			LONG queued;
			LONG highpri;
			hr = m_pCatalog->NumberOfItemsToIndex( &pendingitems, &queued, &highpri );
			if ( SUCCEEDED( hr ) )
			{
				sStatus.AppendFormat(
					_T("Pending items      \t: %ld\r\n")
					_T("Notification queue \t: %ld\r\n")
					_T("High priority queue\t: %ld\r\n"),
					pendingitems, queued, highpri );
			}

			CatalogStatus status;
			CatalogPausedReason reason;
			hr = m_pCatalog->GetCatalogStatus( &status, &reason );
			if ( SUCCEEDED( hr ) )
			{
				sStatus += _T("\r\n");
				sStatus += szStatus[ status ];
				if ( status == CATALOG_STATUS_PAUSED )
				{
					sStatus += _T("\r\n\r\n");
					sStatus += szPausedReason[ reason ];
				}
			}

			LPWSTR szIndex;
			hr = m_pCatalog->URLBeingIndexed( &szIndex );
			if ( SUCCEEDED( hr ) )
			{
				sIndex = szIndex;
				CoTaskMemFree( szIndex );
			}

			if ( ! m_pScope )
			{
				m_wndList.DeleteAllItems();

				SetStatus( _T("Enumerating roots and scope rules...") );

				hr = m_pCatalog->GetCrawlScopeManager( &m_pScope );

				if ( m_pScope )
				{
					EnumerateRoots( m_pScope );
					EnumerateScopeRules( m_pScope );
				}
			}
		}

		if ( hr == E_ACCESSDENIED )
		{
			sStatus = _T("Access Denied. Please restart this application with administrative privileges...");
		}
		else if ( FAILED( hr ) )
		{
			Disconnect();
		}
	}

	if ( ! m_pManager )
	{
		sStatus = _T("Connection to Windows Search\x2122 has been closed. Please press Refresh button...");
	}

	// Output (cached)
	SetStatus( sStatus );
	SetIndex( sIndex );
}

void CSearchManagerDlg::SetStatus(UINT nStatus)
{
	CString sStatus;
	sStatus.LoadString( nStatus );
	SetStatus( sStatus );
}

void CSearchManagerDlg::SetIndex(UINT nIndex)
{
	CString sIndex;
	sIndex.LoadString( nIndex );
	SetIndex( sIndex );
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

	ShellExecute( GetSafeHwnd(), nullptr, sSys + _T("control.exe"), _T(" /name Microsoft.IndexingOptions"), sSys, SW_SHOWDEFAULT );

	Sleep( 1000 );

	*pResult = 0;
}

HRESULT CSearchManagerDlg::EnumerateRoots(ISearchCrawlScopeManager* pScope)
{
	CComPtr< IEnumSearchRoots > pRoots;
	HRESULT hr = pScope->EnumerateRoots( &pRoots );
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
	CComPtr< IEnumSearchScopeRules > pRules;
	HRESULT hr = pScope->EnumerateScopeRules( &pRules );
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
	case VK_DELETE:
		if ( GetFocus() == static_cast< CWnd*>( &m_wndList ) )
		{
			OnDelete();
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void CSearchManagerDlg::OnDelete()
{
	CLock locker( &m_bInUse );
	if ( ! locker.Lock() )
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
		CWaitCursor wc;

		SetStatus( IDS_DELETING );

		for ( POSITION pos = to_delete.GetHeadPosition(); pos; )
		{
			CItem* item = to_delete.GetNext( pos );

			// Delete from WS
			if ( item->Delete( m_pScope ) )
			{
				// Delete from list
				LVFINDINFO fi = { LVFI_PARAM, nullptr, reinterpret_cast< LPARAM >( item ) };
				const int index = m_wndList.FindItem( &fi );
				m_wndList.DeleteItem( index );
				m_wndList.UpdateWindow();

				m_bRefresh = true;
			}
		}

		if ( m_bRefresh )
		{
			m_pManager.Release();
			m_pCatalog.Release();

			m_pScope->SaveAll();

			Disconnect();
		}
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
