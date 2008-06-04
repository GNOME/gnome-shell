#define _GNU_SOURCE
#define _XOPEN_SOURCE 500 /* for usleep() */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <gdk/gdk.h>

#include "display.h"
#include "screen.h"
#include "frame.h"
#include "errors.h"
#include "window.h"
#include "compositor-private.h"
#include "compositor-clutter.h"
#include "xprops.h"
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter/glx/clutter-glx.h>

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 2
#define HAVE_NAME_WINDOW_PIXMAP 1
#endif

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 3
#define HAVE_COW 1
#else
/* Don't have a cow man...HAAHAAHAA */
#endif

#define USE_IDLE_REPAINT 1

#ifdef HAVE_COMPOSITE_EXTENSIONS
static inline gboolean
composite_at_least_version (MetaDisplay *display,
                            int maj, int min)
{
  static int major = -1;
  static int minor = -1;

  if (major == -1)
    meta_display_get_compositor_version (display, &major, &minor);
  
  return (major > maj || (major == maj && minor >= min));
}

#define have_name_window_pixmap(display) \
  composite_at_least_version (display, 0, 2)
#define have_cow(display) \
  composite_at_least_version (display, 0, 3)

#endif

typedef enum _MetaCompWindowType
{
  META_COMP_WINDOW_NORMAL,
  META_COMP_WINDOW_DND,
  META_COMP_WINDOW_DESKTOP,
  META_COMP_WINDOW_DOCK
} MetaCompWindowType;

typedef struct _MetaCompositorClutter 
{
  MetaCompositor compositor;

  MetaDisplay *display;

  Atom atom_x_root_pixmap;
  Atom atom_x_set_root;
  Atom atom_net_wm_window_opacity;
  Atom atom_net_wm_window_type_dnd;

  Atom atom_net_wm_window_type;
  Atom atom_net_wm_window_type_desktop;
  Atom atom_net_wm_window_type_dock;
  Atom atom_net_wm_window_type_menu;
  Atom atom_net_wm_window_type_dialog;
  Atom atom_net_wm_window_type_normal;
  Atom atom_net_wm_window_type_utility;
  Atom atom_net_wm_window_type_splash;
  Atom atom_net_wm_window_type_toolbar;

#ifdef USE_IDLE_REPAINT
  guint repaint_id;
#endif
  guint enabled : 1;
  guint show_redraw : 1;
  guint debug : 1;
} MetaCompositorClutter;

typedef struct _MetaCompScreen 
{
  MetaScreen *screen;

  ClutterActor *stage;
  GList        *windows;
  GHashTable   *windows_by_xid;
  MetaWindow   *focus_window;

  Window        output;

  XserverRegion all_damage;

  guint         overlays;
  gboolean      compositor_active;
  gboolean      clip_changed;

  GSList       *dock_windows;

  ClutterEffectTemplate *destroy_effect;

} MetaCompScreen;

typedef struct _MetaCompWindow 
{
  MetaScreen *screen;
  MetaWindow *window; /* NULL if this window isn't managed by Metacity */
  Window id;
  XWindowAttributes attrs;

  ClutterActor *actor;

  Pixmap back_pixmap;

  int mode;

  gboolean damaged;
  gboolean shaped;

  MetaCompWindowType type;

  Damage damage;

  gboolean needs_shadow;

  XserverRegion border_size;
  XserverRegion extents;

  XserverRegion border_clip;

  gboolean updates_frozen;
  gboolean update_pending;
} MetaCompWindow;


static MetaCompWindow*
find_window_for_screen (MetaScreen *screen,
                        Window      xwindow)
{
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (info == NULL)
    return NULL;
  
  return g_hash_table_lookup (info->windows_by_xid, (gpointer) xwindow);
}

static MetaCompWindow *
find_window_in_display (MetaDisplay *display,
                        Window       xwindow)
{
  GSList *index;

  for (index = meta_display_get_screens (display); index; index = index->next) 
    {
      MetaCompWindow *cw = find_window_for_screen (index->data, xwindow);

      if (cw != NULL)
        return cw;
    }
  
  return NULL;
}

