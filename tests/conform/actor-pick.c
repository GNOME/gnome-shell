#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>

#define STAGE_WIDTH  640
#define STAGE_HEIGHT 480
#define ACTORS_X 12
#define ACTORS_Y 16
#define SHIFT_STEP STAGE_WIDTH / ACTORS_X

typedef struct _State State;

struct _State
{
  ClutterActor *stage;
  int y, x;
  ClutterActor *actors[ACTORS_X * ACTORS_Y];
  guint actor_width, actor_height;
  gboolean pass;
};

struct _ShiftEffect
{
  ClutterShaderEffect parent_instance;
};

struct _ShiftEffectClass
{
  ClutterShaderEffectClass parent_class;
};

typedef struct _ShiftEffect       ShiftEffect;
typedef struct _ShiftEffectClass  ShiftEffectClass;

#define TYPE_SHIFT_EFFECT        (shift_effect_get_type ())

GType shift_effect_get_type (void);

G_DEFINE_TYPE (ShiftEffect,
               shift_effect,
               CLUTTER_TYPE_SHADER_EFFECT);

static void
shader_paint (ClutterEffect           *effect,
              ClutterEffectPaintFlags  flags)
{
  ClutterShaderEffect *shader = CLUTTER_SHADER_EFFECT (effect);
  float tex_width;
  ClutterActor *actor =
    clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));

  if (g_test_verbose ())
    g_debug ("shader_paint");

  clutter_shader_effect_set_shader_source (shader,
    "uniform sampler2D tex;\n"
    "uniform float step;\n"
    "void main (void)\n"
    "{\n"
    "  cogl_color_out = texture2D(tex, vec2 (cogl_tex_coord_in[0].s + step,\n"
    "                                        cogl_tex_coord_in[0].t));\n"
    "}\n");

  tex_width = clutter_actor_get_width (actor);

  clutter_shader_effect_set_uniform (shader, "tex", G_TYPE_INT, 1, 0);
  clutter_shader_effect_set_uniform (shader, "step", G_TYPE_FLOAT, 1,
                                     SHIFT_STEP / tex_width);

  CLUTTER_EFFECT_CLASS (shift_effect_parent_class)->paint (effect, flags);
}

static void
shader_pick (ClutterEffect           *effect,
             ClutterEffectPaintFlags  flags)
{
  shader_paint (effect, flags);
}

static void
shift_effect_class_init (ShiftEffectClass *klass)
{
  ClutterEffectClass *shader_class = CLUTTER_EFFECT_CLASS (klass);

  shader_class->paint = shader_paint;
  shader_class->pick = shader_pick;
}

static void
shift_effect_init (ShiftEffect *self)
{
}

