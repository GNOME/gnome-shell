/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010,2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-renderer-glx-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-display-xlib-private.h"
#include "cogl-display-glx-private.h"
#include "cogl-private.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

typedef CoglFuncPtr (*GLXGetProcAddressProc) (const GLubyte *procName);

#ifdef HAVE_DRM
#include <drm.h>
#include <sys/ioctl.h>
#include <errno.h>
#endif

typedef struct _CoglContextGLX
{
  GLXDrawable current_drawable;
} CoglContextGLX;

typedef struct _CoglOnscreenXlib
{
  Window xwin;
  gboolean is_foreign_xwin;
} CoglOnscreenXlib;

typedef struct _CoglOnscreenGLX
{
  CoglOnscreenXlib _parent;
  GLXDrawable glxwin;
  guint32 last_swap_vsync_counter;
  GList *swap_callbacks;
} CoglOnscreenGLX;

typedef struct _CoglSwapBuffersNotifyEntry
{
  CoglSwapBuffersNotify callback;
  void *user_data;
  unsigned int id;
} CoglSwapBuffersNotifyEntry;


/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  static const CoglFeatureFunction                                      \
  cogl_glx_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglRendererGLX, pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-glx-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  { 255, 255, namespaces, extension_names,                              \
      feature_flags, feature_flags_private,                             \
      winsys_feature, \
      cogl_glx_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData winsys_feature_data[] =
  {
#include "cogl-winsys-glx-feature-functions.h"
  };

CoglFuncPtr
_cogl_winsys_get_proc_address (const char *name)
{
  static GLXGetProcAddressProc get_proc_func = NULL;
  static void *dlhand = NULL;

  if (get_proc_func == NULL && dlhand == NULL)
    {
      dlhand = dlopen (NULL, RTLD_LAZY);

      if (!dlhand)
        {
          g_warning ("Failed to dlopen (NULL, RTDL_LAZY): %s", dlerror ());
          return NULL;
        }

      dlerror ();

      get_proc_func =
        (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddress");

      if (dlerror () != NULL)
        {
          get_proc_func =
            (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddressARB");
        }

      if (dlerror () != NULL)
        {
          get_proc_func = NULL;
          g_warning ("failed to bind GLXGetProcAddress "
                     "or GLXGetProcAddressARB");
        }
    }

  if (get_proc_func)
    return get_proc_func ((GLubyte *) name);

  return NULL;
}

#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f)
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  glx_renderer->pf_ ## name = NULL;
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static void
initialize_function_table (CoglRenderer *renderer)
{
  CoglRendererGLX *glx_renderer = renderer->winsys;

#include "cogl-winsys-glx-feature-functions.h"
}

static CoglOnscreen *
find_onscreen_for_xid (CoglContext *context, guint32 xid)
{
  GList *l;

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;
      CoglOnscreenXlib *xlib_onscreen;

      if (!framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        continue;

      /* Does the GLXEvent have the GLXDrawable or the X Window? */
      xlib_onscreen = COGL_ONSCREEN (framebuffer)->winsys;
      if (xlib_onscreen->xwin == (Window)xid)
        return COGL_ONSCREEN (framebuffer);
    }

  return NULL;
}

static void
notify_swap_buffers (CoglContext *context, GLXDrawable drawable)
{
  CoglOnscreen *onscreen = find_onscreen_for_xid (context, (guint32)drawable);
  CoglOnscreenGLX *glx_onscreen;
  GList *l;

  if (!onscreen)
    return;

  glx_onscreen = onscreen->winsys;

  for (l = glx_onscreen->swap_callbacks; l; l = l->next)
    {
      CoglSwapBuffersNotifyEntry *entry = l->data;
      entry->callback (COGL_FRAMEBUFFER (onscreen), entry->user_data);
    }
}

static CoglXlibFilterReturn
glx_event_filter_cb (XEvent *xevent, void *data)
{
  CoglContext *context = data;
  CoglRendererGLX *glx_renderer = context->display->renderer->winsys;

  if (xevent->type == ConfigureNotify)
    {
      CoglOnscreen *onscreen =
        find_onscreen_for_xid (context, xevent->xconfigure.window);

      if (onscreen)
        {
          CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

          _cogl_framebuffer_winsys_update_size (framebuffer,
                                                xevent->xconfigure.width,
                                                xevent->xconfigure.height);
        }
    }
  else if (xevent->type ==
           (glx_renderer->glx_event_base + GLX_BufferSwapComplete))
    {
      GLXBufferSwapComplete *swap_event = (GLXBufferSwapComplete *)xevent;
      notify_swap_buffers (context, swap_event->drawable);
      return COGL_XLIB_FILTER_REMOVE;
    }

  return COGL_XLIB_FILTER_CONTINUE;
}

gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererGLX *glx_renderer;
  CoglRendererXlib *xlib_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererGLX);

  glx_renderer = renderer->winsys;
  xlib_renderer = renderer->winsys;

  if (!_cogl_renderer_xlib_connect (renderer, error))
    goto error;

  if (!glXQueryExtension (xlib_renderer->xdpy,
                          &glx_renderer->glx_error_base,
                          &glx_renderer->glx_event_base))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "XServer appears to lack required GLX support");
      goto error;
    }

  /* XXX: Note: For a long time Mesa exported a hybrid GLX, exporting
   * extensions specified to require GLX 1.3, but still reporting 1.2
   * via glXQueryVersion. */
  if (!glXQueryVersion (xlib_renderer->xdpy,
                        &glx_renderer->glx_major,
                        &glx_renderer->glx_minor)
      || !(glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 2))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "XServer appears to lack required GLX 1.2 support");
      goto error;
    }

  glx_renderer->dri_fd = -1;

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  _cogl_renderer_xlib_disconnect (renderer);

  g_slice_free (CoglRendererGLX, renderer->winsys);
}

