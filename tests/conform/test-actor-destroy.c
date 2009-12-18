#include <clutter/clutter.h>
#include "test-conform-common.h"

#define TEST_TYPE_DESTROY               (test_destroy_get_type ())
#define TEST_DESTROY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_DESTROY, TestDestroy))
#define TEST_IS_DESTROY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_DESTROY))

typedef struct _TestDestroy             TestDestroy;
typedef struct _TestDestroyClass        TestDestroyClass;

struct _TestDestroy
{
  ClutterActor parent_instance;

  ClutterActor *bg;
  ClutterActor *label;
  ClutterActor *tex;

  GList *children;
};

struct _TestDestroyClass
{
  ClutterActorClass parent_class;
};

static void clutter_container_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (TestDestroy, test_destroy, CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_init));

static void
test_destroy_add (ClutterContainer *container,
                  ClutterActor *actor)
{
  TestDestroy *self = TEST_DESTROY (container);

  if (g_test_verbose ())
    g_print ("Adding '%s' (type:%s)\n",
             clutter_actor_get_name (actor),
             G_OBJECT_TYPE_NAME (actor));

  self->children = g_list_prepend (self->children, actor);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));
}

static void
test_destroy_remove (ClutterContainer *container,
                     ClutterActor *actor)
{
  TestDestroy *self = TEST_DESTROY (container);

  if (g_test_verbose ())
    g_print ("Removing '%s' (type:%s)\n",
             clutter_actor_get_name (actor),
             G_OBJECT_TYPE_NAME (actor));

  g_assert (actor != self->bg);
  g_assert (actor != self->label);

  if (!g_list_find (self->children, actor))
    g_assert (actor == self->tex);
  else
    self->children = g_list_remove (self->children, actor);

  clutter_actor_unparent (actor);
}

static void
clutter_container_init (ClutterContainerIface *iface)
{
  iface->add = test_destroy_add;
  iface->remove = test_destroy_remove;
}

static void
test_destroy_destroy (ClutterActor *self)
{
  TestDestroy *test = TEST_DESTROY (self);

  if (test->bg != NULL)
    {
      if (g_test_verbose ())
        g_print ("Destroying '%s' (type:%s)\n",
                 clutter_actor_get_name (test->bg),
                 G_OBJECT_TYPE_NAME (test->bg));

      clutter_actor_destroy (test->bg);
      test->bg = NULL;
    }

  if (test->label != NULL)
    {
      if (g_test_verbose ())
        g_print ("Destroying '%s' (type:%s)\n",
                 clutter_actor_get_name (test->label),
                 G_OBJECT_TYPE_NAME (test->label));

      clutter_actor_destroy (test->label);
      test->label = NULL;
    }

  if (test->tex != NULL)
    {
      if (g_test_verbose ())
        g_print ("Destroying '%s' (type:%s)\n",
                 clutter_actor_get_name (test->tex),
                 G_OBJECT_TYPE_NAME (test->tex));

      clutter_actor_destroy (test->tex);
      test->tex = NULL;
    }

  g_list_foreach (test->children, (GFunc) clutter_actor_destroy, NULL);
  g_list_free (test->children);
  test->children = NULL;

  if (CLUTTER_ACTOR_CLASS (test_destroy_parent_class)->destroy)
    CLUTTER_ACTOR_CLASS (test_destroy_parent_class)->destroy (self);
}

static void
test_destroy_class_init (TestDestroyClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->destroy = test_destroy_destroy;
}

static void
test_destroy_init (TestDestroy *self)
{
  clutter_actor_push_internal ();

  if (g_test_verbose ())
    g_print ("Adding internal children...\n");

  self->bg = clutter_rectangle_new ();
  clutter_actor_set_parent (self->bg, CLUTTER_ACTOR (self));
  clutter_actor_set_name (self->bg, "Background");

  self->label = clutter_text_new ();
  clutter_actor_set_parent (self->label, CLUTTER_ACTOR (self));
  clutter_actor_set_name (self->label, "Label");

  clutter_actor_pop_internal ();

  self->tex = clutter_texture_new ();
  clutter_actor_set_parent (self->tex, CLUTTER_ACTOR (self));
  clutter_actor_set_name (self->tex, "Texture");
}

void
test_actor_destruction (TestConformSimpleFixture *fixture,
                        gconstpointer dummy)
{
  ClutterActor *test = g_object_new (TEST_TYPE_DESTROY, NULL);
  ClutterActor *child = clutter_rectangle_new ();

  if (g_test_verbose ())
    g_print ("Adding external child...\n");

  clutter_actor_set_name (child, "Child");
  clutter_container_add_actor (CLUTTER_CONTAINER (test), child);

  clutter_actor_destroy (test);
}
