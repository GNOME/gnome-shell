/*
 * shaped texture
 *
 * An actor to draw a texture clipped to a list of rectangles
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2008 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <clutter/clutter-texture.h>
#include <clutter/x11/clutter-x11.h>
#ifdef HAVE_GLX_TEXTURE_PIXMAP
#include <clutter/glx/clutter-glx.h>
#endif /* HAVE_GLX_TEXTURE_PIXMAP */
#include <cogl/cogl.h>
#include <string.h>

#include "mutter-shaped-texture.h"

static void mutter_shaped_texture_dispose (GObject *object);
static void mutter_shaped_texture_finalize (GObject *object);

static void mutter_shaped_texture_paint (ClutterActor *actor);
static void mutter_shaped_texture_pick (ClutterActor *actor,
					const ClutterColor *color);

static void mutter_shaped_texture_dirty_mask (MutterShapedTexture *stex);

#ifdef HAVE_GLX_TEXTURE_PIXMAP
G_DEFINE_TYPE (MutterShapedTexture, mutter_shaped_texture,
               CLUTTER_GLX_TYPE_TEXTURE_PIXMAP);
#else /* HAVE_GLX_TEXTURE_PIXMAP */
G_DEFINE_TYPE (MutterShapedTexture, mutter_shaped_texture,
               CLUTTER_X11_TYPE_TEXTURE_PIXMAP);
#endif /* HAVE_GLX_TEXTURE_PIXMAP */

#define MUTTER_SHAPED_TEXTURE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MUTTER_TYPE_SHAPED_TEXTURE, \
                                MutterShapedTexturePrivate))

enum TstMultiTexSupport
  {
    TST_MULTI_TEX_SUPPORT_UNKNOWN = 0,
    TST_MULTI_TEX_SUPPORT_YES,
    TST_MULTI_TEX_SUPPORT_NO
  };

static enum TstMultiTexSupport
tst_multi_tex_support = TST_MULTI_TEX_SUPPORT_UNKNOWN;

typedef void (* TstActiveTextureFunc) (GLenum texture);
typedef void (* TstClientActiveTextureFunc) (GLenum texture);

static TstActiveTextureFunc tst_active_texture;
static TstClientActiveTextureFunc tst_client_active_texture;

struct _MutterShapedTexturePrivate
{
  CoglHandle mask_texture;

  guint mask_width, mask_height;
  guint mask_gl_width, mask_gl_height;
  GLfloat mask_tex_coords[8];

  GArray *rectangles;
};

static void
mutter_shaped_texture_class_init (MutterShapedTextureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  gobject_class->dispose = mutter_shaped_texture_dispose;
  gobject_class->finalize = mutter_shaped_texture_finalize;

  actor_class->paint = mutter_shaped_texture_paint;
  actor_class->pick = mutter_shaped_texture_pick;

  g_type_class_add_private (klass, sizeof (MutterShapedTexturePrivate));
}

static void
mutter_shaped_texture_init (MutterShapedTexture *self)
{
  MutterShapedTexturePrivate *priv;

  priv = self->priv = MUTTER_SHAPED_TEXTURE_GET_PRIVATE (self);

  priv->rectangles = g_array_new (FALSE, FALSE, sizeof (XRectangle));

  priv->mask_texture = COGL_INVALID_HANDLE;
}

static void
mutter_shaped_texture_dispose (GObject *object)
{
  MutterShapedTexture *self = (MutterShapedTexture *) object;

  mutter_shaped_texture_dirty_mask (self);

  G_OBJECT_CLASS (mutter_shaped_texture_parent_class)->dispose (object);
}

static void
mutter_shaped_texture_finalize (GObject *object)
{
  MutterShapedTexture *self = (MutterShapedTexture *) object;
  MutterShapedTexturePrivate *priv = self->priv;

  g_array_free (priv->rectangles, TRUE);

  G_OBJECT_CLASS (mutter_shaped_texture_parent_class)->finalize (object);
}

static void
mutter_shaped_texture_dirty_mask (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv = stex->priv;

  if (priv->mask_texture != COGL_INVALID_HANDLE)
    {
      GLuint mask_gl_tex;
      GLenum mask_gl_target;

      cogl_texture_get_gl_texture (priv->mask_texture,
                                   &mask_gl_tex, &mask_gl_target);

      if (mask_gl_target == CGL_TEXTURE_RECTANGLE_ARB)
        glDeleteTextures (1, &mask_gl_tex);

      cogl_texture_unref (priv->mask_texture);
      priv->mask_texture = COGL_INVALID_HANDLE;
    }
}

