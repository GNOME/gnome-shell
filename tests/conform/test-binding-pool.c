#include <string.h>

#include <glib.h>

#include <clutter/clutter.h>
#include <clutter/clutter-keysyms.h>

#include "test-conform-common.h"

#define TYPE_KEY_GROUP                  (key_group_get_type ())
#define KEY_GROUP(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_KEY_GROUP, KeyGroup))
#define IS_KEY_GROUP(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_KEY_GROUP))
#define KEY_GROUP_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_KEY_GROUP, KeyGroupClass))
#define IS_KEY_GROUP_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_KEY_GROUP))

typedef struct _KeyGroup        KeyGroup;
typedef struct _KeyGroupClass   KeyGroupClass;

struct _KeyGroup
{
  ClutterGroup parent_instance;

  gint selected_index;
};

struct _KeyGroupClass
{
  ClutterGroupClass parent_class;

  void (* activate) (KeyGroup     *group,
                     ClutterActor *child);
};

G_DEFINE_TYPE (KeyGroup, key_group, CLUTTER_TYPE_GROUP);

enum
{
  ACTIVATE,

  LAST_SIGNAL
};

static guint group_signals[LAST_SIGNAL] = { 0, };

static gboolean
key_group_action_move_left (KeyGroup            *self,
                            const gchar         *action_name,
                            guint                key_val,
                            ClutterModifierType  modifiers)
{
  gint n_children;

  g_assert_cmpstr (action_name, ==, "move-left");
  g_assert_cmpint (key_val, ==, CLUTTER_Left);

  n_children = clutter_group_get_n_children (CLUTTER_GROUP (self));

  self->selected_index -= 1;

  if (self->selected_index < 0)
    self->selected_index = n_children - 1;

  return TRUE;
}

static gboolean
key_group_action_move_right (KeyGroup            *self,
                             const gchar         *action_name,
                             guint                key_val,
                             ClutterModifierType  modifiers)
{
  gint n_children;

  g_assert_cmpstr (action_name, ==, "move-right");
  g_assert_cmpint (key_val, ==, CLUTTER_Right);

  n_children = clutter_group_get_n_children (CLUTTER_GROUP (self));

  self->selected_index += 1;

  if (self->selected_index >= n_children)
    self->selected_index = 0;

  return TRUE;
}

