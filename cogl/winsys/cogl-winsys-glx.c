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

#include "cogl-util.h"
#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-glx-renderer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-glx-display-private.h"
#include "cogl-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-xlib-renderer.h"
#include "cogl-util.h"
#include "cogl-winsys-glx-private.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

#define COGL_ONSCREEN_X11_EVENT_MASK StructureNotifyMask
#define MAX_GLX_CONFIG_ATTRIBS 30

typedef struct _CoglContextGLX
{
  GLXDrawable current_drawable;
} CoglContextGLX;

typedef struct _CoglOnscreenXlib
{
  Window xwin;
  CoglBool is_foreign_xwin;
} CoglOnscreenXlib;

typedef struct _CoglOnscreenGLX
{
  CoglOnscreenXlib _parent;
  GLXDrawable glxwin;
  uint32_t last_swap_vsync_counter;
  CoglBool pending_swap_notify;
  CoglBool pending_resize_notify;
} CoglOnscreenGLX;

typedef struct _CoglTexturePixmapGLX
{
  GLXPixmap glx_pixmap;
  CoglBool has_mipmap_space;
  CoglBool can_mipmap;

  CoglTexture *glx_tex;

  CoglBool bind_tex_image_queued;
  CoglBool pixmap_bound;
} CoglTexturePixmapGLX;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  static const CoglFeatureFunction                                      \
  cogl_glx_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglGLXRenderer, pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-glx-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  { 255, 255, 0, namespaces, extension_names,                           \
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

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  /* The GLX_ARB_get_proc_address extension documents that this should
   * work for core functions too so we don't need to do anything
   * special with in_core */

  return glx_renderer->glXGetProcAddress ((const GLubyte *) name);
}

static CoglOnscreen *
find_onscreen_for_xid (CoglContext *context, uint32_t xid)
{
  GList *l;

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;
      CoglOnscreenXlib *xlib_onscreen;

      if (framebuffer->type != COGL_FRAMEBUFFER_TYPE_ONSCREEN)
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
  CoglOnscreen *onscreen = find_onscreen_for_xid (context, (uint32_t)drawable);
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;
  CoglOnscreenGLX *glx_onscreen;

  if (!onscreen)
    return;

  glx_onscreen = onscreen->winsys;

  /* We only want to notify that the swap is complete when the
     application calls cogl_context_dispatch so instead of immediately
     notifying we'll set a flag to remember to notify later */
  glx_display->pending_swap_notify = TRUE;
  glx_onscreen->pending_swap_notify = TRUE;
}

static void
notify_resize (CoglContext *context,
               GLXDrawable drawable,
               int width,
               int height)
{
  CoglOnscreen *onscreen = find_onscreen_for_xid (context, drawable);
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;
  CoglOnscreenGLX *glx_onscreen;

  if (!onscreen)
    return;

  glx_onscreen = onscreen->winsys;

  _cogl_framebuffer_winsys_update_size (framebuffer, width, height);

  /* We only want to notify that a resize happened when the
     application calls cogl_context_dispatch so instead of immediately
     notifying we'll set a flag to remember to notify later */
  glx_display->pending_resize_notify = TRUE;
  glx_onscreen->pending_resize_notify = TRUE;
}

static CoglFilterReturn
glx_event_filter_cb (XEvent *xevent, void *data)
{
  CoglContext *context = data;
#ifdef GLX_INTEL_swap_event
  CoglGLXRenderer *glx_renderer;
#endif

  if (xevent->type == ConfigureNotify)
    {
      notify_resize (context,
                     xevent->xconfigure.window,
                     xevent->xconfigure.width,
                     xevent->xconfigure.height);

      /* we let ConfigureNotify pass through */
      return COGL_FILTER_CONTINUE;
    }

#ifdef GLX_INTEL_swap_event
  glx_renderer = context->display->renderer->winsys;

  if (xevent->type == (glx_renderer->glx_event_base + GLX_BufferSwapComplete))
    {
      GLXBufferSwapComplete *swap_event = (GLXBufferSwapComplete *) xevent;

      notify_swap_buffers (context, swap_event->drawable);

      /* remove SwapComplete events from the queue */
      return COGL_FILTER_REMOVE;
    }
#endif /* GLX_INTEL_swap_event */

  return COGL_FILTER_CONTINUE;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  _cogl_xlib_renderer_disconnect (renderer);

  if (glx_renderer->libgl_module)
    g_module_close (glx_renderer->libgl_module);

  g_slice_free (CoglGLXRenderer, renderer->winsys);
}

