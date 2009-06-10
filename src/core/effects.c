/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file effects.c "Special effects" other than compositor effects.
 * 
 * Before we had a serious compositor, we supported swooping
 * rectangles for minimising and so on.  These are still supported
 * today, even when the compositor is enabled.  The file contains two
 * parts:
 *
 *  1) A set of functions, each of which implements a special effect.
 *     (Only the minimize function does anything interesting; we should
 *      probably get rid of the rest.)
 *
 *  2) A set of functions for moving a highlighted wireframe box around
 *     the screen, optionally with height and width shown in the middle.
 *     This is used for moving and resizing when reduced_resources is set.
 *
 * There was formerly a system which allowed callers to drop in their
 * own handlers for various things; it was never used (people who want
 * their own handlers can just modify this file, after all) and it added
 * a good deal of extra complexity, so it has been removed.  If you want it,
 * it can be found in svn r3769.
 *
 * Once upon a time there were three different ways of drawing the box
 * animation: window wireframe, window opaque, and root. People who had
 * the shape extension theoretically had the choice of all three, and
 * people who didn't weren't given the choice of the wireframe option.
 * In practice, though, the opaque animation was never perfect, so it came
 * down to the wireframe option for those who had the extension and
 * the root option for those who didn't; there was actually no way of choosing
 * any other option anyway.  Work on the opaque animation stopped in 2002;
 * anyone who wants something like that these days will be using the
 * compositor anyway.
 *
 * In svn r3769 this was made explicit.
 */

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
#include "display-private.h"
#include "ui.h"
#include "window-private.h"
#include "prefs.h"

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

#define META_MINIMIZE_ANIMATION_LENGTH 0.25
#define META_SHADE_ANIMATION_LENGTH 0.2

#include <string.h>

typedef struct MetaEffect MetaEffect;
typedef struct MetaEffectPriv MetaEffectPriv;

typedef struct
{
  MetaScreen *screen;

  double millisecs_duration;
  GTimeVal start_time;

#ifdef HAVE_SHAPE
  /** For wireframe window */
  Window wireframe_xwindow;
#else
  /** Rectangle to erase */
  MetaRectangle last_rect;

  /** First time we've plotted anything in this animation? */
  gboolean first_time;

  /** For wireframe drawn on root window */
  GC gc;
#endif

  MetaRectangle start_rect;
  MetaRectangle end_rect;
  
} BoxAnimationContext;

/**
 * Information we need to know during a maximise or minimise effect.
 */
typedef struct
{
  /** This is the normal-size window. */
  MetaRectangle window_rect;
  /** This is the size of the window when it's an icon. */
  MetaRectangle icon_rect;
} MetaMinimizeEffect, MetaUnminimizeEffect;

struct MetaEffectPriv
{
  MetaEffectFinished finished;
  gpointer           finished_data;
};

struct MetaEffect
{
  /** The window the effect is applied to. */
  MetaWindow *window;
  /** Which effect is happening here. */
  MetaEffectType type;
  /** The effect handler can hang data here. */
  gpointer info;

  union
  {
    MetaMinimizeEffect      minimize;
    /* ... and theoretically anything else */
  } u;
  
  MetaEffectPriv *priv;
};

static void run_default_effect_handler (MetaEffect *effect);
static void run_handler (MetaEffect *effect);
static void effect_free (MetaEffect *effect);

static MetaEffect *
create_effect (MetaEffectType      type,
               MetaWindow         *window,
               MetaEffectFinished  finished,
               gpointer            finished_data);

static void
draw_box_animation (MetaScreen     *screen,
                    MetaRectangle  *initial_rect,
                    MetaRectangle  *destination_rect,
                    double          seconds_duration);

/**
 * Creates an effect.
 *
 */
static MetaEffect*
create_effect (MetaEffectType      type,
               MetaWindow         *window,
               MetaEffectFinished  finished,
               gpointer            finished_data)
{
    MetaEffect *effect = g_new (MetaEffect, 1);

    effect->type = type;
    effect->window = window;
    effect->priv = g_new (MetaEffectPriv, 1);
    effect->priv->finished = finished;
    effect->priv->finished_data = finished_data;

    return effect;
}

/**
 * Destroys an effect.  If the effect has a "finished" hook, it will be
 * called before cleanup.
 *
 * \param effect  The effect.
 */
static void
effect_free (MetaEffect *effect)
{
  if (effect->priv->finished)
    effect->priv->finished (effect->priv->finished_data);
    
  g_free (effect->priv);
  g_free (effect);
}

