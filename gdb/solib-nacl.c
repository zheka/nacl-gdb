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

#include "defs.h"
#include "solib.h"
#include "solib-svr4.h"
#include "solist.h"

void
set_nacl_solib_ops (struct gdbarch *gdbarch)
{
  static struct target_so_ops nacl_so_ops;

  /* Initialize this lazily, to avoid an initialization order
     dependency on solib-svr4.c's _initialize routine.  */
  if (nacl_so_ops.current_sos == NULL)
    {
      nacl_so_ops = svr4_so_ops;
    }

  set_solib_ops (gdbarch, &nacl_so_ops);
}
