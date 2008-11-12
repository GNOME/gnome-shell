/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Neil Jagdish Patel <njp@o-hand.com>
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
 * #ClutterEntry is a #ClutterTexture that allows single line text entry.
 *
 * #ClutterEntry is available since Clutter 0.4.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl/cogl.h>

#include "clutter-entry.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-keysyms.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-rectangle.h"
#include "clutter-units.h"

#include "cogl-pango.h"

#define DEFAULT_FONT_NAME	"Sans 10"
#define ENTRY_CURSOR_WIDTH      1
#define ENTRY_PADDING           5

G_DEFINE_TYPE (ClutterEntry, clutter_entry, CLUTTER_TYPE_ACTOR);

/* Probably move into main */
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
  PROP_TEXT_VISIBLE,
  PROP_MAX_LENGTH,
  PROP_ENTRY_PADDING,
  PROP_X_ALIGN
};

enum
{
  TEXT_CHANGED,
  CURSOR_EVENT,
  ACTIVATE,

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

  gint                  width;
  gint                  n_chars; /* number of chars */
  gint                  n_bytes; /* number of bytes */

  guint                 alignment        : 2;
  guint                 wrap             : 1;
  guint                 use_underline    : 1;
  guint                 use_markup       : 1;
  guint                 ellipsize        : 3;
  guint                 single_line_mode : 1;
  guint                 wrap_mode        : 3;
  gint                  position;
  gint                  text_x;
  gint                  max_length;
  gint                  entry_padding;
  gdouble               x_align;

  PangoAttrList        *attrs;
  PangoAttrList        *effective_attrs;
  PangoLayout          *layout;
  gint                  width_chars;

  ClutterGeometry       cursor_pos;
  gboolean              show_cursor;
};

static void
clutter_entry_set_entry_padding (ClutterEntry *entry,
                                 guint         padding)
{
  ClutterEntryPrivate *priv = entry->priv;

  if (priv->entry_padding != padding)
    {
      priv->entry_padding = padding;

      if (CLUTTER_ACTOR_IS_VISIBLE (entry))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));

      g_object_notify (G_OBJECT (entry), "entry-padding");
    }
}

static void
clutter_entry_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterEntry        *entry;
  ClutterEntryPrivate *priv;

  entry = CLUTTER_ENTRY (object);
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
      clutter_entry_set_cursor_position (entry, g_value_get_int (value));
      break;
    case PROP_CURSOR:
      clutter_entry_set_visible_cursor (entry, g_value_get_boolean (value));
      break;
    case PROP_TEXT_VISIBLE:
      clutter_entry_set_visibility (entry, g_value_get_boolean (value));
      break;
    case PROP_MAX_LENGTH:
      clutter_entry_set_max_length (entry, g_value_get_int (value));
      break;
    case PROP_ENTRY_PADDING:
      clutter_entry_set_entry_padding (entry, g_value_get_uint (value));
      break;
    case PROP_X_ALIGN:
      entry->priv->x_align = g_value_get_double (value);
      clutter_actor_queue_redraw (CLUTTER_ACTOR (object));
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
    case PROP_MAX_LENGTH:
      g_value_set_int (value, priv->max_length);
      break;
    case PROP_ENTRY_PADDING:
      g_value_set_uint (value, priv->entry_padding);
      break;
    case PROP_X_ALIGN:
      g_value_set_double (value, priv->x_align);
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
        pango_layout_set_text (priv->layout, priv->text, priv->n_bytes);
      else
        {
          GString *str = g_string_sized_new (priv->n_bytes);
          gunichar invisible_char;
          gchar buf[7];
          gint char_len, i;

          if (priv->priv_char != 0)
            invisible_char = priv->priv_char;
          else
            invisible_char = '*';

          /* we need to convert the string built of invisible characters
           * into UTF-8 for it to be fed to the Pango layout
           */
          memset (buf, 0, sizeof (buf));
          char_len = g_unichar_to_utf8 (invisible_char, buf);

          for (i = 0; i < priv->n_chars; i++)
            g_string_append_len (str, buf, char_len);

          pango_layout_set_text (priv->layout, str->str, str->len);

          g_string_free (str, TRUE);
        }

      if (priv->wrap)
	pango_layout_set_wrap  (priv->layout, priv->wrap_mode);

      if (priv->wrap && width > 0)
	pango_layout_set_width (priv->layout, width * PANGO_SCALE);
      else
	pango_layout_set_width (priv->layout, -1);

      /* Prime the cache for the layout */
      cogl_pango_ensure_glyph_cache_for_layout (priv->layout);
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