static gboolean
mutter_shaped_texture_is_multi_tex_supported (void)
{
  const gchar *extensions;
  GLint max_tex_units = 0;

  if (tst_multi_tex_support != TST_MULTI_TEX_SUPPORT_UNKNOWN)
    return tst_multi_tex_support == TST_MULTI_TEX_SUPPORT_YES;

  extensions = (const gchar *) glGetString (GL_EXTENSIONS);

  tst_active_texture = (TstActiveTextureFunc)
    cogl_get_proc_address ("glActiveTextureARB");
  tst_client_active_texture = (TstClientActiveTextureFunc)
    cogl_get_proc_address ("glClientActiveTextureARB");

  glGetIntegerv (GL_MAX_TEXTURE_UNITS, &max_tex_units);

  if (extensions
      && cogl_check_extension ("GL_ARB_multitexture", extensions)
      && tst_active_texture
      && tst_client_active_texture
      && max_tex_units > 1)
    {
      tst_multi_tex_support = TST_MULTI_TEX_SUPPORT_YES;
      return TRUE;
    }
  else
    {
      g_warning ("multi texturing not supported");
      tst_multi_tex_support = TST_MULTI_TEX_SUPPORT_NO;
      return FALSE;
    }
}

static void
mutter_shaped_texture_set_coord_array (GLfloat x1, GLfloat y1,
				       GLfloat x2, GLfloat y2,
				       GLfloat *coords)
{
  coords[0] = x1;
  coords[1] = y2;
  coords[2] = x2;
  coords[3] = y2;
  coords[4] = x1;
  coords[5] = y1;
  coords[6] = x2;
  coords[7] = y1;
}

static void
mutter_shaped_texture_get_gl_size (CoglHandle tex,
				   guint *width,
				   guint *height)
{
  /* glGetTexLevelParameteriv isn't supported on GL ES so we need to
     calculate the size that Cogl has used */

  /* If NPOTs textures are supported then assume the GL texture is
     exactly the right size */
  if ((cogl_get_features () & COGL_FEATURE_TEXTURE_NPOT))
    {
      *width = cogl_texture_get_width (tex);
      *height = cogl_texture_get_height (tex);
    }
  /* Otherwise assume that Cogl has used the next power of two */
  else
    {
      guint tex_width = cogl_texture_get_width (tex);
      guint tex_height = cogl_texture_get_height (tex);
      guint real_width = 1;
      guint real_height = 1;

      while (real_width < tex_width)
        real_width <<= 1;
      while (real_height < tex_height)
        real_height <<= 1;

      *width = real_width;
      *height = real_height;
    }
}

