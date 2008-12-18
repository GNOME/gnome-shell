/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Johan Bilien  <johan.bilien@nokia.com>
 *             Matthew Allum <mallum@o-hand.com>
 *             Robert Bragg  <bob@o-hand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

/*  TODO:  
 *  - Automagically handle named pixmaps, and window resizes (i.e
 *    essentially handle window id's being passed in) ?
*/

/**
 * SECTION:clutter-glx-texture-pixmap
 * @short_description: A texture which displays the content of an X Pixmap.
 *
 * #ClutterGLXTexturePixmap is a class for displaying the content of an
 * X Pixmap as a ClutterActor. Used together with the X Composite extension,
 * it allows to display the content of X Windows inside Clutter.
 *
 * The class requires the GLX_EXT_texture_from_pixmap OpenGL extension
 * (http://people.freedesktop.org/~davidr/GLX_EXT_texture_from_pixmap.txt)
 *
 * The GL_ARB_texture_non_power_of_two extension will be used if it is
 * available. Otherwise it will try to use the
 * GL_ARB_texture_rectangle extension. If both are available you can
 * force it to prefer the rectangle extension by setting the
 * CLUTTER_PIXMAP_TEXTURE_RECTANGLE to 'force'. To prevent it ever
 * using the rectangle extension you can set it to 'disable'.
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "../x11/clutter-x11-texture-pixmap.h"
#include "clutter-glx-texture-pixmap.h"
#include "clutter-glx.h"
#include "clutter-backend-glx.h"

#include "../clutter-util.h"
#include "../clutter-debug.h"

#include "cogl/cogl.h"

typedef void    (*BindTexImage) (Display     *display,
                                 GLXDrawable  drawable,
                                 int          buffer,
                                 int         *attribList);
typedef void    (*ReleaseTexImage) (Display     *display,
                                    GLXDrawable  drawable,
                                    int          buffer);

typedef void    (*GenerateMipmap) (GLenum target);

typedef enum
{
  CLUTTER_GLX_RECTANGLE_DISALLOW,
  CLUTTER_GLX_RECTANGLE_ALLOW,
  CLUTTER_GLX_RECTANGLE_FORCE
} RectangleState;

static BindTexImage      _gl_bind_tex_image = NULL;
static ReleaseTexImage   _gl_release_tex_image = NULL;
static GenerateMipmap    _gl_generate_mipmap = NULL;
static gboolean          _have_tex_from_pixmap_ext = FALSE;
static gboolean          _ext_check_done = FALSE;
static gboolean          _have_tex_rectangle = FALSE;
static RectangleState    _rectangle_state = CLUTTER_GLX_RECTANGLE_ALLOW;

struct _ClutterGLXTexturePixmapPrivate
{
  COGLenum      target_type;
  guint         texture_id;
  GLXPixmap     glx_pixmap;

  gboolean      use_fallback;

  gboolean      bound;
  gint          can_mipmap;
  gint          mipmap_generate_queued;

  gboolean      using_rectangle;
};

static void 
clutter_glx_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                        gint x,
                                        gint y,
                                        gint width,
                                        gint height);

static void 
clutter_glx_texture_pixmap_create_glx_pixmap (ClutterGLXTexturePixmap *tex);

static ClutterX11TexturePixmapClass *parent_class = NULL;

G_DEFINE_TYPE (ClutterGLXTexturePixmap,    \
               clutter_glx_texture_pixmap, \
               CLUTTER_X11_TYPE_TEXTURE_PIXMAP);

static gboolean
texture_bind (ClutterGLXTexturePixmap *tex)
{
  GLuint     handle = 0;
  GLenum     target = 0;
  CoglHandle cogl_tex;
  cogl_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE(tex));

  if (!cogl_texture_get_gl_texture (cogl_tex, &handle, &target))
      return FALSE;

  glEnable(target);

  /* FIXME: fire off an error here? */
  glBindTexture (target, handle);

  if (clutter_texture_get_filter_quality (CLUTTER_TEXTURE (tex)) 
         == CLUTTER_TEXTURE_QUALITY_HIGH && tex->priv->can_mipmap)
    {
      cogl_texture_set_filters (cogl_tex, 
                                CGL_LINEAR_MIPMAP_LINEAR,
                                CGL_LINEAR);
    }

  return TRUE;
}

