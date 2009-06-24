/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-actor
 * @short_description: Base abstract class for all visual stage actors.
 *
 * #ClutterActor is a base abstract class for all visual elements on the
 * stage. Every object that must appear on the main #ClutterStage must also
 * be a #ClutterActor, either by using one of the classes provided by
 * Clutter, or by implementing a new #ClutterActor subclass.
 *
 * Every actor is a 2D surface positioned and optionally transformed
 * in 3D space. The actor is positioned relative to top left corner of
 * it parent with the childs origin being its anchor point (also top
 * left by default).
 *
 * The actors 2D surface is contained inside its bounding box,
 * described by the #ClutterActorBox structure:
 *
 * <figure id="actor-box">
 *   <title>Bounding box of an Actor</title>
 *   <graphic fileref="actor-box.png" format="PNG"/>
 * </figure>
 *
 * The actor box represents the untransformed area occupied by an
 * actor. Each visible actor that has been put on a #ClutterStage also
 * has a transformed area, depending on the actual transformations
 * applied to it by the developer (scale, rotation). Tranforms will
 * also be applied to any child actors. Also applied to all actors by
 * the #ClutterStage is a perspective transformation. API is provided
 * for both tranformed and untransformed actor geometry information.
 *
 * The 'modelview' transform matrix for the actor is constructed from
 * the actor settings by the following order of operations:
 * <orderedlist>
 *   <listitem><para>Translation by actor x, y coords,</para></listitem>
 *   <listitem><para>Translation by actor depth (z),</para></listitem>
 *   <listitem><para>Scaling by scale_x, scale_y,</para></listitem>
 *   <listitem><para>Rotation around z axis,</para></listitem>
 *   <listitem><para>Rotation around y axis,</para></listitem>
 *   <listitem><para>Rotation around x axis,</para></listitem>
 *   <listitem><para>Negative translation by anchor point x,
 *   y,</para></listitem>
 *   <listitem><para>Rectangular Clip is applied (this is not an operation on
 *   the matrix as such, but it is done as part of the transform set
 *   up).</para></listitem>
 * </orderedlist>
 *
 * An actor can either be explicitly sized and positioned, using the
 * various size and position accessors, like clutter_actor_set_x() or
 * clutter_actor_set_width(); or it can have a preferred width and
 * height, which then allows a layout manager to implicitly size and
 * position it by "allocating" an area for an actor. This allows for
 * actors to be manipulate in both a fixed or static parent container
 * (i.e. children of #ClutterGroup) and a more automatic or dynamic
 * layout based parent container.
 *
 * When accessing the position and size of an actor, the simple accessors
 * like clutter_actor_get_width() and clutter_actor_get_x() will return
 * a value depending on whether the actor has been explicitly sized and
 * positioned by the developer or implicitly by the layout manager.
 *
 * Depending on whether you are querying an actor or implementing a
 * layout manager, you should either use the simple accessors or use the
 * size negotiation API.
 *
 * Clutter actors are also able to receive input events and react to
 * them. Events are handled in the following ways:
 *
 * <orderedlist>
 *   <listitem><para>Actors emit pointer events if set reactive, see
 *   clutter_actor_set_reactive()</para></listitem>
 *   <listitem><para>The stage is always reactive</para></listitem>
 *   <listitem><para>Events are handled by connecting signal handlers to
 *   the numerous event signal types.</para></listitem>
 *   <listitem><para>Event handlers must return %TRUE if they handled
 *   the event and wish to block the event emission chain, or %FALSE
 *   if the emission chain must continue</para></listitem>
 *   <listitem><para>Keyboard events are emitted if actor has focus, see
 *   clutter_stage_set_key_focus()</para></listitem>
 *   <listitem><para>Motion events (motion, enter, leave) are not emitted
 *   if clutter_set_motion_events_enabled() is called with %FALSE.
 *   See clutter_set_motion_events_enabled() documentation for more
 *   information.</para></listitem>
 *   <listitem><para>Once emitted, an event emission chain has two
 *   phases: capture and bubble. An emitted event starts in the capture
 *   phase (see ClutterActor::captured-event) beginning at the stage and
 *   traversing every child actor until the event source actor is reached.
 *   The emission then enters the bubble phase, traversing back up the
 *   chain via parents until it reaches the stage. Any event handler can
 *   abort this chain by returning %TRUE (meaning "event handled").
 *   </para></listitem>
 *   <listitem><para>Pointer events will 'pass through' non reactive
 *   overlapping actors.</para></listitem>
 * </orderedlist>
 *
 * <figure id="event-flow">
 *   <title>Event flow in Clutter</title>
 *   <graphic fileref="event-flow.png" format="PNG"/>
 * </figure>
 *
 * Every '?' box in the diagram above is an entry point for application
 * code.
 *
 * For implementing a new custom actor class, please read <link
 * linkend="clutter-subclassing-ClutterActor">the corresponding section</link>
 * of the API reference.
 */

/**
 * CLUTTER_ACTOR_IS_MAPPED:
 * @a: a #ClutterActor
 *
 * Evaluates to %TRUE if the %CLUTTER_ACTOR_MAPPED flag is set.
 *
 * Means "the actor will be painted if the stage is mapped."
 *
 * %TRUE if the actor is visible; and all parents with possible exception
 * of the stage are visible; and an ancestor of the actor is a toplevel.
 *
 * Clutter auto-maintains the mapped flag whenever actors are
 * reparented or shown/hidden.
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_IS_REALIZED:
 * @a: a #ClutterActor
 *
 * Evaluates to %TRUE if the %CLUTTER_ACTOR_REALIZED flag is set.
 *
 * Whether GL resources such as textures are allocated;
 * if an actor is mapped it must also be realized, but an actor
 * can be realized and unmapped (this is so hiding an actor temporarily
 * doesn't do an expensive unrealize/realize).
 *
 * To be realized an actor must be inside a stage, and all its parents
 * must be realized. The stage is required so the actor knows the
 * correct GL context and window system resources to use.
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_IS_VISIBLE:
 * @a: a #ClutterActor
 *
 * Evaluates to %TRUE if the actor has been shown, %FALSE if it's hidden.
 * Equivalent to the ClutterActor::visible object property.
 *
 * Note that an actor is only painted onscreen if it's mapped, which
 * means it's visible, and all its parents are visible, and one of the
 * parents is a toplevel stage.
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_IS_REACTIVE:
 * @a: a #ClutterActor
 *
 * Evaluates to %TRUE if the %CLUTTER_ACTOR_REACTIVE flag is set.
 *
 * Only reactive actors will receive event-related signals.
 *
 * Since: 0.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-container.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-scriptable.h"
#include "clutter-script.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-units.h"
#include "cogl/cogl.h"

typedef struct _ShaderData ShaderData;
typedef struct _AnchorCoord AnchorCoord;

#define CLUTTER_ACTOR_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ACTOR, ClutterActorPrivate))

/* Internal helper struct to represent a point that can be stored in
   either direct pixel coordinates or as a fraction of the actor's
   size. It is used for the anchor point, scale center and rotation
   centers. */
struct _AnchorCoord
{
  gboolean is_fractional;

  union
  {
    /* Used when is_fractional == TRUE */
    struct
    {
      gdouble x;
      gdouble y;
    } fraction;

    /* Use when is_fractional == FALSE */
    ClutterVertex units;
  } v;
};

/* Internal enum used to control mapped state update.  This is a hint
 * which indicates when to do something other than just enforce
 * invariants.
 */
typedef enum {
  MAP_STATE_CHECK,           /* just enforce invariants. */
  MAP_STATE_MAKE_UNREALIZED, /* force unrealize, ignoring invariants,
                              * used when about to unparent.
                              */
  MAP_STATE_MAKE_MAPPED,     /* set mapped, error if invariants not met;
                              * used to set mapped on toplevels.
                              */
  MAP_STATE_MAKE_UNMAPPED    /* set unmapped, even if parent is mapped,
                              * used just before unmapping parent.
                              */
} MapStateChange;

struct _ClutterActorPrivate
{
  /* fixed_x, fixed_y, and the allocation box are all in parent
   * coordinates.
   */
  gfloat fixed_x;
  gfloat fixed_y;

  /* request mode */
  ClutterRequestMode request_mode;

  /* our cached request width is for this height */
  gfloat request_width_for_height;
  gfloat request_min_width;
  gfloat request_natural_width;

  /* our cached request height is for this width */
  gfloat request_height_for_width;
  gfloat request_min_height;
  gfloat request_natural_height;

  ClutterActorBox allocation;
  ClutterAllocationFlags allocation_flags;

  guint position_set                : 1;
  guint min_width_set               : 1;
  guint min_height_set              : 1;
  guint natural_width_set           : 1;
  guint natural_height_set          : 1;
  /* cached request is invalid (implies allocation is too) */
  guint needs_width_request         : 1;
  /* cached request is invalid (implies allocation is too) */
  guint needs_height_request        : 1;
  /* cached allocation is invalid (request has changed, probably) */
  guint needs_allocation            : 1;
  guint queued_redraw               : 1;
  guint show_on_set_parent          : 1;
  guint has_clip                    : 1;
  guint clip_to_allocation          : 1;
  guint enable_model_view_transform : 1;
  guint enable_paint_unmapped       : 1;

  gfloat clip[4];

  /* Rotation angles */
  gdouble rxang;
  gdouble ryang;
  gdouble rzang;

  /* Rotation center: X axis */
  AnchorCoord rx_center;

  /* Rotation center: Y axis */
  AnchorCoord ry_center;

  /* Rotation center: Z axis */
  AnchorCoord rz_center;

  /* Anchor point coordinates */
  AnchorCoord anchor;

  /* depth */
  gfloat z;

  guint8 opacity;

  ClutterActor   *parent_actor;

  gchar          *name;
  guint32         id; /* Unique ID */

  gdouble         scale_x;
  gdouble         scale_y;

  AnchorCoord     scale_center;

  ShaderData     *shader_data;

  PangoContext   *pango_context;

  ClutterActor   *opacity_parent;
};

enum
{
  PROP_0,

  PROP_NAME,

  /* X, Y, WIDTH, HEIGHT are "do what I mean" properties;
   * when set they force a size request, when gotten they
   * get the allocation if the allocation is valid, and the
   * request otherwise
   */
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,

  /* Then the rest of these size-related properties are the "actual"
   * underlying properties set or gotten by X, Y, WIDTH, HEIGHT
   */
  PROP_FIXED_X,
  PROP_FIXED_Y,

  PROP_FIXED_POSITION_SET,

  PROP_MIN_WIDTH,
  PROP_MIN_WIDTH_SET,

  PROP_MIN_HEIGHT,
  PROP_MIN_HEIGHT_SET,

  PROP_NATURAL_WIDTH,
  PROP_NATURAL_WIDTH_SET,

  PROP_NATURAL_HEIGHT,
  PROP_NATURAL_HEIGHT_SET,

  PROP_REQUEST_MODE,

  /* Allocation properties are read-only */
  PROP_ALLOCATION,

  PROP_DEPTH,

  PROP_CLIP,
  PROP_HAS_CLIP,
  PROP_CLIP_TO_ALLOCATION,

  PROP_OPACITY,

  PROP_VISIBLE,
  PROP_MAPPED,
  PROP_REALIZED,
  PROP_REACTIVE,

  PROP_SCALE_X,
  PROP_SCALE_Y,
  PROP_SCALE_CENTER_X,
  PROP_SCALE_CENTER_Y,
  PROP_SCALE_GRAVITY,

  PROP_ROTATION_ANGLE_X,
  PROP_ROTATION_ANGLE_Y,
  PROP_ROTATION_ANGLE_Z,
  PROP_ROTATION_CENTER_X,
  PROP_ROTATION_CENTER_Y,
  PROP_ROTATION_CENTER_Z,
  /* This property only makes sense for the z rotation because the
     others would depend on the actor having a size along the
     z-axis */
  PROP_ROTATION_CENTER_Z_GRAVITY,

  PROP_ANCHOR_X,
  PROP_ANCHOR_Y,
  PROP_ANCHOR_GRAVITY,

  PROP_SHOW_ON_SET_PARENT
};

enum
{
  SHOW,
  HIDE,
  DESTROY,
  PARENT_SET,
  KEY_FOCUS_IN,
  KEY_FOCUS_OUT,
  PAINT,
  PICK,
  REALIZE,
  UNREALIZE,
  QUEUE_REDRAW,
  EVENT,
  CAPTURED_EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SCROLL_EVENT,
  KEY_PRESS_EVENT,
  KEY_RELEASE_EVENT,
  MOTION_EVENT,
  ENTER_EVENT,
  LEAVE_EVENT,
  ALLOCATION_CHANGED,

  LAST_SIGNAL
};

static guint actor_signals[LAST_SIGNAL] = { 0, };

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

static void _clutter_actor_apply_modelview_transform           (ClutterActor *self);

static void clutter_actor_shader_pre_paint  (ClutterActor *actor,
                                             gboolean      repeat);
static void clutter_actor_shader_post_paint (ClutterActor *actor);

static void destroy_shader_data (ClutterActor *self);

/* These setters are all static for now, maybe they should be in the
 * public API, but they are perhaps obscure enough to leave only as
 * properties
 */
static void clutter_actor_set_min_width          (ClutterActor *self,
                                                  gfloat        min_width);
static void clutter_actor_set_min_height         (ClutterActor *self,
                                                  gfloat        min_height);
static void clutter_actor_set_natural_width      (ClutterActor *self,
                                                  gfloat        natural_width);
static void clutter_actor_set_natural_height     (ClutterActor *self,
                                                  gfloat        natural_height);
static void clutter_actor_set_min_width_set      (ClutterActor *self,
                                                  gboolean      use_min_width);
static void clutter_actor_set_min_height_set     (ClutterActor *self,
                                                  gboolean      use_min_height);
static void clutter_actor_set_natural_width_set  (ClutterActor *self,
                                                  gboolean  use_natural_width);
static void clutter_actor_set_natural_height_set (ClutterActor *self,
                                                  gboolean  use_natural_height);
static void clutter_actor_set_request_mode       (ClutterActor *self,
                                                  ClutterRequestMode mode);
static void clutter_actor_update_map_state       (ClutterActor  *self,
                                                  MapStateChange change);
static void clutter_actor_unrealize_not_hiding   (ClutterActor *self);

/* Helper routines for managing anchor coords */
static void clutter_anchor_coord_get_units (ClutterActor      *self,
                                            const AnchorCoord *coord,
                                            gfloat            *x,
                                            gfloat            *y,
                                            gfloat            *z);
static void clutter_anchor_coord_set_units (AnchorCoord       *coord,
                                            gfloat             x,
                                            gfloat             y,
                                            gfloat             z);

static ClutterGravity clutter_anchor_coord_get_gravity (AnchorCoord    *coord);
static void           clutter_anchor_coord_set_gravity (AnchorCoord    *coord,
                                                        ClutterGravity  gravity);

static gboolean clutter_anchor_coord_is_zero (const AnchorCoord *coord);

/* Helper macro which translates by the anchor coord, applies the
   given transformation and then translates back */
#define TRANSFORM_ABOUT_ANCHOR_COORD(actor,coord,transform) G_STMT_START { \
  gfloat _tx, _ty, _tz;                                                    \
  clutter_anchor_coord_get_units ((actor), (coord), &_tx, &_ty, &_tz);     \
  cogl_translate (_tx, _ty, _tz);                                          \
  { transform; }                                                           \
  cogl_translate (-_tx, -_ty, -_tz);                        } G_STMT_END

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ClutterActor,
                                  clutter_actor,
                                  G_TYPE_INITIALLY_UNOWNED,
                                  G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                         clutter_scriptable_iface_init));


#ifdef CLUTTER_ENABLE_DEBUG
/* XXX - this is for debugging only, remove once working (or leave
 * in only in some debug mode). Should leave it for a little while
 * until we're confident in the new map/realize/visible handling.
 */
static inline void
clutter_actor_verify_map_state (ClutterActor *self)
{
  ClutterActorPrivate *priv = self->priv;

  if (CLUTTER_ACTOR_IS_REALIZED (self))
    {
      /* all bets are off during reparent when we're potentially realized,
       * but should not be according to invariants
       */
      if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT))
        {
          if (priv->parent_actor == NULL)
            {
              if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
                {
                }
              else
                g_warning ("Realized non-toplevel actor should have a parent");
            }
          else if (!CLUTTER_ACTOR_IS_REALIZED (priv->parent_actor))
            {
              g_warning ("Realized actor %s[%p] has an unrealized parent %s[%p]",
                         G_OBJECT_TYPE_NAME (self), self,
                         G_OBJECT_TYPE_NAME (priv->parent_actor),
                         priv->parent_actor);
            }
        }
    }

  if (CLUTTER_ACTOR_IS_MAPPED (self))
    {
      if (!CLUTTER_ACTOR_IS_REALIZED (self))
        g_warning ("Actor is mapped but not realized");

      /* remaining bets are off during reparent when we're potentially
       * mapped, but should not be according to invariants
       */
      if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT))
        {
          if (priv->parent_actor == NULL)
            {
              if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
                {
                  if (!CLUTTER_ACTOR_IS_VISIBLE (self) &&
                      !(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_DESTRUCTION))
                    {
                      g_warning ("Toplevel actor is mapped but not visible");
                    }
                }
              else
                {
                  g_warning ("Mapped actor %s %p should have a parent",
                             G_OBJECT_TYPE_NAME (self), self);
                }
            }
          else
            {
              ClutterActor *iter = self;

              /* check for the enable_paint_unmapped flag on the actor
               * and parents; if the flag is enabled at any point of this
               * branch of the scene graph then all the later checks
               * become pointless
               */
              while (iter != NULL)
                {
                  if (iter->priv->enable_paint_unmapped)
                    return;

                  iter = iter->priv->parent_actor;
                }

              if (!CLUTTER_ACTOR_IS_VISIBLE (priv->parent_actor))
                {
                  g_warning ("Actor should not be mapped if parent "
                             "is not visible");
                }

              if (!CLUTTER_ACTOR_IS_REALIZED (priv->parent_actor))
                {
                  g_warning ("Actor should not be mapped if parent "
                             "is not realized");
                }

              if (!(CLUTTER_PRIVATE_FLAGS (priv->parent_actor) &
                    CLUTTER_ACTOR_IS_TOPLEVEL))
                {
                  if (!CLUTTER_ACTOR_IS_MAPPED (priv->parent_actor))
                    g_warning ("Actor is mapped but its non-toplevel "
                               "parent is not mapped");
                }
            }
        }
    }
}

#endif /* CLUTTER_ENABLE_DEBUG */

static void
clutter_actor_set_mapped (ClutterActor *self,
                          gboolean      mapped)
{
  if (CLUTTER_ACTOR_IS_MAPPED (self) == mapped)
    return;

  if (mapped)
    {
      CLUTTER_ACTOR_GET_CLASS (self)->map (self);
      g_assert (CLUTTER_ACTOR_IS_MAPPED (self));
    }
  else
    {
      CLUTTER_ACTOR_GET_CLASS (self)->unmap (self);
      g_assert (!CLUTTER_ACTOR_IS_MAPPED (self));
    }
}

/* this function updates the mapped and realized states according to
 * invariants, in the appropriate order.
 */
static void
clutter_actor_update_map_state (ClutterActor  *self,
                                MapStateChange change)
{
  gboolean was_mapped;
  gboolean was_realized;

  was_mapped = CLUTTER_ACTOR_IS_MAPPED (self);
  was_realized = CLUTTER_ACTOR_IS_REALIZED (self);

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      /* the mapped flag on top-level actors must be set by the
       * per-backend implementation because it might be asynchronous.
       *
       * That is, the MAPPED flag on toplevels currently tracks the X
       * server mapped-ness of the window, while the expected behavior
       * (if used to GTK) may be to track WM_STATE!=WithdrawnState.
       * This creates some weird complexity by breaking the invariant
       * that if we're visible and all ancestors shown then we are
       * also mapped - instead, we are mapped if all ancestors
       * _possibly excepting_ the stage are mapped. The stage
       * will map/unmap for example when it is minimized or
       * moved to another workspace.
       *
       * So, the only invariant on the stage is that if visible it
       * should be realized, and that it has to be visible to be
       * mapped.
       */
      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_realize (self);

      switch (change)
        {
        case MAP_STATE_CHECK:
          break;

        case MAP_STATE_MAKE_MAPPED:
          g_assert (!was_mapped);
          clutter_actor_set_mapped (self, TRUE);
          break;

        case MAP_STATE_MAKE_UNMAPPED:
          g_assert (was_mapped);
          clutter_actor_set_mapped (self, FALSE);
          break;

        case MAP_STATE_MAKE_UNREALIZED:
          /* we only use MAKE_UNREALIZED in unparent,
           * and unparenting a stage isn't possible.
           * If someone wants to just unrealize a stage
           * then clutter_actor_unrealize() doesn't
           * go through this codepath.
           */
          g_warning ("Trying to force unrealize stage is not allowed");
          break;
        }

      if (CLUTTER_ACTOR_IS_MAPPED (self) &&
          !CLUTTER_ACTOR_IS_VISIBLE (self) &&
          !(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_DESTRUCTION))
        {
          g_warning ("Clutter toplevel of type '%s' is not visible, but "
                     "it is somehow still mapped",
                     G_OBJECT_TYPE_NAME (self));
        }
    }
  else
    {
      ClutterActorPrivate *priv = self->priv;
      ClutterActor *parent = priv->parent_actor;
      gboolean should_be_mapped;
      gboolean may_be_realized;
      gboolean must_be_realized;

      should_be_mapped = FALSE;
      may_be_realized = TRUE;
      must_be_realized = FALSE;

      if (parent == NULL || change == MAP_STATE_MAKE_UNREALIZED)
        {
          may_be_realized = FALSE;
        }
      else
        {
          /* Maintain invariant that if parent is mapped, and we are
           * visible, then we are mapped ...  unless parent is a
           * stage, in which case we map regardless of parent's map
           * state but do require stage to be visible and realized.
           *
           * If parent is realized, that does not force us to be
           * realized; but if parent is unrealized, that does force
           * us to be unrealized.
           *
           * The reason we don't force children to realize with
           * parents is _clutter_actor_rerealize(); if we require that
           * a realized parent means children are realized, then to
           * unrealize an actor we would have to unrealize its
           * parents, which would end up meaning unrealizing and
           * hiding the entire stage. So we allow unrealizing a
           * child (as long as that child is not mapped) while that
           * child still has a realized parent.
           *
           * Also, if we unrealize from leaf nodes to root, and
           * realize from root to leaf, the invariants are never
           * violated if we allow children to be unrealized
           * while parents are realized.
           *
           * When unmapping, MAP_STATE_MAKE_UNMAPPED is specified
           * to force us to unmap, even though parent is still
           * mapped. This is because we're unmapping from leaf nodes
           * up to root nodes.
           */
          if (CLUTTER_ACTOR_IS_VISIBLE (self) &&
              change != MAP_STATE_MAKE_UNMAPPED)
            {
              gboolean parent_is_visible_realized_toplevel;

              parent_is_visible_realized_toplevel =
                (((CLUTTER_PRIVATE_FLAGS (parent) &
                   CLUTTER_ACTOR_IS_TOPLEVEL) != 0) &&
                 CLUTTER_ACTOR_IS_VISIBLE (parent) &&
                 CLUTTER_ACTOR_IS_REALIZED (parent));

              if (CLUTTER_ACTOR_IS_MAPPED (parent) ||
                  parent_is_visible_realized_toplevel)
                {
                  must_be_realized = TRUE;
                  should_be_mapped = TRUE;
                }
            }

          /* if the actor has been set to be painted even if unmapped
           * then we should map it and check for realization as well;
           * this is an override for the branch of the scene graph
           * which begins with this node
           */
          if (priv->enable_paint_unmapped)
            {
              if (priv->parent_actor == NULL)
                g_warning ("Attempting to map an unparented actor");

              should_be_mapped = TRUE;
              must_be_realized = TRUE;
            }

          if (!CLUTTER_ACTOR_IS_REALIZED (parent))
            may_be_realized = FALSE;
        }

      if (change == MAP_STATE_MAKE_MAPPED && !should_be_mapped)
        {
          g_warning ("Attempting to map a child that does not "
                     "meet the necessary invariants");
        }

      /* If in reparent, we temporarily suspend unmap and unrealize.
       *
       * We want to go in the order "realize, map" and "unmap, unrealize"
       */

      /* Unmap */
      if (!should_be_mapped &&
          !(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT))
        {
          clutter_actor_set_mapped (self, FALSE);
        }

      /* Realize */
      if (must_be_realized)
        clutter_actor_realize (self);

      /* if we must be realized then we may be, presumably */
      g_assert (!(must_be_realized && !may_be_realized));

      /* Unrealize */
      if (!may_be_realized &&
          !(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT))
        clutter_actor_unrealize_not_hiding (self);

      /* Map */
      if (should_be_mapped)
        {
          if (!must_be_realized)
            g_warning ("Somehow we think an actor should be mapped but "
                       "not realized, which isn't allowed");

          /* realization is allowed to fail (though I don't know what
           * an app is supposed to do about that - shouldn't it just
           * be a g_error? anyway, we have to avoid mapping if this
           * happens)
           */
          if (CLUTTER_ACTOR_IS_REALIZED (self))
            clutter_actor_set_mapped (self, TRUE);
        }
    }

#ifdef CLUTTER_ENABLE_DEBUG
  /* check all invariants were kept */
  clutter_actor_verify_map_state (self);
#endif
}

static void
clutter_actor_real_map (ClutterActor *self)
{
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (self));

  CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_MAPPED);
  /* notify on parent mapped before potentially mapping
   * children, so apps see a top-down notification.
   */
  g_object_notify (G_OBJECT (self), "mapped");

  if (CLUTTER_IS_CONTAINER (self))
    clutter_container_foreach_with_internals (CLUTTER_CONTAINER (self),
                                              CLUTTER_CALLBACK (clutter_actor_map),
                                              NULL);
}

/**
 * clutter_actor_map:
 * @self: A #ClutterActor
 *
 * Sets the #CLUTTER_ACTOR_MAPPED flag on the actor and possibly maps
 * and realizes its children if they are visible. Does nothing if the
 * actor is not visible.
 *
 * Calling this is allowed in only one case: you are implementing the
 * #ClutterActor::map virtual function in an actor and you need to map
 * the children of that actor. It is not necessary to call this
 * if you implement #ClutterContainer because the default implementation
 * will automatically map children of containers.
 *
 * When overriding map, it is mandatory to chain up to the parent
 * implementation.
 *
 * Since: 1.0
 */
void
clutter_actor_map (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (CLUTTER_ACTOR_IS_MAPPED (self))
    return;

  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    return;

  clutter_actor_update_map_state (self, MAP_STATE_MAKE_MAPPED);
}

static void
clutter_actor_real_unmap (ClutterActor *self)
{
  g_assert (CLUTTER_ACTOR_IS_MAPPED (self));

  if (CLUTTER_IS_CONTAINER (self))
    clutter_container_foreach_with_internals (CLUTTER_CONTAINER (self),
                                              CLUTTER_CALLBACK (clutter_actor_unmap),
                                              NULL);

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_MAPPED);

  /* notify on parent mapped after potentially unmapping
   * children, so apps see a bottom-up notification.
   */
  g_object_notify (G_OBJECT (self), "mapped");

  /* relinquish keyboard focus if we were unmapped while owning it */
  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL))
    {
      ClutterActor *stage;

      stage = clutter_actor_get_stage (self);

      if (stage &&
          clutter_stage_get_key_focus (CLUTTER_STAGE (stage)) == self)
        {
          clutter_stage_set_key_focus (CLUTTER_STAGE (stage), NULL);
        }
    }
}

/**
 * clutter_actor_unmap:
 * @self: A #ClutterActor
 *
 * Unsets the #CLUTTER_ACTOR_MAPPED flag on the actor and possibly
 * unmaps its children if they were mapped.
 *
 * Calling this is allowed in only one case: you are implementing the
 * #ClutterActor::unmap virtual function in an actor and you need to
 * unmap the children of that actor. It is not necessary to call this
 * if you implement #ClutterContainer because the default implementation
 * will automatically unmap children of containers.
 *
 * When overriding unmap, it is mandatory to chain up to the parent
 * implementation.
 *
 * Since: 1.0
 */
void
clutter_actor_unmap (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (!CLUTTER_ACTOR_IS_MAPPED (self))
    return;

  clutter_actor_update_map_state (self, MAP_STATE_MAKE_UNMAPPED);
}

static void
clutter_actor_real_show (ClutterActor *self)
{
  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_VISIBLE);
      /* we notify on the "visible" flag in the clutter_actor_show()
       * wrapper so the entire show signal emission completes first
       * (?)
       */
      clutter_actor_update_map_state (self, MAP_STATE_CHECK);

      clutter_actor_queue_relayout (self);
    }
}

/**
 * clutter_actor_show:
 * @self: A #ClutterActor
 *
 * Flags an actor to be displayed. An actor that isn't shown will not
 * be rendered on the stage.
 *
 * Actors are visible by default.
 *
 * If this function is called on an actor without a parent, the
 * #ClutterActor:show-on-set-parent will be set to %TRUE as a side
 * effect.
 */
