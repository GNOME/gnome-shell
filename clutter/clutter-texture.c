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

typedef struct _ClutterTextureAsyncData ClutterTextureAsyncData;

struct _ClutterTexturePrivate
{
  gint                         image_width;
  gint                         image_height;

  CoglHandle                   material;
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
  guint                        load_size_async : 1;
  guint                        load_data_async : 1;
  guint                        load_async_set : 1;  /* used to make load_async
                                                       possible */

  ClutterTextureAsyncData     *async_data;
};

struct _ClutterTextureAsyncData
{
  /* Mutex used to synchronize setting the abort flag */
  GMutex         *mutex;

  /* If set to true, the loaded data will be discarded */
  gboolean        abort;

  /* The texture for which the data is being loaded */
  ClutterTexture *texture;

  /* Source ID of the idle handler for loading. If this is zero then
     the data is being loaded in a thread from the thread pool. Once
     the thread is finished it will be converted to idle load handler
     and load_idle will be nonzero. If load_idle is nonzero, and
     upload_queued is FALSE then the rest of the load can safely be
     aborted by just removing the source, otherwise the abort flag
     needs to be set and the data should be disowned */
  guint           load_idle;

  /* Set when the texture is queued for GPU upload, used to determine
   * what to do with the texture data when load_idle is zero.
  */
  gboolean        upload_queued;  

  gchar          *load_filename;
  CoglHandle      load_bitmap;
  GError         *load_error;
};