static void
on_glx_texture_pixmap_pre_paint (ClutterGLXTexturePixmap *texture,
                                 gpointer                 user_data)
{
  if (texture->priv->mipmap_generate_queued)
    {
      GLuint     handle = 0;
      GLenum     target = 0;
      CoglHandle cogl_tex;
      cogl_tex = clutter_texture_get_cogl_texture 
        (CLUTTER_TEXTURE(texture));

      texture_bind (texture);

      cogl_texture_get_gl_texture (cogl_tex, &handle, &target);
      
      _gl_generate_mipmap (target);

      texture->priv->mipmap_generate_queued = 0;
    }

}

static void
clutter_glx_texture_pixmap_init (ClutterGLXTexturePixmap *self)
{
  ClutterGLXTexturePixmapPrivate *priv;

  priv = self->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                                   ClutterGLXTexturePixmapPrivate);

  g_signal_connect (CLUTTER_ACTOR(self),
                    "paint", G_CALLBACK (on_glx_texture_pixmap_pre_paint),
                    NULL);

  if (_ext_check_done == FALSE)
    {
      const char *gl_extensions = NULL;
      const gchar *glx_extensions = NULL;
      const char *rect_env;

      glx_extensions =
        glXQueryExtensionsString (clutter_x11_get_default_display (),
                                  clutter_x11_get_default_screen ());

      /* Check for the texture from pixmap extension */
      if (cogl_check_extension ("GLX_EXT_texture_from_pixmap", glx_extensions))
        {
          _gl_bind_tex_image =
            (BindTexImage)cogl_get_proc_address ("glXBindTexImageEXT");
          _gl_release_tex_image =
            (ReleaseTexImage)cogl_get_proc_address ("glXReleaseTexImageEXT");

          if (_gl_bind_tex_image && _gl_release_tex_image)
            _have_tex_from_pixmap_ext = TRUE;
        }

      _gl_generate_mipmap =
        (GenerateMipmap)cogl_get_proc_address ("glGenerateMipmapEXT");

      gl_extensions = (char *) glGetString (GL_EXTENSIONS);
      _have_tex_rectangle = cogl_check_extension ("GL_ARB_texture_rectangle",
                                                  gl_extensions);

      if ((rect_env = g_getenv ("CLUTTER_PIXMAP_TEXTURE_RECTANGLE")))
        {
          if (strcasecmp (rect_env, "force") == 0)
            _rectangle_state = CLUTTER_GLX_RECTANGLE_FORCE;
	  else if (strcasecmp (rect_env, "disable") == 0)
            _rectangle_state = CLUTTER_GLX_RECTANGLE_DISALLOW;
	  else if (rect_env[0])
	    g_warning ("Unknown value for CLUTTER_PIXMAP_TEXTURE_RECTANGLE, "
		       "should be 'force' or 'disable'");
        }

      _ext_check_done = TRUE;
    }
}

static void
clutter_glx_texture_pixmap_free_rectangle (ClutterGLXTexturePixmap *texture)
{
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;
  CoglHandle cogl_tex;

  /* Cogl won't free the GL texture resource if it was created with
     new_from_foreign so we need to free it manually */
  if (priv->using_rectangle)
    {
      cogl_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (texture));

      if (cogl_tex != COGL_INVALID_HANDLE)
        {
          GLuint gl_handle;
          GLenum gl_target;

          cogl_texture_get_gl_texture (cogl_tex, &gl_handle, &gl_target);

          if (gl_target == CGL_TEXTURE_RECTANGLE_ARB)
            glDeleteTextures (1, &gl_handle);
        }

      priv->using_rectangle = FALSE;
    }
}

static void
clutter_glx_texture_pixmap_dispose (GObject *object)
{
  ClutterGLXTexturePixmap *texture = CLUTTER_GLX_TEXTURE_PIXMAP (object);
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;

  clutter_glx_texture_pixmap_free_rectangle (texture);

  if (priv->glx_pixmap != None)
    {
      clutter_x11_trap_x_errors ();

      glXDestroyGLXPixmap (clutter_x11_get_default_display(),
                           priv->glx_pixmap);
      XSync (clutter_x11_get_default_display(), FALSE);

      clutter_x11_untrap_x_errors ();

      priv->glx_pixmap = None;
    }

  G_OBJECT_CLASS (clutter_glx_texture_pixmap_parent_class)->dispose (object);
}

