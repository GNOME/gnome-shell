/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 *  Johan Bilien   <johan.bilien@nokia.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-texture-pixmap-x11.h"
#include "cogl-texture-pixmap-x11-private.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
#if COGL_HAS_GLX_SUPPORT
#include "cogl-display-glx-private.h"
#include "cogl-renderer-private.h"
#include "cogl-renderer-glx-private.h"
#endif
#include "cogl-pipeline-opengl-private.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <string.h>
#include <math.h>

static void _cogl_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap);

COGL_TEXTURE_DEFINE (TexturePixmapX11, texture_pixmap_x11);

static const CoglTextureVtable cogl_texture_pixmap_x11_vtable;

static void
cogl_damage_rectangle_union (CoglDamageRectangle *damage_rect,
                             int x,
                             int y,
                             int width,
                             int height)
{
  /* If the damage region is empty then we'll just copy the new
     rectangle directly */
  if (damage_rect->x1 == damage_rect->x2 ||
      damage_rect->y1 == damage_rect->y2)
    {
      damage_rect->x1 = x;
      damage_rect->y1 = y;
      damage_rect->x2 = x + width;
      damage_rect->y2 = y + height;
    }
  else
    {
      if (damage_rect->x1 > x)
        damage_rect->x1 = x;
      if (damage_rect->y1 > y)
        damage_rect->y1 = y;
      if (damage_rect->x2 < x + width)
        damage_rect->x2 = x + width;
      if (damage_rect->y2 < y + height)
        damage_rect->y2 = y + height;
    }
}

static gboolean
cogl_damage_rectangle_is_whole (const CoglDamageRectangle *damage_rect,
                                unsigned int width,
                                unsigned int height)
{
  return (damage_rect->x1 == 0 && damage_rect->y1 == 0
          && damage_rect->x2 == width && damage_rect->y2 == height);
}

static void
process_damage_event (CoglTexturePixmapX11 *tex_pixmap,
                      XDamageNotifyEvent *damage_event)
{
  Display *display;
  enum { DO_NOTHING, NEEDS_SUBTRACT, NEED_BOUNDING_BOX } handle_mode;

  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  display = cogl_xlib_get_display ();

  COGL_NOTE (TEXTURE_PIXMAP, "Damage event received for %p", tex_pixmap);

  switch (tex_pixmap->damage_report_level)
    {
    case COGL_TEXTURE_PIXMAP_X11_DAMAGE_RAW_RECTANGLES:
      /* For raw rectangles we don't need do look at the damage region
         at all because the damage area is directly given in the event
         struct and the reporting of events is not affected by
         clearing the damage region */
      handle_mode = DO_NOTHING;
      break;

    case COGL_TEXTURE_PIXMAP_X11_DAMAGE_DELTA_RECTANGLES:
    case COGL_TEXTURE_PIXMAP_X11_DAMAGE_NON_EMPTY:
      /* For delta rectangles and non empty we'll query the damage
         region for the bounding box */
      handle_mode = NEED_BOUNDING_BOX;
      break;

    case COGL_TEXTURE_PIXMAP_X11_DAMAGE_BOUNDING_BOX:
      /* For bounding box we need to clear the damage region but we
         don't actually care what it was because the damage event
         itself contains the bounding box of the region */
      handle_mode = NEEDS_SUBTRACT;
      break;

    default:
      g_assert_not_reached ();
    }

  /* If the damage already covers the whole rectangle then we don't
     need to request the bounding box of the region because we're
     going to update the whole texture anyway. */
  if (cogl_damage_rectangle_is_whole (&tex_pixmap->damage_rect,
                                      tex_pixmap->width,
                                      tex_pixmap->height))
    {
      if (handle_mode != DO_NOTHING)
        XDamageSubtract (display, tex_pixmap->damage, None, None);
    }
  else if (handle_mode == NEED_BOUNDING_BOX)
    {
      XserverRegion parts;
      int r_count;
      XRectangle r_bounds;
      XRectangle *r_damage;

      /* We need to extract the damage region so we can get the
         bounding box */

      parts = XFixesCreateRegion (display, 0, 0);
      XDamageSubtract (display, tex_pixmap->damage, None, parts);
      r_damage = XFixesFetchRegionAndBounds (display,
                                             parts,
                                             &r_count,
                                             &r_bounds);
      cogl_damage_rectangle_union (&tex_pixmap->damage_rect,
                                   r_bounds.x,
                                   r_bounds.y,
                                   r_bounds.width,
                                   r_bounds.height);
      if (r_damage)
        XFree (r_damage);

      XFixesDestroyRegion (display, parts);
    }
  else
    {
      if (handle_mode == NEEDS_SUBTRACT)
        /* We still need to subtract from the damage region but we
           don't care what the region actually was */
        XDamageSubtract (display, tex_pixmap->damage, None, None);

      cogl_damage_rectangle_union (&tex_pixmap->damage_rect,
                                   damage_event->area.x,
                                   damage_event->area.y,
                                   damage_event->area.width,
                                   damage_event->area.height);
    }

  /* If we're using the texture from pixmap extension then there's no
     point in getting the region and we can just mark that the texture
     needs updating */
#ifdef COGL_HAS_GLX_SUPPORT
  tex_pixmap->bind_tex_image_queued = TRUE;
#endif
}