enum
{
  PROP_0,
  PROP_NO_SLICE,
  PROP_MAX_TILE_WASTE,
  PROP_PIXEL_FORMAT,
  PROP_SYNC_SIZE,
  PROP_REPEAT_Y,
  PROP_REPEAT_X,
  PROP_FILTER_QUALITY,
  PROP_COGL_TEXTURE,
  PROP_COGL_MATERIAL,
  PROP_FILENAME,
  PROP_KEEP_ASPECT_RATIO,
  PROP_LOAD_ASYNC,
  PROP_LOAD_DATA_ASYNC,
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

static GThreadPool *async_thread_pool = NULL;
static guint        repaint_upload_func = 0;
static GList       *upload_list = NULL;
static GStaticMutex upload_list_mutex = G_STATIC_MUTEX_INIT;

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

static const struct
{
  gint min_filter;
  gint mag_filter;
}
clutter_texture_quality_filters[] =
  {
    /* CLUTTER_TEXTURE_QUALITY_LOW */
    { COGL_MATERIAL_FILTER_NEAREST, COGL_MATERIAL_FILTER_NEAREST },

    /* CLUTTER_TEXTURE_QUALITY_MEDIUM */
    { COGL_MATERIAL_FILTER_LINEAR, COGL_MATERIAL_FILTER_LINEAR },

    /* CLUTTER_TEXTURE_QUALITY_HIGH */
    { COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR, COGL_MATERIAL_FILTER_LINEAR }
  };

static inline void
clutter_texture_quality_to_filters (ClutterTextureQuality  quality,
                                    gint                  *min_filter_p,
                                    gint                  *mag_filter_p)
{
  g_return_if_fail (quality < G_N_ELEMENTS (clutter_texture_quality_filters));

  if (min_filter_p)
    *min_filter_p = clutter_texture_quality_filters[quality].min_filter;

  if (mag_filter_p)
    *mag_filter_p = clutter_texture_quality_filters[quality].mag_filter;
}

static void
texture_free_gl_resources (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv = texture->priv;

  CLUTTER_MARK();

  if (priv->material != COGL_INVALID_HANDLE)
    /* We want to keep the layer so that the filter settings will
       remain but we want to free its resources so we clear the
       texture handle */
    cogl_material_set_layer (priv->material, 0, COGL_INVALID_HANDLE);
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

  if (priv->fbo_source != NULL)
    {
      /* Free up our fbo handle and texture resources, realize will recreate */
      cogl_handle_unref (priv->fbo_handle);
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
      CoglHandle tex;

      /* Handle FBO's */

      if (priv->no_slice)
        flags |= COGL_TEXTURE_NO_SLICING;

      tex = cogl_texture_new_with_size (priv->image_width,
                                        priv->image_height,
                                        flags,
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE);

      cogl_material_set_layer (priv->material, 0, tex);

      priv->fbo_handle = cogl_offscreen_new_to_texture (tex);

      /* The material now has a reference to the texture so it will
         stick around */
      cogl_handle_unref (tex);

      if (priv->fbo_handle == COGL_INVALID_HANDLE)
        {
          g_warning ("%s: Offscreen texture creation failed", G_STRLOC);
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
          return;
        }

      clutter_actor_set_size (actor, priv->image_width, priv->image_height);

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
      /* If we have no data, then realization is a no-op but
       * we still want to be in REALIZED state to maintain
       * invariants. We may have already created the texture
       * if someone set some data earlier, or we may create it
       * later if someone sets some data later. The fact that
       * we may have created it earlier is really a bug, since
       * it means ClutterTexture can have GL resources without
       * being realized.
       */
    }

  CLUTTER_NOTE (TEXTURE, "Texture realized");
}

static void
clutter_texture_get_preferred_width (ClutterActor *self,
                                     gfloat        for_height,
                                     gfloat       *min_width_p,
                                     gfloat       *natural_width_p)
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
              priv->image_height <= 0)
            {
              *natural_width_p = priv->image_width;
            }
          else
            {
              /* Set the natural width so as to preserve the aspect ratio */
              gfloat ratio = (gfloat) priv->image_width
                           / (gfloat) priv->image_height;

              *natural_width_p = ratio * for_height;
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
                                      gfloat        for_width,
                                      gfloat       *min_height_p,
                                      gfloat       *natural_height_p)
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
              priv->image_width <= 0)
            {
              *natural_height_p = priv->image_height;
            }
          else
            {
              /* Set the natural height so as to preserve the aspect ratio */
              gfloat ratio = (gfloat) priv->image_height
                           / (gfloat) priv->image_width;

              *natural_height_p = ratio * for_width;
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
clutter_texture_allocate (ClutterActor           *self,
			  const ClutterActorBox  *box,
                          ClutterAllocationFlags  flags)
{
  ClutterTexturePrivate *priv = CLUTTER_TEXTURE (self)->priv;

  /* chain up to set actor->allocation */
  CLUTTER_ACTOR_CLASS (clutter_texture_parent_class)->allocate (self,
                                                                box,
                                                                flags);

  /* If we adopted the source fbo then allocate that at its preferred
     size */
  if (priv->fbo_source && clutter_actor_get_parent (priv->fbo_source) == self)
    clutter_actor_allocate_preferred_size (priv->fbo_source, flags);
}

static void
set_viewport_with_buffer_under_fbo_source (ClutterActor *fbo_source,
                                           int viewport_width,
                                           int viewport_height)
{
  ClutterVertex verts[4];
  float x_min = G_MAXFLOAT, y_min = G_MAXFLOAT;
  int x_offset, y_offset;
  int i;

  /* Get the actors allocation transformed into screen coordinates.
   *
   * XXX: Note: this may not be a bounding box for the actor, since an
   * actor with depth may escape the box due to its perspective
   * projection. */
  clutter_actor_get_abs_allocation_vertices (fbo_source, verts);

  for (i = 0; i < G_N_ELEMENTS (verts); ++i)
    {
      if (verts[i].x < x_min)
	x_min = verts[i].x;
      if (verts[i].y < y_min)
	y_min = verts[i].y;
    }

  /* XXX: It's not good enough to round by simply truncating the fraction here
   * via a cast, as it results in offscreen rendering being offset by 1 pixel
   * in many cases... */
#define ROUND(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))

  x_offset = ROUND (-x_min);
  y_offset = ROUND (-y_min);

#undef ROUND

  /* translate the viewport so that the source actor lands on the
   * sub-region backed by the offscreen framebuffer... */
  cogl_set_viewport (x_offset, y_offset, viewport_width, viewport_height);
}

static void
update_fbo (ClutterActor *self)
{
  ClutterTexture        *texture = CLUTTER_TEXTURE (self);
  ClutterTexturePrivate *priv = texture->priv;
  ClutterMainContext    *context;
  ClutterShader         *shader = NULL;
  ClutterActor          *stage = NULL;
  ClutterPerspective     perspective;
  CoglColor              transparent_col;

  context = _clutter_context_get_default ();

  if (context->shaders)
    shader = clutter_actor_get_shader (context->shaders->data);

  /* Temporarily turn off the shader on the top of the context's shader stack,
   * to restore the GL pipeline to it's natural state.
   */
  if (shader)
    clutter_shader_set_is_enabled (shader, FALSE);

  /* Redirect drawing to the fbo */
  cogl_push_framebuffer (priv->fbo_handle);

  if ((stage = clutter_actor_get_stage (self)))
    {
      gfloat stage_width, stage_height;
      ClutterActor *source_parent;

      /* We copy the projection and modelview matrices from the stage to
       * the offscreen framebuffer and create a viewport larger than the
       * offscreen framebuffer - the same size as the stage.
       *
       * The fbo source actor gets rendered into this stage size viewport at the
       * same position it normally would after applying all it's usual parent
       * transforms and it's own scale and rotate transforms etc.
       *
       * The viewport is offset such that the offscreen buffer will be positioned
       * under the actor.
       */

      clutter_stage_get_perspective (CLUTTER_STAGE (stage), &perspective);
      clutter_actor_get_size (stage, &stage_width, &stage_height);

      /* Set the projection matrix modelview matrix and viewport size as
       * they are for the stage... */
      _cogl_setup_viewport (stage_width, stage_height,
                            perspective.fovy,
                            perspective.aspect,
                            perspective.z_near,
                            perspective.z_far);

      /* Negatively offset the viewport so that the offscreen framebuffer is
       * position underneath the fbo_source actor... */
      set_viewport_with_buffer_under_fbo_source (priv->fbo_source,
                                                 stage_width,
                                                 stage_height);

      /* Reapply the source's parent transformations */
      if ((source_parent = clutter_actor_get_parent (priv->fbo_source)))
        _clutter_actor_apply_modelview_transform_recursive (source_parent,
                                                            NULL);
    }


  /* cogl_clear is called to clear the buffers */
  cogl_color_set_from_4ub (&transparent_col, 0, 0, 0, 0);
  cogl_clear (&transparent_col,
              COGL_BUFFER_BIT_COLOR |
              COGL_BUFFER_BIT_DEPTH);
  cogl_disable_fog ();

  /* Render the actor to the fbo */
  clutter_actor_paint (priv->fbo_source);

  /* Restore drawing to the previous framebuffer */
  cogl_pop_framebuffer ();

  /* If there is a shader on top of the shader stack, turn it back on. */
  if (shader)
    clutter_shader_set_is_enabled (shader, TRUE);
}

static void
clutter_texture_paint (ClutterActor *self)
{
  ClutterTexture *texture = CLUTTER_TEXTURE (self);
  ClutterTexturePrivate *priv = texture->priv;
  ClutterActorBox box = { 0, };
  gfloat          t_w, t_h;
  guint8          paint_opacity = clutter_actor_get_paint_opacity (self);

  if (paint_opacity == 0)
    {
      /* Bail early if painting the actor would be a no-op, custom actors that
       * might cause a lot of work/state changes should all do this.
       */
      return;
    }

  if (priv->fbo_handle != COGL_INVALID_HANDLE)
    update_fbo (self);

  CLUTTER_NOTE (PAINT,
                "painting texture '%s'",
		clutter_actor_get_name (self) ? clutter_actor_get_name (self)
                                              : "unknown");

  cogl_material_set_color4ub (priv->material,
			      paint_opacity, paint_opacity, paint_opacity, paint_opacity);

  clutter_actor_get_allocation_box (self, &box);

  CLUTTER_NOTE (PAINT, "paint to x1: %f, y1: %f x2: %f, y2: %f "
		       "opacity: %i",
		box.x1, box.y1, box.x2, box.y2,
		clutter_actor_get_opacity (self));

  if (priv->repeat_x && priv->image_width > 0)
    t_w = (box.x2 - box.x1) / (gfloat) priv->image_width;
  else
    t_w = 1.0;

  if (priv->repeat_y && priv->image_height > 0)
    t_h = (box.y2 - box.y1) / (gfloat) priv->image_height;
  else
    t_h = 1.0;

  /* Paint will have translated us */
  cogl_set_source (priv->material);
  cogl_rectangle_with_texture_coords (0, 0,
			              box.x2 - box.x1,
                                      box.y2 - box.y1,
			              0, 0, t_w, t_h);
}

static void
clutter_texture_async_data_free (ClutterTextureAsyncData *data)
{
  /* This function should only be called either from the main thread
     once it is known that the load thread has completed or from the
     load thread/upload function itself if the abort flag is true (in
     which case the main thread has disowned the data) */

  if (data->load_filename)
    g_free (data->load_filename);

  if (data->load_bitmap)
    cogl_handle_unref (data->load_bitmap);

  if (data->load_error)
    g_error_free (data->load_error);

  if (data->mutex)
    g_mutex_free (data->mutex);

  g_slice_free (ClutterTextureAsyncData, data);
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

  if (priv->async_data)
    {
      GMutex *mutex = priv->async_data->mutex;

      /* The mutex will only be NULL if the no thread was used for
         this load, in which case there's no need for any
         synchronization */
      if (mutex)
        g_mutex_lock (mutex);

      /* If there is no thread behind this load then we can just abort
         the idle handler and destroy the load data immediately */
      if (priv->async_data->load_idle)
        {
          g_source_remove (priv->async_data->load_idle);
          priv->async_data->load_idle = 0;

          if (mutex)
            g_mutex_unlock (mutex);

          clutter_texture_async_data_free (priv->async_data);
        }
      else
        {
          /* Otherwise we need to tell the thread to abort and disown
             the data, if the data has been loaded and decoded the data
             is now waiting for a master clock iteration to be repainted */
          priv->async_data->abort = TRUE;

          if (mutex)
            g_mutex_unlock (mutex);
        }

      priv->async_data = NULL;
    }
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
      cogl_handle_unref (priv->material);
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
  ClutterTexture *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE (object);
  priv = texture->priv;

  switch (prop_id)
    {
    case PROP_SYNC_SIZE:
      clutter_texture_set_sync_size (texture, g_value_get_boolean (value));
      break;

    case PROP_REPEAT_X:
      clutter_texture_set_repeat (texture,
                                  g_value_get_boolean (value),
                                  priv->repeat_y);
      break;

    case PROP_REPEAT_Y:
      clutter_texture_set_repeat (texture,
                                  priv->repeat_x,
                                  g_value_get_boolean (value));
      break;

    case PROP_FILTER_QUALITY:
      clutter_texture_set_filter_quality (texture,
					  g_value_get_enum (value));
      break;

    case PROP_COGL_TEXTURE:
      {
        CoglHandle hnd = g_value_get_boxed (value);

        clutter_texture_set_cogl_texture (texture, hnd);
      }
      break;

    case PROP_COGL_MATERIAL:
      {
        CoglHandle hnd = g_value_get_boxed (value);

        clutter_texture_set_cogl_material (texture, hnd);
      }
      break;

    case PROP_FILENAME:
      clutter_texture_set_from_file (texture,
                                     g_value_get_string (value),
                                     NULL);
      break;

    case PROP_NO_SLICE:
      priv->no_slice = g_value_get_boolean (value);
      break;

    case PROP_KEEP_ASPECT_RATIO:
      clutter_texture_set_keep_aspect_ratio (texture,
                                             g_value_get_boolean (value));
      break;

    case PROP_LOAD_DATA_ASYNC:
      clutter_texture_set_load_data_async (texture,
                                           g_value_get_boolean (value));
      break;

    case PROP_LOAD_ASYNC:
      clutter_texture_set_load_async (texture, g_value_get_boolean (value));
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

  texture = CLUTTER_TEXTURE(object);
  priv = texture->priv;

  switch (prop_id)
    {
    case PROP_PIXEL_FORMAT:
      g_value_set_enum (value, clutter_texture_get_pixel_format (texture));
      break;

    case PROP_MAX_TILE_WASTE:
      g_value_set_int (value, clutter_texture_get_max_tile_waste (texture));
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

    case PROP_COGL_MATERIAL:
      g_value_set_boxed (value, clutter_texture_get_cogl_material (texture));
      break;

    case PROP_NO_SLICE:
      g_value_set_boolean (value, priv->no_slice);
      break;

    case PROP_KEEP_ASPECT_RATIO:
      g_value_set_boolean (value, priv->keep_aspect_ratio);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_texture_class_init (ClutterTextureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

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
    (gobject_class, PROP_MAX_TILE_WASTE,
     g_param_spec_int ("tile-waste",
                       "Tile Waste",
                       "Maximum waste area of a sliced texture",
                       -1, G_MAXINT,
                       COGL_TEXTURE_MAX_WASTE,
                       CLUTTER_PARAM_READABLE));

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
    (gobject_class, PROP_PIXEL_FORMAT,
     g_param_spec_enum ("pixel-format",
		        "Texture pixel format",
		        "CoglPixelFormat to use.",
                        COGL_TYPE_PIXEL_FORMAT,
		        COGL_PIXEL_FORMAT_RGBA_8888,
		        CLUTTER_PARAM_READABLE));

  g_object_class_install_property
    (gobject_class, PROP_COGL_TEXTURE,
     g_param_spec_boxed ("cogl-texture",
			 "COGL Texture",
			 "The underlying COGL texture handle used to draw "
			 "this actor",
			 COGL_TYPE_HANDLE,
			 G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_COGL_MATERIAL,
     g_param_spec_boxed ("cogl-material",
			 "COGL Material",
			 "The underlying COGL material handle used to draw "
			 "this actor",
			 COGL_TYPE_HANDLE,
			 G_PARAM_READWRITE));

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
   * Tries to load a texture from a filename by using a local thread to perform
   * the read operations. The initially created texture has dimensions 0x0 when
   * the true size becomes available the #ClutterTexture::size-change signal is
   * emitted and when the image has completed loading the
   * #ClutterTexture::load-finished signal is emitted.
   *
   * Threading is only enabled if g_thread_init() has been called prior to
   * clutter_init(), otherwise #ClutterTexture will use the main loop to load
   * the image.
   *
   * The upload of the texture data on the GL pipeline is not asynchronous, as
   * it must be performed from within the same thread that called
   * clutter_main().
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
			   CLUTTER_PARAM_WRITABLE));


  /**
   * ClutterTexture:load-data-async:
   *
   * Like #ClutterTexture:load-async but loads the width and height
   * synchronously causing some blocking.
   *
   * Since: 1.0
   */
  g_object_class_install_property
    (gobject_class, PROP_LOAD_DATA_ASYNC,
     g_param_spec_boolean ("load-data-async",
			   "Load data asynchronously",
			   "Decode image data files inside a thread to reduce "
                           "blocking when loading images.",
			   FALSE,
			   CLUTTER_PARAM_WRITABLE));



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
		  G_TYPE_NONE, 2,
                  G_TYPE_INT,
                  G_TYPE_INT);
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
          g_warning ("Unable to open image path at '%s': %s",
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

  priv->repeat_x          = FALSE;
  priv->repeat_y          = FALSE;
  priv->sync_actor_size   = TRUE;
  priv->material          = cogl_material_new ();
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

  priv->local_data = g_malloc (priv->local_data_rowstride
			       * priv->local_data_height);

  if (cogl_texture_get_data (cogl_texture,
			     priv->local_data_has_alpha
			     ? COGL_PIXEL_FORMAT_RGBA_8888_PRE
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
				     CLUTTER_TEXTURE_RGB_FLAG_PREMULT, NULL);

  g_free (priv->local_data);
  priv->local_data = NULL;
}

/**
 * clutter_texture_get_cogl_material:
 * @texture: A #ClutterTexture
 *
 * Returns a handle to the underlying COGL material used for drawing
 * the actor. No extra reference is taken so if you need to keep the
 * handle then you should call cogl_handle_ref() on it.
 *
 * Since: 1.0
 *
 * Return value: COGL material handle
 */
CoglHandle
clutter_texture_get_cogl_material (ClutterTexture *texture)
{
  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), COGL_INVALID_HANDLE);

  return texture->priv->material;
}

/**
 * clutter_texture_set_cogl_material:
 * @texture: A #ClutterTexture
 * @cogl_material: A CoglHandle for a material
 *
 * Replaces the underlying Cogl material drawn by this actor with
 * @cogl_material. A reference to the material is taken so if the
 * handle is no longer needed it should be deref'd with
 * cogl_handle_unref. Texture data is attached to the material so
 * calling this function also replaces the Cogl
 * texture. #ClutterTexture requires that the material have a texture
 * layer so you should set one on the material before calling this
 * function.
 *
 * Since: 0.8
 *
 */
void
clutter_texture_set_cogl_material (ClutterTexture *texture,
                                   CoglHandle cogl_material)
{
  CoglHandle cogl_texture;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  cogl_handle_ref (cogl_material);

  /* This */
  if (texture->priv->material)
    cogl_handle_unref (texture->priv->material);

  texture->priv->material = cogl_material;

  /* XXX: We are re-asserting the first layer of the new material to ensure the
   * priv state is in sync with the contents of the material. */
  cogl_texture = clutter_texture_get_cogl_texture (texture);
  clutter_texture_set_cogl_texture (texture, cogl_texture);
  /* XXX: If we add support for more material layers, this will need
   * extending */
}

/**
 * clutter_texture_get_cogl_texture
 * @texture: A #ClutterTexture
 *
 * Retrieves the handle to the underlying COGL texture used for drawing
 * the actor. No extra reference is taken so if you need to keep the
 * handle then you should call cogl_handle_ref() on it.
 *
 * The texture handle returned is the first layer of the material
 * handle used by the #ClutterTexture. If you need to access the other
 * layers you should use clutter_texture_get_cogl_material() instead
 * and use the #CoglMaterial API.
 *
 * Since: 0.8
 *
 * Return value: COGL texture handle
 */
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
 * no longer needed it should be deref'd with cogl_handle_unref.
 *
 * This should not be called on an unrealizable texture (one that
 * isn't inside a stage). (Currently the ClutterTexture
 * implementation relies on being able to have a GL texture while
 * unrealized, which means you can get away with it, but it's
 * not correct and may change in the future.)
 *
 * Since: 0.8
 */
void
clutter_texture_set_cogl_texture (ClutterTexture  *texture,
				  CoglHandle       cogl_tex)
{
  ClutterTexturePrivate  *priv;
  gboolean size_changed;
  guint width, height;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));
  g_return_if_fail (cogl_is_texture (cogl_tex));

  /* FIXME this implementation should realize the actor if it's in a
   * stage, and warn and return if not in a stage yet. However, right
   * now everything would break if we did that, so we just fudge it
   * and we're broken: we can have a texture without being realized.
   */

  priv = texture->priv;

  width = cogl_texture_get_width (cogl_tex);
  height = cogl_texture_get_height (cogl_tex);

  /* Reference the new texture now in case it is the same one we are
     already using */
  cogl_handle_ref (cogl_tex);

  /* Remove FBO if exisiting */
  if (priv->fbo_source)
    texture_fbo_free_resources (texture);

  /* Remove old texture */
  texture_free_gl_resources (texture);

  /* Free any saved data so realization doesn't resend it to GL */
  if (priv->local_data)
    {
      g_free (priv->local_data);
      priv->local_data = NULL;
    }

  /* Use the new texture */

  cogl_material_set_layer (priv->material, 0, cogl_tex);

  /* The material now holds a reference to the texture so we can
     safely release the reference we claimed above */
  cogl_handle_unref (cogl_tex);

  size_changed = (width != priv->image_width || height != priv->image_height);
  priv->image_width = width;
  priv->image_height = height;

  CLUTTER_NOTE (TEXTURE, "set size (w:%d, h:%d)",
		priv->image_width,
		priv->image_height);

  if (size_changed)
    {
      g_signal_emit (texture, texture_signals[SIZE_CHANGE], 0,
                     priv->image_width,
                     priv->image_height);

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

  if (priv->no_slice)
    flags |= COGL_TEXTURE_NO_SLICING;

  /* FIXME if we are not realized, we should store the data
   * for future use, instead of creating the texture.
   */
  new_texture = cogl_texture_new_from_data (width, height,
                                            flags,
                                            source_format,
                                            COGL_PIXEL_FORMAT_ANY,
                                            rowstride,
                                            data);

  if (G_UNLIKELY (new_texture == COGL_INVALID_HANDLE))
    {
      GError *inner_error = NULL;

      g_set_error (&inner_error, CLUTTER_TEXTURE_ERROR,
                   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
                   "Failed to create COGL texture");

      g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0, inner_error);

      if (error != NULL)
        g_propagate_error (error, inner_error);
      else
        g_error_free (inner_error);

      return FALSE;
    }

  clutter_texture_set_cogl_texture (texture, new_texture);

  cogl_handle_unref (new_texture);

  g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0, NULL);

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
 * Return value: %TRUE on success, %FALSE on failure.
 *
 * Since: 0.4.
 */
