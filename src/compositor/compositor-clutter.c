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

#include <cogl/cogl.h>

#define SHADOW_RADIUS 10
#define SHADOW_OPACITY	0.9
#define SHADOW_OFFSET_X	(-SHADOW_RADIUS)
#define SHADOW_OFFSET_Y	(-SHADOW_RADIUS)

#define MAX_TILE_SZ 30 	/* make sure size/2 < MAX_TILE_SZ */
#define TILE_WIDTH  (3*MAX_TILE_SZ)
#define TILE_HEIGHT (3*MAX_TILE_SZ)

ClutterActor*
tidy_texture_frame_new (ClutterTexture *texture, 
			gint            left,
			gint            top,
			gint            right,
			gint            bottom);

static unsigned char *
shadow_gaussian_make_tile (void);

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

#endif

typedef enum _MetaCompWindowType
{
  META_COMP_WINDOW_NORMAL,
  META_COMP_WINDOW_DND,
  META_COMP_WINDOW_DESKTOP,
  META_COMP_WINDOW_DOCK,
  META_COMP_WINDOW_MENU,
  META_COMP_WINDOW_DROP_DOWN_MENU,
  META_COMP_WINDOW_TOOLTIP,
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

  guint show_redraw : 1;
  guint debug : 1;
} MetaCompositorClutter;

typedef struct _MetaCompScreen 
{
  MetaScreen   *screen;

  ClutterActor *stage;
  GList        *windows;
  GHashTable   *windows_by_xid;
  MetaWindow   *focus_window;
  Window        output;
  XserverRegion all_damage;
  GSList       *dock_windows;

  ClutterEffectTemplate *destroy_effect;

  ClutterActor *shadow_src;

} MetaCompScreen;

