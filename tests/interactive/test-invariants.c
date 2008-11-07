#include <stdlib.h>
#include <string.h>

#include <gmodule.h>
#include <clutter/clutter.h>

/* dummy unit testing API; to be replaced by GTest in 1.0 */
typedef void (* test_func) (void);

typedef struct _TestUnit        TestUnit;

struct _TestUnit
{
  gchar *name;
  test_func func;
};

static GSList *units = NULL;

static void
test_init (gint    *argc,
           gchar ***argv)
{
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  g_assert (clutter_init (argc, argv) == CLUTTER_INIT_SUCCESS);
}

static void
test_add_func (const gchar *name,
               test_func    func)
{
  TestUnit *unit;

  unit = g_slice_new (TestUnit);
  unit->name = g_strdup (name);
  unit->func = func;

  units = g_slist_prepend (units, unit);
}

static int
test_run (void)
{
  GSList *l;

  units = g_slist_reverse (units);

  for (l = units; l != NULL; l = l->next)
    {
      TestUnit *u = l->data;
      GString *test_name = g_string_sized_new (75);
      gsize len, i;

      g_string_append (test_name, "Testing: ");
      g_string_append (test_name, u->name);
      len = 75 - test_name->len;

      for (i = 0; i < len; i++)
        g_string_append_c (test_name, '.');

      g_print ("%s", test_name->str);

      u->func ();

      g_print ("OK\n");
    }

  for (l = units; l != NULL; l = l->next)
    {
      TestUnit *u = l->data;

      g_free (u->name);
      g_slice_free (TestUnit, u);
    }

  g_slist_free (units);

  return EXIT_SUCCESS;
}

/* test units */
static void
test_initial_state (void)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (actor);
}

static void
test_realized (void)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));

  clutter_actor_realize (actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));

  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));

  clutter_actor_destroy (actor);
}


static void
test_mapped (void)
{
  ClutterActor *actor;

  actor = clutter_rectangle_new ();

  g_assert (!(CLUTTER_ACTOR_IS_REALIZED (actor)));
  g_assert (!(CLUTTER_ACTOR_IS_MAPPED (actor)));

  clutter_actor_show (actor);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (CLUTTER_ACTOR_IS_MAPPED (actor));

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));

  clutter_actor_destroy (actor);
}

static void
test_show_on_set_parent (void)
{
  ClutterActor *actor, *group;
  gboolean show_on_set_parent;

  group = clutter_group_new ();

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (group)));

  actor = clutter_rectangle_new ();
  g_object_get (G_OBJECT (actor),
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));
  g_assert (show_on_set_parent == TRUE);

  clutter_group_add (group, actor);
  g_object_get (G_OBJECT (actor),
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (CLUTTER_ACTOR_IS_VISIBLE (actor));
  g_assert (show_on_set_parent == TRUE);

  g_object_ref (actor);
  clutter_actor_unparent (actor);
  g_object_get (G_OBJECT (actor),
                "show-on-set-parent", &show_on_set_parent,
                NULL);

  g_assert (CLUTTER_ACTOR_IS_REALIZED (actor));
  g_assert (!(CLUTTER_ACTOR_IS_VISIBLE (actor)));
  g_assert (show_on_set_parent == TRUE);

  clutter_actor_destroy (actor);
  clutter_actor_destroy (group);
}

G_MODULE_EXPORT int
test_invariants_main (int   argc, char *argv[])
{
  test_init (&argc, &argv);

  test_add_func ("/invariants/initial-state", test_initial_state);
  test_add_func ("/invariants/realized", test_realized);
  test_add_func ("/invariants/mapped", test_mapped);
  test_add_func ("/invariants/show-on-set-parent", test_show_on_set_parent);

  return test_run ();
}
