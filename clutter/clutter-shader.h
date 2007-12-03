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


#ifndef CLUTTER_SHADER_H
#define CLUTTER_SHADER_H

#include <GL/gl.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SHADER         (clutter_shader_get_type ())
#define CLUTTER_SHADER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CLUTTER_TYPE_SHADER, ClutterShader))
#define CLUTTER_SHADER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CLUTTER_TYPE_SHADER, ClutterShaderClass))
#define CLUTTER_IS_SHADER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLUTTER_TYPE_SHADER))
#define CLUTTER_IS_SHADER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CLUTTER_TYPE_SHADER))
#define CLUTTER_SHADER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLUTTER_TYPE_SHADER, ClutterShaderClass))

typedef struct _ClutterShader        ClutterShader;
typedef struct _ClutterShaderPrivate ClutterShaderPrivate;
typedef struct _ClutterShaderClass   ClutterShaderClass;

struct _ClutterShader
{
  GObject               parent;
  ClutterShaderPrivate *priv;
};

struct _ClutterShaderClass
{
  GObjectClass parent_class;
};

GType           clutter_shader_get_type         ();

ClutterShader * clutter_shader_new_from_files   (const gchar *vertex_file,
                                                 const gchar *fragment_file);
ClutterShader * clutter_shader_new_from_strings (const gchar *vertex_file,
                                                 const gchar *fragment_file);
void            clutter_shader_enable           (ClutterShader *self);
void            clutter_shader_disable          (ClutterShader *self);

gboolean        clutter_shader_bind             (ClutterShader *self);
void            clutter_shader_release          (ClutterShader *self);
void            clutter_shader_set_uniform_1f   (ClutterShader *self, 
                                                 const gchar   *name,
                                                 gfloat         value);
/* should be private and internal */
void            clutter_shader_release_all    (void);
gboolean        clutter_shader_has_glsl         (void);


G_END_DECLS

#endif /* CLUTTER_SHADER_H */
