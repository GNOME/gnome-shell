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

#include <config.h>
#include "effects.h"
#include "display.h"
#include "ui.h"

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

#include <string.h>

typedef enum
{
  META_ANIMATION_DRAW_ROOT,
  META_ANIMATION_WINDOW_WIREFRAME,
  META_ANIMATION_WINDOW_OPAQUE

} MetaAnimationStyle;

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
  MetaAnimationStyle style;
  
  /* For wireframe drawn on root window */
  GC gc;

  /* For wireframe window */
  Window wireframe_xwindow;
  
  /* For opaque */
  MetaImageWindow *image_window;
  GdkPixbuf       *orig_pixbuf;

  MetaBoxAnimType anim_type;
  
} BoxAnimationContext;

static void
update_wireframe_window (MetaDisplay         *display,
                         Window               xwindow,
                         const MetaRectangle *rect)
{
  XMoveResizeWindow (display->xdisplay,
                     xwindow,
                     rect->x, rect->y,
                     rect->width, rect->height);

#ifdef HAVE_SHAPE  

#define OUTLINE_WIDTH 3
  
  if (rect->width > OUTLINE_WIDTH * 2 &&
      rect->height > OUTLINE_WIDTH * 2)
    {
      XRectangle xrect;
      Region inner_xregion;
      Region outer_xregion;
      
      inner_xregion = XCreateRegion ();
      outer_xregion = XCreateRegion ();

      xrect.x = 0;
      xrect.y = 0;
      xrect.width = rect->width;
      xrect.height = rect->height;
  
      XUnionRectWithRegion (&xrect, outer_xregion, outer_xregion);
  
      xrect.x += OUTLINE_WIDTH;
      xrect.y += OUTLINE_WIDTH;
      xrect.width -= OUTLINE_WIDTH * 2;
      xrect.height -= OUTLINE_WIDTH * 2;  
  
      XUnionRectWithRegion (&xrect, inner_xregion, inner_xregion);

      XSubtractRegion (outer_xregion, inner_xregion, outer_xregion);

      XShapeCombineRegion (display->xdisplay, xwindow,
                           ShapeBounding, 0, 0, outer_xregion, ShapeSet);
  
      XDestroyRegion (outer_xregion);
      XDestroyRegion (inner_xregion);
    }
  else
    {
      /* Unset the shape */
      XShapeCombineMask (display->xdisplay, xwindow,
                         ShapeBounding, 0, 0, None, ShapeSet);
    }
#endif
}

static void
graphics_sync (BoxAnimationContext *context)
{
  XImage *image;
  
  /* A hack to force the X server to synchronize with the
   * graphics hardware
   */
  image = XGetImage (context->screen->display->xdisplay,
		     context->screen->xroot,
		     0, 0, 1, 1,
		     AllPlanes, ZPixmap);

  XDestroyImage (image);
}

