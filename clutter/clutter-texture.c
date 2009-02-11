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
 * SECTION:clutter-texture
 * @short_description: An actor for displaying and manipulating images.
 *
 * #ClutterTexture is a base class for displaying and manipulating pixel
 * buffer type data.
 *
 * The clutter_texture_set_from_rgb_data() and
 * clutter_texture_set_from_file() functions are used to copy image
 * data into texture memory and subsequently realize the texture.
 *
 * If texture reads are supported by underlying GL implementation,
 * unrealizing frees image data from texture memory moving to main
 * system memory. Re-realizing then performs the opposite operation.
 * This process allows basic management of commonly limited available
 * texture memory.
 *
 * Note: a ClutterTexture will scale its contents to fit the bounding
 * box requested using clutter_actor_set_size(). To display an area of
 * a texture without scaling, you should set the clip area using
 * clutter_actor_set_clip().
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-texture.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-feature.h"
#include "clutter-util.h"
#include "clutter-private.h"
#include "clutter-scriptable.h"
#include "clutter-debug.h"
#include "clutter-fixed.h"
#include "clutter-enum-types.h"

#include "cogl/cogl.h"

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterTexture,
                         clutter_texture,
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_iface_init));

#define CLUTTER_TEXTURE_GET_PRIVATE(obj)        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_TEXTURE, ClutterTexturePrivate))

struct _ClutterTexturePrivate
{
  gint                         width;
  gint                         height;
  gint                         max_tile_waste;
  ClutterTextureQuality        filter_quality;
  CoglHandle                   material;
  CoglHandle                   fbo_texture;
  gboolean                     no_slice;

  ClutterActor                *fbo_source;
  CoglHandle                   fbo_handle;

  /* Non video memory copy of image data */
  guint                        local_data_width, local_data_height;
  guint                        local_data_rowstride;
  guint                        local_data_has_alpha;
  guchar                      *local_data;

  guint                        sync_actor_size : 1;
  guint                        repeat_x : 1;
  guint                        repeat_y : 1;
  guint                        in_dispose : 1;
  guint                        keep_aspect_ratio : 1;
  guint                        load_async : 1;
  
  GThread                     *load_thread;
  guint                        load_idle;
  gchar                       *load_filename;
  CoglBitmap                  *load_bitmap;
  GError                      *load_error;
};

enum
{
  PROP_0,
  PROP_NO_SLICE,
  PROP_MAX_TILE_WASTE,
  PROP_PIXEL_FORMAT,		/* Texture format */
  PROP_SYNC_SIZE,
  PROP_REPEAT_Y,
  PROP_REPEAT_X,
  PROP_FILTER_QUALITY,
  PROP_COGL_TEXTURE,
#if EXPOSE_COGL_MATERIAL_PROP
  PROP_COGL_MATERIAL,
#endif
  PROP_FILENAME,
  PROP_KEEP_ASPECT_RATIO,
  PROP_LOAD_ASYNC
};

enum
{
  SIZE_CHANGE,
  PIXBUF_CHANGE,
  LOAD_SUCCESS,
  LOAD_FINISHED,
  
  LAST_SIGNAL
};

static int texture_signals[LAST_SIGNAL] = { 0 };

static void
texture_fbo_free_resources (ClutterTexture *texture);

static void
clutter_texture_save_to_local_data (ClutterTexture *texture);

static void
clutter_texture_load_from_local_data (ClutterTexture *texture);

GQuark
clutter_texture_error_quark (void)
{
  return g_quark_from_static_string ("clutter-texture-error-quark");
}

static inline void
clutter_texture_quality_to_filters (ClutterTextureQuality  quality,
                                    gint                  *min_filter_p,
                                    gint                  *mag_filter_p)
{
  gint min_filter, mag_filter;

  switch (quality)
    {
    case CLUTTER_TEXTURE_QUALITY_LOW:
      min_filter = CGL_NEAREST;
      mag_filter = CGL_NEAREST;
      break;

    case CLUTTER_TEXTURE_QUALITY_MEDIUM:
      min_filter = CGL_LINEAR;
      mag_filter = CGL_LINEAR;
      break;

    case CLUTTER_TEXTURE_QUALITY_HIGH:
      min_filter = CGL_LINEAR_MIPMAP_LINEAR;
      mag_filter = CGL_LINEAR;
      break;
    }

  if (min_filter_p)
    *min_filter_p = min_filter;

  if (mag_filter_p)
    *mag_filter_p = mag_filter;
}

static void
texture_free_gl_resources (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv = texture->priv;

  CLUTTER_MARK();

  if (priv->material != COGL_INVALID_HANDLE)
    cogl_material_remove_layer (priv->material, 0);
}

static void
clutter_texture_unrealize (ClutterActor *actor)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(actor);
  priv = texture->priv;

  if (priv->material == COGL_INVALID_HANDLE)
    return;

  /* there's no need to read the pixels back when unrealizing inside
   * a dispose run, and the dispose() call will release the GL
   * texture data as well, so we can safely bail out now
   */
  if ((CLUTTER_PRIVATE_FLAGS (actor) & CLUTTER_ACTOR_IN_DESTRUCTION) ||
      priv->in_dispose)
    return;

  CLUTTER_MARK();

  if (priv->fbo_source != COGL_INVALID_HANDLE)
    {
      /* Free up our fbo handle and texture resources, realize will recreate */
      cogl_offscreen_unref (priv->fbo_handle);
      priv->fbo_handle = COGL_INVALID_HANDLE;
      texture_free_gl_resources (texture);
      return;
    }

  if (clutter_feature_available (CLUTTER_FEATURE_TEXTURE_READ_PIXELS))
    {
      /* Move image data from video to main memory.
       * GL/ES cant do this - it probably makes sense
       * to move this kind of thing into a ClutterProxyTexture
       * where this behaviour can be better controlled.
       *
       * Or make it controllable via a property.
       */
      if (priv->local_data == NULL)
	{
	  clutter_texture_save_to_local_data (texture);
	  CLUTTER_NOTE (TEXTURE, "moved pixels into system mem");
	}

      texture_free_gl_resources (texture);
    }

  CLUTTER_NOTE (TEXTURE, "Texture unrealized");
}

static void
clutter_texture_realize (ClutterActor *actor)
{
  ClutterTexture       *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(actor);
  priv = texture->priv;

  CLUTTER_MARK();

  if (priv->fbo_source)
    {
      CoglTextureFlags flags = COGL_TEXTURE_NONE;
      gint min_filter, mag_filter;
      gint max_waste = -1;

      /* Handle FBO's */

      if (priv->fbo_texture != COGL_INVALID_HANDLE)
	cogl_texture_unref (priv->fbo_texture);

      if (!priv->no_slice)
        max_waste = priv->max_tile_waste;

      if (priv->filter_quality == CLUTTER_TEXTURE_QUALITY_HIGH)
        flags |= COGL_TEXTURE_AUTO_MIPMAP;

      priv->fbo_texture =
        cogl_texture_new_with_size (priv->width,
                                    priv->height,
                                    max_waste, flags,
                                    COGL_PIXEL_FORMAT_RGBA_8888);

      clutter_texture_quality_to_filters (priv->filter_quality,
                                          &min_filter,
                                          &mag_filter);

      cogl_texture_set_filters (priv->fbo_texture, min_filter, mag_filter);

      priv->fbo_handle = cogl_offscreen_new_to_texture (priv->fbo_texture);

      if (priv->fbo_handle == COGL_INVALID_HANDLE)
        {
          g_warning ("%s: Offscreen texture creation failed", G_STRLOC);
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
          return;
        }

      clutter_actor_set_size (actor, priv->width, priv->height);
      return;
    }

  if (priv->local_data != NULL)
    {
      /* Move any local image data we have from unrealization
       * back into video memory.
      */
      clutter_texture_load_from_local_data (texture);
    }
  else
    {
      if (clutter_feature_available (CLUTTER_FEATURE_TEXTURE_READ_PIXELS))
	{
	  /* Dont allow realization with no data - note set_data
	   * will set realize flags.
	   */
	  CLUTTER_NOTE (TEXTURE,
			"Texture has no image data cannot realize");

	  CLUTTER_NOTE (TEXTURE, "flags %i", actor->flags);
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  CLUTTER_NOTE (TEXTURE, "flags %i", actor->flags);
	  return;
	}
    }

  CLUTTER_NOTE (TEXTURE, "Texture realized");
}

