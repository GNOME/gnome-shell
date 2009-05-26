#include <config.h>
#include <glib.h>
#include <gmodule.h>
#include <stdlib.h>
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <math.h>

/* Defines the size and resolution of the quad mesh we morph:
 */
#define MESH_WIDTH  100.0 /* number of quads along x axis */
#define MESH_HEIGHT 100.0 /* number of quads along y axis */
#define QUAD_WIDTH  5.0 /* width in pixels of a single quad */
#define QUAD_HEIGHT 5.0 /* height in pixels of a single quad */

/* Defines a sine wave that sweeps across the mesh:
 */
#define WAVE_DEPTH    ((MESH_WIDTH * QUAD_WIDTH) / 16.0) /* peak amplitude */
#define WAVE_PERIODS  4.0
#define WAVE_SPEED    10.0

/* Defines a rippling sine wave emitted from a point:
 */
#define RIPPLE_CENTER_X ((MESH_WIDTH / 2.0) * QUAD_WIDTH)
#define RIPPLE_CENTER_Y ((MESH_HEIGHT / 2.0) * QUAD_HEIGHT)
#define RIPPLE_RADIUS   (MESH_WIDTH * QUAD_WIDTH)
#define RIPPLE_DEPTH    ((MESH_WIDTH * QUAD_WIDTH) / 16.0) /* peak amplitude */
#define RIPPLE_PERIODS  4.0
#define RIPPLE_SPEED    -10.0

/* Defines the width of the gaussian bell used to fade out the alpha
 * towards the edges of the mesh (starting from the ripple center):
 */
#define GAUSSIAN_RADIUS ((MESH_WIDTH * QUAD_WIDTH) / 6.0)

/* Our hues lie in the range [0, 1], and this defines how we map amplitude
 * to hues (before scaling by {WAVE,RIPPLE}_DEPTH)
 * As we are interferring two sine waves together; amplitudes lie in the
 * range [-2, 2]
 */
#define HSL_OFFSET    0.5 /* the hue that we map an amplitude of 0 too */
#define HSL_SCALE     0.25

typedef struct _TestState
{
  ClutterActor    *dummy;
  CoglHandle       buffer;
  float           *quad_mesh_verts;
  GLubyte         *quad_mesh_colors;
  GLushort        *static_indices;
  guint            n_static_indices;
  int              indices_id;
  ClutterTimeline *timeline;
} TestState;

static void
frame_cb (ClutterTimeline *timeline,
          gint             frame_num,
          TestState       *state)
{
  guint x, y;
  guint n_frames = clutter_timeline_get_n_frames (timeline);
  float period_progress = ((float)frame_num / (float)n_frames) * 2.0 * G_PI;
  float period_progress_sin = sinf (period_progress);
  float wave_shift = period_progress * WAVE_SPEED;
  float ripple_shift = period_progress * RIPPLE_SPEED;

  for (y = 0; y <= MESH_HEIGHT; y++)
    for (x = 0; x <= MESH_WIDTH; x++)
      {
        guint    vert_index = (MESH_WIDTH + 1) * y + x;
        float   *vert = &state->quad_mesh_verts[3 * vert_index];

        float    real_x = x * QUAD_WIDTH;
        float    real_y = y * QUAD_HEIGHT;

        float    wave_offset = (float)x / (MESH_WIDTH + 1);
        float    wave_angle =
                    (WAVE_PERIODS * 2 * G_PI * wave_offset) + wave_shift;
        float    wave_sin = sinf (wave_angle);

        float    a_sqr = (RIPPLE_CENTER_X - real_x) * (RIPPLE_CENTER_X - real_x);
        float    b_sqr = (RIPPLE_CENTER_Y - real_y) * (RIPPLE_CENTER_Y - real_y);
        float    ripple_offset = sqrtf (a_sqr + b_sqr) / RIPPLE_RADIUS;
        float    ripple_angle =
                    (RIPPLE_PERIODS * 2 * G_PI * ripple_offset) + ripple_shift;
        float    ripple_sin = sinf (ripple_angle);

        float    h, s, l;
        GLubyte *color;

        vert[2] = (wave_sin * WAVE_DEPTH) + (ripple_sin * RIPPLE_DEPTH);

        /* Burn some CPU time picking a pretty color... */
        h = (HSL_OFFSET
             + wave_sin
             + ripple_sin
             + period_progress_sin) * HSL_SCALE;
        s = 0.5;
        l = 0.25 + (period_progress_sin + 1.0) / 4.0;
        color = &state->quad_mesh_colors[4 * vert_index];
        /* A bit of a sneaky cast, but it seems safe to assume the ClutterColor
         * typedef is set in stone... */
        clutter_color_from_hls ((ClutterColor *)color, h * 360.0, l, s);
      }

  cogl_vertex_buffer_add (state->buffer,
                          "gl_Vertex",
                          3, /* n components */
                          GL_FLOAT,
                          FALSE, /* normalized */
                          0, /* stride */
                          state->quad_mesh_verts);
  cogl_vertex_buffer_add (state->buffer,
                          "gl_Color",
                          4, /* n components */
                          GL_UNSIGNED_BYTE,
                          FALSE, /* normalized */
                          0, /* stride */
                          state->quad_mesh_colors);

  cogl_vertex_buffer_submit (state->buffer);

  clutter_actor_set_rotation (state->dummy,
                              CLUTTER_Z_AXIS,
                              frame_num,
                              (MESH_WIDTH * QUAD_WIDTH) / 2,
                              (MESH_HEIGHT * QUAD_HEIGHT) / 2,
                              0);
  clutter_actor_set_rotation (state->dummy,
                              CLUTTER_X_AXIS,
                              frame_num,
                              (MESH_WIDTH * QUAD_WIDTH) / 2,
                              (MESH_HEIGHT * QUAD_HEIGHT) / 2,
                              0);
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_vertex_buffer_draw_elements (state->buffer,
                                    COGL_VERTICES_MODE_TRIANGLE_STRIP,
                                    state->indices_id,
                                    0, /* indices offset */
                                    state->n_static_indices);
}

