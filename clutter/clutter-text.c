/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006-2008 OpenedHand
 *
 * Authored By Øyvind Kolås <pippin@o-hand.com>
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

/* TODO: undo/redo hooks?
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "clutter-text.h"

#include "clutter-keysyms.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-units.h"

#include "cogl-pango.h"

#define DEFAULT_FONT_NAME	"Sans 10"

/* We need at least three cached layouts to run the allocation without
   regenerating a new layout. First the layout will be generated at
   full width to get the preferred width, then it will be generated at
   the preferred width to get the preferred height and then it might
   be regenerated at a different width to get the height for the
   actual allocated width */
#define N_CACHED_LAYOUTS 3

#define CLUTTER_TEXT_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_TEXT, ClutterTextPrivate))

typedef struct _LayoutCache     LayoutCache;

/* Probably move into main */
static PangoContext *_context = NULL;

static const ClutterColor default_cursor_color = {   0,   0,   0, 255 };
static const ClutterColor default_text_color   = {   0,   0,   0, 255 };

static gboolean clutter_text_key_press          (ClutterActor    *actor,
                                              ClutterKeyEvent *kev);
static gboolean clutter_text_position_to_coords (ClutterText        *ttext,
                                              gint             position,
                                              gint            *x,
                                              gint            *y,
                                              gint            *cursor_height);
static gint     clutter_text_coords_to_position (ClutterText        *text,
                                              gint             x,
                                              gint             y);
static void     clutter_text_set_property       (GObject         *gobject,
                                              guint            prop_id,
                                              const GValue    *value,
                                              GParamSpec      *pspec);
static void     clutter_text_get_property       (GObject         *gobject,
                                              guint            prop_id,
                                              GValue          *value,
                                              GParamSpec      *pspec);
static void     clutter_text_finalize           (GObject         *gobject);

G_CONST_RETURN gchar *clutter_text_get_text (ClutterText *text);
void clutter_text_set_text (ClutterText *text, const gchar *str);
PangoLayout *clutter_text_get_layout (ClutterText *text);
void clutter_text_set_color (ClutterText *text, const ClutterColor *color);
void clutter_text_get_color (ClutterText *text, ClutterColor *color);
void clutter_text_set_font_name (ClutterText *text, const gchar *font_name);
G_CONST_RETURN gchar *clutter_text_get_font_name (ClutterText *text);

static void init_commands (ClutterText *ttext);
static void init_mappings (ClutterText *ttext);

void
clutter_text_delete_text (ClutterText *ttext,
                       gssize    start_pos,
                       gssize    end_pos);

static gboolean
clutter_text_truncate_selection (ClutterText     *ttext,
                              const gchar  *commandline,
                              ClutterEvent *event);

G_DEFINE_TYPE (ClutterText, clutter_text, CLUTTER_TYPE_ACTOR);

struct _LayoutCache
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

struct _ClutterTextPrivate
{
  PangoFontDescription *font_desc;

  gchar *text;
  gchar *font_name;

  ClutterColor text_color;

  LayoutCache cached_layouts[N_CACHED_LAYOUTS];
  guint cache_age;

  PangoAttrList        *attrs;
  PangoAttrList        *effective_attrs;

  guint alignment        : 2;
  guint wrap             : 1;
  guint use_underline    : 1;
  guint use_markup       : 1;
  guint ellipsize        : 3;
  guint single_line_mode : 1;
  guint wrap_mode        : 3;
  guint justify          : 1;
  guint editable         : 1;
  guint cursor_visible   : 1;
  guint activatable      : 1;
  guint selectable       : 1;
  guint in_select_drag   : 1;
  guint cursor_color_set : 1;

  gint            position;   /* current cursor position */
  gint            selection_bound; 
                              /* current 'other end of selection' position */
  gint            x_pos;      /* the x position in the pangolayout, used to
                               * avoid drifting when repeatedly moving up|down
                               */
  ClutterColor    cursor_color;
  ClutterGeometry cursor_pos; /* Where to draw the cursor */

  GList          *mappings;
  GList          *commands; /* each instance has it's own set of commands
                               so that actor specific actions can be added
                               to single actor classes
                              */
};

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
  PROP_POSITION,
  PROP_SELECTION_BOUND,
  PROP_CURSOR_VISIBLE,
  PROP_CURSOR_COLOR,
  PROP_CURSOR_COLOR_SET,
  PROP_EDITABLE,
  PROP_SELECTABLE,
  PROP_ACTIVATABLE
};

enum
{
  TEXT_CHANGED,
  CURSOR_EVENT,
  ACTIVATE,

  LAST_SIGNAL
};

static guint text_signals[LAST_SIGNAL] = { 0, };

#define offset_real(text, pos)                                \
   (pos==-1?g_utf8_strlen(text, -1):pos)                          \

#define offset_to_bytes(text,pos)\
   (pos==-1?strlen(text):((gint)(g_utf8_offset_to_pointer (text, pos) - text)))

#define bytes_to_offset(text, pos)                            \
    (g_utf8_pointer_to_offset (text, text + pos))


typedef struct TextCommand {
  const gchar *name;
  gboolean (*func) (ClutterText     *ttext,
                    const gchar  *commandline,
                    ClutterEvent *event);
} TextCommand;

typedef struct ClutterTextMapping {
  ClutterModifierType    state;
  guint                  keyval;
  const gchar           *action;
} ClutterTextMapping;


void
clutter_text_mappings_clear (ClutterText *ttext)
{
  ClutterTextPrivate *priv = ttext->priv;
  GList *iter;
  for (iter = priv->mappings; iter; iter=iter->next)
    {
      g_free (iter->data);
    }
  g_list_free (priv->mappings);
  priv->mappings = NULL;
}

void clutter_text_add_mapping (ClutterText           *ttext,
                            guint               keyval,
                            ClutterModifierType state,
                            const gchar        *commandline)
{
  ClutterTextMapping *tmapping = g_new (ClutterTextMapping, 1);
  ClutterTextPrivate *priv = ttext->priv;
  tmapping->keyval = keyval;
  tmapping->state = state;
  tmapping->action = commandline;
  priv->mappings = g_list_append (priv->mappings, tmapping);
}