void
clutter_actor_show (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

#ifdef CLUTTER_ENABLE_DEBUG
  clutter_actor_verify_map_state (self);
#endif

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  if (!priv->show_on_set_parent && !priv->parent_actor)
    {
      priv->show_on_set_parent = TRUE;
      g_object_notify (G_OBJECT (self), "show-on-set-parent");
    }

  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      g_signal_emit (self, actor_signals[SHOW], 0);
      g_object_notify (G_OBJECT (self), "visible");
    }

  if (priv->parent_actor)
    clutter_actor_queue_redraw (priv->parent_actor);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_show_all:
 * @self: a #ClutterActor
 *
 * Calls clutter_actor_show() on all children of an actor (if any).
 *
 * Since: 0.2
 */
void
clutter_actor_show_all (ClutterActor *self)
{
  ClutterActorClass *klass;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  if (klass->show_all)
    klass->show_all (self);
}

void
clutter_actor_real_hide (ClutterActor *self)
{
  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_VISIBLE);

      /* we notify on the "visible" flag in the clutter_actor_hide()
       * wrapper so the entire hide signal emission completes first
       * (?)
       */
      clutter_actor_update_map_state (self, MAP_STATE_CHECK);

      clutter_actor_queue_relayout (self);
    }
}

/**
 * clutter_actor_hide:
 * @self: A #ClutterActor
 *
 * Flags an actor to be hidden. A hidden actor will not be
 * rendered on the stage.
 *
 * Actors are visible by default.
 *
 * If this function is called on an actor without a parent, the
 * #ClutterActor:show-on-set-parent property will be set to %FALSE
 * as a side-effect.
 */
void
clutter_actor_hide (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

#ifdef CLUTTER_ENABLE_DEBUG
  clutter_actor_verify_map_state (self);
#endif

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  if (priv->show_on_set_parent && !priv->parent_actor)
    {
      priv->show_on_set_parent = FALSE;
      g_object_notify (G_OBJECT (self), "show-on-set-parent");
    }

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      g_signal_emit (self, actor_signals[HIDE], 0);
      g_object_notify (G_OBJECT (self), "visible");
    }

  if (priv->parent_actor)
    clutter_actor_queue_redraw (priv->parent_actor);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_hide_all:
 * @self: a #ClutterActor
 *
 * Calls clutter_actor_hide() on all child actors (if any).
 *
 * Since: 0.2
 */
void
clutter_actor_hide_all (ClutterActor *self)
{
  ClutterActorClass *klass;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  if (klass->hide_all)
    klass->hide_all (self);
}

/**
 * clutter_actor_realize:
 * @self: A #ClutterActor
 *
 * Creates any underlying graphics resources needed by the actor to be
 * displayed.
 *
 * Realization means the actor is now tied to a specific rendering context
 * (that is, a specific toplevel stage).
 *
 * This function does nothing if the actor is already realized.
 *
 * Because a realized actor must have realized parent actors, calling
 * clutter_actor_realize() will also realize all parents of the actor.
 *
 * This function does not realize child actors, except in the special
 * case that realizing the stage, when the stage is visible, will
 * suddenly map (and thus realize) the children of the stage.
 **/
void
clutter_actor_realize (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

#ifdef CLUTTER_ENABLE_DEBUG
  clutter_actor_verify_map_state (self);
#endif

  if (CLUTTER_ACTOR_IS_REALIZED (self))
    return;

  /* To be realized, our parent actors must be realized first.
   * This will only succeed if we're inside a toplevel.
   */
  if (self->priv->parent_actor != NULL)
    clutter_actor_realize (self->priv->parent_actor);

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      /* toplevels can be realized at any time */
    }
  else
    {
      /* "Fail" the realization if parent is missing or unrealized;
       * this should really be a g_warning() not some kind of runtime
       * failure; how can an app possibly recover? Instead it's a bug
       * in the app and the app should get an explanatory warning so
       * someone can fix it. But for now it's too hard to fix this
       * because e.g. ClutterTexture needs reworking.
       */
      if (self->priv->parent_actor == NULL ||
          !CLUTTER_ACTOR_IS_REALIZED (self->priv->parent_actor))
        return;
    }

  CLUTTER_NOTE (ACTOR, "Realizing actor '%s' [%p]",
                self->priv->name ? self->priv->name
                                 : G_OBJECT_TYPE_NAME (self),
                self);

  CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
  g_object_notify (G_OBJECT (self), "realized");

  g_signal_emit (self, actor_signals[REALIZE], 0);

  /* Stage actor is allowed to unset the realized flag again in its
   * default signal handler, though that is a pathological situation.
   */

  /* If realization "failed" we'll have to update child state. */
  clutter_actor_update_map_state (self, MAP_STATE_CHECK);
}

void
clutter_actor_real_unrealize (ClutterActor *self)
{
  /* we must be unmapped (implying our children are also unmapped) */
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (self));

  if (CLUTTER_IS_CONTAINER (self))
    clutter_container_foreach_with_internals (CLUTTER_CONTAINER (self),
                                              CLUTTER_CALLBACK (clutter_actor_unrealize_not_hiding),
                                              NULL);
}

/**
 * clutter_actor_unrealize:
 * @self: A #ClutterActor
 *
 * Frees up any underlying graphics resources needed by the actor to
 * be displayed.
 *
 * Unrealization means the actor is now independent of any specific
 * rendering context (is not attached to a specific toplevel stage).
 *
 * Because mapped actors must be realized, actors may not be
 * unrealized if they are mapped. This function hides the actor to be
 * sure it isn't mapped, an application-visible side effect that you
 * may not be expecting.
 *
 * This function should not really be in the public API, because
 * there isn't a good reason to call it. ClutterActor will already
 * unrealize things for you when it's important to do so.
 *
 * If you were using clutter_actor_unrealize() in a dispose
 * implementation, then don't, just chain up to ClutterActor's
 * dispose.
 *
 * If you were using clutter_actor_unrealize() to implement
 * unrealizing children of your container, then don't, ClutterActor
 * will already take care of that.
 *
 * If you were using clutter_actor_unrealize() to re-realize to
 * create your resources in a different way, then use
 * _clutter_actor_rerealize() (inside Clutter) or just call your
 * code that recreates your resources directly (outside Clutter).
 */
void
clutter_actor_unrealize (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (!CLUTTER_ACTOR_IS_MAPPED (self));

#ifdef CLUTTER_ENABLE_DEBUG
  clutter_actor_verify_map_state (self);
#endif

  clutter_actor_hide (self);

  clutter_actor_unrealize_not_hiding (self);
}

/**
 * clutter_actor_unrealize_not_hiding:
 * @self: A #ClutterActor
 *
 * Frees up any underlying graphics resources needed by the actor to
 * be displayed.
 *
 * Unrealization means the actor is now independent of any specific
 * rendering context (is not attached to a specific toplevel stage).
 *
 * Because mapped actors must be realized, actors may not be
 * unrealized if they are mapped. You must hide the actor or one of
 * its parents before attempting to unrealize.
 *
 * This function is separate from clutter_actor_unrealize() because it
 * does not automatically hide the actor.
 * Actors need not be hidden to be unrealized, they just need to
 * be unmapped. In fact we don't want to mess up the application's
 * setting of the "visible" flag, so hiding is very undesirable.
 *
 * clutter_actor_unrealize() does a clutter_actor_hide() just for
 * backward compatibility.
 */
static void
clutter_actor_unrealize_not_hiding (ClutterActor *self)
{
  /* All callers of clutter_actor_unrealize_not_hiding() should have
   * taken care of unmapping the actor first. This means
   * all our children should also be unmapped.
   */
  g_assert (!CLUTTER_ACTOR_IS_MAPPED (self));

  if (!CLUTTER_ACTOR_IS_REALIZED (self))
    return;

  /* The default handler for the signal should recursively unrealize
   * child actors. We want to unset the realized flag only _after_
   * child actors are unrealized, to maintain invariants.
   */

  g_signal_emit (self, actor_signals[UNREALIZE], 0);

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  g_object_notify (G_OBJECT (self), "realized");
}

/**
 * _clutter_actor_rerealize:
 * @self: A #ClutterActor
 * @callback: Function to call while unrealized
 * @data: data for callback
 *
 * If an actor is already unrealized, this just calls the callback.
 *
 * If it is realized, it unrealizes temporarily, calls the callback,
 * and then re-realizes the actor.
 *
 * As a side effect, leaves all children of the actor unrealized if
 * the actor was realized but not showing.  This is because when we
 * unrealize the actor temporarily we must unrealize its children
 * (e.g. children of a stage can't be realized if stage window is
 * gone). And we aren't clever enough to save the realization state of
 * all children. In most cases this should not matter, because
 * the children will automatically realize when they next become mapped.
 */
void
_clutter_actor_rerealize (ClutterActor    *self,
                          ClutterCallback  callback,
                          void            *data)
{
  gboolean was_mapped;
  gboolean was_showing;
  gboolean was_realized;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

#ifdef CLUTTER_ENABLE_DEBUG
  clutter_actor_verify_map_state (self);
#endif

  was_realized = CLUTTER_ACTOR_IS_REALIZED (self);
  was_mapped = CLUTTER_ACTOR_IS_MAPPED (self);
  was_showing = CLUTTER_ACTOR_IS_VISIBLE (self);

  /* Must be unmapped to unrealize. Note we only have to hide this
   * actor if it was mapped (if all parents were showing).  If actor
   * is merely visible (but not mapped), then that's fine, we can
   * leave it visible.
   */
  if (was_mapped)
    clutter_actor_hide (self);

  g_assert (!CLUTTER_ACTOR_IS_MAPPED (self));

  /* unrealize self and all children */
  clutter_actor_unrealize_not_hiding (self);

  if (callback != NULL)
    {
      (* callback) (self, data);
    }

  if (was_showing)
    clutter_actor_show (self); /* will realize only if mapping implies it */
  else if (was_realized)
    clutter_actor_realize (self); /* realize self and all parents */
}

static void
clutter_actor_real_pick (ClutterActor       *self,
			 const ClutterColor *color)
{
  /* the default implementation is just to paint a rectangle
   * with the same size of the actor using the passed color
   */
  if (clutter_actor_should_pick_paint (self))
    {
      ClutterActorBox box = { 0, };
      float width, height;

      clutter_actor_get_allocation_box (self, &box);

      width = box.x2 - box.x1;
      height = box.y2 - box.y1;

      cogl_set_source_color4ub (color->red,
                                color->green,
                                color->blue,
                                color->alpha);

      cogl_rectangle (0, 0, width, height);
    }
}

/**
 * clutter_actor_should_pick_paint:
 * @self: A #ClutterActor
 *
 * Should be called inside the implementation of the
 * #ClutterActor::pick virtual function in order to check whether
 * the actor should paint itself in pick mode or not.
 *
 * This function should never be called directly by applications.
 *
 * Return value: %TRUE if the actor should paint its silhouette,
 *   %FALSE otherwise
 */
gboolean
clutter_actor_should_pick_paint (ClutterActor *self)
{
  ClutterMainContext *context;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  context = _clutter_context_get_default ();

  if (CLUTTER_ACTOR_IS_MAPPED (self) &&
      (G_UNLIKELY (context->pick_mode == CLUTTER_PICK_ALL) ||
       CLUTTER_ACTOR_IS_REACTIVE (self)))
    return TRUE;

  return FALSE;
}

static void
clutter_actor_real_get_preferred_width (ClutterActor *self,
                                        gfloat        for_height,
                                        gfloat       *min_width_p,
                                        gfloat       *natural_width_p)
{
  /* Default implementation is always 0x0, usually an actor
   * using this default is relying on someone to set the
   * request manually
   */
  CLUTTER_NOTE (LAYOUT, "Default preferred width: 0, 0");

  if (min_width_p)
    *min_width_p = 0;

  if (natural_width_p)
    *natural_width_p = 0;
}

static void
clutter_actor_real_get_preferred_height (ClutterActor *self,
                                         gfloat        for_width,
                                         gfloat       *min_height_p,
                                         gfloat       *natural_height_p)
{
  /* Default implementation is always 0x0, usually an actor
   * using this default is relying on someone to set the
   * request manually
   */
  CLUTTER_NOTE (LAYOUT, "Default preferred height: 0, 0");

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    *natural_height_p = 0;
}

static void
clutter_actor_store_old_geometry (ClutterActor    *self,
                                  ClutterActorBox *box)
{
  *box = self->priv->allocation;
}

static inline void
clutter_actor_notify_if_geometry_changed (ClutterActor          *self,
                                          const ClutterActorBox *old)
{
  ClutterActorPrivate *priv = self->priv;
  GObject *obj = G_OBJECT (self);

  g_object_freeze_notify (obj);

  /* to avoid excessive requisition or allocation cycles we
   * use the cached values.
   *
   * - if we don't have an allocation we assume that we need
   *   to notify anyway
   * - if we don't have a width or a height request we notify
   *   width and height
   * - if we have a valid allocation then we check the old
   *   bounding box with the current allocation and we notify
   *   the changes
   */
  if (priv->needs_allocation)
    {
      g_object_notify (obj, "x");
      g_object_notify (obj, "y");
      g_object_notify (obj, "width");
      g_object_notify (obj, "height");
    }
  else if (priv->needs_width_request || priv->needs_height_request)
    {
      g_object_notify (obj, "width");
      g_object_notify (obj, "height");
    }
  else
    {
      gfloat xu, yu;
      gfloat widthu, heightu;

      xu = priv->allocation.x1;
      yu = priv->allocation.y1;
      widthu = priv->allocation.x2 - priv->allocation.x1;
      heightu = priv->allocation.y2 - priv->allocation.y1;

      if (xu != old->x1)
        g_object_notify (obj, "x");

      if (yu != old->y1)
        g_object_notify (obj, "y");

      if (widthu != (old->x2 - old->x1))
        g_object_notify (obj, "width");

      if (heightu != (old->y2 - old->y1))
        g_object_notify (obj, "height");
    }

  g_object_thaw_notify (obj);
}

static void
clutter_actor_real_allocate (ClutterActor           *self,
                             const ClutterActorBox  *box,
                             ClutterAllocationFlags  flags)
{
  ClutterActorPrivate *priv = self->priv;
  gboolean x1_changed, y1_changed, x2_changed, y2_changed;
  gboolean flags_changed;
  ClutterActorBox old = { 0, };

  clutter_actor_store_old_geometry (self, &old);

  x1_changed = priv->allocation.x1 != box->x1;
  y1_changed = priv->allocation.y1 != box->y1;
  x2_changed = priv->allocation.x2 != box->x2;
  y2_changed = priv->allocation.y2 != box->y2;

  flags_changed = priv->allocation_flags != flags;

  priv->allocation = *box;
  priv->allocation_flags = flags;
  priv->needs_allocation = FALSE;

  g_object_freeze_notify (G_OBJECT (self));

  if (x1_changed || y1_changed || x2_changed || y2_changed)
    {
      g_object_notify (G_OBJECT (self), "allocation");

      /* we also emit the ::allocation-changed signal for people
       * that wish to track the allocation flags
       */
      g_signal_emit (self, actor_signals[ALLOCATION_CHANGED], 0,
                     box,
                     flags);
    }

  clutter_actor_notify_if_geometry_changed (self, &old);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
clutter_actor_queue_redraw_with_origin (ClutterActor *self,
                                        ClutterActor *origin)
{
  /* already queued since last paint() */
  if (self->priv->queued_redraw)
    return;

  /* calls klass->queue_redraw in default handler */
  g_signal_emit (self, actor_signals[QUEUE_REDRAW], 0, origin);
}

static void
clutter_actor_real_queue_redraw (ClutterActor *self,
                                 ClutterActor *origin)
{
  ClutterActor *parent;

  /* already queued since last paint() */
  if (self->priv->queued_redraw)
    return;

  CLUTTER_NOTE (PAINT, "Redraw queued on '%s'",
                clutter_actor_get_name (self) ? clutter_actor_get_name (self)
                                              : G_OBJECT_TYPE_NAME (self));
  self->priv->queued_redraw = TRUE;

  /* If the actor isn't visible, we still had to emit the signal
   * to allow for a ClutterClone, but the appearance of the parent
   * won't change so we don't have to propagate up the hierarchy.
   */
  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    return;

  /* notify parents, if they are all visible eventually we'll
   * queue redraw on the stage, which queues the redraw idle.
   */
  parent = clutter_actor_get_parent (self);
  if (parent != NULL)
    {
      /* this will go up recursively */
      clutter_actor_queue_redraw_with_origin (parent, origin);
    }
}

/* like ClutterVertex, but with a w component */
typedef struct {
  gfloat x;
  gfloat y;
  gfloat z;
  gfloat w;
} full_vertex_t;

/* copies a fixed vertex into a ClutterVertex */
static inline void
full_vertex_to_units (const full_vertex_t *f,
                      ClutterVertex       *u)
{
  u->x = f->x;
  u->y = f->y;
  u->z = f->z;
}

/* Transforms a vertex using the passed matrix; vertex is
 * an in-out parameter
 */
static void
mtx_transform (const CoglMatrix *matrix,
               full_vertex_t    *vertex)
{
  cogl_matrix_transform_point (matrix,
                               &vertex->x,
                               &vertex->y,
                               &vertex->z,
                               &vertex->w);
}

/* Help macros to scale from OpenGL <-1,1> coordinates system to our
 * X-window based <0,window-size> coordinates
 */
#define MTX_GL_SCALE_X(x,w,v1,v2)       ((((((x) / (w)) + 1.0) / 2) * (v1)) + (v2))
#define MTX_GL_SCALE_Y(y,w,v1,v2)       ((v1) - (((((y) / (w)) + 1.0) / 2) * (v1)) + (v2))
#define MTX_GL_SCALE_Z(z,w,v1,v2)       (MTX_GL_SCALE_X ((z), (w), (v1), (v2)))

/* transforms a 4-tuple of coordinates using @matrix and
 * places the result into a fixed @vertex
 */
static inline void
full_vertex_transform (const CoglMatrix *matrix,
                       gfloat            x,
                       gfloat            y,
                       gfloat            z,
                       gfloat            w,
                       full_vertex_t    *vertex)
{
  full_vertex_t tmp = { 0, };

  tmp.x = x;
  tmp.y = y;
  tmp.z = z;
  tmp.w = w;

  mtx_transform (matrix, &tmp);

  *vertex = tmp;
}

/* scales a fixed @vertex using @matrix and @viewport, and
 * transforms the result into a ClutterVertex, filling @vertex_p
 */
static inline void
full_vertex_scale (const CoglMatrix    *matrix,
                   const full_vertex_t *vertex,
                   const gfloat         viewport[],
                   ClutterVertex       *vertex_p)
{
  gfloat v_x, v_y, v_width, v_height;
  full_vertex_t tmp = { 0, };

  tmp = *vertex;

  mtx_transform (matrix, &tmp);

  v_x      = viewport[0];
  v_y      = viewport[1];
  v_width  = viewport[2];
  v_height = viewport[3];

  tmp.x = MTX_GL_SCALE_X (tmp.x, tmp.w, v_width,  v_x);
  tmp.y = MTX_GL_SCALE_Y (tmp.y, tmp.w, v_height, v_y);
  tmp.z = MTX_GL_SCALE_Z (tmp.z, tmp.w, v_width,  v_x);
  tmp.w = 0;

  full_vertex_to_units (&tmp, vertex_p);
}

/* Applies the transforms associated with this actor and its ancestors,
 * retrieves the resulting OpenGL modelview matrix, and uses the matrix
 * to transform the supplied point
 *
 * The point coordinates are in-out parameters
 */
static void
clutter_actor_transform_point_relative (ClutterActor *actor,
					ClutterActor *ancestor,
					gfloat       *x,
					gfloat       *y,
					gfloat       *z,
					gfloat       *w)
{
  full_vertex_t vertex = { 0, };
  CoglMatrix matrix;

  vertex.x = (x != NULL) ? *x : 0;
  vertex.y = (y != NULL) ? *y : 0;
  vertex.z = (z != NULL) ? *z : 0;
  vertex.w = (w != NULL) ? *w : 0;

  cogl_push_matrix();

  _clutter_actor_apply_modelview_transform_recursive (actor, ancestor);

  cogl_get_modelview_matrix (&matrix);
  mtx_transform (&matrix, &vertex);

  cogl_pop_matrix();

  if (x)
    *x = vertex.x;

  if (y)
    *y = vertex.y;

  if (z)
    *z = vertex.z;

  if (w)
    *w = vertex.w;
}

/* Applies the transforms associated with this actor and its ancestors,
 * retrieves the resulting OpenGL modelview matrix, and uses the matrix
 * to transform the supplied point
 */
static void
clutter_actor_transform_point (ClutterActor *actor,
			       gfloat       *x,
			       gfloat       *y,
			       gfloat       *z,
			       gfloat       *w)
{
  full_vertex_t vertex = { 0, };
  CoglMatrix matrix;

  vertex.x = (x != NULL) ? *x : 0;
  vertex.y = (y != NULL) ? *y : 0;
  vertex.z = (z != NULL) ? *z : 0;
  vertex.w = (w != NULL) ? *w : 0;

  cogl_push_matrix();

  _clutter_actor_apply_modelview_transform_recursive (actor, NULL);

  cogl_get_modelview_matrix (&matrix);
  mtx_transform (&matrix, &vertex);

  cogl_pop_matrix();

  if (x)
    *x = vertex.x;

  if (y)
    *y = vertex.y;

  if (z)
    *z = vertex.z;

  if (w)
    *w = vertex.w;
}

/**
 * clutter_actor_apply_relative_transform_to_point:
 * @self: A #ClutterActor
 * @ancestor: A #ClutterActor ancestor, or %NULL to use the
 *   default #ClutterStage
 * @point: A point as #ClutterVertex
 * @vertex: The translated #ClutterVertex
 *
 * Transforms @point in coordinates relative to the actor into
 * ancestor-relative coordinates using the relevant transform
 * stack (i.e. scale, rotation, etc).
 *
 * If @ancestor is %NULL the ancestor will be the #ClutterStage. In
 * this case, the coordinates returned will be the coordinates on
 * the stage before the projection is applied. This is different from
 * the behaviour of clutter_actor_apply_transform_to_point().
 *
 * Since: 0.6
 */
void
clutter_actor_apply_relative_transform_to_point (ClutterActor        *self,
						 ClutterActor        *ancestor,
						 const ClutterVertex *point,
						 ClutterVertex       *vertex)
{
  gfloat x, y, z, w;
  full_vertex_t tmp;
  gfloat v[4];

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (ancestor == NULL || CLUTTER_IS_ACTOR (ancestor));
  g_return_if_fail (point != NULL);
  g_return_if_fail (vertex != NULL);

  x = vertex->x;
  y = vertex->y;
  z = vertex->z;
  w = 1.0;

  /* First we tranform the point using the OpenGL modelview matrix */
  clutter_actor_transform_point_relative (self, ancestor, &x, &y, &z, &w);

  cogl_get_viewport (v);

  /* The w[3] parameter should always be 1.0 here, so we ignore it; otherwise
   * we would have to divide the original verts with it.
   */
  tmp.x = (x + 0.5) * v[2];
  tmp.y = (0.5 - y) * v[3];
  tmp.z = (z + 0.5) * v[2];
  tmp.w = 0;

  full_vertex_to_units (&tmp, vertex);
}

/**
 * clutter_actor_apply_transform_to_point:
 * @self: A #ClutterActor
 * @point: A point as #ClutterVertex
 * @vertex: The translated #ClutterVertex
 *
 * Transforms @point in coordinates relative to the actor
 * into screen-relative coordinates with the current actor
 * transformation (i.e. scale, rotation, etc)
 *
 * Since: 0.4
 **/
void
clutter_actor_apply_transform_to_point (ClutterActor        *self,
                                        const ClutterVertex *point,
                                        ClutterVertex       *vertex)
{
  full_vertex_t tmp = { 0, };
  gfloat x, y, z, w;
  CoglMatrix matrix_p;
  gfloat v[4];

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (point != NULL);
  g_return_if_fail (vertex != NULL);

  x = point->x;
  y = point->y;
  z = point->z;
  w = 1.0;

  /* First we tranform the point using the OpenGL modelview matrix */
  clutter_actor_transform_point (self, &x, &y, &z, &w);

  tmp.x = x;
  tmp.y = y;
  tmp.z = z;
  tmp.w = w;

  cogl_get_projection_matrix (&matrix_p);
  cogl_get_viewport (v);

  /* Now, transform it again with the projection matrix */
  mtx_transform (&matrix_p, &tmp);

  /* Finaly translate from OpenGL coords to window coords */
  vertex->x = MTX_GL_SCALE_X (tmp.x, tmp.w, v[2], v[0]);
  vertex->y = MTX_GL_SCALE_Y (tmp.y, tmp.w, v[3], v[1]);
  vertex->z = MTX_GL_SCALE_Z (tmp.z, tmp.w, v[2], v[0]);
}

/* Recursively tranform supplied vertices with the tranform for the current
 * actor and up to the ancestor (like clutter_actor_transform_point() but
 * for all the vertices in one go).
 */
static void
clutter_actor_transform_vertices_relative (ClutterActor  *self,
					   ClutterActor  *ancestor,
                                           full_vertex_t  vertices[])
{
  ClutterActorPrivate *priv = self->priv;
  gfloat width, height;
  CoglMatrix mtx;

  width  = priv->allocation.x2 - priv->allocation.x1;
  height = priv->allocation.y2 - priv->allocation.y1;

  cogl_push_matrix();

  _clutter_actor_apply_modelview_transform_recursive (self, ancestor);

  cogl_get_modelview_matrix (&mtx);

  full_vertex_transform (&mtx, 0,     0,      0, 1.0, &vertices[0]);
  full_vertex_transform (&mtx, width, 0,      0, 1.0, &vertices[1]);
  full_vertex_transform (&mtx, 0,     height, 0, 1.0, &vertices[2]);
  full_vertex_transform (&mtx, width, height, 0, 1.0, &vertices[3]);

  cogl_pop_matrix();
}

/* Recursively tranform supplied box with the tranform for the current
 * actor and all its ancestors (like clutter_actor_transform_point()
 * but for all the vertices in one go) and project it into screen
 * coordinates
 */
static void
clutter_actor_transform_and_project_box (ClutterActor          *self,
					 const ClutterActorBox *box,
					 ClutterVertex          verts[4])
{
  ClutterActor *stage;
  CoglMatrix mtx;
  CoglMatrix mtx_p;
  gfloat v[4];
  gfloat width, height;
  full_vertex_t vertices[4];

  width  = box->x2 - box->x1;
  height = box->y2 - box->y1;

  /* We essentially have to dupe some code from clutter_redraw() here
   * to make sure GL Matrices etc are initialised if we're called and we
   * havn't yet rendered anything.
   *
   * Simply duping code for now in wait for Cogl cleanup that can hopefully
   * address this in a nicer way.
  */
  stage = clutter_actor_get_stage (self);

  /* FIXME: if were not yet added to a stage, its probably unsafe to
   * return default - ideally the func should fail
  */
  if (stage == NULL)
    stage = clutter_stage_get_default ();

  clutter_stage_ensure_current (CLUTTER_STAGE (stage));
  _clutter_stage_maybe_setup_viewport (CLUTTER_STAGE (stage));

  cogl_push_matrix();

  _clutter_actor_apply_modelview_transform_recursive (self, NULL);

  cogl_get_modelview_matrix (&mtx);

  full_vertex_transform (&mtx, 0,     0,      0, 1.0, &vertices[0]);
  full_vertex_transform (&mtx, width, 0,      0, 1.0, &vertices[1]);
  full_vertex_transform (&mtx, 0,     height, 0, 1.0, &vertices[2]);
  full_vertex_transform (&mtx, width, height, 0, 1.0, &vertices[3]);

  cogl_pop_matrix();

  cogl_get_projection_matrix (&mtx_p);
  cogl_get_viewport (v);

  full_vertex_scale (&mtx_p, &vertices[0], v, &verts[0]);
  full_vertex_scale (&mtx_p, &vertices[1], v, &verts[1]);
  full_vertex_scale (&mtx_p, &vertices[2], v, &verts[2]);
  full_vertex_scale (&mtx_p, &vertices[3], v, &verts[3]);
}

/**
 * clutter_actor_get_allocation_vertices:
 * @self: A #ClutterActor
 * @ancestor: A #ClutterActor to calculate the vertices against, or %NULL
 *   to use the default #ClutterStage
 * @verts: (out) (array fixed-size=4): return location for an array of
 *   4 #ClutterVertex in which to store the result.
 *
 * Calculates the transformed coordinates of the four corners of the
 * actor in the plane of @ancestor. The returned vertices relate to
 * the #ClutterActorBox coordinates as follows:
 * <itemizedlist>
 *   <listitem><para>@verts[0] contains (x1, y1)</para></listitem>
 *   <listitem><para>@verts[1] contains (x2, y1)</para></listitem>
 *   <listitem><para>@verts[2] contains (x1, y2)</para></listitem>
 *   <listitem><para>@verts[3] contains (x2, y2)</para></listitem>
 * </itemizedlist>
 *
 * If @ancestor is %NULL the ancestor will be the #ClutterStage. In
 * this case, the coordinates returned will be the coordinates on
 * the stage before the projection is applied. This is different from
 * the behaviour of clutter_actor_get_abs_allocation_vertices().
 *
 * Since: 0.6
 */
void
clutter_actor_get_allocation_vertices (ClutterActor  *self,
                                       ClutterActor  *ancestor,
                                       ClutterVertex  verts[4])
{
  ClutterActorPrivate *priv;
  ClutterActor *stage;
  gfloat v[4];
  full_vertex_t vertices[4];
  full_vertex_t tmp = { 0, };

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (ancestor == NULL || CLUTTER_IS_ACTOR (ancestor));

  priv = self->priv;

  /* We essentially have to dupe some code from clutter_redraw() here
   * to make sure GL Matrices etc are initialised if we're called and we
   * havn't yet rendered anything.
   *
   * Simply duping code for now in wait for Cogl cleanup that can hopefully
   * address this in a nicer way.
   */
  stage = clutter_actor_get_stage (self);

  /* FIXME: if were not yet added to a stage, its probably unsafe to
   * return default - idealy the func should fail
  */
  if (stage == NULL)
    stage = clutter_stage_get_default ();

  clutter_stage_ensure_current (CLUTTER_STAGE (stage));
  _clutter_stage_maybe_setup_viewport (CLUTTER_STAGE (stage));

  /* if the actor needs to be allocated we force a relayout, so that
   * clutter_actor_transform_vertices_relative() will have valid values
   * to use in the transformations
   */
  if (priv->needs_allocation)
    _clutter_stage_maybe_relayout (stage);

  clutter_actor_transform_vertices_relative (self, ancestor, vertices);

  cogl_get_viewport (v);

  /* The w[3] parameter should always be 1.0 here, so we ignore it;
   * otherwise we would have to divide the original verts with it.
   */
  tmp.x = ((vertices[0].x + 0.5) * v[2]);
  tmp.y = ((0.5 - vertices[0].y) * v[3]);
  tmp.z = ((vertices[0].z + 0.5) * v[2]);
  full_vertex_to_units (&tmp, &verts[0]);

  tmp.x = ((vertices[1].x + 0.5) * v[2]);
  tmp.y = ((0.5 - vertices[1].y) * v[3]);
  tmp.z = ((vertices[1].z + 0.5) * v[2]);
  full_vertex_to_units (&tmp, &verts[1]);

  tmp.x = ((vertices[2].x + 0.5) * v[2]);
  tmp.y = ((0.5 - vertices[2].y) * v[3]);
  tmp.z = ((vertices[2].z + 0.5) * v[2]);
  full_vertex_to_units (&tmp, &verts[2]);

  tmp.x = ((vertices[3].x + 0.5) * v[2]);
  tmp.y = ((0.5 - vertices[3].y) * v[3]);
  tmp.z = ((vertices[3].z + 0.5) * v[2]);
  full_vertex_to_units (&tmp, &verts[3]);
}

/**
 * clutter_actor_get_abs_allocation_vertices:
 * @self: A #ClutterActor
 * @verts: (out) (array fixed-size=4): Pointer to a location of an array
 *   of 4 #ClutterVertex where to store the result.
 *
 * Calculates the transformed screen coordinates of the four corners of
 * the actor; the returned vertices relate to the #ClutterActorBox
 * coordinates  as follows:
 * <itemizedlist>
 *   <listitem><para>v[0] contains (x1, y1)</para></listitem>
 *   <listitem><para>v[1] contains (x2, y1)</para></listitem>
 *   <listitem><para>v[2] contains (x1, y2)</para></listitem>
 *   <listitem><para>v[3] contains (x2, y2)</para></listitem>
 * </itemizedlist>
 *
 * Since: 0.4
 */
void
clutter_actor_get_abs_allocation_vertices (ClutterActor  *self,
                                           ClutterVertex  verts[4])
{
  ClutterActorPrivate   *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  /* if the actor needs to be allocated we force a relayout, so that
   * the actor allocation box will be valid for
   * clutter_actor_transform_and_project_box()
   */
  if (priv->needs_allocation)
    {
      ClutterActor *stage = clutter_actor_get_stage (self);

      /* FIXME: if were not yet added to a stage, its probably unsafe to
       * return default - idealy the func should fail
       */
      if (stage == NULL)
	stage = clutter_stage_get_default ();

      _clutter_stage_maybe_relayout (stage);
    }

  clutter_actor_transform_and_project_box (self,
					   &self->priv->allocation,
					   verts);
}

/* Applies the transforms associated with this actor to the
 * OpenGL modelview matrix.
 *
 * This function does not push/pop matrix; it is the responsibility
 * of the caller to do so as appropriate
 */
static void
_clutter_actor_apply_modelview_transform (ClutterActor *self)
{
  ClutterActorPrivate *priv = self->priv;
  gboolean             is_stage = CLUTTER_IS_STAGE (self);

  if (!is_stage)
    cogl_translate (priv->allocation.x1,
		    priv->allocation.y1,
		    0);

  if (priv->z)
    cogl_translate (0, 0, priv->z);

  /*
   * because the rotation involves translations, we must scale before
   * applying the rotations (if we apply the scale after the rotations,
   * the translations included in the rotation are not scaled and so the
   * entire object will move on the screen as a result of rotating it).
   */
  if (priv->scale_x != 1.0 || priv->scale_y != 1.0)
    {
      TRANSFORM_ABOUT_ANCHOR_COORD (self,
                                    &priv->scale_center,
                                    cogl_scale (priv->scale_x,
                                                priv->scale_y,
                                                1.0));
    }

  if (priv->rzang)
    TRANSFORM_ABOUT_ANCHOR_COORD (self,
                                  &priv->rz_center,
                                  cogl_rotate (priv->rzang, 0, 0, 1.0));

  if (priv->ryang)
    TRANSFORM_ABOUT_ANCHOR_COORD (self,
                                  &priv->ry_center,
                                  cogl_rotate (priv->ryang, 0, 1.0, 0));

  if (priv->rxang)
    TRANSFORM_ABOUT_ANCHOR_COORD (self,
                                  &priv->rx_center,
                                  cogl_rotate (priv->rxang, 1.0, 0, 0));

  if (!is_stage && !clutter_anchor_coord_is_zero (&priv->anchor))
    {
      gfloat x, y, z;

      clutter_anchor_coord_get_units (self, &priv->anchor, &x, &y, &z);
      cogl_translate (-x, -y, -z);
    }
}

/* Recursively applies the transforms associated with this actor and
 * its ancestors to the OpenGL modelview matrix. Use NULL if you want this
 * to go all the way down to the stage.
 *
 * This function does not push/pop matrix; it is the responsibility
 * of the caller to do so as appropriate
 */
void
_clutter_actor_apply_modelview_transform_recursive (ClutterActor *self,
						    ClutterActor *ancestor)
{
  ClutterActor *parent, *stage;

  parent = clutter_actor_get_parent (self);

  /*
   * If we reached the ancestor, quit
   * NB: NULL ancestor means the stage, and this will not trigger
   * (as it should not)
   */
  if (self == ancestor)
    return;

  stage = clutter_actor_get_stage (self);

  /* FIXME: if were not yet added to a stage, its probably unsafe to
   * return default - idealy the func should fail
  */
  if (stage == NULL)
    stage = clutter_stage_get_default ();

  if (parent)
    _clutter_actor_apply_modelview_transform_recursive (parent, ancestor);
  else if (self != stage)
    _clutter_actor_apply_modelview_transform (stage);

  _clutter_actor_apply_modelview_transform (self);
}

/**
 * clutter_actor_paint:
 * @self: A #ClutterActor
 *
 * Renders the actor to display.
 *
 * This function should not be called directly by applications.
 * Call clutter_actor_queue_redraw() to queue paints, instead.
 *
 * This function will emit the #ClutterActor::paint signal.
 */
void
clutter_actor_paint (ClutterActor *self)
{
  ClutterActorPrivate *priv;
  ClutterMainContext *context;
  gboolean clip_set = FALSE;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  context = _clutter_context_get_default ();

  /* It's an important optimization that we consider painting of
   * actors with 0 opacity to be a NOP... */
  if (G_LIKELY (context->pick_mode == CLUTTER_PICK_NONE) &&
      /* If the actor is being painted from a clone then check the
         clone's opacity instead */
      (priv->opacity_parent ? priv->opacity_parent->priv : priv)->opacity == 0)
    {
      priv->queued_redraw = FALSE;
      return;
    }

  /* if we aren't paintable (not in a toplevel with all
   * parents paintable) then do nothing.
   */
  if (!CLUTTER_ACTOR_IS_MAPPED (self))
    return;

  /* mark that we are in the paint process */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_PAINT);

  cogl_push_matrix();

  if (priv->enable_model_view_transform)
    _clutter_actor_apply_modelview_transform (self);

  if (priv->has_clip)
    {
      cogl_clip_push (priv->clip[0],
		      priv->clip[1],
		      priv->clip[2],
		      priv->clip[3]);
      clip_set = TRUE;
    }
  else if (priv->clip_to_allocation)
    {
      gfloat width, height;

      width  = priv->allocation.x2 - priv->allocation.x1;
      height = priv->allocation.y2 - priv->allocation.y1;

      cogl_clip_push (0, 0, width, height);
      clip_set = TRUE;
    }

  if (G_UNLIKELY (context->pick_mode != CLUTTER_PICK_NONE))
    {
      ClutterColor col = { 0, };

      _clutter_id_to_color (clutter_actor_get_gid (self), &col);

      /* Actor will then paint silhouette of itself in supplied
       * color.  See clutter_stage_get_actor_at_pos() for where
       * picking is enabled.
       */
      g_signal_emit (self, actor_signals[PICK], 0, &col);
    }
  else
    {
      clutter_actor_shader_pre_paint (self, FALSE);

      self->priv->queued_redraw = FALSE;
      g_signal_emit (self, actor_signals[PAINT], 0);

      clutter_actor_shader_post_paint (self);
    }

  if (clip_set)
    cogl_clip_pop();

  cogl_pop_matrix();

  /* paint sequence complete */
  CLUTTER_UNSET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_PAINT);
}

