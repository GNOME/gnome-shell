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
#include "cogl-onscreen-template-private.h"
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
#include "cogl-renderer-xlib-private.h"
#include "cogl-display-xlib-private.h"
#endif
#include "cogl-private.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#ifdef COGL_HAS_GLES1

#include <GLES/gl.h>
#include <GLES/egl.h>

#else

#include <EGL/egl.h>
#define NativeDisplayType EGLNativeDisplayType
#define NativeWindowType EGLNativeWindowType

#endif


#include <EGL/egl.h>

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
#include <X11/Xlib.h>
#endif

typedef struct _CoglRendererEGL
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglRendererXlib _parent;
#endif

  EGLDisplay edpy;

  EGLint egl_version_major;
  EGLint egl_version_minor;

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-egl-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END
} CoglRendererEGL;

typedef struct _CoglDisplayEGL
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglDisplayXlib _parent;
#endif

  EGLContext egl_context;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  EGLSurface dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)
  EGLSurface egl_surface;
  int egl_surface_width;
  int egl_surface_height;
  gboolean have_onscreen;
#else
#error "Unknown EGL platform"
#endif

  EGLConfig egl_config;
  gboolean found_egl_config;
} CoglDisplayEGL;

typedef struct _CoglContextEGL
{
  EGLSurface current_surface;
} CoglContextEGL;

typedef struct _CoglOnscreenXlib
{
  Window xwin;
  gboolean is_foreign_xwin;
} CoglOnscreenXlib;

typedef struct _CoglOnscreenEGL
{
  CoglOnscreenXlib _parent;
  EGLSurface egl_surface;
} CoglOnscreenEGL;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  static const CoglFeatureFunction                                      \
  cogl_egl_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglRendererEGL, pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-egl-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  { 255, 255, namespaces, extension_names,                              \
      feature_flags, feature_flags_private,                             \
      winsys_feature, \
      cogl_egl_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData winsys_feature_data[] =
  {
#include "cogl-winsys-egl-feature-functions.h"
  };

static CoglFuncPtr
_cogl_winsys_get_proc_address (const char *name)
{
  return (CoglFuncPtr) eglGetProcAddress (name);
}

#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f)
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  egl_renderer->pf_ ## name = NULL;
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static void
initialize_function_table (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

#include "cogl-winsys-egl-feature-functions.h"
}

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
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

      xlib_onscreen = COGL_ONSCREEN (framebuffer)->winsys;
      if (xlib_onscreen->xwin == (Window)xid)
        return COGL_ONSCREEN (framebuffer);
    }

  return NULL;
}

static CoglFilterReturn
event_filter_cb (void *event, void *data)
{
  XEvent *xevent = event;
  CoglContext *context = data;

  if (xevent->type == ConfigureNotify)
    {
      CoglOnscreen *onscreen =
        find_onscreen_for_xid (context, xevent->xconfigure.window);

      if (onscreen)
        {
          CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

          /* XXX: consider adding an abstraction for this... */
          framebuffer->width = xevent->xconfigure.width;
          framebuffer->height = xevent->xconfigure.height;
        }
    }

  return COGL_FILTER_CONTINUE;
}
#endif /* COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT */

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  _cogl_renderer_xlib_disconnect (renderer);
#endif

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererEGL *egl_renderer;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglRendererXlib *xlib_renderer;
#endif
  EGLBoolean status;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);

  egl_renderer = renderer->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  xlib_renderer = renderer->winsys;

  if (!_cogl_renderer_xlib_connect (renderer, error))
    goto error;

  egl_renderer->edpy =
    eglGetDisplay ((NativeDisplayType) xlib_renderer->xdpy);

  status = eglInitialize (egl_renderer->edpy,
                          &egl_renderer->egl_version_major,
                          &egl_renderer->egl_version_minor);
#else
  egl_renderer->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

  status = eglInitialize (egl_renderer->edpy,
			  &egl_renderer->egl_version_major,
			  &egl_renderer->egl_version_minor);

  if (status != EGL_TRUE)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to initialize EGL");
      goto error;
    }
