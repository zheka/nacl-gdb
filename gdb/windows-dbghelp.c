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
  bfd *obfd;
  asymbol *symtab[3];
};


/* ATTENTION! 'name' should remain valid until bfd is closed?  */

static asymbol *
add_symbol (bfd *obfd, const char *name, CORE_ADDR addr)
{
  asymbol *s = bfd_make_empty_symbol (obfd);

  s->name = name;
  s->section = &bfd_abs_section;
  s->flags = BSF_GLOBAL;
  s->value = addr;

  return s;
}


BOOL CALLBACK EnumSymProc64(
  PCTSTR SymbolName,
  DWORD64 SymbolAddress,
  ULONG SymbolSize,
  PVOID UserContext)
{
  struct windows_dbghelp_info *dbghelp_info = UserContext;

  if (strcmp(SymbolName, "NaClSandboxMemoryStartForValgrind") == 0)
    {
      /* Real '_ovly_debug_event' is not compiled under Windows, but we can use
         'NaClSandboxMemoryStartForValgrind' instead.  */
      dbghelp_info->symtab[0] = add_symbol (dbghelp_info->obfd, "_ovly_debug_event", SymbolAddress);
    }
  else if (strcmp(SymbolName, "nacl_global_xlate_base") == 0)
    {
      /* Pass symbol name as constant string so it remains valid forever :)  */
      dbghelp_info->symtab[1] = add_symbol (dbghelp_info->obfd, "nacl_global_xlate_base", SymbolAddress);
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
  DWORD sym_options;
  struct windows_dbghelp_info *dbghelp_info;

  /* Fix symbol handling options.  */

  sym_options = SymGetOptions ();
  sym_options |= SYMOPT_LOAD_LINES | SYMOPT_DEBUG;
  SymSetOptions (sym_options);

  /* Initialize symbol handler for a process.  */

  /* FALSE is critical - TRUE does not work!  */
  if (!SymInitialize (process_handle, NULL, FALSE))  
    {
      printf("---> SymInitialize failed: 0x%x\n", GetLastError());
      return NULL;
    }

  /* Load symbol table for a module. SymLoadModule64 has been superseded by
     SymLoadModuleEx, but mingw toolchain we use does not have the latter. */

  /* We already have valid image base, so we don't need the returned value.  */
  if (!SymLoadModule64 (process_handle,
                        file_handle,
                        image_name,
                        NULL,
                        image_base,
                        0))
    {
      printf("---> SymLoadModule64 '%s' failed: 0x%x\n", image_name, GetLastError());
      return NULL;
    }

  /* Create bfd for accepting symbols.  */

  dbghelp_info = XZALLOC (struct windows_dbghelp_info);

  dbghelp_info->obfd = bfd_openw ("xxx.o", "elf64-x86-64");
  if (!dbghelp_info->obfd)
    {
      printf("---> bfd_openw failed: %s\n", bfd_errmsg (bfd_get_error ()));
      return NULL;
    }

  if (!bfd_set_format (dbghelp_info->obfd, bfd_object))
    {
      printf("---> bfd_set_format failed: %s\n", bfd_errmsg (bfd_get_error ()));
      return NULL;
    }

  if (!bfd_set_arch_mach (dbghelp_info->obfd, bfd_arch_i386, bfd_mach_x86_64))
    {
      printf("---> bfd_set_arch_mach failed: %s\n", bfd_errmsg (bfd_get_error ()));
      return NULL;
    }

  /* Feed symbols into bfd.  */

  if (!SymEnumerateSymbols64 (process_handle,
                              image_base,
                              EnumSymProc64,
                              dbghelp_info))
    {
      printf("---> SymEnumerateSymbols64 failed: 0x%x\n", GetLastError());
      return NULL;
    }

  /* Finalize the bfd.  */

  bfd_set_symtab (dbghelp_info->obfd, dbghelp_info->symtab, 2);

  if (!bfd_close (dbghelp_info->obfd))
    {
      printf("---> bfd_close failed: %s\n", bfd_errmsg (bfd_get_error ()));
      return NULL;
    }

  return dbghelp_info;
}


void
add_windows_dbghelp_info (struct windows_dbghelp_info *dbghelp_info)
{
  if (dbghelp_info)
    {
      struct section_addr_info *section_addrs;

      section_addrs = alloc_section_addr_info (1);
      section_addrs->other[0].name = ".text";
      section_addrs->other[0].addr = 0x0;

      symbol_file_add ("xxx.o", SYMFILE_VERBOSE, section_addrs, OBJF_USERLOADED | OBJF_READNOW);
      reinit_frame_cache ();

      xfree (dbghelp_info);
  }
}
