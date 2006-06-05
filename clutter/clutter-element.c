/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

/**
 * SECTION:clutter-elementy
 * @short_description: Base abstract class for all visual stage elements. 
 * 
 * #ClutterElement is an blah blah
 */

#include "config.h"

#include "clutter-element.h"
#include "clutter-main.h"
#include "clutter-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterElement, clutter_element, G_TYPE_OBJECT);

static guint32 __id = 0;

#define CLUTTER_ELEMENT_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ELEMENT, ClutterElementPrivate))

struct _ClutterElementPrivate
{
  ClutterElementBox       coords;

  ClutterGeometry         clip;
  gboolean                has_clip;

  ClutterElementTransform mirror_transform;
  gfloat                  rxang, ryang, rzang; /* Rotation foo. */
  gint                    rzx, rzy, rxy, rxz, ryx, ryz;
  gint                    z; 	/* to element box ? */

  guint8                  opacity;
  ClutterElement         *parent_element; /* This should always be a group */
  gchar                  *name;
  guint32                 id; 	/* Unique ID */
};

enum
{
  PROP_0,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  /* PROP_CLIP FIXME: add */
  PROP_OPACITY,
  PROP_NAME,
};

static gboolean
redraw_update_idle (gpointer data)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();

  clutter_threads_enter();

  if (ctx->update_idle)
    {
      g_source_remove (ctx->update_idle);
      ctx->update_idle = 0;
    }

  clutter_threads_leave();

  clutter_redraw ();

  return FALSE;
}

/**
 * clutter_element_show
 * @self: A #ClutterElement
 *
 * Flags a clutter element to be displayed. An element not shown will not 
 * appear on the display.
 **/
void
clutter_element_show (ClutterElement *self)
{
  ClutterElementClass *klass;

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    return;

  if (!CLUTTER_ELEMENT_IS_REALIZED (self))
    clutter_element_realize(self);

  CLUTTER_ELEMENT_SET_FLAGS (self, CLUTTER_ELEMENT_MAPPED);

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  if (klass->show)
    (klass->show) (self);

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    clutter_element_queue_redraw (self);
}

/**
 * clutter_element_hide
 * @self: A #ClutterElement
 *
 * Flags a clutter element to be hidden. An element not shown will not 
 * appear on the display.
 **/
void
clutter_element_hide (ClutterElement *self)
{
  ClutterElementClass *klass;

  if (!CLUTTER_ELEMENT_IS_VISIBLE (self))
    return;

  CLUTTER_ELEMENT_UNSET_FLAGS (self, CLUTTER_ELEMENT_MAPPED);

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  if (klass->hide)
    (klass->hide) (self);

  clutter_element_queue_redraw (self);
}

/**
 * clutter_element_realize
 * @self: A #ClutterElement
 *
 * Creates any underlying graphics resources needed by the element to be
 * displayed.  
 **/
void
clutter_element_realize (ClutterElement *self)
{
  ClutterElementClass *klass;

  if (CLUTTER_ELEMENT_IS_REALIZED (self))
    return;

  CLUTTER_ELEMENT_SET_FLAGS (self, CLUTTER_ELEMENT_REALIZED);

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  if (klass->realize)
    (klass->realize) (self);
}

/**
 * clutter_element_realize
 * @self: A #ClutterElement
 *
 * Frees up any underlying graphics resources needed by the element to be
 * displayed.  
 **/
void
clutter_element_unrealize (ClutterElement *self)
{
  ClutterElementClass *klass;

  if (!CLUTTER_ELEMENT_IS_REALIZED (self))
    return;

  CLUTTER_ELEMENT_UNSET_FLAGS (self, CLUTTER_ELEMENT_REALIZED);

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  if (klass->unrealize)
    (klass->unrealize) (self);
}

/**
 * clutter_element_paint:
 * @self: A #ClutterElement
 *
 * Renders the element to display.
 *
 * This function should not be called directly by applications instead 
 * #clutter_element_queue_redraw should be used to queue paints. 
 **/
