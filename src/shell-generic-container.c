/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-generic-container
 * @short_description: A container class with signals for allocation
 *
 * #ShellGenericContainer is mainly a workaround for the current
 * lack of GObject subclassing + vfunc overrides in gjs.  We
 * implement the container interface, but proxy the virtual functions
 * into signals, which gjs can catch.
 */

/* Example implementation of a horzontal box with PACK_EXPAND for all,
 vertically and horizontally centering.

function TestFixedBox() {
    this._init();
}

TestFixedBox.prototype = {
    _init : function () {
        this.actor = new Shell.GenericContainer();
        this.spacing = 4;
        this.actor.connect('get-preferred-width', Lang.bind(this, function (actor, for_height, alloc) {
            let children = this.actor.get_children();
            let max_child_min = 0;
            let max_child_nat = 0;
            for (let i = 0; i < children.length; i++) {
                let spacing = i > 0 && i < children.length-1 ? this.spacing : 0;
                let [child_min, child_nat] = children[i].get_preferred_width(for_height);
                if (child_min > max_child_min)
                    max_child_min = child_min;
                if (child_nat > max_child_nat)
                    max_child_nat = child_nat;
            }
            let totalSpacing = this.spacing * Math.abs(children.length - 1);
            alloc.min_size = children.length * max_child_min + totalSpacing;
            alloc.nat_size = children.length * max_child_nat + totalSpacing;
        }));
        this.actor.connect('get-preferred-height', Lang.bind(this, function (actor, for_width, alloc) {
            let children = this.actor.get_children();
            let max_child_min = 0;
            let max_child_nat = 0;
            for (let i = 0; i < children.length; i++) {
                let [child_min, child_nat] = children[i].get_preferred_height(for_width);
                if (child_min > max_child_min)
                    max_child_min = child_min;
                if (child_nat > max_child_nat)
                    max_child_nat = child_nat;
            }
            alloc.min_size = max_child_min;
            alloc.nat_size = max_child_nat;
        }));
        this.actor.connect('allocate', Lang.bind(this, function (actor, box, flags) {
            let children = this.actor.get_children();
            let totalSpacing = (this.spacing * Math.abs(children.length - 1));
            let child_width = (box.x2 - box.x1 - totalSpacing) / (children.length);
            let child_height = box.y2 - box.y1;

            let x = box.x1;
            for (let i = 0; i < children.length; i++) {
                let [child_min, child_nat] = children[i].get_preferred_height(child_width);
                let vSpacing = Math.abs(child_height - child_nat) / 2;
                let childBox = new Clutter.ActorBox();
                childBox.x1 = x;
                childBox.y1 = vSpacing;
                childBox.x2 = x+child_width;
                childBox.y2 = child_height - vSpacing;
                children[i].allocate(childBox, flags);
                x += this.spacing + child_width;
            }
        }));
    }
}

function runTestFixedBox() {
    let testBox = new TestFixedBox();
    let c = new Clutter.Color();
    c.from_pixel(0xff0000a0);
    let r = new Clutter.Rectangle({ width: 50, height: 100, color: c });
    testBox.actor.add_actor(r);
    r = new Clutter.Rectangle({ width: 90, height: 70, color: c });
    testBox.actor.add_actor(r);
    r = new Clutter.Rectangle({ width: 90, height: 70, color: c });
    testBox.actor.add_actor(r);
    r = new Clutter.Rectangle({ width: 30, height: 10, color: c });
    testBox.actor.add_actor(r);

    c.from_pixel(0x00ff00a0);
    let borderBox = new Big.Box({ border: 1, border_color: c });
    borderBox.set_position(100, 100);
    borderBox.append(testBox.actor, Big.BoxPackFlags.NONE);
    Shell.Global.get().stage.add_actor(borderBox);
}
*/

#include "config.h"

#include "shell-generic-container.h"

#include <clutter/clutter.h>
#include <gtk/gtk.h>
#include <girepository.h>

G_DEFINE_TYPE(ShellGenericContainer, shell_generic_container, CLUTTER_TYPE_GROUP);

struct _ShellGenericContainerPrivate {
  gpointer dummy;
};