static MetaCompWindow *
find_window_for_child_window_in_display (MetaDisplay *display,
                                         Window       xwindow)
{
  Window ignored1, *ignored2;
  Window parent;
  guint ignored_children;

  XQueryTree (meta_display_get_xdisplay (display), xwindow, &ignored1,
              &parent, &ignored2, &ignored_children);
  
  if (parent != None)
    return find_window_in_display (display, parent);

  return NULL;
}

static void
get_window_type (MetaDisplay    *display,
                 MetaCompWindow *cw)
{
  MetaCompositorClutter *compositor = (MetaCompositorClutter *)display;
  int n_atoms;
  Atom *atoms, type_atom;
  int i;

  type_atom = None;
  n_atoms = 0;
  atoms = NULL;
  
  meta_prop_get_atom_list (display, cw->id, 
                           compositor->atom_net_wm_window_type,
                           &atoms, &n_atoms);

  for (i = 0; i < n_atoms; i++) 
    {
      if (atoms[i] == compositor->atom_net_wm_window_type_dnd ||
          atoms[i] == compositor->atom_net_wm_window_type_desktop ||
          atoms[i] == compositor->atom_net_wm_window_type_dock ||
          atoms[i] == compositor->atom_net_wm_window_type_toolbar ||
          atoms[i] == compositor->atom_net_wm_window_type_menu ||
          atoms[i] == compositor->atom_net_wm_window_type_dialog ||
          atoms[i] == compositor->atom_net_wm_window_type_normal ||
          atoms[i] == compositor->atom_net_wm_window_type_utility ||
          atoms[i] == compositor->atom_net_wm_window_type_splash)
        {
          type_atom = atoms[i];
          break;
        }
    }

  meta_XFree (atoms);

  if (type_atom == compositor->atom_net_wm_window_type_dnd)
    cw->type = META_COMP_WINDOW_DND;
  else if (type_atom == compositor->atom_net_wm_window_type_desktop)
    cw->type = META_COMP_WINDOW_DESKTOP;
  else if (type_atom == compositor->atom_net_wm_window_type_dock)
    cw->type = META_COMP_WINDOW_DOCK;
  else
    cw->type = META_COMP_WINDOW_NORMAL;

/*   meta_verbose ("Window is %d\n", cw->type); */
}

static gboolean
is_shaped (MetaDisplay *display,
           Window       xwindow)
{
  Display *xdisplay = meta_display_get_xdisplay (display);
  int xws, yws, xbs, ybs;
  unsigned wws, hws, wbs, hbs;
  int bounding_shaped, clip_shaped;

  if (meta_display_has_shape (display))
    {
      XShapeQueryExtents (xdisplay, xwindow, &bounding_shaped,
                          &xws, &yws, &wws, &hws, &clip_shaped,
                          &xbs, &ybs, &wbs, &hbs);
      return (bounding_shaped != 0);
    }
  
  return FALSE;
}

static void 
clutter_cmp_destroy (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS

#endif
}     

static XserverRegion
win_extents (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XRectangle r;

  r.x = cw->attrs.x;
  r.y = cw->attrs.y;
  r.width = cw->attrs.width + cw->attrs.border_width * 2;
  r.height = cw->attrs.height + cw->attrs.border_width * 2;
  
  return XFixesCreateRegion (xdisplay, &r, 1);
}


static void
add_damage (MetaScreen     *screen,
            XserverRegion   damage)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  /*  dump_xserver_region ("add_damage", display, damage); */

  if (info->all_damage) 
    {
      XFixesUnionRegion (xdisplay, info->all_damage, info->all_damage, damage);
      XFixesDestroyRegion (xdisplay, damage);
    } 
  else
    info->all_damage = damage;
}

