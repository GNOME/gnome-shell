/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-feature
 * @short_description: functions to query available GL features ay runtime 
 *
 * Functions to query available GL features ay runtime 
 */

#include "config.h"
#include "clutter-main.h"
#include "clutter-feature.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

 #include <dlfcn.h>

typedef void (*FuncPtr) (void);
typedef int (*GLXGetVideoSyncProc)  (unsigned int *count);
typedef int (*GLXWaitVideoSyncProc) (int           divisor,
				     int          remainder,
				     unsigned int *count);
typedef FuncPtr (*GLXGetProcAddressProc) (const guint8 *procName);

typedef struct ClutterFeatureFuncs
{
  GLXGetVideoSyncProc  get_video_sync;
  GLXWaitVideoSyncProc wait_video_sync;

} ClutterFeatureFuncs;

typedef enum ClutterVBlankType
{
  CLUTTER_VBLANK_NONE = 0,
  CLUTTER_VBLANK_GLX,
  CLUTTER_VBLANK_DRI

} ClutterVBlankType;

typedef struct ClutterFeatures
{
  ClutterFeatureFlags flags;
  ClutterFeatureFuncs funcs;
  gint                dri_fd;
  ClutterVBlankType   vblank_type;

} ClutterFeatures;

static ClutterFeatures* __features = NULL;

/* #ifdef linux */
#define DRM_VBLANK_RELATIVE 0x1;

struct drm_wait_vblank_request {
    int           type;
    unsigned int  sequence;
    unsigned long signal;
};

struct drm_wait_vblank_reply {
    int          type;
    unsigned int sequence;
    long         tval_sec;
    long         tval_usec;
};

typedef union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
} drm_wait_vblank_t;

#define DRM_IOCTL_BASE                  'd'
#define DRM_IOWR(nr,type)               _IOWR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOCTL_WAIT_VBLANK           DRM_IOWR(0x3a, drm_wait_vblank_t)

static int drm_wait_vblank(int fd, drm_wait_vblank_t *vbl)
{
    int ret, rc;

    do 
      {
	ret = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, vbl);
	vbl->request.type &= ~DRM_VBLANK_RELATIVE;
	rc = errno;
      } 
    while (ret && rc == EINTR);
    
    return rc;
}

/* #endif */


/* Note must be called after context created */
static gboolean 
check_gl_extension (const gchar *name,  const gchar *ext)
{
  gchar       *end;
  gint         name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (gchar*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end) 
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

static FuncPtr
get_proc_address (const gchar *name)
{
  static GLXGetProcAddressProc get_proc_func = NULL;
  static void                 *dlhand = NULL;

  if (get_proc_func == NULL && dlhand == NULL)
    {
      dlhand = dlopen (NULL, RTLD_LAZY);

      if (dlhand)
	{
	  dlerror ();
	  get_proc_func 
	    = (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddress");
	  if (dlerror () != NULL)
	    get_proc_func 
	      = (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddressARB");
	  if (dlerror () != NULL)
	    {
	      get_proc_func = NULL;
	      g_warning 
		("failed to bind GLXGetProcAddress or GLXGetProcAddressARB");
	    }
	}
    }

  if (get_proc_func)
    return get_proc_func ((unsigned char*)name);

  return NULL;
}


static gboolean
check_vblank_env (const char *name)
{
  const char *val;

  val = getenv("CLUTTER_VBLANK");

  if (val && !strcasecmp(val, name))
    return TRUE;

  return FALSE;
}

void
clutter_feature_init (void)
{
  const gchar *gl_extensions, *glx_extensions;

  if (__features != NULL)
    {
      g_warning ("You should never call clutter_feature_init() "
                 "more than once.");
      return;
    }

  __features = g_new0 (ClutterFeatures, 1);
  memset(&__features->funcs, 0, sizeof(ClutterFeatureFuncs));

  gl_extensions = (const gchar*)glGetString(GL_EXTENSIONS);
  glx_extensions = glXQueryExtensionsString (clutter_xdisplay(), 
					     clutter_xscreen());

  if (check_gl_extension ("GL_ARB_texture_rectangle", gl_extensions))
    __features->flags |= CLUTTER_FEATURE_TEXTURE_RECTANGLE;

  /* vblank */

  __features->vblank_type = CLUTTER_VBLANK_NONE;

  if (getenv("__GL_SYNC_TO_VBLANK") || check_vblank_env("none"))
    {
      CLUTTER_DBG("vblank sync: disabled at user request");
    }
  else
    {
      if (!check_vblank_env("dri") 
	  && check_gl_extension ("GLX_SGI_video_sync", glx_extensions))
	{
	  __features->funcs.get_video_sync = 
	     (GLXGetVideoSyncProc) get_proc_address ("glXGetVideoSyncSGI");

	  __features->funcs.wait_video_sync = 
	    (GLXWaitVideoSyncProc) get_proc_address ("glXWaitVideoSyncSGI");

	  if (__features->funcs.get_video_sync != NULL
	      && __features->funcs.wait_video_sync != NULL)
	    {
	      CLUTTER_DBG("vblank sync: using glx");
	      __features->vblank_type = CLUTTER_VBLANK_GLX;
	      __features->flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
	    }
	}
	
      if (!(__features->flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
	{
	  __features->dri_fd = open("/dev/dri/card0", O_RDWR);
	  if (__features->dri_fd >= 0)
	    {
	      CLUTTER_DBG("vblank sync: using dri");
	      __features->vblank_type = CLUTTER_VBLANK_DRI;
	      __features->flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
	    }
	}

      if (!(__features->flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
	CLUTTER_DBG("vblank sync: no use-able mechanism found");
    }
}

/**
 * clutter_feature_available:
 * @feature: a #ClutterFeatureFlags
 *
 * Checks whether @feature is available.  @feature can be a logical
 * OR of #ClutterFeatureFlags.
 * 
 * Return value: %TRUE if a feature is available
 *
 * Since: 0.1.1
 */
gboolean
clutter_feature_available (ClutterFeatureFlags feature)
{
  return (__features->flags & feature);
}

/**
 * clutter_feature_get_all:
 *
 * Returns all the suppoerted features.
 *
 * Return value: a logical OR of all the supported features.
 *
 * Since: 0.1.1
 */
ClutterFeatureFlags
clutter_feature_get_all (void)
{
  return __features->flags;
}

/**
 * clutter_feature_wait_for_vblank:
 *
 * FIXME
 *
 * Since: 0.2
 */
void
clutter_feature_wait_for_vblank (void)
{
  switch (__features->vblank_type)
    {
    case CLUTTER_VBLANK_GLX:
      {
	unsigned int retraceCount;
	__features->funcs.get_video_sync(&retraceCount);
	__features->funcs.wait_video_sync(2, 
					  (retraceCount+1)%2, &retraceCount); 
      }
      break;
    case CLUTTER_VBLANK_DRI:
      {
	drm_wait_vblank_t blank;
	blank.request.type     = DRM_VBLANK_RELATIVE;
	blank.request.sequence = 1;
	drm_wait_vblank (__features->dri_fd, &blank);
      }
      break;
    case CLUTTER_VBLANK_NONE:
    default:
      break;
    }
}
