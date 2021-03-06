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

BEGIN_MESSAGE_MAP(CSearchManagerApp, CWinAppEx)
END_MESSAGE_MAP()

CSearchManagerApp::CSearchManagerApp()
	: IsWSearchPresent ( false )
{
	if ( HasServiceState( _T("WSearch") ) == ERROR_SUCCESS )
	{
		IsWSearchPresent = true;
		IndexerService.emplace( _T("WSearch") );
	}
	else if ( HasServiceState( _T("CiSvc") ) == ERROR_SUCCESS )
	{
		IndexerService.emplace( _T("CiSvc") );
	}

	GetModuleFileName( nullptr, ModulePath.GetBuffer( MAX_PATH ), MAX_PATH );
	ModulePath.ReleaseBuffer();

	GetSystemDirectory( SystemDirectory.GetBuffer( MAX_PATH ), MAX_PATH );
	SystemDirectory.ReleaseBuffer();

	if ( SystemDirectory.GetAt( SystemDirectory.GetLength() - 1 ) != _T('\\') )
	{
		SystemDirectory += _T('\\');
	}

	TCHAR folder[ MAX_PATH ] = {};
	DWORD type, folder_size = MAX_PATH * sizeof( TCHAR );
	if ( RegQueryValueFull( HKEY_LOCAL_MACHINE, KEY_SEARCH, _T("DataDirectory"), &type, reinterpret_cast< LPBYTE >( folder ), &folder_size ) == ERROR_SUCCESS )
	{
		PathAddBackslash( folder );

		SearchDirectory.emplace( folder );

		TCHAR expanded[ MAX_PATH ] = {};
		if ( ExpandEnvironmentStrings( SearchDirectory.value(), expanded, MAX_PATH ) )
		{
			PathAddBackslash( expanded );

			SearchDirectory.emplace( expanded  );
		}

		// Check folders for existence
		DWORD attr = GetFileAttributes( SearchDirectory.value() );
		if ( attr != INVALID_FILE_ATTRIBUTES )
		{
			SearchDatabase.emplace( SearchDirectory.value() + _T("Applications\\Windows\\Windows.edb") );

			attr = GetFileAttributes( SearchDatabase.value() );
			if ( attr == INVALID_FILE_ATTRIBUTES )
			{
				SearchDatabase.reset();
			}
		}
		else
		{
			SearchDirectory.reset();
		}
	}

	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}

CSearchManagerApp theApp;

BOOL CSearchManagerApp::InitInstance()
{
	const INITCOMMONCONTROLSEX InitCtrls = { sizeof( INITCOMMONCONTROLSEX ), ICC_WIN95_CLASSES | ICC_USEREX_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS };
	InitCommonControlsEx( &InitCtrls );

	SetAppID( AfxGetAppName() );

	CWinAppEx::InitInstance();

	VERIFY( SUCCEEDED( CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE ) ) );

	SetRegistryKey( AFX_IDS_COMPANY_NAME );

	EnableTaskbarInteraction();

	InitContextMenuManager();

	CMFCVisualManager::SetDefaultManager( RUNTIME_CLASS( CMFCVisualManagerWindows ) );

	{
		CSearchManagerDlg dlg;
		m_pMainWnd = &dlg;
		dlg.DoModal();
	}

	CoUninitialize();

	return FALSE;
}

BOOL CSearchManagerApp::ProcessMessageFilter(int code, LPMSG lpMsg)
{
	CSearchManagerDlg* pMainWnd = static_cast< CSearchManagerDlg* >( m_pMainWnd );
	if ( pMainWnd && ( lpMsg->hwnd == pMainWnd->m_hWnd || ::IsChild( pMainWnd->m_hWnd, lpMsg->hwnd ) ) )
	{
		if ( lpMsg->message == WM_KEYDOWN )
		{
			// Emulate key down message for dialog
			if ( pMainWnd->OnKeyDown( static_cast< UINT >( lpMsg->wParam ), ( lpMsg->lParam & 0xffff ), ( ( lpMsg->lParam >> 16 ) & 0xffff ) ) )
			{
				return TRUE;
			}
		}
	}

	return CWinAppEx::ProcessMessageFilter( code, lpMsg );
}