static void
free_win (MetaCompWindow *cw,
          gboolean        destroy)
{
  MetaDisplay *display = meta_screen_get_display (cw->screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompScreen *info = meta_screen_get_compositor_data (cw->screen);

  /* See comment in map_win */
  if (cw->back_pixmap && destroy) 
    {
      XFreePixmap (xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }
  
  if (cw->border_size) 
    {
      XFixesDestroyRegion (xdisplay, cw->border_size);
      cw->border_size = None;
    }
  
  if (cw->border_clip) 
    {
      XFixesDestroyRegion (xdisplay, cw->border_clip);
      cw->border_clip = None;
    }

  if (cw->extents) 
    {
      XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = None;
    }

  if (destroy) 
    { 
      if (cw->damage != None) {
        meta_error_trap_push (display);
        XDamageDestroy (xdisplay, cw->damage);
        meta_error_trap_pop (display, FALSE);

        cw->damage = None;
      }

      /* The window may not have been added to the list in this case,
         but we can check anyway */
      if (cw->type == META_COMP_WINDOW_DOCK)
        info->dock_windows = g_slist_remove (info->dock_windows, cw);

      clutter_actor_destroy (cw->actor);

      g_free (cw);
    }
}

static void  
on_destroy_effect_complete (ClutterActor *actor,
                            gpointer user_data)
{
  MetaCompWindow *cw = (MetaCompWindow *)user_data;



  free_win (cw, TRUE);
}

static void
destroy_win (MetaDisplay *display,
             Window       xwindow,
             gboolean     gone)
{
  MetaScreen *screen;
  MetaCompScreen *info;
  MetaCompWindow *cw;

  cw = find_window_in_display (display, xwindow);

  if (cw == NULL)
    return;

  printf("destroy %p\n", cw);

  screen = cw->screen;
  
  if (cw->extents != None) 
    {
      add_damage (screen, cw->extents);
      cw->extents = None;
    }
  
  info = meta_screen_get_compositor_data (screen);
  info->windows = g_list_remove (info->windows, (gconstpointer) cw);
  g_hash_table_remove (info->windows_by_xid, (gpointer) xwindow);

  clutter_actor_show (cw->actor);
  clutter_actor_raise_top (cw->actor);
  clutter_actor_set_opacity (cw->actor, 0xff);
  clutter_effect_fade (info->destroy_effect   ,
		       cw->actor,
		       0,
		       on_destroy_effect_complete,
		       (gpointer)cw);
}

static void
restack_win (MetaCompWindow *cw,
             Window          above)
{
  MetaScreen *screen;
  MetaCompScreen *info;
  Window previous_above;
  GList *sibling, *next;

  screen = cw->screen;
  info = meta_screen_get_compositor_data (screen);

  sibling = g_list_find (info->windows, (gconstpointer) cw);
  next = g_list_next (sibling);
  previous_above = None;

  if (next) 
    {
      MetaCompWindow *ncw = (MetaCompWindow *) next->data;
      previous_above = ncw->id;
    }

  /* If above is set to None, the window whose state was changed is on 
   * the bottom of the stack with respect to sibling.
   */
  if (above == None) 
    {
      /* Insert at bottom of window stack */
      info->windows = g_list_delete_link (info->windows, sibling);
      info->windows = g_list_append (info->windows, cw);

      clutter_actor_raise_top (cw->actor);
    } 
  else if (previous_above != above) 
    {
      GList *index;
      
      for (index = info->windows; index; index = index->next) {
        MetaCompWindow *cw2 = (MetaCompWindow *) index->data;
        if (cw2->id == above)
          break;
      }
      
      if (index != NULL) 
        {
          MetaCompWindow *above_win = (MetaCompWindow *) index->data;

          info->windows = g_list_delete_link (info->windows, sibling);
          info->windows = g_list_insert_before (info->windows, index, cw);

          clutter_actor_raise (cw->actor, above_win->actor);
        }
    }
}


static void
resize_win (MetaCompWindow *cw,
            int             x,
            int             y,
            int             width,
            int             height,
            int             border_width,
            gboolean        override_redirect)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  XserverRegion damage;
  gboolean debug;

  if (cw->extents)
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }
  else
    {
      damage = None;
    }

  cw->attrs.x = x;
  cw->attrs.y = y;

  clutter_actor_set_position (cw->actor, x, y);
  
  /* Let named pixmap resync sort this */
  /* clutter_actor_set_size (cw->actor, width, height); */

  if (cw->attrs.width != width || cw->attrs.height != height) 
    {
      if (cw->back_pixmap) 
        {
          XFreePixmap (xdisplay, cw->back_pixmap);
          cw->back_pixmap = None;
        }
    }

  cw->attrs.width = width;
  cw->attrs.height = height;
  cw->attrs.border_width = border_width;
  cw->attrs.override_redirect = override_redirect;

  if (cw->extents)
    XFixesDestroyRegion (xdisplay, cw->extents);

  cw->extents = win_extents (cw);

  if (damage) 
    {
      XFixesUnionRegion (xdisplay, damage, damage, cw->extents);      
    }
  else
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }

  add_damage (screen, damage);

  info->clip_changed = TRUE;
}

