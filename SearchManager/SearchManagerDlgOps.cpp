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

void CSearchManagerDlg::AddRoot(const CString& sURL)
{
	if ( ! m_pScope )
	{
		return;
	}

	CUrlDialog dlg( this );
	dlg.m_sTitle = LoadString( IDS_ADD_ROOT );
	if ( ! sURL.IsEmpty() )
	{
		dlg.m_sURL = sURL;
	}
	dlg.m_sInfo = LoadString( IDS_ADD_ROOT_INFO );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() && ( sURL.IsEmpty() || dlg.m_sURL.CompareNoCase( sURL ) != 0 ) )
	{
		CWaitCursor wc;

		// Delete old root if specified
		HRESULT hr = S_OK;
		if ( ! sURL.IsEmpty() )
		{
			CRoot root( TRUE, sURL );
			hr = root.DeleteFrom( m_pScope );
		}

		// Add new root
		if ( SUCCEEDED( hr ) )
		{
			CRoot root( TRUE, dlg.m_sURL );
			hr = root.AddTo( m_pScope );
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

void CSearchManagerDlg::AddRule(BOOL bInclude, BOOL bDefault, const CString& sURL)
{
	if ( ! m_pScope )
	{
		return;
	}

	CUrlDialog dlg( this );
	dlg.m_sTitle.Format( LoadString( IDS_ADD_SCOPE ),
		(LPCTSTR) LoadString( bInclude ? IDS_RULE_INCLUDE : IDS_RULE_EXCLUDE ),
		(LPCTSTR) LoadString( bDefault ? IDS_RULE_DEFAULT : IDS_RULE_USER ) );
	if ( ! sURL.IsEmpty() )
	{
		dlg.m_sURL = sURL;
	}
	dlg.m_sInfo = LoadString( IDS_ADD_SCOPE_INFO );
	if ( dlg.DoModal() == IDOK && ! dlg.IsEmpty() && ( sURL.IsEmpty() || dlg.m_sURL.CompareNoCase( sURL ) != 0 ) )
	{
		CWaitCursor wc;

		// Delete old scope rule if specified
		HRESULT hr = S_OK;
		if ( ! sURL.IsEmpty() )
		{
			CRule rule( bInclude, bDefault, sURL );
			hr = rule.DeleteFrom( m_pScope );
		}

		// Add new scope rule
		if ( SUCCEEDED( hr ) )
		{
			CRule rule( bInclude, bDefault, dlg.m_sURL );
			hr = rule.AddTo( m_pScope );
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

void CSearchManagerDlg::Delete()
{
	// Enumerate items to delete
	std::list< const CItem* > to_delete;
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			to_delete.push_back( item );
		}
	}

	// Delete items
	if ( ! to_delete.empty() )
	{
		CString msg;
		msg.Format( IDS_DELETE_CONFIRM, to_delete.size() );
		if ( AfxMessageBox( msg, MB_YESNO | MB_ICONQUESTION ) == IDYES )
		{
			const static CString deleting = LoadString( IDS_DELETING );
			SetStatus( deleting );

			// Step 1 - Delete from Windows Search
			for ( auto pos = to_delete.begin(); pos != to_delete.end(); )
			{
				const auto item = *pos;

				if ( item->Group == GROUP_OFFLINE_RULES || item->Group == GROUP_OFFLINE_ROOTS )
				{
					// Try step 1 first but else skip to step 2
					if ( m_pScope )
					{
						HRESULT hr = item->DeleteFrom( m_pScope );
						if ( SUCCEEDED( hr ) )
						{
							hr = m_pScope->SaveAll();
							if ( SUCCEEDED( hr ) )
							{
								pos = to_delete.erase( pos );

								m_bRefresh = true;

								continue;
							}
						}
					}

					++pos;
				}
				else if ( ! m_pScope )
				{
					// Cancel deletion
					to_delete.clear();
					AfxMessageBox( LoadString( IDS_CLOSED ), MB_OK | MB_ICONHAND );
					break;
				}
				else
				{
					pos = to_delete.erase( pos );

					for (;;)
					{
						CWaitCursor wc;
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
							break;
						}
						else
						{
							const error_t result( hr );
							const int res = AfxMessageBox( result.msg + _T("\n\n") + result.error + _T("\n\n") + item->URL,
								MB_ABORTRETRYIGNORE | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONEXCLAMATION ) );
							if ( res == IDRETRY )
							{
								// Retry deletion
								continue;
							}
							else if ( res == IDIGNORE )
							{
								// Ignore error and continue deletion
								break;
							}
							else
							{
								// Cancel deletion
								to_delete.clear();
								break;
							}
						}
					}
				}
			}

			// Step 2 - Delete from registry
			if ( ! to_delete.empty() )
			{
				if ( StopWindowsSearch() )
				{
					SetStatus( deleting );

					{
						// Try to get System privileges
						CAutoPtr< CAsProcess >sys( CAsProcess::RunAsTrustedInstaller() );

						for ( auto pos = to_delete.begin(); pos != to_delete.end(); ++pos )
						{
							const auto item = *pos;

							ASSERT( item->Group == GROUP_OFFLINE_RULES || item->Group == GROUP_OFFLINE_ROOTS );

							for (;;)
							{
								CWaitCursor wc;

								HRESULT hr = item->DeleteFrom( nullptr );
								if ( SUCCEEDED( hr ) )
								{
									break;
								}
								else
								{
									const error_t result( hr );
									const int res = AfxMessageBox( result.msg + _T("\n\n") + result.error + _T("\n\n") + item->URL,
										MB_ABORTRETRYIGNORE | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONEXCLAMATION ) );
									if ( res == IDRETRY )
									{
										// Retry deletion
										continue;
									}
									else if ( res == IDIGNORE )
									{
										// Ignore error and continue deletion
										break;
									}
									else
									{
										// Cancel deletion
										to_delete.clear();
										break;
									}
								}
							}
						}
					}

					// Re-number keys
					RegRenumberKeys( HKEY_LOCAL_MACHINE, KEY_SEARCH_ROOTS, VALUE_ITEM_COUNT );
					RegRenumberKeys( HKEY_LOCAL_MACHINE, KEY_DEFAULT_RULES, VALUE_ITEM_COUNT );
					RegRenumberKeys( HKEY_LOCAL_MACHINE, KEY_WORKING_RULES, VALUE_ITEM_COUNT );

					VERIFY( RevertToSelf() );

					StartWindowsSearch();

					// Update catalog
					Default( false );
				}
			}
		}
	}
}