static void
clutter_texture_get_preferred_width (ClutterActor *self,
                                     ClutterUnit   for_height,
                                     ClutterUnit  *min_width_p,
                                     ClutterUnit  *natural_width_p)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (self);
  ClutterTexturePrivate *priv = texture->priv;

  /* Min request is always 0 since we can scale down or clip */
  if (min_width_p)
    *min_width_p = 0;

  if (priv->sync_actor_size)
    {
      if (natural_width_p)
        {
          if (!priv->keep_aspect_ratio ||
              for_height < 0 ||
              priv->height <= 0)
            {
              *natural_width_p = CLUTTER_UNITS_FROM_DEVICE (priv->width);
            }
          else
            {
              /* Set the natural width so as to preserve the aspect ratio */
              ClutterFixed ratio, height;

              ratio = CLUTTER_FIXED_DIV ((float)(priv->width),
                                      (float)(priv->height));

              height = CLUTTER_UNITS_TO_FIXED (for_height);

              *natural_width_p =
                CLUTTER_UNITS_FROM_FIXED (CLUTTER_FIXED_MUL (ratio, height));
            }
        }
    }
  else
    {
      if (natural_width_p)
        *natural_width_p = 0;
    }
}

static void
clutter_texture_get_preferred_height (ClutterActor *self,
                                      ClutterUnit   for_width,
                                      ClutterUnit  *min_height_p,
                                      ClutterUnit  *natural_height_p)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (self);
  ClutterTexturePrivate *priv = texture->priv;

  /* Min request is always 0 since we can scale down or clip */
  if (min_height_p)
    *min_height_p = 0;

  if (priv->sync_actor_size)
    {
      if (natural_height_p)
        {
          if (!priv->keep_aspect_ratio ||
              for_width < 0 ||
              priv->width <= 0)
            {
              *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (priv->height);
            }
          else
            {
              /* Set the natural height so as to preserve the aspect ratio */
              ClutterFixed ratio, width;

              ratio = CLUTTER_FIXED_DIV ((float)(priv->height),
                                      (float)(priv->width));

              width = CLUTTER_UNITS_TO_FIXED (for_width);

              *natural_height_p =
                CLUTTER_UNITS_FROM_FIXED (CLUTTER_FIXED_MUL (ratio, width));
            }
        }
    }
  else
    {
      if (natural_height_p)
        *natural_height_p = 0;
    }
}

static void
clutter_texture_allocate (ClutterActor          *self,
			  const ClutterActorBox *box,
			  gboolean               origin_changed)
{
  ClutterTexturePrivate *priv = CLUTTER_TEXTURE (self)->priv;

  /* chain up to set actor->allocation */
  CLUTTER_ACTOR_CLASS (clutter_texture_parent_class)->allocate (self, box,
								origin_changed);

  /* If we adopted the source fbo then allocate that at its preferred
     size */
  if (priv->fbo_source && clutter_actor_get_parent (priv->fbo_source) == self)
    clutter_actor_allocate_preferred_size (priv->fbo_source, origin_changed);
}

static void
clutter_texture_set_fbo_projection (ClutterActor *self)
{
  ClutterTexturePrivate *priv = CLUTTER_TEXTURE (self)->priv;
  ClutterVertex verts[4];
  ClutterFixed viewport[4];
  ClutterUnit x_min, x_max, y_min, y_max;
  ClutterFixed tx_min, tx_max, ty_min, ty_max;
  ClutterPerspective perspective;
  ClutterStage *stage;
  ClutterFixed tan_angle, near_size;
  int i;

  /* Get the bounding rectangle of the source as drawn in screen
     coordinates */
  clutter_actor_get_abs_allocation_vertices (priv->fbo_source, verts);

  x_min = x_max = verts[0].x;
  y_min = y_max = verts[0].y;

  for (i = 1; i < G_N_ELEMENTS (verts); ++i)
    {
      if (verts[i].x < x_min)
	x_min = verts[i].x;

      if (verts[i].x > x_max)
	x_max = verts[i].x;

      if (verts[i].y < y_min)
	y_min = verts[i].y;

      if (verts[i].y > y_max)
	y_max = verts[i].y;
    }

  stage = CLUTTER_STAGE (clutter_actor_get_stage (self));
  clutter_stage_get_perspectivex (stage, &perspective);

  /* Convert the coordinates back to [-1,1] range */
  cogl_get_viewport (viewport);

  tx_min = CLUTTER_FIXED_DIV (CLUTTER_UNITS_TO_FIXED (x_min), viewport[2])
         * 2 - 1.0;
  tx_max = CLUTTER_FIXED_DIV (CLUTTER_UNITS_TO_FIXED (x_max), viewport[2])
         * 2 - 1.0;
  ty_min = CLUTTER_FIXED_DIV (CLUTTER_UNITS_TO_FIXED (y_min), viewport[3])
         * 2 - 1.0;
  ty_max = CLUTTER_FIXED_DIV (CLUTTER_UNITS_TO_FIXED (y_max), viewport[3])
         * 2 - 1.0;

  /* Set up a projection matrix so that the actor will be projected as
     if it was drawn at its original location */
  tan_angle = tanf ((perspective.fovy / 2) * (G_PI/180.0));
  near_size = CLUTTER_FIXED_MUL (perspective.z_near, tan_angle);

  cogl_frustum (CLUTTER_FIXED_MUL (tx_min, near_size),
                CLUTTER_FIXED_MUL (tx_max, near_size),
                CLUTTER_FIXED_MUL (-ty_min, near_size),
                CLUTTER_FIXED_MUL (-ty_max, near_size),
                perspective.z_near, perspective.z_far);
}

static void
clutter_texture_paint (ClutterActor *self)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (self);
  ClutterTexturePrivate *priv = texture->priv;
  gint            x_1, y_1, x_2, y_2;
  CoglColor       transparent_col;
  ClutterFixed    t_w, t_h;

  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR(texture)))
    clutter_actor_realize (CLUTTER_ACTOR(texture));

  if (priv->fbo_handle != COGL_INVALID_HANDLE)
    {
      ClutterMainContext *context;
      ClutterShader      *shader = NULL;
      ClutterActor       *stage = NULL;
      ClutterPerspective  perspective;

      context = clutter_context_get_default ();

      if (context->shaders)
        shader = clutter_actor_get_shader (context->shaders->data);

      /* Temporarily turn of the shader on the top of the context's
       * shader stack, to restore the GL pipeline to it's natural state.
       */
      if (shader)
        clutter_shader_set_is_enabled (shader, FALSE);

      /* Redirect drawing to the fbo */
      cogl_draw_buffer (COGL_OFFSCREEN_BUFFER, priv->fbo_handle);

      if ((stage = clutter_actor_get_stage (self)))
	{
	  guint               stage_width, stage_height;
	  ClutterActor       *source_parent;

	  clutter_stage_get_perspectivex (CLUTTER_STAGE (stage), &perspective);
	  clutter_actor_get_size (stage, &stage_width, &stage_height);

	  /* Use below to set the modelview matrix as if the viewport
	     was still the same size as the stage */
	  cogl_setup_viewport (stage_width, stage_height,
			       perspective.fovy,
			       perspective.aspect,
			       perspective.z_near,
			       perspective.z_far);
	  /* Use a projection matrix that makes the actor appear as it
	     would if it was rendered at its normal screen location */
	  clutter_texture_set_fbo_projection (self);
	  /* Reset the viewport to the size of the FBO */
	  cogl_viewport (priv->width, priv->height);
	  /* Reapply the source's parent transformations */
	  if ((source_parent = clutter_actor_get_parent (priv->fbo_source)))
	    _clutter_actor_apply_modelview_transform_recursive (source_parent,
								NULL);
	}

      /* cogl_paint_init is called to clear the buffers */
      cogl_color_set_from_4ub (&transparent_col, 0, 0, 0, 0);
      cogl_paint_init (&transparent_col);

      /* Clear the clipping stack so that if the FBO actor is being
	 clipped then it won't affect drawing the source */
      cogl_clip_stack_save ();

      /* Render out actor scene to fbo */
      clutter_actor_paint (priv->fbo_source);

      cogl_clip_stack_restore ();

      /* Restore drawing to the frame buffer */
      cogl_draw_buffer (COGL_WINDOW_BUFFER, COGL_INVALID_HANDLE);

      /* Restore the perspective matrix using cogl_perspective so that
	 the inverse matrix will be right */
      cogl_perspective (perspective.fovy,
                        perspective.aspect,
                        perspective.z_near,
                        perspective.z_far);

      /* If there is a shader on top of the shader stack, turn it back on. */
      if (shader)
        clutter_shader_set_is_enabled (shader, TRUE);
    }

  /* A clone may need to fire above if were a TFP/FBO but not visible.
   * Ultimatly needs some reworking with maybe an extra prepare_paint
   * method or some such.
  */
  if (CLUTTER_PRIVATE_FLAGS(self) & CLUTTER_TEXTURE_IN_CLONE_PAINT)
    return;

  CLUTTER_NOTE (PAINT,
                "painting texture '%s'",
		clutter_actor_get_name (self) ? clutter_actor_get_name (self)
                                              : "unknown");

  cogl_material_set_color4ub (priv->material, 0xff, 0xff, 0xff,
                              clutter_actor_get_paint_opacity (self));

  clutter_actor_get_allocation_coords (self, &x_1, &y_1, &x_2, &y_2);

  CLUTTER_NOTE (PAINT, "paint to x1: %i, y1: %i x2: %i, y2: %i "
		       "opacity: %i",
		x_1, y_1, x_2, y_2,
		clutter_actor_get_opacity (self));

  if (priv->repeat_x && priv->width > 0)
    t_w = CLUTTER_FIXED_DIV ((float)(x_2 - x_1),
		          (float)(priv->width));
  else
    t_w = 1.0;

  if (priv->repeat_y && priv->height > 0)
    t_h = CLUTTER_FIXED_DIV ((float)(y_2 - y_1),
                          (float)(priv->height));
  else
    t_h = 1.0;

  /* Paint will have translated us */
  cogl_set_source (priv->material);
  cogl_rectangle_with_texture_coords (0, 0,
			              (float) (x_2 - x_1),
			              (float) (y_2 - y_1),
			              0, 0, t_w, t_h);
}