static void
map_win (MetaDisplay *display,
         MetaScreen  *screen,
         Window       id)
{
  MetaCompWindow *cw = find_window_for_screen (screen, id);
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (cw == NULL)
    return;

  cw->attrs.map_state = IsViewable;
  cw->damaged = FALSE;

  if (cw->back_pixmap) 
    {
      XFreePixmap (xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }

  printf("map %p\n", cw);

  clutter_actor_show (cw->actor);
}


static void
unmap_win (MetaDisplay *display,
           MetaScreen  *screen,
           Window       id)
{
  MetaCompWindow *cw = find_window_for_screen (screen, id);
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (cw == NULL) 
    {
      return;
    }

  printf("unmap %p\n", cw);

  if (cw->window && cw->window == info->focus_window) 
    info->focus_window = NULL;

  cw->attrs.map_state = IsUnmapped;
  cw->damaged = FALSE;

  if (cw->extents != None) 
    {
      add_damage (screen, cw->extents);
      cw->extents = None;
    }

  free_win (cw, FALSE);
  info->clip_changed = TRUE;

  clutter_actor_hide (cw->actor);
}


static void
add_win (MetaScreen *screen,
         MetaWindow *window,
         Window     xwindow)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);
  MetaCompWindow *cw;
  gulong event_mask;

  if (info == NULL) 
    return;

  if (xwindow == info->output) 
    return;

  cw = g_new0 (MetaCompWindow, 1);
  cw->screen = screen;
  cw->window = window;
  cw->id = xwindow;

  if (!XGetWindowAttributes (xdisplay, xwindow, &cw->attrs)) 
    {
      g_free (cw);
      return;
    }

  get_window_type (display, cw);

  /* If Metacity has decided not to manage this window then the input events
     won't have been set on the window */
  event_mask = cw->attrs.your_event_mask | PropertyChangeMask;
  
  XSelectInput (xdisplay, xwindow, event_mask);

  cw->damaged = FALSE;
  cw->shaped = is_shaped (display, xwindow);

  if (cw->attrs.class == InputOnly)
    cw->damage = None;
  else
    cw->damage = XDamageCreate (xdisplay, xwindow, XDamageReportNonEmpty);

#if 0
  cw->alpha_pict = None;
  cw->shadow_pict = None;
  cw->border_size = None;
  cw->extents = None;
  cw->shadow = None;
  cw->shadow_dx = 0;
  cw->shadow_dy = 0;
  cw->shadow_width = 0;
  cw->shadow_height = 0;

  if (window && meta_window_has_focus (window))
    cw->shadow_type = META_SHADOW_LARGE;
  else
    cw->shadow_type = META_SHADOW_MEDIUM;

  cw->opacity = OPAQUE;
  
  cw->border_clip = None;

#endif

  /* Only add the window to the list of docks if it needs a shadow */
  if (cw->type == META_COMP_WINDOW_DOCK) 
    {
      meta_verbose ("Appending %p to dock windows\n", cw);
      info->dock_windows = g_slist_append (info->dock_windows, cw);
    }

  /* Add this to the list at the top of the stack
     before it is mapped so that map_win can find it again */
  info->windows = g_list_prepend (info->windows, cw);
  g_hash_table_insert (info->windows_by_xid, (gpointer) xwindow, cw);