/* internal helper function set the rotation angle without affecting
   the center point
 */
static void
clutter_actor_set_rotation_internal (ClutterActor      *self,
                                     ClutterRotateAxis  axis,
                                     gdouble            angle)
{
  ClutterActorPrivate *priv = self->priv;

  g_object_ref (self);
  g_object_freeze_notify (G_OBJECT (self));

  switch (axis)
    {
    case CLUTTER_X_AXIS:
      priv->rxang = angle;
      g_object_notify (G_OBJECT (self), "rotation-angle-x");
      break;

    case CLUTTER_Y_AXIS:
      priv->ryang = angle;
      g_object_notify (G_OBJECT (self), "rotation-angle-y");
      break;

    case CLUTTER_Z_AXIS:
      priv->rzang = angle;
      g_object_notify (G_OBJECT (self), "rotation-angle-z");
      break;
    }

  g_object_thaw_notify (G_OBJECT (self));
  g_object_unref (self);

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

static void
clutter_actor_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterActor *actor = CLUTTER_ACTOR (object);
  ClutterActorPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_X:
      clutter_actor_set_x (actor, g_value_get_float (value));
      break;

    case PROP_Y:
      clutter_actor_set_y (actor, g_value_get_float (value));
      break;

    case PROP_WIDTH:
      clutter_actor_set_width (actor, g_value_get_float (value));
      break;

    case PROP_HEIGHT:
      clutter_actor_set_height (actor, g_value_get_float (value));
      break;

    case PROP_FIXED_X:
      clutter_actor_set_x (actor, g_value_get_float (value));
      break;

    case PROP_FIXED_Y:
      clutter_actor_set_y (actor, g_value_get_float (value));
      break;

    case PROP_FIXED_POSITION_SET:
      clutter_actor_set_fixed_position_set (actor, g_value_get_boolean (value));
      break;

    case PROP_MIN_WIDTH:
      clutter_actor_set_min_width (actor, g_value_get_float (value));
      break;

    case PROP_MIN_HEIGHT:
      clutter_actor_set_min_height (actor, g_value_get_float (value));
      break;

    case PROP_NATURAL_WIDTH:
      clutter_actor_set_natural_width (actor, g_value_get_float (value));
      break;

    case PROP_NATURAL_HEIGHT:
      clutter_actor_set_natural_height (actor, g_value_get_float (value));
      break;

    case PROP_MIN_WIDTH_SET:
      clutter_actor_set_min_width_set (actor, g_value_get_boolean (value));
      break;

    case PROP_MIN_HEIGHT_SET:
      clutter_actor_set_min_height_set (actor, g_value_get_boolean (value));
      break;

    case PROP_NATURAL_WIDTH_SET:
      clutter_actor_set_natural_width_set (actor, g_value_get_boolean (value));
      break;

    case PROP_NATURAL_HEIGHT_SET:
      clutter_actor_set_natural_height_set (actor, g_value_get_boolean (value));
      break;

    case PROP_REQUEST_MODE:
      clutter_actor_set_request_mode (actor, g_value_get_enum (value));
      break;

    case PROP_DEPTH:
      clutter_actor_set_depth (actor, g_value_get_float (value));
      break;

    case PROP_OPACITY:
      clutter_actor_set_opacity (actor, g_value_get_uchar (value));
      break;

    case PROP_NAME:
      clutter_actor_set_name (actor, g_value_get_string (value));
      break;

    case PROP_VISIBLE:
      if (g_value_get_boolean (value) == TRUE)
	clutter_actor_show (actor);
      else
	clutter_actor_hide (actor);
      break;

    case PROP_SCALE_X:
      clutter_actor_set_scale (actor,
                               g_value_get_double (value),
                               priv->scale_y);
      break;

    case PROP_SCALE_Y:
      clutter_actor_set_scale (actor,
                               priv->scale_x,
                               g_value_get_double (value));
      break;

    case PROP_SCALE_CENTER_X:
      {
	gfloat center_x = g_value_get_float (value);
        gfloat center_y;

        clutter_anchor_coord_get_units (actor, &priv->scale_center,
                                        NULL,
                                        &center_y,
                                        NULL);
	clutter_actor_set_scale_full (actor,
                                      priv->scale_x,
                                      priv->scale_y,
                                      center_x,
                                      center_y);
      }
      break;

    case PROP_SCALE_CENTER_Y:
      {
        gfloat center_y = g_value_get_float (value);
	gfloat center_x;

        clutter_anchor_coord_get_units (actor, &priv->scale_center,
                                        &center_x,
                                        NULL,
                                        NULL);
	clutter_actor_set_scale_full (actor,
                                      priv->scale_x,
                                      priv->scale_y,
                                      center_x,
                                      center_y);
      }
      break;

    case PROP_SCALE_GRAVITY:
      clutter_actor_set_scale_with_gravity (actor,
                                            priv->scale_x,
                                            priv->scale_y,
                                            g_value_get_enum (value));
      break;

    case PROP_CLIP:
      {
        const ClutterGeometry *geom = g_value_get_boxed (value);

	clutter_actor_set_clip (actor,
				geom->x, geom->y,
				geom->width, geom->height);
      }
      break;

    case PROP_CLIP_TO_ALLOCATION:
      if (priv->clip_to_allocation != g_value_get_boolean (value))
        {
          priv->clip_to_allocation = g_value_get_boolean (value);
          clutter_actor_queue_redraw (actor);
        }
      break;

    case PROP_REACTIVE:
      clutter_actor_set_reactive (actor, g_value_get_boolean (value));
      break;

    case PROP_ROTATION_ANGLE_X:
      clutter_actor_set_rotation_internal (actor,
                                           CLUTTER_X_AXIS,
                                           g_value_get_double (value));
      break;

    case PROP_ROTATION_ANGLE_Y:
      clutter_actor_set_rotation_internal (actor,
                                           CLUTTER_Y_AXIS,
                                           g_value_get_double (value));
      break;

    case PROP_ROTATION_ANGLE_Z:
      clutter_actor_set_rotation_internal (actor,
                                           CLUTTER_Z_AXIS,
                                           g_value_get_double (value));
      break;

    case PROP_ROTATION_CENTER_X:
      {
        const ClutterVertex *center;

        if ((center = g_value_get_boxed (value)))
          clutter_actor_set_rotation (actor,
                                      CLUTTER_X_AXIS,
                                      priv->rxang,
                                      center->x,
                                      center->y,
                                      center->z);
      }
      break;

    case PROP_ROTATION_CENTER_Y:
      {
        const ClutterVertex *center;

        if ((center = g_value_get_boxed (value)))
          clutter_actor_set_rotation (actor,
                                      CLUTTER_Y_AXIS,
                                      priv->ryang,
                                      center->x,
                                      center->y,
                                      center->z);
      }
      break;

    case PROP_ROTATION_CENTER_Z:
      {
        const ClutterVertex *center;

        if ((center = g_value_get_boxed (value)))
          clutter_actor_set_rotation (actor,
                                      CLUTTER_Z_AXIS,
                                      priv->rzang,
                                      center->x,
                                      center->y,
                                      center->z);
      }
      break;

    case PROP_ROTATION_CENTER_Z_GRAVITY:
      clutter_actor_set_z_rotation_from_gravity (actor, priv->rzang,
                                                 g_value_get_enum (value));
      break;

    case PROP_ANCHOR_X:
      {
        gfloat anchor_x = g_value_get_float (value);
        gfloat anchor_y;

        clutter_anchor_coord_get_units (actor, &priv->anchor,
                                        NULL,
                                        &anchor_y,
                                        NULL);
	clutter_actor_set_anchor_point (actor, anchor_x, anchor_y);
      }
      break;

    case PROP_ANCHOR_Y:
      {
        gfloat anchor_y = g_value_get_float (value);
        gfloat anchor_x;

        clutter_anchor_coord_get_units (actor, &priv->anchor,
                                        &anchor_x,
                                        NULL,
                                        NULL);
	clutter_actor_set_anchor_point (actor, anchor_x, anchor_y);
      }
      break;

    case PROP_ANCHOR_GRAVITY:
      clutter_actor_set_anchor_point_from_gravity (actor,
                                                   g_value_get_enum (value));
      break;

    case PROP_SHOW_ON_SET_PARENT:
      priv->show_on_set_parent = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterActor *actor = CLUTTER_ACTOR (object);
  ClutterActorPrivate *priv = actor->priv;

  switch (prop_id)
    {
    case PROP_X:
      g_value_set_float (value, clutter_actor_get_x (actor));
      break;

    case PROP_Y:
      g_value_set_float (value, clutter_actor_get_y (actor));
      break;

    case PROP_WIDTH:
      g_value_set_float (value, clutter_actor_get_width (actor));
      break;

    case PROP_HEIGHT:
      g_value_set_float (value, clutter_actor_get_height (actor));
      break;

    case PROP_FIXED_X:
      g_value_set_float (value, priv->fixed_x);
      break;

    case PROP_FIXED_Y:
      g_value_set_float (value, priv->fixed_y);
      break;

    case PROP_FIXED_POSITION_SET:
      g_value_set_boolean (value, priv->position_set);
      break;

    case PROP_MIN_WIDTH:
      g_value_set_float (value, priv->request_min_width);
      break;

    case PROP_MIN_HEIGHT:
      g_value_set_float (value, priv->request_min_height);
      break;

    case PROP_NATURAL_WIDTH:
      g_value_set_float (value, priv->request_natural_width);
      break;

    case PROP_NATURAL_HEIGHT:
      g_value_set_float (value, priv->request_natural_height);
      break;

    case PROP_MIN_WIDTH_SET:
      g_value_set_boolean (value, priv->min_width_set);
      break;

    case PROP_MIN_HEIGHT_SET:
      g_value_set_boolean (value, priv->min_height_set);
      break;

    case PROP_NATURAL_WIDTH_SET:
      g_value_set_boolean (value, priv->natural_width_set);
      break;

    case PROP_NATURAL_HEIGHT_SET:
      g_value_set_boolean (value, priv->natural_height_set);
      break;

    case PROP_REQUEST_MODE:
      g_value_set_enum (value, priv->request_mode);
      break;

    case PROP_ALLOCATION:
      g_value_set_boxed (value, &priv->allocation);
      break;

    case PROP_DEPTH:
      g_value_set_float (value, clutter_actor_get_depth (actor));
      break;

    case PROP_OPACITY:
      g_value_set_uchar (value, priv->opacity);
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, CLUTTER_ACTOR_IS_VISIBLE (actor));
      break;

    case PROP_MAPPED:
      g_value_set_boolean (value, CLUTTER_ACTOR_IS_MAPPED (actor));
      break;

    case PROP_REALIZED:
      g_value_set_boolean (value, CLUTTER_ACTOR_IS_REALIZED (actor));
      break;

    case PROP_HAS_CLIP:
      g_value_set_boolean (value, priv->has_clip);
      break;

    case PROP_CLIP:
      {
        ClutterGeometry clip = { 0, };

        clip.x      = priv->clip[0];
        clip.y      = priv->clip[1];
        clip.width  = priv->clip[2];
        clip.height = priv->clip[3];

        g_value_set_boxed (value, &clip);
      }
      break;

    case PROP_CLIP_TO_ALLOCATION:
      g_value_set_boolean (value, priv->clip_to_allocation);
      break;

    case PROP_SCALE_X:
      g_value_set_double (value, priv->scale_x);
      break;

    case PROP_SCALE_Y:
      g_value_set_double (value, priv->scale_y);
      break;

    case PROP_SCALE_CENTER_X:
      {
        gfloat center;

        clutter_actor_get_scale_center (actor, &center, NULL);

        g_value_set_float (value, center);
      }
      break;

    case PROP_SCALE_CENTER_Y:
      {
        gfloat center;

        clutter_actor_get_scale_center (actor, NULL, &center);

        g_value_set_float (value, center);
      }
      break;

    case PROP_SCALE_GRAVITY:
      g_value_set_enum (value, clutter_actor_get_scale_gravity (actor));
      break;

    case PROP_REACTIVE:
      g_value_set_boolean (value, clutter_actor_get_reactive (actor));
      break;

    case PROP_ROTATION_ANGLE_X:
      g_value_set_double (value, priv->rxang);
      break;

    case PROP_ROTATION_ANGLE_Y:
      g_value_set_double (value, priv->ryang);
      break;

    case PROP_ROTATION_ANGLE_Z:
      g_value_set_double (value, priv->rzang);
      break;

    case PROP_ROTATION_CENTER_X:
      {
        ClutterVertex center;

        clutter_actor_get_rotation (actor, CLUTTER_X_AXIS,
                                    &center.x,
                                    &center.y,
                                    &center.z);

        g_value_set_boxed (value, &center);
      }
      break;

    case PROP_ROTATION_CENTER_Y:
      {
        ClutterVertex center;

        clutter_actor_get_rotation (actor, CLUTTER_Y_AXIS,
                                    &center.x,
                                    &center.y,
                                    &center.z);

        g_value_set_boxed (value, &center);
      }
      break;

    case PROP_ROTATION_CENTER_Z:
      {
        ClutterVertex center;

        clutter_actor_get_rotation (actor, CLUTTER_Z_AXIS,
                                    &center.x,
                                    &center.y,
                                    &center.z);

        g_value_set_boxed (value, &center);
      }
      break;

    case PROP_ROTATION_CENTER_Z_GRAVITY:
      g_value_set_enum (value, clutter_actor_get_z_rotation_gravity (actor));
      break;

    case PROP_ANCHOR_X:
      {
        gfloat anchor_x;

        clutter_anchor_coord_get_units (actor, &priv->anchor,
                                        &anchor_x,
                                        NULL,
                                        NULL);
        g_value_set_float (value, anchor_x);
      }
      break;

    case PROP_ANCHOR_Y:
      {
        gfloat anchor_y;

        clutter_anchor_coord_get_units (actor, &priv->anchor,
                                        NULL,
                                        &anchor_y,
                                        NULL);
        g_value_set_float (value, anchor_y);
      }
      break;

    case PROP_ANCHOR_GRAVITY:
      g_value_set_enum (value, clutter_actor_get_anchor_point_gravity (actor));
      break;

    case PROP_SHOW_ON_SET_PARENT:
      g_value_set_boolean (value, priv->show_on_set_parent);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_dispose (GObject *object)
{
  ClutterActor *self = CLUTTER_ACTOR (object);
  ClutterActorPrivate *priv = self->priv;

  CLUTTER_NOTE (MISC, "Disposing of object (id=%d) of type '%s' (ref_count:%d)",
		self->priv->id,
		g_type_name (G_OBJECT_TYPE (self)),
                object->ref_count);

  /* avoid recursing when called from clutter_actor_destroy() */
  if (priv->parent_actor != NULL)
    {
      ClutterActor *parent = priv->parent_actor;

      if (CLUTTER_IS_CONTAINER (parent))
        clutter_container_remove_actor (CLUTTER_CONTAINER (parent), self);
      else
        priv->parent_actor = NULL;
    }

  /* parent should be gone */
  g_assert (priv->parent_actor == NULL);

  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL))
    {
      /* can't be mapped or realized with no parent */
      g_assert (!CLUTTER_ACTOR_IS_MAPPED (self));
      g_assert (!CLUTTER_ACTOR_IS_REALIZED (self));
    }

  destroy_shader_data (self);

  if (priv->pango_context)
    {
      g_object_unref (priv->pango_context);
      priv->pango_context = NULL;
    }

  g_signal_emit (self, actor_signals[DESTROY], 0);

  G_OBJECT_CLASS (clutter_actor_parent_class)->dispose (object);
}

static void
clutter_actor_finalize (GObject *object)
{
  ClutterActor *actor = CLUTTER_ACTOR (object);

  CLUTTER_NOTE (MISC, "Finalize object (id=%d) of type '%s'",
		actor->priv->id,
		g_type_name (G_OBJECT_TYPE (actor)));

  g_free (actor->priv->name);
  clutter_id_pool_remove (CLUTTER_CONTEXT()->id_pool, actor->priv->id);

  G_OBJECT_CLASS (clutter_actor_parent_class)->finalize (object);
}