/*
 * clutter_texture_async_load_cancel:
 * @texture: a #ClutterTexture
 *
 * Cancels an asynchronous loading operation, whether done
 * with threads enabled or just using the main loop
 */
static void
clutter_texture_async_load_cancel (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv = texture->priv;

  if (priv->load_thread)
    {
      g_thread_join (priv->load_thread);
      priv->load_thread = NULL;
    }

  if (priv->load_idle)
    {
      g_source_remove (priv->load_idle);
      priv->load_idle = 0;
    }

  if (priv->load_error)
    {
      g_error_free (priv->load_error);
      priv->load_error = NULL;
    }

  if (priv->load_bitmap)
    {
      cogl_bitmap_free (priv->load_bitmap);
      priv->load_bitmap = NULL;
    }

  g_free (priv->load_filename);
}

static void
clutter_texture_dispose (GObject *object)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (object);
  ClutterTexturePrivate *priv;

  priv = texture->priv;

  /* mark that we are in dispose, so when the parent class'
   * dispose implementation will call unrealize on us we'll
   * not try to copy back the resources from video memory
   * to system memory
   */
  if (!priv->in_dispose)
    priv->in_dispose = TRUE;

  texture_free_gl_resources (texture);
  texture_fbo_free_resources (texture);

  if (priv->local_data != NULL)
    {
      g_free (priv->local_data);
      priv->local_data = NULL;
    }

  clutter_texture_async_load_cancel (texture);

  G_OBJECT_CLASS (clutter_texture_parent_class)->dispose (object);
}

static void
clutter_texture_finalize (GObject *object)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (object);
  ClutterTexturePrivate *priv = texture->priv;

  if (priv->material != COGL_INVALID_HANDLE)
    {
      cogl_material_unref (priv->material);
      priv->material = COGL_INVALID_HANDLE;
    }

  G_OBJECT_CLASS (clutter_texture_parent_class)->finalize (object);
}

static void
clutter_texture_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(object);
  priv = texture->priv;

  switch (prop_id)
    {
    case PROP_MAX_TILE_WASTE:
      clutter_texture_set_max_tile_waste (texture,
					  g_value_get_int (value));
      break;
    case PROP_SYNC_SIZE:
      priv->sync_actor_size = g_value_get_boolean (value);
      clutter_actor_queue_relayout (CLUTTER_ACTOR (texture));
      break;
    case PROP_REPEAT_X:
      if (priv->repeat_x != g_value_get_boolean (value))
	{
	  priv->repeat_x = !priv->repeat_x;
	  if (CLUTTER_ACTOR_IS_VISIBLE (texture))
	    clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));
	}
      break;
    case PROP_REPEAT_Y:
      if (priv->repeat_y != g_value_get_boolean (value))
	{
	  priv->repeat_y = !priv->repeat_y;
	  if (CLUTTER_ACTOR_IS_VISIBLE (texture))
	    clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));
	}
      break;
    case PROP_FILTER_QUALITY:
      clutter_texture_set_filter_quality (texture,
					  g_value_get_enum (value));
      break;
    case PROP_COGL_TEXTURE:
      clutter_texture_set_cogl_texture
	(texture, (CoglHandle) g_value_get_boxed (value));
      break;
#if EXPOSE_COGL_MATERIAL_PROP
    case PROP_COGL_MATERIAL:
      clutter_texture_set_cogl_material
        (texture, (CoglHandle) g_value_get_boxed (value));
      break;