void
clutter_element_paint (ClutterElement *self)
{
  ClutterElementClass *klass;

  if (!CLUTTER_ELEMENT_IS_REALIZED (self))
    {
      CLUTTER_DBG("@@@ Attempting realize via paint() @@@");
      clutter_element_realize(self);

      if (!CLUTTER_ELEMENT_IS_REALIZED (self))
	{
	  CLUTTER_DBG("*** Attempt failed, aborting paint ***");
	  return;
	}
    }

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  if (self->priv->has_clip)
    {
      ClutterGeometry *clip = &(self->priv->clip);
      gint             absx, absy;
      ClutterElement  *stage = clutter_stage_get_default ();

      clutter_element_get_abs_position (self, &absx, &absy);

      CLUTTER_DBG("clip +%i+%i, %ix%i\n", 
		  absx + clip->x, 
		  clutter_element_get_height (stage) 
		  - (absy + clip->y) - clip->height, 
		  clip->width, 
		  clip->height);

      glEnable (GL_SCISSOR_TEST);

      glScissor (absx + clip->x, 
		 clutter_element_get_height (stage) 
                    - (absy + clip->y) - clip->height, 
		 clip->width, 
		 clip->height);

      g_object_unref (stage);
    }

  glPushMatrix();

  glLoadName (clutter_element_get_id (self));

  /* FIXME: Less clunky */

  if (self->priv->rzang)
    {
      glTranslatef ( (float)(self->priv->coords.x1) + self->priv->rzx,
		     (float)(self->priv->coords.y1) + self->priv->rzy,
		     0.0);

      glRotatef (self->priv->rzang, 0.0f, 0.0f, 1.0f);

      glTranslatef ( (-1.0 * self->priv->coords.x1) - self->priv->rzx,
		     (-1.0 * self->priv->coords.y1) - self->priv->rzy,
		     0.0 );
    }

  if (self->priv->ryang)
    {
      glTranslatef ( (float)(self->priv->coords.x1) + self->priv->ryx,
		     0.0,
		     (float)(self->priv->z) + self->priv->ryz);

      glRotatef (self->priv->ryang, 0.0f, 1.0f, 0.0f);

      glTranslatef ( (float)(-1.0 * self->priv->coords.x1) - self->priv->ryx,
		     0.0,
		     (float)(-1.0 * self->priv->z) - self->priv->ryz);
    }

  if (self->priv->rxang)
    {
      glTranslatef ( 0.0,
		     (float)(self->priv->coords.x1) + self->priv->rxy,
		     (float)(self->priv->z) + self->priv->rxz);

      glRotatef (self->priv->rxang, 1.0f, 0.0f, 0.0f);

      glTranslatef ( 0.0,
		     (float)(-1.0 * self->priv->coords.x1) - self->priv->rxy,
		     (float)(-1.0 * self->priv->z) - self->priv->rxz);
    }

  if (self->priv->z)
    glTranslatef ( 0.0, 0.0, (float)self->priv->z);

  if (klass->paint)
    (klass->paint) (self);

  glPopMatrix();

  if (self->priv->has_clip)
    glDisable (GL_SCISSOR_TEST);
}

/**
 * clutter_element_request_coords:
 * @self: A #ClutterElement
 * @box: A #ClutterElementBox with requested new co-ordinates.
 *
 * Requests new co-ordinates for the #ClutterElement ralative to any parent.
 *
 * This function should not be called directly by applications instead 
 * the various position/geometry methods should be used.
 **/
void
clutter_element_request_coords (ClutterElement    *self,
				ClutterElementBox *box)
{
  ClutterElementClass *klass;

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  /* FIXME: Kludgy see allocate co-ords */
  if (klass->request_coords)
    klass->request_coords(self, box);

  self->priv->coords.x1 = box->x1;
  self->priv->coords.y1 = box->y1; 
  self->priv->coords.x2 = box->x2; 
  self->priv->coords.y2 = box->y2; 

  /* TODO: Fire a signal ? Could be usage for WM resizing stage */

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    clutter_element_queue_redraw (self);
}

/**
 * clutter_element_allocate_coords:
 * @self: A #ClutterElement
 * @box: A location to store the elements #ClutterElementBox co-ordinates
 *
 * Requests the allocated co-ordinates for the #ClutterElement ralative 
 * to any parent.
 *
 * This function should not be called directly by applications instead 
 * the various position/geometry methods should be used.
 **/
