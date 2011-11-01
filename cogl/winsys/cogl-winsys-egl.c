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

#include "cogl-util.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-onscreen-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-onscreen-template-private.h"
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
#include "cogl-xlib-renderer-private.h"
#include "cogl-xlib-display-private.h"
#include "cogl-xlib-renderer.h"
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
#include "cogl-texture-pixmap-x11-private.h"
#include "cogl-texture-2d-private.h"
#endif

#include "cogl-private.h"

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include <wayland-client.h>
#include <wayland-egl.h>
#endif

#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include <android/native_window.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
#include <X11/Xlib.h>

#define COGL_ONSCREEN_X11_EVENT_MASK StructureNotifyMask
#endif

#define MAX_EGL_CONFIG_ATTRIBS 30

typedef enum _CoglEGLWinsysFeature
{
  COGL_EGL_WINSYS_FEATURE_SWAP_REGION                   =1L<<0,
  COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP     =1L<<1,
  COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_WAYLAND_BUFFER =1L<<2
} CoglEGLWinsysFeature;

typedef struct _CoglRendererEGL
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglXlibRenderer _parent;
#endif

  CoglEGLWinsysFeature private_features;

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  struct wl_display *wayland_display;
  struct wl_compositor *wayland_compositor;
  struct wl_shell *wayland_shell;
#endif

  EGLDisplay edpy;

  EGLint egl_version_major;
  EGLint egl_version_minor;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  gboolean gdl_initialized;
#endif

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d)

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
  CoglXlibDisplay _parent;
#endif

  EGLContext egl_context;
#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT)
  EGLSurface dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  struct wl_surface *wayland_surface;
  struct wl_egl_window *wayland_egl_native_window;
  EGLSurface dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT)
  EGLSurface egl_surface;
  int egl_surface_width;
  int egl_surface_height;
  gboolean have_onscreen;
#else
#error "Unknown EGL platform"
#endif

  EGLConfig egl_config;
  gboolean found_egl_config;
  gboolean stencil_disabled;
} CoglDisplayEGL;

typedef struct _CoglContextEGL
{
  EGLSurface current_surface;
} CoglContextEGL;

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
typedef struct _CoglOnscreenXlib
{
  Window xwin;
  gboolean is_foreign_xwin;
} CoglOnscreenXlib;
#endif

typedef struct _CoglOnscreenEGL
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglOnscreenXlib _parent;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  struct wl_egl_window *wayland_egl_native_window;
  struct wl_surface *wayland_surface;
#endif

  EGLSurface egl_surface;
} CoglOnscreenEGL;

#ifdef EGL_KHR_image_pixmap
typedef struct _CoglTexturePixmapEGL
{
  EGLImageKHR image;
  CoglTexture *texture;
} CoglTexturePixmapEGL;
#endif

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  egl_private_flags)                    \
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
                                  egl_private_flags)                    \
  { 255, 255, 0, namespaces, extension_names,                           \
      0, egl_private_flags,                                             \
      0,                                                                \
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
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  void *ptr;

  ptr = eglGetProcAddress (name);

  /* eglGetProcAddress doesn't support fetching core API so we need to
     get that separately with GModule */
  if (ptr == NULL)
    g_module_symbol (renderer->libgl_module, name, &ptr);

  return ptr;
}

#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
static ANativeWindow *android_native_window;

void
cogl_android_set_native_window (ANativeWindow *window)
{
  _cogl_init ();

  android_native_window = window;
}
#endif

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
event_filter_cb (XEvent *xevent, void *data)
{
  CoglContext *context = data;

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

  return COGL_FILTER_CONTINUE;
}
#endif /* COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT */

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  if (egl_renderer->gdl_initialized)
    gdl_close ();
#endif

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  _cogl_xlib_renderer_disconnect (renderer);
#endif

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT

