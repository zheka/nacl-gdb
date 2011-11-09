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

#include <stdio.h>
#include <string.h>


static char *nacl_program_filename;


static char *nacl_irt_filename;


const char *
nacl_manifest_program (void)
{
  return nacl_program_filename;
}


const char *
nacl_manifest_irt (void)
{
  return nacl_irt_filename;
}


const char *
nacl_manifest_find (const char *original_name)
{
  /* HACK: NaCl ld.so uses "/lib" library path to inform service runtime that
           the file should be opened as solib vs. ordinary file. Split that
           prefix here so that GDB can find these files.  */
  if (strncmp (original_name, "/lib/", 5) == 0)
    original_name += 5;

  return original_name;
}


static void
nacl_file_command (char *args, int from_tty)
{
  if (args)
    {
      xfree (nacl_program_filename);
      nacl_program_filename = tilde_expand (args);

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


static void
nacl_manifest_command (char *args, int from_tty)
{
  if (args)
    {
      char* manifest_filename;
      FILE* manifest_file;

      manifest_filename = tilde_expand (args);
      make_cleanup (xfree, manifest_filename);

      manifest_file = fopen (manifest_filename, "r");
      if (!manifest_file)
        perror_with_name (manifest_filename);
      make_cleanup_fclose (manifest_file);

      /* TODO: do something useful with the manifest file...  */
      error (_("Manifest files not implemented."));

      solib_add (NULL, from_tty, NULL, 1);

      /* Caller will cleanup for us... hopefully.  */
    }
}


void
_initialize_nacl_manifest (void)
{
  add_com ("nacl-file", class_files, nacl_file_command,
	   _("Use FILE as Native Client program to be debugged."));
  add_com ("nacl-irt", class_files, nacl_irt_command,
	   _("Use FILE as Native Client IRT to be debugged."));
  add_com ("nacl-manifest", class_files, nacl_manifest_command,
	   _("Use FILE as Native Client manifest for the program to be debugged."));
}