#endif

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static void
update_winsys_features (CoglContext *context)
{
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  const char *egl_extensions;
  int i;

  g_return_if_fail (egl_display->egl_context);

  _cogl_gl_update_features (context);

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  egl_extensions = eglQueryString (egl_renderer->edpy, EGL_EXTENSIONS);

  COGL_NOTE (WINSYS, "  EGL Extensions: %s", egl_extensions);

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);
#endif

  initialize_function_table (context->display->renderer);

  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check ("EGL", winsys_feature_data + i, 0, 0,
                             egl_extensions,
                             egl_renderer))
      {
        context->feature_flags |= winsys_feature_data[i].feature_flags;
        if (winsys_feature_data[i].winsys_feature)
          COGL_FLAGS_SET (context->winsys_features,
                          winsys_feature_data[i].winsys_feature,
                          TRUE);
      }

  /* FIXME: the winsys_feature_data can currently only have one
   * winsys feature per extension... */
  if (egl_renderer->pf_eglSwapBuffersRegion)
    {
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);
    }
}

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
static XVisualInfo *
get_visual_info (CoglDisplay *display, EGLConfig egl_config)
{
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  XVisualInfo visinfo_template;
  int template_mask = 0;
  XVisualInfo *visinfo = NULL;
  int visinfos_count;
  EGLint visualid, red_size, green_size, blue_size, alpha_size;

  eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                      EGL_NATIVE_VISUAL_ID, &visualid);

  if (visualid != 0)
    {
      visinfo_template.visualid = visualid;
      template_mask |= VisualIDMask;
    }
  else
    {
      /* some EGL drivers don't implement the EGL_NATIVE_VISUAL_ID
       * attribute, so attempt to find the closest match. */

      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_RED_SIZE, &red_size);
      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_GREEN_SIZE, &green_size);
      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_BLUE_SIZE, &blue_size);
      eglGetConfigAttrib (egl_renderer->edpy, egl_config,
                          EGL_ALPHA_SIZE, &alpha_size);

      visinfo_template.depth = red_size + green_size + blue_size + alpha_size;
      template_mask |= VisualDepthMask;

      visinfo_template.screen = DefaultScreen (xlib_renderer->xdpy);
      template_mask |= VisualScreenMask;
    }

  visinfo = XGetVisualInfo (xlib_renderer->xdpy,
                            template_mask,
                            &visinfo_template,
                            &visinfos_count);

  return visinfo;
}
#endif

static gboolean
try_create_context (CoglDisplay *display,
                    int retry_cookie,
                    gboolean *try_fallback,
                    GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglDisplayXlib *xlib_display = display->winsys;
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
#endif
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  EGLDisplay edpy;
  EGLConfig config;
  EGLint config_count = 0;
  EGLBoolean status;
  EGLint cfg_attribs[] = {
    /* NB: This must be the first attribute, since we may
     * try and fallback to no stencil buffer */
    EGL_STENCIL_SIZE,    2,

    EGL_RED_SIZE,        1,
    EGL_GREEN_SIZE,      1,
    EGL_BLUE_SIZE,       1,
    EGL_ALPHA_SIZE,      EGL_DONT_CARE,

    EGL_DEPTH_SIZE,      1,

    EGL_BUFFER_SIZE,     EGL_DONT_CARE,

#if defined (HAVE_COGL_GL)
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#elif defined (HAVE_COGL_GLES2)
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
#endif

    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,

    EGL_NONE
  };
#if defined (HAVE_COGL_GLES2)
  EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
#else
  EGLint *attribs = NULL;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;
#endif
  const char *error_message;

  edpy = egl_renderer->edpy;

#ifdef HAVE_COGL_GL
  eglBindAPI (EGL_OPENGL_API);
#endif

  /* Some GLES hardware can't support a stencil buffer: */
  if (retry_cookie == 1)
    {
      g_warning ("Trying with stencil buffer disabled...");
      cfg_attribs[1 /* EGL_STENCIL_SIZE */] = 0;
    }
  /* XXX: at this point we only have one fallback */

  status = eglChooseConfig (edpy,
                            cfg_attribs,
                            &config, 1,
                            &config_count);
  if (status != EGL_TRUE || config_count == 0)
    {
      error_message = "Unable to find a usable EGL configuration";
      goto fail;
    }

  egl_display->egl_config = config;

  egl_display->egl_context = eglCreateContext (edpy,
                                               config,
                                               EGL_NO_CONTEXT,
                                               attribs);
  if (egl_display->egl_context == EGL_NO_CONTEXT)
    {
      error_message = "Unable to create a suitable EGL context";
      goto fail;
    }

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT

  xvisinfo = get_visual_info (display, config);
  if (xvisinfo == NULL)
    {
      error_message = "Unable to find suitable X visual";
      goto fail;
    }

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
                   CWOverrideRedirect |
                   CWColormap |
                   CWBorderPixel,
                   &attrs);

  XFree (xvisinfo);

  egl_display->dummy_surface =
    eglCreateWindowSurface (edpy,
                            egl_display->egl_config,
                            (NativeWindowType) xlib_display->dummy_xwin,
                            NULL);

  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to create an EGL surface");
      goto fail;
    }

  if (!eglMakeCurrent (edpy,
                       egl_display->dummy_surface,
                       egl_display->dummy_surface,
                       egl_display->egl_context))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to eglMakeCurrent with dummy surface");
      goto fail;
    }