static void
display_handle_global_cb (struct wl_display *display,
                          uint32_t id,
                          const char *interface,
                          uint32_t version,
                          void *data)
{
  CoglRendererEGL *egl_renderer = (CoglRendererEGL *)data;

  if (strcmp (interface, "wl_compositor") == 0)
    egl_renderer->wayland_compositor =
      wl_display_bind (display, id, &wl_compositor_interface);
  else if (strcmp(interface, "wl_shell") == 0)
    egl_renderer->wayland_shell =
      wl_display_bind (display, id, &wl_shell_interface);
}

#endif

/* Updates all the function pointers */
static void
check_egl_extensions (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  const char *egl_extensions;
  int i;

  egl_extensions = eglQueryString (egl_renderer->edpy, EGL_EXTENSIONS);

  COGL_NOTE (WINSYS, "  EGL Extensions: %s", egl_extensions);

  egl_renderer->private_features = 0;
  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check (renderer,
                             "EGL", winsys_feature_data + i, 0, 0,
                             COGL_DRIVER_GL, /* the driver isn't used */
                             egl_extensions,
                             egl_renderer))
      {
        egl_renderer->private_features |=
          winsys_feature_data[i].feature_flags_private;
      }
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererEGL *egl_renderer;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglXlibRenderer *xlib_renderer;
#endif
  EGLBoolean status;
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  gdl_ret_t rc = GDL_SUCCESS;
  gdl_display_info_t gdl_display_info;
#endif

  renderer->winsys = g_slice_new0 (CoglRendererEGL);

  egl_renderer = renderer->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  xlib_renderer = renderer->winsys;

  if (!_cogl_xlib_renderer_connect (renderer, error))
    goto error;

  egl_renderer->edpy =
    eglGetDisplay ((NativeDisplayType) xlib_renderer->xdpy);

  status = eglInitialize (egl_renderer->edpy,
                          &egl_renderer->egl_version_major,
                          &egl_renderer->egl_version_minor);

#elif defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)

  /* The EGL API doesn't provide for a way to explicitly select a
   * platform when the driver can support multiple. Mesa allows
   * selection using an environment variable though so that's what
   * we're doing here... */
  setenv("EGL_PLATFORM", "wayland", 1);

  if (renderer->foreign_wayland_display)
    {
      egl_renderer->wayland_display = renderer->foreign_wayland_display;
      /* XXX: For now we have to assume that if a foreign display is
       * given then so is a foreing compositor because there is no way
       * to retrospectively be notified of the compositor. */
      g_assert (renderer->foreign_wayland_compositor);
      egl_renderer->wayland_compositor = renderer->foreign_wayland_compositor;
    }
  else
    {
      egl_renderer->wayland_display = wl_display_connect (NULL);
      if (!egl_renderer->wayland_display)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to connect wayland display");
          goto error;
        }

      wl_display_add_global_listener (egl_renderer->wayland_display,
                                      display_handle_global_cb,
                                      egl_renderer);
    }

  /*
   * Ensure that that we've received the messages setting up the compostor and
   * shell object. This is better than just wl_display_iterate since it will
   * always ensure that something is available to be read
   */
  while (!(egl_renderer->wayland_compositor && egl_renderer->wayland_shell))
    wl_display_roundtrip (egl_renderer->wayland_display);

  egl_renderer->edpy =
    eglGetDisplay ((EGLNativeDisplayType)egl_renderer->wayland_display);

  status = eglInitialize (egl_renderer->edpy,
			  &egl_renderer->egl_version_major,
			  &egl_renderer->egl_version_minor);

#else
  egl_renderer->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

  status = eglInitialize (egl_renderer->edpy,
			  &egl_renderer->egl_version_major,
			  &egl_renderer->egl_version_minor);
#endif

  if (status != EGL_TRUE)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to initialize EGL");
      goto error;
    }

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  /* Check we can talk to the GDL library */

  rc = gdl_init (NULL);
  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GDL initialize failed. %s",
                   gdl_get_error_string (rc));
      goto error;
    }

  rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &gdl_display_info);
  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GDL failed to get display information: %s",
                   gdl_get_error_string (rc));
      gdl_close ();
      goto error;
    }

  gdl_close ();
