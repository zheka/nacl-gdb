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

#include "defs.h"
#include "elf-bfd.h"
#include "frame.h"
#include "objfiles.h"
#include "symfile.h"
#include <windows.h>
#include <imagehlp.h>
#include "windows-dbghelp.h"


struct windows_dbghelp_info
{
  int i;
};





bfd *fake_obfd;
asymbol *fake_syms[3];

BOOL CALLBACK EnumSymProc64(
  PCTSTR SymbolName,
  DWORD64 SymbolAddress,
  ULONG SymbolSize,
  PVOID UserContext)
{
    if (strcmp(SymbolName, "NaClSandboxMemoryStartForValgrind") == 0)
    {
      asymbol *s;
      s = bfd_make_empty_symbol (fake_obfd);
      s->name = "_ovly_debug_event";
      s->section = &bfd_abs_section;
      s->flags = BSF_GLOBAL;
      s->value = SymbolAddress;
      fake_syms[0] = s;
    }
    else if (strcmp(SymbolName, "nacl_global_xlate_base") == 0)
    {
      asymbol *s;
      s = bfd_make_empty_symbol (fake_obfd);
      s->name = "nacl_global_xlate_base";
      s->section = &bfd_abs_section;
      s->flags = BSF_GLOBAL;
      s->value = SymbolAddress;
      fake_syms[1] = s;
    }

    if (strcmp(SymbolName, "_ovly_debug_event") == 0 ||
        strcmp(SymbolName, "NaClSandboxMemoryStartForValgrind") == 0 ||
        strcmp(SymbolName, "nacl_global_xlate_base") == 0)
    {
      printf("= 0x%llx %4lu '%s'\n", SymbolAddress, SymbolSize, SymbolName);
    }
    return TRUE;
}






struct windows_dbghelp_info *
retrieve_windows_dbghelp_info (
    HANDLE process_handle,
    HANDLE file_handle,
    LPVOID image_name,
    LPVOID image_base)
{
  DWORD dwOpts;
  DWORD64 base;

  dwOpts = SymGetOptions();
  dwOpts |= SYMOPT_LOAD_LINES /*| SYMOPT_DEFERRED_LOADS*/ | SYMOPT_DEBUG;
  SymSetOptions(dwOpts);

  if (!SymInitialize(process_handle, NULL, FALSE))  /* FALSE is critical - TRUE does not work!  */
  {
    printf("---> SymInitialize failed: 0x%x\n", GetLastError());
  }
  else
  {
    printf("---> SymInitialize OK\n");
  }

  base = SymLoadModule64(process_handle,
                         file_handle,
                         image_name,
                         NULL,
                         image_base,
                         0);
  if (!base)
  {
    printf("---> SymLoadModule64 '%s' failed: 0x%x\n", image_name, GetLastError());
  }
  else
  {
    printf("---> SymLoadModule64 OK, base=0x%llx\n", base);
  }
  /* gdb_assert (base == image_base);  */

{
  fake_obfd = bfd_openw ("xxx.o", "elf64-x86-64");
  if (!fake_obfd)
    {
      printf ("Error: bfd_openw\n");
      return NULL;
    }

  if (!bfd_set_format (fake_obfd, bfd_object))
    {
      printf ("Error: bfd_set_format\n");
      return NULL;
    }

  if (!bfd_set_arch_mach (fake_obfd, bfd_arch_i386, bfd_mach_x86_64))
    {
      printf ("Error: bfd_set_arch_mach\n");
      return NULL;
    }

  fake_syms[0] = NULL;
  fake_syms[1] = NULL;
  fake_syms[2] = NULL;
}

  if (!SymEnumerateSymbols64(process_handle,
                             image_base,
                             EnumSymProc64,
                             NULL))
  {
    printf("---> SymEnumerateSymbols64 failed: 0x%x\n", GetLastError());
  }
  else
  {
    printf("---> SymEnumerateSymbols64 OK\n");
  }

{
  bfd_set_symtab (fake_obfd, fake_syms, 2);

  if (!bfd_close (fake_obfd))
    {
      printf ("Error: bfd_close\n");
      return NULL;
    }
}

{
  static struct windows_dbghelp_info dbghelp_info;
  return &dbghelp_info;
}
}


void
add_windows_dbghelp_info (struct windows_dbghelp_info *info)
{

if (info)
{
  struct section_addr_info *section_addrs;

  section_addrs = alloc_section_addr_info (1);
  section_addrs->other[0].name = ".text";
  section_addrs->other[0].addr = 0x0;

  symbol_file_add ("xxx.o", SYMFILE_VERBOSE, section_addrs, OBJF_USERLOADED|OBJF_READNOW);
  reinit_frame_cache ();
}

}
