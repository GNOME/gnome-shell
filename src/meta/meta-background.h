/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * meta-background.h: CoglTexture for paintnig the system background
 *
 * Copyright 2013 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_BACKGROUND_H
#define META_BACKGROUND_H

#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include <meta/gradient.h>
#include <meta/screen.h>

#include <gsettings-desktop-schemas/gdesktop-enums.h>

/**
 * MetaBackground:
 *
 * This class handles loading a background from file, screenshot, or
 * color scheme. The resulting object can be associated with one or
 * more #MetaBackgroundActor objects to handle loading the background.
 */

#define META_TYPE_BACKGROUND            (meta_background_get_type ())
#define META_BACKGROUND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BACKGROUND, MetaBackground))
#define META_BACKGROUND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_BACKGROUND, MetaBackgroundClass))
#define META_IS_BACKGROUND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BACKGROUND))
#define META_IS_BACKGROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_BACKGROUND))
#define META_BACKGROUND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_BACKGROUND, MetaBackgroundClass))

typedef struct _MetaBackground        MetaBackground;
typedef struct _MetaBackgroundClass   MetaBackgroundClass;
typedef struct _MetaBackgroundPrivate MetaBackgroundPrivate;

/**
 * MetaBackgroundEffects:
 * @META_BACKGROUND_EFFECTS_NONE: No effect
 * @META_BACKGROUND_EFFECTS_VIGNETTE: Vignette
 *
 * Which effects to enable on the background
 */

typedef enum
{
  META_BACKGROUND_EFFECTS_NONE       = 0,
  META_BACKGROUND_EFFECTS_VIGNETTE   = 1 << 1,
} MetaBackgroundEffects;

struct _MetaBackgroundClass
{
  /*< private >*/
  GObjectClass parent_class;
};

struct _MetaBackground
{
  /*< private >*/
  GObject parent;

  MetaBackgroundPrivate *priv;
};

GType meta_background_get_type (void);

MetaBackground *meta_background_new (MetaScreen           *screen,
                                     int                   monitor,
				     MetaBackgroundEffects effects);
MetaBackground *meta_background_copy (MetaBackground        *self,
                                      int                    monitor,
				      MetaBackgroundEffects  effects);

void meta_background_load_gradient (MetaBackground            *self,
                                    GDesktopBackgroundShading  shading_direction,
                                    ClutterColor              *color,
                                    ClutterColor              *second_color);
void meta_background_load_color (MetaBackground *self,
                                 ClutterColor   *color);
void meta_background_load_file_async (MetaBackground          *self,
                                      const char              *filename,
                                      GDesktopBackgroundStyle  style,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data);
gboolean meta_background_load_file_finish (MetaBackground       *self,
                                           GAsyncResult         *result,
                                           GError              **error);

const char *meta_background_get_filename (MetaBackground *self);
GDesktopBackgroundStyle meta_background_get_style (MetaBackground *self);
GDesktopBackgroundShading meta_background_get_shading (MetaBackground *self);
const ClutterColor *meta_background_get_color (MetaBackground *self);
const ClutterColor *meta_background_get_second_color (MetaBackground *self);

#endif /* META_BACKGROUND_H */