#endif

  check_egl_extensions (renderer);

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static gboolean
update_winsys_features (CoglContext *context, GError **error)
{
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  check_egl_extensions (context->display->renderer);

  if (!_cogl_context_update_features (context, error))
    return FALSE;

#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT) || \
    defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);
#endif

  if (egl_renderer->private_features & COGL_EGL_WINSYS_FEATURE_SWAP_REGION)
    {
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);
    }

  return TRUE;
}

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
static XVisualInfo *
get_visual_info (CoglDisplay *display, EGLConfig egl_config)
{
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
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

static void
egl_attributes_from_framebuffer_config (CoglDisplay *display,
                                        CoglFramebufferConfig *config,
                                        gboolean needs_stencil_override,
                                        EGLint *attributes)
{
  int i = 0;

  attributes[i++] = EGL_STENCIL_SIZE;
  attributes[i++] = needs_stencil_override ? 2 : 0;

  attributes[i++] = EGL_RED_SIZE;
  attributes[i++] = 1;
  attributes[i++] = EGL_GREEN_SIZE;
  attributes[i++] = 1;
  attributes[i++] = EGL_BLUE_SIZE;
  attributes[i++] = 1;

  attributes[i++] = EGL_ALPHA_SIZE;
  attributes[i++] = config->swap_chain->has_alpha ? 1 : EGL_DONT_CARE;

  attributes[i++] = EGL_DEPTH_SIZE;
  attributes[i++] = 1;

  /* XXX: Why does the GDL platform choose these by default? */
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  attributes[i++] = EGL_BIND_TO_TEXTURE_RGBA;
  attributes[i++] = EGL_TRUE;
  attributes[i++] = EGL_BIND_TO_TEXTURE_RGB;
  attributes[i++] = EGL_TRUE;
#endif

  attributes[i++] = EGL_BUFFER_SIZE;
  attributes[i++] = EGL_DONT_CARE;

  attributes[i++] = EGL_RENDERABLE_TYPE;
  attributes[i++] = (display->renderer->driver == COGL_DRIVER_GL ?
                      EGL_OPENGL_BIT :
                      display->renderer->driver == COGL_DRIVER_GLES1 ?
                      EGL_OPENGL_ES_BIT :
                      EGL_OPENGL_ES2_BIT);

  attributes[i++] = EGL_SURFACE_TYPE;
  attributes[i++] = EGL_WINDOW_BIT;

  if (config->samples_per_pixel)
    {
       attributes[i++] = EGL_SAMPLE_BUFFERS;
       attributes[i++] = 1;
       attributes[i++] = EGL_SAMPLES;
       attributes[i++] = config->samples_per_pixel;
    }

  attributes[i++] = EGL_NONE;

  g_assert (i < MAX_EGL_CONFIG_ATTRIBS);
}

static gboolean
try_create_context (CoglDisplay *display,
                    gboolean with_stencil_buffer,
                    GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglXlibDisplay *xlib_display = display->winsys;
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
#endif
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  EGLDisplay edpy;
  EGLConfig config;
  EGLint config_count = 0;
  EGLBoolean status;
  EGLint cfg_attribs[MAX_EGL_CONFIG_ATTRIBS];
  EGLint attribs[3];
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;
#endif
  const char *error_message;

  egl_attributes_from_framebuffer_config (display,
                                          &display->onscreen_template->config,
                                          with_stencil_buffer,
                                          cfg_attribs);

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context == NULL, TRUE);

  edpy = egl_renderer->edpy;

  if (display->renderer->driver == COGL_DRIVER_GL)
    eglBindAPI (EGL_OPENGL_API);

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

  if (display->renderer->driver == COGL_DRIVER_GLES2)
    {
      attribs[0] = EGL_CONTEXT_CLIENT_VERSION;
      attribs[1] = 2;
      attribs[2] = EGL_NONE;
    }
  else
    attribs[0] = EGL_NONE;

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
      error_message = "Unable to create an EGL surface";
      goto fail;
    }

  if (!eglMakeCurrent (edpy,
                       egl_display->dummy_surface,
                       egl_display->dummy_surface,
                       egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with dummy surface";
      goto fail;
    }

#elif defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)

  egl_display->wayland_surface =
    wl_compositor_create_surface (egl_renderer->wayland_compositor);
  if (!egl_display->wayland_surface)
    {
      error_message= "Failed to create a dummy wayland surface";
      goto fail;
    }

  egl_display->wayland_egl_native_window =
    wl_egl_window_create (egl_display->wayland_surface,
                          1,
                          1);
  if (!egl_display->wayland_egl_native_window)
    {
      error_message= "Failed to create a dummy wayland native egl surface";
      goto fail;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (edpy,
                            egl_display->egl_config,
                            (EGLNativeWindowType)
                            egl_display->wayland_egl_native_window,
                            NULL);
  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      error_message= "Unable to eglMakeCurrent with dummy surface";
      goto fail;
    }

  if (!eglMakeCurrent (edpy,
                       egl_display->dummy_surface,
                       egl_display->dummy_surface,
                       egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with dummy surface";
      goto fail;
    }

#elif defined (COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT)
  {
    EGLint format;

    if (android_native_window == NULL)
      {
        error_message = "No ANativeWindow window specified with "
          "cogl_android_set_native_window()";
        goto fail;
      }

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry ().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib (edpy, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry (android_native_window,
                                      0,
                                      0,
                                      format);

    egl_display->egl_surface =
      eglCreateWindowSurface (edpy,
                              config,
                              (NativeWindowType) android_native_window,
                              NULL);
    if (egl_display->egl_surface == EGL_NO_SURFACE)
      {
        error_message = "Unable to create EGL window surface";
        goto fail;
      }

    if (!eglMakeCurrent (egl_renderer->edpy,
                         egl_display->egl_surface,
                         egl_display->egl_surface,
                         egl_display->egl_context))
      {
        error_message = "Unable to eglMakeCurrent with egl surface";
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
  }

#elif defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT)

  egl_display->egl_surface =
    eglCreateWindowSurface (edpy,
                            config,
                            (NativeWindowType) display->gdl_plane,
                            NULL);

  if (egl_display->egl_surface == EGL_NO_SURFACE)
    {
      error_message = "Unable to create EGL window surface";
      goto fail;
    }

  if (!eglMakeCurrent (egl_renderer->edpy,
                       egl_display->egl_surface,
                       egl_display->egl_surface,
                       egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with egl surface";
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

#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)

  egl_display->egl_surface =
    eglCreateWindowSurface (edpy,
                            config,
                            (NativeWindowType) NULL,
                            NULL);
  if (egl_display->egl_surface == EGL_NO_SURFACE)
    {
      error_message = "Unable to create EGL window surface";
      goto fail;
    }

  if (!eglMakeCurrent (egl_renderer->edpy,
                       egl_display->egl_surface,
                       egl_display->egl_surface,
                       egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with egl surface";
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

  g_set_error (error, COGL_WINSYS_ERROR,
               COGL_WINSYS_ERROR_CREATE_CONTEXT,
               "%s", error_message);
  return FALSE;
}

static void
cleanup_context (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  CoglXlibDisplay *xlib_display = display->winsys;
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
#endif

  if (egl_display->egl_context != EGL_NO_CONTEXT)
    {
      eglMakeCurrent (egl_renderer->edpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      EGL_NO_CONTEXT);
      eglDestroyContext (egl_renderer->edpy, egl_display->egl_context);
      egl_display->egl_context = EGL_NO_CONTEXT;
    }

#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) || \
    defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT)
  if (egl_display->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->egl_surface);
      egl_display->egl_surface = EGL_NO_SURFACE;
    }
#elif COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
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
#elif defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (egl_display->wayland_egl_native_window)
    {
      wl_egl_window_destroy (egl_display->wayland_egl_native_window);
      egl_display->wayland_egl_native_window = NULL;
    }

  if (egl_display->wayland_surface)
    {
      wl_surface_destroy (egl_display->wayland_surface);
      egl_display->wayland_surface = NULL;
    }
#endif
}

static gboolean
create_context (CoglDisplay *display, GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;

  /* Note: we don't just rely on eglChooseConfig to correctly
   * report that the driver doesn't support a stencil buffer
   * because we've seen PVR drivers that claim stencil buffer
   * support according to the EGLConfig but then later fail
   * when trying to create a context with such a config.
   */
  if (try_create_context (display, TRUE, error))
    {
      egl_display->stencil_disabled = FALSE;
      return TRUE;
    }
  else
    {
      g_clear_error (error);
      cleanup_context (display);
      egl_display->stencil_disabled = TRUE;
      return try_create_context (display, FALSE, error);
    }
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  _COGL_RETURN_IF_FAIL (egl_display != NULL);

  cleanup_context (display);

  g_slice_free (CoglDisplayEGL, display->winsys);
  display->winsys = NULL;
}

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
static gboolean
gdl_plane_init (CoglDisplay *display, GError **error)
{
  gboolean ret = TRUE;
  gdl_color_space_t colorSpace = GDL_COLOR_SPACE_RGB;
  gdl_pixel_format_t pixfmt = GDL_PF_ARGB_32;
  gdl_rectangle_t dstRect;
  gdl_display_info_t display_info;
  gdl_ret_t rc = GDL_SUCCESS;

  if (!display->gdl_plane)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "No GDL plane specified with "
                   "cogl_gdl_display_set_plane");
      return FALSE;
    }

  rc = gdl_init (NULL);
  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL initialize failed. %s", gdl_get_error_string (rc));
      return FALSE;
    }

  rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &display_info);
  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL failed to get display infomation: %s",
                   gdl_get_error_string (rc));
      gdl_close ();
      return FALSE;
    }

  dstRect.origin.x = 0;
  dstRect.origin.y = 0;
  dstRect.width = display_info.tvmode.width;
  dstRect.height = display_info.tvmode.height;

  /* Configure the plane attribute. */
  rc = gdl_plane_reset (display->gdl_plane);
  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_begin (display->gdl_plane);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_PIXEL_FORMAT, &pixfmt);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_DST_RECT, &dstRect);

  /* Default to triple buffering if the swap_chain doesn't have an explicit
   * length */
  if (rc == GDL_SUCCESS)
    {
      if (display->onscreen_template->swap_chain &&
          display->onscreen_template->swap_chain->length != -1)
        rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES,
                                 display->onscreen_template->swap_chain->length);
      else
        rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES, 3);
    }

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_end (GDL_FALSE);
  else
    gdl_plane_config_end (GDL_TRUE);

  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL configuration failed: %s.", gdl_get_error_string (rc));
      ret = FALSE;
    }

  gdl_close ();

  return TRUE;
}
#endif

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplayEGL *egl_display;
#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
#endif

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  egl_display = g_slice_new0 (CoglDisplayEGL);
  display->winsys = egl_display;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  if (!gdl_plane_init (display, error))
    goto error;
