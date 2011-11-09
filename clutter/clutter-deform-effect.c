/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Based on the MxDeformTexture class, written by:
 *   Chris Lord <chris@linux.intel.com>
 */

/**
 * SECTION:clutter-deform-effect
 * @Title: ClutterDeformEffect
 * @Short_Description: A base class for effects deforming the geometry
 *   of an actor
 *
 * #ClutterDeformEffect is an abstract class providing all the plumbing
 * for creating effects that result in the deformation of an actor's
 * geometry.
 *
 * #ClutterDeformEffect uses offscreen buffers to render the contents of
 * a #ClutterActor and then the Cogl vertex buffers API to submit the
 * geometry to the GPU.
 *
 * <refsect2>
 *   <title>Implementing ClutterDeformEffect</title>
 *   <para>Sub-classes of #ClutterDeformEffect should override the
 *   <function>deform_vertex()</function> virtual function; this function
 *   is called on every vertex that needs to be deformed by the effect.
 *   Each passed vertex is an in-out parameter that initially contains the
 *   position of the vertex and should be modified according to a specific
 *   deformation algorithm.</para>
 * </refsect2>
 *
 * #ClutterDeformEffect is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-deform-effect.h"

#include <cogl/cogl.h>

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-offscreen-effect-private.h"
#include "clutter-private.h"

#define DEFAULT_N_TILES         32

struct _ClutterDeformEffectPrivate
{
  CoglHandle back_material;

  gint x_tiles;
  gint y_tiles;

  CoglHandle vbo;

  CoglHandle indices;
  CoglHandle back_indices;
  gint n_indices;

  CoglTextureVertex *vertices;

  gulong allocation_id;

  guint is_dirty : 1;
};

enum
{
  PROP_0,

  PROP_X_TILES,
  PROP_Y_TILES,

  PROP_BACK_MATERIAL,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_ABSTRACT_TYPE (ClutterDeformEffect,
                        clutter_deform_effect,
                        CLUTTER_TYPE_OFFSCREEN_EFFECT);

static void
clutter_deform_effect_real_deform_vertex (ClutterDeformEffect *effect,
                                          gfloat               width,
                                          gfloat               height,
                                          CoglTextureVertex   *vertex)
{
  g_warning ("%s: Deformation effect of type '%s' does not implement "
             "the required ClutterDeformEffect::deform_vertex virtual "
             "function.",
             G_STRLOC,
             G_OBJECT_TYPE_NAME (effect));
}

void
_clutter_deform_effect_deform_vertex (ClutterDeformEffect *effect,
                                      gfloat               width,
                                      gfloat               height,
                                      CoglTextureVertex   *vertex)
{
  CLUTTER_DEFORM_EFFECT_GET_CLASS (effect)->deform_vertex (effect,
                                                           width, height,
                                                           vertex);
}

static void
vbo_invalidate (ClutterActor           *actor,
                const ClutterActorBox  *allocation,
                ClutterAllocationFlags  flags,
                ClutterDeformEffect    *effect)
{
  effect->priv->is_dirty = TRUE;
}

static void
clutter_deform_effect_set_actor (ClutterActorMeta *meta,
                                 ClutterActor     *actor)
{
  ClutterDeformEffectPrivate *priv = CLUTTER_DEFORM_EFFECT (meta)->priv;

  if (priv->allocation_id != 0)
    {
      ClutterActor *old_actor = clutter_actor_meta_get_actor (meta);

      g_signal_handler_disconnect (old_actor, priv->allocation_id);
      priv->allocation_id = 0;
    }

  /* we need to invalidate the VBO whenever the allocation of the actor
   * changes
   */
  if (actor != NULL)
    priv->allocation_id = g_signal_connect (actor, "allocation-changed",
                                            G_CALLBACK (vbo_invalidate),
                                            meta);

  priv->is_dirty = TRUE;

  CLUTTER_ACTOR_META_CLASS (clutter_deform_effect_parent_class)->set_actor (meta, actor);
}

