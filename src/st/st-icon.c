/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-icon.c: icon widget
 *
 * Copyright 2009, 2010 Intel Corporation.
 * Copyright 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:st-icon
 * @short_description: a simple styled icon actor
 *
 * #StIcon is a simple styled texture actor that displays an image from
 * a stylesheet.
 */

#include "st-enum-types.h"
#include "st-icon.h"
#include "st-texture-cache.h"
#include "st-theme-context.h"
#include "st-private.h"

enum
{
  PROP_0,

  PROP_GICON,
  PROP_FALLBACK_GICON,

  PROP_ICON_NAME,
  PROP_ICON_SIZE,
  PROP_FALLBACK_ICON_NAME,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

struct _StIconPrivate
{
  ClutterActor    *icon_texture;
  ClutterActor    *pending_texture;
  gulong           opacity_handler_id;

  GIcon           *gicon;
  gint             prop_icon_size;  /* icon size set as property */
  gint             theme_icon_size; /* icon size from theme node */
  gint             icon_size;       /* icon size we are using */
  GIcon           *fallback_gicon;
  gboolean         needs_update;

  StIconColors     *colors;

  CoglPipeline    *shadow_pipeline;
  StShadow        *shadow_spec;
  graphene_size_t  shadow_size;
};

G_DEFINE_TYPE_WITH_PRIVATE (StIcon, st_icon, ST_TYPE_WIDGET)

static void st_icon_update               (StIcon *icon);
static gboolean st_icon_update_icon_size (StIcon *icon);
static void st_icon_update_shadow_pipeline (StIcon *icon);
static void st_icon_clear_shadow_pipeline (StIcon *icon);

static GIcon *default_gicon = NULL;

#define IMAGE_MISSING_ICON_NAME "image-missing"
#define DEFAULT_ICON_SIZE 48

static void
st_icon_set_property (GObject      *gobject,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  StIcon *icon = ST_ICON (gobject);

  switch (prop_id)
    {
    case PROP_GICON:
      st_icon_set_gicon (icon, g_value_get_object (value));
      break;

    case PROP_FALLBACK_GICON:
      st_icon_set_fallback_gicon (icon, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      st_icon_set_icon_name (icon, g_value_get_string (value));
      break;

    case PROP_ICON_SIZE:
      st_icon_set_icon_size (icon, g_value_get_int (value));
      break;

    case PROP_FALLBACK_ICON_NAME:
      st_icon_set_fallback_icon_name (icon, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_icon_get_property (GObject    *gobject,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  StIcon *icon = ST_ICON (gobject);

  switch (prop_id)
    {
    case PROP_GICON:
      g_value_set_object (value, st_icon_get_gicon (icon));
      break;

    case PROP_FALLBACK_GICON:
      g_value_set_object (value, st_icon_get_fallback_gicon (icon));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, st_icon_get_icon_name (icon));
      break;

    case PROP_ICON_SIZE:
      g_value_set_int (value, st_icon_get_icon_size (icon));
      break;

    case PROP_FALLBACK_ICON_NAME:
      g_value_set_string (value, st_icon_get_fallback_icon_name (icon));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_icon_dispose (GObject *gobject)
{
  StIconPrivate *priv = ST_ICON (gobject)->priv;

  if (priv->icon_texture)
    {
      clutter_actor_destroy (priv->icon_texture);
      priv->icon_texture = NULL;
    }

  if (priv->pending_texture)
    {
      clutter_actor_destroy (priv->pending_texture);
      g_object_unref (priv->pending_texture);
      priv->pending_texture = NULL;
    }

  g_clear_object (&priv->gicon);
  g_clear_object (&priv->fallback_gicon);
  g_clear_pointer (&priv->colors, st_icon_colors_unref);
  g_clear_pointer (&priv->shadow_pipeline, cogl_object_unref);
  g_clear_pointer (&priv->shadow_spec, st_shadow_unref);

  G_OBJECT_CLASS (st_icon_parent_class)->dispose (gobject);
}

static void
st_icon_paint (ClutterActor        *actor,
               ClutterPaintContext *paint_context)
{
  StIcon *icon = ST_ICON (actor);
  StIconPrivate *priv = icon->priv;

  st_widget_paint_background (ST_WIDGET (actor), paint_context);

  if (priv->icon_texture)
    {
      st_icon_update_shadow_pipeline (icon);

      if (priv->shadow_pipeline)
        {
          ClutterActorBox allocation;
          CoglFramebuffer *framebuffer;

          clutter_actor_get_allocation_box (priv->icon_texture, &allocation);
          framebuffer = clutter_paint_context_get_framebuffer (paint_context);
          _st_paint_shadow_with_opacity (priv->shadow_spec,
                                         framebuffer,
                                         priv->shadow_pipeline,
                                         &allocation,
                                         clutter_actor_get_paint_opacity (priv->icon_texture));
        }

      clutter_actor_paint (priv->icon_texture, paint_context);
    }
}

static void
st_icon_style_changed (StWidget *widget)
{
  StIcon *self = ST_ICON (widget);
  StThemeNode *theme_node = st_widget_get_theme_node (widget);
  StIconPrivate *priv = self->priv;
  gboolean should_update = FALSE;
  g_autoptr(StShadow) shadow_spec = NULL;
  StIconColors *colors;

  shadow_spec = st_theme_node_get_shadow (theme_node, "icon-shadow");

  if (shadow_spec && shadow_spec->inset)
    {
      g_warning ("The icon-shadow property does not support inset shadows");
      g_clear_pointer (&shadow_spec, st_shadow_unref);
    }

  if ((shadow_spec && priv->shadow_spec && !st_shadow_equal (shadow_spec, priv->shadow_spec)) ||
      (shadow_spec && !priv->shadow_spec) || (!shadow_spec && priv->shadow_spec))
    {
      st_icon_clear_shadow_pipeline (self);

      g_clear_pointer (&priv->shadow_spec, st_shadow_unref);
      priv->shadow_spec = g_steal_pointer (&shadow_spec);

      should_update = TRUE;
    }

  colors = st_theme_node_get_icon_colors (theme_node);

  if ((colors && priv->colors && !st_icon_colors_equal (colors, priv->colors)) ||
      (colors && !priv->colors) || (!colors && priv->colors))
    {
      g_clear_pointer (&priv->colors, st_icon_colors_unref);
      priv->colors = st_icon_colors_ref (colors);

      should_update = TRUE;
    }

  priv->theme_icon_size = (int)(0.5 + st_theme_node_get_length (theme_node, "icon-size"));

  should_update |= st_icon_update_icon_size (self);

  if (priv->needs_update || should_update)
    st_icon_update (self);

  ST_WIDGET_CLASS (st_icon_parent_class)->style_changed (widget);
}

static void
st_icon_resource_scale_changed (ClutterActor *actor)
{
  st_icon_update (ST_ICON (actor));

  if (CLUTTER_ACTOR_CLASS (st_icon_parent_class)->resource_scale_changed)
    CLUTTER_ACTOR_CLASS (st_icon_parent_class)->resource_scale_changed (actor);
}

static void
st_icon_class_init (StIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  object_class->get_property = st_icon_get_property;
  object_class->set_property = st_icon_set_property;
  object_class->dispose = st_icon_dispose;

  actor_class->paint = st_icon_paint;

  widget_class->style_changed = st_icon_style_changed;
  actor_class->resource_scale_changed = st_icon_resource_scale_changed;

  /**
   * StIcon:gicon:
   *
   * The #GIcon being displayed by this #StIcon.
   */
  props[PROP_GICON] =
    g_param_spec_object ("gicon",
                         "GIcon",
                         "The GIcon shown by this icon actor",
                         G_TYPE_ICON,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StIcon:fallback-gicon:
   *
   * The fallback #GIcon to display if #StIcon:gicon fails to load.
   */
  props[PROP_FALLBACK_GICON] =
    g_param_spec_object ("fallback-gicon",
                         "Fallback GIcon",
                         "The fallback GIcon shown if the normal icon fails to load",
                         G_TYPE_ICON,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StIcon:icon-name:
   *
   * The name of the icon if the icon being displayed is a #GThemedIcon.
   */
  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon name",
                         "An icon name",
                         NULL,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StIcon:icon-size:
   *
   * The size of the icon, if greater than `0`. Other the icon size is derived
   * from the current style.
   */
  props[PROP_ICON_SIZE] =
    g_param_spec_int ("icon-size",
                      "Icon size",
                      "The size if the icon, if positive. Otherwise the size will be derived from the current style",
                      -1, G_MAXINT, -1,
                      ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StIcon:fallback-icon-name:
   *
   * The fallback icon name of the #StIcon. See st_icon_set_fallback_icon_name()
   * for details.
   */
  props[PROP_FALLBACK_ICON_NAME] =
    g_param_spec_string ("fallback-icon-name",
                         "Fallback icon name",
                         "A fallback icon name",
                         NULL,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
st_icon_init (StIcon *self)
{
  ClutterLayoutManager *layout_manager;

  if (G_UNLIKELY (default_gicon == NULL))
    default_gicon = g_themed_icon_new (IMAGE_MISSING_ICON_NAME);

  self->priv = st_icon_get_instance_private (self);

  layout_manager = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FILL,
                                           CLUTTER_BIN_ALIGNMENT_FILL);
  clutter_actor_set_layout_manager (CLUTTER_ACTOR (self), layout_manager);

  /* Set the icon size to -1 here to make sure we apply the scale to the
   * default size on the first "style-changed" signal. */
  self->priv->icon_size = -1;
  self->priv->prop_icon_size = -1;

  self->priv->shadow_pipeline = NULL;
}

static void
st_icon_clear_shadow_pipeline (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;

  g_clear_pointer (&priv->shadow_pipeline, cogl_object_unref);
  graphene_size_init (&priv->shadow_size, 0, 0);
}

static void
st_icon_update_shadow_pipeline (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;

  if (priv->icon_texture && priv->shadow_spec)
    {
      ClutterActorBox box;
      float width, height;

      clutter_actor_get_allocation_box (CLUTTER_ACTOR (priv->icon_texture),
                                        &box);
      clutter_actor_box_get_size (&box, &width, &height);

      if (priv->shadow_pipeline == NULL ||
          priv->shadow_size.width != width ||
          priv->shadow_size.height != height)
        {
          st_icon_clear_shadow_pipeline (icon);

          priv->shadow_pipeline =
            _st_create_shadow_pipeline_from_actor (priv->shadow_spec,
                                                   priv->icon_texture);

          if (priv->shadow_pipeline)
            graphene_size_init (&priv->shadow_size, width, height);
        }
    }
}

static void
on_content_changed (ClutterActor *actor,
                    GParamSpec   *pspec,
                    StIcon       *icon)
{
  st_icon_clear_shadow_pipeline (icon);
}

static void
st_icon_finish_update (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;

  if (priv->icon_texture)
    {
      clutter_actor_destroy (priv->icon_texture);
      priv->icon_texture = NULL;
    }

  if (priv->pending_texture)
    {
      priv->icon_texture = priv->pending_texture;
      priv->pending_texture = NULL;
      clutter_actor_set_x_align (priv->icon_texture, CLUTTER_ACTOR_ALIGN_CENTER);
      clutter_actor_set_y_align (priv->icon_texture, CLUTTER_ACTOR_ALIGN_CENTER);
      clutter_actor_add_child (CLUTTER_ACTOR (icon), priv->icon_texture);

      /* Remove the temporary ref we added */
      g_object_unref (priv->icon_texture);

      st_icon_clear_shadow_pipeline (icon);

      g_signal_connect_object (priv->icon_texture, "notify::content",
                               G_CALLBACK (on_content_changed), icon, 0);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (icon));
}

static void
opacity_changed_cb (GObject *object,
                    GParamSpec *pspec,
                    gpointer user_data)
{
  StIcon *icon = user_data;
  StIconPrivate *priv = icon->priv;

  g_clear_signal_handler (&priv->opacity_handler_id, priv->pending_texture);

  st_icon_finish_update (icon);
}

static void
st_icon_update (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;
  StThemeNode *theme_node;
  StTextureCache *cache;
  gint paint_scale;
  ClutterActor *stage;
  StThemeContext *context;
  float resource_scale;

  if (priv->pending_texture)
    {
      clutter_actor_destroy (priv->pending_texture);
      g_object_unref (priv->pending_texture);
      priv->pending_texture = NULL;
      priv->opacity_handler_id = 0;
    }

  if (priv->gicon == NULL && priv->fallback_gicon == NULL)
    {
      g_clear_pointer (&priv->icon_texture, clutter_actor_destroy);
      return;
    }

  priv->needs_update = TRUE;

  theme_node = st_widget_peek_theme_node (ST_WIDGET (icon));
  if (theme_node == NULL)
    return;

  if (priv->icon_size <= 0)
    return;

  resource_scale = clutter_actor_get_resource_scale (CLUTTER_ACTOR (icon));

  stage = clutter_actor_get_stage (CLUTTER_ACTOR (icon));
  context = st_theme_context_get_for_stage (CLUTTER_STAGE (stage));
  g_object_get (context, "scale-factor", &paint_scale, NULL);

  cache = st_texture_cache_get_default ();

  if (priv->gicon != NULL)
    priv->pending_texture = st_texture_cache_load_gicon (cache,
                                                         theme_node,
                                                         priv->gicon,
                                                         priv->icon_size / paint_scale,
                                                         paint_scale,
                                                         resource_scale);

  if (priv->pending_texture == NULL && priv->fallback_gicon != NULL)
    priv->pending_texture = st_texture_cache_load_gicon (cache,
                                                         theme_node,
                                                         priv->fallback_gicon,
                                                         priv->icon_size / paint_scale,
                                                         paint_scale,
                                                         resource_scale);

  if (priv->pending_texture == NULL)
    priv->pending_texture = st_texture_cache_load_gicon (cache,
                                                         theme_node,
                                                         default_gicon,
                                                         priv->icon_size / paint_scale,
                                                         paint_scale,
                                                         resource_scale);
  priv->needs_update = FALSE;

  if (priv->pending_texture)
    {
      g_object_ref_sink (priv->pending_texture);

      if (clutter_actor_get_opacity (priv->pending_texture) != 0 || priv->icon_texture == NULL)
        {
          /* This icon is ready for showing, or nothing else is already showing */
          st_icon_finish_update (icon);
        }
      else
        {
          /* Will be shown when fully loaded */
          priv->opacity_handler_id = g_signal_connect_object (priv->pending_texture, "notify::opacity", G_CALLBACK (opacity_changed_cb), icon, 0);
        }
    }
  else if (priv->icon_texture)
    {
      clutter_actor_destroy (priv->icon_texture);
      priv->icon_texture = NULL;
    }
}

static gboolean
st_icon_update_icon_size (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;
  int new_size;
  gint scale = 1;
  ClutterActor *stage;
  StThemeContext *context;

  stage = clutter_actor_get_stage (CLUTTER_ACTOR (icon));
  if (stage != NULL)
    {
      context = st_theme_context_get_for_stage (CLUTTER_STAGE (stage));
      g_object_get (context, "scale-factor", &scale, NULL);
    }

  if (priv->prop_icon_size > 0)
    new_size = priv->prop_icon_size * scale;
  else if (priv->theme_icon_size > 0)
    new_size = priv->theme_icon_size;
  else
    new_size = DEFAULT_ICON_SIZE * scale;

  if (new_size != priv->icon_size)
    {
      priv->icon_size = new_size;
      return TRUE;
    }

  return FALSE;
}

/**
 * st_icon_new:
 *
 * Create a newly allocated #StIcon.
 *
 * Returns: A newly allocated #StIcon
 */
ClutterActor *
st_icon_new (void)
{
  return g_object_new (ST_TYPE_ICON, NULL);
}

/**
 * st_icon_get_icon_name:
 * @icon: an #StIcon
 *
 * This is a convenience method to get the icon name of the current icon, if it
 * is currenyly a #GThemedIcon, or %NULL otherwise.
 *
 * Returns: (transfer none) (nullable): The name of the icon or %NULL
 */
const gchar *
st_icon_get_icon_name (StIcon *icon)
{
  StIconPrivate *priv;

  g_return_val_if_fail (ST_IS_ICON (icon), NULL);

  priv = icon->priv;

  if (priv->gicon && G_IS_THEMED_ICON (priv->gicon))
    return g_themed_icon_get_names (G_THEMED_ICON (priv->gicon)) [0];
  else
    return NULL;
}

/**
 * st_icon_set_icon_name:
 * @icon: an #StIcon
 * @icon_name: (nullable): the name of the icon
 *
 * This is a convenience method to set the #GIcon to a #GThemedIcon created
 * using the given icon name. If @icon_name is an empty string, %NULL or
 * fails to load, the fallback icon will be shown.
 */
void
st_icon_set_icon_name (StIcon      *icon,
                       const gchar *icon_name)
{
  g_autoptr(GIcon) gicon = NULL;

  g_return_if_fail (ST_IS_ICON (icon));

  if (g_strcmp0 (icon_name, st_icon_get_icon_name (icon)) == 0)
    return;

  if (icon_name && *icon_name)
    gicon = g_themed_icon_new_with_default_fallbacks (icon_name);

  g_object_freeze_notify (G_OBJECT (icon));

  st_icon_set_gicon (icon, gicon);
  g_object_notify_by_pspec (G_OBJECT (icon), props[PROP_ICON_NAME]);

  g_object_thaw_notify (G_OBJECT (icon));
}

/**
 * st_icon_get_gicon:
 * @icon: an #StIcon
 *
 * Gets the current #GIcon in use.
 *
 * Returns: (nullable) (transfer none): The current #GIcon, if set, otherwise %NULL
 */
GIcon *
st_icon_get_gicon (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), NULL);

  return icon->priv->gicon;
}

/**
 * st_icon_set_gicon:
 * @icon: an #StIcon
 * @gicon: (nullable): a #GIcon
 *
 * Sets a #GIcon to show for the icon. If @gicon is %NULL or fails to load,
 * the fallback icon set using st_icon_set_fallback_icon() will be shown.
 */
void
st_icon_set_gicon (StIcon *icon, GIcon *gicon)
{
  g_return_if_fail (ST_IS_ICON (icon));
  g_return_if_fail (gicon == NULL || G_IS_ICON (gicon));

  if (g_icon_equal (icon->priv->gicon, gicon)) /* do nothing */
    return;

  g_set_object (&icon->priv->gicon, gicon);
  g_object_notify_by_pspec (G_OBJECT (icon), props[PROP_GICON]);

  st_icon_update (icon);
}

/**
 * st_icon_get_fallback_gicon:
 * @icon: a #StIcon
 *
 * Gets the currently set fallback #GIcon.
 *
 * Returns: (transfer none): The fallback #GIcon, if set, otherwise %NULL
 */
GIcon *
st_icon_get_fallback_gicon (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), NULL);

  return icon->priv->fallback_gicon;
}

/**
 * st_icon_set_fallback_gicon:
 * @icon: a #StIcon
 * @fallback_gicon: (nullable): the fallback #GIcon
 *
 * Sets a fallback #GIcon to show if the normal icon fails to load.
 * If @fallback_gicon is %NULL or fails to load, the icon is unset and no
 * texture will be visible for the fallback icon.
 */
void
st_icon_set_fallback_gicon (StIcon *icon,
                            GIcon  *fallback_gicon)
{
  g_return_if_fail (ST_IS_ICON (icon));
  g_return_if_fail (fallback_gicon == NULL || G_IS_ICON (fallback_gicon));

  if (g_icon_equal (icon->priv->fallback_gicon, fallback_gicon))
    return;

  g_set_object (&icon->priv->fallback_gicon, fallback_gicon);
  g_object_notify_by_pspec (G_OBJECT (icon), props[PROP_FALLBACK_GICON]);

  st_icon_update (icon);
}

/**
 * st_icon_get_icon_size:
 * @icon: an #StIcon
 *
 * Gets the explicit size set using st_icon_set_icon_size() for the icon.
 * This is not necessarily the size that the icon will be displayed at.
 *
 * Returns: The explicitly set size, or -1 if no size has been set
 */
gint
st_icon_get_icon_size (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), -1);

  return icon->priv->prop_icon_size;
}

/**
 * st_icon_set_icon_size:
 * @icon: an #StIcon
 * @size: if positive, the new size, otherwise the size will be
 *   derived from the current style
 *
 * Sets an explicit size for the icon. Setting @size to -1 will use the size
 * defined by the current style or the default icon size.
 */
void
st_icon_set_icon_size (StIcon *icon,
                       gint    size)
{
  StIconPrivate *priv;

  g_return_if_fail (ST_IS_ICON (icon));

  priv = icon->priv;
  if (priv->prop_icon_size != size)
    {
      priv->prop_icon_size = size;
      if (st_icon_update_icon_size (icon))
        st_icon_update (icon);
      g_object_notify_by_pspec (G_OBJECT (icon), props[PROP_ICON_SIZE]);
    }
}

/**
 * st_icon_get_fallback_icon_name:
 * @icon: an #StIcon
 *
 * This is a convenience method to get the icon name of the fallback
 * #GThemedIcon that is currently set.
 *
 * Returns: (transfer none): The name of the icon or %NULL if no icon is set
 */
const gchar *
st_icon_get_fallback_icon_name (StIcon *icon)
{
  StIconPrivate *priv;

  g_return_val_if_fail (ST_IS_ICON (icon), NULL);

  priv = icon->priv;

  if (priv->fallback_gicon && G_IS_THEMED_ICON (priv->fallback_gicon))
    return g_themed_icon_get_names (G_THEMED_ICON (priv->fallback_gicon)) [0];
  else
    return NULL;
}

/**
 * st_icon_set_fallback_icon_name:
 * @icon: an #StIcon
 * @fallback_icon_name: (nullable): the name of the fallback icon
 *
 * This is a convenience method to set the fallback #GIcon to a #GThemedIcon
 * created using the given icon name. If @fallback_icon_name is an empty
 * string, %NULL or fails to load, the icon is unset and no texture will
 * be visible for the fallback icon.
 */
void
st_icon_set_fallback_icon_name (StIcon      *icon,
                                const gchar *fallback_icon_name)
{
  g_autoptr(GIcon) gicon = NULL;

  g_return_if_fail (ST_IS_ICON (icon));

  if (g_strcmp0 (fallback_icon_name, st_icon_get_fallback_icon_name (icon)) == 0)
    return;

  if (fallback_icon_name && *fallback_icon_name)
    gicon = g_themed_icon_new_with_default_fallbacks (fallback_icon_name);

  g_object_freeze_notify (G_OBJECT (icon));

  st_icon_set_fallback_gicon (icon, gicon);
  g_object_notify_by_pspec (G_OBJECT (icon), props[PROP_FALLBACK_ICON_NAME]);

  g_object_thaw_notify (G_OBJECT (icon));
}
