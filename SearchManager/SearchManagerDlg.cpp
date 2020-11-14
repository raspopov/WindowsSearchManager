//
// SearchManagerDlg.cpp
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
	: CDialog(CSearchManagerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon( IDR_MAINFRAME );
}

void CSearchManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control( pDX, IDC_STATUS, m_wndStatus );
	DDX_Control( pDX, IDC_NAME, m_wndName );
	DDX_Control( pDX, IDC_INDEX, m_wndIndex );
	DDX_Control( pDX, IDC_ROOTS, m_wndRoots );
	DDX_Control( pDX, IDC_RULES, m_wndRules );
}

BEGIN_MESSAGE_MAP(CSearchManagerDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_NOTIFY( NM_CLICK, IDC_SYSINDEX, &CSearchManagerDlg::OnNMClickSysindex )
END_MESSAGE_MAP()

BOOL CSearchManagerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon( m_hIcon, TRUE );
	SetIcon( m_hIcon, FALSE );

	SetDlgItemText( IDC_GROUP_ROOTS, _T( "Search roots (\x25A0 - included, \x25CB - excluded):" ) );
	SetDlgItemText( IDC_GROUP_RULES, _T( "Scope rules (\x25A0 - included, \x25CB - excluded | \x25A0 - default, \x25CB - optional):" ) );

	ConnectToWDS();

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
		CString sStatus, sURL;
		CList< CString > pRootURLs, pRuleURLs;

		if ( m_pManager )
		{
			CComPtr< ISearchCatalogManager > pCatalog;
			HRESULT hr = m_pManager->GetCatalog( L"SystemIndex", &pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				/*LPWSTR szName = NULL;
				hr = pCatalog->get_Name( &szName );
				if ( SUCCEEDED( hr ) )
				{
					sName.Format( _T("\"%s\" status:"), szName );
					CoTaskMemFree( szName );
				}*/

				LONG numitems;
				hr = pCatalog->NumberOfItems( &numitems );
				if ( SUCCEEDED( hr ) )
					sStatus.AppendFormat( 
						_T("Total items      \t\t: %ld\r\n"), numitems );

				LONG pendingitems;
				LONG queued;
				LONG highpri;
				hr = pCatalog->NumberOfItemsToIndex( &pendingitems, &queued, &highpri );
				if ( SUCCEEDED( hr ) )
					sStatus.AppendFormat( 
						_T("Pending items      \t: %ld\r\n")
						_T("Notification queue \t: %ld\r\n")
						_T("High priority queue\t: %ld\r\n"),
						pendingitems, queued, highpri );

				DWORD timeout = 0;
				hr = pCatalog->get_ConnectTimeout( &timeout );
				if ( SUCCEEDED( hr ) )
					sStatus.AppendFormat( _T("Connect timeout     \t: %ld s\r\n"), timeout );

				timeout = 0;
				hr = pCatalog->get_DataTimeout( &timeout );
				if ( SUCCEEDED( hr ) )
					sStatus.AppendFormat( _T("Data timeout        \t: %ld s\r\n"), timeout );

				BOOL diacritic = FALSE;
				hr = pCatalog->get_DiacriticSensitivity( &diacritic );
				if ( SUCCEEDED( hr ) )
					sStatus.AppendFormat( _T("Diacritic sensitivity\t: %s\r\n"), ( diacritic ? _T("yes") : _T("no") ) );
	
				CatalogStatus status;
				CatalogPausedReason reason;
				hr = pCatalog->GetCatalogStatus( &status, &reason );
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

				CComPtr< ISearchCrawlScopeManager > pScope;
				hr = pCatalog->GetCrawlScopeManager( &pScope );
				if ( SUCCEEDED( hr ) )
				{
					// Roots
					{
						CComPtr< IEnumSearchRoots > pRoots;
						hr = pScope->EnumerateRoots( &pRoots );
						if ( SUCCEEDED( hr ) )
						{
							for (;;)
							{
								CComPtr< ISearchRoot > pRoot;
								hr = pRoots->Next( 1, &pRoot, NULL );
								if ( hr != S_OK )
									break;

								LPWSTR szURL = NULL;
								hr = pRoot->get_RootURL( &szURL );
								if ( SUCCEEDED( hr ) )
								{
									BOOL bIncluded = FALSE;
									pScope->IncludedInCrawlScope( szURL, &bIncluded );

									CString sScopeURL;
									sScopeURL.Format( _T("%s %s"), ( bIncluded ? _T("\x25A0") : _T("\x25CB") ), szURL ); 
									pRootURLs.AddTail( sScopeURL );

									CoTaskMemFree( szURL );
								}
							}
						}
					}

					// Rules
					{
						CComPtr< IEnumSearchScopeRules > pRules;
						hr = pScope->EnumerateScopeRules( &pRules );
						if ( SUCCEEDED( hr ) )
						{
							for (;;)
							{
								CComPtr< ISearchScopeRule > pRule;
								hr = pRules->Next( 1, &pRule, NULL );
								if ( hr != S_OK )
									break;

								LPWSTR szURL = NULL;
								hr = pRule->get_PatternOrURL( &szURL );
								if ( SUCCEEDED( hr ) )
								{
									BOOL bIncluded = FALSE;
									pRule->get_IsIncluded( &bIncluded );
									BOOL bDefault = FALSE;
									pRule->get_IsDefault( &bDefault );

									CString sRuleURL;
									sRuleURL.Format( _T("%s | %s %s"), ( bIncluded ? _T("\x25A0") : _T("\x25CB") ), ( bDefault ? _T("\x25A0") : _T("\x25CB") ), szURL ); 
									pRuleURLs.AddTail( sRuleURL );

									CoTaskMemFree( szURL );
								}
							}
						}
						}
				}

				LPWSTR szURL = NULL;
				hr = pCatalog->URLBeingIndexed( &szURL );
				if ( SUCCEEDED( hr ) )
				{
					sURL = szURL;
					CoTaskMemFree( szURL );
				}
				else if ( hr == E_ACCESSDENIED )
				{
					sURL.Format( _T("Access Denied. Please restart this application with administrative privileges...") );
				}
			}
			else if ( hr == E_ACCESSDENIED )
			{
				sStatus = _T("Access Denied. Please restart this application with administrative privileges...");
			}
			else
				m_pManager.Release();
		}

		if ( ! m_pManager )
		{
			sStatus = _T("Connection to Windos Search\x2122 has been closed. Please restart this application...");
		}

		// Output (cached)

		if ( m_sStatusCache != sStatus )
		{
			m_sStatusCache = sStatus;
			m_wndStatus.SetWindowText( m_sStatusCache );
		}

		if ( m_sURLCache != sURL )
		{
			m_sURLCache = sURL;
			m_wndIndex.SetWindowText( m_sURLCache );
		}

		{
			CList< CString > pRootURLsDelete;
			pRootURLsDelete.AddTail( &m_pRootURLsCache );
			for ( POSITION pos = pRootURLs.GetHeadPosition(); pos; )
			{
				CString sURL = pRootURLs.GetNext( pos );
				POSITION posOld = pRootURLsDelete.Find( sURL );
				if ( posOld )
					pRootURLsDelete.RemoveAt( posOld );
				else
					m_wndRoots.AddString( sURL );
			}
			for ( POSITION pos = pRootURLsDelete.GetHeadPosition(); pos; )
			{
				CString sRootURLDelete = pRootURLsDelete.GetNext( pos );
				m_wndRoots.DeleteString( m_wndRoots.FindString( 0, sRootURLDelete ) );
			}
			m_pRootURLsCache.RemoveAll();
			m_pRootURLsCache.AddTail( &pRootURLs );
		}

		{
			CList< CString > pRuleURLsDelete;
			pRuleURLsDelete.AddTail( &m_pRuleURLsCache );
			for ( POSITION pos = pRuleURLs.GetHeadPosition(); pos; )
			{
				CString sURL = pRuleURLs.GetNext( pos );
				POSITION posOld = pRuleURLsDelete.Find( sURL );
				if ( posOld )
					pRuleURLsDelete.RemoveAt( posOld );
				else
					m_wndRules.AddString( sURL );
			}
			for ( POSITION pos = pRuleURLsDelete.GetHeadPosition(); pos; )
			{
				CString sURLDelete = pRuleURLsDelete.GetNext( pos );
				m_wndRules.DeleteString( m_wndRules.FindString( 0, sURLDelete ) );
			}
			m_pRuleURLsCache.RemoveAll();
			m_pRuleURLsCache.AddTail( &pRuleURLs );
		}
	}

	CDialog::OnTimer(nIDEvent);
}