gboolean
clutter_texture_set_from_rgb_data (ClutterTexture       *texture,
				   const guchar         *data,
				   gboolean              has_alpha,
				   gint                  width,
				   gint                  height,
				   gint                  rowstride,
				   gint                  bpp,
				   ClutterTextureFlags   flags,
				   GError              **error)
{
  ClutterTexturePrivate *priv;
  CoglPixelFormat source_format;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  priv = texture->priv;

  /* Convert the flags to a CoglPixelFormat */
  if (has_alpha)
    {
      if (bpp != 4)
	{
	  g_set_error (error, CLUTTER_TEXTURE_ERROR,
		       CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		       "Unsupported bits per pixel value '%d': "
                       "Clutter supports only a BPP value of 4 "
                       "for RGBA data",
                       bpp);
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
		       "Unsupported bits per pixel value '%d': "
                       "Clutter supports only a BPP value of 3 "
                       "for RGB data",
                       bpp);
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
		   "YUV2 textues are not supported");
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
 * @bitmap: a handle to a CoglBitmap
 * @error: load error
 *
 * If @error is %NULL, loads @bitmap into a #CoglTexture.
 *
 * This function emits the ::load-finished signal on @self.
 */
static void
clutter_texture_async_load_complete (ClutterTexture *self,
                                     CoglHandle      bitmap,
                                     const GError   *error)
{
  ClutterTexturePrivate *priv = self->priv;
  CoglHandle handle;
  CoglTextureFlags flags = COGL_TEXTURE_NONE;

  priv->async_data = NULL;

  if (error == NULL)
    {
      if (priv->no_slice)
        flags |= COGL_TEXTURE_NO_SLICING;

      handle = cogl_texture_new_from_bitmap (bitmap,
                                             flags,
                                             COGL_PIXEL_FORMAT_ANY);
      clutter_texture_set_cogl_texture (self, handle);

      if (priv->load_size_async)
        {
          g_signal_emit (self, texture_signals[SIZE_CHANGE], 0,
                         cogl_texture_get_width (handle),
                         cogl_texture_get_height (handle));
        }

      cogl_handle_unref (handle);
    }

  g_signal_emit (self, texture_signals[LOAD_FINISHED], 0, error);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

static gboolean
clutter_texture_thread_idle_func (gpointer user_data)
{
  ClutterTextureAsyncData *data = user_data;

  /* Grab the mutex so we can be sure the thread has unlocked it
     before we destroy it */
  g_mutex_lock (data->mutex);
  if (data->abort)
    {
      g_mutex_unlock (data->mutex);
      clutter_texture_async_data_free (data);
      return FALSE;
    }
  g_mutex_unlock (data->mutex);

  clutter_texture_async_load_complete (data->texture, data->load_bitmap,
                                       data->load_error);

  clutter_texture_async_data_free (data);

  return FALSE;
}

static gboolean
texture_repaint_upload_func (gpointer user_data)
{
  gulong start_time;

  g_static_mutex_lock (&upload_list_mutex);

  if (upload_list)
    {
      start_time = clutter_get_timestamp ();

      /* continue uploading textures as long as we havent spent more
       * then 5ms doing so this stage redraw cycle.
       */
      do
        {
          ClutterTextureAsyncData *data = upload_list->data;

          clutter_texture_thread_idle_func (data);

          upload_list = g_list_remove (upload_list, data);
        }
      while (upload_list && clutter_get_timestamp () < start_time + 5 * 1000);
    }

  if (upload_list)
    {
      ClutterMasterClock *master_clock;

      master_clock = _clutter_master_clock_get_default ();
      _clutter_master_clock_ensure_next_iteration (master_clock);
    }

  g_static_mutex_unlock (&upload_list_mutex);

  return TRUE;
}

static void
clutter_texture_thread_func (gpointer user_data, gpointer pool_data)
{
  ClutterTextureAsyncData *data = user_data;
  gboolean should_abort;

  /* Make sure we haven't been told to abort before the thread had a
     chance to run */
  g_mutex_lock (data->mutex);
  should_abort = data->abort;
  g_mutex_unlock (data->mutex);

  if (should_abort)
    {
      /* If we've been told to abort then main thread has disowned the
         async data and we need to free it */
      clutter_texture_async_data_free (data);
      return;
    }

  data->load_bitmap = cogl_bitmap_new_from_file (data->load_filename,
                                                 &data->load_error);

  /* Check again if we've been told to abort */
  g_mutex_lock (data->mutex);

  if (data->abort)
    {
      g_mutex_unlock (data->mutex);

      clutter_texture_async_data_free (data);
    }
  else
    {
      ClutterMasterClock *master_clock = _clutter_master_clock_get_default ();

      /* Make sure we give the image to GL in the main thread, where we
       * hold the main Clutter lock. Once load_idle is non-NULL then the
       * main thread is guaranteed not to set the abort flag. It can't
       * set it while we're holding the mutex so we can safely start the
       * idle handler now without the possibility of calling the
       * callback after it is aborted */
      g_static_mutex_lock (&upload_list_mutex);

      if (repaint_upload_func == 0)
        {
          repaint_upload_func =
            clutter_threads_add_repaint_func (texture_repaint_upload_func,
                                              NULL, NULL);
        }

      upload_list = g_list_append (upload_list, data);
      data->upload_queued = TRUE;

      g_static_mutex_unlock (&upload_list_mutex);

      g_mutex_unlock (data->mutex);

      _clutter_master_clock_ensure_next_iteration (master_clock);
    }
}

static gboolean
clutter_texture_idle_func (gpointer user_data)
{
  ClutterTextureAsyncData *data = user_data;
  GError *internal_error = NULL;

  data->load_bitmap = cogl_bitmap_new_from_file (data->load_filename,
                                                 &internal_error);

  clutter_texture_async_load_complete (data->texture, data->load_bitmap,
                                       internal_error);

  if (internal_error)
    g_error_free (internal_error);

  clutter_texture_async_data_free (data);

  return FALSE;
}

/*
 * clutter_texture_async_load:
 * @self: a #ClutterTExture
 * @filename: name of the file to load
 * @error: return location for a #GError
 *
 * Starts an asynchronous load of the file name stored inside
 * the load_filename member of @data.
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
clutter_texture_async_load (ClutterTexture *self,
                            const gchar *filename,
                            GError **error)
{
  ClutterTexturePrivate *priv = self->priv;
  ClutterTextureAsyncData *data;
  gint width, height;
  gboolean res;

  /* ask the file for a size; if we cannot get the size then
   * there's no point in even continuing the asynchronous
   * loading, so we just stop there
   */

  if (priv->load_size_async)
    {
      res = TRUE;
      width = 0;
      height = 0;
    }
  else
    {
      res = cogl_bitmap_get_size_from_file (filename, &width, &height);
    }

  if (!res)
    {
      g_set_error (error, CLUTTER_TEXTURE_ERROR,
		   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		   "Failed to create COGL texture");
      return FALSE;
    }
  else
    {
      priv->image_width = width;
      priv->image_height = height;
    }

  clutter_texture_async_load_cancel (self);

  data = g_slice_new (ClutterTextureAsyncData);

  data->abort = FALSE;
  data->texture = self;
  data->load_idle = 0;
  data->load_filename = g_strdup (filename);
  data->load_bitmap = NULL;
  data->load_error = NULL;

  priv->async_data = data;

  if (g_thread_supported ())
    {
      data->mutex = g_mutex_new ();

      if (async_thread_pool == NULL)
        /* This apparently can't fail if exclusive == FALSE */
        async_thread_pool
          = g_thread_pool_new (clutter_texture_thread_func,
                               NULL, 1, FALSE, NULL);

      g_thread_pool_push (async_thread_pool, data, NULL);
    }
  else
    {
      data->mutex = NULL;

      data->load_idle
        = clutter_threads_add_idle (clutter_texture_idle_func, data);
    }

  return TRUE;
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
 * from disk will be performed asynchronously. #ClutterTexture::size-change
 * will be emitten when the size of the texture is available and
 * #ClutterTexture::load-finished will be emitted when the image has been
 * loaded or if an error occurred.
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

  priv = texture->priv;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (priv->load_data_async)
    return clutter_texture_async_load (texture, filename, error);

  if (priv->no_slice)
    flags |= COGL_TEXTURE_NO_SLICING;

  new_texture = cogl_texture_new_from_file (filename,
                                            flags,
                                            COGL_PIXEL_FORMAT_ANY,
                                            &internal_error);

  /* If COGL didn't give an error then make one up */
  if (internal_error == NULL && new_texture == COGL_INVALID_HANDLE)
    {
      g_set_error (&internal_error, CLUTTER_TEXTURE_ERROR,
                   CLUTTER_TEXTURE_ERROR_BAD_FORMAT,
		   "Failed to create COGL texture");
    }

  if (internal_error != NULL)
    {
      g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0,
                     internal_error);

      g_propagate_error (error, internal_error);

      return FALSE;
    }

  clutter_texture_set_cogl_texture (texture, new_texture);

  cogl_handle_unref (new_texture);

  g_signal_emit (texture, texture_signals[LOAD_FINISHED], 0, NULL);

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
      gint min_filter, mag_filter;

      min_filter = mag_filter = COGL_MATERIAL_FILTER_LINEAR;
      clutter_texture_quality_to_filters (filter_quality,
                                          &min_filter,
                                          &mag_filter);

      cogl_material_set_layer_filters (priv->material, 0,
                                       min_filter, mag_filter);

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
  const GList *layers;
  CoglMaterialFilter min_filter, mag_filter;
  int i;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), 0);

  priv = texture->priv;

  layers = cogl_material_get_layers (priv->material);
  if (layers == NULL)
    return CLUTTER_TEXTURE_QUALITY_MEDIUM;

  min_filter = cogl_material_layer_get_min_filter (layers->data);
  mag_filter = cogl_material_layer_get_mag_filter (layers->data);

  for (i = 0; i < G_N_ELEMENTS (clutter_texture_quality_filters); i++)
    if (clutter_texture_quality_filters[i].min_filter == min_filter
        && clutter_texture_quality_filters[i].mag_filter == mag_filter)
      return i;

  /* Unknown filter combination */
  return CLUTTER_TEXTURE_QUALITY_LOW;
}