void
clutter_element_allocate_coords (ClutterElement    *self,
				 ClutterElementBox *box)
{
  ClutterElementClass *klass;

  klass = CLUTTER_ELEMENT_GET_CLASS (self);

  box->x1 = self->priv->coords.x1;
  box->y1 = self->priv->coords.y1;
  box->x2 = self->priv->coords.x2;
  box->y2 = self->priv->coords.y2;

  if (klass->request_coords)
    {
      /* FIXME: This is kind of a cludge - we pass out *private* 
       *        co-ords down to any subclasses so they can modify
       *        we then resync any changes. Needed for group class.
       *        Need to figure out nicer way.
      */
      klass->allocate_coords(self, box);

      self->priv->coords.x1 = box->x1;
      self->priv->coords.y1 = box->y1; 
      self->priv->coords.x2 = box->x2; 
      self->priv->coords.y2 = box->y2; 
    }
}

static void 
clutter_element_set_property (GObject      *object, 
			      guint         prop_id,
			      const GValue *value, 
			      GParamSpec   *pspec)
{

  ClutterElement        *element;
  ClutterElementPrivate *priv;

  element = CLUTTER_ELEMENT(object);
  priv = element->priv;

  switch (prop_id) 
    {
    case PROP_X:
      clutter_element_set_position (element, 
				    g_value_get_int (value), 
				    clutter_element_get_y (element));
      break;
    case PROP_Y:
      clutter_element_set_position (element, 
				    clutter_element_get_x (element),
				    g_value_get_int (value));
      break;
    case PROP_WIDTH:
      clutter_element_set_size (element, 
				g_value_get_int (value),
				clutter_element_get_height (element));
      break;
    case PROP_HEIGHT:
      clutter_element_set_size (element, 
				clutter_element_get_width (element),
				g_value_get_int (value));
      break;
    case PROP_OPACITY:
      clutter_element_set_opacity (element, g_value_get_uchar (value));
      break;
    case PROP_NAME:
      clutter_element_set_name (element, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
clutter_element_get_property (GObject    *object, 
			      guint       prop_id,
			      GValue     *value, 
			      GParamSpec *pspec)
{
  ClutterElement        *element;
  ClutterElementPrivate *priv;

  element = CLUTTER_ELEMENT(object);
  priv = element->priv;

  switch (prop_id) 
    {
    case PROP_X:
      g_value_set_int (value, clutter_element_get_x (element));
      break;
    case PROP_Y:
      g_value_set_int (value, clutter_element_get_y (element));
      break;
    case PROP_WIDTH:
      g_value_set_int (value, clutter_element_get_width (element));
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, clutter_element_get_height (element));
      break;
    case PROP_OPACITY:
      g_value_set_uchar (value, priv->opacity);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
clutter_element_dispose (GObject *object)
{
  ClutterElement *self = CLUTTER_ELEMENT(object); 

  if (self->priv->parent_element)
    {
      clutter_group_remove (CLUTTER_GROUP(self->priv->parent_element), self);
    }

  G_OBJECT_CLASS (clutter_element_parent_class)->dispose (object);
}

static void 
clutter_element_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_element_parent_class)->finalize (object);
}

static void
clutter_element_class_init (ClutterElementClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = clutter_element_set_property;
  object_class->get_property = clutter_element_get_property;
  object_class->dispose      = clutter_element_dispose;
  object_class->finalize     = clutter_element_finalize;

  g_type_class_add_private (klass, sizeof (ClutterElementPrivate));

  g_object_class_install_property
    (object_class, PROP_X,
     g_param_spec_int ("x",
		       "X co-ord",
		       "X co-ord of element",
		       0,
		       G_MAXINT,
		       0,
		       G_PARAM_READWRITE));

  g_object_class_install_property
    (object_class, PROP_Y,
     g_param_spec_int ("y",
		       "Y co-ord",
		       "Y co-ord of element",
		       0,
		       G_MAXINT,
		       0,
		       G_PARAM_READWRITE));

  g_object_class_install_property
    (object_class, PROP_WIDTH,
     g_param_spec_int ("width",
		       "Width",
		       "Width of element in pixels",
		       0,
		       G_MAXINT,
		       0,
		       G_PARAM_READWRITE));

  g_object_class_install_property
    (object_class, PROP_HEIGHT,
     g_param_spec_int ("height",
		       "Height",
		       "Height of element in pixels",
		       0,
		       G_MAXINT,
		       0,
		       G_PARAM_READWRITE));
  
  g_object_class_install_property
    (object_class, PROP_OPACITY,
     g_param_spec_uchar ("opacity",
			 "Opacity",
			 "Opacity of element",
			 0,
			 0xff,
			 0xff,
			 G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  /* FIXME: add - as boxed ?  
   *  g_object_class_install_property
   * (gobject_class, PROP_CLIP,
   *  g_param_spec_pointer ("clip",
   *			   "Clip",
   *			   "Clip",
   *			   G_PARAM_READWRITE));
  */
}

static void
clutter_element_init (ClutterElement *self)
{
  self->priv = CLUTTER_ELEMENT_GET_PRIVATE (self); 

  self->priv->parent_element = NULL;
  self->priv->has_clip       = FALSE;
  self->priv->opacity        = 0xff;
  self->priv->id             = __id++;

  clutter_element_set_position (self, 0, 0);
  clutter_element_set_size (self, 0, 0);
}

/**
 * clutter_element_queue_redraw:
 * @self: A #ClutterElement
 *
 * Queues up a redraw of an element and any children. The redraw occurs 
 * once the main loop becomes idle (after the current batch of events 
 * has been processed, roughly).
 *
 * Applications rarely need to call this as redraws are handled automatically
 * by modification functions. 
 */
void
clutter_element_queue_redraw (ClutterElement *self)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();

  clutter_threads_enter();

  if (!ctx->update_idle)
    {
      ctx->update_idle = g_idle_add_full (-100 , /* very high priority */
					  redraw_update_idle, 
					  NULL, NULL);
    }

  clutter_threads_leave();
}

/**
 * clutter_element_set_geometry:
 * @self: A #ClutterElement
 * @geom: A #ClutterGeometry
 *
 * Sets the elements geometry in pixels relative to any parent element.
 */
void
clutter_element_set_geometry (ClutterElement  *self,
			      ClutterGeometry *geom)
{
  ClutterElementBox box;

  box.x1 = geom->x;
  box.y1 = geom->y;
  box.x2 = geom->x + geom->width;
  box.y2 = geom->y + geom->height;
  
  clutter_element_request_coords (self, &box);
}

/**
 * clutter_element_get_geometry:
 * @self: A #ClutterElement
 * @geom: A location to store elements #ClutterGeometry
 *
 * Gets the elements geometry in pixels relative to any parent element.
 */
void
clutter_element_get_geometry (ClutterElement  *self,
			      ClutterGeometry *geom)
{
  ClutterElementBox box;

  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  clutter_element_allocate_coords (self, &box);

  geom->x      = box.x1;
  geom->y      = box.y1;
  geom->width  = box.x2 - box.x1;
  geom->height = box.y2 - box.y1;
}

/**
 * clutter_element_get_coords:
 * @self: A #ClutterElement
 * @x1: A location to store elements left position if non NULL.
 * @y1: A location to store elements top position if non NULL.
 * @x2: A location to store elements right position if non NULL.
 * @y2: A location to store elements bottom position if non NULL.
 *
 * Gets the elements bounding rectangle co-ordinates in pixels 
 * relative to any parent element. 
 */
void
clutter_element_get_coords (ClutterElement *self,
			    gint           *x1,
			    gint           *y1,
			    gint           *x2,
			    gint           *y2)
{
  ClutterElementBox box;

  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  clutter_element_allocate_coords (self, &box);

  if (x1) *x1 = box.x1;
  if (y1) *y1 = box.y1;
  if (x2) *x2 = box.x2;
  if (y2) *y2 = box.y2;
}

/**
 * clutter_element_set_position
 * @self: A #ClutterElement
 * @x: New left position of element in pixels.
 * @y: New top position of element in pixels.
 *
 * Sets the elements position in pixels relative to any
 * parent element. 
 */
void
clutter_element_set_position (ClutterElement *self,
			      gint            x,
			      gint            y)
{
  ClutterElementBox box;

  g_return_if_fail (CLUTTER_IS_ELEMENT (self));

  clutter_element_allocate_coords (self, &box);

  box.x2 += (x - box.x1);
  box.y2 += (y - box.y1);

  box.x1 = x;
  box.y1 = y;

  clutter_element_request_coords (self, &box);
}

/**
 * clutter_element_set_size
 * @self: A #ClutterElement
 * @width: New width of element in pixels 
 * @height: New height of element in pixels
 *
 * Sets the elements position in pixels relative to any
 * parent element. 
 */
void
clutter_element_set_size (ClutterElement *self,
			  gint            width,
			  gint            height)
{
  ClutterElementBox box;

  g_return_if_fail (CLUTTER_IS_ELEMENT (self));

  clutter_element_allocate_coords (self, &box);

  box.x2 = box.x1 + width;
  box.y2 = box.y1 + height;

  clutter_element_request_coords (self, &box);
}

/**
 * clutter_element_set_position
 * @self: A #ClutterElement
 * @x: Location to store x position if non NULL.
 * @y: Location to store y position if non NULL.
 *
 * Gets the absolute position of an element in pixels relative
 * to the stage.
 */
void
clutter_element_get_abs_position (ClutterElement *self,
				  gint           *x,
				  gint           *y)
{
  ClutterElementBox  box;
  ClutterElement    *parent;
  gint               px = 0, py = 0;
  
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));

  clutter_element_allocate_coords (self, &box);

  parent = self->priv->parent_element;

  /* FIXME: must be nicer way to get 0,0 for stage ? */
  if (parent && !CLUTTER_IS_STAGE (parent))
    clutter_element_get_abs_position (parent, &px, &py);

  if (x)
    *x = px + box.x1;
  
  if (y)
    *y = py + box.y1;
}

/**
 * clutter_element_get_width
 * @self: A #ClutterElement
 *
 * Retrieves the elements width.
 *
 * Return value: The element width in pixels
 **/
guint
clutter_element_get_width (ClutterElement *self)
{
  ClutterElementBox box;

  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), 0);
  
  clutter_element_allocate_coords (self, &box);

  return box.x2 - box.x1;
}