static void
clutter_deform_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterDeformEffect *self= CLUTTER_DEFORM_EFFECT (effect);
  ClutterDeformEffectPrivate *priv = self->priv;
  gboolean is_depth_enabled, is_cull_enabled;
  CoglHandle material;
  gint n_tiles;

  if (priv->is_dirty)
    {
      ClutterActor *actor;
      gfloat width, height;
      guint opacity;
      gint i, j;

      actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
      opacity = clutter_actor_get_paint_opacity (actor);

      /* if we don't have a target size, fall back to the actor's
       * allocation, though wrong it might be
       */
      if (!clutter_offscreen_effect_get_target_size (effect, &width, &height))
        clutter_actor_get_size (actor, &width, &height);

      for (i = 0; i < priv->y_tiles + 1; i++)
        {
          for (j = 0; j < priv->x_tiles + 1; j++)
            {
              CoglTextureVertex *vertex;

              vertex = &priv->vertices[(i * (priv->x_tiles + 1)) + j];

              vertex->tx = (float) j / priv->x_tiles;
              vertex->ty = (float) i / priv->y_tiles;

              vertex->x = width * vertex->tx;
              vertex->y = height * vertex->ty;
              vertex->z = 0.0f;

              cogl_color_init_from_4ub (&vertex->color, 255, 255, 255, opacity);

              _clutter_deform_effect_deform_vertex (self, width, height, vertex);
            }
        }

      /* XXX in theory, the sub-classes should tell us what they changed
       * in the texture vertices; we then would be able to avoid resubmitting
       * the same data, if it did not change. for the time being, we resubmit
       * everything
       */
      cogl_vertex_buffer_add (priv->vbo, "gl_Vertex",
                              3,
                              COGL_ATTRIBUTE_TYPE_FLOAT,
                              FALSE,
                              sizeof (CoglTextureVertex),
                              &priv->vertices->x);
      cogl_vertex_buffer_add (priv->vbo, "gl_MultiTexCoord0",
                              2,
                              COGL_ATTRIBUTE_TYPE_FLOAT,
                              FALSE,
                              sizeof (CoglTextureVertex),
                              &priv->vertices->tx);
      cogl_vertex_buffer_add (priv->vbo, "gl_Color",
                              4,
                              COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE,
                              FALSE,
                              sizeof (CoglTextureVertex),
                              &priv->vertices->color);

      priv->is_dirty = FALSE;
    }

  /* enable depth test, if it's not already enabled */
  is_depth_enabled = cogl_get_depth_test_enabled ();
  if (!is_depth_enabled)
    cogl_set_depth_test_enabled (TRUE);

  /* enable backface culling if it's not already enabled and if
   * we have a back material
   */
  is_cull_enabled = cogl_get_backface_culling_enabled ();
  if (priv->back_material != COGL_INVALID_HANDLE && !is_cull_enabled)
    cogl_set_backface_culling_enabled (TRUE);
  else if (priv->back_material == COGL_INVALID_HANDLE && is_cull_enabled)
    cogl_set_backface_culling_enabled (FALSE);

  n_tiles = (priv->x_tiles + 1) * (priv->y_tiles + 1);

  /* draw the front */
  material = clutter_offscreen_effect_get_target (effect);
  if (material != COGL_INVALID_HANDLE)
    {
      cogl_set_source (material);
      cogl_vertex_buffer_draw_elements (priv->vbo,
                                        COGL_VERTICES_MODE_TRIANGLE_STRIP,
                                        priv->indices,
                                        0,
                                        n_tiles,
                                        0,
                                        priv->n_indices);
    }

  /* draw the back */
  material = priv->back_material;
  if (material != COGL_INVALID_HANDLE)
    {
      cogl_set_source (priv->back_material);
      cogl_vertex_buffer_draw_elements (priv->vbo,
                                        COGL_VERTICES_MODE_TRIANGLE_STRIP,
                                        priv->back_indices,
                                        0,
                                        n_tiles,
                                        0,
                                        priv->n_indices);
    }

  /* restore the previous state */
  if (!is_depth_enabled)
    cogl_set_depth_test_enabled (FALSE);

  if (priv->back_material != COGL_INVALID_HANDLE && !is_cull_enabled)
    cogl_set_backface_culling_enabled (FALSE);
  else if (priv->back_material == COGL_INVALID_HANDLE && is_cull_enabled)
    cogl_set_backface_culling_enabled (TRUE);

  if (G_UNLIKELY (clutter_paint_debug_flags & CLUTTER_DEBUG_PAINT_DEFORM_TILES))
    {
      cogl_set_source_color4f (1.0, 0, 0, 1.0);
      cogl_vertex_buffer_draw_elements (priv->vbo,
                                        COGL_VERTICES_MODE_LINE_STRIP,
                                        priv->indices,
                                        0,
                                        n_tiles,
                                        0,
                                        priv->n_indices);

      cogl_vertex_buffer_draw_elements (priv->vbo,
                                        COGL_VERTICES_MODE_LINE_STRIP,
                                        priv->back_indices,
                                        0,
                                        n_tiles,
                                        0,
                                        priv->n_indices);
    }
}

