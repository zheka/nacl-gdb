/* Working with Windows Debug Help library.

   Copyright (C) 2012 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef WINDOWS_DBGHELP_H
#define WINDOWS_DBGHELP_H

/* Information obtained using DbgHelp library. We retrieve it at the process
   creation point when Windows-specific process info is still available, but
   consume a while after that when everything needed to add a symbol file is
   initialized. */

struct windows_dbghelp_info;

extern struct windows_dbghelp_info *retrieve_windows_dbghelp_info (
    HANDLE process_handle,
    HANDLE file_handle,
    LPVOID image_name,
    LPVOID image_base);

extern void add_windows_dbghelp_info (struct windows_dbghelp_info *info);

#endif
