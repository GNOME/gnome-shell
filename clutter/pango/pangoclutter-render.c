/* Pango
 * Rendering routines to Clutter
 *
 * Copyright (C) 2006 Matthew Allum <mallum@o-hand.com>
 * Copyright (C) 2006 Marc Lehmann <pcg@goof.com>
 * Copyright (C) 2004 Red Hat Software
 * Copyright (C) 2000 Tor Lillqvist
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "pangoclutter.h"
#include "pangoclutter-private.h"
#include "../clutter-debug.h"

/* 
 * Texture cache support code
 */

#define TC_WIDTH  256
#define TC_HEIGHT 256
#define TC_ROUND  4

typedef struct {
  GLuint name;
  int x, y, w, h;
} tc_area;

typedef struct tc_texture {
  struct tc_texture *next;
  GLuint name;
  int avail;
} tc_texture;

typedef struct tc_slice {
  GLuint name;
  int avail, y;
} tc_slice;

static int tc_generation;
static tc_slice slices[TC_HEIGHT / TC_ROUND];
static tc_texture *first_texture;

static void
tc_clear ()
{
  int i;

  for (i = TC_HEIGHT / TC_ROUND; i--; )
    slices [i].name = 0;

  while (first_texture)
    {
      tc_texture *next = first_texture->next;
      glDeleteTextures (1, &first_texture->name);
      g_slice_free (tc_texture, first_texture);
      first_texture = next;
    }

  printf("freeing textures\n");

  ++tc_generation;
}