#if 0  
  cw->back_pixmap = XCompositeNameWindowPixmap (xdisplay, xwindow);
  cw->actor = clutter_glx_texture_pixmap_new_with_pixmap (cw->back_pixmap);
#endif

  cw->actor = clutter_glx_texture_pixmap_new ();

  clutter_container_add_actor (CLUTTER_CONTAINER (info->stage), cw->actor);

  clutter_actor_set_position (cw->actor, cw->attrs.x, cw->attrs.y);

  clutter_actor_hide (cw->actor);

  if (cw->attrs.map_state == IsViewable)
    map_win (display, screen, xwindow);
}

static void
repair_win (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  // XserverRegion parts;
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (cw->id == meta_screen_get_xroot (screen)
      || cw->id == clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage)))
    return;

  meta_error_trap_push (display);

  if (cw->back_pixmap == None)
    {
      gint pxm_width, pxm_height;

      cw->back_pixmap = XCompositeNameWindowPixmap (xdisplay, cw->id);

      if (cw->back_pixmap == None)
        {
          printf("dammit no valid pixmap\n");
          return;
        }

      clutter_x11_texture_pixmap_set_pixmap (CLUTTER_X11_TEXTURE_PIXMAP (cw->actor),
                                             cw->back_pixmap);

      g_object_get (cw->actor,
                    "pixmap-width", &pxm_width,
                    "pixmap-height", &pxm_height,
                    NULL);
      
      clutter_actor_set_size (cw->actor, pxm_width, pxm_height);

      clutter_actor_show (cw->actor);
    }

  if (!cw->damaged) 
    {
      XDamageSubtract (xdisplay, cw->damage, None, None);
    } 
  else 
    {
      XRectangle   *r_damage;
      XRectangle    r_bounds;
      int           i, r_count;
      XserverRegion parts;

      parts = XFixesCreateRegion (xdisplay, 0, 0);
      XDamageSubtract (xdisplay, cw->damage, None, parts);

#if 0      
      r_damage = XFixesFetchRegionAndBounds (xdisplay, 
                                             parts,
                                             &r_count,
                                             &r_bounds);

      if (r_damage)
        {
          for (i = 0; i < r_count; ++i)
            {
              // ClutterGeometry geom;

              clutter_x11_texture_pixmap_update_area 
                (CLUTTER_X11_TEXTURE_PIXMAP (cw->actor),
                 r_damage[i].x,
                 r_damage[i].y,
                 r_damage[i].width,
                 r_damage[i].height);

              /*
              geom.x = clutter_actor_get_x (cw->actor) + r_damage[i].x;
              geom.y = clutter_actor_get_y (cw->actor) + r_damage[i].y;
              geom.width = r_damage[i].width;
              geom.height = r_damage[i].height;

              clutter_stage_queue_redraw_area (info->stage, &geom);
              */
            }
	}

      XFree (r_damage);
#endif

      clutter_x11_texture_pixmap_update_area 
        (CLUTTER_GLX_TEXTURE_PIXMAP (cw->actor), 
         0, 
         0,
         clutter_actor_get_width (cw->actor),
         clutter_actor_get_height (cw->actor));
    }

  meta_error_trap_pop (display, FALSE);

  cw->damaged = TRUE;
}


static void
process_create (MetaCompositorClutter *compositor,
                XCreateWindowEvent    *event,
                MetaWindow            *window)
{
  MetaScreen *screen;
  /* We are only interested in top level windows, others will
     be caught by normal metacity functions */

  screen = meta_display_screen_for_root (compositor->display, event->parent);
  if (screen == NULL)
    return;
  
  if (!find_window_in_display (compositor->display, event->window))
    add_win (screen, window, event->window);
}