void
meta_effect_run_focus (MetaWindow	    *window,
		       MetaEffectFinished   finished,
		       gpointer		    data)
{
    MetaEffect *effect;

    g_return_if_fail (window != NULL);

    effect = create_effect (META_EFFECT_FOCUS, window, finished, data);
    
    run_handler (effect);
}

void
meta_effect_run_minimize (MetaWindow         *window,
                          MetaRectangle      *window_rect,
                          MetaRectangle      *icon_rect,
                          MetaEffectFinished  finished,
                          gpointer            data)
{
    MetaEffect *effect;

    g_return_if_fail (window != NULL);
    g_return_if_fail (icon_rect != NULL);
    
    effect = create_effect (META_EFFECT_MINIMIZE, window, finished, data);

    effect->u.minimize.window_rect = *window_rect;
    effect->u.minimize.icon_rect = *icon_rect;

    run_handler (effect);
}

void
meta_effect_run_unminimize (MetaWindow         *window,
                            MetaRectangle      *window_rect,
                            MetaRectangle      *icon_rect,
                            MetaEffectFinished  finished,
                            gpointer            data)
{
    MetaEffect *effect;

    g_return_if_fail (window != NULL);
    g_return_if_fail (icon_rect != NULL);
    
    effect = create_effect (META_EFFECT_UNMINIMIZE, window, finished, data);

    effect->u.minimize.window_rect = *window_rect;
    effect->u.minimize.icon_rect = *icon_rect;

    run_handler (effect);
}

void
meta_effect_run_close (MetaWindow         *window,
		       MetaEffectFinished  finished,
		       gpointer            data)
{
    MetaEffect *effect;
    
    g_return_if_fail (window != NULL);

    effect = create_effect (META_EFFECT_CLOSE, window,
                            finished, data);

    run_handler (effect);
}


/* old ugly minimization effect */

#ifdef HAVE_SHAPE  
static void
update_wireframe_window (MetaDisplay         *display,
                         Window               xwindow,
                         const MetaRectangle *rect)
{
  XMoveResizeWindow (display->xdisplay,
                     xwindow,
                     rect->x, rect->y,
                     rect->width, rect->height);

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
}
#endif

/**
 * A hack to force the X server to synchronize with the
 * graphics hardware.
 */
static void
graphics_sync (BoxAnimationContext *context)
{
  XImage *image;
  
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
  
#ifndef HAVE_SHAPE
  if (!context->first_time)
    {
       /* Restore the previously drawn background */
       XDrawRectangle (context->screen->display->xdisplay,
                       context->screen->xroot,
                       context->gc,
                       context->last_rect.x, context->last_rect.y,
                       context->last_rect.width, context->last_rect.height);
    }
  else
    context->first_time = FALSE;

#endif /* !HAVE_SHAPE */

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
#ifdef HAVE_SHAPE
        XDestroyWindow (context->screen->display->xdisplay,
                          context->wireframe_xwindow);
#else
        meta_display_ungrab (context->screen->display);
        meta_ui_pop_delay_exposes (context->screen->ui);
        XFreeGC (context->screen->display->xdisplay,
                 context->gc);
#endif /* !HAVE_SHAPE */

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

#ifdef HAVE_SHAPE
  update_wireframe_window (context->screen->display,
                           context->wireframe_xwindow,
                           &draw_rect);
#else
  context->last_rect = draw_rect;

  /* Draw the rectangle */
  XDrawRectangle (context->screen->display->xdisplay,
                  context->screen->xroot,
                  context->gc,
                  draw_rect.x, draw_rect.y,
                  draw_rect.width, draw_rect.height);
    
#endif /* !HAVE_SHAPE */

  /* kick changes onto the server */
  graphics_sync (context);
  
  return TRUE;
}
 