void
update_winsys_features (CoglContext *context)
{
  CoglDisplayGLX *glx_display = context->display->winsys;
  CoglRendererXlib *xlib_renderer = context->display->renderer->winsys;
  CoglRendererGLX *glx_renderer = context->display->renderer->winsys;
  const char *glx_extensions;
  int i;

  g_return_if_fail (glx_display->glx_context);

  _cogl_gl_update_features (context);

  _cogl_bitmask_init (&context->winsys_features);

  glx_extensions =
    glXQueryExtensionsString (xlib_renderer->xdpy,
                              DefaultScreen (xlib_renderer->xdpy));

  COGL_NOTE (WINSYS, "  GLX Extensions: %s", glx_extensions);

  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  _cogl_bitmask_set (&context->winsys_features,
                     COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                     TRUE);

  initialize_function_table (context->display->renderer);

  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check ("GLX", winsys_feature_data + i, 0, 0,
                             glx_extensions,
                             glx_renderer))
      {
        context->feature_flags |= winsys_feature_data[i].feature_flags;
        if (winsys_feature_data[i].winsys_feature)
          _cogl_bitmask_set (&context->winsys_features,
                             winsys_feature_data[i].winsys_feature,
                             TRUE);
      }

  /* Note: the GLX_SGI_video_sync spec explicitly states this extension
   * only works for direct contexts. */
  if (!glx_renderer->is_direct)
    {
      glx_renderer->pf_glXGetVideoSync = NULL;
      glx_renderer->pf_glXWaitVideoSync = NULL;
    }

  if (glx_renderer->pf_glXWaitVideoSync)
    _cogl_bitmask_set (&context->winsys_features,
                       COGL_WINSYS_FEATURE_VBLANK_WAIT,
                       TRUE);

#ifdef HAVE_DRM
  /* drm is really an extreme fallback -rumoured to work with Via
   * chipsets... */
  if (!glx_renderer->pf_glXWaitVideoSync)
    {
      if (glx_renderer->dri_fd < 0)
        glx_renderer->dri_fd = open("/dev/dri/card0", O_RDWR);
      if (glx_renderer->dri_fd >= 0)
        _cogl_bitmask_set (&context->winsys_features,
                           COGL_WINSYS_FEATURE_VBLANK_WAIT,
                           TRUE);
    }
#endif

  if (glx_renderer->pf_glXCopySubBuffer || context->drv.pf_glBlitFramebuffer)
    _cogl_bitmask_set (&context->winsys_features,
                       COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);

  /* Note: glXCopySubBuffer and glBlitFramebuffer won't be throttled
   * by the SwapInterval so we have to throttle swap_region requests
   * manually... */
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION) &&
      _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT))
    _cogl_bitmask_set (&context->winsys_features,
                       COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);
}

