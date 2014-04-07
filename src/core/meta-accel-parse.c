/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "meta-accel-parse.h"

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

static void
accelerator_parse (const char      *accel,
                   guint           *keysym,
                   guint           *keycode,
                   GdkModifierType *keymask)
{
  const char *above_tab;

  if (accel[0] == '0' && accel[1] == 'x')
    {
      *keysym = 0;
      *keycode = (guint) strtoul (accel, NULL, 16);
      *keymask = 0;

      return;
    }

  /* The key name 'Above_Tab' is special - it's not an actual keysym name,
   * but rather refers to the key above the tab key. In order to use
   * the GDK parsing for modifiers in combination with it, we substitute
   * it with 'Tab' temporarily before calling gtk_accelerator_parse().
   */
#define is_word_character(c) (g_ascii_isalnum(c) || ((c) == '_'))
#define ABOVE_TAB "Above_Tab"
#define ABOVE_TAB_LEN 9

  above_tab = strstr (accel, ABOVE_TAB);
  if (above_tab &&
      (above_tab == accel || !is_word_character (above_tab[-1])) &&
      !is_word_character (above_tab[ABOVE_TAB_LEN]))
    {
      char *before = g_strndup (accel, above_tab - accel);
      char *after = g_strdup (above_tab + ABOVE_TAB_LEN);
      char *replaced = g_strconcat (before, "Tab", after, NULL);

      gtk_accelerator_parse (replaced, NULL, keymask);

      g_free (before);
      g_free (after);
      g_free (replaced);

      *keysym = META_KEY_ABOVE_TAB;
      return;
    }

#undef is_word_character
#undef ABOVE_TAB
#undef ABOVE_TAB_LEN

  gtk_accelerator_parse (accel, keysym, keymask);
}

gboolean
meta_parse_accelerator (const char          *accel,
                        unsigned int        *keysym,
                        unsigned int        *keycode,
                        MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;
  
  *keysym = 0;
  *keycode = 0;
  *mask = 0;

  if (!accel[0] || strcmp (accel, "disabled") == 0)
    return TRUE;
  
  accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym == None && gdk_code == 0)
    return FALSE;
  
  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;
  
  *keysym = gdk_sym;
  *keycode = gdk_code;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;
  
  return TRUE;
}

gboolean
meta_parse_modifier (const char          *accel,
                     MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;
  
  *mask = 0;

  if (accel == NULL || !accel[0] || strcmp (accel, "disabled") == 0)
    return TRUE;
  
  accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym != None || gdk_code != 0)
    return FALSE;
  
  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;
  
  return TRUE;
}
