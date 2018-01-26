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
  PROP_ICON_NAME,
  PROP_ICON_SIZE,
  PROP_FALLBACK_ICON_NAME
};

struct _StIconPrivate
{
  ClutterActor *icon_texture;
  ClutterActor *pending_texture;
  guint         opacity_handler_id;

  GIcon        *gicon;
  gint          prop_icon_size;  /* icon size set as property */
  gint          theme_icon_size; /* icon size from theme node */
  gint          icon_size;       /* icon size we are using */
  GIcon        *fallback_gicon;

  CoglPipeline *shadow_pipeline;
  StShadow     *shadow_spec;
};

G_DEFINE_TYPE_WITH_PRIVATE (StIcon, st_icon, ST_TYPE_WIDGET)

static void st_icon_update               (StIcon *icon);
static gboolean st_icon_update_icon_size (StIcon *icon);

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
      g_value_set_object (value, icon->priv->gicon);
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
  g_clear_pointer (&priv->shadow_pipeline, cogl_object_unref);
  g_clear_pointer (&priv->shadow_spec, st_shadow_unref);

  G_OBJECT_CLASS (st_icon_parent_class)->dispose (gobject);
}

static void
st_icon_paint (ClutterActor *actor)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;

  st_widget_paint_background (ST_WIDGET (actor));

  if (priv->icon_texture)
    {
      if (priv->shadow_pipeline)
        {
          ClutterActorBox allocation;
          float width, height;

          clutter_actor_get_allocation_box (priv->icon_texture, &allocation);
          clutter_actor_box_get_size (&allocation, &width, &height);

          _st_paint_shadow_with_opacity (priv->shadow_spec,
                                         priv->shadow_pipeline,
                                         &allocation,
                                         clutter_actor_get_paint_opacity (priv->icon_texture));
        }

      clutter_actor_paint (priv->icon_texture);
    }
}

static void
st_icon_style_changed (StWidget *widget)
{
  StIcon *self = ST_ICON (widget);
  StThemeNode *theme_node = st_widget_get_theme_node (widget);
  StIconPrivate *priv = self->priv;

  g_clear_pointer (&priv->shadow_pipeline, cogl_object_unref);
  g_clear_pointer (&priv->shadow_spec, st_shadow_unref);

  priv->shadow_spec = st_theme_node_get_shadow (theme_node, "icon-shadow");

  if (priv->shadow_spec && priv->shadow_spec->inset)
    {
      g_warning ("The icon-shadow property does not support inset shadows");
      st_shadow_unref (priv->shadow_spec);
      priv->shadow_spec = NULL;
    }

  priv->theme_icon_size = (int)(0.5 + st_theme_node_get_length (theme_node, "icon-size"));
  st_icon_update_icon_size (self);
  st_icon_update (self);
}

static void
st_icon_class_init (StIconClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  object_class->get_property = st_icon_get_property;
  object_class->set_property = st_icon_set_property;
  object_class->dispose = st_icon_dispose;

  actor_class->paint = st_icon_paint;

  widget_class->style_changed = st_icon_style_changed;

  pspec = g_param_spec_object ("gicon",
                               "GIcon",
                               "The GIcon shown by this icon actor",
                               G_TYPE_ICON,
                               ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_GICON, pspec);

  pspec = g_param_spec_string ("icon-name",
                               "Icon name",
                               "An icon name",
                               NULL, ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_NAME, pspec);

  pspec = g_param_spec_int ("icon-size",
                            "Icon size",
                            "The size if the icon, if positive. Otherwise the size will be derived from the current style",
                            -1, G_MAXINT, -1,
                            ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_SIZE, pspec);

  pspec = g_param_spec_string ("fallback-icon-name",
                               "Fallback icon name",
                               "A fallback icon name",
                               NULL, ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FALLBACK_ICON_NAME, pspec);
}

static void
st_icon_init (StIcon *self)
{
  ClutterLayoutManager *layout_manager;

  self->priv = st_icon_get_instance_private (self);

  layout_manager = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FILL,
                                           CLUTTER_BIN_ALIGNMENT_FILL);
  clutter_actor_set_layout_manager (CLUTTER_ACTOR (self), layout_manager);

  self->priv->icon_size = DEFAULT_ICON_SIZE;
  self->priv->prop_icon_size = -1;

  self->priv->shadow_pipeline = NULL;
}

static void
st_icon_update_shadow_pipeline (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;

  g_clear_pointer (&priv->shadow_pipeline, cogl_object_unref);

  if (priv->shadow_spec)
   priv->shadow_pipeline = _st_create_shadow_pipeline_from_actor (priv->shadow_spec, priv->icon_texture);
}

static void
on_pixbuf_changed (ClutterTexture *texture,
                   StIcon         *icon)
{
  st_icon_update_shadow_pipeline (icon);
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

      st_icon_update_shadow_pipeline (icon);

      /* "pixbuf-change" is actually a misnomer for "texture-changed" */
      g_signal_connect_object (priv->icon_texture, "pixbuf-change",
                               G_CALLBACK (on_pixbuf_changed), icon, 0);
    }
}

static void
opacity_changed_cb (GObject *object,
                    GParamSpec *pspec,
                    gpointer user_data)
{
  StIcon *icon = user_data;
  StIconPrivate *priv = icon->priv;

  g_signal_handler_disconnect (priv->pending_texture, priv->opacity_handler_id);
  priv->opacity_handler_id = 0;

  st_icon_finish_update (icon);
}

