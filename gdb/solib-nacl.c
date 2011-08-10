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
#include "breakpoint.h"
#include "gdbcore.h"
#include "solib.h"
#include "solib-svr4.h"
#include "solist.h"

#include "command.h"
#include "readline/readline.h"


/* Native Client executable file name.
   Intentional memory leak, freed at exit.  */

static char* nacl_filename;


static void
nacl_file_command (char *args, int from_tty)
{
  if (args)
    {
      xfree (nacl_filename);
      nacl_filename = tilde_expand (args);
    }
}


/* Native Client loader calls "_ovly_debug_event" right after the program
   is loaded into memory.  Report this as a solib event.  */

static void
nacl_enable_break (void)
{
  struct minimal_symbol *solib_event_sym;

  solib_event_sym = lookup_minimal_symbol ("_ovly_debug_event", NULL, NULL);
  if (solib_event_sym)
    {
      CORE_ADDR addr;

      addr = SYMBOL_VALUE_ADDRESS (solib_event_sym);
      addr = gdbarch_convert_from_func_ptr_addr (target_gdbarch,
                                                 addr,
                                                 &current_target);
      create_solib_event_breakpoint (target_gdbarch, addr);
    }
}


static void
nacl_solib_create_inferior_hook (int from_tty)
{
  /* Call SVR4 hook -- this will re-insert the SVR4 solib breakpoints.  */
  svr4_so_ops.solib_create_inferior_hook (from_tty);

  nacl_enable_break ();
}


void
set_nacl_solib_ops (struct gdbarch *gdbarch)
{
  static struct target_so_ops nacl_so_ops;

  /* Initialize this lazily, to avoid an initialization order
     dependency on solib-svr4.c's _initialize routine.  */
  if (nacl_so_ops.current_sos == NULL)
    {
      nacl_so_ops = svr4_so_ops;
      nacl_so_ops.solib_create_inferior_hook = nacl_solib_create_inferior_hook;
    }

  set_solib_ops (gdbarch, &nacl_so_ops);
}


void
_initialize_nacl_solib (void)
{
  add_com ("nacl-file", class_files, nacl_file_command,
	   _("Use FILE as Native Client program to be debugged."));
}
