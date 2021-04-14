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

// Class to login as process

#define TRUSTED_INSTALLER_PROCESS	_T("TrustedInstaller.exe")
#define TRUSTED_INSTALLER_SERVICE	_T("TrustedInstaller")

class CAsProcess : public CAccessToken
{
public:
	CAsProcess(DWORD dwProcessID) noexcept
	{
		if ( Access.GetProcessToken( TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY ) )
		{
			// For process enumeration
			if ( ! Access.EnablePrivilege( SE_DEBUG_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_DEBUG_NAME) error: %d\n"), GetLastError() );
			}

			// For token access under Win2K/WinXP
			if ( ! Access.EnablePrivilege( SE_TAKE_OWNERSHIP_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_TAKE_OWNERSHIP_NAME) error: %d\n"), GetLastError() );
			}
			if ( ! Access.EnablePrivilege( SE_SECURITY_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_SECURITY_NAME) error: %d\n"), GetLastError() );
			}

			if ( ! Access.EnablePrivilege( SE_IMPERSONATE_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_IMPERSONATE_NAME) error: %d\n"), GetLastError() );
			}
		}
		else
		{
			TRACE( _T("GetProcessToken error: %d\n"), GetLastError() );
		}

		// Open process to steal its token
		Process.Attach( OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, dwProcessID ) );
		if ( Process )
		{
			// Need a token with impersonation rights
			Attach( OpenProcessToken( Process, TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE ) );
			if ( m_hToken )
			{
				// OK
			}
			else
			{
				TRACE( _T("OpenProcessToken error: %u\n"), GetLastError() );
				Process.Close();
			}
		}
		else
		{
			TRACE( _T("OpenProcess error: %u\n"), GetLastError() );
		}
	}

	inline operator bool() const noexcept
	{
		return ( m_hToken != nullptr );
	}

	static CAsProcess* RunAsTrustedInstaller()
	{
		// At start System privilege needed
		CAutoPtr< CAsProcess > process( RunAsSystem() );
		if ( process )
		{
			// Try to start TrustedInstaller service
			if ( SC_HANDLE scm = OpenSCManager( nullptr, nullptr, SC_MANAGER_CONNECT ) )
			{
				if ( SC_HANDLE service = OpenService( scm, TRUSTED_INSTALLER_SERVICE, SERVICE_START | SERVICE_QUERY_STATUS ) )
				{
					// Try to start service for 10 seconds
					for ( int i = 0; i < 40; ++i )
					{
						SERVICE_STATUS status = {};
						if ( ! QueryServiceStatus( service, &status ) )
						{
							break;
						}
						if ( status.dwCurrentState == SERVICE_RUNNING )
						{
							break;
						}
						if ( ! StartService( service, 0, nullptr ) )
						{
							break;
						}
						SleepEx( 250 , FALSE );
					}
					CloseServiceHandle( service );
				}
				CloseServiceHandle( scm );
			}

			// Looking for trusted installer process
			CAutoPtr< CAsProcess > trusted( RunAs( TRUSTED_INSTALLER_PROCESS ) );
			if ( trusted )
			{
				if ( trusted->ImpersonateLoggedOnUser() )
				{
					TRACE( _T("Impersonated as: %s\n"), TRUSTED_INSTALLER_PROCESS );

					process = trusted;
				}
				else
				{
					trusted.Free();
				}
			}

			// Re-impersonate
			process->Revert();
			VERIFY( process->ImpersonateLoggedOnUser() );

			return process.Detach();
		}

		return nullptr;
	}

	static CAsProcess* RunAsSystem()
	{
		// Looking for system processes
		const static LPCTSTR victims[] = { _T("lsass.exe"),  _T("winlogon.exe") };
		for ( auto victim : victims )
		{
			CAutoPtr< CAsProcess > process( RunAs( victim ) );
			if ( process && process->ImpersonateLoggedOnUser() )
			{
				TRACE( _T("Impersonated as: %s\n"), victim );
				return process.Detach();
			}
		}
		return nullptr;
	}

	static CAsProcess* RunAs(LPCTSTR target)
	{
		// Enumerate all processes
		PROCESSENTRY32 pe32 = { sizeof( PROCESSENTRY32 ) };
		HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if ( hProcessSnap != INVALID_HANDLE_VALUE )
		{
			if ( Process32First( hProcessSnap, &pe32 ) )
			{
				do
				{
					// Skip System
					if ( pe32.th32ProcessID )
					{
						LPCTSTR name = PathFindFileName( pe32.szExeFile );
						if ( _tcsicmp( target, name ) == 0 )
						{
							CAutoPtr< CAsProcess > process( new CAsProcess( pe32.th32ProcessID ) );
							if ( *process )
							{
								TRACE( _T("Got process: %s (PID: %d)\n"), name, pe32.th32ProcessID );
								CloseHandle( hProcessSnap );
								return process.Detach();
							}
							else
							{
								TRACE( _T("Failed to get process: %s (PID: %d)\n"), name, pe32.th32ProcessID );
							}
						}
					}
				}
				while ( Process32Next( hProcessSnap, &pe32 ) );
			}
			else
			{
				TRACE( _T("Process32First error: %u\n"), GetLastError() );
			}
			CloseHandle( hProcessSnap );
		}
		else
		{
			TRACE( _T("CreateToolhelp32Snapshot(Process) error: %u\n"), GetLastError() );
		}
		return nullptr;
	}