static void
clutter_glx_texture_pixmap_notify (GObject *object, GParamSpec *pspec)
{
  if (g_str_equal (pspec->name, "pixmap"))
    {
      ClutterGLXTexturePixmap *texture = CLUTTER_GLX_TEXTURE_PIXMAP (object);
      clutter_glx_texture_pixmap_create_glx_pixmap (texture);
    }
}

static gboolean
should_use_rectangle (void)
{
  /* Use the rectangle only if it is available and either:

     the CLUTTER_PIXMAP_TEXTURE_RECTANGLE environment variable is
     set to 'force'

     *or*

     the env var is set to 'allow' (which is the default) and NPOTs
     textures are not available */
  return _have_tex_rectangle
    && ((_rectangle_state == CLUTTER_GLX_RECTANGLE_ALLOW
         && !clutter_feature_available (CLUTTER_FEATURE_TEXTURE_NPOT))
        || _rectangle_state == CLUTTER_GLX_RECTANGLE_FORCE);
}

static gboolean
create_cogl_texture (ClutterTexture *texture,
		     guint width,
		     guint height)
{
  ClutterGLXTexturePixmap *texture_glx = CLUTTER_GLX_TEXTURE_PIXMAP (texture);
  ClutterGLXTexturePixmapPrivate *priv = texture_glx->priv;
  CoglHandle  handle;
  gboolean    using_rectangle;

  /* We want to use the GL_ARB_texture_rectangle extension on some
     chipsets because GL_ARB_texture_non_power_of_two is not always
     supported or might be buggy */
  if (should_use_rectangle ())
    {
      GLuint tex = 0;

      using_rectangle = TRUE;

      glGenTextures (1, &tex);
      glBindTexture (CGL_TEXTURE_RECTANGLE_ARB, tex);
      glTexImage2D (CGL_TEXTURE_RECTANGLE_ARB, 0,
                    GL_RGBA, width, height,
                    0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

      handle = cogl_texture_new_from_foreign (tex, CGL_TEXTURE_RECTANGLE_ARB,
                                              width, height,
                                              0, 0,
                                              COGL_PIXEL_FORMAT_RGBA_8888
                                              | COGL_BGR_BIT);
    }
  else
    {
      handle
        = cogl_texture_new_with_size (width, height,
                                      -1, FALSE,
                                      COGL_PIXEL_FORMAT_RGBA_8888|COGL_BGR_BIT);

      using_rectangle = FALSE;
    }

  if (handle)
    {
      clutter_glx_texture_pixmap_free_rectangle (texture_glx);
      priv->using_rectangle = using_rectangle;

      clutter_texture_set_cogl_texture (texture, handle);

      CLUTTER_ACTOR_SET_FLAGS (texture, CLUTTER_ACTOR_REALIZED);

      clutter_glx_texture_pixmap_update_area
                                  (CLUTTER_X11_TEXTURE_PIXMAP (texture),
                                   0, 0,
                                   width, height);
      return TRUE;
    }

  return FALSE;
}

static void
clutter_glx_texture_pixmap_realize (ClutterActor *actor)
{
  ClutterGLXTexturePixmapPrivate *priv;
  Pixmap                          pixmap;
  guint                           pixmap_width, pixmap_height;

  priv = CLUTTER_GLX_TEXTURE_PIXMAP (actor)->priv;

  if (priv->use_fallback)
    {
      CLUTTER_NOTE (TEXTURE, "texture from pixmap appears unsupported");
      CLUTTER_NOTE (TEXTURE, "Falling back to X11 manual mechansim");

      CLUTTER_ACTOR_CLASS (clutter_glx_texture_pixmap_parent_class)->
        realize (actor);
      return;
    }

  g_object_get (actor,
                "pixmap", &pixmap,
                "pixmap-width",   &pixmap_width,
                "pixmap-height",  &pixmap_height,
                NULL);
  
  if (!pixmap)
      return;

  if (!create_cogl_texture (CLUTTER_TEXTURE (actor), 
                            pixmap_width, pixmap_height))
    {
      CLUTTER_NOTE (TEXTURE, "Unable to create a valid pixmap");
      CLUTTER_NOTE (TEXTURE, "Falling back to X11 manual mechanism");
      priv->use_fallback = TRUE;
      CLUTTER_ACTOR_CLASS (clutter_glx_texture_pixmap_parent_class)->
        realize (actor);
      return;
    }

  CLUTTER_NOTE (TEXTURE, "texture pixmap realised");
}

static void
clutter_glx_texture_pixmap_unrealize (ClutterActor *actor)
{
  ClutterGLXTexturePixmap        *texture = CLUTTER_GLX_TEXTURE_PIXMAP (actor);
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;
  Display                        *dpy;

  dpy = clutter_x11_get_default_display();

  clutter_glx_texture_pixmap_free_rectangle (texture);

  if (!_have_tex_from_pixmap_ext)
    {
      CLUTTER_ACTOR_CLASS (clutter_glx_texture_pixmap_parent_class)->
          unrealize (actor);
      return;
    }

  if (!CLUTTER_ACTOR_IS_REALIZED (actor))
    return;

  if (priv->glx_pixmap && priv->bound)
    {
      clutter_x11_trap_x_errors ();

      (_gl_release_tex_image) (dpy,
                               priv->glx_pixmap,
                               GLX_FRONT_LEFT_EXT);

      XSync (clutter_x11_get_default_display(), FALSE);
      clutter_x11_untrap_x_errors ();

      priv->bound = FALSE;
    }
  
  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static GLXFBConfig *
get_fbconfig_for_depth (ClutterGLXTexturePixmap *texture, guint depth)
{
  GLXFBConfig *fbconfigs, *ret = NULL;
  int          n_elements, i, found;
  Display     *dpy;
  int          db, stencil, alpha, mipmap, rgba, value;

  static GLXFBConfig *cached_config = NULL;
  static gboolean     have_cached_config = FALSE;
  static int          cached_mipmap = 0;

  if (have_cached_config)
    {
      texture->priv->can_mipmap = cached_mipmap;
      return cached_config;
    }

  dpy = clutter_x11_get_default_display ();

  fbconfigs = glXGetFBConfigs (dpy,
                               clutter_x11_get_default_screen (),
                               &n_elements);

  db      = G_MAXSHORT;
  stencil = G_MAXSHORT;
  mipmap  = 0;
  rgba    = 0;

  found = n_elements;

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

      if (_gl_generate_mipmap)
        {
          glXGetFBConfigAttrib (dpy,
                                fbconfigs[i],
                                GLX_BIND_TO_MIPMAP_TEXTURE_EXT,
                                &value);

          if (value < mipmap)
            continue;

          mipmap =  value;
        }

      found = i;
    }

  if (found != n_elements)
    {
      ret = g_malloc (sizeof (GLXFBConfig));
      *ret = fbconfigs[found];
    }

  if (n_elements)
    XFree (fbconfigs);

  have_cached_config = TRUE;
  cached_config = ret;
  texture->priv->can_mipmap = cached_mipmap = mipmap;

  return ret;
}

static void
clutter_glx_texture_pixmap_free_glx_pixmap (ClutterGLXTexturePixmap *texture)
{
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;
  Display                        *dpy;

  dpy = clutter_x11_get_default_display ();

  if (priv->glx_pixmap &&
      priv->bound)
    {
      texture_bind (texture);

      clutter_x11_trap_x_errors ();

      (_gl_release_tex_image) (dpy,
			       priv->glx_pixmap,
			       GLX_FRONT_LEFT_EXT);

      XSync (clutter_x11_get_default_display(), FALSE);

      if (clutter_x11_untrap_x_errors ())
	CLUTTER_NOTE (TEXTURE, "Failed to release?");

      CLUTTER_NOTE (TEXTURE, "Destroyed pxm: %li", priv->glx_pixmap);

      priv->bound = FALSE;
    }

  clutter_x11_trap_x_errors ();
  if (priv->glx_pixmap)
    glXDestroyGLXPixmap (dpy, priv->glx_pixmap);
  XSync (dpy, FALSE);
  clutter_x11_untrap_x_errors ();
  priv->glx_pixmap = None;
}

static void
clutter_glx_texture_pixmap_create_glx_pixmap (ClutterGLXTexturePixmap *texture)
{
  ClutterGLXTexturePixmapPrivate *priv = texture->priv;
  GLXPixmap                       glx_pixmap = None;
  int                             attribs[7], i = 0, mipmap = 0;
  GLXFBConfig                    *fbconfig;
  Display                        *dpy;
  guint                           depth;
  Pixmap                          pixmap;
  guint				  pixmap_width, pixmap_height;
  ClutterBackendGLX              *backend_glx;
  ClutterTextureQuality           quality;

  CLUTTER_NOTE (TEXTURE, "Creating GLXPixmap");

  backend_glx = CLUTTER_BACKEND_GLX(clutter_get_default_backend ());

  dpy = clutter_x11_get_default_display ();

  if (!clutter_glx_texture_pixmap_using_extension (texture))
    goto cleanup;

  priv->use_fallback = FALSE;

  g_object_get (texture,
                "pixmap-width",  &pixmap_width,
                "pixmap-height", &pixmap_height,
                "pixmap-depth",  &depth,
                "pixmap",        &pixmap,
                NULL);

  if (!pixmap)
    {
      goto cleanup;
    }

  fbconfig = get_fbconfig_for_depth (texture, depth);

  if (!fbconfig)
    {
      g_warning ("Could not find an FBConfig for selected pixmap");
      goto cleanup;
    }

  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

  if (depth == 24)
    {
      attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
    }
  else if (depth == 32)
    {
      attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;
    }
  else
    {
      g_warning ("Pixmap with depth bellow 24 are not supported");
      goto cleanup;
    }

  quality = clutter_texture_get_filter_quality (CLUTTER_TEXTURE (texture));

  if (quality == CLUTTER_TEXTURE_QUALITY_HIGH && priv->can_mipmap)
    mipmap = priv->can_mipmap;

  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = mipmap;

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;

  attribs[i++] = (should_use_rectangle ()
                  ? GLX_TEXTURE_RECTANGLE_EXT : GLX_TEXTURE_2D_EXT);

  attribs[i++] = None;

  /* Note: some drivers (e.g. Nvidia) get upset if you effectivly create a glx
   * pixmap for the same server side object, even though you might have unique
   * client side names, we currently avoid hitting this problem by destroying
   * the current glx pixmap first */
  if (priv->glx_pixmap)
    clutter_glx_texture_pixmap_free_glx_pixmap (texture);

  clutter_x11_trap_x_errors ();
  glx_pixmap = glXCreatePixmap (dpy,
                                *fbconfig,
                                pixmap,
                                attribs);
  XSync (dpy, FALSE);
  if (clutter_x11_untrap_x_errors ())
    {
      CLUTTER_NOTE (TEXTURE, "Failed to create GLXPixmap");

      /* Make sure we don't think the call actually succeeded */
      glx_pixmap = None;
    }

 cleanup:

  if (priv->glx_pixmap)
    clutter_glx_texture_pixmap_free_glx_pixmap (texture);

  if (glx_pixmap != None)
    {
      priv->glx_pixmap = glx_pixmap;
      
      create_cogl_texture (CLUTTER_TEXTURE (texture), 
                           pixmap_width, pixmap_height);

      CLUTTER_NOTE (TEXTURE, "Created GLXPixmap");

      /* Get ready to queue initial mipmap generation */
      if (_gl_generate_mipmap
          && priv->can_mipmap
          &&  clutter_texture_get_filter_quality (CLUTTER_TEXTURE (texture))
          == CLUTTER_TEXTURE_QUALITY_HIGH)
        {
          priv->mipmap_generate_queued++;
        }

      return;
    }
  else
    {
      priv->use_fallback = TRUE;
      priv->glx_pixmap   = None;

      /* Some fucky logic here - we've fallen back and need to make sure
       * we realize here..  
      */
      clutter_actor_realize (CLUTTER_ACTOR (texture));
    }
}

static void
clutter_glx_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                        gint                     x,
                                        gint                     y,
                                        gint                     width,
                                        gint                     height)
{
  ClutterGLXTexturePixmap *texture_glx = CLUTTER_GLX_TEXTURE_PIXMAP (texture);
  ClutterGLXTexturePixmapPrivate       *priv = texture_glx->priv;
  Display                              *dpy;

  CLUTTER_NOTE (TEXTURE, "Updating texture pixmap");

  dpy = clutter_x11_get_default_display();

  if (!CLUTTER_ACTOR_IS_REALIZED (texture))
    return;

  if (priv->use_fallback)
    {
      CLUTTER_NOTE (TEXTURE, "Falling back to X11");

      clutter_glx_texture_pixmap_free_rectangle (texture_glx);

      parent_class->update_area (texture,
                                 x, y,
                                 width, height);
      return;
    }

  if (priv->glx_pixmap == None)
    return;
  
  if (texture_bind (CLUTTER_GLX_TEXTURE_PIXMAP(texture)))
    {
      CLUTTER_NOTE (TEXTURE, "Really updating via GLX");

      clutter_x11_trap_x_errors ();
      
      (_gl_bind_tex_image) (dpy,
                            priv->glx_pixmap,
                            GLX_FRONT_LEFT_EXT,
                            NULL);
      
      XSync (clutter_x11_get_default_display(), FALSE);
      
      /* Note above fires X error for non name pixmaps - but
       * things still seem to work - i.e pixmap updated  
       */
      if (clutter_x11_untrap_x_errors ())
        CLUTTER_NOTE (TEXTURE, "Update bind_tex_image failed");

      priv->bound = TRUE;

      if (_gl_generate_mipmap
          && priv->can_mipmap
          &&  clutter_texture_get_filter_quality (CLUTTER_TEXTURE (texture))
              == CLUTTER_TEXTURE_QUALITY_HIGH)
        {
          /* FIXME: It may make more sense to set a flag here and only
           *        generate the mipmap on a pre paint.. compressing need
           *        to call generate mipmap 
           *        May break clones however..
          */
          priv->mipmap_generate_queued++;
        }

    }
  else
    g_warning ("Failed to bind initial tex");

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(texture)))
  clutter_actor_queue_redraw (CLUTTER_ACTOR(texture));

}