#endif

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
  if (display->wayland_compositor_display)
    {
      struct wl_display *wayland_display = display->wayland_compositor_display;
      egl_renderer->pf_eglBindWaylandDisplay (egl_renderer->edpy,
                                              wayland_display);
    }
#endif

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
  cogl_xlib_renderer_add_filter (context->display->renderer,
                                 event_filter_cb,
                                 context);
#endif
  return update_winsys_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
  cogl_xlib_renderer_remove_filter (context->display->renderer,
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
  CoglXlibRenderer *xlib_renderer = display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen;
  Window xwin;
#endif
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
#endif
  CoglOnscreenEGL *egl_onscreen;
  EGLint attributes[MAX_EGL_CONFIG_ATTRIBS];
  EGLConfig egl_config;
  EGLint config_count = 0;
  EGLBoolean status;
  gboolean need_stencil =
    egl_display->stencil_disabled ? FALSE : framebuffer->config.need_stencil;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  egl_attributes_from_framebuffer_config (display,
                                          &framebuffer->config,
                                          need_stencil,
                                          attributes);

  status = eglChooseConfig (egl_renderer->edpy,
                            attributes,
                            &egl_config, 1,
                            &config_count);
  if (status != EGL_TRUE || config_count == 0)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to find a suitable EGL configuration");
      return FALSE;
    }

  /* Update the real number of samples_per_pixel now that we have
   * found an egl_config... */
  if (framebuffer->config.samples_per_pixel)
    {
      EGLint samples;
      status = eglGetConfigAttrib (egl_renderer->edpy,
                                   egl_config,
                                   EGL_SAMPLES, &samples);
      g_return_val_if_fail (status == EGL_TRUE, TRUE);
      framebuffer->samples_per_pixel = samples;
    }

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

      _cogl_xlib_renderer_trap_errors (display->renderer, &state);

      status = XGetWindowAttributes (xlib_renderer->xdpy, xwin, &attr);
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
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

      /* Make sure the app selects for the events we require... */
      onscreen->foreign_update_mask_callback (onscreen,
                                              COGL_ONSCREEN_X11_EVENT_MASK,
                                              onscreen->foreign_update_mask_data);
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

      _cogl_xlib_renderer_trap_errors (display->renderer, &state);

      xvisinfo = get_visual_info (display, egl_config);
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
      xattr.event_mask = COGL_ONSCREEN_X11_EVENT_MASK;

      mask = CWBorderPixel | CWColormap | CWEventMask;

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
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
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
                            egl_config,
                            (NativeWindowType) xlib_onscreen->xwin,
                            NULL);
