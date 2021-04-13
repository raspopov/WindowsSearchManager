#pragma once

// Class to login as process

class CAsProcess
{
public:
	CAsProcess(DWORD dwProcessID) noexcept
		: hProcess	( nullptr )
		, hToken	( nullptr )
		, bResult	( false )
	{
		if ( oToken.GetProcessToken( TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY ) )
		{
			// For process enumeration
			if ( ! oToken.EnablePrivilege( SE_DEBUG_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_DEBUG_NAME) error: %d\n"), GetLastError() );
			}

			// For token access under Win2K/WinXP
			if ( ! oToken.EnablePrivilege( SE_TAKE_OWNERSHIP_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_TAKE_OWNERSHIP_NAME) error: %d\n"), GetLastError() );
			}
			if ( ! oToken.EnablePrivilege( SE_SECURITY_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_SECURITY_NAME) error: %d\n"), GetLastError() );
			}
		}
		else
		{
			TRACE( _T("GetProcessToken error: %d\n"), GetLastError() );
		}

		// Open process to steal its token
		hProcess = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, dwProcessID );
		if ( hProcess )
		{
			// Need a token with impersonation rights
			hToken = OpenProcessToken( hProcess, TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE );
			if ( hToken )
			{
				// Login
				if ( ImpersonateLoggedOnUser( hToken ) )
				{
					bResult = true;
					return;
				}
				else
				{
					TRACE( _T("ImpersonateLoggedOnUser error: %u\n"), GetLastError() );
				}
				CloseHandle( hToken );
			}
			else
			{
				TRACE( _T("OpenProcessToken error: %u\n"), GetLastError() );
			}
			CloseHandle( hProcess );
		}
		else
		{
			TRACE( _T("OpenProcess error: %u\n"), GetLastError() );
		}
	}

	inline operator bool() const noexcept
	{
		return bResult;
	}

	~CAsProcess() noexcept
	{
		if ( bResult )
		{
			RevertToSelf();

			CloseHandle( hToken );
			CloseHandle( hProcess );
		}
	}

	static CAsProcess* RunAsSystem()
	{
		const static LPCTSTR victims[] = { _T("lsass.exe"),  _T("winlogon.exe"), _T("smss.exe"), _T("csrss.exe"), _T("services.exe") };

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

						// Looking for victims
						for ( auto victim : victims )
						{
							if ( _tcsicmp( victim, name ) == 0 )
							{
								CAutoPtr< CAsProcess > process( new CAsProcess( pe32.th32ProcessID ) );
								if ( *process )
								{
									TRACE( _T("Run as process: %s (PID: %d)\n"), name, pe32.th32ProcessID );
									CloseHandle( hProcessSnap );
									return process.Detach();
								}

								TRACE( _T("Skipped process: %s (PID: %d)\n"), name, pe32.th32ProcessID );
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
	CAccessToken oToken;
	HANDLE hProcess;
	HANDLE hToken;
	bool bResult;

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
									TRACE( _T("Got it!\n") );
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
