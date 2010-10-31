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
#include "st-private.h"

enum
{
  PROP_0,

  PROP_ICON_NAME,
  PROP_ICON_TYPE,
  PROP_ICON_SIZE
};

G_DEFINE_TYPE (StIcon, st_icon, ST_TYPE_WIDGET)

#define ST_ICON_GET_PRIVATE(obj)    \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_ICON, StIconPrivate))

struct _StIconPrivate
{
  ClutterActor *icon_texture;

  gchar        *icon_name;
  StIconType    icon_type;
  gint          prop_icon_size;  /* icon size set as property */
  gint          theme_icon_size; /* icon size from theme node */
  gint          icon_size;       /* icon size we are using */
};

static void st_icon_update           (StIcon *icon);
static void st_icon_update_icon_size (StIcon *icon);

#define DEFAULT_ICON_SIZE 48
#define DEFAULT_ICON_TYPE ST_ICON_SYMBOLIC

static void
st_icon_set_property (GObject      *gobject,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  StIcon *icon = ST_ICON (gobject);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      st_icon_set_icon_name (icon, g_value_get_string (value));
      break;

    case PROP_ICON_TYPE:
      st_icon_set_icon_type (icon, g_value_get_enum (value));
      break;

    case PROP_ICON_SIZE:
      st_icon_set_icon_size (icon, g_value_get_int (value));
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
    case PROP_ICON_NAME:
      g_value_set_string (value, st_icon_get_icon_name (icon));
      break;

    case PROP_ICON_TYPE:
      g_value_set_enum (value, st_icon_get_icon_type (icon));
      break;

    case PROP_ICON_SIZE:
      g_value_set_int (value, st_icon_get_icon_size (icon));
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

  G_OBJECT_CLASS (st_icon_parent_class)->dispose (gobject);
}

static void
st_icon_get_preferred_height (ClutterActor *actor,
                              gfloat        for_width,
                              gfloat       *min_height_p,
                              gfloat       *nat_height_p)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  gfloat pref_height;

  if (priv->icon_texture)
    {
      gint width, height;
      clutter_texture_get_base_size (CLUTTER_TEXTURE (priv->icon_texture),
                                     &width,
                                     &height);

      if (width <= height)
        pref_height = priv->icon_size;
      else
        pref_height = height / (gfloat)width * priv->icon_size;
    }
  else
    pref_height = 0;

  if (min_height_p)
    *min_height_p = pref_height;

  if (nat_height_p)
    *nat_height_p = pref_height;

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, nat_height_p);
}

static void
st_icon_get_preferred_width (ClutterActor *actor,
                             gfloat        for_height,
                             gfloat       *min_width_p,
                             gfloat       *nat_width_p)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));
  gfloat pref_width;

  if (priv->icon_texture)
    {
      gint width, height;
      clutter_texture_get_base_size (CLUTTER_TEXTURE (priv->icon_texture),
                                     &width,
                                     &height);

      if (height <= width)
        pref_width = priv->icon_size;
      else
        pref_width = width / (gfloat)height * priv->icon_size;
    }
  else
    pref_width = 0;

  if (min_width_p)
    *min_width_p = pref_width;

  if (nat_width_p)
    *nat_width_p = pref_width;

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, nat_width_p);
}

static void
st_icon_allocate (ClutterActor           *actor,
                  const ClutterActorBox  *box,
                  ClutterAllocationFlags  flags)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  CLUTTER_ACTOR_CLASS (st_icon_parent_class)->allocate (actor, box, flags);

  if (priv->icon_texture)
    {
      ClutterActorBox content_box;

      st_theme_node_get_content_box (theme_node, box, &content_box);
      clutter_actor_allocate (priv->icon_texture, &content_box, flags);
    }
}

static void
st_icon_paint (ClutterActor *actor)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;

  /* Chain up to paint background */
  CLUTTER_ACTOR_CLASS (st_icon_parent_class)->paint (actor);

  if (priv->icon_texture)
    clutter_actor_paint (priv->icon_texture);
}

static void
st_icon_map (ClutterActor *actor)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_icon_parent_class)->map (actor);

  if (priv->icon_texture)
    clutter_actor_map (priv->icon_texture);
}

static void
st_icon_unmap (ClutterActor *actor)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_icon_parent_class)->unmap (actor);

  if (priv->icon_texture)
    clutter_actor_unmap (priv->icon_texture);
}

static void
st_icon_style_changed (StWidget *widget)
{
  StIcon *self = ST_ICON (widget);
  StThemeNode *theme_node = st_widget_get_theme_node (widget);
  StIconPrivate *priv = self->priv;

  priv->theme_icon_size = st_theme_node_get_length (theme_node, "icon-size");
  st_icon_update_icon_size (self);
}