void CSearchManagerDlg::Reindex()
{
	if ( ! m_pCatalog )
	{
		return;
	}

	// Enumerate items to re-index
	for ( POSITION pos = m_wndList.GetFirstSelectedItemPosition(); pos; )
	{
		const POSITION prev = pos;
		const int index = m_wndList.GetNextSelectedItem( pos );
		if ( auto item = reinterpret_cast< const CItem* >( m_wndList.GetItemData( index ) ) )
		{
			HRESULT hr = item->Reindex( m_pCatalog );
			if ( hr != S_OK )
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

void CSearchManagerDlg::ReindexAll()
{
	if ( AfxMessageBox( IDS_REINDEX_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		CComPtr< ISearchManager > pManager;
		HRESULT hr = pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( SUCCEEDED( hr ) )
		{
			// Get catalog
			CComPtr< ISearchCatalogManager > pCatalog;
			hr = pManager->GetCatalog( CATALOG_NAME, &pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				hr = pCatalog->Reindex();
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Reset()
{
	if ( AfxMessageBox( IDS_RESET_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		CComPtr< ISearchManager > pManager;
		HRESULT hr = pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( SUCCEEDED( hr ) )
		{
			// Get catalog
			CComPtr< ISearchCatalogManager > pCatalog;
			hr = pManager->GetCatalog( CATALOG_NAME, &pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				hr = pCatalog->Reset();
			}
		}

		const error_t result( hr );
		AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Rebuild()
{
	CString folder = GetSearchDirectory();
	if ( ! folder.IsEmpty() )
	{
		if ( AfxMessageBox( IDS_REBUILD_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
		{
			if ( StopWindowsSearch() )
			{
				// Delete Windows Search directory
				const static CString status = LoadString( IDS_INDEXER_DELETING );
				SetStatus( status );

				{
					CWaitCursor wc;

					// Try to get System privileges
					CAutoPtr< CAsProcess >sys( CAsProcess::RunAsTrustedInstaller() );

					folder.AppendChar( 0 ); // double null terminated for SHFileOperation
					SHFILEOPSTRUCT fop = { GetSafeHwnd(), FO_DELETE, static_cast< LPCTSTR >( folder ), nullptr, FOF_ALLOWUNDO | FOF_NOCONFIRMATION };
					if ( GetFileAttributes( folder ) == INVALID_FILE_ATTRIBUTES || SHFileOperation( &fop ) == 0 )
					{
						// Set an option to revert to initial state
						const DWORD zero = 0;
						DWORD res = RegSetValueFull( HKEY_LOCAL_MACHINE, KEY_SEARCH, _T("SetupCompletedSuccessfully"), REG_DWORD,
							reinterpret_cast< const BYTE* >( &zero ), sizeof( zero ) );
						if ( res != ERROR_SUCCESS )
						{
							const error_t error( HRESULT_FROM_WIN32( res ) );
							AfxMessageBox( LoadString( IDS_INDEXER_REG_ERROR ) + _T("\n\n") + error.msg + _T("\n\n") + error.error, MB_OK | MB_ICONHAND );
						}
					}
				}

				VERIFY( RevertToSelf() );

				StartWindowsSearch();
			}
		}
	}
}

void CSearchManagerDlg::Defrag()
{
	const static CString prompt = LoadString( IDS_DEFRAG_CONFIRM );
	const static CString status = LoadString( IDS_INDEXER_DEFRAG );
	DatabaseOperation( prompt, status, _T("/d") );
}

void CSearchManagerDlg::Check()
{
	const static CString prompt = LoadString( IDS_CHECK_CONFIRM );
	const static CString status = LoadString( IDS_INDEXER_CHECK );
	DatabaseOperation( prompt, status, _T("/g") );
}

void CSearchManagerDlg::DatabaseOperation(const CString& prompt, const CString& status, const CString& options)
{
	CString system_dir;
	GetSystemDirectory( system_dir.GetBuffer( MAX_PATH ), MAX_PATH );
	system_dir.ReleaseBuffer();

	CString folder = GetSearchDirectory();
	if ( ! folder.IsEmpty() )
	{
		if ( AfxMessageBox( prompt, MB_YESNO | MB_ICONQUESTION ) == IDYES )
		{
			if ( StopWindowsSearch() )
			{
				// Start
				SetStatus( status );

				{
					CWaitCursor wc;

					// Try to get System privileges
					CAutoPtr< CAsProcess >sys( CAsProcess::RunAsTrustedInstaller() );

					const CString cmd = system_dir + _T("\\cmd.exe");
					const CString params = _T("/k ") + system_dir + _T("\\esentutl.exe ") + options +
						_T(" \"") + folder + _T("Applications\\Windows\\Windows.edb\" /o");

					SHELLEXECUTEINFO sei = { sizeof( SHELLEXECUTEINFO ), SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC,
						GetSafeHwnd(), nullptr, cmd, params, system_dir, SW_SHOWNORMAL };
					if ( ShellExecuteEx( &sei ) && sei.hProcess )
					{
						// Wait for termination
						do
						{
							MSG msg;
							while ( PeekMessage( &msg, NULL, NULL, NULL, PM_REMOVE ) )
							{
								TranslateMessage( &msg );
								DispatchMessage( &msg );
							}
						}
						while ( MsgWaitForMultipleObjects( 1, &sei.hProcess, FALSE, INFINITE, QS_ALLEVENTS ) == WAIT_OBJECT_0 + 1 );

						CloseHandle( sei.hProcess );
					}
					else
					{
						const error_t error;
						AfxMessageBox( error.msg + _T("\n\n") + error.error, MB_OK | MB_ICONHAND );
					}
				}

				VERIFY( RevertToSelf() );

				StartWindowsSearch();
			}
		}
	}
}

void CSearchManagerDlg::Default(bool bInteractive)
{
	if ( ! bInteractive || AfxMessageBox( IDS_DEFAULT_CONFIRM, MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		CComPtr< ISearchManager > pManager;
		HRESULT hr = pManager.CoCreateInstance( __uuidof( CSearchManager ) );
		if ( SUCCEEDED( hr ) )
		{
			// Get catalog
			CComPtr< ISearchCatalogManager > pCatalog;
			hr = pManager->GetCatalog( CATALOG_NAME, &pCatalog );
			if ( SUCCEEDED( hr ) )
			{
				// Get scope
				CComPtr< ISearchCrawlScopeManager >	pScope;
				hr = pCatalog->GetCrawlScopeManager( &pScope );
				if ( SUCCEEDED( hr ) )
				{
					hr = pScope->RevertToDefaultScopes();
					if ( SUCCEEDED( hr ) )
					{
						const HRESULT hr_save = pScope->SaveAll();
						if ( hr == S_OK || FAILED( hr_save ) )
						{
							hr = hr_save;
						}
					}
				}
			}
		}

		if ( bInteractive )
		{
			const error_t result( hr );
			AfxMessageBox( result.msg + _T("\n\n") + result.error, MB_OK | ( SUCCEEDED( hr ) ? MB_ICONINFORMATION : MB_ICONERROR ) );
		}

		m_bRefresh = true;
	}
}

void CSearchManagerDlg::Explore()
{
	CString system_dir;
	GetSystemDirectory( system_dir.GetBuffer( MAX_PATH ), MAX_PATH );
	system_dir.ReleaseBuffer();

	const CString folder = GetSearchDirectory();
	if ( ! folder.IsEmpty() )
	{
		CWaitCursor wc;

		const CString cmd = system_dir + _T("\\cmd.exe");
		const CString params = CString( _T("/k cd /d \"") ) + folder + _T("Applications\\Windows\\\" && dir");

		SHELLEXECUTEINFO sei = { sizeof( SHELLEXECUTEINFO ), SEE_MASK_DEFAULT, GetSafeHwnd(), nullptr, cmd, params, system_dir, SW_SHOWNORMAL };
		if ( ShellExecuteEx( &sei ) )
		{
			SleepEx( 500, FALSE );
		}
		else
		{
			const error_t error;
			AfxMessageBox( error.msg + _T("\n\n") + error.error, MB_OK | MB_ICONHAND );
		}
	}
}

bool CSearchManagerDlg::StopWindowsSearch()
{
	CWaitCursor wc;

	Disconnect();

	const static CString status = LoadString( IDS_INDEXER_STOPPING );
	SetStatus( status );

	m_bRefresh = true;

	const DWORD res = StopService( INDEXER_SERVICE );
	if ( res == ERROR_SUCCESS )
	{
		SleepEx( 2000, FALSE );
		return true;
	}
	else
	{
		const error_t error( HRESULT_FROM_WIN32( res ) );
		AfxMessageBox( LoadString( IDS_INDEXER_STOP_ERROR ) + _T("\n\n") + error.msg + _T("\n\n") + error.error, MB_OK | MB_ICONHAND );
		return false;
	}
}

bool CSearchManagerDlg::StartWindowsSearch()
{
	CWaitCursor wc;

	const static CString status = LoadString( IDS_INDEXER_STARTING );
	SetStatus( status );

	m_bRefresh = true;

	const DWORD res = StartService( INDEXER_SERVICE );
	if ( res == ERROR_SUCCESS )
	{
		SleepEx( 250, FALSE );
		return true;
	}
	else
	{
		const error_t error( HRESULT_FROM_WIN32( res ) );
		AfxMessageBox( LoadString( IDS_INDEXER_START_ERROR ) + _T("\n\n") + error.msg + _T("\n\n") + error.error, MB_OK | MB_ICONHAND );
		return false;
	}
}