static CoglBool
resolve_core_glx_functions (CoglRenderer *renderer,
                            GError **error)
{
  CoglGLXRenderer *glx_renderer;

  glx_renderer = renderer->winsys;

  if (!g_module_symbol (glx_renderer->libgl_module, "glXCreatePixmap",
                        (void **) &glx_renderer->glXCreatePixmap) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXDestroyPixmap",
                        (void **) &glx_renderer->glXDestroyPixmap) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXChooseFBConfig",
                        (void **) &glx_renderer->glXChooseFBConfig) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXCreateNewContext",
                        (void **) &glx_renderer->glXCreateNewContext) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXGetFBConfigAttrib",
                        (void **) &glx_renderer->glXGetFBConfigAttrib) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryVersion",
                        (void **) &glx_renderer->glXQueryVersion) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXDestroyContext",
                        (void **) &glx_renderer->glXDestroyContext) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXMakeContextCurrent",
                        (void **) &glx_renderer->glXMakeContextCurrent) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXSwapBuffers",
                        (void **) &glx_renderer->glXSwapBuffers) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryExtension",
                        (void **) &glx_renderer->glXQueryExtension) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXIsDirect",
                        (void **) &glx_renderer->glXIsDirect) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXGetVisualFromFBConfig",
                        (void **) &glx_renderer->glXGetVisualFromFBConfig) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXSelectEvent",
                        (void **) &glx_renderer->glXSelectEvent) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXCreateWindow",
                        (void **) &glx_renderer->glXCreateWindow) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXGetFBConfigs",
                        (void **) &glx_renderer->glXGetFBConfigs) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXDestroyWindow",
                        (void **) &glx_renderer->glXDestroyWindow) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryExtensionsString",
                        (void **) &glx_renderer->glXQueryExtensionsString) ||
      (!g_module_symbol (glx_renderer->libgl_module, "glXGetProcAddress",
                         (void **) &glx_renderer->glXGetProcAddress) &&
       !g_module_symbol (glx_renderer->libgl_module, "glXGetProcAddressARB",
                         (void **) &glx_renderer->glXGetProcAddress)))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to resolve required GLX symbol");
      return FALSE;
    }

  return TRUE;
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglGLXRenderer *glx_renderer;
  CoglXlibRenderer *xlib_renderer;

  renderer->winsys = g_slice_new0 (CoglGLXRenderer);

  glx_renderer = renderer->winsys;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  if (!_cogl_xlib_renderer_connect (renderer, error))
    goto error;

  if (renderer->driver != COGL_DRIVER_GL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GLX Backend can only be used in conjunction with OpenGL");
      goto error;
    }

  glx_renderer->libgl_module = g_module_open (COGL_GL_LIBNAME,
                                              G_MODULE_BIND_LAZY);

  if (glx_renderer->libgl_module == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to dynamically open the OpenGL library");
      goto error;
    }

  if (!resolve_core_glx_functions (renderer, error))
    goto error;

  if (!glx_renderer->glXQueryExtension (xlib_renderer->xdpy,
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
  if (!glx_renderer->glXQueryVersion (xlib_renderer->xdpy,
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

static CoglBool
update_winsys_features (CoglContext *context, GError **error)
{
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  const char *glx_extensions;
  int default_screen;
  int i;

  _COGL_RETURN_VAL_IF_FAIL (glx_display->glx_context, FALSE);

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  default_screen = DefaultScreen (xlib_renderer->xdpy);
  glx_extensions =
    glx_renderer->glXQueryExtensionsString (xlib_renderer->xdpy,
                                            default_screen);

  COGL_NOTE (WINSYS, "  GLX Extensions: %s", glx_extensions);

  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check (context->display->renderer,
                             "GLX", winsys_feature_data + i, 0, 0,
                             COGL_DRIVER_GL, /* the driver isn't used */
                             glx_extensions,
                             glx_renderer))
      {
        context->feature_flags |= winsys_feature_data[i].feature_flags;
        if (winsys_feature_data[i].winsys_feature)
          COGL_FLAGS_SET (context->winsys_features,
                          winsys_feature_data[i].winsys_feature,
                          TRUE);
      }

  /* Note: the GLX_SGI_video_sync spec explicitly states this extension
   * only works for direct contexts. */
  if (!glx_renderer->is_direct)
    {
      glx_renderer->pf_glXGetVideoSync = NULL;
      glx_renderer->pf_glXWaitVideoSync = NULL;
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_VBLANK_COUNTER,
                      FALSE);
    }

  if (glx_renderer->pf_glXWaitVideoSync)
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_VBLANK_WAIT,
                    TRUE);

  if (glx_renderer->pf_glXCopySubBuffer || context->glBlitFramebuffer)
    {
      CoglGpuInfoArchitecture arch;

      /* XXX: ONGOING BUG:
       * (Don't change the line above since we use this to grep for
       * un-resolved bug workarounds as part of the release process.)
       *
       * "The "drisw" binding in Mesa for loading sofware renderers is
       * broken, and neither glBlitFramebuffer nor glXCopySubBuffer
       * work correctly."
       * - ajax
       * - https://bugzilla.gnome.org/show_bug.cgi?id=674208
       *
       * This is broken in software Mesa at least as of 7.10
       */
      arch = context->gpu.architecture;
      if (arch != COGL_GPU_INFO_ARCHITECTURE_LLVMPIPE &&
          arch != COGL_GPU_INFO_ARCHITECTURE_SOFTPIPE &&
          arch != COGL_GPU_INFO_ARCHITECTURE_SWRAST)
	{
	  COGL_FLAGS_SET (context->winsys_features,
			  COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);
	}
    }

  /* Note: glXCopySubBuffer and glBlitFramebuffer won't be throttled
   * by the SwapInterval so we have to throttle swap_region requests
   * manually... */
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION) &&
      _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_WAIT))
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);

  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT))
    COGL_FLAGS_SET (context->features,
                    COGL_FEATURE_ID_SWAP_BUFFERS_EVENT,
                    TRUE);

  return TRUE;
}

