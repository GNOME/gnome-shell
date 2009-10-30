/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MutterTextureTower
 *
 * Mipmap emulation by creation of scaled down images
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <math.h>
#include <string.h>

#include "mutter-texture-tower.h"

#ifndef M_LOG2E
#define M_LOG2E 1.4426950408889634074
#endif

#if !CLUTTER_CHECK_VERSION(1,1,3)
static PFNGLGENFRAMEBUFFERSPROC genFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC bindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture2D;
#endif


#define MAX_TEXTURE_LEVELS 12

/* If the texture format in memory doesn't match this, then Mesa
 * will do the conversion, so things will still work, but it might
 * be slow depending on how efficient Mesa is. These should be the
 * native formats unless the display is 16bpp. If conversions
 * here are a bottleneck, investigate whether we are converting when
 * storing window data *into* the texture before adding extra code
 * to handle multiple texture formats.
 */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define TEXTURE_FORMAT COGL_PIXEL_FORMAT_BGRA_8888_PRE
#else
#define TEXTURE_FORMAT COGL_PIXEL_FORMAT_ARGB_8888_PRE
#endif

typedef struct
{
  guint16 x1;
  guint16 y1;
  guint16 x2;
  guint16 y2;
} Box;

struct _MutterTextureTower
{
  int n_levels;
  CoglHandle textures[MAX_TEXTURE_LEVELS];
#if CLUTTER_CHECK_VERSION(1,1,3)
  CoglHandle fbos[MAX_TEXTURE_LEVELS];
#else
  GLuint fbos[MAX_TEXTURE_LEVELS];
#endif
  Box invalid[MAX_TEXTURE_LEVELS];
};

/**
 * mutter_texture_tower_new:
 *
 * Creates a new texture tower. The base texture has to be set with
 * mutter_texture_tower_set_base_texture() before use.
 *
 * Return value: the new texture tower. Free with mutter_texture_tower_free()
 */
MutterTextureTower *
mutter_texture_tower_new (void)
{
  MutterTextureTower *tower;

  tower = g_slice_new0 (MutterTextureTower);

  return tower;
}

/**
 * mutter_texture_tower_free:
 * @tower: a #MutterTextureTower
 *
 * Frees a texture tower created with mutter_texture_tower_new().
 */
void
mutter_texture_tower_free (MutterTextureTower *tower)
{
  g_return_if_fail (tower != NULL);

  mutter_texture_tower_set_base_texture (tower, COGL_INVALID_HANDLE);

  g_slice_free (MutterTextureTower, tower);
}

static gboolean
texture_is_rectangle (CoglHandle texture)
{
  GLuint gl_tex;
  GLenum gl_target;

  cogl_texture_get_gl_texture (texture, &gl_tex, &gl_target);
  return gl_target == CGL_TEXTURE_RECTANGLE_ARB;
}

static void
free_texture (CoglHandle texture)
{
  GLuint gl_tex;
  GLenum gl_target;

  cogl_texture_get_gl_texture (texture, &gl_tex, &gl_target);

  if (gl_target == CGL_TEXTURE_RECTANGLE_ARB)
    glDeleteTextures (1, &gl_tex);

  cogl_texture_unref (texture);
}

/**
 * mutter_texture_tower_update_area:
 * @tower: a MutterTextureTower
 * @texture: the new texture used as a base for scaled down versions
 *
 * Sets the base texture that is the scaled texture that the
 * scaled textures of the tower are derived from. The texture itself
 * will be used as level 0 of the tower and will be referenced until
 * unset or until the tower is freed.
 */