/* Signals */
enum
{
  GET_PREFERRED_WIDTH,
  GET_PREFERRED_HEIGHT,
  ALLOCATE,
  LAST_SIGNAL
};

static guint shell_generic_container_signals [LAST_SIGNAL] = { 0 };


static gpointer
shell_generic_container_allocation_ref (ShellGenericContainerAllocation *alloc)
{
  alloc->_refcount++;
  return alloc;
}

static void
shell_generic_container_allocation_unref (ShellGenericContainerAllocation *alloc)
{
  if (--alloc->_refcount == 0)
    {
      g_slice_free1 (sizeof (ShellGenericContainerAllocation), alloc);
    }
}

static void
shell_generic_container_allocate (ClutterActor          *self,
                                  const ClutterActorBox *box,
                                  ClutterAllocationFlags flags)
{
  /* chain up to set actor->allocation */
  (CLUTTER_ACTOR_CLASS (g_type_class_peek (clutter_actor_get_type ())))->allocate (self, box, flags);

  g_signal_emit (G_OBJECT (self), shell_generic_container_signals[ALLOCATE], 0,
                 box, flags);
}

static void
shell_generic_container_get_preferred_width (ClutterActor *actor,
                                             gfloat for_height,
                                             gfloat *min_width_p,
                                             gfloat *natural_width_p)
{
  ShellGenericContainerAllocation *alloc = g_slice_alloc0 (sizeof (ShellGenericContainerAllocation));
  alloc->_refcount = 1;
  g_signal_emit (G_OBJECT (actor), shell_generic_container_signals[GET_PREFERRED_WIDTH], 0,
                 for_height, alloc);
  if (min_width_p)
    *min_width_p = alloc->min_size;
  if (natural_width_p)
    *natural_width_p = alloc->natural_size;
  shell_generic_container_allocation_unref (alloc);
}

static void
shell_generic_container_get_preferred_height (ClutterActor *actor,
                                              gfloat for_width,
                                              gfloat *min_height_p,
                                              gfloat *natural_height_p)
{
  ShellGenericContainerAllocation *alloc = g_slice_alloc0 (sizeof (ShellGenericContainerAllocation));
  alloc->_refcount = 1;
  g_signal_emit (G_OBJECT (actor), shell_generic_container_signals[GET_PREFERRED_HEIGHT], 0,
                 for_width, alloc);
  if (min_height_p)
    *min_height_p = alloc->min_size;
  if (natural_height_p)
    *natural_height_p = alloc->natural_size;
  shell_generic_container_allocation_unref (alloc);
}

static void
shell_generic_container_class_init (ShellGenericContainerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = shell_generic_container_get_preferred_width;
  actor_class->get_preferred_height = shell_generic_container_get_preferred_height;
  actor_class->allocate = shell_generic_container_allocate;

  shell_generic_container_signals[GET_PREFERRED_WIDTH] =
    g_signal_new ("get-preferred-width",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, G_TYPE_FLOAT, SHELL_TYPE_GENERIC_CONTAINER_ALLOCATION);

  shell_generic_container_signals[GET_PREFERRED_HEIGHT] =
    g_signal_new ("get-preferred-height",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, G_TYPE_FLOAT, SHELL_TYPE_GENERIC_CONTAINER_ALLOCATION);

  shell_generic_container_signals[ALLOCATE] =
    g_signal_new ("allocate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, CLUTTER_TYPE_ACTOR_BOX, CLUTTER_TYPE_ALLOCATION_FLAGS);

  g_type_class_add_private (gobject_class, sizeof (ShellGenericContainerPrivate));
}

static void
shell_generic_container_init (ShellGenericContainer *area)
{
  area->priv = G_TYPE_INSTANCE_GET_PRIVATE (area, SHELL_TYPE_GENERIC_CONTAINER,
                                            ShellGenericContainerPrivate);
}

GType shell_generic_container_allocation_get_type (void)
{
  static GType gtype = G_TYPE_INVALID;
  if (gtype == G_TYPE_INVALID)
    {
      gtype = g_boxed_type_register_static ("ShellGenericContainerAllocation",
         (GBoxedCopyFunc)shell_generic_container_allocation_ref,
         (GBoxedFreeFunc)shell_generic_container_allocation_unref);
    }
  return gtype;
}
