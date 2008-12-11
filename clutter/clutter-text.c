/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored By: Øyvind Kolås <pippin@o-hand.com>
 *              Emmanuele Bassi <ebassi@linux.intel.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* TODO: undo/redo hooks?
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "clutter-text.h"

#include "clutter-binding-pool.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-keysyms.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-units.h"

#include "cogl-pango.h"

#define DEFAULT_FONT_NAME	"Sans 10"

/* We need at least three cached layouts to run the allocation without
 * regenerating a new layout. First the layout will be generated at
 * full width to get the preferred width, then it will be generated at
 * the preferred width to get the preferred height and then it might
 * be regenerated at a different width to get the height for the
 * actual allocated width
 */
#define N_CACHED_LAYOUTS 3

#define CLUTTER_TEXT_GET_PRIVATE(obj)   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_TEXT, ClutterTextPrivate))

typedef struct _LayoutCache     LayoutCache;

/* Probably move into main */
static PangoContext *_context = NULL;

static const ClutterColor default_cursor_color = {   0,   0,   0, 255 };
static const ClutterColor default_text_color   = {   0,   0,   0, 255 };

G_DEFINE_TYPE (ClutterText, clutter_text, CLUTTER_TYPE_ACTOR);

struct _LayoutCache
{
  /* Cached layout. Pango internally caches the computed extents
   * when they are requested so there is no need to cache that as
   * well
   */
  PangoLayout *layout;

  /* The width that used to generate this layout */
  ClutterUnit  width;

  /* A number representing the age of this cache (so that when a
   * new layout is needed the last used cache is replaced)
   */
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

  PangoAttrList *attrs;
  PangoAttrList *effective_attrs;

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
  guint text_visible     : 1;

  /* current cursor position */
  gint position;

  /* current 'other end of selection' position */
  gint selection_bound;

  /* the x position in the pangolayout, used to
   * avoid drifting when repeatedly moving up|down
   */
  gint x_pos;

  /* the length of the text, in bytes */
  gint n_bytes;

  /* the length of the text, in characters */
  gint n_chars;

  /* Where to draw the cursor */
  ClutterGeometry cursor_pos;
  ClutterColor cursor_color;

  gint max_length;

  gunichar priv_char;
};

enum
{
  PROP_0,

  PROP_FONT_NAME,
  PROP_TEXT,
  PROP_COLOR,
  PROP_USE_MARKUP,
  PROP_ATTRIBUTES,
  PROP_ALIGNMENT,
  PROP_LINE_WRAP,
  PROP_LINE_WRAP_MODE,
  PROP_JUSTIFY,
  PROP_ELLIPSIZE,
  PROP_POSITION,
  PROP_SELECTION_BOUND,
  PROP_CURSOR_VISIBLE,
  PROP_CURSOR_COLOR,
  PROP_CURSOR_COLOR_SET,
  PROP_EDITABLE,
  PROP_SELECTABLE,
  PROP_ACTIVATABLE,
  PROP_TEXT_VISIBLE,
  PROP_INVISIBLE_CHAR,
  PROP_MAX_LENGTH
};

enum
{
  TEXT_CHANGED,
  CURSOR_EVENT,
  ACTIVATE,

  LAST_SIGNAL
};

static guint text_signals[LAST_SIGNAL] = { 0, };

#define offset_real(t,p)                        \
  ((p) == -1 ? g_utf8_strlen ((t), -1) : (p))