static void
mutter_shaped_texture_ensure_mask (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv = stex->priv;
  CoglHandle paint_tex;
  guint tex_width, tex_height;
  GLuint mask_gl_tex;
  GLenum mask_target;

  paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

  if (paint_tex == COGL_INVALID_HANDLE)
    return;

  tex_width = cogl_texture_get_width (paint_tex);
  tex_height = cogl_texture_get_height (paint_tex);

  /* If the mask texture we have was created for a different size then
     recreate it */
  if (priv->mask_texture != COGL_INVALID_HANDLE
      && (priv->mask_width != tex_width || priv->mask_height != tex_height))
    mutter_shaped_texture_dirty_mask (stex);

  /* If we don't have a mask texture yet then create one */
  if (priv->mask_texture == COGL_INVALID_HANDLE)
    {
      guchar *mask_data;
      const XRectangle *rect;
      GLenum paint_gl_target;

      /* Create data for an empty image */
      mask_data = g_malloc0 (tex_width * tex_height);

      /* Cut out a hole for each rectangle */
      for (rect = (XRectangle *) priv->rectangles->data
             + priv->rectangles->len;
           rect-- > (XRectangle *) priv->rectangles->data;)
        {
          gint x1 = rect->x, x2 = x1 + rect->width;
          gint y1 = rect->y, y2 = y1 + rect->height;
          guchar *p;

          /* Clip the rectangle to the size of the texture */
          x1 = CLAMP (x1, 0, (gint) tex_width - 1);
          x2 = CLAMP (x2, x1, (gint) tex_width);
          y1 = CLAMP (y1, 0, (gint) tex_height - 1);
          y2 = CLAMP (y2, y1, (gint) tex_height);

          /* Fill the rectangle */
          for (p = mask_data + y1 * tex_width + x1;
               y1 < y2;
               y1++, p += tex_width)
            memset (p, 255, x2 - x1);
        }

      cogl_texture_get_gl_texture (paint_tex, NULL, &paint_gl_target);

      if (paint_gl_target == CGL_TEXTURE_RECTANGLE_ARB)
        {
          GLuint tex;

          glGenTextures (1, &tex);
          glBindTexture (CGL_TEXTURE_RECTANGLE_ARB, tex);
          glPixelStorei (GL_UNPACK_ROW_LENGTH, tex_width);
          glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
          glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);
          glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
          glTexImage2D (CGL_TEXTURE_RECTANGLE_ARB, 0,
                        GL_ALPHA, tex_width, tex_height,
                        0, GL_ALPHA, GL_UNSIGNED_BYTE, mask_data);

          priv->mask_texture
            = cogl_texture_new_from_foreign (tex,
                                             CGL_TEXTURE_RECTANGLE_ARB,
                                             tex_width, tex_height,
                                             0, 0,
                                             COGL_PIXEL_FORMAT_A_8);
        }
      else
        priv->mask_texture = cogl_texture_new_from_data (tex_width, tex_height,
                                                         -1, FALSE,
                                                         COGL_PIXEL_FORMAT_A_8,
                                                         COGL_PIXEL_FORMAT_ANY,
                                                         tex_width,
                                                         mask_data);

      g_free (mask_data);

      priv->mask_width = tex_width;
      priv->mask_height = tex_height;

      cogl_texture_get_gl_texture (priv->mask_texture, &mask_gl_tex, &mask_target);

      mutter_shaped_texture_get_gl_size (priv->mask_texture,
					 &priv->mask_gl_width,
					 &priv->mask_gl_height);

      if (mask_target == GL_TEXTURE_RECTANGLE_ARB)
	mutter_shaped_texture_set_coord_array (0.0f, 0.0f, tex_width, tex_height,
					       priv->mask_tex_coords);
      else if ((guint) priv->mask_gl_width == tex_width
	       && (guint) priv->mask_gl_height == tex_height)
        mutter_shaped_texture_set_coord_array (0.0f, 0.0f, 1.0f, 1.0f,
					       priv->mask_tex_coords);
      else
        mutter_shaped_texture_set_coord_array (0.0f, 0.0f,
					       tex_width
					       / (GLfloat) priv->mask_gl_width,
					       tex_height
					       / (GLfloat) priv->mask_gl_height,
					       priv->mask_tex_coords);
    }
}