/**
 * clutter_element_get_height
 * @self: A #ClutterElement
 *
 * Retrieves the elements height.
 * 
 * Return value: The element height in pixels
 **/
guint
clutter_element_get_height (ClutterElement *self)
{
  ClutterElementBox box;

  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), 0);
  
  clutter_element_allocate_coords (self, &box);

  return box.y2 - box.y1;
}

/**
 * clutter_element_get_x
 * @self: A #ClutterElement
 *
 * Retrieves the elements x position relative to any parent.
 *
 * Return value: The element x position in pixels
 **/
gint
clutter_element_get_x (ClutterElement *self)
{
  ClutterElementBox box;
  
  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), 0);

  clutter_element_allocate_coords (self, &box);

  return box.x1;
}

/**
 * clutter_element_get_y:
 * @self: A #ClutterElement
 *
 * Retrieves the elements y position relative to any parent.
 *
 * Return value: The element y position in pixels
 **/
gint
clutter_element_get_y (ClutterElement *self)
{
  ClutterElementBox box;

  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), 0);

  clutter_element_allocate_coords (self, &box);

  return box.y1;
}

/**
 * clutter_element_set_opacity:
 * @self: A #ClutterElement
 * @opacity: New opacity value for element.
 *
 * Sets the elements opacity, with zero being completely transparent.
 */