static gint
offset_to_bytes (const gchar *text,
                 gint         pos)
{
  gchar *c = NULL;
  gint i, j, len;

  if (pos < 0)
    return strlen (text);

#if 0
  if (pos < 1)
    return pos;
#endif

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

#define bytes_to_offset(t,p)                    \
  (g_utf8_pointer_to_offset ((t), (t) + (p)))


static inline void
clutter_text_clear_selection (ClutterText *self)
{
  ClutterTextPrivate *priv = self->priv;

  priv->selection_bound = priv->position;
}

static PangoLayout *
clutter_text_create_layout_no_cache (ClutterText *text,
                                     ClutterUnit  allocation_width)
{
  ClutterTextPrivate *priv = text->priv;
  PangoLayout *layout;

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
        {
          if (priv->text_visible)
            pango_layout_set_text (layout, priv->text, priv->n_bytes);
          else
            {
              GString *str = g_string_sized_new (priv->n_bytes);
              gunichar invisible_char;
              gchar buf[7];
              gint char_len, i;

              if (priv->priv_char != 0)
                invisible_char = priv->priv_char;
              else
                invisible_char = ' ';

              /* we need to convert the string built of invisible
               * characters into UTF-8 for it to be fed to the Pango
               * layout
               */
              memset (buf, 0, sizeof (buf));
              char_len = g_unichar_to_utf8 (invisible_char, buf);

              for (i = 0; i < priv->n_chars; i++)
                g_string_append_len (str, buf, char_len);

              pango_layout_set_text (layout, str->str, str->len);

              g_string_free (str, TRUE);
            }
        }
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
clutter_text_position_to_coords (ClutterText *self,
                                 gint         position,
                                 gint        *x,
                                 gint        *y,
                                 gint        *cursor_height)
{
  ClutterTextPrivate *priv = self->priv;
  PangoRectangle rect;
  gint priv_char_bytes;
  gint index_;

  if (!priv->text_visible && priv->priv_char)
    priv_char_bytes = g_unichar_to_utf8 (priv->priv_char, NULL);
  else
    priv_char_bytes = 1;

  if (position == -1)
    {
      if (priv->text_visible)
        index_ = strlen (priv->text);
      else
        index_ = priv->n_chars * priv_char_bytes;
    }
  else
    {
      if (priv->text_visible)
        index_ = offset_to_bytes (priv->text, position);
      else
        index_ = priv->position * priv_char_bytes;
    }

  pango_layout_get_cursor_pos (clutter_text_get_layout (self), index_,
                               &rect, NULL);

  if (x)
    *x = rect.x / PANGO_SCALE;

  if (y)
    *y = (rect.y + rect.height) / PANGO_SCALE;

  if (cursor_height)
    *cursor_height = rect.height / PANGO_SCALE;

  return TRUE; /* FIXME: should return false if coords were outside text */
}

static inline void
clutter_text_ensure_cursor_position (ClutterText *self)
{
  ClutterTextPrivate *priv = self->priv;
  gint x, y, cursor_height;

  clutter_text_position_to_coords (self, priv->position,
                                   &x, &y,
                                   &cursor_height);

  priv->cursor_pos.x = x;
  priv->cursor_pos.y = y - cursor_height;
  priv->cursor_pos.width = 2;
  priv->cursor_pos.height = cursor_height;

  g_signal_emit (self, text_signals[CURSOR_EVENT], 0, &priv->cursor_pos);
}

static gboolean
clutter_text_truncate_selection (ClutterText *self)
{
  ClutterTextPrivate *priv = self->priv;
  gint start_index;
  gint end_index;

  if (!priv->text)
    return TRUE;

  start_index = offset_real (priv->text, priv->position);
  end_index = offset_real (priv->text, priv->selection_bound);

  if (end_index == start_index)
    return FALSE;

  if (end_index < start_index)
    {
      gint temp = start_index;
      start_index = end_index;
      end_index = temp;
    }

  clutter_text_delete_text (self, start_index, end_index);

  priv->position = start_index;
  priv->selection_bound = start_index;

  return TRUE;
}

static void
clutter_text_set_property (GObject      *gobject,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ClutterText *self = CLUTTER_TEXT (gobject);

  switch (prop_id)
    {
    case PROP_TEXT:
      clutter_text_set_text (self, g_value_get_string (value));
      break;

    case PROP_COLOR:
      clutter_text_set_color (self, clutter_value_get_color (value));
      break;

    case PROP_FONT_NAME:
      clutter_text_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      clutter_text_set_use_markup (self, g_value_get_boolean (value));
      break;

    case PROP_ATTRIBUTES:
      clutter_text_set_attributes (self, g_value_get_boxed (value));
      break;

    case PROP_ALIGNMENT:
      clutter_text_set_alignment (self, g_value_get_enum (value));
      break;

    case PROP_LINE_WRAP:
      clutter_text_set_line_wrap (self, g_value_get_boolean (value));
      break;

    case PROP_LINE_WRAP_MODE:
      clutter_text_set_line_wrap_mode (self, g_value_get_enum (value));
      break;

    case PROP_JUSTIFY:
      clutter_text_set_justify (self, g_value_get_boolean (value));
      break;

    case PROP_ELLIPSIZE:
      clutter_text_set_ellipsize (self, g_value_get_enum (value));
      break;

    case PROP_POSITION:
      clutter_text_set_cursor_position (self, g_value_get_int (value));
      break;

    case PROP_SELECTION_BOUND:
      clutter_text_set_selection_bound (self, g_value_get_int (value));
      break;

    case PROP_CURSOR_VISIBLE:
      clutter_text_set_cursor_visible (self, g_value_get_boolean (value));
      break;

    case PROP_CURSOR_COLOR:
      clutter_text_set_cursor_color (self, g_value_get_boxed (value));
      break;

    case PROP_EDITABLE:
      clutter_text_set_editable (self, g_value_get_boolean (value));
      break;

    case PROP_ACTIVATABLE:
      clutter_text_set_activatable (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTABLE:
      clutter_text_set_selectable (self, g_value_get_boolean (value));
      break;

    case PROP_TEXT_VISIBLE:
      clutter_text_set_text_visible (self, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR:
      clutter_text_set_invisible_char (self, g_value_get_uint (value));
      break;

    case PROP_MAX_LENGTH:
      clutter_text_set_max_length (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_text_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ClutterTextPrivate *priv = CLUTTER_TEXT (gobject)->priv;

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

    case PROP_CURSOR_COLOR_SET:
      g_value_set_boolean (value, priv->cursor_color_set);
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

    case PROP_TEXT_VISIBLE:
      g_value_set_boolean (value, priv->text_visible);
      break;

    case PROP_INVISIBLE_CHAR:
      g_value_set_uint (value, priv->priv_char);
      break;

    case PROP_MAX_LENGTH:
      g_value_set_int (value, priv->max_length);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_text_dispose (GObject *gobject)
{
  ClutterText *self = CLUTTER_TEXT (gobject);

  /* get rid of the entire cache */
  clutter_text_dirty_cache (self);

  G_OBJECT_CLASS (clutter_text_parent_class)->dispose (gobject);
}

static void
clutter_text_finalize (GObject *gobject)
{
  ClutterText *self = CLUTTER_TEXT (gobject);
  ClutterTextPrivate *priv = self->priv;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  g_free (priv->text);
  g_free (priv->font_name);

  G_OBJECT_CLASS (clutter_text_parent_class)->finalize (gobject);
}

static void
cursor_paint (ClutterText *self)
{
  ClutterTextPrivate *priv = self->priv;
  ClutterActor *actor = CLUTTER_ACTOR (self);
  guint8 real_opacity;

  if (priv->editable && priv->cursor_visible)
    {
      if (priv->cursor_color_set)
        {
          real_opacity = clutter_actor_get_paint_opacity (actor)
                       * priv->cursor_color.alpha
                       / 255;

          cogl_set_source_color4ub (priv->cursor_color.red,
                                    priv->cursor_color.green,
                                    priv->cursor_color.blue,
                                    real_opacity);
        }
      else
        {
          real_opacity = clutter_actor_get_paint_opacity (actor)
                       * priv->text_color.alpha
                       / 255;

          cogl_set_source_color4ub (priv->text_color.red,
                                    priv->text_color.green,
                                    priv->text_color.blue,
                                    real_opacity);
        }

      clutter_text_ensure_cursor_position (self);

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
          PangoLayout *layout = clutter_text_get_layout (self);
          const gchar *utf8 = priv->text;
          gint lines;
          gint start_index;
          gint end_index;
          gint line_no;

          start_index = offset_to_bytes (utf8, priv->position);
          end_index = offset_to_bytes (utf8, priv->selection_bound);

          if (start_index > end_index)
            {
              gint temp = start_index;
              start_index = end_index;
              end_index = temp;
            }
          
          lines = pango_layout_get_line_count (layout);

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

              pango_layout_line_get_x_ranges (line, start_index, end_index,
                                              &ranges,
                                              &n_ranges);
              pango_layout_line_x_to_index (line, 0, &index, NULL);

              clutter_text_position_to_coords (self,
                                               bytes_to_offset (utf8, index),
                                               NULL, &y, &height);

              for (i = 0; i < n_ranges; i++)
                cogl_rectangle (ranges[i * 2 + 0] / PANGO_SCALE,
                                y - height,
                                ((ranges[i * 2 + 1] - ranges[i * 2 + 0])
                                 / PANGO_SCALE),
                                height);

              g_free (ranges);

            }
        }
    }
}

static gboolean
clutter_text_button_press (ClutterActor       *actor,
                           ClutterButtonEvent *event)
{
  ClutterText *self = CLUTTER_TEXT (actor);
  ClutterTextPrivate *priv = self->priv;
  ClutterUnit x, y;
  gint index_;

  x = CLUTTER_UNITS_FROM_INT (event->x);
  y = CLUTTER_UNITS_FROM_INT (event->y);

  clutter_actor_transform_stage_point (actor, x, y, &x, &y);

  index_ = clutter_text_coords_to_position (self,
                                            CLUTTER_UNITS_TO_INT (x),
                                            CLUTTER_UNITS_TO_INT (y));

  clutter_text_set_cursor_position (self, bytes_to_offset (priv->text, index_));
  clutter_text_set_selection_bound (self, bytes_to_offset (priv->text, index_));

  /* grab the pointer */
  priv->in_select_drag = TRUE;
  clutter_grab_pointer (actor);

  /* we'll steal keyfocus if we do not have it */
  clutter_actor_grab_key_focus (actor);

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
    return FALSE;

  text = clutter_text_get_text (ttext);

  x = CLUTTER_UNITS_FROM_INT (mev->x);
  y = CLUTTER_UNITS_FROM_INT (mev->y);

  clutter_actor_transform_stage_point (actor, x, y, &x, &y);

  index_ = clutter_text_coords_to_position (ttext, CLUTTER_UNITS_TO_INT (x),
                                                CLUTTER_UNITS_TO_INT (y));

  if (priv->selectable)
    clutter_text_set_cursor_position (ttext, bytes_to_offset (text, index_));
  else
    {
      clutter_text_set_cursor_position (ttext, bytes_to_offset (text, index_));
      clutter_text_set_selection_bound (ttext, bytes_to_offset (text, index_));
    }

  return TRUE;
}

static gboolean
clutter_text_button_release (ClutterActor       *actor,
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

static gboolean
clutter_text_key_press (ClutterActor    *actor,
                        ClutterKeyEvent *event)
{
  ClutterText *self = CLUTTER_TEXT (actor);
  ClutterTextPrivate *priv = self->priv;
  ClutterBindingPool *pool;
  gboolean res;
  gint keyval;

  if (!priv->editable)
    return FALSE;

  keyval = clutter_key_event_symbol (event);

  pool = clutter_binding_pool_find (G_OBJECT_TYPE_NAME (actor));
  g_assert (pool != NULL);

  res = clutter_binding_pool_activate (pool, keyval,
                                       event->modifier_state,
                                       G_OBJECT (actor));
  if (res)
    return TRUE;
  else
    {
      gunichar key_unichar = clutter_key_event_unicode (event);

      if (key_unichar == '\r')  /* return is reported as CR we want LF */
        key_unichar = '\n';

      if (g_unichar_validate (key_unichar))
        {
          clutter_text_truncate_selection (self);
          clutter_text_insert_unichar (self, key_unichar);

          return TRUE;
        }
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
  guint8 real_opacity;

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

  real_opacity = clutter_actor_get_paint_opacity (self)
               * priv->text_color.alpha
               / 255;

  cogl_color_set_from_4ub (&color,
                           priv->text_color.red,
                           priv->text_color.green,
                           priv->text_color.blue,
                           real_opacity);
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

  /* Ensure that there is a cached layout with the right width so
   * that we don't need to create the text during the paint run
   */
  clutter_text_create_layout (text, box->x2 - box->x1);

  parent_class = CLUTTER_ACTOR_CLASS (clutter_text_parent_class);
  parent_class->allocate (self, box, origin_changed);
}

static gboolean
clutter_text_real_move_left (ClutterText         *self,
                             const gchar         *action,
                             guint                keyval,
                             ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  gint pos = priv->position;
  gint len;

  len = g_utf8_strlen (priv->text, -1);

  if (pos != 0 && len !=0)
    {
      if (pos == -1)
        clutter_text_set_cursor_position (self, len - 1);
      else
        clutter_text_set_cursor_position (self, pos - 1);
    }

  if (!(priv->selectable && (modifiers & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (self);

  return TRUE;
}

static gboolean
clutter_text_real_move_right (ClutterText         *self,
                              const gchar         *action,
                              guint                keyval,
                              ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  gint pos = priv->position;
  gint len;

  len = g_utf8_strlen (priv->text, -1);

  if (pos != -1 && len !=0)
    {
      if (pos != len)
        clutter_text_set_cursor_position (self, pos + 1);
    }

  if (!(priv->selectable && (modifiers & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (self);

  return TRUE;
}

static gboolean
clutter_text_real_move_up (ClutterText         *self,
                           const gchar         *action,
                           guint                keyval,
                           ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  PangoLayoutLine *layout_line;
  PangoLayout *layout;
  gint line_no;
  gint index_;
  gint x;

  layout = clutter_text_get_layout (self);

  pango_layout_index_to_line_x (layout,
                                offset_to_bytes (priv->text, priv->position),
                                0,
                                &line_no, &x);

  if (priv->x_pos != -1)
    x = priv->x_pos;
  else
    priv->x_pos = x;

  line_no -= 1;
  if (line_no < 0)
    return FALSE;

  layout_line = pango_layout_get_line_readonly (layout, line_no);
  if (!layout_line)
    return FALSE;

  pango_layout_line_x_to_index (layout_line, x, &index_, NULL);

  {
    gint pos = bytes_to_offset (priv->text, index_);

    clutter_text_set_cursor_position (self, pos);
  }

  if (!(priv->selectable && (modifiers & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (self);

  return TRUE;
}

static gboolean
clutter_text_real_move_down (ClutterText         *self,
                             const gchar         *action,
                             guint                keyval,
                             ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  PangoLayoutLine *layout_line;
  PangoLayout *layout;
  gint line_no;
  gint index_;
  gint x;

  layout = clutter_text_get_layout (self);

  pango_layout_index_to_line_x (layout,
                                offset_to_bytes (priv->text, priv->position),
                                0,
                                &line_no, &x);

  if (priv->x_pos != -1)
    x = priv->x_pos;
  else
    priv->x_pos = x;

  layout_line = pango_layout_get_line_readonly (layout, line_no + 1);
  if (!layout_line)
    return FALSE;

  pango_layout_line_x_to_index (layout_line, x, &index_, NULL);

  {
    gint pos = bytes_to_offset (priv->text, index_);

    clutter_text_set_cursor_position (self, pos);
  }

  if (!(priv->selectable && (modifiers & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (self);

  return TRUE;
}

static gboolean
clutter_text_real_line_start (ClutterText         *self,
                              const gchar         *action,
                              guint                keyval,
                              ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  PangoLayoutLine *layout_line;
  PangoLayout *layout;
  gint line_no;
  gint index_;
  gint position;

  layout = clutter_text_get_layout (self);

  pango_layout_index_to_line_x (layout,
                                offset_to_bytes (priv->text, priv->position),
                                0,
                                &line_no, NULL);

  layout_line = pango_layout_get_line_readonly (layout, line_no);
  if (!layout_line)
    return FALSE;

  pango_layout_line_x_to_index (layout_line, 0, &index_, NULL);

  position = bytes_to_offset (priv->text, index_);
  clutter_text_set_cursor_position (self, position);

  if (!(priv->selectable && (modifiers & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (self);

  return TRUE;
}

static gboolean
clutter_text_real_line_end (ClutterText         *self,
                            const gchar         *action,
                            guint                keyval,
                            ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  PangoLayoutLine *layout_line;
  PangoLayout *layout;
  gint line_no;
  gint index_;
  gint trailing;
  gint position;

  layout = clutter_text_get_layout (self);
  index_ = offset_to_bytes (priv->text, priv->position);

  pango_layout_index_to_line_x (layout, index_,
                                0,
                                &line_no, NULL);

  layout_line = pango_layout_get_line_readonly (layout, line_no);
  if (!layout_line)
    return FALSE;

  pango_layout_line_x_to_index (layout_line, G_MAXINT, &index_, &trailing);
  index_ += trailing;

  position = bytes_to_offset (priv->text, index_);

  clutter_text_set_cursor_position (self, position);

  if (!(priv->selectable && (modifiers & CLUTTER_SHIFT_MASK)))
    clutter_text_clear_selection (self);

  return TRUE;
}

static gboolean
clutter_text_real_del_next (ClutterText         *self,
                            const gchar         *action,
                            guint                keyval,
                            ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  gint pos;
  gint len;

  if (clutter_text_truncate_selection (self))
    return TRUE;

  pos = priv->position;
  len = g_utf8_strlen (priv->text, -1);

  if (len && pos != -1 && pos < len)
    {
      clutter_text_delete_text (self, pos, pos + 1);

      return TRUE;
    }

  return FALSE;
}

static gboolean
clutter_text_real_del_prev (ClutterText         *self,
                            const gchar         *action,
                            guint                keyval,
                            ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;
  gint pos;
  gint len;

  if (clutter_text_truncate_selection (self))
    return TRUE;

  pos = priv->position;
  len = g_utf8_strlen (priv->text, -1);

  if (pos != 0 && len != 0)
    {
      if (pos == -1)
        {
          clutter_text_set_cursor_position (self, len - 1);
          clutter_text_set_selection_bound (self, len - 1);
        }
      else
        {
          clutter_text_set_cursor_position (self, pos - 1);
          clutter_text_set_selection_bound (self, pos - 1);
        }

      clutter_text_delete_text (self, pos - 1, pos);

      return TRUE;
    }

  return FALSE;
}

static gboolean
clutter_text_real_activate (ClutterText         *self,
                            const gchar         *action,
                            guint                keyval,
                            ClutterModifierType  modifiers)
{
  ClutterTextPrivate *priv = self->priv;

  if (priv->activatable)
    {
      g_signal_emit (self, text_signals[ACTIVATE], 0);

      return TRUE;
    }

  return FALSE;
}

static gboolean
clutter_text_real_page_up (ClutterText         *self,
                           const gchar         *action,
                           guint                keyval,
                           ClutterModifierType  modifiers)
{
  return FALSE;
}

static gboolean
clutter_text_real_page_down (ClutterText         *self,
                             const gchar         *action,
                             guint                keyval,
                             ClutterModifierType  modifiers)
{
  return FALSE;
}

static void
clutter_text_add_move_binding (ClutterBindingPool *pool,
                               const gchar        *action,
                               guint               key_val,
                               GCallback           callback)
{
  clutter_binding_pool_install_action (pool, action,
                                       key_val, 0,
                                       callback,
                                       NULL, NULL);
  clutter_binding_pool_install_action (pool, action,
                                       key_val, CLUTTER_SHIFT_MASK,
                                       callback,
                                       NULL, NULL);
}

static void
clutter_text_class_init (ClutterTextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterBindingPool *binding_pool;
  GParamSpec *pspec;

  _context = _clutter_context_create_pango_context (CLUTTER_CONTEXT ());

  g_type_class_add_private (klass, sizeof (ClutterTextPrivate));

  gobject_class->set_property = clutter_text_set_property;
  gobject_class->get_property = clutter_text_get_property;
  gobject_class->dispose = clutter_text_dispose;
  gobject_class->finalize = clutter_text_finalize;

  actor_class->paint = clutter_text_paint;
  actor_class->get_preferred_width = clutter_text_get_preferred_width;
  actor_class->get_preferred_height = clutter_text_get_preferred_height;
  actor_class->allocate = clutter_text_allocate;
  actor_class->key_press_event = clutter_text_key_press;
  actor_class->button_press_event = clutter_text_button_press;
  actor_class->button_release_event = clutter_text_button_release;
  actor_class->motion_event = clutter_text_motion;

  /**
   * ClutterText:font-name:
   *
   * The font to be used by the #ClutterText, as a string
   * that can be parsed by pango_font_description_from_string().
   *
   * Since: 1.0
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
  pspec = g_param_spec_boolean ("editable",
                                "Editable",
                                "Whether the text is editable",
                                TRUE,
                                G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_EDITABLE, pspec);

  /**
   * ClutterText:selectable:
   *
   * Whether it is possible to select text.
   */
  pspec = g_param_spec_boolean ("selectable",
                                "Selectable",
                                "Whether the text is selectable",
                                TRUE,
                                G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_SELECTABLE, pspec);

  /**
   * ClutterText:activatable:
   *
   * Toggles whether return invokes the activate signal or not.
   */
  pspec = g_param_spec_boolean ("activatable",
                                "Activatable",
                                "Whether pressing return causes the "
                                "activate signal to be emitted",
                                TRUE,
                                G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ACTIVATABLE, pspec);

  /**
   * ClutterText:cursor-visible:
   *
   * Whether the input cursor is visible or not, it will only be visible
   * if both cursor-visible is set and editable is set at the same time,
   * the value defaults to TRUE.
   */
  pspec = g_param_spec_boolean ("cursor-visible",
                                "Cursor Visible",
                                "Whether the input cursor is visible",
                                TRUE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CURSOR_VISIBLE, pspec);

  pspec = clutter_param_spec_color ("cursor-color",
                                    "Cursor Color",
                                    "Cursor Color",
                                    &default_cursor_color,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CURSOR_COLOR, pspec);

  pspec = g_param_spec_boolean ("cursor-color-set",
                                "Cursor Color Set",
                                "Whether the cursor color has been set",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_CURSOR_COLOR_SET, pspec);

  /**
   * ClutterText:position:
   *
   * The current input cursor position. -1 is taken to be the end of the text
   */
  pspec = g_param_spec_int ("position",
                            "Position",
                            "The cursor position",
                            -1, G_MAXINT,
                            -1,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_POSITION, pspec);

  /**
   * ClutterText:selection-bound:
   *
   * The current input cursor position. -1 is taken to be the end of the text
   */
  pspec = g_param_spec_int ("selection-bound",
                            "Selection-bound",
                            "The cursor position of the other end "
                            "of the selection",
                            -1, G_MAXINT,
                            -1,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_SELECTION_BOUND, pspec);

  pspec = g_param_spec_boxed ("attributes",
                              "Attributes",
                              "A list of style attributes to apply to "
                              "the contents of the actor",
                              PANGO_TYPE_ATTR_LIST,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ATTRIBUTES, pspec);

  /**
   * ClutterText:use-markup:
   *
   * Whether the text includes Pango markup. See pango_layout_set_markup()
   * in the Pango documentation.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("use-markup",
                                "Use markup",
                                "Whether or not the text "
                                "includes Pango markup",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_USE_MARKUP, pspec);

  /**
   * ClutterText:line-wrap:
   *
   * Whether to wrap the lines of #ClutterText:text if the contents
   * exceed the available allocation. The wrapping strategy is
   * controlled by the #ClutterText:line-wrap-mode property.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("line-wrap",
                                "Line wrap",
                                "If set, wrap the lines if the text "
                                "becomes too wide",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LINE_WRAP, pspec);

  /**
   * ClutterText:line-wrap-mode:
   *
   * If #ClutterText:line-wrap is set to %TRUE, this property will
   * control how the text is wrapped.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_enum ("line-wrap-mode",
                             "Line wrap mode",
                             "Control how line-wrapping is done",
                             PANGO_TYPE_WRAP_MODE,
                             PANGO_WRAP_WORD,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LINE_WRAP_MODE, pspec);

  pspec = g_param_spec_enum ("ellipsize",
                             "Ellipsize",
                             "The preferred place to ellipsize the string",
                             PANGO_TYPE_ELLIPSIZE_MODE,
                             PANGO_ELLIPSIZE_NONE,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ELLIPSIZE, pspec);

  /**
   * ClutterText:alignment:
   *
   * The preferred alignment for the text. This property controls
   * the alignment of multi-line paragraphs.
   *
   * Since: 1.0
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
   * ClutterText:justify:
   *
   * Whether the contents of the #ClutterText should be justified
   * on both margins.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("justify",
                                "Justify",
                                "Whether the text should be justified",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_JUSTIFY, pspec);

  pspec = g_param_spec_boolean ("text-visible",
                                "Text Visible",
                                "Whether the text should be visible "
                                "or subsituted with an invisible "
                                "Unicode character",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TEXT_VISIBLE, pspec);

  pspec = g_param_spec_unichar ("invisible-char",
                                "Invisible Character",
                                "The Unicode character used when the "
                                "text is set as not visible",
                                '*',
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_INVISIBLE_CHAR, pspec);

  pspec = g_param_spec_int ("max-length",
                            "Max Length",
                            "Maximum length of the text inside the actor",
                            -1, G_MAXINT, 0,
                            CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_MAX_LENGTH, pspec);

  /**
   * ClutterText::text-changed:
   * @self: the #ClutterText that emitted the signal
   *
   * The ::text-changed signal is emitted after @actor's text changes
   *
   * Since: 1.0
   */
  text_signals[TEXT_CHANGED] =
    g_signal_new ("text-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterTextClass, text_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterText::cursor-event:
   * @self: the #ClutterText that emitted the signal
   * @geometry: the coordinates of the cursor
   *
   * FIXME
   *
   * Since: 1.0
   */
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
   * @self: the #ClutterText that emitted the signal
   *
   * The ::activate signal is emitted each time the actor is 'activated'
   * by the user, normally by pressing the 'Enter' key.
   *
   * Since: 1.0
   */
  text_signals[ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterTextClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  binding_pool = clutter_binding_pool_get_for_class (klass);

  clutter_text_add_move_binding (binding_pool, "move-left",
                                 CLUTTER_Left,
                                 G_CALLBACK (clutter_text_real_move_left));
  clutter_text_add_move_binding (binding_pool, "move-left",
                                 CLUTTER_KP_Left,
                                 G_CALLBACK (clutter_text_real_move_left));
  clutter_text_add_move_binding (binding_pool, "move-right",
                                 CLUTTER_Right,
                                 G_CALLBACK (clutter_text_real_move_right));
  clutter_text_add_move_binding (binding_pool, "move-right",
                                 CLUTTER_KP_Right,
                                 G_CALLBACK (clutter_text_real_move_right));
  clutter_text_add_move_binding (binding_pool, "move-up",
                                 CLUTTER_Up,
                                 G_CALLBACK (clutter_text_real_move_up));
  clutter_text_add_move_binding (binding_pool, "move-up",
                                 CLUTTER_KP_Up,
                                 G_CALLBACK (clutter_text_real_move_up));
  clutter_text_add_move_binding (binding_pool, "move-down",
                                 CLUTTER_Down,
                                 G_CALLBACK (clutter_text_real_move_down));
  clutter_text_add_move_binding (binding_pool, "move-down",
                                 CLUTTER_KP_Down,
                                 G_CALLBACK (clutter_text_real_move_down));

  clutter_binding_pool_install_action (binding_pool, "line-start",
                                       CLUTTER_Home, 0,
                                       G_CALLBACK (clutter_text_real_line_start),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "line-start",
                                       CLUTTER_KP_Home, 0,
                                       G_CALLBACK (clutter_text_real_line_start),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "line-start",
                                       CLUTTER_Begin, 0,
                                       G_CALLBACK (clutter_text_real_line_start),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "line-end",
                                       CLUTTER_End, 0,
                                       G_CALLBACK (clutter_text_real_line_end),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "line-end",
                                       CLUTTER_KP_End, 0,
                                       G_CALLBACK (clutter_text_real_line_end),
                                       NULL, NULL);

  clutter_binding_pool_install_action (binding_pool, "page-up",
                                       CLUTTER_Page_Up, 0,
                                       G_CALLBACK (clutter_text_real_page_up),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "page-up",
                                       CLUTTER_KP_Page_Up, 0,
                                       G_CALLBACK (clutter_text_real_page_up),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "page-down",
                                       CLUTTER_Page_Down, 0,
                                       G_CALLBACK (clutter_text_real_page_down),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "page-up",
                                       CLUTTER_KP_Page_Down, 0,
                                       G_CALLBACK (clutter_text_real_page_down),
                                       NULL, NULL);

  clutter_binding_pool_install_action (binding_pool, "delete-next",
                                       CLUTTER_Delete, 0,
                                       G_CALLBACK (clutter_text_real_del_next),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "delete-next",
                                       CLUTTER_KP_Delete, 0,
                                       G_CALLBACK (clutter_text_real_del_next),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "delete-prev",
                                       CLUTTER_BackSpace, 0,
                                       G_CALLBACK (clutter_text_real_del_prev),
                                       NULL, NULL);

  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_Return, 0,
                                       G_CALLBACK (clutter_text_real_activate),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_KP_Enter, 0,
                                       G_CALLBACK (clutter_text_real_activate),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_ISO_Enter, 0,
                                       G_CALLBACK (clutter_text_real_activate),
                                       NULL, NULL);
}

static void
clutter_text_init (ClutterText *self)
{
  ClutterTextPrivate *priv;
  int i;

  self->priv = priv = CLUTTER_TEXT_GET_PRIVATE (self);

  priv->alignment     = PANGO_ALIGN_LEFT;
  priv->wrap          = FALSE;
  priv->wrap_mode     = PANGO_WRAP_WORD;
  priv->ellipsize     = PANGO_ELLIPSIZE_NONE;
  priv->use_underline = FALSE;
  priv->use_markup    = FALSE;
  priv->justify       = FALSE;

  for (i = 0; i < N_CACHED_LAYOUTS; i++)
    priv->cached_layouts[i].layout = NULL;

  priv->text = NULL;

  priv->text_color = default_text_color;
  priv->cursor_color = default_cursor_color;

  priv->font_name = g_strdup (DEFAULT_FONT_NAME);
  priv->font_desc = pango_font_description_from_string (priv->font_name);

  priv->position = -1;
  priv->selection_bound = -1;

  priv->x_pos = -1;
  priv->cursor_visible = TRUE;
  priv->editable = FALSE;
  priv->selectable = TRUE;

  priv->cursor_color_set = FALSE;

  priv->text_visible = TRUE;
  priv->priv_char = '*';

  priv->max_length = 0;
}

ClutterActor *
clutter_text_new (void)
{
  return g_object_new (CLUTTER_TYPE_TEXT, NULL);
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
clutter_text_set_editable (ClutterText *self,
                           gboolean     editable)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->editable != editable)
    {
      priv->editable = editable;

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "editable");
    }
}

gboolean
clutter_text_get_editable (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), FALSE);

  return self->priv->editable;
}


void
clutter_text_set_selectable (ClutterText *self,
                             gboolean     selectable)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->selectable != selectable)
    {
      priv->selectable = selectable;

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "selectable");
    }
}

gboolean
clutter_text_get_selectable (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), TRUE);

  return self->priv->selectable;
}


void
clutter_text_set_activatable (ClutterText *self,
                              gboolean     activatable)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->activatable != activatable)
    {
      priv->activatable = activatable;

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "activatable");
    }
}

gboolean
clutter_text_get_activatable (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), TRUE);

  return self->priv->activatable;
}

void
clutter_text_set_cursor_visible (ClutterText *self,
                                 gboolean     cursor_visible)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->cursor_visible != cursor_visible)
    {
      priv->cursor_visible = cursor_visible;

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "cursor-visible");
    }
}

gboolean
clutter_text_get_cursor_visible (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), TRUE);

  return self->priv->cursor_visible;
}

void
clutter_text_set_cursor_color (ClutterText        *self,
                               const ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (color)
    {
      priv->cursor_color = *color;
      priv->cursor_color_set = TRUE;
    }
  else
    priv->cursor_color_set = FALSE;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "cursor-color");
  g_object_notify (G_OBJECT (self), "cursor-color-set");
}


void
clutter_text_get_cursor_color (ClutterText  *self,
                               ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (color != NULL);

  priv = self->priv;

  *color = priv->cursor_color;
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
clutter_text_set_selection_bound (ClutterText *self,
                                  gint         selection_bound)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->selection_bound != selection_bound)
    {
      priv->selection_bound = selection_bound;

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "selection-bound");
    }
}

gint
clutter_text_get_selection_bound (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), -1);

  return self->priv->selection_bound;
}

