#include "cltr-button.h"
#include "cltr-private.h"

struct CltrButton
{
  CltrWidget       widget;  
  CltrLabel       *label;

  CltrButtonActivate  activate_cb;
  void               *activate_cb_data;

  CltrButtonState  state;  	/* may be better in widget ? */
};

#define BUTTON_BORDER 1
#define BUTTON_PAD    5

static void
cltr_button_show(CltrWidget *widget);

static gboolean 
cltr_button_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_button_paint(CltrWidget *widget);

static void
cltr_button_focus(CltrWidget *widget);

static void
cltr_button_unfocus(CltrWidget *widget);


CltrWidget*
cltr_button_new(int width, int height)
{
  CltrButton *button;

  button = g_malloc0(sizeof(CltrButton));
  
  button->widget.width          = width;
  button->widget.height         = height;
  
  button->widget.show           = cltr_button_show;
  button->widget.paint          = cltr_button_paint;
  button->widget.focus_in       = cltr_button_focus;
  button->widget.focus_out      = cltr_button_unfocus;
  button->widget.xevent_handler = cltr_button_handle_xevent;

  return CLTR_WIDGET(button);
}

void
cltr_button_on_activate(CltrButton         *button,
			CltrButtonActivate  callback,
			void               *userdata)
{
  button->activate_cb      = callback;
  button->activate_cb_data = userdata;
}

CltrWidget*
cltr_button_new_with_label(const char  *label, 
			   CltrFont    *font,
			   PixbufPixel *col)
{
  CltrButton *button = NULL;

  button = CLTR_BUTTON(cltr_button_new(-1, -1));

  button->label = CLTR_LABEL(cltr_label_new(label, font, col));

  button->widget.width  = cltr_widget_width((CltrWidget*)button->label) + (2 * ( BUTTON_BORDER + BUTTON_PAD));
  button->widget.height = cltr_widget_height((CltrWidget*)button->label) + ( 2 * ( BUTTON_BORDER + BUTTON_PAD));

  CLTR_DBG("width: %i, height %i", 
	   cltr_widget_width((CltrWidget*)button->label),
	   cltr_widget_height((CltrWidget*)button->label));

  cltr_widget_add_child(CLTR_WIDGET(button), 
			CLTR_WIDGET(button->label), 
			( BUTTON_BORDER + BUTTON_PAD),
			( BUTTON_BORDER + BUTTON_PAD));

  return CLTR_WIDGET(button);
}

static void
cltr_button_show(CltrWidget *widget)
{

}

static void
cltr_button_focus(CltrWidget *widget)
{
  CltrButton *button = CLTR_BUTTON(widget);

  if (button->state != CltrButtonStateFocused)
    {
      button->state = CltrButtonStateFocused;
      cltr_widget_queue_paint(widget);
    }
}

static void
cltr_button_unfocus(CltrWidget *widget)
{
  CltrButton *button = CLTR_BUTTON(widget);

  if (button->state != CltrButtonStateInactive)
    {
      button->state = CltrButtonStateInactive;
      
      cltr_widget_queue_paint(CLTR_WIDGET(button));
    }
}

static void
cltr_button_handle_xkeyevent(CltrButton *button, XKeyEvent *xkeyev)
{
  KeySym          kc;
  CltrButtonState old_state;
  CltrWidget     *next_focus = NULL;

  old_state = button->state;

  kc = XKeycodeToKeysym(xkeyev->display, xkeyev->keycode, 0);

  switch (kc)
    {
    case XK_Left:
    case XK_KP_Left:
      if (xkeyev->type != KeyPress)
	break;
      next_focus = cltr_widget_get_focus_next(CLTR_WIDGET(button), CLTR_WEST);
      break;
    case XK_Up:
    case XK_KP_Up:
      if (xkeyev->type != KeyPress)
	break;

      next_focus = cltr_widget_get_focus_next(CLTR_WIDGET(button), CLTR_NORTH);
      break;
    case XK_Right:
    case XK_KP_Right:
      if (xkeyev->type != KeyPress)
	break;

      next_focus = cltr_widget_get_focus_next(CLTR_WIDGET(button), CLTR_EAST);
      break;
    case XK_Down:	
    case XK_KP_Down:	
      if (xkeyev->type != KeyPress)
	break;

      next_focus = cltr_widget_get_focus_next(CLTR_WIDGET(button), CLTR_SOUTH);
      break;
    case XK_Return:
      if (xkeyev->type == KeyPress)
	{
	  if (button->state != CltrButtonStateActive)
	    button->state = CltrButtonStateActive;
	  CLTR_DBG("press");

	  if (button->activate_cb)
	    button->activate_cb(CLTR_WIDGET(button), button->activate_cb_data);

	}
      else 	      /* KeyRelease */
	{
	  CLTR_DBG("release");
	  if (button->state != CltrButtonStateFocused)
	    button->state = CltrButtonStateFocused;

	  /* What to do about key repeats ? */

	}
      break;
    default:
      /* ??? */
    }

  if (button->state != old_state)
    {
      CLTR_DBG("queueing paint");
      cltr_widget_queue_paint(CLTR_WIDGET(button));
    }

  if (next_focus)
    {
      /* Evil - need to centralise focus management */
      ClutterMainContext *ctx = CLTR_CONTEXT();
      cltr_window_focus_widget(ctx->window, next_focus);
    }
}


static gboolean 
cltr_button_handle_xevent (CltrWidget *widget, XEvent *xev) 
{
  CltrButton *button = CLTR_BUTTON(widget);

  switch (xev->type)
    {
    case KeyPress:
    case KeyRelease:
      CLTR_DBG("KeyPress");
      cltr_button_handle_xkeyevent(button, &xev->xkey);
      break;
    }
}


static void
cltr_button_paint(CltrWidget *widget)
{
  CltrButton *button = CLTR_BUTTON(widget);

  CLTR_MARK();

  glPushMatrix();

  glEnable(GL_BLEND);

  switch (button->state) 
    {
    case CltrButtonStateFocused:
      glColor4f(1.0, 1.0, 0.0, 1.0);
      break;
    case CltrButtonStateActive:
      glColor4f(1.0, 0.0, 0.0, 1.0);
      break;
    default:
      glColor4f(1.0, 1.0, 1.0, 1.0);
    }

  cltr_glu_rounded_rect(widget->x,
			widget->y,
			widget->x + widget->width,
			widget->y + widget->height,
			widget->width/30,
			NULL);

  glDisable(GL_BLEND);

  glPopMatrix();
}