void
mutter_texture_tower_set_base_texture (MutterTextureTower *tower,
                                       CoglHandle          texture)
{
  int i;

  g_return_if_fail (tower != NULL);

  if (texture == tower->textures[0])
    return;

  if (tower->textures[0] != COGL_INVALID_HANDLE)
    {
      for (i = 1; i < tower->n_levels; i++)
        {
          if (tower->textures[i] != COGL_INVALID_HANDLE)
            {
              free_texture (tower->textures[i]);
              tower->textures[i] = COGL_INVALID_HANDLE;
            }

#if CLUTTER_CHECK_VERSION(1,1,3)
          if (tower->fbos[i] != COGL_INVALID_HANDLE)
            {
              cogl_offscreen_unref (tower->fbos[i]);
              tower->fbos[i] = COGL_INVALID_HANDLE;
            }
#else
          if (tower->fbos[i] != 0)
            {
              (*deleteFramebuffers) (1, &tower->fbos[i]);
              tower->fbos[i] = 0;
            }
#endif
        }

      cogl_texture_unref (tower->textures[0]);
    }

  tower->textures[0] = texture;

  if (tower->textures[0] != COGL_INVALID_HANDLE)
    {
      int width, height;

      cogl_texture_ref (tower->textures[0]);

      width = cogl_texture_get_width (tower->textures[0]);
      height = cogl_texture_get_height (tower->textures[0]);

      tower->n_levels = 1 + MAX ((int)(M_LOG2E * log (width)), (int)(M_LOG2E * log (height)));
      tower->n_levels = MIN(tower->n_levels, MAX_TEXTURE_LEVELS);

      mutter_texture_tower_update_area (tower, 0, 0, width, height);
    }
  else
    {
      tower->n_levels = 0;
    }
}

/**
 * mutter_texture_tower_update_area:
 * @tower: a MutterTextureTower
 * @x: X coordinate of upper left of rectangle that changed
 * @y: Y coordinate of upper left of rectangle that changed
 * @width: width of rectangle that changed
 * @height: height rectangle that changed
 *
 * Mark a region of the base texture as having changed; the next
 * time a scaled down version of the base texture is retrieved,
 * the appropriate area of the scaled down texture will be updated.
 */
void
mutter_texture_tower_update_area (MutterTextureTower *tower,
                                  int                 x,
                                  int                 y,
                                  int                 width,
                                  int                 height)
{
  int texture_width, texture_height;
  Box invalid;
  int i;

  g_return_if_fail (tower != NULL);

  texture_width = cogl_texture_get_width (tower->textures[0]);
  texture_height = cogl_texture_get_height (tower->textures[0]);

  invalid.x1 = x;
  invalid.y1 = y;
  invalid.x2 = x + width;
  invalid.y2 = y + height;

  for (i = 1; i < tower->n_levels; i++)
    {
      texture_width = MAX (1, texture_width / 2);
      texture_height = MAX (1, texture_height / 2);

      invalid.x1 = invalid.x1 / 2;
      invalid.y1 = invalid.y1 / 2;
      invalid.x2 = MIN (texture_width, (invalid.x2 + 1) / 2);
      invalid.y2 = MIN (texture_height, (invalid.y2 + 1) / 2);

      if (tower->invalid[i].x1 == tower->invalid[i].x2 ||
          tower->invalid[i].y1 == tower->invalid[i].y2)
        {
          tower->invalid[i] = invalid;
        }
      else
        {
          tower->invalid[i].x1 = MIN (tower->invalid[i].x1, invalid.x1);
          tower->invalid[i].y1 = MIN (tower->invalid[i].y1, invalid.y1);
          tower->invalid[i].x2 = MAX (tower->invalid[i].x2, invalid.x2);
          tower->invalid[i].y2 = MAX (tower->invalid[i].y2, invalid.y2);
        }
    }
}

/* It generally looks worse if we scale up a window texture by even a
 * small amount than if we scale it down using bilinear filtering, so
 * we always pick the *larger* adjacent level. */
#define LOD_BIAS (-0.49)

/* This determines the appropriate level of detail to use when drawing the
 * texture, in a way that corresponds to what the GL specification does
 * when mip-mapping. This is probably fancier and slower than what we need,
 * but we do the computation only once each time we paint a window, and
 * its easier to just use the equations from the specification than to
 * come up with something simpler.
 *
 * If window is being painted at an angle from the viewer, then we have to
 * pick a point in the texture; we use the middle of the texture (which is
 * why the width/height are passed in.) This is not the normal case for
 * Mutter.
 */