/* It seems the GLX spec never defined an invalid GLXFBConfig that
 * we could overload as an indication of error, so we have to return
 * an explicit boolean status. */
static gboolean
find_fbconfig (CoglDisplay *display,
               gboolean with_alpha,
               GLXFBConfig *config_ret,
               GError **error)
{
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
  GLXFBConfig *configs = NULL;
  int n_configs, i;
  static const int attributes[] = {
    GLX_DRAWABLE_TYPE,    GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,      GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,     GL_TRUE,
    GLX_RED_SIZE,         1,
    GLX_GREEN_SIZE,       1,
    GLX_BLUE_SIZE,        1,
    GLX_ALPHA_SIZE,       1,
    GLX_DEPTH_SIZE,       1,
    GLX_STENCIL_SIZE,     1,
    None
  };
  gboolean ret = TRUE;
  int xscreen_num = DefaultScreen (xlib_renderer->xdpy);

  configs = glXChooseFBConfig (xlib_renderer->xdpy,
                               xscreen_num,
                               attributes,
                               &n_configs);
  if (!configs || n_configs == 0)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to find any compatible fbconfigs");
      ret = FALSE;
      goto done;
    }

  if (with_alpha)
    {
      for (i = 0; i < n_configs; i++)
        {
          XVisualInfo *vinfo;

          vinfo = glXGetVisualFromFBConfig (xlib_renderer->xdpy, configs[i]);
          if (vinfo == NULL)
            continue;

          if (vinfo->depth == 32 &&
              (vinfo->red_mask | vinfo->green_mask | vinfo->blue_mask)
              != 0xffffffff)
            {
              COGL_NOTE (WINSYS, "Found an ARGB FBConfig [index:%d]", i);
              *config_ret = configs[i];
              goto done;
            }
        }

      /* If we make it here then we didn't find an RGBA config so
         we'll fall back to using an RGB config */
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find fbconfig with rgba visual");
      ret = FALSE;
      goto done;
    }
  else
    {
      COGL_NOTE (WINSYS, "Using the first available FBConfig");
      *config_ret = configs[0];
    }

done:
  XFree (configs);
  return ret;
}

static gboolean
create_context (CoglDisplay *display, GError **error)
{
  CoglDisplayGLX *glx_display = display->winsys;
  CoglDisplayXlib *xlib_display = display->winsys;
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
  CoglRendererGLX *glx_renderer = display->renderer->winsys;
  gboolean support_transparent_windows;
  GLXFBConfig config;
  GError *fbconfig_error = NULL;
  XSetWindowAttributes attrs;
  XVisualInfo *xvisinfo;
  GLXDrawable dummy_drawable;
  CoglXlibTrapState old_state;

  g_return_val_if_fail (glx_display->glx_context == NULL, TRUE);

  if (display->onscreen_template &&
      display->onscreen_template->swap_chain &&
      display->onscreen_template->swap_chain->has_alpha)
    support_transparent_windows = TRUE;
  else
    support_transparent_windows = FALSE;

  glx_display->found_fbconfig =
    find_fbconfig (display, support_transparent_windows, &config,
                   &fbconfig_error);
  if (!glx_display->found_fbconfig)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find suitable fbconfig for the GLX context: %s",
                   fbconfig_error->message);
      g_error_free (fbconfig_error);
      return FALSE;
    }

  glx_display->fbconfig = config;
  glx_display->fbconfig_has_rgba_visual = support_transparent_windows;

  COGL_NOTE (WINSYS, "Creating GLX Context (display: %p)",
             xlib_renderer->xdpy);

  glx_display->glx_context = glXCreateNewContext (xlib_renderer->xdpy,
                                                  config,
                                                  GLX_RGBA_TYPE,
                                                  NULL,
                                                  True);
  if (glx_display->glx_context == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to create suitable GL context");
      return FALSE;
    }

  glx_renderer->is_direct =
    glXIsDirect (xlib_renderer->xdpy, glx_display->glx_context);

  COGL_NOTE (WINSYS, "Setting %s context",
             glx_renderer->is_direct ? "direct" : "indirect");

  /* XXX: GLX doesn't let us make a context current without a window
   * so we create a dummy window that we can use while no CoglOnscreen
   * framebuffer is in use.
   */

  xvisinfo = glXGetVisualFromFBConfig (xlib_renderer->xdpy, config);
  if (xvisinfo == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to retrieve the X11 visual");
      return FALSE;
    }

  _cogl_renderer_xlib_trap_errors (display->renderer, &old_state);

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (xlib_renderer->xdpy,
                                    DefaultRootWindow (xlib_renderer->xdpy),
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  xlib_display->dummy_xwin =
    XCreateWindow (xlib_renderer->xdpy,
                   DefaultRootWindow (xlib_renderer->xdpy),
                   -100, -100, 1, 1,
                   0,
                   xvisinfo->depth,
                   CopyFromParent,
                   xvisinfo->visual,
                   CWOverrideRedirect | CWColormap | CWBorderPixel,
                   &attrs);

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3)
    {
      glx_display->dummy_glxwin = glXCreateWindow (xlib_renderer->xdpy,
                                                   config,
                                                   xlib_display->dummy_xwin,
                                                   NULL);
    }

  if (glx_display->dummy_glxwin)
    dummy_drawable = glx_display->dummy_glxwin;
  else
    dummy_drawable = xlib_display->dummy_xwin;

  COGL_NOTE (WINSYS, "Selecting dummy 0x%x for the GLX context",
             (unsigned int) dummy_drawable);

  glXMakeContextCurrent (xlib_renderer->xdpy,
                         dummy_drawable,
                         dummy_drawable,
                         glx_display->glx_context);

  XFree (xvisinfo);

  if (_cogl_renderer_xlib_untrap_errors (display->renderer, &old_state))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to select the newly created GLX context");
      return FALSE;
    }

  return TRUE;
}

gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplayGLX *glx_display;
  CoglDisplayXlib *xlib_display;
  int i;

  g_return_val_if_fail (display->winsys == NULL, FALSE);

  glx_display = g_slice_new0 (CoglDisplayGLX);
  display->winsys = glx_display;

  xlib_display = display->winsys;

  if (!create_context (display, error))
    goto error;

  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    glx_display->glx_cached_configs[i].depth = -1;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplayGLX *glx_display = display->winsys;
  CoglDisplayXlib *xlib_display = display->winsys;
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;

  g_return_if_fail (glx_display != NULL);

  if (glx_display->glx_context)
    {
      glXMakeContextCurrent (xlib_renderer->xdpy, None, None, NULL);
      glXDestroyContext (xlib_renderer->xdpy, glx_display->glx_context);
      glx_display->glx_context = NULL;
    }

  if (glx_display->dummy_glxwin)
    {
      glXDestroyWindow (xlib_renderer->xdpy, glx_display->dummy_glxwin);
      glx_display->dummy_glxwin = None;
    }

  if (xlib_display->dummy_xwin)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_display->dummy_xwin);
      xlib_display->dummy_xwin = None;
    }

  g_slice_free (CoglDisplayGLX, display->winsys);
  display->winsys = NULL;
}

gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  context->winsys = g_new0 (CoglContextGLX, 1);

  cogl_renderer_xlib_add_filter (context->display->renderer,
                                 glx_event_filter_cb,
                                 context);
  update_winsys_features (context);

  return TRUE;
}

void
_cogl_winsys_context_deinit (CoglContext *context)
{
  cogl_renderer_xlib_remove_filter (context->display->renderer,
                                    glx_event_filter_cb,
                                    context);
  g_free (context->winsys);
}

gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayGLX *glx_display = display->winsys;
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
  CoglRendererGLX *glx_renderer = display->renderer->winsys;
  Window xwin;
  CoglOnscreenXlib *xlib_onscreen;
  CoglOnscreenGLX *glx_onscreen;

  g_return_val_if_fail (glx_display->glx_context, FALSE);

  /* FIXME: We need to explicitly Select for ConfigureNotify events.
   * For foreign windows we need to be careful not to mess up any
   * existing event mask.
   * We need to document that for windows we create then toolkits
   * must be careful not to clear event mask bits that we select.
   */

  /* XXX: Note we ignore the user's original width/height when
   * given a foreign X window. */
  if (onscreen->foreign_xid)
    {
      Status status;
      CoglXlibTrapState state;
      XWindowAttributes attr;
      int xerror;

      xwin = onscreen->foreign_xid;

      _cogl_renderer_xlib_trap_errors (display->renderer, &state);

      status = XGetWindowAttributes (xlib_renderer->xdpy, xwin, &attr);
      XSync (xlib_renderer->xdpy, False);
      xerror = _cogl_renderer_xlib_untrap_errors (display->renderer, &state);
      if (status == 0 || xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror, message, sizeof(message));
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to query geometry of foreign xid 0x%08lX: %s",
                       xwin, message);
          return FALSE;
        }

      _cogl_framebuffer_winsys_update_size (framebuffer,
                                            attr.width, attr.height);
    }
  else
    {
      int width;
      int height;
      CoglXlibTrapState state;
      XVisualInfo *xvisinfo;
      XSetWindowAttributes xattr;
      unsigned long mask;
      int xerror;

      width = cogl_framebuffer_get_width (framebuffer);
      height = cogl_framebuffer_get_height (framebuffer);

      _cogl_renderer_xlib_trap_errors (display->renderer, &state);

      xvisinfo = glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                           glx_display->fbconfig);
      if (xvisinfo == NULL)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to retrieve the X11 visual of context's "
                       "fbconfig");
          return FALSE;
        }

      /* window attributes */
      xattr.background_pixel = WhitePixel (xlib_renderer->xdpy,
                                           DefaultScreen (xlib_renderer->xdpy));
      xattr.border_pixel = 0;
      /* XXX: is this an X resource that we are leaking‽... */
      xattr.colormap = XCreateColormap (xlib_renderer->xdpy,
                                        DefaultRootWindow (xlib_renderer->xdpy),
                                        xvisinfo->visual,
                                        AllocNone);
      mask = CWBorderPixel | CWColormap;

      xwin = XCreateWindow (xlib_renderer->xdpy,
                            DefaultRootWindow (xlib_renderer->xdpy),
                            0, 0,
                            width, height,
                            0,
                            xvisinfo->depth,
                            InputOutput,
                            xvisinfo->visual,
                            mask, &xattr);

      XFree (xvisinfo);

      XSync (xlib_renderer->xdpy, False);
      xerror = _cogl_renderer_xlib_untrap_errors (display->renderer, &state);
      if (xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror,
                         message, sizeof (message));
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "X error while creating Window for CoglOnscreen: %s",
                       message);
          return FALSE;
        }
    }

  onscreen->winsys = g_slice_new0 (CoglOnscreenGLX);
  xlib_onscreen = onscreen->winsys;
  glx_onscreen = onscreen->winsys;

  xlib_onscreen->xwin = xwin;
  xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? TRUE : FALSE;

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3)
    {
      glx_onscreen->glxwin =
        glXCreateWindow (xlib_renderer->xdpy,
                         glx_display->fbconfig,
                         xlib_onscreen->xwin,
                         NULL);
    }

#ifdef GLX_INTEL_swap_event
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT))
    {
      GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

      /* similarly to above, we unconditionally select this event
       * because we rely on it to advance the master clock, and
       * drive redraw/relayout, animations and event handling.
       */
      glXSelectEvent (xlib_renderer->xdpy,
                      drawable,
                      GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
#endif /* GLX_INTEL_swap_event */

  return TRUE;
}

void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRendererXlib *xlib_renderer = context->display->renderer->winsys;
  CoglXlibTrapState old_state;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;

  _cogl_xlib_trap_errors (&old_state);

  if (glx_onscreen->glxwin != None)
    {
      glXDestroyWindow (xlib_renderer->xdpy, glx_onscreen->glxwin);
      glx_onscreen->glxwin = None;
    }

  if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
      xlib_onscreen->xwin = None;
    }
  else
    xlib_onscreen->xwin = None;

  XSync (xlib_renderer->xdpy, False);

  _cogl_xlib_untrap_errors (&old_state);
}

