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
 *
 * FIXME: this actually breaks i18n because the schemas.in->schemas process
 * doesn't recognise the concatenated strings, and so we will have to do
 * them ourselves; this will need to be fixed before the next release.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#define _(x) x

static void single_stanza (gboolean is_window, char *name, char *default_value,
               gboolean can_reverse, gboolean going_backwards,
               char *short_description, char *long_description);

char *liberal, *could_go_backwards, *could_go_forwards;

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
single_stanza (gboolean is_window, char *name, char *default_value,
               gboolean can_reverse, gboolean going_backwards,
               char *short_description, char *long_description)
{
  char *keybinding_type = is_window? "window": "global";
  char *escaped_default_value;

  if (short_description == NULL || long_description == NULL)
    /* it must be undocumented, so it can't be in this table */
    return;

  /* Escape the text.  The old values point at constants (literals, actually)
   * so it doesn't matter that we lose the reference.
   */
  short_description = g_markup_escape_text (short_description, -1);
  long_description = g_markup_escape_text (long_description, -1);
  
  escaped_default_value = g_markup_escape_text (
        default_value? default_value: "disabled",
        -1);
  
  printf ("    <schema>\n");
  printf ("      <key>/schemas/apps/metacity/%s_keybindings/%s</key>\n",
            keybinding_type, name);
  printf ("      <applyto>/apps/metacity/%s_keybindings/%s</applyto>\n",
            keybinding_type, name);
  printf ("      <owner>metacity</owner>\n");
  printf ("      <type>string</type>\n");
  printf ("      <default>%s</default>\n", escaped_default_value);
  printf ("      <locale name=\"C\">\n");
  printf ("        <short>%s</short>\n", short_description);
  printf ("        <long>\n");
  printf ("          %s\n", long_description);

  if (can_reverse)
    {
      /* I don't think this is very useful, tbh: */
      if (default_value != NULL)
        {
          printf (" (Traditionally %s)\n", escaped_default_value);
        }

      if (going_backwards)
        printf ("%s\n", could_go_forwards);
      else
        printf ("%s\n", could_go_backwards);
    }

  printf ("          %s\n", liberal);
  
  printf ("        </long>\n");
  printf ("      </locale>\n");
  printf ("    </schema>\n\n\n");
  
  g_free (escaped_default_value);
  g_free (short_description);
  g_free (long_description);
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
#define item(name, suffix, param, short, long, keystroke) \
  single_stanza (TRUE, #name suffix, \
              keystroke, \
              FALSE, FALSE, \
              short, long);
#include "window-bindings.h"
#undef item

#define item(name, suffix, param, flags, short, long, keystroke) \
  single_stanza (FALSE, #name suffix,  \
               keystroke, \
               flags & BINDING_REVERSES, \
               flags & BINDING_IS_REVERSED, \
               short, long);
#include "screen-bindings.h"
#undef item
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
  liberal = g_markup_escape_text(_("The format looks like \"<Control>a\" or "
        "<Shift><Alt>F1\". \n"\
        "The parser is fairly liberal and allows "\
  	"lower or upper case, and also abbreviations such as \"<Ctl>\" and " \
	"\"<Ctrl>\". If you set the option to the special string " \
	"\"disabled\", then there will be no keybinding for this action."), -1);

  /* These were more dissimilar at some point but have been regularised
   * for the translators' benefit.
   */
  could_go_backwards = g_markup_escape_text (_("Holding the \"shift\" key "
        "while using this binding reverses the direction of movement."), -1);

  could_go_forwards = g_markup_escape_text (_("Holding the \"shift\" key "
        "while using this binding makes the direction go forward again."), -1);

  produce_bindings ();
  
  g_free (could_go_forwards);
  g_free (could_go_backwards);
  g_free (liberal);
  
  return 0;
}

/* eof schema-bindings.c */

