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

#include "resource.h"

#ifndef _WIN64
	#define BITNESS _T("32-bit")
#else
	#define BITNESS _T("64-bit")
#endif

inline CString LoadString(UINT nID)
{
	CString str;
	VERIFY( str.LoadString( nID ) );
	return str;
}

inline CString StringFromGUID(REFGUID guid)
{
	CString str;
	LPOLESTR str_guid = nullptr;
	if ( SUCCEEDED( StringFromIID( guid, &str_guid ) ) )
	{
		str = str_guid;
		CoTaskMemFree( str_guid );
	}
	return str;
}

#include "System.h"
#include "Error.h"

#define CRLF				_T("\r\n")
#define ARROW				_T(" \x2192 ")

#define CATALOG_NAME		_T("SystemIndex")
#define DEFAULT_PROTOCOL	_T("defaultroot")
#define FILE_PROTOCOL		_T("file")

#define INDEXER_VOLUME		_T("System Volume Information\\IndexerVolumeGuid")

#define KEY_SEARCH			_T("SOFTWARE\\Microsoft\\Windows Search")
#define KEY_PROTOCOLS		KEY_SEARCH _T("\\Gather\\Windows\\SystemIndex\\Protocols")
#define KEY_SEARCH_ROOTS	KEY_SEARCH _T("\\CrawlScopeManager\\Windows\\SystemIndex\\SearchRoots")
#define KEY_DEFAULT_RULES	KEY_SEARCH _T("\\CrawlScopeManager\\Windows\\SystemIndex\\DefaultRules")
#define KEY_WORKING_RULES	KEY_SEARCH _T("\\CrawlScopeManager\\Windows\\SystemIndex\\WorkingSetRules")

#define VALUE_ITEM_COUNT	_T("ItemCount")

class CSearchManagerApp : public CWinAppEx
{
public:
	CSearchManagerApp();

	CString IndexerService;
	CString	ModulePath;

protected:
	BOOL InitInstance() override;
	BOOL ProcessMessageFilter(int code, LPMSG lpMsg) override;

	DECLARE_MESSAGE_MAP()
};

extern CSearchManagerApp theApp;

// Check administrative rights
BOOL IsProcessElevated();

// Open registry key in 64-bit or 32-bit parts of the registry
LSTATUS RegOpenKeyFull(HKEY hKey, LPCTSTR lpSubKey, REGSAM samDesired, PHKEY phkResult);

// Create registry key in 64-bit or 32-bit parts of the registry
LSTATUS RegCreateKeyFull(HKEY hKey, LPCTSTR lpSubKey, REGSAM samDesired, PHKEY phkResult);

// Read registry key in 64-bit or 32-bit parts of the registry
LSTATUS RegQueryValueFull(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpValue, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

// Write registry key in 64-bit or 32-bit parts of the registry
LSTATUS RegSetValueFull(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpValue, DWORD dwType, const BYTE* lpData, DWORD cbData);

// Get Windows Search protocol handler ProgID
CString ProgIDFromProtocol(LPCTSTR szProtocol);

// Get Windows Search directory
CString GetSearchDirectory();

// Disable and stop service
DWORD StopService(LPCTSTR szService, bool& bWasStarted);

// Enable and start service
DWORD StartService(LPCTSTR szService);

// Check service state
DWORD HasServiceState(LPCTSTR szService, DWORD dwState);

// Renumber registry keys
LSTATUS RegRenumberKeys(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpNumberKey);