void CSearchManagerDlg::OnDestroy()
{
	KillTimer( 1 );

	m_pManager.Release();

	CDialog::OnDestroy();
}

void CSearchManagerDlg::ConnectToWDS()
{
	HRESULT hr = m_pManager.CoCreateInstance( __uuidof( CSearchManager ) );
	if ( SUCCEEDED( hr ) )
	{
		LPWSTR szVersion = NULL;
		hr = m_pManager->GetIndexerVersionStr( &szVersion );
		if ( SUCCEEDED( hr ) )
		{
			CString sTitle;
			GetWindowText( sTitle );
			SetWindowText( sTitle + _T(" - Indexer version: ") + szVersion );
			CoTaskMemFree( szVersion );

			SetTimer( 1, 500, NULL );

			PostMessage( WM_TIMER, 1, 0 );
		}
		else
			m_wndStatus.SetWindowText( m_sStatusCache = _T("Can't get Windows Search\x2122 version.") );
	}
	else
		m_wndStatus.SetWindowText( m_sStatusCache = _T("Can't connect to Windows Search\x2122.") );
}

void CSearchManagerDlg::OnNMClickSysindex(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	CWaitCursor wc;

	CString sSys;
	GetSystemWindowsDirectory( sSys.GetBuffer( MAX_PATH ), MAX_PATH );
	sSys.ReleaseBuffer();
	sSys += _T("\\system32\\");

	ShellExecute( GetSafeHwnd(), NULL, sSys + _T("control.exe"), _T(" /name Microsoft.IndexingOptions"), sSys, SW_SHOWDEFAULT );

	Sleep( 1000 );

	*pResult = 0;
}
