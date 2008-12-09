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
 * #ClutterLabel is a #ClutterActor that displays text using Pango.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-label.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-units.h"

#include "cogl-pango.h"

#define DEFAULT_FONT_NAME	"Sans 10"

G_DEFINE_TYPE (ClutterLabel, clutter_label, CLUTTER_TYPE_ACTOR)

/* Probably move into main */
static PangoContext *_context = NULL;

static const ClutterColor default_text_color = { 0, 0, 0, 255 };

enum
{
  PROP_0,
  PROP_FONT_NAME,
  PROP_TEXT,
  PROP_COLOR,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_ALIGNMENT,
  PROP_WRAP,
  PROP_WRAP_MODE,
  PROP_JUSTIFY,
  PROP_ELLIPSIZE,
};

#define CLUTTER_LABEL_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_LABEL, ClutterLabelPrivate))

typedef struct _ClutterLabelCachedLayout ClutterLabelCachedLayout;

struct _ClutterLabelCachedLayout
{
  /* Cached layout. Pango internally caches the computed extents when
     they are requested so there is no need to cache that as well */
  PangoLayout *layout;
  /* The width that used to generate this layout */
  ClutterUnit  width;
  /* A number representing the age of this cache (so that when a new
     layout is needed the last used cache is replaced) */
  guint        age;
};

/* We need at least three cached layouts to run the allocation without
   regenerating a new layout. First the layout will be generated at
   full width to get the preferred width, then it will be generated at
   the preferred width to get the preferred height and then it might
   be regenerated at a different width to get the height for the
   actual allocated width */
#define CLUTTER_LABEL_N_CACHED_LAYOUTS 3

struct _ClutterLabelPrivate
{
  PangoFontDescription *font_desc;
  
  ClutterColor          fgcol;

  gchar                *text;
  gchar                *font_name;

  PangoAttrList        *attrs;
  PangoAttrList        *effective_attrs;

  ClutterLabelCachedLayout cached_layouts[CLUTTER_LABEL_N_CACHED_LAYOUTS];
  guint                 cache_age;
  
  guint                 alignment        : 2;
  guint                 wrap             : 1;
  guint                 use_underline    : 1;
  guint                 use_markup       : 1;
  guint                 ellipsize        : 3;
  guint                 single_line_mode : 1;
  guint                 wrap_mode        : 3;
  guint                 justify          : 1;
};

/*
 * clutter_label_create_layout_no_cache:
 * @label: a #ClutterLabel
 * @allocation_width: the width of the layout, or -1
 *
 * Creates a new #PangoLayout for the given @allocation_width, using
 * the layout properties of the @label.
 *
 * This function will not touch the glyphs cache.
 *
 * This function should be used by clutter_label_get_preferred_width()
 * and clutter_label_get_preferred_height().
 */
static PangoLayout *
clutter_label_create_layout_no_cache (ClutterLabel *label,
                                      ClutterUnit   allocation_width)
{
  ClutterLabelPrivate *priv = label->priv;
  PangoLayout *layout;

  layout = pango_layout_new (_context);

  if (priv->effective_attrs)
    pango_layout_set_attributes (layout, priv->effective_attrs);

  pango_layout_set_alignment (layout, priv->alignment);
  pango_layout_set_single_paragraph_mode (layout, priv->single_line_mode);

  pango_layout_set_font_description (layout, priv->font_desc);
  pango_layout_set_justify (layout, priv->justify);

  if (priv->text)
    {
      if (!priv->use_markup)
        pango_layout_set_text (layout, priv->text, -1);
      else
        pango_layout_set_markup (layout, priv->text, -1);
    }

  if (allocation_width > 0 &&
      (priv->ellipsize != PANGO_ELLIPSIZE_NONE || priv->wrap))
    {
      int layout_width, layout_height;

      pango_layout_get_size (layout, &layout_width, &layout_height);

      /* No need to set ellipsize or wrap if we already have enough
       * space, since we don't want to make the layout wider than it
       * would be otherwise.
       */

      if (CLUTTER_UNITS_FROM_PANGO_UNIT (layout_width) > allocation_width)
        {
          if (priv->ellipsize != PANGO_ELLIPSIZE_NONE)
            {
              gint width;

              width = allocation_width > 0
                ? CLUTTER_UNITS_TO_PANGO_UNIT (allocation_width)
                : -1;

              pango_layout_set_ellipsize (layout, priv->ellipsize);
              pango_layout_set_width (layout, width);
            }
          else if (priv->wrap)
            {
              gint width;

              width = allocation_width > 0
                ? CLUTTER_UNITS_TO_PANGO_UNIT (allocation_width)
                : -1;

              pango_layout_set_wrap (layout, priv->wrap_mode);
              pango_layout_set_width (layout, width);
            }
        }
    }

  return layout;
}

