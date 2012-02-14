/* Handle Native Client shared libraries.

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

#ifndef SOLIB_NACL_H
#define SOLIB_NACL_H

extern int nacl_sandbox_base;

extern int nacl_sandbox_address_p (CORE_ADDR addr);

extern void set_gdbarch_nacl_pointer_to_address (struct gdbarch *gdbarch);

extern CORE_ADDR nacl_entry_point_address (void);

extern void set_nacl_solib_ops (struct gdbarch *gdbarch);

#endif
