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
#include "target.h"

#include "command.h"
#include "readline/readline.h"

#include <stdio.h>
#include <string.h>

#define MANIFEST_MAX_NESTING 4

#define MANIFEST_MAX_STRING_SIZE 256


struct file_list
  {
    struct file_list *next;

    char original_name[MANIFEST_MAX_STRING_SIZE];

    char name[MANIFEST_MAX_STRING_SIZE];
  };


static struct file_list *nacl_file_list;


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
  struct file_list *curr;

  /* HACK: NaCl uses "/lib/" library path to inform service runtime that the
           file should be opened as solib vs. ordinary file. Split that prefix
           here so that GDB can find these files via manifest or as is.  */
  if (strncmp (original_name, "/lib/", 5) == 0)
    original_name += 5;

  for (curr = nacl_file_list; curr; curr = curr->next)
    {
      if (strcmp (original_name, curr->original_name) == 0)
        return curr->name;
    }

  /* TODO: Should we complain if we have a manifest but failed to find
     /lib/filename there? */

  return original_name;
}


static void
nacl_manifest_free (void)
{
  while (nacl_file_list)
    {
      struct file_list *next = nacl_file_list->next;

      xfree (nacl_file_list);
      nacl_file_list = next;
    }

  xfree (nacl_program_filename);
  nacl_program_filename = NULL;
}


/* Very dumb parser for JSON subset used in Native Client manifest files.

   This is a SAX-style parser that runs callbacks on JSON events.  */


struct json_manifest_reader
  {
    /* Manifest file.  */
    FILE* file;

    /* Stack of members being parsed.  */
    const char *members[MANIFEST_MAX_NESTING];

    /* Members stack size.  */
    int nesting;

    /* Manifest file dirname, with slash at the end if non-empty. */
    char* dirname;

    /* Where to append new file_list.  */
    struct file_list **link_ptr;
  };


static void
json_on_member (struct json_manifest_reader *r, const char *member)
{
  if (r->nesting == MANIFEST_MAX_NESTING)
    error (_("Invalid manifest file."));

  r->members[r->nesting] = member;
  ++r->nesting;
}


static void
json_on_end_member (struct json_manifest_reader *r, const char *member)
{
  --r->nesting;
}


static struct file_list *
json_append_file_list (struct json_manifest_reader *r,
                       const char *original_name,
                       const char *name)
{
  struct file_list *curr = XZALLOC (struct file_list);

  strcpy (curr->original_name, original_name);

  /* TODO: Check if the result fits!  */
  if (r->dirname)
    {
      strcpy (curr->name, r->dirname);
      strcat (curr->name, SLASH_STRING);
    }
  strcat (curr->name, name);

  *r->link_ptr = curr;
  r->link_ptr = &curr->next;

  return curr;
}


static void
json_on_string_value (struct json_manifest_reader *r, const char *value)
{
  if (r->nesting == 3 &&
      strcmp (r->members[0], "program") == 0 &&
      strcmp (r->members[1], "x86-64") == 0 &&
      strcmp (r->members[2], "url") == 0)
    {
      /* HACK: NaCl ld.so refers to itself in the link_map.
               It used to always name itself NaClMain, we support this by
               substituting nacl_manifest_program () in solib-nacl.c.
               New style is to name itself with its own basename, we support
               this by adding a fake file_list entry.  */
      struct file_list *p = json_append_file_list (r, lbasename (value), value);

      /* We'll xfree nacl_program_filename.  */
      nacl_program_filename = xstrdup (p->name);
    }
  else if (r->nesting == 4 &&
           strcmp (r->members[0], "files") == 0 &&
           strcmp (r->members[2], "x86-64") == 0 &&
           strcmp (r->members[3], "url") == 0)
    {
      json_append_file_list (r, r->members[1], value);
    }
}


/* Basic parsing utilities.  */


static int
json_getc (struct json_manifest_reader *r)
{
  return fgetc (r->file);
}


static int
json_getc_nonspace (struct json_manifest_reader *r)
{
  int c;

  while (isspace (c = json_getc (r)));
  return c;
}


/* Parsing routines.

   json_parse_something assumes we are just going to read first character of
   something, probably skipping preceeding whitespaces.

   json_finish_parse_something assumes we already read first character of
   something, and checked it was correct.  */


static void json_parse_value (struct json_manifest_reader *r);


static void
json_finish_parse_string (struct json_manifest_reader *r, char *buf, int len)
{
  int c;

  for (; len; --len)
    {
      c = json_getc (r);
      if (c == '"')
        {
          *buf = '\0';
          return;
        }

      if (c == '\n' || c == EOF)
        break;

      *buf++ = (char)c;
    }

  error (_("Invalid manifest file."));
}


/* We only accept non-empty objects.  */

static void
json_finish_parse_object (struct json_manifest_reader *r)
{
  int c;
  char buf[MANIFEST_MAX_STRING_SIZE];

  do
    {
      if (json_getc_nonspace (r) != '\"')
        error (_("Invalid manifest file."));

      json_finish_parse_string (r, buf, MANIFEST_MAX_STRING_SIZE);
      json_on_member (r, buf);

      if (json_getc_nonspace (r) != ':')
        error (_("Invalid manifest file."));

      json_parse_value (r);
      json_on_end_member (r, buf);
    }
  while ((c = json_getc_nonspace (r)) == ',');

  if (c != '}')
    error (_("Invalid manifest file."));
}


/* We only accept objects or strings.  */

static void
json_parse_value (struct json_manifest_reader *r)
{
  int c = json_getc_nonspace (r);

  if (c == '{')
    {
      json_finish_parse_object (r);
    }
  else if (c == '\"')
    {
      char buf[MANIFEST_MAX_STRING_SIZE];

      json_finish_parse_string (r, buf, MANIFEST_MAX_STRING_SIZE);
      json_on_string_value (r, buf);
    }
  else
    {
      error (_("Invalid manifest file."));
    }
}


/* GDB commands for specifying Native Client files.  */


static void
nacl_file_command (char *args, int from_tty)
{
  if (args)
    {
      nacl_manifest_free ();
      nacl_program_filename = tilde_expand (args);
      if (target_has_execution)
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
      if (target_has_execution)
        solib_add (NULL, from_tty, NULL, 1);
    }
}


static void
nacl_manifest_command (char *args, int from_tty)
{
  if (args)
    {
      char* manifest_filename;
      struct json_manifest_reader r = { 0 };

      manifest_filename = tilde_expand (args);
      make_cleanup (xfree, manifest_filename);

      r.file = fopen (manifest_filename, "r");
      if (!r.file)
        perror_with_name (manifest_filename);
      make_cleanup_fclose (r.file);

      r.dirname = ldirname (manifest_filename);
      make_cleanup (xfree, r.dirname);

      r.link_ptr = &nacl_file_list;

      /* TODO: Kill existing manifest only if new one parsed OK!  */
      nacl_manifest_free ();
      json_parse_value (&r);

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