/**
 * clutter_texture_get_max_tile_waste
 * @texture: A #ClutterTexture
 *
 * Gets the maximum waste that will be used when creating a texture or
 * -1 if slicing is disabled.
 *
 * Return value: The maximum waste or -1 if the texture waste is
 *   unlimited.
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
    return priv->no_slice ? -1 : COGL_TEXTURE_MAX_WASTE;
  else
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
 * @texture: a #ClutterTexture
 * @width: (out): return location for the width, or %NULL
 * @height: (out): return location for the height, or %NULL
 *
 * Gets the size in pixels of the untransformed underlying image
 */
void
clutter_texture_get_base_size (ClutterTexture *texture,
			       gint           *width,
			       gint           *height)
{
  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  if (width)
    *width = texture->priv->image_width;

  if (height)
    *height = texture->priv->image_height;
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

  /* attempt to realize ... */
  if (!CLUTTER_ACTOR_IS_REALIZED (texture) &&
      clutter_actor_get_stage (CLUTTER_ACTOR (texture)) != NULL)
    clutter_actor_realize (CLUTTER_ACTOR (texture));

  /* due to the fudging of clutter_texture_set_cogl_texture()
   * which allows setting a texture pre-realize, we may end
   * up having a texture even if we couldn't realize yet.
   */
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
  gfloat w, h;

  clutter_actor_get_transformed_size (priv->fbo_source, &w, &h);

  if (w != priv->image_width || h != priv->image_height)
    {
      CoglTextureFlags flags = COGL_TEXTURE_NONE;
      CoglHandle tex;

      /* tear down the FBO */
      if (priv->fbo_handle != COGL_INVALID_HANDLE)
        cogl_handle_unref (priv->fbo_handle);

      texture_free_gl_resources (texture);

      priv->image_width = w;
      priv->image_height = h;

      flags |= COGL_TEXTURE_NO_SLICING;

      tex = cogl_texture_new_with_size (MAX (priv->image_width, 1),
                                        MAX (priv->image_height, 1),
                                        flags,
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE);

      cogl_material_set_layer (priv->material, 0, tex);

      priv->fbo_handle = cogl_offscreen_new_to_texture (tex);

      /* The material now has a reference to the texture so it will
         stick around */
      cogl_handle_unref (tex);

      if (priv->fbo_handle == COGL_INVALID_HANDLE)
        {
          g_warning ("%s: Offscreen texture creation failed", G_STRLOC);
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
  gfloat w, h;

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

  priv->image_width = w;
  priv->image_height = h;

  clutter_actor_set_size (CLUTTER_ACTOR (texture),
                          priv->image_width,
                          priv->image_height);

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
      cogl_handle_unref (priv->fbo_handle);
      priv->fbo_handle = COGL_INVALID_HANDLE;
    }
}

/**
 * clutter_texture_set_sync_size:
 * @texture: a #ClutterTexture
 * @sync_size: %TRUE if the texture should have the same size of the
 *    underlying image data
 *
 * Sets whether @texture should have the same preferred size as the
 * underlying image data.
 *
 * Since: 1.0
 */
void
clutter_texture_set_sync_size (ClutterTexture *texture,
                               gboolean        sync_size)
{
  ClutterTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;

  if (priv->sync_actor_size != sync_size)
    {
      priv->sync_actor_size = sync_size;

      clutter_actor_queue_relayout (CLUTTER_ACTOR (texture));

      g_object_notify (G_OBJECT (texture), "sync-size");
    }
}

/**
 * clutter_texture_get_sync_size:
 * @texture: a #ClutterTexture
 *
 * Retrieves the value set with clutter_texture_get_sync_size()
 *
 * Return value: %TRUE if the #ClutterTexture should have the same
 *   preferred size of the underlying image data
 *
 * Since: 1.0
 */
gboolean
clutter_texture_get_sync_size (ClutterTexture *texture)
{
  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  return texture->priv->sync_actor_size;
}

/**
 * clutter_texture_set_repeat:
 * @texture: a #ClutterTexture
 * @repeat_x: %TRUE if the texture should repeat horizontally
 * @repeat_y: %TRUE if the texture should repeat vertically
 *
 * Sets whether the @texture should repeat horizontally or
 * vertically when the actor size is bigger than the image size
 *
 * Since: 1.0
 */
void
clutter_texture_set_repeat (ClutterTexture *texture,
                            gboolean        repeat_x,
                            gboolean        repeat_y)
{
  ClutterTexturePrivate *priv;
  gboolean changed = FALSE;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;

  g_object_freeze_notify (G_OBJECT (texture));

  if (priv->repeat_x != repeat_x)
    {
      priv->repeat_x = repeat_x;

      g_object_notify (G_OBJECT (texture), "repeat-x");

      changed = TRUE;
    }

  if (priv->repeat_y != repeat_y)
    {
      priv->repeat_y = repeat_y;

      g_object_notify (G_OBJECT (texture), "repeat-y");

      changed = TRUE;
    }

  if (changed)
    clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));

  g_object_thaw_notify (G_OBJECT (texture));
}