static gint
offset_to_bytes (const gchar *text, gint pos)
{
  gchar *c = NULL;
  gint i, j, len;

  if (pos < 1)
    return pos;

  c = g_utf8_next_char (text);
  j = 1;
  len = strlen (text);

  for (i = 0; i < len; i++)
    {
      if (&text[i] == c)
      {
        if (j == pos)
          break;
        else
          {
            c = g_utf8_next_char (c);
            j++;
          }
      }
    }
  return i;
}


static void
clutter_entry_ensure_cursor_position (ClutterEntry *entry)
{
  ClutterEntryPrivate  *priv;
  gint                  index_;
  PangoRectangle        rect;
  gint                  priv_char_bytes;

  priv = entry->priv;

  /* If characters are invisible, get the byte-length of the invisible
   * character. If priv_char is 0, we use '*', which is ASCII (1 byte).
   */
  if (!priv->text_visible && priv->priv_char)
    priv_char_bytes = g_unichar_to_utf8 (priv->priv_char, NULL);
  else
    priv_char_bytes = 1;
  
  if (priv->position == -1)
    {
      if (priv->text_visible)
        index_ = strlen (priv->text);
      else
        index_ = priv->n_chars * priv_char_bytes;
    }
  else
    {
      if (priv->text_visible)
        index_ = offset_to_bytes (priv->text, priv->position);
      else
        index_ = priv->position * priv_char_bytes;
    }

  pango_layout_get_cursor_pos (priv->layout, index_, &rect, NULL);
  priv->cursor_pos.x = rect.x / PANGO_SCALE;
  priv->cursor_pos.y = rect.y / PANGO_SCALE;
  priv->cursor_pos.width = ENTRY_CURSOR_WIDTH;
  priv->cursor_pos.height = rect.height / PANGO_SCALE;

  g_signal_emit (entry, entry_signals[CURSOR_EVENT], 0, &priv->cursor_pos);
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
      cogl_set_source_color4ub (priv->fgcol.red,
                                priv->fgcol.green,
                                priv->fgcol.blue,
                                priv->fgcol.alpha);

      cogl_rectangle (priv->cursor_pos.x,
                      priv->cursor_pos.y,
                      priv->cursor_pos.width,
                      priv->cursor_pos.height);
    }
}

