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
 * This program simply takes the items given in the binding lists in
 * window-bindings.h and scheme-bindings.h and turns them into a portion of
 * the GConf .schemas file.
 *
 * FIXME: also need to make 50-metacity-desktop-key.xml
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include "config.h"

#define _(x) x

static void single_stanza (gboolean is_window, const char *name,
               const char *default_value,
               gboolean can_reverse,
               const char *description);

char *about_keybindings, *about_reversible_keybindings;

const char* window_string = \
	"    <schema>\n" \
	"      <key>/schemas/apps/metacity/%s_keybindings/%s%s</key>\n" \
	"      <applyto>/apps/metacity/%s_keybindings/%s%s</applyto>\n" \
	"      <owner>metacity</owner>\n" \
	"      <type>string</type>\n" \
	"      <default>%s</default>\n" \
	"      <locale name=\"C\">\n" \
	"         <short>%s</short>\n" \
	"         <long>\n" \
	"          %s  %s\n" \
	"         </long>\n" \
	"      </locale>\n" \
	"    </schema>\n\n\n";

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
        g_markup_escape_text (description, -1);
  
  printf ("    <schema>\n");
  printf ("      <key>/schemas/apps/metacity/%s_keybindings/%s</key>\n",
            keybinding_type, name);
  printf ("      <applyto>/apps/metacity/%s_keybindings/%s</applyto>\n",
            keybinding_type, name);
  printf ("      <owner>metacity</owner>\n");
  printf ("      <type>string</type>\n");
  printf ("      <default>%s</default>\n", escaped_default_value);
  
  printf ("      <locale name=\"C\">\n");
  printf ("        <short>%s</short>\n", description);
  printf ("        <long>%s</long>\n",
                   can_reverse? about_reversible_keybindings:
                   about_keybindings);
  printf ("      </locale>\n");
  printf ("    </schema>\n\n");

  g_free (escaped_description);

  if (default_value!=NULL)
    g_free (escaped_default_value);
}

static void produce_bindings ();

static void
produce_bindings ()
{
  FILE *metacity_schemas_in_in = fopen("metacity.schemas.in.in", "r");

  if (!metacity_schemas_in_in)
    {
      g_error ("Cannot compile without metacity.schemas.in.in: %s\n",
        strerror (errno));
    }

  while (!feof (metacity_schemas_in_in))
    {
      /* 10240 is ridiculous overkill; we're writing the input file and
       * the lines are always 80 chars or less.
       */
      char buffer[10240];
      
      fgets (buffer, sizeof (buffer), metacity_schemas_in_in);
      
      if (strstr (buffer, "<!-- GENERATED -->"))
         break;
         
      printf ("%s", buffer);
    }

  if (!feof (metacity_schemas_in_in))
    {
#define keybind(name, handler, param, flags, stroke, description) \
  single_stanza ( \
               flags & BINDING_PER_WINDOW, \
               #name, \
               stroke, \
               flags & BINDING_REVERSES, \
               description);
#include "window-bindings.h"
#include "screen-bindings.h"
#undef keybind
    }

  while (!feof (metacity_schemas_in_in))
    {
      char buffer[10240];
      
      fgets (buffer, sizeof (buffer), metacity_schemas_in_in);
      
      printf ("%s", buffer);
    }

  fclose (metacity_schemas_in_in);
}

int
main ()
{
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