static void
clutter_label_dirty_cache (ClutterLabel *label)
{
  ClutterLabelPrivate *priv = label->priv;
  int i;

  /* Delete the cached layouts so they will be recreated the next time
     they are needed */
  for (i = 0; i < CLUTTER_LABEL_N_CACHED_LAYOUTS; i++)
    if (priv->cached_layouts[i].layout)
      {
	g_object_unref (priv->cached_layouts[i].layout);
	priv->cached_layouts[i].layout = NULL;
      }
}

/*
 * clutter_label_create_layout:
 * @label: a #ClutterLabel
 * @allocation_width: the allocation width
 *
 * Like clutter_label_create_layout_no_cache(), but will also ensure
 * the glyphs cache. If a previously cached layout generated using the
 * same width is available then that will be used instead of
 * generating a new one.
 */
static PangoLayout *
clutter_label_create_layout (ClutterLabel *label,
                             ClutterUnit   allocation_width)
{
  ClutterLabelPrivate *priv = label->priv;
  int i;
  ClutterLabelCachedLayout *oldest_cache = priv->cached_layouts;
  gboolean found_free_cache = FALSE;

  /* Search for a cached layout with the same width and keep track of
     the oldest one */
  for (i = 0; i < CLUTTER_LABEL_N_CACHED_LAYOUTS; i++)
    {
      if (priv->cached_layouts[i].layout == NULL)
	{
	  /* Always prefer free cache spaces */
	  found_free_cache = TRUE;
	  oldest_cache = priv->cached_layouts + i;
	}
      /* If this cached layout is using the same width then we can
	 just return that directly */
      else if (priv->cached_layouts[i].width == allocation_width)
	{
	  CLUTTER_NOTE (ACTOR, "ClutterLabel: %p: cache hit for width %i",
			label, CLUTTER_UNITS_TO_DEVICE (allocation_width));
	  return priv->cached_layouts[i].layout;
	}
      else if (!found_free_cache && (priv->cached_layouts[i].age
				     < oldest_cache->age))
	oldest_cache = priv->cached_layouts + i;
    }

  CLUTTER_NOTE (ACTOR, "ClutterLabel: %p: cache miss for width %i",
		label, CLUTTER_UNITS_TO_DEVICE (allocation_width));

  /* If we make it here then we didn't have a cached version so we
     need to recreate the layout */
  if (oldest_cache->layout)
    g_object_unref (oldest_cache->layout);

  oldest_cache->layout
    = clutter_label_create_layout_no_cache (label, allocation_width);

  cogl_pango_ensure_glyph_cache_for_layout (oldest_cache->layout);

  /* Mark the 'time' this cache was created and advance the time */
  oldest_cache->age = priv->cache_age++;

  oldest_cache->width = allocation_width;

  return oldest_cache->layout;
}

static void
clutter_label_paint (ClutterActor *self)
{
  ClutterLabel        *label = CLUTTER_LABEL (self);
  ClutterLabelPrivate *priv = label->priv;
  PangoLayout         *layout;
  ClutterActorBox alloc = { 0, };
  CoglColor color = { 0, };

  if (priv->font_desc == NULL || priv->text == NULL)
    {
      CLUTTER_NOTE (ACTOR, "desc: %p, text %p",
		    priv->font_desc ? priv->font_desc : 0x0,
		    priv->text ? priv->text : 0x0);
      return;
    }

  CLUTTER_NOTE (PAINT, "painting label (text:`%s')", priv->text);

  clutter_actor_get_allocation_box (self, &alloc);
  layout = clutter_label_create_layout (label, alloc.x2 - alloc.x1);

  cogl_color_set_from_4ub (&color,
                           priv->fgcol.red,
                           priv->fgcol.green,
                           priv->fgcol.blue,
                           clutter_actor_get_paint_opacity (self));

  cogl_pango_render_layout (layout, 0, 0, &color, 0);
}