BOOL IsProcessElevated()
{
	HANDLE hToken = nullptr;
	if ( ! OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) )
	{
		TRACE( _T("OpenProcessToken error: %d\n"), GetLastError() );
		return FALSE;
	}

	TOKEN_ELEVATION elevation = {};
	DWORD dwSize = 0;
	if ( ! GetTokenInformation( hToken, TokenElevation, &elevation, sizeof( elevation ), &dwSize ) )
	{
		TRACE( _T("GetTokenInformation error: %d\n"), GetLastError() );
		CloseHandle( hToken );
		return FALSE;
	}

	CloseHandle( hToken );
	return ( elevation.TokenIsElevated != FALSE );
}

LSTATUS RegOpenKeyFull(HKEY hKey, LPCTSTR lpSubKey, REGSAM samDesired, PHKEY phkResult)
{
	LSTATUS res = RegOpenKeyEx( hKey, lpSubKey, 0, samDesired | KEY_WOW64_64KEY, phkResult );
	if ( res != ERROR_SUCCESS )
	{
		res = RegOpenKeyEx( hKey, lpSubKey, 0, samDesired | KEY_WOW64_32KEY, phkResult );
	}
	if ( res != ERROR_SUCCESS )
	{
		res = RegOpenKeyEx( hKey, lpSubKey, 0, samDesired, phkResult );
	}
	return res;
}

LSTATUS RegCreateKeyFull(HKEY hKey, LPCTSTR lpSubKey, REGSAM samDesired, PHKEY phkResult)
{
	LSTATUS res = RegCreateKeyEx( hKey, lpSubKey, 0, nullptr, 0, samDesired | KEY_WOW64_64KEY, nullptr, phkResult, nullptr );
	if ( res != ERROR_SUCCESS )
	{
		res = RegCreateKeyEx( hKey, lpSubKey, 0, nullptr, 0, samDesired | KEY_WOW64_32KEY, nullptr, phkResult, nullptr );
	}
	if ( res != ERROR_SUCCESS )
	{
		res = RegCreateKeyEx( hKey, lpSubKey, 0, nullptr, 0, samDesired, nullptr, phkResult, nullptr );
	}
	return res;
}

LSTATUS RegQueryValueFull(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpValue, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	HKEY hValueKey;
	LSTATUS res = RegOpenKeyFull( hKey, lpSubKey, KEY_QUERY_VALUE, &hValueKey );
	if ( res == ERROR_SUCCESS )
	{
		res = RegQueryValueEx( hValueKey, lpValue, nullptr, lpType, lpData, lpcbData );
		RegCloseKey( hValueKey );
	}
	return res;
}

LSTATUS RegSetValueFull(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpValue, DWORD dwType, const BYTE* lpData, DWORD cbData)
{
	HKEY hValueKey;
	LSTATUS res = RegOpenKeyFull( hKey, lpSubKey, KEY_SET_VALUE, &hValueKey );
	if ( res == ERROR_SUCCESS )
	{
		res = RegSetValueEx( hValueKey, lpValue, 0, dwType, lpData, cbData );
		RegCloseKey( hValueKey );
	}
	return res;
}

CString ProgIDFromProtocol(LPCTSTR szProtocol)
{
	TCHAR progid[ MAX_PATH ] = {};
	DWORD type, progid_size = sizeof( progid );
	LSTATUS res = RegQueryValueFull( HKEY_LOCAL_MACHINE, KEY_PROTOCOLS, szProtocol, &type, reinterpret_cast< LPBYTE >( progid ), &progid_size );
	if ( res == ERROR_SUCCESS )
	{
		return progid;
	}

	CString prot_key;
	prot_key.Format( _T("%s\\%s\\0"), KEY_PROTOCOLS, szProtocol );
	progid_size = sizeof( progid );
	res = RegQueryValueFull( HKEY_LOCAL_MACHINE, prot_key, _T("ProgIdHandler"), &type, reinterpret_cast< LPBYTE >( progid ), &progid_size );
	if ( res == ERROR_SUCCESS )
	{
		return progid;
	}

	return CString();
}

