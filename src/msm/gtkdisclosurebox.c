/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * this file Copyright (C) 2001 Havoc Pennington 
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */



/* FIXME implementation contains a bunch of cut-and-paste from GtkFrame
 * that would be easy to avoid by adding an "int space_before_label_widget"
 * in GtkFrame that was overridden by subclasses.
 */

/* FIXME the whole GtkFrame derivation idea is fucked since we can't get
 * the click event on the arrow.
 */


#define ARROW_SIZE 12
#define ARROW_PAD 2

enum {
  PROP_0,
  PROP_DISCLOSED,
  PROP_LAST
};

static void gtk_disclosure_box_class_init         (GtkDisclosureBoxClass *klass);
static void gtk_disclosure_box_init               (GtkDisclosureBox      *box);
static void gtk_disclosure_box_set_property       (GObject           *object,
                                                   guint              prop_id,
                                                   const GValue      *value,
                                                   GParamSpec        *pspec);
static void gtk_disclosure_box_get_property       (GObject           *object,
                                                   guint              prop_id,
                                                   GValue            *value,
                                                   GParamSpec        *pspec);

static void gtk_disclosure_box_paint         (GtkWidget      *widget,
                                              GdkRectangle   *area);
static gint gtk_disclosure_box_expose        (GtkWidget      *widget,
                                              GdkEventExpose *event);
static void gtk_disclosure_box_size_request  (GtkWidget      *widget,
                                              GtkRequisition *requisition);
static void gtk_disclosure_box_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *allocation);

static void gtk_frame_compute_child_allocation (GtkFrame      *frame,
                                                GtkAllocation *child_allocation);

GType
gtk_disclosure_box_get_type (void)
{
  static GType disclosure_box_type = 0;

  if (!disclosure_box_type)
    {
      static const GtkTypeInfo disclosure_box_info =
      {
	"GtkDisclosureBox",
	sizeof (GtkDisclosureBox),
	sizeof (GtkDisclosureBoxClass),
	(GtkClassInitFunc) gtk_disclosure_box_class_init,
	(GtkObjectInitFunc) gtk_disclosure_box_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      disclosure_box_type = gtk_type_unique (gtk_box_get_type (), &disclosure_box_info);
    }

  return disclosure_box_type;
}

static void
gtk_disclosure_box_class_init (GtkDisclosureBoxClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GtkContainerClass *container_class;
  GtkFrameClass *frame_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);
  frame_class = GTK_FRAME_CLASS (class);
  
  gobject_class->set_property = gtk_disclosure_box_set_property;
  gobject_class->get_property = gtk_disclosure_box_get_property;

  widget_class->size_request = gtk_disclosure_box_size_request;
  widget_class->size_allocate = gtk_disclosure_box_size_allocate;
  widget_class->expose_event = gtk_disclosure_box_expose;
}

static void
gtk_disclosure_box_init (GtkDisclosureBox *disclosure_box)
{

}

GtkWidget*
gtk_disclosure_box_new (const char *label)
{
  return g_object_new (GTK_TYPE_DISCLOSURE_BOX, "label", label, NULL);
}

