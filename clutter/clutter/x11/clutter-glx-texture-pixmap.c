/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Johan Bilien  <johan.bilien@nokia.com>
 *             Matthew Allum <mallum@o-hand.com>
 *             Robert Bragg  <bob@o-hand.com>
 *             Neil Roberts <neil@linux.intel.com>
 *
 * Copyright (C) 2007 OpenedHand
 * Copyright (C) 2010 Intel Corporation.
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
 *
 */

/**
 * SECTION:clutter-glx-texture-pixmap
 * @Title: ClutterGLXTexturePixmap
 * @short_description: A texture which displays the content of an X Pixmap
 * @Deprecated: 1.4: Use #ClutterX11TexturePixmap instead.
 *
 * #ClutterGLXTexturePixmap is a class for displaying the content of an
 * X Pixmap as a ClutterActor. Used together with the X Composite extension,
 * it allows to display the content of X Windows inside Clutter.
 *
 * This class used to be necessary to use the
 * GLX_EXT_texture_from_pixmap extension to get fast texture
 * updates. However since Clutter 1.4 the handling of this extension
 * has moved down to Cogl. ClutterX11TexturePixmap and
 * ClutterGLXTexturePixmap are now equivalent and either one of them
 * may use the extension if it is possible.
 */

#include "config.h"

#include <string.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "x11/clutter-x11-texture-pixmap.h"

#include <cogl/cogl-texture-pixmap-x11.h>

#include "clutter-glx-texture-pixmap.h"

G_DEFINE_TYPE (ClutterGLXTexturePixmap,    \
               clutter_glx_texture_pixmap, \
               CLUTTER_X11_TYPE_TEXTURE_PIXMAP);

static void
clutter_glx_texture_pixmap_init (ClutterGLXTexturePixmap *self)
{
}

static void
clutter_glx_texture_pixmap_class_init (ClutterGLXTexturePixmapClass *klass)
{
}

/**
 * clutter_glx_texture_pixmap_using_extension:
 * @texture: A #ClutterGLXTexturePixmap
 *
 * Checks whether @texture is using the GLX_EXT_texture_from_pixmap
 * extension; this extension can be optionally (though it is strongly
 * encouraged) implemented as a zero-copy between a GLX pixmap and
 * a GL texture.
 *
 * Return value: %TRUE if the texture is using the
 *   GLX_EXT_texture_from_pixmap OpenGL extension or falling back to the
 *   slower software mechanism.
 *
 * Deprecated: 1.6: Use cogl_texture_pixmap_x11_is_using_tfp_extension()
 *   on the texture handle instead.
 *
 * Since: 0.8
 */
gboolean
clutter_glx_texture_pixmap_using_extension (ClutterGLXTexturePixmap *texture)
{
  CoglHandle cogl_texture;

  g_return_val_if_fail (CLUTTER_GLX_IS_TEXTURE_PIXMAP (texture), FALSE);

  cogl_texture = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (texture));

  return (cogl_is_texture_pixmap_x11 (cogl_texture) &&
          cogl_texture_pixmap_x11_is_using_tfp_extension (cogl_texture));
}

/**
 * clutter_glx_texture_pixmap_new_with_pixmap:
 * @pixmap: the X Pixmap to which this texture should be bound
 *
 * Creates a new #ClutterGLXTexturePixmap for @pixmap
 *
 * Return value: A new #ClutterGLXTexturePixmap bound to the given X Pixmap
 *
 * Since: 0.8
 *
 * Deprecated: 1.6: Use clutter_x11_texture_pixmap_new_with_pixmap() instead
 */
ClutterActor *
clutter_glx_texture_pixmap_new_with_pixmap (Pixmap pixmap)
{
  return g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                       "pixmap", pixmap,
                       NULL);
}

/**
 * clutter_glx_texture_pixmap_new_with_window:
 * @window: the X window to which this texture should be bound
 *
 * Creates a new #ClutterGLXTexturePixmap for @window
 *
 * Return value: A new #ClutterGLXTexturePixmap bound to the given X window
 *
 * Since: 0.8
 *
 * Deprecated: 1.6: Use clutter_x11_texture_pixmap_new_with_window() instead
 */
ClutterActor *
clutter_glx_texture_pixmap_new_with_window (Window window)
{
  return g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP,
                       "window", window,
                       NULL);
}

/**
 * clutter_glx_texture_pixmap_new:
 *
 * Creates a new, empty #ClutterGLXTexturePixmap
 *
 * Return value: A new #ClutterGLXTexturePixmap
 *
 * Since: 0.8
 *
 * Deprecated: 1.6: Use clutter_x11_texture_pixmap_new() instead
 */
ClutterActor *
clutter_glx_texture_pixmap_new (void)
{
  return g_object_new (CLUTTER_GLX_TYPE_TEXTURE_PIXMAP, NULL);
}
