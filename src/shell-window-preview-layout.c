#include "config.h"

#include <clutter/clutter.h>
#include <meta/window.h>
#include "shell-window-preview-layout.h"

typedef struct _ShellWindowPreviewLayoutPrivate ShellWindowPreviewLayoutPrivate;
struct _ShellWindowPreviewLayoutPrivate
{
  ClutterActor *container;
  GHashTable *windows;

  ClutterActorBox bounding_box;
};

enum
{
  PROP_0,

  PROP_BOUNDING_BOX,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (ShellWindowPreviewLayout, shell_window_preview_layout,
                            CLUTTER_TYPE_LAYOUT_MANAGER);

typedef struct _WindowInfo
{
  MetaWindow *window;
  ClutterActor *window_actor;

  gulong size_changed_id;
  gulong position_changed_id;
  gulong window_actor_destroy_id;
  gulong destroy_id;
} WindowInfo;

static void
shell_window_preview_layout_get_property (GObject      *object,
                                          unsigned int  property_id,
                                          GValue       *value,
                                          GParamSpec   *pspec)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (object);
  ShellWindowPreviewLayoutPrivate *priv;

  priv = shell_window_preview_layout_get_instance_private (self);

  switch (property_id)
    {
    case PROP_BOUNDING_BOX:
      g_value_set_boxed (value, &priv->bounding_box);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
shell_window_preview_layout_set_container (ClutterLayoutManager *layout,
                                           ClutterContainer     *container)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (layout);
  ShellWindowPreviewLayoutPrivate *priv;
  ClutterLayoutManagerClass *parent_class;

  priv = shell_window_preview_layout_get_instance_private (self);

  priv->container = CLUTTER_ACTOR (container);

  parent_class = CLUTTER_LAYOUT_MANAGER_CLASS (shell_window_preview_layout_parent_class);
  parent_class->set_container (layout, container);
}

static void
shell_window_preview_layout_get_preferred_width (ClutterLayoutManager *layout,
                                                 ClutterContainer     *container,
                                                 float                 for_height,
                                                 float                *min_width_p,
                                                 float                *natural_width_p)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (layout);
  ShellWindowPreviewLayoutPrivate *priv;

  priv = shell_window_preview_layout_get_instance_private (self);

  if (min_width_p)
    *min_width_p = 0;

  if (natural_width_p)
    *natural_width_p = clutter_actor_box_get_width (&priv->bounding_box);
}

static void
shell_window_preview_layout_get_preferred_height (ClutterLayoutManager *layout,
                                                  ClutterContainer     *container,
                                                  float                 for_width,
                                                  float                *min_height_p,
                                                  float                *natural_height_p)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (layout);
  ShellWindowPreviewLayoutPrivate *priv;

  priv = shell_window_preview_layout_get_instance_private (self);

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    *natural_height_p = clutter_actor_box_get_height (&priv->bounding_box);
}


static void
shell_window_preview_layout_allocate (ClutterLayoutManager   *layout,
                                      ClutterContainer       *container,
                                      const ClutterActorBox  *box)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (layout);
  ShellWindowPreviewLayoutPrivate *priv;
  float scale_x, scale_y;
  float bounding_box_width, bounding_box_height;
  ClutterActorIter iter;
  ClutterActor *child;

  priv = shell_window_preview_layout_get_instance_private (self);

  bounding_box_width = clutter_actor_box_get_width (&priv->bounding_box);
  bounding_box_height = clutter_actor_box_get_height (&priv->bounding_box);

  if (bounding_box_width == 0)
    scale_x = 1.f;
  else
    scale_x = clutter_actor_box_get_width (box) / bounding_box_width;

  if (bounding_box_height == 0)
    scale_y = 1.f;
  else
    scale_y = clutter_actor_box_get_height (box) / bounding_box_height;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (container));
  while (clutter_actor_iter_next (&iter, &child))
    {
      ClutterActorBox child_box = { 0, };
      WindowInfo *window_info;

      if (!clutter_actor_is_visible (child))
        continue;

      window_info = g_hash_table_lookup (priv->windows, child);

      if (window_info)
        {
          MetaRectangle buffer_rect;
          float child_nat_width, child_nat_height;

          meta_window_get_buffer_rect (window_info->window, &buffer_rect);

          clutter_actor_box_set_origin (&child_box,
                                        buffer_rect.x - priv->bounding_box.x1,
                                        buffer_rect.y - priv->bounding_box.y1);

          clutter_actor_get_preferred_size (child, NULL, NULL,
                                            &child_nat_width, &child_nat_height);

          clutter_actor_box_set_size (&child_box, child_nat_width, child_nat_height);

          child_box.x1 *= scale_x;
          child_box.x2 *= scale_x;
          child_box.y1 *= scale_y;
          child_box.y2 *= scale_y;

          clutter_actor_allocate (child, &child_box);
        }
      else
        {
          float x, y;

          clutter_actor_get_fixed_position (child, &x, &y);
          clutter_actor_allocate_preferred_size (child, x, y);
        }
    }
}