static inline void
clutter_deform_effect_free_arrays (ClutterDeformEffect *self)
{
  ClutterDeformEffectPrivate *priv = self->priv;

  if (priv->vbo != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->vbo);
      priv->vbo = COGL_INVALID_HANDLE;
    }

  if (priv->indices != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->indices);
      priv->indices = COGL_INVALID_HANDLE;
    }

  g_free (priv->vertices);
  priv->vertices = NULL;
}

static void
clutter_deform_effect_init_arrays (ClutterDeformEffect *self)
{
  ClutterDeformEffectPrivate *priv = self->priv;
  GLushort *static_indices, *static_back_indices;
  GLushort *idx, *back_idx;
  gint x, y, direction;
  gint n_tiles;

  clutter_deform_effect_free_arrays (self);

  priv->n_indices = (2 + 2 * priv->x_tiles)
                  * priv->y_tiles
                  + (priv->y_tiles - 1);

  static_indices = g_new (GLushort, priv->n_indices);
  static_back_indices = g_new (GLushort, priv->n_indices);

#define MESH_INDEX(x,y) ((y) * (priv->x_tiles + 1) + (x))

  /* compute all the triangles from the various tiles */
  direction = 1;

  idx = static_indices;
  idx[0] = MESH_INDEX (0, 0);
  idx[1] = MESH_INDEX (0, 1);
  idx += 2;

  back_idx = static_back_indices;
  back_idx[0] = MESH_INDEX (priv->x_tiles, 0);
  back_idx[1] = MESH_INDEX (priv->x_tiles, 1);
  back_idx += 2;

  for (y = 0; y < priv->y_tiles; y++)
    {
      for (x = 0; x < priv->x_tiles; x++)
        {
          if (direction)
            {
              idx[0] = MESH_INDEX (x + 1, y);
              idx[1] = MESH_INDEX (x + 1, y + 1);

              back_idx[0] = MESH_INDEX (priv->x_tiles - (x + 1), y);
              back_idx[1] = MESH_INDEX (priv->x_tiles - (x + 1), y + 1);
            }
          else
            {
              idx[0] = MESH_INDEX (priv->x_tiles - x - 1, y);
              idx[1] = MESH_INDEX (priv->x_tiles - x - 1, y + 1);

              back_idx[0] = MESH_INDEX (x + 1, y);
              back_idx[1] = MESH_INDEX (x + 1, y + 1);
            }

          idx += 2;
          back_idx += 2;
        }

      if (y == (priv->y_tiles - 1))
        break;

      if (direction)
        {
          idx[0] = MESH_INDEX (priv->x_tiles, y + 1);
          idx[1] = MESH_INDEX (priv->x_tiles, y + 1);
          idx[2] = MESH_INDEX (priv->x_tiles, y + 2);

          back_idx[0] = MESH_INDEX (0, y + 1);
          back_idx[1] = MESH_INDEX (0, y + 1);
          back_idx[2] = MESH_INDEX (0, y + 2);
        }
      else
        {
          idx[0] = MESH_INDEX (0, y + 1);
          idx[1] = MESH_INDEX (0, y + 1);
          idx[2] = MESH_INDEX (0, y + 2);

          back_idx[0] = MESH_INDEX (priv->x_tiles, y + 1);
          back_idx[1] = MESH_INDEX (priv->x_tiles, y + 1);
          back_idx[2] = MESH_INDEX (priv->x_tiles, y + 2);
        }

      idx += 3;
      back_idx += 3;

      direction = !direction;
    }

#undef MESH_INDEX

  priv->indices =
    cogl_vertex_buffer_indices_new (COGL_INDICES_TYPE_UNSIGNED_SHORT,
                                    static_indices,
                                    priv->n_indices);
  priv->back_indices =
    cogl_vertex_buffer_indices_new (COGL_INDICES_TYPE_UNSIGNED_SHORT,
                                    static_back_indices,
                                    priv->n_indices);

  g_free (static_indices);
  g_free (static_back_indices);

  n_tiles = (priv->x_tiles + 1) * (priv->y_tiles + 1);
  priv->vertices = g_new (CoglTextureVertex, n_tiles);
  priv->vbo = cogl_vertex_buffer_new (n_tiles);

  priv->is_dirty = TRUE;
}