static void
st_icon_class_init (StIconClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (StIconPrivate));

  object_class->get_property = st_icon_get_property;
  object_class->set_property = st_icon_set_property;
  object_class->dispose = st_icon_dispose;

  actor_class->get_preferred_height = st_icon_get_preferred_height;
  actor_class->get_preferred_width = st_icon_get_preferred_width;
  actor_class->allocate = st_icon_allocate;
  actor_class->paint = st_icon_paint;
  actor_class->map = st_icon_map;
  actor_class->unmap = st_icon_unmap;

  widget_class->style_changed = st_icon_style_changed;

  pspec = g_param_spec_string ("icon-name",
                               "Icon name",
                               "An icon name",
                               NULL, ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_NAME, pspec);

  pspec = g_param_spec_enum ("icon-type",
                             "Icon type",
                             "The type of icon that should be used",
                             ST_TYPE_ICON_TYPE,
                             DEFAULT_ICON_TYPE,
                             ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_TYPE, pspec);

  pspec = g_param_spec_int ("icon-size",
                            "Icon size",
                            "The size if the icon, if positive. Otherwise the size will be derived from the current style",
                            -1, G_MAXINT, -1,
                            ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_SIZE, pspec);
}

static void
st_icon_init (StIcon *self)
{
  self->priv = ST_ICON_GET_PRIVATE (self);

  self->priv->icon_size = DEFAULT_ICON_SIZE;
  self->priv->prop_icon_size = -1;
  self->priv->icon_type = DEFAULT_ICON_TYPE;
}

static void
st_icon_update (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;

  /* Get rid of the old one */
  if (priv->icon_texture)
    {
      clutter_actor_destroy (priv->icon_texture);
      priv->icon_texture = NULL;
    }

  /* Try to lookup the new one */
  if (priv->icon_name)
    {
      StTextureCache *cache = st_texture_cache_get_default ();

      priv->icon_texture = st_texture_cache_load_icon_name (cache,
                                                            priv->icon_name,
                                                            priv->icon_type,
                                                            priv->icon_size);

      if (priv->icon_texture)
        clutter_actor_set_parent (priv->icon_texture, CLUTTER_ACTOR (icon));
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (icon));
}

static void
st_icon_update_icon_size (StIcon *icon)
{
  StIconPrivate *priv = icon->priv;
  int new_size;

  if (priv->prop_icon_size > 0)
    new_size = priv->prop_icon_size;
  else if (priv->theme_icon_size > 0)
    new_size = priv->theme_icon_size;
  else
    new_size = DEFAULT_ICON_SIZE;

  if (new_size != priv->icon_size)
    {
      priv->icon_size = new_size;
      st_icon_update (icon);
    }
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
  g_return_val_if_fail (ST_IS_ICON (icon), NULL);

  return icon->priv->icon_name;
}

void
st_icon_set_icon_name (StIcon      *icon,
                       const gchar *icon_name)
{
  StIconPrivate *priv;

  g_return_if_fail (ST_IS_ICON (icon));

  priv = icon->priv;

  /* Check if there's no change */
  if (g_strcmp0 (priv->icon_name, icon_name) == 0)
    return;

  g_free (priv->icon_name);
  priv->icon_name = g_strdup (icon_name);

  st_icon_update (icon);

  g_object_notify (G_OBJECT (icon), "icon-name");
}

/**
 * st_icon_get_icon_type:
 * @icon: a #StIcon
 *
 * Gets the type of icon we'll look up to display in the actor.
 * See st_icon_set_icon_type().
 *
 * Return value: the icon type.
 */
StIconType
st_icon_get_icon_type (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), DEFAULT_ICON_TYPE);

  return icon->priv->icon_type;
}

/**
 * st_icon_set_icon_type:
 * @icon: a #StIcon
 * @icon_type: the type of icon to use
 *
 * Sets the type of icon we'll look up to display in the actor.
 * The icon type determines whether we use a symbolic icon or
 * a full color icon and also is used for specific handling for
 * application and document icons.
 */
void
st_icon_set_icon_type (StIcon     *icon,
                       StIconType  icon_type)
{
  StIconPrivate *priv;

  g_return_if_fail (ST_IS_ICON (icon));

  priv = icon->priv;

  if (icon_type == priv->icon_type)
    return;

  priv->icon_type = icon_type;
  st_icon_update (icon);

  g_object_notify (G_OBJECT (icon), "icon-type");
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
      st_icon_update_icon_size (icon);

      g_object_notify (G_OBJECT (icon), "icon-size");
    }
}