static int
get_paint_level (int width, int height)
{
  CoglMatrix projection, modelview, pm;
  float v[4];
  double viewport_width, viewport_height;
  double u0, v0;
  double xc, yc, wc;
  double dxdu_, dxdv_, dydu_, dydv_;
  double det_, det_sq;
  double rho_sq;
  double lambda;

  /* See
   * http://www.opengl.org/registry/doc/glspec32.core.20090803.pdf
   * Section 3.8.9, p. 1.6.2. Here we have
   *
   *  u(x,y) = x_o;
   *  v(x,y) = y_o;
   *
   * Since we are mapping 1:1 from object coordinates into pixel
   * texture coordinates, the clip coordinates are:
   *
   *  (x_c)                               (x_o)        (u)
   *  (y_c) = (M_projection)(M_modelview) (y_o) = (PM) (v)
   *  (z_c)                               (z_o)        (0)
   *  (w_c)                               (w_o)        (1)
   */

  cogl_get_projection_matrix (&projection);
  cogl_get_modelview_matrix (&modelview);

  cogl_matrix_multiply (&pm, &projection, &modelview);

  cogl_get_viewport (v);
  viewport_width = v[2];
  viewport_height = v[3];

  u0 = width / 2.;
  v0 = height / 2.;

  xc = pm.xx * u0 + pm.xy * v0 + pm.xw;
  yc = pm.yx * u0 + pm.yy * v0 + pm.yw;
  wc = pm.wx * u0 + pm.wy * v0 + pm.ww;

  /* We'll simplify the equations below for a bit of micro-optimization.
   * The commented out code is the unsimplified version.

  // Partial derivates of window coordinates:
  //
  //  x_w = 0.5 * viewport_width * x_c / w_c + viewport_center_x
  //  y_w = 0.5 * viewport_height * y_c / w_c + viewport_center_y
  //
  // with respect to u, v, using
  // d(a/b)/dx = da/dx * (1/b) - a * db/dx / (b^2)

  dxdu = 0.5 * viewport_width * (pm.xx - pm.wx * (xc/wc)) / wc;
  dxdv = 0.5 * viewport_width * (pm.xy - pm.wy * (xc/wc)) / wc;
  dydu = 0.5 * viewport_height * (pm.yx - pm.wx * (yc/wc)) / wc;
  dydv = 0.5 * viewport_height * (pm.yy - pm.wy * (yc/wc)) / wc;

  // Compute the inverse partials as the matrix inverse
  det = dxdu * dydv - dxdv * dydu;

  dudx =   dydv / det;
  dudy = - dxdv / det;
  dvdx = - dydu / det;
  dvdy =   dvdu / det;

  // Scale factor; maximum of the distance in texels for a change of 1 pixel
  // in the X direction or 1 pixel in the Y direction
  rho = MAX (sqrt (dudx * dudx + dvdx * dvdx), sqrt(dudy * dudy + dvdy * dvdy));

  // Level of detail
  lambda = log2 (rho) + LOD_BIAS;
  */

  /* dxdu * wc, etc */
  dxdu_ = 0.5 * viewport_width * (pm.xx - pm.wx * (xc/wc));
  dxdv_ = 0.5 * viewport_width * (pm.xy - pm.wy * (xc/wc));
  dydu_ = 0.5 * viewport_height * (pm.yx - pm.wx * (yc/wc));
  dydv_ = 0.5 * viewport_height * (pm.yy - pm.wy * (yc/wc));

  /* det * wc^2 */
  det_ = dxdu_ * dydv_ - dxdv_ * dydu_;
  det_sq = det_ * det_;
  if (det_sq == 0.0)
    return -1;

  /* (rho * det * wc)^2 */
  rho_sq = MAX (dydv_ * dydv_ + dydu_ * dydu_, dxdv_ * dxdv_ + dxdu_ * dxdu_);
  lambda = 0.5 * M_LOG2E * log (rho_sq * wc * wc / det_sq) + LOD_BIAS;

#if 0
  g_print ("%g %g %g\n", 0.5 * viewport_width * pm.xx / pm.ww, 0.5 * viewport_height * pm.yy / pm.ww, lambda);
#endif

  if (lambda <= 0.)
    return 0;
  else
    return (int)(0.5 + lambda);
}

