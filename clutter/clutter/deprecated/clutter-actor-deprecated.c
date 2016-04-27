#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-actor.h"

#include "clutter-actor-private.h"
#include "clutter-private.h"
#include "clutter-shader.h"

typedef struct _ShaderData ShaderData;

struct _ShaderData
{
  ClutterShader *shader;

  /* back pointer to the actor */
  ClutterActor *actor;

  /* list of values that should be set on the shader
   * before each paint cycle
   */
  GHashTable *value_hash;
};

static void
shader_value_free (gpointer data)
{
  GValue *var = data;
  g_value_unset (var);
  g_slice_free (GValue, var);
}

static void
destroy_shader_data (gpointer data)
{
  ShaderData *shader_data = data;

  if (shader_data == NULL)
    return;

  if (shader_data->shader != NULL)
    {
      g_object_unref (shader_data->shader);
      shader_data->shader = NULL;
    }

  if (shader_data->value_hash != NULL)
    {
      g_hash_table_destroy (shader_data->value_hash);
      shader_data->value_hash = NULL;
    }

  g_slice_free (ShaderData, shader_data);
}

/**
 * clutter_actor_get_shader:
 * @self: a #ClutterActor
 *
 * Queries the currently set #ClutterShader on @self.
 *
 * Return value: (transfer none): The currently set #ClutterShader
 *   or %NULL if no shader is set.
 *
 * Since: 0.6
 *
 * Deprecated: 1.8: Use clutter_actor_get_effect() instead.
 */
ClutterShader *
clutter_actor_get_shader (ClutterActor *self)
{
  ShaderData *shader_data;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  shader_data = g_object_get_data (G_OBJECT (self), "-clutter-actor-shader-data");
  if (shader_data != NULL)
    return shader_data->shader;

  return NULL;
}

/**
 * clutter_actor_set_shader:
 * @self: a #ClutterActor
 * @shader: (allow-none): a #ClutterShader or %NULL to unset the shader.
 *
 * Sets the #ClutterShader to be used when rendering @self.
 *
 * If @shader is %NULL this function will unset any currently set shader
 * for the actor.
 *
 * Any #ClutterEffect applied to @self will take the precedence
 * over the #ClutterShader set using this function.
 *
 * Return value: %TRUE if the shader was successfully applied
 *   or removed
 *
 * Since: 0.6
 *
 * Deprecated: 1.8: Use #ClutterShaderEffect and
 *   clutter_actor_add_effect() instead.
 */
gboolean
clutter_actor_set_shader (ClutterActor  *self,
                          ClutterShader *shader)
{
  ShaderData *shader_data;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);
  g_return_val_if_fail (shader == NULL || CLUTTER_IS_SHADER (shader), FALSE);

  if (shader != NULL)
    g_object_ref (shader);
  else
    {
      /* if shader passed in is NULL we destroy the shader */
      g_object_set_data (G_OBJECT (self), "-clutter-actor-shader-data", NULL);
      return TRUE;
    }

  shader_data = g_object_get_data (G_OBJECT (self), "-clutter-actor-shader-data");
  if (shader_data == NULL)
    {
      shader_data = g_slice_new (ShaderData);
      shader_data->actor = self;
      shader_data->shader = NULL;
      shader_data->value_hash =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free,
                               shader_value_free);

      g_object_set_data_full (G_OBJECT (self), "-clutter-actor-shader-data",
                              shader_data,
                              destroy_shader_data);
    }

  if (shader_data->shader != NULL)
    g_object_unref (shader_data->shader);

  shader_data->shader = shader;

  clutter_actor_queue_redraw (self);

  return TRUE;
}

static void
set_each_param (gpointer key,
                gpointer value,
                gpointer user_data)
{
  ClutterShader *shader = user_data;
  const gchar *uniform = key;
  GValue *var = value;

  clutter_shader_set_uniform (shader, uniform, var);
}

void
_clutter_actor_shader_pre_paint (ClutterActor *actor,
                                 gboolean      repeat)
{
  ShaderData *shader_data;
  ClutterShader *shader;

  shader_data = g_object_get_data (G_OBJECT (actor), "-clutter-actor-shader-data");
  if (shader_data == NULL)
    return;

  shader = shader_data->shader;
  if (shader != NULL)
    {
      clutter_shader_set_is_enabled (shader, TRUE);

      g_hash_table_foreach (shader_data->value_hash, set_each_param, shader);

      if (!repeat)
        _clutter_context_push_shader_stack (actor);
    }
}

void
_clutter_actor_shader_post_paint (ClutterActor *actor)
{
  ShaderData *shader_data;
  ClutterShader *shader;

  shader_data = g_object_get_data (G_OBJECT (actor), "-clutter-actor-shader-data");
  if (G_LIKELY (shader_data == NULL))
    return;

  shader = shader_data->shader;
  if (shader != NULL)
    {
      ClutterActor *head;

      clutter_shader_set_is_enabled (shader, FALSE);

      /* remove the actor from the shaders stack; if there is another
       * actor inside it, then call pre-paint again to set its shader
       * but this time with the second argument being TRUE, indicating
       * that we are re-applying an existing shader and thus should it
       * not be prepended to the stack
       */
      head = _clutter_context_pop_shader_stack (actor);
      if (head != NULL)
        _clutter_actor_shader_pre_paint (head, TRUE);
    }
}

