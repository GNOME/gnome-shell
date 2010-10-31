/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-icon.c: icon widget
 *
 * Copyright 2009, 2010 Intel Corporation.
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

#include "st-icon.h"
#include "st-icon-theme.h"
#include "st-stylable.h"

#include "st-private.h"

enum
{
  PROP_0,

  PROP_ICON_NAME,
  PROP_ICON_SIZE
};

static void st_stylable_iface_init (StStylableIface *iface);

G_DEFINE_TYPE_WITH_CODE (StIcon, st_icon, ST_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ST_TYPE_STYLABLE,
                                                st_stylable_iface_init))

#define ST_ICON_GET_PRIVATE(obj)    \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ST_TYPE_ICON, StIconPrivate))

struct _StIconPrivate
{
  ClutterActor *icon_texture;

  gchar        *icon_name;
  gint          icon_size;
};

static void st_icon_update (StIcon *icon);

static void
st_stylable_iface_init (StStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (G_UNLIKELY (!is_initialized))
    {
      GParamSpec *pspec;

      is_initialized = TRUE;

      pspec = g_param_spec_string ("x-st-icon-name",
                                   "Icon name",
                                   "Icon name to load from the theme",
                                   NULL,
                                   G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_ICON, pspec);

      pspec = g_param_spec_int ("x-st-icon-size",
                                "Icon size",
                                "Size to use for icon",
                                -1, G_MAXINT, -1,
                                G_PARAM_READWRITE);
      st_stylable_iface_install_property (iface, ST_TYPE_ICON, pspec);
    }
}

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

    case PROP_ICON_SIZE:
      g_value_set_int (value, st_icon_get_icon_size (icon));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
st_icon_notify_theme_name_cb (StIconTheme *theme,
                              GParamSpec  *pspec,
                              StIcon      *self)
{
  st_icon_update (self);
}

static void
st_icon_dispose (GObject *gobject)
{
  StIconTheme *theme;
  StIconPrivate *priv = ST_ICON (gobject)->priv;

  if (priv->icon_texture)
    {
      clutter_actor_destroy (priv->icon_texture);
      priv->icon_texture = NULL;
    }

  if ((theme = st_icon_theme_get_default ()))
    {
      g_signal_handlers_disconnect_by_func (st_icon_theme_get_default (),
                                            st_icon_notify_theme_name_cb,
                                            gobject);
    }

  G_OBJECT_CLASS (st_icon_parent_class)->dispose (gobject);
}

static void
st_icon_get_preferred_height (ClutterActor *actor,
                              gfloat        for_width,
                              gfloat       *min_height_p,
                              gfloat       *nat_height_p)
{
  StPadding padding;
  gfloat pref_height;

  StIconPrivate *priv = ST_ICON (actor)->priv;

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

  st_widget_get_padding (ST_WIDGET (actor), &padding);
  pref_height += padding.top + padding.bottom;

  if (min_height_p)
    *min_height_p = pref_height;

  if (nat_height_p)
    *nat_height_p = pref_height;
}

static void
st_icon_get_preferred_width (ClutterActor *actor,
                             gfloat        for_height,
                             gfloat       *min_width_p,
                             gfloat       *nat_width_p)
{
  StPadding padding;
  gfloat pref_width;

  StIconPrivate *priv = ST_ICON (actor)->priv;

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

  st_widget_get_padding (ST_WIDGET (actor), &padding);
  pref_width += padding.left + padding.right;

  if (min_width_p)
    *min_width_p = pref_width;

  if (nat_width_p)
    *nat_width_p = pref_width;
}

static void
st_icon_allocate (ClutterActor           *actor,
                  const ClutterActorBox  *box,
                  ClutterAllocationFlags  flags)
{
  StIconPrivate *priv = ST_ICON (actor)->priv;

  CLUTTER_ACTOR_CLASS (st_icon_parent_class)->allocate (actor, box, flags);

  if (priv->icon_texture)
    {
      StPadding padding;
      ClutterActorBox child_box;

      st_widget_get_padding (ST_WIDGET (actor), &padding);
      child_box.x1 = padding.left;
      child_box.y1 = padding.top;
      child_box.x2 = box->x2 - box->x1 - padding.right;
      child_box.y2 = box->y2 - box->y1 - padding.bottom;

      clutter_actor_allocate (priv->icon_texture, &child_box, flags);
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
st_icon_class_init (StIconClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

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

  pspec = g_param_spec_string ("icon-name",
                               "Icon name",
                               "An icon name",
                               NULL, ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_NAME, pspec);

  pspec = g_param_spec_int ("icon-size",
                            "Icon size",
                            "Size of the icon",
                            1, G_MAXINT, 48,
                            ST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ICON_SIZE, pspec);
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
      StIconTheme *theme = st_icon_theme_get_default ();
      priv->icon_texture = (ClutterActor *)
        st_icon_theme_lookup_texture (theme, priv->icon_name, priv->icon_size);

      /* If the icon is missing, use the image-missing icon */
      if (!priv->icon_texture)
        priv->icon_texture = (ClutterActor *)
          st_icon_theme_lookup_texture (theme,
                                        "image-missing",
                                        priv->icon_size);

      if (priv->icon_texture)
        clutter_actor_set_parent (priv->icon_texture, CLUTTER_ACTOR (icon));
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (icon));
}

static void
st_icon_style_changed_cb (StWidget *widget)
{
  StIcon *self = ST_ICON (widget);
  StIconPrivate *priv = self->priv;

  gboolean changed = FALSE;
  gchar *icon_name = NULL;
  gint icon_size = -1;

  st_stylable_get (ST_STYLABLE (widget),
                   "x-st-icon-name", &icon_name,
                   "x-st-icon-size", &icon_size,
                   NULL);

  if (icon_name && (!priv->icon_name ||
                    !g_str_equal (icon_name, priv->icon_name)))
    {
      g_free (priv->icon_name);
      priv->icon_name = g_strdup (icon_name);
      changed = TRUE;

      g_object_notify (G_OBJECT (self), "icon-name");
    }

  if ((icon_size > 0) && (priv->icon_size != icon_size))
    {
      priv->icon_size = icon_size;
      changed = TRUE;

      g_object_notify (G_OBJECT (self), "icon-size");
    }

  if (changed)
    st_icon_update (self);
}

static void
st_icon_init (StIcon *self)
{
  self->priv = ST_ICON_GET_PRIVATE (self);

  self->priv->icon_size = 48;

  g_signal_connect (self, "style-changed",
                    G_CALLBACK (st_icon_style_changed_cb), NULL);

  /* make sure we are not reactive */
  clutter_actor_set_reactive (CLUTTER_ACTOR (self), FALSE);

  /* Reload the icon when the theme changes */
  g_signal_connect (st_icon_theme_get_default (), "notify::theme-name",
                    G_CALLBACK (st_icon_notify_theme_name_cb), self);
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

gint
st_icon_get_icon_size (StIcon *icon)
{
  g_return_val_if_fail (ST_IS_ICON (icon), -1);

  return icon->priv->icon_size;
}
void
st_icon_set_icon_size (StIcon *icon,
                       gint    size)
{
  StIconPrivate *priv;

  g_return_if_fail (ST_IS_ICON (icon));
  g_return_if_fail (size > 0);

  priv = icon->priv;
  if (priv->icon_size != size)
    {
      priv->icon_size = size;
      st_icon_update (icon);
      g_object_notify (G_OBJECT (icon), "icon-size");
    }
}