static void
tc_get (tc_area *area, int width, int height)
{
  int slice_height = MIN (height + TC_ROUND - 1, TC_HEIGHT) & ~(TC_ROUND - 1);
  tc_slice *slice = slices + slice_height / TC_ROUND;

  area->w = width;
  area->h = height;

  width = MIN (width, TC_WIDTH);

  if (!slice->name || slice->avail < width)
    {
      /* try to find a texture with enough space */
      tc_texture *tex, *match = 0;

      for (tex = first_texture; tex; tex = tex->next)
        if (tex->avail >= slice_height && (!match || match->avail > tex->avail))
          match = tex;

      /* create a new texture if necessary */
      if (!match)
        {
	  CLUTTER_NOTE (PANGO, g_message ("creating new texture %i x %i\n",
                                          TC_WIDTH, TC_HEIGHT));

          match = g_slice_new (tc_texture);
          match->next = first_texture;
          first_texture = match;
          glGenTextures (1, &match->name);
          match->avail = TC_HEIGHT;

          glBindTexture (GL_TEXTURE_2D, match->name);
          glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glTexImage2D (GL_TEXTURE_2D, 0, GL_ALPHA, 
			TC_WIDTH, TC_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
        }

      match->avail -= slice_height;

      slice->name  = match->name;
      slice->avail = TC_WIDTH;
      slice->y     = match->avail;
    }

  slice->avail -= width;

  area->name = slice->name;
  area->x    = slice->avail;
  area->y    = slice->y;
}

static void
tc_put (tc_area *area)
{
  /* our management is too primitive to support this operation yet */
}

/*******************/


#define PANGO_CLUTTER_RENDERER_CLASS(klass)                       \
          (G_TYPE_CHECK_CLASS_CAST ((klass),                      \
                                    PANGO_TYPE_CLUTTER_RENDERER,  \
                                    PangoClutterRendererClass))

#define PANGO_IS_CLUTTER_RENDERER_CLASS(klass)                    \
          (G_TYPE_CHECK_CLASS_TYPE ((klass), PANGO_TYPE_CLUTTER_RENDERER))

#define PANGO_CLUTTER_RENDERER_GET_CLASS(obj)                      \
          (G_TYPE_INSTANCE_GET_CLASS ((obj),                       \
				      PANGO_TYPE_CLUTTER_RENDERER, \
				      PangoClutterRendererClass))

typedef struct {
  PangoRendererClass parent_class;
} PangoClutterRendererClass;

struct _PangoClutterRenderer
{
  PangoRenderer parent_instance;
  ClutterColor  color;
  int           flags;
  GLuint        curtex; /* current texture */
};

G_DEFINE_TYPE (PangoClutterRenderer,   \
	       pango_clutter_renderer, \
	       PANGO_TYPE_RENDERER)

typedef struct
{
  guint8 *bitmap;
  int     width, stride, height, top, left;
} Glyph;

static void *
temp_buffer (size_t size)
{
  static char *buffer;
  static size_t alloc;

  if (size > alloc)
    {
      size = (size + 4095) & ~4095;
      free (buffer);
      alloc = size;
      buffer = malloc (size);
    }

  return buffer;
}

static void
render_box (Glyph *glyph, int width, int height, int top)
{
  int i;
  int left = 0;

  if (height > 2)
    {
      height -= 2;
      top++;
    }

  if (width > 2)
    {
      width -= 2;
      left++;
    }

  glyph->stride = (width + 3) & ~3;
  glyph->width  = width;
  glyph->height = height;
  glyph->top    = top;
  glyph->left   = left;

  glyph->bitmap = temp_buffer (width * height);
  memset (glyph->bitmap, 0, glyph->stride * height);

  for (i = width; i--; )
    glyph->bitmap [i] 
      = glyph->bitmap [i + (height - 1) * glyph->stride] = 0xff;

  for (i = height; i--; )
    glyph->bitmap [i * glyph->stride] 
      = glyph->bitmap [i * glyph->stride + (width - 1)] = 0xff;
}

static void
font_render_glyph (Glyph *glyph, PangoFont *font, int glyph_index)
{
  FT_Face face;

  if (glyph_index & PANGO_GLYPH_UNKNOWN_FLAG)
    {
      PangoFontMetrics *metrics;

      if (!font)
	goto generic_box;

      metrics = pango_font_get_metrics (font, NULL);
      if (!metrics)
	goto generic_box;

      render_box (glyph, PANGO_PIXELS (metrics->approximate_char_width),
		         PANGO_PIXELS (metrics->ascent + metrics->descent),
		         PANGO_PIXELS (metrics->ascent));

      pango_font_metrics_unref (metrics);

      return;
    }

  face = pango_clutter_font_get_face (font);
  
  if (face)
    {
      PangoClutterFont *glfont = (PangoClutterFont *)font;

      FT_Load_Glyph (face, glyph_index, glfont->load_flags);
      FT_Render_Glyph (face->glyph, ft_render_mode_normal);

      glyph->width  = face->glyph->bitmap.width;
      glyph->stride = face->glyph->bitmap.pitch;
      glyph->height = face->glyph->bitmap.rows;
      glyph->top    = face->glyph->bitmap_top;
      glyph->left   = face->glyph->bitmap_left;
      glyph->bitmap = face->glyph->bitmap.buffer;
    }
  else
    generic_box:
      render_box (glyph, PANGO_UNKNOWN_GLYPH_WIDTH, 
		  PANGO_UNKNOWN_GLYPH_HEIGHT, PANGO_UNKNOWN_GLYPH_HEIGHT);
}

typedef struct glyph_info 
{
  tc_area tex;
  int     left, top;
  int     generation;
} 
glyph_info;

static void
free_glyph_info (glyph_info *g)
{
  tc_put (&g->tex);
  g_slice_free (glyph_info, g);
}

static void
draw_glyph (PangoRenderer *renderer_, 
	    PangoFont     *font, 
	    PangoGlyph     glyph, 
	    double         x, 
	    double         y)
{
  PangoClutterRenderer *renderer = PANGO_CLUTTER_RENDERER (renderer_);
  glyph_info           *g;
  float                 x1, y1, x2, y2;

  if (glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    {
      glyph = pango_clutter_get_unknown_glyph (font);
      
      if (glyph == PANGO_GLYPH_EMPTY)
	glyph = PANGO_GLYPH_UNKNOWN_FLAG;
    }

  g = _pango_clutter_font_get_cache_glyph_data (font, glyph);

  if (!g || g->generation != tc_generation)
    {
      Glyph bm;
      font_render_glyph (&bm, font, glyph);

      if (g)
        g->generation = tc_generation;
      else
        {
          g = g_slice_new (glyph_info);

          _pango_clutter_font_set_glyph_cache_destroy 
	                        (font, (GDestroyNotify)free_glyph_info);
          _pango_clutter_font_set_cache_glyph_data (font, glyph, g);
        }

      if (renderer->curtex)
        glEnd ();

      tc_get (&g->tex, bm.width, bm.height);

      g->left = bm.left;
      g->top  = bm.top;

      CLUTTER_NOTE (PANGO, g_message ("cache fail; subimage2d %i\n", glyph));

      glBindTexture (GL_TEXTURE_2D, g->tex.name);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, bm.stride);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      glTexSubImage2D (GL_TEXTURE_2D, 
		       0, 
		       g->tex.x, 
		       g->tex.y, 
		       bm.width, 
		       bm.height, 
		       GL_ALPHA, 
		       GL_UNSIGNED_BYTE, 
		       bm.bitmap);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

      renderer->curtex = g->tex.name;
      glBegin (GL_QUADS);
    }
  else CLUTTER_NOTE (PANGO, g_message ("cache succsess %i\n", glyph));

  x += g->left;
  y -= g->top;

  x1 = g->tex.x * (1. / TC_WIDTH );
  y1 = g->tex.y * (1. / TC_HEIGHT);
  x2 = g->tex.w * (1. / TC_WIDTH ) + x1;
  y2 = g->tex.h * (1. / TC_HEIGHT) + y1;

  if (g->tex.name != renderer->curtex)
    {
      if (renderer->curtex)
        glEnd ();

      glBindTexture (GL_TEXTURE_2D, g->tex.name);
      renderer->curtex = g->tex.name;

      glBegin (GL_QUADS);
    }

  glTexCoord2f (x1, y1); glVertex2i (x           , y           );
  glTexCoord2f (x2, y1); glVertex2i (x + g->tex.w, y           );
  glTexCoord2f (x2, y2); glVertex2i (x + g->tex.w, y + g->tex.h);
  glTexCoord2f (x1, y2); glVertex2i (x           , y + g->tex.h);
}

static void
draw_trapezoid (PangoRenderer   *renderer_,
		PangoRenderPart  part,
		double           y1,
		double           x11,
		double           x21,
		double           y2,
		double           x12,
		double           x22)
{
  PangoClutterRenderer *renderer = (PangoClutterRenderer *)renderer_;

  if (renderer->curtex)
    {
      glEnd ();
      renderer->curtex = 0;
    }

  glDisable (GL_TEXTURE_2D);

  glBegin (GL_QUADS);
  glVertex2d (x11, y1);
  glVertex2d (x21, y1);
  glVertex2d (x22, y2);
  glVertex2d (x12, y2);
  glEnd ();

  glEnable (GL_TEXTURE_2D);
}

void 
pango_clutter_render_layout_subpixel (PangoLayout *layout,
				      int           x, 
				      int           y,
				      ClutterColor *color,
				      int           flags)
{
  PangoContext  *context;
  PangoFontMap  *fontmap;
  PangoRenderer *renderer;

  context = pango_layout_get_context (layout);
  fontmap = pango_context_get_font_map (context);
  renderer = _pango_clutter_font_map_get_renderer 
                     (PANGO_CLUTTER_FONT_MAP (fontmap));

  memcpy (&(PANGO_CLUTTER_RENDERER (renderer)->color), 
	  color, sizeof(ClutterColor));
  
  pango_renderer_draw_layout (renderer, layout, x, y);
}

void 
pango_clutter_render_layout (PangoLayout  *layout,
			     int           x, 
			     int           y,
			     ClutterColor *color,
			     int           flags)
{
  pango_clutter_render_layout_subpixel (layout, 
					x * PANGO_SCALE, 
					y * PANGO_SCALE, 
					color,
					flags);
}

void 
pango_clutter_render_layout_line (PangoLayoutLine *line,
				  int              x,
				  int              y,
				  ClutterColor    *color)
{
  PangoContext  *context;
  PangoFontMap  *fontmap;
  PangoRenderer *renderer;

  context = pango_layout_get_context (line->layout);
  fontmap = pango_context_get_font_map (context);
  renderer = _pango_clutter_font_map_get_renderer 
                     (PANGO_CLUTTER_FONT_MAP (fontmap));

  memcpy (&(PANGO_CLUTTER_RENDERER (renderer)->color), 
	  color, sizeof(ClutterColor));
  
  pango_renderer_draw_layout_line (renderer, line, x, y);
}

void 
pango_clutter_render_clear_caches (void)
{
  tc_clear();
}

static void
pango_clutter_renderer_init (PangoClutterRenderer *renderer)
{
  memset (&renderer->color, 0xff, sizeof(ClutterColor));
}

static void
prepare_run (PangoRenderer *renderer, PangoLayoutRun *run)
{
  PangoClutterRenderer *glrenderer = (PangoClutterRenderer *)renderer;
  PangoColor           *fg = 0;
  GSList               *l;
  unsigned char         r, g, b, a;

  renderer->underline = PANGO_UNDERLINE_NONE;
  renderer->strikethrough = FALSE;

  for (l = run->item->analysis.extra_attrs; l; l = l->next)
    {
      PangoAttribute *attr = l->data;
      
      switch (attr->klass->type)
	{
	case PANGO_ATTR_UNDERLINE:
	  renderer->underline = ((PangoAttrInt *)attr)->value;
	  break;
	  
	case PANGO_ATTR_STRIKETHROUGH:
	  renderer->strikethrough = ((PangoAttrInt *)attr)->value;
	  break;
	  
	case PANGO_ATTR_FOREGROUND:
          fg = &((PangoAttrColor *)attr)->color;
	  break;
	default:
	  break;
	}
    }

  if (fg)
    {
      r = fg->red   * (255.f / 65535.f);
      g = fg->green * (255.f / 65535.f);
      b = fg->blue  * (255.f / 65535.f);
    }
  else 
    {
      r = glrenderer->color.red;
      g = glrenderer->color.green;
      b = glrenderer->color.blue;
    }

  a = glrenderer->color.alpha;

  if (glrenderer->flags & FLAG_INVERSE)
    {
      r ^= 0xffU;
      g ^= 0xffU;
      b ^= 0xffU;
    } 
  
  glColor4ub (r, g, b, a);
}

static void
draw_begin (PangoRenderer *renderer_)
{
  PangoClutterRenderer *renderer = (PangoClutterRenderer *)renderer_;

  renderer->curtex = 0;

  glEnable (GL_TEXTURE_2D);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glEnable (GL_BLEND);
#if 0
  gl_BlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE      , GL_ONE_MINUS_SRC_ALPHA);
#endif
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if 0
  glEnable (GL_ALPHA_TEST);
  glAlphaFunc (GL_GREATER, 0.01f);
#endif
}

static void
draw_end (PangoRenderer *renderer_)
{
  PangoClutterRenderer *renderer = (PangoClutterRenderer *)renderer_;

  if (renderer->curtex)
    glEnd ();

  glDisable (GL_ALPHA_TEST);
  glDisable (GL_BLEND);
  glDisable (GL_TEXTURE_2D);
}

static void
pango_clutter_renderer_class_init (PangoClutterRendererClass *klass)
{
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);

  renderer_class->draw_glyph     = draw_glyph;
  renderer_class->draw_trapezoid = draw_trapezoid;
  renderer_class->prepare_run    = prepare_run;
  renderer_class->begin          = draw_begin;
  renderer_class->end            = draw_end;
}