static void
process_reparent (MetaCompositorClutter *compositor,
                  XReparentEvent        *event,
                  MetaWindow            *window)
{
  MetaScreen *screen;

  screen = meta_display_screen_for_root (compositor->display, event->parent);
  if (screen != NULL)
    add_win (screen, window, event->window);
  else
    destroy_win (compositor->display, event->window, FALSE); 
}

static void
process_destroy (MetaCompositorClutter *compositor,
                 XDestroyWindowEvent   *event)
{
  destroy_win (compositor->display, event->window, FALSE);
}

static void
process_damage (MetaCompositorClutter *compositor,
                XDamageNotifyEvent    *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->drawable);
  if (cw == NULL)
    return;

  repair_win (cw);
}

static void
process_configure_notify (MetaCompositorClutter  *compositor,
                          XConfigureEvent        *event)
{
  MetaDisplay *display = compositor->display;
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompWindow *cw = find_window_in_display (display, event->window);

  if (cw) 
    {
      restack_win (cw, event->above);
      resize_win (cw, 
                  event->x, event->y, event->width, event->height,
                  event->border_width, event->override_redirect);
    }
  else
    { 
      MetaScreen *screen;
      MetaCompScreen *info;

      /* Might be the root window? */
      screen = meta_display_screen_for_root (display, event->window);
      if (screen == NULL)
        return;

      info = meta_screen_get_compositor_data (screen);
      /*
      if (info->root_buffer)
        {
          XRenderFreePicture (xdisplay, info->root_buffer);
          info->root_buffer = None;
        }

      damage_screen (screen);
      */
    }
}

static void
process_circulate_notify (MetaCompositorClutter  *compositor,
                          XCirculateEvent        *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->window);
  MetaCompWindow *top;
  MetaCompScreen *info;
  MetaScreen *screen;
  GList *first;
  Window above;

  if (!cw) 
    return;

  screen = cw->screen;
  info = meta_screen_get_compositor_data (screen);
  first = info->windows;
  top = (MetaCompWindow *) first->data;

  if ((event->place == PlaceOnTop) && top)
    above = top->id;
  else
    above = None;
  restack_win (cw, above);

  info->clip_changed = TRUE;
}

static void
expose_area (MetaScreen *screen,
             XRectangle *rects,
             int         nrects)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion region;

  region = XFixesCreateRegion (xdisplay, rects, nrects);

  add_damage (screen, region);
}

static void
process_expose (MetaCompositorClutter *compositor,
                XExposeEvent          *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->window);
  MetaScreen *screen = NULL;
  XRectangle rect[1];
  int origin_x = 0, origin_y = 0;

  if (cw != NULL)
    {
      screen = cw->screen;
      origin_x = cw->attrs.x; /* + cw->attrs.border_width; ? */
      origin_y = cw->attrs.y; /* + cw->attrs.border_width; ? */
    }
  else
    {
      screen = meta_display_screen_for_root (compositor->display, 
                                             event->window);
      if (screen == NULL)
        return;
    }

  rect[0].x = event->x + origin_x;
  rect[0].y = event->y + origin_y;
  rect[0].width = event->width;
  rect[0].height = event->height;
  
  expose_area (screen, rect, 1);
}


static void
process_unmap (MetaCompositorClutter *compositor,
               XUnmapEvent           *event)
{
  MetaCompWindow *cw;

  if (event->from_configure) 
    {
      /* Ignore unmap caused by parent's resize */
      return;
    }
  

  cw = find_window_in_display (compositor->display, event->window);
  if (cw)
    unmap_win (compositor->display, cw->screen, event->window);
}

static void
process_map (MetaCompositorClutter *compositor,
             XMapEvent             *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display, 
                                               event->window);

  if (cw)
    map_win (compositor->display, cw->screen, event->window);
}



static void
show_overlay_window (MetaScreen *screen,
                     Window      cow)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);

#ifdef HAVE_COW
  if (have_cow (display))
    {
      XserverRegion region;

      region = XFixesCreateRegion (xdisplay, NULL, 0);
      
      XFixesSetWindowShapeRegion (xdisplay, cow, ShapeBounding, 0, 0, 0);
      XFixesSetWindowShapeRegion (xdisplay, cow, ShapeInput, 0, 0, region);
      
      XFixesDestroyRegion (xdisplay, region);
      
      //damage_screen (screen);
    }