static void
clutter_entry_paint (ClutterActor *self)
{
  ClutterEntry         *entry;
  ClutterEntryPrivate  *priv;
  PangoRectangle        logical;
  gint                  width, actor_width;
  gint                  text_width;
  gint                  cursor_x;
  CoglColor             color = { 0, };

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

  if (priv->width < 0)
    width = clutter_actor_get_width (self);
  else
    width = priv->width;

  cogl_clip_set (0, 0,
                 COGL_FIXED_FROM_INT (width),
		 COGL_FIXED_FROM_INT (clutter_actor_get_height (self)));

  actor_width = width - (2 * priv->entry_padding);
  clutter_entry_ensure_layout (entry, actor_width);
  clutter_entry_ensure_cursor_position (entry);

  pango_layout_get_extents (priv->layout, NULL, &logical);
  text_width = logical.width / PANGO_SCALE;

  if (actor_width < text_width)
    {
      /* We need to do some scrolling */
      cursor_x = priv->cursor_pos.x;

      /* If the cursor is at the begining or the end of the text, the placement
       * is easy, however, if the cursor is in the middle somewhere, we need to
       * make sure the text doesn't move until the cursor is either in the
       * far left or far right
       */

      if (priv->position == 0)
        priv->text_x = 0;
      else if (priv->position == -1)
        {
          priv->text_x = actor_width - text_width;
          priv->cursor_pos.x += priv->text_x + priv->entry_padding;
        }
      else
        {
           if (priv->text_x <= 0)
             {
               gint diff = -1 * priv->text_x;

               if (cursor_x < diff)
                 priv->text_x += diff - cursor_x;
               else if (cursor_x > (diff + actor_width))
                 priv->text_x -= cursor_x - (diff+actor_width);
             }

           priv->cursor_pos.x += priv->text_x + priv->entry_padding;
        }

    }
  else
    {
      priv->text_x = (actor_width - text_width) * priv->x_align;
      priv->cursor_pos.x += priv->text_x + priv->entry_padding;
    }

  cogl_color_set_from_4ub (&color,
                           priv->fgcol.red,
                           priv->fgcol.green,
                           priv->fgcol.blue,
                           clutter_actor_get_paint_opacity (self));

  cogl_pango_render_layout (priv->layout,
                            priv->text_x + priv->entry_padding, 0,
                            &color, 0);

  if (CLUTTER_ENTRY_GET_CLASS (entry)->paint_cursor)
    CLUTTER_ENTRY_GET_CLASS (entry)->paint_cursor (entry);

  cogl_clip_unset ();
}

static void
clutter_entry_allocate (ClutterActor          *self,
                        const ClutterActorBox *box,
                        gboolean               absolute_origin_changed)
{
  ClutterEntry *entry = CLUTTER_ENTRY (self);
  ClutterEntryPrivate *priv = entry->priv;
  gint width;

  width = CLUTTER_UNITS_TO_DEVICE (box->x2 - box->x1);

  if (priv->width != width)
    {
      clutter_entry_clear_layout (entry);
      clutter_entry_ensure_layout (entry, width);

      priv->width = width;
    }

  CLUTTER_ACTOR_CLASS (clutter_entry_parent_class)->allocate (self, box, absolute_origin_changed);
}

static inline void
clutter_entry_handle_key_event_internal (ClutterEntry    *entry,
                                         ClutterKeyEvent *event)
{
  gunichar key_unichar;
  ClutterEntryPrivate *priv = entry->priv;
  gint pos = priv->position;
  gint len = 0;
  gint keyval = clutter_key_event_symbol (event);

  if (priv->text)
    len = g_utf8_strlen (priv->text, -1);

  switch (keyval)
    {
      case CLUTTER_Return:
      case CLUTTER_KP_Enter:
      case CLUTTER_ISO_Enter:
        g_signal_emit (entry, entry_signals[ACTIVATE], 0);
        break;

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
          clutter_entry_delete_chars (entry, 1);
        break;

      case CLUTTER_Delete:
      case CLUTTER_KP_Delete:
        if (len && pos != -1)
          clutter_entry_delete_text (entry, pos, pos+1);;
        break;

      case CLUTTER_Left:
      case CLUTTER_KP_Left:
        if (pos != 0 && len != 0)
          {
            if (pos == -1)
              clutter_entry_set_cursor_position (entry, len - 1);
            else
              clutter_entry_set_cursor_position (entry, pos - 1);
          }
        break;

      case CLUTTER_Right:
      case CLUTTER_KP_Right:
        if (pos != -1 && len != 0)
          {
            if (pos != len)
              clutter_entry_set_cursor_position (entry, pos + 1);
          }
        break;

      case CLUTTER_End:
      case CLUTTER_KP_End:
        clutter_entry_set_cursor_position (entry, -1);
        break;

      case CLUTTER_Begin:
      case CLUTTER_Home:
      case CLUTTER_KP_Home:
        clutter_entry_set_cursor_position (entry, 0);
        break;

      default:
        key_unichar = clutter_key_event_unicode (event);
        if (g_unichar_validate (key_unichar))
          clutter_entry_insert_unichar (entry, key_unichar);
        break;
    }
}

