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
#include <glib.h>

#define _(x) x

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

void
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
        printf (could_go_forwards);
      else
        printf (could_go_backwards);
    }
  
  printf ("        </long>\n");
  printf ("      </locale>\n");
  printf ("    </schema>\n\n\n");
  
  g_free (escaped_default_value);
  g_free (short_description);
  g_free (long_description);
}

void
produce_bindings ()
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

int
main ()
{
  /* XXX: TODO: find out what/how gdk i18ns the keycaps as, and add a
   * translator comment
   */
  liberal = g_markup_escape_text(_("The parser is fairly liberal and allows "\
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
}

/* eof schema-bindings.c */