typedef struct _MetaCompWindow 
{
  MetaScreen       *screen;
  MetaWindow       *window;
  Window            id;
  XWindowAttributes attrs;
  ClutterActor     *actor;
  ClutterActor     *shadow;
  Pixmap            back_pixmap;
  int               mode;

  gboolean          damaged;
  gboolean          shaped;

  MetaCompWindowType type;

  Damage             damage;

  gboolean           needs_shadow;

  XserverRegion      extents;

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

  for (index = meta_display_get_screens (display); 
       index; 
       index = index->next) 
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

static gboolean
window_has_shadow (MetaCompWindow *cw)
{
  /* Always put a shadow around windows with a frame - This should override
     the restriction about not putting a shadow around shaped windows
     as the frame might be the reason the window is shaped */
  if (cw->window) 
    {
      if (meta_window_get_frame (cw->window)) {
        meta_verbose ("Window has shadow because it has a frame\n");
        return TRUE;
      }
    }

  /* Never put a shadow around shaped windows */
  if (cw->shaped) {
    meta_verbose ("Window has no shadow as it is shaped\n");
    return FALSE;
  }

  /* Don't put shadow around DND icon windows */
  if (cw->type == META_COMP_WINDOW_DND ||
      cw->type == META_COMP_WINDOW_DESKTOP) {
    meta_verbose ("Window has no shadow as it is DND or Desktop\n");
    return FALSE;
  }

#if 0
  if (cw->mode != WINDOW_ARGB) {
    meta_verbose ("Window has shadow as it is not ARGB\n");
    return TRUE;
  }
#endif

  if (cw->type == META_COMP_WINDOW_MENU || 
      cw->type == META_COMP_WINDOW_DROP_DOWN_MENU) {
    meta_verbose ("Window has shadow as it is a menu\n");
    return TRUE;
  }

  if (cw->type == META_COMP_WINDOW_TOOLTIP) {
    meta_verbose ("Window has shadow as it is a tooltip\n");
    return TRUE;
  }

  meta_verbose ("Window has no shadow as it fell through\n");
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

  if (info->all_damage) 
    {
      XFixesUnionRegion (xdisplay, info->all_damage, 
                         info->all_damage, damage);
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
  
  if (cw->extents) 
    {
      XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = None;
    }

  if (destroy) 
    { 
      if (cw->damage != None) 
        {
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

  printf("destroying a window... %p\n", cw);

  screen = cw->screen;
  
  if (cw->extents != None) 
    {
      add_damage (screen, cw->extents);
      cw->extents = None;
    }
  
  info = meta_screen_get_compositor_data (screen);
  info->windows = g_list_remove (info->windows, (gconstpointer) cw);
  g_hash_table_remove (info->windows_by_xid, (gpointer) xwindow);

  free_win (cw, TRUE);
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

      if (cw->shadow)
        clutter_actor_raise_top (cw->shadow);
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
          if (cw->shadow)
            clutter_actor_raise(cw->shadow, above_win->actor);
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
  XserverRegion damage;

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
  
  if (cw->shadow)
    clutter_actor_set_position (cw->shadow, 
                                x + SHADOW_OFFSET_X , 
                                y + SHADOW_OFFSET_Y);

  /* Note, let named named pixmap resync actually resize actor */

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

  if (cw->shadow)
    clutter_actor_show (cw->shadow);

  clutter_actor_show (cw->actor);
}


static void
unmap_win (MetaDisplay *display,
           MetaScreen  *screen,
           Window       id)
{
  MetaCompWindow *cw = find_window_for_screen (screen, id);
  MetaCompScreen *info = meta_screen_get_compositor_data (screen);

  if (cw == NULL) 
    {
      return;
    }

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

  clutter_actor_hide (cw->actor);

  if (cw->shadow)
    clutter_actor_hide (cw->shadow);
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

  cw->extents = None;

  /* Only add the window to the list of docks if it needs a shadow */
  if (cw->type == META_COMP_WINDOW_DOCK) 
    {
      meta_verbose ("Appending %p to dock windows\n", cw);
      info->dock_windows = g_slist_append (info->dock_windows, cw);
    }

#if 0
  /* Add this to the list at the top of the stack
     before it is mapped so that map_win can find it again */
  printf("added %li type:", xwindow);

  switch (cw->type)
    {
    case META_COMP_WINDOW_NORMAL:
      printf("normal"); break;
    case META_COMP_WINDOW_DND:
      printf("dnd"); break;
    case META_COMP_WINDOW_DESKTOP:
      printf("desktop"); break;
    case META_COMP_WINDOW_DOCK:
      printf("dock"); break;
    case META_COMP_WINDOW_MENU:
      printf("menu"); break;
    case META_COMP_WINDOW_DROP_DOWN_MENU:
      printf("menu"); break;
    case META_COMP_WINDOW_TOOLTIP:
      printf("tooltip"); break;
    default:
      printf("unknown");
      break;
    }

  if (window && meta_window_get_frame (window))
    printf(" *HAS FRAME* ");

  printf("\n");
#endif

  info->windows = g_list_prepend (info->windows, cw);
  g_hash_table_insert (info->windows_by_xid, (gpointer) xwindow, cw);


  cw->actor = clutter_glx_texture_pixmap_new ();

  if (window_has_shadow(cw))
    {
      cw->shadow = tidy_texture_frame_new (CLUTTER_TEXTURE (info->shadow_src),
                                           MAX_TILE_SZ,
                                           MAX_TILE_SZ,
                                           MAX_TILE_SZ,
                                           MAX_TILE_SZ);

      clutter_container_add_actor (CLUTTER_CONTAINER (info->stage), 
                                   cw->shadow);
      clutter_actor_set_position (cw->shadow, 
                                  cw->attrs.x + SHADOW_OFFSET_X , 
                                  cw->attrs.y + SHADOW_OFFSET_Y);
      clutter_actor_hide (cw->shadow);
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (info->stage), cw->actor);
  clutter_actor_set_position (cw->actor, cw->attrs.x, cw->attrs.y);
  clutter_actor_hide (cw->actor);

  if (cw->attrs.map_state == IsViewable)
    map_win (display, screen, xwindow);
}

static void
damage_screen (MetaScreen *screen)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion region;
  int width, height;
  XRectangle r;

  r.x = 0;
  r.y = 0;
  meta_screen_get_size (screen, &width, &height);
  r.width = width;
  r.height = height;

  region = XFixesCreateRegion (xdisplay, &r, 1);

  add_damage (screen, region);
}

static void
repair_win (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
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
          printf ("Unable to get named pixmap for %p\n", cw);
          return;
        }

      clutter_x11_texture_pixmap_set_pixmap 
                       (CLUTTER_X11_TEXTURE_PIXMAP (cw->actor),
                        cw->back_pixmap);

      g_object_get (cw->actor,
                    "pixmap-width", &pxm_width,
                    "pixmap-height", &pxm_height,
                    NULL);
      
      clutter_actor_set_size (cw->actor, pxm_width, pxm_height);

      if (cw->shadow)
        clutter_actor_set_size (cw->shadow, 
                                pxm_width + (2 * SHADOW_RADIUS), 
                                pxm_height  + (2 * SHADOW_RADIUS));

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

      if (1) /* clutter_glx_texture_pixmap_using_extension 
                (CLUTTER_GLX_TEXTURE_PIXMAP (cw->actor))) */
        {
          clutter_x11_texture_pixmap_update_area 
            (CLUTTER_X11_TEXTURE_PIXMAP (cw->actor), 
             0, 
             0,
             clutter_actor_get_width (cw->actor),
             clutter_actor_get_height (cw->actor));

        }
      else
        {
          r_damage = XFixesFetchRegionAndBounds (xdisplay, 
                                                 parts,
                                                 &r_count,
                                                 &r_bounds);

          if (r_damage)
            {
              for (i = 0; i < r_count; ++i)
                {
                  clutter_x11_texture_pixmap_update_area 
                    (CLUTTER_X11_TEXTURE_PIXMAP (cw->actor),
                     r_damage[i].x,
                     r_damage[i].y,
                     r_damage[i].width,
                     r_damage[i].height);
                }
            }
          
          XFree (r_damage);
        }
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

  /* This is quite silly as we end up creating windows as then immediatly
   * destroying them as they (likely) become framed and thus reparented. 
  */
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
    {
        printf("reparent: adding a new window %li\n", event->window);
        add_win (screen, window, event->window);
    }
  else
    {
      printf("reparent: destroying a window %li\n", event->window);
      destroy_win (compositor->display, event->window, FALSE); 
    }
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

      damage_screen (screen);
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
    {
      printf("processing unmap  of %p\n", cw);
      unmap_win (compositor->display, cw->screen, event->window);
    }
}

static void
process_map (MetaCompositorClutter *compositor,
             XMapEvent             *event,
             MetaWindow            *window)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display, 
                                               event->window);

  if (cw)
    map_win (compositor->display, cw->screen, event->window);
}