static void
on_layout_changed (ShellWindowPreviewLayout *self)
{
  ShellWindowPreviewLayoutPrivate *priv;
  GHashTableIter iter;
  gpointer value;
  gboolean first_rect = TRUE;
  MetaRectangle bounding_rect = { 0, };
  ClutterActorBox old_bounding_box;

  priv = shell_window_preview_layout_get_instance_private (self);

  old_bounding_box =
    (ClutterActorBox) CLUTTER_ACTOR_BOX_INIT (priv->bounding_box.x1,
                                              priv->bounding_box.y1,
                                              priv->bounding_box.x2,
                                              priv->bounding_box.y2);

  g_hash_table_iter_init (&iter, priv->windows);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      WindowInfo *window_info = value;
      MetaRectangle frame_rect;

      meta_window_get_frame_rect (window_info->window, &frame_rect);

      if (first_rect)
        {
          bounding_rect = frame_rect;
          first_rect = FALSE;
          continue;
        }

      meta_rectangle_union (&frame_rect, &bounding_rect, &bounding_rect);
    }

  clutter_actor_box_set_origin (&priv->bounding_box,
                                (float) bounding_rect.x,
                                (float) bounding_rect.y);
  clutter_actor_box_set_size (&priv->bounding_box,
                              (float) bounding_rect.width,
                              (float) bounding_rect.height);

  if (!clutter_actor_box_equal (&priv->bounding_box, &old_bounding_box))
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_BOUNDING_BOX]);

  clutter_layout_manager_layout_changed (CLUTTER_LAYOUT_MANAGER (self));
}

static void
on_window_size_position_changed (MetaWindow               *window,
                                 ShellWindowPreviewLayout *self)
{
  on_layout_changed (self);
}

static void
on_window_destroyed (ClutterActor *actor)
{
  clutter_actor_destroy (actor);
}

static void
on_actor_destroyed (ClutterActor *actor,
                    ShellWindowPreviewLayout *self)
{
  ShellWindowPreviewLayoutPrivate *priv;
  WindowInfo *window_info;

  priv = shell_window_preview_layout_get_instance_private (self);

  window_info = g_hash_table_lookup (priv->windows, actor);
  g_assert (window_info != NULL);

  shell_window_preview_layout_remove_window (self, window_info->window);
}

static void
shell_window_preview_layout_dispose (GObject *gobject)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (gobject);
  ShellWindowPreviewLayoutPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;

  priv = shell_window_preview_layout_get_instance_private (self);

  g_hash_table_iter_init (&iter, priv->windows);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ClutterActor *actor = key;
      WindowInfo *info = value;

      g_clear_signal_handler (&info->size_changed_id, info->window);
      g_clear_signal_handler (&info->position_changed_id, info->window);
      g_clear_signal_handler (&info->window_actor_destroy_id, info->window_actor);
      g_clear_signal_handler (&info->destroy_id, actor);

      clutter_actor_remove_child (priv->container, actor);
    }

  g_hash_table_remove_all (priv->windows);

  G_OBJECT_CLASS (shell_window_preview_layout_parent_class)->dispose (gobject);
}

static void
shell_window_preview_layout_finalize (GObject *gobject)
{
  ShellWindowPreviewLayout *self = SHELL_WINDOW_PREVIEW_LAYOUT (gobject);
  ShellWindowPreviewLayoutPrivate *priv;

  priv = shell_window_preview_layout_get_instance_private (self);

  g_hash_table_destroy (priv->windows);

  G_OBJECT_CLASS (shell_window_preview_layout_parent_class)->finalize (gobject);
}

static void
shell_window_preview_layout_init (ShellWindowPreviewLayout *self)
{
  ShellWindowPreviewLayoutPrivate *priv;

  priv = shell_window_preview_layout_get_instance_private (self);

  priv->windows = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) g_free);
}