void clutter_text_add_action (ClutterText    *ttext,
                           const gchar *name,
                           gboolean (*func) (ClutterText            *ttext,
                                             const gchar         *commandline,
                                             ClutterEvent        *event))
{
  TextCommand *tcommand = g_new (TextCommand, 1);
  ClutterTextPrivate *priv = ttext->priv;
  tcommand->name = name;
  tcommand->func = func;
  priv->commands = g_list_append (priv->commands, tcommand);
}

static void init_mappings (ClutterText *ttext)
{
  ClutterTextPrivate *priv = ttext->priv;
  if (priv->mappings)
    return;
  clutter_text_add_mapping (ttext, CLUTTER_Left, 0,    "move-left");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Left, 0, "move-left");
  clutter_text_add_mapping (ttext, CLUTTER_Right, 0,   "move-right");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Right, 0,"move-right");
  clutter_text_add_mapping (ttext, CLUTTER_Up, 0,      "move-up");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Up, 0,   "move-up");
  clutter_text_add_mapping (ttext, CLUTTER_Down, 0,    "move-down");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Down, 0, "move-down");
  clutter_text_add_mapping (ttext, CLUTTER_Begin, 0,   "move-start-line");
  clutter_text_add_mapping (ttext, CLUTTER_Home, 0,    "move-start-line");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Home, 0, "move-start-line");
  clutter_text_add_mapping (ttext, CLUTTER_End, 0,     "move-end-line");
  clutter_text_add_mapping (ttext, CLUTTER_KP_End, 0,  "move-end-line");
  clutter_text_add_mapping (ttext, CLUTTER_BackSpace,0,"delete-previous");
  clutter_text_add_mapping (ttext, CLUTTER_Delete, 0,  "delete-next");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Delete,0,"delete-next");
  clutter_text_add_mapping (ttext, CLUTTER_Return, 0,  "activate");
  clutter_text_add_mapping (ttext, CLUTTER_KP_Enter, 0,"activate");
  clutter_text_add_mapping (ttext, CLUTTER_ISO_Enter,0,"activate");
}

static PangoLayout *
clutter_text_create_layout_no_cache (ClutterText *text,
                                     ClutterUnit  allocation_width)
{
  ClutterTextPrivate *priv = text->priv;
  PangoLayout *layout;

  if (G_UNLIKELY (_context == NULL))
    _context = _clutter_context_create_pango_context (CLUTTER_CONTEXT ());

  layout = pango_layout_new (_context);

  pango_layout_set_font_description (layout, priv->font_desc);

  if (priv->effective_attrs)
    pango_layout_set_attributes (layout, priv->effective_attrs);

  pango_layout_set_alignment (layout, priv->alignment);
  pango_layout_set_single_paragraph_mode (layout, priv->single_line_mode);
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
clutter_text_dirty_cache (ClutterText *text)
{
  ClutterTextPrivate *priv = text->priv;
  int i;

  /* Delete the cached layouts so they will be recreated the next time
     they are needed */
  for (i = 0; i < N_CACHED_LAYOUTS; i++)
    if (priv->cached_layouts[i].layout)
      {
	g_object_unref (priv->cached_layouts[i].layout);
	priv->cached_layouts[i].layout = NULL;
      }
}

/*
 * clutter_text_create_layout:
 * @text: a #ClutterText
 * @allocation_width: the allocation width
 *
 * Like clutter_text_create_layout_no_cache(), but will also ensure
 * the glyphs cache. If a previously cached layout generated using the
 * same width is available then that will be used instead of
 * generating a new one.
 */
static PangoLayout *
clutter_text_create_layout (ClutterText *text,
                            ClutterUnit   allocation_width)
{
  ClutterTextPrivate *priv = text->priv;
  LayoutCache *oldest_cache = priv->cached_layouts;
  gboolean found_free_cache = FALSE;
  int i;

  /* Search for a cached layout with the same width and keep track of
     the oldest one */
  for (i = 0; i < N_CACHED_LAYOUTS; i++)
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
	  CLUTTER_NOTE (ACTOR, "ClutterText: %p: cache hit for width %i",
			text,
                        CLUTTER_UNITS_TO_DEVICE (allocation_width));

	  return priv->cached_layouts[i].layout;
	}
      else if (!found_free_cache &&
               (priv->cached_layouts[i].age < oldest_cache->age))
        {
	  oldest_cache = priv->cached_layouts + i;
        }
    }

  CLUTTER_NOTE (ACTOR, "ClutterText: %p: cache miss for width %i",
		text,
                CLUTTER_UNITS_TO_DEVICE (allocation_width));

  /* If we make it here then we didn't have a cached version so we
     need to recreate the layout */
  if (oldest_cache->layout)
    g_object_unref (oldest_cache->layout);

  oldest_cache->layout =
    clutter_text_create_layout_no_cache (text, allocation_width);

  cogl_pango_ensure_glyph_cache_for_layout (oldest_cache->layout);

  /* Mark the 'time' this cache was created and advance the time */
  oldest_cache->age = priv->cache_age++;
  oldest_cache->width = allocation_width;

  return oldest_cache->layout;
}

static gint
clutter_text_coords_to_position (ClutterText *text,
                                 gint      x,
                                 gint      y)
{
  gint index_;
  gint px, py;
  gint trailing;

  px = x * PANGO_SCALE;
  py = y * PANGO_SCALE;

  pango_layout_xy_to_index (clutter_text_get_layout (text),
                            px, py, &index_, &trailing);

  return index_ + trailing;
}

static gboolean
clutter_text_position_to_coords (ClutterText *ttext,
                              gint      position,
                              gint     *x,
                              gint     *y,
                              gint     *cursor_height)
{
  ClutterTextPrivate  *priv;
  gint              index_;
  PangoRectangle    rect;
  const gchar      *text;

  text = clutter_text_get_text (ttext);

  priv = ttext->priv;

  if (position == -1)
    {
      index_ = strlen (text);
    }
  else
    {
      index_ = offset_to_bytes (text, position);
    }

  if (index_ > strlen (text))
   index_ = strlen (text);

  pango_layout_get_cursor_pos (
        clutter_text_get_layout (ttext),
        index_, &rect, NULL);

  if (x)
    *x = rect.x / PANGO_SCALE;
  if (y)
    *y = (rect.y + rect.height) / PANGO_SCALE;
  if (cursor_height)
    *cursor_height = rect.height / PANGO_SCALE;

  return TRUE; /* FIXME: should return false if coords were outside text */
}

