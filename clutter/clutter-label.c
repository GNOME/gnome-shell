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
 * SECTION:clutter-label
 * @short_description: Actor for displaying text
 *
 * #ClutterLabel is a #ClutterTexture that displays text.
 */

#include "config.h"

#include "clutter-label.h"
#include "clutter-layout.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-units.h"

#include "pangoclutter.h"

#define DEFAULT_FONT_NAME	"Sans 10"

static void clutter_layout_iface_init (ClutterLayoutIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterLabel,
                         clutter_label,
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_LAYOUT,
                                                clutter_layout_iface_init));

/* Probably move into main */
static PangoClutterFontMap  *_font_map = NULL;
static PangoContext         *_context  = NULL;

enum
{
  PROP_0,
  PROP_FONT_NAME,
  PROP_TEXT,
  PROP_COLOR,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_ALIGNMENT, 		/* FIXME */
  PROP_WRAP,
  PROP_WRAP_MODE,
  PROP_ELLIPSIZE,
  PROP_LAYOUT_FLAGS,
};

#define CLUTTER_LABEL_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_LABEL, ClutterLabelPrivate))

struct _ClutterLabelPrivate
{
  PangoContext         *context;
  PangoFontDescription *desc;
  
  ClutterColor          fgcol;

  ClutterLayoutFlags    layout_flags;
  
  gchar                *text;
  gchar                *font_name;
  
  gint                  extents_width;
  gint                  extents_height;

  guint                 alignment        : 2;
  guint                 wrap             : 1;
  guint                 use_underline    : 1;
  guint                 use_markup       : 1;
  guint                 ellipsize        : 3;
  guint                 single_line_mode : 1;
  guint                 wrap_mode        : 3;