static CoglXlibFilterReturn
_cogl_texture_pixmap_x11_filter (XEvent *event, gpointer data)
{
  CoglTexturePixmapX11 *tex_pixmap = data;
  int damage_base;

  _COGL_GET_CONTEXT (ctxt, COGL_XLIB_FILTER_CONTINUE);

  damage_base = _cogl_xlib_get_damage_base ();
  if (event->type == damage_base + XDamageNotify)
    {
      XDamageNotifyEvent *damage_event = (XDamageNotifyEvent *) event;

      if (damage_event->damage == tex_pixmap->damage)
        process_damage_event (tex_pixmap, damage_event);
    }

  return COGL_XLIB_FILTER_CONTINUE;
}

#ifdef COGL_HAS_GLX_SUPPORT

static gboolean
get_fbconfig_for_depth (unsigned int depth,
                        GLXFBConfig *fbconfig_ret,
                        gboolean *can_mipmap_ret)
{
  GLXFBConfig *fbconfigs;
  int n_elements, i;
  Display *dpy;
  int db, stencil, alpha, mipmap, rgba, value;
  int spare_cache_slot = 0;
  gboolean found = FALSE;
  CoglDisplayGLX *glx_display;

  _COGL_GET_CONTEXT (ctxt, FALSE);
  glx_display = ctxt->display->winsys;

  /* Check if we've already got a cached config for this depth */
  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    if (glx_display->glx_cached_configs[i].depth == -1)
      spare_cache_slot = i;
    else if (glx_display->glx_cached_configs[i].depth == depth)
      {
        *fbconfig_ret = glx_display->glx_cached_configs[i].fb_config;
        *can_mipmap_ret = glx_display->glx_cached_configs[i].can_mipmap;
        return glx_display->glx_cached_configs[i].found;
      }

  dpy = cogl_xlib_get_display ();

  fbconfigs = glXGetFBConfigs (dpy, DefaultScreen (dpy),
                               &n_elements);

  db      = G_MAXSHORT;
  stencil = G_MAXSHORT;
  mipmap  = 0;
  rgba    = 0;

  for (i = 0; i < n_elements; i++)
    {
      XVisualInfo *vi;
      int          visual_depth;

      vi = glXGetVisualFromFBConfig (dpy,
                                     fbconfigs[i]);
      if (vi == NULL)
        continue;

      visual_depth = vi->depth;

      XFree (vi);

      if (visual_depth != depth)
        continue;

      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_ALPHA_SIZE,
                            &alpha);
      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_BUFFER_SIZE,
                            &value);
      if (value != depth && (value - alpha) != depth)
        continue;

      value = 0;
      if (depth == 32)
        {
          glXGetFBConfigAttrib (dpy,
                                fbconfigs[i],
                                GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                &value);
          if (value)
            rgba = 1;
        }

      if (!value)
        {
          if (rgba)
            continue;

          glXGetFBConfigAttrib (dpy,
                                fbconfigs[i],
                                GLX_BIND_TO_TEXTURE_RGB_EXT,
                                &value);
          if (!value)
            continue;
        }

      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_DOUBLEBUFFER,
                            &value);
      if (value > db)
        continue;

      db = value;

      glXGetFBConfigAttrib (dpy,
                            fbconfigs[i],
                            GLX_STENCIL_SIZE,
                            &value);
      if (value > stencil)
        continue;

      stencil = value;

      /* glGenerateMipmap is defined in the offscreen extension */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        {
          glXGetFBConfigAttrib (dpy,
                                fbconfigs[i],
                                GLX_BIND_TO_MIPMAP_TEXTURE_EXT,
                                &value);

          if (value < mipmap)
            continue;

          mipmap =  value;
        }

      *fbconfig_ret = fbconfigs[i];
      *can_mipmap_ret = mipmap;
      found = TRUE;
    }

  if (n_elements)
    XFree (fbconfigs);

  glx_display->glx_cached_configs[spare_cache_slot].depth = depth;
  glx_display->glx_cached_configs[spare_cache_slot].found = found;
  glx_display->glx_cached_configs[spare_cache_slot].fb_config = *fbconfig_ret;
  glx_display->glx_cached_configs[spare_cache_slot].can_mipmap = mipmap;

  return found;
}

static gboolean
should_use_rectangle (void)
{
  _COGL_GET_CONTEXT (ctxt, FALSE);

  if (ctxt->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_UNKNOWN)
    {
      if (cogl_features_available (COGL_FEATURE_TEXTURE_RECTANGLE))
        {
          const char *rect_env;

          /* Use the rectangle only if it is available and either:

             the COGL_PIXMAP_TEXTURE_RECTANGLE environment variable is
             set to 'force'

             *or*

             the env var is set to 'allow' or not set and NPOTs textures
             are not available */

          ctxt->rectangle_state =
            cogl_features_available (COGL_FEATURE_TEXTURE_NPOT) ?
            COGL_WINSYS_RECTANGLE_STATE_DISABLE :
            COGL_WINSYS_RECTANGLE_STATE_ENABLE;

          if ((rect_env = g_getenv ("COGL_PIXMAP_TEXTURE_RECTANGLE")) ||
              /* For compatibility, we'll also look at the old Clutter
                 environment variable */
              (rect_env = g_getenv ("CLUTTER_PIXMAP_TEXTURE_RECTANGLE")))
            {
              if (g_ascii_strcasecmp (rect_env, "force") == 0)
                ctxt->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_ENABLE;
              else if (g_ascii_strcasecmp (rect_env, "disable") == 0)
                ctxt->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_DISABLE;
              else if (g_ascii_strcasecmp (rect_env, "allow"))
                g_warning ("Unknown value for COGL_PIXMAP_TEXTURE_RECTANGLE, "
                           "should be 'force' or 'disable'");
            }
        }
      else
        ctxt->rectangle_state = COGL_WINSYS_RECTANGLE_STATE_DISABLE;
    }

  return ctxt->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_ENABLE;
}

