#include "fonts.h"

ClutterFont*
font_new (const char *face)
{
  ClutterFont          *font; 
  PangoFontDescription *desc;

  font = util_malloc0(sizeof(ClutterFont));

  font->font_map = pango_ft2_font_map_new ();

  pango_ft2_font_map_set_resolution (PANGO_FT2_FONT_MAP (font->font_map),
				     96., 96.);
  
  desc = pango_font_description_from_string (face);

  font->context 
    = pango_ft2_font_map_create_context (PANGO_FT2_FONT_MAP (font->font_map));

  pango_context_set_font_description (font->context, desc);

  pango_font_description_free (desc);

  cltr_font_ref(font);

  return font;
}

#if 0
static int
decoration_get_title_width (LmcDecoration *decoration)
{
  int title_space;
  int width;
  
  title_space = (decoration->window_width
		 + lmc_theme->border_info.left
		 + lmc_theme->border_info.right
		 - lmc_theme->border_info.left_unscaled
		 - lmc_theme->border_info.right_unscaled);
  title_space = MAX (title_space, 0);

  pango_layout_get_pixel_size (decoration->layout, &width, NULL);

  return MIN (width + TITLE_RIGHT_PAD, title_space);
}
#endif

static void
get_layout_bitmap (PangoLayout    *layout,
		   FT_Bitmap      *bitmap,
		   PangoRectangle *ink)
{
  PangoRectangle ink_rect;
  
  pango_layout_get_extents (layout, &ink_rect, NULL);

  printf("%s() gave width:%i, height %i\n", __func__, ink->width, ink->height);

  /* XXX why the >> 10 */
  ink->x      = ink_rect.x >> 10;
  ink->width  = ((ink_rect.x + ink_rect.width + 1023) >> 10) - ink->x;
  ink->y      = ink_rect.y >> 10;
  ink->height = ((ink_rect.y + ink_rect.height + 1023) >> 10) - ink->y;

  if (ink->width == 0)
    ink->width = 1;
  if (ink->height == 0)
    ink->height = 1;

  bitmap->width      = ink->width;
  bitmap->pitch      = (bitmap->width + 3) & ~3;
  bitmap->rows       = ink->height;
  bitmap->buffer     = malloc (bitmap->pitch * bitmap->rows);
  bitmap->num_grays  = 256;
  bitmap->pixel_mode = FT_PIXEL_MODE_GRAY;

  memset (bitmap->buffer, 0, bitmap->pitch * bitmap->rows);

  pango_ft2_render_layout (bitmap, layout, - ink->x, - ink->y);
}

static void
draw_layout_on_pixbuf (PangoLayout       *layout,
		       Pixbuf            *pixb,
		       const PixbufPixel *color,
		       int                x,
		       int                y,
		       int                clip_x,
		       int                clip_y,
		       int                clip_width,
		       int                clip_height)
{
  PangoRectangle ink;
  FT_Bitmap      bitmap;
  int            i, j;
  unsigned char *layout_bits;

  get_layout_bitmap (layout, &bitmap, &ink);

  layout_bits = bitmap.buffer;
  
  for (j = y + ink.y; j < y + ink.y + ink.height; j++)
    {
      if (j >= clip_y && j < clip_y + clip_height)
	{
	  int start_x, end_x;

	  start_x = MAX (x + ink.x, clip_x);
	  end_x   = MIN (x + ink.x + ink.width, clip_x + clip_width);

	  if (start_x < end_x)
	    {
	      unsigned char *b = layout_bits + (start_x - (x + ink.x));

	      for (i = start_x ; i < end_x; i++)
		{
		  PixbufPixel pixel;

#if 0
		  int tr1, tg1, tb1, tr2, tg2, tb2;
		  int a = (*b * color->a + 0x80) >> 8;

		  /* 
		     this is wrong for when the backing has an
                     alpha of zero. we need a different algorythm
                     to handle that - so we can overlay just a font
                     text texture with no bg
 
		  */

		  if (!a)
		    { b++; continue; }

		  pixbuf_get_pixel (pixb, i, j, &pixel);

		  tr1 = (255 - a) * pixel.r + 0x80;
		  tr2 = a * color->r + 0x80;
		  pixel.r = ((tr1 + (tr1 >> 8)) >> 8) + ((tr2 + (tr2 >> 8)) >> 8);
		  tg1 = (255 - a) * pixel.g + 0x80;
		  tg2 = a * color->g + 0x80;
		  pixel.g = ((tg1 + (tg1 >> 8)) >> 8) + ((tg2 + (tg2 >> 8)) >> 8);
		  tb1 = (255 - a) * pixel.b + 0x80;
		  tb2 = a * color->b + 0x80;
		  pixel.b = ((tb1 + (tb1 >> 8)) >> 8) + ((tb2 + (tb2 >> 8)) >> 8);
		  tb1 = (255 - a) * pixel.a + 0x80;
		  tb2 = a * color->a + 0x80;
		  pixel.a = ((tb1 + (tb1 >> 8)) >> 8) + ((tb2 + (tb2 >> 8)) >> 8);
#endif		  
		  pixel.r = color->r; 
		  pixel.g = color->g; 
		  pixel.b = color->b; 
		  pixel.a = (( *b * color->a ) >> 8 );

		  pixbuf_set_pixel (pixb, i, j, &pixel);
		  b++;
		}
	    }
	}

      layout_bits += bitmap.pitch;
    }

  free (bitmap.buffer);
}


void
font_draw(ClutterFont *font, 
	  Pixbuf      *pixb, 
	  const char  *text,
	  int          x, 
	  int          y,
	  PixbufPixel *p)
{
  int layout_width, layout_height;
  PangoLayout *layout;

  layout = pango_layout_new (font->context);

  pango_layout_set_text (layout, text, -1);

  /* cant rely on just clip - need to set layout width too ? */
  /* pango_layout_set_width(layout, (pixb->width - x) << 10); */

  draw_layout_on_pixbuf (layout, pixb, p, x, y, 
			 x, 
			 y, 
			 pixb->width  - x,
			 pixb->height - y);

  g_object_unref(G_OBJECT(layout));
}

void
font_get_pixel_size (ClutterFont *font, 
		     const char  *text,
		     int         *width,
		     int         *height)
{
  PangoLayout *layout;

  layout = pango_layout_new (font->context);

  pango_layout_set_text (layout, text, -1);

  pango_layout_get_pixel_size (layout, width, height);

  printf("%s() gave width:%i, height %i\n", __func__, *width, *height);

  g_object_unref(G_OBJECT(layout));
}

void
cltr_font_ref(CltrFont *font)
{
  font->refcnt++;
}

void
cltr_font_unref(CltrFont *font)
{
  font->refcnt--;

  if (font->refcnt <= 0)
    {
      /* XXX free up pango stuff  */
      g_free(font);
    }
}
