/* Metacity window placement */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "place.h"

void
meta_window_place (MetaWindow *window,
                   MetaFrameGeometry *fgeom,
                   int         x,
                   int         y,
                   int        *new_x,
                   int        *new_y)
{
  /* frame member variables should NEVER be used in here, only
   * MetaFrameGeometry
   */
  
  meta_verbose ("Placing window %s\n", window->desc);      
      
  if (window->xtransient_for != None)
    {
      /* Center horizontally, at top of parent vertically */

      MetaWindow *parent;
          
      parent =
        meta_display_lookup_x_window (window->display,
                                      window->xtransient_for);

      if (parent)
        {
          int w;

          meta_window_get_position (parent, &x, &y);
          w = parent->rect.width;

          /* center of parent */
          x = x + w / 2;
          /* center of child over center of parent */
          x -= window->rect.width / 2;

          y += fgeom->top_height;

          meta_verbose ("Centered window %s over transient parent\n",
                        window->desc);

          goto done;
        }
    }

  if (window->type == META_WINDOW_DIALOG ||
      window->type == META_WINDOW_MODAL_DIALOG)
    {
      /* Center on screen */
      int w, h;

          /* I think whole screen will look nicer than workarea */
      w = WidthOfScreen (window->screen->xscreen);
      h = HeightOfScreen (window->screen->xscreen);

      x = (w - window->rect.width) / 2;
      y = (y - window->rect.height) / 2;

      meta_verbose ("Centered window %s on screen\n",
                    window->desc);

      goto done;
    }

  /* "Origin" placement algorithm */
  x = 0;
  y = 0;

 done:
  *new_x = x;
  *new_y = y;
}