static void
try_create_glx_pixmap (CoglTexturePixmapX11 *tex_pixmap,
                       gboolean mipmap)
{
  Display *dpy;
  /* We have to initialize this *opaque* variable because gcc tries to
   * be too smart for its own good and warns that the variable may be
   * used uninitialized otherwise. */
  GLXFBConfig fb_config = (GLXFBConfig)0;
  int attribs[7];
  int i = 0;
  GLenum target;
  CoglXlibTrapState trap_state;

  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  tex_pixmap->pixmap_bound = FALSE;
  tex_pixmap->glx_pixmap = None;

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP))
    return;

  dpy = cogl_xlib_get_display ();

  if (!get_fbconfig_for_depth (tex_pixmap->depth, &fb_config,
                               &tex_pixmap->glx_can_mipmap))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "No suitable FBConfig found for depth %i",
                 tex_pixmap->depth);
      return;
    }

  if (should_use_rectangle ())
    {
      target = GLX_TEXTURE_RECTANGLE_EXT;
      tex_pixmap->glx_can_mipmap = FALSE;
    }
  else
    target = GLX_TEXTURE_2D_EXT;

  if (!tex_pixmap->glx_can_mipmap)
    mipmap = FALSE;

  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

  if (tex_pixmap->depth == 24)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
  else if (tex_pixmap->depth == 32)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;
  else
    return;

  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = mipmap;

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;
  attribs[i++] = target;

  attribs[i++] = None;

  /* We need to trap errors from glXCreatePixmap because it can
     sometimes fail during normal usage. For example on NVidia it gets
     upset if you try to create two GLXPixmaps for the same
     drawable. */

  _cogl_xlib_trap_errors (&trap_state);

  tex_pixmap->glx_pixmap = glXCreatePixmap (dpy,
                                            fb_config,
                                            tex_pixmap->pixmap,
                                            attribs);
  tex_pixmap->glx_pixmap_has_mipmap = mipmap;

  XSync (dpy, False);

  if (_cogl_xlib_untrap_errors (&trap_state))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "Failed to create pixmap for %p", tex_pixmap);
      _cogl_xlib_trap_errors (&trap_state);
      glXDestroyPixmap (dpy, tex_pixmap->glx_pixmap);
      XSync (dpy, False);
      _cogl_xlib_untrap_errors (&trap_state);

      tex_pixmap->glx_pixmap = None;
    }
}

#endif /* COGL_HAS_GLX_SUPPORT */

static void
set_damage_object_internal (CoglTexturePixmapX11 *tex_pixmap,
                            Damage damage,
                            CoglTexturePixmapX11ReportLevel report_level)
{
  if (tex_pixmap->damage)
    {
      _cogl_xlib_remove_filter (_cogl_texture_pixmap_x11_filter, tex_pixmap);

      if (tex_pixmap->damage_owned)
        {
          XDamageDestroy (cogl_xlib_get_display (), tex_pixmap->damage);
          tex_pixmap->damage_owned = FALSE;
        }
    }

  tex_pixmap->damage = damage;
  tex_pixmap->damage_report_level = report_level;

  if (damage)
    _cogl_xlib_add_filter (_cogl_texture_pixmap_x11_filter, tex_pixmap);
}