static void
glx_attributes_from_framebuffer_config (CoglDisplay *display,
                                        CoglFramebufferConfig *config,
                                        int *attributes)
{
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  int i = 0;

  attributes[i++] = GLX_DRAWABLE_TYPE;
  attributes[i++] = GLX_WINDOW_BIT;

  attributes[i++] = GLX_RENDER_TYPE;
  attributes[i++] = GLX_RGBA_BIT;

  attributes[i++] = GLX_DOUBLEBUFFER;
  attributes[i++] = GL_TRUE;

  attributes[i++] = GLX_RED_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_GREEN_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_BLUE_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_ALPHA_SIZE;
  attributes[i++] = config->swap_chain->has_alpha ? 1 : GLX_DONT_CARE;
  attributes[i++] = GLX_DEPTH_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_STENCIL_SIZE;
  attributes[i++] = config->need_stencil ? 1: GLX_DONT_CARE;

  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 4 &&
      config->samples_per_pixel)
    {
       attributes[i++] = GLX_SAMPLE_BUFFERS;
       attributes[i++] = 1;
       attributes[i++] = GLX_SAMPLES;
       attributes[i++] = config->samples_per_pixel;
    }

  attributes[i++] = None;

  g_assert (i < MAX_GLX_CONFIG_ATTRIBS);
}

/* It seems the GLX spec never defined an invalid GLXFBConfig that
 * we could overload as an indication of error, so we have to return
 * an explicit boolean status. */