static void
clutter_text_ensure_cursor_position (ClutterText *ttext)
{
  gint x,y,cursor_height;
  
  ClutterTextPrivate  *priv;
  priv = ttext->priv;

  clutter_text_position_to_coords (ttext, priv->position, &x, &y, &cursor_height);

  priv->cursor_pos.x = x;
  priv->cursor_pos.y = y - cursor_height;
  priv->cursor_pos.width = 2; 
  priv->cursor_pos.height = cursor_height;

  g_signal_emit (ttext, text_signals[CURSOR_EVENT], 0, &priv->cursor_pos);
}

gint
clutter_text_get_cursor_position (ClutterText *ttext)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (ttext), -1);
  return ttext->priv->position;
}

void
clutter_text_set_cursor_position (ClutterText *ttext,
                               gint       position)
{
  const gchar         *text;
  ClutterTextPrivate *priv;
  gint len;

  g_return_if_fail (CLUTTER_IS_TEXT (ttext));

  priv = ttext->priv;

  text = clutter_text_get_text (ttext);
  if (text == NULL)
    return;

  len = g_utf8_strlen (text, -1);

  if (position < 0 || position >= len)
    priv->position = -1;
  else
    priv->position = position;

  if (CLUTTER_ACTOR_IS_VISIBLE (ttext))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (ttext));
}

static gboolean
clutter_text_truncate_selection (ClutterText     *ttext,
                              const gchar  *commandline,
                              ClutterEvent *event)
{
  const gchar *utf8 = clutter_text_get_text (ttext);
  ClutterTextPrivate *priv;
  gint             start_index;
  gint             end_index;

  priv = ttext->priv;

  g_object_ref (ttext);

  start_index = offset_real (utf8, priv->position);
  end_index = offset_real (utf8, priv->selection_bound);

  if (end_index == start_index)
    return FALSE;

  if (end_index < start_index)
    {
      gint temp = start_index;
      start_index = end_index;
      end_index = temp;
    }

  clutter_text_delete_text (ttext, start_index, end_index);
  priv->position = start_index;
  priv->selection_bound = start_index;
  return TRUE;
}


void
clutter_text_insert_unichar (ClutterText *ttext,
                           gunichar   wc)
{
  ClutterTextPrivate *priv;
  GString *new = NULL;
  const gchar *old_text;
  glong pos;

  g_return_if_fail (CLUTTER_IS_TEXT (ttext));
  g_return_if_fail (g_unichar_validate (wc));

  if (wc == 0)
    return;

  clutter_text_truncate_selection (ttext, NULL, 0);

  priv = ttext->priv;

  g_object_ref (ttext);

  old_text = clutter_text_get_text (ttext);


  new = g_string_new (old_text);
  pos = offset_to_bytes (old_text, priv->position);
  new = g_string_insert_unichar (new, pos, wc);

  clutter_text_set_text (ttext, new->str);

  if (priv->position >= 0)
    {
      clutter_text_set_cursor_position (ttext, priv->position + 1);
      clutter_text_set_selection_bound (ttext, priv->position);
    }

  g_string_free (new, TRUE);

  g_object_unref (ttext);

  g_signal_emit (G_OBJECT (ttext), text_signals[TEXT_CHANGED], 0);
}

void
clutter_text_delete_text (ClutterText *ttext,
                       gssize    start_pos,
                       gssize    end_pos)
{
  ClutterTextPrivate *priv;
  GString *new = NULL;
  gint start_bytes;
  gint end_bytes;
  const gchar *text;

  g_return_if_fail (CLUTTER_IS_TEXT (ttext));

  priv = ttext->priv;
  text = clutter_text_get_text (ttext);

  if (end_pos == -1)
    {
      start_bytes = offset_to_bytes (text, g_utf8_strlen (text, -1) - 1);
      end_bytes = offset_to_bytes (text, g_utf8_strlen (text, -1));
    }
  else
    {
      start_bytes = offset_to_bytes (text, start_pos);
      end_bytes = offset_to_bytes (text, end_pos);
    }

  new = g_string_new (text);

  new = g_string_erase (new, start_bytes, end_bytes - start_bytes);

  clutter_text_set_text (ttext, new->str);

  g_string_free (new, TRUE);
  g_signal_emit (G_OBJECT (ttext), text_signals[TEXT_CHANGED], 0);
}

static void
clutter_text_finalize (GObject *gobject)
{
  ClutterTextPrivate *priv;
  ClutterText        *ttext;
  GList           *iter;

  ttext = CLUTTER_TEXT (gobject);
  priv = ttext->priv;

  clutter_text_mappings_clear (ttext);

  for (iter = priv->commands; iter; iter=iter->next)
      g_free (iter->data);
  g_list_free (priv->commands);
  priv->commands = NULL;
}

