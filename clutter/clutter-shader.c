/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Øyvind Kolås   <pippin@o-hand.com>
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

/**
 * SECTION:clutter-shader
 * @short_description: Programmable pipeline abstraction
 *
 * #ClutterShader is an object providing an abstraction over the
 * OpenGL programmable pipeline. By using #ClutterShader<!-- -->s is
 * possible to override the drawing pipeline by using small programs
 * also known as "shaders".
 *
 * #ClutterShader is available since Clutter 0.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

#include <cogl/cogl.h>

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-shader.h"

static GList *clutter_shaders_list = NULL;

#define CLUTTER_SHADER_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),  \
   CLUTTER_TYPE_SHADER, ClutterShaderPrivate))

typedef enum {
  CLUTTER_VERTEX_SHADER,
  CLUTTER_FRAGMENT_SHADER
} ClutterShaderType;

struct _ClutterShaderPrivate
{
  guint       bound            : 1; /* Shader is bound to the GL context */
  guint       is_enabled       : 1;

  guint       vertex_is_glsl   : 1;
  guint       fragment_is_glsl : 1;

  gchar      *vertex_source;        /* GLSL source for vertex shader */
  gchar      *fragment_source;      /* GLSL source for fragment shader */

  COGLhandle  program;

  COGLhandle  vertex_shader;
  COGLhandle  fragment_shader;
};

enum 
{
  PROP_0,

  PROP_VERTEX_SOURCE,
  PROP_FRAGMENT_SOURCE,
  PROP_BOUND,
  PROP_ENABLED
};



G_DEFINE_TYPE (ClutterShader, clutter_shader, G_TYPE_OBJECT);

G_CONST_RETURN gchar *clutter_shader_get_source (ClutterShader      *shader,
                                                 ClutterShaderType   type);

static void
clutter_shader_finalize (GObject *object)
{
  ClutterShader        *shader;
  ClutterShaderPrivate *priv;

  shader = CLUTTER_SHADER (object);
  priv   = shader->priv;

  clutter_shader_release (shader);

  clutter_shaders_list = g_list_remove (clutter_shaders_list, object);

  g_free (priv->fragment_source);
  g_free (priv->vertex_source);

  G_OBJECT_CLASS (clutter_shader_parent_class)->finalize (object);
}

