
#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

/* This test verifies that interleved attributes work with the mesh API.
 * We add (x,y) GLfloat vertices, interleved with RGBA GLubyte color
 * attributes to a mesh object, submit and draw.
 *
 * If you want visual feedback of what this test paints for debugging purposes,
 * then remove the call to clutter_main_quit() in validate_result.
 */

typedef struct _TestState
{
  CoglHandle mesh;
  ClutterGeometry stage_geom;
  guint frame;
} TestState;

typedef struct _InterlevedVertex
{
  GLfloat x;
  GLfloat y;

  GLubyte r;
  GLubyte g;
  GLubyte b;
  GLubyte a;
} InterlevedVertex;


static void
validate_result (TestState *state)
{
  GLubyte pixel[4];
  GLint y_off = state->stage_geom.height - 90;
  /* NB: glReadPixels is done in GL screen space so y = 0 is at the bottom */

  /* NB: We ignore the alpha, since we don't know if our render target is
   * RGB or RGBA */

#define RED 0
#define GREEN 1
#define BLUE 2

  /* Should see a blue pixel */
  glReadPixels (10, y_off, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  g_print ("pixel 0 = %x, %x, %x\n", pixel[RED], pixel[GREEN], pixel[BLUE]);
  g_assert (pixel[RED] == 0 && pixel[GREEN] == 0 && pixel[BLUE] != 0);
   
#undef RED
#undef GREEN
#undef BLUE
  
  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  /* Draw a faded blue triangle */
  cogl_mesh_draw_arrays (state->mesh,
			 GL_TRIANGLE_STRIP, /* mode */
			 0, /* first */
			 3); /* count */

  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  if (state->frame >= 2)
    validate_result (state);
  else
    sleep (1);
  
  state->frame++;
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

void
test_mesh_interleved (TestConformSimpleFixture *fixture,
		      gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterColor stage_clr = {0x0, 0x0, 0x0, 0xff};
  ClutterActor *group;

  state.frame = 0;

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_clr);
  clutter_actor_get_geometry (stage, &state.stage_geom);

  group = clutter_group_new ();
  clutter_actor_set_size (group,
			  state.stage_geom.width,
			  state.stage_geom.height);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  g_idle_add (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);
  
  {
    InterlevedVertex verts[3] =
      {
	{ /* .x = */ 0.0, /* .y = */ 0.0,
	  /* blue */
	  /* .r = */ 0x00, /* .g = */ 0x00, /* .b = */ 0xff, /* .a = */ 0xff },

	{ /* .x = */ 100.0, /* .y = */ 100.0,
	  /* transparent blue */
	  /* .r = */ 0x00, /* .g = */ 0x00, /* .b = */ 0xff, /* .a = */ 0x00 },

	{ /* .x = */ 0.0, /* .y = */ 100.0,
	  /* transparent blue */
	  /* .r = */ 0x00, /* .g = */ 0x00, /* .b = */ 0xff, /* .a = */ 0x00 },
      };

    /* We assume the compiler is doing no funny struct padding for this test:
     */
    g_assert (sizeof (InterlevedVertex) == 12);

    state.mesh = cogl_mesh_new (3 /* n vertices */);
    cogl_mesh_add_attribute (state.mesh,
			     "gl_Vertex",
			     2, /* n components */
			     GL_FLOAT,
			     FALSE, /* normalized */
			     12, /* stride */
			     &verts[0].x);
    cogl_mesh_add_attribute (state.mesh,
			     "gl_Color",
			     4, /* n components */
			     GL_UNSIGNED_BYTE,
			     FALSE, /* normalized */
			     12, /* stride */
			     &verts[0].r);
    cogl_mesh_submit (state.mesh);
  }

  clutter_actor_show_all (stage);

  clutter_main ();

  cogl_mesh_unref (state.mesh);

  g_print ("OK\n");
}