  PangoAttrList        *attrs;
  PangoAttrList        *effective_attrs;
  PangoLayout          *layout;
  gint                  width_chars;
};


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
    case PROP_ATTRIBUTES:
      clutter_label_set_attributes (label, g_value_get_boxed (value));
      break;
    case PROP_ALIGNMENT:
      clutter_label_set_alignment (label, g_value_get_enum (value));
      break;
    case PROP_USE_MARKUP:
      clutter_label_set_use_markup (label, g_value_get_boolean (value));
      break;
    case PROP_WRAP:
      clutter_label_set_line_wrap (label, g_value_get_boolean (value));
      break;	  
    case PROP_WRAP_MODE:
      clutter_label_set_line_wrap_mode (label, g_value_get_enum (value));
      break;	  
    case PROP_ELLIPSIZE:
      clutter_label_set_ellipsize (label, g_value_get_enum (value));
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
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, priv->attrs);
      break;
    case PROP_ALIGNMENT:
      g_value_set_enum (value, priv->alignment);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, priv->use_markup);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, priv->wrap);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, priv->wrap_mode);
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, priv->ellipsize);
      break;
    case PROP_LAYOUT_FLAGS:
      g_value_set_flags (value, priv->layout_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
clutter_label_ensure_layout (ClutterLabel *label,
                             gint          width)
{
  ClutterLabelPrivate  *priv;

  priv   = label->priv;

  if (!priv->layout)
    {
      priv->layout = pango_layout_new (_context);

      if (priv->effective_attrs)
	pango_layout_set_attributes (priv->layout, priv->effective_attrs);
  
      pango_layout_set_alignment (priv->layout, priv->alignment);
      pango_layout_set_ellipsize (priv->layout, priv->ellipsize);
      pango_layout_set_single_paragraph_mode (priv->layout, 
					      priv->single_line_mode);
      
      pango_layout_set_font_description (priv->layout, priv->desc);
      
      if (!priv->use_markup)
        pango_layout_set_text (priv->layout, priv->text, -1);
      else
        pango_layout_set_markup (priv->layout, priv->text, -1);
      
      if (priv->wrap)
	pango_layout_set_wrap  (priv->layout, priv->wrap_mode);

      if ((priv->ellipsize || priv->wrap) && width > 0)
	{
	  pango_layout_set_width (priv->layout, width * PANGO_SCALE);
	}
      else
	pango_layout_set_width (priv->layout, -1);
    }
}

static void
clutter_label_clear_layout (ClutterLabel *label)
{
  if (label->priv->layout)
    {
      g_object_unref (label->priv->layout);
      label->priv->layout = NULL;
    }
}

void
clutter_label_paint (ClutterActor *self)
{
  ClutterLabel         *label;
  ClutterLabelPrivate  *priv;

  label  = CLUTTER_LABEL(self);
  priv   = label->priv;
  
  if (priv->desc == NULL || priv->text == NULL)
    {
      CLUTTER_NOTE (ACTOR, "layout: %p , desc: %p, text %p",
		    priv->layout,
		    priv->desc,
		    priv->text);
      return;
    }

  clutter_label_ensure_layout (label, clutter_actor_get_width (self));

  priv->fgcol.alpha = clutter_actor_get_opacity(self);

  pango_clutter_render_layout (priv->layout, 0, 0, &priv->fgcol, 0);
}

static void
clutter_label_query_coords (ClutterActor        *self,
			    ClutterActorBox     *box)
{
  ClutterLabel         *label = CLUTTER_LABEL(self);
  ClutterLabelPrivate  *priv;  
  PangoRectangle        logical_rect;

  priv = label->priv;

  clutter_label_ensure_layout (label, CLUTTER_UNITS_TO_INT (box->x2 - box->x1));

  pango_layout_get_extents (priv->layout, NULL, &logical_rect);

  box->x2 = box->x1 + CLUTTER_UNITS_FROM_PANGO_UNIT (logical_rect.width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_PANGO_UNIT (logical_rect.height);

  return;
}

static void
clutter_label_request_coords (ClutterActor        *self,
			      ClutterActorBox     *box)
{
  /* do we need to do anything ? */
  clutter_label_clear_layout (CLUTTER_LABEL (self));
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

static ClutterLayoutFlags
clutter_layout_label_get_layout_flags (ClutterLayout *layout)
{
  return CLUTTER_LABEL (layout)->priv->layout_flags;
}

static void
clutter_layout_label_height_for_width (ClutterLayout *layout,
                                       ClutterUnit    width,
                                       ClutterUnit   *height)
{
  ClutterLabel *label = CLUTTER_LABEL (layout);
  gint layout_height;

  clutter_label_ensure_layout (label, CLUTTER_UNITS_TO_INT (width));
  pango_layout_get_pixel_size (label->priv->layout, NULL, &layout_height);

  if (height)
    *height = CLUTTER_UNITS_FROM_INT (layout_height);
}

static void
clutter_layout_iface_init (ClutterLayoutIface *iface)
{
  iface->get_layout_flags = clutter_layout_label_get_layout_flags;
  iface->height_for_width = clutter_layout_label_height_for_width;
}

static void
clutter_label_class_init (ClutterLabelClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint           = clutter_label_paint;
  actor_class->request_coords  = clutter_label_request_coords;
  actor_class->query_coords    = clutter_label_query_coords;

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
			  G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_TEXT,
     g_param_spec_string ("text",
			  "Text",
			  "Text to render",
			  NULL,
			  G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
			 "Font Colour",
			 "Font Colour",
			 CLUTTER_TYPE_COLOR,
			 CLUTTER_PARAM_READWRITE));

  g_object_class_install_property 
    (gobject_class, PROP_ATTRIBUTES,
     g_param_spec_boxed ("attributes",
			 "Attributes",
			 "A list of style attributes to apply to the" 
			 "text of the label",
			 PANGO_TYPE_ATTR_LIST,
			 CLUTTER_PARAM_READWRITE));

  g_object_class_install_property 
    (gobject_class, PROP_USE_MARKUP,
     g_param_spec_boolean ("use-markup",
			   "Use markup",
			   "The text of the label includes XML markup." 
			   "See pango_parse_markup()",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

  g_object_class_install_property 
    (gobject_class, PROP_WRAP,
     g_param_spec_boolean ("wrap",
			   "Line wrap",
			   "If set, wrap lines if the text becomes too wide",
			   TRUE,
			   CLUTTER_PARAM_READWRITE));

  g_object_class_install_property 
    (gobject_class, PROP_WRAP_MODE,
     g_param_spec_enum ("wrap-mode",
			"Line wrap mode",
			"If wrap is set, controls how linewrapping is done",
			PANGO_TYPE_WRAP_MODE,
			PANGO_WRAP_WORD,
			CLUTTER_PARAM_READWRITE));

  g_object_class_install_property 
    (gobject_class, PROP_ELLIPSIZE,
     g_param_spec_enum ( "ellipsize",
			 "Ellipsize",
			 "The preferred place to ellipsize the string,"
			 "if the label does not have enough room to "
			 "display the entire string",
			 PANGO_TYPE_ELLIPSIZE_MODE,
			 PANGO_ELLIPSIZE_NONE,
			 CLUTTER_PARAM_READWRITE));

  g_object_class_install_property 
    (gobject_class, PROP_ALIGNMENT,
     g_param_spec_enum ( "alignment",
			 "Alignment",
			 "The preferred alignment for the string,",
			 PANGO_TYPE_ALIGNMENT,
			 PANGO_ALIGN_LEFT,
			 CLUTTER_PARAM_READWRITE));

  g_object_class_override_property (gobject_class,
                                    PROP_LAYOUT_FLAGS,
                                    "layout-flags");

  g_type_class_add_private (gobject_class, sizeof (ClutterLabelPrivate));
}

static void
clutter_label_init (ClutterLabel *self)
{
  ClutterLabelPrivate *priv;

  self->priv = priv = CLUTTER_LABEL_GET_PRIVATE (self);

  if (_context == NULL)
    {
      _font_map = PANGO_CLUTTER_FONT_MAP (pango_clutter_font_map_new ());
      /* pango_clutter_font_map_set_resolution (font_map, 96.0, 96.0); */
      _context = pango_clutter_font_map_create_context (_font_map);
    }

  priv->alignment     = PANGO_ALIGN_LEFT;
  priv->wrap          = TRUE;
  priv->wrap_mode     = PANGO_WRAP_WORD;
  priv->ellipsize     = PANGO_ELLIPSIZE_NONE;
  priv->use_underline = FALSE;
  priv->use_markup    = FALSE;
  priv->layout        = NULL;
  priv->text          = NULL;
  priv->attrs         = NULL;
  priv->layout_flags  = CLUTTER_LAYOUT_HEIGHT_FOR_WIDTH;

  priv->fgcol.red     = 0;
  priv->fgcol.green   = 0;
  priv->fgcol.blue    = 0;
  priv->fgcol.alpha   = 255;

  priv->font_name     = g_strdup (DEFAULT_FONT_NAME);
  priv->desc          = pango_font_description_from_string (priv->font_name);

  CLUTTER_MARK();
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
ClutterActor*
clutter_label_new_with_text (const gchar *font_name,
		             const gchar *text)
{
  ClutterActor *label;

  CLUTTER_MARK();

  label = clutter_label_new ();

  clutter_label_set_font_name (CLUTTER_LABEL(label), font_name);
  clutter_label_set_text (CLUTTER_LABEL(label), text);

  return label;
}

/**
 * clutter_label_new_full:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 * @color: #ClutterColor for text
 *
 * Creates a new #ClutterLabel displaying @text with color @color 
 * using @font_name.
 *
 * Return value: a #ClutterLabel
 */
ClutterActor*
clutter_label_new_full (const gchar  *font_name,
			const gchar  *text,
			ClutterColor *color)
{
  /* FIXME: really new_with_text should take color argument... */
  ClutterActor *label;

  label = clutter_label_new_with_text (font_name, text);
  clutter_label_set_color (CLUTTER_LABEL(label), color);

  return label;
}

/**
 * clutter_label_new:
 *
 * Creates a new, empty #ClutterLabel.
 *
 * Returns: the newly created #ClutterLabel
 */
ClutterActor *
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
 * owned by #ClutterLabel and should not be modified or freed.
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

  g_object_ref (label);

  g_free (priv->text);
  priv->text = g_strdup (text);

  clutter_label_clear_layout (label);   

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(label)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(label));

  g_object_notify (G_OBJECT (label), "text");
  g_object_unref (label);
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
  ClutterLabelPrivate *priv;
  PangoFontDescription *desc;

  g_return_if_fail (CLUTTER_IS_LABEL (label));
  
  if (!font_name || font_name[0] == '\0')
    font_name = DEFAULT_FONT_NAME;

  priv = label->priv;

  if (strcmp (priv->font_name, font_name) == 0)
    return;

  desc = pango_font_description_from_string (font_name);
  if (!desc)
    {
      g_warning ("Attempting to create a PangoFontDescription for "
		 "font name `%s', but failed.",
		 font_name);
      return;
    }

  g_object_ref (label);

  g_free (priv->font_name);
  priv->font_name = g_strdup (font_name);
  
  if (priv->desc)
    pango_font_description_free (priv->desc);

  priv->desc = desc;

  if (label->priv->text && label->priv->text[0] != '\0')
    {
      clutter_label_clear_layout (label);      

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(label)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR(label));
    }
  
  g_object_notify (G_OBJECT (label), "font-name");
  g_object_unref (label);
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
  ClutterActor *actor;
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));
  g_return_if_fail (color != NULL);

  priv = label->priv;

  g_object_ref (label);

  priv->fgcol.red = color->red;
  priv->fgcol.green = color->green;
  priv->fgcol.blue = color->blue;
  priv->fgcol.alpha = color->alpha;

  actor = CLUTTER_ACTOR (label);

  clutter_actor_set_opacity (actor, priv->fgcol.alpha);

  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
    clutter_actor_queue_redraw (actor);

  g_object_notify (G_OBJECT (label), "color");
  g_object_unref (label);
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

/**
 * clutter_label_set_ellipsize:
 * @label: a #ClutterLabel
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") to the text 
 * if there is not enough space to render the entire string.
 *
 * Since: 0.2
 **/
void
clutter_label_set_ellipsize (ClutterLabel          *label,
			     PangoEllipsizeMode     mode)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE &&
		    mode <= PANGO_ELLIPSIZE_END);
  
  priv = label->priv;

  if ((PangoEllipsizeMode) priv->ellipsize != mode)
    {
      g_object_ref (label);

      priv->ellipsize = mode;

      clutter_label_clear_layout (label);      
      
      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(label)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR(label));

      g_object_notify (G_OBJECT (label), "ellipsize");
      g_object_unref (label);
    }
}

