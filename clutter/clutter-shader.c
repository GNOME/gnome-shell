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

#include "clutter.h"
#include "clutter-private.h"
#include "clutter-shader.h"

#include <unistd.h>
#include <glib.h>
#include <cogl/cogl.h>
#include <string.h>
#include <stdlib.h>

#define CLUTTER_SHADER_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),  \
   CLUTTER_TYPE_SHADER, ClutterShaderPrivate))

static GList *shader_list;

static void     clutter_shader_class_init   (ClutterShaderClass    *klass);
static void     clutter_shader_init         (ClutterShader         *sp);
static void     clutter_shader_finalize     (GObject               *object);
static GObject *clutter_shader_constructor  (GType                  type,
                                             guint                  n_params,
                                             GObjectConstructParam *params);
static void     clutter_shader_set_property (GObject               *object,
                                             guint                  prop_id,
                                             const GValue          *value,
                                             GParamSpec            *pspec);
static void     clutter_shader_get_property (GObject               *object,
                                             guint                  prop_id,
                                             GValue                *value,
                                             GParamSpec            *pspec);

struct _ClutterShaderPrivate
{
  gboolean  glsl;   /* The shader is a GLSL shader */
  gboolean  bound;  /* The shader is bound to the GL context */

  gchar    *vertex_shader_source;  /* source (or asm) for vertex shader  */
  gchar    *fragment_shader_source;/* source (or asm) for fragment shader*/

  COGLint   program;

  COGLint   vertex_shader;
  COGLint   fragment_shader;
};

enum 
{
  PROP_0,
  PROP_VERTEX_SOURCE,
  PROP_FRAGMENT_SOURCE
};

G_DEFINE_TYPE (ClutterShader, clutter_shader, G_TYPE_OBJECT);

