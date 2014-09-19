/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MetaBackgroundImageCache:
 *
 * Simple cache for background textures loaded from files
 *
 * Copyright 2014 Red Hat, Inc.
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

#ifndef __META_BACKGROUND_IMAGE_H__
#define __META_BACKGROUND_IMAGE_H__

#include <glib-object.h>
#include <cogl/cogl.h>

#define META_TYPE_BACKGROUND_IMAGE            (meta_background_image_get_type ())
#define META_BACKGROUND_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BACKGROUND_IMAGE, MetaBackgroundImage))
#define META_BACKGROUND_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BACKGROUND_IMAGE, MetaBackgroundImageClass))
#define META_IS_BACKGROUND_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BACKGROUND_IMAGE))
#define META_IS_BACKGROUND_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BACKGROUND_IMAGE))
#define META_BACKGROUND_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BACKGROUND_IMAGE, MetaBackgroundImageClass))

/**
 * MetaBackgroundImage:
 *
 * #MetaBackgroundImage is an object that represents a loaded or loading background image.
 */
typedef struct _MetaBackgroundImage      MetaBackgroundImage;
typedef struct _MetaBackgroundImageClass MetaBackgroundImageClass;

GType meta_background_image_get_type (void);

gboolean     meta_background_image_is_loaded   (MetaBackgroundImage *image);
gboolean     meta_background_image_get_success (MetaBackgroundImage *image);
CoglTexture *meta_background_image_get_texture (MetaBackgroundImage *image);

#define META_TYPE_BACKGROUND_IMAGE_CACHE            (meta_background_image_cache_get_type ())
#define META_BACKGROUND_IMAGE_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BACKGROUND_IMAGE_CACHE, MetaBackgroundImageCache))
#define META_BACKGROUND_IMAGE_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BACKGROUND_IMAGE_CACHE, MetaBackgroundImageCacheClass))
#define META_IS_BACKGROUND_IMAGE_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BACKGROUND_IMAGE_CACHE))
#define META_IS_BACKGROUND_IMAGE_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BACKGROUND_IMAGE_CACHE))
#define META_BACKGROUND_IMAGE_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BACKGROUND_IMAGE_CACHE, MetaBackgroundImageCacheClass))

/**
 * MetaBackgroundImageCache:
 *
 * #MetaBackgroundImageCache caches loading of textures for backgrounds; there's actually
 * nothing background specific about it, other than it is tuned to work well for
 * large images as typically are used for backgrounds.
 */
typedef struct _MetaBackgroundImageCache      MetaBackgroundImageCache;
typedef struct _MetaBackgroundImageCacheClass MetaBackgroundImageCacheClass;

MetaBackgroundImageCache *meta_background_image_cache_get_default (void);

GType meta_background_image_cache_get_type (void);

MetaBackgroundImage *meta_background_image_cache_load  (MetaBackgroundImageCache *cache,
                                                        GFile                    *file);
void                 meta_background_image_cache_purge (MetaBackgroundImageCache *cache,
                                                        GFile                    *file);

#endif /* __META_BACKGROUND_IMAGE_H__ */