static CoglBool
find_fbconfig (CoglDisplay *display,
               CoglFramebufferConfig *config,
               GLXFBConfig *config_ret,
               GError **error)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  GLXFBConfig *configs = NULL;
  int n_configs;
  static int attributes[MAX_GLX_CONFIG_ATTRIBS];
  CoglBool ret = TRUE;
  int xscreen_num = DefaultScreen (xlib_renderer->xdpy);

  glx_attributes_from_framebuffer_config (display, config, attributes);

  configs = glx_renderer->glXChooseFBConfig (xlib_renderer->xdpy,
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

  if (config->swap_chain->has_alpha)
    {
      int i;

      for (i = 0; i < n_configs; i++)
        {
          XVisualInfo *vinfo;

          vinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                          configs[i]);
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

static CoglBool
create_context (CoglDisplay *display, GError **error)
{
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  CoglBool support_transparent_windows =
    display->onscreen_template->config.swap_chain->has_alpha;
  GLXFBConfig config;
  GError *fbconfig_error = NULL;
  XSetWindowAttributes attrs;
  XVisualInfo *xvisinfo;
  GLXDrawable dummy_drawable;
  CoglXlibTrapState old_state;

  _COGL_RETURN_VAL_IF_FAIL (glx_display->glx_context == NULL, TRUE);

  glx_display->found_fbconfig =
    find_fbconfig (display, &display->onscreen_template->config, &config,
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

  glx_display->glx_context =
    glx_renderer->glXCreateNewContext (xlib_renderer->xdpy,
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
    glx_renderer->glXIsDirect (xlib_renderer->xdpy, glx_display->glx_context);

  COGL_NOTE (WINSYS, "Setting %s context",
             glx_renderer->is_direct ? "direct" : "indirect");

  /* XXX: GLX doesn't let us make a context current without a window
   * so we create a dummy window that we can use while no CoglOnscreen
   * framebuffer is in use.
   */

  xvisinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                     config);
  if (xvisinfo == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to retrieve the X11 visual");
      return FALSE;
    }

  _cogl_xlib_renderer_trap_errors (display->renderer, &old_state);

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (xlib_renderer->xdpy,
                                    DefaultRootWindow (xlib_renderer->xdpy),
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  glx_display->dummy_xwin =
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
      glx_display->dummy_glxwin =
        glx_renderer->glXCreateWindow (xlib_renderer->xdpy,
                                       config,
                                       glx_display->dummy_xwin,
                                       NULL);
    }

  if (glx_display->dummy_glxwin)
    dummy_drawable = glx_display->dummy_glxwin;
  else
    dummy_drawable = glx_display->dummy_xwin;

  COGL_NOTE (WINSYS, "Selecting dummy 0x%x for the GLX context",
             (unsigned int) dummy_drawable);

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                       dummy_drawable,
                                       dummy_drawable,
                                       glx_display->glx_context);

  XFree (xvisinfo);

  if (_cogl_xlib_renderer_untrap_errors (display->renderer, &old_state))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to select the newly created GLX context");
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;

  _COGL_RETURN_IF_FAIL (glx_display != NULL);

  if (glx_display->glx_context)
    {
      glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                           None, None, NULL);
      glx_renderer->glXDestroyContext (xlib_renderer->xdpy,
                                       glx_display->glx_context);
      glx_display->glx_context = NULL;
    }

  if (glx_display->dummy_glxwin)
    {
      glx_renderer->glXDestroyWindow (xlib_renderer->xdpy,
                                      glx_display->dummy_glxwin);
      glx_display->dummy_glxwin = None;
    }

  if (glx_display->dummy_xwin)
    {
      XDestroyWindow (xlib_renderer->xdpy, glx_display->dummy_xwin);
      glx_display->dummy_xwin = None;
    }

  g_slice_free (CoglGLXDisplay, display->winsys);
  display->winsys = NULL;
}

static CoglBool
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglGLXDisplay *glx_display;
  int i;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  glx_display = g_slice_new0 (CoglGLXDisplay);
  display->winsys = glx_display;

  if (!create_context (display, error))
    goto error;

  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    glx_display->glx_cached_configs[i].depth = -1;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static CoglBool
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  context->winsys = g_new0 (CoglContextGLX, 1);

  cogl_xlib_renderer_add_filter (context->display->renderer,
                                 glx_event_filter_cb,
                                 context);
  return update_winsys_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  cogl_xlib_renderer_remove_filter (context->display->renderer,
                                    glx_event_filter_cb,
                                    context);
  g_free (context->winsys);
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  Window xwin;
  CoglOnscreenXlib *xlib_onscreen;
  CoglOnscreenGLX *glx_onscreen;
  GLXFBConfig fbconfig;
  GError *fbconfig_error = NULL;

  _COGL_RETURN_VAL_IF_FAIL (glx_display->glx_context, FALSE);

  if (!find_fbconfig (display, &framebuffer->config,
                      &fbconfig,
                      &fbconfig_error))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find suitable fbconfig for the GLX context: %s",
                   fbconfig_error->message);
      g_error_free (fbconfig_error);
      return FALSE;
    }

  /* Update the real number of samples_per_pixel now that we have
   * found an fbconfig... */
  if (framebuffer->config.samples_per_pixel)
    {
      int samples;
      int status = glx_renderer->glXGetFBConfigAttrib (xlib_renderer->xdpy,
                                                       fbconfig,
                                                       GLX_SAMPLES,
                                                       &samples);
      g_return_val_if_fail (status == Success, TRUE);
      framebuffer->samples_per_pixel = samples;
    }

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
      XSync (xlib_renderer->xdpy, False);
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
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

      xvisinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                         fbconfig);
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
        glx_renderer->glXCreateWindow (xlib_renderer->xdpy,
                                       fbconfig,
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
      glx_renderer->glXSelectEvent (xlib_renderer->xdpy,
                                    drawable,
                                    GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
#endif /* GLX_INTEL_swap_event */

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglXlibTrapState old_state;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;

  /* If we never successfully allocated then there's nothing to do */
  if (glx_onscreen == NULL)
    return;

  _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

  if (glx_onscreen->glxwin != None)
    {
      glx_renderer->glXDestroyWindow (xlib_renderer->xdpy,
                                      glx_onscreen->glxwin);
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

  _cogl_xlib_renderer_untrap_errors (context->display->renderer, &old_state);

  g_slice_free (CoglOnscreenGLX, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglXlibTrapState old_state;
  GLXDrawable drawable;

  drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (glx_context->current_drawable == drawable)
    return;

  _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

  COGL_NOTE (WINSYS,
             "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
             xlib_renderer->xdpy,
             (unsigned int) drawable,
             xlib_onscreen->is_foreign_xwin ? "foreign" : "native",
             glx_display->glx_context);

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
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
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (onscreen);
      if (fb->config.swap_throttled)
        glx_renderer->pf_glXSwapInterval (1);
      else
        glx_renderer->pf_glXSwapInterval (0);
    }

  XSync (xlib_renderer->xdpy, False);

  /* FIXME: We should be reporting a GError here
   */
  if (_cogl_xlib_renderer_untrap_errors (context->display->renderer,
                                         &old_state))
    {
      g_warning ("X Error received while making drawable 0x%08lX current",
                 drawable);
      return;
    }

  glx_context->current_drawable = drawable;
}

static void
_cogl_winsys_wait_for_vblank (void)
{
  CoglGLXRenderer *glx_renderer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  glx_renderer = ctx->display->renderer->winsys;

  if (glx_renderer->pf_glXGetVideoSync)
    {
      uint32_t current_count;

      glx_renderer->pf_glXGetVideoSync (&current_count);
      glx_renderer->pf_glXWaitVideoSync (2,
                                         (current_count + 1) % 2,
                                         &current_count);
    }
}

static uint32_t
_cogl_winsys_get_vsync_counter (void)
{
  uint32_t video_sync_count;
  CoglGLXRenderer *glx_renderer;

  _COGL_GET_CONTEXT (ctx, 0);

  glx_renderer = ctx->display->renderer->winsys;

  glx_renderer->pf_glXGetVideoSync (&video_sync_count);

  return video_sync_count;
}

static void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   const int *user_rectangles,
                                   int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
  uint32_t end_frame_vsync_counter = 0;
  CoglBool have_counter;
  CoglBool can_wait;

  /*
   * We assume that glXCopySubBuffer is synchronized which means it won't prevent multiple
   * blits per retrace if they can all be performed in the blanking period. If that's the
   * case then we still want to use the vblank sync menchanism but
   * we only need it to throttle redraws.
   */
  CoglBool blit_sub_buffer_is_synchronized =
     _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED);

  int framebuffer_height =  cogl_framebuffer_get_height (framebuffer);
  int *rectangles = g_alloca (sizeof (int) * n_rectangles * 4);
  int i;

  /* glXCopySubBuffer expects rectangles relative to the bottom left corner but
   * we are given rectangles relative to the top left so we need to flip
   * them... */
  memcpy (rectangles, user_rectangles, sizeof (int) * n_rectangles * 4);
  for (i = 0; i < n_rectangles; i++)
    {
      int *rect = &rectangles[4 * i];
      rect[1] = framebuffer_height - rect[1] - rect[3];
    }

  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

  if (framebuffer->config.swap_throttled)
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
  context->glFinish ();

  if (blit_sub_buffer_is_synchronized && have_counter && can_wait)
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
  else if (context->glBlitFramebuffer)
    {
      int i;
      /* XXX: checkout how this state interacts with the code to use
       * glBlitFramebuffer in Neil's texture atlasing branch */

      /* glBlitFramebuffer is affected by the scissor so we need to
       * ensure we have flushed an empty clip stack to get rid of it.
       * We also mark that the clip state is dirty so that it will be
       * flushed to the correct state the next time something is
       * drawn */
      _cogl_clip_stack_flush (NULL, framebuffer);
      context->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;

      context->glDrawBuffer (GL_FRONT);
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          int x2 = rect[0] + rect[2];
          int y2 = rect[1] + rect[3];
          context->glBlitFramebuffer (rect[0], rect[1], x2, y2,
                                      rect[0], rect[1], x2, y2,
                                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
      context->glDrawBuffer (GL_BACK);
    }

  /* NB: unlike glXSwapBuffers, glXCopySubBuffer and
   * glBlitFramebuffer don't issue an implicit glFlush() so we
   * have to flush ourselves if we want the request to complete in
   * a finite amount of time since otherwise the driver can batch
   * the command indefinitely. */
  context->glFlush ();

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

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglBool have_counter;
  GLXDrawable drawable;

  /* XXX: theoretically this shouldn't be necessary but at least with
   * the Intel drivers we have see that if we don't call
   * glXMakeContextCurrent for the drawable we are swapping then
   * we get a BadDrawable error from the X server. */
  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

  drawable = glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (framebuffer->config.swap_throttled)
    {
      uint32_t end_frame_vsync_counter = 0;

      have_counter =
        _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_VBLANK_COUNTER);

      /* If the swap_region API is also being used then we need to track
       * the vsync counter for each swap request so we can manually
       * throttle swap_region requests. */
      if (have_counter)
        end_frame_vsync_counter = _cogl_winsys_get_vsync_counter ();

      if (!glx_renderer->pf_glXSwapInterval)
        {
          CoglBool can_wait =
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
          context->glFinish ();

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

  glx_renderer->glXSwapBuffers (xlib_renderer->xdpy, drawable);

  if (have_counter)
    glx_onscreen->last_swap_vsync_counter = _cogl_winsys_get_vsync_counter ();
}

static uint32_t
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  return xlib_onscreen->xwin;
}

static void
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

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;

  if (visibility)
    XMapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
  else
    XUnmapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
}

