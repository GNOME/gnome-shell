/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Neil Jagdish Patel <njp@o-hand.com
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
 * SECTION:clutter-entry
 * @short_description: A single line text entry actor
 *
 * #ClutterEntry is a #ClutterTexture that allows single line text entry
 */

#include "config.h"

#include "clutter-entry.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-keysyms.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-rectangle.h"
#include "clutter-units.h"
#include "pangoclutter.h"

#define DEFAULT_FONT_NAME	"Sans 10"
#define ENTRY_CURSOR_WIDTH      1
#define ENTRY_PADDING           5

G_DEFINE_TYPE (ClutterEntry, clutter_entry, CLUTTER_TYPE_ACTOR);

/* Probably move into main */
static PangoClutterFontMap  *_font_map = NULL;
static PangoContext         *_context  = NULL;

enum
{
  PROP_0,
  PROP_FONT_NAME,
  PROP_TEXT,
  PROP_COLOR,
  PROP_ALIGNMENT, 		/* FIXME */
  PROP_POSITION,
  PROP_CURSOR,
  PROP_TEXT_VISIBLE
};

enum
{
  TEXT_CHANGED,
  CURSOR_EVENT,
  
  LAST_SIGNAL
};

static guint entry_signals[LAST_SIGNAL] = { 0, };

#define CLUTTER_ENTRY_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ENTRY, ClutterEntryPrivate))

struct _ClutterEntryPrivate
{
  PangoContext         *context;
  PangoFontDescription *desc;
  
  ClutterColor          fgcol;
  
  gchar                *text;
  gchar                *font_name;
  gboolean              text_visible;
  gunichar              priv_char;
  
  gint                  extents_width;
  gint                  extents_height;

  guint                 alignment        : 2;
  guint                 wrap             : 1;
  guint                 use_underline    : 1;
  guint                 use_markup       : 1;
  guint                 ellipsize        : 3;
  guint                 single_line_mode : 1;
  guint                 wrap_mode        : 3;
  gint                  position;
  gint                  text_x;

  PangoAttrList        *attrs;
  PangoAttrList        *effective_attrs;
  PangoLayout          *layout;
  gint                  width_chars;
  
  ClutterGeometry       cursor_pos;
  ClutterActor         *cursor;
  gboolean              show_cursor;
};


