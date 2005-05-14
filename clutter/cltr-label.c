#include "cltr-label.h"
#include "cltr-private.h"

struct CltrLabel
{
  CltrWidget  widget;

  char        *text;
  Pixbuf      *pixb;
  PixbufPixel *col;
  CltrFont    *font;
  CltrTexture *texture;
};

static void
cltr_label_show(CltrWidget *widget);

static gboolean 
cltr_label_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_label_paint(CltrWidget *widget);


CltrWidget*
cltr_label_new(const char  *text, 
	       CltrFont    *font,
	       PixbufPixel *col)
{
  CltrLabel *label;
  int        width,height;

  label = g_malloc0(sizeof(CltrLabel));

  font_get_pixel_size (font, text, &width, &height);

  if (width && height)
    {
      PixbufPixel bg = { 0x00, 0x00, 0x00, 0x00 };

      label->text = strdup(text);
      label->pixb  = pixbuf_new(width, height);
      
      pixbuf_fill_rect(label->pixb, 0, 0, -1, -1, &bg);

      font_draw(font, 
		label->pixb, 
		label->text,
		0,
		0,
		col);

      label->texture = cltr_texture_new(label->pixb);
    }

  label->font = font; 		/* XXX Ref The font XXX*/
  label->col  = col;            /* XXX Ref The Col  XXX*/

  label->widget.width          = width;
  label->widget.height         = height;
  
  label->widget.show           = cltr_label_show;
  label->widget.paint          = cltr_label_paint;
  
  label->widget.xevent_handler = cltr_label_handle_xevent;

  return CLTR_WIDGET(label);
}

void
cltr_label_set_text(CltrLabel *label)
{
  if (label->texture)
    cltr_texture_unref(label->texture);

  if (label->pixb)
    cltr_texture_unref(label->pixb);

  if (label->text)
    free(label->text);

  /* XXX TODO */
}

const char*
cltr_label_get_text(CltrLabel *label)
{
  return label->text;
}

static void
cltr_label_show(CltrWidget *widget)
{
  ;
}

static gboolean 
cltr_label_handle_xevent (CltrWidget *widget, XEvent *xev) 
{
  ;
}

static void
cltr_label_paint(CltrWidget *widget)
{
  CltrLabel *label = CLTR_LABEL(widget);

  CLTR_MARK();

  if (label->text)
    {
      glPushMatrix();

      glEnable(GL_TEXTURE_2D);

      glEnable(GL_BLEND);

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glColor4f(1.0, 1.0, 1.0, 1.0);

      cltr_texture_render_to_gl_quad(label->texture,
				     cltr_widget_abs_x(widget),
				     cltr_widget_abs_y(widget),
				     cltr_widget_abs_x2(widget),
				     cltr_widget_abs_y2(widget));

      glDisable(GL_BLEND);

      glDisable(GL_TEXTURE_2D);

      glPopMatrix();
    }
}
