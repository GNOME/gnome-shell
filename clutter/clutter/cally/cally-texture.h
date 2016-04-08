/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

#ifndef __CALLY_TEXTURE_H__
#define __CALLY_TEXTURE_H__

#if !defined(__CALLY_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cally/cally.h> can be included directly."
#endif

#include <clutter/clutter.h>
#include <cally/cally-actor.h>

G_BEGIN_DECLS

#define CALLY_TYPE_TEXTURE            (cally_texture_get_type ())
#define CALLY_TEXTURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CALLY_TYPE_TEXTURE, CallyTexture))
#define CALLY_TEXTURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CALLY_TYPE_TEXTURE, CallyTextureClass))
#define CALLY_IS_TEXTURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALLY_TYPE_TEXTURE))
#define CALLY_IS_TEXTURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CALLY_TYPE_TEXTURE))
#define CALLY_TEXTURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CALLY_TYPE_TEXTURE, CallyTextureClass))

typedef struct _CallyTexture        CallyTexture;
typedef struct _CallyTextureClass   CallyTextureClass;
typedef struct _CallyTexturePrivate CallyTexturePrivate;

/**
 * CallyTexture:
 *
 * The <structname>CallyTexture</structname> structure contains only
 * private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _CallyTexture
{
  /*< private >*/
  CallyActor parent;

  CallyTexturePrivate *priv;
};

/**
 * CallyTextureClass:
 *
 * The <structname>CallyTextureClass</structname> structure contains
 * only private data
 *
 * Since: 1.4
 */
struct _CallyTextureClass
{
  /*< private >*/
  CallyActorClass parent_class;

  /* padding for future expansion */
  gpointer _padding_dummy[8];
};

CLUTTER_AVAILABLE_IN_1_4
GType      cally_texture_get_type (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_1_4
AtkObject *cally_texture_new      (ClutterActor *actor);

G_END_DECLS

#endif /* __CALLY_TEXTURE_H__ */