static void
clutter_text_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  ClutterTextPrivate *priv;
  ClutterText        *ttext;

  ttext = CLUTTER_TEXT (gobject);
  priv = ttext->priv;

  switch (prop_id)
    {
    case PROP_TEXT:
      clutter_text_set_text (ttext, g_value_get_string (value));
      break;
    case PROP_COLOR:
      clutter_text_set_color (ttext, clutter_value_get_color (value));
      break;
    case PROP_FONT_NAME:
      clutter_text_set_font_name (ttext, g_value_get_string (value));
      break;
    case PROP_POSITION:
      clutter_text_set_cursor_position (ttext, g_value_get_int (value));
      break;
    case PROP_SELECTION_BOUND:
      clutter_text_set_selection_bound (ttext, g_value_get_int (value));
      break;
    case PROP_CURSOR_VISIBLE:
      clutter_text_set_cursor_visible (ttext, g_value_get_boolean (value));
      break;
    case PROP_CURSOR_COLOR:
      clutter_text_set_cursor_color (ttext, g_value_get_boxed (value));
      break;
    case PROP_EDITABLE:
      clutter_text_set_editable (ttext, g_value_get_boolean (value));
      break;
    case PROP_ACTIVATABLE:
      clutter_text_set_activatable (ttext, g_value_get_boolean (value));
      break;
    case PROP_SELECTABLE:
      clutter_text_set_selectable (ttext, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_text_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  ClutterTextPrivate *priv;

  priv = CLUTTER_TEXT (gobject)->priv;

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_FONT_NAME:
      g_value_set_string (value, priv->font_name);
      break;
    case PROP_COLOR:
      clutter_value_set_color (value, &priv->text_color);
      break;
    case PROP_CURSOR_VISIBLE:
      g_value_set_boolean (value, priv->cursor_visible);
      break;
    case PROP_CURSOR_COLOR:
      clutter_value_set_color (value, &priv->cursor_color);
      break;
    case PROP_POSITION:
      g_value_set_int (value, CLUTTER_FIXED_TO_FLOAT (priv->position));
      break;
    case PROP_SELECTION_BOUND:
      g_value_set_int (value, CLUTTER_FIXED_TO_FLOAT (priv->selection_bound));
      break;
    case PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, priv->selectable);
      break;
    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, priv->activatable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
cursor_paint (ClutterText *ttext)
{
  ClutterTextPrivate *priv   = ttext->priv;

  if (priv->editable &&
      priv->cursor_visible)
    {
      if (priv->cursor_color_set)
        {
          cogl_set_source_color4ub (priv->cursor_color.red,
                                    priv->cursor_color.green,
                                    priv->cursor_color.blue,
                                    priv->cursor_color.alpha);
        }
      else
        {
          ClutterColor color;
          clutter_text_get_color (ttext, &color);
          cogl_set_source_color4ub (color.red,
                                    color.green,
                                    color.blue,
                                    color.alpha);
        }

      clutter_text_ensure_cursor_position (ttext);

      if (priv->position == 0)
        priv->cursor_pos.x -= 2;

      if (priv->position == priv->selection_bound)
        {
          cogl_rectangle (priv->cursor_pos.x,
                          priv->cursor_pos.y,
                          priv->cursor_pos.width,
                          priv->cursor_pos.height);
        }
      else
        {
          gint lines;
          gint start_index;
          gint end_index;
          const gchar *utf8 = clutter_text_get_text (ttext);

          start_index = offset_to_bytes (utf8, priv->position);
          end_index = offset_to_bytes (utf8, priv->selection_bound);

          if (start_index > end_index)
            {
              gint temp = start_index;
              start_index = end_index;
              end_index = temp;
            }
          
          PangoLayout *layout = clutter_text_get_layout (ttext);
          lines = pango_layout_get_line_count (layout);
          gint line_no;
          for (line_no = 0; line_no < lines; line_no++)
            {
              PangoLayoutLine *line;
              gint  n_ranges;
              gint *ranges;
              gint  i;
              gint y;
              gint height;
              gint index;
              gint maxindex;

              line = pango_layout_get_line_readonly (layout, line_no);
              pango_layout_line_x_to_index (line, G_MAXINT, &maxindex, NULL);
              if (maxindex < start_index)
                continue;

              pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);
              pango_layout_line_x_to_index (line, 0, &index, NULL);

              clutter_text_position_to_coords (ttext, bytes_to_offset (utf8, index), NULL, &y, &height);

              for (i=0;i<n_ranges;i++)
                {
                  cogl_rectangle (ranges[i*2+0]/PANGO_SCALE,
                                  y-height,
                                  (ranges[i*2+1]-ranges[i*2+0])/PANGO_SCALE,
                                  height);
                }

              g_free (ranges);

            }
      }
    }
}



static gboolean
clutter_text_press (ClutterActor       *actor,
                 ClutterButtonEvent *bev)
{
  ClutterText *ttext = CLUTTER_TEXT (actor);
  ClutterTextPrivate *priv = ttext->priv;
  ClutterUnit           x, y;
  gint                  index_;
  const gchar          *text;

  text = clutter_text_get_text (ttext);

  x = CLUTTER_UNITS_FROM_INT (bev->x);
  y = CLUTTER_UNITS_FROM_INT (bev->y);

  clutter_actor_transform_stage_point (actor, x, y, &x, &y);

  index_ = clutter_text_coords_to_position (ttext, CLUTTER_UNITS_TO_INT (x),
                                                CLUTTER_UNITS_TO_INT (y));

  clutter_text_set_cursor_position (ttext, bytes_to_offset (text, index_));
  clutter_text_set_selection_bound (ttext, bytes_to_offset (text, index_)
    );

  /* we'll steal keyfocus if we do not have it */
  {
    ClutterActor *stage;
    for (stage = actor;
         clutter_actor_get_parent (stage);
         stage = clutter_actor_get_parent (stage));
    if (stage && CLUTTER_IS_STAGE (stage))
      clutter_stage_set_key_focus (CLUTTER_STAGE (stage), actor);
  }

  priv->in_select_drag = TRUE;
  clutter_grab_pointer (actor);

  return TRUE;
}


static gboolean
clutter_text_motion (ClutterActor       *actor,
                  ClutterMotionEvent *mev)
{
  ClutterText *ttext = CLUTTER_TEXT (actor);
  ClutterTextPrivate *priv = ttext->priv;
  ClutterUnit           x, y;
  gint                  index_;
  const gchar          *text;

  if (!priv->in_select_drag)
    {
      return FALSE;
    }

  text = clutter_text_get_text (ttext);

  x = CLUTTER_UNITS_FROM_INT (mev->x);
  y = CLUTTER_UNITS_FROM_INT (mev->y);

  clutter_actor_transform_stage_point (actor, x, y, &x, &y);

  index_ = clutter_text_coords_to_position (ttext, CLUTTER_UNITS_TO_INT (x),
                                                CLUTTER_UNITS_TO_INT (y));

  if (priv->selectable)
    {
      clutter_text_set_cursor_position (ttext, bytes_to_offset (text, index_));
    }
  else
    {
      clutter_text_set_cursor_position (ttext, bytes_to_offset (text, index_));
      clutter_text_set_selection_bound (ttext, bytes_to_offset (text, index_));
    }

  return TRUE;
}

static gboolean
clutter_text_release (ClutterActor       *actor,
                   ClutterButtonEvent *bev)
{
  ClutterText *ttext = CLUTTER_TEXT (actor);
  ClutterTextPrivate *priv = ttext->priv;
  if (priv->in_select_drag)
    {
      clutter_ungrab_pointer ();
      priv->in_select_drag = FALSE;
      return TRUE;
    }
  return FALSE;
}