/**
 * clutter_label_get_ellipsize:
 * @label: a #ClutterLabel
 *
 * Returns the ellipsizing position of the label. 
 * See clutter_label_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 *
 * Since: 0.2
 **/
PangoEllipsizeMode
clutter_label_get_ellipsize (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), PANGO_ELLIPSIZE_NONE);

  return label->priv->ellipsize;
}

/**
 * clutter_label_set_line_wrap:
 * @label: a #ClutterLabel
 * @wrap: the setting
 *
 * Toggles line wrapping within the #ClutterLabel widget.  %TRUE makes
 * it break lines if text exceeds the widget's size.  %FALSE lets the
 * text get cut off by the edge of the widget if it exceeds the widget
 * size.
 *
 * Since: 0.2
 */
void
clutter_label_set_line_wrap (ClutterLabel *label,
			     gboolean      wrap)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;
  
  wrap = wrap != FALSE;
  
  if (priv->wrap != wrap)
    {
      g_object_ref (label);

      priv->wrap = wrap;

      clutter_label_clear_layout (label);            

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (label)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "wrap");
      g_object_unref (label);
    }
}

/**
 * clutter_label_get_line_wrap:
 * @label: a #ClutterLabel
 *
 * Returns whether lines in the label are automatically wrapped. 
 * See clutter_label_set_line_wrap ().
 *
 * Return value: %TRUE if the lines of the label are automatically wrapped.
 *
 * Since: 0.2
 */