#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)

  egl_display->egl_surface =
    eglCreateWindowSurface (edpy,
                            config,
                            (NativeWindowType) NULL,
                            NULL);
  if (egl_display->egl_surface == EGL_NO_SURFACE)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to create EGL window surface");
      goto fail;
    }

  if (!eglMakeCurrent (egl_renderer->edpy,
                       egl_display->egl_surface,
                       egl_display->egl_surface,
                       egl_display->egl_context))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to eglMakeCurrent with egl surface");
      goto fail;
    }

  eglQuerySurface (egl_renderer->edpy,
                   egl_display->egl_surface,
                   EGL_WIDTH,
                   &egl_display->egl_surface_width);

  eglQuerySurface (egl_renderer->edpy,
                   egl_display->egl_surface,
                   EGL_HEIGHT,
                   &egl_display->egl_surface_height);

#else
#error "Unknown EGL platform"
#endif

  return TRUE;

fail:

  /* Currently we only have one fallback path... */
  if (retry_cookie == 0)
    {
      *try_fallback = TRUE;
      return FALSE;
    }
  else
    {
      *try_fallback = FALSE;
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "%s", error_message);
      return FALSE;
    }
}

static void
cleanup_context (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglDisplayXlib *xlib_display = display->winsys;
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
#endif

  if (egl_display->egl_context != EGL_NO_CONTEXT)
    {
      eglMakeCurrent (egl_renderer->edpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      EGL_NO_CONTEXT);
      eglDestroyContext (egl_renderer->edpy, egl_display->egl_context);
      egl_display->egl_context = EGL_NO_CONTEXT;
    }

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (xlib_display->dummy_xwin)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_display->dummy_xwin);
      xlib_display->dummy_xwin = None;
    }
#endif
}

static gboolean
create_context (CoglDisplay *display, GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  gboolean support_transparent_windows;
  int retry_cookie = 0;
  gboolean status;
  gboolean try_fallback;
  GError *try_error = NULL;

  g_return_val_if_fail (egl_display->egl_context == NULL, TRUE);

  if (display->onscreen_template &&
      display->onscreen_template->swap_chain &&
      display->onscreen_template->swap_chain->has_alpha)
    support_transparent_windows = TRUE;
  else
    support_transparent_windows = FALSE;

  retry_cookie = 0;
  while (!(status = try_create_context (display,
                                        retry_cookie,
                                        &try_fallback,
                                        &try_error)) &&
         try_fallback)
    {
      g_error_free (try_error);
      cleanup_context (display);
      try_error = NULL;
      retry_cookie++;
    }
  if (!status)
    g_propagate_error (error, try_error);

  return status;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_return_if_fail (egl_display != NULL);

  cleanup_context (display);

  g_slice_free (CoglDisplayEGL, display->winsys);
  display->winsys = NULL;
}

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplayEGL *egl_display;

  g_return_val_if_fail (display->winsys == NULL, FALSE);

  egl_display = g_slice_new0 (CoglDisplayEGL);
  display->winsys = egl_display;

  if (!create_context (display, error))
    goto error;

  egl_display->found_egl_config = TRUE;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  context->winsys = g_new0 (CoglContextEGL, 1);

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  cogl_renderer_add_native_filter (context->display->renderer,
                                   event_filter_cb,
                                   context);
#endif
  update_winsys_features (context);

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  cogl_renderer_remove_native_filter (context->display->renderer,
                                      event_filter_cb,
                                      context);
#endif
  g_free (context->winsys);
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererXlib *xlib_renderer = display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen;
  Window xwin;
#endif
  CoglOnscreenEGL *egl_onscreen;

  g_return_val_if_fail (egl_display->egl_context, FALSE);

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT

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
      xerror = _cogl_renderer_xlib_untrap_errors (display->renderer, &state);
      if (status == 0 || xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror,
                         message, sizeof (message));
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

      xvisinfo = get_visual_info (display, egl_display->egl_config);
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

      XMapWindow (xlib_renderer->xdpy, xwin);

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
#endif

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  xlib_onscreen = onscreen->winsys;

  xlib_onscreen->xwin = xwin;
  xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? TRUE : FALSE;

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) xlib_onscreen->xwin,
                            NULL);
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)
  if (egl_display->have_onscreen)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "EGL platform only supports a single onscreen window");
      return FALSE;
    }

  egl_onscreen->egl_surface = egl_display->egl_surface;

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        egl_display->egl_surface_width,
                                        egl_display->egl_surface_height);
  egl_display->have_onscreen = TRUE;
#else
#error "Unknown EGL platform"
#endif

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglRendererXlib *xlib_renderer = context->display->renderer->winsys;
  CoglXlibTrapState old_state;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)
  CoglDisplayEGL *egl_display = context->display->winsys;