static void
clutter_text_paint (ClutterActor *self)
{
  ClutterText *text = CLUTTER_TEXT (self);
  ClutterTextPrivate *priv = text->priv;
  PangoLayout *layout;
  ClutterActorBox alloc = { 0, };
  CoglColor color = { 0, };

  if (priv->font_desc == NULL || priv->text == NULL)
    {
      CLUTTER_NOTE (ACTOR, "desc: %p, text %p",
		    priv->font_desc ? priv->font_desc : 0x0,
		    priv->text ? priv->text : 0x0);
      return;
    }

  cursor_paint (text);

  CLUTTER_NOTE (PAINT, "painting text (text:`%s')", priv->text);

  clutter_actor_get_allocation_box (self, &alloc);
  layout = clutter_text_create_layout (text, alloc.x2 - alloc.x1);

  cogl_color_set_from_4ub (&color,
                           priv->text_color.red,
                           priv->text_color.green,
                           priv->text_color.blue,
                           clutter_actor_get_paint_opacity (self));
  cogl_pango_render_layout (layout, 0, 0, &color, 0);
}

static void
clutter_text_get_preferred_width (ClutterActor *self,
                                   ClutterUnit   for_height,
                                   ClutterUnit  *min_width_p,
                                   ClutterUnit  *natural_width_p)
{
  ClutterText *text = CLUTTER_TEXT (self);
  ClutterTextPrivate *priv = text->priv;
  PangoRectangle logical_rect = { 0, };
  PangoLayout *layout;
  ClutterUnit layout_width;

  layout = clutter_text_create_layout (text, -1);

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
clutter_text_get_preferred_height (ClutterActor *self,
                                    ClutterUnit   for_width,
                                    ClutterUnit  *min_height_p,
                                    ClutterUnit  *natural_height_p)
{
  ClutterText *text = CLUTTER_TEXT (self);

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

      layout = clutter_text_create_layout (text, for_width);

      pango_layout_get_extents (layout, NULL, &logical_rect);
      height = CLUTTER_UNITS_FROM_PANGO_UNIT (logical_rect.height);

      if (min_height_p)
        *min_height_p = height;

      if (natural_height_p)
        *natural_height_p = height;
    }
}

static void
clutter_text_allocate (ClutterActor          *self,
                        const ClutterActorBox *box,
                        gboolean               origin_changed)
{
  ClutterText *text = CLUTTER_TEXT (self);
  ClutterActorClass *parent_class;

  /* Ensure that there is a cached layout with the right width so that
     we don't need to create the text during the paint run */
  clutter_text_create_layout (text, box->x2 - box->x1);

  parent_class = CLUTTER_ACTOR_CLASS (clutter_text_parent_class);
  parent_class->allocate (self, box, origin_changed);
}

static void
clutter_text_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (clutter_text_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (clutter_text_parent_class)->constructed (object);
}

static void
clutter_text_class_init (ClutterTextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_text_set_property;
  gobject_class->get_property = clutter_text_get_property;
  gobject_class->constructed = clutter_text_constructed;
  gobject_class->finalize = clutter_text_finalize;

  actor_class->paint = clutter_text_paint;
  actor_class->get_preferred_width = clutter_text_get_preferred_width;
  actor_class->get_preferred_height = clutter_text_get_preferred_height;
  actor_class->allocate = clutter_text_allocate;
  actor_class->key_press_event = clutter_text_key_press;
  actor_class->button_press_event = clutter_text_press;
  actor_class->button_release_event = clutter_text_release;
  actor_class->motion_event = clutter_text_motion;

  /**
   * ClutterText:font-name:
   *
   * The font to be used by the #ClutterText, as a string
   * that can be parsed by pango_font_description_from_string().
   *
   * Since: 0.2
   */
  pspec = g_param_spec_string ("font-name",
                               "Font Name",
                               "The font to be used by the text",
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
                                    "Color of the font used by the text",
                                    &default_text_color,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);

  /**
   * ClutterText:editable:
   *
   * Whether key events delivered to the actor causes editing.
   */
  g_object_class_install_property
    (gobject_class, PROP_EDITABLE,
     g_param_spec_boolean ("editable",
			"Editable",
			"Whether the text is editable",
			TRUE,
			G_PARAM_READWRITE));


  /**
   * ClutterText:selectable:
   *
   * Whether it is possible to select text.
   */
  g_object_class_install_property
    (gobject_class, PROP_SELECTABLE,
     g_param_spec_boolean ("selectable",
			"Editable",
			"Whether the text is selectable",
			TRUE,
			G_PARAM_READWRITE));

  /**
   * ClutterText:activatable:
   *
   * Toggles whether return invokes the activate signal or not.
   */
  g_object_class_install_property
    (gobject_class, PROP_ACTIVATABLE,
     g_param_spec_boolean ("activatable",
			"Editable",
			"Whether return causes the activate signal to be fired",
			TRUE,
			G_PARAM_READWRITE));

  /**
   * ClutterText:cursor-visible:
   *
   * Whether the input cursor is visible or not, it will only be visible
   * if both cursor-visible is set and editable is set at the same time,
   * the value defaults to TRUE.
   */
  g_object_class_install_property
    (gobject_class, PROP_CURSOR_VISIBLE,
     g_param_spec_boolean ("cursor-visible",
			"Cursor Visible",
			"Whether the input cursor is visible",
			TRUE,
			G_PARAM_READWRITE));


  g_object_class_install_property
    (gobject_class, PROP_CURSOR_COLOR,
     g_param_spec_boxed ("cursor-color",
			 "Cursor Colour",
			 "Cursor  Colour",
			 CLUTTER_TYPE_COLOR,
			 G_PARAM_READWRITE));

  /**
   * ClutterText:position:
   *
   * The current input cursor position. -1 is taken to be the end of the text
   */
  g_object_class_install_property
    (gobject_class, PROP_POSITION,
     g_param_spec_int ("position",
                       "Position",
                       "The cursor position",
                       -1, G_MAXINT,
                       -1,
                       G_PARAM_READWRITE));


  /**
   * ClutterText:selection-bound:
   *
   * The current input cursor position. -1 is taken to be the end of the text
   */
  g_object_class_install_property
    (gobject_class, PROP_SELECTION_BOUND,
     g_param_spec_int ("selection-bound",
                       "Selection-bound",
                       "The cursor position of the other end of the selection.",
                       -1, G_MAXINT,
                       -1,
                       G_PARAM_READWRITE));

 /**
   * ClutterText::text-changed:
   * @actor: the actor which received the event
   *
   * The ::text-changed signal is emitted after @entry's text changes
   */
  text_signals[TEXT_CHANGED] =
    g_signal_new ("text-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterTextClass, text_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);


  text_signals[CURSOR_EVENT] =
    g_signal_new ("cursor-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTextClass, cursor_event),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_GEOMETRY | G_SIGNAL_TYPE_STATIC_SCOPE);


 /**
   * ClutterText::activate
   * @actor: the actor which received the event
   *
   * The ::activate signal is emitted each time the entry is 'activated'
   * by the user, normally by pressing the 'Enter' key.
   *
   * Since: 0.4
   */
  text_signals[ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterTextClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);


  g_type_class_add_private (klass, sizeof (ClutterTextPrivate));
}

static void
clutter_text_init (ClutterText *self)
{
  ClutterTextPrivate *priv;

  self->priv = priv = CLUTTER_TEXT_GET_PRIVATE (self);

  priv->x_pos = -1;
  priv->cursor_visible = TRUE;
  priv->editable = FALSE;

  priv->cursor_color_set = FALSE;

  init_commands (self); /* FIXME: free */
  init_mappings (self); /* FIXME: free */
}

ClutterActor *
clutter_text_new_full (const gchar        *font_name,
                       const gchar        *text,
                       const ClutterColor *color)
{
  return g_object_new (CLUTTER_TYPE_TEXT,
                       "font-name", font_name,
                       "text", text,
                       "color", color,
                       NULL);
}

ClutterActor *
clutter_text_new_with_text (const gchar *font_name,
                         const gchar *text)
{
  return g_object_new (CLUTTER_TYPE_TEXT,
                       "font-name", font_name,
                       "text", text,
                       NULL);
}


void
clutter_text_set_editable (ClutterText *text,
                           gboolean     editable)
{
  ClutterTextPrivate *priv;

  priv = text->priv;
  priv->editable = editable;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (text));
}

gboolean
clutter_text_get_editable (ClutterText *text)
{
  ClutterTextPrivate *priv;
  priv = text->priv;
  return priv->editable;
}


void
clutter_text_set_selectable (ClutterText *text,
                          gboolean  selectable)
{
  ClutterTextPrivate *priv;

  priv = text->priv;
  priv->selectable = selectable;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (text));
}