static gboolean
clutter_entry_key_press (ClutterActor    *actor,
                         ClutterKeyEvent *event)
{
  clutter_entry_handle_key_event_internal (CLUTTER_ENTRY (actor), event);

  return TRUE;
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
  ClutterEntryPrivate *priv = CLUTTER_ENTRY (object)->priv;

  if (priv->desc)
    pango_font_description_free (priv->desc);

  g_free (priv->text);
  g_free (priv->font_name);

  G_OBJECT_CLASS (clutter_entry_parent_class)->finalize (object);
}

static void
clutter_entry_class_init (ClutterEntryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  klass->paint_cursor = clutter_entry_paint_cursor;

  actor_class->paint           = clutter_entry_paint;
  actor_class->allocate        = clutter_entry_allocate;
  actor_class->key_press_event = clutter_entry_key_press;

  gobject_class->finalize     = clutter_entry_finalize;
  gobject_class->dispose      = clutter_entry_dispose;
  gobject_class->set_property = clutter_entry_set_property;
  gobject_class->get_property = clutter_entry_get_property;

  /**
   * ClutterEntry:font-name:
   *
   * The font to be used by the entry, expressed in a string that
   * can be parsed by pango_font_description_from_string().
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_FONT_NAME,
     g_param_spec_string ("font-name",
			  "Font Name",
			  "Pango font description",
			  NULL,
			  CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry:text:
   *
   * The text inside the entry.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_TEXT,
     g_param_spec_string ("text",
			  "Text",
			  "Text to render",
			  NULL,
			  CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry:color:
   *
   * The color of the text inside the entry.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
			 "Font Colour",
			 "Font Colour",
			 CLUTTER_TYPE_COLOR,
			 CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry:alignment:
   *
   * The preferred alignment for the string.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_ALIGNMENT,
     g_param_spec_enum ("alignment",
			"Alignment",
			"The preferred alignment for the string,",
			PANGO_TYPE_ALIGNMENT,
			PANGO_ALIGN_LEFT,
			CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry:position:
   *
   * The current input cursor position. -1 is taken to be the end of the text
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_POSITION,
     g_param_spec_int ("position",
                       "Position",
                       "The cursor position",
                       -1, G_MAXINT,
                       -1,
                       CLUTTER_PARAM_READWRITE));

  /**
   * ClutterEntry:cursor-visible:
   *
   * Whether the input cursor is visible or not.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_CURSOR,
     g_param_spec_boolean ( "cursor-visible",
			"Cursor Visible",
			"Whether the input cursor is visible",
			TRUE,
			CLUTTER_PARAM_READWRITE));

  /**
   * ClutterEntry:text-visible:
   *
   * Whether the text is visible in plain form, or replaced by the
   * character set by clutter_entry_set_invisible_char().
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_TEXT_VISIBLE,
     g_param_spec_boolean ("text-visible",
			   "Text Visible",
			   "Whether the text is visible in plain form",
			   TRUE,
			   CLUTTER_PARAM_READWRITE));

  /**
   * ClutterEntry:max-length:
   *
   * The maximum length of the entry text.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_MAX_LENGTH,
     g_param_spec_int ("max-length",
		       "Max Length",
		       "The maximum length of the entry text",
		       0, G_MAXINT,
	               0,
		       CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry:entry-padding:
   *
   * The padding space between the text and the entry right and left borders.
   *
   * Since: 0.4
   */
  g_object_class_install_property
    (gobject_class, PROP_ENTRY_PADDING,
     g_param_spec_uint ("entry-padding",
                        "Entry Padding",
                        "The padding space between the text and the left and "
                        "right borders",
                        0, G_MAXUINT,
                        ENTRY_PADDING,
                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry:x-align:
   * 
   * Horizontal alignment to be used for the text (0.0 for left alignment,
   * 1.0 for right alignment).
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_X_ALIGN,
                                   g_param_spec_double ("x-align",
                                                        "Horizontal Alignment",
                                                        "The horizontal alignment to be used for the text",
                                                        0.0, 1.0, 0.0,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEntry::text-changed:
   * @entry: the actor which received the event
   *
   * The ::text-changed signal is emitted after @entry's text changes
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
   * The ::cursor-event signal is emitted each time the input cursor's geometry
   * changes, this could be a positional or size change. If you would like to
   * implement your own input cursor, set the cursor-visible property to %FALSE,
   * and connect to this signal to position and size your own cursor.
   *
   * Since: 0.4
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

  /**
  * ClutterEntry::activate:
   * @entry: the actor which received the event
   *
   * The ::activate signal is emitted each time the entry is 'activated'
   * by the user, normally by pressing the 'Enter' key.
   *
   * Since: 0.4
   */
  entry_signals[ACTIVATE] =
    g_signal_new ("activate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterEntryClass, activate),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ClutterEntryPrivate));
}

static void
clutter_entry_init (ClutterEntry *self)
{
  ClutterEntryPrivate *priv;
  gdouble resolution;
  gint font_size;

  self->priv = priv = CLUTTER_ENTRY_GET_PRIVATE (self);

  if (G_UNLIKELY (_context == NULL))
    _context = _clutter_context_create_pango_context (CLUTTER_CONTEXT ());

  resolution = pango_cairo_context_get_resolution (_context);

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
  priv->max_length    = 0;
  priv->entry_padding = ENTRY_PADDING;
  priv->x_align       = 0.0;

  priv->fgcol.red     = 0;
  priv->fgcol.green   = 0;
  priv->fgcol.blue    = 0;
  priv->fgcol.alpha   = 255;

  priv->font_name     = g_strdup (DEFAULT_FONT_NAME);
  priv->desc          = pango_font_description_from_string (priv->font_name);

  /* we use the font size to set the default width and height, in case
   * the user doesn't call clutter_actor_set_size().
   */
  font_size = PANGO_PIXELS (pango_font_description_get_size (priv->desc))
              * resolution
              / 72.0;
  clutter_actor_set_size (CLUTTER_ACTOR (self), font_size * 20, 50);

  priv->show_cursor   = TRUE;
}

/**
 * clutter_entry_new_with_text:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 *
 * Creates a new #ClutterEntry displaying @text using @font_name.
 *
 * Return value: the newly created #ClutterEntry
 *
 * Since: 0.4
 */
ClutterActor *
clutter_entry_new_with_text (const gchar *font_name,
		             const gchar *text)
{
  ClutterActor *entry = clutter_entry_new ();

  g_object_set (entry,
                "font-name", font_name,
                "text", text,
                NULL);
  return entry;
}

/**
 * clutter_entry_new_full:
 * @font_name: the name (and size) of the font to be used
 * @text: the text to be displayed
 * @color: #ClutterColor for text
 *
 * Creates a new #ClutterEntry displaying @text with @color
 * using @font_name.
 *
 * Return value: the newly created #ClutterEntry
 *
 * Since: 0.4
 */
ClutterActor *
clutter_entry_new_full (const gchar        *font_name,
			const gchar        *text,
			const ClutterColor *color)
{
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
  ClutterActor *entry =  g_object_new (CLUTTER_TYPE_ENTRY,
                                       NULL);
  clutter_actor_set_size (entry, 50, 50);

  return entry;
}

/**
 * clutter_entry_get_text:
 * @entry: a #ClutterEntry
 *
 * Retrieves the text displayed by @entry.
 *
 * Return value: the text of the entry.  The returned string is
 *   owned by #ClutterEntry and should not be modified or freed.
 *
 * Since: 0.4
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
 * Sets @text as the text to be displayed by @entry. The
 * ClutterEntry::text-changed signal is emitted.
 *
 * Since: 0.4
 */
void
clutter_entry_set_text (ClutterEntry *entry,
		        const gchar  *text)
{
  ClutterEntryPrivate  *priv;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  priv = entry->priv;

  g_object_ref (entry);

  if (priv->max_length > 0)
    {
      gint len = g_utf8_strlen (text, -1);

      if (len < priv->max_length)
        {
           g_free (priv->text);

           priv->text = g_strdup (text);
           priv->n_bytes = strlen (text);
           priv->n_chars = len;
        }
      else
        {
          gchar * n = g_malloc0 (priv->max_length + 1);

          g_utf8_strncpy (n, text, priv->max_length);
          g_free (priv->text);

          priv->text = n;
          priv->n_bytes = strlen (n);
          priv->n_chars = priv->max_length;
        }
    }
  else
    {
      g_free (priv->text);

      priv->text = g_strdup (text);
      priv->n_bytes = strlen (text);
      priv->n_chars = g_utf8_strlen (priv->text, -1);
    }

  clutter_entry_clear_layout (entry);
  clutter_entry_clear_cursor_position (entry);
  /* Recreate the layout so the glyph cache will be primed */
  clutter_entry_ensure_layout (entry, -1);

  if (CLUTTER_ACTOR_IS_VISIBLE (entry))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));

  g_signal_emit (G_OBJECT (entry), entry_signals[TEXT_CHANGED], 0);

  g_object_notify (G_OBJECT (entry), "text");
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
 *
 * Since: 0.4
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
 *
 * Since: 0.4
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
      /* Recreate the layout so the glyph cache will be primed */
      clutter_entry_ensure_layout (entry, -1);

      if (CLUTTER_ACTOR_IS_VISIBLE (entry))
	clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));
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
 *
 * Since: 0.4
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
 *
 * Since: 0.4
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
 * Since: 0.4
 **/
PangoLayout *
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
 *
 * Since: 0.4
 */
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

      if (CLUTTER_ACTOR_IS_VISIBLE (entry))
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
 * Return value: The entry's #PangoAlignment
 *
 * Since 0.4
 */
PangoAlignment
clutter_entry_get_alignment (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), FALSE);

  return entry->priv->alignment;
}

