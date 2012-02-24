#include "cb-button.h"

/**
 * SECTION:cb-button
 * @short_description: Button widget
 *
 * A button widget with support for a text label and background color.
 */

/* convenience macro for GType implementations; see:
 * http://library.gnome.org/devel/gobject/2.27/gobject-Type-Information.html#G-DEFINE-TYPE:CAPS
 */
G_DEFINE_TYPE (CbButton, cb_button, CLUTTER_TYPE_ACTOR);

/* macro for accessing the object's private structure */
#define CB_BUTTON_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CB_TYPE_BUTTON, CbButtonPrivate))

/* private structure - should only be accessed through the public API;
 * this is used to store member variables whose properties
 * need to be accessible from the implementation; for example, if we
 * intend to create wrapper functions which modify properties on the
 * actors composing an object, we should keep a reference to the actors
 * here
 *
 * this is also the place where other state variables go:
 * for example, you might record the current state of the button
 * (toggled on or off) or a background image
 */
struct _CbButtonPrivate
{
  ClutterActor  *child;
  ClutterActor  *label;
  ClutterAction *click_action;
  gchar         *text;
};

/* enumerates property identifiers for this class;
 * note that property identifiers should be non-zero integers,
 * so we add an unused PROP_0 to occupy the 0 position in the enum
 */
enum {
  PROP_0,
  PROP_TEXT
};

/* enumerates signal identifiers for this class;
 * LAST_SIGNAL is not used as a signal identifier, but is instead
 * used to delineate the size of the cache array for signals (see below)
 */
enum {
  CLICKED,
  LAST_SIGNAL
};

/* cache array for signals */
static guint cb_button_signals[LAST_SIGNAL] = { 0, };

/* from http://mail.gnome.org/archives/gtk-devel-list/2004-July/msg00158.html:
 *
 * "The finalize method finishes releasing the remaining
 * resources just before the object itself will be freed from memory, and
 * therefore it will only be called once. The two step process helps break
 * cyclic references. Both dispose and finalize must chain up to their
 * parent objects by calling their parent's respective methods *after* they
 * have disposed or finalized their own members."
 */
static void
cb_button_finalize (GObject *gobject)
{
  CbButtonPrivate *priv = CB_BUTTON (gobject)->priv;

  g_free (priv->text);

  /* call the parent class' finalize() method */
  G_OBJECT_CLASS (cb_button_parent_class)->finalize (gobject);
}

/* enables objects to be uniformly treated as GObjects;
 * also exposes properties so they become scriptable, e.g.
 * through ClutterScript
 */
