/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
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

/* 
 * COGL
 * ====
 *
 * 'cogl' is a very simple abstraction layer which wraps GL and GLES.
 * 
 *
 * !!!! DO NOT USE THIS API YET OUTSIDE OF CLUTTER CORE !!!!
 *              THE API WILL FLUCTUATE WILDLY
 *
 * TODO:
 *  - Use ClutterReal for fixed/float params.
 *  - Add Perspective/viewport setup
 *  - Add Features..
 */

#ifndef __COGL_H__
#define __COGL_H__

#include <glib.h>
#include <clutter/clutter.h>

#include "cogl-defines.h"

G_BEGIN_DECLS

#define CGL_ENABLE_BLEND        (1<<1)
#define CGL_ENABLE_TEXTURE_2D   (1<<2)
#define CGL_ENABLE_ALPHA_TEST   (1<<3)
#define CGL_ENABLE_TEXTURE_RECT (1<<4)

typedef void (*CoglFuncPtr) (void);

CoglFuncPtr
cogl_get_proc_address (const gchar* name);

gboolean 
cogl_check_extension (const gchar *name, const gchar *ext);

void
cogl_perspective (ClutterFixed fovy,
		  ClutterFixed aspect,
		  ClutterFixed zNear,
		  ClutterFixed zFar);

void
cogl_setup_viewport (guint        width,
		     guint        height,
		     ClutterFixed fovy,
		     ClutterFixed aspect,
		     ClutterFixed z_near,
		     ClutterFixed z_far);

void
cogl_paint_init (const ClutterColor *color);

void
cogl_push_matrix (void);

void
cogl_pop_matrix (void);

void
cogl_scale (ClutterFixed x, ClutterFixed z);

void
cogl_translatex (ClutterFixed x, ClutterFixed y, ClutterFixed z);

void
cogl_translate (gint x, gint y, gint z);

void
cogl_rotatex (ClutterFixed angle, gint x, gint y, gint z);

void
cogl_rotate (gint angle, gint x, gint y, gint z);

void
cogl_color (const ClutterColor *color);

void
cogl_clip_set (const ClutterGeometry *clip);

void
cogl_clip_unset (void);

void
cogl_enable (gulong flags);

gboolean
cogl_texture_can_size (COGLenum       target,
		       COGLenum pixel_format,
		       COGLenum pixel_type,
		       int    width, 
		       int    height);

void
cogl_texture_quad (gint   x1,
		   gint   x2, 
		   gint   y1, 
		   gint   y2,
		   ClutterFixed tx1,
		   ClutterFixed ty1,
		   ClutterFixed tx2,
		   ClutterFixed ty2);

void
cogl_textures_create (guint num, COGLuint *textures);

void
cogl_textures_destroy (guint num, const COGLuint *textures);

void
cogl_texture_bind (COGLenum target, COGLuint texture);

void
cogl_texture_set_alignment (COGLenum target, 
			    guint    alignment,
			    guint    row_length);

void
cogl_texture_set_filters (COGLenum target, 
			  COGLenum min_filter,
			  COGLenum max_filter);

void
cogl_texture_set_wrap (COGLenum target, 
		       COGLenum wrap_s,
		       COGLenum wrap_t);

void
cogl_texture_image_2d (COGLenum      target,
		       COGLint       internal_format,
		       gint          width, 
		       gint          height, 
		       COGLenum      format,
		       COGLenum      type,
		       const guchar* pixels);

void
cogl_texture_sub_image_2d (COGLenum      target,
			   gint          xoff,
			   gint          yoff,
			   gint          width, 
			   gint          height,
			   COGLenum      format,  
			   COGLenum      type,
			   const guchar* pixels);

void
cogl_rectangle (gint x, gint y, guint width, guint height);

void
cogl_trapezoid (gint y1,
		gint x11,
		gint x21,
		gint y2,
		gint x12,
		gint x22);
void
cogl_alpha_func (COGLenum     func, 
		 ClutterFixed ref);

ClutterFeatureFlags
cogl_get_features ();

void
cogl_get_modelview_matrix (ClutterFixed m[16]);

void
cogl_get_projection_matrix (ClutterFixed m[16]);

void
cogl_get_viewport (ClutterFixed v[4]);

void
cogl_get_bitmasks (gint *red, gint *green, gint *blue, gint *alpha);

void
cogl_fog_set (const ClutterColor *fog_color,
              ClutterFixed        density,
              ClutterFixed        z_near,
              ClutterFixed        z_far);

G_END_DECLS

#endif /* __COGL_H__ */