static gboolean
key_group_action_activate (KeyGroup            *self,
                           const gchar         *action_name,
                           guint                key_val,
                           ClutterModifierType  modifiers)
{
  ClutterActor *child = NULL;

  g_assert_cmpstr (action_name, ==, "activate");
  g_assert (key_val == CLUTTER_Return ||
            key_val == CLUTTER_KP_Enter ||
            key_val == CLUTTER_ISO_Enter);

  if (self->selected_index == -1)
    return FALSE;

  child = clutter_group_get_nth_child (CLUTTER_GROUP (self),
                                       self->selected_index);

  if (child)
    {
      g_signal_emit (self, group_signals[ACTIVATE], 0, child);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
key_group_key_press (ClutterActor    *actor,
                     ClutterKeyEvent *event)
{
  ClutterBindingPool *pool;
  gboolean res;

  pool = clutter_binding_pool_find (G_OBJECT_TYPE_NAME (actor));
  g_assert (pool != NULL);

  res = clutter_binding_pool_activate (pool,
                                       event->keyval,
                                       event->modifier_state,
                                       G_OBJECT (actor));

  /* if we activate a key binding, redraw the actor */
  if (res)
    clutter_actor_queue_redraw (actor);

  return res;
}

static void
key_group_paint (ClutterActor *actor)
{
  KeyGroup *self = KEY_GROUP (actor);
  GList *children, *l;
  gint i;

  children = clutter_container_get_children (CLUTTER_CONTAINER (self));

  for (l = children, i = 0; l != NULL; l = l->next, i++)
    {
      ClutterActor *child = l->data;

      /* paint the selection rectangle */
      if (i == self->selected_index)
        {
          ClutterActorBox box = { 0, };

          clutter_actor_get_allocation_box (child, &box);

          box.x1 -= CLUTTER_UNITS_FROM_DEVICE (2);
          box.y1 -= CLUTTER_UNITS_FROM_DEVICE (2);
          box.x2 += CLUTTER_UNITS_FROM_DEVICE (2);
          box.y2 += CLUTTER_UNITS_FROM_DEVICE (2);

          cogl_set_source_color4ub (255, 255, 0, 224);
          cogl_rectangle (CLUTTER_UNITS_TO_DEVICE (box.x1),
                          CLUTTER_UNITS_TO_DEVICE (box.y1),
                          CLUTTER_UNITS_TO_DEVICE (box.x2),
                          CLUTTER_UNITS_TO_DEVICE (box.y2));
        }

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }

  g_list_free (children);
}

static void
key_group_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (key_group_parent_class)->finalize (gobject);
}

static void
key_group_class_init (KeyGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterBindingPool *binding_pool;

  gobject_class->finalize = key_group_finalize;

  actor_class->paint = key_group_paint;
  actor_class->key_press_event = key_group_key_press;

  group_signals[ACTIVATE] =
    g_signal_new (g_intern_static_string ("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (KeyGroupClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  binding_pool = clutter_binding_pool_get_for_class (klass);

  clutter_binding_pool_install_action (binding_pool, "move-right",
                                       CLUTTER_Right, 0,
                                       G_CALLBACK (key_group_action_move_right),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "move-left",
                                       CLUTTER_Left, 0,
                                       G_CALLBACK (key_group_action_move_left),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_Return, 0,
                                       G_CALLBACK (key_group_action_activate),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_KP_Enter, 0,
                                       G_CALLBACK (key_group_action_activate),
                                       NULL, NULL);
  clutter_binding_pool_install_action (binding_pool, "activate",
                                       CLUTTER_ISO_Enter, 0,
                                       G_CALLBACK (key_group_action_activate),
                                       NULL, NULL);
}

static void
key_group_init (KeyGroup *self)
{
  self->selected_index = -1;
}

static void
init_event (ClutterKeyEvent *event)
{
  event->type = CLUTTER_KEY_PRESS;
  event->time = 0;      /* not needed */
  event->flags = CLUTTER_EVENT_FLAG_SYNTHETIC;
  event->stage = NULL;  /* not needed */
  event->source = NULL; /* not needed */
  event->modifier_state = 0;
  event->hardware_keycode = 0; /* not needed */
}

static void
send_keyval (KeyGroup *group, int keyval)
{
  ClutterKeyEvent event;

  init_event (&event);
  event.keyval = keyval;
  event.unicode_value = 0; /* should be ignored for cursor keys etc. */

  clutter_actor_event (CLUTTER_ACTOR (group), (ClutterEvent *) &event, FALSE);
}

static void
on_activate (KeyGroup     *key_group,
             ClutterActor *child,
             gpointer      data)
{
  gint _index = GPOINTER_TO_INT (data);

  g_assert_cmpint (key_group->selected_index, ==, _index);
}

void
test_binding_pool (TestConformSimpleFixture *fixture,
                   gconstpointer             data)
{
  KeyGroup *key_group = g_object_new (TYPE_KEY_GROUP, NULL);

  clutter_container_add (CLUTTER_CONTAINER (key_group),
                         g_object_new (CLUTTER_TYPE_RECTANGLE,
                                       "width", 50,
                                       "height", 50,
                                       "x", 0, "y", 0,
                                       NULL),
                         g_object_new (CLUTTER_TYPE_RECTANGLE,
                                       "width", 50,
                                       "height", 50,
                                       "x", 75, "y", 0,
                                       NULL),
                         g_object_new (CLUTTER_TYPE_RECTANGLE,
                                       "width", 50,
                                       "height", 50,
                                       "x", 150, "y", 0,
                                       NULL),
                         NULL);

  g_assert_cmpint (key_group->selected_index, ==, -1);

  send_keyval (key_group, CLUTTER_Left);
  g_assert_cmpint (key_group->selected_index, ==, 2);

  send_keyval (key_group, CLUTTER_Left);
  g_assert_cmpint (key_group->selected_index, ==, 1);

  send_keyval (key_group, CLUTTER_Right);
  g_assert_cmpint (key_group->selected_index, ==, 2);

  send_keyval (key_group, CLUTTER_Right);
  g_assert_cmpint (key_group->selected_index, ==, 0);

  g_signal_connect (key_group,
                    "activate", G_CALLBACK (on_activate),
                    GINT_TO_POINTER (0));

  send_keyval (key_group, CLUTTER_Return);

  clutter_actor_destroy (CLUTTER_ACTOR (key_group));
}
