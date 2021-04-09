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

class CSearchManagerApp : public CWinAppEx
{
public:
	CSearchManagerApp();

protected:
	BOOL InitInstance() override;
	BOOL ProcessMessageFilter(int code, LPMSG lpMsg) override;

	DECLARE_MESSAGE_MAP()
};

extern CSearchManagerApp theApp;

inline CString LoadString(UINT nID)
{
	CString str;
	VERIFY( str.LoadString( nID ) );
	return str;
}
