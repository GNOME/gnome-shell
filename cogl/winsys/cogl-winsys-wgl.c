/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>

#include "cogl.h"

#include "cogl-util.h"
#include "cogl-winsys-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-onscreen-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-private.h"
#include "cogl-feature-private.h"
#include "cogl-win32-renderer.h"
#include "cogl-winsys-wgl-private.h"
#include "cogl-error-private.h"
#include "cogl-poll-private.h"

/* This magic handle will cause g_poll to wakeup when there is a
 * pending message */
#define WIN32_MSG_HANDLE 19981206

typedef struct _CoglRendererWgl
{
  GModule *gl_module;

  /* Function pointers for GLX specific extensions */
#define COGL_WINSYS_FEATURE_BEGIN(a, b, c, d, e, f)

#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_WINSYS_FEATURE_END()

#include "cogl-winsys-wgl-feature-functions.h"

#undef COGL_WINSYS_FEATURE_BEGIN
#undef COGL_WINSYS_FEATURE_FUNCTION
#undef COGL_WINSYS_FEATURE_END
} CoglRendererWgl;

typedef struct _CoglDisplayWgl
{
  ATOM window_class;
  HGLRC wgl_context;
  HWND dummy_hwnd;
  HDC dummy_dc;
} CoglDisplayWgl;

typedef struct _CoglOnscreenWin32
{
  HWND hwnd;
  CoglBool is_foreign_hwnd;
} CoglOnscreenWin32;

typedef struct _CoglContextWgl
{
  HDC current_dc;
} CoglContextWgl;

typedef struct _CoglOnscreenWgl
{
  CoglOnscreenWin32 _parent;

  HDC client_dc;

} CoglOnscreenWgl;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  static const CoglFeatureFunction                                      \
  cogl_wgl_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglRendererWgl, pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-wgl-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  feature_flags, feature_flags_private, \
                                  winsys_feature)                       \
  { 255, 255, 0, namespaces, extension_names,                            \
      feature_flags, feature_flags_private,                             \
      winsys_feature,                                                   \
      cogl_wgl_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData winsys_feature_data[] =
  {
#include "cogl-winsys-wgl-feature-functions.h"
  };

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core)
{
  CoglRendererWgl *wgl_renderer = renderer->winsys;
  void *proc = wglGetProcAddress ((LPCSTR) name);

  /* The documentation for wglGetProcAddress implies that it only
     returns pointers to extension functions so if it fails we'll try
     resolving the symbol directly from the the GL library. We could
     completely avoid using wglGetProcAddress if in_core is TRUE but
     on WGL any function that is in GL > 1.1 is considered an
     extension and is not directly exported from opengl32.dll.
     Therefore we currently just assume wglGetProcAddress will return
     NULL for GL 1.1 functions and we can fallback to querying them
     directly from the library */

  if (proc == NULL)
    {
      if (wgl_renderer->gl_module == NULL)
        wgl_renderer->gl_module = g_module_open ("opengl32", 0);

      if (wgl_renderer->gl_module)
        g_module_symbol (wgl_renderer->gl_module, name, &proc);
    }

  return proc;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererWgl *wgl_renderer = renderer->winsys;

  if (renderer->win32_enable_event_retrieval)
    _cogl_poll_renderer_remove_fd (renderer, WIN32_MSG_HANDLE);

  if (wgl_renderer->gl_module)
    g_module_close (wgl_renderer->gl_module);

  g_slice_free (CoglRendererWgl, renderer->winsys);
}

static CoglOnscreen *
find_onscreen_for_hwnd (CoglContext *context, HWND hwnd)
{
  CoglDisplayWgl *display_wgl = context->display->winsys;
  GList *l;

  /* If the hwnd has Cogl's window class then we can lookup the
     onscreen pointer directly by reading the extra window data */
  if (GetClassLongPtr (hwnd, GCW_ATOM) == display_wgl->window_class)
    {
      CoglOnscreen *onscreen = (CoglOnscreen *) GetWindowLongPtr (hwnd, 0);

      if (onscreen)
        return onscreen;
    }

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;

      if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        {
          CoglOnscreenWin32 *win32_onscreen =
            COGL_ONSCREEN (framebuffer)->winsys;

          if (win32_onscreen->hwnd == hwnd)
            return COGL_ONSCREEN (framebuffer);
        }
    }

  return NULL;
}