#elif defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)

  egl_onscreen->wayland_surface =
    wl_compositor_create_surface (egl_renderer->wayland_compositor);
  if (!egl_onscreen->wayland_surface)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Error while creating wayland surface for CoglOnscreen");
      return FALSE;
    }

  egl_onscreen->wayland_egl_native_window =
    wl_egl_window_create (egl_onscreen->wayland_surface,
                          cogl_framebuffer_get_width (framebuffer),
                          cogl_framebuffer_get_height (framebuffer));
  if (!egl_onscreen->wayland_egl_native_window)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Error while creating wayland egl native window "
                   "for CoglOnscreen");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_config,
                            (EGLNativeWindowType)
                            egl_onscreen->wayland_egl_native_window,
                            NULL);

  wl_shell_set_toplevel (egl_renderer->wayland_shell,
                         egl_onscreen->wayland_surface);

#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT)      || \
      defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT)
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
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglXlibTrapState old_state;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT)
  CoglDisplayEGL *egl_display = context->display->winsys;
#endif
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

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
  _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

  if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
      xlib_onscreen->xwin = None;
    }
  else
    xlib_onscreen->xwin = None;

  XSync (xlib_renderer->xdpy, False);

  if (_cogl_xlib_renderer_untrap_errors (context->display->renderer,
                                         &old_state) != Success)
    g_warning ("X Error while destroying X window");
