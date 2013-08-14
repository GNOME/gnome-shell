/**
 * SECTION:clutter-settings
 * @Title: ClutterSettings
 * @Short_Description: Settings configuration
 *
 * Clutter depends on some settings to perform operations like detecting
 * multiple button press events, or font options to render text.
 *
 * Usually, Clutter will strive to use the platform's settings in order
 * to be as much integrated as possible. It is, however, possible to
 * change these settings on a per-application basis, by using the
 * #ClutterSettings singleton object and setting its properties. It is
 * also possible, for toolkit developers, to retrieve the settings from
 * the #ClutterSettings properties when implementing new UI elements,
 * for instance the default font name.
 *
 * #ClutterSettings is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-settings.h"

#ifdef HAVE_PANGO_FT2
/* for pango_fc_font_map_cache_clear() */
#define PANGO_ENABLE_BACKEND
#include <pango/pangofc-fontmap.h>
#endif /* HAVE_PANGO_FT2 */

#include "clutter-debug.h"
#include "clutter-settings-private.h"
#include "clutter-stage-private.h"
#include "clutter-private.h"

#define DEFAULT_FONT_NAME       "Sans 12"

#define CLUTTER_SETTINGS_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_SETTINGS, ClutterSettingsClass))
#define CLUTTER_IS_SETTINGS_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_SETTINGS))
#define CLUTTER_SETTINGS_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_SETTINGS, ClutterSettingsClass))

/**
 * ClutterSettings:
 *
 * <structname>ClutterSettings</structname> is an opaque structure whose
 * members cannot be directly accessed.
 *
 * Since: 1.4
 */
struct _ClutterSettings
{
  GObject parent_instance;

  ClutterBackend *backend;

  gint double_click_time;
  gint double_click_distance;

  gint dnd_drag_threshold;

  gdouble resolution;

  gchar *font_name;
  gint font_dpi;

  gint xft_hinting;
  gint xft_antialias;
  gchar *xft_hint_style;
  gchar *xft_rgba;

  gint long_press_duration;

  guint last_fontconfig_timestamp;

  guint password_hint_time;

  gint window_scaling_factor;
  gint unscaled_font_dpi;
};

struct _ClutterSettingsClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,

  PROP_BACKEND,

  PROP_DOUBLE_CLICK_TIME,
  PROP_DOUBLE_CLICK_DISTANCE,

  PROP_DND_DRAG_THRESHOLD,

  PROP_FONT_NAME,

  PROP_FONT_ANTIALIAS,
  PROP_FONT_DPI,
  PROP_FONT_HINTING,
  PROP_FONT_HINT_STYLE,
  PROP_FONT_RGBA,

  PROP_LONG_PRESS_DURATION,

  PROP_FONTCONFIG_TIMESTAMP,

  PROP_PASSWORD_HINT_TIME,

  PROP_WINDOW_SCALING_FACTOR,
  PROP_UNSCALED_FONT_DPI,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterSettings, clutter_settings, G_TYPE_OBJECT);