#endif
    case PROP_FILENAME:
      clutter_texture_set_from_file (texture,
                                     g_value_get_string (value),
                                     NULL);
      break;
    case PROP_NO_SLICE:
      priv->no_slice = g_value_get_boolean (value);
      break;
    case PROP_KEEP_ASPECT_RATIO:
      priv->keep_aspect_ratio = g_value_get_boolean (value);
      break;
    case PROP_LOAD_ASYNC:
      priv->load_async = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_texture_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;
  CoglHandle             cogl_texture;

  texture = CLUTTER_TEXTURE(object);
  priv = texture->priv;

  switch (prop_id)
    {
    case PROP_MAX_TILE_WASTE:
      g_value_set_int (value, clutter_texture_get_max_tile_waste (texture));
      break;
    case PROP_PIXEL_FORMAT:
      cogl_texture = clutter_texture_get_cogl_texture (texture);
      if (cogl_texture == COGL_INVALID_HANDLE)
	g_value_set_int (value, COGL_PIXEL_FORMAT_ANY);
      else
	g_value_set_int (value, cogl_texture_get_format (cogl_texture));
      break;
    case PROP_SYNC_SIZE:
      g_value_set_boolean (value, priv->sync_actor_size);
      break;
    case PROP_REPEAT_X:
      g_value_set_boolean (value, priv->repeat_x);
      break;
    case PROP_REPEAT_Y:
      g_value_set_boolean (value, priv->repeat_y);
      break;
    case PROP_FILTER_QUALITY:
      g_value_set_enum (value, clutter_texture_get_filter_quality (texture));
      break;
    case PROP_COGL_TEXTURE:
      g_value_set_boxed (value, clutter_texture_get_cogl_texture (texture));
      break;
#if EXPOSE_COGL_MATERIAL_PROP
    case PROP_COGL_MATERIAL:
      g_value_set_boxed (value, clutter_texture_get_cogl_material (texture));
      break;
#endif
    case PROP_NO_SLICE:
      g_value_set_boolean (value, priv->no_slice);
      break;
    case PROP_KEEP_ASPECT_RATIO:
      g_value_set_boolean (value, priv->keep_aspect_ratio);
      break;
    case PROP_LOAD_ASYNC:
      g_value_set_boolean (value, priv->load_async);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_texture_class_init (ClutterTextureClass *klass)
{
  GObjectClass        *gobject_class;
  ClutterActorClass *actor_class;

  gobject_class = (GObjectClass*) klass;
  actor_class = (ClutterActorClass*) klass;

  g_type_class_add_private (klass, sizeof (ClutterTexturePrivate));

  actor_class->paint          = clutter_texture_paint;
  actor_class->realize        = clutter_texture_realize;
  actor_class->unrealize      = clutter_texture_unrealize;

  actor_class->get_preferred_width  = clutter_texture_get_preferred_width;
  actor_class->get_preferred_height = clutter_texture_get_preferred_height;
  actor_class->allocate             = clutter_texture_allocate;

  gobject_class->dispose      = clutter_texture_dispose;
  gobject_class->finalize     = clutter_texture_finalize;
  gobject_class->set_property = clutter_texture_set_property;
  gobject_class->get_property = clutter_texture_get_property;

  g_object_class_install_property
    (gobject_class, PROP_SYNC_SIZE,
     g_param_spec_boolean ("sync-size",
			   "Sync size of actor",
			   "Auto sync size of actor to underlying pixbuf "
			   "dimensions",
			   TRUE,
			   CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_NO_SLICE,
     g_param_spec_boolean ("disable-slicing",
			   "Disable Slicing",
			   "Force the underlying texture to be singlular"
			   "and not made of of smaller space saving "
                           "inidivual textures.",
			   FALSE,
			   G_PARAM_CONSTRUCT_ONLY | CLUTTER_PARAM_READWRITE));


  g_object_class_install_property
    (gobject_class, PROP_REPEAT_X,
     g_param_spec_boolean ("repeat-x",
			   "Tile underlying pixbuf in x direction",
			   "Repeat underlying pixbuf rather than scale "
			   "in x direction.",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_REPEAT_Y,
     g_param_spec_boolean ("repeat-y",
			   "Tile underlying pixbuf in y direction",
			   "Repeat underlying pixbuf rather than scale "
			   "in y direction.",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_FILTER_QUALITY,
     g_param_spec_enum ("filter-quality",
                       "Filter Quality",
                       "Rendering quality used when drawing the texture.",
                       CLUTTER_TYPE_TEXTURE_QUALITY,
		       CLUTTER_TEXTURE_QUALITY_MEDIUM,
		       G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_MAX_TILE_WASTE,
     g_param_spec_int ("tile-waste",
		       "Tile dimension to waste",
		       "Max wastage dimension of a texture when using "
		       "sliced textures or -1 to disable slicing. "
		       "Bigger values use less textures, "
		       "smaller values less texture memory.",
		       -1,
		       G_MAXINT,
		       63,
		       G_PARAM_CONSTRUCT_ONLY | CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_PIXEL_FORMAT,
     g_param_spec_int ("pixel-format",
		       "Texture pixel format",
		       "CoglPixelFormat to use.",
		       0,
		       G_MAXINT,
		       COGL_PIXEL_FORMAT_RGBA_8888,
		       G_PARAM_READABLE));

  g_object_class_install_property
    (gobject_class, PROP_COGL_TEXTURE,
     g_param_spec_boxed ("cogl-texture",
			 "COGL Texture",
			 "The underlying COGL texture handle used to draw "
			 "this actor",
			 CLUTTER_TYPE_TEXTURE_HANDLE,
			 G_PARAM_READWRITE));

#if EXPOSE_COGL_MATERIAL_PROP
  g_object_class_install_property
    (gobject_class, PROP_COGL_MATERIAL,
     g_param_spec_boxed ("cogl-material",
			 "COGL Material",
			 "The underlying COGL material handle used to draw "
			 "this actor",
			 CLUTTER_TYPE_MATERIAL_HANDLE,
			 G_PARAM_READWRITE));
#endif

  g_object_class_install_property
    (gobject_class, PROP_FILENAME,
     g_param_spec_string ("filename",
                          "Filename",
                          "The full path of the file containing the texture",
                          NULL,
                          G_PARAM_WRITABLE));

  g_object_class_install_property
    (gobject_class, PROP_KEEP_ASPECT_RATIO,
     g_param_spec_boolean ("keep-aspect-ratio",
			   "Keep Aspect Ratio",
			   "Keep the aspect ratio of the texture when "
			   "requesting the preferred width or height",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

  /**
   * ClutterTexture:load-async:
   *
   * Tries to load a texture from a filename by using a local thread
   * to perform the read operations. Threading is only enabled if
   * g_thread_init() has been called prior to clutter_init(), otherwise
   * #ClutterTexture will use the main loop to load the image.
   *
   * The upload of the texture data on the GL pipeline is not
   * asynchronous, as it must be performed from within the same
   * thread that called clutter_main().
   *
   * Since: 1.0
   */
  g_object_class_install_property
    (gobject_class, PROP_LOAD_ASYNC,
     g_param_spec_boolean ("load-async",
			   "Load asynchronously",
			   "Load files inside a thread to avoid blocking when "
                           "loading images.",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

  /**
   * ClutterTexture::size-change:
   * @texture: the texture which received the signal
   * @width: the width of the new texture
   * @height: the height of the new texture
   *
   * The ::size-change signal is emitted each time the size of the
   * pixbuf used by @texture changes.  The new size is given as
   * argument to the callback.
   */
  texture_signals[SIZE_CHANGE] =
    g_signal_new ("size-change",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTextureClass, size_change),
		  NULL, NULL,
		  clutter_marshal_VOID__INT_INT,
		  G_TYPE_NONE,
		  2, G_TYPE_INT, G_TYPE_INT);
  /**
   * ClutterTexture::pixbuf-change:
   * @texture: the texture which received the signal
   *
   * The ::pixbuf-change signal is emitted each time the pixbuf
   * used by @texture changes.
   */
  texture_signals[PIXBUF_CHANGE] =
    g_signal_new ("pixbuf-change",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTextureClass, pixbuf_change),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
  /**
   * ClutterTexture::load-finished:
   * @texture: the texture which received the signal
   * @error: A set error, or %NULL
   *
   * The ::load-finished signal is emitted when a texture load has
   * completed. If there was an error during loading, @error will
   * be set, otherwise it will be %NULL
   *
   * Since: 1.0
   */
  texture_signals[LOAD_FINISHED] =
    g_signal_new (I_("load-finished"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTextureClass, load_finished),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE,
		  1,
                  G_TYPE_POINTER);
}

static ClutterScriptableIface *parent_scriptable_iface = NULL;

static void
clutter_texture_set_custom_property (ClutterScriptable *scriptable,
                                     ClutterScript     *script,
                                     const gchar       *name,
                                     const GValue      *value)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (scriptable);

  if (strcmp ("filename", name) == 0)
    {
      const gchar *str = g_value_get_string (value);
      gchar *path;
      GError *error;

      path = clutter_script_lookup_filename (script, str);
      if (G_UNLIKELY (!path))
        return;

      error = NULL;
      clutter_texture_set_from_file (texture, path, &error);
      if (error)
        {
          g_warning ("Unable to open image path at `%s': %s",
                     path,
                     error->message);
          g_error_free (error);
        }

      g_free (path);
    }
  else
    {
      /* chain up */
      if (parent_scriptable_iface->set_custom_property)
        parent_scriptable_iface->set_custom_property (scriptable, script,
                                                      name,
                                                      value);
    }
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  parent_scriptable_iface = g_type_interface_peek_parent (iface);

  if (!parent_scriptable_iface)
    parent_scriptable_iface = g_type_default_interface_peek
                                          (CLUTTER_TYPE_SCRIPTABLE);

  iface->set_custom_property = clutter_texture_set_custom_property;
}

static void
clutter_texture_init (ClutterTexture *self)
{
  ClutterTexturePrivate *priv;

  self->priv = priv = CLUTTER_TEXTURE_GET_PRIVATE (self);

  priv->max_tile_waste    = 63;
  priv->filter_quality    = CLUTTER_TEXTURE_QUALITY_MEDIUM;
  priv->repeat_x          = FALSE;
  priv->repeat_y          = FALSE;
  priv->sync_actor_size   = TRUE;
  priv->material          = cogl_material_new ();
  priv->fbo_texture       = COGL_INVALID_HANDLE;
  priv->fbo_handle        = COGL_INVALID_HANDLE;
  priv->local_data        = NULL;
  priv->keep_aspect_ratio = FALSE;
}

static void
clutter_texture_save_to_local_data (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;
  int                    bpp;
  CoglPixelFormat        pixel_format;
  CoglHandle             cogl_texture;

  priv = texture->priv;

  if (priv->local_data)
    {
      g_free (priv->local_data);
      priv->local_data = NULL;
    }

  if (priv->material == COGL_INVALID_HANDLE)
    return;

  cogl_texture = clutter_texture_get_cogl_texture (texture);

  priv->local_data_width = cogl_texture_get_width (cogl_texture);
  priv->local_data_height = cogl_texture_get_height (cogl_texture);
  pixel_format = cogl_texture_get_format (cogl_texture);
  priv->local_data_has_alpha = pixel_format & COGL_A_BIT;
  bpp = priv->local_data_has_alpha ? 4 : 3;

  /* Align to 4 bytes */
  priv->local_data_rowstride = (priv->local_data_width * bpp + 3) & ~3;

  /* Store the filter quality and max_tile_waste from the texture
     properties so that they will be restored the data is loaded
     again */
  priv->max_tile_waste = clutter_texture_get_max_tile_waste (texture);
  priv->filter_quality = clutter_texture_get_filter_quality (texture);

  priv->local_data = g_malloc (priv->local_data_rowstride
			       * priv->local_data_height);

  if (cogl_texture_get_data (cogl_texture,
			     priv->local_data_has_alpha
			     ? COGL_PIXEL_FORMAT_RGBA_8888
			     : COGL_PIXEL_FORMAT_RGB_888,
			     priv->local_data_rowstride,
			     priv->local_data) == 0)
    {
      g_free (priv->local_data);
      priv->local_data = NULL;
    }
}

static void
clutter_texture_load_from_local_data (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;

  priv = texture->priv;

  if (priv->local_data == NULL)
    return;

  clutter_texture_set_from_rgb_data (texture,
				     priv->local_data,
				     priv->local_data_has_alpha,
				     priv->local_data_width,
				     priv->local_data_height,
				     priv->local_data_rowstride,
				     priv->local_data_has_alpha ? 4: 3,
				     0, NULL);

  g_free (priv->local_data);
  priv->local_data = NULL;
}

#if EXPOSE_COGL_MATERIAL_PROP
/**
 * clutter_texture_get_cogl_material:
 * @texture: A #ClutterTexture
 *
 * Returns a handle to the underlying COGL material used for drawing
 * the actor. No extra reference is taken so if you need to keep the
 * handle then you should call cogl_material_ref on it.
 *
 * Since: 1.0
 *
 * Return value: COGL material handle
 */
CoglHandle
clutter_texture_get_cogl_material (ClutterTexture *texture)
{
  return texture->priv->material;
}

/**
 * clutter_texture_set_cogl_material:
 * @texture: A #ClutterTexture
 * @cogl_material: A CoglHandle for a material
 *
 * Replaces the underlying COGL texture drawn by this actor with
 * @cogl_tex. A reference to the texture is taken so if the handle is
 * no longer needed it should be deref'd with cogl_texture_unref.
 *
 * Since: 0.8
 *
 */
void
clutter_texture_set_cogl_material (ClutterTexture *texture,
                                   CoglHandle cogl_material)
{
  CoglHandle cogl_texture;

  /* This */
  if (texture->priv->material)
    cogl_material_unref (texture->priv->material);

  texture->priv->material = cogl_material;

  /* XXX: We are re-asserting the first layer of the new material to ensure the
   * priv state is in sync with the contents of the material. */
  cogl_texture = clutter_texture_get_cogl_texture (texture);
  clutter_texture_set_cogl_texture (texture, cogl_texture);
  /* XXX: If we add support for more material layers, this will need
   * extending */
}
#endif

/**
 * clutter_texture_get_cogl_texture
 * @texture: A #ClutterTexture
 *
 * Returns a handle to the underlying COGL texture used for drawing
 * the actor. No extra reference is taken so if you need to keep the
 * handle then you should call cogl_texture_ref on it.
 *
 * Since: 0.8
 *
 * Return value: COGL texture handle
 **/
CoglHandle
clutter_texture_get_cogl_texture (ClutterTexture *texture)
{
  const GList *layers;
  int n_layers;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), COGL_INVALID_HANDLE);

  layers = cogl_material_get_layers (texture->priv->material);
  n_layers = g_list_length ((GList *)layers);
  if (n_layers == 0)
    return COGL_INVALID_HANDLE;

  return cogl_material_layer_get_texture (layers->data);
}

/**
 * clutter_texture_set_cogl_texture
 * @texture: A #ClutterTexture
 * @cogl_tex: A CoglHandle for a texture
 *
 * Replaces the underlying COGL texture drawn by this actor with
 * @cogl_tex. A reference to the texture is taken so if the handle is
 * no longer needed it should be deref'd with cogl_texture_unref.
 *
 * Since: 0.8
 */
void
clutter_texture_set_cogl_texture (ClutterTexture  *texture,
				  CoglHandle       cogl_tex)
{
  ClutterTexturePrivate  *priv;
  gboolean                size_change;
  guint                   width, height;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));
  g_return_if_fail (cogl_is_texture (cogl_tex));

  priv = texture->priv;

  width = cogl_texture_get_width (cogl_tex);
  height = cogl_texture_get_height (cogl_tex);

  /* Reference the new texture now in case it is the same one we are
     already using */
  cogl_texture_ref (cogl_tex);

  /* Remove FBO if exisiting */
  if (priv->fbo_source)
    texture_fbo_free_resources (texture);

  /* Remove old texture */
  texture_free_gl_resources (texture);
  /* Use the new texture */

  cogl_material_set_layer (priv->material, 0, cogl_tex);

  /* The material now holds a reference to the texture so we can
     safely release the reference we claimed above */
  cogl_texture_unref (cogl_tex);

  size_change      = width != priv->width || height != priv->height;
  priv->width      = width;
  priv->height     = height;

  CLUTTER_NOTE (TEXTURE, "set size %ix%i\n",
		priv->width,
		priv->height);

  CLUTTER_ACTOR_SET_FLAGS (CLUTTER_ACTOR (texture), CLUTTER_ACTOR_REALIZED);

  if (size_change)
    {
      g_signal_emit (texture, texture_signals[SIZE_CHANGE], 0,
                     priv->width,
                     priv->height);

      if (priv->sync_actor_size)
        clutter_actor_queue_relayout (CLUTTER_ACTOR (texture));
    }

  /* rename signal */
  g_signal_emit (texture, texture_signals[PIXBUF_CHANGE], 0);

  g_object_notify (G_OBJECT (texture), "cogl-texture");

  /* If resized actor may need resizing but paint() will do this */
  if (CLUTTER_ACTOR_IS_VISIBLE (texture))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));
}

static gboolean
clutter_texture_set_from_data (ClutterTexture     *texture,
			       const guchar       *data,
			       CoglPixelFormat     source_format,
			       gint                width,
			       gint                height,
			       gint                rowstride,
			       gint                bpp,
			       GError            **error)
{
  ClutterTexturePrivate *priv = texture->priv;
  CoglHandle new_texture = COGL_INVALID_HANDLE;
  CoglTextureFlags flags = COGL_TEXTURE_NONE;
  gint min_filter, mag_filter;
  gint max_waste = -1;

  if (!priv->no_slice)
    max_waste = priv->max_tile_waste;

  if (priv->filter_quality == CLUTTER_TEXTURE_QUALITY_HIGH)
    flags |= COGL_TEXTURE_AUTO_MIPMAP;

  new_texture = cogl_texture_new_from_data (width, height,
                                            max_waste, flags,
                                            source_format,
                                            COGL_PIXEL_FORMAT_ANY,
                                            rowstride,
                                            data);

  if (G_UNLIKELY (new_texture == COGL_INVALID_HANDLE))
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
                   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
                   "Failed to create COGL texture");

      return FALSE;
    }

  clutter_texture_quality_to_filters (priv->filter_quality,
                                      &min_filter,
                                      &mag_filter);

  cogl_texture_set_filters (new_texture, min_filter, mag_filter);

  clutter_texture_set_cogl_texture (texture, new_texture);

  cogl_texture_unref (new_texture);

  g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0, error);

  return TRUE;
}