static void
clutter_actor_class_init (ClutterActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = clutter_actor_set_property;
  object_class->get_property = clutter_actor_get_property;
  object_class->dispose      = clutter_actor_dispose;
  object_class->finalize     = clutter_actor_finalize;

  g_type_class_add_private (klass, sizeof (ClutterActorPrivate));

  /**
   * ClutterActor:x:
   *
   * X coordinate of the actor in pixels. If written, forces a fixed
   * position for the actor. If read, returns the fixed position if any,
   * otherwise the allocation if available, otherwise 0.
   */
  pspec = g_param_spec_float ("x",
                              "X coordinate",
                              "X coordinate of the actor",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_X, pspec);

  /**
   * ClutterActor:y:
   *
   * Y coordinate of the actor in pixels. If written, forces a fixed
   * position for the actor.  If read, returns the fixed position if
   * any, otherwise the allocation if available, otherwise 0.
   */
  pspec = g_param_spec_float ("y",
                              "Y coordinate",
                              "Y coordinate of the actor",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_Y, pspec);

  /**
   * ClutterActor:width:
   *
   * Width of the actor (in pixels). If written, forces the minimum and
   * natural size request of the actor to the given width. If read, returns
   * the allocated width if available, otherwise the width request.
   */
  pspec = g_param_spec_float ("width",
                              "Width",
                              "Width of the actor",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_WIDTH, pspec);
  /**
   * ClutterActor:height:
   *
   * Height of the actor (in pixels).  If written, forces the minimum and
   * natural size request of the actor to the given height. If read, returns
   * the allocated height if available, otherwise the height request.
   */
  pspec = g_param_spec_float ("height",
                              "Height",
                              "Height of the actor",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_HEIGHT, pspec);

  /**
   * ClutterActor:fixed-x:
   *
   * The fixed X position of the actor in pixels.
   *
   * Writing this property sets #ClutterActor:fixed-position-set
   * property as well, as a side effect
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("fixed-x",
                              "Fixed X",
                              "Forced X position of the actor",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FIXED_X, pspec);

  /**
   * ClutterActor:fixed-y:
   *
   * The fixed Y position of the actor in pixels.
   *
   * Writing this property sets the #ClutterActor:fixed-position-set
   * property as well, as a side effect
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("fixed-y",
                              "Fixed Y",
                              "Forced Y position of the actor",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FIXED_Y, pspec);

  /**
   * ClutterActor:fixed-position-set:
   *
   * This flag controls whether the #ClutterActor:fixed-x and
   * #ClutterActor:fixed-y properties are used
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boolean ("fixed-position-set",
                                "Fixed position set",
                                "Whether to use fixed positioning "
                                "for the actor",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_FIXED_POSITION_SET,
                                   pspec);

  /**
   * ClutterActor:min-width:
   *
   * A forced minimum width request for the actor, in pixels
   *
   * Writing this property sets the #ClutterActor:min-width-set property
   * as well, as a side effect.
   *
   *This property overrides the usual width request of the actor.
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("min-width",
                              "Min Width",
                              "Forced minimum width request for the actor",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MIN_WIDTH, pspec);

  /**
   * ClutterActor:min-height:
   *
   * A forced minimum height request for the actor, in pixels
   *
   * Writing this property sets the #ClutterActor:min-height-set property
   * as well, as a side effect. This property overrides the usual height
   * request of the actor.
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("min-height",
                              "Min Height",
                              "Forced minimum height request for the actor",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MIN_HEIGHT, pspec);

  /**
   * ClutterActor:natural-width:
   *
   * A forced natural width request for the actor, in pixels
   *
   * Writing this property sets the #ClutterActor:natural-width-set
   * property as well, as a side effect. This property overrides the
   * usual width request of the actor
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("natural-width",
                              "Natural Width",
                              "Forced natural width request for the actor",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_NATURAL_WIDTH, pspec);

  /**
   * ClutterActor:natural-height:
   *
   * A forced natural height request for the actor, in pixels
   *
   * Writing this property sets the #ClutterActor:natural-height-set
   * property as well, as a side effect. This property overrides the
   * usual height request of the actor
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("natural-height",
                              "Natural Height",
                              "Forced natural height request for the actor",
                              0.0, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_NATURAL_HEIGHT, pspec);

  /**
   * ClutterActor:min-width-set:
   *
   * This flag controls whether the #ClutterActor:min-width property
   * is used
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boolean ("min-width-set",
                                "Minimum width set",
                                "Whether to use the min-width property",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MIN_WIDTH_SET, pspec);

  /**
   * ClutterActor:min-height-set:
   *
   * This flag controls whether the #ClutterActor:min-height property
   * is used
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boolean ("min-height-set",
                                "Minimum height set",
                                "Whether to use the min-height property",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_MIN_HEIGHT_SET, pspec);

  /**
   * ClutterActor:natural-width-set:
   *
   * This flag controls whether the #ClutterActor:natural-width property
   * is used
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boolean ("natural-width-set",
                                "Natural width set",
                                "Whether to use the natural-width property",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_NATURAL_WIDTH_SET,
                                   pspec);

  /**
   * ClutterActor:natural-height-set:
   *
   * This flag controls whether the #ClutterActor:natural-height property
   * is used
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boolean ("natural-height-set",
                                "Natural height set",
                                "Whether to use the natural-height property",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_NATURAL_HEIGHT_SET,
                                   pspec);

  /**
   * ClutterActor:allocation:
   *
   * The allocation for the actor, in pixels
   *
   * This is property is read-only, but you might monitor it to know when an
   * actor moves or resizes
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boxed ("allocation",
                              "Allocation",
                              "The actor's allocation",
                              CLUTTER_TYPE_ACTOR_BOX,
                              CLUTTER_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_ALLOCATION, pspec);

  /**
   * ClutterActor:request-mode:
   *
   * Request mode for the #ClutterActor. The request mode determines the
   * type of geometry management used by the actor, either height for width
   * (the default) or width for height.
   *
   * For actors implementing height for width, the parent container should get
   * the preferred width first, and then the preferred height for that width.
   *
   * For actors implementing width for height, the parent container should get
   * the preferred height first, and then the preferred width for that height.
   *
   * For instance:
   *
   * |[
   *   ClutterRequestMode mode;
   *   gfloat natural_width, min_width;
   *   gfloat natural_height, min_height;
   *
   *   g_object_get (G_OBJECT (child), "request-mode", &amp;mode, NULL);
   *   if (mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
   *     {
   *       clutter_actor_get_preferred_width (child, -1,
   *                                          &amp;min_width,
   *                                          &amp;natural_width);
   *       clutter_actor_get_preferred_height (child, natural_width,
   *                                           &amp;min_height,
   *                                           &amp;natural_height);
   *     }
   *   else
   *     {
   *       clutter_actor_get_preferred_height (child, -1,
   *                                           &amp;min_height,
   *                                           &amp;natural_height);
   *       clutter_actor_get_preferred_width (child, natural_height,
   *                                          &amp;min_width,
   *                                          &amp;natural_width);
   *     }
   * ]|
   *
   * will retrieve the minimum and natural width and height depending on the
   * preferred request mode of the #ClutterActor "child".
   *
   * The clutter_actor_get_preferred_size() function will implement this
   * check for you.
   *
   * Since: 0.8
   */
  pspec = g_param_spec_enum ("request-mode",
                             "Request Mode",
                             "The actor's request mode",
                             CLUTTER_TYPE_REQUEST_MODE,
                             CLUTTER_REQUEST_HEIGHT_FOR_WIDTH,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_REQUEST_MODE, pspec);

  /**
   * ClutterActor:depth:
   *
   * The position of the actor on the Z axis
   *
   * Since: 0.6
   */
  pspec = g_param_spec_float ("depth",
                              "Depth",
                              "Position on the Z axis",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DEPTH, pspec);

  /**
   * ClutterActor:opacity:
   *
   * Opacity of the actor, between 0 (fully transparent) and
   * 255 (fully opaque)
   */
  pspec = g_param_spec_uchar ("opacity",
                              "Opacity",
                              "Opacity of actor",
                              0, 255,
                              255,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_OPACITY, pspec);

  /**
   * ClutterActor:visible:
   *
   * Whether the actor is set to be visible or not
   *
   * See also #ClutterActor:mapped
   */
  pspec = g_param_spec_boolean ("visible",
                                "Visible",
                                "Whether the actor is visible or not",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_VISIBLE, pspec);

  /**
   * ClutterActor:mapped:
   *
   * Whether the actor is mapped (will be painted when the stage
   * to which it belongs is mapped)
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("mapped",
                                "Mapped",
                                "Whether the actor will be painted",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_MAPPED, pspec);

  /**
   * ClutterActor:realized:
   *
   * Whether the actor has been realized
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("realized",
                                "Realized",
                                "Whether the actor has been realized",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_REALIZED, pspec);

  /**
   * ClutterActor:reactive:
   *
   * Whether the actor is reactive to events or not
   *
   * Only reactive actors will emit event-related signals
   *
   * Since: 0.6
   */
  pspec = g_param_spec_boolean ("reactive",
                                "Reactive",
                                "Whether the actor is reactive to events",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_REACTIVE, pspec);

  /**
   * ClutterActor:has-clip:
   *
   * Whether the actor has the #ClutterActor:clip property set or not
   */
  pspec = g_param_spec_boolean ("has-clip",
                                "Has Clip",
                                "Whether the actor has a clip set",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_HAS_CLIP, pspec);

  /**
   * ClutterActor:clip:
   *
   * The clip region for the actor, in actor-relative coordinates
   *
   * Every part of the actor outside the clip region will not be
   * painted
   */
  pspec = g_param_spec_boxed ("clip",
                              "Clip",
                              "The clip region for the actor",
                              CLUTTER_TYPE_GEOMETRY,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CLIP, pspec);

  /**
   * ClutterActor:name:
   *
   * The name of the actor
   *
   * Since: 0.2
   */
  pspec = g_param_spec_string ("name",
                               "Name",
                               "Name of the actor",
                               NULL,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_NAME, pspec);

  /**
   * ClutterActor:scale-x:
   *
   * The horizontal scale of the actor
   *
   * Since: 0.6
   */
  pspec = g_param_spec_double ("scale-x",
                               "Scale X",
                               "Scale factor on the X axis",
                               0.0, G_MAXDOUBLE,
                               1.0,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SCALE_X, pspec);

  /**
   * ClutterActor:scale-y:
   *
   * The vertical scale of the actor
   *
   * Since: 0.6
   */
  pspec = g_param_spec_double ("scale-y",
                               "Scale Y",
                               "Scale factor on the Y axis",
                               0.0, G_MAXDOUBLE,
                               1.0,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SCALE_Y, pspec);

  /**
   * ClutterActor:scale-center-x:
   *
   * The horizontal center point for scaling
   *
   * Since: 1.0
   */
  pspec = g_param_spec_float ("scale-center-x",
                              "Scale-Center-X",
                              "Horizontal scale center",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SCALE_CENTER_X, pspec);

  /**
   * ClutterActor:scale-center-y:
   *
   * The vertical center point for scaling
   *
   * Since: 1.0
   */
  pspec = g_param_spec_float ("scale-center-y",
                              "Scale-Center-Y",
                              "Vertical scale center",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SCALE_CENTER_Y, pspec);

  /**
   * ClutterActor:scale-gravity:
   *
   * The center point for scaling expressed as a #ClutterGravity
   *
   * Since: 1.0
   */
  pspec = g_param_spec_enum ("scale-gravity",
                             "Scale-Gravity",
                             "The center of scaling",
                             CLUTTER_TYPE_GRAVITY,
                             CLUTTER_GRAVITY_NONE,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_SCALE_GRAVITY,
                                   pspec);

  /**
   * ClutterActor:rotation-angle-x:
   *
   * The rotation angle on the X axis
   *
   * Since: 0.6
   */
  pspec = g_param_spec_double ("rotation-angle-x",
                               "Rotation Angle X",
                               "The rotation angle on the X axis",
                               -G_MAXDOUBLE, G_MAXDOUBLE,
                               0.0,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ROTATION_ANGLE_X, pspec);

  /**
   * ClutterActor:rotation-angle-y:
   *
   * The rotation angle on the Y axis
   *
   * Since: 0.6
   */
  pspec = g_param_spec_double ("rotation-angle-y",
                               "Rotation Angle Y",
                               "The rotation angle on the Y axis",
                               -G_MAXDOUBLE, G_MAXDOUBLE,
                               0.0,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ROTATION_ANGLE_Y, pspec);

  /**
   * ClutterActor:rotation-angle-z:
   *
   * The rotation angle on the Z axis
   *
   * Since: 0.6
   */
  pspec = g_param_spec_double ("rotation-angle-z",
                               "Rotation Angle Z",
                               "The rotation angle on the Z axis",
                               -G_MAXDOUBLE, G_MAXDOUBLE,
                               0.0,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ROTATION_ANGLE_Z, pspec);

  /**
   * ClutterActor:rotation-center-x:
   *
   * The rotation center on the X axis.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_boxed ("rotation-center-x",
                              "Rotation Center X",
                              "The rotation center on the X axis",
                              CLUTTER_TYPE_VERTEX,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_ROTATION_CENTER_X,
                                   pspec);

  /**
   * ClutterActor:rotation-center-y:
   *
   * The rotation center on the Y axis.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_boxed ("rotation-center-y",
                              "Rotation Center Y",
                              "The rotation center on the Y axis",
                              CLUTTER_TYPE_VERTEX,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_ROTATION_CENTER_Y,
                                   pspec);

  /**
   * ClutterActor:rotation-center-z:
   *
   * The rotation center on the Z axis.
   *
   * Since: 0.6
   */
  pspec = g_param_spec_boxed ("rotation-center-z",
                              "Rotation Center Z",
                              "The rotation center on the Z axis",
                              CLUTTER_TYPE_VERTEX,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_ROTATION_CENTER_Z,
                                   pspec);

  /**
   * ClutterActor:rotation-center-z-gravity:
   *
   * The rotation center on the Z axis expressed as a #ClutterGravity.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_enum ("rotation-center-z-gravity",
                             "Rotation-Center-Z-Gravity",
                             "Center point for rotation around the Z axis",
                             CLUTTER_TYPE_GRAVITY,
                             CLUTTER_GRAVITY_NONE,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_ROTATION_CENTER_Z_GRAVITY,
                                   pspec);

  /**
   * ClutterActor:anchor-x:
   *
   * The X coordinate of an actor's anchor point, relative to
   * the actor coordinate space, in pixels
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("anchor-x",
                              "Anchor X",
                              "X coordinate of the anchor point",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ANCHOR_X, pspec);

  /**
   * ClutterActor:anchor-y:
   *
   * The Y coordinate of an actor's anchor point, relative to
   * the actor coordinate space, in pixels
   *
   * Since: 0.8
   */
  pspec = g_param_spec_float ("anchor-y",
                              "Anchor Y",
                              "Y coordinate of the anchor point",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ANCHOR_Y, pspec);

  /**
   * ClutterActor:anchor-gravity:
   *
   * The anchor point expressed as a #ClutterGravity
   *
   * Since: 1.0
   */
  pspec = g_param_spec_enum ("anchor-gravity",
                             "Anchor-Gravity",
                             "The anchor point as a ClutterGravity",
                             CLUTTER_TYPE_GRAVITY,
                             CLUTTER_GRAVITY_NONE,
                             CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_ANCHOR_GRAVITY, pspec);

  /**
   * ClutterActor:show-on-set-parent:
   *
   * If %TRUE, the actor is automatically shown when parented.
   *
   * Calling clutter_actor_hide() on an actor which has not been
   * parented will set this property to %FALSE as a side effect.
   *
   * Since: 0.8
   */
  pspec = g_param_spec_boolean ("show-on-set-parent",
                                "Show on set parent",
                                "Whether the actor is shown when parented",
                                TRUE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_SHOW_ON_SET_PARENT,
                                   pspec);

  /**
   * ClutterActor:clip-to-allocation:
   *
   * Whether the clip region should track the allocated area
   * of the actor.
   *
   * This property is ignored if a clip area has been explicitly
   * set using clutter_actor_set_clip().
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("clip-to-allocation",
                                "Clip to Allocation",
                                "Sets the clip region to track the "
                                "actor's allocation",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_CLIP_TO_ALLOCATION,
                                   pspec);

  /**
   * ClutterActor::destroy:
   * @actor: the object which received the signal
   *
   * The ::destroy signal is emitted when an actor is destroyed,
   * either by direct invocation of clutter_actor_destroy() or
   * when the #ClutterGroup that contains the actor is destroyed.
   *
   * Since: 0.2
   */
  actor_signals[DESTROY] =
    g_signal_new (I_("destroy"),
		  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (ClutterActorClass, destroy),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterActor::show:
   * @actor: the object which received the signal
   *
   * The ::show signal is emitted when an actor is visible and
   * rendered on the stage.
   *
   * Since: 0.2
   */
  actor_signals[SHOW] =
    g_signal_new (I_("show"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterActorClass, show),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterActor::hide:
   * @actor: the object which received the signal
   *
   * The ::hide signal is emitted when an actor is no longer rendered
   * on the stage.
   *
   * Since: 0.2
   */
  actor_signals[HIDE] =
    g_signal_new (I_("hide"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterActorClass, hide),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterActor::parent-set:
   * @actor: the object which received the signal
   * @old_parent: the previous parent of the actor, or %NULL
   *
   * This signal is emitted when the parent of the actor changes.
   *
   * Since: 0.2
   */
  actor_signals[PARENT_SET] =
    g_signal_new (I_("parent-set"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterActorClass, parent_set),
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterActor::queue-redraw:
   * @actor: the actor we're bubbling the redraw request through
   * @origin: the actor which initiated the redraw request
   *
   * The ::queue_redraw signal is emitted when clutter_actor_queue_redraw()
   * is called on @origin.
   *
   * The default implementation for #ClutterActor chains up to the
   * parent actor and queues a redraw on the parent, thus "bubbling"
   * the redraw queue up through the actor graph. The default
   * implementation for #ClutterStage queues a clutter_redraw() in a
   * main loop idle handler.
   *
   * Note that the @origin actor may be the stage, or a container; it
   * does not have to be a leaf node in the actor graph.
   *
   * Toolkits embedding a #ClutterStage which require a redraw and
   * relayout cycle can stop the emission of this signal using the
   * GSignal API, redraw the UI and then call clutter_redraw()
   * themselves, like:
   *
   * |[
   *   static void
   *   on_redraw_complete (void)
   *   {
   *     /&ast; execute the Clutter drawing pipeline &ast;/
   *     clutter_redraw ();
   *   }
   *
   *   static void
   *   on_stage_queue_redraw (ClutterStage *stage)
   *   {
   *     /&ast; this prevents the default handler to run &ast;/
   *     g_signal_stop_emission_by_name (stage, "queue-redraw");
   *
   *     /&ast; queue a redraw with the host toolkit and call
   *      &ast; a function when the redraw has been completed
   *      &ast;/
   *     queue_a_redraw (G_CALLBACK (on_redraw_complete));
   *   }
   * ]|
   *
   * <note><para>This signal is emitted before the Clutter paint
   * pipeline is executed. If you want to know when the pipeline has
   * been completed you should connect to the ::paint signal on the
   * Stage with g_signal_connect_after().</para></note>
   *
   * Since: 1.0
   */
  actor_signals[QUEUE_REDRAW] =
    g_signal_new (I_("queue-redraw"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, queue_redraw),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterActor::event:
   * @actor: the actor which received the event
   * @event: a #ClutterEvent
   *
   * The ::event signal is emitted each time an event is received
   * by the @actor. This signal will be emitted on every actor,
   * following the hierarchy chain, until it reaches the top-level
   * container (the #ClutterStage).
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[EVENT] =
    g_signal_new (I_("event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::button-press-event:
   * @actor: the actor which received the event
   * @event: a #ClutterButtonEvent
   *
   * The ::button-press-event signal is emitted each time a mouse button
   * is pressed on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[BUTTON_PRESS_EVENT] =
    g_signal_new (I_("button-press-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, button_press_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::button-release-event:
   * @actor: the actor which received the event
   * @event: a #ClutterButtonEvent
   *
   * The ::button-release-event signal is emitted each time a mouse button
   * is released on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new (I_("button-release-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, button_release_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::scroll-event:
   * @actor: the actor which received the event
   * @event: a #ClutterScrollEvent
   *
   * The ::scroll-event signal is emitted each time the mouse is
   * scrolled on @actor
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[SCROLL_EVENT] =
    g_signal_new (I_("scroll-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, scroll_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::key-press-event:
   * @actor: the actor which received the event
   * @event: a #ClutterKeyEvent
   *
   * The ::key-press-event signal is emitted each time a keyboard button
   * is pressed while @actor has key focus (see clutter_stage_set_key_focus()).
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[KEY_PRESS_EVENT] =
    g_signal_new (I_("key-press-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, key_press_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::key-release-event:
   * @actor: the actor which received the event
   * @event: a #ClutterKeyEvent
   *
   * The ::key-release-event signal is emitted each time a keyboard button
   * is released while @actor has key focus (see
   * clutter_stage_set_key_focus()).
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[KEY_RELEASE_EVENT] =
    g_signal_new (I_("key-release-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, key_release_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::motion-event:
   * @actor: the actor which received the event
   * @event: a #ClutterMotionEvent
   *
   * The ::motion-event signal is emitted each time the mouse pointer is
   * moved over @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[MOTION_EVENT] =
    g_signal_new (I_("motion-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, motion_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::key-focus-in:
   * @actor: the actor which now has key focus
   *
   * The ::focus-in signal is emitted when @actor recieves key focus.
   *
   * Since: 0.6
   */
  actor_signals[KEY_FOCUS_IN] =
    g_signal_new (I_("key-focus-in"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, key_focus_in),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ClutterActor::key-focus-out:
   * @actor: the actor which now has key focus
   *
   * The ::key-focus-out signal is emitted when @actor loses key focus.
   *
   * Since: 0.6
   */
  actor_signals[KEY_FOCUS_OUT] =
    g_signal_new (I_("key-focus-out"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, key_focus_out),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ClutterActor::enter-event:
   * @actor: the actor which the pointer has entered.
   * @event: a #ClutterCrossingEvent
   *
   * The ::enter-event signal is emitted when the pointer enters the @actor
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[ENTER_EVENT] =
    g_signal_new (I_("enter-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, enter_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::leave-event:
   * @actor: the actor which the pointer has left
   * @event: a #ClutterCrossingEvent
   *
   * The ::leave-event signal is emitted when the pointer leaves the @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[LEAVE_EVENT] =
    g_signal_new (I_("leave-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, leave_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::captured-event:
   * @actor: the actor which received the signal
   * @event: a #ClutterEvent
   *
   * The ::captured-event signal is emitted when an event is captured
   * by Clutter. This signal will be emitted starting from the top-level
   * container (the #ClutterStage) to the actor which received the event
   * going down the hierarchy. This signal can be used to intercept every
   * event before the specialized events (like
   * ClutterActor::button-press-event or ::key-released-event) are
   * emitted.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[CAPTURED_EVENT] =
    g_signal_new (I_("captured-event"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, captured_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::paint:
   * @actor: the #ClutterActor that received the signal
   *
   * The ::paint signal is emitted each time an actor is being painted.
   *
   * Subclasses of #ClutterActor should override the class signal handler
   * and paint themselves in that function.
   *
   * It is possible to connect a handler to the ::paint signal in order
   * to set up some custom aspect of a paint.
   *
   * Since: 0.8
   */
  actor_signals[PAINT] =
    g_signal_new (I_("paint"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterActorClass, paint),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * ClutterActor::realize:
   * @actor: the #ClutterActor that received the signal
   *
   * The ::realize signal is emitted each time an actor is being
   * realized.
   *
   * Since: 0.8
   */
  actor_signals[REALIZE] =
    g_signal_new (I_("realize"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterActorClass, realize),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * ClutterActor::unrealize:
   * @actor: the #ClutterActor that received the signal
   *
   * The ::unrealize signal is emitted each time an actor is being
   * unrealized.
   *
   * Since: 0.8
   */
  actor_signals[UNREALIZE] =
    g_signal_new (I_("unrealize"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterActorClass, unrealize),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterActor::map:
   * @actor: the #ClutterActor to map
   *
   * The ::map virtual functon must be overridden in order to call
   * clutter_actor_map() on any child actors if the actor is not a
   * #ClutterContainer.  When overriding, it is mandatory to chain up
   * to the parent implementation.
   *
   * Since: 1.0
   */

  /**
   * ClutterActor::unmap:
   * @actor: the #ClutterActor to unmap
   *
   * The ::unmap virtual functon must be overridden in order to call
   * clutter_actor_unmap() on any child actors if the actor is not a
   * #ClutterContainer.  When overriding, it is mandatory to chain up
   * to the parent implementation.
   *
   * Since: 1.0
   */

  /**
   * ClutterActor::pick:
   * @actor: the #ClutterActor that received the signal
   * @color: the #ClutterColor to be used when picking
   *
   * The ::pick signal is emitted each time an actor is being painted
   * in "pick mode". The pick mode is used to identify the actor during
   * the event handling phase, or by clutter_stage_get_actor_at_pos().
   * The actor should paint its shape using the passed @pick_color.
   *
   * Subclasses of #ClutterActor should override the class signal handler
   * and paint themselves in that function.
   *
   * It is possible to connect a handler to the ::pick signal in order
   * to set up some custom aspect of a paint in pick mode.
   *
   * Since: 1.0
   */
  actor_signals[PICK] =
    g_signal_new (I_("pick"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterActorClass, pick),
                  NULL, NULL,
                  clutter_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_COLOR);

  /**
   * ClutterActor::allocation-changed:
   * @actor: the #ClutterActor that emitted the signal
   * @box: a #ClutterActorBox with the new allocation
   * @flags: #ClutterAllocationFlags for the allocation
   *
   * The ::allocation-changed signal is emitted when the
   * #ClutterActor:allocation property changes. Usually, application
   * code should just use the notifications for the :allocation property
   * but if you want to track the allocation flags as well, for instance
   * to know whether the absolute origin of @actor changed, then you might
   * want use this signal instead.
   *
   * Since: 1.0
   */
  actor_signals[ALLOCATION_CHANGED] =
    g_signal_new (I_("allocation-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  clutter_marshal_VOID__BOXED_FLAGS,
                  G_TYPE_NONE, 2,
                  CLUTTER_TYPE_ACTOR_BOX,
                  CLUTTER_TYPE_ALLOCATION_FLAGS);

  klass->show = clutter_actor_real_show;
  klass->show_all = clutter_actor_show;
  klass->hide = clutter_actor_real_hide;
  klass->hide_all = clutter_actor_hide;
  klass->map = clutter_actor_real_map;
  klass->unmap = clutter_actor_real_unmap;
  klass->unrealize = clutter_actor_real_unrealize;
  klass->pick = clutter_actor_real_pick;
  klass->get_preferred_width = clutter_actor_real_get_preferred_width;
  klass->get_preferred_height = clutter_actor_real_get_preferred_height;
  klass->allocate = clutter_actor_real_allocate;
  klass->queue_redraw = clutter_actor_real_queue_redraw;
}

static void
clutter_actor_init (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  self->priv = priv = CLUTTER_ACTOR_GET_PRIVATE (self);

  priv->parent_actor = NULL;
  priv->has_clip     = FALSE;
  priv->opacity      = 0xff;
  priv->id           = clutter_id_pool_add (CLUTTER_CONTEXT()->id_pool, self);
  priv->scale_x      = 1.0;
  priv->scale_y      = 1.0;
  priv->shader_data  = NULL;
  priv->show_on_set_parent = TRUE;

  priv->needs_width_request  = TRUE;
  priv->needs_height_request = TRUE;
  priv->needs_allocation     = TRUE;

  priv->opacity_parent = NULL;
  priv->enable_model_view_transform = TRUE;

  memset (priv->clip, 0, sizeof (gfloat) * 4);
}

/**
 * clutter_actor_destroy:
 * @self: a #ClutterActor
 *
 * Destroys an actor.  When an actor is destroyed, it will break any
 * references it holds to other objects.  If the actor is inside a
 * container, the actor will be removed.
 *
 * When you destroy a container, its children will be destroyed as well.
 *
 * Note: you cannot destroy the #ClutterStage returned by
 * clutter_stage_get_default().
 */
void
clutter_actor_destroy (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_ref (self);

  /* avoid recursion while destroying */
  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_DESTRUCTION))
    {
      CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_DESTRUCTION);

      /* if we are destroying we want to unrealize ourselves
       * first before the dispose run removes the parent
       */
      if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL))
        clutter_actor_update_map_state (self, MAP_STATE_MAKE_UNREALIZED);

      g_object_run_dispose (G_OBJECT (self));

      CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_DESTRUCTION);
    }

  g_object_unref (self);
}

/**
 * clutter_actor_queue_redraw:
 * @self: A #ClutterActor
 *
 * Queues up a redraw of an actor and any children. The redraw occurs
 * once the main loop becomes idle (after the current batch of events
 * has been processed, roughly).
 *
 * Applications rarely need to call this, as redraws are handled
 * automatically by modification functions.
 *
 * This function will not do anything if @self is not visible, or
 * if the actor is inside an invisible part of the scenegraph.
 *
 * Also be aware that painting is a NOP for actors with an opacity of
 * 0
 */
void
clutter_actor_queue_redraw (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_queue_redraw_with_origin (self, self);
}

/**
 * clutter_actor_queue_relayout:
 * @self: A #ClutterActor
 *
 * Indicates that the actor's size request or other layout-affecting
 * properties may have changed. This function is used inside #ClutterActor
 * subclass implementations, not by applications directly.
 *
 * Queueing a new layout automatically queues a redraw as well.
 *
 * Since: 0.8
 */
void
clutter_actor_queue_relayout (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  priv = self->priv;

  if (priv->needs_width_request &&
      priv->needs_height_request &&
      priv->needs_allocation)
    return; /* save some cpu cycles */

  priv->needs_width_request  = TRUE;
  priv->needs_height_request = TRUE;
  priv->needs_allocation     = TRUE;

  /* always repaint also (no-op if not mapped) */
  clutter_actor_queue_redraw (self);

  /* We need to go all the way up the hierarchy */
  if (priv->parent_actor)
    clutter_actor_queue_relayout (priv->parent_actor);
}

/**
 * clutter_actor_get_preferred_size:
 * @self: a #ClutterActor
 * @min_width_p: (out) (allow-none): return location for the minimum width, or %NULL
 * @min_height_p: (out) (allow-none): return location for the minimum height, or %NULL
 * @natural_width_p: (out) (allow-none): return location for the natural width, or %NULL
 * @natural_height_p: (out) (allow-none): return location for the natural height, or %NULL
 *
 * Computes the preferred minimum and natural size of an actor, taking into
 * account the actor's geometry management (either height-for-width
 * or width-for-height).
 *
 * The width and height used to compute the preferred height and preferred
 * width are the actor's natural ones.
 *
 * If you need to control the height for the preferred width, or the width for
 * the preferred height, you should use clutter_actor_get_preferred_width()
 * and clutter_actor_get_preferred_height(), and check the actor's preferred
 * geometry management using the #ClutterActor:request-mode property.
 *
 * Since: 0.8
 */
void
clutter_actor_get_preferred_size (ClutterActor *self,
                                  gfloat       *min_width_p,
                                  gfloat       *min_height_p,
                                  gfloat       *natural_width_p,
                                  gfloat       *natural_height_p)
{
  ClutterActorPrivate *priv;
  gfloat for_width, for_height;
  gfloat min_width, min_height;
  gfloat natural_width, natural_height;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  for_width = for_height = 0;
  min_width = min_height = 0;
  natural_width = natural_height = 0;

  if (priv->request_mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
    {
      CLUTTER_NOTE (LAYOUT, "Preferred size (height-for-width)");
      clutter_actor_get_preferred_width (self, -1,
                                         &min_width,
                                         &natural_width);
      clutter_actor_get_preferred_height (self, natural_width,
                                          &min_height,
                                          &natural_height);
    }
  else
    {
      CLUTTER_NOTE (LAYOUT, "Preferred size (width-for-height)");
      clutter_actor_get_preferred_height (self, -1,
                                          &min_height,
                                          &natural_height);
      clutter_actor_get_preferred_width (self, natural_height,
                                         &min_width,
                                         &natural_width);
    }

  if (min_width_p)
    *min_width_p = min_width;

  if (min_height_p)
    *min_height_p = min_height;

  if (natural_width_p)
    *natural_width_p = natural_width;

  if (natural_height_p)
    *natural_height_p = natural_height;
}

/**
 * clutter_actor_get_preferred_width:
 * @self: A #ClutterActor
 * @for_height: available height when computing the preferred width,
 *   or a negative value to indicate that no height is defined
 * @min_width_p: (out): return location for minimum width, or %NULL
 * @natural_width_p: (out): return location for the natural width, or %NULL
 *
 * Computes the requested minimum and natural widths for an actor,
 * optionally depending on the specified height, or if they are
 * already computed, returns the cached values.
 *
 * An actor may not get its request - depending on the layout
 * manager that's in effect.
 *
 * A request should not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * Since: 0.8
 */
void
clutter_actor_get_preferred_width (ClutterActor *self,
                                   gfloat        for_height,
                                   gfloat       *min_width_p,
                                   gfloat       *natural_width_p)
{
  ClutterActorClass *klass;
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  priv = self->priv;

  if (priv->needs_width_request ||
      priv->request_width_for_height != for_height)
    {
      gfloat min_width, natural_width;

      min_width = natural_width = 0;

      CLUTTER_NOTE (LAYOUT, "Width request for %.2f px", for_height);

      klass->get_preferred_width (self, for_height,
                                  &min_width,
                                  &natural_width);

      /* Due to accumulated float errors, it's better not to warn
       * on this, but just fix it.
       */
      if (natural_width < min_width)
	natural_width = min_width;

      if (!priv->min_width_set)
        priv->request_min_width = min_width;

      if (!priv->natural_width_set)
        priv->request_natural_width = natural_width;

      priv->request_width_for_height = for_height;
      priv->needs_width_request = FALSE;
    }

  if (min_width_p)
    *min_width_p = priv->request_min_width;

  if (natural_width_p)
    *natural_width_p = priv->request_natural_width;
}

/**
 * clutter_actor_get_preferred_height:
 * @self: A #ClutterActor
 * @for_width: available width to assume in computing desired height,
 *   or a negative value to indicate that no width is defined
 * @min_height_p: (out): return location for minimum height, or %NULL
 * @natural_height_p: (out): return location for natural height, or %NULL
 *
 * Computes the requested minimum and natural heights for an actor,
 * or if they are already computed, returns the cached values.
 *
 * An actor may not get its request - depending on the layout
 * manager that's in effect.
 *
 * A request should not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * Since: 0.8
 */
void
clutter_actor_get_preferred_height (ClutterActor *self,
                                    gfloat        for_width,
                                    gfloat       *min_height_p,
                                    gfloat       *natural_height_p)
{
  ClutterActorClass *klass;
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  priv = self->priv;

  if (priv->needs_height_request ||
      priv->request_height_for_width != for_width)
    {
      gfloat min_height, natural_height;

      min_height = natural_height = 0;

      CLUTTER_NOTE (LAYOUT, "Width request for %.2f px", for_width);

      klass->get_preferred_height (self, for_width,
                                   &min_height,
                                   &natural_height);

      /* Due to accumulated float errors, it's better not to warn
       * on this, but just fix it.
       */
      if (natural_height < min_height)
	natural_height = min_height;

      if (!priv->min_height_set)
        priv->request_min_height = min_height;

      if (!priv->natural_height_set)
        priv->request_natural_height = natural_height;

      priv->request_height_for_width = for_width;
      priv->needs_height_request = FALSE;
    }

  if (min_height_p)
    *min_height_p = priv->request_min_height;

  if (natural_height_p)
    *natural_height_p = priv->request_natural_height;
}

/**
 * clutter_actor_get_allocation_coords:
 * @self: A #ClutterActor
 * @x_1: (out): x1 coordinate
 * @y_1: (out): y1 coordinate
 * @x_2: (out): x2 coordinate
 * @y_2: (out): y2 coordinate
 *
 * Gets the layout box an actor has been assigned.  The allocation can
 * only be assumed valid inside a paint() method; anywhere else, it
 * may be out-of-date.
 *
 * An allocation does not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * The returned coordinates are in pixels.
 *
 * Since: 0.8
 */
void
clutter_actor_get_allocation_coords (ClutterActor  *self,
                                     gint          *x_1,
                                     gint          *y_1,
                                     gint          *x_2,
                                     gint          *y_2)
{
  ClutterActorBox allocation = { 0, };

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_get_allocation_box (self, &allocation);

  if (x_1)
    *x_1 = allocation.x1;

  if (y_1)
    *y_1 = allocation.y1;

  if (x_2)
    *x_2 = allocation.x2;

  if (y_2)
    *y_2 = allocation.y2;
}

/**
 * clutter_actor_get_allocation_box:
 * @self: A #ClutterActor
 * @box: (out): the function fills this in with the actor's allocation
 *
 * Gets the layout box an actor has been assigned. The allocation can
 * only be assumed valid inside a paint() method; anywhere else, it
 * may be out-of-date.
 *
 * An allocation does not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * <note>Do not call any of the clutter_actor_get_allocation_*() family
 * of functions inside the implementation of the get_preferred_width()
 * or get_preferred_height() virtual functions.</note>
 *
 * Since: 0.8
 */
void
clutter_actor_get_allocation_box (ClutterActor    *self,
                                  ClutterActorBox *box)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  /* XXX - if needs_allocation=TRUE, we can either 1) g_return_if_fail,
   * which limits calling get_allocation to inside paint() basically; or
   * we can 2) force a layout, which could be expensive if someone calls
   * get_allocation somewhere silly; or we can 3) just return the latest
   * value, allowing it to be out-of-date, and assume people know what
   * they are doing.
   *
   * The least-surprises approach that keeps existing code working is
   * likely to be 2). People can end up doing some inefficient things,
   * though, and in general code that requires 2) is probably broken.
   */

  /* this implements 2) */
  if (G_UNLIKELY (self->priv->needs_allocation))
    {
      ClutterActor *stage = clutter_actor_get_stage (self);

      /* do not queue a relayout on an unparented actor */
      if (stage)
        _clutter_stage_maybe_relayout (stage);
    }

  /* commenting out the code above and just keeping this assigment
   * implements 3)
   */
  *box = self->priv->allocation;
}

/**
 * clutter_actor_get_allocation_geometry:
 * @self: A #ClutterActor
 * @geom: (out): allocation geometry in pixels
 *
 * Gets the layout box an actor has been assigned.  The allocation can
 * only be assumed valid inside a paint() method; anywhere else, it
 * may be out-of-date.
 *
 * An allocation does not incorporate the actor's scale or anchor point;
 * those transformations do not affect layout, only rendering.
 *
 * The returned rectangle is in pixels.
 *
 * Since: 0.8
 */
void
clutter_actor_get_allocation_geometry (ClutterActor    *self,
                                       ClutterGeometry *geom)
{
  gint x2, y2;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_get_allocation_coords (self, &geom->x, &geom->y, &x2, &y2);

  geom->width = x2 - geom->x;
  geom->height = y2 - geom->y;
}

/**
 * clutter_actor_allocate:
 * @self: A #ClutterActor
 * @box: new allocation of the actor, in parent-relative coordinates
 * @flags: flags that control the allocation
 *
 * Called by the parent of an actor to assign the actor its size.
 * Should never be called by applications (except when implementing
 * a container or layout manager).
 *
 * Actors can know from their allocation box whether they have moved
 * with respect to their parent actor. The @flags parameter describes
 * additional information about the allocation, for instance whether
 * the parent has moved with respect to the stage, for example because
 * a grandparent's origin has moved.
 *
 * Since: 0.8
 */
void
clutter_actor_allocate (ClutterActor           *self,
                        const ClutterActorBox  *box,
                        ClutterAllocationFlags  flags)
{
  ClutterActorPrivate *priv;
  ClutterActorClass *klass;
  gboolean child_moved;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  child_moved = (box->x1 != priv->allocation.x1 ||
                 box->y1 != priv->allocation.y1);

  /* If we get an allocation "out of the blue"
   * (we did not queue relayout), then we want to
   * ignore it. But if we have needs_allocation set,
   * we want to guarantee that allocate() virtual
   * method is always called, i.e. that queue_relayout()
   * always results in an allocate() invocation on
   * an actor.
   *
   * The optimization here is to avoid re-allocating
   * actors that did not queue relayout and were
   * not moved.
   */

  if (!priv->needs_allocation &&
      !(flags & CLUTTER_ABSOLUTE_ORIGIN_CHANGED) &&
      !child_moved &&
      box->x2 == priv->allocation.x2 &&
      box->y2 == priv->allocation.y2)
    {
      CLUTTER_NOTE (LAYOUT, "No allocation needed");
      return;
    }

  /* When ABSOLUTE_ORIGIN_CHANGED is passed in to
   * clutter_actor_allocate(), it indicates whether the parent has its
   * absolute origin moved; when passed in to ClutterActor::allocate()
   * virtual method though, it indicates whether the child has its
   * absolute origin moved.  So we set it when child_moved is TRUE
   */
  if (child_moved)
    flags |= CLUTTER_ABSOLUTE_ORIGIN_CHANGED;

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  klass->allocate (self, box, flags);
}

/**
 * clutter_actor_set_geometry:
 * @self: A #ClutterActor
 * @geometry: A #ClutterGeometry
 *
 * Sets the actor's fixed position and forces its minimum and natural
 * size, in pixels. This means the untransformed actor will have the
 * given geometry. This is the same as calling clutter_actor_set_position()
 * and clutter_actor_set_size().
 */
void
clutter_actor_set_geometry (ClutterActor          *self,
			    const ClutterGeometry *geometry)
{
  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_position (self, geometry->x, geometry->y);
  clutter_actor_set_size (self, geometry->width, geometry->height);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_get_geometry:
 * @self: A #ClutterActor
 * @geometry: (out): A location to store actors #ClutterGeometry
 *
 * Gets the size and position of an actor relative to its parent
 * actor. This is the same as calling clutter_actor_get_position() and
 * clutter_actor_get_size(). It tries to "do what you mean" and get the
 * requested size and position if the actor's allocation is invalid.
 */
void
clutter_actor_get_geometry (ClutterActor    *self,
			    ClutterGeometry *geometry)
{
  gfloat x, y, width, height;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (geometry != NULL);

  clutter_actor_get_position (self, &x, &y);
  clutter_actor_get_size (self, &width, &height);

  geometry->x = (int) x;
  geometry->y = (int) y;
  geometry->width = (int) width;
  geometry->height = (int) height;
}

/**
 * clutter_actor_set_position
 * @self: A #ClutterActor
 * @x: New left position of actor in pixels.
 * @y: New top position of actor in pixels.
 *
 * Sets the actor's fixed position in pixels relative to any parent
 * actor.
 *
 * If a layout manager is in use, this position will override the
 * layout manager and force a fixed position.
 */
void
clutter_actor_set_position (ClutterActor *self,
			    gfloat        x,
			    gfloat        y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_x (self, x);
  clutter_actor_set_y (self, y);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_get_fixed_position_set:
 * @self: A #ClutterActor
 *
 * Checks whether an actor has a fixed position set (and will thus be
 * unaffected by any layout manager).
 *
 * Return value: %TRUE if the fixed position is set on the actor
 *
 * Since: 0.8
 */
gboolean
clutter_actor_get_fixed_position_set (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  return self->priv->position_set;
}

/**
 * clutter_actor_set_fixed_position_set:
 * @self: A #ClutterActor
 * @is_set: whether to use fixed position
 *
 * Sets whether an actor has a fixed position set (and will thus be
 * unaffected by any layout manager).
 *
 * Since: 0.8
 */
void
clutter_actor_set_fixed_position_set (ClutterActor *self,
                                      gboolean      is_set)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (self->priv->position_set == (is_set != FALSE))
    return;

  self->priv->position_set = is_set != FALSE;
  g_object_notify (G_OBJECT (self), "fixed-position-set");

  clutter_actor_queue_relayout (self);
}

/**
 * clutter_actor_move_by:
 * @self: A #ClutterActor
 * @dx: Distance to move Actor on X axis.
 * @dy: Distance to move Actor on Y axis.
 *
 * Moves an actor by the specified distance relative to its current
 * position in pixels.
 *
 * This function modifies the fixed position of an actor and thus removes
 * it from any layout management. Another way to move an actor is with an
 * anchor point, see clutter_actor_set_anchor_point().
 *
 * Since: 0.2
 */
void
clutter_actor_move_by (ClutterActor *self,
		       gfloat        dx,
		       gfloat        dy)
{
  ClutterActorPrivate *priv;
  gfloat x, y;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  x = priv->fixed_x;
  y = priv->fixed_y;

  clutter_actor_set_position (self, x + dx, y + dy);
}

static void
clutter_actor_set_min_width (ClutterActor *self,
                             gfloat        min_width)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  /* if we are setting the size on a top-level actor and the
   * backend only supports static top-levels (e.g. framebuffers)
   * then we ignore the passed value and we override it with
   * the stage implementation's preferred size.
   */
  if ((CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL) &&
      clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC))
    return;

  if (priv->min_width_set && min_width == priv->request_min_width)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_store_old_geometry (self, &old);

  priv->request_min_width = min_width;
  g_object_notify (G_OBJECT (self), "min-width");
  clutter_actor_set_min_width_set (self, TRUE);

  clutter_actor_notify_if_geometry_changed (self, &old);

  g_object_thaw_notify (G_OBJECT (self));

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_min_height (ClutterActor *self,
                              gfloat        min_height)

{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  /* if we are setting the size on a top-level actor and the
   * backend only supports static top-levels (e.g. framebuffers)
   * then we ignore the passed value and we override it with
   * the stage implementation's preferred size.
   */
  if ((CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL) &&
      clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC))
    return;

  if (priv->min_height_set && min_height == priv->request_min_height)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_store_old_geometry (self, &old);

  priv->request_min_height = min_height;
  g_object_notify (G_OBJECT (self), "min-height");
  clutter_actor_set_min_height_set (self, TRUE);

  clutter_actor_notify_if_geometry_changed (self, &old);

  g_object_thaw_notify (G_OBJECT (self));

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_natural_width (ClutterActor *self,
                                 gfloat        natural_width)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  /* if we are setting the size on a top-level actor and the
   * backend only supports static top-levels (e.g. framebuffers)
   * then we ignore the passed value and we override it with
   * the stage implementation's preferred size.
   */
  if ((CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL) &&
      clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC))
    return;

  if (priv->natural_width_set &&
      natural_width == priv->request_natural_width)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_store_old_geometry (self, &old);

  priv->request_natural_width = natural_width;
  g_object_notify (G_OBJECT (self), "natural-width");
  clutter_actor_set_natural_width_set (self, TRUE);

  clutter_actor_notify_if_geometry_changed (self, &old);

  g_object_thaw_notify (G_OBJECT (self));

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_natural_height (ClutterActor *self,
                                  gfloat        natural_height)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  /* if we are setting the size on a top-level actor and the
   * backend only supports static top-levels (e.g. framebuffers)
   * then we ignore the passed value and we override it with
   * the stage implementation's preferred size.
   */
  if ((CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL) &&
      clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC))
    return;

  if (priv->natural_height_set &&
      natural_height == priv->request_natural_height)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_store_old_geometry (self, &old);

  priv->request_natural_height = natural_height;
  g_object_notify (G_OBJECT (self), "natural-height");
  clutter_actor_set_natural_height_set (self, TRUE);

  clutter_actor_notify_if_geometry_changed (self, &old);

  g_object_thaw_notify (G_OBJECT (self));

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_min_width_set (ClutterActor *self,
                                 gboolean      use_min_width)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  if (priv->min_width_set == (use_min_width != FALSE))
    return;

  clutter_actor_store_old_geometry (self, &old);

  priv->min_width_set = use_min_width != FALSE;
  g_object_notify (G_OBJECT (self), "min-width-set");

  clutter_actor_notify_if_geometry_changed (self, &old);

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_min_height_set (ClutterActor *self,
                                  gboolean      use_min_height)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  if (priv->min_height_set == (use_min_height != FALSE))
    return;

  clutter_actor_store_old_geometry (self, &old);

  priv->min_height_set = use_min_height != FALSE;
  g_object_notify (G_OBJECT (self), "min-height-set");

  clutter_actor_notify_if_geometry_changed (self, &old);

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_natural_width_set (ClutterActor *self,
                                     gboolean      use_natural_width)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  if (priv->natural_width_set == (use_natural_width != FALSE))
    return;

  clutter_actor_store_old_geometry (self, &old);

  priv->natural_width_set = use_natural_width != FALSE;
  g_object_notify (G_OBJECT (self), "natural-width-set");

  clutter_actor_notify_if_geometry_changed (self, &old);

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_natural_height_set (ClutterActor *self,
                                      gboolean      use_natural_height)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActorBox old = { 0, };

  if (priv->natural_height_set == (use_natural_height != FALSE))
    return;

  clutter_actor_store_old_geometry (self, &old);

  priv->natural_height_set = use_natural_height != FALSE;
  g_object_notify (G_OBJECT (self), "natural-height-set");

  clutter_actor_notify_if_geometry_changed (self, &old);

  clutter_actor_queue_relayout (self);
}

static void
clutter_actor_set_request_mode (ClutterActor *self,
                                ClutterRequestMode mode)
{
  ClutterActorPrivate *priv = self->priv;

  if (priv->request_mode == mode)
    return;

  priv->request_mode = mode;

  priv->needs_width_request = TRUE;
  priv->needs_height_request = TRUE;

  g_object_notify (G_OBJECT (self), "request-mode");

  clutter_actor_queue_relayout (self);
}

/**
 * clutter_actor_set_size
 * @self: A #ClutterActor
 * @width: New width of actor in pixels, or -1
 * @height: New height of actor in pixels, or -1
 *
 * Sets the actor's size request in pixels. This overrides any
 * "normal" size request the actor would have. For example
 * a text actor might normally request the size of the text;
 * this function would force a specific size instead.
 *
 * If @width and/or @height are -1 the actor will use its
 * "normal" size request instead of overriding it, i.e.
 * you can "unset" the size with -1.
 *
 * This function sets or unsets both the minimum and natural size.
 */
void
clutter_actor_set_size (ClutterActor *self,
			gfloat        width,
			gfloat        height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (width >= 0)
    {
      clutter_actor_set_min_width (self, width);
      clutter_actor_set_natural_width (self, width);
    }
  else
    {
      clutter_actor_set_min_width_set (self, FALSE);
      clutter_actor_set_natural_width_set (self, FALSE);
    }

  if (height >= 0)
    {
      clutter_actor_set_min_height (self, height);
      clutter_actor_set_natural_height (self, height);
    }
  else
    {
      clutter_actor_set_min_height_set (self, FALSE);
      clutter_actor_set_natural_height_set (self, FALSE);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_get_size:
 * @self: A #ClutterActor
 * @width: return location for the width, or %NULL.
 * @height: return location for the height, or %NULL.
 *
 * This function tries to "do what you mean" and return
 * the size an actor will have. If the actor has a valid
 * allocation, the allocation will be returned; otherwise,
 * the actors natural size request will be returned.
 *
 * If you care whether you get the request vs. the allocation, you
 * should probably call a different function like
 * clutter_actor_get_allocation_coords() or
 * clutter_actor_get_preferred_width().
 *
 * Since: 0.2
 */
void
clutter_actor_get_size (ClutterActor *self,
			gfloat       *width,
			gfloat       *height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (width)
    *width = clutter_actor_get_width (self);

  if (height)
    *height = clutter_actor_get_height (self);
}

/**
 * clutter_actor_get_position:
 * @self: a #ClutterActor
 * @x: return location for the X coordinate, or %NULL
 * @y: return location for the Y coordinate, or %NULL
 *
 * This function tries to "do what you mean" and tell you where the
 * actor is, prior to any transformations. Retrieves the fixed
 * position of an actor in pixels, if one has been set; otherwise, if
 * the allocation is valid, returns the actor's allocated position;
 * otherwise, returns 0,0.
 *
 * The returned position is in pixels.
 *
 * Since: 0.6
 */
void
clutter_actor_get_position (ClutterActor *self,
                            gfloat       *x,
                            gfloat       *y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (x)
    *x = clutter_actor_get_x (self);

  if (y)
    *y = clutter_actor_get_y (self);
}

/**
 * clutter_actor_get_transformed_position:
 * @self: A #ClutterActor
 * @x: (out): return location for the X coordinate, or %NULL
 * @y: (out): return location for the Y coordinate, or %NULL
 *
 * Gets the absolute position of an actor, in pixels relative to the stage.
 *
 * Since: 0.8
 */
void
clutter_actor_get_transformed_position (ClutterActor *self,
                                        gfloat       *x,
                                        gfloat       *y)
{
  ClutterVertex v1;
  ClutterVertex v2;

  v1.x = v1.y = v1.z = 0;
  clutter_actor_apply_transform_to_point (self, &v1, &v2);

  if (x)
    *x = v2.x;

  if (y)
    *y = v2.y;
}

/**
 * clutter_actor_get_transformed_size:
 * @self: A #ClutterActor
 * @width: (out): return location for the width, or %NULL
 * @height: (out): return location for the height, or %NULL
 *
 * Gets the absolute size of an actor in pixels, taking into account the
 * scaling factors.
 *
 * If the actor has a valid allocation, the allocated size will be used.
 * If the actor has not a valid allocation then the preferred size will
 * be transformed and returned.
 *
 * If you want the transformed allocation, see
 * clutter_actor_get_abs_allocation_vertices() instead.
 *
 * <note>When the actor (or one of its ancestors) is rotated around the
 * X or Y axis, it no longer appears as on the stage as a rectangle, but
 * as a generic quadrangle; in that case this function returns the size
 * of the smallest rectangle that encapsulates the entire quad. Please
 * note that in this case no assumptions can be made about the relative
 * position of this envelope to the absolute position of the actor, as
 * returned by clutter_actor_get_transformed_position(); if you need this
 * information, you need to use clutter_actor_get_abs_allocation_vertices()
 * to get the coords of the actual quadrangle.</note>
 *
 * Since: 0.8
 */
void
clutter_actor_get_transformed_size (ClutterActor *self,
                                    gfloat       *width,
                                    gfloat       *height)
{
  ClutterActorPrivate *priv;
  ClutterVertex v[4];
  gfloat x_min, x_max, y_min, y_max;
  gint i;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  /* if the actor hasn't been allocated yet, get the preferred
   * size and transform that
   */
  if (priv->needs_allocation)
    {
      gfloat natural_width, natural_height;
      ClutterActorBox box;

      /* make a fake allocation to transform */
      clutter_actor_get_position (self, &box.x1, &box.y1);

      natural_width = natural_height = 0;
      clutter_actor_get_preferred_size (self, NULL, NULL,
                                        &natural_width,
                                        &natural_height);

      box.x2 = box.x1 + natural_width;
      box.y2 = box.y1 + natural_height;

      clutter_actor_transform_and_project_box (self, &box, v);
    }
  else
    clutter_actor_get_abs_allocation_vertices (self, v);

  x_min = x_max = v[0].x;
  y_min = y_max = v[0].y;

  for (i = 1; i < G_N_ELEMENTS (v); ++i)
    {
      if (v[i].x < x_min)
	x_min = v[i].x;

      if (v[i].x > x_max)
	x_max = v[i].x;

      if (v[i].y < y_min)
	y_min = v[i].y;

      if (v[i].y > y_max)
	y_max = v[i].y;
    }

  if (width)
    *width  = x_max - x_min;

  if (height)
    *height = y_max - y_min;
}

/**
 * clutter_actor_get_width:
 * @self: A #ClutterActor
 *
 * Retrieves the width of a #ClutterActor.
 *
 * If the actor has a valid allocation, this function will return the
 * width of the allocated area given to the actor.
 *
 * If the actor does not have a valid allocation, this function will
 * return the actor's natural width, that is the preferred width of
 * the actor.
 *
 * If you care whether you get the preferred width or the width that
 * has been assigned to the actor, you should probably call a different
 * function like clutter_actor_get_allocation_coords() to retrieve the
 * allocated size or clutter_actor_get_preferred_width() to retrieve the
 * preferred width.
 *
 * If an actor has a fixed width, for instance a width that has been
 * assigned using clutter_actor_set_width(), the width returned will
 * be the same value.
 *
 * Return value: the width of the actor, in pixels
 */
gfloat
clutter_actor_get_width (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  priv = self->priv;

  if (priv->needs_allocation)
    {
      gfloat natural_width = 0;

      if (self->priv->request_mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
        clutter_actor_get_preferred_width (self, -1, NULL, &natural_width);
      else
        {
          gfloat natural_height = 0;

          clutter_actor_get_preferred_height (self, -1, NULL, &natural_height);
          clutter_actor_get_preferred_width (self, natural_height,
                                             NULL,
                                             &natural_width);
        }

      return natural_width;
    }
  else
    return priv->allocation.x2 - priv->allocation.x1;
}

/**
 * clutter_actor_get_height:
 * @self: A #ClutterActor
 *
 * Retrieves the height of a #ClutterActor.
 *
 * If the actor has a valid allocation, this function will return the
 * height of the allocated area given to the actor.
 *
 * If the actor does not have a valid allocation, this function will
 * return the actor's natural height, that is the preferred height of
 * the actor.
 *
 * If you care whether you get the preferred height or the height that
 * has been assigned to the actor, you should probably call a different
 * function like clutter_actor_get_allocation_coords() to retrieve the
 * allocated size or clutter_actor_get_preferred_height() to retrieve the
 * preferred height.
 *
 * If an actor has a fixed height, for instance a height that has been
 * assigned using clutter_actor_set_height(), the height returned will
 * be the same value.
 *
 * Return value: the height of the actor, in pixels
 */
gfloat
clutter_actor_get_height (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  priv = self->priv;

  if (priv->needs_allocation)
    {
      gfloat natural_height = 0;

      if (priv->request_mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
        {
          gfloat natural_width = 0;

          clutter_actor_get_preferred_width (self, -1, NULL, &natural_width);
          clutter_actor_get_preferred_height (self, natural_width,
                                              NULL, &natural_height);
        }
      else
        clutter_actor_get_preferred_height (self, -1, NULL, &natural_height);

      return natural_height;
    }
  else
    return priv->allocation.y2 - priv->allocation.y1;
}

/**
 * clutter_actor_set_width
 * @self: A #ClutterActor
 * @width: Requested new width for the actor, in pixels
 *
 * Forces a width on an actor, causing the actor's preferred width
 * and height (if any) to be ignored.
 *
 * This function sets both the minimum and natural size of the actor.
 *
 * since: 0.2
 **/
void
clutter_actor_set_width (ClutterActor *self,
                         gfloat        width)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_min_width (self, width);
  clutter_actor_set_natural_width (self, width);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_set_height
 * @self: A #ClutterActor
 * @height: Requested new height for the actor, in pixels
 *
 * Forces a height on an actor, causing the actor's preferred width
 * and height (if any) to be ignored.
 *
 * This function sets both the minimum and natural size of the actor.
 *
 * since: 0.2
 **/
void
clutter_actor_set_height (ClutterActor *self,
                          gfloat        height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_min_height (self, height);
  clutter_actor_set_natural_height (self, height);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_set_x:
 * @self: a #ClutterActor
 * @x: the actor's position on the X axis
 *
 * Sets the actor's X coordinate, relative to its parent, in pixels.
 *
 * Overrides any layout manager and forces a fixed position for
 * the actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_x (ClutterActor *self,
                     gfloat        x)
{
  ClutterActorBox old = { 0, };
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->position_set && priv->fixed_x == x)
    return;

  clutter_actor_store_old_geometry (self, &old);

  priv->fixed_x = x;
  clutter_actor_set_fixed_position_set (self, TRUE);

  clutter_actor_notify_if_geometry_changed (self, &old);

  clutter_actor_queue_relayout (self);
}

/**
 * clutter_actor_set_y:
 * @self: a #ClutterActor
 * @y: the actor's position on the Y axis
 *
 * Sets the actor's Y coordinate, relative to its parent, in pixels.#
 *
 * Overrides any layout manager and forces a fixed position for
 * the actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_y (ClutterActor *self,
                     gfloat        y)
{
  ClutterActorBox old = { 0, };
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->position_set && priv->fixed_y == y)
    return;

  clutter_actor_store_old_geometry (self, &old);

  priv->fixed_y = y;
  clutter_actor_set_fixed_position_set (self, TRUE);

  clutter_actor_notify_if_geometry_changed (self, &old);

  clutter_actor_queue_relayout (self);
}

/**
 * clutter_actor_get_x
 * @self: A #ClutterActor
 *
 * Retrieves the X coordinate of a #ClutterActor.
 *
 * This function tries to "do what you mean", by returning the
 * correct value depending on the actor's state.
 *
 * If the actor has a valid allocation, this function will return
 * the X coordinate of the origin of the allocation box.
 *
 * If the actor has any fixed coordinate set using clutter_actor_set_x(),
 * clutter_actor_set_position() or clutter_actor_set_geometry(), this
 * function will return that coordinate.
 *
 * If both the allocation and a fixed position are missing, this function
 * will return 0.
 *
 * Return value: the X coordinate, in pixels, ignoring any
 *   transformation (i.e. scaling, rotation)
 */
gfloat
clutter_actor_get_x (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  priv = self->priv;

  if (priv->needs_allocation)
    {
      if (priv->position_set)
        return priv->fixed_x;
      else
        return 0;
    }
  else
    return priv->allocation.x1;
}

/**
 * clutter_actor_get_y
 * @self: A #ClutterActor
 *
 * Retrieves the Y coordinate of a #ClutterActor.
 *
 * This function tries to "do what you mean", by returning the
 * correct value depending on the actor's state.
 *
 * If the actor has a valid allocation, this function will return
 * the Y coordinate of the origin of the allocation box.
 *
 * If the actor has any fixed coordinate set using clutter_actor_set_y(),
 * clutter_actor_set_position() or clutter_actor_set_geometry(), this
 * function will return that coordinate.
 *
 * If both the allocation and a fixed position are missing, this function
 * will return 0.
 *
 * Return value: the Y coordinate, in pixels, ignoring any
 *   transformation (i.e. scaling, rotation)
 */
gfloat
clutter_actor_get_y (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  if (self->priv->needs_allocation)
    {
      if (self->priv->position_set)
        return self->priv->fixed_y;
      else
        return 0;
    }
  else
    return self->priv->allocation.y1;
}

/**
 * clutter_actor_set_scale:
 * @self: A #ClutterActor
 * @scale_x: double factor to scale actor by horizontally.
 * @scale_y: double factor to scale actor by vertically.
 *
 * Scales an actor with the given factors. The scaling is relative to
 * the scale center and the anchor point. The scale center is
 * unchanged by this function and defaults to 0,0.
 *
 * Since: 0.2
 */
void
clutter_actor_set_scale (ClutterActor *self,
                         gdouble       scale_x,
                         gdouble       scale_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  priv->scale_x = scale_x;
  g_object_notify (G_OBJECT (self), "scale-x");

  priv->scale_y = scale_y;
  g_object_notify (G_OBJECT (self), "scale-y");

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_set_scale_full:
 * @self: A #ClutterActor
 * @scale_x: double factor to scale actor by horizontally.
 * @scale_y: double factor to scale actor by vertically.
 * @center_x: X coordinate of the center of the scale.
 * @center_y: Y coordinate of the center of the scale
 *
 * Scales an actor with the given factors around the given center
 * point. The center point is specified in pixels relative to the
 * anchor point (usually the top left corner of the actor).
 *
 * Since: 1.0
 */
void
clutter_actor_set_scale_full (ClutterActor *self,
                              gdouble       scale_x,
                              gdouble       scale_y,
                              gfloat        center_x,
                              gfloat        center_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_scale (self, scale_x, scale_y);

  if (priv->scale_center.is_fractional)
    g_object_notify (G_OBJECT (self), "scale-gravity");

  g_object_notify (G_OBJECT (self), "scale-center-x");
  g_object_notify (G_OBJECT (self), "scale-center-y");

  clutter_anchor_coord_set_units (&priv->scale_center, center_x, center_y, 0);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_set_scale_with_gravity:
 * @self: A #ClutterActor
 * @scale_x: double factor to scale actor by horizontally.
 * @scale_y: double factor to scale actor by vertically.
 * @gravity: the location of the scale center expressed as a compass
 * direction.
 *
 * Scales an actor with the given factors around the given
 * center point. The center point is specified as one of the compass
 * directions in #ClutterGravity. For example, setting it to north
 * will cause the top of the actor to remain unchanged and the rest of
 * the actor to expand left, right and downwards.
 *
 * Since: 1.0
 */
void
clutter_actor_set_scale_with_gravity (ClutterActor   *self,
                                      gdouble         scale_x,
                                      gdouble         scale_y,
                                      ClutterGravity  gravity)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (gravity == CLUTTER_GRAVITY_NONE)
    clutter_actor_set_scale_full (self, scale_x, scale_y, 0, 0);
  else
    {
      g_object_freeze_notify (G_OBJECT (self));

      clutter_actor_set_scale (self, scale_x, scale_y);

      g_object_notify (G_OBJECT (self), "scale-gravity");
      g_object_notify (G_OBJECT (self), "scale-center-x");
      g_object_notify (G_OBJECT (self), "scale-center-y");

      clutter_anchor_coord_set_gravity (&priv->scale_center, gravity);

      g_object_thaw_notify (G_OBJECT (self));
    }
}

/**
 * clutter_actor_get_scale:
 * @self: A #ClutterActor
 * @scale_x: (out): Location to store horizonal float scale factor, or %NULL.
 * @scale_y: (out): Location to store vertical float scale factor, or %NULL.
 *
 * Retrieves an actors scale in floating point.
 *
 * Since: 0.2
 */
void
clutter_actor_get_scale (ClutterActor *self,
			 gdouble      *scale_x,
			 gdouble      *scale_y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (scale_x)
    *scale_x = self->priv->scale_x;

  if (scale_y)
    *scale_y = self->priv->scale_y;
}

/**
 * clutter_actor_get_scale_center:
 * @self: A #ClutterActor
 * @center_x: (out): Location to store the X position of the scale center, or %NULL.
 * @center_y: (out): Location to store the Y position of the scale center, or %NULL.
 *
 * Retrieves the scale center coordinate in pixels relative to the top
 * left corner of the actor. If the scale center was specified using a
 * #ClutterGravity this will calculate the pixel offset using the
 * current size of the actor.
 *
 * Since: 1.0
 */
void
clutter_actor_get_scale_center (ClutterActor *self,
                                gfloat       *center_x,
                                gfloat       *center_y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_anchor_coord_get_units (self, &self->priv->scale_center,
                                  center_x,
                                  center_y,
                                  NULL);
}

/**
 * clutter_actor_get_scale_gravity:
 * @self: A #ClutterActor
 *
 * Retrieves the scale center as a compass direction. If the scale
 * center was specified in pixels or units this will return
 * %CLUTTER_GRAVITY_NONE.
 *
 * Return value: the scale gravity
 *
 * Since: 1.0
 */
ClutterGravity
clutter_actor_get_scale_gravity (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), CLUTTER_GRAVITY_NONE);

  return clutter_anchor_coord_get_gravity (&self->priv->scale_center);
}

/**
 * clutter_actor_set_opacity:
 * @self: A #ClutterActor
 * @opacity: New opacity value for the actor.
 *
 * Sets the actor's opacity, with zero being completely transparent and
 * 255 (0xff) being fully opaque.
 */
void
clutter_actor_set_opacity (ClutterActor *self,
			   guint8        opacity)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->opacity != opacity)
    {
      priv->opacity = opacity;

      clutter_actor_queue_redraw (self);

      g_object_notify (G_OBJECT (self), "opacity");
    }
}

/**
 * clutter_actor_get_paint_opacity:
 * @self: A #ClutterActor
 *
 * Retrieves the absolute opacity of the actor, as it appears on the stage.
 *
 * This function traverses the hierarchy chain and composites the opacity of
 * the actor with that of its parents.
 *
 * This function is intended for subclasses to use in the paint virtual
 * function, to paint themselves with the correct opacity.
 *
 * Return value: The actor opacity value.
 *
 * Since: 0.8
 */
guint8
clutter_actor_get_paint_opacity (ClutterActor *self)
{
  ClutterActorPrivate *priv;
  ClutterActor *parent;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  priv = self->priv;

  if (priv->opacity_parent)
    return clutter_actor_get_paint_opacity (priv->opacity_parent);

  parent = priv->parent_actor;

  /* Factor in the actual actors opacity with parents */
  if (G_LIKELY (parent))
    {
      guint8 opacity = clutter_actor_get_paint_opacity (parent);

      if (opacity != 0xff)
        return (opacity * priv->opacity) / 0xff;
    }

  return clutter_actor_get_opacity (self);
}

/**
 * clutter_actor_get_opacity:
 * @self: a #ClutterActor
 *
 * Retrieves the opacity value of an actor, as set by
 * clutter_actor_set_opacity().
 *
 * For retrieving the absolute opacity of the actor inside a paint
 * virtual function, see clutter_actor_get_paint_opacity().
 *
 * Return value: the opacity of the actor
 */
guint8
clutter_actor_get_opacity (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  return self->priv->opacity;
}

/**
 * clutter_actor_set_name:
 * @self: A #ClutterActor
 * @name: Textual tag to apply to actor
 *
 * Sets the given name to @self. The name can be used to identify
 * a #ClutterActor.
 */
void
clutter_actor_set_name (ClutterActor *self,
			const gchar  *name)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_free (self->priv->name);
  self->priv->name = g_strdup (name);

  g_object_notify (G_OBJECT (self), "name");
}

/**
 * clutter_actor_get_name:
 * @self: A #ClutterActor
 *
 * Retrieves the name of @self.
 *
 * Return value: the name of the actor, or %NULL. The returned string is
 *   owned by the actor and should not be modified or freed.
 */
G_CONST_RETURN gchar *
clutter_actor_get_name (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  return self->priv->name;
}

/**
 * clutter_actor_get_gid:
 * @self: A #ClutterActor
 *
 * Retrieves the unique id for @self.
 *
 * Return value: Globally unique value for this object instance.
 *
 * Since: 0.6
 */
guint32
clutter_actor_get_gid (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  return self->priv->id;
}

/**
 * clutter_actor_set_depth:
 * @self: a #ClutterActor
 * @depth: Z co-ord
 *
 * Sets the Z coordinate of @self to @depth.
 *
 * The unit used by @depth is dependant on the perspective setup. See
 * also clutter_stage_set_perspective().
 */
void
clutter_actor_set_depth (ClutterActor *self,
                         gfloat        depth)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->z != depth)
    {
      /* Sets Z value - XXX 2.0: should we invert? */
      priv->z = depth;

      if (priv->parent_actor && CLUTTER_IS_CONTAINER (priv->parent_actor))
        {
          ClutterContainer *parent;

          /* We need to resort the container stacking order as to
           * correctly render alpha values.
           *
           * FIXME: This is sub-optimal. maybe queue the the sort
           *        before stacking
           */
          parent = CLUTTER_CONTAINER (priv->parent_actor);
          clutter_container_sort_depth_order (parent);
        }

      clutter_actor_queue_redraw (self);

      g_object_notify (G_OBJECT (self), "depth");
    }
}

/**
 * clutter_actor_get_depth:
 * @self: a #ClutterActor
 *
 * Retrieves the depth of @self.
 *
 * Return value: the depth of the actor
 */
gfloat
clutter_actor_get_depth (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), -1);

  return self->priv->z;
}

/**
 * clutter_actor_set_rotation:
 * @self: a #ClutterActor
 * @axis: the axis of rotation
 * @angle: the angle of rotation
 * @x: X coordinate of the rotation center
 * @y: Y coordinate of the rotation center
 * @z: Z coordinate of the rotation center
 *
 * Sets the rotation angle of @self around the given axis.
 *
 * The rotation center coordinates used depend on the value of @axis:
 * <itemizedlist>
 *   <listitem><para>%CLUTTER_X_AXIS requires @y and @z</para></listitem>
 *   <listitem><para>%CLUTTER_Y_AXIS requires @x and @z</para></listitem>
 *   <listitem><para>%CLUTTER_Z_AXIS requires @x and @y</para></listitem>
 * </itemizedlist>
 *
 * The rotation coordinates are relative to the anchor point of the
 * actor, set using clutter_actor_set_anchor_point(). If no anchor
 * point is set, the upper left corner is assumed as the origin.
 *
 * Since: 0.8
 */
void
clutter_actor_set_rotation (ClutterActor      *self,
                            ClutterRotateAxis  axis,
                            gdouble            angle,
                            gfloat             x,
                            gfloat             y,
                            gfloat             z)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_rotation_internal (self, axis, angle);

  switch (axis)
    {
    case CLUTTER_X_AXIS:
      clutter_anchor_coord_set_units (&priv->rx_center, x, y, z);
      g_object_notify (G_OBJECT (self), "rotation-center-x");
      break;

    case CLUTTER_Y_AXIS:
      clutter_anchor_coord_set_units (&priv->ry_center, x, y, z);
      g_object_notify (G_OBJECT (self), "rotation-center-y");
      break;

    case CLUTTER_Z_AXIS:
      if (priv->rz_center.is_fractional)
        g_object_notify (G_OBJECT (self), "rotation-center-z-gravity");
      clutter_anchor_coord_set_units (&priv->rz_center, x, y, z);
      g_object_notify (G_OBJECT (self), "rotation-center-z");
      break;
    }

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_set_z_rotation_from_gravity:
 * @self: a #ClutterActor
 * @angle: the angle of rotation
 * @gravity: the center point of the rotation
 *
 * Sets the rotation angle of @self around the Z axis using the center
 * point specified as a compass point. For example to rotate such that
 * the center of the actor remains static you can use
 * %CLUTTER_GRAVITY_CENTER. If the actor changes size the center point
 * will move accordingly.
 *
 * Since: 1.0
 */
void
clutter_actor_set_z_rotation_from_gravity (ClutterActor   *self,
                                           gdouble         angle,
                                           ClutterGravity  gravity)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (gravity == CLUTTER_GRAVITY_NONE)
    clutter_actor_set_rotation (self, CLUTTER_Z_AXIS, angle, 0, 0, 0);
  else
    {
      priv = self->priv;

      g_object_freeze_notify (G_OBJECT (self));

      clutter_actor_set_rotation_internal (self, CLUTTER_Z_AXIS, angle);

      clutter_anchor_coord_set_gravity (&priv->rz_center, gravity);
      g_object_notify (G_OBJECT (self), "rotation-center-z-gravity");
      g_object_notify (G_OBJECT (self), "rotation-center-z");

      g_object_thaw_notify (G_OBJECT (self));
    }
}

/**
 * clutter_actor_get_rotation:
 * @self: a #ClutterActor
 * @axis: the axis of rotation
 * @x: (out): return value for the X coordinate of the center of rotation
 * @y: (out): return value for the Y coordinate of the center of rotation
 * @z: (out): return value for the Z coordinate of the center of rotation
 *
 * Retrieves the angle and center of rotation on the given axis,
 * set using clutter_actor_set_rotation().
 *
 * Return value: the angle of rotation
 *
 * Since: 0.8
 */
gdouble
clutter_actor_get_rotation (ClutterActor      *self,
                            ClutterRotateAxis  axis,
                            gfloat            *x,
                            gfloat            *y,
                            gfloat            *z)
{
  ClutterActorPrivate *priv;
  gdouble retval = 0;
  AnchorCoord *anchor_coord = NULL;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  priv = self->priv;

  switch (axis)
    {
    case CLUTTER_X_AXIS:
      anchor_coord = &priv->rx_center;
      retval = priv->rxang;
      break;

    case CLUTTER_Y_AXIS:
      anchor_coord = &priv->ry_center;
      retval = priv->ryang;
      break;

    case CLUTTER_Z_AXIS:
      anchor_coord = &priv->rz_center;
      retval = priv->rzang;
      break;
    }

  clutter_anchor_coord_get_units (self, anchor_coord, x, y, z);

  return retval;
}

/**
 * clutter_actor_get_z_rotation_gravity:
 * @self: A #ClutterActor
 *
 * Retrieves the center for the rotation around the Z axis as a
 * compass direction. If the center was specified in pixels or units
 * this will return %CLUTTER_GRAVITY_NONE.
 *
 * Return value: the Z rotation center
 *
 * Since: 1.0
 */
ClutterGravity
clutter_actor_get_z_rotation_gravity (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0.0);

  return clutter_anchor_coord_get_gravity (&self->priv->rz_center);
}

/**
 * clutter_actor_set_clip:
 * @self: A #ClutterActor
 * @xoff: X offset of the clip rectangle
 * @yoff: Y offset of the clip rectangle
 * @width: Width of the clip rectangle
 * @height: Height of the clip rectangle
 *
 * Sets clip area for @self. The clip area is always computed from the
 * upper left corner of the actor, even if the anchor point is set
 * otherwise.
 *
 * Since: 0.6
 */
void
clutter_actor_set_clip (ClutterActor *self,
                        gfloat        xoff,
                        gfloat        yoff,
                        gfloat        width,
                        gfloat        height)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->has_clip &&
      priv->clip[0] == xoff &&
      priv->clip[1] == yoff &&
      priv->clip[2] == width &&
      priv->clip[3] == height)
    return;

  priv->clip[0] = xoff;
  priv->clip[1] = yoff;
  priv->clip[2] = width;
  priv->clip[3] = height;

  priv->has_clip = TRUE;

  clutter_actor_queue_redraw (self);

  g_object_notify (G_OBJECT (self), "has-clip");
  g_object_notify (G_OBJECT (self), "clip");
}

/**
 * clutter_actor_remove_clip
 * @self: A #ClutterActor
 *
 * Removes clip area from @self.
 */
void
clutter_actor_remove_clip (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (!self->priv->has_clip)
    return;

  self->priv->has_clip = FALSE;

  clutter_actor_queue_redraw (self);

  g_object_notify (G_OBJECT (self), "has-clip");
}

/**
 * clutter_actor_has_clip:
 * @self: a #ClutterActor
 *
 * Determines whether the actor has a clip area set or not.
 *
 * Return value: %TRUE if the actor has a clip area set.
 *
 * Since: 0.1.1
 */
gboolean
clutter_actor_has_clip (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  return self->priv->has_clip;
}

/**
 * clutter_actor_get_clip:
 * @self: a #ClutterActor
 * @xoff: (out): return location for the X offset of the clip rectangle, or %NULL
 * @yoff: (out): return location for the Y offset of the clip rectangle, or %NULL
 * @width: (out): return location for the width of the clip rectangle, or %NULL
 * @height: (out): return location for the height of the clip rectangle, or %NULL
 *
 * Gets the clip area for @self, if any is set
 *
 * Since: 0.6
 */
void
clutter_actor_get_clip (ClutterActor *self,
                        gfloat       *xoff,
                        gfloat       *yoff,
                        gfloat       *width,
                        gfloat       *height)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (!priv->has_clip)
    return;

  if (xoff)
    *xoff = priv->clip[0];

  if (yoff)
    *yoff = priv->clip[1];

  if (width)
    *width = priv->clip[2];

  if (height)
    *height = priv->clip[3];
}

/**
 * clutter_actor_set_parent:
 * @self: A #ClutterActor
 * @parent: A new #ClutterActor parent
 *
 * Sets the parent of @self to @parent.  The opposite function is
 * clutter_actor_unparent().
 *
 * This function should not be used by applications, but by custom
 * container actor subclasses.
 */
void
clutter_actor_set_parent (ClutterActor *self,
		          ClutterActor *parent)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (parent));
  g_return_if_fail (self != parent);

  priv = self->priv;

  if (priv->parent_actor != NULL)
    {
      g_warning ("Cannot set a parent on an actor which has a parent.\n"
		 "You must use clutter_actor_unparent() first.\n");
      return;
    }

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      g_warning ("Cannot set a parent on a toplevel actor\n");
      return;
    }

  g_object_ref_sink (self);
  priv->parent_actor = parent;

  /* clutter_actor_reparent() will emit ::parent-set for us */
  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT))
    g_signal_emit (self, actor_signals[PARENT_SET], 0, NULL);

  /* If parent is mapped or realized, we need to also be mapped or
   * realized once we're inside the parent.
   */
  clutter_actor_update_map_state (self, MAP_STATE_CHECK);

  if (priv->show_on_set_parent)
    clutter_actor_show (self);

  if (CLUTTER_ACTOR_IS_MAPPED (self))
    {
      clutter_actor_queue_redraw (self);
    }

  /* maintain the invariant that if an actor needs layout,
   * its parents do as well
   */
  if (priv->needs_width_request ||
      priv->needs_height_request ||
      priv->needs_allocation)
    {
      /* we work around the short-circuiting we do
       * in clutter_actor_queue_relayout() since we
       * want to force a relayout
       */
      priv->needs_width_request = TRUE;
      priv->needs_height_request = TRUE;
      priv->needs_allocation = TRUE;

      clutter_actor_queue_relayout (priv->parent_actor);
    }
}

/**
 * clutter_actor_get_parent:
 * @self: A #ClutterActor
 *
 * Retrieves the parent of @self.
 *
 * Return Value: (transfer none): The #ClutterActor parent, or %NULL if no parent is set
 */
ClutterActor *
clutter_actor_get_parent (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  return self->priv->parent_actor;
}

/**
 * clutter_actor_get_paint_visibility:
 * @self: A #ClutterActor
 *
 * Retrieves the 'paint' visibility of an actor recursively checking for non
 * visible parents.
 *
 * This is by definition the same as CLUTTER_ACTOR_IS_MAPPED().
 *
 * Return Value: TRUE if the actor is visibile and will be painted.
 *
 * Since: 0.8.4
 */
gboolean
clutter_actor_get_paint_visibility (ClutterActor *actor)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);

  return CLUTTER_ACTOR_IS_MAPPED (actor);
}

/**
 * clutter_actor_unparent:
 * @self: a #ClutterActor
 *
 * Removes the parent of @self.
 *
 * This function should not be used in applications.  It should be called by
 * implementations of container actors, to dissociate a child from the
 * container.
 *
 * Since: 0.1.1
 */
void
clutter_actor_unparent (ClutterActor *self)
{
  ClutterActorPrivate *priv;
  ClutterActor *old_parent;
  gboolean was_mapped;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->parent_actor == NULL)
    return;

  was_mapped = CLUTTER_ACTOR_IS_MAPPED (self);

  /* we need to unrealize *before* we set parent_actor to NULL,
   * because in an unrealize method actors are dissociating from the
   * stage, which means they need to be able to
   * clutter_actor_get_stage(). This should unmap and unrealize,
   * unless we're reparenting.
   */
  clutter_actor_update_map_state (self, MAP_STATE_MAKE_UNREALIZED);

  old_parent = priv->parent_actor;
  priv->parent_actor = NULL;

  /* clutter_actor_reparent() will emit ::parent-set for us */
  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT))
    g_signal_emit (self, actor_signals[PARENT_SET], 0, old_parent);

  /* Queue a redraw on old_parent only if we were painted in the first
   * place. Will be no-op if old parent is not shown.
   */
  if (was_mapped && !CLUTTER_ACTOR_IS_MAPPED (self))
    clutter_actor_queue_redraw (old_parent);

  /* remove the reference we acquired in clutter_actor_set_parent() */
  g_object_unref (self);
}

/**
 * clutter_actor_reparent:
 * @self: a #ClutterActor
 * @new_parent: the new #ClutterActor parent
 *
 * This function resets the parent actor of @self.  It is
 * logically equivalent to calling clutter_actor_unparent()
 * and clutter_actor_set_parent(), but more efficiently
 * implemented, ensures the child is not finalized
 * when unparented, and emits the parent-set signal only
 * one time.
 *
 * Since: 0.2
 */
void
clutter_actor_reparent (ClutterActor *self,
                        ClutterActor *new_parent)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (new_parent));
  g_return_if_fail (self != new_parent);

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      g_warning ("Cannot set a parent on a toplevel actor\n");
      return;
    }

  priv = self->priv;

  if (priv->parent_actor != new_parent)
    {
      ClutterActor *old_parent;

      CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_REPARENT);

      old_parent = priv->parent_actor;

      g_object_ref (self);

      if (CLUTTER_IS_CONTAINER (priv->parent_actor))
        {
          ClutterContainer *parent = CLUTTER_CONTAINER (priv->parent_actor);
          /* Note, will call unparent() */
          clutter_container_remove_actor (parent, self);
        }
      else
        clutter_actor_unparent (self);

      if (CLUTTER_IS_CONTAINER (new_parent))
          /* Note, will call parent() */
        clutter_container_add_actor (CLUTTER_CONTAINER (new_parent), self);
      else
        clutter_actor_set_parent (self, new_parent);

      /* we emit the ::parent-set signal once */
      g_signal_emit (self, actor_signals[PARENT_SET], 0, old_parent);

      g_object_unref (self);

      CLUTTER_UNSET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_REPARENT);

      /* the IN_REPARENT flag suspends state updates */
      clutter_actor_update_map_state (self, MAP_STATE_CHECK);
   }
}
/**
 * clutter_actor_raise:
 * @self: A #ClutterActor
 * @below: (allow-none): A #ClutterActor to raise above.
 *
 * Puts @self above @below.
 *
 * Both actors must have the same parent.
 *
 * This function is the equivalent of clutter_container_raise_child().
 */