#endif
}

static Window
get_output_window (MetaScreen *screen)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  Window output, xroot;

  xroot = meta_screen_get_xroot (screen);

#ifdef HAVE_COW
  if (have_cow (display))
    {
      output = XCompositeGetOverlayWindow (xdisplay, xroot);
      XSelectInput (xdisplay, output, ExposureMask);
    }
  else
#endif
    {
      output = xroot;
    }

  return output;
}

static void 
clutter_cmp_manage_screen (MetaCompositor *compositor,
                           MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompScreen *info;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  int screen_number = meta_screen_get_screen_number (screen);
  Window xroot = meta_screen_get_xroot (screen);
  Window xwin;
  gint width, height;

  /* Check if the screen is already managed */
  if (meta_screen_get_compositor_data (screen))
    return;

  gdk_error_trap_push ();
  XCompositeRedirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
  XSync (xdisplay, FALSE);

  if (gdk_error_trap_pop ())
    {
      g_warning ("Another compositing manager is running on screen %i",
                 screen_number);
      return;
    }

  info = g_new0 (MetaCompScreen, 1);
  info->screen = screen;
  
  meta_screen_set_compositor_data (screen, info);

  info->output = get_output_window (screen);

  info->all_damage = None;
  
  info->windows = NULL;
  info->windows_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);

  info->focus_window = meta_display_get_focus_window (display);

  info->compositor_active = TRUE;
  info->overlays = 0;
  info->clip_changed = TRUE;

  XClearArea (xdisplay, info->output, 0, 0, 0, 0, TRUE);

  meta_screen_set_cm_selection (screen);

  info->stage = clutter_stage_get_default ();

  meta_screen_get_size (screen, &width, &height);
  clutter_actor_set_size (info->stage, width, height);

  xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XReparentWindow (xdisplay, xwin, info->output, 0, 0);

  clutter_actor_show_all (info->stage);

  /* Now we're up and running we can show the output if needed */
  show_overlay_window (screen, info->output);

  info->destroy_effect 
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (2000),
                                    CLUTTER_ALPHA_SINE_INC);

  printf("managing screen\n");
#endif
}

static void 
clutter_cmp_unmanage_screen (MetaCompositor *compositor,
                             MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS

#endif
}

static void 
clutter_cmp_add_window (MetaCompositor    *compositor,
                        MetaWindow        *window,
                        Window             xwindow,
                        XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositorClutter *xrc = (MetaCompositorClutter *) compositor;
  MetaScreen *screen = meta_screen_for_x_screen (attrs->screen);

  meta_error_trap_push (xrc->display);
  add_win (screen, window, xwindow);
  meta_error_trap_pop (xrc->display, FALSE);
#endif
}

static void 
clutter_cmp_remove_window (MetaCompositor *compositor,
                           Window          xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS

#endif
}

static void 
clutter_cmp_set_updates (MetaCompositor *compositor,
                         MetaWindow     *window,
                         gboolean        update)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS

#endif
}

