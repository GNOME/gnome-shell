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
#include "ui.h"

typedef struct
{
  MetaScreen *screen;

  double millisecs_duration;
  GTimeVal start_time;

  gboolean first_time;

  MetaRectangle start_rect;
  MetaRectangle end_rect;

  /* rect to erase */
  MetaRectangle last_rect;

  /* used instead of the global flag, since
   * we don't want to change midstream.
   */
  gboolean use_opaque;
  
  /* For wireframe */
  GC gc;

  /* For opaque */
  MetaImageWindow *image_window;
  GdkPixbuf       *orig_pixbuf;

  MetaBoxAnimType anim_type;
  
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
      if (!context->use_opaque)
        {
          /* Restore the previously drawn background */
          XDrawRectangle (context->screen->display->xdisplay,
                          context->screen->xroot,
                          context->gc,
                          context->last_rect.x, context->last_rect.y,
                          context->last_rect.width, context->last_rect.height);
        }
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
      if (context->use_opaque)
        {
          g_object_unref (G_OBJECT (context->orig_pixbuf));
          meta_image_window_free (context->image_window);
        }
      else
        {
          meta_display_ungrab (context->screen->display);
          meta_ui_pop_delay_exposes (context->screen->ui);
          XFreeGC (context->screen->display->xdisplay,
                   context->gc);
        }
      
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
  
  /* don't confuse X or gdk-pixbuf with bogus rectangles */
  if (draw_rect.width < 1)
    draw_rect.width = 1;
  if (draw_rect.height < 1)
    draw_rect.height = 1;
  
  context->last_rect = draw_rect;

  if (context->use_opaque)
    {
      GdkPixbuf *scaled;

      scaled = NULL;
      switch (context->anim_type)
        {
        case META_BOX_ANIM_SCALE:
          scaled = gdk_pixbuf_scale_simple (context->orig_pixbuf,
                                            draw_rect.width,
                                            draw_rect.height,
                                            GDK_INTERP_BILINEAR);
          break;
        case META_BOX_ANIM_SLIDE_UP:
          {
            int x, y;

            x = context->start_rect.width - draw_rect.width;
            y = context->start_rect.height - draw_rect.height;

            /* paranoia */
            if (x < 0)
              x = 0;
            if (y < 0)
              y = 0;
            
            scaled = gdk_pixbuf_new_subpixbuf (context->orig_pixbuf,
                                               x, y,
                                               draw_rect.width,
                                               draw_rect.height);
          }
          break;
        }

      /* handle out-of-memory */
      if (scaled != NULL)
        {
          meta_image_window_set (context->image_window,
                                 scaled,
                                 draw_rect.x, draw_rect.y);
          
          g_object_unref (G_OBJECT (scaled));
        }
    }
  else
    {
      /* Draw the rectangle */
      XDrawRectangle (context->screen->display->xdisplay,
                      context->screen->xroot,
                      context->gc,
                      draw_rect.x, draw_rect.y,
                      draw_rect.width, draw_rect.height);
    }

  /* kick changes onto the server */
  XFlush (context->screen->display->xdisplay);
  
  return TRUE;
}


/* I really don't want this to be a configuration option, but I think
 * the wireframe is sucky from a UI standpoint (more confusing than
 * opaque), but the opaque is definitely still too slow on some
 * systems, and also doesn't look quite right due to the mapping
 * and unmapping of windows that's going on.
 */
 
static gboolean use_opaque_animations = FALSE;

void
meta_effects_draw_box_animation (MetaScreen     *screen,
                                 MetaRectangle  *initial_rect,
                                 MetaRectangle  *destination_rect,
                                 double          seconds_duration,
                                 MetaBoxAnimType anim_type)
{
  BoxAnimationContext *context;

  g_return_if_fail (seconds_duration > 0.0);

  if (g_getenv ("METACITY_DEBUG_EFFECTS"))
    seconds_duration *= 10; /* slow things down */
  
  /* Create the animation context */
  context = g_new0 (BoxAnimationContext, 1);	

  context->screen = screen;

  context->millisecs_duration = seconds_duration * 1000.0;
  context->first_time = TRUE;
  context->start_rect = *initial_rect;
  context->end_rect = *destination_rect;
  context->anim_type = anim_type;

  context->use_opaque = use_opaque_animations;

  if (context->use_opaque)
    {
      GdkPixbuf *pix;
      
      pix = meta_gdk_pixbuf_get_from_window (NULL,
                                             screen->xroot,
                                             initial_rect->x,
                                             initial_rect->y,
                                             0, 0,
                                             initial_rect->width,
                                             initial_rect->height);

      if (pix == NULL)
        {
          /* Fall back to wireframe */
          context->use_opaque = FALSE;
        }
      else
        {
          context->image_window = meta_image_window_new (initial_rect->width,
                                                         initial_rect->height);
          context->orig_pixbuf = pix;
          meta_image_window_set (context->image_window,
                                 context->orig_pixbuf,
                                 initial_rect->x,
                                 initial_rect->y);
          meta_image_window_set_showing (context->image_window, TRUE);
        }
    }

  /* Not an else, so that fallback works */
  if (!context->use_opaque)
    {
      XGCValues gc_values;
      
      gc_values.subwindow_mode = IncludeInferiors;
      gc_values.function = GXinvert;

      context->gc = XCreateGC (screen->display->xdisplay,
                               screen->xroot,
                               GCSubwindowMode | GCFunction,
                               &gc_values);
      
      /* Grab the X server to avoid screen dirt */
      meta_display_grab (context->screen->display);
      meta_ui_push_delay_exposes (context->screen->ui);
    }

  /* Do this only after we get the pixbuf from the server,
   * so that the animation doesn't get truncated.
   */
  g_get_current_time (&context->start_time);
  
  /* Add the timeout - a short one, could even use an idle,
   * but this is maybe more CPU-friendly.
   */
  g_timeout_add (15,
                 (GSourceFunc)effects_draw_box_animation_timeout,
                 context);

  /* kick changes onto the server */
  XFlush (context->screen->display->xdisplay);  
}