static void
shell_window_preview_layout_class_init (ShellWindowPreviewLayoutClass *klass)
{
  ClutterLayoutManagerClass *layout_class = CLUTTER_LAYOUT_MANAGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  layout_class->get_preferred_width = shell_window_preview_layout_get_preferred_width;
  layout_class->get_preferred_height = shell_window_preview_layout_get_preferred_height;
  layout_class->allocate = shell_window_preview_layout_allocate;
  layout_class->set_container = shell_window_preview_layout_set_container;

  gobject_class->dispose = shell_window_preview_layout_dispose;
  gobject_class->finalize = shell_window_preview_layout_finalize;
  gobject_class->get_property = shell_window_preview_layout_get_property;

  /**
   * ShellWindowPreviewLayout:bounding-box:
   */
  obj_props[PROP_BOUNDING_BOX] =
    g_param_spec_boxed ("bounding-box",
                        "Bounding Box",
                        "Bounding Box",
                        CLUTTER_TYPE_ACTOR_BOX,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

/**
 * shell_window_preview_layout_add_window:
 * @self: a #ShellWindowPreviewLayout
 * @window: the #MetaWindow
 *
 * Creates a ClutterActor drawing the texture of @window and adds it
 * to the container. If @window is already part of the preview, this
 * function will do nothing.
 *
 * Returns: (nullable) (transfer none): The newly created actor drawing @window
 */
ClutterActor *
shell_window_preview_layout_add_window (ShellWindowPreviewLayout *self,
                                        MetaWindow               *window)
{
  ShellWindowPreviewLayoutPrivate *priv;
  ClutterActor *window_actor, *actor;
  WindowInfo *window_info;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (SHELL_IS_WINDOW_PREVIEW_LAYOUT (self), NULL);
  g_return_val_if_fail (META_IS_WINDOW (window), NULL);

  priv = shell_window_preview_layout_get_instance_private (self);

  g_hash_table_iter_init (&iter, priv->windows);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      WindowInfo *info = value;

      if (info->window == window)
        return NULL;
    }

  window_actor = CLUTTER_ACTOR (meta_window_get_compositor_private (window));
  actor = clutter_clone_new (window_actor);

  window_info = g_new0 (WindowInfo, 1);

  window_info->window = window;
  window_info->window_actor = window_actor;
  window_info->size_changed_id =
    g_signal_connect (window, "size-changed",
                      G_CALLBACK (on_window_size_position_changed), self);
  window_info->position_changed_id =
    g_signal_connect (window, "position-changed",
                      G_CALLBACK (on_window_size_position_changed), self);
  window_info->window_actor_destroy_id =
    g_signal_connect_swapped (window_actor, "destroy",
                              G_CALLBACK (on_window_destroyed), actor);
  window_info->destroy_id =
    g_signal_connect (actor, "destroy",
                      G_CALLBACK (on_actor_destroyed), self);

  g_hash_table_insert (priv->windows, actor, window_info);

  clutter_actor_add_child (priv->container, actor);

  on_layout_changed (self);

  return actor;
}

/**
 * shell_window_preview_layout_remove_window:
 * @self: a #ShellWindowPreviewLayout
 * @window: the #MetaWindow
 *
 * Removes a MetaWindow @window from the preview which has been added
 * previously using shell_window_preview_layout_add_window().
 * If @window is not part of preview, this function will do nothing.
 */
void
shell_window_preview_layout_remove_window (ShellWindowPreviewLayout *self,
                                           MetaWindow               *window)
{
  ShellWindowPreviewLayoutPrivate *priv;
  ClutterActor *actor;
  WindowInfo *window_info = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (SHELL_IS_WINDOW_PREVIEW_LAYOUT (self));
  g_return_if_fail (META_IS_WINDOW (window));

  priv = shell_window_preview_layout_get_instance_private (self);

  g_hash_table_iter_init (&iter, priv->windows);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      WindowInfo *info = value;

      if (info->window == window)
        {
          actor = CLUTTER_ACTOR (key);
          window_info = info;
          break;
        }
    }

  if (window_info == NULL)
    return;

  g_clear_signal_handler (&window_info->size_changed_id, window);
  g_clear_signal_handler (&window_info->position_changed_id, window);
  g_clear_signal_handler (&window_info->window_actor_destroy_id, window_info->window_actor);
  g_clear_signal_handler (&window_info->destroy_id, actor);

  g_hash_table_remove (priv->windows, actor);

  clutter_actor_remove_child (priv->container, actor);

  on_layout_changed (self);
}

/**
 * shell_window_preview_layout_get_windows:
 * @self: a #ShellWindowPreviewLayout
 *
 * Gets an array of all MetaWindows that were added to the layout
 * using shell_window_preview_layout_add_window(), ordered by the
 * insertion order.
 *
 * Returns: (transfer container) (element-type Meta.Window): The list of windows
 */
GList *
shell_window_preview_layout_get_windows (ShellWindowPreviewLayout *self)
{
  ShellWindowPreviewLayoutPrivate *priv;
  GList *windows = NULL;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (SHELL_IS_WINDOW_PREVIEW_LAYOUT (self), NULL);

  priv = shell_window_preview_layout_get_instance_private (self);

  g_hash_table_iter_init (&iter, priv->windows);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      WindowInfo *window_info = value;

      windows = g_list_prepend (windows, window_info->window);
    }

  return windows;
}