static void
process_property_notify (MetaCompositorClutter *compositor,
                         XPropertyEvent        *event)
{
  MetaDisplay *display = compositor->display;
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaScreen *screen;
  int p;
  Atom background_atoms[2];

  /* Check for the background property changing */
  background_atoms[0] = compositor->atom_x_root_pixmap;
  background_atoms[1] = compositor->atom_x_set_root;

  for (p = 0; p < 2; p++) 
    {
      if (event->atom == background_atoms[p])
        {
          screen = meta_display_screen_for_root (display, event->window);

          if (screen)
            {
              damage_screen (screen);
              return;
            }
        }
    }

  /* Check for the opacity changing */
  if (event->atom == compositor->atom_net_wm_window_opacity) 
    {
      MetaCompWindow *cw = find_window_in_display (display, event->window);
      gulong value;

      if (!cw) 
        {
          /* Applications can set this for their toplevel windows, so
           * this must be propagated to the window managed by the compositor
           */
          cw = find_window_for_child_window_in_display (display, 
                                                        event->window);
        }
      
      if (!cw)
        return;

      if (meta_prop_get_cardinal (display, event->window,
                                  compositor->atom_net_wm_window_opacity,
                                  &value) == FALSE)
        value = 0xffffffff;

      clutter_actor_set_opacity (cw->actor, 200); /* FIXME */

      if (cw->extents)
        XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = win_extents (cw);

      cw->damaged = TRUE;
      return;
    }

  if (event->atom == compositor->atom_net_wm_window_type) 
    {
      MetaCompWindow *cw = find_window_in_display (display, event->window);

      if (!cw)
        return;

      get_window_type (display, cw);
      //cw->needs_shadow = window_has_shadow (cw);
      return;
    }
}