CoglHandle
cogl_texture_pixmap_x11_new (guint32 pixmap,
                             gboolean automatic_updates)
{
  CoglTexturePixmapX11 *tex_pixmap = g_new (CoglTexturePixmapX11, 1);
  Display *display = cogl_xlib_get_display ();
  Window pixmap_root_window;
  int pixmap_x, pixmap_y;
  unsigned int pixmap_border_width;
  CoglTexture *tex = COGL_TEXTURE (tex_pixmap);
  XWindowAttributes window_attributes;
  int damage_base;

  _COGL_GET_CONTEXT (ctxt, COGL_INVALID_HANDLE);

  _cogl_texture_init (tex, &cogl_texture_pixmap_x11_vtable);

  tex_pixmap->pixmap = pixmap;
  tex_pixmap->image = NULL;
  tex_pixmap->shm_info.shmid = -1;
  tex_pixmap->tex = COGL_INVALID_HANDLE;
  tex_pixmap->damage_owned = FALSE;
  tex_pixmap->damage = 0;

  if (!XGetGeometry (display, pixmap, &pixmap_root_window,
                     &pixmap_x, &pixmap_y,
                     &tex_pixmap->width, &tex_pixmap->height,
                     &pixmap_border_width, &tex_pixmap->depth))
    {
      g_free (tex_pixmap);
      g_warning ("Unable to query pixmap size");
      return COGL_INVALID_HANDLE;
    }

  /* We need a visual to use for shared memory images so we'll query
     it from the pixmap's root window */
  if (!XGetWindowAttributes (display, pixmap_root_window, &window_attributes))
    {
      g_free (tex_pixmap);
      g_warning ("Unable to query root window attributes");
      return COGL_INVALID_HANDLE;
    }
  tex_pixmap->visual = window_attributes.visual;

  /* If automatic updates are requested and the Xlib connection
     supports damage events then we'll register a damage object on the
     pixmap */
  damage_base = _cogl_xlib_get_damage_base ();
  if (automatic_updates && damage_base >= 0)
    {
      Damage damage = XDamageCreate (display,
                                     pixmap,
                                     XDamageReportBoundingBox);
      set_damage_object_internal (tex_pixmap,
                                  damage,
                                  COGL_TEXTURE_PIXMAP_X11_DAMAGE_BOUNDING_BOX);
      tex_pixmap->damage_owned = TRUE;
    }

  /* Assume the entire pixmap is damaged to begin with */
  tex_pixmap->damage_rect.x1 = 0;
  tex_pixmap->damage_rect.x2 = tex_pixmap->width;
  tex_pixmap->damage_rect.y1 = 0;
  tex_pixmap->damage_rect.y2 = tex_pixmap->height;

#ifdef COGL_HAS_GLX_SUPPORT
  try_create_glx_pixmap (tex_pixmap, FALSE);

  tex_pixmap->glx_tex = COGL_INVALID_HANDLE;
  tex_pixmap->bind_tex_image_queued = TRUE;
  tex_pixmap->use_glx_texture = FALSE;
#endif

  return _cogl_texture_pixmap_x11_handle_new (tex_pixmap);
}

/* Tries to allocate enough shared mem to handle a full size
 * update size of the X Pixmap. */
static void
try_alloc_shm (CoglTexturePixmapX11 *tex_pixmap)
{
  XImage *dummy_image;
  Display *display;

  display = cogl_xlib_get_display ();

  if (!XShmQueryExtension (display))
    return;

  /* We are creating a dummy_image so we can have Xlib calculate
   * image->bytes_per_line - including any magic padding it may
   * want - for the largest possible ximage we might need to use
   * when handling updates to the texture.
   *
   * Note: we pass a NULL shminfo here, but that has no bearing
   * on the setup of the XImage, except that ximage->obdata will
   * == NULL.
   */
  dummy_image =
    XShmCreateImage (display,
                     tex_pixmap->visual,
                     tex_pixmap->depth,
                     ZPixmap,
                     NULL,
                     NULL, /* shminfo, */
                     tex_pixmap->width,
                     tex_pixmap->height);
  if (!dummy_image)
    goto failed_image_create;

  tex_pixmap->shm_info.shmid = shmget (IPC_PRIVATE,
                                       dummy_image->bytes_per_line
                                       * dummy_image->height,
                                       IPC_CREAT | 0777);
  if (tex_pixmap->shm_info.shmid == -1)
    goto failed_shmget;

  tex_pixmap->shm_info.shmaddr = shmat (tex_pixmap->shm_info.shmid, 0, 0);
  if (tex_pixmap->shm_info.shmaddr == (void *) -1)
    goto failed_shmat;

  tex_pixmap->shm_info.readOnly = False;

  if (XShmAttach (display, &tex_pixmap->shm_info) == 0)
    goto failed_xshmattach;

  XDestroyImage (dummy_image);

  return;

 failed_xshmattach:
  g_warning ("XShmAttach failed");
  shmdt (tex_pixmap->shm_info.shmaddr);

 failed_shmat:
  g_warning ("shmat failed");
  shmctl (tex_pixmap->shm_info.shmid, IPC_RMID, 0);

 failed_shmget:
  g_warning ("shmget failed");
  XDestroyImage (dummy_image);

 failed_image_create:
  tex_pixmap->shm_info.shmid = -1;
}

void
cogl_texture_pixmap_x11_update_area (CoglHandle handle,
                                     int x,
                                     int y,
                                     int width,
                                     int height)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (handle);

  if (!cogl_is_texture_pixmap_x11 (handle))
    return;

  /* We'll queue the update for both the GLX texture and the regular
     texture because we can't determine which will be needed until we
     actually render something */

#ifdef COGL_HAS_GLX_SUPPORT
  tex_pixmap->bind_tex_image_queued = TRUE;
#endif

  cogl_damage_rectangle_union (&tex_pixmap->damage_rect,
                               x, y, width, height);
}

gboolean
cogl_texture_pixmap_x11_is_using_tfp_extension (CoglHandle handle)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (handle);

  if (!cogl_is_texture_pixmap_x11 (tex_pixmap))
    return FALSE;

#ifdef COGL_HAS_GLX_SUPPORT
  return tex_pixmap->glx_pixmap != None;
#else
  return FALSE;
#endif
}

void
cogl_texture_pixmap_x11_set_damage_object (CoglHandle handle,
                                           guint32 damage,
                                           CoglTexturePixmapX11ReportLevel
                                                                  report_level)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (handle);
  int damage_base;

  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  if (!cogl_is_texture_pixmap_x11 (tex_pixmap))
    return;

  damage_base = _cogl_xlib_get_damage_base ();
  if (damage_base >= 0)
    set_damage_object_internal (tex_pixmap, damage, report_level);
}