gboolean
clutter_text_get_selectable (ClutterText *text)
{
  ClutterTextPrivate *priv;
  priv = text->priv;
  return priv->selectable;
}


void
clutter_text_set_activatable (ClutterText *text,
                           gboolean  activatable)
{
  ClutterTextPrivate *priv;

  priv = text->priv;
  priv->activatable = activatable;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (text));
}

gboolean
clutter_text_get_activatable (ClutterText *text)
{
  ClutterTextPrivate *priv;
  priv = text->priv;
  return priv->activatable;
}


void
clutter_text_set_cursor_visible (ClutterText *text,
                              gboolean  cursor_visible)
{
  ClutterTextPrivate *priv;

  priv = text->priv;
  priv->cursor_visible = cursor_visible;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (text));
}

gboolean
clutter_text_get_cursor_visible (ClutterText *text)
{
  ClutterTextPrivate *priv;
  priv = text->priv;
  return priv->cursor_visible;
}

void
clutter_text_set_cursor_color (ClutterText           *text,
		            const ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (text));
  g_return_if_fail (color != NULL);

  priv = text->priv;

  g_object_ref (text);

  if (color)
    {
      priv->cursor_color = *color;
      priv->cursor_color_set = TRUE;
    }
  else
    {
      priv->cursor_color_set = FALSE;
    }
}


void
clutter_text_get_cursor_color (ClutterText     *text,
                            ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (text));
  g_return_if_fail (color != NULL);

  priv = text->priv;

  *color = priv->cursor_color;
}


gint
clutter_text_get_selection_bound (ClutterText *text)
{
  ClutterTextPrivate *priv;
 
  priv = text->priv;

  return priv->selection_bound;
}

gchar *
clutter_text_get_selection (ClutterText *text)
{
  ClutterTextPrivate *priv;
 
  const gchar *utf8 = clutter_text_get_text (text);
  gchar       *str;
  gint         len;
  gint         start_index;
  gint         end_index;
  gint         start_offset;
  gint         end_offset;

  priv = text->priv;

  start_index = priv->position;
  end_index = priv->selection_bound;

  if (end_index == start_index)
    return g_strdup ("");
  if (end_index < start_index)
    {
      gint temp = start_index;
      start_index = end_index;
      end_index = temp;
    }

  start_offset = offset_to_bytes (utf8, start_index);
  end_offset = offset_to_bytes (utf8, end_index);
  len = end_offset - start_offset;

  str = g_malloc (len + 1);
  g_utf8_strncpy (str, utf8 + start_offset, end_index-start_index);
  return str;
}



void
clutter_text_set_selection_bound (ClutterText *text,
                               gint      selection_bound)
{
  ClutterTextPrivate *priv;

  priv = text->priv;
  priv->selection_bound = selection_bound;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (text));
}


/****************************************************************/
/* The following are the commands available for keybinding when */
/* using the entry, these can also be invoked programmatically  */
/* through clutter_text_action()                                   */
/****************************************************************/

static gboolean
clutter_text_action_activate (ClutterText            *ttext,
                           const gchar         *commandline,
                           ClutterEvent *event)
{
  g_signal_emit (G_OBJECT (ttext), text_signals[ACTIVATE], 0);
  return TRUE;
}

static void
clutter_text_clear_selection (ClutterText *ttext)
{
  ClutterTextPrivate *priv = ttext->priv;
  priv->selection_bound = priv->position;
}