static void
show_overlay_window (MetaScreen *screen,
                     Window      cow)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion region;

  region = XFixesCreateRegion (xdisplay, NULL, 0);
      
  XFixesSetWindowShapeRegion (xdisplay, cow, ShapeBounding, 0, 0, 0);
  XFixesSetWindowShapeRegion (xdisplay, cow, ShapeInput, 0, 0, region);
      
  XFixesDestroyRegion (xdisplay, region);
      
  damage_screen (screen);
}

static Window
get_output_window (MetaScreen *screen)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  Window output, xroot;

  xroot = meta_screen_get_xroot (screen);

  output = XCompositeGetOverlayWindow (xdisplay, xroot);
  XSelectInput (xdisplay, output, ExposureMask);

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
  guchar *data;

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

  XClearArea (xdisplay, info->output, 0, 0, 0, 0, TRUE);

  meta_screen_set_cm_selection (screen);

  info->stage = clutter_stage_get_default ();

  meta_screen_get_size (screen, &width, &height);
  clutter_actor_set_size (info->stage, width, height);

  xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XReparentWindow (xdisplay, xwin, info->output, 0, 0);

  /* Shadow setup */
  
  data = shadow_gaussian_make_tile ();

  info->shadow_src = clutter_texture_new ();

  clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (info->shadow_src),
                                     data,
                                     TRUE,
                                     TILE_WIDTH,
                                     TILE_HEIGHT,
                                     TILE_WIDTH*4,
                                     4,
                                     0,
                                     NULL);
  free (data);

  clutter_actor_show_all (info->stage);

  /* Now we're up and running we can show the output if needed */
  show_overlay_window (screen, info->output);

  info->destroy_effect 
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (200),
                                    CLUTTER_ALPHA_SINE_INC);
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
      process_property_notify (xrc, (XPropertyEvent *) event);
      break;

    case Expose:
      process_expose (xrc, (XExposeEvent *) event);
      break;

    case UnmapNotify:
      process_unmap (xrc, (XUnmapEvent *) event);
      break;

    case MapNotify:
      process_map (xrc, (XMapEvent *) event, window);
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


      /* else if (event->type == meta_display_get_shape_event_base (xrc->display) + ShapeNotify) 
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

static void  
on_destroy_effect_complete (ClutterActor *actor,
                            gpointer user_data)
{
  MetaCompWindow *cw = (MetaCompWindow *)user_data;

  free_win (cw, TRUE);
}

static void 
clutter_cmp_destroy_window (MetaCompositor *compositor,
                            MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompWindow *cw = NULL;
  MetaCompScreen *info;
  MetaScreen *screen;
  MetaFrame *f = meta_window_get_frame (window);

  screen = meta_window_get_screen (window);
  info = meta_screen_get_compositor_data (screen);

  /* Chances are we actually get the window frame here */
  cw = find_window_for_screen (screen,
                               f ? meta_frame_get_xwindow (f) :
                               meta_window_get_xwindow (window));
  if (!cw)
      return;

  printf("animating destroy of %p\n", cw);

  /* We remove the window from internal lookup hashes and thus any other
   * unmap events etc fail
  */
  info->windows = g_list_remove (info->windows, (gconstpointer) cw);
  g_hash_table_remove (info->windows_by_xid, 
                       (gpointer) (f ? meta_frame_get_xwindow (f) :
                                   meta_window_get_xwindow (window)));

  /* Crappy TV power down effect */
  clutter_actor_move_anchor_point_from_gravity (cw->actor, 
                                                CLUTTER_GRAVITY_CENTER);

  clutter_effect_fade (info->destroy_effect,
		       cw->actor,
		       0x99,
		       on_destroy_effect_complete,
		       (gpointer)cw);

  clutter_effect_scale (info->destroy_effect   ,
                        cw->actor,
                        10.0,
                        0.0,
                        NULL,
                        NULL);
  if (cw->shadow)
    {
      clutter_actor_move_anchor_point_from_gravity (cw->shadow, 
                                                    CLUTTER_GRAVITY_CENTER);

      clutter_effect_fade (info->destroy_effect,
                           cw->shadow,
                           0,
                           NULL,
                           NULL);

      clutter_effect_scale (info->destroy_effect,
                            cw->shadow,
                            5.0,
                            0.0,
                            NULL,
                            NULL);
    }

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
  clutter_cmp_set_active_window,
  clutter_cmp_destroy_window
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

  if (!composite_at_least_version (display, 0, 3))
    return NULL;

  clc = g_new (MetaCompositorClutter, 1);
  clc->compositor = comp_info;

  clutter_x11_set_display (xdisplay);
  clutter_x11_disable_event_retrieval ();
  clutter_init (NULL, NULL);

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

  return compositor;