static void
_cogl_texture_pixmap_x11_update_image_texture (CoglTexturePixmapX11 *tex_pixmap)
{
  Display *display;
  CoglPixelFormat image_format;
  XImage *image;
  int src_x, src_y;
  int x, y, width, height;

  display = cogl_xlib_get_display ();

  /* If the damage region is empty then there's nothing to do */
  if (tex_pixmap->damage_rect.x2 == tex_pixmap->damage_rect.x1)
    return;

  x = tex_pixmap->damage_rect.x1;
  y = tex_pixmap->damage_rect.y1;
  width = tex_pixmap->damage_rect.x2 - x;
  height = tex_pixmap->damage_rect.y2 - y;

  /* We lazily create the texture the first time it is needed in case
     this texture can be entirely handled using the GLX texture
     instead */
  if (tex_pixmap->tex == COGL_INVALID_HANDLE)
    {
      CoglPixelFormat texture_format;

      texture_format = (tex_pixmap->depth >= 32
                        ? COGL_PIXEL_FORMAT_RGBA_8888_PRE
                        : COGL_PIXEL_FORMAT_RGB_888);

      tex_pixmap->tex = cogl_texture_new_with_size (tex_pixmap->width,
                                                    tex_pixmap->height,
                                                    COGL_TEXTURE_NONE,
                                                    texture_format);
    }

  if (tex_pixmap->image == NULL)
    {
      /* If we also haven't got a shm segment then this must be the
         first time we've tried to update, so lets try allocating shm
         first */
      if (tex_pixmap->shm_info.shmid == -1)
        try_alloc_shm (tex_pixmap);

      if (tex_pixmap->shm_info.shmid == -1)
        {
          COGL_NOTE (TEXTURE_PIXMAP, "Updating %p using XGetImage", tex_pixmap);

          /* We'll fallback to using a regular XImage. We'll download
             the entire area instead of a sub region because presumably
             if this is the first update then the entire pixmap is
             needed anyway and it saves trying to manually allocate an
             XImage at the right size */
          tex_pixmap->image = XGetImage (display,
                                         tex_pixmap->pixmap,
                                         0, 0,
                                         tex_pixmap->width, tex_pixmap->height,
                                         AllPlanes, ZPixmap);
          image = tex_pixmap->image;
          src_x = x;
          src_y = y;
        }
      else
        {
          COGL_NOTE (TEXTURE_PIXMAP, "Updating %p using XShmGetImage",
                     tex_pixmap);

          /* Create a temporary image using the beginning of the
             shared memory segment and the right size for the region
             we want to update. We need to reallocate the XImage every
             time because there is no XShmGetSubImage. */
          image = XShmCreateImage (display,
                                   tex_pixmap->visual,
                                   tex_pixmap->depth,
                                   ZPixmap,
                                   NULL,
                                   &tex_pixmap->shm_info,
                                   width,
                                   height);
          image->data = tex_pixmap->shm_info.shmaddr;
          src_x = 0;
          src_y = 0;

          XShmGetImage (display, tex_pixmap->pixmap, image, x, y, AllPlanes);
        }
    }
  else
    {
      COGL_NOTE (TEXTURE_PIXMAP, "Updating %p using XGetSubImage", tex_pixmap);

      image = tex_pixmap->image;
      src_x = x;
      src_y = y;

      XGetSubImage (display,
                    tex_pixmap->pixmap,
                    x, y, width, height,
                    AllPlanes, ZPixmap,
                    image,
                    x, y);
    }

  /* xlib doesn't appear to fill in image->{red,green,blue}_mask so
     this just assumes that the image is stored as ARGB from most
     significant byte to to least significant. If the format is little
     endian that means the order will be BGRA in memory */

  switch (image->bits_per_pixel)
    {
    default:
    case 32:
      {
        /* If the pixmap is actually non-packed-pixel RGB format then
           the texture would have been created in RGB_888 format so Cogl
           will ignore the alpha channel and effectively pack it for
           us */
        image_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

        /* If the format is actually big endian then the alpha
           component will come first */
        if (image->byte_order == MSBFirst)
          image_format |= COGL_AFIRST_BIT;
      }
      break;

    case 24:
      image_format = COGL_PIXEL_FORMAT_RGB_888;
      break;

    case 16:
      /* FIXME: this should probably swap the orders around if the
         endianness does not match */
      image_format = COGL_PIXEL_FORMAT_RGB_565;
      break;
    }

  if (image->bits_per_pixel != 16)
    {
      /* If the image is in little-endian then the order in memory is
         reversed */
      if (image->byte_order == LSBFirst)
        image_format |= COGL_BGR_BIT;
    }

  cogl_texture_set_region (tex_pixmap->tex,
                           src_x, src_y,
                           x, y, width, height,
                           image->width,
                           image->height,
                           image_format,
                           image->bytes_per_line,
                           (const guint8 *) image->data);

  /* If we have a shared memory segment then the XImage would be a
     temporary one with no data allocated so we can just XFree it */
  if (tex_pixmap->shm_info.shmid != -1)
    XFree (image);

  memset (&tex_pixmap->damage_rect, 0, sizeof (CoglDamageRectangle));
}

#ifdef COGL_HAS_GLX_SUPPORT