DWORD StopService(LPCTSTR szService, bool& bWasStarted, bool bDisable)
{
	bWasStarted = false;

	// Read-only access
	DWORD res = HasServiceState( szService, SERVICE_STOPPED );

	// Full access
	if ( res != ERROR_SUCCESS )
	{
		if ( SC_HANDLE scm = OpenSCManager( nullptr, nullptr, SC_MANAGER_ALL_ACCESS ) )
		{
			if ( SC_HANDLE service = OpenService( scm, szService, SERVICE_STOP | SERVICE_QUERY_STATUS | ( bDisable ? SERVICE_CHANGE_CONFIG : 0 ) | SERVICE_ENUMERATE_DEPENDENTS ) )
			{
				if ( bDisable == false ||
					ChangeServiceConfig( service, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) )
				{
					// Try to stop service for 10 seconds
					res = ERROR_TIMEOUT;
					for ( int i = 0; i < 40; ++i )
					{
						SERVICE_STATUS status = {};
						if ( ! QueryServiceStatus( service, &status ) )
						{
							res = GetLastError();
							break;
						}
						if ( status.dwCurrentState == SERVICE_STOPPED )
						{
							res = ERROR_SUCCESS;
							break;
						}

						bWasStarted = true;

						if ( ! ControlService( service, SERVICE_CONTROL_STOP, &status ) )
						{
							res = GetLastError();

							// Check and stop all dependent services
							if ( res == ERROR_DEPENDENT_SERVICES_RUNNING )
							{
								DWORD dwBytesNeeded = 0, dwCount = 0;
								if ( ! EnumDependentServices( service, SERVICE_ACTIVE, nullptr, 0, &dwBytesNeeded, &dwCount ) )
								{
									res = GetLastError();
									if ( res == ERROR_MORE_DATA )
									{
										CAutoVectorPtr< char > buf( new char[ dwBytesNeeded ] );
										LPENUM_SERVICE_STATUS lpDependencies = reinterpret_cast< LPENUM_SERVICE_STATUS >( static_cast< char * >( buf ) );
										if ( EnumDependentServices( service, SERVICE_ACTIVE, lpDependencies, dwBytesNeeded, &dwBytesNeeded, &dwCount ) )
										{
											for ( DWORD j = 0; j < dwCount; ++j )
											{
												bool started = false;
												StopService( lpDependencies[ j ].lpServiceName, started, false );
											}
										}
									}
								}
							}
						}

						SleepEx( 250 , FALSE );
					}
				}
				else
				{
					res = GetLastError();
				}
				CloseServiceHandle( service );
			}
			else
			{
				res = GetLastError();
			}
			CloseServiceHandle( scm );
		}
		else
		{
			res = GetLastError();
		}
	}
	return res;
}

DWORD StartService(LPCTSTR szService)
{
	// Read-only access
	DWORD res = HasServiceState( szService, SERVICE_RUNNING );

	// Full access
	if ( res != ERROR_SUCCESS )
	{
		if ( SC_HANDLE scm = OpenSCManager( nullptr, nullptr, SC_MANAGER_ALL_ACCESS ) )
		{
			if ( SC_HANDLE service = OpenService( scm, szService, SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG ) )
			{
				if ( ChangeServiceConfig( service, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_NO_CHANGE,
					nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) )
				{
					// Try to start service for 10 seconds
					res = ERROR_TIMEOUT;
					for ( int i = 0; i < 40; ++i )
					{
						SERVICE_STATUS status = {};
						if ( ! QueryServiceStatus( service, &status ) )
						{
							res = GetLastError();
							break;
						}
						if ( status.dwCurrentState == SERVICE_RUNNING )
						{
							res = ERROR_SUCCESS;
							break;
						}
						if ( ! StartService( service, 0, nullptr ) )
						{
							res = GetLastError();
						}
						SleepEx( 250 , FALSE );
					}
				}
				else
				{
					res = GetLastError();
				}
				CloseServiceHandle( service );
			}
			else
			{
				res = GetLastError();
			}
			CloseServiceHandle( scm );
		}
		else
		{
			res = GetLastError();
		}
	}
	return res;
}

DWORD HasServiceState(LPCTSTR szService, DWORD dwState)
{
	DWORD res = ERROR_SUCCESS;
	if ( SC_HANDLE scm = OpenSCManager( nullptr, nullptr, SC_MANAGER_CONNECT ) )
	{
		if ( SC_HANDLE service = OpenService( scm, szService, SERVICE_QUERY_STATUS ) )
		{
			if ( dwState )
			{
				SERVICE_STATUS status = {};
				if ( QueryServiceStatus( service, &status ) )
				{
					res = ( status.dwCurrentState == dwState ) ? ERROR_SUCCESS : ERROR_INVALID_DATA;
				}
				else
				{
					res = GetLastError();
				}
			}
			CloseServiceHandle( service );
		}
		else
		{
			res = GetLastError();
		}
		CloseServiceHandle( scm );
	}
	else
	{
		res = GetLastError();
	}
	return res;
}