#endif
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (egl_onscreen->egl_surface != EGL_NO_SURFACE)
    {
      if (eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface)
          == EGL_FALSE)
        g_warning ("Failed to destroy EGL surface");
      egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
  egl_display->have_onscreen = FALSE;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  _cogl_xlib_trap_errors (&old_state);

  if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
      xlib_onscreen->xwin = None;
    }
  else
    xlib_onscreen->xwin = None;

  XSync (xlib_renderer->xdpy, False);

  if (_cogl_xlib_untrap_errors (&old_state) != Success)
    g_warning ("X Error while destroying X window");
#endif
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextEGL *egl_context = context->winsys;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (egl_context->current_surface == egl_onscreen->egl_surface)
    return;

  if (G_UNLIKELY (!onscreen))
    {
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
      eglMakeCurrent (egl_renderer->edpy,
                      egl_display->dummy_surface,
                      egl_display->dummy_surface,
                      egl_display->egl_context);
      egl_context->current_surface = egl_display->dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)
      return;
#else
#error "Unknown EGL platform"
#endif
    }
  else
    {
      eglMakeCurrent (egl_renderer->edpy,
                      egl_onscreen->egl_surface,
                      egl_onscreen->egl_surface,
                      egl_display->egl_context);
      egl_context->current_surface = egl_onscreen->egl_surface;
    }

  if (onscreen->swap_throttled)
    eglSwapInterval (egl_renderer->edpy, 1);
  else
    eglSwapInterval (egl_renderer->edpy, 0);
}

static void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   int *rectangles,
                                   int n_rectangles)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (egl_renderer->pf_eglSwapBuffersRegion (egl_renderer->edpy,
                                             egl_onscreen->egl_surface,
                                             n_rectangles,
                                             rectangles) == EGL_FALSE)
    g_warning ("Error reported by eglSwapBuffersRegion");
}

static guint32
_cogl_winsys_get_vsync_counter (void)
{
  /* Unsupported feature */
  return 0;
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  eglSwapBuffers (egl_renderer->edpy, egl_onscreen->egl_surface);
}

static guint32
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  return xlib_onscreen->xwin;
}

static unsigned int
_cogl_winsys_onscreen_add_swap_buffers_callback (CoglOnscreen *onscreen,
                                                 CoglSwapBuffersNotify callback,
                                                 void *user_data)
{
  /* Unsupported feature */
  return 0;
}

static void
_cogl_winsys_onscreen_remove_swap_buffers_callback (CoglOnscreen *onscreen,
                                                    unsigned int id)
{
  /* Unsupported feature */
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextEGL *egl_context = context->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (egl_context->current_surface != egl_onscreen->egl_surface)
    return;

  egl_context->current_surface = EGL_NO_SURFACE;
  _cogl_winsys_onscreen_bind (onscreen);
}

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
/* XXX: This is a particularly hacky _cogl_winsys interface... */
static XVisualInfo *
_cogl_winsys_xlib_get_visual_info (void)
{
  CoglDisplayEGL *egl_display;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_return_val_if_fail (ctx->display->winsys, FALSE);

  egl_display = ctx->display->winsys;

  if (!egl_display->found_egl_config)
    return NULL;

  return get_visual_info (ctx->display, egl_display->egl_config);
}
#endif

static EGLDisplay
_cogl_winsys_context_egl_get_egl_display (CoglContext *context)
{
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;

  return egl_renderer->edpy;
}

static CoglWinsysVtable _cogl_winsys_vtable =
  {
    .name = "EGL",
    .get_proc_address = _cogl_winsys_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,
    .context_egl_get_egl_display =
      _cogl_winsys_context_egl_get_egl_display,
#ifdef COGL_HAS_XLIB_SUPPORT
    .xlib_get_visual_info = _cogl_winsys_xlib_get_visual_info,
#endif
    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers,
    .onscreen_swap_region = _cogl_winsys_onscreen_swap_region,
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
    .onscreen_x11_get_window_xid =
      _cogl_winsys_onscreen_x11_get_window_xid,
    .onscreen_add_swap_buffers_callback =
      _cogl_winsys_onscreen_add_swap_buffers_callback,
    .onscreen_remove_swap_buffers_callback =
      _cogl_winsys_onscreen_remove_swap_buffers_callback,
    .get_vsync_counter = _cogl_winsys_get_vsync_counter
  };

/* XXX: we use a function because no doubt someone will complain
 * about using c99 member initializers because they aren't portable
 * to windows. We want to avoid having to rigidly follow the real
 * order of members since some members are #ifdefd and we'd have
 * to mirror the #ifdefing to add padding etc. For any winsys that
 * can assume the platform has a sane compiler then we can just use
 * c99 initializers for insane platforms they can initialize
 * the members by name in a function.
 */
const CoglWinsysVtable *
_cogl_winsys_egl_get_vtable (void)
{
  return &_cogl_winsys_vtable;
}
