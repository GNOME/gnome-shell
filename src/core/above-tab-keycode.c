/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Find the keycode for the key above the tab key */
/*
 * Copyright 2010 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* The standard cycle-windows keybinding should be the key above the
 * tab key. This will have a different keysym on different keyboards -
 * it's the ` (grave) key on US keyboards but something else on many
 * other national layouts. So we need to figure out the keycode for
 * this key without reference to key symbol.
 *
 * The "correct" way to do this is to get the XKB geometry from the
 * X server, find the Tab key, find the key above the Tab key in the
 * same section and use the keycode for that key. This is what I
 * implemented here, but unfortunately, fetching the geometry is rather
 * slow (It could take 20ms or more.)
 *
 * If you looking for a way to optimize Mutter startup performance:
 * On all Linux systems using evdev the key above TAB will have
 * keycode 49. (KEY_GRAVE=41 + the 8 code point offset between
 * evdev keysyms and X keysyms.) So a configure option
 * --with-above-tab-keycode=49 could be added that bypassed this
 * code. It wouldn't work right for displaying Mutter remotely
 * to a non-Linux X server, but that is pretty rare.
 */

#include <config.h>

#include <string.h>

#include "display-private.h"

#include <X11/keysym.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>

static guint
compute_above_tab_keycode (Display *xdisplay)
{
  XkbDescPtr keyboard;
  XkbGeometryPtr geometry;
  int i, j, k;
  int tab_keycode;
  char *tab_name;
  XkbSectionPtr tab_section;
  XkbBoundsRec tab_bounds;
  XkbKeyPtr best_key = NULL;
  guint best_keycode = (guint)-1;
  int best_x_dist = G_MAXINT;
  int best_y_dist = G_MAXINT;

  /* We need only the Names and the Geometry, but asking for these results
   * in the Keyboard information retrieval failing for unknown reasons.
   * (Testing with xorg-1.9.1.) So we ask for a part that we don't need
   * as well.
   */
  keyboard = XkbGetKeyboard (xdisplay,
                             XkbGBN_ClientSymbolsMask | XkbGBN_KeyNamesMask | XkbGBN_GeometryMask,
                             XkbUseCoreKbd);
  if (!keyboard)
    return best_keycode;

  geometry = keyboard->geom;

  /* There could potentially be multiple keys with the Tab keysym on the keyboard;
   * but XKeysymToKeycode() returns us the one that the alt-Tab binding will
   * use which is good enough
   */
  tab_keycode = XKeysymToKeycode (xdisplay, XK_Tab);
  if (tab_keycode == 0 || tab_keycode < keyboard->min_key_code || tab_keycode > keyboard->max_key_code)
    goto out;

  /* The keyboard geometry is stored by key "name" rather than keycode.
   * (Key names are 4-character strings like like TAB or AE01.) We use the
   * 'names' part of the keyboard description to map keycode to key name.
   *
   * XKB has a "key aliases" feature where a single keyboard key can have
   * multiple names (with separate sets of aliases in the 'names' part and
   * in the 'geometry' part), but I don't really understand it or how it is used,
   * so I'm ignoring it here.
   */

  tab_name = keyboard->names->keys[tab_keycode].name; /* Not NULL terminated! */

  /* First, iterate through the keyboard geometry to find the tab key; the keyboard
   * geometry has a three-level heirarchy of section > row > key
   */
  for (i = 0; i < geometry->num_sections; i++)
    {
      XkbSectionPtr section = &geometry->sections[i];
      for (j = 0; j < section->num_rows; j++)
        {
          int x = 0;
          int y = 0;

          XkbRowPtr row = &section->rows[j];
          for (k = 0; k < row->num_keys; k++)
            {
              XkbKeyPtr key = &row->keys[k];
              XkbShapePtr shape = XkbKeyShape (geometry, key);

              if (row->vertical)
                y += key->gap;
              else
                x += key->gap;

              if (strncmp (key->name.name, tab_name, XkbKeyNameLength) == 0)
                {
                  tab_section = section;
                  tab_bounds = shape->bounds;
                  tab_bounds.x1 += row->left + x;
                  tab_bounds.x2 += row->left + x;
                  tab_bounds.y1 += row->top + y;
                  tab_bounds.y2 += row->top + y;

                  goto found_tab;
                }

              if (row->vertical)
                y += (shape->bounds.y2 - shape->bounds.y1);
              else
                x += (shape->bounds.x2 - shape->bounds.x1);
            }
        }
    }

  /* No tab key found */
  goto out;

 found_tab:

  /* Now find the key that:
   *  - Is in the same section as the Tab key
   *  - Has a horizontal center in the Tab key's horizonal bounds
   *  - Is above the Tab key at a distance closer than any other key
   *  - In case of ties, has its horizontal center as close as possible
   *    to the Tab key's horizontal center
   */
  for (j = 0; j < tab_section->num_rows; j++)
    {
      int x = 0;
      int y = 0;

      XkbRowPtr row = &tab_section->rows[j];
      for (k = 0; k < row->num_keys; k++)
        {
          XkbKeyPtr key = &row->keys[k];
          XkbShapePtr shape = XkbKeyShape(geometry, key);
          XkbBoundsRec bounds = shape->bounds;
          int x_center;
          int x_dist, y_dist;

          if (row->vertical)
            y += key->gap;
          else
            x += key->gap;

          bounds.x1 += row->left + x;
          bounds.x2 += row->left + x;
          bounds.y1 += row->top + y;
          bounds.y2 += row->top + y;

          y_dist = tab_bounds.y1 - bounds.y2;
          if (y_dist < 0)
            continue;

          x_center = (bounds.x1 + bounds.x2) / 2;
          if (x_center < tab_bounds.x1 || x_center > tab_bounds.x2)
            continue;

          x_dist = ABS (x_center - (tab_bounds.x1 + tab_bounds.x2) / 2);

          if (y_dist < best_y_dist ||
              (y_dist == best_y_dist && x_dist < best_x_dist))
            {
              best_key = key;
              best_x_dist = x_dist;
              best_y_dist = y_dist;
             }

          if (row->vertical)
            y += (shape->bounds.y2 - shape->bounds.y1);
          else
            x += (shape->bounds.x2 - shape->bounds.x1);
        }
    }

  if (best_key == NULL)
    goto out;

  /* Now we need to resolve the name of the best key back to a keycode */
  for (i = keyboard->min_key_code; i < keyboard->max_key_code; i++)
    {
      if (strncmp (best_key->name.name, keyboard->names->keys[i].name, XkbKeyNameLength) == 0)
        {
          best_keycode = i;
          break;
        }
    }

 out:
  XkbFreeKeyboard (keyboard, 0, True);

  return best_keycode;
}
#else /* !HAVE_XKB */
static guint
compute_above_tab_keycode (Display *xdisplay)
{
  return XKeysymToKeycode (xdisplay, XK_grave);
}
#endif /* HAVE_XKB */

guint
meta_display_get_above_tab_keycode (MetaDisplay *display)
{
  if (display->above_tab_keycode == 0) /* not yet computed */
    display->above_tab_keycode = compute_above_tab_keycode (display->xdisplay);

  if (display->above_tab_keycode == (guint)-1) /* failed to compute */
    return 0;
  else
    return display->above_tab_keycode;
}