/**
 * clutter_text_get_font_name:
 * @self: a #ClutterText
 *
 * Retrieves the font name as set by clutter_text_set_font_name().
 *
 * Return value: a string containing the font name. The returned
 *   string is owned by the #ClutterText actor and should not be
 *   modified or freed
 *
 * Since: 1.0
 */
G_CONST_RETURN gchar *
clutter_text_get_font_name (ClutterText *text)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (text), NULL);

  return text->priv->font_name;
}

/**
 * clutter_text_set_font_name:
 * @self: a #ClutterText
 * @font_name: a font name
 *
 * Sets the font used by a #ClutterText. The @font_name string
 * must be something that can be parsed by the
 * pango_font_description_from_string() function, like:
 *
 * |[
 *   clutter_text_set_font_name (text, "Sans 10pt");
 *   clutter_text_set_font_name (text, "Serif 16px");
 *   clutter_text_set_font_name (text, "Helvetica 10");
 * ]|
 *
 * Since: 1.0
 */
void
clutter_text_set_font_name (ClutterText *self,
                            const gchar *font_name)
{
  ClutterTextPrivate *priv;
  PangoFontDescription *desc;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  if (!font_name || font_name[0] == '\0')
    font_name = DEFAULT_FONT_NAME;

  priv = self->priv;

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

  clutter_text_dirty_cache (self);

  if (priv->text && priv->text[0] != '\0')
    clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "font-name");
}

