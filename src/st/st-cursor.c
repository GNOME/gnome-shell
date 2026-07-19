/*
 * st-cursor.c: Pointer cursor image
 *
 * Copyright 2026 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "st-cursor.h"

#include <librsvg/rsvg.h>
#include <json-glib/json-glib.h>

#define RESOURCE_CURSOR_THEME_URI_BASE "resource:///org/gnome/shell/cursor-theme/"

#define FALLBACK_THEME_NAME "Adwaita"

typedef struct _StCursorFrame StCursorFrame;

struct _StCursorFrame
{
  RsvgHandle *image;
  CoglTexture *texture;
  int64_t delay;
  int64_t nominal_size;
  double hotspot_x, hotspot_y;
  double width, height;
};

struct _StCursor
{
  MetaCursor parent_instance;

  GArray *frames;
  unsigned int current_frame;
  double scale;
  gboolean invalidated;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (StCursor, st_cursor, META_TYPE_CURSOR,
                               g_io_extension_point_implement (META_CURSOR_EXTENSION_POINT_NAME,
                                                               g_define_type_id, "svg-cursor", 99))

static void
clear_frame (gpointer user_data)
{
  StCursorFrame *frame = user_data;

  g_clear_object (&frame->image);
  g_clear_object (&frame->texture);
}

static gboolean
check_metadata_exists (StCursor    *cursor,
                       const char  *theme_name,
                       GFile       *root,
                       GFile      **metadata_out)
{
  ClutterCursorType cursor_type =
    clutter_cursor_get_cursor_type (CLUTTER_CURSOR (cursor));
  g_autoptr (GFile) child = NULL;
  g_autofree char *subpath = NULL;
  const char *cursor_name;

  cursor_name = clutter_cursor_type_to_name (cursor_type);
  if (!cursor_name)
    return FALSE;

  subpath = g_strconcat ("icons/",
                         theme_name,
                         "/cursors_scalable/",
                         cursor_name,
                         "/metadata.json", NULL);
  child = g_file_get_child (root, subpath);

  if (!g_file_query_exists (child, NULL))
    return FALSE;

  if (metadata_out)
    *metadata_out = g_steal_pointer (&child);

  return TRUE;
}

static GFile *
find_cursor_metadata (StCursor *cursor)
{
  const char * const * system_data_dirs;
  const char * user_data_dir, * theme_name;
  g_autoptr (GFile) resource_root = NULL;
  GFile *file;
  int i;

  user_data_dir = g_get_user_data_dir ();
  system_data_dirs = g_get_system_data_dirs ();
  theme_name = meta_cursor_get_theme_name (META_CURSOR (cursor));

  if (user_data_dir)
    {
      g_autoptr (GFile) root = NULL;

      root = g_file_new_for_path (user_data_dir);
      if (check_metadata_exists (cursor, theme_name, root, &file))
        return file;
    }

  for (i = 0; system_data_dirs[i]; i++)
    {
      g_autoptr (GFile) root = NULL;

      root = g_file_new_for_path (system_data_dirs[i]);
      if (check_metadata_exists (cursor, theme_name, root, &file))
        return file;
    }

  resource_root = g_file_new_for_uri (RESOURCE_CURSOR_THEME_URI_BASE);
  if (check_metadata_exists (cursor, theme_name, resource_root, &file))
    return file;

  return NULL;
}

static gboolean
load_cursor (StCursor  *cursor,
             GFile     *file,
             GError   **error)
{
  g_autoptr (JsonParser) json_parser = NULL;
  g_autoptr (GFile) dir = NULL;
  g_autoptr (GInputStream) istream = NULL;
  g_autofree char *uri = NULL;
  JsonNode *root;
  JsonArray *frames;
  int i;

  istream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  if (!istream)
    return FALSE;

  json_parser = json_parser_new ();

  if (!json_parser_load_from_stream (json_parser, istream, NULL, error))
    return FALSE;

  root = json_parser_get_root (json_parser);
  if (!root || !JSON_NODE_HOLDS_ARRAY (root))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Malformed JSON");
      return FALSE;
    }

  dir = g_file_get_parent (file);
  frames = json_node_get_array (root);

  for (i = 0; i < json_array_get_length (frames); i++)
    {
      StCursorFrame frame = { 0, };
      JsonObject *frame_desc;
      const char *filename;
      g_autoptr (GFile) svg_file = NULL;

      frame_desc = json_array_get_object_element (frames, i);
      if (!frame_desc)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Malformed JSON");
          return FALSE;
        }

      filename = json_object_get_string_member_with_default (frame_desc, "filename", NULL);
      svg_file = g_file_get_child (dir, filename);

      frame.image =
        rsvg_handle_new_from_gfile_sync (svg_file, RSVG_HANDLE_FLAGS_NONE, NULL, error);
      if (!frame.image)
        return FALSE;

      rsvg_handle_get_intrinsic_size_in_pixels (frame.image, &frame.width, &frame.height);

      frame.delay =
        json_object_get_int_member_with_default (frame_desc, "delay", 0);
      frame.hotspot_x =
        json_object_get_double_member_with_default (frame_desc, "hotspot_x", 0);
      frame.hotspot_y =
        json_object_get_double_member_with_default (frame_desc, "hotspot_y", 0);
      frame.nominal_size =
        json_object_get_int_member_with_default (frame_desc, "nominal_size", 0);

      g_array_append_val (cursor->frames, frame);
    }

  return TRUE;
}

static void
ensure_cursor (StCursor *cursor)
{
  ClutterCursorType cursor_type =
    clutter_cursor_get_cursor_type (CLUTTER_CURSOR (cursor));
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  if (cursor_type == CLUTTER_CURSOR_NONE)
    return;

  file = find_cursor_metadata (cursor);

  if (!file || !load_cursor (cursor, file, &error))
    {
      g_autoptr (GFile) fallback = NULL;

      g_warning ("Could not load cursor '%s': %s",
                 g_file_peek_path (file), error->message);

      g_clear_object (&file);
      g_clear_error (&error);
      g_array_set_size (cursor->frames, 0);

      fallback = g_file_new_for_uri (RESOURCE_CURSOR_THEME_URI_BASE);

      if (!check_metadata_exists (cursor, FALLBACK_THEME_NAME,
                                  fallback, &file) ||
          !load_cursor (cursor, file, &error))
        {
          ClutterCursorType cursor_type =
            clutter_cursor_get_cursor_type (CLUTTER_CURSOR (cursor));

          g_error ("Could not load fallback for '%s': %s",
                   clutter_cursor_type_to_name (cursor_type),
                   error->message);
        }
    }
}

static CoglTexture *
create_texture_for_frame (StCursor      *cursor,
                          StCursorFrame *frame)
{
  MetaBackend *backend = meta_cursor_get_backend (META_CURSOR (cursor));
  ClutterActor *stage = meta_backend_get_stage (backend);
  ClutterContext *clutter_ctx = clutter_actor_get_context (stage);
  ClutterBackend *clutter_backend = clutter_context_get_backend (clutter_ctx);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglTexture *texture = NULL;
  cairo_surface_t *cairo_surface = NULL;
  cairo_t *cairo_ctx = NULL;
  double w, h, scale;

  scale = (double) meta_cursor_get_size (META_CURSOR (cursor)) / frame->nominal_size;
  w = frame->width * scale * cursor->scale;
  h = frame->height * scale * cursor->scale;

  cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  if (!cairo_surface)
    goto error;

  cairo_ctx = cairo_create (cairo_surface);
  if (!cairo_ctx)
    goto error;

  if (!rsvg_handle_render_document (frame->image, cairo_ctx,
                                    &(const RsvgRectangle) { 0, 0, w, h },
                                    NULL))
    goto error;

  texture = cogl_texture_2d_new_from_data (cogl_context,
                                           w, h,
                                           COGL_PIXEL_FORMAT_CAIRO_ARGB32_COMPAT,
                                           cairo_image_surface_get_stride (cairo_surface),
                                           cairo_image_surface_get_data (cairo_surface),
                                           NULL);

 error:
  g_clear_pointer (&cairo_ctx, cairo_destroy);
  g_clear_pointer (&cairo_surface, cairo_surface_destroy);

  return texture;
}

static void
st_cursor_constructed (GObject *object)
{
  StCursor *cursor = ST_CURSOR (object);

  G_OBJECT_CLASS (st_cursor_parent_class)->constructed (object);

  ensure_cursor (cursor);
}

static void
st_cursor_finalize (GObject *object)
{
  StCursor *cursor = ST_CURSOR (object);

  g_clear_pointer (&cursor->frames, g_array_unref);

  return G_OBJECT_CLASS (st_cursor_parent_class)->finalize (object);
}

static CoglTexture *
st_cursor_get_texture (ClutterCursor *clutter_cursor,
                       int           *hot_x,
                       int           *hot_y)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);
  StCursorFrame *frame;
  double scale;

  if (cursor->frames->len == 0)
    return NULL;

  g_assert (cursor->current_frame < cursor->frames->len);
  frame = &g_array_index (cursor->frames, StCursorFrame, cursor->current_frame);

  scale = (double) meta_cursor_get_size (META_CURSOR (cursor)) / frame->nominal_size;

  if (hot_x)
    *hot_x = frame->hotspot_x * scale * cursor->scale;
  if (hot_y)
    *hot_y = frame->hotspot_y * scale * cursor->scale;

  return frame->texture;
}

static void
st_cursor_invalidate (ClutterCursor *clutter_cursor)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);

  cursor->invalidated = TRUE;
}

static gboolean
st_cursor_realize_texture (ClutterCursor *clutter_cursor)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);
  StCursorFrame *frame;
  CoglTexture *texture;

  if (cursor->frames->len == 0)
    return FALSE;
  if (!cursor->invalidated)
    return FALSE;

  g_assert (cursor->current_frame < cursor->frames->len);
  frame = &g_array_index (cursor->frames, StCursorFrame, cursor->current_frame);

  texture = create_texture_for_frame (cursor, frame);
  g_set_object (&frame->texture, texture);
  cursor->invalidated = FALSE;

  clutter_cursor_emit_texture_changed (clutter_cursor);

  return frame->texture != NULL;
}

static gboolean
st_cursor_is_animated (ClutterCursor *clutter_cursor)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);

  return cursor->frames->len > 1;
}

static void
st_cursor_tick_frame (ClutterCursor *clutter_cursor)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);

  g_assert (cursor->current_frame < cursor->frames->len);

  cursor->current_frame++;
  if (cursor->current_frame >= cursor->frames->len)
    cursor->current_frame = 0;

  cursor->invalidated = TRUE;
}

static unsigned int
st_cursor_get_current_frame_time (ClutterCursor *clutter_cursor)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);
  StCursorFrame *frame;

  g_assert (cursor->current_frame < cursor->frames->len);
  frame = &g_array_index (cursor->frames, StCursorFrame, cursor->current_frame);

  return frame->delay;
}

static void
st_cursor_prepare_at (ClutterCursor *clutter_cursor,
                      float          best_scale,
                      float          x,
                      float          y)
{
  StCursor *cursor = ST_CURSOR (clutter_cursor);
  StCursorFrame *frame;
  double scale;

  if (cursor->current_frame >= cursor->frames->len)
    return;
  if (cursor->scale == best_scale)
    return;

  cursor->scale = best_scale;
  cursor->invalidated = TRUE;

  frame = &g_array_index (cursor->frames, StCursorFrame, cursor->current_frame);
  scale = (double) meta_cursor_get_size (META_CURSOR (cursor)) / frame->nominal_size;
  clutter_cursor_set_viewport_dst_size (clutter_cursor,
                                        frame->width * scale,
                                        frame->height * scale);
}

static void
st_cursor_class_init (StCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterCursorClass *clutter_cursor_class = CLUTTER_CURSOR_CLASS (klass);

  object_class->finalize = st_cursor_finalize;
  object_class->constructed = st_cursor_constructed;

  clutter_cursor_class->get_texture = st_cursor_get_texture;
  clutter_cursor_class->invalidate = st_cursor_invalidate;
  clutter_cursor_class->realize_texture = st_cursor_realize_texture;
  clutter_cursor_class->is_animated = st_cursor_is_animated;
  clutter_cursor_class->tick_frame = st_cursor_tick_frame;
  clutter_cursor_class->get_current_frame_time =
    st_cursor_get_current_frame_time;
  clutter_cursor_class->prepare_at = st_cursor_prepare_at;
}

static void
st_cursor_init (StCursor *cursor)
{
  cursor->frames = g_array_new (FALSE, FALSE, sizeof (StCursorFrame));
  g_array_set_clear_func (cursor->frames, (GDestroyNotify) clear_frame);
}