static void
cb_button_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  CbButton *button = CB_BUTTON (gobject);

  switch (prop_id)
    {
    case PROP_TEXT:
      cb_button_set_text (button, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

/* enables objects to be uniformly treated as GObjects */
static void
cb_button_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  CbButtonPrivate *priv = CB_BUTTON (gobject)->priv;

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

/* ClutterActor implementation
 *
 * we only implement destroy(), get_preferred_height(), get_preferred_width(),
 * allocate(), and paint(), as this is the minimum we can get away with
 */

/* composite actors should implement destroy(), and inside their
 * implementation destroy any actors they are composed from;
 * in this case, we just destroy the child ClutterBox
 */
static void
cb_button_destroy (ClutterActor *self)
{
  CbButtonPrivate *priv = CB_BUTTON (self)->priv;

  /* we just destroy the child, and let the child
   * deal with destroying _its_ children; note that we have a guard
   * here in case the child has already been destroyed
   */
  if (priv->child)
    {
      clutter_actor_destroy (priv->child);
      priv->child = NULL;
    }

  /* chain up to destroy() on the parent ClutterActorClass;
   * note that we check the parent class has a destroy() implementation
   * before calling it
   */
  if (CLUTTER_ACTOR_CLASS (cb_button_parent_class)->destroy)
    CLUTTER_ACTOR_CLASS (cb_button_parent_class)->destroy (self);
}

/* get_preferred_height and get_preferred_width defer to the
 * internal ClutterBox, adding 20px padding on each axis;
 * min_*_p is the minimum height or width the actor should occupy
 * to be useful; natural_*_p is the height or width the actor
 * would occupy if not constrained
 *
 * note that if we required explicit sizing for CbButtons
 * (i.e. a developer must set their height and width),
 * we wouldn't need to implement these functions
 */
static void
cb_button_get_preferred_height (ClutterActor *self,
                                gfloat for_width,
                                gfloat *min_height_p,
                                gfloat *natural_height_p)
{
  CbButtonPrivate *priv = CB_BUTTON (self)->priv;

  clutter_actor_get_preferred_height (priv->child,
                                      for_width,
                                      min_height_p,
                                      natural_height_p);

  *min_height_p += 20.0;
  *natural_height_p += 20.0;
}

static void
cb_button_get_preferred_width (ClutterActor *self,
                               gfloat for_height,
                               gfloat *min_width_p,
                               gfloat *natural_width_p)
{
  CbButtonPrivate *priv = CB_BUTTON (self)->priv;

  clutter_actor_get_preferred_width (priv->child,
                                     for_height,
                                     min_width_p,
                                     natural_width_p);

  *min_width_p += 20.0;
  *natural_width_p += 20.0;
}

/* use the actor's allocation for the ClutterBox */
static void
cb_button_allocate (ClutterActor          *actor,
                    const ClutterActorBox *box,
                    ClutterAllocationFlags flags)
{
  CbButtonPrivate *priv = CB_BUTTON (actor)->priv;
  ClutterActorBox child_box = { 0, };

  /* set the allocation for the whole button */
  CLUTTER_ACTOR_CLASS (cb_button_parent_class)->allocate (actor, box, flags);

  /* make the child (the ClutterBox) fill the parent;
   * note that this allocation box is relative to the
   * coordinates of the whole button actor, so we can't just
   * use the box passed into this function; instead, it
   * is adjusted to span the whole of the actor, from its
   * top-left corner (0,0) to its bottom-right corner
   * (width,height)
   */
  child_box.x1 = 0.0;
  child_box.y1 = 0.0;
  child_box.x2 = clutter_actor_box_get_width (box);
  child_box.y2 = clutter_actor_box_get_height (box);

  clutter_actor_allocate (priv->child, &child_box, flags);
}

/* paint function implementation: just calls paint() on the ClutterBox */
static void
cb_button_paint (ClutterActor *actor)
{
  CbButtonPrivate *priv = CB_BUTTON (actor)->priv;

  clutter_actor_paint (priv->child);
}

/* proxy ClickAction signals so they become signals from the actor */
static void
cb_button_clicked (ClutterClickAction *action,
                   ClutterActor       *actor,
                   gpointer            user_data)
{
  /* emit signal via the cache array */
  g_signal_emit (actor, cb_button_signals[CLICKED], 0);
}

/* GObject class and instance initialization functions; note that
 * these have been placed after the Clutter implementation, as
 * they refer to the static function implementations above
 */

/* class init: attach functions to superclasses, define properties
 * and signals
 */
static void
cb_button_class_init (CbButtonClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->finalize = cb_button_finalize;
  gobject_class->set_property = cb_button_set_property;
  gobject_class->get_property = cb_button_get_property;

  actor_class->destroy = cb_button_destroy;
  actor_class->get_preferred_height = cb_button_get_preferred_height;
  actor_class->get_preferred_width = cb_button_get_preferred_width;
  actor_class->allocate = cb_button_allocate;
  actor_class->paint = cb_button_paint;

  g_type_class_add_private (klass, sizeof (CbButtonPrivate));

  /**
   * CbButton:text:
   *
   * The text shown on the #CbButton
   */
  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text of the button",
                               NULL,
                               G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TEXT, pspec);

  /**
   * CbButton::clicked:
   * @button: the #CbButton that emitted the signal
   *
   * The ::clicked signal is emitted when the internal #ClutterClickAction
   * associated with a #CbButton emits its own ::clicked signal
   */
  cb_button_signals[CLICKED] =
    g_signal_new ("clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (CbButtonClass, clicked),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

/* object init: create a private structure and pack
 * composed ClutterActors into it
 */
static void
cb_button_init (CbButton *self)
{
  CbButtonPrivate *priv;
  ClutterLayoutManager *layout;

  priv = self->priv = CB_BUTTON_GET_PRIVATE (self);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  /* the only child of this actor is a ClutterBox with a
   * ClutterBinLayout: painting and allocation of the actor basically
   * involves painting and allocating this child box
   */
  layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER,
                                   CLUTTER_BIN_ALIGNMENT_CENTER);

  priv->child = clutter_actor_new ();
  clutter_actor_set_layout_manager (priv->child, layout);

  /* set the parent of the ClutterBox to this instance */
  clutter_actor_add_child (CLUTTER_ACTOR (self), priv->child);

  /* add text label to the button; see the ClutterText API docs
   * for more information about available properties
   */
  priv->label = g_object_new (CLUTTER_TYPE_TEXT,
                              "line-alignment", PANGO_ALIGN_CENTER,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              NULL);

  clutter_actor_add_child (priv->child, priv->label);

  /* add a ClutterClickAction on this actor, so we can proxy its
   * "clicked" signal into a signal from this actor
   */
  priv->click_action = clutter_click_action_new ();
  clutter_actor_add_action (CLUTTER_ACTOR (self), priv->click_action);

  g_signal_connect (priv->click_action,
                    "clicked",
                    G_CALLBACK (cb_button_clicked),
                    NULL);
}

/* public API */
/* examples of public API functions which wrap functions
 * on internal actors
 */

/**
 * cb_button_set_text:
 * @self: a #CbButton
 * @text: the text to display on the button
 *
 * Set the text on the button
 */
void
cb_button_set_text (CbButton    *self,
                    const gchar *text)
{
  CbButtonPrivate *priv;

  /* public API should check its arguments;
   * see also g_return_val_if_fail for functions which
   * return a value
   */
  g_return_if_fail (CB_IS_BUTTON (self));

  priv = self->priv;

  g_free (priv->text);

  if (text)
    priv->text = g_strdup (text);
  else
    priv->text = g_strdup ("");

  /* call a function on the ClutterText inside the layout */
  clutter_text_set_text (CLUTTER_TEXT (priv->label), priv->text);
}

/**
 * cb_button_set_background_color:
 * @self: a #CbButton
 * @color: the #ClutterColor to use for the button's background
 *
 * Set the color of the button's background
 */
void
cb_button_set_background_color (CbButton           *self,
                                const ClutterColor *color)
{
  g_return_if_fail (CB_IS_BUTTON (self));

  clutter_actor_set_background_color (self->priv->child, color);
}

/**
 * cb_button_set_text_color:
 * @self: a #CbButton
 * @color: the #ClutterColor to use as the color for the button text
 *
 * Set the color of the text on the button
 */
void
cb_button_set_text_color (CbButton           *self,
                          const ClutterColor *color)
{
  g_return_if_fail (CB_IS_BUTTON (self));

  clutter_text_set_color (CLUTTER_TEXT (self->priv->label), color);
}

/**
 * cb_button_get_text:
 * @self: a #CbButton
 *
 * Get the text displayed on the button
 *
 * Returns: the button's text. This must not be freed by the application.
 */
const gchar *
cb_button_get_text (CbButton *self)
{
  g_return_val_if_fail (CB_IS_BUTTON (self), NULL);

  return self->priv->text;
}

/**
 * cb_button_new:
 *
 * Creates a new #CbButton instance
 *
 * Returns: a new #CbButton
 */
ClutterActor *
cb_button_new (void)
{
  return g_object_new (CB_TYPE_BUTTON, NULL);
}