static gboolean
on_timeout (gpointer data)
{
  State *state = data;
  int test_num = 0;
  int y, x;
  ClutterActor *over_actor = NULL;

  /* This will cause an unclipped pick redraw that will get buffered.
     We'll check below that this buffer is discarded because we also need
     to pick non-reactive actors */
  clutter_stage_get_actor_at_pos (CLUTTER_STAGE (state->stage),
                                  CLUTTER_PICK_REACTIVE, 10, 10);

  clutter_stage_get_actor_at_pos (CLUTTER_STAGE (state->stage),
                                  CLUTTER_PICK_REACTIVE, 10, 10);

  for (test_num = 0; test_num < 5; test_num++)
    {
      if (test_num == 0)
        {
          if (g_test_verbose ())
            g_print ("No covering actor:\n");
        }
      if (test_num == 1)
        {
          static const ClutterColor red = { 0xff, 0x00, 0x00, 0xff };
          /* Create an actor that covers the whole stage but that
             isn't visible so it shouldn't affect the picking */
          over_actor = clutter_rectangle_new_with_color (&red);
          clutter_actor_set_size (over_actor, STAGE_WIDTH, STAGE_HEIGHT);
          clutter_actor_add_child (state->stage, over_actor);
          clutter_actor_hide (over_actor);

          if (g_test_verbose ())
            g_print ("Invisible covering actor:\n");
        }
      else if (test_num == 2)
        {
          /* Make the actor visible but set a clip so that only some
             of the actors are accessible */
          clutter_actor_show (over_actor);
          clutter_actor_set_clip (over_actor,
                                  state->actor_width * 2,
                                  state->actor_height * 2,
                                  state->actor_width * (ACTORS_X - 4),
                                  state->actor_height * (ACTORS_Y - 4));

          if (g_test_verbose ())
            g_print ("Clipped covering actor:\n");
        }
      else if (test_num == 3)
        {
          if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
            continue;

          clutter_actor_hide (over_actor);

          clutter_actor_add_effect_with_name (CLUTTER_ACTOR (state->stage),
                                              "blur",
                                              clutter_blur_effect_new ());

          if (g_test_verbose ())
            g_print ("With blur effect:\n");
        }
      else if (test_num == 4)
        {
          if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
            continue;

          clutter_actor_hide (over_actor);
          clutter_actor_remove_effect_by_name (CLUTTER_ACTOR (state->stage),
                                               "blur");

          clutter_actor_add_effect_with_name (CLUTTER_ACTOR (state->stage),
                                              "shift",
                                              g_object_new (TYPE_SHIFT_EFFECT,
                                                            NULL));

          if (g_test_verbose ())
            g_print ("With shift effect:\n");
        }

      for (y = 0; y < ACTORS_Y; y++)
        {
          if (test_num == 4)
            x = 1;
          else
            x = 0;

          for (; x < ACTORS_X; x++)
            {
              gboolean pass = FALSE;
              gfloat pick_x;
              ClutterActor *actor;

              pick_x = x * state->actor_width + state->actor_width / 2;

              if (test_num == 4)
                pick_x -= SHIFT_STEP;

              actor =
                clutter_stage_get_actor_at_pos (CLUTTER_STAGE (state->stage),
                                                CLUTTER_PICK_ALL,
                                                pick_x,
                                                y * state->actor_height
                                                + state->actor_height / 2);

              if (g_test_verbose ())
                g_print ("% 3i,% 3i / %p -> ",
                         x, y, state->actors[y * ACTORS_X + x]);

              if (actor == NULL)
                {
                  if (g_test_verbose ())
                    g_print ("NULL:       FAIL\n");
                }
              else if (actor == over_actor)
                {
                  if (test_num == 2
                      && x >= 2 && x < ACTORS_X - 2
                      && y >= 2 && y < ACTORS_Y - 2)
                    pass = TRUE;

                  if (g_test_verbose ())
                    g_print ("over_actor: %s\n", pass ? "pass" : "FAIL");
                }
              else
                {
                  if (actor == state->actors[y * ACTORS_X + x]
                      && (test_num != 2
                          || x < 2 || x >= ACTORS_X - 2
                          || y < 2 || y >= ACTORS_Y - 2))
                    pass = TRUE;

                  if (g_test_verbose ())
                    g_print ("%p: %s\n", actor, pass ? "pass" : "FAIL");
                }

              if (!pass)
                state->pass = FALSE;
            }
        }
    }

  clutter_main_quit ();

  return G_SOURCE_REMOVE;
}

static void
actor_pick (void)
{
  int y, x;
  State state;
  
  state.pass = TRUE;

  state.stage = clutter_test_get_stage ();

  state.actor_width = STAGE_WIDTH / ACTORS_X;
  state.actor_height = STAGE_HEIGHT / ACTORS_Y;

  for (y = 0; y < ACTORS_Y; y++)
    for (x = 0; x < ACTORS_X; x++)
      {
	ClutterColor color = { x * 255 / (ACTORS_X - 1),
			       y * 255 / (ACTORS_Y - 1),
			       128, 255 };
	ClutterActor *rect = clutter_rectangle_new_with_color (&color);

        clutter_actor_set_position (rect,
                                    x * state.actor_width,
                                    y * state.actor_height);
        clutter_actor_set_size (rect,
                                state.actor_width,
                                state.actor_height);

	clutter_actor_add_child (state.stage, rect);

	state.actors[y * ACTORS_X + x] = rect;
      }

  clutter_actor_show (state.stage);

  clutter_threads_add_idle (on_timeout, &state);

  clutter_main ();

  g_assert (state.pass);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/pick", actor_pick)
)