static CoglFilterReturn
win32_event_filter_cb (MSG *msg, void *data)
{
  CoglContext *context = data;

  if (msg->message == WM_SIZE)
    {
      CoglOnscreen *onscreen =
        find_onscreen_for_hwnd (context, msg->hwnd);

      if (onscreen)
        {
          CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

          /* Ignore size changes resulting from the stage being
             minimized - otherwise it will think the window has been
             resized to 0,0 */
          if (msg->wParam != SIZE_MINIMIZED)
            {
              WORD new_width = LOWORD (msg->lParam);
              WORD new_height = HIWORD (msg->lParam);
              _cogl_framebuffer_winsys_update_size (framebuffer,
                                                    new_width,
                                                    new_height);
            }
        }
    }
  else if (msg->message == WM_PAINT)
    {
      CoglOnscreen *onscreen =
        find_onscreen_for_hwnd (context, msg->hwnd);
      RECT rect;

      if (onscreen && GetUpdateRect (msg->hwnd, &rect, FALSE))
        {
          CoglOnscreenDirtyInfo info;

          /* Apparently this removes the dirty region from the window
           * so that it won't be included in the next WM_PAINT
           * message. This is also what SDL does to emit dirty
           * events */
          ValidateRect (msg->hwnd, &rect);

          info.x = rect.left;
          info.y = rect.top;
          info.width = rect.right - rect.left;
          info.height = rect.bottom - rect.top;

          _cogl_onscreen_queue_dirty (onscreen, &info);
        }
    }

  return COGL_FILTER_CONTINUE;
}

static CoglBool
check_messages (void *user_data)
{
  MSG msg;

  return PeekMessageW (&msg, NULL, 0, 0, PM_NOREMOVE) ? TRUE : FALSE;
}

static void
dispatch_messages (void *user_data)
{
  MSG msg;

  while (PeekMessageW (&msg, NULL, 0, 0, PM_REMOVE))
    /* This should cause the message to be sent to our window proc */
    DispatchMessageW (&msg);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  renderer->winsys = g_slice_new0 (CoglRendererWgl);

  if (renderer->win32_enable_event_retrieval)
    {
      /* We'll add a magic handle that will cause a GLib main loop to
       * wake up when there are messages. This will only work if the
       * application is using GLib but it shouldn't matter if it
       * doesn't work in other cases because the application shouldn't
       * be using the cogl_poll_* functions on non-Unix systems
       * anyway */
      _cogl_poll_renderer_add_fd (renderer,
                                  WIN32_MSG_HANDLE,
                                  COGL_POLL_FD_EVENT_IN,
                                  check_messages,
                                  dispatch_messages,
                                  renderer);
    }

  return TRUE;
}