static gboolean
clutter_text_action_move_left (ClutterText     *ttext,
                            const gchar  *commandline,
                            ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;
  gint pos = priv->position;
  gint len;
  len = g_utf8_strlen (clutter_text_get_text (ttext), -1);

  if (pos != 0 && len !=0)
    {
      if (pos == -1)
        {
          clutter_text_set_cursor_position (ttext, len - 1);
        }
      else
        {
          clutter_text_set_cursor_position (ttext, pos - 1);
        }
    }

  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);

  return TRUE;
}


static gboolean
clutter_text_action_move_right (ClutterText            *ttext,
                             const gchar         *commandline,
                             ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;
  gint pos;
 
  gint len;
  len = g_utf8_strlen (clutter_text_get_text (ttext), -1);

  pos = priv->position;
  if (pos != -1 && len !=0)
    {
      if (pos != len)
        {
          clutter_text_set_cursor_position (ttext, pos + 1);
        }
    }

  if (!(priv->selectable &&
        event &&
       (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    {
      clutter_text_clear_selection (ttext);
    }

  return TRUE;
}

static gboolean
clutter_text_action_move_up (ClutterText            *ttext,
                          const gchar         *commandline,
                          ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;
  gint                          line_no;
  gint                          index_;
  gint                          x;
  const gchar                  *text;
  PangoLayoutLine              *layout_line;

  text = clutter_text_get_text (ttext);

  pango_layout_index_to_line_x (
        clutter_text_get_layout (ttext),
        offset_to_bytes (text, priv->position),
        0,
        &line_no,
        &x);
  
  if (priv->x_pos != -1)
    x = priv->x_pos;
  else
    priv->x_pos = x;

  line_no -= 1;
  if (line_no < 0)
    return FALSE;

  layout_line = pango_layout_get_line_readonly (
                    clutter_text_get_layout (ttext),
                    line_no);

  if (!layout_line)
    return TRUE;

  pango_layout_line_x_to_index (layout_line, x, &index_, NULL);

  {
    gint pos = bytes_to_offset (text, index_);
    clutter_text_set_cursor_position (ttext, pos);
  }

  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);

  return TRUE;
}

static gboolean
clutter_text_action_move_down (ClutterText            *ttext,
                            const gchar         *commandline,
                            ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;
  gint                          line_no;
  gint                          index_;
  gint                          x;
  const gchar                  *text;
  PangoLayoutLine              *layout_line;

  text = clutter_text_get_text (ttext);

  pango_layout_index_to_line_x (
        clutter_text_get_layout (ttext),
        offset_to_bytes (text, priv->position),
        0,
        &line_no,
        &x);
  
  if (priv->x_pos != -1)
    x = priv->x_pos;
  else
    priv->x_pos = x;

  layout_line = pango_layout_get_line_readonly (
                    clutter_text_get_layout (ttext),
                    line_no + 1);

  if (!layout_line)
    {
      return FALSE;
    }

  pango_layout_line_x_to_index (layout_line, x, &index_, NULL);

    {
      gint pos = bytes_to_offset (text, index_);
      clutter_text_set_cursor_position (ttext, pos);
    }
  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);
  return TRUE;
}

static gboolean
clutter_text_action_move_start (ClutterText            *ttext,
                             const gchar         *commandline,
                             ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;

  clutter_text_set_cursor_position (ttext, 0);
  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);
  return TRUE;
}

static gboolean
clutter_text_action_move_end (ClutterText            *ttext,
                           const gchar         *commandline,
                           ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;

  clutter_text_set_cursor_position (ttext, -1);
  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);

  return TRUE;
}

static gboolean
clutter_text_action_move_start_line (ClutterText            *ttext,
                                  const gchar         *commandline,
                                  ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;
  gint                          line_no;
  gint                          index_;
  const gchar                  *text;
  PangoLayoutLine              *layout_line;
  gint                          position;

  text = clutter_text_get_text (ttext);


  pango_layout_index_to_line_x (
        clutter_text_get_layout (ttext),
        offset_to_bytes (text, priv->position),
        0,
        &line_no,
        NULL);

  layout_line = pango_layout_get_line_readonly (
                    clutter_text_get_layout (ttext),
                    line_no);

  pango_layout_line_x_to_index (layout_line, 0, &index_, NULL);

  position = bytes_to_offset (text, index_);
  clutter_text_set_cursor_position (ttext, position);

  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);

  return TRUE;
}