/**
 * clutter_entry_set_cursor_position:
 * @entry: a #ClutterEntry
 * @position: the position of the cursor.
 *
 * Sets the position of the cursor. The @position must be less than or
 * equal to the number of characters in the entry. A value of -1 indicates
 * that the position should be set after the last character in the entry.
 * Note that this position is in characters, not in bytes.
 *
 * Since: 0.6
 */
void
clutter_entry_set_cursor_position (ClutterEntry *entry,
                                   gint          position)
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

  if (CLUTTER_ACTOR_IS_VISIBLE (entry))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));
}

/**
 * clutter_entry_get_cursor_position:
 * @entry: a #ClutterEntry
 *
 * Gets the position, in characters, of the cursor in @entry.
 *
 * Return value: the position of the cursor.
 *
 * Since: 0.6
 */
gint
clutter_entry_get_cursor_position (ClutterEntry *entry)
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
 * input cursor. You should use this function inside a handler for the
 * ClutterStage::key-press-event or ClutterStage::key-release-event.
 *
 * Since: 0.4
 *
 * Deprecated: 0.8: The key events will automatically be handled when
 *   giving the key focus to an entry using clutter_stage_set_key_focus().
 */
void
clutter_entry_handle_key_event (ClutterEntry    *entry,
                                ClutterKeyEvent *kev)
{
  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  clutter_entry_handle_key_event_internal (entry, kev);
}

