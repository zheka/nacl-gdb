/* Target-dependent code for Native Client.

   Copyright (C) 2011 Free Software Foundation, Inc.

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
#include "osabi.h"

int
nacl_bfd_p (bfd *abfd)
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour &&
      elf_elfheader (abfd)->e_ident[EI_OSABI] == 123)
    return 1;

  return 0;
}

static enum gdb_osabi
nacl_osabi_sniffer (bfd *abfd)
{
  if (nacl_bfd_p (abfd))
    return GDB_OSABI_NACL;

  return GDB_OSABI_UNKNOWN;
}

void
gdbarch_register_nacl_osabi_sniffer (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_elf_flavour,
				  nacl_osabi_sniffer);
}
