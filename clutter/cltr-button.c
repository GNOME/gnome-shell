#include "cltr-button.h"
#include "cltr-private.h"

struct CltrButton
{
  CltrWidget  widget;  
  char       *label;
  Pixbuf     *pixb;
  
};

static void
cltr_button_show(CltrWidget *widget);

static gboolean 
cltr_button_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_button_paint(CltrWidget *widget);


CltrWidget*
cltr_button_new(int width, int height)
{
  CltrButton *button;

  button = g_malloc0(sizeof(CltrButton));
  
  button->widget.width          = width;
  button->widget.height         = height;
  
  button->widget.show           = cltr_button_show;
  button->widget.paint          = cltr_button_paint;
  
  button->widget.xevent_handler = cltr_button_handle_xevent;

  return CLTR_WIDGET(button);
}

CltrWidget*
cltr_button_new_with_label(const char  *label, 
			   CltrFont    *font,
			   PixbufPixel *col)
{
  CltrButton *button = NULL;
  int         label_width, label_height;

  if (font == NULL)
    {
      /* XXXX grab default font */
    }


  return CLTR_WIDGET(button);
}

static void
cltr_button_show(CltrWidget *widget)
{

}

static gboolean 
cltr_button_handle_xevent (CltrWidget *widget, XEvent *xev) 
{

}

static void
cltr_button_paint(CltrWidget *widget)
{


}