static void
mutter_shaped_texture_paint (ClutterActor *actor)
{
  MutterShapedTexture *stex = (MutterShapedTexture *) actor;
  MutterShapedTexturePrivate *priv = stex->priv;
  CoglHandle paint_tex;
  guint tex_width, tex_height;
  GLboolean texture_was_enabled, blend_was_enabled;
  GLboolean vertex_array_was_enabled, tex_coord_array_was_enabled;
  GLboolean color_array_was_enabled;
  GLuint paint_gl_tex, mask_gl_tex;
  GLenum paint_target, mask_target;
  guint paint_gl_width, paint_gl_height;
  GLfloat vertex_coords[8], paint_tex_coords[8];
  ClutterActorBox alloc;
  static const ClutterColor white = { 0xff, 0xff, 0xff, 0xff };
#if 1 /* please see comment below about workaround */
  guint depth;
  GLint orig_gl_tex_env_mode;
  GLint orig_gl_combine_alpha;
  GLint orig_gl_src0_alpha;
  GLfloat orig_gl_tex_env_color[4];
  gboolean need_to_restore_tex_env = FALSE;
  const GLfloat const_alpha[4] = { 0.0, 0.0, 0.0, 1.0 };
#endif

  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (stex)))
    clutter_actor_realize (CLUTTER_ACTOR (stex));

  paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

  tex_width = cogl_texture_get_width (paint_tex);
  tex_height = cogl_texture_get_height (paint_tex);

  if (tex_width == 0 || tex_width == 0) /* no contents yet */
    return;

  /* If there are no rectangles or multi-texturing isn't supported,
     fallback to the regular paint method */
  if (priv->rectangles->len < 1
      || !mutter_shaped_texture_is_multi_tex_supported ())
    {
      CLUTTER_ACTOR_CLASS (mutter_shaped_texture_parent_class)
        ->paint (actor);
      return;
    }

  if (paint_tex == COGL_INVALID_HANDLE)
    return;

  /* If the texture is sliced then the multitexturing won't work */
  if (cogl_texture_is_sliced (paint_tex))
    {
      CLUTTER_ACTOR_CLASS (mutter_shaped_texture_parent_class)
        ->paint (actor);
      return;
    }

  mutter_shaped_texture_ensure_mask (stex);

  cogl_texture_get_gl_texture (paint_tex, &paint_gl_tex, &paint_target);
  cogl_texture_get_gl_texture (priv->mask_texture, &mask_gl_tex, &mask_target);

  /* We need to keep track of the some of the old state so that we
     don't confuse Cogl */
  texture_was_enabled = glIsEnabled (paint_target);
  blend_was_enabled = glIsEnabled (GL_BLEND);
  vertex_array_was_enabled = glIsEnabled (GL_VERTEX_ARRAY);
  tex_coord_array_was_enabled = glIsEnabled (GL_TEXTURE_COORD_ARRAY);
  color_array_was_enabled = glIsEnabled (GL_COLOR_ARRAY);

  glEnable (paint_target);
  glEnable (GL_BLEND);
  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glDisableClientState (GL_COLOR_ARRAY);
  glVertexPointer (2, GL_FLOAT, 0, vertex_coords);
  glTexCoordPointer (2, GL_FLOAT, 0, paint_tex_coords);
  cogl_color (&white);

  /* Put the main painting texture in the first texture unit */
  glBindTexture (paint_target, paint_gl_tex);

  /* We need the actual size of the texture so that we can calculate
     the right texture coordinates if NPOTs textures are not supported
     and Cogl has oversized the texture */
  mutter_shaped_texture_get_gl_size (paint_tex,
                                   &paint_gl_width,
                                   &paint_gl_height);

#if 1
  /* This was added as a workaround. It seems that with the intel drivers when
   * multi-texturing using an RGB TFP texture, the texture is actually
   * setup internally as an RGBA texture, where the alpha channel is mostly
   * 0.0 so you only see a shimmer of the window. This workaround forcibly
   * defines the alpha channel as 1.0. Maybe there is some clutter/cogl state
   * that is interacting with this that is being overlooked, but for now this
   * seems to work. */
  g_object_get (G_OBJECT (stex), "pixmap-depth", &depth, NULL);
  if (depth == 24)
    {
      glGetTexEnviv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                     &orig_gl_tex_env_mode);
      glGetTexEnviv (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, &orig_gl_combine_alpha);
      glGetTexEnviv (GL_TEXTURE_ENV, GL_SRC0_ALPHA, &orig_gl_src0_alpha);
      glGetTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                     orig_gl_tex_env_color);
      need_to_restore_tex_env = TRUE;

      glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
      glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
      glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, const_alpha);

      /* Replace the RGB in the second texture with that of the first
         texture */
      glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
      glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    }
#endif

  /* Put the mask texture in the second texture unit */
  tst_active_texture (GL_TEXTURE1);
  tst_client_active_texture (GL_TEXTURE1);
  glBindTexture (mask_target, mask_gl_tex);

  glEnable (mask_target);

  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  glTexCoordPointer (2, GL_FLOAT, 0, priv->mask_tex_coords);

  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

#if 1 /* see workaround notes above */
  if (depth == 24)
    {
      /* NOTE: This should be redundant, since we already explicitly forced an
       * an alhpa value of 1.0 for texture unit 0, but this workaround only
       * seems to help if we explicitly force the alpha values for both texture
       * units. */
      /* XXX - we should also save/restore the values for this texture unit too
       */
      glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
      glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
      glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_CONSTANT);
      glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, const_alpha);
    }
  else
    {
#endif

  /* Multiply the alpha by the alpha in the second texture */
  glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
  glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
  glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PREVIOUS);

#if 1 /* see workaround notes above */
    }