void
clutter_element_set_opacity (ClutterElement *self,
			     guint8          opacity)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  self->priv->opacity = opacity;

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    clutter_element_queue_redraw (self);
}

/**
 * clutter_element_get_opacity:
 * @self: A #ClutterElement
 *
 * Retrieves the elements opacity.
 *
 * Return value: The element opacity value.
 */
guint8
clutter_element_get_opacity (ClutterElement *self)
{
  ClutterElement *parent;
  
  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), 0);

  parent = self->priv->parent_element;
  
  /* FIXME: need to factor in the actual elements opacity with parents */
  if (parent && clutter_element_get_opacity (parent) != 0xff)
    return clutter_element_get_opacity(parent);

  return self->priv->opacity;
}

/**
 * clutter_element_set_name:
 * @self: A #ClutterElement
 * @id: Textual tag to apply to element
 *
 * Sets a textual tag to the element.
 */
void
clutter_element_set_name (ClutterElement *self,
			  const gchar    *name)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  if (name || name[0] != '\0')
    {
      g_free (self->priv->name);
      
      self->priv->name = g_strdup(name);
    }
}

/**
 * clutter_element_get_name:
 * @self: A #ClutterElement
 *
 * Return value: pointer to textual tag for the element.  The
 *   returned string is owned by the element and should not
 *   be modified or freed.
 */