static gboolean
clutter_text_action_move_end_line (ClutterText            *ttext,
                                const gchar         *commandline,
                                ClutterEvent *event)
{
  ClutterTextPrivate *priv = ttext->priv;
  gint                          line_no;
  gint                          index_;
  gint                          trailing;
  const gchar                  *text;
  PangoLayoutLine              *layout_line;
  gint                          position;

  text = clutter_text_get_text (ttext);

  index_ = offset_to_bytes (text, priv->position);

  pango_layout_index_to_line_x (
        clutter_text_get_layout (ttext),
        index_,
        0,
        &line_no,
        NULL);

  layout_line = pango_layout_get_line_readonly (
                    clutter_text_get_layout (ttext),
                    line_no);

  pango_layout_line_x_to_index (layout_line, G_MAXINT, &index_, &trailing);
  index_ += trailing;

  position = bytes_to_offset (text, index_);

  clutter_text_set_cursor_position (ttext, position);

  if (!(priv->selectable && event &&
      (event->key.modifier_state & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (ttext);

  return TRUE;
}

static gboolean
clutter_text_action_delete_next (ClutterText *ttext,
                              const gchar         *commandline,
                              ClutterEvent *event)
{
  ClutterTextPrivate *priv;
  gint pos;
  gint len;
 
  if (clutter_text_truncate_selection (ttext, NULL, 0))
    return TRUE;
  priv = ttext->priv;
  pos = priv->position;
  len = g_utf8_strlen (clutter_text_get_text (ttext), -1);

  if (len && pos != -1 && pos < len)
    clutter_text_delete_text (ttext, pos, pos+1);;
  return TRUE;
}

static gboolean
clutter_text_action_delete_previous (ClutterText            *ttext,
                                  const gchar         *commandline,
                                  ClutterEvent *event)
{
  ClutterTextPrivate *priv;
  gint pos;
  gint len;
 
  if (clutter_text_truncate_selection (ttext, NULL, 0))
    return TRUE;
  priv = ttext->priv;
  pos = priv->position;
  len = g_utf8_strlen (clutter_text_get_text (ttext), -1);

  if (pos != 0 && len !=0)
    {
      if (pos == -1)
        {
          clutter_text_set_cursor_position (ttext, len - 1);
          clutter_text_set_selection_bound (ttext, len - 1);
        }
      else
        {
          clutter_text_set_cursor_position (ttext, pos - 1);
          clutter_text_set_selection_bound (ttext, pos - 1);
        }
      clutter_text_delete_text (ttext, pos-1, pos);;
    }
  return TRUE;
}


static void init_commands (ClutterText *ttext)
{
  ClutterTextPrivate *priv = ttext->priv;
  if (priv->commands)
    return;
  clutter_text_add_action (ttext, "move-left",       clutter_text_action_move_left);
  clutter_text_add_action (ttext, "move-right",      clutter_text_action_move_right);
  clutter_text_add_action (ttext, "move-up",         clutter_text_action_move_up);
  clutter_text_add_action (ttext, "move-down",       clutter_text_action_move_down);
  clutter_text_add_action (ttext, "move-start",      clutter_text_action_move_start);
  clutter_text_add_action (ttext, "move-end",        clutter_text_action_move_end);
  clutter_text_add_action (ttext, "move-start-line", clutter_text_action_move_start_line);
  clutter_text_add_action (ttext, "move-end-line",   clutter_text_action_move_end_line);
  clutter_text_add_action (ttext, "delete-previous", clutter_text_action_delete_previous);
  clutter_text_add_action (ttext, "delete-next",     clutter_text_action_delete_next);
  clutter_text_add_action (ttext, "activate",        clutter_text_action_activate);
  clutter_text_add_action (ttext, "truncate-selection", clutter_text_truncate_selection);
}

gboolean
clutter_text_action (ClutterText     *ttext,
                  const gchar  *command,
                  ClutterEvent *event)
{
  gchar command2[64];
  gint i;
  
  GList *iter;
  ClutterTextPrivate *priv = ttext->priv;

  for (i=0; command[i] &&
            command[i]!=' '&&
            i<62; i++)
    {
      command2[i]=command[i];
    }
  command2[i]='\0';

  if (!g_str_equal (command2, "move-up") &&
      !g_str_equal (command2, "move-down"))
    priv->x_pos = -1;

  for (iter=priv->commands;iter;iter=iter->next)
    {
      TextCommand *tcommand = iter->data;
      if (g_str_equal (command2, tcommand->name))
        return tcommand->func (ttext, command, event);
    }

  g_warning ("unhandled text command %s", command);
  return FALSE;
}

static gboolean
clutter_text_key_press (ClutterActor    *actor,
                     ClutterKeyEvent *kev)
{
  ClutterTextPrivate *priv   = CLUTTER_TEXT (actor)->priv;
  gint             keyval = clutter_key_event_symbol (kev);
  GList           *iter;

  if (!priv->editable)
    return FALSE;

  for (iter=priv->mappings;iter;iter=iter->next)
    {
      ClutterTextMapping *mapping = iter->data;
      
      if (
          (mapping->keyval == keyval) &&
            (
             (mapping->state == 0) || 
             (mapping->state && (kev->modifier_state & mapping->state)) 
            ) 
         )
        {
          if (!g_str_equal (mapping->action, "activate") ||
              priv->activatable)
            return clutter_text_action (CLUTTER_TEXT (actor), mapping->action, (ClutterEvent*)kev);
        }
    }

    {
      gunichar key_unichar = clutter_key_event_unicode (kev);

      if (key_unichar == '\r')  /* return is reported as CR we want LF */
        key_unichar = '\n';
      if (g_unichar_validate (key_unichar))
        {
          clutter_text_insert_unichar (CLUTTER_TEXT (actor), key_unichar);
          return TRUE;
        }
    }
  return FALSE;
}

G_CONST_RETURN gchar *
clutter_text_get_font_name (ClutterText *text)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (text), NULL);

  return text->priv->font_name;
}

void
clutter_text_set_font_name (ClutterText *text,
                            const gchar *font_name)
{
  ClutterTextPrivate *priv;
  PangoFontDescription *desc;

  g_return_if_fail (CLUTTER_IS_TEXT (text));

  if (!font_name || font_name[0] == '\0')
    font_name = DEFAULT_FONT_NAME;

  priv = text->priv;

  if (priv->font_name && strcmp (priv->font_name, font_name) == 0)
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

  clutter_text_dirty_cache (text);

  if (priv->text && priv->text[0] != '\0')
    clutter_actor_queue_relayout (CLUTTER_ACTOR (text));

  g_object_notify (G_OBJECT (text), "font-name");
}

G_CONST_RETURN gchar *
clutter_text_get_text (ClutterText *text)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (text), NULL);

  return text->priv->text;
}

void
clutter_text_set_text (ClutterText *text,
                       const gchar *str)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (text));

  priv = text->priv;

  g_free (priv->text);
  priv->text = g_strdup (str);

  clutter_text_dirty_cache (text);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (text));

  g_object_notify (G_OBJECT (text), "text");
}

PangoLayout *
clutter_text_get_layout (ClutterText *text)
{
  ClutterUnit width;

  g_return_val_if_fail (CLUTTER_IS_TEXT (text), NULL);

  width = clutter_actor_get_widthu (CLUTTER_ACTOR (text));

  return clutter_text_create_layout (text, width);
}

void
clutter_text_set_color (ClutterText        *text,
                        const ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (text));
  g_return_if_fail (color != NULL);

  priv = text->priv;

  priv->text_color = *color;

  if (CLUTTER_ACTOR_IS_VISIBLE (text))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (text));

  g_object_notify (G_OBJECT (text), "color");
}

void
clutter_text_get_color (ClutterText  *text,
                        ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (text));
  g_return_if_fail (color != NULL);

  priv = text->priv;

  *color = priv->text_color;
}

gboolean
clutter_text_get_line_wrap (ClutterText *text)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (text), FALSE);

  return text->priv->wrap;
}

void
clutter_text_set_line_wrap (ClutterText *text,
                            gboolean     line_wrap)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (text));

  priv = text->priv;

  if (priv->wrap != line_wrap)
    {
      priv->wrap = line_wrap;

      clutter_text_dirty_cache (text);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (text));
    }
}
