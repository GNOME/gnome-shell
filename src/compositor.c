/* Metacity compositing manager */

/* 
 * Copyright (C) 2003 Red Hat, Inc.
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
#include "compositor.h"

#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>


#endif /* HAVE_COMPOSITE_EXTENSIONS */

/* Unlike MetaWindow, there's one of these for _all_ toplevel windows,
 * override redirect or not. We also track unmapped windows as
 * otherwise on window map we'd have to determine where the
 * newly-mapped window was in the stack. A MetaCompositorWindow may
 * correspond to a metacity window frame rather than an application
 * window.
 */
typedef struct
{
  Window xwindow;

  
  
} MetaCompositorWindow;

typedef struct
{
  GList *windows;

} MetaCompositorScreen;

struct MetaCompositor
{
  MetaDisplay *display;

  int composite_error_base;
  int composite_event_base;
  int damage_error_base;
  int damage_event_base;
  int fixes_error_base;
  int fixes_event_base;
  int render_error_base;
  int render_event_base;  

  GSList *screens;
  
  guint enabled : 1;
};

MetaCompositor*
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;

  compositor = g_new0 (MetaCompositor, 1);

  compositor->display = display;
  
  if (!XCompositeQueryExtension (display->xdisplay,
                                 &compositor->composite_event_base,
                                 &compositor->composite_error_base))
    {
      compositor->composite_event_base = 0;
      compositor->composite_error_base = 0;
    }

  meta_verbose ("Composite extension event base %d error base %d",
                compositor->composite_event_base,
                compositor->composite_error_base);

  if (!XDamageQueryExtension (display->xdisplay,
                              &compositor->damage_event_base,
                              &compositor->damage_error_base))
    {
      compositor->damage_event_base = 0;
      compositor->damage_error_base = 0;
    }

  meta_verbose ("Damage extension event base %d error base %d",
                compositor->damage_event_base,
                compositor->damage_error_base);
  
  if (!XFixesQueryExtension (display->xdisplay,
                             &compositor->fixes_event_base,
                             &compositor->fixes_error_base))
    {
      compositor->fixes_event_base = 0;
      compositor->fixes_error_base = 0;
    }

  meta_verbose ("Fixes extension event base %d error base %d",
                compositor->fixes_event_base,
                compositor->fixes_error_base);

  if (!XRenderQueryExtension (display->xdisplay,
                              &compositor->render_event_base,
                              &compositor->render_error_base))
    {
      compositor->render_event_base = 0;
      compositor->render_error_base = 0;
    }

  meta_verbose ("Render extension event base %d error base %d",
                compositor->render_event_base,
                compositor->render_error_base);
  
  if (compositor->composite_event_base == 0 ||
      compositor->fixes_event_base == 0 ||
      compositor->render_event_base == 0 ||
      compositor->damage_event_base == 0)
    {
      meta_verbose ("Failed to find all extensions needed for compositing manager, disabling compositing manager\n");
      g_assert (!compositor->enabled);
      return compositor;
    }
  
  compositor->enabled = TRUE;
  
  return compositor;
#else /* HAVE_COMPOSITE_EXTENSIONS */
  return (void*) 0xdeadbeef; /* non-NULL value */
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_unref (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  /* There isn't really a refcount at the moment since
   * there's no ref()
   */
  
  g_free (compositor);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *xevent,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (!compositor->enabled)
    return; /* no extension */

  

#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

/* This is called when metacity does its XQueryTree() on startup
 * and when a new window is created.
 */
void
meta_compositor_add_window (MetaCompositor    *compositor,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  if (!compositor->enabled)
    return; /* no extension */

  

#endif /* HAVE_COMPOSITE_EXTENSIONS */
}