static void
clutter_shader_class_init (ClutterShaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  clutter_shader_parent_class = g_type_class_peek_parent (klass);
  object_class->finalize      = clutter_shader_finalize;
  object_class->set_property  = clutter_shader_set_property;
  object_class->get_property  = clutter_shader_get_property;
  object_class->constructor   = clutter_shader_constructor;
  g_type_class_add_private (klass, sizeof (ClutterShaderPrivate));

  g_object_class_install_property (object_class,
                                   PROP_VERTEX_SOURCE,
                                   g_param_spec_string ("vertex-source",
                                                        "Vertex Source",
                                                        "Source of vertex shader",
                                                        NULL,
                                                        CLUTTER_PARAM_READWRITE|
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_FRAGMENT_SOURCE,
                                   g_param_spec_string ("fragment-source",
                                                        "Fragment Source",
                                                        "Source of fragment shader",
                                                        NULL,
                                                        CLUTTER_PARAM_READWRITE|
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
clutter_shader_init (ClutterShader *self)
{
  ClutterShaderPrivate *priv;

  priv = self->priv = CLUTTER_SHADER_GET_PRIVATE (self);

  priv->glsl = FALSE;
  priv->bound = FALSE;
  priv->vertex_shader_source = NULL;
  priv->fragment_shader_source = NULL;
  priv->program = 0;
  priv->vertex_shader = 0;
  priv->fragment_shader = 0;
}

static gboolean bind_glsl_shader (ClutterShader *self)
{
  ClutterShaderPrivate *priv;
  priv = self->priv;

  cogl_enable (CGL_FRAGMENT_SHADER);
  cogl_enable (CGL_VERTEX_SHADER);

  priv->glsl    = TRUE;
  priv->program = cogl_create_program ();

  if (priv->vertex_shader_source)
    {
     priv->vertex_shader = cogl_create_shader (CGL_VERTEX_SHADER);
     cogl_shader_source (priv->vertex_shader, priv->vertex_shader_source);
     cogl_shader_compile (priv->vertex_shader);
     cogl_program_attach_shader (priv->program, priv->vertex_shader);
    }
  if (priv->fragment_shader_source)
    {
      GLint compiled = CGL_FALSE;
      priv->fragment_shader = cogl_create_shader (CGL_FRAGMENT_SHADER);
      cogl_shader_source (priv->fragment_shader, priv->fragment_shader_source);
      cogl_shader_compile (priv->fragment_shader);

      cogl_shader_get_parameteriv (priv->fragment_shader,
                                   CGL_OBJECT_COMPILE_STATUS,
                                   &compiled);
      if (compiled != CGL_TRUE)
        {
          GLcharARB *buffer;
          gint       max_length = 512;
          buffer = g_malloc (max_length);
          cogl_shader_get_info_log (priv->fragment_shader, max_length, buffer);
          g_print ("Shader compilation failed:\n%s", buffer);
          g_free (buffer);
          g_object_unref (self);
          return FALSE;
        }
      cogl_program_attach_shader (priv->program, priv->fragment_shader);
    }
  cogl_program_link (priv->program);
  return TRUE;
}

gboolean
clutter_shader_bind (ClutterShader *self)
{
  ClutterShaderPrivate *priv;
 
  priv = self->priv;
  if (priv->bound)
    return priv->bound;

  if (priv->glsl)
    {
      priv->bound = bind_glsl_shader (self);
    }

  return priv->bound;
}

void
clutter_shader_release (ClutterShader *self)
{
  ClutterShaderPrivate *priv;

  priv = self->priv;
  if (!priv->bound)
    return;

  g_assert (priv->program);

  if (priv->glsl)
    {
     if (priv->vertex_shader)
       cogl_shader_destroy (priv->vertex_shader);
     if (priv->fragment_shader)
       cogl_shader_destroy (priv->fragment_shader);
     if (priv->program)
       cogl_program_destroy (priv->program);
     priv->vertex_shader = 0;
     priv->fragment_shader = 0;
     priv->program = 0;
    }
  priv->bound = FALSE;
}

static void
clutter_shader_finalize (GObject *object)
{
  ClutterShader        *shader;
  ClutterShaderPrivate *priv;

  shader = CLUTTER_SHADER (object);
  priv   = shader->priv;

  clutter_shader_release (shader);

  shader_list = g_list_remove (shader_list, object);

  if (priv->fragment_shader_source)
    g_free (priv->fragment_shader_source);
  if (priv->vertex_shader_source)
    g_free (priv->vertex_shader_source);

  G_OBJECT_CLASS (clutter_shader_parent_class)->finalize (object);
}


void
clutter_shader_enable (ClutterShader *self)
{
  ClutterShaderPrivate *priv = self->priv;

  clutter_shader_bind (self);

  cogl_program_use (priv->program);
}

void
clutter_shader_disable (ClutterShader *self)
{
  cogl_program_use (0);
}

void
clutter_shader_set_uniform_1f (ClutterShader *self,
                               const gchar   *name,
                               gfloat         value)
{
  ClutterShaderPrivate *priv     = self->priv;
  GLint                 location = 0;
  GLfloat               foo      = value;

  location =cogl_program_get_uniform_location (priv->program, name);
  cogl_program_uniform_1f (location, foo);
}

void
clutter_shader_release_all (void)
{
  GList *iter;
  for (iter = shader_list; iter; iter = g_list_next (iter))
    {
      clutter_shader_release (iter->data);
    }
}

static void
clutter_shader_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ClutterShader        *shader;
  ClutterShaderPrivate *priv;

  shader = CLUTTER_SHADER(object);
  priv = shader->priv;

  switch (prop_id)
    {
      case PROP_VERTEX_SOURCE:
        if (priv->vertex_shader_source)
          {
            g_free (priv->vertex_shader_source);
            priv->vertex_shader_source = NULL;
          }
        priv->vertex_shader_source = g_value_dup_string (value);
        break;
      case PROP_FRAGMENT_SOURCE:
        if (priv->fragment_shader_source)
          {
            g_free (priv->fragment_shader_source);
            priv->fragment_shader_source = NULL;
          }
        priv->fragment_shader_source = g_value_dup_string (value);
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
        g_value_set_string (value, priv->vertex_shader_source);
        break;
      case PROP_FRAGMENT_SOURCE:
        g_value_set_string (value, priv->fragment_shader_source);
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
  GObject       *object;
  ClutterShader *shader;
  ClutterShaderPrivate *priv;

  object    = G_OBJECT_CLASS (clutter_shader_parent_class)->constructor (
                                                      type, n_params, params);
  shader = CLUTTER_SHADER (object);
  priv = shader->priv;

  priv->glsl = !((priv->vertex_shader_source &&
               g_str_has_prefix (priv->vertex_shader_source, "!!ARBvp")) ||
               (priv->fragment_shader_source &&
               g_str_has_prefix (priv->fragment_shader_source, "!!ARBfp")));
  if (!priv->glsl)
    {
      g_warning ("ASM shader support not available");
      g_object_unref (object);
      return NULL;
    }
  if (priv->glsl && !clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      g_warning ("GLSL shaders not supported\n");
      g_object_unref (object);
      return NULL;
    }

  shader_list = g_list_prepend (shader_list, object);
  return object;
}

ClutterShader *
clutter_shader_new_from_strings (const gchar *vertex_shader_program,
                                 const gchar *fragment_shader_program)
{
  /* evil hack, since g_object_new would interpret a NULL passed as
   * a argument termination
   */
  if (vertex_shader_program &&
      fragment_shader_program)
    return g_object_new (CLUTTER_TYPE_SHADER,
                         "vertex-source", vertex_shader_program,
                         "fragment-source", fragment_shader_program,
                         NULL);
  else if (fragment_shader_program)
    return g_object_new (CLUTTER_TYPE_SHADER,
                         "fragment-source", fragment_shader_program,
                         NULL);
  else if (vertex_shader_program)
    return g_object_new (CLUTTER_TYPE_SHADER,
                         "vertex-source", vertex_shader_program,
                         NULL);
  else {
    g_warning ("neither fragment nor vertex shader provided");
    return NULL;
  }
}

ClutterShader *
clutter_shader_new_from_files (const gchar *vertex_file,
                               const gchar *fragment_file)
{
  ClutterShader *shader;
  gchar         *vertex_shader_program   = NULL;
  gchar         *fragment_shader_program = NULL;

  g_assert (vertex_file != NULL ||
            fragment_file != NULL);

  if (vertex_file)
    {
      g_file_get_contents (vertex_file, &vertex_shader_program,
                           NULL, NULL);
    }
  if (fragment_file)
    {
      g_file_get_contents (fragment_file, &fragment_shader_program,
                           NULL, NULL);
    }
  shader = clutter_shader_new_from_strings (vertex_shader_program,
                                            fragment_shader_program);

  if (vertex_shader_program)
    {
      g_free (vertex_shader_program);
    }
  if (fragment_shader_program)
    {
      g_free (fragment_shader_program);
    }
  return shader;
}