void
draw_box_animation (MetaScreen     *screen,
                    MetaRectangle  *initial_rect,
                    MetaRectangle  *destination_rect,
                    double          seconds_duration)
{
  BoxAnimationContext *context;

#ifdef HAVE_SHAPE
  XSetWindowAttributes attrs;
#else
  XGCValues gc_values;
#endif
    
  g_return_if_fail (seconds_duration > 0.0);

  if (g_getenv ("MUTTER_DEBUG_EFFECTS"))
    seconds_duration *= 10; /* slow things down */
  
  /* Create the animation context */
  context = g_new0 (BoxAnimationContext, 1);

  context->screen = screen;

  context->millisecs_duration = seconds_duration * 1000.0;

  context->start_rect = *initial_rect;
  context->end_rect = *destination_rect;

#ifdef HAVE_SHAPE

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

#else /* !HAVE_SHAPE */

  context->first_time = TRUE;
  gc_values.subwindow_mode = IncludeInferiors;
  gc_values.function = GXinvert;

  context->gc = XCreateGC (screen->display->xdisplay,
                           screen->xroot,
                           GCSubwindowMode | GCFunction,
                           &gc_values);
      
  /* Grab the X server to avoid screen dirt */
  meta_display_grab (context->screen->display);
  meta_ui_push_delay_exposes (context->screen->ui);
#endif

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
  MetaRectangle shrunk_rect;
  int i;
  
#define LINE_WIDTH META_WIREFRAME_XOR_LINE_WIDTH

  /* We don't want the wireframe going outside the window area.
   * It makes it harder for the user to position windows and it exposes other
   * annoying bugs.
   */
  shrunk_rect = *rect;

  shrunk_rect.x += LINE_WIDTH / 2 + LINE_WIDTH % 2;
  shrunk_rect.y += LINE_WIDTH / 2 + LINE_WIDTH % 2;
  shrunk_rect.width -= LINE_WIDTH + 2 * (LINE_WIDTH % 2);
  shrunk_rect.height -= LINE_WIDTH + 2 * (LINE_WIDTH % 2);

  XDrawRectangle (screen->display->xdisplay,
                  screen->xroot,
                  screen->root_xor_gc,
                  shrunk_rect.x, shrunk_rect.y,
                  shrunk_rect.width, shrunk_rect.height);

  /* Don't put lines inside small rectangles where they won't fit */
  if (shrunk_rect.width < (LINE_WIDTH * 4) ||
      shrunk_rect.height < (LINE_WIDTH * 4))
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


              box_x = shrunk_rect.x + (shrunk_rect.width - box_width) / 2;
              box_y = shrunk_rect.y + (shrunk_rect.height - box_height) / 2;

              if ((box_width < shrunk_rect.width) &&
                  (box_height < shrunk_rect.height))
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

              XFreeFontInfo (NULL, font_struct, 1);

              if ((box_width + LINE_WIDTH) >= (shrunk_rect.width / 3))
                return;

              if ((box_height + LINE_WIDTH) >= (shrunk_rect.height / 3))
                return;
            }
        }
    }

  /* Two vertical lines at 1/3 and 2/3 */
  segments[0].x1 = shrunk_rect.x + shrunk_rect.width / 3;
  segments[0].y1 = shrunk_rect.y + LINE_WIDTH / 2 + LINE_WIDTH % 2;
  segments[0].x2 = segments[0].x1;
  segments[0].y2 = shrunk_rect.y + shrunk_rect.height - LINE_WIDTH / 2;  

  segments[1] = segments[0];
  segments[1].x1 = shrunk_rect.x + (shrunk_rect.width / 3) * 2;
  segments[1].x2 = segments[1].x1;

  /* Now make two horizontal lines at 1/3 and 2/3, but not
   * overlapping the verticals
   */

  segments[2].x1 = shrunk_rect.x + LINE_WIDTH / 2 + LINE_WIDTH % 2;
  segments[2].x2 = segments[0].x1 - LINE_WIDTH / 2;
  segments[2].y1 = shrunk_rect.y + shrunk_rect.height / 3;
  segments[2].y2 = segments[2].y1;

  segments[3] = segments[2];
  segments[3].x1 = segments[2].x2 + LINE_WIDTH;
  segments[3].x2 = segments[1].x1 - LINE_WIDTH / 2;
  
  segments[4] = segments[3];
  segments[4].x1 = segments[3].x2 + LINE_WIDTH;
  segments[4].x2 = shrunk_rect.x + shrunk_rect.width - LINE_WIDTH / 2;

  /* Second horizontal line is just like the first, but
   * shifted down
   */
  i = 5;
  while (i < 8)
    {
      segments[i] = segments[i - 3];
      segments[i].y1 = shrunk_rect.y + (shrunk_rect.height / 3) * 2;
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

static void
run_default_effect_handler (MetaEffect *effect)
{
    switch (effect->type)
    {
    case META_EFFECT_MINIMIZE:
       draw_box_animation (effect->window->screen,
                     &(effect->u.minimize.window_rect),
                     &(effect->u.minimize.icon_rect),
                     META_MINIMIZE_ANIMATION_LENGTH);
       break;

    default:
       break;
    }
}

static void
run_handler (MetaEffect *effect)
{
  if (meta_prefs_get_gnome_animations ())
    run_default_effect_handler (effect);

  effect_free (effect);
}
