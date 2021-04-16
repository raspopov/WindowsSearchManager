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

struct error_t
{
	CString error;
	CString msg;

	inline operator CString() const
	{
		return msg + _T(' ') + error;
	}

	error_t(HRESULT hr = HRESULT_FROM_WIN32( GetLastError() ))
	{
		static LPCTSTR szModules [] =
		{
			_T("tquery.dll"),
			_T("oledb32.dll"),
			_T("lsasrv.dll"),
			_T("user32.dll"),
			_T("netmsg.dll"),
			_T("netevent.dll"),
			_T("wininet.dll"),
			_T("ntdll.dll"),
			_T("mprmsg.dll"),
			_T("IoLogMsg.dll")
		};

		LPTSTR lpszTemp = nullptr;
		if ( ! FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0,
			reinterpret_cast< LPTSTR >( &lpszTemp ), 0, nullptr ) )
		{
			for ( auto & szModule : szModules )
			{
				if ( HMODULE hModule = LoadLibraryEx( szModule, nullptr, LOAD_LIBRARY_AS_DATAFILE ) )
				{
					const BOOL res = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE, hModule, hr, 0,
						reinterpret_cast< LPTSTR >( &lpszTemp ), 0, nullptr );

					FreeLibrary( hModule );

					if ( res )
					{
						break;
					}
					else if ( GetLastError() == ERROR_RESOURCE_TYPE_NOT_FOUND )
					{
						TRACE( _T("Library has no message table: %s\n"), szModule );
					}
				}
				else
				{
					TRACE( _T("Library not found: %s\n"), szModule );
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
				const static CString admin = _T(" ") + LoadString( IDS_ADMIN );
				msg += admin;
			}
		}
		else
		{
			const static CString unknown = LoadString( IDS_UNKNOWN_ERROR );
			msg = unknown;
		}

		const static CString code = LoadString( IDS_ERROR_CODE );
		error.Format( code, hr );
	}
};

#define GetError()		( static_cast< LPCTSTR >( static_cast< CString >( error_t() ) ) )
#define GetErrorEx(x)	( static_cast< LPCTSTR >( static_cast< CString >( error_t( HRESULT_FROM_WIN32( x ) ) ) ) )
