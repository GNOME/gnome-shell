/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Thomas Thurman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/** \file  Schema bindings generator.
 *
 * This program simply takes the items given in the binding list in
 * all-keybindings.h and turns them into a portion of
 * the GConf .schemas file.
 *
 * FIXME: also need to make 50-metacity-desktop-key.xml
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>
#include "config.h"

#define _(x) x

static void single_stanza (gboolean is_window, const char *name,
               const char *default_value,
               gboolean can_reverse,
               const char *description);

char *about_keybindings, *about_reversible_keybindings;

char *source_filename, *target_filename;
FILE *source_file, *target_file;

static void
single_stanza (gboolean is_window, const char *name,
               const char *default_value,
               gboolean can_reverse,
               const char *description)
{
  char *keybinding_type = is_window? "window": "global";
  char *escaped_default_value, *escaped_description;

  if (description==NULL)
    return; /* it must be undocumented, so it can't go in this table */

  escaped_description = g_markup_escape_text (description, -1);
  escaped_default_value = default_value==NULL? "disabled":
        g_markup_escape_text (default_value, -1);
  
  fprintf (target_file, "    <schema>\n");
  fprintf (target_file, "      <key>/schemas/apps/metacity/%s_keybindings/%s</key>\n",
            keybinding_type, name);
  fprintf (target_file, "      <applyto>/apps/metacity/%s_keybindings/%s</applyto>\n",
            keybinding_type, name);
  fprintf (target_file, "      <owner>metacity</owner>\n");
  fprintf (target_file, "      <type>string</type>\n");
  fprintf (target_file, "      <default>%s</default>\n", escaped_default_value);
  
  fprintf (target_file, "      <locale name=\"C\">\n");
  fprintf (target_file, "        <short>%s</short>\n", description);
  fprintf (target_file, "        <long>%s</long>\n",
                   can_reverse? about_reversible_keybindings:
                   about_keybindings);
  fprintf (target_file, "      </locale>\n");
  fprintf (target_file, "    </schema>\n\n");

  g_free (escaped_description);

  if (default_value!=NULL)
    g_free (escaped_default_value);
}

static void produce_bindings ();

static void
produce_bindings ()
{
  /* 10240 is ridiculous overkill; we're writing the input file and
   * the lines are always 80 chars or less.
   */
  char buffer[10240];

  source_file = fopen(source_filename, "r");

  if (!source_file)
    {
      g_error ("Cannot compile without %s: %s\n",
        source_filename, strerror (errno));
    }

  target_file = fopen(target_filename, "w");

  if (!target_file)
    {
      g_error ("Cannot create %s: %s\n",
        target_filename, strerror (errno));
    }

  while (fgets (buffer, sizeof (buffer), source_file))
    {
      if (strstr (buffer, "<!-- GENERATED -->"))
         break;
         
      fprintf (target_file, "%s", buffer);
    }

  if (!feof (source_file))
    {
#define keybind(name, handler, param, flags, stroke, description) \
  single_stanza ( \
               flags & BINDING_PER_WINDOW, \
               #name, \
               stroke, \
               flags & BINDING_REVERSES, \
               description);
#include "all-keybindings.h"
#undef keybind
    }

  while (fgets (buffer, sizeof (buffer), source_file))
      fprintf (target_file, "%s", buffer);

  if (fclose (source_file)!=0)
    {
      g_error ("Cannot close %s: %s\n",
        source_filename, strerror (errno));
    }

  if (fclose (target_file)!=0)
    {
      g_error ("Cannot close %s: %s\n",
        target_filename, strerror (errno));
    }
}

int
main (int argc, char **argv)
{
  if (argc!=3)
    {
      g_error ("Syntax: %s <source.in.in> <target.in>\n", argv[0]);
    }
  
  source_filename = argv[1];
  target_filename = argv[2];

  /* Translators: Please don't translate "Control", "Shift", etc, since these
   * are hardcoded (in gtk/gtkaccelgroup.c; it's not metacity's fault).
   * "disabled" must also stay as it is.
   */
  about_keybindings = g_markup_escape_text(_( \
        "The format looks like \"<Control>a\" or <Shift><Alt>F1\".\n\n"\
        "The parser is fairly liberal and allows "\
  	"lower or upper case, and also abbreviations such as \"<Ctl>\" and " \
	"\"<Ctrl>\". If you set the option to the special string " \
	"\"disabled\", then there will be no keybinding for this action."),
        -1);

  about_reversible_keybindings = g_markup_escape_text(_( \
        "The format looks like \"<Control>a\" or <Shift><Alt>F1\".\n\n"\
        "The parser is fairly liberal and allows "\
  	"lower or upper case, and also abbreviations such as \"<Ctl>\" and " \
	"\"<Ctrl>\". If you set the option to the special string " \
	"\"disabled\", then there will be no keybinding for this action.\n\n"\
	"This keybinding may be reversed by holding down the \"shift\" key; "
	"therefore, \"shift\" cannot be one of the keys it uses."),
	-1);

  produce_bindings ();

  g_free (about_keybindings);
  g_free (about_reversible_keybindings);
  
  return 0;
}

/* eof schema-bindings.c */