static gboolean
is_power_of_two (int x)
{
  return (x & (x - 1)) == 0;
}

static void
texture_tower_create_texture (MutterTextureTower *tower,
                              int                 level,
                              int                 width,
                              int                 height)
{
  if ((!is_power_of_two (width) || !is_power_of_two (height)) &&
      texture_is_rectangle (tower->textures[level - 1]))
    {
      GLuint tex = 0;

      glGenTextures (1, &tex);
      glBindTexture (CGL_TEXTURE_RECTANGLE_ARB, tex);
      glTexImage2D (CGL_TEXTURE_RECTANGLE_ARB, 0,
                    GL_RGBA, width,height,
#if TEXTURE_FORMAT == COGL_PIXEL_FORMAT_BGRA_8888_PRE
                    0, GL_BGRA, GL_UNSIGNED_BYTE,
#else /* assume big endian */
                    0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
#endif
                    NULL);

      tower->textures[level] = cogl_texture_new_from_foreign (tex, CGL_TEXTURE_RECTANGLE_ARB,
                                                              width, height,
                                                              0, 0,
                                                              TEXTURE_FORMAT);
    }
  else
    {
      tower->textures[level] = cogl_texture_new_with_size (width, height,
                                                           COGL_TEXTURE_NO_AUTO_MIPMAP,
                                                           TEXTURE_FORMAT);
    }

  tower->invalid[level].x1 = 0;
  tower->invalid[level].y1 = 0;
  tower->invalid[level].x2 = width;
  tower->invalid[level].y2 = height;
}

/* The COGL fbo (render-to-texture) support is pretty hard to use in
 * Clutter 1.0; there's no way to save and restore the old projection
 * matrix and viewport without ugly workarounds that require explicit
 * access to the ClutterStage.  In Clutter 1.2, the save/restore is
 * automatic. For now, until we depend on Clutter 1.2, we use GL
 * directly for render-to-texture. The main downside (other than
 * a lot of verbosity) is that we have to save the state, reset anything
 * that we think COGL might have left in a way we don't want it, then
 * restore the old state.
 */
#if CLUTTER_CHECK_VERSION(1,1,3)
static gboolean
texture_tower_revalidate_fbo (MutterTextureTower *tower,
                              int                 level)
{
  CoglHandle source_texture = tower->textures[level - 1];
  int source_texture_width = cogl_texture_get_width (source_texture);
  int source_texture_height = cogl_texture_get_height (source_texture);
  CoglHandle dest_texture = tower->textures[level];
  int dest_texture_width = cogl_texture_get_width (dest_texture);
  int dest_texture_height = cogl_texture_get_height (dest_texture);
  Box *invalid = &tower->invalid[level];
  CoglMatrix modelview;

  if (tower->fbos[level] == COGL_INVALID_HANDLE)
    tower->fbos[level] = cogl_offscreen_new_to_texture (dest_texture);

  if (tower->fbos[level] == COGL_INVALID_HANDLE)
    return FALSE;

  cogl_push_draw_buffer ();
  cogl_set_draw_buffer (COGL_OFFSCREEN_BUFFER, tower->fbos[level]);

  cogl_ortho (0, dest_texture_width, dest_texture_height, 0, -1., 1.);

  cogl_matrix_init_identity (&modelview);
  cogl_set_modelview_matrix (&modelview);

  cogl_set_source_texture (tower->textures[level - 1]);
  cogl_rectangle_with_texture_coords (invalid->x1, invalid->y1,
                                      invalid->x2, invalid->y2,
                                      (2. * invalid->x1) / source_texture_width,
                                      (2. * invalid->y1) / source_texture_height,
                                      (2. * invalid->x2) / source_texture_width,
                                      (2. * invalid->y2) / source_texture_height);

  cogl_pop_draw_buffer ();

  return TRUE;
}
#else
static void
initialize_gl_functions (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;

      genFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) cogl_get_proc_address ("glGenFramebuffersEXT");
      deleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) cogl_get_proc_address ("glDeleteFramebuffersEXT");
      bindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) cogl_get_proc_address ("glBindFramebufferEXT");
      framebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) cogl_get_proc_address ("glFramebufferTexture2D");
    }
}