/**
 * clutter_text_get_text:
 * @self: a #ClutterText
 *
 * Retrieves a pointer to the current contents of a #ClutterText
 * actor.
 *
 * If you need a copy of the contents for manipulating, either
 * use g_strdup() on the returned string, or use:
 *
 * |[
 *    copy = clutter_text_get_chars (text, 0, -1);
 * ]|
 *
 * Which will return a newly allocated string.
 *
 * Return value: the contents of the actor. The returned string
 *   is owned by the #ClutterText actor and should never be
 *   modified or freed
 *
 * Since: 1.0
 */
G_CONST_RETURN gchar *
clutter_text_get_text (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), NULL);

  return self->priv->text;
}

/**
 * clutter_text_set_text:
 * @self: a #ClutterText
 * @text: the text to set
 *
 * Sets the contents of a #ClutterText actor.
 *
 * Since: 1.0
 */
void
clutter_text_set_text (ClutterText *self,
                       const gchar *text)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (text != NULL);

  priv = self->priv;

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
          gchar *n = g_malloc0 (priv->max_length + 1);

          g_free (priv->text);

          g_utf8_strncpy (n, text, priv->max_length);

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
      priv->n_chars = g_utf8_strlen (text, -1);
    }

  clutter_text_dirty_cache (self);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

  g_signal_emit (self, text_signals[TEXT_CHANGED], 0);

  g_object_notify (G_OBJECT (self), "text");
}