static void
clutter_entry_set_property (GObject      *object, 
			    guint         prop_id,
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  ClutterEntry        *entry;
  ClutterEntryPrivate *priv;

  entry = CLUTTER_ENTRY(object);
  priv = entry->priv;

  switch (prop_id) 
    {
    case PROP_FONT_NAME:
      clutter_entry_set_font_name (entry, g_value_get_string (value));
      break;
    case PROP_TEXT:
      clutter_entry_set_text (entry, g_value_get_string (value));
      break;
    case PROP_COLOR:
      clutter_entry_set_color (entry, g_value_get_boxed (value));
      break;
    case PROP_ALIGNMENT:
      clutter_entry_set_alignment (entry, g_value_get_enum (value));
      break;
    case PROP_POSITION:
      clutter_entry_set_position (entry, g_value_get_int (value));
      break;
    case PROP_CURSOR:
      clutter_entry_set_visible_cursor (entry, g_value_get_boolean (value));
      break;
    case PROP_TEXT_VISIBLE:
      clutter_entry_set_visibility (entry, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_entry_get_property (GObject    *object, 
			    guint       prop_id,
			    GValue     *value, 
			    GParamSpec *pspec)
{
  ClutterEntry        *entry;
  ClutterEntryPrivate *priv;
  ClutterColor         color;

  entry = CLUTTER_ENTRY(object);
  priv = entry->priv;

  switch (prop_id) 
    {
    case PROP_FONT_NAME:
      g_value_set_string (value, priv->font_name);
      break;
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_COLOR:
      clutter_entry_get_color (entry, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_ALIGNMENT:
      g_value_set_enum (value, priv->alignment);
      break;
    case PROP_POSITION:
      g_value_set_int (value, priv->position);
      break;
    case PROP_CURSOR:
      g_value_set_boolean (value, priv->show_cursor);
      break;
    case PROP_TEXT_VISIBLE:
      g_value_set_boolean (value, priv->text_visible);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
clutter_entry_ensure_layout (ClutterEntry *entry, gint width)
{
  ClutterEntryPrivate  *priv;

  priv   = entry->priv;

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
      
      if (priv->text_visible)
        pango_layout_set_text (priv->layout, priv->text, -1);
      else
        {
          gint len = g_utf8_strlen (priv->text, -1);
          gchar *invisible = g_strnfill (len, priv->priv_char);
          pango_layout_set_markup (priv->layout, invisible, -1);
          g_free (invisible);
        }
      if (priv->wrap)
	pango_layout_set_wrap  (priv->layout, priv->wrap_mode);
      
      if (priv->wrap && width > 0)
	{
	  pango_layout_set_width (priv->layout, width * PANGO_SCALE);
	}
      else
	pango_layout_set_width (priv->layout, -1);
    }
}

static void
clutter_entry_clear_layout (ClutterEntry *entry)
{
  if (entry->priv->layout)
    {
      g_object_unref (entry->priv->layout);
      entry->priv->layout = NULL;
    }
}

static void
clutter_entry_ensure_cursor_position (ClutterEntry *entry)
{
  ClutterEntryPrivate  *priv;
  gint                  index;
  PangoRectangle        rect;
    
  priv = entry->priv;
  
  if (priv->position == -1)
    index = g_utf8_strlen (priv->text, -1);
  else
    index = priv->position;
  
  if (1) 
    {
       pango_layout_get_cursor_pos (priv->layout, index, &rect, NULL);
       priv->cursor_pos.x = rect.x / PANGO_SCALE;
       priv->cursor_pos.y = rect.y / PANGO_SCALE;
       priv->cursor_pos.width = ENTRY_CURSOR_WIDTH;
       priv->cursor_pos.height = rect.height / PANGO_SCALE;
  
       g_signal_emit (entry, entry_signals[CURSOR_EVENT], 0, &priv->cursor_pos);
  }
}

static void
clutter_entry_clear_cursor_position (ClutterEntry *entry)
{
  entry->priv->cursor_pos.width = 0;
}

void
clutter_entry_paint_cursor (ClutterEntry *entry)
{
  ClutterEntryPrivate  *priv;

  priv   = entry->priv;
  
  if (priv->show_cursor)
    {
      clutter_actor_set_size (CLUTTER_ACTOR (priv->cursor), 
                              priv->cursor_pos.width,
                              priv->cursor_pos.height);
      
      clutter_actor_set_position (priv->cursor,
                                  priv->cursor_pos.x,
                                  priv->cursor_pos.y);
                              
      clutter_actor_paint (priv->cursor);
    }  
}

void
clutter_entry_paint (ClutterActor *self)
{
  ClutterEntry         *entry;
  ClutterEntryPrivate  *priv;
  PangoRectangle        logical;
  gint                  actor_width;
  gint                  text_width;
  gint                  cursor_x;
  
  entry  = CLUTTER_ENTRY(self);
  priv   = entry->priv;

  if (priv->desc == NULL || priv->text == NULL)
    {
      CLUTTER_NOTE (ACTOR, "layout: %p , desc: %p, text %p",
		    priv->layout,
		    priv->desc,
		    priv->text);
      return;
    }
  
  clutter_actor_set_clip (self, 0, 0,
                          clutter_actor_get_width (self),
                          clutter_actor_get_height (self));
  
  actor_width = clutter_actor_get_width(self) - (2*ENTRY_PADDING);
  clutter_entry_ensure_layout (entry, actor_width);
  clutter_entry_ensure_cursor_position (entry);

  pango_layout_get_extents (priv->layout, NULL, &logical);
  text_width = logical.width / PANGO_SCALE;
  
  if (actor_width < text_width)
    {
      /* We need to do some scrolling */
      cursor_x = priv->cursor_pos.x;
      
      /* If the cursor is at the begining or the end of the text, the placement
         is easy, however, if the cursor is in the middle somewhere, we need to
         make sure the text doesn't move until the cursor is either in the 
         far left or far right
      */
      
      if (priv->position == 0)
        priv->text_x = 0;
      else if (priv->position == -1)
        {
          priv->text_x = actor_width - text_width;
          priv->cursor_pos.x += priv->text_x + ENTRY_PADDING;
        }
      else 
        {
           if (priv->text_x < 0)
             {
               gint diff = -1 * priv->text_x;
               if (cursor_x < diff)
                 priv->text_x += diff - cursor_x;
               else if (cursor_x > (diff + actor_width))
                 priv->text_x -= cursor_x - (diff+actor_width);
             }
           priv->cursor_pos.x += priv->text_x + ENTRY_PADDING;
        }
      
    } 
  else
    {
      priv->text_x = 0;
      priv->cursor_pos.x += ENTRY_PADDING;
    }
  
  priv->fgcol.alpha   =  clutter_actor_get_opacity(self);
  pango_clutter_render_layout (priv->layout, 
                               priv->text_x + ENTRY_PADDING, 
                               0, &priv->fgcol, 0);
  
  if (CLUTTER_ENTRY_GET_CLASS (entry)->paint_cursor != NULL)
    CLUTTER_ENTRY_GET_CLASS (entry)->paint_cursor (entry);
}

static void
clutter_entry_request_coords (ClutterActor        *self,
			      ClutterActorBox     *box)
{
  /* do we need to do anything ? */
  clutter_entry_clear_layout (CLUTTER_ENTRY(self));
}

static void 
clutter_entry_dispose (GObject *object)
{
  ClutterEntry         *self = CLUTTER_ENTRY(object);
  ClutterEntryPrivate  *priv;  

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

  G_OBJECT_CLASS (clutter_entry_parent_class)->dispose (object);
}

static void 
clutter_entry_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_entry_parent_class)->finalize (object);
}

static void
clutter_entry_class_init (ClutterEntryClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  
  klass->paint_cursor          = clutter_entry_paint_cursor;

  actor_class->paint           = clutter_entry_paint;
  actor_class->request_coords  = clutter_entry_request_coords;
  
  gobject_class->finalize   = clutter_entry_finalize;
  gobject_class->dispose    = clutter_entry_dispose;
  gobject_class->set_property = clutter_entry_set_property;
  gobject_class->get_property = clutter_entry_get_property;

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
    (gobject_class, PROP_ALIGNMENT,
     g_param_spec_enum ( "alignment",
			 "Alignment",
			 "The preferred alignment for the string,",
			 PANGO_TYPE_ALIGNMENT,
			 PANGO_ALIGN_LEFT,
			 CLUTTER_PARAM_READWRITE));

  /**
   * ClutterEntry:position
   *
   * The current input cursor position. -1 is taken to be the end of the text
   */	
  g_object_class_install_property 
    (gobject_class, PROP_POSITION,
     g_param_spec_int ( "position",
			"Position",
			"The cursor position",
			-1, G_MAXINT,
			-1,
			CLUTTER_PARAM_READWRITE));

  /**
   * ClutterEntry:cursor-visible
   *
   * Whether the input cursor is visible
   */			
  g_object_class_install_property 
    (gobject_class, PROP_CURSOR,
     g_param_spec_boolean ( "cursor-visible",
			"Cursor Visible",
			"Whether the input cursor is visible",
			TRUE,
			CLUTTER_PARAM_READWRITE));
			
  /**
   * ClutterEntry:text-visible
   *
   * Whether the text is visible in plain text
   */			
  g_object_class_install_property 
    (gobject_class, PROP_TEXT_VISIBLE,
     g_param_spec_boolean ("text-visible",
			   "Text Visible",
			   "Whether the text is visible in plain text",
			   TRUE,
			   CLUTTER_PARAM_READWRITE));
			

  /**
   * ClutterEntry::text-changed:
   * @entry: the actor which received the event
   *
   * The ::text-changed signal is emitted after the @entrys text changes
   */
  entry_signals[TEXT_CHANGED] =
    g_signal_new ("text-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterEntryClass, text_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterEntry::cursor-event:
   * @entry: the actor which received the event
   * @geometry: a #ClutterGeometry
   *
   * The ::cursor-event signal is emitted each time the input cursors geometry
   * changes, this could be a positional or size change. If you would like to
   * implement your own input cursor, set the cursor-visible property to FALSE, 
   * and connect to this signal to position and size your own cursor.
   */
  entry_signals[CURSOR_EVENT] =
    g_signal_new ("cursor-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterEntryClass, cursor_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_GEOMETRY | G_SIGNAL_TYPE_STATIC_SCOPE);

  g_type_class_add_private (gobject_class, sizeof (ClutterEntryPrivate));
}

static void
clutter_entry_init (ClutterEntry *self)
{
  ClutterEntryPrivate *priv;
  
  self->priv = priv = CLUTTER_ENTRY_GET_PRIVATE (self);

  if (_context == NULL)
    {
      _font_map = PANGO_CLUTTER_FONT_MAP (pango_clutter_font_map_new ());
      /* pango_clutter_font_map_set_resolution (font_map, 96.0, 96.0); */
      _context = pango_clutter_font_map_create_context (_font_map);
    }

  priv->alignment     = PANGO_ALIGN_LEFT;
  priv->wrap          = FALSE;
  priv->wrap_mode     = PANGO_WRAP_WORD;
  priv->ellipsize     = PANGO_ELLIPSIZE_NONE;
  priv->use_underline = FALSE;
  priv->use_markup    = FALSE;
  priv->layout        = NULL;
  priv->text          = NULL;
  priv->attrs         = NULL;
  priv->position      = -1;
  priv->priv_char     = '*';
  priv->text_visible  = TRUE;
  priv->text_x        = 0;

  priv->fgcol.red     = 0;
  priv->fgcol.green   = 0;
  priv->fgcol.blue    = 0;
  priv->fgcol.alpha   = 255;

  priv->font_name     = g_strdup (DEFAULT_FONT_NAME);
  priv->desc          = pango_font_description_from_string (priv->font_name);
  
  priv->cursor        = clutter_rectangle_new_with_color (&priv->fgcol);
  clutter_actor_set_parent (priv->cursor, CLUTTER_ACTOR (self));
  priv->show_cursor   = TRUE;

  CLUTTER_MARK();
}

/**
 * clutter_entry_new_with_text:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 *
 * Creates a new #ClutterEntry displaying @text using @font_name.
 *
 * Return value: a #ClutterEntry
 */
ClutterActor*
clutter_entry_new_with_text (const gchar *font_name,
		             const gchar *text)
{
  ClutterActor *entry;

  CLUTTER_MARK();

  entry = clutter_entry_new ();

  clutter_entry_set_font_name (CLUTTER_ENTRY(entry), font_name);
  clutter_entry_set_text (CLUTTER_ENTRY(entry), text);

  return entry;
}

/**
 * clutter_entry_new_full:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 * @color: #ClutterColor for text
 *
 * Creates a new #ClutterEntry displaying @text with color @color 
 * using @font_name.
 *
 * Return value: a #ClutterEntry
 */
ClutterActor*
clutter_entry_new_full (const gchar  *font_name,
			const gchar  *text,
			ClutterColor *color)
{
  /* FIXME: really new_with_text should take color argument... */
  ClutterActor *entry;

  entry = clutter_entry_new_with_text (font_name, text);
  clutter_entry_set_color (CLUTTER_ENTRY(entry), color);

  return entry;
}

/**
 * clutter_entry_new:
 *
 * Creates a new, empty #ClutterEntry.
 *
 * Returns: the newly created #ClutterEntry
 */
ClutterActor *
clutter_entry_new (void)
{
  return g_object_new (CLUTTER_TYPE_ENTRY, NULL);
}

/**
 * clutter_entry_get_text:
 * @entry: a #ClutterEntry
 *
 * Retrieves the text displayed by @entry
 *
 * Return value: the text of the entry.  The returned string is
 * owned by #ClutterEntry and should not be modified or freed.
 */
G_CONST_RETURN gchar *
clutter_entry_get_text (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), NULL);

  return entry->priv->text;
}

/**
 * clutter_entry_set_text:
 * @entry: a #ClutterEntry
 * @text: the text to be displayed
 *
 * Sets @text as the text to be displayed by @entry.
 */
void
clutter_entry_set_text (ClutterEntry *entry,
		        const gchar  *text)
{
  ClutterEntryPrivate  *priv;  

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  g_object_ref (entry);

  g_free (priv->text);
  priv->text = g_strdup (text);

  clutter_entry_clear_layout (entry);  
  clutter_entry_clear_cursor_position (entry); 

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(entry)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(entry));

  g_object_notify (G_OBJECT (entry), "text");
  g_signal_emit (G_OBJECT (entry), entry_signals[TEXT_CHANGED], 0);  
  g_object_unref (entry);
}

/**
 * clutter_entry_get_font_name:
 * @entry: a #ClutterEntry
 *
 * Retrieves the font used by @entry.
 *
 * Return value: a string containing the font name, in a format
 *   understandable by pango_font_description_from_string().  The
 *   string is owned by #ClutterEntry and should not be modified
 *   or freed.
 */
G_CONST_RETURN gchar *
clutter_entry_get_font_name (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), NULL);
  
  return entry->priv->font_name;
}