void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglDisplayXlib *xlib_display = context->display->winsys;
  CoglDisplayGLX *glx_display = context->display->winsys;
  CoglRendererXlib *xlib_renderer = context->display->renderer->winsys;
  CoglRendererGLX *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglXlibTrapState old_state;
  GLXDrawable drawable;

  if (G_UNLIKELY (!onscreen))
    {
      drawable =
        glx_display->dummy_glxwin ?
        glx_display->dummy_glxwin : xlib_display->dummy_xwin;

      if (glx_context->current_drawable == drawable)
        return;

      _cogl_xlib_trap_errors (&old_state);

      glXMakeContextCurrent (xlib_renderer->xdpy,
                             drawable, drawable,
                             glx_display->glx_context);
    }
  else
    {
      drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

      if (glx_context->current_drawable == drawable)
        return;

      _cogl_xlib_trap_errors (&old_state);

      COGL_NOTE (WINSYS,
                 "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
                 xlib_renderer->xdpy,
                 (unsigned int) drawable,
                 xlib_onscreen->is_foreign_xwin ? "foreign" : "native",
                 glx_display->glx_context);

      glXMakeContextCurrent (xlib_renderer->xdpy,
                             drawable,
                             drawable,
                             glx_display->glx_context);

      /* In case we are using GLX_SGI_swap_control for vblank syncing
       * we need call glXSwapIntervalSGI here to make sure that it
       * affects the current drawable.
       *
       * Note: we explicitly set to 0 when we aren't using the swap
       * interval to synchronize since some drivers have a default
       * swap interval of 1. Sadly some drivers even ignore requests
       * to disable the swap interval.
       *
       * NB: glXSwapIntervalSGI applies to the context not the
       * drawable which is why we can't just do this once when the
       * framebuffer is allocated.
       *
       * FIXME: We should check for GLX_EXT_swap_control which allows
       * per framebuffer swap intervals. GLX_MESA_swap_control also
       * allows per-framebuffer swap intervals but the semantics tend
       * to be more muddled since Mesa drivers tend to expose both the
       * MESA and SGI extensions which should technically be mutually
       * exclusive.
       */
      if (glx_renderer->pf_glXSwapInterval)
        {
          if (onscreen->swap_throttled)
            glx_renderer->pf_glXSwapInterval (1);
          else
            glx_renderer->pf_glXSwapInterval (0);
        }
    }

  XSync (xlib_renderer->xdpy, False);

  /* FIXME: We should be reporting a GError here
   */
  if (_cogl_xlib_untrap_errors (&old_state))
    {
      g_warning ("X Error received while making drawable 0x%08lX current",
                 drawable);
      return;
    }

  glx_context->current_drawable = drawable;
}

#ifdef HAVE_DRM
static int
drm_wait_vblank (int fd, drm_wait_vblank_t *vbl)
{
    int ret, rc;

    do
      {
        ret = ioctl (fd, DRM_IOCTL_WAIT_VBLANK, vbl);
        vbl->request.type &= ~_DRM_VBLANK_RELATIVE;
        rc = errno;
      }
    while (ret && rc == EINTR);

    return rc;
}
#endif /* HAVE_DRM */

void
_cogl_winsys_wait_for_vblank (void)
{
  CoglDisplayGLX *glx_display;
  CoglRendererGLX *glx_renderer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  glx_display = ctx->display->winsys;
  glx_renderer = ctx->display->renderer->winsys;

  if (glx_renderer->pf_glXGetVideoSync)
    {
      guint32 current_count;

      glx_renderer->pf_glXGetVideoSync (&current_count);
      glx_renderer->pf_glXWaitVideoSync (2,
                                         (current_count + 1) % 2,
                                         &current_count);
    }
#ifdef HAVE_DRM
  else
    {
      drm_wait_vblank_t blank;

      COGL_NOTE (WINSYS, "Waiting for vblank (drm)");
      blank.request.type = _DRM_VBLANK_RELATIVE;
      blank.request.sequence = 1;
      blank.request.signal = 0;
      drm_wait_vblank (glx_renderer->dri_fd, &blank);
    }
#endif /* HAVE_DRM */
}