static void
_cogl_texture_pixmap_x11_free_glx_pixmap (CoglTexturePixmapX11 *tex_pixmap)
{
  if (tex_pixmap->glx_pixmap)
    {
      CoglXlibTrapState trap_state;
      CoglRendererGLX *glx_renderer;

      _COGL_GET_CONTEXT (ctx, NO_RETVAL);
      glx_renderer = ctx->display->renderer->winsys;

      if (tex_pixmap->pixmap_bound)
        glx_renderer->pf_glXReleaseTexImage (cogl_xlib_get_display (),
                                             tex_pixmap->glx_pixmap,
                                             GLX_FRONT_LEFT_EXT);

      /* FIXME - we need to trap errors and synchronize here because
       * of ordering issues between the XPixmap destruction and the
       * GLXPixmap destruction.
       *
       * If the X pixmap is destroyed, the GLX pixmap is destroyed as
       * well immediately, and thus, when Cogl calls glXDestroyPixmap()
       * it'll cause a BadDrawable error.
       *
       * this is technically a bug in the X server, which should not
       * destroy either pixmaps until the call to glXDestroyPixmap(); so
       * at some point we should revisit this code and remove the
       * trap+sync after verifying that the destruction is indeed safe.
       *
       * for reference, see:
       *   http://bugzilla.clutter-project.org/show_bug.cgi?id=2324
       */
      _cogl_xlib_trap_errors (&trap_state);
      glXDestroyPixmap (cogl_xlib_get_display (), tex_pixmap->glx_pixmap);
      XSync (cogl_xlib_get_display (), False);
      _cogl_xlib_untrap_errors (&trap_state);

      tex_pixmap->glx_pixmap = None;
      tex_pixmap->pixmap_bound = FALSE;
    }
}

static gboolean
_cogl_texture_pixmap_x11_update_glx_texture (CoglTexturePixmapX11 *tex_pixmap,
                                             gboolean needs_mipmap)
{
  gboolean ret = TRUE;
  CoglRendererGLX *glx_renderer;

  _COGL_GET_CONTEXT (ctx, FALSE);
  glx_renderer = ctx->display->renderer->winsys;

  /* If we don't have a GLX pixmap then fallback */
  if (tex_pixmap->glx_pixmap == None)
    ret = FALSE;
  else
    {
      /* Lazily create a texture to hold the pixmap */
      if (tex_pixmap->glx_tex == COGL_INVALID_HANDLE)
        {
          CoglPixelFormat texture_format;

          texture_format = (tex_pixmap->depth >= 32 ?
                            COGL_PIXEL_FORMAT_RGBA_8888_PRE :
                            COGL_PIXEL_FORMAT_RGB_888);

          if (should_use_rectangle ())
            {
              tex_pixmap->glx_tex =
                _cogl_texture_rectangle_new_with_size (tex_pixmap->width,
                                                       tex_pixmap->height,
                                                       COGL_TEXTURE_NO_ATLAS,
                                                       texture_format);

              if (tex_pixmap->glx_tex)
                COGL_NOTE (TEXTURE_PIXMAP, "Created a texture rectangle for %p",
                           tex_pixmap);
              else
                {
                  COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                             "texture rectangle could not be created",
                             tex_pixmap);
                  _cogl_texture_pixmap_x11_free_glx_pixmap (tex_pixmap);
                  ret = FALSE;
                }
            }
          else
            {
              tex_pixmap->glx_tex =
                _cogl_texture_2d_new_with_size (tex_pixmap->width,
                                                tex_pixmap->height,
                                                COGL_TEXTURE_NO_ATLAS,
                                                texture_format);

              if (tex_pixmap->glx_tex)
                COGL_NOTE (TEXTURE_PIXMAP, "Created a texture 2d for %p",
                           tex_pixmap);
              else
                {
                  COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                             "texture 2d could not be created",
                             tex_pixmap);
                  _cogl_texture_pixmap_x11_free_glx_pixmap (tex_pixmap);
                  ret = FALSE;
                }
            }
        }

      if (ret && needs_mipmap)
        {
          /* If we can't support mipmapping then temporarily fallback */
          if (!tex_pixmap->glx_can_mipmap)
            ret = FALSE;
          /* Recreate the GLXPixmap if it wasn't previously created with a
             mipmap tree */
          else if (!tex_pixmap->glx_pixmap_has_mipmap)
            {
              _cogl_texture_pixmap_x11_free_glx_pixmap (tex_pixmap);

              COGL_NOTE (TEXTURE_PIXMAP, "Recreating GLXPixmap with mipmap "
                         "support for %p", tex_pixmap);
              try_create_glx_pixmap (tex_pixmap, TRUE);

              /* If the pixmap failed then we'll permanently fallback to using
                 XImage. This shouldn't happen */
              if (tex_pixmap->glx_pixmap == None)
                {
                  COGL_NOTE (TEXTURE_PIXMAP, "Falling back to XGetImage "
                             "updates for %p because creating the GLXPixmap "
                             "with mipmap support failed", tex_pixmap);

                  if (tex_pixmap->glx_tex)
                    cogl_handle_unref (tex_pixmap->glx_tex);

                  ret = FALSE;
                }
              else
                tex_pixmap->bind_tex_image_queued = TRUE;
            }
        }

      if (ret && tex_pixmap->bind_tex_image_queued)
        {
          GLuint gl_handle, gl_target;

          cogl_texture_get_gl_texture (tex_pixmap->glx_tex,
                                       &gl_handle, &gl_target);

          COGL_NOTE (TEXTURE_PIXMAP, "Rebinding GLXPixmap for %p", tex_pixmap);

          GE( _cogl_bind_gl_texture_transient (gl_target, gl_handle, FALSE) );

          if (tex_pixmap->pixmap_bound)
            glx_renderer->pf_glXReleaseTexImage (cogl_xlib_get_display (),
                                                 tex_pixmap->glx_pixmap,
                                                 GLX_FRONT_LEFT_EXT);

          glx_renderer->pf_glXBindTexImage (cogl_xlib_get_display (),
                                            tex_pixmap->glx_pixmap,
                                            GLX_FRONT_LEFT_EXT,
                                            NULL);

          /* According to the recommended usage in the spec for
             GLX_EXT_texture_pixmap we should release the texture after
             we've finished drawing with it and it is undefined what
             happens if you render to a pixmap that is bound to a
             texture. However that would require the texture backend to
             know when Cogl has finished painting and it may be more
             expensive to keep unbinding the texture. Leaving it bound
             appears to work on Mesa and NVidia drivers and it is also
             what Compiz does so it is probably ok */

          tex_pixmap->bind_tex_image_queued = FALSE;
          tex_pixmap->pixmap_bound = TRUE;

          _cogl_texture_2d_externally_modified (tex_pixmap->glx_tex);
        }
    }

  return ret;
}