/**
 * clutter_text_get_layout:
 * @self: a #ClutterText
 *
 * Retrieves the current #PangoLayout used by a #ClutterText actor.
 *
 * Return value: a #PangoLayout. The returned object is owned by
 *   the #ClutterText actor and should not be modified or freed
 *
 * Since: 1.0
 */
PangoLayout *
clutter_text_get_layout (ClutterText *self)
{
  ClutterUnit width;

  g_return_val_if_fail (CLUTTER_IS_TEXT (self), NULL);

  width = clutter_actor_get_widthu (CLUTTER_ACTOR (self));

  return clutter_text_create_layout (self, width);
}

/**
 * clutter_text_set_color:
 * @self: a #ClutterText
 * @color: a #ClutterColor
 *
 * Sets the color of the contents of a #ClutterText actor.
 *
 * The overall opacity of the #ClutterText actor will be the
 * result of the alpha value of @color and the composited
 * opacity of the actor itself on the scenegraph, as returned
 * by clutter_actor_get_paint_opacity().
 *
 * Since: 1.0
 */
void
clutter_text_set_color (ClutterText        *self,
                        const ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (color != NULL);

  priv = self->priv;

  priv->text_color = *color;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "color");
}

/**
 * clutter_text_get_color:
 * @self: a #ClutterText
 * @color: return location for a #ClutterColor
 *
 * Retrieves the text color as set by clutter_text_get_color().
 *
 * Since: 1.0
 */
