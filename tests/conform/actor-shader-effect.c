#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include <clutter/clutter.h>

/****************************************************************
 Old style shader effect
 This uses clutter_shader_effect_set_source
 ****************************************************************/

static const gchar
old_shader_effect_source[] =
  "uniform vec3 override_color;\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = vec4 (override_color, 1.0);\n"
  "}";

typedef struct _FooOldShaderEffectClass
{
  ClutterShaderEffectClass parent_class;
} FooOldShaderEffectClass;

typedef struct _FooOldShaderEffect
{
  ClutterShaderEffect parent;
} FooOldShaderEffect;

GType foo_old_shader_effect_get_type (void);

G_DEFINE_TYPE (FooOldShaderEffect,
               foo_old_shader_effect,
               CLUTTER_TYPE_SHADER_EFFECT);

static void
foo_old_shader_effect_paint_target (ClutterOffscreenEffect *effect)
{
  clutter_shader_effect_set_shader_source (CLUTTER_SHADER_EFFECT (effect),
                                           old_shader_effect_source);
  clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect),
                                     "override_color",
                                     G_TYPE_FLOAT, 3,
                                     1.0f, 0.0f, 0.0f);

  CLUTTER_OFFSCREEN_EFFECT_CLASS (foo_old_shader_effect_parent_class)->
    paint_target (effect);
}

static void
foo_old_shader_effect_class_init (FooOldShaderEffectClass *klass)
{
  ClutterOffscreenEffectClass *offscreen_effect_class =
    CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);

  offscreen_effect_class->paint_target = foo_old_shader_effect_paint_target;
}

static void
foo_old_shader_effect_init (FooOldShaderEffect *self)
{
}

/****************************************************************
 New style shader effect
 This overrides get_static_shader_source()
 ****************************************************************/

static const gchar
new_shader_effect_source[] =
  "uniform vec3 override_color;\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = (vec4 (override_color, 1.0) +\n"
  "                    vec4 (0.0, 0.0, 1.0, 0.0));\n"
  "}";

typedef struct _FooNewShaderEffectClass
{
  ClutterShaderEffectClass parent_class;
} FooNewShaderEffectClass;

typedef struct _FooNewShaderEffect
{
  ClutterShaderEffect parent;
} FooNewShaderEffect;

GType foo_new_shader_effect_get_type (void);

G_DEFINE_TYPE (FooNewShaderEffect,
               foo_new_shader_effect,
               CLUTTER_TYPE_SHADER_EFFECT);

static gchar *
foo_new_shader_effect_get_static_source (ClutterShaderEffect *effect)
{
  static gboolean already_called = FALSE;

  /* This should only be called once even though we have two actors
     using this effect */
  g_assert (!already_called);

  already_called = TRUE;

  return g_strdup (new_shader_effect_source);
}

static void
foo_new_shader_effect_paint_target (ClutterOffscreenEffect *effect)
{
  clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect),
                                     "override_color",
                                     G_TYPE_FLOAT, 3,
                                     0.0f, 1.0f, 0.0f);

  CLUTTER_OFFSCREEN_EFFECT_CLASS (foo_new_shader_effect_parent_class)->
    paint_target (effect);
}

static void
foo_new_shader_effect_class_init (FooNewShaderEffectClass *klass)
{
  ClutterOffscreenEffectClass *offscreen_effect_class =
    CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  ClutterShaderEffectClass *shader_effect_class =
    CLUTTER_SHADER_EFFECT_CLASS (klass);

  offscreen_effect_class->paint_target = foo_new_shader_effect_paint_target;

  shader_effect_class->get_static_shader_source =
    foo_new_shader_effect_get_static_source;
}

static void
foo_new_shader_effect_init (FooNewShaderEffect *self)
{
}

/****************************************************************
 Another new style shader effect
 This is the same but with a different shader. This is just
 sanity check that each class gets its own copy of the private
 data
 ****************************************************************/

static const gchar
another_new_shader_effect_source[] =
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  cogl_color_out = vec4 (1.0, 0.0, 1.0, 1.0);\n"
  "}";

typedef struct _FooAnotherNewShaderEffectClass
{
  ClutterShaderEffectClass parent_class;
} FooAnotherNewShaderEffectClass;

typedef struct _FooAnotherNewShaderEffect
{
  ClutterShaderEffect parent;
} FooAnotherNewShaderEffect;

GType foo_another_new_shader_effect_get_type (void);

G_DEFINE_TYPE (FooAnotherNewShaderEffect,
               foo_another_new_shader_effect,
               CLUTTER_TYPE_SHADER_EFFECT);

static gchar *
foo_another_new_shader_effect_get_static_source (ClutterShaderEffect *effect)
{
  return g_strdup (another_new_shader_effect_source);
}

static void
foo_another_new_shader_effect_class_init (FooAnotherNewShaderEffectClass *klass)
{
  ClutterShaderEffectClass *shader_effect_class =
    CLUTTER_SHADER_EFFECT_CLASS (klass);

  shader_effect_class->get_static_shader_source =
    foo_another_new_shader_effect_get_static_source;
}

static void
foo_another_new_shader_effect_init (FooAnotherNewShaderEffect *self)
{
}

/****************************************************************/

static ClutterActor *
make_actor (GType shader_type)
{
  ClutterActor *rect;
  const ClutterColor white = { 0xff, 0xff, 0xff, 0xff };

  rect = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &white);
  clutter_actor_set_size (rect, 50, 50);

  clutter_actor_add_effect (rect, g_object_new (shader_type, NULL));

  return rect;
}

static guint32
get_pixel (int x, int y)
{
  guint8 data[4];

  cogl_read_pixels (x, y, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    data);

  return (((guint32) data[0] << 16) |
          ((guint32) data[1] << 8) |
          data[2]);
}

static void
paint_cb (ClutterStage *stage,
          gpointer      data)
{
  gboolean *was_painted = data;

  /* old shader effect */
  g_assert_cmpint (get_pixel (50, 50), ==, 0xff0000);
  /* new shader effect */
  g_assert_cmpint (get_pixel (150, 50), ==, 0x00ffff);
  /* another new shader effect */
  g_assert_cmpint (get_pixel (250, 50), ==, 0xff00ff);
  /* new shader effect */
  g_assert_cmpint (get_pixel (350, 50), ==, 0x00ffff);

  *was_painted = TRUE;
}

static void
actor_shader_effect (void)
{
  ClutterActor *stage;
  ClutterActor *rect;
  gboolean was_painted;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    return;

  stage = clutter_stage_new ();

  rect = make_actor (foo_old_shader_effect_get_type ());
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  rect = make_actor (foo_new_shader_effect_get_type ());
  clutter_actor_set_x (rect, 100);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  rect = make_actor (foo_another_new_shader_effect_get_type ());
  clutter_actor_set_x (rect, 200);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  rect = make_actor (foo_new_shader_effect_get_type ());
  clutter_actor_set_x (rect, 300);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);

  clutter_actor_show (stage);

  was_painted = FALSE;
  g_signal_connect (stage, "after-paint",
                    G_CALLBACK (paint_cb), NULL);

  while (!was_painted)
    g_main_context_iteration (NULL, FALSE);
}

CLUTTER_TEST_SUITE (
  CLUTTER_TEST_UNIT ("/actor/shader-effect", actor_shader_effect)
)
