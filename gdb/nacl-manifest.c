/* Handle Native Client manifest files.

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
#include "nacl-manifest.h"
#include "solib.h"

#include "command.h"
#include "readline/readline.h"

#include <string.h>


static char* nacl_filename;


static char* nacl_irt_filename;


static void
nacl_file_command (char *args, int from_tty)
{
  if (args)
    {
      xfree (nacl_filename);
      nacl_filename = tilde_expand (args);

      solib_add (NULL, from_tty, NULL, 1);
    }
}


static void
nacl_irt_command (char *args, int from_tty)
{
  if (args)
    {
      xfree (nacl_irt_filename);
      nacl_irt_filename = tilde_expand (args);

      solib_add (NULL, from_tty, NULL, 1);
    }
}


const char*
nacl_manifest_program (void)
{
  return nacl_filename;
}


const char*
nacl_manifest_irt (void)
{
  return nacl_irt_filename;
}


const char*
nacl_manifest_find (const char* original_name)
{
  /* HACK: NaCl ld.so uses "/lib" library path to inform service runtime that
           the file should be opened as solib vs. ordinary file. Split that
           prefix here so that GDB can find these files.  */
  if (strncmp(original_name, "/lib/", 5) == 0)
    original_name += 5;

  return original_name;
}


void
_initialize_nacl_manifest (void)
{
  add_com ("nacl-file", class_files, nacl_file_command,
	   _("Use FILE as Native Client program to be debugged."));
  add_com ("nacl-irt", class_files, nacl_irt_command,
	   _("Use FILE as Native Client IRT to be debugged."));
}