const gchar*
clutter_element_get_name (ClutterElement *self)
{
  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), NULL);
  
  return self->priv->name;
}

/**
 * clutter_element_get_id:
 * @self: A #ClutterElement
 *
 * FIXME
 * 
 * Return value: Globally unique value for object instance.
 */
guint32
clutter_element_get_id (ClutterElement *self)
{
  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), 0);
  
  return self->priv->id;
}

static void
depth_sorter_foreach (ClutterElement *element, gpointer user_data)
{
  ClutterElement *element_to_sort = CLUTTER_ELEMENT(user_data);
  gint            z_copy;

  z_copy = element->priv->z;

  if (element_to_sort->priv->z > element->priv->z) 
    {
      clutter_element_raise (element_to_sort, element);
      element->priv->z = z_copy;
    }
}

/**
 * clutter_element_set_depth:
 * @self: a #ClutterElement
 * @depth: FIXME
 *
 * FIXME
 */
void
clutter_element_set_depth (ClutterElement *self,
                           gint            depth)
{
  /* Sets Z value.*/
  self->priv->z = depth;

  if (self->priv->parent_element)
    {
      /* We need to resort the group stacking order as to
       * correctly render alpha values. 
       *
       * FIXME: This is sub optimal. maybe queue the the sort 
       *        before stacking  
      */
      clutter_group_sort_depth_order 
	(CLUTTER_GROUP(self->priv->parent_element));
    }
}

/**
 * clutter_element_get_depth:
 * @self: a #ClutterElement
 *
 * Retrieves the depth of @self.
 *
 * Return value: the depth of a #ClutterElement
 */
gint
clutter_element_get_depth (ClutterElement *self)
{
  g_return_val_if_fail (CLUTTER_IS_ELEMENT (self), -1);
  
  return self->priv->z;
}

/**
 * clutter_element_rotate_z:
 * @self: A #ClutterElement
 * @angle: Angle of rotation
 * @x:     X co-ord to rotate element around ( relative to element position )
 * @y:     Y co-ord to rotate element around ( relative to element position )
 *
 * Rotates element around the Z axis.
 */
void
clutter_element_rotate_z (ClutterElement          *self,
			  gfloat                   angle,
			  gint                     x,
			  gint                     y)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));

  self->priv->rzang = angle;
  self->priv->rzx   = x;
  self->priv->rzy   = y;

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    clutter_element_queue_redraw (self);
}

/**
 * clutter_element_rotate_x:
 * @self:  A #ClutterElement
 * @angle: Angle of rotation
 * @y:     Y co-ord to rotate element around ( relative to element position )
 * @z:     Z co-ord to rotate element around ( relative to element position )
 *
 * Rotates element around the X axis.
 */
void
clutter_element_rotate_x (ClutterElement          *self,
			  gfloat                   angle,
			  gint                     y,
			  gint                     z)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  self->priv->rxang = angle;
  self->priv->rxy   = y;
  self->priv->rxz   = z;

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    clutter_element_queue_redraw (self);
}

/**
 * clutter_element_rotate_y:
 * @self:  A #ClutterElement
 * @angle: Angle of rotation
 * @x:     X co-ord to rotate element around ( relative to element position )
 * @z:     Z co-ord to rotate element around ( relative to element position )
 *
 * Rotates element around the X axis.
 */
void
clutter_element_rotate_y (ClutterElement          *self,
			  gfloat                   angle,
			  gint                     x,
			  gint                     z)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  self->priv->ryang = angle;
  self->priv->ryx   = x;
  self->priv->ryz   = z;

  if (CLUTTER_ELEMENT_IS_VISIBLE (self))
    clutter_element_queue_redraw (self);
}

/**
 * clutter_element_mirror:
 * @self: a #ClutterElement
 * @transform: a #ClutterElementTransform
 *
 * FIXME
 */
void
clutter_element_mirror (ClutterElement          *self,
			ClutterElementTransform  transform)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  self->priv->mirror_transform = transform;
}

/**
 * clutter_element_set_clip:
 * @self: A #ClutterElement
 * @xoff: FIXME
 * @yoff: FIXME
 * @width: FIXME
 * @height: FIXME
 *
 * Sets clip area for @self.
 */