static LRESULT CALLBACK
window_proc (HWND hwnd, UINT umsg,
             WPARAM wparam, LPARAM lparam)
{
  CoglBool message_handled = FALSE;
  CoglOnscreen *onscreen;

  /* It's not clear what the best thing to do with messages sent to
     the window proc is. We want the application to forward on all
     messages through Cogl so that it can have a chance to process
     them which might mean that that in it's GetMessage loop it could
     call cogl_win32_renderer_handle_event for every message. However
     the message loop would usually call DispatchMessage as well which
     mean this window proc would be invoked and Cogl would see the
     message twice. However we can't just ignore messages in the
     window proc because some messages are sent directly from windows
     without going through the message queue. This function therefore
     just forwards on all messages directly. This means that the
     application is not expected to forward on messages if it has let
     Cogl create the window itself because it will already see them
     via the window proc. This limits the kinds of messages that Cogl
     can handle to ones that are sent to the windows it creates, but I
     think that is a reasonable restriction */

  /* Convert the message to a MSG struct and pass it through the Cogl
     message handling mechanism */

  /* This window proc is only called for messages created with Cogl's
     window class so we should be able to work out the corresponding
     window class by looking in the extra window data. Windows will
     send some extra messages before we get a chance to set this value
     so we have to ignore these */
  onscreen = (CoglOnscreen *) GetWindowLongPtr (hwnd, 0);

  if (onscreen != NULL)
    {
      CoglRenderer *renderer;
      DWORD message_pos;
      MSG msg;

      msg.hwnd = hwnd;
      msg.message = umsg;
      msg.wParam = wparam;
      msg.lParam = lparam;
      msg.time = GetMessageTime ();
      /* Neither MAKE_POINTS nor GET_[XY]_LPARAM is defined in MinGW
         headers so we need to convert to a signed type explicitly */
      message_pos = GetMessagePos ();
      msg.pt.x = (SHORT) LOWORD (message_pos);
      msg.pt.y = (SHORT) HIWORD (message_pos);

      renderer = COGL_FRAMEBUFFER (onscreen)->context->display->renderer;

      message_handled =
        cogl_win32_renderer_handle_event (renderer, &msg);
    }

  if (!message_handled)
    return DefWindowProcW (hwnd, umsg, wparam, lparam);
  else
    return 0;
}

static CoglBool
pixel_format_is_better (const PIXELFORMATDESCRIPTOR *pfa,
                        const PIXELFORMATDESCRIPTOR *pfb)
{
  /* Always prefer a format with a stencil buffer */
  if (pfa->cStencilBits == 0)
    {
      if (pfb->cStencilBits > 0)
        return TRUE;
    }
  else if (pfb->cStencilBits == 0)
    return FALSE;

  /* Prefer a bigger color buffer */
  if (pfb->cColorBits > pfa->cColorBits)
    return TRUE;
  else if (pfb->cColorBits < pfa->cColorBits)
    return FALSE;

  /* Prefer a bigger depth buffer */
  return pfb->cDepthBits > pfa->cDepthBits;
}

static int
choose_pixel_format (CoglFramebufferConfig *config,
                     HDC dc, PIXELFORMATDESCRIPTOR *pfd)
{
  int i, num_formats, best_pf = 0;
  PIXELFORMATDESCRIPTOR best_pfd;

  num_formats = DescribePixelFormat (dc, 0, sizeof (best_pfd), NULL);

  /* XXX: currently we don't support multisampling on windows... */
  if (config->samples_per_pixel)
    return best_pf;

  for (i = 1; i <= num_formats; i++)
    {
      memset (pfd, 0, sizeof (*pfd));

      if (DescribePixelFormat (dc, i, sizeof (best_pfd), pfd) &&
          /* Check whether this format is useable by Cogl */
          ((pfd->dwFlags & (PFD_SUPPORT_OPENGL |
                            PFD_DRAW_TO_WINDOW |
                            PFD_DOUBLEBUFFER |
                            PFD_GENERIC_FORMAT)) ==
           (PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW)) &&
          pfd->iPixelType == PFD_TYPE_RGBA &&
          pfd->cColorBits >= 16 && pfd->cColorBits <= 32 &&
          pfd->cDepthBits >= 16 && pfd->cDepthBits <= 32 &&
          /* Check whether this is a better format than one we've
             already found */
          (best_pf == 0 || pixel_format_is_better (&best_pfd, pfd)))
        {
          if (config->swap_chain->has_alpha && pfd->cAlphaBits == 0)
            continue;
          if (config->need_stencil && pfd->cStencilBits == 0)
            continue;

          best_pf = i;
          best_pfd = *pfd;
        }
    }

  *pfd = best_pfd;

  return best_pf;
}