/**
 * clutter_texture_get_repeat:
 * @texture: a #ClutterTexture
 * @repeat_x: (out): return location for the horizontal repeat
 * @repeat_y: (out): return location for the vertical repeat
 *
 * Retrieves the horizontal and vertical repeat values set
 * using clutter_texture_set_repeat()
 *
 * Since: 1.0
 */
void
clutter_texture_get_repeat (ClutterTexture *texture,
                            gboolean       *repeat_x,
                            gboolean       *repeat_y)
{
  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  if (repeat_x != NULL)
    *repeat_x = texture->priv->repeat_x;

  if (repeat_y != NULL)
    *repeat_y = texture->priv->repeat_y;
}

/**
 * clutter_texture_get_pixel_format:
 * @texture: a #ClutterTexture
 *
 * Retrieves the pixel format used by @texture. This is
 * equivalent to:
 *
 * |[
 *   handle = clutter_texture_get_pixel_format (texture);
 *
 *   if (handle != COGL_INVALID_HANDLE)
 *     format = cogl_texture_get_format (handle);
 * ]|
 *
 * Return value: a #CoglPixelFormat value
 *
 * Since: 1.0
 */
CoglPixelFormat
clutter_texture_get_pixel_format (ClutterTexture *texture)
{
  CoglHandle cogl_texture;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), COGL_PIXEL_FORMAT_ANY);

  cogl_texture = clutter_texture_get_cogl_texture (texture);
  if (cogl_texture == COGL_INVALID_HANDLE)
    return COGL_PIXEL_FORMAT_ANY;

  return cogl_texture_get_format (cogl_texture);
}