static void
st_icon_update (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;
  StThemeNode *theme_node;
  StTextureCache *cache;
  gint scale;
  ClutterActor *stage;
  StThemeContext *context;

  if (priv->pending_texture)
    {
      clutter_actor_destroy (priv->pending_texture);
      g_object_unref (priv->pending_texture);
      priv->pending_texture = NULL;
      priv->opacity_handler_id = 0;
    }

  theme_node = st_widget_peek_theme_node (ST_WIDGET (icon));
  if (theme_node == NULL)
    return;

  stage = clutter_actor_get_stage (CLUTTER_ACTOR (icon));
  context = st_theme_context_get_for_stage (CLUTTER_STAGE (stage));
  g_object_get (context, "scale-factor", &scale, NULL);

  cache = st_texture_cache_get_default ();

  if (priv->gicon != NULL)
    priv->pending_texture = st_texture_cache_load_gicon (cache,
                                                         theme_node,
                                                         priv->gicon,
                                                         priv->icon_size,
                                                         scale,
                                                         1);

  if (priv->pending_texture == NULL && priv->fallback_gicon != NULL)
    priv->pending_texture = st_texture_cache_load_gicon (cache,
                                                         theme_node,
                                                         priv->fallback_gicon,
                                                         priv->icon_size,
                                                         scale,
                                                         1);

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

  if (priv->prop_icon_size > 0)
    new_size = priv->prop_icon_size;
  else if (priv->theme_icon_size > 0)
    {
      gint scale;
      ClutterActor *stage;
      StThemeContext *context;

      /* The theme will give us an already-scaled size, so we
       * undo it here, as priv->icon_size is in unscaled pixels.
       */
      stage = clutter_actor_get_stage (CLUTTER_ACTOR (icon));
      context = st_theme_context_get_for_stage (CLUTTER_STAGE (stage));
      g_object_get (context, "scale-factor", &scale, NULL);
      new_size = (gint) (priv->theme_icon_size / scale);
    }
  else
    new_size = DEFAULT_ICON_SIZE;

  if (new_size != priv->icon_size)
    {
      clutter_actor_queue_relayout (CLUTTER_ACTOR (icon));
      priv->icon_size = new_size;
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * st_icon_new:
 *
 * Create a newly allocated #StIcon
 *
 * Returns: A newly allocated #StIcon
 */
ClutterActor *
st_icon_new (void)
{
  return g_object_new (ST_TYPE_ICON, NULL);
}

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

void
st_icon_set_icon_name (StIcon      *icon,
                       const gchar *icon_name)
{
  StIconPrivate *priv = icon->priv;
  GIcon *gicon = NULL;

  g_return_if_fail (ST_IS_ICON (icon));

  if (icon_name)
    gicon = g_themed_icon_new_with_default_fallbacks (icon_name);

  if (g_icon_equal (priv->gicon, gicon)) /* do nothing */
    {
      if (gicon)
        g_object_unref (gicon);
      return;
    }

  if (priv->gicon)
    g_object_unref (priv->gicon);

  g_object_freeze_notify (G_OBJECT (icon));

  priv->gicon = gicon;

  g_object_notify (G_OBJECT (icon), "gicon");
  g_object_notify (G_OBJECT (icon), "icon-name");

  g_object_thaw_notify (G_OBJECT (icon));

  st_icon_update (icon);
}

/**
 * st_icon_get_gicon:
 * @icon: an icon
 *
 * Return value: (transfer none): the override GIcon, if set, or NULL
 */
GIcon *
st_icon_get_gicon (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), NULL);

  return icon->priv->gicon;
}

/**
 * st_icon_set_gicon:
 * @icon: an icon
 * @gicon: (nullable): a #GIcon to override :icon-name
 */
void
st_icon_set_gicon (StIcon *icon, GIcon *gicon)
{
  g_return_if_fail (ST_IS_ICON (icon));
  g_return_if_fail (gicon == NULL || G_IS_ICON (gicon));

  if (g_icon_equal (icon->priv->gicon, gicon)) /* do nothing */
    return;

  if (icon->priv->gicon)
    {
      g_object_unref (icon->priv->gicon);
      icon->priv->gicon = NULL;
    }

  if (gicon)
    icon->priv->gicon = g_object_ref (gicon);

  g_object_notify (G_OBJECT (icon), "gicon");

  st_icon_update (icon);
}

/**
 * st_icon_get_icon_size:
 * @icon: an icon
 *
 * Gets the size explicit size on the icon. This is not necesariily
 *  the size that the icon will actually be displayed at.
 *
 * Return value: the size explicitly set, or -1 if no size has been set
 */
gint
st_icon_get_icon_size (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), -1);

  return icon->priv->prop_icon_size;
}

/**
 * st_icon_set_icon_size:
 * @icon: an icon
 * @size: if positive, the new size, otherwise the size will be
 *   derived from the current style
 *
 * Sets an explicit size for the icon.
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
      g_object_notify (G_OBJECT (icon), "icon-size");
    }
}

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

void
st_icon_set_fallback_icon_name (StIcon      *icon,
                                const gchar *fallback_icon_name)
{
  StIconPrivate *priv = icon->priv;
  GIcon *gicon = NULL;

  g_return_if_fail (ST_IS_ICON (icon));

  if (fallback_icon_name != NULL)
    gicon = g_themed_icon_new_with_default_fallbacks (fallback_icon_name);

  if (g_icon_equal (priv->fallback_gicon, gicon)) /* do nothing */
    {
      if (gicon)
        g_object_unref (gicon);
      return;
    }

  if (priv->fallback_gicon)
    g_object_unref (priv->fallback_gicon);

  priv->fallback_gicon = gicon;

  g_object_notify (G_OBJECT (icon), "fallback-icon-name");

  st_icon_update (icon);
}
