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

  double millisecs_duration;
  GTimeVal start_time;

  gboolean first_time;

  MetaRectangle start_rect;
  MetaRectangle end_rect;

  /* rect to erase */
  MetaRectangle last_rect;
  
} BoxAnimationContext;

static gboolean
effects_draw_box_animation_timeout (BoxAnimationContext *context)
{
  double elapsed;
  GTimeVal current_time;
  MetaRectangle draw_rect;
  double fraction;
  
  if (!context->first_time)
    {
      /* Restore the previously drawn background */
      XDrawRectangle (context->screen->display->xdisplay,
                      context->screen->xroot,
                      context->gc,
                      context->last_rect.x, context->last_rect.y,
                      context->last_rect.width, context->last_rect.height);
    }

  context->first_time = FALSE;

  g_get_current_time (&current_time);
  
  /* We use milliseconds for all times */
  elapsed =
    ((((double)current_time.tv_sec - context->start_time.tv_sec) * G_USEC_PER_SEC +
      (current_time.tv_usec - context->start_time.tv_usec))) / 1000.0;
  
  if (elapsed < 0)
    {
      /* Probably the system clock was set backwards? */
      meta_warning ("System clock seemed to go backwards?\n");
      elapsed = G_MAXDOUBLE; /* definitely done. */
    }

  if (elapsed > context->millisecs_duration)
    {
      /* All done */
      meta_display_ungrab (context->screen->display);
      XFreeGC (context->screen->display->xdisplay,
               context->gc);
      g_free (context);
      return FALSE;
    }

  g_assert (context->millisecs_duration > 0.0);
  fraction = elapsed / context->millisecs_duration;
  
  draw_rect = context->start_rect;
  
  /* Now add a delta proportional to elapsed time. */
  draw_rect.x += (context->end_rect.x - context->start_rect.x) * fraction;
  draw_rect.y += (context->end_rect.y - context->start_rect.y) * fraction;
  draw_rect.width += (context->end_rect.width - context->start_rect.width) * fraction;
  draw_rect.height += (context->end_rect.height - context->start_rect.height) * fraction;

  /* don't confuse X with bogus rectangles */
  if (draw_rect.width < 1)
    draw_rect.width = 1;
  if (draw_rect.height < 1)
    draw_rect.height = 1;
  
  context->last_rect = draw_rect;
  
  /* Draw the rectangle */
  XDrawRectangle (context->screen->display->xdisplay,
                  context->screen->xroot,
                  context->gc,
                  draw_rect.x, draw_rect.y,
                  draw_rect.width, draw_rect.height);

  return TRUE;
}

void
meta_effects_draw_box_animation (MetaScreen *screen,
                                 MetaRectangle *initial_rect,
                                 MetaRectangle *destination_rect,
                                 double         seconds_duration)
{
  BoxAnimationContext *context;
  XGCValues gc_values;

  g_return_if_fail (seconds_duration > 0.0);

  if (g_getenv ("METACITY_DEBUG_EFFECTS"))
    seconds_duration *= 10; /* slow things down */
  
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

  context->millisecs_duration = seconds_duration * 1000.0;
  g_get_current_time (&context->start_time);
  context->first_time = TRUE;
  context->start_rect = *initial_rect;
  context->end_rect = *destination_rect;
  
  /* Grab the X server to avoid screen dirt */
  meta_display_grab (context->screen->display);
  
  /* Add the timeout - a short one, could even use an idle,
   * but this is maybe more CPU-friendly.
   */
  g_timeout_add (15,
                 (GSourceFunc)effects_draw_box_animation_timeout,
                 context);
}