#endif

  /* Replace the RGB in the second texture with that of the first
     texture */
  glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
  glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);

  clutter_actor_get_allocation_box (actor, &alloc);

  mutter_shaped_texture_set_coord_array (0, 0,
					 CLUTTER_UNITS_TO_FLOAT (alloc.x2
								 - alloc.x1),
					 CLUTTER_UNITS_TO_FLOAT (alloc.y2
					                         - alloc.y1),
					 vertex_coords);

  if (paint_target == GL_TEXTURE_RECTANGLE_ARB)
    mutter_shaped_texture_set_coord_array (0.0f, 0.0f, tex_width, tex_height,
					   paint_tex_coords);
  else if ((guint) paint_gl_width == tex_width
	&& (guint) paint_gl_height == tex_height)
    mutter_shaped_texture_set_coord_array (0.0f, 0.0f, 1.0f, 1.0f,
                                         paint_tex_coords);
  else
    mutter_shaped_texture_set_coord_array (0.0f, 0.0f,
					   tex_width
					   / (GLfloat) paint_gl_width,
					   tex_height
					   / (GLfloat) paint_gl_height,
					   paint_tex_coords);

  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

  /* Disable the second texture unit and coord array */
  glDisable (mask_target);
  glDisableClientState (GL_TEXTURE_COORD_ARRAY);

  /* Go back to operating on the first texture unit */
  tst_active_texture (GL_TEXTURE0);
  tst_client_active_texture (GL_TEXTURE0);

  /* Restore the old state */
  if (!texture_was_enabled)
    glDisable (paint_target);
  if (!blend_was_enabled)
    glDisable (GL_BLEND);
  if (!vertex_array_was_enabled)
    glDisableClientState (GL_VERTEX_ARRAY);
  if (!tex_coord_array_was_enabled)
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  if (color_array_was_enabled)
    glEnableClientState (GL_COLOR_ARRAY);
#if 1 /* see note about workaround above */
  if (need_to_restore_tex_env)
    {
      glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, orig_gl_tex_env_mode);
      glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, orig_gl_combine_alpha);
      glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA, orig_gl_src0_alpha);
      glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, orig_gl_tex_env_color);
    }
#endif
}

static void
mutter_shaped_texture_pick (ClutterActor *actor,
			    const ClutterColor *color)
{
  MutterShapedTexture *stex = (MutterShapedTexture *) actor;
  MutterShapedTexturePrivate *priv = stex->priv;

  /* If there are no rectangles then use the regular pick */
  if (priv->rectangles->len < 1
      || !mutter_shaped_texture_is_multi_tex_supported ())
    CLUTTER_ACTOR_CLASS (mutter_shaped_texture_parent_class)
      ->pick (actor, color);
  else if (clutter_actor_should_pick_paint (actor))
    {
      CoglHandle paint_tex;
      ClutterActorBox alloc;
      guint tex_width, tex_height;

      paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

      if (paint_tex == COGL_INVALID_HANDLE)
        return;

      tex_width = cogl_texture_get_width (paint_tex);
      tex_height = cogl_texture_get_height (paint_tex);

      if (tex_width == 0 || tex_width == 0) /* no contents yet */
	return;

      mutter_shaped_texture_ensure_mask (stex);

      cogl_color (color);

      clutter_actor_get_allocation_box (actor, &alloc);

      /* Paint the mask rectangle in the given color */
      cogl_texture_rectangle (priv->mask_texture,
                              0, 0,
                              CLUTTER_UNITS_TO_FIXED (alloc.x2 - alloc.x1),
                              CLUTTER_UNITS_TO_FIXED (alloc.y2 - alloc.y1),
                              0, 0, CFX_ONE, CFX_ONE);
    }
}

ClutterActor *
mutter_shaped_texture_new (void)
{
  ClutterActor *self = g_object_new (MUTTER_TYPE_SHAPED_TEXTURE, NULL);

  return self;
}

void
mutter_shaped_texture_clear_rectangles (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_array_set_size (priv->rectangles, 0);
  mutter_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

void
mutter_shaped_texture_add_rectangle (MutterShapedTexture *stex,
				     const XRectangle *rect)
{
  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  mutter_shaped_texture_add_rectangles (stex, 1, rect);
}

void
mutter_shaped_texture_add_rectangles (MutterShapedTexture *stex,
				      size_t num_rects,
				      const XRectangle *rects)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_array_append_vals (priv->rectangles, rects, num_rects);

  mutter_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}