/**
 * clutter_entry_set_font_name:
 * @entry: a #ClutterEntry
 * @font_name: a font name and size, or %NULL for the default font
 *
 * Sets @font_name as the font used by @entry.
 *
 * @font_name must be a string containing the font name and its
 * size, similarly to what you would feed to the
 * pango_font_description_from_string() function.
 */
void
clutter_entry_set_font_name (ClutterEntry *entry,
		             const gchar  *font_name)
{
  ClutterEntryPrivate *priv;
  PangoFontDescription *desc;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  if (!font_name || font_name[0] == '\0')
    font_name = DEFAULT_FONT_NAME;

  priv = entry->priv;

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

  g_object_ref (entry);

  g_free (priv->font_name);
  priv->font_name = g_strdup (font_name);
  
  if (priv->desc)
    pango_font_description_free (priv->desc);

  priv->desc = desc;

  if (entry->priv->text && entry->priv->text[0] != '\0')
    {
      clutter_entry_clear_layout (entry);      

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(entry)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR(entry));
    }
  
  g_object_notify (G_OBJECT (entry), "font-name");
  g_object_unref (entry);
}


/**
 * clutter_entry_set_color:
 * @entry: a #ClutterEntry
 * @color: a #ClutterColor
 *
 * Sets the color of @entry.
 */