/**
 * clutter_texture_set_from_rgb_data:
 * @texture: A #ClutterTexture
 * @data: Image data in RGBA type colorspace.
 * @has_alpha: Set to TRUE if image data has an alpha channel.
 * @width: Width in pixels of image data.
 * @height: Height in pixels of image data
 * @rowstride: Distance in bytes between row starts.
 * @bpp: bytes per pixel (Currently only 3 and 4 supported,
 *                        depending on @has_alpha)
 * @flags: #ClutterTextureFlags
 * @error: return location for a #GError, or %NULL.
 *
 * Sets #ClutterTexture image data.
 *
 * Note: This function is likely to change in future versions.
 *
 * Return value: %TRUE on success, %FALSE on failure.
 *
 * Since: 0.4.
 **/
gboolean
clutter_texture_set_from_rgb_data   (ClutterTexture     *texture,
				     const guchar       *data,
				     gboolean            has_alpha,
				     gint                width,
				     gint                height,
				     gint                rowstride,
				     gint                bpp,
				     ClutterTextureFlags flags,
				     GError            **error)
{
  ClutterTexturePrivate *priv;
  CoglPixelFormat        source_format;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  priv = texture->priv;

  /* Convert the flags to a CoglPixelFormat */
  if (has_alpha)
    {
      if (bpp != 4)
	{
	  g_set_error (error, CLUTTER_TEXTURE_ERROR,
		       CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		       "Unsupported BPP");
	  return FALSE;
	}
      source_format = COGL_PIXEL_FORMAT_RGBA_8888;
    }
  else
    {
      if (bpp != 3)
	{
	  g_set_error (error, CLUTTER_TEXTURE_ERROR,
		       CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		       "Unsupported BPP");
	  return FALSE;
	}
      source_format = COGL_PIXEL_FORMAT_RGB_888;
    }
  if ((flags & CLUTTER_TEXTURE_RGB_FLAG_BGR))
    source_format |= COGL_BGR_BIT;
  if ((flags & CLUTTER_TEXTURE_RGB_FLAG_PREMULT))
    source_format |= COGL_PREMULT_BIT;

  return clutter_texture_set_from_data (texture, data,
					source_format,
					width, height,
					rowstride, bpp,
					error);
}

/**
 * clutter_texture_set_from_yuv_data:
 * @texture: A #ClutterTexture
 * @data: Image data in YUV type colorspace.
 * @width: Width in pixels of image data.
 * @height: Height in pixels of image data
 * @flags: #ClutterTextureFlags
 * @error: Return location for a #GError, or %NULL.
 *
 * Sets a #ClutterTexture from YUV image data. If an error occurred,
 * %FALSE is returned and @error is set.
 *
 * This function is likely to change in future versions.
 *
 * Return value: %TRUE if the texture was successfully updated
 *
 * Since: 0.4
 */
