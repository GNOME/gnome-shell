#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#include "test-conform-common.h"

#define TEST_TYPE_GROUP                 (test_group_get_type ())
#define TEST_TYPE_GROUP_META            (test_group_meta_get_type ())

#define TEST_GROUP(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_GROUP, TestGroup))
#define TEST_IS_GROUP(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_GROUP))

#define TEST_GROUP_META(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_GROUP_META, TestGroupMeta))
#define TEST_IS_GROUP_META(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_GROUP_META))

typedef struct _ClutterGroup            TestGroup;

typedef struct _TestGroupMeta {
  ClutterChildMeta parent_instance;

  guint is_focus : 1;
} TestGroupMeta;

typedef struct _ClutterGroupClass       TestGroupClass;
typedef struct _ClutterChildMetaClass   TestGroupMetaClass;

G_DEFINE_TYPE (TestGroupMeta, test_group_meta, CLUTTER_TYPE_CHILD_META);

enum
{
  PROP_META_0,

  PROP_META_FOCUS
};

static void
test_group_meta_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  TestGroupMeta *self = TEST_GROUP_META (gobject);

  switch (prop_id)
    {
    case PROP_META_FOCUS:
      self->is_focus = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
test_group_meta_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  TestGroupMeta *self = TEST_GROUP_META (gobject);

  switch (prop_id)
    {
    case PROP_META_FOCUS:
      g_value_set_boolean (value, self->is_focus);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
test_group_meta_class_init (TestGroupMetaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = test_group_meta_set_property;
  gobject_class->get_property = test_group_meta_get_property;

  pspec = g_param_spec_boolean ("focus", "Focus", "Focus",
                                FALSE,
                                G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_META_FOCUS, pspec);
}

static void
test_group_meta_init (TestGroupMeta *meta)
{
  meta->is_focus = FALSE;
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->child_meta_type = TEST_TYPE_GROUP_META;
}

G_DEFINE_TYPE_WITH_CODE (TestGroup, test_group, CLUTTER_TYPE_GROUP,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

static void
test_group_class_init (TestGroupClass *klass)
{
}

static void
test_group_init (TestGroup *self)
{
}

void
test_script_child (TestConformSimpleFixture *fixture,
                   gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  GObject *container, *actor;
  GError *error = NULL;
  gboolean focus_ret;
  gchar *test_file;

  test_file = clutter_test_get_data_file ("test-script-child.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

  container = actor = NULL;
  clutter_script_get_objects (script,
                              "test-group", &container,
                              "test-rect-1", &actor,
                              NULL);
  g_assert (TEST_IS_GROUP (container));
  g_assert (CLUTTER_IS_RECTANGLE (actor));

  focus_ret = FALSE;
  clutter_container_child_get (CLUTTER_CONTAINER (container),
                               CLUTTER_ACTOR (actor),
                               "focus", &focus_ret,
                               NULL);
  g_assert (focus_ret);

  actor = clutter_script_get_object (script, "test-rect-2");
  g_assert (CLUTTER_IS_RECTANGLE (actor));

  focus_ret = FALSE;
  clutter_container_child_get (CLUTTER_CONTAINER (container),
                               CLUTTER_ACTOR (actor),
                               "focus", &focus_ret,
                               NULL);
  g_assert (!focus_ret);

  g_object_unref (script);
  clutter_actor_destroy (CLUTTER_ACTOR (container));
  g_free (test_file);
}

void
test_script_single (TestConformSimpleFixture *fixture,
                    gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  ClutterColor color = { 0, };
  GObject *actor = NULL;
  GError *error = NULL;
  ClutterActor *rect;
  gchar *test_file;

  test_file = clutter_test_get_data_file ("test-script-single.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

  actor = clutter_script_get_object (script, "test");
  g_assert (CLUTTER_IS_RECTANGLE (actor));

  rect = CLUTTER_ACTOR (actor);
  g_assert_cmpfloat (clutter_actor_get_width (rect), ==, 50.0);
  g_assert_cmpfloat (clutter_actor_get_y (rect), ==, 100.0);

  clutter_rectangle_get_color (CLUTTER_RECTANGLE (rect), &color);
  g_assert_cmpint (color.red, ==, 255);
  g_assert_cmpint (color.green, ==, 0xcc);
  g_assert_cmpint (color.alpha, ==, 0xff);

  g_object_unref (script);

  clutter_actor_destroy (rect);
  g_free (test_file);
}

void
test_script_implicit_alpha (TestConformSimpleFixture *fixture,
                            gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  ClutterTimeline *timeline;
  GObject *behaviour = NULL;
  GError *error = NULL;
  ClutterAlpha *alpha;
  gchar *test_file;

  test_file = clutter_test_get_data_file ("test-script-implicit-alpha.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

  behaviour = clutter_script_get_object (script, "test");
  g_assert (CLUTTER_IS_BEHAVIOUR (behaviour));

  alpha = clutter_behaviour_get_alpha (CLUTTER_BEHAVIOUR (behaviour));
  g_assert (CLUTTER_IS_ALPHA (alpha));

  g_assert_cmpint (clutter_alpha_get_mode (alpha), ==, CLUTTER_EASE_OUT_CIRC);

  timeline = clutter_alpha_get_timeline (alpha);
  g_assert (CLUTTER_IS_TIMELINE (timeline));

  g_assert_cmpint (clutter_timeline_get_duration (timeline), ==, 500);

  g_object_unref (script);
  g_free (test_file);
}

void
test_script_object_property (TestConformSimpleFixture *fixture,
                             gconstpointer dummy)
{
  ClutterScript *script = clutter_script_new ();
  ClutterLayoutManager *manager;
  GObject *actor = NULL;
  GError *error = NULL;
  gchar *test_file;

  test_file = clutter_test_get_data_file ("test-script-object-property.json");
  clutter_script_load_from_file (script, test_file, &error);
  if (g_test_verbose () && error)
    g_print ("Error: %s", error->message);
  g_assert (error == NULL);

  actor = clutter_script_get_object (script, "test");
  g_assert (CLUTTER_IS_BOX (actor));

  manager = clutter_box_get_layout_manager (CLUTTER_BOX (actor));
  g_assert (CLUTTER_IS_BIN_LAYOUT (manager));

  g_object_unref (script);
  clutter_actor_destroy (CLUTTER_ACTOR (actor));
  g_free (test_file);
}