static CoglBool
create_window_class (CoglDisplay *display, CoglError **error)
{
  CoglDisplayWgl *wgl_display = display->winsys;
  char *class_name_ascii, *src;
  WCHAR *class_name_wchar, *dst;
  WNDCLASSW wndclass;

  /* We create a window class per display so that we have an
     opportunity to clean up the class when the display is
     destroyed */

  /* Generate a unique name containing the address of the display */
  class_name_ascii = g_strdup_printf ("CoglWindow0x%0*" G_GINTPTR_MODIFIER "x",
                                      sizeof (guintptr) * 2,
                                      (guintptr) display);
  /* Convert it to WCHARs */
  class_name_wchar = g_malloc ((strlen (class_name_ascii) + 1) *
                               sizeof (WCHAR));
  for (src = class_name_ascii, dst = class_name_wchar;
       *src;
       src++, dst++)
    *dst = *src;
  *dst = L'\0';

  memset (&wndclass, 0, sizeof (wndclass));
  wndclass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc = window_proc;
  /* We reserve extra space in the window data for a pointer back to
     the CoglOnscreen */
  wndclass.cbWndExtra = sizeof (LONG_PTR);
  wndclass.hInstance = GetModuleHandleW (NULL);
  wndclass.hIcon = LoadIconW (NULL, (LPWSTR) IDI_APPLICATION);
  wndclass.hCursor = LoadCursorW (NULL, (LPWSTR) IDC_ARROW);
  wndclass.hbrBackground = NULL;
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = class_name_wchar;
  wgl_display->window_class = RegisterClassW (&wndclass);

  g_free (class_name_wchar);
  g_free (class_name_ascii);

  if (wgl_display->window_class == 0)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Unable to register window class");
      return FALSE;
    }

  return TRUE;
}

static CoglBool
create_context (CoglDisplay *display, CoglError **error)
{
  CoglDisplayWgl *wgl_display = display->winsys;

  _COGL_RETURN_VAL_IF_FAIL (wgl_display->wgl_context == NULL, FALSE);

  /* Cogl assumes that there is always a GL context selected; in order
   * to make sure that a WGL context exists and is made current, we
   * use a small dummy window that never gets shown to which we can
   * always fall back if no onscreen is available
   */
  if (wgl_display->dummy_hwnd == NULL)
    {
      wgl_display->dummy_hwnd =
        CreateWindowW ((LPWSTR) MAKEINTATOM (wgl_display->window_class),
                       L".",
                       WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT,
                       CW_USEDEFAULT,
                       1, 1,
                       NULL, NULL,
                       GetModuleHandle (NULL),
                       NULL);

      if (wgl_display->dummy_hwnd == NULL)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Unable to create dummy window");
          return FALSE;
        }
    }

  if (wgl_display->dummy_dc == NULL)
    {
      PIXELFORMATDESCRIPTOR pfd;
      int pf;

      wgl_display->dummy_dc = GetDC (wgl_display->dummy_hwnd);

      pf = choose_pixel_format (&display->onscreen_template->config,
                                wgl_display->dummy_dc, &pfd);

      if (pf == 0 || !SetPixelFormat (wgl_display->dummy_dc, pf, &pfd))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Unable to find suitable GL pixel format");
          ReleaseDC (wgl_display->dummy_hwnd, wgl_display->dummy_dc);
          wgl_display->dummy_dc = NULL;
          return FALSE;
        }
    }

  if (wgl_display->wgl_context == NULL)
    {
      wgl_display->wgl_context = wglCreateContext (wgl_display->dummy_dc);

      if (wgl_display->wgl_context == NULL)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "Unable to create suitable GL context");
          return FALSE;
        }
    }

  COGL_NOTE (WINSYS, "Selecting dummy 0x%x for the WGL context",
             (unsigned int) wgl_display->dummy_hwnd);

  wglMakeCurrent (wgl_display->dummy_dc, wgl_display->wgl_context);

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplayWgl *wgl_display = display->winsys;

  _COGL_RETURN_IF_FAIL (wgl_display != NULL);

  if (wgl_display->wgl_context)
    {
      wglMakeCurrent (NULL, NULL);
      wglDeleteContext (wgl_display->wgl_context);
    }

  if (wgl_display->dummy_dc)
    ReleaseDC (wgl_display->dummy_hwnd, wgl_display->dummy_dc);

  if (wgl_display->dummy_hwnd)
    DestroyWindow (wgl_display->dummy_hwnd);

  if (wgl_display->window_class)
    UnregisterClassW ((LPWSTR) MAKEINTATOM (wgl_display->window_class),
                      GetModuleHandleW (NULL));

  g_slice_free (CoglDisplayWgl, display->winsys);
  display->winsys = NULL;
}

