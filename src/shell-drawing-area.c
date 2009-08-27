/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-drawing-area
 * @short_description: A dynamically-sized Cairo drawing area
 *
 * #ShellDrawingArea is similar to #ClutterCairoTexture in that
 * it allows drawing via Cairo; the primary difference is that
 * it is dynamically sized.  To use, connect to the @redraw
 * signal, and inside the signal handler, call
 * clutter_cairo_texture_create() to begin drawing.
 */

#include "shell-drawing-area.h"

#include <clutter/clutter.h>
#include <gtk/gtk.h>
#include <cairo.h>

G_DEFINE_TYPE(ShellDrawingArea, shell_drawing_area, CLUTTER_TYPE_GROUP);

struct _ShellDrawingAreaPrivate {
  ClutterCairoTexture *texture;
};

/* Signals */
enum
{
  REDRAW,
  LAST_SIGNAL
};

static guint shell_drawing_area_signals [LAST_SIGNAL] = { 0 };

static void
shell_drawing_area_allocate (ClutterActor          *self,
                             const ClutterActorBox *box,
                             ClutterAllocationFlags flags)
{
  ShellDrawingArea *area = SHELL_DRAWING_AREA (self);
  int width = box->x2 - box->x1;
  int height = box->y2 - box->y1;
  ClutterActorBox child_box;

  /* Chain up directly to ClutterActor to set actor->allocation.  We explicitly skip our parent class
   * ClutterGroup here because we want to override the allocate function. */
  (CLUTTER_ACTOR_CLASS (g_type_class_peek (clutter_actor_get_type ())))->allocate (self, box, flags);

  child_box.x1 = 0;
  child_box.x2 = width;
  child_box.y1 = 0;
  child_box.y2 = height;

  clutter_actor_allocate (CLUTTER_ACTOR (area->priv->texture), &child_box, flags);
  if (width > 0 && height > 0)
    {
      clutter_cairo_texture_set_surface_size (area->priv->texture,
                                              width, height);
      g_signal_emit (G_OBJECT (self), shell_drawing_area_signals[REDRAW], 0,
                     area->priv->texture);
    }
}

static void
shell_drawing_area_class_init (ShellDrawingAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->allocate = shell_drawing_area_allocate;

  shell_drawing_area_signals[REDRAW] =
    g_signal_new ("redraw",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ShellDrawingAreaClass, redraw),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, G_TYPE_OBJECT);

  g_type_class_add_private (gobject_class, sizeof (ShellDrawingAreaPrivate));
}

static void
shell_drawing_area_init (ShellDrawingArea *area)
{
  area->priv = G_TYPE_INSTANCE_GET_PRIVATE (area, SHELL_TYPE_DRAWING_AREA,
                                            ShellDrawingAreaPrivate);
  area->priv->texture = CLUTTER_CAIRO_TEXTURE (clutter_cairo_texture_new (1, 1));
  clutter_container_add_actor (CLUTTER_CONTAINER (area), CLUTTER_ACTOR (area->priv->texture));
}

/**
 * shell_drawing_area_get_texture:
 *
 * Return Value: (transfer none):
 */
ClutterCairoTexture *
shell_drawing_area_get_texture (ShellDrawingArea *area)
{
  return area->priv->texture;
}