static void 
clutter_cmp_process_event (MetaCompositor *compositor,
                           XEvent         *event,
                           MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositorClutter *xrc = (MetaCompositorClutter *) compositor;
  /*
   * This trap is so that none of the compositor functions cause
   * X errors. This is really a hack, but I'm afraid I don't understand
   * enough about Metacity/X to know how else you are supposed to do it
   */
  meta_error_trap_push (xrc->display);
  switch (event->type) 
    {
    case CirculateNotify:
      process_circulate_notify (xrc, (XCirculateEvent *) event);
      break;
      
    case ConfigureNotify:
      process_configure_notify (xrc, (XConfigureEvent *) event);
      break;

    case PropertyNotify:
      // process_property_notify (xrc, (XPropertyEvent *) event);
      break;

    case Expose:
      process_expose (xrc, (XExposeEvent *) event);
      break;

    case UnmapNotify:
      process_unmap (xrc, (XUnmapEvent *) event);
      break;

    case MapNotify:
      process_map (xrc, (XMapEvent *) event);
      break;
      
    case ReparentNotify:
      process_reparent (xrc, (XReparentEvent *) event, window);
      break;
      
    case CreateNotify:
      process_create (xrc, (XCreateWindowEvent *) event, window);
      break;
      
    case DestroyNotify:
      process_destroy (xrc, (XDestroyWindowEvent *) event);
      break;
      
    default:
      if (event->type == meta_display_get_damage_event_base (xrc->display) + XDamageNotify)
        {
          process_damage (xrc, (XDamageNotifyEvent *) event);
        }


      /*
      if (event->type == meta_display_get_damage_event_base (xrc->display) + XDamageNotify) 
        process_damage (xrc, (XDamageNotifyEvent *) event);
      else if (event->type == meta_display_get_shape_event_base (xrc->display) + ShapeNotify) 
        process_shape (xrc, (XShapeEvent *) event);
      else 
        {
          meta_error_trap_pop (xrc->display, FALSE);
          return;
        }
      */
      break;
    }
  
  meta_error_trap_pop (xrc->display, FALSE);

#endif
}

static Pixmap 
clutter_cmp_get_window_pixmap (MetaCompositor *compositor,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  return None;
#else
  return None;
#endif
}

static void 
clutter_cmp_set_active_window (MetaCompositor *compositor,
                               MetaScreen     *screen,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS

#endif
}


static MetaCompositor comp_info = {
  clutter_cmp_destroy,
  clutter_cmp_manage_screen,
  clutter_cmp_unmanage_screen,
  clutter_cmp_add_window,
  clutter_cmp_remove_window,
  clutter_cmp_set_updates,
  clutter_cmp_process_event,
  clutter_cmp_get_window_pixmap,
  clutter_cmp_set_active_window
};

MetaCompositor *
meta_compositor_clutter_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  char *atom_names[] = {
    "_XROOTPMAP_ID",
    "_XSETROOT_ID",
    "_NET_WM_WINDOW_OPACITY",
    "_NET_WM_WINDOW_TYPE_DND",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_WINDOW_TYPE_TOOLBAR"
  };
  Atom atoms[G_N_ELEMENTS(atom_names)];
  MetaCompositorClutter *clc;
  MetaCompositor *compositor;
  Display *xdisplay = meta_display_get_xdisplay (display);

  clc = g_new (MetaCompositorClutter, 1);
  clc->compositor = comp_info;

  clutter_x11_set_display (xdisplay);
  clutter_x11_disable_event_retrieval ();
  clutter_init (NULL, NULL);

  printf("clutter initiated\n");

  compositor = (MetaCompositor *) clc;

  clc->display = display;

  meta_verbose ("Creating %d atoms\n", (int) G_N_ELEMENTS (atom_names));
  XInternAtoms (xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);

  clc->atom_x_root_pixmap = atoms[0];
  clc->atom_x_set_root = atoms[1];
  clc->atom_net_wm_window_opacity = atoms[2];
  clc->atom_net_wm_window_type_dnd = atoms[3];
  clc->atom_net_wm_window_type = atoms[4];
  clc->atom_net_wm_window_type_desktop = atoms[5];
  clc->atom_net_wm_window_type_dock = atoms[6];
  clc->atom_net_wm_window_type_menu = atoms[7];
  clc->atom_net_wm_window_type_dialog = atoms[8];
  clc->atom_net_wm_window_type_normal = atoms[9];
  clc->atom_net_wm_window_type_utility = atoms[10];
  clc->atom_net_wm_window_type_splash = atoms[11];
  clc->atom_net_wm_window_type_toolbar = atoms[12];

#ifdef USE_IDLE_REPAINT
  meta_verbose ("Using idle repaint\n");
  clc->repaint_id = 0;
#endif

  clc->enabled = TRUE;

  return compositor;
#else
  return NULL;
#endif
}