static gboolean
effects_draw_box_animation_timeout (BoxAnimationContext *context)
{
  double elapsed;
  GTimeVal current_time;
  MetaRectangle draw_rect;
  double fraction;
  
  if (!context->first_time)
    {
      if (context->style == META_ANIMATION_DRAW_ROOT)
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
      if (context->style == META_ANIMATION_WINDOW_OPAQUE)
        {
          g_object_unref (G_OBJECT (context->orig_pixbuf));
          meta_image_window_free (context->image_window);
        }
      else if (context->style == META_ANIMATION_DRAW_ROOT)
        {
          meta_display_ungrab (context->screen->display);
          meta_ui_pop_delay_exposes (context->screen->ui);
          XFreeGC (context->screen->display->xdisplay,
                   context->gc);
        }
      else if (context->style == META_ANIMATION_WINDOW_WIREFRAME)
        {
          XDestroyWindow (context->screen->display->xdisplay,
                          context->wireframe_xwindow);
        }

      graphics_sync (context);
      
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

  if (context->style == META_ANIMATION_WINDOW_OPAQUE)
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
  else if (context->style == META_ANIMATION_DRAW_ROOT)
    {
      /* Draw the rectangle */
      XDrawRectangle (context->screen->display->xdisplay,
                      context->screen->xroot,
                      context->gc,
                      draw_rect.x, draw_rect.y,
                      draw_rect.width, draw_rect.height);
    }
  else if (context->style == META_ANIMATION_WINDOW_WIREFRAME)
    {
      update_wireframe_window (context->screen->display,
                               context->wireframe_xwindow,
                               &draw_rect);
    }

  /* kick changes onto the server */
  graphics_sync (context);
  
  return TRUE;
}


/* I really don't want this to be a configuration option, but I think
 * the wireframe is sucky from a UI standpoint (more confusing than
 * opaque), but the opaque is definitely still too slow on some
 * systems, and also doesn't look quite right due to the mapping
 * and unmapping of windows that's going on.
 */
 
static MetaAnimationStyle animation_style = META_ANIMATION_WINDOW_WIREFRAME;

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

  context->style = animation_style;

#ifndef HAVE_SHAPE
  if (context->style == META_ANIMATION_WINDOW_WIREFRAME)
    context->style = META_ANIMATION_DRAW_ROOT;
#endif
  
  if (context->style == META_ANIMATION_WINDOW_OPAQUE)
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
          context->style = META_ANIMATION_WINDOW_WIREFRAME;
        }
      else
        {
          context->image_window = meta_image_window_new (screen->display->xdisplay,
                                                         screen->number,
                                                         initial_rect->width,
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
  if (context->style == META_ANIMATION_WINDOW_WIREFRAME)
    {
      XSetWindowAttributes attrs;

      attrs.override_redirect = True;
      attrs.background_pixel = BlackPixel (screen->display->xdisplay,
                                           screen->number);
      
      context->wireframe_xwindow = XCreateWindow (screen->display->xdisplay,
                                                  screen->xroot,
                                                  initial_rect->x,
                                                  initial_rect->y,
                                                  initial_rect->width,
                                                  initial_rect->height,
                                                  0,
                                                  CopyFromParent,
                                                  CopyFromParent,
                                                  (Visual *)CopyFromParent,
                                                  CWOverrideRedirect | CWBackPixel,
                                                  &attrs);

      update_wireframe_window (screen->display,
                               context->wireframe_xwindow,
                               initial_rect);

      XMapWindow (screen->display->xdisplay,
                  context->wireframe_xwindow);
    }
  
  if (context->style == META_ANIMATION_DRAW_ROOT)
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

void
meta_effects_begin_wireframe (MetaScreen          *screen,
                              const MetaRectangle *rect,
                              int                  width,
                              int                  height)
{
  /* Grab the X server to avoid screen dirt */
  meta_display_grab (screen->display);
  meta_ui_push_delay_exposes (screen->ui);  

  meta_effects_update_wireframe (screen, 
                                 NULL, -1, -1,
                                 rect, width, height);
}

static void
draw_xor_rect (MetaScreen          *screen,
               const MetaRectangle *rect,
               int                  width,
               int                  height)
{
  /* The lines in the center can't overlap the rectangle or each
   * other, or the XOR gets reversed. So we have to draw things
   * a bit oddly.
   */
  XSegment segments[8];
  int i;
  
#define LINE_WIDTH META_WIREFRAME_XOR_LINE_WIDTH
  
  XDrawRectangle (screen->display->xdisplay,
                  screen->xroot,
                  screen->root_xor_gc,
                  rect->x, rect->y,
                  rect->width, rect->height);

  /* Don't put lines inside small rectangles where they won't fit */
  if (rect->width < (LINE_WIDTH * 4) ||
      rect->height < (LINE_WIDTH * 4))
    return;

  if ((width >= 0) && (height >= 0))
    {
      XGCValues gc_values = { 0 };

      if (XGetGCValues (screen->display->xdisplay,
                        screen->root_xor_gc, 
                        GCFont, &gc_values))
        {
          char *text;
          int text_length;

          XFontStruct *font_struct;
          int text_width, text_height; 
          int box_x, box_y;
          int box_width, box_height;

          font_struct = XQueryFont (screen->display->xdisplay,
                                    gc_values.font);

          if (font_struct != NULL)
            {
              text = g_strdup_printf ("%d x %d", width, height);
              text_length = strlen (text);

              text_width = text_length * font_struct->max_bounds.width;
              text_height = font_struct->max_bounds.descent + 
                            font_struct->max_bounds.ascent;

              box_width = text_width + 2 * LINE_WIDTH;
              box_height = text_height + 2 * LINE_WIDTH;


              box_x = rect->x + (rect->width - box_width) / 2;
              box_y = rect->y + (rect->height - box_height) / 2;

              if ((box_width < rect->width) &&
                  (box_height < rect->height))
                {
                  XFillRectangle (screen->display->xdisplay,
                                  screen->xroot,
                                  screen->root_xor_gc,
                                  box_x, box_y,
                                  box_width, box_height);
                  XDrawString (screen->display->xdisplay, 
                               screen->xroot,
                               screen->root_xor_gc,
                               box_x + LINE_WIDTH,
                               box_y + LINE_WIDTH + font_struct->max_bounds.ascent,
                               text, text_length);
                }

              g_free (text);

              if ((box_width + LINE_WIDTH) >= (rect->width / 3))
                return;

              if ((box_height + LINE_WIDTH) >= (rect->height / 3))
                return;
            }
        }
    }

  /* Two vertical lines at 1/3 and 2/3 */
  segments[0].x1 = rect->x + rect->width / 3;
  segments[0].y1 = rect->y + LINE_WIDTH / 2 + LINE_WIDTH % 2;
  segments[0].x2 = segments[0].x1;
  segments[0].y2 = rect->y + rect->height - LINE_WIDTH / 2;  

  segments[1] = segments[0];
  segments[1].x1 = rect->x + (rect->width / 3) * 2;
  segments[1].x2 = segments[1].x1;

  /* Now make two horizontal lines at 1/3 and 2/3, but not
   * overlapping the verticals
   */

  segments[2].x1 = rect->x + LINE_WIDTH / 2 + LINE_WIDTH % 2;
  segments[2].x2 = segments[0].x1 - LINE_WIDTH / 2;
  segments[2].y1 = rect->y + rect->height / 3;
  segments[2].y2 = segments[2].y1;

  segments[3] = segments[2];
  segments[3].x1 = segments[2].x2 + LINE_WIDTH;
  segments[3].x2 = segments[1].x1 - LINE_WIDTH / 2;
  
  segments[4] = segments[3];
  segments[4].x1 = segments[3].x2 + LINE_WIDTH;
  segments[4].x2 = rect->x + rect->width - LINE_WIDTH / 2;

  /* Second horizontal line is just like the first, but
   * shifted down
   */
  i = 5;
  while (i < 8)
    {
      segments[i] = segments[i - 3];
      segments[i].y1 = rect->y + (rect->height / 3) * 2;
      segments[i].y2 = segments[i].y1;
      ++i;
    }
  
  XDrawSegments (screen->display->xdisplay,
                 screen->xroot,
                 screen->root_xor_gc,
                 segments,
                 G_N_ELEMENTS (segments));
}

void
meta_effects_update_wireframe (MetaScreen          *screen,
                               const MetaRectangle *old_rect,
                               int                  old_width,
                               int                  old_height,
                               const MetaRectangle *new_rect,
                               int                  new_width,
                               int                  new_height)
{
  if (old_rect)
    draw_xor_rect (screen, old_rect, old_width, old_height);
    
  if (new_rect)
    draw_xor_rect (screen, new_rect, new_width, new_height);
    
  XFlush (screen->display->xdisplay);
}

void
meta_effects_end_wireframe (MetaScreen          *screen,
                            const MetaRectangle *old_rect,
                            int                  old_width,
                            int                  old_height)
{
  meta_effects_update_wireframe (screen, 
                                 old_rect, old_width, old_height,
                                 NULL, -1, -1);
  
  meta_display_ungrab (screen->display);
  meta_ui_pop_delay_exposes (screen->ui);
}