#endif

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  if (egl_onscreen->wayland_egl_native_window)
    {
      wl_egl_window_destroy (egl_onscreen->wayland_egl_native_window);
      egl_onscreen->wayland_egl_native_window = NULL;
    }

  if (egl_onscreen->wayland_surface)
    {
      wl_surface_destroy (egl_onscreen->wayland_surface);
      egl_onscreen->wayland_surface = NULL;
    }
#endif

  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
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
#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT) || \
    defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
      eglMakeCurrent (egl_renderer->edpy,
                      egl_display->dummy_surface,
                      egl_display->dummy_surface,
                      egl_display->egl_context);
      egl_context->current_surface = egl_display->dummy_surface;
#elif defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) || \
      defined (COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT)      || \
      defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT)
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
                                   const int *user_rectangles,
                                   int n_rectangles)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  int framebuffer_height  = cogl_framebuffer_get_height (framebuffer);
  int *rectangles = g_alloca (sizeof (int) * n_rectangles * 4);
  int i;

  /* eglSwapBuffersRegion expects rectangles relative to the bottom left corner
   * but we are given rectangles relative to the top left so we need to flip
   * them... */
  memcpy (rectangles, user_rectangles, sizeof (int) * n_rectangles * 4);
  for (i = 0; i < n_rectangles; i++)
    {
      int *rect = &rectangles[4 * i];
      rect[1] = framebuffer_height - rect[1] - rect[3];
    }

  if (egl_renderer->pf_eglSwapBuffersRegion (egl_renderer->edpy,
                                             egl_onscreen->egl_surface,
                                             n_rectangles,
                                             rectangles) == EGL_FALSE)
    g_warning ("Error reported by eglSwapBuffersRegion");
}

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglXlibRenderer *xlib_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;

  if (visibility)
    XMapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
  else
    XUnmapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
}
#endif

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  eglSwapBuffers (egl_renderer->edpy, egl_onscreen->egl_surface);

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  /*
   * The implementation of eglSwapBuffers may do a flush however the semantics
   * of eglSwapBuffers on Wayland has changed in the past. So to be safe to
   * the implementation changing we should explicitly ensure all messages are
   * sent.
   */
  wl_display_flush (egl_renderer->wayland_display);
