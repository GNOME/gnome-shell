#include <clutter/clutter.h>
#include <cairo.h>
#include <string.h>
#include <math.h>

#include "test-conform-common.h"

#define MAX_NODES 128

#define FLOAT_FUZZ_AMOUNT 5.0f

typedef struct _CallbackData CallbackData;

typedef gboolean (* PathTestFunc) (CallbackData *data);

static void compare_node (const ClutterPathNode *node, gpointer data_p);

struct _CallbackData
{
  ClutterPath *path;

  guint n_nodes;
  ClutterPathNode nodes[MAX_NODES];

  gboolean nodes_different;
  guint nodes_found;
};

static const char path_desc[] =
  "M 21 22 "
  "L 25 26 "
  "C 29 30 31 32 33 34 "
  "m 23 24 "
  "l 27 28 "
  "c 35 36 37 38 39 40 "
  "z";
static const ClutterPathNode path_nodes[] =
  { { CLUTTER_PATH_MOVE_TO,      { { 21, 22 }, { 0,  0 },  { 0,  0 } } },
    { CLUTTER_PATH_LINE_TO,      { { 25, 26 }, { 0,  0 },  { 0,  0 } } },
    { CLUTTER_PATH_CURVE_TO,     { { 29, 30 }, { 31, 32 }, { 33, 34 } } },
    { CLUTTER_PATH_REL_MOVE_TO,  { { 23, 24 }, { 0,  0 },  { 0,  0 } } },
    { CLUTTER_PATH_REL_LINE_TO,  { { 27, 28 }, { 0,  0 },  { 0,  0 } } },
    { CLUTTER_PATH_REL_CURVE_TO, { { 35, 36 }, { 37, 38 }, { 39, 40 } } },
    { CLUTTER_PATH_CLOSE,        { { 0,  0 },  { 0,  0 },  { 0,  0 } } } };