void
clutter_text_get_color (ClutterText  *self,
                        ClutterColor *color)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (color != NULL);

  priv = self->priv;

  *color = priv->text_color;
}

/**
 * clutter_text_set_ellipsize:
 * @self: a #ClutterText
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") to the
 * text if there is not enough space to render the entire contents
 * of a #ClutterText actor
 *
 * Since: 1.0
 */
void
clutter_text_set_ellipsize (ClutterText        *self,
			    PangoEllipsizeMode  mode)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE &&
		    mode <= PANGO_ELLIPSIZE_END);

  priv = self->priv;

  if ((PangoEllipsizeMode) priv->ellipsize != mode)
    {
      priv->ellipsize = mode;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "ellipsize");
    }
}

/**
 * clutter_text_get_ellipsize:
 * @self: a #ClutterText
 *
 * Returns the ellipsizing position of a #ClutterText actor, as
 * set by clutter_text_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 *
 * Since: 1.0
 */
PangoEllipsizeMode
clutter_text_get_ellipsize (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), PANGO_ELLIPSIZE_NONE);

  return self->priv->ellipsize;
}

/**
 * clutter_text_get_line_wrap:
 * @self: a #ClutterText
 *
 * Retrieves the value set using clutter_text_set_line_wrap().
 *
 * Return value: %TRUE if the #ClutterText actor should wrap
 *   its contents
 *
 * Since: 1.0
 */
gboolean
clutter_text_get_line_wrap (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), FALSE);

  return self->priv->wrap;
}

/**
 * clutter_text_set_line_wrap:
 * @self: a #ClutterText
 * @line_wrap: whether the contents should wrap
 *
 * Sets whether the contents of a #ClutterText actor should wrap,
 * if they don't fit the size assigned to the actor.
 *
 * Since: 1.0
 */
void
clutter_text_set_line_wrap (ClutterText *self,
                            gboolean     line_wrap)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->wrap != line_wrap)
    {
      priv->wrap = line_wrap;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "line-wrap");
    }
}

/**
 * clutter_text_set_line_wrap_mode:
 * @self: a #ClutterText
 * @wrap_mode: the line wrapping mode
 *
 * If line wrapping is enabled (see clutter_text_set_line_wrap()) this
 * function controls how the line wrapping is performed. The default is
 * %PANGO_WRAP_WORD which means wrap on word boundaries.
 *
 * Since: 1.0
 */