void
clutter_element_set_clip (ClutterElement *self,
			  gint            xoff, 
			  gint            yoff, 
			  gint            width, 
			  gint            height)
{
  ClutterGeometry *clip;
  
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  clip = &self->priv->clip;
  
  clip->x      = xoff;
  clip->y      = yoff;
  clip->width  = width;
  clip->height = height;

  self->priv->has_clip = TRUE;
} 

/**
 * clutter_element_remove_clip
 * @self: A #ClutterElement
 *
 * Removes clip area from @self.
 */
void
clutter_element_remove_clip (ClutterElement *self)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  
  self->priv->has_clip = FALSE;
} 

/**
 * clutter_element_set_parent:
 * @self: A #ClutterElement
 * @parent: A new #ClutterElement parent or NULL
 *
 * This function should not be used by applications.
 */
void
clutter_element_set_parent (ClutterElement *self,
		            ClutterElement *parent)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT (self));
  g_return_if_fail ((parent == NULL) || CLUTTER_IS_ELEMENT (parent));

  if (self->priv->parent_element == parent)
    return;
  
  if (self->priv->parent_element && self->priv->parent_element != parent)
    g_object_unref (self->priv->parent_element);
  
  self->priv->parent_element = parent;

  if (self->priv->parent_element)
    g_object_ref (self->priv->parent_element);
}

/**
 * clutter_element_get_parent:
 * @self: A #ClutterElement
 *
 * Return Value: The #ClutterElement parent or NULL
 */
ClutterElement*
clutter_element_get_parent (ClutterElement *self)
{
  return self->priv->parent_element;
}

/**
 * clutter_element_raise:
 * @self: A #ClutterElement
 * @below: A #ClutterElement to raise above.
 *
 * Both elements must have the same parent.
 */
void
clutter_element_raise (ClutterElement *self, ClutterElement *below)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT(self));
  g_return_if_fail (clutter_element_get_parent (self) != NULL);

  clutter_group_raise (CLUTTER_GROUP(clutter_element_get_parent (self)),
		       self,
		       below);
}

/**
 * clutter_element_lower:
 * @self: A #ClutterElement
 * @above: A #ClutterElement to lower below
 *
 * Both elements must have the same parent.
 */
void
clutter_element_lower (ClutterElement *self, ClutterElement *above)
{
  g_return_if_fail (CLUTTER_IS_ELEMENT(self));
  g_return_if_fail (clutter_element_get_parent (self) != NULL);

  /* FIXME: fix Z ordering ? */
  if (above != NULL)
    {
      g_return_if_fail 
	(clutter_element_get_parent (self) 
	   != clutter_element_get_parent (above));
    }

  /* FIXME: group_lower should be an overidable method ? */
  clutter_group_lower (CLUTTER_GROUP(clutter_element_get_parent (self)),
		       self,
		       above);
}

/**
 * clutter_element_rise_top:
 * @self: A #ClutterElement
 *
 * Rises @self to the top.
 */
void
clutter_element_raise_top (ClutterElement *self)
{
  clutter_element_raise (self, NULL);
}

/**
 * clutter_element_lower_bottom:
 * @self: A #ClutterElement
 *
 * Lowers @self to the bottom.
 */
void
clutter_element_lower_bottom (ClutterElement *self)
{
  clutter_element_lower (self, NULL);
}

/*
 * ClutterGemoetry
 */

static ClutterGeometry*
clutter_geometry_copy (const ClutterGeometry *geometry)
{
  ClutterGeometry *result = g_new (ClutterGeometry, 1);

  *result = *geometry;

  return result;
}

GType
clutter_geometry_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static (
              g_intern_static_string ("ClutterGeometry"),
	      (GBoxedCopyFunc) clutter_geometry_copy,
	      (GBoxedFreeFunc) g_free);

  return our_type;
}

/*
 * ClutterElementBox
 */
static ClutterElementBox *
clutter_element_box_copy (const ClutterElementBox *box)
{
  ClutterElementBox *result = g_new (ClutterElementBox, 1);

  *result = *box;

  return result;
}

GType
clutter_element_box_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static (
              g_intern_static_string ("ClutterElementBox"),
	      (GBoxedCopyFunc) clutter_element_box_copy,
	      (GBoxedFreeFunc) g_free);
  return our_type;
}