private:
	CAccessToken	Access;		// Own process token
	CHandle			Process;	// Target process handler

	static HANDLE OpenProcessToken(HANDLE hProcess, DWORD dwAccess)
	{
		// Try good way to get a token
		HANDLE hToken = nullptr;
		if ( ::OpenProcessToken( hProcess, dwAccess, &hToken ) )
		{
			return hToken;
		}

		// Now try a bad way...

		// Get the own SID from the self
		HANDLE hSelfProcess = GetCurrentProcess();
		HANDLE hSelfToken = nullptr;
		if ( ::OpenProcessToken( hSelfProcess, TOKEN_READ, &hSelfToken ) )
		{
			BYTE pBuf[ 512 ] = {};
			DWORD dwLen = sizeof( pBuf );
			if ( GetTokenInformation( hSelfToken, TokenUser, pBuf, dwLen, &dwLen ) )
			{
				// Making a DACL member with granted access for the own SID
				EXPLICIT_ACCESS ea =
				{
					dwAccess,
					GRANT_ACCESS,
					NO_INHERITANCE,
					{
						nullptr,
						NO_MULTIPLE_TRUSTEE,
						TRUSTEE_IS_SID,
						TRUSTEE_IS_USER,
						(LPTSTR)( (TOKEN_USER*)pBuf )->User.Sid
					}
				};

				// Open token with DACL rewrite rights (need a privilege)
				if ( ::OpenProcessToken( hProcess, TOKEN_READ | WRITE_OWNER | WRITE_DAC, &hToken ) )
				{
					// Get current DACL
					PACL pOrigDacl = nullptr;
					PSECURITY_DESCRIPTOR pSD = nullptr;
					if ( GetSecurityInfo( hToken, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pOrigDacl, nullptr, &pSD ) == ERROR_SUCCESS )
					{
						// Adding own SID to the DACL
						PACL pNewDacl = nullptr;
						if ( SetEntriesInAcl( 1, &ea, pOrigDacl, &pNewDacl ) == ERROR_SUCCESS )
						{
							// Set a new DACL (permanent!)
							if ( SetSecurityInfo( hToken, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDacl, nullptr ) == ERROR_SUCCESS )
							{
								// Reopen it
								if ( DuplicateHandle( hSelfProcess, hToken, hSelfProcess, &hToken, dwAccess, FALSE, DUPLICATE_CLOSE_SOURCE ) )
								{
									// OK
								}
								else
								{
									hToken = nullptr;
								}
							}
							LocalFree( pNewDacl );
						}
						LocalFree( pSD );
					}
				}
			}
			CloseHandle( hSelfToken );
		}
		return hToken;
	}
};