static void
clutter_label_get_preferred_width (ClutterActor *self,
                                   ClutterUnit   for_height,
                                   ClutterUnit  *min_width_p,
                                   ClutterUnit  *natural_width_p)
{ 
  ClutterLabel *label = CLUTTER_LABEL (self);
  ClutterLabelPrivate *priv = label->priv;
  PangoRectangle logical_rect = { 0, };
  PangoLayout *layout;
  ClutterUnit layout_width;

  layout = clutter_label_create_layout (label, -1);

  pango_layout_get_extents (layout, NULL, &logical_rect);

  layout_width = logical_rect.width > 0
    ? CLUTTER_UNITS_FROM_PANGO_UNIT (logical_rect.width)
    : 1;

  if (min_width_p)
    {
      if (priv->wrap || priv->ellipsize)
        *min_width_p = 1;
      else
        *min_width_p = layout_width;
    }

  if (natural_width_p)
    *natural_width_p = layout_width; 
}

static void
clutter_label_get_preferred_height (ClutterActor *self,
                                    ClutterUnit   for_width,
                                    ClutterUnit  *min_height_p,
                                    ClutterUnit  *natural_height_p)
{
  ClutterLabel *label = CLUTTER_LABEL (self);

  if (for_width == 0)
    {
      if (min_height_p)
        *min_height_p = 0;

      if (natural_height_p)
        *natural_height_p = 0;
    }
  else
    {
      PangoLayout *layout;
      PangoRectangle logical_rect = { 0, };
      ClutterUnit height;

      layout = clutter_label_create_layout (label, for_width);

      pango_layout_get_extents (layout, NULL, &logical_rect);
      height = CLUTTER_UNITS_FROM_PANGO_UNIT (logical_rect.height);

      if (min_height_p)
        *min_height_p = height;

      if (natural_height_p)
        *natural_height_p = height;
    }
}

static void
clutter_label_allocate (ClutterActor          *self,
                        const ClutterActorBox *box,
                        gboolean               origin_changed)
{
  ClutterLabel *label = CLUTTER_LABEL (self);
  ClutterActorClass *parent_class;

  /* Ensure that there is a cached label with the right width so that
     we don't need to create the label during the paint run */
  clutter_label_create_layout (label, box->x2 - box->x1);

  parent_class = CLUTTER_ACTOR_CLASS (clutter_label_parent_class);
  parent_class->allocate (self, box, origin_changed);
}

static void 
clutter_label_dispose (GObject *object)
{
  ClutterLabel         *self = CLUTTER_LABEL(object);
  ClutterLabelPrivate  *priv;  

  priv = self->priv;

  /* Get rid of the cached layouts */
  clutter_label_dirty_cache (self);

  G_OBJECT_CLASS (clutter_label_parent_class)->dispose (object);
}