static void
_cogl_winsys_onscreen_set_resizable (CoglOnscreen *onscreen,
                                     CoglBool resizable)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;

  XSizeHints *size_hints = XAllocSizeHints ();

  if (resizable)
    {
      /* TODO: Add cogl_onscreen_request_minimum_size () */
      size_hints->min_width = 1;
      size_hints->min_height = 1;

      size_hints->max_width = INT_MAX;
      size_hints->max_height = INT_MAX;
    }
  else
    {
      int width = cogl_framebuffer_get_width (framebuffer);
      int height = cogl_framebuffer_get_height (framebuffer);

      size_hints->min_width = width;
      size_hints->min_height = height;

      size_hints->max_width = width;
      size_hints->max_height = height;
    }

  XSetWMNormalHints (xlib_renderer->xdpy, xlib_onscreen->xwin, size_hints);

  XFree (size_hints);
}

/* XXX: This is a particularly hacky _cogl_winsys interface... */
static XVisualInfo *
_cogl_winsys_xlib_get_visual_info (void)
{
  CoglGLXDisplay *glx_display;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;

  _COGL_GET_CONTEXT (ctx, NULL);

  _COGL_RETURN_VAL_IF_FAIL (ctx->display->winsys, FALSE);

  glx_display = ctx->display->winsys;
  xlib_renderer = _cogl_xlib_renderer_get_data (ctx->display->renderer);
  glx_renderer = ctx->display->renderer->winsys;

  if (!glx_display->found_fbconfig)
    return NULL;

  return glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                 glx_display->fbconfig);
}

static CoglBool
get_fbconfig_for_depth (CoglContext *context,
                        unsigned int depth,
                        GLXFBConfig *fbconfig_ret,
                        CoglBool *can_mipmap_ret)
{
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;
  CoglGLXDisplay *glx_display;
  Display *dpy;
  GLXFBConfig *fbconfigs;
  int n_elements, i;
  int db, stencil, alpha, mipmap, rgba, value;
  int spare_cache_slot = 0;
  CoglBool found = FALSE;

  xlib_renderer = _cogl_xlib_renderer_get_data (context->display->renderer);
  glx_renderer = context->display->renderer->winsys;
  glx_display = context->display->winsys;

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

  dpy = xlib_renderer->xdpy;

  fbconfigs = glx_renderer->glXGetFBConfigs (dpy, DefaultScreen (dpy),
                                             &n_elements);

  db = G_MAXSHORT;
  stencil = G_MAXSHORT;
  mipmap = 0;
  rgba = 0;

  for (i = 0; i < n_elements; i++)
    {
      XVisualInfo *vi;
      int visual_depth;

      vi = glx_renderer->glXGetVisualFromFBConfig (dpy, fbconfigs[i]);
      if (vi == NULL)
        continue;

      visual_depth = vi->depth;

      XFree (vi);

      if (visual_depth != depth)
        continue;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_ALPHA_SIZE,
                                          &alpha);
      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_BUFFER_SIZE,
                                          &value);
      if (value != depth && (value - alpha) != depth)
        continue;

      value = 0;
      if (depth == 32)
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
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

          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_TEXTURE_RGB_EXT,
                                              &value);
          if (!value)
            continue;
        }

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_DOUBLEBUFFER,
                                          &value);
      if (value > db)
        continue;

      db = value;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_STENCIL_SIZE,
                                          &value);
      if (value > stencil)
        continue;

      stencil = value;

      /* glGenerateMipmap is defined in the offscreen extension */
      if (cogl_has_feature (context, COGL_FEATURE_ID_OFFSCREEN))
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
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