void
clutter_text_set_line_wrap_mode (ClutterText   *self,
				 PangoWrapMode  wrap_mode)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->wrap_mode != wrap_mode)
    {
      priv->wrap_mode = wrap_mode;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "line-wrap-mode");
    }
}

/**
 * clutter_text_get_line_wrap_mode:
 * @self: a #ClutterText
 *
 * Retrieves the line wrap mode used by the #ClutterText actor.
 *
 * See clutter_text_set_line_wrap_mode ().
 *
 * Return value: the wrap mode used by the #ClutterText
 *
 * Since: 1.0
 */
PangoWrapMode
clutter_text_get_line_wrap_mode (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), PANGO_WRAP_WORD);

  return self->priv->wrap_mode;
}

/**
 * clutter_text_set_attributes:
 * @self: a #ClutterText
 * @attrs: a #PangoAttrList or %NULL to unset the attributes
 *
 * Sets the attributes list that are going to be applied to the
 * #ClutterText contents. The attributes set with this function
 * will be ignored if the #ClutterText:use_markup property is
 * set to %TRUE.
 *
 * The #ClutterText actor will take a reference on the #PangoAttrList
 * passed to this function.
 *
 * Since: 1.0
 */
void
clutter_text_set_attributes (ClutterText   *self,
			     PangoAttrList *attrs)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

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

  clutter_text_dirty_cache (self);

  g_object_notify (G_OBJECT (self), "attributes");

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

/**
 * clutter_text_get_attributes:
 * @self: a #ClutterText
 *
 * Gets the attribute list that was set on the #ClutterText actor
 * clutter_text_set_attributes(), if any.
 *
 * Return value: the attribute list, or %NULL if none was set. The
 *  returned value is owned by the #ClutterText and should not be
 *  unreferenced.
 *
 * Since: 1.0
 */
PangoAttrList *
clutter_text_get_attributes (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), NULL);

  return self->priv->attrs;
}

/**
 * clutter_text_set_alignment:
 * @self: a #ClutterText
 * @alignment: A #PangoAlignment
 *
 * Sets text alignment of the #ClutterText actor.
 *
 * The alignment will only be used when the contents of the
 * #ClutterText actor are enough to wrap, and the #ClutterText:line-wrap
 * property is set to %TRUE.
 *
 * Since: 1.0
 */
void
clutter_text_set_alignment (ClutterText    *self,
                            PangoAlignment  alignment)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->alignment != alignment)
    {
      priv->alignment = alignment;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "alignment");
    }
}

/**
 * clutter_text_get_alignment:
 * @self: a #ClutterText
 *
 * Retrieves the alignment of @self.
 *
 * Return value: a #PangoAlignment
 *
 * Since 1.0
 */
PangoAlignment
clutter_text_get_alignment (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), PANGO_ALIGN_LEFT);

  return self->priv->alignment;
}

/**
 * clutter_text_set_use_markup:
 * @self: a #ClutterText
 * @setting: %TRUE if the text should be parsed for markup.
 *
 * Sets whether the contents of the #ClutterText actor contains markup
 * in <link linkend="PangoMarkupFormat">Pango's text markup language</link>.
 *
 * Since: 1.0
 */
void
clutter_text_set_use_markup (ClutterText *self,
			     gboolean     setting)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->use_markup != setting)
    {
      priv->use_markup = setting;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "use-markup");
    }
}

/**
 * clutter_text_get_use_markup:
 * @self: a #ClutterText
 *
 * Retrieves whether the contents of the #ClutterText actor should be
 * parsed for the Pango text markup.
 *
 * Return value: %TRUE if the contents will be parsed for markup
 *
 * Since: 1.0
 */
gboolean
clutter_text_get_use_markup (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), FALSE);

  return self->priv->use_markup;
}

/**
 * clutter_text_set_justify:
 * @self: a #ClutterText
 * @justify: whether the text should be justified
 *
 * Sets whether the text of the #ClutterText actor should be justified
 * on both margins. This setting is ignored if Clutter is compiled
 * against Pango &lt; 1.18.
 *
 * Since: 1.0
 */
void
clutter_text_set_justify (ClutterText *self,
                          gboolean     justify)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->justify != justify)
    {
      priv->justify = justify;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "justify");
    }
}

/**
 * clutter_text_get_justify:
 * @self: a #ClutterText
 *
 * Retrieves whether the #ClutterText actor should justify its contents
 * on both margins.
 *
 * Return value: %TRUE if the text should be justified
 *
 * Since: 0.6
 */
gboolean
clutter_text_get_justify (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), FALSE);

  return self->priv->justify;
}

/**
 * clutter_text_get_cursor_position:
 * @self: a #ClutterText
 *
 * Retrieves the cursor position.
 *
 * Return value: the cursor position, in characters
 *
 * Since: 1.0
 */
gint
clutter_text_get_cursor_position (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), -1);

  return self->priv->position;
}

/**
 * clutter_text_set_cursor_position:
 * @self: a #ClutterText
 * @position: the new cursor position, in characters
 *
 * Sets the cursor of a #ClutterText actor at @position.
 *
 * The position is expressed in characters, not in bytes.
 *
 * Since: 1.0
 */
void
clutter_text_set_cursor_position (ClutterText *self,
                                  gint         position)
{
  ClutterTextPrivate *priv;
  gint len;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  len = g_utf8_strlen (priv->text, -1);

  if (position < 0 || position >= len)
    priv->position = -1;
  else
    priv->position = position;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
}

/**
 * clutter_text_set_text_visible:
 * @self: a #ClutterText
 * @visible: %TRUE if the contents of the actor are displayed as plain text.
 *
 * Sets whether the contents of the text actor are visible or not. When
 * visibility is set to %FALSE, characters are displayed as the invisible
 * char, and will also appear that way when the text in the text actor is
 * copied elsewhere.
 *
 * The default invisible char is the asterisk '*', but it can be changed with
 * clutter_text_set_invisible_char().
 *
 * Since: 1.0
 */
void
clutter_text_set_text_visible (ClutterText *self,
                               gboolean     visible)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->text_visible != visible)
    {
      priv->text_visible = visible;

      clutter_text_dirty_cache (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

      g_object_notify (G_OBJECT (self), "text-visible");
    }
}

/**
 * clutter_text_get_text_visible:
 * @self: a #ClutterText
 *
 * Retrieves the actor's text visibility.
 *
 * Return value: %TRUE if the contents of the actor are displayed as plaintext
 *
 * Since: 1.0
 */
gboolean
clutter_text_get_text_visible (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), TRUE);

  return self->priv->text_visible;
}

/**
 * clutter_text_set_invisible_char:
 * @self: a #ClutterText
 * @wc: a Unicode character
 *
 * Sets the character to use in place of the actual text when
 * clutter_text_set_text_visible() has been called to set text visibility
 * to %FALSE. i.e. this is the character used in "password mode" to show the
 * user how many characters have been typed. The default invisible char is an
 * asterisk ('*'). If you set the invisible char to 0, then the user will get
 * no feedback at all: there will be no text on the screen as they type.
 *
 * Since: 1.0
 */
void
clutter_text_set_invisible_char (ClutterText *self,
                                 gunichar     wc)
{
  ClutterTextPrivate *priv;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  priv->priv_char = wc;

  if (priv->text_visible)
    {
      clutter_text_dirty_cache (self);
      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
    }

  g_object_notify (G_OBJECT (self), "invisible-char");
}