void
clutter_actor_raise (ClutterActor *self,
                     ClutterActor *below)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  parent = clutter_actor_get_parent (self);
  if (!parent)
    {
      g_warning ("Actor of type %s is not inside a container",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  if (below)
    {
      if (parent != clutter_actor_get_parent (below))
        {
          g_warning ("Actor of type %s is not in the same "
                     "container of actor of type %s",
                     g_type_name (G_OBJECT_TYPE (self)),
                     g_type_name (G_OBJECT_TYPE (below)));
          return;
        }
    }

  clutter_container_raise_child (CLUTTER_CONTAINER (parent), self, below);
}

/**
 * clutter_actor_lower:
 * @self: A #ClutterActor
 * @above: (allow-none): A #ClutterActor to lower below
 *
 * Puts @self below @above.
 *
 * Both actors must have the same parent.
 *
 * This function is the equivalent of clutter_container_lower_child().
 */
void
clutter_actor_lower (ClutterActor *self,
                     ClutterActor *above)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_ACTOR(self));

  parent = clutter_actor_get_parent (self);
  if (!parent)
    {
      g_warning ("Actor of type %s is not inside a container",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  if (above)
    {
      if (parent != clutter_actor_get_parent (above))
        {
          g_warning ("Actor of type %s is not in the same "
                     "container of actor of type %s",
                     g_type_name (G_OBJECT_TYPE (self)),
                     g_type_name (G_OBJECT_TYPE (above)));
          return;
        }
    }

  clutter_container_lower_child (CLUTTER_CONTAINER (parent), self, above);
}

/**
 * clutter_actor_raise_top:
 * @self: A #ClutterActor
 *
 * Raises @self to the top.
 *
 * This function calls clutter_actor_raise() internally.
 */
void
clutter_actor_raise_top (ClutterActor *self)
{
  clutter_actor_raise (self, NULL);
}

/**
 * clutter_actor_lower_bottom:
 * @self: A #ClutterActor
 *
 * Lowers @self to the bottom.
 *
 * This function calls clutter_actor_lower() internally.
 */
void
clutter_actor_lower_bottom (ClutterActor *self)
{
  clutter_actor_lower (self, NULL);
}

/*
 * Event handling
 */

/**
 * clutter_actor_event:
 * @actor: a #ClutterActor
 * @event: a #ClutterEvent
 * @capture: TRUE if event in in capture phase, FALSE otherwise.
 *
 * This function is used to emit an event on the main stage.
 * You should rarely need to use this function, except for
 * synthetising events.
 *
 * Return value: the return value from the signal emission: %TRUE
 *   if the actor handled the event, or %FALSE if the event was
 *   not handled
 *
 * Since: 0.6
 */
gboolean
clutter_actor_event (ClutterActor *actor,
                     ClutterEvent *event,
		     gboolean      capture)
{
  gboolean retval = FALSE;
  gint signal_num = -1;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  g_object_ref (actor);

  if (capture)
    {
      g_signal_emit (actor, actor_signals[CAPTURED_EVENT], 0,
		     event,
                     &retval);
      goto out;
    }

  g_signal_emit (actor, actor_signals[EVENT], 0, event, &retval);

  if (!retval)
    {
      switch (event->type)
	{
	case CLUTTER_NOTHING:
	  break;
	case CLUTTER_BUTTON_PRESS:
	  signal_num = BUTTON_PRESS_EVENT;
	  break;
	case CLUTTER_BUTTON_RELEASE:
	  signal_num = BUTTON_RELEASE_EVENT;
	  break;
	case CLUTTER_SCROLL:
	  signal_num = SCROLL_EVENT;
	  break;
	case CLUTTER_KEY_PRESS:
	  signal_num = KEY_PRESS_EVENT;
	  break;
	case CLUTTER_KEY_RELEASE:
	  signal_num = KEY_RELEASE_EVENT;
	  break;
	case CLUTTER_MOTION:
	  signal_num = MOTION_EVENT;
	  break;
	case CLUTTER_ENTER:
	  signal_num = ENTER_EVENT;
	  break;
	case CLUTTER_LEAVE:
	  signal_num = LEAVE_EVENT;
	  break;
	case CLUTTER_DELETE:
	case CLUTTER_DESTROY_NOTIFY:
	case CLUTTER_CLIENT_MESSAGE:
	default:
	  signal_num = -1;
	  break;
	}

      if (signal_num != -1)
	g_signal_emit (actor, actor_signals[signal_num], 0,
		       event, &retval);
    }

out:
  g_object_unref (actor);

  return retval;
}

/**
 * clutter_actor_set_reactive:
 * @actor: a #ClutterActor
 * @reactive: whether the actor should be reactive to events
 *
 * Sets @actor as reactive. Reactive actors will receive events.
 *
 * Since: 0.6
 */
void
clutter_actor_set_reactive (ClutterActor *actor,
                            gboolean      reactive)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  if (reactive == CLUTTER_ACTOR_IS_REACTIVE (actor))
    return;

  if (reactive)
    CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REACTIVE);
  else
    CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REACTIVE);

  g_object_notify (G_OBJECT (actor), "reactive");
}