gboolean
clutter_texture_set_from_yuv_data (ClutterTexture     *texture,
				   const guchar       *data,
				   gint                width,
				   gint                height,
				   ClutterTextureFlags flags,
				   GError            **error)
{
  ClutterTexturePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  if (!clutter_feature_available (CLUTTER_FEATURE_TEXTURE_YUV))
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
                   CLUTTER_TEXTURE_ERROR_NO_YUV,
                   "YUV textures are not supported");
      return FALSE;
    }

  priv = texture->priv;

  /* Convert the flags to a CoglPixelFormat */
  if ((flags & CLUTTER_TEXTURE_YUV_FLAG_YUV2))
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
		   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		   "YUV2 not supported");
      return FALSE;
    }

  return clutter_texture_set_from_data (texture, data,
					COGL_PIXEL_FORMAT_YUV,
					width, height,
					width * 3, 3,
					error);
}

/*
 * clutter_texture_async_load_complete:
 * @self: a #ClutterTexture
 * @error: load error
 *
 * If @error is %NULL, loads the #CoglBitmap into a #CoglTexture.
 *
 * This function emits the ::load-finished signal on @self.
 */
static void
clutter_texture_async_load_complete (ClutterTexture *self,
                                     const GError   *error)
{
  ClutterTexturePrivate *priv = self->priv;
  CoglHandle handle;
  CoglTextureFlags flags = COGL_TEXTURE_NONE;
  gint waste = -1;

  if (error == NULL)
    {
      if (!priv->no_slice)
        waste = priv->max_tile_waste;

      if (priv->filter_quality == CLUTTER_TEXTURE_QUALITY_HIGH)
        flags |= COGL_TEXTURE_AUTO_MIPMAP;

      handle = cogl_texture_new_from_bitmap (priv->load_bitmap,
                                             waste, flags,
                                             COGL_PIXEL_FORMAT_ANY);
      clutter_texture_set_cogl_texture (self, handle);
      cogl_texture_unref (handle);
      
      cogl_bitmap_free (priv->load_bitmap);
      priv->load_bitmap = NULL;
    }

  g_signal_emit (self, texture_signals[LOAD_FINISHED], 0, error);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

static gboolean
clutter_texture_thread_cb (gpointer data)
{
  ClutterTexture *self = data;
  ClutterTexturePrivate *priv = self->priv;

  priv->load_idle = 0;

  if (priv->load_thread)
    {
      g_thread_join (priv->load_thread);
      priv->load_thread = NULL;
    }
  else
    return FALSE;

  clutter_texture_async_load_complete (self, priv->load_error);

  if (priv->load_error)
    {
      g_error_free (priv->load_error);
      priv->load_error = NULL;
    }
  
  return FALSE;
}

static gpointer
clutter_texture_thread_func (gpointer data)
{
  ClutterTexture *self = data;
  ClutterTexturePrivate *priv = self->priv;

  /* Try loading with imaging backend */
  priv->load_bitmap = cogl_bitmap_new_from_file (priv->load_filename,
                                                 &priv->load_error);
  g_free (priv->load_filename);
  priv->load_filename = NULL;

  /* make sure we load the image in the main thread, where we
   * hold the main Clutter lock
   */
  priv->load_idle =
    clutter_threads_add_idle (clutter_texture_thread_cb, self);

  return NULL;
}

static gboolean
clutter_texture_idle_func (gpointer data)
{
  ClutterTexture *self = data;
  ClutterTexturePrivate *priv = self->priv;
  GError *internal_error;

  internal_error = NULL;
  priv->load_bitmap = cogl_bitmap_new_from_file (priv->load_filename,
                                                 &internal_error);

  clutter_texture_async_load_complete (self, internal_error);

  g_free (priv->load_filename);
  priv->load_filename = NULL;

  if (internal_error)
    g_error_free (internal_error);

  return FALSE;
}

/*
 * clutter_texture_async_load:
 * @self: a #ClutterTexture
 * @error: return location for a #GError
 *
 * Starts an asynchronous load of the file name stored inside
 * the load_filename private member.
 *
 * If threading is enabled we use a GThread to perform the actual
 * I/O; if threading is not enabled, we use an idle GSource.
 *
 * The I/O is the only bit done in a thread -- uploading the
 * texture data to the GL pipeline must be done from within the
 * same thread that called clutter_main(). Threaded upload should
 * be part of the GL implementation.
 *
 * This function will block until we get a size from the file
 * so that we can effectively get the size the texture actor after
 * clutter_texture_set_from_file().
 *
 * Return value: %TRUE if the asynchronous loading was successfully
 *   initiated, %FALSE otherwise
 */
static gboolean
clutter_texture_async_load (ClutterTexture  *self,
                            GError         **error)
{
  ClutterTexturePrivate *priv = self->priv;
  gint width, height;
  gboolean res;

  g_assert (priv->load_filename != NULL);

  /* ask the file for a size; if we cannot get the size then
   * there's no point in even continuing the asynchronous
   * loading, so we just stop there
   */
  res = cogl_bitmap_get_size_from_file (priv->load_filename,
                                        &width,
                                        &height);
  if (!res)
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
		   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		   "Failed to create COGL texture");
      return FALSE;
    }
  else
    {
      priv->width = width;
      priv->height = height;
    }

  if (g_thread_supported ())
    {
      priv->load_thread =
        g_thread_create ((GThreadFunc) clutter_texture_thread_func,
                         self, TRUE,
                         error);

      return priv->load_thread != NULL? TRUE : FALSE;
    }
  else
    {
      priv->load_idle =
        clutter_threads_add_idle (clutter_texture_idle_func, self);

      return TRUE;
    }
}

/**
 * clutter_texture_set_from_file:
 * @texture: A #ClutterTexture
 * @filename: The filename of the image in GLib file name encoding
 * @error: Return location for a #GError, or %NULL
 *
 * Sets the #ClutterTexture image data from an image file. In case of
 * failure, %FALSE is returned and @error is set.
 *
 * If #ClutterTexture:load-async is set to %TRUE, this function
 * will return as soon as possible, and the actual image loading
 * from disk will be performed asynchronously. #ClutterTexture::load-finished
 * will be emitted when the image has been loaded or if an error
 * occurred.
 *
 * Return value: %TRUE if the image was successfully loaded and set
 *
 * Since: 0.8
 */
gboolean
clutter_texture_set_from_file (ClutterTexture *texture,
			       const gchar    *filename,
			       GError        **error)
{
  ClutterTexturePrivate *priv;
  CoglHandle new_texture = COGL_INVALID_HANDLE;
  GError *internal_error = NULL;
  CoglTextureFlags flags = COGL_TEXTURE_NONE;
  gint min_filter, mag_filter;
  gint max_waste = -1;

  priv = texture->priv;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (priv->load_async)
    {
      clutter_texture_async_load_cancel (texture);

      priv->load_filename = g_strdup (filename);

      return clutter_texture_async_load (texture, error);
    }

  if (!priv->no_slice)
    max_waste = priv->max_tile_waste;

  if (priv->filter_quality == CLUTTER_TEXTURE_QUALITY_HIGH)
    flags |= COGL_TEXTURE_AUTO_MIPMAP;

  new_texture = cogl_texture_new_from_file (filename,
                                            max_waste, flags,
                                            COGL_PIXEL_FORMAT_ANY,
                                            &internal_error);
  if (new_texture == COGL_INVALID_HANDLE)
    {
      /* If COGL didn't give an error then make one up */
      if (internal_error == NULL)
	{
	  g_set_error (error, CLUTTER_TEXTURE_ERROR,
		       CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		       "Failed to create COGL texture");
	}
      else
        g_propagate_error (error, internal_error);

      g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0, error);

      return FALSE;
    }

  clutter_texture_quality_to_filters (priv->filter_quality,
                                      &min_filter,
                                      &mag_filter);

  cogl_texture_set_filters (new_texture, min_filter, mag_filter);

  clutter_texture_set_cogl_texture (texture, new_texture);

  cogl_texture_unref (new_texture);

  g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0, error);

  return TRUE;
}

/**
 * clutter_texture_set_filter_quality:
 * @texture: a #ClutterTexture
 * @filter_quality: new filter quality value
 *
 * Sets the filter quality when scaling a texture. The quality is an
 * enumeration currently the following values are supported:
 * %CLUTTER_TEXTURE_QUALITY_LOW which is fast but only uses nearest neighbour
 * interpolation. %CLUTTER_TEXTURE_QUALITY_MEDIUM which is computationally a
 * bit more expensive (bilinear interpolation), and
 * %CLUTTER_TEXTURE_QUALITY_HIGH which uses extra texture memory resources to
 * improve scaled down rendering as well (by using mipmaps). The default value
 * is %CLUTTER_TEXTURE_QUALITY_MEDIUM.
 *
 * Since: 0.8
 */
