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


/* Link map info to include in an allocated so_list entry.
   ATTENTION: copy-paste from solib-svr4.c!  */

struct lm_info
  {
    /* Pointer to copy of link map from inferior.  The type is char *
       rather than void *, so that we may use byte offsets to find the
       various fields without the need for a cast.  */
    gdb_byte *lm;

    /* Amount by which addresses in the binary should be relocated to
       match the inferior.  This could most often be taken directly
       from lm, but when prelinking is involved and the prelink base
       address changes, we may need a different offset, we want to
       warn about the difference and compute it only once.  */
    CORE_ADDR l_addr;

    /* The target location of lm.  */
    CORE_ADDR lm_addr;
  };


/* Native Client executable file name.
   Intentional memory leak, freed at exit.  */

static char* nacl_filename;


/* Start address of Native Client sandbox.  */

static CORE_ADDR nacl_sandbox_addr;


static void
nacl_file_command (char *args, int from_tty)
{
  if (args)
    {
      xfree (nacl_filename);
      nacl_filename = tilde_expand (args);
    }
}


static CORE_ADDR
nacl_update_sandbox_addr (void)
{
  struct minimal_symbol *addr_sym;

  addr_sym = lookup_minimal_symbol ("nacl_global_xlate_base", NULL, NULL);
  if (addr_sym)
    {
      nacl_sandbox_addr
        = read_memory_unsigned_integer (SYMBOL_VALUE_ADDRESS (addr_sym),
                                        8,
                                        BFD_ENDIAN_LITTLE);
    }
  else
    {
      nacl_sandbox_addr = 0;
    }

  return nacl_sandbox_addr;
}


static struct so_list *
nacl_current_sos (void)
{
  struct so_list *head;
  struct so_list **link_ptr;

  /* First, retrieve the SVR4 shared library list.  */
  head = svr4_so_ops.current_sos ();

  /* Append our libraries to the end of the list.  */
  for (link_ptr = &head; *link_ptr; link_ptr = &(*link_ptr)->next)
    ;

  if (nacl_filename && nacl_update_sandbox_addr ())
    {
      struct so_list *so;

      so = XZALLOC (struct so_list);

      strcpy (so->so_name, nacl_filename);
      strcpy (so->so_original_name, so->so_name);

      so->lm_info = XZALLOC (struct lm_info);

      so->lm_info->l_addr = nacl_sandbox_addr;

      *link_ptr = so;
      link_ptr = &so->next;
    }

  return head;
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
      nacl_so_ops.current_sos = nacl_current_sos;
    }

  set_solib_ops (gdbarch, &nacl_so_ops);
}


void
_initialize_nacl_solib (void)
{
  add_com ("nacl-file", class_files, nacl_file_command,
	   _("Use FILE as Native Client program to be debugged."));
}