/**
 * clutter_actor_get_reactive:
 * @actor: a #ClutterActor
 *
 * Checks whether @actor is marked as reactive.
 *
 * Return value: %TRUE if the actor is reactive
 *
 * Since: 0.6
 */
gboolean
clutter_actor_get_reactive (ClutterActor *actor)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);

  return CLUTTER_ACTOR_IS_REACTIVE (actor) ? TRUE : FALSE;
}

/**
 * clutter_actor_get_anchor_point:
 * @self: a #ClutterActor
 * @anchor_x: (out): return location for the X coordinate of the anchor point
 * @anchor_y: (out): return location for the Y coordinate of the anchor point
 *
 * Gets the current anchor point of the @actor in pixels.
 *
 * Since: 0.6
 */
void
clutter_actor_get_anchor_point (ClutterActor *self,
				gfloat       *anchor_x,
                                gfloat       *anchor_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  clutter_anchor_coord_get_units (self, &priv->anchor,
                                  anchor_x,
                                  anchor_y,
                                  NULL);
}

/**
 * clutter_actor_set_anchor_point:
 * @self: a #ClutterActor
 * @anchor_x: X coordinate of the anchor point
 * @anchor_y: Y coordinate of the anchor point
 *
 * Sets an anchor point for @self. The anchor point is a point in the
 * coordinate space of an actor to which the actor position within its
 * parent is relative; the default is (0, 0), i.e. the top-left corner
 * of the actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_anchor_point (ClutterActor *self,
                                gfloat        anchor_x,
                                gfloat        anchor_y)
{
  ClutterActorPrivate *priv;
  gboolean changed = FALSE;
  gfloat old_anchor_x, old_anchor_y;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_anchor_coord_get_units (self, &priv->anchor,
                                  &old_anchor_x,
                                  &old_anchor_y,
                                  NULL);

  if (priv->anchor.is_fractional)
    g_object_notify (G_OBJECT (self), "anchor-gravity");

  if (old_anchor_x != anchor_x)
    {
      g_object_notify (G_OBJECT (self), "anchor-x");
      changed = TRUE;
    }

  if (old_anchor_y != anchor_y)
    {
      g_object_notify (G_OBJECT (self), "anchor-y");
      changed = TRUE;
    }

  clutter_anchor_coord_set_units (&priv->anchor, anchor_x, anchor_y, 0);

  if (changed && CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_get_anchor_point_gravity:
 * @self: a #ClutterActor
 *
 * Retrieves the anchor position expressed as a #ClutterGravity. If
 * the anchor point was specified using pixels or units this will
 * return %CLUTTER_GRAVITY_NONE.
 *
 * Return value: the #ClutterGravity used by the anchor point
 *
 * Since: 1.0
 */
