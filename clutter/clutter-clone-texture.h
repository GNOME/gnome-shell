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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CLONE_TEXTURE_H__
#define __CLUTTER_CLONE_TEXTURE_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-texture.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CLONE_TEXTURE              (clutter_clone_texture_get_type ())
#define CLUTTER_CLONE_TEXTURE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CLONE_TEXTURE, ClutterCloneTexture))
#define CLUTTER_CLONE_TEXTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CLONE_TEXTURE, ClutterCloneTextureClass))
#define CLUTTER_IS_CLONE_TEXTURE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CLONE_TEXTURE))
#define CLUTTER_IS_CLONE_TEXTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CLONE_TEXTURE))
#define CLUTTER_CLONE_TEXTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CLONE_TEXTURE, ClutterCloneTextureClass))

typedef struct _ClutterCloneTexture        ClutterCloneTexture;
typedef struct _ClutterCloneTexturePrivate ClutterCloneTexturePrivate;
typedef struct _ClutterCloneTextureClass   ClutterCloneTextureClass;

struct _ClutterCloneTexture
{
  /*< private >*/
  ClutterActor                 parent;

  ClutterCloneTexturePrivate    *priv;
};

struct _ClutterCloneTextureClass 
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_clone_1) (void);
  void (*_clutter_clone_2) (void);
  void (*_clutter_clone_3) (void);
  void (*_clutter_clone_4) (void);
}; 

GType           clutter_clone_texture_get_type           (void) G_GNUC_CONST;

ClutterActor *  clutter_clone_texture_new                (ClutterTexture      *texture);
ClutterTexture *clutter_clone_texture_get_parent_texture (ClutterCloneTexture *clone);
void            clutter_clone_texture_set_parent_texture (ClutterCloneTexture *clone,
                                                          ClutterTexture      *texture);

G_END_DECLS

#endif /* __CLUTTER_CLONE_TEXTURE_H__ */