static CoglBool
should_use_rectangle (CoglContext *context)
{

  if (context->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_UNKNOWN)
    {
      if (cogl_has_feature (context, COGL_FEATURE_ID_TEXTURE_RECTANGLE))
        {
          const char *rect_env;

          /* Use the rectangle only if it is available and either:

             the COGL_PIXMAP_TEXTURE_RECTANGLE environment variable is
             set to 'force'

             *or*

             the env var is set to 'allow' or not set and NPOTs textures
             are not available */

          context->rectangle_state =
            cogl_has_feature (context, COGL_FEATURE_ID_TEXTURE_NPOT) ?
            COGL_WINSYS_RECTANGLE_STATE_DISABLE :
            COGL_WINSYS_RECTANGLE_STATE_ENABLE;

          if ((rect_env = g_getenv ("COGL_PIXMAP_TEXTURE_RECTANGLE")) ||
              /* For compatibility, we'll also look at the old Clutter
                 environment variable */
              (rect_env = g_getenv ("CLUTTER_PIXMAP_TEXTURE_RECTANGLE")))
            {
              if (g_ascii_strcasecmp (rect_env, "force") == 0)
                context->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_ENABLE;
              else if (g_ascii_strcasecmp (rect_env, "disable") == 0)
                context->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_DISABLE;
              else if (g_ascii_strcasecmp (rect_env, "allow"))
                g_warning ("Unknown value for COGL_PIXMAP_TEXTURE_RECTANGLE, "
                           "should be 'force' or 'disable'");
            }
        }
      else
        context->rectangle_state = COGL_WINSYS_RECTANGLE_STATE_DISABLE;
    }

  return context->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_ENABLE;
}

static CoglBool
try_create_glx_pixmap (CoglContext *context,
                       CoglTexturePixmapX11 *tex_pixmap,
                       CoglBool mipmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;
  Display *dpy;
  /* We have to initialize this *opaque* variable because gcc tries to
   * be too smart for its own good and warns that the variable may be
   * used uninitialized otherwise. */
  GLXFBConfig fb_config = (GLXFBConfig)0;
  int attribs[7];
  int i = 0;
  GLenum target;
  CoglXlibTrapState trap_state;

  unsigned int depth = tex_pixmap->depth;
  Visual* visual = tex_pixmap->visual;

  renderer = context->display->renderer;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  glx_renderer = renderer->winsys;
  dpy = xlib_renderer->xdpy;

  if (!get_fbconfig_for_depth (context, depth, &fb_config,
                               &glx_tex_pixmap->can_mipmap))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "No suitable FBConfig found for depth %i",
                 depth);
      return FALSE;
    }

  if (should_use_rectangle (context))
    {
      target = GLX_TEXTURE_RECTANGLE_EXT;
      glx_tex_pixmap->can_mipmap = FALSE;
    }
  else
    target = GLX_TEXTURE_2D_EXT;

  if (!glx_tex_pixmap->can_mipmap)
    mipmap = FALSE;

  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

  /* Check whether an alpha channel is used by comparing the total
   * number of 1-bits in color masks against the color depth requested
   * by the client.
   */
  if (_cogl_util_popcountl (visual->red_mask |
                            visual->green_mask |
                            visual->blue_mask) == depth)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
  else
    attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;

  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = mipmap;

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;
  attribs[i++] = target;

  attribs[i++] = None;

  /* We need to trap errors from glXCreatePixmap because it can
   * sometimes fail during normal usage. For example on NVidia it gets
   * upset if you try to create two GLXPixmaps for the same drawable.
   */

  _cogl_xlib_renderer_trap_errors (renderer, &trap_state);

  glx_tex_pixmap->glx_pixmap =
    glx_renderer->glXCreatePixmap (dpy,
                                   fb_config,
                                   tex_pixmap->pixmap,
                                   attribs);
  glx_tex_pixmap->has_mipmap_space = mipmap;

  XSync (dpy, False);

  if (_cogl_xlib_renderer_untrap_errors (renderer, &trap_state))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "Failed to create pixmap for %p", tex_pixmap);
      _cogl_xlib_renderer_trap_errors (renderer, &trap_state);
      glx_renderer->glXDestroyPixmap (dpy, glx_tex_pixmap->glx_pixmap);
      XSync (dpy, False);
      _cogl_xlib_renderer_untrap_errors (renderer, &trap_state);

      glx_tex_pixmap->glx_pixmap = None;
      return FALSE;
    }

  return TRUE;
}