void
clutter_entry_set_color (ClutterEntry       *entry,
		         const ClutterColor *color)
{
  ClutterActor *actor;
  ClutterEntryPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  g_return_if_fail (color != NULL);

  priv = entry->priv;

  g_object_ref (entry);

  priv->fgcol.red = color->red;
  priv->fgcol.green = color->green;
  priv->fgcol.blue = color->blue;
  priv->fgcol.alpha = color->alpha;

  actor = CLUTTER_ACTOR (entry);

  clutter_actor_set_opacity (actor, priv->fgcol.alpha);
  
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->cursor), &priv->fgcol);

  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
    clutter_actor_queue_redraw (actor);

  g_object_notify (G_OBJECT (entry), "color");
  g_object_unref (entry);
}

/**
 * clutter_entry_get_color:
 * @entry: a #ClutterEntry
 * @color: return location for a #ClutterColor
 *
 * Retrieves the color of @entry.
 */
void
clutter_entry_get_color (ClutterEntry *entry,
			 ClutterColor *color)
{
  ClutterEntryPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  g_return_if_fail (color != NULL);

  priv = entry->priv;

  color->red = priv->fgcol.red;
  color->green = priv->fgcol.green;
  color->blue = priv->fgcol.blue;
  color->alpha = priv->fgcol.alpha;
}

