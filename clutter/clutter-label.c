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

#include "config.h"

#include "clutter-label.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */

#include <pango/pangoft2.h>

#define DEFAULT_FONT_NAME	"Sans 10"

G_DEFINE_TYPE (ClutterLabel, clutter_label, CLUTTER_TYPE_TEXTURE);

enum
{
  PROP_0,
  PROP_FONT_NAME,
  PROP_TEXT,
  PROP_COLOR
};

#define CLUTTER_LABEL_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_LABEL, ClutterLabelPrivate))

struct _ClutterLabelPrivate
{
  PangoLayout          *layout;
  PangoContext         *context;
  PangoFontDescription *desc;
  
  ClutterColor          fgcol;
  
  gchar                *text;
  gchar                *font_name;
  
  gint                  extents_width;
  gint                  extents_height;
};

static void
clutter_label_make_pixbuf (ClutterLabel *label)
{
  gint                 bx, by, w, h;
  FT_Bitmap            ft_bitmap;
  guint8 const         *ps;
  guint8               *pd;
  ClutterLabelPrivate  *priv;
  ClutterTexture       *texture;
  GdkPixbuf            *pixbuf;
  
  priv  = label->priv;

  texture = CLUTTER_TEXTURE(label);

  if (priv->layout == NULL || priv->desc == NULL || priv->text == NULL)
    {
      CLUTTER_DBG("*** FAIL: layout: %p , desc: %p, text %p ***",
		  priv->layout, priv->desc, priv->text);
      return;
    }

  pango_layout_set_font_description (priv->layout, priv->desc);
  pango_layout_set_text (priv->layout, priv->text, -1);

  if (priv->extents_width != 0)
    {
      CLUTTER_DBG("forcing width to '%i'", priv->extents_width);
      pango_layout_set_width (priv->layout, PANGO_SCALE * priv->extents_width);
      pango_layout_set_wrap  (priv->layout, PANGO_WRAP_WORD);
    }

  pango_layout_get_pixel_size (priv->layout, 
			       &w, 
			       &h);

  if (w == 0 || h == 0)
    return;

  ft_bitmap.rows         = h;
  ft_bitmap.width        = w;
  ft_bitmap.pitch        = (w+3) & ~3;
  ft_bitmap.buffer       = g_malloc0 (ft_bitmap.rows * ft_bitmap.pitch);
  ft_bitmap.num_grays    = 256;
  ft_bitmap.pixel_mode   = ft_pixel_mode_grays;
  ft_bitmap.palette_mode = 0;
  ft_bitmap.palette      = NULL;

  pango_ft2_render_layout (&ft_bitmap, priv->layout, 0, 0);

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
			   TRUE, 
			   8,
			   ft_bitmap.width, 
			   ft_bitmap.rows);

  for (by = 0; by < ft_bitmap.rows; by++) 
    {
      pd = gdk_pixbuf_get_pixels (pixbuf)
	+ by * gdk_pixbuf_get_rowstride (pixbuf);
      ps = ft_bitmap.buffer + by * ft_bitmap.pitch;

      for (bx = 0; bx < ft_bitmap.width; bx++) 
	{
	  *pd++ = priv->fgcol.red;
	  *pd++ = priv->fgcol.green;
	  *pd++ = priv->fgcol.blue;
	  *pd++ = *ps++;
	}
    }

  g_free (ft_bitmap.buffer);

  CLUTTER_DBG("Calling set_pixbuf with text : '%s' , pixb %ix%i", 
	      priv->text, w, h);
  clutter_texture_set_pixbuf (CLUTTER_TEXTURE (label), pixbuf);
  
  /* Texture has the ref now */
  g_object_unref (pixbuf); 
}

