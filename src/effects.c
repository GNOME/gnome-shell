/* Metacity animation effects */

/* 
 * Copyright (C) 2001 Anders Carlsson, Havoc Pennington
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

#include "effects.h"
#include "display.h"

typedef struct
{
  MetaScreen *screen;
  
  GC gc;

  int step;
  int steps;
  
  double current_x, current_y;
  double current_width, current_height;
  
  double delta_x, delta_y;
  double delta_width, delta_height;
} BoxAnimationContext;

static gboolean
effects_draw_box_animation_timeout (BoxAnimationContext *context)
{
  if (context->step == 0)
    {
      /* It's our first time, grab the X server */
      meta_display_grab (context->screen->display);
    }
  else
    {
      /* Restore the previously drawn background */
      XDrawRectangle (context->screen->display->xdisplay,
                      context->screen->xroot,
                      context->gc,
                      context->current_x, context->current_y,
                      context->current_width, context->current_height);
    }

  /* Return if we're done */
  if (context->step == context->steps)
    {
      meta_display_ungrab (context->screen->display);
      XFreeGC (context->screen->display->xdisplay,
               context->gc);
      g_free (context);
      return FALSE;
    }

  context->current_x += context->delta_x;
  context->current_y += context->delta_y;
  context->current_width += context->delta_width;
  context->current_height += context->delta_height;
	
  /* Draw the rectangle */
  XDrawRectangle (context->screen->display->xdisplay,
                  context->screen->xroot,
                  context->gc,
                  context->current_x, context->current_y,
                  context->current_width, context->current_height);  

  context->step += 1;

  return TRUE;
}

void
meta_effects_draw_box_animation (MetaScreen *screen,
                                 MetaRectangle *initial_rect,
                                 MetaRectangle *destination_rect,
                                 int steps,
                                 int delay)
{
  BoxAnimationContext *context;
  XGCValues gc_values;

  /* Create the animation context */
  context = g_new (BoxAnimationContext, 1);
	
  gc_values.subwindow_mode = IncludeInferiors;
  gc_values.function = GXinvert;
	
  /* Create a gc for the root window */
  context->screen = screen;
  context->gc = XCreateGC (screen->display->xdisplay,
                           screen->xroot,
                           GCSubwindowMode | GCFunction,
                           &gc_values);
  context->step = 0;
  context->steps = steps;
  context->delta_x = (destination_rect->x - initial_rect->x) / (double)steps;
  context->delta_y = (destination_rect->y - initial_rect->y) / (double)steps;
  context->delta_width = (destination_rect->width - initial_rect->width) / (double)steps;
  context->delta_height = (destination_rect->height - initial_rect->height) / (double)steps;
  
  context->current_x = initial_rect->x;
  context->current_y = initial_rect->y;
  context->current_width = initial_rect->width;
  context->current_height = initial_rect->height;
  
  /* Add the timeout */
  g_timeout_add (delay,
                 (GSourceFunc)effects_draw_box_animation_timeout,
                 context);
}