void
clutter_texture_set_filter_quality (ClutterTexture        *texture,
				    ClutterTextureQuality  filter_quality)
{
  ClutterTexturePrivate *priv;
  ClutterTextureQuality  old_quality;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;

  old_quality = clutter_texture_get_filter_quality (texture);

  if (filter_quality != old_quality)
    {
      CoglHandle cogl_texture = clutter_texture_get_cogl_texture (texture);
      gint min_filter, mag_filter;

      priv->filter_quality = filter_quality;

      clutter_texture_quality_to_filters (priv->filter_quality,
                                          &min_filter,
                                          &mag_filter);

      /* Is this actually needed - causes problems with TFP mipmaps */
      if (cogl_texture != COGL_INVALID_HANDLE)
	cogl_texture_set_filters (cogl_texture, min_filter, mag_filter);

      if ((old_quality == CLUTTER_TEXTURE_QUALITY_HIGH ||
           filter_quality == CLUTTER_TEXTURE_QUALITY_HIGH) &&
           CLUTTER_ACTOR_IS_REALIZED (texture))
        {
          gboolean was_visible;

          was_visible = CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (texture));

          clutter_actor_unrealize (CLUTTER_ACTOR (texture));
          clutter_actor_realize (CLUTTER_ACTOR (texture));

          if (was_visible)
            clutter_actor_show (CLUTTER_ACTOR (texture));
        }

      g_object_notify (G_OBJECT (texture), "filter-quality");

      if (CLUTTER_ACTOR_IS_VISIBLE (texture))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));
    }
}

/**
 * clutter_texture_get_filter_quality
 * @texture: A #ClutterTexture
 *
 * Gets the filter quality used when scaling a texture.
 *
 * Return value: The filter quality value.
 *
 * Since: 0.8
 */
ClutterTextureQuality
clutter_texture_get_filter_quality (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), 0);

  priv = texture->priv;

  return priv->filter_quality;
}

/**
 * clutter_texture_set_max_tile_waste
 * @texture: A #ClutterTexture
 * @max_tile_waste: Maximum amount of waste in pixels or -1
 *
 * Sets the maximum number of pixels in either axis that can be wasted
 * for an individual texture slice. If -1 is specified then the
 * texture is forced not to be sliced and the texture creation will
 * fail if the hardware can't create a texture large enough.
 *
 * The value is only used when first creating a texture so changing it
 * after the texture data has been set has no effect.
 *
 * Since: 0.8
 */
void
clutter_texture_set_max_tile_waste (ClutterTexture *texture,
				    gint            max_tile_waste)
{
  ClutterTexturePrivate *priv;
  CoglHandle cogl_texture;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;
  cogl_texture = clutter_texture_get_cogl_texture (texture);

  /* There's no point in changing the max_tile_waste if the texture
     has already been created because it will be overridden with the
     value from the texture handle */
  if (cogl_texture == COGL_INVALID_HANDLE)
    priv->max_tile_waste = max_tile_waste;
}

/**
 * clutter_texture_get_max_tile_waste
 * @texture: A #ClutterTexture
 *
 * Gets the maximum waste that will be used when creating a texture or
 * -1 if slicing is disabled.
 *
 * Return value: The maximum waste or -1 if the texture waste is
 * unlimited.
 *
 * Since: 0.8
 */
gint
clutter_texture_get_max_tile_waste (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;
  CoglHandle             cogl_texture;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), 0);

  priv = texture->priv;
  cogl_texture = clutter_texture_get_cogl_texture (texture);

  if (cogl_texture == COGL_INVALID_HANDLE)
    return texture->priv->max_tile_waste;
  else
    /* If we have a valid texture handle then use the value from that
       instead */
    return cogl_texture_get_max_waste (cogl_texture);
}

/**
 * clutter_texture_new_from_file:
 * @filename: The name of an image file to load.
 * @error: Return locatoin for an error.
 *
 * Creates a new ClutterTexture actor to display the image contained a
 * file. If the image failed to load then NULL is returned and @error
 * is set.
 *
 * Return value: A newly created #ClutterTexture object or NULL on
 * error.
 *
 * Since: 0.8
 **/
ClutterActor*
clutter_texture_new_from_file (const gchar *filename,
			       GError     **error)
{
  ClutterActor *texture = clutter_texture_new ();

  if (!clutter_texture_set_from_file (CLUTTER_TEXTURE (texture),
				      filename, error))
    {
      g_object_ref_sink (texture);
      g_object_unref (texture);

      return NULL;
    }
  else
    return texture;
}

/**
 * clutter_texture_new:
 *
 * Creates a new empty #ClutterTexture object.
 *
 * Return value: A newly created #ClutterTexture object.
 **/
ClutterActor *
clutter_texture_new (void)
{
  return g_object_new (CLUTTER_TYPE_TEXTURE, NULL);
}

/**
 * clutter_texture_get_base_size:
 * @texture: A #ClutterTexture
 * @width:   Pointer to gint to be populated with width value if non NULL.
 * @height:  Pointer to gint to be populated with height value if non NULL.
 *
 * Gets the size in pixels of the untransformed underlying texture pixbuf data.
 *
 **/
void 				/* FIXME: rename to get_image_size */
clutter_texture_get_base_size (ClutterTexture *texture,
			       gint           *width,
			       gint           *height)
{
  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  /* Attempt to realize, mainly for subclasses ( such as labels )
   * which may not create pixbuf data and thus base size until
   * realization happens.
   */
  if (!CLUTTER_ACTOR_IS_REALIZED (texture))
    clutter_actor_realize (CLUTTER_ACTOR (texture));

  if (width)
    *width = texture->priv->width;

  if (height)
    *height = texture->priv->height;
}

/**
 * clutter_texture_set_area_from_rgb_data:
 * @texture: A #ClutterTexture
 * @data: Image data in RGB type colorspace.
 * @has_alpha: Set to TRUE if image data has an alpha channel.
 * @x: X coordinate of upper left corner of region to update.
 * @y: Y coordinate of upper left corner of region to update.
 * @width: Width in pixels of region to update.
 * @height: Height in pixels of region to update.
 * @rowstride: Distance in bytes between row starts on source buffer.
 * @bpp: bytes per pixel (Currently only 3 and 4 supported,
 *                        depending on @has_alpha)
 * @flags: #ClutterTextureFlags
 * @error: return location for a #GError, or %NULL
 *
 * Updates a sub-region of the pixel data in a #ClutterTexture.
 *
 * Return value: %TRUE on success, %FALSE on failure.
 *
 * Since: 0.6
 */
gboolean
clutter_texture_set_area_from_rgb_data (ClutterTexture     *texture,
                                        const guchar       *data,
                                        gboolean            has_alpha,
                                        gint                x,
                                        gint                y,
                                        gint                width,
                                        gint                height,
                                        gint                rowstride,
                                        gint                bpp,
                                        ClutterTextureFlags flags,
                                        GError            **error)
{
  ClutterTexturePrivate *priv;
  CoglPixelFormat        source_format;
  CoglHandle             cogl_texture;

  priv = texture->priv;

  if (has_alpha)
    {
      if (bpp != 4)
	{
	  g_set_error (error, CLUTTER_TEXTURE_ERROR,
		       CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		       "Unsupported BPP");
	  return FALSE;
	}
      source_format = COGL_PIXEL_FORMAT_RGBA_8888;
    }
  else
    {
      if (bpp != 3)
	{
	  g_set_error (error, CLUTTER_TEXTURE_ERROR,
		       CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		       "Unsupported BPP");
	  return FALSE;
	}
      source_format = COGL_PIXEL_FORMAT_RGB_888;
    }
  if ((flags & CLUTTER_TEXTURE_RGB_FLAG_BGR))
    source_format |= COGL_BGR_BIT;
  if ((flags & CLUTTER_TEXTURE_RGB_FLAG_PREMULT))
    source_format |= COGL_PREMULT_BIT;

  clutter_actor_realize (CLUTTER_ACTOR (texture));

  cogl_texture = clutter_texture_get_cogl_texture (texture);
  if (cogl_texture == COGL_INVALID_HANDLE)
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
		   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		   "Failed to realize actor");
      return FALSE;
    }

  if (!cogl_texture_set_region (cogl_texture,
				0, 0,
				x, y, width, height,
				width, height,
				source_format,
				rowstride,
				data))
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
		   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		   "Failed to upload COGL texture data");
      return FALSE;
    }

  /* rename signal */
  g_signal_emit (texture, texture_signals[PIXBUF_CHANGE], 0);

  if (CLUTTER_ACTOR_IS_VISIBLE (texture))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));

  return TRUE;
}