#else
  return NULL;
#endif
}

/* ------------------------------- */
/* Shadow Generation */

typedef struct GaussianMap
{
  int	   size;
  double * data;
} GaussianMap;

static double
gaussian (double r, double x, double y)
{
  return ((1 / (sqrt (2 * M_PI * r))) *
	  exp ((- (x * x + y * y)) / (2 * r * r)));
}


static GaussianMap *
make_gaussian_map (double r)
{
  GaussianMap  *c;
  int	          size = ((int) ceil ((r * 3)) + 1) & ~1;
  int	          center = size / 2;
  int	          x, y;
  double          t = 0.0;
  double          g;

  c = malloc (sizeof (GaussianMap) + size * size * sizeof (double));
  c->size = size;

  c->data = (double *) (c + 1);

  for (y = 0; y < size; y++)
    for (x = 0; x < size; x++)
      {
	g = gaussian (r, (double) (x - center), (double) (y - center));
	t += g;
	c->data[y * size + x] = g;
      }

  for (y = 0; y < size; y++)
    for (x = 0; x < size; x++)
      c->data[y*size + x] /= t;

  return c;
}

static unsigned char
sum_gaussian (GaussianMap * map, double opacity,
              int x, int y, int width, int height)
{
  int	           fx, fy;
  double         * g_data;
  double         * g_line = map->data;
  int	           g_size = map->size;
  int	           center = g_size / 2;
  int	           fx_start, fx_end;
  int	           fy_start, fy_end;
  double           v;
  unsigned int     r;

  /*
   * Compute set of filter values which are "in range",
   * that's the set with:
   *	0 <= x + (fx-center) && x + (fx-center) < width &&
   *  0 <= y + (fy-center) && y + (fy-center) < height
   *
   *  0 <= x + (fx - center)	x + fx - center < width
   *  center - x <= fx	fx < width + center - x
   */

  fx_start = center - x;
  if (fx_start < 0)
    fx_start = 0;
  fx_end = width + center - x;
  if (fx_end > g_size)
    fx_end = g_size;

  fy_start = center - y;
  if (fy_start < 0)
    fy_start = 0;
  fy_end = height + center - y;
  if (fy_end > g_size)
    fy_end = g_size;

  g_line = g_line + fy_start * g_size + fx_start;

  v = 0;
  for (fy = fy_start; fy < fy_end; fy++)
    {
      g_data = g_line;
      g_line += g_size;

      for (fx = fx_start; fx < fx_end; fx++)
	v += *g_data++;
    }
  if (v > 1)
    v = 1;

  v *= (opacity * 255.0);

  r = (unsigned int) v;

  return (unsigned char) r;
}

