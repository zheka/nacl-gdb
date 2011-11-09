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
#include "nacl-manifest.h"
#include "nacl-tdep.h"
#include "objfiles.h"
#include "solib.h"
#include "solib-svr4.h"
#include "solist.h"


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


/* Start address of Native Client sandbox.  */

CORE_ADDR nacl_sandbox_addr;


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


struct ldso_interface
  {
    /* Function that gets called on solib events (_dl_debug_state).  */
    CORE_ADDR solib_event_addr;

    /* Structure that holds solib list head (_r_debug).  */
    CORE_ADDR debug_struct_addr;

    /* ld.so command line argv (_dl_argv).  */
    CORE_ADDR argv_addr;
  };


static int
nacl_discover_ldso_interface (struct ldso_interface *ldso)
{
  bfd* abfd;

  gdb_assert (nacl_manifest_program ());

  memset (ldso, 0, sizeof (*ldso));

  /* TODO: try faster pass first: walk master so_list and find native client
     executable's objfile, then lookup_minimal_symbol in that objfile.  */

  /* Slow and crappy: open native client executable's bfd and walk its dynamic
     symbol table.  */

  abfd = solib_bfd_open ((char *) nacl_manifest_program ());
  if (abfd)
    {
      long storage_needed;

      storage_needed = bfd_get_dynamic_symtab_upper_bound (abfd);
      if (storage_needed > 0)
        {
          void *storage;
          asymbol **symbol_table;
          unsigned int number_of_symbols;
          int i;

          storage = xmalloc (storage_needed);
          symbol_table = (asymbol **) storage;
          number_of_symbols = bfd_canonicalize_dynamic_symtab (abfd, symbol_table);

          for (i = 0; i < number_of_symbols; i++)
            {
              asymbol *sym;

              sym = *symbol_table++;
              if (strcmp (sym->name, "_dl_debug_state") == 0)
                {
                  ldso->solib_event_addr = sym->value + sym->section->vma;
                }
              else if (strcmp (sym->name, "_r_debug") == 0)
                {
                  ldso->debug_struct_addr = sym->value + sym->section->vma;
                }
              else if (strcmp (sym->name, "_dl_argv") == 0)
                {
                  ldso->argv_addr = sym->value + sym->section->vma;
                }
            }

          xfree (storage);
        }

      bfd_close (abfd);
    }

  return ldso->debug_struct_addr;
}


static struct so_list *
nacl_alloc_so (CORE_ADDR addr, const char *name)
{
  struct so_list *so;

  so = XZALLOC (struct so_list);

  strcpy (so->so_name, name);
  strcpy (so->so_original_name, so->so_name);

  so->lm_info = XZALLOC (struct lm_info);

  so->lm_info->l_addr = addr;

  return so;
}


static struct so_list *
nacl_find_and_alloc_so (CORE_ADDR addr, const char *original_name)
{
  /* Actual objfiles pathnames are usually different from pathnames seen by NaCl
     program, the mapping between them is defined by manifest file. Pathnames
     coming here are extracted from NaCl ld.so runtime structures, while GDB
     needs their mapped values.  */
  return nacl_alloc_so (addr, nacl_manifest_find (original_name));
}