static CoglBool
_cogl_winsys_display_setup (CoglDisplay *display,
                            CoglError **error)
{
  CoglDisplayWgl *wgl_display;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  wgl_display = g_slice_new0 (CoglDisplayWgl);
  display->winsys = wgl_display;

  if (!create_window_class (display, error))
    goto error;

  if (!create_context (display, error))
    goto error;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static const char *
get_wgl_extensions_string (HDC dc)
{
  const char * (APIENTRY *pf_wglGetExtensionsStringARB) (HDC);
  const char * (APIENTRY *pf_wglGetExtensionsStringEXT) (void);

  _COGL_GET_CONTEXT (ctx, NULL);

  /* According to the docs for these two extensions, you are supposed
     to use wglGetProcAddress to detect their availability so
     presumably it will return NULL if they are not available */

  pf_wglGetExtensionsStringARB =
    (void *) wglGetProcAddress ("wglGetExtensionsStringARB");

  if (pf_wglGetExtensionsStringARB)
    return pf_wglGetExtensionsStringARB (dc);

  pf_wglGetExtensionsStringEXT =
    (void *) wglGetProcAddress ("wglGetExtensionsStringEXT");

  if (pf_wglGetExtensionsStringEXT)
    return pf_wglGetExtensionsStringEXT ();

  /* The WGL_EXT_swap_control is also advertised as a GL extension as
     GL_EXT_SWAP_CONTROL so if the extension to get the list of WGL
     extensions isn't supported then we can at least fake it to
     support the swap control extension */
  {
    char **extensions = _cogl_context_get_gl_extensions (ctx);
    CoglBool have_ext = _cogl_check_extension ("WGL_EXT_swap_control",
                                               extensions);
    g_strfreev (extensions);
    if (have_ext)
      return "WGL_EXT_swap_control";
  }

  return NULL;
}

static CoglBool
update_winsys_features (CoglContext *context, CoglError **error)
{
  CoglDisplayWgl *wgl_display = context->display->winsys;
  CoglRendererWgl *wgl_renderer = context->display->renderer->winsys;
  const char *wgl_extensions;
  int i;

  _COGL_RETURN_VAL_IF_FAIL (wgl_display->wgl_context, FALSE);

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  wgl_extensions = get_wgl_extensions_string (wgl_display->dummy_dc);

  if (wgl_extensions)
    {
      char **split_extensions =
        g_strsplit (wgl_extensions, " ", 0 /* max_tokens */);

      COGL_NOTE (WINSYS, "  WGL Extensions: %s", wgl_extensions);

      for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
        if (_cogl_feature_check (context->display->renderer,
                                 "WGL", winsys_feature_data + i, 0, 0,
                                 COGL_DRIVER_GL,
                                 split_extensions,
                                 wgl_renderer))
          {
            context->feature_flags |= winsys_feature_data[i].feature_flags;
            if (winsys_feature_data[i].winsys_feature)
              COGL_FLAGS_SET (context->winsys_features,
                              winsys_feature_data[i].winsys_feature,
                              TRUE);
          }

      g_strfreev (split_extensions);
    }

  /* We'll manually handle queueing dirty events in response to
   * WM_PAINT messages */
  context->private_feature_flags |= COGL_PRIVATE_FEATURE_DIRTY_EVENTS;

  return TRUE;
}

static CoglBool
_cogl_winsys_context_init (CoglContext *context, CoglError **error)
{
  context->winsys = g_new0 (CoglContextWgl, 1);

  cogl_win32_renderer_add_filter (context->display->renderer,
                                  win32_event_filter_cb,
                                  context);

  return update_winsys_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  cogl_win32_renderer_remove_filter (context->display->renderer,
                                     win32_event_filter_cb,
                                     context);

  g_free (context->winsys);
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglFramebuffer *fb;
  CoglContext *context;
  CoglContextWgl *wgl_context;
  CoglDisplayWgl *wgl_display;
  CoglOnscreenWgl *wgl_onscreen;
  CoglRendererWgl *wgl_renderer;

  /* The glx backend tries to bind the dummy context if onscreen ==
     NULL, but this isn't really going to work because before checking
     whether onscreen == NULL it reads the pointer to get the
     context */
  _COGL_RETURN_IF_FAIL (onscreen != NULL);

  fb = COGL_FRAMEBUFFER (onscreen);
  context = fb->context;
  wgl_context = context->winsys;
  wgl_display = context->display->winsys;
  wgl_onscreen = onscreen->winsys;
  wgl_renderer = context->display->renderer->winsys;

  if (wgl_context->current_dc == wgl_onscreen->client_dc)
    return;

  wglMakeCurrent (wgl_onscreen->client_dc, wgl_display->wgl_context);

  /* According to the specs for WGL_EXT_swap_control SwapInterval()
   * applies to the current window not the context so we apply it here
   * to ensure its up-to-date even for new windows.
   */
  if (wgl_renderer->pf_wglSwapInterval)
    {
      if (fb->config.swap_throttled)
        wgl_renderer->pf_wglSwapInterval (1);
      else
        wgl_renderer->pf_wglSwapInterval (0);
    }

  wgl_context->current_dc = wgl_onscreen->client_dc;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextWgl *wgl_context = context->winsys;
  CoglOnscreenWin32 *win32_onscreen = onscreen->winsys;
  CoglOnscreenWgl *wgl_onscreen = onscreen->winsys;

  /* If we never successfully allocated then there's nothing to do */
  if (wgl_onscreen == NULL)
    return;

  if (wgl_onscreen->client_dc)
    {
      if (wgl_context->current_dc == wgl_onscreen->client_dc)
        _cogl_winsys_onscreen_bind (NULL);

      ReleaseDC (win32_onscreen->hwnd, wgl_onscreen->client_dc);
    }

  if (!win32_onscreen->is_foreign_hwnd && win32_onscreen->hwnd)
    {
      /* Drop the pointer to the onscreen in the window so that any
         further messages won't be processed */
      SetWindowLongPtrW (win32_onscreen->hwnd, 0, (LONG_PTR) 0);
      DestroyWindow (win32_onscreen->hwnd);
    }

  g_slice_free (CoglOnscreenWgl, onscreen->winsys);
  onscreen->winsys = NULL;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayWgl *wgl_display = display->winsys;
  CoglOnscreenWgl *wgl_onscreen;
  CoglOnscreenWin32 *win32_onscreen;
  PIXELFORMATDESCRIPTOR pfd;
  int pf;
  HWND hwnd;

  _COGL_RETURN_VAL_IF_FAIL (wgl_display->wgl_context, FALSE);

  /* XXX: Note we ignore the user's original width/height when given a
   * foreign window. */
  if (onscreen->foreign_hwnd)
    {
      RECT client_rect;

      hwnd = onscreen->foreign_hwnd;

      GetClientRect (hwnd, &client_rect);

      _cogl_framebuffer_winsys_update_size (framebuffer,
                                            client_rect.right,
                                            client_rect.bottom);
    }
  else
    {
      int width, height;

      width = COGL_FRAMEBUFFER (onscreen)->width;
      height = COGL_FRAMEBUFFER (onscreen)->height;

      /* The size of the window passed to CreateWindow for some reason
         includes the window decorations so we need to compensate for
         that */
      width += GetSystemMetrics (SM_CXSIZEFRAME) * 2;
      height += (GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
                 GetSystemMetrics (SM_CYCAPTION));

      hwnd = CreateWindowW ((LPWSTR) MAKEINTATOM (wgl_display->window_class),
                            L".",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, /* xpos */
                            CW_USEDEFAULT, /* ypos */
                            width,
                            height,
                            NULL, /* parent */
                            NULL, /* menu */
                            GetModuleHandle (NULL),
                            NULL /* lparam for the WM_CREATE message */);

      if (hwnd == NULL)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to create window");
          return FALSE;
        }

      /* Store a pointer back to the onscreen in the window extra data
         so we can refer back to it quickly */
      SetWindowLongPtrW (hwnd, 0, (LONG_PTR) onscreen);
    }

  onscreen->winsys = g_slice_new0 (CoglOnscreenWgl);
  win32_onscreen = onscreen->winsys;
  wgl_onscreen = onscreen->winsys;

  win32_onscreen->hwnd = hwnd;

  wgl_onscreen->client_dc = GetDC (hwnd);

  /* Use the same pixel format as the dummy DC from the renderer */
  pf = choose_pixel_format (&framebuffer->config,
                            wgl_onscreen->client_dc, &pfd);

  if (pf == 0 || !SetPixelFormat (wgl_onscreen->client_dc, pf, &pfd))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Error setting pixel format on the window");

      _cogl_winsys_onscreen_deinit (onscreen);

      return FALSE;
    }

  return TRUE;
}

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
{
  CoglOnscreenWgl *wgl_onscreen = onscreen->winsys;

  SwapBuffers (wgl_onscreen->client_dc);
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextWgl *wgl_context = context->winsys;
  CoglOnscreenWgl *wgl_onscreen = onscreen->winsys;

  if (wgl_context->current_dc != wgl_onscreen->client_dc)
    return;

  /* This will cause it to rebind the context and update the swap interval */
  wgl_context->current_dc = NULL;
  _cogl_winsys_onscreen_bind (onscreen);
}