static void
gtk_disclosure_box_set_property (GObject         *object,
                                 guint            prop_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  GtkDisclosureBox *box;

  box = GTK_DISCLOSURE_BOX (object);
  
  switch (prop_id) 
    {
    case PROP_DISCLOSED:
      gtk_disclosure_box_set_disclosed (box,
                                        g_value_get_boolean (value));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_disclosure_box_get_property (GObject         *object,
                                 guint            prop_id,
                                 GValue          *value,
                                 GParamSpec      *pspec)
{
  GtkDisclosureBox *box;

  box = GTK_DISCLOSURE_BOX (object);

  switch (prop_id)
    {
    case PROP_DISCLOSED:
      g_value_set_boolean (value, box->disclosed);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_disclosure_box_set_disclosed (GtkDisclosureBox *box,
                                  gboolean          disclosed)
{
  g_return_if_fail (GTK_IS_DISCLOSURE_BOX (box));
  
  disclosed = disclosed != FALSE;

  if (disclosed != box->disclosed)
    {
      box->disclosed = disclosed;
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

gboolean
gtk_disclosure_box_get_disclosed (GtkDisclosureBox *box)
{
  g_return_val_if_fail (GTK_IS_DISCLOSURE_BOX (box), FALSE);

  return box->disclosed;
}


static void
gtk_disclosure_box_paint (GtkWidget    *widget,
                          GdkRectangle *area)
{
  GtkFrame *frame;
  gint x, y, width, height;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);

      x = frame->child_allocation.x - widget->style->xthickness;
      y = frame->child_allocation.y - widget->style->ythickness;
      width = frame->child_allocation.width + 2 * widget->style->xthickness;
      height =  frame->child_allocation.height + 2 * widget->style->ythickness;

      if (frame->label_widget)
	{
	  GtkRequisition child_requisition;
	  gfloat xalign;
	  gint height_extra;
	  gint x2;

	  gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);

	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	    xalign = frame->label_xalign;
	  else
	    xalign = 1 - frame->label_xalign;

	  height_extra = MAX (0, child_requisition.height - widget->style->xthickness);
	  y -= height_extra * (1 - frame->label_yalign);
	  height += height_extra * (1 - frame->label_yalign);
	  
	  x2 = widget->style->xthickness + (frame->child_allocation.width - child_requisition.width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD) * xalign + LABEL_SIDE_PAD;

	  
	  gtk_paint_shadow_gap (widget->style, widget->window,
				GTK_STATE_NORMAL, frame->shadow_type,
				area, widget, "frame",
				x, y, width, height,
				GTK_POS_TOP, 
				x2 + ARROW_SIZE + ARROW_PAD * 2, child_requisition.width + 2 * LABEL_PAD);

          gtk_paint_arrow (widget->style, widget->window,
                           widget->state, GTK_SHADOW_OUT,
                           area, widget, "arrow",
                           GTK_DISCLOSURE_BOX (widget)->disclosed ?
                           GTK_ARROW_RIGHT : GTK_ARROW_DOWN,
                           TRUE,
                           x2 + ARROW_PAD, y, ARROW_SIZE, ARROW_SIZE);
	}
       else
	 gtk_paint_shadow (widget->style, widget->window,
			   GTK_STATE_NORMAL, frame->shadow_type,
			   area, widget, "frame",
			   x, y, width, height);
    }
}

static gboolean
gtk_disclosure_box_expose (GtkWidget      *widget,
                           GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_disclosure_box_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static void
gtk_disclosure_box_size_request (GtkWidget      *widget,
                                 GtkRequisition *requisition)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkRequisition child_requisition;
  
  if (frame->label_widget && GTK_WIDGET_VISIBLE (frame->label_widget))
    {
      gtk_widget_size_request (frame->label_widget, &child_requisition);

      requisition->width = child_requisition.width + 2 * LABEL_PAD + 2 * LABEL_SIDE_PAD + ARROW_SIZE + ARROW_PAD * 2;
      requisition->height =
	MAX (0, child_requisition.height - GTK_WIDGET (widget)->style->xthickness);
    }
  else
    {
      requisition->width = 0;
      requisition->height = 0;
    }
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height;
    }

  requisition->width += (GTK_CONTAINER (widget)->border_width +
			 GTK_WIDGET (widget)->style->xthickness) * 2;
  requisition->height += (GTK_CONTAINER (widget)->border_width +
			  GTK_WIDGET (widget)->style->ythickness) * 2;
}

static void
gtk_disclosure_box_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation new_allocation;

  widget->allocation = *allocation;

  gtk_frame_compute_child_allocation (frame, &new_allocation);
  
  /* If the child allocation changed, that means that the frame is drawn
   * in a new place, so we must redraw the entire widget.
   */
  if (GTK_WIDGET_MAPPED (widget) &&
      (new_allocation.x != frame->child_allocation.x ||
       new_allocation.y != frame->child_allocation.y ||
       new_allocation.width != frame->child_allocation.width ||
       new_allocation.height != frame->child_allocation.height))
    gtk_widget_queue_clear (widget);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, &new_allocation);
  
  frame->child_allocation = new_allocation;
  
  if (frame->label_widget && GTK_WIDGET_VISIBLE (frame->label_widget))
    {
      GtkRequisition child_requisition;
      GtkAllocation child_allocation;
      gfloat xalign;

      gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = frame->label_xalign;
      else
	xalign = 1 - frame->label_xalign;
      
      child_allocation.x = frame->child_allocation.x + LABEL_SIDE_PAD +
	(frame->child_allocation.width - child_requisition.width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD) * xalign + LABEL_PAD + ARROW_SIZE + ARROW_PAD * 2;
      child_allocation.width = child_requisition.width;

      child_allocation.y = frame->child_allocation.y - child_requisition.height;
      child_allocation.height = child_requisition.height;

      gtk_widget_size_allocate (frame->label_widget, &child_allocation);
    }
}