gboolean
clutter_label_get_line_wrap (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), FALSE);

  return label->priv->wrap;
}

/**
 * clutter_label_set_line_wrap_mode:
 * @label: a #ClutterLabel
 * @wrap_mode: the line wrapping mode
 *
 * If line wrapping is on (see clutter_label_set_line_wrap()) this controls how
 * the line wrapping is done. The default is %PANGO_WRAP_WORD which means
 * wrap on word boundaries.
 *
 * Since: 0.2
 **/
void
clutter_label_set_line_wrap_mode (ClutterLabel *label,
				  PangoWrapMode wrap_mode)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;
  
  if (priv->wrap_mode != wrap_mode)
    {
      g_object_ref (label);

      priv->wrap_mode = wrap_mode;

      clutter_label_clear_layout (label);      

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (label)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "wrap-mode");
      g_object_unref (label);
    }
}

/**
 * clutter_label_get_line_wrap_mode:
 * @label: a #ClutterLabel
 *
 * Returns line wrap mode used by the label.
 * See clutter_label_set_line_wrap_mode ().
 *
 * Return value: %TRUE if the lines of the label are automatically wrapped.
 *
 * Since: 0.2
 */
PangoWrapMode
clutter_label_get_line_wrap_mode (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), FALSE);

  return label->priv->wrap_mode;
}