static void
clutter_shader_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ClutterShader *shader = CLUTTER_SHADER(object);

  switch (prop_id)
    {
    case PROP_VERTEX_SOURCE:
      clutter_shader_set_vertex_source (shader, g_value_get_string (value), -1);
      break;
    case PROP_FRAGMENT_SOURCE:
      clutter_shader_set_fragment_source (shader, g_value_get_string (value), -1);
      break;
    case PROP_ENABLED:
      clutter_shader_set_is_enabled (shader, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_shader_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ClutterShader        *shader;
  ClutterShaderPrivate *priv;

  shader = CLUTTER_SHADER(object);
  priv = shader->priv;

  switch (prop_id)
    {
    case PROP_VERTEX_SOURCE:
      g_value_set_string (value, priv->vertex_source);
      break;
    case PROP_FRAGMENT_SOURCE:
      g_value_set_string (value, priv->fragment_source);
      break;
    case PROP_BOUND:
      g_value_set_boolean (value, priv->bound);
      break;
    case PROP_ENABLED:
      g_value_set_boolean (value, priv->is_enabled);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GObject *
clutter_shader_constructor (GType                  type,
                            guint                  n_params,
                            GObjectConstructParam *params)
{
  GObject         *object;

  object = G_OBJECT_CLASS (clutter_shader_parent_class)->constructor (type, n_params, params);

  /* add this instance to the global list of shaders */
  clutter_shaders_list = g_list_prepend (clutter_shaders_list, object);

  return object;
}


static void
clutter_shader_class_init (ClutterShaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize      = clutter_shader_finalize;
  object_class->set_property  = clutter_shader_set_property;
  object_class->get_property  = clutter_shader_get_property;
  object_class->constructor   = clutter_shader_constructor;

  g_type_class_add_private (klass, sizeof (ClutterShaderPrivate));

  /**
   * ClutterShader:vertex-source:
   *
   * FIXME
   *
   * Since: 0.6
   */
  g_object_class_install_property (object_class,
                                   PROP_VERTEX_SOURCE,
                                   g_param_spec_string ("vertex-source",
                                                        "Vertex Source",
                                                        "Source of vertex shader",
                                                        NULL,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterShader:fragment-source:
   *
   * FIXME
   *
   * Since: 0.6
   */
  g_object_class_install_property (object_class,
                                   PROP_FRAGMENT_SOURCE,
                                   g_param_spec_string ("fragment-source",
                                                        "Fragment Source",
                                                        "Source of fragment shader",
                                                        NULL,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterShader:bound:
   *
   * FIXME
   *
   * Since: 0.6
   */
  g_object_class_install_property (object_class,
                                   PROP_BOUND,
                                   g_param_spec_boolean ("bound",
                                                         "Bound",
                                                         "Whether the shader is bound",
                                                         FALSE,
                                                         CLUTTER_PARAM_READABLE));
  /**
   * ClutterShader:enabled:
   *
   * FIXME
   *
   * Since: 0.6
   */
  g_object_class_install_property (object_class,
                                   PROP_ENABLED,
                                   g_param_spec_boolean ("enabled",
                                                         "Enabled",
                                                         "Whether the shader is enabled",
                                                         FALSE,
                                                         CLUTTER_PARAM_READWRITE));
}

static void
clutter_shader_init (ClutterShader *self)
{
  ClutterShaderPrivate *priv;

  priv = self->priv = CLUTTER_SHADER_GET_PRIVATE (self);

  priv->bound = FALSE;

  priv->vertex_source = NULL;
  priv->fragment_source = NULL;

  priv->program = 0;
  priv->vertex_shader = 0;
  priv->fragment_shader = 0;
}

/**
 * clutter_shader_new:
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.6
 */
ClutterShader *
clutter_shader_new (void)
{
  return g_object_new (CLUTTER_TYPE_SHADER, NULL);
}


/**
 * clutter_shader_set_fragment_source:
 * @shader: a #ClutterShader
 * @data: FIXME
 * @length: FIXME (currently ignored)
 *
 * FIXME
 *
 *
 * Since: 0.6
 */
void
clutter_shader_set_fragment_source (ClutterShader      *shader,
                                    const gchar        *data,
                                    gssize              length)
{
  ClutterShaderPrivate *priv;
  gboolean is_glsl;

  if (shader == NULL)
    g_error ("quack!");

  g_return_if_fail (CLUTTER_IS_SHADER (shader));
  g_return_if_fail (data != NULL);

  priv = shader->priv;

  /* release shader if bound when changing the source, the shader will
   * automatically be rebound on the next use.
   */
  if (clutter_shader_is_bound (shader))
    clutter_shader_release (shader);

  is_glsl = !g_str_has_prefix (data, "!!ARBfp");

  if (priv->fragment_source)
    {
      g_free (priv->fragment_source);
    }

  CLUTTER_NOTE (SHADER, "setting fragment shader (GLSL:%s, len:%" G_GSSIZE_FORMAT ")",
                is_glsl ? "yes" : "no",
                length);

  priv->fragment_source = g_strdup (data);
  priv->fragment_is_glsl = is_glsl;
}


/**
 * clutter_shader_set_vertex_source:
 * @shader: a #ClutterShader
 * @data: FIXME
 * @length: FIXME (currently ignored)
 *
 * FIXME
 *
 * Since: 0.6
 */
void
clutter_shader_set_vertex_source (ClutterShader      *shader,
                                  const gchar        *data,
                                  gssize              length)
{
  ClutterShaderPrivate *priv;
  gboolean is_glsl;

  if (shader == NULL)
    g_error ("quack!");

  g_return_if_fail (CLUTTER_IS_SHADER (shader));
  g_return_if_fail (data != NULL);

  priv = shader->priv;

  /* release shader if bound when changing the source, the shader will
   * automatically be rebound on the next use.
   */
  if (clutter_shader_is_bound (shader))
    clutter_shader_release (shader);


  is_glsl = !g_str_has_prefix (data, "!!ARBvp");

  if (priv->vertex_source)
    {
      g_free (priv->vertex_source);
    }

  CLUTTER_NOTE (SHADER, "setting vertex shader (GLSL:%s, len:%" G_GSSIZE_FORMAT ")",
                is_glsl ? "yes" : "no",
                length);

  priv->vertex_source = g_strdup (data);
  priv->vertex_is_glsl = is_glsl;
}

static gboolean
bind_glsl_shader (ClutterShader  *self,
                  GError        **error)
{
  ClutterShaderPrivate *priv;
  priv = self->priv;

  cogl_enable (CGL_FRAGMENT_SHADER);
  cogl_enable (CGL_VERTEX_SHADER);

  priv->program = cogl_create_program ();

  if (priv->vertex_is_glsl && priv->vertex_source)
    {
     priv->vertex_shader = cogl_create_shader (CGL_VERTEX_SHADER);

     cogl_shader_source (priv->vertex_shader, priv->vertex_source);
     cogl_shader_compile (priv->vertex_shader);
     cogl_program_attach_shader (priv->program, priv->vertex_shader);
    }

  if (priv->fragment_is_glsl && priv->fragment_source)
    {
      GLint compiled = CGL_FALSE;

      priv->fragment_shader = cogl_create_shader (CGL_FRAGMENT_SHADER);

      cogl_shader_source (priv->fragment_shader, priv->fragment_source);
      cogl_shader_compile (priv->fragment_shader);

      cogl_shader_get_parameteriv (priv->fragment_shader,
                                   CGL_OBJECT_COMPILE_STATUS,
                                   &compiled);
      if (compiled != CGL_TRUE)
        {
          gchar error_buf[512];

          cogl_shader_get_info_log (priv->fragment_shader, 512, error_buf);

          g_set_error (error, CLUTTER_SHADER_ERROR,
                       CLUTTER_SHADER_ERROR_COMPILE,
                       "Shader compilation failed: %s",
                       error_buf);

          return FALSE;
        }
      else
        cogl_program_attach_shader (priv->program, priv->fragment_shader);
    }

  cogl_program_link (priv->program);

  return TRUE;
}

/**
 * clutter_shader_bind:
 * @shader: a #ClutterShader
 * @error: return location for a #GError, or %NULL
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.6
 */
gboolean
clutter_shader_bind (ClutterShader  *shader,
                     GError        **error)
{
  ClutterShaderPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_SHADER (shader), FALSE);

  priv = shader->priv;

  if (priv->bound)
    return priv->bound;

  if ((priv->vertex_source   && !priv->vertex_is_glsl) ||
      (priv->fragment_source && !priv->fragment_is_glsl))
    {
      /* XXX: maybe this error message should be about only GLSL
shaders supportes as of now
       */
      g_set_error (error, CLUTTER_SHADER_ERROR,
                   CLUTTER_SHADER_ERROR_NO_ASM,
                   "ASM shaders not supported");
      priv->bound = FALSE;
      return priv->bound;
    }

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      g_set_error (error, CLUTTER_SHADER_ERROR,
                   CLUTTER_SHADER_ERROR_NO_GLSL,
                   "GLSL shaders not supported");
      priv->bound = FALSE;
      return priv->bound;
    }

  priv->bound = bind_glsl_shader (shader, error);

  if (priv->bound)
    g_object_notify (G_OBJECT (shader), "bound");

  return priv->bound;
}

/**
 * clutter_shader_release:
 * @shader: a #ClutterShader
 *
 * FIXME
 *
 * Since: 0.6
 */
void
clutter_shader_release (ClutterShader *shader)
{
  ClutterShaderPrivate *priv;

  g_return_if_fail (CLUTTER_IS_SHADER (shader));

  priv = shader->priv;

  if (!priv->bound)
    return;

  g_assert (priv->program);

  if (priv->vertex_is_glsl && priv->vertex_shader)
    cogl_shader_destroy (priv->vertex_shader);

  if (priv->fragment_is_glsl && priv->fragment_shader)
    cogl_shader_destroy (priv->fragment_shader);

  if (priv->program)
    cogl_program_destroy (priv->program);

  priv->vertex_shader = 0;
  priv->fragment_shader = 0;
  priv->program = 0;
  priv->bound = FALSE;

  g_object_notify (G_OBJECT (shader), "bound");
}

/**
 * clutter_shader_is_bound:
 * @shader: a #ClutterShader
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.6
 */
gboolean
clutter_shader_is_bound (ClutterShader *shader)
{
  g_return_val_if_fail (CLUTTER_IS_SHADER (shader), FALSE);

  return shader->priv->bound;
}

/**
 * clutter_shader_set_is_enabled:
 * @shader: a #ClutterShader
 * @enabled: FIXME
 *
 * FIXME
 *
 * Since: 0.6
 */
void
clutter_shader_set_is_enabled (ClutterShader *shader,
                               gboolean       enabled)
{
  ClutterShaderPrivate *priv;

  g_return_if_fail (CLUTTER_IS_SHADER (shader));

  priv = shader->priv;

  if (priv->is_enabled != enabled)
    {
      GError *error = NULL;
      gboolean res;

      res = clutter_shader_bind (shader, &error);
      if (!res)
        {
          g_warning ("Unable to bind the shader: %s",
                     error ? error->message : "unknown error");
          if (error)
            g_error_free (error);

          return;
        }

      priv->is_enabled = enabled;

      if (priv->is_enabled)
        cogl_program_use (priv->program);
      else
        cogl_program_use (0);

      g_object_notify (G_OBJECT (shader), "enabled");
    }
}

/**
 * clutter_shader_get_is_enabled:
 * @shader: a #ClutterShader
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.6
 */
gboolean
clutter_shader_get_is_enabled (ClutterShader *shader)
{
  g_return_val_if_fail (CLUTTER_IS_SHADER (shader), FALSE);

  return shader->priv->is_enabled;
}

/**
 * clutter_shader_set_uniform_1f:
 * @shader: a #ClutterShader
 * @name: FIXME
 * @value: FIXME
 *
 * FIXME
 *
 * Since: 0.6
 */
void
clutter_shader_set_uniform_1f (ClutterShader *shader,
                               const gchar   *name,
                               gfloat         value)
{
  ClutterShaderPrivate *priv;
  GLint                 location = 0;
  GLfloat               foo      = value;

  g_return_if_fail (CLUTTER_IS_SHADER (shader));

  priv = shader->priv;

  location = cogl_program_get_uniform_location (priv->program, name);
  cogl_program_uniform_1f (location, foo);
}

/**
 * clutter_shader_release_all:
 *
 * FIXME
 *
 * Since: 0.6
 */
void
clutter_shader_release_all (void)
{
  g_list_foreach (clutter_shaders_list,
                  (GFunc) clutter_shader_release,
                  NULL);
}


/**
 * clutter_shader_get_fragment_source:
 * @shader: a #ClutterShader
 *
 * FIXME
 *
 * Return value: the source of the fragment shader for this ClutterShader object
 * or %NULL. The returned string is owned by the shader object and should never
 * be modified or freed
 *
 * Since: 0.6
 */
G_CONST_RETURN gchar *
clutter_shader_get_fragment_source (ClutterShader *shader)
{
  g_return_val_if_fail (CLUTTER_IS_SHADER (shader), NULL);
  return shader->priv->fragment_source;
}

/**
 * clutter_shader_get_vertex_source:
 * @shader: a #ClutterShader
 *
 * FIXME
 *
 * Return value: the source of the vertex shader for this ClutterShader object
 * or %NULL. The returned string is owned by the shader object and should never
 * be modified or freed
 *
 * Since: 0.6
 */
G_CONST_RETURN gchar *
clutter_shader_get_vertex_source (ClutterShader *shader)
{
  g_return_val_if_fail (CLUTTER_IS_SHADER (shader), NULL);
  return shader->priv->vertex_source;
}

GQuark
clutter_shader_error_quark (void)
{
  return g_quark_from_static_string ("clutter-shader-error");
}