void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   int *rectangles,
                                   int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRendererXlib *xlib_renderer = context->display->renderer->winsys;
  CoglRendererGLX *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
  guint32 end_frame_vsync_counter;
  gboolean have_counter;
  gboolean can_wait;

  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_FLUSH_BIND_ONLY);

  if (onscreen->swap_throttled)
    {
      have_counter =
        _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_COUNTER);
      can_wait = _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT);
    }
  else
    {
      have_counter = FALSE;
      can_wait = FALSE;
    }

  /* We need to ensure that all the rendering is done, otherwise
   * redraw operations that are slower than the framerate can
   * queue up in the pipeline during a heavy animation, causing a
   * larger and larger backlog of rendering visible as lag to the
   * user.
   *
   * For an exaggerated example consider rendering at 60fps (so 16ms
   * per frame) and you have a really slow frame that takes 160ms to
   * render, even though painting the scene and issuing the commands
   * to the GPU takes no time at all. If all we did was use the
   * video_sync extension to throttle the painting done by the CPU
   * then every 16ms we would have another frame queued up even though
   * the GPU has only rendered one tenth of the current frame. By the
   * time the GPU would get to the 2nd frame there would be 9 frames
   * waiting to be rendered.
   *
   * The problem is that we don't currently have a good way to throttle
   * the GPU, only the CPU so we have to resort to synchronizing the
   * GPU with the CPU to throttle it.
   *
   * Note: since calling glFinish() and synchronizing the CPU with
   * the GPU is far from ideal, we hope that this is only a short
   * term solution.
   * - One idea is to using sync objects to track render
   *   completion so we can throttle the backlog (ideally with an
   *   additional extension that lets us get notifications in our
   *   mainloop instead of having to busy wait for the
   *   completion.)
   * - Another option is to support clipped redraws by reusing the
   *   contents of old back buffers such that we can flip instead
   *   of using a blit and then we can use GLX_INTEL_swap_events
   *   to throttle. For this though we would still probably want an
   *   additional extension so we can report the limited region of
   *   the window damage to X/compositors.
   */
  glFinish ();

  if (have_counter && can_wait)
    {
      end_frame_vsync_counter = _cogl_winsys_get_vsync_counter ();

      /* If we have the GLX_SGI_video_sync extension then we can
       * be a bit smarter about how we throttle blits by avoiding
       * any waits if we can see that the video sync count has
       * already progressed. */
      if (glx_onscreen->last_swap_vsync_counter == end_frame_vsync_counter)
        _cogl_winsys_wait_for_vblank ();
    }
  else if (can_wait)
    _cogl_winsys_wait_for_vblank ();

  if (glx_renderer->pf_glXCopySubBuffer)
    {
      Display *xdpy = xlib_renderer->xdpy;
      int i;
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          glx_renderer->pf_glXCopySubBuffer (xdpy, drawable,
                                             rect[0], rect[1], rect[2], rect[3]);
        }
    }
  else if (context->drv.pf_glBlitFramebuffer)
    {
      int i;
      /* XXX: checkout how this state interacts with the code to use
       * glBlitFramebuffer in Neil's texture atlasing branch */
      glDrawBuffer (GL_FRONT);
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          int x2 = rect[0] + rect[2];
          int y2 = rect[1] + rect[3];
          context->drv.pf_glBlitFramebuffer (rect[0], rect[1], x2, y2,
                                             rect[0], rect[1], x2, y2,
                                             GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
      glDrawBuffer (GL_BACK);
    }

  /* NB: unlike glXSwapBuffers, glXCopySubBuffer and
   * glBlitFramebuffer don't issue an implicit glFlush() so we
   * have to flush ourselves if we want the request to complete in
   * a finite amount of time since otherwise the driver can batch
   * the command indefinitely. */
  glFlush ();

  /* NB: It's important we save the counter we read before acting on
   * the swap request since if we are mixing and matching different
   * swap methods between frames we don't want to read the timer e.g.
   * after calling glFinish() some times and not for others.
   *
   * In other words; this way we consistently save the time at the end
   * of the applications frame such that the counter isn't muddled by
   * the varying costs of different swap methods.
   */
  if (have_counter)
    glx_onscreen->last_swap_vsync_counter = end_frame_vsync_counter;
}

guint32
_cogl_winsys_get_vsync_counter (void)
{
  guint32 video_sync_count;
  CoglRendererGLX *glx_renderer;

  _COGL_GET_CONTEXT (ctx, 0);

  glx_renderer = ctx->display->renderer->winsys;

  glx_renderer->pf_glXGetVideoSync (&video_sync_count);

  return video_sync_count;
}