#endif
}

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
static guint32
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  return xlib_onscreen->xwin;
}
#endif

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

  _COGL_RETURN_VAL_IF_FAIL (ctx->display->winsys, FALSE);

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

#if defined (COGL_HAS_XLIB_SUPPORT) && defined (EGL_KHR_image_pixmap)
static gboolean
_cogl_winsys_texture_pixmap_x11_create (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapEGL *egl_tex_pixmap;
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  CoglPixelFormat texture_format;
  CoglRendererEGL *egl_renderer;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, FALSE);

  egl_renderer = ctx->display->renderer->winsys;

  if (!(egl_renderer->private_features &
        COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP) ||
      !(ctx->private_feature_flags &
        COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE))
    {
      tex_pixmap->winsys = NULL;
      return FALSE;
    }

  egl_tex_pixmap = g_new0 (CoglTexturePixmapEGL, 1);

  egl_tex_pixmap->image =
    _cogl_egl_create_image (ctx,
                            EGL_NATIVE_PIXMAP_KHR,
                            (EGLClientBuffer)tex_pixmap->pixmap,
                            attribs);
  if (egl_tex_pixmap->image == EGL_NO_IMAGE_KHR)
    {
      g_free (egl_tex_pixmap);
      return FALSE;
    }

  texture_format = (tex_pixmap->depth >= 32 ?
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE :
                    COGL_PIXEL_FORMAT_RGB_888);

  egl_tex_pixmap->texture = COGL_TEXTURE (
    _cogl_egl_texture_2d_new_from_image (ctx,
                                         tex_pixmap->width,
                                         tex_pixmap->height,
                                         texture_format,
                                         egl_tex_pixmap->image,
                                         NULL));

  tex_pixmap->winsys = egl_tex_pixmap;

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapEGL *egl_tex_pixmap;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!tex_pixmap->winsys)
    return;

  egl_tex_pixmap = tex_pixmap->winsys;

  if (egl_tex_pixmap->texture)
    cogl_object_unref (egl_tex_pixmap->texture);

  if (egl_tex_pixmap->image != EGL_NO_IMAGE_KHR)
    _cogl_egl_destroy_image (ctx, egl_tex_pixmap->image);

  tex_pixmap->winsys = NULL;
  g_free (egl_tex_pixmap);
}