static unsigned char *
shadow_gaussian_make_tile ()
{
  unsigned char              * data;
  int		               size;
  int		               center;
  int		               x, y;
  unsigned char                d;
  int                          pwidth, pheight;
  double                       opacity = SHADOW_OPACITY;
  static GaussianMap       * gaussian_map = NULL;

  struct _mypixel
  {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
  } * _d;


  if (!gaussian_map)
    gaussian_map =
      make_gaussian_map (SHADOW_RADIUS);

  size   = gaussian_map->size;
  center = size / 2;

  /* Top & bottom */

  pwidth  = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  data = g_malloc0 (4 * TILE_WIDTH * TILE_HEIGHT);

  _d = (struct _mypixel*) data;

  /* N */
  for (y = 0; y < pheight; y++)
    {
      d = sum_gaussian (gaussian_map, opacity,
                        center, y - center,
                        TILE_WIDTH, TILE_HEIGHT);
      for (x = 0; x < pwidth; x++)
	{
	  _d[y*3*pwidth + x + pwidth].r = 0;
	  _d[y*3*pwidth + x + pwidth].g = 0;
	  _d[y*3*pwidth + x + pwidth].b = 0;
	  _d[y*3*pwidth + x + pwidth].a = d;
	}

    }

  /* S */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  for (y = 0; y < pheight; y++)
    {
      d = sum_gaussian (gaussian_map, opacity,
                        center, y - center,
                        TILE_WIDTH, TILE_HEIGHT);
      for (x = 0; x < pwidth; x++)
	{
	  _d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x + pwidth].r = 0;
	  _d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x + pwidth].g = 0;
	  _d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x + pwidth].b = 0;
	  _d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x + pwidth].a = d;
	}

    }


  /* w */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  for (x = 0; x < pwidth; x++)
    {
      d = sum_gaussian (gaussian_map, opacity,
                        x - center, center,
                        TILE_WIDTH, TILE_HEIGHT);
      for (y = 0; y < pheight; y++)
	{
	  _d[y*3*pwidth + 3*pwidth*pheight + x].r = 0;
	  _d[y*3*pwidth + 3*pwidth*pheight + x].g = 0;
	  _d[y*3*pwidth + 3*pwidth*pheight + x].b = 0;
	  _d[y*3*pwidth + 3*pwidth*pheight + x].a = d;
	}

    }

  /* E */
  for (x = 0; x < pwidth; x++)
    {
      d = sum_gaussian (gaussian_map, opacity,
					       x - center, center,
					       TILE_WIDTH, TILE_HEIGHT);
      for (y = 0; y < pheight; y++)
	{
	  _d[y*3*pwidth + 3*pwidth*pheight + (pwidth-x-1) + 2*pwidth].r = 0;
	  _d[y*3*pwidth + 3*pwidth*pheight + (pwidth-x-1) + 2*pwidth].g = 0;
	  _d[y*3*pwidth + 3*pwidth*pheight + (pwidth-x-1) + 2*pwidth].b = 0;
	  _d[y*3*pwidth + 3*pwidth*pheight + (pwidth-x-1) + 2*pwidth].a = d;
	}

    }

  /* NW */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gaussian_map, opacity,
                          x-center, y-center,
                          TILE_WIDTH, TILE_HEIGHT);

	_d[y*3*pwidth + x].r = 0;
	_d[y*3*pwidth + x].g = 0;
	_d[y*3*pwidth + x].b = 0;
	_d[y*3*pwidth + x].a = d;
      }

  /* SW */
  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gaussian_map, opacity,
                          x-center, y-center,
                          TILE_WIDTH, TILE_HEIGHT);

	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x].r = 0;
	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x].g = 0;
	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x].b = 0;
	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + x].a = d;
      }

  /* SE */
  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gaussian_map, opacity,
                          x-center, y-center,
                          TILE_WIDTH, TILE_HEIGHT);

	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + (pwidth-x-1) +
	   2*pwidth].r = 0;
	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + (pwidth-x-1) +
	   2*pwidth].g = 0;
	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + (pwidth-x-1) +
	   2*pwidth].b = 0;
	_d[(pheight-y-1)*3*pwidth + 6*pwidth*pheight + (pwidth-x-1) +
	   2*pwidth].a = d;
      }

  /* NE */
  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = sum_gaussian (gaussian_map, opacity,
                          x-center, y-center, 
                          TILE_WIDTH, TILE_HEIGHT);

	_d[y*3*pwidth + (pwidth - x - 1) + 2*pwidth].r = 0;
	_d[y*3*pwidth + (pwidth - x - 1) + 2*pwidth].g = 0;
	_d[y*3*pwidth + (pwidth - x - 1) + 2*pwidth].b = 0;
	_d[y*3*pwidth + (pwidth - x - 1) + 2*pwidth].a = d;
      }

  /* center */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  d = sum_gaussian (gaussian_map, opacity,
                    center, center, TILE_WIDTH, TILE_HEIGHT);

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	_d[y*3*pwidth + 3*pwidth*pheight + x + pwidth].r = 0;
	_d[y*3*pwidth + 3*pwidth*pheight + x + pwidth].g = 0;
	_d[y*3*pwidth + 3*pwidth*pheight + x + pwidth].b = 0;
	_d[y*3*pwidth + 3*pwidth*pheight + x + pwidth].a = d;
      }

  return data;
}

#define TIDY_TYPE_TEXTURE_FRAME (tidy_texture_frame_get_type ())

#define TIDY_TEXTURE_FRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrame))

#define TIDY_TEXTURE_FRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrameClass))