/**
 * clutter_entry_get_layout:
 * @entry: a #ClutterEntry
 * 
 * Gets the #PangoLayout used to display the entry.
 * The layout is useful to e.g. convert text positions to
 * pixel positions.
 * The returned layout is owned by the entry so need not be
 * freed by the caller.
 * 
 * Return value: the #PangoLayout for this entry
 *
 * Since: 0.2
 **/
PangoLayout*
clutter_entry_get_layout (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), NULL);

  clutter_entry_ensure_layout (entry, -1);

  return entry->priv->layout;
}

/**
 * clutter_entry_set_alignment:
 * @entry: a #ClutterEntry
 * @alignment: A #PangoAlignment
 *
 * Sets text alignment of the entry.
 **/
void
clutter_entry_set_alignment (ClutterEntry   *entry,
			     PangoAlignment  alignment)
{
  ClutterEntryPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  if (priv->alignment != alignment)
    {
      g_object_ref (entry);

      priv->alignment = alignment;
      clutter_entry_clear_layout (entry);

      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (entry)))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));

      g_object_notify (G_OBJECT (entry), "alignment");
      g_object_unref (entry);
    }
}

/**
 * clutter_entry_get_alignment:
 * @entry: a #ClutterEntry
 *
 * Returns the entry's text alignment
 *
 * Return value: The entrys #PangoAlignment
 *
 * Since 0.2
 **/