static gboolean
_cogl_winsys_texture_pixmap_x11_update (CoglTexturePixmapX11 *tex_pixmap,
                                        gboolean needs_mipmap)
{
  if (needs_mipmap)
    return FALSE;

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_damage_notify (CoglTexturePixmapX11 *tex_pixmap)
{
}

static CoglHandle
_cogl_winsys_texture_pixmap_x11_get_texture (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapEGL *egl_tex_pixmap = tex_pixmap->winsys;

  return egl_tex_pixmap->texture;
}
#endif /* defined (COGL_HAS_XLIB_SUPPORT) && defined (EGL_KHR_image_pixmap) */


static CoglWinsysVtable _cogl_winsys_vtable =
  {
    .id = COGL_WINSYS_ID_EGL,
    .name = "EGL",
    .renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,
    .context_egl_get_egl_display =
      _cogl_winsys_context_egl_get_egl_display,
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
    .xlib_get_visual_info = _cogl_winsys_xlib_get_visual_info,
#endif
    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers,
    .onscreen_swap_region = _cogl_winsys_onscreen_swap_region,
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
    .onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility,
#endif
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT
    .onscreen_x11_get_window_xid =
      _cogl_winsys_onscreen_x11_get_window_xid,
#endif
#if defined (COGL_HAS_XLIB_SUPPORT) && defined (EGL_KHR_image_pixmap)
    /* X11 tfp support... */
    /* XXX: instead of having a rather monolithic winsys vtable we could
     * perhaps look for a way to separate these... */
    .texture_pixmap_x11_create =
      _cogl_winsys_texture_pixmap_x11_create,
    .texture_pixmap_x11_free =
      _cogl_winsys_texture_pixmap_x11_free,
    .texture_pixmap_x11_update =
      _cogl_winsys_texture_pixmap_x11_update,
    .texture_pixmap_x11_damage_notify =
      _cogl_winsys_texture_pixmap_x11_damage_notify,
    .texture_pixmap_x11_get_texture =
      _cogl_winsys_texture_pixmap_x11_get_texture,
#endif /* defined (COGL_HAS_XLIB_SUPPORT) && defined (EGL_KHR_image_pixmap) */
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

/* FIXME: we should have a separate wayland file for these entry
 * points... */
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
void
cogl_wayland_renderer_set_foreign_display (CoglRenderer *renderer,
                                           struct wl_display *display)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_wayland_display = display;
}

struct wl_display *
cogl_wayland_renderer_get_display (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  if (renderer->foreign_wayland_display)
    return renderer->foreign_wayland_display;
  else if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      return egl_renderer->wayland_display;
    }
  else
    return NULL;
}

void
cogl_wayland_renderer_set_foreign_compositor (CoglRenderer *renderer,
                                              struct wl_compositor *compositor)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_wayland_compositor = compositor;
}

struct wl_compositor *
cogl_wayland_renderer_get_compositor (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  if (renderer->foreign_wayland_compositor)
    return renderer->foreign_wayland_compositor;
  else if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      return egl_renderer->wayland_compositor;
    }
  else
    return NULL;
}

struct wl_surface *
cogl_wayland_onscreen_get_surface (CoglOnscreen *onscreen)
{
  CoglFramebuffer *fb;

  fb = COGL_FRAMEBUFFER (onscreen);
  if (fb->allocated)
    {
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      return egl_onscreen->wayland_surface;
    }
  else
    return NULL;
}

#endif /* COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT */

#ifdef EGL_KHR_image_base
EGLImageKHR
_cogl_egl_create_image (CoglContext *ctx,
                        EGLenum target,
                        EGLClientBuffer buffer,
                        const EGLint *attribs)
{
  CoglDisplayEGL *egl_display = ctx->display->winsys;
  CoglRendererEGL *egl_renderer = ctx->display->renderer->winsys;
  EGLContext egl_ctx;

  _COGL_RETURN_VAL_IF_FAIL (egl_renderer->pf_eglCreateImage, EGL_NO_IMAGE_KHR);

  /* The EGL_KHR_image_pixmap spec explicitly states that EGL_NO_CONTEXT must
   * always be used in conjunction with the EGL_NATIVE_PIXMAP_KHR target */
#ifdef EGL_KHR_image_pixmap
  if (target == EGL_NATIVE_PIXMAP_KHR)
    egl_ctx = EGL_NO_CONTEXT;
  else
#endif
    egl_ctx = egl_display->egl_context;

  return egl_renderer->pf_eglCreateImage (egl_renderer->edpy,
                                          egl_ctx,
                                          target,
                                          buffer,
                                          attribs);
}

void
_cogl_egl_destroy_image (CoglContext *ctx,
                         EGLImageKHR image)
{
  CoglRendererEGL *egl_renderer = ctx->display->renderer->winsys;

  _COGL_RETURN_IF_FAIL (egl_renderer->pf_eglDestroyImage);

  egl_renderer->pf_eglDestroyImage (egl_renderer->edpy, image);
}
#endif