static void
nacl_append_sos (struct so_list **link_ptr, const struct ldso_interface *ldso)
{
  CORE_ADDR lm_addr;

  /* for (lm_addr = _r_debug.r_map; lm_addr; lm_addr = lm_addr->l_next)  */
  for (lm_addr = read_memory_unsigned_integer (nacl_sandbox_addr + ldso->debug_struct_addr + 4, 4, BFD_ENDIAN_LITTLE);
       lm_addr;
       lm_addr = read_memory_unsigned_integer (nacl_sandbox_addr + lm_addr + 16, 4, BFD_ENDIAN_LITTLE))
    {
      CORE_ADDR l_addr;
      CORE_ADDR l_name;
      char* so_name;
      int err;
      struct so_list *so;

      /* link_map::l_addr.  */
      l_addr = read_memory_unsigned_integer (nacl_sandbox_addr + lm_addr, 4, BFD_ENDIAN_LITTLE);

      /* link_map::l_name. */
      l_name = read_memory_unsigned_integer (nacl_sandbox_addr + lm_addr + 8, 4, BFD_ENDIAN_LITTLE);
      target_read_string (nacl_sandbox_addr + l_name, &so_name, SO_NAME_MAX_PATH_SIZE - 1, &err);

      if (strcmp (so_name, "") == 0)
        {
          /* Native client dynamic executable. */
          xfree (so_name);

          l_name = read_memory_unsigned_integer (nacl_sandbox_addr + ldso->argv_addr, 4, BFD_ENDIAN_LITTLE);
          l_name = read_memory_unsigned_integer (nacl_sandbox_addr + l_name, 4, BFD_ENDIAN_LITTLE);
          target_read_string (nacl_sandbox_addr + l_name, &so_name, SO_NAME_MAX_PATH_SIZE - 1, &err);

          so = nacl_find_and_alloc_so (nacl_sandbox_addr + l_addr, so_name);
        }
      else if (strcmp (so_name, "NaClMain") == 0)
        {
          /* Native client ld.so. */
          so = nacl_alloc_so (nacl_sandbox_addr + l_addr, nacl_manifest_program ());
        }
      else
        {
          /* Solib. */
          so = nacl_find_and_alloc_so (nacl_sandbox_addr + l_addr, so_name);
        }
      xfree (so_name);

      *link_ptr = so;
      link_ptr = &so->next;
    }
}


static struct so_list *
nacl_current_sos (void)
{
  struct so_list *head;
  struct so_list **link_ptr;

  /* First, retrieve the SVR4 shared library list.  */
  head = svr4_so_ops.current_sos ();

  if (nacl_manifest_program ())
    {
      CORE_ADDR prev_sandbox_addr = nacl_sandbox_addr;

      if (nacl_update_sandbox_addr ())
        {
          struct ldso_interface ldso;

          /* Append our libraries to the end of the list.  */
          for (link_ptr = &head; *link_ptr; link_ptr = &(*link_ptr)->next)
            ;

          /* Append IRT.  */
          if (nacl_manifest_irt ())
            {
              *link_ptr = nacl_alloc_so (nacl_sandbox_addr, nacl_manifest_irt ());
              link_ptr = &(*link_ptr)->next;
            }

          if (nacl_discover_ldso_interface (&ldso))
            {
              /* Dynamic case - walk ld.so solib list.  */
              nacl_append_sos (link_ptr, &ldso);

              if (!prev_sandbox_addr)
                {
                  /* This is the first time we are here with loaded NaCl.
                     Set NaCl solib event breakpoint here.  */
                  create_solib_event_breakpoint (target_gdbarch, nacl_sandbox_addr + ldso.solib_event_addr);
                }
            }
          else
            {
              /* Static case - just add the main executable.  */
              *link_ptr = nacl_alloc_so (nacl_sandbox_addr, nacl_manifest_program ());
            }
        }
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


static struct symbol *
nacl_lookup_lib_symbol (const struct objfile *objfile,
		        const char *name,
		        const domain_enum domain)
{
  /* Use bfd to distinguish nacl objfiles.
     Another way is to check that objfile->addr_low is nacl address.  */
  if (nacl_bfd_p (objfile->obfd))
    return lookup_global_symbol_from_objfile (objfile, name, domain);

  return svr4_so_ops.lookup_lib_global_symbol (objfile, name, domain);
}


static void
nacl_solib_create_inferior_hook (int from_tty)
{
  nacl_sandbox_addr = 0;

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
      nacl_so_ops.lookup_lib_global_symbol = nacl_lookup_lib_symbol;
    }

  set_solib_ops (gdbarch, &nacl_so_ops);
}