ClutterGravity
clutter_actor_get_anchor_point_gravity (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), CLUTTER_GRAVITY_NONE);

  priv = self->priv;

  return clutter_anchor_coord_get_gravity (&priv->anchor);
}

/**
 * clutter_actor_move_anchor_point:
 * @self: a #ClutterActor
 * @anchor_x: X coordinate of the anchor point
 * @anchor_y: Y coordinate of the anchor point
 *
 * Sets an anchor point for the actor, and adjusts the actor postion so that
 * the relative position of the actor toward its parent remains the same.
 *
 * Since: 0.6
 */
void
clutter_actor_move_anchor_point (ClutterActor *self,
                                 gfloat        anchor_x,
                                 gfloat        anchor_y)
{
  ClutterActorPrivate *priv;
  gfloat old_anchor_x, old_anchor_y;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  clutter_anchor_coord_get_units (self, &priv->anchor,
                                  &old_anchor_x,
                                  &old_anchor_y,
                                  NULL);

  g_object_freeze_notify (G_OBJECT (self));

  clutter_actor_set_anchor_point (self, anchor_x, anchor_y);

  if (priv->position_set)
    clutter_actor_move_by (self,
                           anchor_x - old_anchor_x,
                           anchor_y - old_anchor_y);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_move_anchor_point_from_gravity:
 * @self: a #ClutterActor
 * @gravity: #ClutterGravity.
 *
 * Sets an anchor point on the actor based on the given gravity, adjusting the
 * actor postion so that its relative position within its parent remains
 * unchanged.
 *
 * Since version 1.0 the anchor point will be stored as a gravity so
 * that if the actor changes size then the anchor point will move. For
 * example, if you set the anchor point to %CLUTTER_GRAVITY_SOUTH_EAST
 * and later double the size of the actor, the anchor point will move
 * to the bottom right.
 *
 * Since: 0.6
 */
void
clutter_actor_move_anchor_point_from_gravity (ClutterActor   *self,
					      ClutterGravity  gravity)
{
  gfloat old_anchor_x, old_anchor_y, new_anchor_x, new_anchor_y;
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_freeze_notify (G_OBJECT (self));

  clutter_anchor_coord_get_units (self, &priv->anchor,
                                  &old_anchor_x,
                                  &old_anchor_y,
                                  NULL);
  clutter_actor_set_anchor_point_from_gravity (self, gravity);
  clutter_anchor_coord_get_units (self, &priv->anchor,
                                  &new_anchor_x,
                                  &new_anchor_y,
                                  NULL);

  if (priv->position_set)
    clutter_actor_move_by (self,
                           new_anchor_x - old_anchor_x,
                           new_anchor_y - old_anchor_y);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_actor_set_anchor_point_from_gravity:
 * @self: a #ClutterActor
 * @gravity: #ClutterGravity.
 *
 * Sets an anchor point on the actor, based on the given gravity (this is a
 * convenience function wrapping clutter_actor_set_anchor_point()).
 *
 * Since version 1.0 the anchor point will be stored as a gravity so
 * that if the actor changes size then the anchor point will move. For
 * example, if you set the anchor point to %CLUTTER_GRAVITY_SOUTH_EAST
 * and later double the size of the actor, the anchor point will move
 * to the bottom right.
 *
 * Since: 0.6
 */
void
clutter_actor_set_anchor_point_from_gravity (ClutterActor   *self,
					     ClutterGravity  gravity)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (gravity == CLUTTER_GRAVITY_NONE)
    clutter_actor_set_anchor_point (self, 0, 0);
  else
    {
      clutter_anchor_coord_set_gravity (&self->priv->anchor, gravity);

      g_object_notify (G_OBJECT (self), "anchor-gravity");
      g_object_notify (G_OBJECT (self), "anchor-x");
      g_object_notify (G_OBJECT (self), "anchor-y");
    }
}

typedef enum
{
  PARSE_X,
  PARSE_Y,
  PARSE_WIDTH,
  PARSE_HEIGHT,
  PARSE_ANCHOR_X,
  PARSE_ANCHOR_Y
} ParseDimension;

static gfloat
parse_units (ClutterActor   *self,
             ParseDimension  dimension,
             JsonNode       *node)
{
  GValue value = { 0, };
  gfloat retval = 0;

  if (JSON_NODE_TYPE (node) != JSON_NODE_VALUE)
    return 0;

  json_node_get_value (node, &value);

  if (G_VALUE_HOLDS (&value, G_TYPE_INT))
    {
      retval = (gfloat) g_value_get_int (&value);
    }
  else if (G_VALUE_HOLDS (&value, G_TYPE_FLOAT))
    {
      retval = g_value_get_float (&value);
    }
  else if (G_VALUE_HOLDS (&value, G_TYPE_STRING))
    {
      ClutterUnits units;
      gboolean res;

      res = clutter_units_from_string (&units, g_value_get_string (&value));
      if (res)
        retval = clutter_units_to_pixels (&units);
      else
        {
          g_warning ("Invalid value '%s': integers, strings or floating point "
                     "values can be used for the x, y, width and height "
                     "properties. Valid modifiers for strings are 'px', 'mm', "
                     "'pt' and 'em'.",
                     g_value_get_string (&value));
          retval = 0;
        }
    }
  else if (G_VALUE_HOLDS (&value, G_TYPE_DOUBLE))
    {
      ClutterActor *stage;
      gdouble val;

      stage = clutter_actor_get_stage (self);
      if (stage == NULL)
        stage = clutter_stage_get_default ();

      if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
        {
          g_warning ("Unable to set percentage of %s on a top-level "
                     "actor of type '%s'",
                     (dimension == PARSE_X || dimension == PARSE_WIDTH) ? "width"
                                                                        : "height",
                     g_type_name (G_OBJECT_TYPE (self)));
          retval = 0;
          goto out;
        }

      val = g_value_get_double (&value);

      if (dimension == PARSE_X ||
          dimension == PARSE_WIDTH ||
          dimension == PARSE_ANCHOR_X)
        {
          retval = clutter_actor_get_width (stage) * val;
        }
      else
        {
          retval = clutter_actor_get_height (stage) * val;
        }
    }
  else
    {
      g_warning ("Invalid value of type '%s': integers, strings of floating "
                 "point values can be used for the x, y, width, height "
                 "anchor-x and anchor-y properties.",
                 g_type_name (G_VALUE_TYPE (&value)));
    }

out:
  g_value_unset (&value);

  return retval;
}

typedef struct {
  ClutterRotateAxis axis;

  gdouble angle;

  gfloat center_x;
  gfloat center_y;
  gfloat center_z;
} RotationInfo;

static inline gboolean
parse_rotation_array (ClutterActor *actor,
                      JsonArray    *array,
                      RotationInfo *info)
{
  JsonNode *element;

  if (json_array_get_length (array) != 2)
    return FALSE;

  /* angle */
  element = json_array_get_element (array, 0);
  if (JSON_NODE_TYPE (element) == JSON_NODE_VALUE)
    info->angle = json_node_get_double (element);
  else
    return FALSE;

  /* center */
  element = json_array_get_element (array, 1);
  if (JSON_NODE_TYPE (element) == JSON_NODE_ARRAY)
    {
      JsonArray *center = json_node_get_array (element);

      if (json_array_get_length (center) != 2)
        return FALSE;

      switch (info->axis)
        {
        case CLUTTER_X_AXIS:
          info->center_y = parse_units (actor, PARSE_Y,
                                        json_array_get_element (center, 0));
          info->center_z = parse_units (actor, PARSE_Y,
                                        json_array_get_element (center, 1));
          return TRUE;

        case CLUTTER_Y_AXIS:
          info->center_x = parse_units (actor, PARSE_X,
                                        json_array_get_element (center, 0));
          info->center_z = parse_units (actor, PARSE_X,
                                        json_array_get_element (center, 1));
          return TRUE;

        case CLUTTER_Z_AXIS:
          info->center_x = parse_units (actor, PARSE_X,
                                        json_array_get_element (center, 0));
          info->center_y = parse_units (actor, PARSE_Y,
                                        json_array_get_element (center, 1));
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
parse_rotation (ClutterActor *actor,
                JsonNode     *node,
                RotationInfo *info)
{
  JsonArray *array;
  guint len, i;
  gboolean retval = FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    {
      g_warning ("Invalid node of type '%s' found, expecting an array",
                 json_node_type_name (node));
      return FALSE;
    }

  array = json_node_get_array (node);
  len = json_array_get_length (array);

  for (i = 0; i < len; i++)
    {
      JsonNode *element = json_array_get_element (array, i);
      JsonObject *object;
      JsonNode *member;

      if (JSON_NODE_TYPE (element) != JSON_NODE_OBJECT)
        {
          g_warning ("Invalid node of type '%s' found, expecting an object",
                     json_node_type_name (element));
          return FALSE;
        }

      object = json_node_get_object (element);

      if (json_object_has_member (object, "x-axis"))
        {
          member = json_object_get_member (object, "x-axis");

          info->axis = CLUTTER_X_AXIS;

          if (JSON_NODE_TYPE (member) == JSON_NODE_VALUE)
            {
              info->angle = json_node_get_double (member);
              retval = TRUE;
            }
          else if (JSON_NODE_TYPE (member) == JSON_NODE_ARRAY)
            retval = parse_rotation_array (actor,
                                           json_node_get_array (member),
                                           info);
          else
            retval = FALSE;
        }
      else if (json_object_has_member (object, "y-axis"))
        {
          member = json_object_get_member (object, "y-axis");

          info->axis = CLUTTER_Y_AXIS;

          if (JSON_NODE_TYPE (member) == JSON_NODE_VALUE)
            {
              info->angle = json_node_get_double (member);
              retval = TRUE;
            }
          else if (JSON_NODE_TYPE (member) == JSON_NODE_ARRAY)
            retval = parse_rotation_array (actor,
                                           json_node_get_array (member),
                                           info);
          else
            retval = FALSE;
        }
      else if (json_object_has_member (object, "z-axis"))
        {
          member = json_object_get_member (object, "z-axis");

          info->axis = CLUTTER_Z_AXIS;

          if (JSON_NODE_TYPE (member) == JSON_NODE_VALUE)
            {
              info->angle = json_node_get_double (member);
              retval = TRUE;
            }
          else if (JSON_NODE_TYPE (member) == JSON_NODE_ARRAY)
            retval = parse_rotation_array (actor,
                                           json_node_get_array (member),
                                           info);
          else
            retval = FALSE;
        }
    }

  return retval;
}

static gboolean
clutter_actor_parse_custom_node (ClutterScriptable *scriptable,
                                 ClutterScript     *script,
                                 GValue            *value,
                                 const gchar       *name,
                                 JsonNode          *node)
{
  ClutterActor *actor = CLUTTER_ACTOR (scriptable);
  gboolean retval = FALSE;

  if ((name[0] == 'x' && name[1] == '\0') ||
      (name[0] == 'y' && name[1] == '\0') ||
      (strcmp (name, "width") == 0) ||
      (strcmp (name, "height") == 0) ||
      (strcmp (name, "anchor_x") == 0) ||
      (strcmp (name, "anchor_y") == 0))
    {
      ParseDimension dimension;
      gfloat units;

      if (name[0] == 'x')
        dimension = PARSE_X;
      else if (name[0] == 'y')
        dimension = PARSE_Y;
      else if (name[0] == 'w')
        dimension = PARSE_WIDTH;
      else if (name[0] == 'h')
        dimension = PARSE_HEIGHT;
      else if (name[0] == 'a' && name[7] == 'x')
        dimension = PARSE_ANCHOR_X;
      else if (name[0] == 'a' && name[7] == 'y')
        dimension = PARSE_ANCHOR_Y;
      else
        return FALSE;

      units = parse_units (actor, dimension, node);

      /* convert back to pixels: all properties are pixel-based */
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, units);

      retval = TRUE;
    }
  else if (strcmp (name, "rotation") == 0)
    {
      RotationInfo *info;

      info = g_slice_new0 (RotationInfo);
      retval = parse_rotation (actor, node, info);

      if (retval)
        {
          g_value_init (value, G_TYPE_POINTER);
          g_value_set_pointer (value, info);
        }
      else
        g_slice_free (RotationInfo, info);
    }

  return retval;
}

static void
clutter_actor_set_custom_property (ClutterScriptable *scriptable,
                                   ClutterScript     *script,
                                   const gchar       *name,
                                   const GValue      *value)
{
#ifdef CLUTTER_ENABLE_DEBUG
  {
    gchar *tmp = g_strdup_value_contents (value);

    CLUTTER_NOTE (SCRIPT,
                  "in ClutterActor::set_custom_property('%s') = %s",
                  name,
                  tmp);

    g_free (tmp);
  }
#endif

  if (strcmp (name, "rotation") == 0)
    {
      RotationInfo *info;

      if (!G_VALUE_HOLDS (value, G_TYPE_POINTER))
        return;

      info = g_value_get_pointer (value);

      clutter_actor_set_rotation (CLUTTER_ACTOR (scriptable),
                                  info->axis, info->angle,
                                  info->center_x,
                                  info->center_y,
                                  info->center_z);

      g_slice_free (RotationInfo, info);
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_actor_parse_custom_node;
  iface->set_custom_property = clutter_actor_set_custom_property;
}

/**
 * clutter_actor_transform_stage_point
 * @self: A #ClutterActor
 * @x: (in): x screen coordinate of the point to unproject
 * @y: (in): y screen coordinate of the point to unproject
 * @x_out: (out): return location for the unprojected x coordinance
 * @y_out: (out): return location for the unprojected y coordinance
 *
 * This function translates screen coordinates (@x, @y) to
 * coordinates relative to the actor. For example, it can be used to translate
 * screen events from global screen coordinates into actor-local coordinates.
 *
 * The conversion can fail, notably if the transform stack results in the
 * actor being projected on the screen as a mere line.
 *
 * The conversion should not be expected to be pixel-perfect due to the
 * nature of the operation. In general the error grows when the skewing
 * of the actor rectangle on screen increases.
 *
 * Note: This function is fairly computationally intensive.
 *
 * Note: This function only works when the allocation is up-to-date,
 * i.e. inside of paint()
 *
 * Return value: %TRUE if conversion was successful.
 *
 * Since: 0.6
 */
gboolean
clutter_actor_transform_stage_point (ClutterActor *self,
				     gfloat        x,
				     gfloat        y,
				     gfloat       *x_out,
				     gfloat       *y_out)
{
  ClutterVertex v[4];
  float ST[3][3];
  float RQ[3][3];
  int du, dv, xi, yi;
  float px, py;
  float xf, yf, wf, det;
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  priv = self->priv;

  /* This implementation is based on the quad -> quad projection algorithm
   * described by Paul Heckbert in:
   *
   *   http://www.cs.cmu.edu/~ph/texfund/texfund.pdf
   *
   * and the sample implementation at:
   *
   *   http://www.cs.cmu.edu/~ph/src/texfund/
   *
   * Our texture is a rectangle with origin [0, 0], so we are mapping from
   * quad to rectangle only, which significantly simplifies things; the
   * function calls have been unrolled, and most of the math is done in fixed
   * point.
   */

  clutter_actor_get_abs_allocation_vertices (self, v);

  /* Keeping these as ints simplifies the multiplication (no significant
   * loss of precision here).
   */
  du = (int) (priv->allocation.x2 - priv->allocation.x1);
  dv = (int) (priv->allocation.y2 - priv->allocation.y1);

  if (!du || !dv)
    return FALSE;

#define UX2FP(x)        (x)
#define DET2FP(a,b,c,d) (((a) * (d)) - ((b) * (c)))

  /* First, find mapping from unit uv square to xy quadrilateral; this
   * equivalent to the pmap_square_quad() functions in the sample
   * implementation, which we can simplify, since our target is always
   * a rectangle.
   */
  px = v[0].x - v[1].x + v[3].x - v[2].x;
  py = v[0].y - v[1].y + v[3].y - v[2].y;

  if (!px && !py)
    {
      /* affine transform */
      RQ[0][0] = UX2FP (v[1].x - v[0].x);
      RQ[1][0] = UX2FP (v[3].x - v[1].x);
      RQ[2][0] = UX2FP (v[0].x);
      RQ[0][1] = UX2FP (v[1].y - v[0].y);
      RQ[1][1] = UX2FP (v[3].y - v[1].y);
      RQ[2][1] = UX2FP (v[0].y);
      RQ[0][2] = 0;
      RQ[1][2] = 0;
      RQ[2][2] = 1.0;
    }
  else
    {
      /* projective transform */
      double dx1, dx2, dy1, dy2, del;

      dx1 = UX2FP (v[1].x - v[3].x);
      dx2 = UX2FP (v[2].x - v[3].x);
      dy1 = UX2FP (v[1].y - v[3].y);
      dy2 = UX2FP (v[2].y - v[3].y);

      del = DET2FP (dx1, dx2, dy1, dy2);
      if (!del)
	return FALSE;

      /*
       * The division here needs to be done in floating point for
       * precisions reasons.
       */
      RQ[0][2] = (DET2FP (UX2FP (px), dx2, UX2FP (py), dy2) / del);
      RQ[1][2] = (DET2FP (dx1, UX2FP (px), dy1, UX2FP (py)) / del);
      RQ[1][2] = (DET2FP (dx1, UX2FP (px), dy1, UX2FP (py)) / del);
      RQ[2][2] = 1.0;
      RQ[0][0] = UX2FP (v[1].x - v[0].x) + (RQ[0][2] * UX2FP (v[1].x));
      RQ[1][0] = UX2FP (v[2].x - v[0].x) + (RQ[1][2] * UX2FP (v[2].x));
      RQ[2][0] = UX2FP (v[0].x);
      RQ[0][1] = UX2FP (v[1].y - v[0].y) + (RQ[0][2] * UX2FP (v[1].y));
      RQ[1][1] = UX2FP (v[2].y - v[0].y) + (RQ[1][2] * UX2FP (v[2].y));
      RQ[2][1] = UX2FP (v[0].y);
    }

  /*
   * Now combine with transform from our rectangle (u0,v0,u1,v1) to unit
   * square. Since our rectangle is based at 0,0 we only need to scale.
   */
  RQ[0][0] /= du;
  RQ[1][0] /= dv;
  RQ[0][1] /= du;
  RQ[1][1] /= dv;
  RQ[0][2] /= du;
  RQ[1][2] /= dv;

  /*
   * Now RQ is transform from uv rectangle to xy quadrilateral; we need an
   * inverse of that.
   */
  ST[0][0] = DET2FP (RQ[1][1], RQ[1][2], RQ[2][1], RQ[2][2]);
  ST[1][0] = DET2FP (RQ[1][2], RQ[1][0], RQ[2][2], RQ[2][0]);
  ST[2][0] = DET2FP (RQ[1][0], RQ[1][1], RQ[2][0], RQ[2][1]);
  ST[0][1] = DET2FP (RQ[2][1], RQ[2][2], RQ[0][1], RQ[0][2]);
  ST[1][1] = DET2FP (RQ[2][2], RQ[2][0], RQ[0][2], RQ[0][0]);
  ST[2][1] = DET2FP (RQ[2][0], RQ[2][1], RQ[0][0], RQ[0][1]);
  ST[0][2] = DET2FP (RQ[0][1], RQ[0][2], RQ[1][1], RQ[1][2]);
  ST[1][2] = DET2FP (RQ[0][2], RQ[0][0], RQ[1][2], RQ[1][0]);
  ST[2][2] = DET2FP (RQ[0][0], RQ[0][1], RQ[1][0], RQ[1][1]);

  /*
   * Check the resulting matrix is OK.
   */
  det = (RQ[0][0] * ST[0][0])
      + (RQ[0][1] * ST[0][1])
      + (RQ[0][2] * ST[0][2]);
  if (!det)
    return FALSE;

  /*
   * Now transform our point with the ST matrix; the notional w
   * coordinate is 1, hence the last part is simply added.
   */
  xi = (int) x;
  yi = (int) y;

  xf = xi * ST[0][0] + yi * ST[1][0] + ST[2][0];
  yf = xi * ST[0][1] + yi * ST[1][1] + ST[2][1];
  wf = xi * ST[0][2] + yi * ST[1][2] + ST[2][2];

  if (x_out)
    *x_out = xf / wf;

  if (y_out)
    *y_out = yf / wf;

#undef UX2FP
#undef DET2FP

  return TRUE;
}

/*
 * ClutterGeometry
 */

static ClutterGeometry*
clutter_geometry_copy (const ClutterGeometry *geometry)
{
  return g_slice_dup (ClutterGeometry, geometry);
}

static void
clutter_geometry_free (ClutterGeometry *geometry)
{
  if (G_LIKELY (geometry != NULL))
    g_slice_free (ClutterGeometry, geometry);
}

GType
clutter_geometry_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterGeometry"),
                                    (GBoxedCopyFunc) clutter_geometry_copy,
                                    (GBoxedFreeFunc) clutter_geometry_free);

  return our_type;
}

/*
 * ClutterVertices
 */

/**
 * clutter_vertex_new:
 * @x: X coordinate
 * @y: Y coordinate
 * @z: Z coordinate
 *
 * Creates a new #ClutterVertex for the point in 3D space
 * identified by the 3 coordinates @x, @y, @z
 *
 * Return value: the newly allocate #ClutterVertex. Use
 *   clutter_vertex_free() to free the resources
 *
 * Since: 1.0
 */
ClutterVertex *
clutter_vertex_new (gfloat x,
                    gfloat y,
                    gfloat z)
{
  ClutterVertex *vertex;

  vertex = g_slice_new (ClutterVertex);
  vertex->x = x;
  vertex->y = y;
  vertex->z = z;

  return vertex;
}

/**
 * clutter_vertex_copy:
 * @vertex: a #ClutterVertex
 *
 * Copies @vertex
 *
 * Return value: a newly allocated copy of #ClutterVertex. Use
 *   clutter_vertex_free() to free the allocated resources
 *
 * Since: 1.0
 */
ClutterVertex *
clutter_vertex_copy (const ClutterVertex *vertex)
{
  if (G_LIKELY (vertex != NULL))
    return g_slice_dup (ClutterVertex, vertex);

  return NULL;
}

/**
 * clutter_vertex_free:
 * @vertex: a #ClutterVertex
 *
 * Frees a #ClutterVertex allocated using clutter_vertex_copy()
 *
 * Since: 1.0
 */
void
clutter_vertex_free (ClutterVertex *vertex)
{
  if (G_UNLIKELY (vertex != NULL))
    g_slice_free (ClutterVertex, vertex);
}

/**
 * clutter_vertex_equal:
 * @vertex_a: a #ClutterVertex
 * @vertex_b: a #ClutterVertex
 *
 * Compares @vertex_a and @vertex_b for equality
 *
 * Return value: %TRUE if the passed #ClutterVertex are equal
 *
 * Since: 1.0
 */
gboolean
clutter_vertex_equal (const ClutterVertex *vertex_a,
                      const ClutterVertex *vertex_b)
{
  g_return_val_if_fail (vertex_a != NULL && vertex_b != NULL, FALSE);

  if (vertex_a == vertex_b)
    return TRUE;

  return vertex_a->x == vertex_b->x &&
         vertex_a->y == vertex_b->y &&
         vertex_a->z == vertex_b->z;
}

GType
clutter_vertex_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterVertex"),
                                    (GBoxedCopyFunc) clutter_vertex_copy,
                                    (GBoxedFreeFunc) clutter_vertex_free);

  return our_type;
}

/*
 * ClutterActorBox
 */

/**
 * clutter_actor_box_new:
 * @x_1: X coordinate of the top left point
 * @y_1: Y coordinate of the top left point
 * @x_2: X coordinate of the bottom right point
 * @y_2: Y coordinate of the bottom right point
 *
 * Allocates a new #ClutterActorBox using the passed coordinates
 * for the top left and bottom right points
 *
 * Return value: the newly allocated #ClutterActorBox. Use
 *   clutter_actor_box_free() to free the resources
 *
 * Since: 1.0
 */
ClutterActorBox *
clutter_actor_box_new (gfloat x_1,
                       gfloat y_1,
                       gfloat x_2,
                       gfloat y_2)
{
  ClutterActorBox *box;

  box = g_slice_new (ClutterActorBox);
  box->x1 = x_1;
  box->y1 = y_1;
  box->x2 = x_2;
  box->y2 = y_2;

  return box;
}

/**
 * clutter_actor_box_copy:
 * @box: a #ClutterActorBox
 *
 * Copies @box
 *
 * Return value: a newly allocated copy of #ClutterActorBox. Use
 *   clutter_actor_box_free() to free the allocated resources
 *
 * Since: 1.0
 */
ClutterActorBox *
clutter_actor_box_copy (const ClutterActorBox *box)
{
  if (G_LIKELY (box != NULL))
    return g_slice_dup (ClutterActorBox, box);

  return NULL;
}

/**
 * clutter_actor_box_free:
 * @box: a #ClutterActorBox
 *
 * Frees a #ClutterActorBox allocated using clutter_actor_box_new()
 * or clutter_actor_box_copy()
 *
 * Since: 1.0
 */
void
clutter_actor_box_free (ClutterActorBox *box)
{
  if (G_LIKELY (box != NULL))
    g_slice_free (ClutterActorBox, box);
}

GType
clutter_actor_box_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterActorBox"),
                                    (GBoxedCopyFunc) clutter_actor_box_copy,
                                    (GBoxedFreeFunc) clutter_actor_box_free);
  return our_type;
}

/**
 * clutter_actor_box_equal:
 * @box_a: a #ClutterActorBox
 * @box_b: a #ClutterActorBox
 *
 * Checks @box_a and @box_b for equality
 *
 * Return value: %TRUE if the passed #ClutterActorBox are equal
 *
 * Since: 1.0
 */
gboolean
clutter_actor_box_equal (const ClutterActorBox *box_a,
                         const ClutterActorBox *box_b)
{
  g_return_val_if_fail (box_a != NULL && box_b != NULL, FALSE);

  if (box_a == box_b)
    return TRUE;

  return box_a->x1 == box_b->x1 && box_a->y1 == box_b->y1 &&
         box_a->x2 == box_b->x2 && box_a->y2 == box_b->y2;
}

/**
 * clutter_actor_box_get_x:
 * @box: a #ClutterActorBox
 *
 * Retrieves the X coordinate of the origin of @box
 *
 * Return value: the X coordinate of the origin
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_x (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->x1;
}

/**
 * clutter_actor_box_get_y:
 * @box: a #ClutterActorBox
 *
 * Retrieves the Y coordinate of the origin of @box
 *
 * Return value: the Y coordinate of the origin
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_y (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->y1;
}

/**
 * clutter_actor_box_get_width:
 * @box: a #ClutterActorBox
 *
 * Retrieves the width of the @box
 *
 * Return value: the width of the box
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_width (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->x2 - box->x1;
}

/**
 * clutter_actor_box_get_height:
 * @box: a #ClutterActorBox
 *
 * Retrieves the height of the @box
 *
 * Return value: the height of the box
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_height (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->y2 - box->y1;
}

/**
 * clutter_actor_box_get_origin:
 * @box: a #ClutterActorBox
 * @x: (out): return location for the X coordinate, or %NULL
 * @y: (out): return location for the Y coordinate, or %NULL
 *
 * Retrieves the origin of @box
 *
 * Since: 1.0
 */