PangoAlignment
clutter_entry_get_alignment (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), FALSE);
  
  return entry->priv->alignment;
}

/**
 * clutter_entry_set_position:
 * @entry: a #ClutterEntry
 * @position: the position of the cursor. The cursor is displayed before 
 * the character with the given (base 0) index.
 * The value must be less than or equal to the number of 
 * characters in the entry. A value of -1 indicates that the position should
 * be set after the last character in the entry. 
 * Note that this position is in characters, not in bytes.
 * 
 **/
void
clutter_entry_set_position (ClutterEntry *entry, gint position)
{
  ClutterEntryPrivate *priv;
  gint len;
  
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;
  
  if (priv->text == NULL)
    return;
  
  len = g_utf8_strlen (priv->text, -1);
  
  if (position < 0 || position >= len)
    priv->position = -1;
  else
    priv->position = position;

  clutter_entry_clear_cursor_position (entry);
  
  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(entry)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(entry));
}

/**
 * clutter_entry_get_position:
 * @entry: a #ClutterEntry
 *
 * Returns the entry's text alignment
 *
 * Return value: the position of the cursor. 
 * The cursor is displayed before the character with the given (base 0) index 
 * in the widget. The value will be less than or equal to the number of 
 * characters in the widget. Note that this position is in characters, 
 * not in bytes.
 *
 **/
gint
clutter_entry_get_position (ClutterEntry *entry)
{
  ClutterEntryPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), 0);

  priv = entry->priv;
  
  return priv->position;
}

/**
 * clutter_entry_handle_key_event:
 * @entry: a #ClutterEntry
 * @kev: a #ClutterKeyEvent
 *
 * This function will handle a #ClutterKeyEvent, like those returned in a 
 * key-press/release-event, and will translate it for the @entry. This includes
 * non-alphanumeric keys, such as the arrows keys, which will move the
 * input cursor.
 *
 **/