static void 
clutter_label_finalize (GObject *object)
{
  ClutterLabel         *self = CLUTTER_LABEL(object);
  ClutterLabelPrivate  *priv;  

  priv = self->priv;
  
  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  g_free (priv->text);
  g_free (priv->font_name);

  G_OBJECT_CLASS (clutter_label_parent_class)->finalize (object);
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
      clutter_label_set_color (label, clutter_value_get_color (value));
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
    case PROP_JUSTIFY:
      clutter_label_set_justify (label, g_value_get_boolean (value));
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

  label = CLUTTER_LABEL (object);
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
      clutter_value_set_color (value, &priv->fgcol);
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
    case PROP_JUSTIFY:
      g_value_set_boolean (value, priv->justify);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
clutter_label_class_init (ClutterLabelClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec        *pspec;

  actor_class->paint                = clutter_label_paint;
  actor_class->get_preferred_width  = clutter_label_get_preferred_width;
  actor_class->get_preferred_height = clutter_label_get_preferred_height;
  actor_class->allocate             = clutter_label_allocate;

  gobject_class->finalize     = clutter_label_finalize;
  gobject_class->dispose      = clutter_label_dispose;
  gobject_class->set_property = clutter_label_set_property;
  gobject_class->get_property = clutter_label_get_property;

  /**
   * ClutterLabel:font-name:
   *
   * The font to be used by the #ClutterLabel, as a string
   * that can be parsed by pango_font_description_from_string().
   *
   * Since: 0.2
   */
  pspec = g_param_spec_string ("font-name",
                               "Font Name",
                               "The font to be used by the label",
                               NULL,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_FONT_NAME, pspec);

  pspec = g_param_spec_string ("text",
                               "Text",
                               "The text to render",
                               NULL,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TEXT, pspec);

  pspec = clutter_param_spec_color ("color",
                                    "Font Color",
                                    "Color of the font used by the label",
                                    &default_text_color,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);

  pspec = g_param_spec_boxed ("attributes",
                              "Attributes",
                              "A list of style attributes to apply to "
                              "the text of the label",
                              PANGO_TYPE_ATTR_LIST,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ATTRIBUTES, pspec);

  /**
   * ClutterLabel:use-markup:
   *
   * Whether the text of the label includes Pango markup. See
   * pango_layout_set_markup() in the Pango documentation.
   *
   * Since: 0.2
   */
  pspec = g_param_spec_boolean ("use-markup",
                                "Use markup",
                                "Whether or not the text of the label "
                                "includes Pango markup",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_USE_MARKUP, pspec);

  /**
   * ClutterLabel:wrap:
   *
   * Whether to wrap the lines of #ClutterLabel:text if the contents
   * exceed the available allocation. The wrapping strategy is
   * controlled by the #ClutterLabel:wrap-mode property.
   *
   * Since: 0.2
   */
  pspec = g_param_spec_boolean ("wrap",
                                "Line wrap",
                                "If set, wrap the lines if the text "
                                "becomes too wide",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_WRAP, pspec);

  /**
   * ClutterLabel:wrap-mode:
   *
   * If #ClutterLabel:wrap is set to %TRUE, this property will
   * control how the text is wrapped.
   *
   * Since: 0.2
   */
  pspec = g_param_spec_enum ("wrap-mode",
                             "Line wrap mode",
                             "Control how line-wrapping is done",
                             PANGO_TYPE_WRAP_MODE,
                             PANGO_WRAP_WORD,
                             CLUTTER_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_WRAP_MODE, pspec);

  pspec = g_param_spec_enum ("ellipsize",
                             "Ellipsize",
                             "The preferred place to ellipsize the string, "
                             "if the label does not have enough room to "
                             "display the entire string",
                             PANGO_TYPE_ELLIPSIZE_MODE,
                             PANGO_ELLIPSIZE_NONE,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ELLIPSIZE, pspec);

  /**
   * ClutterLabel:alignment:
   *
   * The preferred alignment for the text. This property controls
   * the alignment of multi-line paragraphs.
   *
   * Since: 0.2
   */
  pspec = g_param_spec_enum ("alignment",
                             "Alignment",
                             "The preferred alignment for the string, "
                             "for multi-line text",
                             PANGO_TYPE_ALIGNMENT,
                             PANGO_ALIGN_LEFT,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ALIGNMENT, pspec);

  /**
   * ClutterLabel:justify:
   *
   * Whether the contents of the label should be justified on both
   * margins.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_boolean ("justify",
                                "Justify",
                                "Whether the contents of the label "
                                "should be justified",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_JUSTIFY, pspec);

  g_type_class_add_private (gobject_class, sizeof (ClutterLabelPrivate));
}

static void
clutter_label_init (ClutterLabel *self)
{
  ClutterLabelPrivate *priv;
  int i;

  self->priv = priv = CLUTTER_LABEL_GET_PRIVATE (self);

  if (G_UNLIKELY (_context == NULL))
    _context = _clutter_context_create_pango_context (CLUTTER_CONTEXT ());

  priv->alignment     = PANGO_ALIGN_LEFT;
  priv->wrap          = FALSE;
  priv->wrap_mode     = PANGO_WRAP_WORD;
  priv->ellipsize     = PANGO_ELLIPSIZE_NONE;
  priv->use_underline = FALSE;
  priv->use_markup    = FALSE;
  priv->justify       = FALSE;

  for (i = 0; i < CLUTTER_LABEL_N_CACHED_LAYOUTS; i++)
    priv->cached_layouts[i].layout = NULL;

  priv->text          = NULL;
  priv->attrs         = NULL;

  priv->fgcol         = default_text_color;

  priv->font_name     = g_strdup (DEFAULT_FONT_NAME);
  priv->font_desc     = pango_font_description_from_string (priv->font_name);
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
  return g_object_new (CLUTTER_TYPE_LABEL,
                       "font-name", font_name,
                       "text", text,
                       NULL);
}

/**
 * clutter_label_new_full:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 * @color: #ClutterColor for text
 *
 * Creates a new #ClutterLabel displaying @text with @color 
 * using @font_name.
 *
 * Return value: a #ClutterLabel
 */
ClutterActor*
clutter_label_new_full (const gchar        *font_name,
			const gchar        *text,
			const ClutterColor *color)
{
  return g_object_new (CLUTTER_TYPE_LABEL,
                       "font-name", font_name,
                       "text", text,
                       "color", color,
                       NULL);
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

  g_free (priv->text);

  priv->text = g_strdup (text);

  clutter_label_dirty_cache (label);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

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
 *   string is owned by @label and should not be modified
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

  g_free (priv->font_name);
  priv->font_name = g_strdup (font_name);
  
  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  priv->font_desc = desc;

  clutter_label_dirty_cache (label);

  if (label->priv->text && label->priv->text[0] != '\0')
    clutter_actor_queue_relayout (CLUTTER_ACTOR (label));
  
  g_object_notify (G_OBJECT (label), "font-name");
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
      priv->ellipsize = mode;

      clutter_label_dirty_cache (label);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "ellipsize");
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
      priv->wrap = wrap;

      clutter_label_dirty_cache (label);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "wrap");
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
      priv->wrap_mode = wrap_mode;

      clutter_label_dirty_cache (label);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "wrap-mode");
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
PangoLayout *
clutter_label_get_layout (ClutterLabel *label)
{
  ClutterUnit width;

  g_return_val_if_fail (CLUTTER_IS_LABEL (label), NULL);

  width = clutter_actor_get_widthu (CLUTTER_ACTOR (label));

  return clutter_label_create_layout (label, width);
}