void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRendererXlib *xlib_renderer = context->display->renderer->winsys;
  CoglRendererGLX *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  gboolean have_counter;
  GLXDrawable drawable;

  /* XXX: theoretically this shouldn't be necessary but at least with
   * the Intel drivers we have see that if we don't call
   * glXMakeContextCurrent for the drawable we are swapping then
   * we get a BadDrawable error from the X server. */
  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_FLUSH_BIND_ONLY);

  drawable = glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (onscreen->swap_throttled)
    {
      guint32 end_frame_vsync_counter;

      have_counter =
        _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_COUNTER);

      /* If the swap_region API is also being used then we need to track
       * the vsync counter for each swap request so we can manually
       * throttle swap_region requests. */
      if (have_counter)
        end_frame_vsync_counter = _cogl_winsys_get_vsync_counter ();

      if (!glx_renderer->pf_glXSwapInterval)
        {
          gboolean can_wait =
            _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT);

          /* If we are going to wait for VBLANK manually, we not only
           * need to flush out pending drawing to the GPU before we
           * sleep, we need to wait for it to finish. Otherwise, we
           * may end up with the situation:
           *
           *        - We finish drawing      - GPU drawing continues
           *        - We go to sleep         - GPU drawing continues
           * VBLANK - We call glXSwapBuffers - GPU drawing continues
           *                                 - GPU drawing continues
           *                                 - Swap buffers happens
           *
           * Producing a tear. Calling glFinish() first will cause us
           * to properly wait for the next VBLANK before we swap. This
           * obviously does not happen when we use _GLX_SWAP and let
           * the driver do the right thing
           */
          glFinish ();

          if (have_counter && can_wait)
            {
              if (glx_onscreen->last_swap_vsync_counter ==
                  end_frame_vsync_counter)
                _cogl_winsys_wait_for_vblank ();
            }
          else if (can_wait)
            _cogl_winsys_wait_for_vblank ();
        }
    }
  else
    have_counter = FALSE;

  glXSwapBuffers (xlib_renderer->xdpy, drawable);

  if (have_counter)
    glx_onscreen->last_swap_vsync_counter = _cogl_winsys_get_vsync_counter ();
}

guint32
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  return xlib_onscreen->xwin;
}

unsigned int
_cogl_winsys_onscreen_add_swap_buffers_callback (CoglOnscreen *onscreen,
                                                 CoglSwapBuffersNotify callback,
                                                 void *user_data)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglSwapBuffersNotifyEntry *entry = g_slice_new0 (CoglSwapBuffersNotifyEntry);
  static int next_swap_buffers_callback_id = 0;

  entry->callback = callback;
  entry->user_data = user_data;
  entry->id = next_swap_buffers_callback_id++;

  glx_onscreen->swap_callbacks =
    g_list_prepend (glx_onscreen->swap_callbacks, entry);

  return entry->id;
}

void
_cogl_winsys_onscreen_remove_swap_buffers_callback (CoglOnscreen *onscreen,
                                                    unsigned int id)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GList *l;

  for (l = glx_onscreen->swap_callbacks; l; l = l->next)
    {
      CoglSwapBuffersNotifyEntry *entry = l->data;
      if (entry->id == id)
        {
          g_slice_free (CoglSwapBuffersNotifyEntry, entry);
          glx_onscreen->swap_callbacks =
            g_list_delete_link (glx_onscreen->swap_callbacks, l);
          return;
        }
    }
}

void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (glx_context->current_drawable != drawable)
    return;

  glx_context->current_drawable = 0;
  _cogl_winsys_onscreen_bind (onscreen);
}

/* FIXME: we should distinguish renderer and context features */
gboolean
_cogl_winsys_has_feature (CoglWinsysFeature feature)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  return _cogl_bitmask_get (&ctx->winsys_features, feature);
}

/* XXX: This is a particularly hacky _cogl_winsys interface... */
XVisualInfo *
_cogl_winsys_xlib_get_visual_info (void)
{
  CoglDisplayXlib *xlib_display;
  CoglDisplayGLX *glx_display;
  CoglRendererXlib *xlib_renderer;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_return_val_if_fail (ctx->display->winsys, FALSE);

  xlib_display = ctx->display->winsys;
  glx_display = ctx->display->winsys;
  xlib_renderer = ctx->display->renderer->winsys;

  if (!glx_display->found_fbconfig)
    return NULL;

  return glXGetVisualFromFBConfig (xlib_renderer->xdpy, glx_display->fbconfig);
}