static inline void
clutter_deform_effect_free_back_material (ClutterDeformEffect *self)
{
  ClutterDeformEffectPrivate *priv = self->priv;

  if (priv->back_material != NULL)
    {
      cogl_handle_unref (priv->back_material);
      priv->back_material = COGL_INVALID_HANDLE;
    }
}

static void
clutter_deform_effect_finalize (GObject *gobject)
{
  ClutterDeformEffect *self = CLUTTER_DEFORM_EFFECT (gobject);

  clutter_deform_effect_free_arrays (self);
  clutter_deform_effect_free_back_material (self);

  G_OBJECT_CLASS (clutter_deform_effect_parent_class)->finalize (gobject);
}

static void
clutter_deform_effect_set_property (GObject      *gobject,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ClutterDeformEffect *self = CLUTTER_DEFORM_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_X_TILES:
      clutter_deform_effect_set_n_tiles (self, g_value_get_uint (value),
                                         self->priv->y_tiles);
      break;

    case PROP_Y_TILES:
      clutter_deform_effect_set_n_tiles (self, self->priv->x_tiles,
                                         g_value_get_uint (value));
      break;

    case PROP_BACK_MATERIAL:
      clutter_deform_effect_set_back_material (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_deform_effect_get_property (GObject    *gobject,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ClutterDeformEffectPrivate *priv = CLUTTER_DEFORM_EFFECT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_X_TILES:
      g_value_set_uint (value, priv->x_tiles);
      break;

    case PROP_Y_TILES:
      g_value_set_uint (value, priv->y_tiles);
      break;

    case PROP_BACK_MATERIAL:
      g_value_set_boxed (value, priv->back_material);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_deform_effect_class_init (ClutterDeformEffectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterDeformEffectPrivate));

  klass->deform_vertex = clutter_deform_effect_real_deform_vertex;

  /**
   * ClutterDeformEffect:x-tiles:
   *
   * The number of horizontal tiles. The bigger the number, the
   * smaller the tiles
   *
   * Since: 1.4
   */
  obj_props[PROP_X_TILES] =
    g_param_spec_uint ("x-tiles",
                       P_("Horizontal Tiles"),
                       P_("The number of horizontal tiles"),
                       1, G_MAXUINT,
                       DEFAULT_N_TILES,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDeformEffect:y-tiles:
   *
   * The number of vertical tiles. The bigger the number, the
   * smaller the tiles
   *
   * Since: 1.4
   */
  obj_props[PROP_Y_TILES] =
    g_param_spec_uint ("y-tiles",
                       P_("Vertical Tiles"),
                       P_("The number of vertical tiles"),
                       1, G_MAXUINT,
                       DEFAULT_N_TILES,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDeformEffect:back-material:
   *
   * A material to be used when painting the back of the actor
   * to which this effect has been applied
   *
   * By default, no material will be used
   *
   * Since: 1.4
   */
  obj_props[PROP_BACK_MATERIAL] =
    g_param_spec_boxed ("back-material",
                        P_("Back Material"),
                        P_("The material to be used when painting the back of the actor"),
                        COGL_TYPE_HANDLE,
                        CLUTTER_PARAM_READWRITE);

  gobject_class->finalize = clutter_deform_effect_finalize;
  gobject_class->set_property = clutter_deform_effect_set_property;
  gobject_class->get_property = clutter_deform_effect_get_property;
  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);

  meta_class->set_actor = clutter_deform_effect_set_actor;

  offscreen_class->paint_target = clutter_deform_effect_paint_target;
}

static void
clutter_deform_effect_init (ClutterDeformEffect *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLUTTER_TYPE_DEFORM_EFFECT,
                                            ClutterDeformEffectPrivate);

  self->priv->x_tiles = self->priv->y_tiles = DEFAULT_N_TILES;
  self->priv->back_material = COGL_INVALID_HANDLE;

  clutter_deform_effect_init_arrays (self);
}

/**
 * clutter_deform_effect_set_back_material:
 * @effect: a #ClutterDeformEffect
 * @material: (allow-none): a handle to a Cogl material
 *
 * Sets the material that should be used when drawing the back face
 * of the actor during a deformation
 *
 * The #ClutterDeformEffect will take a reference on the material's
 * handle
 *
 * Since: 1.4
 */
void
clutter_deform_effect_set_back_material (ClutterDeformEffect *effect,
                                         CoglHandle           material)
{
  ClutterDeformEffectPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DEFORM_EFFECT (effect));
  g_return_if_fail (material == COGL_INVALID_HANDLE || cogl_is_material (material));

  priv = effect->priv;

  clutter_deform_effect_free_back_material (effect);

  priv->back_material = material;
  if (priv->back_material != COGL_INVALID_HANDLE)
    cogl_handle_ref (priv->back_material);

  clutter_deform_effect_invalidate (effect);
}