static void
_cogl_texture_pixmap_x11_set_use_glx_texture (CoglTexturePixmapX11 *tex_pixmap,
                                              gboolean new_value)
{
  if (tex_pixmap->use_glx_texture != new_value)
    {
      /* Notify cogl-pipeline.c that the texture's underlying GL texture
       * storage is changing so it knows it may need to bind a new texture
       * if the CoglTexture is reused with the same texture unit. */
      _cogl_pipeline_texture_storage_change_notify (tex_pixmap);

      tex_pixmap->use_glx_texture = new_value;
    }
}

#endif /* COGL_HAS_GLX_SUPPORT */

static void
_cogl_texture_pixmap_x11_update (CoglTexturePixmapX11 *tex_pixmap,
                                 gboolean needs_mipmap)
{
#ifdef COGL_HAS_GLX_SUPPORT

  /* First try updating with GLX TFP */
  if (_cogl_texture_pixmap_x11_update_glx_texture (tex_pixmap, needs_mipmap))
    {
      _cogl_texture_pixmap_x11_set_use_glx_texture (tex_pixmap, TRUE);
      return;
    }

  /* If it didn't work then fallback to using XGetImage. This may be
     temporary */
  _cogl_texture_pixmap_x11_set_use_glx_texture (tex_pixmap, FALSE);

#endif /* COGL_HAS_GLX_SUPPORT */

  _cogl_texture_pixmap_x11_update_image_texture (tex_pixmap);
}

static CoglHandle
_cogl_texture_pixmap_x11_get_texture (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglHandle tex;
  int i;

  /* We try getting the texture twice, once without flushing the
     updates and once with. If pre_paint has been called already then
     we should have a good idea of which texture to use so we don't
     want to mess with that by ensuring the updates. However, if we
     couldn't find a texture then we'll just make a best guess by
     flushing without expecting mipmap support and try again. This
     would happen for example if an application calls
     get_gl_texture before the first paint */

  for (i = 0; i < 2; i++)
    {
#ifdef COGL_HAS_GLX_SUPPORT
      if (tex_pixmap->use_glx_texture)
        tex = tex_pixmap->glx_tex;
      else
#endif
        tex = tex_pixmap->tex;

      if (tex)
        return tex;

      _cogl_texture_pixmap_x11_update (tex_pixmap, FALSE);
    }

  g_assert_not_reached ();

  return COGL_INVALID_HANDLE;
}

static gboolean
_cogl_texture_pixmap_x11_set_region (CoglTexture     *tex,
                                     int              src_x,
                                     int              src_y,
                                     int              dst_x,
                                     int              dst_y,
                                     unsigned int     dst_width,
                                     unsigned int     dst_height,
                                     CoglBitmap      *bmp)
{
  /* This doesn't make much sense for texture from pixmap so it's not
     supported */
  return FALSE;
}

static gboolean
_cogl_texture_pixmap_x11_get_data (CoglTexture     *tex,
                                   CoglPixelFormat  format,
                                   unsigned int     rowstride,
                                   guint8          *data)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  return cogl_texture_get_data (child_tex, format, rowstride, data);
}

static void
_cogl_texture_pixmap_x11_foreach_sub_texture_in_region
                                  (CoglTexture              *tex,
                                   float                     virtual_tx_1,
                                   float                     virtual_ty_1,
                                   float                     virtual_tx_2,
                                   float                     virtual_ty_2,
                                   CoglTextureSliceCallback  callback,
                                   void                     *user_data)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  _cogl_texture_foreach_sub_texture_in_region (child_tex,
                                               virtual_tx_1,
                                               virtual_ty_1,
                                               virtual_tx_2,
                                               virtual_ty_2,
                                               callback,
                                               user_data);
}