/**
 * clutter_entry_insert_unichar:
 * @entry: a #ClutterEntry
 * @wc: a Unicode character
 *
 * Insert a character to the right of the current position of the cursor,
 * and updates the position of the cursor.
 *
 * Since: 0.4
 */
void
clutter_entry_insert_unichar (ClutterEntry *entry,
                              gunichar      wc)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;
  glong pos;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));
  g_return_if_fail (g_unichar_validate (wc));

  if (wc == 0)
    return;

  priv = entry->priv;

  g_object_ref (entry);

  new = g_string_new (priv->text);
  pos = offset_to_bytes (priv->text, priv->position);
  new = g_string_insert_unichar (new, pos, wc);

  clutter_entry_set_text (entry, new->str);

  if (priv->position >= 0)
    clutter_entry_set_cursor_position (entry, priv->position + 1);

  g_string_free (new, TRUE);

  g_object_notify (G_OBJECT (entry), "text");
  g_object_unref (entry);
}

/**
 * clutter_entry_delete_chars:
 * @entry: a #ClutterEntry
 * @len: the number of characters to remove.
 *
 * Characters are removed from before the current postion of the cursor.
 *
 * Since: 0.4
 */
void
clutter_entry_delete_chars (ClutterEntry *entry,
                            guint         num)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;
  gint len;
  gint pos;
  gint num_pos;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  if (!priv->text)
    return;

  g_object_ref (entry);

  len = g_utf8_strlen (priv->text, -1);
  new = g_string_new (priv->text);

  if (priv->position == -1)
   {
     num_pos = offset_to_bytes (priv->text, len - num);
     new = g_string_erase (new, num_pos, -1);
   }
  else
  {
    pos = offset_to_bytes (priv->text, priv->position - num);
    num_pos = offset_to_bytes (priv->text, priv->position);
    new = g_string_erase (new, pos, num_pos-pos);
  }
  clutter_entry_set_text (entry, new->str);

  if (priv->position > 0)
    clutter_entry_set_cursor_position (entry, priv->position - num);

  g_string_free (new, TRUE);

  g_object_notify (G_OBJECT (entry), "text");
  g_object_unref (entry);
}