static void
clutter_glx_texture_pixmap_class_init (ClutterGLXTexturePixmapClass *klass)
{
  GObjectClass                 *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass            *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterX11TexturePixmapClass *x11_texture_class =
      CLUTTER_X11_TEXTURE_PIXMAP_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterGLXTexturePixmapPrivate));

  parent_class = g_type_class_peek_parent(klass);

  object_class->dispose = clutter_glx_texture_pixmap_dispose;
  object_class->notify  = clutter_glx_texture_pixmap_notify;

  actor_class->realize   = clutter_glx_texture_pixmap_realize;
  actor_class->unrealize = clutter_glx_texture_pixmap_unrealize;

  x11_texture_class->update_area = clutter_glx_texture_pixmap_update_area;

}

/**
 * clutter_glx_texture_pixmap_using_extension:
 * @texture: A #ClutterGLXTexturePixmap
 *
 * Return value: A boolean indicating if the texture is using the  
 * GLX_EXT_texture_from_pixmap OpenGL extension or falling back to
 * slower software mechanism.
 *
 * Since: 0.8
 **/
gboolean
clutter_glx_texture_pixmap_using_extension (ClutterGLXTexturePixmap *texture)
{
  ClutterGLXTexturePixmapPrivate       *priv;

  priv = CLUTTER_GLX_TEXTURE_PIXMAP (texture)->priv;

  return (_have_tex_from_pixmap_ext && !priv->use_fallback); 
  /* Assume NPOT TFP's are supported even if regular NPOT isn't advertised 
   * but tfp is. Seemingly some Intel drivers do this ?
  */
  /* && clutter_feature_available (COGL_FEATURE_TEXTURE_NPOT)); */
}

/**
 * clutter_glx_texture_pixmap_new_with_pixmap:
 * @pixmap: the X Pixmap to which this texture should be bound
 *
 * Return value: A new #ClutterGLXTexturePixmap bound to the given X Pixmap
 *
 * Since: 0.8
 **/
ClutterActor*
clutter_glx_texture_pixmap_new_with_pixmap (Pixmap pixmap)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                        "pixmap", pixmap,
                        NULL);

  return actor;
}

/**
 * clutter_glx_texture_pixmap_new_with_window:
 * @window: the X window to which this texture should be bound
 *
 * Return value: A new #ClutterGLXTexturePixmap bound to the given X window
 *
 * Since: 0.8
 **/
ClutterActor*
clutter_glx_texture_pixmap_new_with_window (Window window)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                        "window", window,
                        NULL);

  return actor;
}

/**
 * clutter_glx_texture_pixmap_new:
 *
 * Return value: A new #ClutterGLXTexturePixmap
 *
 * Since: 0.8
 **/
ClutterActor *
clutter_glx_texture_pixmap_new (void)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP, NULL);

  return actor;
}