/**
 * clutter_text_get_invisible_char:
 * @self: a #ClutterText
 *
 * Returns the character to use in place of the actual text when
 * the #ClutterText:text-visibility property is set to %FALSE.
 *
 * Return value: a Unicode character
 *
 * Since: 1.0
 */
gunichar
clutter_text_get_invisible_char (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), '*');

  return self->priv->priv_char;
}

/**
 * clutter_text_set_max_length:
 * @self: a #ClutterText
 * @max: the maximum number of characters allowed in the text actor; 0
 *   to disable or -1 to set the length of the current string
 *
 * Sets the maximum allowed length of the contents of the actor. If the
 * current contents are longer than the given length, then they will be
 * truncated to fit.
 *
 * Since: 1.0
 */
void
clutter_text_set_max_length (ClutterText *self,
                             gint         max)
{
  ClutterTextPrivate *priv;
  gchar *new = NULL;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (priv->max_length != max)
    {
      if (max < 0)
        max = g_utf8_strlen (priv->text, -1);

      priv->max_length = max;

      new = g_strdup (priv->text);
      clutter_text_set_text (self, new);
      g_free (new);

      g_object_notify (G_OBJECT (self), "max-length");
    }
}

/**
 * clutter_text_get_max_length:
 * @self: a #ClutterText
 *
 * Gets the maximum length of text that can be set into a text actor.
 *
 * See clutter_text_set_max_length().
 *
 * Return value: the maximum number of characters.
 *
 * Since: 1.0
 */
gint
clutter_text_get_max_length (ClutterText *self)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT (self), 0);

  return self->priv->max_length;
}

/**
 * clutter_text_insert_unichar:
 * @self: a #ClutterText
 * @wc: a Unicode character
 *
 * Inserts @wc at the current cursor position of a
 * #ClutterText actor.
 *
 * Since: 1.0
 */
void
clutter_text_insert_unichar (ClutterText *self,
                             gunichar     wc)
{
  ClutterTextPrivate *priv;
  GString *new = NULL;
  glong pos;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (g_unichar_validate (wc));

  if (wc == 0)
    return;

  priv = self->priv;

  new = g_string_new (priv->text);

  if (priv->text)
    pos = offset_to_bytes (priv->text, priv->position);
  else
    pos = 0;

  new = g_string_insert_unichar (new, pos, wc);

  clutter_text_set_text (self, new->str);

  if (priv->position >= 0)
    {
      clutter_text_set_cursor_position (self, priv->position + 1);
      clutter_text_set_selection_bound (self, priv->position);
    }

  g_string_free (new, TRUE);
}

/**
 * clutter_text_insert_text:
 * @self: a #ClutterText
 * @text: the text to be inserted
 * @position: the position of the insertion, or -1
 *
 * Inserts @text into a #ClutterActor at the given position.
 *
 * If @position is a negative number, the text will be appended
 * at the end of the current contents of the #ClutterText.
 *
 * The position is expressed in characters, not in bytes.
 *
 * Since: 1.0
 */
void
clutter_text_insert_text (ClutterText *self,
                          const gchar *text,
                          gssize       position)
{
  ClutterTextPrivate *priv;
  GString *new = NULL;

  g_return_if_fail (CLUTTER_IS_TEXT (self));
  g_return_if_fail (text != NULL);

  priv = self->priv;

  new = g_string_new (priv->text);
  new = g_string_insert (new, position, text);

  clutter_text_set_text (self, new->str);

  g_string_free (new, TRUE);
}

/**
 * clutter_text_delete_text:
 * @self: a #ClutterText
 * @start_pos: starting position
 * @end_pos: ending position
 *
 * Deletes the text inside a #ClutterText actor between @start_pos
 * and @end_pos.
 *
 * The starting and ending positions are expressed in characters,
 * not in bytes.
 *
 * Since: 1.0
 */
void
clutter_text_delete_text (ClutterText *self,
                          gssize       start_pos,
                          gssize       end_pos)
{
  ClutterTextPrivate *priv;
  GString *new = NULL;
  gint start_bytes;
  gint end_bytes;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (!priv->text)
    return;

  if (end_pos == -1)
    {
      start_bytes = offset_to_bytes (priv->text,
                                     g_utf8_strlen (priv->text, -1) - 1);
      end_bytes = offset_to_bytes (priv->text,
                                   g_utf8_strlen (priv->text, -1));
    }
  else
    {
      start_bytes = offset_to_bytes (priv->text, start_pos);
      end_bytes = offset_to_bytes (priv->text, end_pos);
    }

  new = g_string_new (priv->text);
  new = g_string_erase (new, start_bytes, end_bytes - start_bytes);

  clutter_text_set_text (self, new->str);

  g_string_free (new, TRUE);
}

/**
 * clutter_text_delete_chars:
 * @self: a #ClutterText
 * @n_chars: the number of characters to delete
 *
 * Deletes @n_chars inside a #ClutterText actor, starting from the
 * current cursor position.
 *
 * Since: 1.0
 */
void
clutter_text_delete_chars (ClutterText *self,
                           guint        n_chars)
{
  ClutterTextPrivate *priv;
  GString *new = NULL;
  gint len;
  gint pos;
  gint num_pos;

  g_return_if_fail (CLUTTER_IS_TEXT (self));

  priv = self->priv;

  if (!priv->text)
    return;

  len = g_utf8_strlen (priv->text, -1);
  new = g_string_new (priv->text);

  if (priv->position == -1)
    {
      num_pos = offset_to_bytes (priv->text, priv->n_chars - n_chars);
      new = g_string_erase (new, num_pos, -1);
    }
  else
    {
      pos = offset_to_bytes (priv->text, priv->position - n_chars);
      num_pos = offset_to_bytes (priv->text, priv->position);
      new = g_string_erase (new, pos, num_pos - pos);
    }

  clutter_text_set_text (self, new->str);

  if (priv->position > 0)
    clutter_text_set_cursor_position (self, priv->position - n_chars);

  g_string_free (new, TRUE);

  g_object_notify (G_OBJECT (self), "text");
}

/**
 * clutter_text_get_chars:
 * @self: a #ClutterText
 * @start_pos: start of text, in characters
 * @end_pos: end of text, in characters
 *
 * Retrieves the contents of the #ClutterText actor between
 * @start_pos and @end_pos.
 *
 * The positions are specified in characters, not in bytes.
 *
 * Return value: a newly allocated string with the contents of
 *   the text actor between the specified positions. Use g_free()
 *   to free the resources when done
 *
 * Since: 1.0
 */
gchar *
clutter_text_get_chars (ClutterText *self,
                        gssize       start_pos,
                        gssize       end_pos)
{
  ClutterTextPrivate *priv;
  gint start_index, end_index;

  g_return_val_if_fail (CLUTTER_IS_TEXT (self), NULL);

  priv = self->priv;

  if (end_pos < 0)
    end_pos = priv->n_chars;

  start_pos = MIN (priv->n_chars, start_pos);
  end_pos = MIN (priv->n_chars, end_pos);

  start_index = g_utf8_offset_to_pointer (priv->text, start_pos)
              - priv->text;
  end_index   = g_utf8_offset_to_pointer (priv->text, end_pos)
              - priv->text;

  return g_strndup (priv->text + start_index, end_index - start_index);
}