/**
 * clutter_entry_insert_text:
 * @entry: a #ClutterEntry
 * @text: the text to insert
 * @position: the position at which to insert the text.
 *
 * Insert text at a specifc position.
 *
 * A value of 0 indicates  that the text will be inserted before the first
 * character in the entry's text, and a value of -1 indicates that the text
 * will be inserted after the last character in the entry's text.
 *
 * Since: 0.4
 */
void
clutter_entry_insert_text (ClutterEntry *entry,
                           const gchar  *text,
                           gssize        position)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  new = g_string_new (priv->text);
  new = g_string_insert (new, position, text);

  clutter_entry_set_text (entry, new->str);

  g_string_free (new, TRUE);
}

/**
 * clutter_entry_delete_text:
 * @entry: a #ClutterEntry
 * @start_pos: the starting position.
 * @end_pos: the end position.
 *
 * Deletes a sequence of characters. The characters that are deleted are
 * those characters at positions from @start_pos up to, but not including,
 * @end_pos. If @end_pos is negative, then the characters deleted will be
 * those characters from @start_pos to the end of the text.
 *
 * Since: 0.4
 */
void
clutter_entry_delete_text (ClutterEntry       *entry,
                           gssize              start_pos,
                           gssize              end_pos)
{
  ClutterEntryPrivate *priv;
  GString *new = NULL;
  gint start_bytes;
  gint end_bytes;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  if (!priv->text)
    return;

  start_bytes = offset_to_bytes (priv->text, start_pos);
  end_bytes = offset_to_bytes (priv->text, end_pos);

  new = g_string_new (priv->text);
  new = g_string_erase (new, start_bytes, end_bytes - start_bytes);

  clutter_entry_set_text (entry, new->str);

  g_string_free (new, TRUE);
}

/**
 * clutter_entry_set_visible_cursor:
 * @entry: a #ClutterEntry
 * @visible: whether the input cursor should be visible
 *
 * Sets the visibility of the input cursor.
 *
 * Since: 0.4
 */
void
clutter_entry_set_visible_cursor (ClutterEntry *entry,
                                  gboolean      visible)
{
  ClutterEntryPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  if (priv->show_cursor != visible)
    {
      priv->show_cursor = visible;

      g_object_notify (G_OBJECT (entry), "cursor-visible");

      if (CLUTTER_ACTOR_IS_VISIBLE (entry))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));
    }
}