static int
_cogl_texture_pixmap_x11_get_max_waste (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  return cogl_texture_get_max_waste (child_tex);
}

static gboolean
_cogl_texture_pixmap_x11_is_sliced (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  return cogl_texture_is_sliced (child_tex);
}

static gboolean
_cogl_texture_pixmap_x11_can_hardware_repeat (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  return cogl_texture_get_max_waste (child_tex);
}

static void
_cogl_texture_pixmap_x11_transform_coords_to_gl (CoglTexture *tex,
                                                 float       *s,
                                                 float       *t)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  _cogl_texture_transform_coords_to_gl (child_tex, s, t);
}

static CoglTransformResult
_cogl_texture_pixmap_x11_transform_quad_coords_to_gl (CoglTexture *tex,
                                                      float       *coords)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  return _cogl_texture_transform_quad_coords_to_gl (child_tex, coords);
}

static gboolean
_cogl_texture_pixmap_x11_get_gl_texture (CoglTexture *tex,
                                         GLuint      *out_gl_handle,
                                         GLenum      *out_gl_target)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  return cogl_texture_get_gl_texture (child_tex,
                                      out_gl_handle,
                                      out_gl_target);
}

static void
_cogl_texture_pixmap_x11_set_filters (CoglTexture *tex,
                                      GLenum       min_filter,
                                      GLenum       mag_filter)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  _cogl_texture_set_filters (child_tex, min_filter, mag_filter);
}

static void
_cogl_texture_pixmap_x11_pre_paint (CoglTexture *tex,
                                    CoglTexturePrePaintFlags flags)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  _cogl_texture_pixmap_x11_update (tex_pixmap,
                                   !!(flags & COGL_TEXTURE_NEEDS_MIPMAP));

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  _cogl_texture_pre_paint (child_tex, flags);
}

static void
_cogl_texture_pixmap_x11_ensure_non_quad_rendering (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  _cogl_texture_ensure_non_quad_rendering (child_tex);
}

static void
_cogl_texture_pixmap_x11_set_wrap_mode_parameters (CoglTexture *tex,
                                                   GLenum       wrap_mode_s,
                                                   GLenum       wrap_mode_t,
                                                   GLenum       wrap_mode_p)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  _cogl_texture_set_wrap_mode_parameters (child_tex,
                                          wrap_mode_s,
                                          wrap_mode_t,
                                          wrap_mode_p);
}

static CoglPixelFormat
_cogl_texture_pixmap_x11_get_format (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  /* Forward on to the child texture */
  return cogl_texture_get_format (child_tex);
}

static GLenum
_cogl_texture_pixmap_x11_get_gl_format (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);
  CoglHandle child_tex;

  child_tex = _cogl_texture_pixmap_x11_get_texture (tex_pixmap);

  return _cogl_texture_get_gl_format (child_tex);
}

static int
_cogl_texture_pixmap_x11_get_width (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);

  return tex_pixmap->width;
}

static int
_cogl_texture_pixmap_x11_get_height (CoglTexture *tex)
{
  CoglTexturePixmapX11 *tex_pixmap = COGL_TEXTURE_PIXMAP_X11 (tex);

  return tex_pixmap->height;
}

static void
_cogl_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap)
{
  set_damage_object_internal (tex_pixmap, 0, 0);

  if (tex_pixmap->image)
    XDestroyImage (tex_pixmap->image);

  if (tex_pixmap->shm_info.shmid != -1)
    {
      XShmDetach (cogl_xlib_get_display (), &tex_pixmap->shm_info);
      shmdt (tex_pixmap->shm_info.shmaddr);
      shmctl (tex_pixmap->shm_info.shmid, IPC_RMID, 0);
    }

  if (tex_pixmap->tex)
    cogl_handle_unref (tex_pixmap->tex);

#ifdef COGL_HAS_GLX_SUPPORT
  _cogl_texture_pixmap_x11_free_glx_pixmap (tex_pixmap);

  if (tex_pixmap->glx_tex)
    cogl_handle_unref (tex_pixmap->glx_tex);
#endif

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_pixmap));
}

static const CoglTextureVtable
cogl_texture_pixmap_x11_vtable =
  {
    _cogl_texture_pixmap_x11_set_region,
    _cogl_texture_pixmap_x11_get_data,
    _cogl_texture_pixmap_x11_foreach_sub_texture_in_region,
    _cogl_texture_pixmap_x11_get_max_waste,
    _cogl_texture_pixmap_x11_is_sliced,
    _cogl_texture_pixmap_x11_can_hardware_repeat,
    _cogl_texture_pixmap_x11_transform_coords_to_gl,
    _cogl_texture_pixmap_x11_transform_quad_coords_to_gl,
    _cogl_texture_pixmap_x11_get_gl_texture,
    _cogl_texture_pixmap_x11_set_filters,
    _cogl_texture_pixmap_x11_pre_paint,
    _cogl_texture_pixmap_x11_ensure_non_quad_rendering,
    _cogl_texture_pixmap_x11_set_wrap_mode_parameters,
    _cogl_texture_pixmap_x11_get_format,
    _cogl_texture_pixmap_x11_get_gl_format,
    _cogl_texture_pixmap_x11_get_width,
    _cogl_texture_pixmap_x11_get_height,
    NULL /* is_foreign */
  };