/**
 * clutter_texture_set_keep_aspect_ratio:
 * @texture: a #ClutterTexture
 * @keep_aspect: %TRUE to maintain aspect ratio
 *
 * Sets whether @texture should have a preferred size maintaining
 * the aspect ratio of the underlying image
 *
 * Since: 1.0
 */
void
clutter_texture_set_keep_aspect_ratio (ClutterTexture *texture,
                                       gboolean        keep_aspect)
{
  ClutterTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;

  if (priv->keep_aspect_ratio != keep_aspect)
    {
      priv->keep_aspect_ratio = keep_aspect;

      clutter_actor_queue_relayout (CLUTTER_ACTOR (texture));

      g_object_notify (G_OBJECT (texture), "keep-aspect-ratio");
    }
}

/**
 * clutter_texture_get_keep_aspect_ratio:
 * @texture: a #ClutterTexture
 *
 * Retrieves the value set using clutter_texture_get_keep_aspect_ratio()
 *
 * Return value: %TRUE if the #ClutterTexture should maintain the
 *   aspect ratio of the underlying image
 *
 * Since: 1.0
 */
gboolean
clutter_texture_get_keep_aspect_ratio (ClutterTexture *texture)
{
  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  return texture->priv->keep_aspect_ratio;
}

/**
 * clutter_texture_set_load_async:
 * @texture: a #ClutterTexture
 * @load_async: %TRUE if the texture should asynchronously load data
 *   from a filename
 *
 * Sets whether @texture should use a worker thread to load the data
 * from disk asynchronously. Setting @load_async to %TRUE will make
 * clutter_texture_set_from_file() return immediately.
 *
 * See the #ClutterTexture:load-async property documentation, and
 * clutter_texture_set_load_data_async().
 *
 * Since: 1.0
 */