void
clutter_entry_handle_key_event (ClutterEntry *entry, ClutterKeyEvent *kev)
{
  ClutterEntryPrivate *priv;
  gint pos = 0;
  gint len = 0;
  gint keyval = clutter_key_event_symbol (kev);

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;  
  
  pos = priv->position;
  if (priv->text)
    len = g_utf8_strlen (priv->text, -1);
  
  switch (keyval)
    {
      case CLUTTER_Return:
      case CLUTTER_KP_Enter:
      case CLUTTER_ISO_Enter:
      case CLUTTER_Escape:
      case CLUTTER_Up:
      case CLUTTER_KP_Up:
      case CLUTTER_Down:
      case CLUTTER_KP_Down:
      case CLUTTER_Shift_L:
      case CLUTTER_Shift_R:
        break;
      case CLUTTER_BackSpace:
        if (pos != 0 && len != 0)
          clutter_entry_remove (entry, 1);
        break;
      case CLUTTER_Delete:
      case CLUTTER_KP_Delete:
        if (len && pos != -1)
          {
            clutter_entry_delete_text (entry, pos, pos+1);;
          }
        break;
      case CLUTTER_Left:
      case CLUTTER_KP_Left:
        if (pos != 0 && len != 0)
          {
            if (pos == -1)
              {
                clutter_entry_set_position (entry, len-1);  
              }
            else
              clutter_entry_set_position (entry, pos - 1);  
          }         
        break;
      case CLUTTER_Right:
      case CLUTTER_KP_Right:
        if (pos != -1 && len != 0)
          {
            if (pos != len)
              clutter_entry_set_position (entry, pos +1);  
          } 
        break;
      case CLUTTER_End:
      case CLUTTER_KP_End:
        clutter_entry_set_position (entry, -1);  
        break;
      case CLUTTER_Begin:
      case CLUTTER_Home:
      case CLUTTER_KP_Home:
        clutter_entry_set_position (entry, 0);
        break;
      default:
        clutter_entry_add (entry, clutter_keysym_to_unicode (keyval));
        break;
    }
}

/**
 * clutter_entry_add:
 * @entry: a #ClutterEntry
 * @wc: a Unicode character
 *
 * Insert a character to the right of the current position of the cursor.
 * 
 **/
void
clutter_entry_add (ClutterEntry *entry, gunichar wc)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  g_return_if_fail (g_unichar_validate (wc));

  priv = entry->priv;

  g_object_ref (entry);
  
  new = g_string_new (priv->text);
  
  new = g_string_insert_unichar (new, priv->position, wc);
  
  clutter_entry_set_text (entry, new->str);

  if (priv->position >= 0)
    clutter_entry_set_position (entry, priv->position + 1);
  
  g_string_free (new, TRUE);
  g_object_unref (entry);
}

/**
 * clutter_entry_remove:
 * @entry: a #ClutterEntry
 * @len: the number of characters to remove.
 *
 * Characters are removed from before the current postion of the cursor.
 * 
 **/
void
clutter_entry_remove (ClutterEntry *entry, guint num)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;
  gint len;
  
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;

  g_object_ref (entry);
  
  if (priv->text == NULL)
    {
      g_object_unref (entry);
      return;
    }
  len = g_utf8_strlen (priv->text, -1);
  new = g_string_new (priv->text);
  
  if (priv->position == -1)
    new = g_string_erase (new, len-num, num);
  else
    new = g_string_erase (new, priv->position-num, num);

  clutter_entry_set_text (entry, new->str);

  if (priv->position > 0)
    clutter_entry_set_position (entry, priv->position-num);  

  g_string_free (new, TRUE);
  g_object_unref (entry);
}

/**
 * clutter_entry_insert_text:
 * @entry: a #ClutterEntry
 * @text: the text to insert
 * @position: the position at which to insert the text. A value of 0 indicates 
 * that the text will be inserted before the first character in the entrys text,
 * and a value of -1 indicates that the text will be inserted after the last
 * character in the entrys text.
 * 
 * Insert text at a specifc position.
 **/
void
clutter_entry_insert_text (ClutterEntry *entry, 
                           const gchar  *text,
                           gssize        position)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;
  
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;

  g_object_ref (entry);
  
  new = g_string_new (priv->text);
  new = g_string_insert (new, position, text);
  
  clutter_entry_set_text (entry, new->str);
  
  g_string_free (new, TRUE);
  g_object_unref (entry);
}

/**
 * clutter_entry_delete_text
 * @entry: a #ClutterEntry
 * @start_pos: the starting position. 
 * @end_pos: the end position. 
 * 
 * Deletes a sequence of characters. The characters that are deleted are those
 * characters at positions from start_pos up to, but not including end_pos. 
 * If end_pos is negative, then the the characters deleted will be those 
 * characters from start_pos to the end of the text.
 **/