static inline void
clutter_label_set_attributes_internal (ClutterLabel  *label,
				       PangoAttrList *attrs)
{
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
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;

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

  priv->attrs = attrs;

  clutter_label_dirty_cache (label);

  g_object_notify (G_OBJECT (label), "attributes");

  clutter_actor_queue_relayout (CLUTTER_ACTOR (label));
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
      priv->use_markup = setting;

      clutter_label_dirty_cache (label);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "use-markup");
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
 *
 * The alignment will only be used when the contents of the
 * label are enough to wrap, and the #ClutterLabel:wrap
 * property is set to %TRUE.
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
      priv->alignment = alignment;

      clutter_label_dirty_cache (label);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "alignment");
    }
}

/**
 * clutter_label_get_alignment:
 * @label: a #ClutterLabel
 *
 * Returns the label's text alignment
 *
 * Return value: The label's #PangoAlignment
 *
 * Since: 0.2
 **/
PangoAlignment
clutter_label_get_alignment (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), FALSE);
  
  return label->priv->alignment;
}

/**
 * clutter_label_set_justify:
 * @label: a #ClutterLabel
 * @justify: whether the text should be justified
 *
 * Sets whether the text of the @label actor should be justified
 * on both margins. This setting is ignored if Clutter is compiled
 * against Pango &lt; 1.18.
 *
 * Since: 0.6
 */
void
clutter_label_set_justify (ClutterLabel *label,
                           gboolean      justify)
{
  ClutterLabelPrivate *priv;

  g_return_if_fail (CLUTTER_IS_LABEL (label));

  priv = label->priv;

  if (priv->justify != justify)
    {
      priv->justify = justify;

      clutter_label_dirty_cache (label);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (label));

      g_object_notify (G_OBJECT (label), "justify");
    }
}

/**
 * clutter_label_get_justify:
 * @label: a #ClutterLabel
 *
 * Retrieves whether the label should justify the text on both margins.
 *
 * Return value: %TRUE if the text should be justified
 *
 * Since: 0.6
 */
gboolean
clutter_label_get_justify (ClutterLabel *label)
{
  g_return_val_if_fail (CLUTTER_IS_LABEL (label), FALSE);

  return label->priv->justify;
}
