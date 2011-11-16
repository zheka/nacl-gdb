/* Native Client multi-architecture debugging support.

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
#include "arch-utils.h"
#include "inferior.h"
#include "observer.h"
#include "regcache.h"

#include "solib-nacl.h"


static struct target_ops nacl_ops;


static void
nacl_mourn_inferior (struct target_ops *ops)
{
  struct target_ops *ops_beneath = find_target_beneath (ops);
  while (ops_beneath && !ops_beneath->to_mourn_inferior)
    ops_beneath = find_target_beneath (ops_beneath);
  gdb_assert (ops_beneath);
  ops_beneath->to_mourn_inferior (ops_beneath);

  unpush_target (&nacl_ops);
  registers_changed ();
}


static void
nacl_fetch_registers (struct target_ops *ops, struct regcache *regcache, int regno)
{
  struct target_ops *ops_beneath = find_target_beneath (ops);
  while (ops_beneath && !ops_beneath->to_fetch_registers)
    ops_beneath = find_target_beneath (ops_beneath);
  gdb_assert (ops_beneath);
  ops_beneath->to_fetch_registers (ops_beneath, regcache, regno);
}


static void
nacl_store_registers (struct target_ops *ops, struct regcache *regcache, int regno)
{
  struct target_ops *ops_beneath = find_target_beneath (ops);
  while (ops_beneath && !ops_beneath->to_store_registers)
    ops_beneath = find_target_beneath (ops_beneath);
  gdb_assert (ops_beneath);
  ops_beneath->to_store_registers (ops_beneath, regcache, regno);
}


static LONGEST
nacl_xfer_partial (struct target_ops *ops, enum target_object object,
		   const char *annex, gdb_byte *readbuf,
		   const gdb_byte *writebuf, ULONGEST offset, LONGEST len)
{
  struct target_ops *ops_beneath = find_target_beneath (ops);
  while (ops_beneath && !ops_beneath->to_xfer_partial)
    ops_beneath = find_target_beneath (ops_beneath);
  gdb_assert (ops_beneath);
  return ops_beneath->to_xfer_partial (ops_beneath, object, annex, readbuf, writebuf, offset, len);
}


static int
nacl_search_memory (struct target_ops* ops,
		    CORE_ADDR start_addr, ULONGEST search_space_len,
		    const gdb_byte *pattern, ULONGEST pattern_len,
		    CORE_ADDR *found_addrp)
{
  struct target_ops *ops_beneath = find_target_beneath (ops);
  while (ops_beneath && !ops_beneath->to_search_memory)
    ops_beneath = find_target_beneath (ops_beneath);
  gdb_assert (ops_beneath);
  return ops_beneath->to_search_memory (ops_beneath, start_addr, search_space_len, pattern, pattern_len, found_addrp);
}


static int
nacl_region_ok_for_hw_watchpoint (CORE_ADDR addr, int len)
{
  struct target_ops *ops_beneath = find_target_beneath (&nacl_ops);
  while (ops_beneath && !ops_beneath->to_region_ok_for_hw_watchpoint)
    ops_beneath = find_target_beneath (ops_beneath);
  gdb_assert (ops_beneath);
  return ops_beneath->to_region_ok_for_hw_watchpoint (addr, len);
}


static struct gdbarch *
nacl_thread_architecture (struct target_ops *ops, ptid_t ptid)
{
  if (nacl_sandbox_addr)
    {
      struct regcache *regcache;
      CORE_ADDR pc;

      /* Get runtime-side pc.  */
      regcache = get_thread_arch_regcache (ptid, target_gdbarch);
      pc = regcache_read_pc (regcache);

      /* TODO: fix this hacky check that only works for 4gb x86_64 sandbox!  */
      if (pc >= nacl_sandbox_addr && pc < nacl_sandbox_addr + 4UL * 1024UL * 1024UL * 1024UL)
        {
          struct gdbarch_info info;
          struct gdbarch *gdbarch;

          /* NaCl and runtime architectures differ by OS ABI only.  */
          gdbarch_info_init (&info);
          info.bfd_arch_info = gdbarch_bfd_arch_info (target_gdbarch);
          info.osabi = GDB_OSABI_NACL;

          gdbarch = gdbarch_find_by_info (info);
          if (gdbarch)
            return gdbarch;
        }
    }

  return target_gdbarch;
}


static void
nacl_multiarch_inferior_created (struct target_ops *ops, int from_tty)
{
  push_target (&nacl_ops);
  registers_changed ();
}


static void
init_nacl_ops (void)
{
  nacl_ops.to_shortname = "nacl";
  nacl_ops.to_longname = "NaCl multi-architecture support.";
  nacl_ops.to_doc = "NaCl multi-architecture support.";
  nacl_ops.to_mourn_inferior = nacl_mourn_inferior;
  nacl_ops.to_fetch_registers = nacl_fetch_registers;
  nacl_ops.to_store_registers = nacl_store_registers;
  nacl_ops.to_xfer_partial = nacl_xfer_partial;
  nacl_ops.to_search_memory = nacl_search_memory;
  nacl_ops.to_region_ok_for_hw_watchpoint = nacl_region_ok_for_hw_watchpoint;
  nacl_ops.to_thread_architecture = nacl_thread_architecture;
  nacl_ops.to_stratum = arch_stratum;
  nacl_ops.to_magic = OPS_MAGIC;
}


void
_initialize_nacl_multiarch (void)
{
  init_nacl_ops ();
  add_target (&nacl_ops);

  observer_attach_inferior_created (nacl_multiarch_inferior_created);
}