static gboolean
path_test_add_move_to (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_MOVE_TO;
  node.points[0].x = 1;
  node.points[0].y = 2;

  clutter_path_add_move_to (data->path, node.points[0].x, node.points[0].y);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_line_to (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_LINE_TO;
  node.points[0].x = 3;
  node.points[0].y = 4;

  clutter_path_add_line_to (data->path, node.points[0].x, node.points[0].y);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_curve_to (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_CURVE_TO;
  node.points[0].x = 5;
  node.points[0].y = 6;
  node.points[1].x = 7;
  node.points[1].y = 8;
  node.points[2].x = 9;
  node.points[2].y = 10;

  clutter_path_add_curve_to (data->path,
                             node.points[0].x, node.points[0].y,
                             node.points[1].x, node.points[1].y,
                             node.points[2].x, node.points[2].y);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_close (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_CLOSE;

  clutter_path_add_close (data->path);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_rel_move_to (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_REL_MOVE_TO;
  node.points[0].x = 11;
  node.points[0].y = 12;

  clutter_path_add_rel_move_to (data->path, node.points[0].x, node.points[0].y);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_rel_line_to (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_REL_LINE_TO;
  node.points[0].x = 13;
  node.points[0].y = 14;

  clutter_path_add_rel_line_to (data->path, node.points[0].x, node.points[0].y);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_rel_curve_to (CallbackData *data)
{
  ClutterPathNode node = { 0, };

  node.type = CLUTTER_PATH_REL_CURVE_TO;
  node.points[0].x = 15;
  node.points[0].y = 16;
  node.points[1].x = 17;
  node.points[1].y = 18;
  node.points[2].x = 19;
  node.points[2].y = 20;

  clutter_path_add_rel_curve_to (data->path,
                                 node.points[0].x, node.points[0].y,
                                 node.points[1].x, node.points[1].y,
                                 node.points[2].x, node.points[2].y);

  data->nodes[data->n_nodes++] = node;

  return TRUE;
}

static gboolean
path_test_add_string (CallbackData *data)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (path_nodes); i++)
    data->nodes[data->n_nodes++] = path_nodes[i];

  clutter_path_add_string (data->path, path_desc);

  return TRUE;
}

static gboolean
path_test_add_node_by_struct (CallbackData *data)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (path_nodes); i++)
    {
      data->nodes[data->n_nodes++] = path_nodes[i];
      clutter_path_add_node (data->path, path_nodes + i);
    }

  return TRUE;
}

static gboolean
path_test_get_n_nodes (CallbackData *data)
{
  return clutter_path_get_n_nodes (data->path) == data->n_nodes;
}

static gboolean
path_test_get_node (CallbackData *data)
{
  int i;

  data->nodes_found = 0;
  data->nodes_different = FALSE;

  for (i = 0; i < data->n_nodes; i++)
    {
      ClutterPathNode node;

      clutter_path_get_node (data->path, i, &node);

      compare_node (&node, data);
    }

  return !data->nodes_different;
}

static gboolean
path_test_get_nodes (CallbackData *data)
{
  GSList *list, *node;

  data->nodes_found = 0;
  data->nodes_different = FALSE;

  list = clutter_path_get_nodes (data->path);

  for (node = list; node; node = node->next)
    compare_node (node->data, data);

  g_slist_free (list);

  return !data->nodes_different && data->nodes_found == data->n_nodes;
}

static gboolean
path_test_insert_beginning (CallbackData *data)
{
  ClutterPathNode node;

  node.type = CLUTTER_PATH_LINE_TO;
  node.points[0].x = 41;
  node.points[0].y = 42;

  memmove (data->nodes + 1, data->nodes,
           data->n_nodes++ * sizeof (ClutterPathNode));
  data->nodes[0] = node;

  clutter_path_insert_node (data->path, 0, &node);

  return TRUE;
}

static gboolean
path_test_insert_end (CallbackData *data)
{
  ClutterPathNode node;

  node.type = CLUTTER_PATH_LINE_TO;
  node.points[0].x = 43;
  node.points[0].y = 44;

  data->nodes[data->n_nodes++] = node;

  clutter_path_insert_node (data->path, -1, &node);

  return TRUE;
}

static gboolean
path_test_insert_middle (CallbackData *data)
{
  ClutterPathNode node;
  int pos = data->n_nodes / 2;

  node.type = CLUTTER_PATH_LINE_TO;
  node.points[0].x = 45;
  node.points[0].y = 46;

  memmove (data->nodes + pos + 1, data->nodes + pos,
           (data->n_nodes - pos) * sizeof (ClutterPathNode));
  data->nodes[pos] = node;
  data->n_nodes++;

  clutter_path_insert_node (data->path, pos, &node);

  return TRUE;
}

static gboolean
path_test_clear (CallbackData *data)
{
  clutter_path_clear (data->path);

  data->n_nodes = 0;

  return TRUE;
}

static gboolean
path_test_clear_insert (CallbackData *data)
{
  return path_test_clear (data) && path_test_insert_middle (data);
}

static gboolean
path_test_remove_beginning (CallbackData *data)
{
  memmove (data->nodes, data->nodes + 1,
           --data->n_nodes * sizeof (ClutterPathNode));

  clutter_path_remove_node (data->path, 0);

  return TRUE;
}

static gboolean
path_test_remove_end (CallbackData *data)
{
  clutter_path_remove_node (data->path, --data->n_nodes);

  return TRUE;
}

static gboolean
path_test_remove_middle (CallbackData *data)
{
  int pos = data->n_nodes / 2;

  memmove (data->nodes + pos, data->nodes + pos + 1,
           (--data->n_nodes - pos) * sizeof (ClutterPathNode));

  clutter_path_remove_node (data->path, pos);

  return TRUE;
}

static gboolean
path_test_remove_only (CallbackData *data)
{
  return path_test_clear (data)
    && path_test_add_line_to (data)
    && path_test_remove_beginning (data);
}

static gboolean
path_test_replace (CallbackData *data)
{
  ClutterPathNode node;
  int pos = data->n_nodes / 2;

  node.type = CLUTTER_PATH_LINE_TO;
  node.points[0].x = 47;
  node.points[0].y = 48;

  data->nodes[pos] = node;

  clutter_path_replace_node (data->path, pos, &node);

  return TRUE;
}

static gboolean
path_test_set_description (CallbackData *data)
{
  data->n_nodes = G_N_ELEMENTS (path_nodes);
  memcpy (data->nodes, path_nodes, sizeof (path_nodes));

  return clutter_path_set_description (data->path, path_desc);
}

static gboolean
path_test_get_description (CallbackData *data)
{
  char *desc1, *desc2;
  gboolean ret = TRUE;

  desc1 = clutter_path_get_description (data->path);
  clutter_path_clear (data->path);
  if (!clutter_path_set_description (data->path, desc1))
    ret = FALSE;
  desc2 = clutter_path_get_description (data->path);

  if (strcmp (desc1, desc2))
    ret = FALSE;

  g_free (desc1);
  g_free (desc2);

  return ret;
}

static gboolean
path_test_convert_to_cairo_path (CallbackData *data)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *cpath;
  guint i, j;
  ClutterKnot path_start = { 0, 0 }, last_point = { 0, 0 };

  /* Create a temporary image surface and context to hold the cairo
     path */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
  cr = cairo_create (surface);

  /* Convert to a cairo path */
  clutter_path_to_cairo_path (data->path, cr);

  /* Get a copy of the cairo path data */
  cpath = cairo_copy_path (cr);

  /* Convert back to a clutter path */
  clutter_path_clear (data->path);
  clutter_path_add_cairo_path (data->path, cpath);

  /* The relative nodes will have been converted to absolute so we
     need to reflect this in the node array for comparison */
  for (i = 0; i < data->n_nodes; i++)
    {
      switch (data->nodes[i].type)
        {
        case CLUTTER_PATH_MOVE_TO:
          path_start = last_point = data->nodes[i].points[0];
          break;

        case CLUTTER_PATH_LINE_TO:
          last_point = data->nodes[i].points[0];
          break;

        case CLUTTER_PATH_CURVE_TO:
          last_point = data->nodes[i].points[2];
          break;

        case CLUTTER_PATH_REL_MOVE_TO:
          last_point.x += data->nodes[i].points[0].x;
          last_point.y += data->nodes[i].points[0].y;
          data->nodes[i].points[0] = last_point;
          data->nodes[i].type = CLUTTER_PATH_MOVE_TO;
          path_start = last_point;
          break;

        case CLUTTER_PATH_REL_LINE_TO:
          last_point.x += data->nodes[i].points[0].x;
          last_point.y += data->nodes[i].points[0].y;
          data->nodes[i].points[0] = last_point;
          data->nodes[i].type = CLUTTER_PATH_LINE_TO;
          break;

        case CLUTTER_PATH_REL_CURVE_TO:
          for (j = 0; j < 3; j++)
            {
              data->nodes[i].points[j].x += last_point.x;
              data->nodes[i].points[j].y += last_point.y;
            }
          last_point = data->nodes[i].points[2];
          data->nodes[i].type = CLUTTER_PATH_CURVE_TO;
          break;

        case CLUTTER_PATH_CLOSE:
          last_point = path_start;

          /* Cairo always adds a move to after every close so we need
             to insert one here. Since Cairo commit 166453c1abf2 it
             doesn't seem to do this anymore so will assume that if
             Cairo's minor version is >= 11 then it includes that
             commit */
          if (cairo_version () < CAIRO_VERSION_ENCODE (1, 11, 0))
            {
              memmove (data->nodes + i + 2, data->nodes + i + 1,
                       (data->n_nodes - i - 1) * sizeof (ClutterPathNode));
              data->nodes[i + 1].type = CLUTTER_PATH_MOVE_TO;
              data->nodes[i + 1].points[0] = last_point;
              data->n_nodes++;
            }
          break;
        }
    }

  /* Free the cairo resources */
  cairo_path_destroy (cpath);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return TRUE;
}

static gboolean
float_fuzzy_equals (float fa, float fb)
{
  return fabs (fa - fb) <= FLOAT_FUZZ_AMOUNT;
}

static void
set_triangle_path (CallbackData *data)
{
  /* Triangular shaped path hitting (0,0), (64,64) and (128,0) in four
     parts. The two curves are actually straight lines */
  static const ClutterPathNode nodes[] =
    { { CLUTTER_PATH_MOVE_TO,      { { 0, 0 } } },
      { CLUTTER_PATH_LINE_TO,      { { 32, 32 } } },
      { CLUTTER_PATH_CURVE_TO,     { { 40, 40 }, { 56, 56 }, { 64, 64 } } },
      { CLUTTER_PATH_REL_CURVE_TO, { { 8, -8 }, { 24, -24 }, { 32, -32 } } },
      { CLUTTER_PATH_REL_LINE_TO,  { { 32, -32 } } } };
  gint i;

  clutter_path_clear (data->path);

  for (i = 0; i < G_N_ELEMENTS (nodes); i++)
    clutter_path_add_node (data->path, nodes + i);

  memcpy (data->nodes, nodes, sizeof (nodes));
  data->n_nodes = G_N_ELEMENTS (nodes);
}

static gboolean
path_test_get_position (CallbackData *data)
{
  static const float values[] = { 0.125f, 16.0f, 16.0f,
                                  0.375f, 48.0f, 48.0f,
                                  0.625f, 80.0f, 48.0f,
                                  0.875f, 112.0f, 16.0f };
  gint i;

  set_triangle_path (data);

  for (i = 0; i < G_N_ELEMENTS (values); i += 3)
    {
      ClutterKnot pos;

      clutter_path_get_position (data->path,
                                 values[i],
                                 &pos);

      if (!float_fuzzy_equals (values[i + 1], pos.x)
          || !float_fuzzy_equals (values[i + 2], pos.y))
        return FALSE;
    }

  return TRUE;
}

static gboolean
path_test_get_length (CallbackData *data)
{
  const float actual_length /* sqrt(64**2 + 64**2) * 2 */ = 181.019336f;
  guint approx_length;

  clutter_path_set_description (data->path, "M 0 0 L 46340 0");
  g_object_get (data->path, "length", &approx_length, NULL);

  if (!(fabs (approx_length - 46340.f) / 46340.f <= 0.15f))
    {
      if (g_test_verbose ())
        g_print ("M 0 0 L 46340 0 - Expected 46340, got %d instead.", approx_length);

      return FALSE;
    }

  clutter_path_set_description (data->path, "M 0 0 L 46341 0");
  g_object_get (data->path, "length", &approx_length, NULL);

  if (!(fabs (approx_length - 46341.f) / 46341.f <= 0.15f))
    {
      if (g_test_verbose ())
        g_print ("M 0 0 L 46341 0 - Expected 46341, got %d instead.", approx_length);

      return FALSE;
    }

  set_triangle_path (data);

  g_object_get (data->path, "length", &approx_length, NULL);

  /* Allow 15% margin of error */
  if (!(fabs (approx_length - actual_length) / (float) actual_length <= 0.15f))
    {
      if (g_test_verbose ())
        g_print ("Expected %g, got %d instead.\n", actual_length, approx_length);

      return FALSE;
    }

  return TRUE;
}

static gboolean
path_test_boxed_type (CallbackData *data)
{
  gboolean ret = TRUE;
  GSList *nodes, *l;
  GValue value;

  nodes = clutter_path_get_nodes (data->path);

  memset (&value, 0, sizeof (value));

  for (l = nodes; l; l = l->next)
    {
      g_value_init (&value, CLUTTER_TYPE_PATH_NODE);

      g_value_set_boxed (&value, l->data);

      if (!clutter_path_node_equal (l->data,
                                    g_value_get_boxed (&value)))
        ret = FALSE;

      g_value_unset (&value);
    }

  g_slist_free (nodes);

  return ret;
}

static const struct
{
  const char *desc;
  PathTestFunc func;
}
path_tests[] =
  {
    { "Add line to", path_test_add_line_to },
    { "Add move to", path_test_add_move_to },
    { "Add curve to", path_test_add_curve_to },
    { "Add close", path_test_add_close },
    { "Add relative line to", path_test_add_rel_line_to },
    { "Add relative move to", path_test_add_rel_move_to },
    { "Add relative curve to", path_test_add_rel_curve_to },
    { "Add string", path_test_add_string },
    { "Add node by struct", path_test_add_node_by_struct },
    { "Get number of nodes", path_test_get_n_nodes },
    { "Get a node", path_test_get_node },
    { "Get all nodes", path_test_get_nodes },
    { "Insert at beginning", path_test_insert_beginning },
    { "Insert at end", path_test_insert_end },
    { "Insert at middle", path_test_insert_middle },
    { "Add after insert", path_test_add_line_to },
    { "Clear then insert", path_test_clear_insert },
    { "Add string again", path_test_add_string },
    { "Remove from beginning", path_test_remove_beginning },
    { "Remove from end", path_test_remove_end },
    { "Remove from middle", path_test_remove_middle },
    { "Add after remove", path_test_add_line_to },
    { "Remove only node", path_test_remove_only },
    { "Add after remove again", path_test_add_line_to },
    { "Replace a node", path_test_replace },
    { "Set description", path_test_set_description },
    { "Get description", path_test_get_description },
    { "Convert to cairo path and back", path_test_convert_to_cairo_path },
    { "Clear", path_test_clear },
    { "Get position", path_test_get_position },
    { "Check node boxed type", path_test_boxed_type },
    { "Get length", path_test_get_length }
  };

static void
compare_node (const ClutterPathNode *node, gpointer data_p)
{
  CallbackData *data = data_p;

  if (data->nodes_found >= data->n_nodes)
    data->nodes_different = TRUE;
  else
    {
      guint n_points = 0, i;
      const ClutterPathNode *onode = data->nodes + data->nodes_found;

      if (node->type != onode->type)
        data->nodes_different = TRUE;

      switch (node->type & ~CLUTTER_PATH_RELATIVE)
        {
        case CLUTTER_PATH_MOVE_TO: n_points = 1; break;
        case CLUTTER_PATH_LINE_TO: n_points = 1; break;
        case CLUTTER_PATH_CURVE_TO: n_points = 3; break;
        case CLUTTER_PATH_CLOSE: n_points = 0; break;

        default:
          data->nodes_different = TRUE;
          break;
        }

      for (i = 0; i < n_points; i++)
        if (node->points[i].x != onode->points[i].x
            || node->points[i].y != onode->points[i].y)
          {
            data->nodes_different = TRUE;
            break;
          }
    }

  data->nodes_found++;
}

static gboolean
compare_nodes (CallbackData *data)
{
  data->nodes_different = FALSE;
  data->nodes_found = 0;

  clutter_path_foreach (data->path, compare_node, data);

  return !data->nodes_different && data->nodes_found == data->n_nodes;
}

void
path_base (TestConformSimpleFixture *fixture,
           gconstpointer _data)
{
  CallbackData data;
  gint i;

  memset (&data, 0, sizeof (data));

  data.path = clutter_path_new ();

  for (i = 0; i < G_N_ELEMENTS (path_tests); i++)
    {
      gboolean succeeded;

      if (g_test_verbose ())
        g_print ("%s... ", path_tests[i].desc);

      succeeded = path_tests[i].func (&data) && compare_nodes (&data);

      if (g_test_verbose ())
        g_print ("%s\n", succeeded ? "ok" : "FAIL");

      g_assert (succeeded);
    }

  g_object_unref (data.path);
}