static CoglBool
_cogl_winsys_texture_pixmap_x11_create (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP))
    {
      tex_pixmap->winsys = NULL;
      return FALSE;
    }

  glx_tex_pixmap = g_new0 (CoglTexturePixmapGLX, 1);

  glx_tex_pixmap->glx_pixmap = None;
  glx_tex_pixmap->can_mipmap = FALSE;
  glx_tex_pixmap->has_mipmap_space = FALSE;

  glx_tex_pixmap->glx_tex = NULL;

  glx_tex_pixmap->bind_tex_image_queued = TRUE;
  glx_tex_pixmap->pixmap_bound = FALSE;

  tex_pixmap->winsys = glx_tex_pixmap;

  if (!try_create_glx_pixmap (ctx, tex_pixmap, FALSE))
    {
      tex_pixmap->winsys = NULL;
      g_free (glx_tex_pixmap);
      return FALSE;
    }

  return TRUE;
}

static void
free_glx_pixmap (CoglContext *context,
                 CoglTexturePixmapGLX *glx_tex_pixmap)
{
  CoglXlibTrapState trap_state;
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;

  renderer = context->display->renderer;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  glx_renderer = renderer->winsys;

  if (glx_tex_pixmap->pixmap_bound)
    glx_renderer->pf_glXReleaseTexImage (xlib_renderer->xdpy,
                                         glx_tex_pixmap->glx_pixmap,
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
  _cogl_xlib_renderer_trap_errors (renderer, &trap_state);
  glx_renderer->glXDestroyPixmap (xlib_renderer->xdpy,
                                  glx_tex_pixmap->glx_pixmap);
  XSync (xlib_renderer->xdpy, False);
  _cogl_xlib_renderer_untrap_errors (renderer, &trap_state);

  glx_tex_pixmap->glx_pixmap = None;
  glx_tex_pixmap->pixmap_bound = FALSE;
}

static void
_cogl_winsys_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap;

  /* FIXME: It should be possible to get to a CoglContext from any
   * CoglTexture pointer. */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!tex_pixmap->winsys)
    return;

  glx_tex_pixmap = tex_pixmap->winsys;

  free_glx_pixmap (ctx, glx_tex_pixmap);

  if (glx_tex_pixmap->glx_tex)
    cogl_object_unref (glx_tex_pixmap->glx_tex);

  tex_pixmap->winsys = NULL;
  g_free (glx_tex_pixmap);
}

static CoglBool
_cogl_winsys_texture_pixmap_x11_update (CoglTexturePixmapX11 *tex_pixmap,
                                        CoglBool needs_mipmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;
  CoglGLXRenderer *glx_renderer;

  /* FIXME: It should be possible to get to a CoglContext from any CoglTexture
   * pointer. */
  _COGL_GET_CONTEXT (ctx, FALSE);

  /* If we don't have a GLX pixmap then fallback */
  if (glx_tex_pixmap->glx_pixmap == None)
    return FALSE;

  glx_renderer = ctx->display->renderer->winsys;

  /* Lazily create a texture to hold the pixmap */
  if (glx_tex_pixmap->glx_tex == NULL)
    {
      CoglPixelFormat texture_format;
      GError *error = NULL;

      texture_format = (tex_pixmap->depth >= 32 ?
                        COGL_PIXEL_FORMAT_RGBA_8888_PRE :
                        COGL_PIXEL_FORMAT_RGB_888);

      if (should_use_rectangle (ctx))
        {
          glx_tex_pixmap->glx_tex = COGL_TEXTURE (
            cogl_texture_rectangle_new_with_size (ctx,
                                                  tex_pixmap->width,
                                                  tex_pixmap->height,
                                                  texture_format,
                                                  &error));

          if (glx_tex_pixmap->glx_tex)
            COGL_NOTE (TEXTURE_PIXMAP, "Created a texture rectangle for %p",
                       tex_pixmap);
          else
            {
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                         "texture rectangle could not be created: %s",
                         tex_pixmap, error->message);
              g_error_free (error);
              free_glx_pixmap (ctx, glx_tex_pixmap);
              return FALSE;
            }
        }
      else
        {
          glx_tex_pixmap->glx_tex = COGL_TEXTURE (
            cogl_texture_2d_new_with_size (ctx,
                                           tex_pixmap->width,
                                           tex_pixmap->height,
                                           texture_format,
                                           NULL));

          if (glx_tex_pixmap->glx_tex)
            COGL_NOTE (TEXTURE_PIXMAP, "Created a texture 2d for %p",
                       tex_pixmap);
          else
            {
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                         "texture 2d could not be created",
                         tex_pixmap);
              free_glx_pixmap (ctx, glx_tex_pixmap);
              return FALSE;
            }
        }
    }

  if (needs_mipmap)
    {
      /* If we can't support mipmapping then temporarily fallback */
      if (!glx_tex_pixmap->can_mipmap)
        return FALSE;

      /* Recreate the GLXPixmap if it wasn't previously created with a
       * mipmap tree */
      if (!glx_tex_pixmap->has_mipmap_space)
        {
          free_glx_pixmap (ctx, glx_tex_pixmap);

          COGL_NOTE (TEXTURE_PIXMAP, "Recreating GLXPixmap with mipmap "
                     "support for %p", tex_pixmap);
          if (!try_create_glx_pixmap (ctx, tex_pixmap, TRUE))

            {
              /* If the pixmap failed then we'll permanently fallback
               * to using XImage. This shouldn't happen. */
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back to XGetImage "
                         "updates for %p because creating the GLXPixmap "
                         "with mipmap support failed", tex_pixmap);

              if (glx_tex_pixmap->glx_tex)
                cogl_object_unref (glx_tex_pixmap->glx_tex);
              return FALSE;
            }

          glx_tex_pixmap->bind_tex_image_queued = TRUE;
        }
    }

  if (glx_tex_pixmap->bind_tex_image_queued)
    {
      GLuint gl_handle, gl_target;
      CoglXlibRenderer *xlib_renderer =
        _cogl_xlib_renderer_get_data (ctx->display->renderer);

      cogl_texture_get_gl_texture (glx_tex_pixmap->glx_tex,
                                   &gl_handle, &gl_target);

      COGL_NOTE (TEXTURE_PIXMAP, "Rebinding GLXPixmap for %p", tex_pixmap);

      _cogl_bind_gl_texture_transient (gl_target, gl_handle, FALSE);

      if (glx_tex_pixmap->pixmap_bound)
        glx_renderer->pf_glXReleaseTexImage (xlib_renderer->xdpy,
                                             glx_tex_pixmap->glx_pixmap,
                                             GLX_FRONT_LEFT_EXT);

      glx_renderer->pf_glXBindTexImage (xlib_renderer->xdpy,
                                        glx_tex_pixmap->glx_pixmap,
                                        GLX_FRONT_LEFT_EXT,
                                        NULL);

      /* According to the recommended usage in the spec for
       * GLX_EXT_texture_pixmap we should release the texture after
       * we've finished drawing with it and it is undefined what
       * happens if you render to a pixmap that is bound to a texture.
       * However that would require the texture backend to know when
       * Cogl has finished painting and it may be more expensive to
       * keep unbinding the texture. Leaving it bound appears to work
       * on Mesa and NVidia drivers and it is also what Compiz does so
       * it is probably ok */

      glx_tex_pixmap->bind_tex_image_queued = FALSE;
      glx_tex_pixmap->pixmap_bound = TRUE;

      _cogl_texture_2d_externally_modified (glx_tex_pixmap->glx_tex);
    }

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_damage_notify (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;

  glx_tex_pixmap->bind_tex_image_queued = TRUE;
}