/**
 * clutter_entry_get_visible_cursor:
 * @entry: a #ClutterEntry
 *
 * Returns the input cursor's visibility
 *
 * Return value: whether the input cursor is visible
 *
 * Since: 0.4
 */
gboolean
clutter_entry_get_visible_cursor (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), FALSE);

  return entry->priv->show_cursor;
}

/**
 * clutter_entry_set_visibility:
 * @entry: a #ClutterEntry
 * @visible: %TRUE if the contents of the entry are displayed as plaintext.
 *
 * Sets whether the contents of the entry are visible or not. When visibility
 * is set to %FALSE, characters are displayed as the invisible char, and will
 * also appear that way when the text in the entry widget is copied elsewhere.
 *
 * The default invisible char is the asterisk '*', but it can be changed with
 * clutter_entry_set_invisible_char().
 *
 * Since: 0.4
 */
void
clutter_entry_set_visibility (ClutterEntry *entry, gboolean visible)
{
  ClutterEntryPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  priv->text_visible = visible;

  clutter_entry_clear_layout (entry);
  clutter_entry_clear_cursor_position (entry);

  if (CLUTTER_ACTOR_IS_VISIBLE (entry))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));
}

/**
 * clutter_entry_get_visibility:
 * @entry: a #ClutterEntry
 *
 * Returns the entry text visibility.
 *
 * Return value: %TRUE if the contents of the entry are displayed as plaintext.
 *
 * Since: 0.4
 */
gboolean
clutter_entry_get_visibility (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), TRUE);

  return entry->priv->text_visible;
}

/**
 * clutter_entry_set_invisible_char:
 * @entry: a #ClutterEntry
 * @wc: a Unicode character
 *
 * Sets the character to use in place of the actual text when
 * clutter_entry_set_visibility() has been called to set text visibility
 * to %FALSE. i.e. this is the character used in "password mode" to show the
 * user how many characters have been typed. The default invisible char is an
 * asterisk ('*'). If you set the invisible char to 0, then the user will get
 * no feedback at all; there will be no text on the screen as they type.
 *
 * Since: 0.4
 */
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

  if (CLUTTER_ACTOR_IS_VISIBLE (entry))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (entry));
}

/**
 * clutter_entry_get_invisible_char:
 * @entry: a #ClutterEntry
 *
 * Returns the character to use in place of the actual text when text-visibility
 * is set to %FALSE
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

/**
 * clutter_entry_set_max_length:
 * @entry: a #ClutterEntry
 * @max: the maximum number of characters allowed in the entry; 0
 *   to disable or -1 to set the length of the current string
 *
 * Sets the maximum allowed length of the contents of the actor. If the
 * current contents are longer than the given length, then they will be
 * truncated to fit.
 *
 * Since: 0.4
 */
void
clutter_entry_set_max_length (ClutterEntry *entry,
                              gint          max)
{
  ClutterEntryPrivate *priv;
  gchar *new = NULL;

  g_return_if_fail (CLUTTER_IS_ENTRY (entry));

  priv = entry->priv;

  if (priv->max_length != max)
    {
      g_object_ref (entry);

      if (max < 0)
        max = g_utf8_strlen (priv->text, -1);

      priv->max_length = max;

      new = g_strdup (priv->text);
      clutter_entry_set_text (entry, new);
      g_free (new);

      g_object_notify (G_OBJECT (entry), "max-length");
      g_object_unref (entry);
    }
}

/**
 * clutter_entry_get_max_length:
 * @entry: a #ClutterEntry
 *
 * Gets the maximum length of text that can be set into @entry.
 * See clutter_entry_set_max_length().
 *
 * Return value: the maximum number of characters.
 *
 * Since: 0.4
 */
gint
clutter_entry_get_max_length (ClutterEntry *entry)
{
  g_return_val_if_fail (CLUTTER_IS_ENTRY (entry), -1);

  return entry->priv->max_length;
}
