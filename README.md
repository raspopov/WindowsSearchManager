[![Build status](https://ci.appveyor.com/api/projects/status/8drxfdxvngn71fix?svg=true)](https://ci.appveyor.com/project/raspopov/windowssearchmanager)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/458991e27e0d473d92547d5f7c68c2d1)](https://www.codacy.com/gh/raspopov/WindowsSearchManager/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=raspopov/WindowsSearchManager&amp;utm_campaign=Badge_Grade)
[![GitHub All Releases](https://img.shields.io/github/downloads/raspopov/WindowsSearchManager/total)](https://github.com/raspopov/WindowsSearchManager/releases)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/raspopov/WindowsSearchManager)](https://github.com/raspopov/WindowsSearchManager/releases)

# Windows Search™ Manager

Utility to manage a [Windows Search™](https://en.wikipedia.org/wiki/Windows_Search) internals.

![Windows Search™ Manager](https://raw.githubusercontent.com/raspopov/WindowsSearchManager/main/SearchManager-main.png)

## Features

- Fine tuning of indexed locations (add, edit, delete) including deleting of orphan ones (on unavailable disks)
- Bulk operations
- Direct control of Windows Search™ engine
- Database checking and defragmentation (Windows.edb file)
- Nifty context menu

### Hotkeys

- F5, ENTER - Refresh location list
- Ctrl+C, Ctrl+Insert - Copy location to clipboard
- Insert - Add new location
- Delete - Delete existing location

## System Requirements

- Windows Vista or later, 32 or 64-bit.
- Microsoft Visual C++ 2015-2019 Redistributables ([32-bit](https://aka.ms/vs/15/release/VC_redist.x86.exe)/[64-bit](https://aka.ms/vs/15/release/VC_redist.x64.exe)).
- Administrator rights optional but highly recommended.

## Development Requirements

- [Microsoft Visual Studio 2017 Community](https://aka.ms/vs/15/release/vs_Community.exe) with components:
  - Microsoft.VisualStudio.Workload.NativeDesktop
  - Microsoft.VisualStudio.Component.VC.ATLMFC
  - Microsoft.VisualStudio.Component.WinXP
  - Microsoft.VisualStudio.ComponentGroup.NativeDesktop.WinXP
  - Microsoft.VisualStudio.Component.NuGet
- Windows 10 SDK 10.0.17763
- NuGet packages (automatically installed):
  - InnoSetup 5.6.1
  - Pandoc 2.1.0

## Licenses

Copyright (C) 2012-2021 Nikolay Raspopov <raspopov@cherubicsoft.com>

<https://github.com/raspopov/WindowsSearchManager>

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
( at your option ) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.

## Thanks

[![PVS-Studio](https://pvs-studio.com/static/img/footer-unic.png)](https://pvs-studio.com/)