static gboolean
texture_tower_revalidate_fbo (MutterTextureTower *tower,
                              int                 level)
{
  CoglHandle source_texture = tower->textures[level - 1];
  int source_texture_width = cogl_texture_get_width (source_texture);
  int source_texture_height = cogl_texture_get_height (source_texture);
  CoglHandle dest_texture = tower->textures[level];
  int dest_texture_width = cogl_texture_get_width (dest_texture);
  int dest_texture_height = cogl_texture_get_height (dest_texture);
  ClutterActorBox source_box;
  Box *dest_box;

  GLuint source_gl_tex;
  GLenum source_gl_target;

  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return FALSE;

  initialize_gl_functions ();

  /* Create the frame-buffer object that renders to the texture, if
   * it doesn't exist; just bind it for rendering if it does */
  if (tower->fbos[level] == 0)
    {
      GLuint dest_gl_tex;
      GLenum dest_gl_target;

      cogl_texture_get_gl_texture (dest_texture, &dest_gl_tex, &dest_gl_target);

      (*genFramebuffers) (1, &tower->fbos[level]);
      (*bindFramebuffer) (GL_FRAMEBUFFER_EXT, tower->fbos[level]);
      (*framebufferTexture2D) (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                               dest_gl_target, dest_gl_tex, 0);
    }
  else
    {
      (*bindFramebuffer) (GL_FRAMEBUFFER_EXT, tower->fbos[level]);
    }

  /* Save the old state (other than the transformation matrices) */
  glPushAttrib (GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT);

  /* And set up the state we need */
  glDisable (GL_BLEND);
  glDisable (GL_SCISSOR_TEST);
  glDisable (GL_STENCIL_TEST);

  glDisable (GL_CLIP_PLANE3);
  glDisable (GL_CLIP_PLANE2);
  glDisable (GL_CLIP_PLANE1);
  glDisable (GL_CLIP_PLANE0);

  cogl_texture_get_gl_texture (source_texture, &source_gl_tex, &source_gl_target);

  glActiveTextureARB (GL_TEXTURE0_ARB);
  if (source_gl_target == GL_TEXTURE_2D)
    glDisable (GL_TEXTURE_RECTANGLE);
  else
    glDisable (GL_TEXTURE_2D);
  glEnable (source_gl_target);
  glBindTexture (source_gl_target, source_gl_tex);
  glTexParameteri (source_gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (source_gl_target, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (source_gl_target, GL_TEXTURE_WRAP_T, GL_CLAMP);

  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  /* In theory, we should loop over all the texture units supported
   * by the GL implementation, but here we just assume that no more
   * than three are used by Mutter and all GL implementations we care
   * about will support at least 3.
   */
  glActiveTextureARB (GL_TEXTURE1_ARB);
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_TEXTURE_RECTANGLE);
  glActiveTextureARB (GL_TEXTURE2_ARB);
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_TEXTURE_RECTANGLE);

  glViewport (0, 0, dest_texture_width, dest_texture_height);

  /* Save the transformation matrices and set up new ones that map
   * coordinates directly onto the destination texture */
  glMatrixMode (GL_MODELVIEW);
  glPushMatrix ();
  glLoadIdentity ();

  glMatrixMode (GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity ();
  glOrtho (0, dest_texture_width, 0, dest_texture_height, -1., 1.);

  /* Draw */

  dest_box = &tower->invalid[level];
  if (texture_is_rectangle (source_texture))
    {
      source_box.x1 = 2 * dest_box->x1;
      source_box.y1 = 2 * dest_box->y1;
      source_box.x2 = 2 * dest_box->x2;
      source_box.y2 = 2 * dest_box->y2;
    }
  else
    {
      source_box.x1 = (2. * dest_box->x1) / source_texture_width;
      source_box.y1 = (2. * dest_box->y1) / source_texture_height;
      source_box.x2 = (2. * dest_box->x2) / source_texture_width;
      source_box.y2 = (2. * dest_box->y2) / source_texture_height;
    }

  glColor3f (0., 1., 1.);

  glBegin (GL_QUADS);
  glTexCoord2f (source_box.x1, source_box.y1);
  glVertex2f (dest_box->x1, dest_box->y1);
  glTexCoord2f (source_box.x2, source_box.y1);
  glVertex2f (dest_box->x2, dest_box->y1);
  glTexCoord2f (source_box.x2, source_box.y2);
  glVertex2f (dest_box->x2, dest_box->y2);
  glTexCoord2f (source_box.x1, source_box.y2);
  glVertex2f (dest_box->x1, dest_box->y2);
  glEnd ();

  /* And restore everything back the way we found it */

  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();
  glMatrixMode (GL_MODELVIEW);
  glPopMatrix ();

  glPopAttrib ();

  (*bindFramebuffer) (GL_FRAMEBUFFER_EXT, 0);

  return TRUE;
}
#endif

static void
fill_copy (guchar       *buf,
           const guchar *source,
           int           width)
{
  memcpy (buf, source, width * 4);
}

static void
fill_scale_down (guchar       *buf,
                 const guchar *source,
                 int           width)
{
  while (width > 1)
    {
      buf[0] = (source[0] + source[4]) / 2;
      buf[1] = (source[1] + source[5]) / 2;
      buf[2] = (source[2] + source[6]) / 2;
      buf[3] = (source[3] + source[7]) / 2;

      buf += 4;
      source += 8;
      width -= 2;
    }

  if (width > 0)
    {
      buf[0] = source[0] / 2;
      buf[1] = source[1] / 2;
      buf[2] = source[2] / 2;
      buf[3] = source[3] / 2;
    }
}

static void
texture_tower_revalidate_client (MutterTextureTower *tower,
                                 int                 level)
{
  CoglHandle source_texture = tower->textures[level - 1];
  int source_texture_width = cogl_texture_get_width (source_texture);
  int source_texture_height = cogl_texture_get_height (source_texture);
  guint source_rowstride;
  guchar *source_data;
  CoglHandle dest_texture = tower->textures[level];
  int dest_texture_width = cogl_texture_get_width (dest_texture);
  int dest_texture_height = cogl_texture_get_height (dest_texture);
  int dest_x = tower->invalid[level].x1;
  int dest_y = tower->invalid[level].y1;
  int dest_width = tower->invalid[level].x2 - tower->invalid[level].x1;
  int dest_height = tower->invalid[level].y2 - tower->invalid[level].y1;
  guchar *dest_data;
  guchar *source_tmp1 = NULL, *source_tmp2 = NULL;
  int i, j;

  source_rowstride = source_texture_width * 4;

  source_data = g_malloc (source_texture_height * source_rowstride);
  cogl_texture_get_data (source_texture, TEXTURE_FORMAT, source_rowstride,
                         source_data);

  dest_data = g_malloc (dest_height * dest_width * 4);

  if (dest_texture_height < source_texture_height)
    {
      source_tmp1 = g_malloc (dest_width * 4);
      source_tmp2 = g_malloc (dest_width * 4);
    }

  for (i = 0; i < dest_height; i++)
    {
      guchar *dest_row = dest_data + i * dest_width * 4;
      if (dest_texture_height < source_texture_height)
        {
          guchar *source1, *source2;
          guchar *dest;

          if (dest_texture_width < source_texture_width)
            {
              fill_scale_down (source_tmp1,
                               source_data + ((i + dest_y) * 2) * source_rowstride + dest_x * 2 * 4,
                               dest_width * 2);
              fill_scale_down (source_tmp2,
                               source_data + ((i + dest_y) * 2 + 1) * source_rowstride + dest_x * 2 * 4,
                               dest_width * 2);
            }
          else
            {
              fill_copy (source_tmp1,
                         source_data + ((i + dest_y) * 2) * source_rowstride + dest_x * 4,
                         dest_width);
              fill_copy (source_tmp2,
                         source_data + ((i + dest_y) * 2 + 1) * source_rowstride + dest_x * 4,
                         dest_width);
            }

          source1 = source_tmp1;
          source2 = source_tmp2;

          dest = dest_row;
          for (j = 0; j < dest_width * 4; j++)
            *(dest++) = (*(source1++) + *(source2++)) / 2;
        }
      else
        {
          if (dest_texture_width < source_texture_width)
            fill_scale_down (dest_row,
                             source_data + (i + dest_y) * source_rowstride + dest_x * 2 * 4,
                             dest_width * 2);
          else
            fill_copy (dest_row,
                       source_data + (i + dest_y) * source_rowstride,
                       dest_width);
        }
    }

  cogl_texture_set_region (dest_texture,
                           0, 0,
                           dest_x, dest_y,
                           dest_width, dest_height,
                           dest_width, dest_height,
                           TEXTURE_FORMAT,
                           4 * dest_width,
                           dest_data);

  if (dest_height < source_texture_height)
    {
      g_free (source_tmp1);
      g_free (source_tmp2);
    }

  g_free (source_data);
  g_free (dest_data);
}

static void
texture_tower_revalidate (MutterTextureTower *tower,
                          int                 level)
{
  if (!texture_tower_revalidate_fbo (tower, level))
    texture_tower_revalidate_client (tower, level);
}

/**
 * mutter_texture_tower_get_paint_texture:
 * @tower: a MutterTextureTower
 *
 * Gets the texture from the tower that best matches the current
 * rendering scale. (On the assumption here the texture is going to
 * be rendered with vertex coordinates that correspond to its
 * size in pixels, so a 200x200 texture will be rendered on the
 * rectangle (0, 0, 200, 200).
 *
 * Return value: the COGL texture handle to use for painting, or
 *  %COGL_INVALID_HANDLE if no base texture has yet been set.
 */
CoglHandle
mutter_texture_tower_get_paint_texture (MutterTextureTower *tower)
{
  int texture_width, texture_height;
  int level;

  g_return_val_if_fail (tower != NULL, COGL_INVALID_HANDLE);

  if (tower->textures[0] == COGL_INVALID_HANDLE)
    return COGL_INVALID_HANDLE;

  texture_width = cogl_texture_get_width (tower->textures[0]);
  texture_height = cogl_texture_get_height (tower->textures[0]);

  level = get_paint_level(texture_width, texture_height);
  if (level < 0) /* singular paint matrix, scaled to nothing */
    return COGL_INVALID_HANDLE;
  level = MIN (level, tower->n_levels - 1);

  if (tower->textures[level] == COGL_INVALID_HANDLE ||
      (tower->invalid[level].x2 != tower->invalid[level].x1 &&
       tower->invalid[level].y2 != tower->invalid[level].y1))
    {
      int i;

      for (i = 1; i <= level; i++)
       {
         /* Use "floor" convention here to be consistent with the NPOT texture extension */
         texture_width = MAX (1, texture_width / 2);
         texture_height = MAX (1, texture_height / 2);

         if (tower->textures[i] == COGL_INVALID_HANDLE)
           texture_tower_create_texture (tower, i, texture_width, texture_height);
       }

      for (i = 1; i <= level; i++)
       {
         if (tower->invalid[level].x2 != tower->invalid[level].x1 &&
             tower->invalid[level].y2 != tower->invalid[level].y1)
           texture_tower_revalidate (tower, i);
       }
   }

  return tower->textures[level];
}