static void
init_static_index_arrays (TestState *state)
{
  guint     n_indices;
  int       x, y;
  GLushort *i;
  guint     dir;

  /* - Each row takes (2 + 2 * MESH_WIDTH indices)
   *    - Thats 2 to start the triangle strip then 2 indices to add 2 triangles
   *      per mesh quad.
   * - We have MESH_HEIGHT rows
   * - It takes one extra index for linking between rows (MESH_HEIGHT - 1)
   * - A 2 x 3 mesh == 20 indices... */
  n_indices = (2 + 2 * MESH_WIDTH) * MESH_HEIGHT + (MESH_HEIGHT - 1);
  state->static_indices = g_malloc (sizeof (GLushort) * n_indices);
  state->n_static_indices = n_indices;

#define MESH_INDEX(X, Y) (Y) * (MESH_WIDTH + 1) + (X)

  i = state->static_indices;

  /* NB: front facing == anti-clockwise winding */

  i[0] = MESH_INDEX (0, 0);
  i[1] = MESH_INDEX (0, 1);
  i += 2;

#define LEFT  0
#define RIGHT 1

  dir = RIGHT;

  for (y = 0; y < MESH_HEIGHT; y++)
    {
      for (x = 0; x < MESH_WIDTH; x++)
        {
          /* Add 2 triangles per mesh quad... */
          if (dir == RIGHT)
            {
              i[0] = MESH_INDEX (x + 1, y);
              i[1] = MESH_INDEX (x + 1, y + 1);
            }
          else
            {
              i[0] = MESH_INDEX (MESH_WIDTH - x - 1, y);
              i[1] = MESH_INDEX (MESH_WIDTH - x - 1, y + 1);
            }
          i += 2;
        }

      /* Link rows... */

      if (y == (MESH_HEIGHT - 1))
        break;

      if (dir == RIGHT)
        {
          i[0] = MESH_INDEX (MESH_WIDTH, y + 1);
          i[1] = MESH_INDEX (MESH_WIDTH, y + 1);
          i[2] = MESH_INDEX (MESH_WIDTH, y + 2);
        }
      else
        {
          i[0] = MESH_INDEX (0, y + 1);
          i[1] = MESH_INDEX (0, y + 1);
          i[2] = MESH_INDEX (0, y + 2);
        }
      i += 3;
      dir = !dir;
    }

#undef MESH_INDEX

  state->indices_id =
    cogl_vertex_buffer_add_indices (state->buffer,
                                    0, /* min index */
                                    (MESH_WIDTH + 1) *
                                    (MESH_HEIGHT + 1), /* max index */
                                    COGL_INDICES_TYPE_UNSIGNED_SHORT,
                                    state->static_indices,
                                    state->n_static_indices);
}