void
clutter_texture_set_load_async (ClutterTexture *texture,
                                gboolean        load_async)
{
  ClutterTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;

  if (priv->load_async_set != load_async)
    {
      priv->load_data_async = load_async;
      priv->load_size_async = load_async;

      priv->load_async_set = load_async;

      g_object_notify (G_OBJECT (texture), "load-async");
      g_object_notify (G_OBJECT (texture), "load-data-async");
    }
}

/**
 * clutter_texture_get_load_async:
 * @texture: a #ClutterTexture
 *
 * Retrieves the value set using clutter_texture_get_load_async()
 *
 * Return value: %TRUE if the #ClutterTexture should load the data from
 *   disk asynchronously
 *
 * Since: 1.0
 */
gboolean
clutter_texture_get_load_async (ClutterTexture *texture)
{
  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  return texture->priv->load_async_set;
}

/**
 * clutter_texture_set_load_data_async:
 * @texture: a #ClutterTexture
 * @load_async: %TRUE if the texture should asynchronously load data
 *   from a filename
 *
 * Sets whether @texture should use a worker thread to load the data
 * from disk asynchronously. Setting @load_async to %TRUE will make
 * clutter_texture_set_from_file() block until the #ClutterTexture has
 * determined the width and height of the image data.
 *
 * See the #ClutterTexture:load-async property documentation, and
 * clutter_texture_set_load_async().
 *
 * Since: 1.0
 */
void
clutter_texture_set_load_data_async (ClutterTexture *texture,
                                     gboolean        load_async)
{
  ClutterTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXTURE (texture));

  priv = texture->priv;

  if (priv->load_data_async != load_async)
    {
      /* load-data-async always unsets load-size-async */
      priv->load_data_async = load_async;
      priv->load_size_async = FALSE;

      priv->load_async_set = load_async;

      g_object_notify (G_OBJECT (texture), "load-async");
      g_object_notify (G_OBJECT (texture), "load-data-async");
    }
}

/**
 * clutter_texture_get_load_data_async:
 * @texture: a #ClutterTexture
 *
 * Retrieves the value set by clutter_texture_set_load_data_async()
 *
 * Return value: %TRUE if the #ClutterTexture should load the image
 *   data from a file asynchronously
 *
 * Since: 1.0
 */
gboolean
clutter_texture_get_load_data_async (ClutterTexture *texture)
{
  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), FALSE);

  return texture->priv->load_async_set &&
         texture->priv->load_data_async;
}