/**
 * clutter_label_get_layout:
 * @label: a #ClutterLabel
 * 
 * Gets the #PangoLayout used to display the label.
 * The layout is useful to e.g. convert text positions to
 * pixel positions.
 * The returned layout is owned by the label so need not be
 * freed by the caller.
 * 
 * Return value: the #PangoLayout for this label
 *
 * Since: 0.2
 **/
PangoLayout*
clutter_label_get_layout (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), NULL);

  clutter_label_ensure_layout (label, -1);

  return label->priv->layout;
}

static inline void
clutter_label_set_attributes_internal (ClutterLabel  *label,
				       PangoAttrList *attrs)
{
  ClutterLabelPrivate *priv;

  priv = label->priv;

  g_object_ref (label);

  if (attrs)
    pango_attr_list_ref (attrs);
  
  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);

  if (!priv->use_markup)
    {
      if (attrs)
	pango_attr_list_ref (attrs);
      if (priv->effective_attrs)
	pango_attr_list_unref (priv->effective_attrs);
      priv->effective_attrs = attrs;
    }

  label->priv->attrs = attrs;
  g_object_notify (G_OBJECT (label), "attributes");
  g_object_unref (label);
}

/**
 * clutter_label_set_attributes:
 * @label: a #ClutterLabel
 * @attrs: a #PangoAttrList
 * 
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * label text. The attributes set with this function will be ignored
 * if the "use_markup" property
 * is %TRUE.
 *
 * Since: 0.2
 **/
void
clutter_label_set_attributes (ClutterLabel     *label,
			      PangoAttrList    *attrs)
{
  g_return_if_fail (CLUTTER_IS_LABEL (label));

  clutter_label_set_attributes_internal (label, attrs);

  clutter_label_clear_layout (label);  

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(label)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(label));
}

/**
 * clutter_label_get_attributes:
 * @label: a #ClutterLabel
 *
 * Gets the attribute list that was set on the label using
 * clutter_label_set_attributes(), if any.
 *
 * Return value: the attribute list, or %NULL if none was set.
 *
 * Since: 0.2
 **/
PangoAttrList *
clutter_label_get_attributes (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), NULL);

  return label->priv->attrs;
}

/**
 * clutter_label_set_use_markup:
 * @label: a #ClutterLabel
 * @setting: %TRUE if the label's text should be parsed for markup.
 *
 * Sets whether the text of the label contains markup in <link
 * linkend="PangoMarkupFormat">Pango's text markup
 * language</link>.
 **/
void
clutter_label_set_use_markup (ClutterLabel *label,
			      gboolean  setting)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;

  if (priv->use_markup != setting)
    {
      g_object_ref (label);

      priv->use_markup = setting;
      clutter_label_clear_layout (label);

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (label)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "use-markup");
      g_object_unref (label);
    }
}

/**
 * clutter_label_get_use_markup:
 * @label: a #ClutterLabel
 *
 * Returns whether the label's text is interpreted as marked up with
 * the <link linkend="PangoMarkupFormat">Pango text markup
 * language</link>. See clutter_label_set_use_markup ().
 *
 * Return value: %TRUE if the label's text will be parsed for markup.
 **/
gboolean
clutter_label_get_use_markup (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), FALSE);
  
  return label->priv->use_markup;
}

/**
 * clutter_label_set_alignment:
 * @label: a #ClutterLabel
 * @alignment: A #PangoAlignment
 *
 * Sets text alignment of the label.
 **/
void
clutter_label_set_alignment (ClutterLabel   *label,
			     PangoAlignment  alignment)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;

  if (priv->alignment != alignment)
    {
      g_object_ref (label);

      priv->alignment = alignment;
      clutter_label_clear_layout (label);

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (label)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "alignment");
      g_object_unref (label);
    }
}

/**
 * clutter_label_get_alignment:
 * @label: a #ClutterLabel
 *
 * Returns the label's text alignment
 *
 * Return value: The labels #PangoAlignment
 *
 * Since 0.2
 **/
PangoAlignment
clutter_label_get_alignment (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), FALSE);
  
  return label->priv->alignment;
}

