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

#include <string.h>


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