void
clutter_entry_delete_text (ClutterEntry       *entry,
                           gssize              start_pos,
                           gssize              end_pos)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;

  g_object_ref (entry);
  if (priv->text == NULL)
    {
      g_object_unref (entry);
      return;
    }
    
  new = g_string_new (priv->text);
  new = g_string_erase (new, start_pos, end_pos-start_pos);
  
  clutter_entry_set_text (entry, new->str);
  
  g_string_free (new, TRUE);
  g_object_unref (entry);
}

/**
 * clutter_entry_set_visible_cursor
 * @entry: a #ClutterEntry
 * @visible: whether the input cursor should be visible
 * 
 * Sets the visibility of the input cursor.
 **/
void
clutter_entry_set_visible_cursor (ClutterEntry *entry, gboolean visible)
{
  ClutterEntryPrivate *priv;
  
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;
  priv->show_cursor = visible;
  
  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(entry)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(entry));
}

/**
 * clutter_entry_get_visible_cursor
 * @entry: a #ClutterEntry
 *
 * Returns the input cursors visiblity
 *
 * Return value: whether the input cursor is visible
 * 
 **/
gboolean
clutter_entry_get_visible_cursor (ClutterEntry *entry)
{
  ClutterEntryPrivate *priv;
  
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), FALSE);
  
  priv = entry->priv;
  
  return priv->show_cursor;
}

/**
 * clutter_entry_set_visibility
 * @entry: a #ClutterEntry
 * @visible: TRUE if the contents of the entry are displayed as plaintext. 
 *
 * Sets whether the contents of the entry are visible or not. When visibility 
 * is set to FALSE, characters are displayed as the invisible char, and will 
 * also appear that way when the text in the entry widget is copied elsewhere.
 * 
 * The default invisible char is the asterisk '*', but it can be changed with  
 * #clutter_entry_set_invisible_char().  
 * 
 **/
void
clutter_entry_set_visibility (ClutterEntry *entry, gboolean visible)
{
  ClutterEntryPrivate *priv;
  
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;
  
  priv->text_visible = visible;
  
  clutter_entry_clear_layout (entry);
  clutter_entry_clear_cursor_position (entry);
  
  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(entry)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(entry));  
}

/**
 * clutter_entry_get_visibility
 * @entry: a #ClutterEntry
 *
 * Returns the entry text visiblity
 *
 * Return value: TRUE if the contents of the entry are displayed as plaintext. 
 * 
 **/
gboolean
clutter_entry_get_visibility (ClutterEntry *entry)
{
  ClutterEntryPrivate *priv;
  
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), TRUE);
  
  priv = entry->priv;
  
  return priv->text_visible;
}

/**
 * clutter_entry_set_invisible_cha
 * @entry: a #ClutterEntry
 * @wc: a Unicode character 
 *
 * Sets the character to use in place of the actual text when   
 * #clutter_entry_set_visibility() has been called to set text visibility 
 * to FALSE. i.e. this is the character used in "password mode" to show the 
 * user how many characters have been typed. The default invisible char is an
 * asterisk ('*'). If you set the invisible char to 0, then the user will get
 * no feedback at all; there will be no text on the screen as they type. 
 * 
 **/
void
clutter_entry_set_invisible_char (ClutterEntry *entry, gunichar wc)
{
  ClutterEntryPrivate *priv;
  
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  
  priv = entry->priv;
  
  priv->priv_char = wc;
  
  if (!priv->text_visible)
    return;
  
  clutter_entry_clear_layout (entry);
  clutter_entry_clear_cursor_position (entry);
  
  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(entry)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(entry));  
}

/**
 * clutter_entry_get_invisible_char
 * @entry: a #ClutterEntry
 *
 * Returns the character to use in place of the actual text when text-visibility
 * is set to FALSE
 *
 * Return value: a Unicode character 
 * 
 **/
gunichar
clutter_entry_get_invisible_char (ClutterEntry *entry)
{
  ClutterEntryPrivate *priv;
  
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), TRUE);
  
  priv = entry->priv;
  
  return priv->priv_char;
}