static float
gaussian (float x, float y)
{
  /* Bell width */
  float c = GAUSSIAN_RADIUS;

  /* Peak amplitude */
  float a = 1.0;
  /* float a = 1.0 / (c * sqrtf (2.0 * G_PI)); */

  /* Center offset */
  float b = 0.0;

  x = x - RIPPLE_CENTER_X;
  y = y - RIPPLE_CENTER_Y;
  float dist = sqrtf (x*x + y*y);

  return a * exp ((- ((dist - b) * (dist - b))) / (2.0 * c * c));
}

static void
init_quad_mesh (TestState *state)
{
  int x, y;
  float *vert;
  GLubyte *color;

  /* Note: we maintain the minimum number of vertices possible. This minimizes
   * the work required when we come to morph the geometry.
   *
   * We use static indices into our mesh so that we can treat the data like a
   * single triangle list and drawing can be done in one operation (Note: We
   * are using degenerate triangles at the edges to link to the next row)
   */
  state->quad_mesh_verts =
    g_malloc0 (sizeof (float) * 3 * (MESH_WIDTH + 1) * (MESH_HEIGHT + 1));

  state->quad_mesh_colors =
    g_malloc0 (sizeof (GLubyte) * 4 * (MESH_WIDTH + 1) * (MESH_HEIGHT + 1));

  vert = state->quad_mesh_verts;
  color = state->quad_mesh_colors;
  for (y = 0; y <= MESH_HEIGHT; y++)
    for (x = 0; x <= MESH_WIDTH; x++)
      {
        vert[0] = x * QUAD_WIDTH;
        vert[1] = y * QUAD_HEIGHT;
        vert += 3;

        color[3] = gaussian (x * QUAD_WIDTH,
                             y * QUAD_HEIGHT) * 255.0;
        color += 4;
      }

  state->buffer = cogl_vertex_buffer_new ((MESH_WIDTH + 1)*(MESH_HEIGHT + 1));
  cogl_vertex_buffer_add (state->buffer,
                          "gl_Vertex",
                          3, /* n components */
                          COGL_ATTRIBUTE_TYPE_FLOAT,
                          FALSE, /* normalized */
                          0, /* stride */
                          state->quad_mesh_verts);

  cogl_vertex_buffer_add (state->buffer,
                          "gl_Color",
                          4, /* n components */
                          COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE,
                          FALSE, /* normalized */
                          0, /* stride */
                          state->quad_mesh_colors);

  cogl_vertex_buffer_submit (state->buffer);

  init_static_index_arrays (state);
}

/* This creates an actor that has a specific size but that does not result
 * in any drawing so we can do our own drawing using Cogl... */
static ClutterActor *
create_dummy_actor (guint width, guint height)
{
  ClutterActor *group, *rect;
  ClutterColor clr = { 0xff, 0xff, 0xff, 0xff};

  group = clutter_group_new ();
  rect = clutter_rectangle_new_with_color (&clr);
  clutter_actor_set_size (rect, width, height);
  clutter_actor_hide (rect);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), rect);
  return group;
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

G_MODULE_EXPORT int
test_cogl_vertex_buffer_main (int argc, char *argv[])
{
  TestState       state;
  ClutterActor   *stage;
  ClutterColor    stage_clr = {0x0, 0x0, 0x0, 0xff};
  ClutterGeometry stage_geom;
  gint            dummy_width, dummy_height;
  guint           idle_source;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_clr);
  clutter_actor_get_geometry (stage, &stage_geom);

  dummy_width = MESH_WIDTH * QUAD_WIDTH;
  dummy_height = MESH_HEIGHT * QUAD_HEIGHT;
  state.dummy = create_dummy_actor (dummy_width, dummy_height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), state.dummy);
  clutter_actor_set_position (state.dummy,
                              (stage_geom.width / 2.0) - (dummy_width / 2.0),
                              (stage_geom.height / 2.0) - (dummy_height / 2.0));

  state.timeline = clutter_timeline_new (360, 60);
  clutter_timeline_set_loop (state.timeline, TRUE);
  g_signal_connect (state.timeline,
                    "new-frame",
                    G_CALLBACK (frame_cb),
                    &state);

  /* We want continuous redrawing of the stage... */
  idle_source = g_idle_add (queue_redraw, stage);

  g_signal_connect (state.dummy, "paint", G_CALLBACK (on_paint), &state);

  init_quad_mesh (&state);

  clutter_actor_show_all (stage);

  clutter_timeline_start (state.timeline);

  clutter_main ();

  cogl_handle_unref (state.buffer);

  g_source_remove (idle_source);

  return 0;
}