#define TIDY_IS_TEXTURE_FRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TIDY_TYPE_TEXTURE_FRAME))

#define TIDY_IS_TEXTURE_FRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TIDY_TYPE_TEXTURE_FRAME))

#define TIDY_TEXTURE_FRAME_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrameClass))

typedef struct _TidyTextureFrame        TidyTextureFrame;
typedef struct _TidyTextureFramePrivate TidyTextureFramePrivate;
typedef struct _TidyTextureFrameClass   TidyTextureFrameClass;

struct _TidyTextureFrame
{
  ClutterCloneTexture              parent;
  
  /*< priv >*/
  TidyTextureFramePrivate    *priv;
};

struct _TidyTextureFrameClass 
{
  ClutterCloneTextureClass parent_class;

  /* padding for future expansion */
  void (*_clutter_box_1) (void);
  void (*_clutter_box_2) (void);
  void (*_clutter_box_3) (void);
  void (*_clutter_box_4) (void);
}; 

GType         tidy_texture_frame_get_type (void) G_GNUC_CONST;

#define TIDY_PARAM_READABLE     \
        (G_PARAM_READABLE |     \
         G_PARAM_STATIC_NICK | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB)

#define TIDY_PARAM_READWRITE    \
        (G_PARAM_READABLE | G_PARAM_WRITABLE | \
         G_PARAM_STATIC_NICK | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB)


enum
{
  PROP_0,
  PROP_LEFT,
  PROP_TOP,
  PROP_RIGHT,
  PROP_BOTTOM
};

G_DEFINE_TYPE (TidyTextureFrame,
	       tidy_texture_frame,
	       CLUTTER_TYPE_CLONE_TEXTURE);

#define TIDY_TEXTURE_FRAME_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFramePrivate))

struct _TidyTextureFramePrivate
{
  gint left, top, right, bottom;
};

static void
tidy_texture_frame_paint (ClutterActor *self)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;
  ClutterCloneTexture     *clone_texture = CLUTTER_CLONE_TEXTURE (self);
  ClutterTexture          *parent_texture;
  guint                    width, height;
  guint                    tex_width, tex_height;
  guint                    ex, ey;
  ClutterFixed             tx1, ty1, tx2, ty2;
  ClutterColor             col = { 0xff, 0xff, 0xff, 0xff };
  CoglHandle               cogl_texture;

  priv = TIDY_TEXTURE_FRAME (self)->priv;

  /* no need to paint stuff if we don't have a texture */
  parent_texture = clutter_clone_texture_get_parent_texture (clone_texture);
  if (!parent_texture)
    return;

  /* parent texture may have been hidden, so need to make sure it gets
   * realized
   */
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (CLUTTER_ACTOR (parent_texture));

  cogl_texture = clutter_texture_get_cogl_texture (parent_texture);
  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  cogl_push_matrix ();

  tex_width  = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);

  clutter_actor_get_size (self, &width, &height); 

  tx1 = CLUTTER_INT_TO_FIXED (priv->left) / tex_width;
  tx2 = CLUTTER_INT_TO_FIXED (tex_width - priv->right) / tex_width;
  ty1 = CLUTTER_INT_TO_FIXED (priv->top) / tex_height;
  ty2 = CLUTTER_INT_TO_FIXED (tex_height - priv->bottom) / tex_height;

  col.alpha = clutter_actor_get_abs_opacity (self);
  cogl_color (&col);

  ex = width - priv->right;
  if (ex < 0) 
    ex = priv->right; 		/* FIXME ? */

  ey = height - priv->bottom;
  if (ey < 0) 
    ey = priv->bottom; 		/* FIXME ? */