static HWND
_cogl_winsys_onscreen_win32_get_window (CoglOnscreen *onscreen)
{
  CoglOnscreenWin32 *win32_onscreen = onscreen->winsys;
  return win32_onscreen->hwnd;
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglOnscreenWin32 *win32_onscreen = onscreen->winsys;

  ShowWindow (win32_onscreen->hwnd, visibility ? SW_SHOW : SW_HIDE);
}

const CoglWinsysVtable *
_cogl_winsys_wgl_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  /* It would be nice if we could use C99 struct initializers here
     like the GLX backend does. However this code is more likely to be
     compiled using Visual Studio which (still!) doesn't support them
     so we initialize it in code instead */

  if (!vtable_inited)
    {
      memset (&vtable, 0, sizeof (vtable));

      vtable.id = COGL_WINSYS_ID_WGL;
      vtable.name = "WGL";
      vtable.renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address;
      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;
      vtable.display_setup = _cogl_winsys_display_setup;
      vtable.display_destroy = _cogl_winsys_display_destroy;
      vtable.context_init = _cogl_winsys_context_init;
      vtable.context_deinit = _cogl_winsys_context_deinit;
      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;
      vtable.onscreen_bind = _cogl_winsys_onscreen_bind;
      vtable.onscreen_swap_buffers_with_damage =
        _cogl_winsys_onscreen_swap_buffers_with_damage;
      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;
      vtable.onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility;
      vtable.onscreen_win32_get_window = _cogl_winsys_onscreen_win32_get_window;

      vtable_inited = TRUE;
    }

  return &vtable;
}