static CoglTexture *
_cogl_winsys_texture_pixmap_x11_get_texture (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;

  return glx_tex_pixmap->glx_tex;
}

static void
_cogl_winsys_poll_get_info (CoglContext *context,
                            CoglPollFD **poll_fds,
                            int *n_poll_fds,
                            int64_t *timeout)
{
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;

  _cogl_xlib_renderer_poll_get_info (context->display->renderer,
                                     poll_fds,
                                     n_poll_fds,
                                     timeout);

  /* If we've already got a pending swap notify then we'll dispatch
     immediately */
  if (glx_display->pending_swap_notify || glx_display->pending_resize_notify)
    *timeout = 0;
}

static void
flush_pending_notifications_cb (void *data,
                                void *user_data)
{
  CoglFramebuffer *framebuffer = data;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenGLX *glx_onscreen = onscreen->winsys;

      if (glx_onscreen->pending_swap_notify)
        {
          _cogl_onscreen_notify_swap_buffers (onscreen);
          glx_onscreen->pending_swap_notify = FALSE;
        }

      if (glx_onscreen->pending_resize_notify)
        {
          _cogl_onscreen_notify_resize (onscreen);
          glx_onscreen->pending_resize_notify = FALSE;
        }
    }
}

static void
_cogl_winsys_poll_dispatch (CoglContext *context,
                            const CoglPollFD *poll_fds,
                            int n_poll_fds)
{
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;

  _cogl_xlib_renderer_poll_dispatch (context->display->renderer,
                                     poll_fds,
                                     n_poll_fds);

  if (glx_display->pending_swap_notify || glx_display->pending_resize_notify)
    {
      g_list_foreach (context->framebuffers,
                      flush_pending_notifications_cb,
                      NULL);
      glx_display->pending_swap_notify = FALSE;
      glx_display->pending_resize_notify = FALSE;
    }
}

static CoglWinsysVtable _cogl_winsys_vtable =
  {
    .id = COGL_WINSYS_ID_GLX,
    .name = "GLX",
    .constraints = (COGL_RENDERER_CONSTRAINT_USES_X11 |
                    COGL_RENDERER_CONSTRAINT_USES_XLIB),

    .renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,
    .xlib_get_visual_info = _cogl_winsys_xlib_get_visual_info,
    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers,
    .onscreen_swap_region = _cogl_winsys_onscreen_swap_region,
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
    .onscreen_x11_get_window_xid =
      _cogl_winsys_onscreen_x11_get_window_xid,
    .onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility,
    .onscreen_set_resizable =
      _cogl_winsys_onscreen_set_resizable,

    .poll_get_info = _cogl_winsys_poll_get_info,
    .poll_dispatch = _cogl_winsys_poll_dispatch,

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
_cogl_winsys_glx_get_vtable (void)
{
  return &_cogl_winsys_vtable;
}