void
clutter_actor_box_get_origin (const ClutterActorBox *box,
                              gfloat                *x,
                              gfloat                *y)
{
  g_return_if_fail (box != NULL);

  if (x)
    *x = box->x1;

  if (y)
    *y = box->y1;
}

/**
 * clutter_actor_box_get_size:
 * @box: a #ClutterActorBox
 * @width: (out): return location for the width, or %NULL
 * @height: (out): return location for the height, or %NULL
 *
 * Retrieves the size of @box
 *
 * Since: 1.0
 */
void
clutter_actor_box_get_size (const ClutterActorBox *box,
                            gfloat                *width,
                            gfloat                *height)
{
  g_return_if_fail (box != NULL);

  if (width)
    *width = box->x2 - box->x1;

  if (height)
    *height = box->y2 - box->y1;
}

/**
 * clutter_actor_box_get_area:
 * @box: a #ClutterActorBox
 *
 * Retrieves the area of @box
 *
 * Return value: the area of a #ClutterActorBox, in pixels
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_area (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return (box->x2 - box->x1) * (box->y2 - box->y1);
}

/**
 * clutter_actor_box_contains:
 * @box: a #ClutterActorBox
 * @x: X coordinate of the point
 * @y: Y coordinate of the point
 *
 * Checks whether a point with @x, @y coordinates is contained
 * withing @box
 *
 * Return value: %TRUE if the point is contained by the #ClutterActorBox
 *
 * Since: 1.0
 */
gboolean
clutter_actor_box_contains (const ClutterActorBox *box,
                            gfloat                 x,
                            gfloat                 y)
{
  g_return_val_if_fail (box != NULL, FALSE);

  return (x > box->x1 && x < box->x2) &&
         (y > box->y1 && y < box->y2);
}

/**
 * clutter_actor_box_from_vertices:
 * @box: a #ClutterActorBox
 * @verts: (array fixed-size=4): array of four #ClutterVertex
 *
 * Calculates the bounding box represented by the four vertices; for details
 * of the vertex array see clutter_actor_get_abs_allocation_vertices().
 *
 * Since: 1.0
 */
void
clutter_actor_box_from_vertices (ClutterActorBox     *box,
                                 const ClutterVertex  verts[])
{
  gfloat x_1, x_2, y_1, y_2;

  g_return_if_fail (box != NULL);
  g_return_if_fail (verts != NULL);

  /* 4-way min/max */
  x_1 = verts[0].x;
  y_1 = verts[0].y;

  if (verts[1].x < x_1)
    x_1 = verts[1].x;

  if (verts[2].x < x_1)
    x_1 = verts[2].x;

  if (verts[3].x < x_1)
    x_1 = verts[3].x;

  if (verts[1].y < y_1)
    y_1 = verts[1].y;

  if (verts[2].y < y_1)
    y_1 = verts[2].y;

  if (verts[3].y < y_1)
    y_1 = verts[3].y;

  x_2 = verts[0].x;
  y_2 = verts[0].y;

  if (verts[1].x > x_2)
    x_2 = verts[1].x;

  if (verts[2].x > x_2)
    x_2 = verts[2].x;

  if (verts[3].x > x_2)
    x_2 = verts[3].x;

  if (verts[1].y > y_2)
    y_2 = verts[1].y;

  if (verts[2].y > y_2)
    y_2 = verts[2].y;

  if (verts[3].y > y_2)
    y_2 = verts[3].y;

  box->x1 = x_1;
  box->x2 = x_2;
  box->y1 = y_1;
  box->y2 = y_2;
}

/******************************************************************************/

struct _ShaderData
{
  ClutterShader *shader;

  /* list of values that should be set on the shader
   * before each paint cycle
   */
  GHashTable *value_hash;
};

static void
shader_value_free (gpointer data)
{
  GValue *var = data;
  g_value_unset (var);
  g_slice_free (GValue, var);
}

static void
destroy_shader_data (ClutterActor *self)
{
  ClutterActorPrivate *actor_priv = self->priv;
  ShaderData *shader_data = actor_priv->shader_data;

  if (shader_data == NULL)
    return;

  if (shader_data->shader)
    {
      g_object_unref (shader_data->shader);
      shader_data->shader = NULL;
    }

  if (shader_data->value_hash)
    {
      g_hash_table_destroy (shader_data->value_hash);
      shader_data->value_hash = NULL;
    }

  g_slice_free (ShaderData, shader_data);
  actor_priv->shader_data = NULL;
}


/**
 * clutter_actor_get_shader:
 * @self: a #ClutterActor
 *
 * Queries the currently set #ClutterShader on @self.
 *
 * Return value: (transfer none): The currently set #ClutterShader or %NULL if no
 *   shader is set.
 *
 * Since: 0.6
 */
ClutterShader *
clutter_actor_get_shader (ClutterActor *self)
{
  ClutterActorPrivate *actor_priv;
  ShaderData     *shader_data;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  actor_priv = self->priv;
  shader_data = actor_priv->shader_data;

  if (shader_data == NULL)
    return NULL;

  return shader_data->shader;
}

/**
 * clutter_actor_set_shader:
 * @self: a #ClutterActor
 * @shader: a #ClutterShader or %NULL to unset the shader.
 *
 * Sets the #ClutterShader to be used when rendering @self.
 *
 * If @shader is %NULL it will unset any currently set shader
 * for the actor.
 *
 * Return value: %TRUE if the shader was successfully applied
 *
 * Since: 0.6
 */
gboolean
clutter_actor_set_shader (ClutterActor  *self,
                          ClutterShader *shader)
{
  ClutterActorPrivate *actor_priv;
  ShaderData          *shader_data;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);
  g_return_val_if_fail (shader == NULL || CLUTTER_IS_SHADER (shader), FALSE);

  if (shader != NULL)
    g_object_ref (shader);
  else
    {
      /* if shader passed in is NULL we destroy the shader */
      destroy_shader_data (self);
      return TRUE;
    }

  actor_priv = self->priv;
  shader_data = actor_priv->shader_data;

  if (shader_data == NULL)
    {
      actor_priv->shader_data = shader_data = g_slice_new (ShaderData);
      shader_data->shader = NULL;
      shader_data->value_hash =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free,
                               shader_value_free);
    }
  if (shader_data->shader != NULL)
    g_object_unref (shader_data->shader);

  shader_data->shader = shader;

  clutter_actor_queue_redraw (self);

  return TRUE;
}


static void
set_each_param (gpointer key,
                gpointer value,
                gpointer user_data)
{
  ClutterShader *shader      = user_data;
  GValue        *var         = value;

  clutter_shader_set_uniform (shader, (const gchar *)key, var);
}

static void
clutter_actor_shader_pre_paint (ClutterActor *actor,
                                gboolean      repeat)
{
  ClutterActorPrivate *priv;
  ShaderData          *shader_data;
  ClutterShader       *shader;
  ClutterMainContext  *context;

  priv = actor->priv;
  shader_data = priv->shader_data;

  if (!shader_data)
    return;

  context = _clutter_context_get_default ();
  shader = shader_data->shader;

  if (shader)
    {
      clutter_shader_set_is_enabled (shader, TRUE);

      g_hash_table_foreach (shader_data->value_hash, set_each_param, shader);

      if (!repeat)
        context->shaders = g_slist_prepend (context->shaders, actor);
    }
}

static void
clutter_actor_shader_post_paint (ClutterActor *actor)
{
  ClutterActorPrivate *priv;
  ShaderData          *shader_data;
  ClutterShader       *shader;
  ClutterMainContext  *context;

  priv = actor->priv;
  shader_data = priv->shader_data;

  if (!shader_data)
    return;

  context = _clutter_context_get_default ();
  shader = shader_data->shader;

  if (shader)
    {
      clutter_shader_set_is_enabled (shader, FALSE);

      context->shaders = g_slist_remove (context->shaders, actor);
      if (context->shaders)
        {
          /* call pre-paint again, this time with the second argument being
           * TRUE, indicating that we are reapplying the shader and thus
           * should not be prepended to the stack
           */
          clutter_actor_shader_pre_paint (context->shaders->data, TRUE);
        }
    }
}

/**
 * clutter_actor_set_shader_param:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named parameter of the shader applied
 * to @actor.
 *
 * Since: 1.0
 */
void
clutter_actor_set_shader_param (ClutterActor *self,
                                const gchar  *param,
                                const GValue *value)
{
  ClutterActorPrivate *priv;
  ShaderData *shader_data;
  GValue *var;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (param != NULL);
  g_return_if_fail (CLUTTER_VALUE_HOLDS_SHADER_FLOAT (value) ||
                    CLUTTER_VALUE_HOLDS_SHADER_INT (value) ||
                    CLUTTER_VALUE_HOLDS_SHADER_MATRIX (value) ||
                    G_VALUE_HOLDS_FLOAT (value) ||
                    G_VALUE_HOLDS_INT (value));

  priv = self->priv;
  shader_data = priv->shader_data;

  if (!shader_data)
    return;

  var = g_slice_new0 (GValue);
  g_value_init (var, G_VALUE_TYPE (value));
  g_value_copy (value, var);
  g_hash_table_insert (shader_data->value_hash, g_strdup (param), var);

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_set_shader_param_float:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named float parameter of the shader applied
 * to @actor.
 *
 * Since: 0.8
 */
void
clutter_actor_set_shader_param_float (ClutterActor *self,
                                      const gchar  *param,
                                      gfloat        value)
{
  GValue var = { 0, };

  g_value_init (&var, G_TYPE_FLOAT);
  g_value_set_float (&var, value);

  clutter_actor_set_shader_param (self, param, &var);

  g_value_unset (&var);
}

/**
 * clutter_actor_set_shader_param_int:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named int parameter of the shader applied to
 * @actor.
 *
 * Since: 0.8
 */
void
clutter_actor_set_shader_param_int (ClutterActor *self,
                                    const gchar  *param,
                                    gint          value)
{
  GValue var = { 0, };

  g_value_init (&var, G_TYPE_INT);
  g_value_set_int (&var, value);

  clutter_actor_set_shader_param (self, param, &var);

  g_value_unset (&var);
}

/**
 * clutter_actor_is_rotated:
 * @self: a #ClutterActor
 *
 * Checks whether any rotation is applied to the actor.
 *
 * Return value: %TRUE if the actor is rotated.
 *
 * Since: 0.6
 */
gboolean
clutter_actor_is_rotated (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  priv = self->priv;

  if (priv->rxang || priv->ryang || priv->rzang)
    return TRUE;

  return FALSE;
}

/**
 * clutter_actor_is_scaled:
 * @self: a #ClutterActor
 *
 * Checks whether the actor is scaled in either dimension.
 *
 * Return value: %TRUE if the actor is scaled.
 *
 * Since: 0.6
 */
gboolean
clutter_actor_is_scaled (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  priv = self->priv;

  if (priv->scale_x != 1.0 || priv->scale_y != 1.0)
    return TRUE;

  return FALSE;
}

/**
 * clutter_actor_get_stage:
 * @actor: a #ClutterActor
 *
 * Retrieves the #ClutterStage where @actor is contained.
 *
 * Return value: (transfer none): the stage containing the actor, or %NULL
 *
 * Since: 0.8
 */
ClutterActor *
clutter_actor_get_stage (ClutterActor *actor)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  while (actor && !(CLUTTER_PRIVATE_FLAGS (actor) & CLUTTER_ACTOR_IS_TOPLEVEL))
    actor = clutter_actor_get_parent (actor);

  return actor;
}

/**
 * clutter_actor_allocate_available_size:
 * @self: a #ClutterActor
 * @x: the actor's X coordinate
 * @y: the actor's Y coordinate
 * @available_width: the maximum available width, or -1 to use the
 *   actor's natural width
 * @available_height: the maximum available height, or -1 to use the
 *   actor's natural height
 * @flags: flags controlling the allocation
 *
 * Allocates @self taking into account the #ClutterActor<!-- -->'s
 * preferred size, but limiting it to the maximum available width
 * and height provided.
 *
 * This function will do the right thing when dealing with the
 * actor's request mode.
 *
 * The implementation of this function is equivalent to:
 *
 * |[
 *   if (request_mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
 *     {
 *       clutter_actor_get_preferred_width (self, available_height,
 *                                          &amp;min_width,
 *                                          &amp;natural_width);
 *       width = CLAMP (natural_width, min_width, available_width);
 *
 *       clutter_actor_get_preferred_height (self, width,
 *                                           &amp;min_height,
 *                                           &amp;natural_height);
 *       height = CLAMP (natural_height, min_height, available_height);
 *     }
 *   else
 *     {
 *       clutter_actor_get_preferred_height (self, available_width,
 *                                           &amp;min_height,
 *                                           &amp;natural_height);
 *       height = CLAMP (natural_height, min_height, available_height);
 *
 *       clutter_actor_get_preferred_width (self, height,
 *                                          &amp;min_width,
 *                                          &amp;natural_width);
 *       width = CLAMP (natural_width, min_width, available_width);
 *     }
 *
 *   box.x1 = x; box.y1 = y;
 *   box.x2 = box.x1 + available_width;
 *   box.y2 = box.y1 + available_height;
 *   clutter_actor_allocate (self, &amp;box, flags);
 * ]|
 *
 * This function can be used by fluid layout managers to allocate
 * an actor's preferred size without making it bigger than the area
 * available for the container.
 *
 * Since: 1.0
 */
void
clutter_actor_allocate_available_size (ClutterActor           *self,
                                       gfloat                  x,
                                       gfloat                  y,
                                       gfloat                  available_width,
                                       gfloat                  available_height,
                                       ClutterAllocationFlags  flags)
{
  ClutterActorPrivate *priv;
  gfloat width, height;
  gfloat min_width, min_height;
  gfloat natural_width, natural_height;
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  width = height = 0.0;

  switch (priv->request_mode)
    {
    case CLUTTER_REQUEST_HEIGHT_FOR_WIDTH:
      clutter_actor_get_preferred_width (self, available_height,
                                         &min_width,
                                         &natural_width);
      width  = CLAMP (natural_width, min_width, available_width);

      clutter_actor_get_preferred_height (self, width,
                                          &min_height,
                                          &natural_height);
      height = CLAMP (natural_height, min_height, available_height);
      break;

    case CLUTTER_REQUEST_WIDTH_FOR_HEIGHT:
      clutter_actor_get_preferred_height (self, available_width,
                                          &min_height,
                                          &natural_height);
      height = CLAMP (natural_height, min_height, available_height);

      clutter_actor_get_preferred_width (self, height,
                                         &min_width,
                                         &natural_width);
      width  = CLAMP (natural_width, min_width, available_width);
      break;
    }


  box.x1 = x;
  box.y1 = y;
  box.x2 = box.x1 + width;
  box.y2 = box.y1 + height;
  clutter_actor_allocate (self, &box, flags);
}

/**
 * clutter_actor_allocate_preferred_size:
 * @self: a #ClutterActor
 * @flags: flags controlling the allocation
 *
 * Allocates the natural size of @self.
 *
 * This function is a utility call for #ClutterActor implementations
 * that allocates the actor's preferred natural size. It can be used
 * by fixed layout managers (like #ClutterGroup or so called
 * 'composite actors') inside the ClutterActor::allocate
 * implementation to give each child exactly how much space it
 * requires.
 *
 * This function is not meant to be used by applications. It is also
 * not meant to be used outside the implementation of the
 * ClutterActor::allocate virtual function.
 *
 * Since: 0.8
 */
void
clutter_actor_allocate_preferred_size (ClutterActor           *self,
                                       ClutterAllocationFlags  flags)
{
  gfloat actor_x, actor_y;
  gfloat natural_width, natural_height;
  ClutterActorBox actor_box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  actor_x = clutter_actor_get_x (self);
  actor_y = clutter_actor_get_y (self);

  clutter_actor_get_preferred_size (self,
                                    NULL, NULL,
                                    &natural_width,
                                    &natural_height);

  actor_box.x1 = actor_x;
  actor_box.y1 = actor_y;
  actor_box.x2 = actor_box.x1 + natural_width;
  actor_box.y2 = actor_box.y1 + natural_height;

  clutter_actor_allocate (self, &actor_box, flags);
}

/**
 * clutter_actor_grab_key_focus:
 * @self: a #ClutterActor
 *
 * Sets the key focus of the #ClutterStage including @self
 * to this #ClutterActor.
 *
 * Since: 1.0
 */
void
clutter_actor_grab_key_focus (ClutterActor *self)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  parent = clutter_actor_get_parent (self);
  if (!parent)
    return;

  parent = clutter_actor_get_stage (self);
  if (parent && CLUTTER_IS_STAGE (parent))
    clutter_stage_set_key_focus (CLUTTER_STAGE (parent), self);
}

/**
 * clutter_actor_get_pango_context:
 * @self: a #ClutterActor
 *
 * Retrieves the #PangoContext for @self. The actor's #PangoContext
 * is already configured using the appropriate font map, resolution
 * and font options.
 *
 * Unlike clutter_actor_create_pango_context(), this context is owend
 * by the #ClutterActor and it will be updated each time the options
 * stored by the #ClutterBackend change.
 *
 * You can use the returned #PangoContext to create a #PangoLayout
 * and render text using cogl_pango_render_layout() to reuse the
 * glyphs cache also used by Clutter.
 *
 * Return value: (transfer none): the #PangoContext for a #ClutterActor.
 *   The returned #PangoContext is owned by the actor and should not be
 *   unreferenced by the application code
 *
 * Since: 1.0
 */
PangoContext *
clutter_actor_get_pango_context (ClutterActor *self)
{
  ClutterActorPrivate *priv;
  ClutterMainContext *ctx;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  priv = self->priv;

  if (priv->pango_context)
    return priv->pango_context;

  ctx = CLUTTER_CONTEXT ();
  priv->pango_context = _clutter_context_get_pango_context (ctx);
  g_object_ref (priv->pango_context);

  return priv->pango_context;
}

/**
 * clutter_actor_create_pango_context:
 * @self: a #ClutterActor
 *
 * Creates a #PangoContext for the given actor. The #PangoContext
 * is already configured using the appropriate font map, resolution
 * and font options.
 *
 * See also clutter_actor_get_pango_context().
 *
 * Return value: the newly created #PangoContext. Use g_object_unref()
 *   on the returned value to deallocate its resources
 *
 * Since: 1.0
 */
PangoContext *
clutter_actor_create_pango_context (ClutterActor *self)
{
  ClutterMainContext *ctx;
  PangoContext *retval;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  ctx = CLUTTER_CONTEXT ();
  retval = _clutter_context_create_pango_context (ctx);

  return retval;
}

/**
 * clutter_actor_create_pango_layout:
 * @self: a #ClutterActor
 * @text: the text to set on the #PangoLayout, or %NULL
 *
 * Creates a new #PangoLayout from the same #PangoContext used
 * by the #ClutterActor. The #PangoLayout is already configured
 * with the font map, resolution and font options, and the
 * given @text.
 *
 * If you want to keep around a #PangoLayout created by this
 * function you will have to connect to the #ClutterBackend::font-changed
 * and #ClutterBackend::resolution-changed signals, and call
 * pango_layout_context_changed() in response to them.
 *
 * Return value: the newly created #PangoLayout. Use g_object_unref()
 *   when done
 *
 * Since: 1.0
 */
PangoLayout *
clutter_actor_create_pango_layout (ClutterActor *self,
                                   const gchar  *text)
{
  PangoContext *context;
  PangoLayout *layout;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  context = clutter_actor_get_pango_context (self);
  layout = pango_layout_new (context);

  if (text)
    pango_layout_set_text (layout, text, -1);

  return layout;
}

/* Allows overriding the parent traversed when querying an actors paint
 * opacity. Used by ClutterClone. */
void
_clutter_actor_set_opacity_parent (ClutterActor *self,
                                   ClutterActor *parent)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  self->priv->opacity_parent = parent;
}

/* Allows you to disable applying the actors model view transform during
 * a paint. Used by ClutterClone. */
void
_clutter_actor_set_enable_model_view_transform (ClutterActor *self,
                                                gboolean      enable)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  self->priv->enable_model_view_transform = enable;
}

void
_clutter_actor_set_enable_paint_unmapped (ClutterActor *self,
                                          gboolean      enable)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  priv->enable_paint_unmapped = enable;

  if (priv->enable_paint_unmapped)
    {
      /* Make sure that the parents of the widget are realized first;
       * otherwise checks in clutter_actor_update_map_state() will
       * fail.
       */
      clutter_actor_realize (self);

      clutter_actor_update_map_state (self, MAP_STATE_MAKE_MAPPED);
    }
  else
    {
      clutter_actor_update_map_state (self, MAP_STATE_MAKE_UNMAPPED);
    }
}

static void
clutter_anchor_coord_get_units (ClutterActor      *self,
                                const AnchorCoord *coord,
                                gfloat            *x,
                                gfloat            *y,
                                gfloat            *z)
{
  if (coord->is_fractional)
    {
      gfloat actor_width, actor_height;

      clutter_actor_get_size (self, &actor_width, &actor_height);

      if (x)
        *x = actor_width * coord->v.fraction.x;

      if (y)
        *y = actor_height * coord->v.fraction.y;

      if (z)
        *z = 0;
    }
  else
    {
      if (x)
        *x = coord->v.units.x;

      if (y)
        *y = coord->v.units.y;

      if (z)
        *z = coord->v.units.z;
    }
}

static void
clutter_anchor_coord_set_units (AnchorCoord *coord,
                                gfloat       x,
                                gfloat       y,
                                gfloat       z)
{
  coord->is_fractional = FALSE;
  coord->v.units.x = x;
  coord->v.units.y = y;
  coord->v.units.z = z;
}

static ClutterGravity
clutter_anchor_coord_get_gravity (AnchorCoord *coord)
{
  if (coord->is_fractional)
    {
      if (coord->v.fraction.x == 0.0)
        {
          if (coord->v.fraction.y == 0.0)
            return CLUTTER_GRAVITY_NORTH_WEST;
          else if (coord->v.fraction.y == 0.5)
            return CLUTTER_GRAVITY_WEST;
          else if (coord->v.fraction.y == 1.0)
            return CLUTTER_GRAVITY_SOUTH_WEST;
          else
            return CLUTTER_GRAVITY_NONE;
        }
      else if (coord->v.fraction.x == 0.5)
        {
          if (coord->v.fraction.y == 0.0)
            return CLUTTER_GRAVITY_NORTH;
          else if (coord->v.fraction.y == 0.5)
            return CLUTTER_GRAVITY_CENTER;
          else if (coord->v.fraction.y == 1.0)
            return CLUTTER_GRAVITY_SOUTH;
          else
            return CLUTTER_GRAVITY_NONE;
        }
      else if (coord->v.fraction.x == 1.0)
        {
          if (coord->v.fraction.y == 0.0)
            return CLUTTER_GRAVITY_NORTH_EAST;
          else if (coord->v.fraction.y == 0.5)
            return CLUTTER_GRAVITY_EAST;
          else if (coord->v.fraction.y == 1.0)
            return CLUTTER_GRAVITY_SOUTH_EAST;
          else
            return CLUTTER_GRAVITY_NONE;
        }
      else
        return CLUTTER_GRAVITY_NONE;
    }
  else
    return CLUTTER_GRAVITY_NONE;
}

static void
clutter_anchor_coord_set_gravity (AnchorCoord    *coord,
                                  ClutterGravity  gravity)
{
  switch (gravity)
    {
    case CLUTTER_GRAVITY_NORTH:
      coord->v.fraction.x = 0.5;
      coord->v.fraction.y = 0.0;
      break;

    case CLUTTER_GRAVITY_NORTH_EAST:
      coord->v.fraction.x = 1.0;
      coord->v.fraction.y = 0.0;
      break;

    case CLUTTER_GRAVITY_EAST:
      coord->v.fraction.x = 1.0;
      coord->v.fraction.y = 0.5;
      break;

    case CLUTTER_GRAVITY_SOUTH_EAST:
      coord->v.fraction.x = 1.0;
      coord->v.fraction.y = 1.0;
      break;

    case CLUTTER_GRAVITY_SOUTH:
      coord->v.fraction.x = 0.5;
      coord->v.fraction.y = 1.0;
      break;

    case CLUTTER_GRAVITY_SOUTH_WEST:
      coord->v.fraction.x = 0.0;
      coord->v.fraction.y = 1.0;
      break;

    case CLUTTER_GRAVITY_WEST:
      coord->v.fraction.x = 0.0;
      coord->v.fraction.y = 0.5;
      break;

    case CLUTTER_GRAVITY_NORTH_WEST:
      coord->v.fraction.x = 0.0;
      coord->v.fraction.y = 0.0;
      break;

    case CLUTTER_GRAVITY_CENTER:
      coord->v.fraction.x = 0.5;
      coord->v.fraction.y = 0.5;
      break;

    default:
      coord->v.fraction.x = 0.0;
      coord->v.fraction.y = 0.0;
      break;
    }

  coord->is_fractional = TRUE;
}

static gboolean
clutter_anchor_coord_is_zero (const AnchorCoord *coord)
{
  if (coord->is_fractional)
    return coord->v.fraction.x == 0.0 && coord->v.fraction.y == 0.0;
  else
    return (coord->v.units.x == 0.0
            && coord->v.units.y == 0.0
            && coord->v.units.z == 0.0);
}

/**
 * clutter_actor_get_flags:
 * @self: a #ClutterActor
 *
 * Retrieves the flags set on @self
 *
 * Return value: a bitwise or of #ClutterActorFlags or 0
 *
 * Since: 1.0
 */
ClutterActorFlags
clutter_actor_get_flags (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  return self->flags;
}

/**
 * clutter_actor_set_flags:
 * @self: a #ClutterActor
 * @flags: the flags to set
 *
 * Sets @flags on @self
 *
 * This function will emit notifications for the changed properties
 *
 * Since: 1.0
 */
void
clutter_actor_set_flags (ClutterActor      *self,
                         ClutterActorFlags  flags)
{
  ClutterActorFlags old_flags;
  GObject *obj;
  gboolean was_reactive_set, reactive_set;
  gboolean was_realized_set, realized_set;
  gboolean was_mapped_set, mapped_set;
  gboolean was_visible_set, visible_set;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (self->flags == flags)
    return;

  obj = G_OBJECT (self);
  g_object_freeze_notify (obj);

  old_flags = self->flags;

  was_reactive_set = ((old_flags & CLUTTER_ACTOR_REACTIVE) != 0);
  was_realized_set = ((old_flags & CLUTTER_ACTOR_REALIZED) != 0);
  was_mapped_set   = ((old_flags & CLUTTER_ACTOR_MAPPED)   != 0);
  was_visible_set  = ((old_flags & CLUTTER_ACTOR_VISIBLE)  != 0);

  self->flags |= flags;

  reactive_set = ((self->flags & CLUTTER_ACTOR_REACTIVE) != 0);
  realized_set = ((self->flags & CLUTTER_ACTOR_REALIZED) != 0);
  mapped_set   = ((self->flags & CLUTTER_ACTOR_MAPPED)   != 0);
  visible_set  = ((self->flags & CLUTTER_ACTOR_VISIBLE)  != 0);

  if (reactive_set != was_reactive_set)
    g_object_notify (obj, "reactive");

  if (realized_set != was_realized_set)
    g_object_notify (obj, "realized");

  if (mapped_set != was_mapped_set)
    g_object_notify (obj, "mapped");

  if (visible_set != was_visible_set)
    g_object_notify (obj, "visible");

  g_object_thaw_notify (obj);
}

/**
 * clutter_actor_unset_flags:
 * @self: a #ClutterActor
 * @flags: the flags to unset
 *
 * Unsets @flags on @self
 *
 * This function will emit notifications for the changed properties
 *
 * Since: 1.0
 */
void
clutter_actor_unset_flags (ClutterActor      *self,
                           ClutterActorFlags  flags)
{
  ClutterActorFlags old_flags;
  GObject *obj;
  gboolean was_reactive_set, reactive_set;
  gboolean was_realized_set, realized_set;
  gboolean was_mapped_set, mapped_set;
  gboolean was_visible_set, visible_set;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  obj = G_OBJECT (self);
  g_object_freeze_notify (obj);

  old_flags = self->flags;

  was_reactive_set = ((old_flags & CLUTTER_ACTOR_REACTIVE) != 0);
  was_realized_set = ((old_flags & CLUTTER_ACTOR_REALIZED) != 0);
  was_mapped_set   = ((old_flags & CLUTTER_ACTOR_MAPPED)   != 0);
  was_visible_set  = ((old_flags & CLUTTER_ACTOR_VISIBLE)  != 0);

  self->flags &= ~flags;

  if (self->flags == old_flags)
    return;

  reactive_set = ((self->flags & CLUTTER_ACTOR_REACTIVE) != 0);
  realized_set = ((self->flags & CLUTTER_ACTOR_REALIZED) != 0);
  mapped_set   = ((self->flags & CLUTTER_ACTOR_MAPPED)   != 0);
  visible_set  = ((self->flags & CLUTTER_ACTOR_VISIBLE)  != 0);

  if (reactive_set != was_reactive_set)
    g_object_notify (obj, "reactive");

  if (realized_set != was_realized_set)
    g_object_notify (obj, "realized");

  if (mapped_set != was_mapped_set)
    g_object_notify (obj, "mapped");

  if (visible_set != was_visible_set)
    g_object_notify (obj, "visible");

  g_object_thaw_notify (obj);
}