static void
clutter_label_set_property (GObject      *object, 
			    guint         prop_id,
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  ClutterLabel        *label;
  ClutterLabelPrivate *priv;

  label = CLUTTER_LABEL(object);
  priv = label->priv;

  switch (prop_id) 
    {
    case PROP_FONT_NAME:
      clutter_label_set_font_name (label, g_value_get_string (value));
      break;
    case PROP_TEXT:
      clutter_label_set_text (label, g_value_get_string (value));
      break;
    case PROP_COLOR:
      clutter_label_set_color (label, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_label_get_property (GObject    *object, 
			    guint       prop_id,
			    GValue     *value, 
			    GParamSpec *pspec)
{
  ClutterLabel        *label;
  ClutterLabelPrivate *priv;
  ClutterColor         color;

  label = CLUTTER_LABEL(object);
  priv = label->priv;

  switch (prop_id) 
    {
    case PROP_FONT_NAME:
      g_value_set_string (value, priv->font_name);
      break;
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_COLOR:
      clutter_label_get_color (label, &color);
      g_value_set_boxed (value, &color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}


static void 
clutter_label_dispose (GObject *object)
{
  ClutterLabel         *self = CLUTTER_LABEL(object);
  ClutterLabelPrivate  *priv;  

  priv = self->priv;
  
  if (priv->layout)
    {
      g_object_unref (priv->layout);
      priv->layout = NULL;
    }
  
  if (priv->desc)
    {
      pango_font_description_free (priv->desc);    
      priv->desc = NULL;
    }

  g_free (priv->text);
  priv->text = NULL;

  g_free (priv->font_name);
  priv->font_name = NULL;
      
  if (priv->context)
    {
      g_object_unref (priv->context);
      priv->context = NULL;
    }

  G_OBJECT_CLASS (clutter_label_parent_class)->dispose (object);
}

static void 
clutter_label_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_label_parent_class)->finalize (object);
}

static void
clutter_label_class_init (ClutterLabelClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  ClutterElementClass *element_class = CLUTTER_ELEMENT_CLASS (klass);
  ClutterElementClass *parent_class = CLUTTER_ELEMENT_CLASS (clutter_label_parent_class);

  element_class->paint      = parent_class->paint;
  element_class->realize    = parent_class->realize;
  element_class->unrealize  = parent_class->unrealize;
  element_class->show       = parent_class->show;
  element_class->hide       = parent_class->hide;

  gobject_class->finalize   = clutter_label_finalize;
  gobject_class->dispose    = clutter_label_dispose;
  gobject_class->set_property = clutter_label_set_property;
  gobject_class->get_property = clutter_label_get_property;

  g_object_class_install_property
    (gobject_class, PROP_FONT_NAME,
     g_param_spec_string ("font-name",
			  "Font Name",
			  "Pango font description",
			  NULL,
			  G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_TEXT,
     g_param_spec_string ("text",
			  "Text",
			  "Text to render",
			  NULL,
			  G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
			 "Font Colour",
			 "Font Colour",
			 CLUTTER_TYPE_COLOR,
			 G_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (ClutterLabelPrivate));
}

static void
clutter_label_init (ClutterLabel *self)
{
  ClutterLabelPrivate *priv;
  PangoFT2FontMap     *font_map;

  self->priv = priv = CLUTTER_LABEL_GET_PRIVATE (self);

  priv->fgcol.red = 255;
  priv->fgcol.green = 255;
  priv->fgcol.blue = 255;
  priv->fgcol.alpha = 255;

  priv->text = NULL;
  priv->font_name = g_strdup (DEFAULT_FONT_NAME);
  priv->desc = pango_font_description_from_string (priv->font_name);
  
  font_map = PANGO_FT2_FONT_MAP (pango_ft2_font_map_new ());
  pango_ft2_font_map_set_resolution (font_map, 96.0, 96.0);
  priv->context = pango_ft2_font_map_create_context (font_map);

  priv->layout  = pango_layout_new (priv->context);

  /* See http://bugzilla.gnome.org/show_bug.cgi?id=143542  ?? 
  pango_ft2_font_map_substitute_changed (font_map);
  g_object_unref (font_map);
  */
}

/**
 * clutter_label_new_with_text:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 *
 * Creates a new #ClutterLabel displaying @text using @font_name.
 *
 * Return value: a #ClutterLabel
 */
ClutterElement *
clutter_label_new_with_text (const gchar *font_name,
		             const gchar *text)
{
  return g_object_new (CLUTTER_TYPE_LABEL, 
		       "font-name", font_name,
		       "text", text,
		       NULL);
}

/**
 * clutter_label_new:
 *
 * Creates a new, empty #ClutterLabel.
 *
 * Return: the newly created #ClutterLabel
 */
ClutterElement *
clutter_label_new (void)
{
  return g_object_new (CLUTTER_TYPE_LABEL, NULL);
}

/**
 * clutter_label_get_text:
 * @label: a #ClutterLabel
 *
 * Retrieves the text displayed by @label
 *
 * Return value: the text of the label.  The returned string is
 *   owned by #ClutterLabel and should not be modified or freed.
 */
G_CONST_RETURN gchar *
clutter_label_get_text (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), NULL);

  return label->priv->text;
}

/**
 * clutter_label_set_text:
 * @label: a #ClutterLabel
 * @text: the text to be displayed
 *
 * Sets @text as the text to be displayed by @label.
 */
void
clutter_label_set_text (ClutterLabel *label,
		        const gchar  *text)
{
  ClutterLabelPrivate  *priv;  

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;
  
  g_free (priv->text);
  priv->text = g_strdup (text);

  clutter_label_make_pixbuf (label);

  if (CLUTTER_ELEMENT_IS_VISIBLE (CLUTTER_ELEMENT(label)))
    clutter_element_queue_redraw (CLUTTER_ELEMENT(label));

  g_object_notify (G_OBJECT (label), "text");
}

/**
 * clutter_label_get_font_name:
 * @label: a #ClutterLabel
 *
 * Retrieves the font used by @label.
 *
 * Return value: a string containing the font name, in a format
 *   understandable by pango_font_description_from_string().  The
 *   string is owned by #ClutterLabel and should not be modified
 *   or freed.
 */
G_CONST_RETURN gchar *
clutter_label_get_font_name (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), NULL);
  
  return label->priv->font_name;
}

/**
 * clutter_label_set_font_name:
 * @label: a #ClutterLabel
 * @font_name: a font name and size, or %NULL for the default font
 *
 * Sets @font_name as the font used by @label.
 *
 * @font_name must be a string containing the font name and its
 * size, similarly to what you would feed to the
 * pango_font_description_from_string() function.
 */
void
clutter_label_set_font_name (ClutterLabel *label,
		             const gchar  *font_name)
{
  ClutterLabelPrivate  *priv;  

  g_return_if_fail (CLUTTER_IS_LABEL (label));
  
  if (!font_name || font_name[0] == '\0')
    font_name = DEFAULT_FONT_NAME;

  priv = label->priv;

  if (priv->desc)
    pango_font_description_free (priv->desc);

  g_free (priv->font_name);
  priv->font_name = g_strdup (font_name);

  priv->desc = pango_font_description_from_string (priv->font_name);
  if (!priv->desc)
    {
      g_warning ("Attempting to create a PangoFontDescription for "
		 "font name `%s', but failed.",
		 priv->font_name);
      return;
    }

  if (label->priv->text && label->priv->text[0] != '\0')
    {
      clutter_label_make_pixbuf (label);

      if (CLUTTER_ELEMENT_IS_VISIBLE (CLUTTER_ELEMENT(label)))
	clutter_element_queue_redraw (CLUTTER_ELEMENT(label));
    }
  
  g_object_notify (G_OBJECT (label), "font-name");
}

/**
 * clutter_label_set_text_extents:
 * @label: a #ClutterLabel
 * @width: the width of the text
 * @height: the height of the text
 *
 * Sets the maximum extents of the label's text.
 */
void
clutter_label_set_text_extents (ClutterLabel *label, 
				gint          width,
				gint          height)
{
  /* FIXME: height extents is broken.... 
  */

  label->priv->extents_width = width;
  label->priv->extents_height = height;

  clutter_label_make_pixbuf (label);

  if (CLUTTER_ELEMENT_IS_VISIBLE (CLUTTER_ELEMENT(label)))
    clutter_element_queue_redraw (CLUTTER_ELEMENT(label));
}

/**
 * clutter_label_get_text_extents:
 * @label: a #ClutterLabel
 * @width: return location for the width of the extents or %NULL
 * @height: return location for the height of the extents or %NULL
 *
 * Returns the extents of the label.
 */
void
clutter_label_get_text_extents (ClutterLabel *label,
				gint         *width,
				gint         *height)
{
  g_return_if_fail (CLUTTER_IS_LABEL (label));

  if (width)
    *width = label->priv->extents_width;

  if (height)
    *height = label->priv->extents_height;
}

/**
 * clutter_label_set_color:
 * @label: a #ClutterLabel
 * @color: a #ClutterColor
 *
 * Sets the color of @label.
 */
void
clutter_label_set_color (ClutterLabel       *label,
		         const ClutterColor *color)
{
  ClutterElement *element;
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));
  g_return_if_fail (color != NULL);

  priv = label->priv;
  priv->fgcol.red = color->red;
  priv->fgcol.green = color->green;
  priv->fgcol.blue = color->blue;
  priv->fgcol.alpha = color->alpha;

  clutter_label_make_pixbuf (label);
  
  element = CLUTTER_ELEMENT (label);
  clutter_element_set_opacity (element, priv->fgcol.alpha);

  if (CLUTTER_ELEMENT_IS_VISIBLE (element))
    clutter_element_queue_redraw (element);

  g_object_notify (G_OBJECT (label), "color");
}

/**
 * clutter_label_get_color:
 * @label: a #ClutterLabel
 * @color: return location for a #ClutterColor
 *
 * Retrieves the color of @label.
 */
void
clutter_label_get_color (ClutterLabel *label,
			 ClutterColor *color)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));
  g_return_if_fail (color != NULL);

  priv = label->priv;

  color->red = priv->fgcol.red;
  color->green = priv->fgcol.green;
  color->blue = priv->fgcol.blue;
  color->alpha = priv->fgcol.alpha;
}