/**
 * clutter_deform_effect_get_back_material:
 * @effect: a #ClutterDeformEffect
 *
 * Retrieves the handle to the back face material used by @effect
 *
 * Return value: (transfer none): a handle for the material, or %NULL.
 *   The returned material is owned by the #ClutterDeformEffect and it
 *   should not be freed directly
 *
 * Since: 1.4
 */
CoglHandle
clutter_deform_effect_get_back_material (ClutterDeformEffect *effect)
{
  g_return_val_if_fail (CLUTTER_IS_DEFORM_EFFECT (effect), NULL);

  return effect->priv->back_material;
}

/**
 * clutter_deform_effect_set_n_tiles:
 * @effect: a #ClutterDeformEffect
 * @x_tiles: number of horizontal tiles
 * @y_tiles: number of vertical tiles
 *
 * Sets the number of horizontal and vertical tiles to be used
 * when applying the effect
 *
 * More tiles allow a finer grained deformation at the expenses
 * of computation
 *
 * Since: 1.4
 */
void
clutter_deform_effect_set_n_tiles (ClutterDeformEffect *effect,
                                   guint                x_tiles,
                                   guint                y_tiles)
{
  ClutterDeformEffectPrivate *priv;
  gboolean tiles_changed = FALSE;

  g_return_if_fail (CLUTTER_IS_DEFORM_EFFECT (effect));
  g_return_if_fail (x_tiles > 0 && y_tiles > 0);

  priv = effect->priv;

  g_object_freeze_notify (G_OBJECT (effect));

  if (priv->x_tiles != x_tiles)
    {
      priv->x_tiles = x_tiles;

      g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_X_TILES]);

      tiles_changed = TRUE;
    }

  if (priv->y_tiles != y_tiles)
    {
      priv->y_tiles = y_tiles;

      g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_Y_TILES]);

      tiles_changed = TRUE;
    }

  if (tiles_changed)
    {
      clutter_deform_effect_init_arrays (effect);
      clutter_deform_effect_invalidate (effect);
    }

  g_object_thaw_notify (G_OBJECT (effect));
}

/**
 * clutter_deform_effect_get_n_tiles:
 * @effect: a #ClutterDeformEffect
 * @x_tiles: (out): return location for the number of horizontal tiles,
 *   or %NULL
 * @y_tiles: (out): return location for the number of vertical tiles,
 *   or %NULL
 *
 * Retrieves the number of horizontal and vertical tiles used to sub-divide
 * the actor's geometry during the effect
 *
 * Since: 1.4
 */
void
clutter_deform_effect_get_n_tiles (ClutterDeformEffect *effect,
                                   guint               *x_tiles,
                                   guint               *y_tiles)
{
  g_return_if_fail (CLUTTER_IS_DEFORM_EFFECT (effect));

  if (x_tiles != NULL)
    *x_tiles = effect->priv->x_tiles;

  if (y_tiles != NULL)
    *y_tiles = effect->priv->y_tiles;
}

/**
 * clutter_deform_effect_invalidate:
 * @effect: a #ClutterDeformEffect
 *
 * Invalidates the @effect<!-- -->'s vertices and, if it is associated
 * to an actor, it will queue a redraw
 *
 * Since: 1.4
 */
void
clutter_deform_effect_invalidate (ClutterDeformEffect *effect)
{
  ClutterActor *actor;

  g_return_if_fail (CLUTTER_IS_DEFORM_EFFECT (effect));

  if (effect->priv->is_dirty)
    return;

  effect->priv->is_dirty = TRUE;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  if (actor != NULL)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));
}