#define FX(x) CLUTTER_INT_TO_FIXED(x)

  /* top left corner */
  cogl_texture_rectangle (cogl_texture, 
                          0,
                          0,
                          FX(priv->left), /* FIXME: clip if smaller */
                          FX(priv->top),
                          0,
                          0,
                          tx1,
                          ty1);

  /* top middle */
  cogl_texture_rectangle (cogl_texture,
                          FX(priv->left),
                          FX(priv->top),
                          FX(ex),
                          0,
                          tx1,
                          0,
                          tx2,
                          ty1);

  /* top right */
  cogl_texture_rectangle (cogl_texture,
                          FX(ex),
                          0,
                          FX(width),
                          FX(priv->top),
                          tx2,
                          0,
                          CFX_ONE,
                          ty1);

  /* mid left */
  cogl_texture_rectangle (cogl_texture,
                          0, 
                          FX(priv->top),
                          FX(priv->left),
                          FX(ey),
                          0,
                          ty1,
                          tx1,
                          ty2);

  /* center */
  cogl_texture_rectangle (cogl_texture,
                          FX(priv->left),
                          FX(priv->top),
                          FX(ex),
                          FX(ey),
                          tx1,
                          ty1,
                          tx2,
                          ty2);

  /* mid right */
  cogl_texture_rectangle (cogl_texture,
                          FX(ex),
                          FX(priv->top),
                          FX(width),
                          FX(ey),
                          tx2,
                          ty1,
                          CFX_ONE,
                          ty2);
  
  /* bottom left */
  cogl_texture_rectangle (cogl_texture,
                          0, 
                          FX(ey),
                          FX(priv->left),
                          FX(height),
                          0,
                          ty2,
                          tx1,
                          CFX_ONE);

  /* bottom center */
  cogl_texture_rectangle (cogl_texture,
                          FX(priv->left),
                          FX(ey),
                          FX(ex),
                          FX(height),
                          tx1,
                          ty2,
                          tx2,
                          CFX_ONE);

  /* bottom right */
  cogl_texture_rectangle (cogl_texture,
                          FX(ex),
                          FX(ey),
                          FX(width),
                          FX(height),
                          tx2,
                          ty2,
                          CFX_ONE,
                          CFX_ONE);


  cogl_pop_matrix ();
}


static void
tidy_texture_frame_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  TidyTextureFrame         *ctexture = TIDY_TEXTURE_FRAME (object);
  TidyTextureFramePrivate  *priv = ctexture->priv;  

  switch (prop_id)
    {
    case PROP_LEFT:
      priv->left = g_value_get_int (value);
      break;
    case PROP_TOP:
      priv->top = g_value_get_int (value);
      break;
    case PROP_RIGHT:
      priv->right = g_value_get_int (value);
      break;
    case PROP_BOTTOM:
      priv->bottom = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  TidyTextureFrame *ctexture = TIDY_TEXTURE_FRAME (object);
  TidyTextureFramePrivate  *priv = ctexture->priv;  

  switch (prop_id)
    {
    case PROP_LEFT:
      g_value_set_int (value, priv->left);
      break;
    case PROP_TOP:
      g_value_set_int (value, priv->top);
      break;
    case PROP_RIGHT:
      g_value_set_int (value, priv->right);
      break;
    case PROP_BOTTOM:
      g_value_set_int (value, priv->bottom);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_class_init (TidyTextureFrameClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = tidy_texture_frame_paint;

  gobject_class->set_property = tidy_texture_frame_set_property;
  gobject_class->get_property = tidy_texture_frame_get_property;

  g_object_class_install_property 
            (gobject_class,
	     PROP_LEFT,
	     g_param_spec_int ("left",
			       "left",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));

  g_object_class_install_property 
            (gobject_class,
	     PROP_TOP,
	     g_param_spec_int ("top",
			       "top",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));

  g_object_class_install_property 
            (gobject_class,
	     PROP_BOTTOM,
	     g_param_spec_int ("bottom",
			       "bottom",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));
  
  g_object_class_install_property 
            (gobject_class,
	     PROP_RIGHT,
	     g_param_spec_int ("right",
			       "right",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));
  
  g_type_class_add_private (gobject_class, sizeof (TidyTextureFramePrivate));
}

static void
tidy_texture_frame_init (TidyTextureFrame *self)
{
  TidyTextureFramePrivate *priv;

  self->priv = priv = TIDY_TEXTURE_FRAME_GET_PRIVATE (self);
}

ClutterActor*
tidy_texture_frame_new (ClutterTexture *texture, 
			gint            left,
			gint            top,
			gint            right,
			gint            bottom)
{
  g_return_val_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture), NULL);

  return g_object_new (TIDY_TYPE_TEXTURE_FRAME,
 		       "parent-texture", texture,
		       "left", left,
		       "top", top,
		       "right", right,
		       "bottom", bottom,
		       NULL);
}