LSTATUS RegRenumberKeys(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpNumberKey)
{
	HKEY hRootsKey;
	LSTATUS res = RegOpenKeyFull( hKey, lpSubKey, KEY_ALL_ACCESS, &hRootsKey );
	if ( res == ERROR_SUCCESS )
	{
		DWORD count = 0;
		DWORD type, count_size = sizeof( DWORD );
		res = RegQueryValueEx( hRootsKey, lpNumberKey, nullptr, &type, reinterpret_cast< LPBYTE >( &count ), &count_size );
		if ( res == ERROR_SUCCESS && type == REG_DWORD )
		{
			TRACE( _T("Read count value %s: %u\n"), lpSubKey, count );

			std::set< DWORD > keys;

			TCHAR name[ MAX_PATH ] = {};
			for ( DWORD i = 0; ; ++i )
			{
				name[ 0 ] = 0;
				DWORD name_size = _countof( name );
				res = SHEnumKeyEx( hRootsKey, i, name, &name_size );
				if ( res != ERROR_SUCCESS )
				{
					res = ERROR_SUCCESS;
					break;
				}

				DWORD number = 0;
				if ( _stscanf_s( name, _T("%u"), &number ) != 1 )
				{
					TRACE( _T("Abort. Found non-number key: %s\n"), name );
					res = ERROR_INVALID_DATA;
					break;
				}

				if ( keys.find( number ) != keys.end() )
				{
					TRACE( _T("Abort. Found duplicate key: %s\n"), name );
					res = ERROR_INVALID_DATA;
					break;
				}

				keys.emplace( number );
			}

			if ( res == ERROR_SUCCESS )
			{
				TRACE( _T("Actual count value: %u\n"), keys.size() );

				DWORD number = 0;
				for ( auto i = keys.begin(); i != keys.end() && res == ERROR_SUCCESS; ++i, ++number )
				{
					if ( number != *i )
					{
						CString old_key;
						old_key.Format( _T("%u"), *i );

						CString new_key;
						new_key.Format( _T("%u"), number );

						TRACE( _T("Renaming: %s -> %s\n"), (LPCTSTR)old_key, (LPCTSTR)new_key );

						HKEY hNewKey;
						res = RegCreateKeyFull( hRootsKey, new_key, KEY_ALL_ACCESS, &hNewKey );
						if ( res == ERROR_SUCCESS )
						{
							res = SHCopyKey( hRootsKey, old_key, hNewKey, 0 );
							if ( res == ERROR_SUCCESS )
							{
								res = SHDeleteKey( hRootsKey, old_key );
								if ( res != ERROR_SUCCESS )
								{
									TRACE( _T("Failed to delete key \"%s\": %s\n"), (LPCTSTR)old_key, GetErrorEx( res ) );
								}
							}
							else
							{
								TRACE( _T("Failed to copy key \"%s\": %s\n"), (LPCTSTR)old_key, GetErrorEx( res ) );
							}
							RegCloseKey( hNewKey );
						}
						else
						{
							TRACE( _T("Failed to open key: %s\n"), GetErrorEx( res ) );
						}
					}
				}

				if ( res == ERROR_SUCCESS && count != number )
				{
					TRACE( _T("Setting new count: %u -> %u\n"), count, number );

					res = RegSetValueEx( hRootsKey, lpNumberKey, 0, REG_DWORD, reinterpret_cast< const BYTE* >( &number ), sizeof( DWORD ) );
					if ( res != ERROR_SUCCESS )
					{
						TRACE( _T("Failed to set count value \"%s\": %s\n"), lpSubKey, GetErrorEx( res ) );
					}
				}
			}
		}
		else
		{
			TRACE( _T("Failed to read count value \"%s\": %s\n"), lpSubKey, GetErrorEx( res ) );
		}
		RegCloseKey( hRootsKey );
	}
	else
	{
		TRACE( _T("Failed to open key \"%s\": %s\n"), lpSubKey, GetErrorEx( res ) );
	}

	return res;
}