static void
on_fbo_source_size_change (GObject          *object,
                           GParamSpec       *param_spec,
                           ClutterTexture   *texture)
{
  ClutterTexturePrivate *priv = texture->priv;
  guint                  w, h;

  clutter_actor_get_transformed_size (priv->fbo_source, &w, &h);

  if (w != priv->width || h != priv->height)
    {
      CoglTextureFlags flags = COGL_TEXTURE_NONE;
      gint min_filter, mag_filter;

      /* tear down the FBO */
      cogl_offscreen_unref (priv->fbo_handle);

      texture_free_gl_resources (texture);

      priv->width = w;
      priv->height = h;

      if (priv->filter_quality == CLUTTER_TEXTURE_QUALITY_HIGH)
        flags |= COGL_TEXTURE_AUTO_MIPMAP;

      priv->fbo_texture =
        cogl_texture_new_with_size (MAX (priv->width, 1),
				    MAX (priv->height, 1),
				    -1,
                                    flags,
				    COGL_PIXEL_FORMAT_RGBA_8888);

      clutter_texture_quality_to_filters (priv->filter_quality,
                                          &min_filter,
                                          &mag_filter);

      cogl_texture_set_filters (priv->fbo_texture, min_filter, mag_filter);

      priv->fbo_handle = cogl_offscreen_new_to_texture (priv->fbo_texture);

      if (priv->fbo_handle == COGL_INVALID_HANDLE)
        {
          g_warning ("%s: Offscreen texture creation failed", G_STRLOC);
	  CLUTTER_ACTOR_UNSET_FLAGS (CLUTTER_ACTOR (texture),
				     CLUTTER_ACTOR_REALIZED);
          return;
        }

      clutter_actor_set_size (CLUTTER_ACTOR (texture), w, h);
    }
}

static void
on_fbo_parent_change (ClutterActor        *actor,
                      ClutterActor        *old_parent,
                      ClutterTexture      *texture)
{
  ClutterActor        *parent = CLUTTER_ACTOR(texture);

  while ((parent = clutter_actor_get_parent (parent)) != NULL)
    if (parent == actor)
      {
        g_warning ("Offscreen texture is ancestor of source!");
        /* Desperate but will avoid infinite loops */
        clutter_actor_unparent (actor);
      }
}

/**
 * clutter_texture_new_from_actor:
 * @actor: A source #ClutterActor
 *
 * Creates a new #ClutterTexture object with its source a prexisting
 * actor (and associated children). The textures content will contain
 * 'live' redirected output of the actors scene.
 *
 * Note this function is intented as a utility call for uniformly applying
 * shaders to groups and other potential visual effects. It requires that
 * the %CLUTTER_FEATURE_OFFSCREEN feature is supported by the current backend
 * and the target system.
 *
 * Some tips on usage:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>The source actor must be made visible (i.e by calling
 *     #clutter_actor_show).</para>
 *   </listitem>
 *   <listitem>
 *     <para>The source actor must have a parent in order for it to be
 *     allocated a size from the layouting mechanism. If the source
 *     actor does not have a parent when this function is called then
 *     the ClutterTexture will adopt it and allocate it at its
 *     preferred size. Using this you can clone an actor that is
 *     otherwise not displayed. Because of this feature if you do
 *     intend to display the source actor then you must make sure that
 *     the actor is parented before calling
 *     clutter_texture_new_from_actor() or that you unparent it before
 *     adding it to a container.</para>
 *   </listitem>
 *   <listitem>
 *     <para>When getting the image for the clone texture, Clutter
 *     will attempt to render the source actor exactly as it would
 *     appear if it was rendered on screen. The source actor's parent
 *     transformations are taken into account. Therefore if your
 *     source actor is rotated along the X or Y axes so that it has
 *     some depth, the texture will appear differently depending on
 *     the on-screen location of the source actor. While painting the
 *     source actor, Clutter will set up a temporary asymmetric
 *     perspective matrix as the projection matrix so that the source
 *     actor will be projected as if a small section of the screen was
 *     being viewed. Before version 0.8.2, an orthogonal identity
 *     projection was used which meant that the source actor would be
 *     clipped if any part of it was not on the zero Z-plane.</para>
 *   </listitem>
 *   <listitem>
 *     <para>Avoid reparenting the source with the created texture.</para>
 *   </listitem>
 *   <listitem>
 *     <para>A group can be padded with a transparent rectangle as to
 *     provide a border to contents for shader output (blurring text
 *     for example).</para>
 *   </listitem>
 *   <listitem>
 *     <para>The texture will automatically resize to contain a further
 *     transformed source. However, this involves overhead and can be
 *     avoided by placing the source actor in a bounding group
 *     sized large enough to contain any child tranformations.</para>
 *   </listitem>
 *   <listitem>
 *     <para>Uploading pixel data to the texture (e.g by using
 *     clutter_actor_set_from_file()) will destroy the offscreen texture data
 *     and end redirection.</para>
 *   </listitem>
 *   <listitem>
 *     <para>cogl_texture_get_data() with the handle returned by
 *     clutter_texture_get_cogl_texture() can be used to read the
 *     offscreen texture pixels into a pixbuf.</para>
 *   </listitem>
 * </itemizedlist>
 *
 * Return value: A newly created #ClutterTexture object, or %NULL on failure.
 *
 * Since: 0.6
 */
ClutterActor *
clutter_texture_new_from_actor (ClutterActor *actor)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;
  guint                  w, h;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  if (clutter_feature_available (CLUTTER_FEATURE_OFFSCREEN) == FALSE)
    return NULL;

  if (!CLUTTER_ACTOR_IS_REALIZED (actor))
    {
      clutter_actor_realize (actor);

      if (!CLUTTER_ACTOR_IS_REALIZED (actor))
	return NULL;
    }

  clutter_actor_get_transformed_size (actor, &w, &h);

  if (w == 0 || h == 0)
    return NULL;

  /* Hopefully now were good.. */
  texture = g_object_new (CLUTTER_TYPE_TEXTURE,
                          "disable-slicing", TRUE,
                          NULL);

  priv = texture->priv;

  priv->fbo_source = g_object_ref_sink (actor);

  /* If the actor doesn't have a parent then claim it so that it will
     get a size allocation during layout */
  if (clutter_actor_get_parent (actor) == NULL)
    clutter_actor_set_parent (actor, CLUTTER_ACTOR (texture));

  /* Connect up any signals which could change our underlying size */
  g_signal_connect (actor,
                    "notify::width",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);
  g_signal_connect (actor,
                    "notify::height",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);
  g_signal_connect (actor,
                    "notify::scale-x",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);
  g_signal_connect (actor,
                    "notify::scale-y",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);
  g_signal_connect (actor,
                    "notify::rotation-angle-x",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);
  g_signal_connect (actor,
                    "notify::rotation-angle-y",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);
  g_signal_connect (actor,
                    "notify::rotation-angle-z",
                    G_CALLBACK(on_fbo_source_size_change),
                    texture);

  /* And a warning if the source becomes a child of the texture */
  g_signal_connect (actor,
                    "parent-set",
                    G_CALLBACK(on_fbo_parent_change),
                    texture);

  priv->width        = w;
  priv->height       = h;

  clutter_actor_set_size (CLUTTER_ACTOR (texture), priv->width, priv->height);

  return CLUTTER_ACTOR (texture);
}

static void
texture_fbo_free_resources (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;

  priv = texture->priv;

  CLUTTER_MARK();

  if (priv->fbo_source != NULL)
    {
      /* If we parented the texture then unparent it again so that it
	 will lose the reference */
      if (clutter_actor_get_parent (priv->fbo_source)
	  == CLUTTER_ACTOR (texture))
	clutter_actor_unparent (priv->fbo_source);

      g_signal_handlers_disconnect_by_func
                            (priv->fbo_source,
                             G_CALLBACK(on_fbo_parent_change),
                             texture);

      g_signal_handlers_disconnect_by_func
                            (priv->fbo_source,
                             G_CALLBACK(on_fbo_source_size_change),
                             texture);

      g_object_unref (priv->fbo_source);

      priv->fbo_source = NULL;
    }

  if (priv->fbo_handle != COGL_INVALID_HANDLE)
    {
      cogl_offscreen_unref (priv->fbo_handle);
      priv->fbo_handle = COGL_INVALID_HANDLE;
    }
}

GType
clutter_texture_handle_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (!our_type))
    {
      our_type =
	g_boxed_type_register_static (I_("ClutterTextureHandle"),
				      (GBoxedCopyFunc) cogl_texture_ref,
				      (GBoxedFreeFunc) cogl_texture_unref);
    }

  return our_type;
}

#if EXPOSE_COGL_MATERIAL_PROP
GType
clutter_material_handle_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (!our_type))
    {
      our_type =
	g_boxed_type_register_static (I_("ClutterMaterialHandle"),
				      (GBoxedCopyFunc) cogl_material_ref,
				      (GBoxedFreeFunc) cogl_material_unref);
    }

  return our_type;
}
#endif