static inline void
settings_update_font_options (ClutterSettings *self)
{
  cairo_hint_style_t hint_style = CAIRO_HINT_STYLE_NONE;
  cairo_antialias_t antialias_mode = CAIRO_ANTIALIAS_GRAY;
  cairo_subpixel_order_t subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
  cairo_font_options_t *options;

  if (self->backend == NULL)
    return;

  options = cairo_font_options_create ();

  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);

  if (self->xft_hinting >= 0 &&
      self->xft_hint_style == NULL)
    {
      hint_style = CAIRO_HINT_STYLE_NONE;
    }
  else if (self->xft_hint_style != NULL)
    {
      if (strcmp (self->xft_hint_style, "hintnone") == 0)
        hint_style = CAIRO_HINT_STYLE_NONE;
      else if (strcmp (self->xft_hint_style, "hintslight") == 0)
        hint_style = CAIRO_HINT_STYLE_SLIGHT;
      else if (strcmp (self->xft_hint_style, "hintmedium") == 0)
        hint_style = CAIRO_HINT_STYLE_MEDIUM;
      else if (strcmp (self->xft_hint_style, "hintfull") == 0)
        hint_style = CAIRO_HINT_STYLE_FULL;
    }

  cairo_font_options_set_hint_style (options, hint_style);

  if (self->xft_rgba)
    {
      if (strcmp (self->xft_rgba, "rgb") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
      else if (strcmp (self->xft_rgba, "bgr") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
      else if (strcmp (self->xft_rgba, "vrgb") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
      else if (strcmp (self->xft_rgba, "vbgr") == 0)
        subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
    }

  cairo_font_options_set_subpixel_order (options, subpixel_order);

  if (self->xft_antialias >= 0 && !self->xft_antialias)
    antialias_mode = CAIRO_ANTIALIAS_NONE;
  else if (subpixel_order != CAIRO_SUBPIXEL_ORDER_DEFAULT)
    antialias_mode = CAIRO_ANTIALIAS_SUBPIXEL;
  else if (self->xft_antialias >= 0)
    antialias_mode = CAIRO_ANTIALIAS_GRAY;

  cairo_font_options_set_antialias (options, antialias_mode);

  CLUTTER_NOTE (BACKEND, "New font options:\n"
                " - font-name:  %s\n"
                " - antialias:  %d\n"
                " - hinting:    %d\n"
                " - hint-style: %s\n"
                " - rgba:       %s\n",
                self->font_name != NULL ? self->font_name : DEFAULT_FONT_NAME,
                self->xft_antialias,
                self->xft_hinting,
                self->xft_hint_style != NULL ? self->xft_hint_style : "<null>",
                self->xft_rgba != NULL ? self->xft_rgba : "<null>");

  clutter_backend_set_font_options (self->backend, options);
  cairo_font_options_destroy (options);
}

static void
settings_update_font_name (ClutterSettings *self)
{
  CLUTTER_NOTE (BACKEND, "New font-name: %s", self->font_name);

  if (self->backend != NULL)
    g_signal_emit_by_name (self->backend, "font-changed");
}

static void
settings_update_resolution (ClutterSettings *self)
{
  if (self->unscaled_font_dpi > 0)
    self->resolution = (gdouble) self->unscaled_font_dpi / 1024.0;
  else if (self->font_dpi > 0)
    self->resolution = (gdouble) self->font_dpi / 1024.0;
  else
    self->resolution = 96.0;

  CLUTTER_NOTE (BACKEND, "New resolution: %.2f (%s)",
                self->resolution,
                self->unscaled_font_dpi > 0 ? "unscaled" : "scaled");

  if (self->backend != NULL)
    g_signal_emit_by_name (self->backend, "resolution-changed");
}

static void
settings_update_fontmap (ClutterSettings *self,
                         guint            stamp)
{
  if (self->backend == NULL)
    return;

#ifdef HAVE_PANGO_FT2
  CLUTTER_NOTE (BACKEND, "Update fontmaps (stamp: %d)", stamp);

  if (self->last_fontconfig_timestamp != stamp)
    {
      ClutterMainContext *context;
      gboolean update_needed = FALSE;

      context = _clutter_context_get_default ();

      /* If there is no font map yet then we don't need to do anything
       * because the config for fontconfig will be read when it is
       * created */
      if (context->font_map)
        {
          PangoFontMap *fontmap = PANGO_FONT_MAP (context->font_map);

          if (PANGO_IS_FC_FONT_MAP (fontmap) &&
              !FcConfigUptoDate (NULL))
            {
              pango_fc_font_map_cache_clear (PANGO_FC_FONT_MAP (fontmap));

              if (FcInitReinitialize ())
                update_needed = TRUE;
            }
        }

      self->last_fontconfig_timestamp = stamp;

      if (update_needed)
        g_signal_emit_by_name (self->backend, "font-changed");
    }
#endif /* HAVE_PANGO_FT2 */
}

static void
settings_update_window_scale (ClutterSettings *self)
{
  ClutterStageManager *manager;
  const GSList *stages, *l;

  manager = clutter_stage_manager_get_default ();
  stages = clutter_stage_manager_peek_stages (manager);
  for (l = stages; l != NULL; l = l->next)
    {
      ClutterStage *stage = l->data;

      _clutter_stage_set_scale_factor (stage, self->window_scaling_factor);
    }
}

static void
clutter_settings_finalize (GObject *gobject)
{
  ClutterSettings *self = CLUTTER_SETTINGS (gobject);

  g_free (self->font_name);
  g_free (self->xft_hint_style);
  g_free (self->xft_rgba);

  G_OBJECT_CLASS (clutter_settings_parent_class)->finalize (gobject);
}

static void
clutter_settings_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ClutterSettings *self = CLUTTER_SETTINGS (gobject);

  switch (prop_id)
    {
    case PROP_BACKEND:
      self->backend = g_value_get_object (value);
      break;

    case PROP_DOUBLE_CLICK_TIME:
      self->double_click_time = g_value_get_int (value);
      break;

    case PROP_DOUBLE_CLICK_DISTANCE:
      self->double_click_distance = g_value_get_int (value);
      break;

    case PROP_DND_DRAG_THRESHOLD:
      self->dnd_drag_threshold = g_value_get_int (value);
      break;

    case PROP_FONT_NAME:
      g_free (self->font_name);
      self->font_name = g_value_dup_string (value);
      settings_update_font_name (self);
      break;

    case PROP_FONT_ANTIALIAS:
      self->xft_antialias = g_value_get_int (value);
      settings_update_font_options (self);
      break;

    case PROP_FONT_DPI:
      self->font_dpi = g_value_get_int (value);
      settings_update_resolution (self);
      break;

    case PROP_FONT_HINTING:
      self->xft_hinting = g_value_get_int (value);
      settings_update_font_options (self);
      break;

    case PROP_FONT_HINT_STYLE:
      g_free (self->xft_hint_style);
      self->xft_hint_style = g_value_dup_string (value);
      settings_update_font_options (self);
      break;

    case PROP_FONT_RGBA:
      g_free (self->xft_rgba);
      self->xft_rgba = g_value_dup_string (value);
      settings_update_font_options (self);
      break;

    case PROP_LONG_PRESS_DURATION:
      self->long_press_duration = g_value_get_int (value);
      break;

    case PROP_FONTCONFIG_TIMESTAMP:
      settings_update_fontmap (self, g_value_get_uint (value));
      break;

    case PROP_PASSWORD_HINT_TIME:
      self->password_hint_time = g_value_get_uint (value);
      break;

    case PROP_WINDOW_SCALING_FACTOR:
      self->window_scaling_factor = g_value_get_int (value);
      settings_update_window_scale (self);
      break;

    case PROP_UNSCALED_FONT_DPI:
      self->unscaled_font_dpi = g_value_get_int (value);
      settings_update_resolution (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_settings_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ClutterSettings *self = CLUTTER_SETTINGS (gobject);

  switch (prop_id)
    {
    case PROP_DOUBLE_CLICK_TIME:
      g_value_set_int (value, self->double_click_time);
      break;

    case PROP_DOUBLE_CLICK_DISTANCE:
      g_value_set_int (value, self->double_click_distance);
      break;

    case PROP_DND_DRAG_THRESHOLD:
      g_value_set_int (value, self->dnd_drag_threshold);
      break;

    case PROP_FONT_NAME:
      g_value_set_string (value, self->font_name);
      break;

    case PROP_FONT_ANTIALIAS:
      g_value_set_int (value, self->xft_antialias);
      break;

    case PROP_FONT_DPI:
      g_value_set_int (value, self->resolution * 1024);
      break;

    case PROP_FONT_HINTING:
      g_value_set_int (value, self->xft_hinting);
      break;

    case PROP_FONT_HINT_STYLE:
      g_value_set_string (value, self->xft_hint_style);
      break;

    case PROP_FONT_RGBA:
      g_value_set_string (value, self->xft_rgba);
      break;

    case PROP_LONG_PRESS_DURATION:
      g_value_set_int (value, self->long_press_duration);
      break;

    case PROP_PASSWORD_HINT_TIME:
      g_value_set_uint (value, self->password_hint_time);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_settings_dispatch_properties_changed (GObject     *gobject,
                                              guint        n_pspecs,
                                              GParamSpec **pspecs)
{
  ClutterSettings *self = CLUTTER_SETTINGS (gobject);
  GObjectClass *klass;

  /* chain up to emit ::notify */
  klass = G_OBJECT_CLASS (clutter_settings_parent_class);
  klass->dispatch_properties_changed (gobject, n_pspecs, pspecs);

  /* emit settings-changed just once for multiple properties */
  if (self->backend != NULL)
    g_signal_emit_by_name (self->backend, "settings-changed");
}

static void
clutter_settings_class_init (ClutterSettingsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /**
   * ClutterSettings:backend:
   *
   * A back pointer to the #ClutterBackend
   *
   * Since: 1.4
   *
   * Deprecated: 1.10
   */
  obj_props[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "Backend",
                         "A pointer to the backend",
                         CLUTTER_TYPE_BACKEND,
                         CLUTTER_PARAM_WRITABLE |
                         G_PARAM_DEPRECATED |
                         G_PARAM_CONSTRUCT_ONLY);

  /**
   * ClutterSettings:double-click-time:
   *
   * The time, in milliseconds, that should elapse between button-press
   * events in order to increase the click count by 1.
   *
   * Since: 1.4
   */
  obj_props[PROP_DOUBLE_CLICK_TIME] =
    g_param_spec_int ("double-click-time",
                      P_("Double Click Time"),
                      P_("The time between clicks necessary to detect a multiple click"),
                      0, G_MAXINT,
                      250,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:double-click-distance:
   *
   * The maximum distance, in pixels, between button-press events that
   * determines whether or not to increase the click count by 1.
   *
   * Since: 1.4
   */
  obj_props[PROP_DOUBLE_CLICK_DISTANCE] =
    g_param_spec_int ("double-click-distance",
                      P_("Double Click Distance"),
                      P_("The distance between clicks necessary to detect a multiple click"),
                      0, G_MAXINT,
                      5,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:dnd-drag-threshold:
   *
   * The default distance that the cursor of a pointer device
   * should travel before a drag operation should start.
   *
   * Since: 1.8
   */
  obj_props[PROP_DND_DRAG_THRESHOLD] =
    g_param_spec_int ("dnd-drag-threshold",
                      P_("Drag Threshold"),
                      P_("The distance the cursor should travel before starting to drag"),
                      1, G_MAXINT,
                      8,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:font-name:
   *
   * The default font name that should be used by text actors, as
   * a string that can be passed to pango_font_description_from_string().
   *
   * Since: 1.4
   */
  obj_props[PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         P_("Font Name"),
                         P_("The description of the default font, as one that could be parsed by Pango"),
                         NULL,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:font-antialias:
   *
   * Whether or not to use antialiasing when rendering text; a value
   * of 1 enables it unconditionally; a value of 0 disables it
   * unconditionally; and -1 will use the system's default.
   *
   * Since: 1.4
   */
  obj_props[PROP_FONT_ANTIALIAS] =
    g_param_spec_int ("font-antialias",
                      P_("Font Antialias"),
                      P_("Whether to use antialiasing (1 to enable, 0 to disable, and -1 to use the default)"),
                      -1, 1,
                      -1,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:font-dpi:
   *
   * The DPI used when rendering text, as a value of 1024 * dots/inch.
   *
   * If set to -1, the system's default will be used instead
   *
   * Since: 1.4
   */
  obj_props[PROP_FONT_DPI] =
    g_param_spec_int ("font-dpi",
                      P_("Font DPI"),
                      P_("The resolution of the font, in 1024 * dots/inch, or -1 to use the default"),
                      -1, 1024 * 1024,
                      -1,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:unscaled-font-dpi:
   *
   * The DPI used when rendering unscaled text, as a value of 1024 * dots/inch.
   *
   * If set to -1, the system's default will be used instead
   *
   * Since: 1.4
   */
  obj_props[PROP_UNSCALED_FONT_DPI] =
    g_param_spec_int ("unscaled-font-dpi",
                      P_("Font DPI"),
                      P_("The resolution of the font, in 1024 * dots/inch, or -1 to use the default"),
                      -1, 1024 * 1024,
                      -1,
                      CLUTTER_PARAM_WRITABLE);

  /**
   * ClutterSettings:font-hinting:
   *
   * Whether or not to use hinting when rendering text; a value of 1
   * unconditionally enables it; a value of 0 unconditionally disables
   * it; and a value of -1 will use the system's default.
   *
   * Since: 1.4
   */
  obj_props[PROP_FONT_HINTING] =
    g_param_spec_int ("font-hinting",
                      P_("Font Hinting"),
                      P_("Whether to use hinting (1 to enable, 0 to disable and -1 to use the default)"),
                      -1, 1,
                      -1,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:font-hint-style:
   *
   * The style of the hinting used when rendering text. Valid values
   * are:
   * <itemizedlist>
   *   <listitem><simpara>hintnone</simpara></listitem>
   *   <listitem><simpara>hintslight</simpara></listitem>
   *   <listitem><simpara>hintmedium</simpara></listitem>
   *   <listitem><simpara>hintfull</simpara></listitem>
   * </itemizedlist>
   *
   * Since: 1.4
   */
  obj_props[PROP_FONT_HINT_STYLE] =
    g_param_spec_string ("font-hint-style",
                         P_("Font Hint Style"),
                         P_("The style of hinting (hintnone, hintslight, hintmedium, hintfull)"),
                         NULL,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:font-subpixel-order:
   *
   * The type of sub-pixel antialiasing used when rendering text. Valid
   * values are:
   * <itemizedlist>
   *   <listitem><simpara>none</simpara></listitem>
   *   <listitem><simpara>rgb</simpara></listitem>
   *   <listitem><simpara>bgr</simpara></listitem>
   *   <listitem><simpara>vrgb</simpara></listitem>
   *   <listitem><simpara>vbgr</simpara></listitem>
   * </itemizedlist>
   *
   * Since: 1.4
   */
  obj_props[PROP_FONT_RGBA] =
    g_param_spec_string ("font-subpixel-order",
                         P_("Font Subpixel Order"),
                         P_("The type of subpixel antialiasing (none, rgb, bgr, vrgb, vbgr)"),
                         NULL,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterSettings:long-press-duration:
   *
   * Sets the minimum duration for a press to be recognized as a long press
   * gesture. The duration is expressed in milliseconds.
   *
   * See also #ClutterClickAction:long-press-duration.
   *
   * Since: 1.8
   */
  obj_props[PROP_LONG_PRESS_DURATION] =
    g_param_spec_int ("long-press-duration",
                      P_("Long Press Duration"),
                      P_("The minimum duration for a long press gesture to be recognized"),
                      0, G_MAXINT,
                      500,
                      CLUTTER_PARAM_READWRITE);

  obj_props[PROP_WINDOW_SCALING_FACTOR] =
    g_param_spec_int ("window-scaling-factor",
                      P_("Window Scaling Factor"),
                      P_("The scaling factor to be applied to windows"),
                      1, G_MAXINT,
                      1,
                      CLUTTER_PARAM_WRITABLE);

  obj_props[PROP_FONTCONFIG_TIMESTAMP] =
    g_param_spec_uint ("fontconfig-timestamp",
                       P_("Fontconfig configuration timestamp"),
                       P_("Timestamp of the current fontconfig configuration"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_WRITABLE);

  /**
   * ClutterText:password-hint-time:
   *
   * How long should Clutter show the last input character in editable
   * ClutterText actors. The value is in milliseconds. A value of 0
   * disables showing the password hint. 600 is a good value for
   * enabling the hint.
   *
   * Since: 1.10
   */
  obj_props[PROP_PASSWORD_HINT_TIME] =
    g_param_spec_uint ("password-hint-time",
                       P_("Password Hint Time"),
                       P_("How long to show the last input character in hidden entries"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_READWRITE);

  gobject_class->set_property = clutter_settings_set_property;
  gobject_class->get_property = clutter_settings_get_property;
  gobject_class->dispatch_properties_changed =
    clutter_settings_dispatch_properties_changed;
  gobject_class->finalize = clutter_settings_finalize;
  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_settings_init (ClutterSettings *self)
{
  self->resolution = -1.0;

  self->double_click_time = 250;
  self->double_click_distance = 5;

  self->dnd_drag_threshold = 8;

  self->font_name = g_strdup (DEFAULT_FONT_NAME);

  self->xft_antialias = -1;
  self->xft_hinting = -1;
  self->xft_hint_style = NULL;
  self->xft_rgba = NULL;

  self->long_press_duration = 500;
}

/**
 * clutter_settings_get_default:
 *
 * Retrieves the singleton instance of #ClutterSettings
 *
 * Return value: (transfer none): the instance of #ClutterSettings. The
 *   returned object is owned by Clutter and it should not be unreferenced
 *   directly
 *
 * Since: 1.4
 */
ClutterSettings *
clutter_settings_get_default (void)
{
  static ClutterSettings *settings = NULL;

  if (G_UNLIKELY (settings == NULL))
    settings = g_object_new (CLUTTER_TYPE_SETTINGS, NULL);

  return settings;
}

void
_clutter_settings_set_backend (ClutterSettings *settings,
                               ClutterBackend  *backend)
{
  g_assert (CLUTTER_IS_SETTINGS (settings));
  g_assert (CLUTTER_IS_BACKEND (backend));

  settings->backend = backend;
}

#define SETTINGS_GROUP  "Settings"

void
_clutter_settings_read_from_key_file (ClutterSettings *settings,
                                      GKeyFile        *keyfile)
{
  GObjectClass *settings_class;
  GObject *settings_obj;
  GParamSpec **pspecs;
  guint n_pspecs, i;

  if (!g_key_file_has_group (keyfile, SETTINGS_GROUP))
    return;

  settings_obj = G_OBJECT (settings);
  settings_class = G_OBJECT_GET_CLASS (settings);
  pspecs = g_object_class_list_properties (settings_class, &n_pspecs);

  for (i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];
      const gchar *p_name = pspec->name;
      GType p_type = G_TYPE_FUNDAMENTAL (pspec->value_type);
      GValue value = G_VALUE_INIT;
      GError *key_error = NULL;

      g_value_init (&value, p_type);

      switch (p_type)
        {
        case G_TYPE_INT:
        case G_TYPE_UINT:
          {
            gint val;

            val = g_key_file_get_integer (keyfile,
                                          SETTINGS_GROUP, p_name,
                                          &key_error);
            if (p_type == G_TYPE_INT)
              g_value_set_int (&value, val);
            else
              g_value_set_uint (&value, val);
          }
          break;

        case G_TYPE_BOOLEAN:
          {
            gboolean val;

            val = g_key_file_get_boolean (keyfile,
                                          SETTINGS_GROUP, p_name,
                                          &key_error);
            g_value_set_boolean (&value, val);
          }
          break;

        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
          {
            gdouble val;

            val = g_key_file_get_double (keyfile,
                                         SETTINGS_GROUP, p_name,
                                         &key_error);
            if (p_type == G_TYPE_FLOAT)
              g_value_set_float (&value, val);
            else
              g_value_set_double (&value, val);
          }
          break;

        case G_TYPE_STRING:
          {
            gchar *val;

            val = g_key_file_get_string (keyfile,
                                         SETTINGS_GROUP, p_name,
                                         &key_error);
            g_value_take_string (&value, val);
          }
          break;
        }

      if (key_error != NULL &&
          key_error->domain != G_KEY_FILE_ERROR &&
          key_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND)
        {
          g_critical ("Unable to read the value for setting '%s': %s",
                      p_name,
                      key_error->message);
        }

      if (key_error == NULL)
        g_object_set_property (settings_obj, p_name, &value);
      else
        g_error_free (key_error);

      g_value_unset (&value);
    }

  g_free (pspecs);
}