static inline void
clutter_actor_set_shader_param_internal (ClutterActor *self,
                                         const gchar  *param,
                                         const GValue *value)
{
  ShaderData *shader_data;
  GValue *var;

  shader_data = g_object_get_data (G_OBJECT (self), "-clutter-actor-shader-data");
  if (shader_data == NULL)
    return;

  var = g_slice_new0 (GValue);
  g_value_init (var, G_VALUE_TYPE (value));
  g_value_copy (value, var);
  g_hash_table_insert (shader_data->value_hash, g_strdup (param), var);

  clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_set_shader_param:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named parameter of the shader applied
 * to @actor.
 *
 * Since: 1.0
 *
 * Deprecated: 1.8: Use clutter_shader_effect_set_uniform_value() instead
 */
void
clutter_actor_set_shader_param (ClutterActor *self,
                                const gchar  *param,
                                const GValue *value)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (param != NULL);
  g_return_if_fail (CLUTTER_VALUE_HOLDS_SHADER_FLOAT (value) ||
                    CLUTTER_VALUE_HOLDS_SHADER_INT (value) ||
                    CLUTTER_VALUE_HOLDS_SHADER_MATRIX (value) ||
                    G_VALUE_HOLDS_FLOAT (value) ||
                    G_VALUE_HOLDS_INT (value));

  clutter_actor_set_shader_param_internal (self, param, value);
}

/**
 * clutter_actor_set_shader_param_float:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named float parameter of the shader applied
 * to @actor.
 *
 * Since: 0.8
 *
 * Deprecated: 1.8: Use clutter_shader_effect_set_uniform() instead
 */
void
clutter_actor_set_shader_param_float (ClutterActor *self,
                                      const gchar  *param,
                                      gfloat        value)
{
  GValue var = { 0, };

  g_value_init (&var, G_TYPE_FLOAT);
  g_value_set_float (&var, value);

  clutter_actor_set_shader_param_internal (self, param, &var);

  g_value_unset (&var);
}

/**
 * clutter_actor_set_shader_param_int:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named int parameter of the shader applied to
 * @actor.
 *
 * Since: 0.8
 *
 * Deprecated: 1.8: Use clutter_shader_effect_set_uniform() instead
 */
void
clutter_actor_set_shader_param_int (ClutterActor *self,
                                    const gchar  *param,
                                    gint          value)
{
  GValue var = { 0, };

  g_value_init (&var, G_TYPE_INT);
  g_value_set_int (&var, value);

  clutter_actor_set_shader_param_internal (self, param, &var);

  g_value_unset (&var);
}

/**
 * clutter_actor_set_geometry:
 * @self: A #ClutterActor
 * @geometry: A #ClutterGeometry
 *
 * Sets the actor's fixed position and forces its minimum and natural
 * size, in pixels. This means the untransformed actor will have the
 * given geometry. This is the same as calling clutter_actor_set_position()
 * and clutter_actor_set_size().
 *
 * Deprecated: 1.10: Use clutter_actor_set_position() and
 *   clutter_actor_set_size() instead.
 */
void
clutter_actor_set_geometry (ClutterActor          *self,
			    const ClutterGeometry *geometry)
{
  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_position (self, geometry->x, geometry->y);
  clutter_actor_set_size (self, geometry->width, geometry->height);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_get_geometry:
 * @self: A #ClutterActor
 * @geometry: (out caller-allocates): A location to store actors #ClutterGeometry
 *
 * Gets the size and position of an actor relative to its parent
 * actor. This is the same as calling clutter_actor_get_position() and
 * clutter_actor_get_size(). It tries to "do what you mean" and get the
 * requested size and position if the actor's allocation is invalid.
 *
 * Deprecated: 1.10: Use clutter_actor_get_position() and
 *   clutter_actor_get_size(), or clutter_actor_get_allocation_geometry()
 *   instead.
 */
void
clutter_actor_get_geometry (ClutterActor    *self,
			    ClutterGeometry *geometry)
{
  gfloat x, y, width, height;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (geometry != NULL);

  clutter_actor_get_position (self, &x, &y);
  clutter_actor_get_size (self, &width, &height);

  geometry->x = (int) x;
  geometry->y = (int) y;
  geometry->width = (int) width;
  geometry->height = (int) height;
}

/**
 * clutter_actor_get_allocation_geometry:
 * @self: A #ClutterActor
 * @geom: (out): allocation geometry in pixels
 *
 * Gets the layout box an actor has been assigned.  The allocation can
 * only be assumed valid inside a paint() method; anywhere else, it
 * may be out-of-date.
 *
 * An allocation does not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * The returned rectangle is in pixels.
 *
 * Since: 0.8
 *
 * Deprecated: 1.12: Use clutter_actor_get_allocation_box() instead.
 */
void
clutter_actor_get_allocation_geometry (ClutterActor    *self,
                                       ClutterGeometry *geom)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (geom != NULL);

  clutter_actor_get_allocation_box (self, &box);

  geom->x = CLUTTER_NEARBYINT (clutter_actor_box_get_x (&box));
  geom->y = CLUTTER_NEARBYINT (clutter_actor_box_get_y (&box));
  geom->width = CLUTTER_NEARBYINT (clutter_actor_box_get_width (&box));
  geom->height = CLUTTER_NEARBYINT (clutter_actor_box_get_height (&box));
}
