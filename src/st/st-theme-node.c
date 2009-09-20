/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <stdlib.h>
#include <string.h>

#include "st-theme-private.h"
#include "st-theme-context.h"
#include "st-theme-node.h"

static void st_theme_node_init               (StThemeNode          *node);
static void st_theme_node_class_init         (StThemeNodeClass     *klass);
static void st_theme_node_finalize           (GObject                 *object);

struct _StThemeNode {
  GObject parent;

  StThemeContext *context;
  StThemeNode *parent_node;
  StTheme *theme;

  PangoFontDescription *font_desc;

  ClutterColor background_color;
  ClutterColor foreground_color;
  ClutterColor border_color[4];
  double border_width[4];
  guint padding[4];

  char *background_image;
  StThemeImage *background_theme_image;

  GType element_type;
  char *element_id;
  char *element_class;
  char *pseudo_class;
  char *inline_style;

  CRDeclaration **properties;
  int n_properties;

  /* We hold onto these separately so we can destroy them on finalize */
  CRDeclaration *inline_properties;

  guint properties_computed : 1;
  guint borders_computed : 1;
  guint background_computed : 1;
  guint foreground_computed : 1;
  guint background_theme_image_computed : 1;
  guint link_type : 2;
};

struct _StThemeNodeClass {
  GObjectClass parent_class;

};

static const ClutterColor BLACK_COLOR = { 0, 0, 0, 0xff };
static const ClutterColor TRANSPARENT_COLOR = { 0, 0, 0, 0 };

G_DEFINE_TYPE (StThemeNode, st_theme_node, G_TYPE_OBJECT)

static void
st_theme_node_init (StThemeNode *node)
{
}

static void
st_theme_node_class_init (StThemeNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = st_theme_node_finalize;
}

static void
st_theme_node_finalize (GObject *object)
{
  StThemeNode *node = ST_THEME_NODE (object);

  g_free (node->element_id);
  g_free (node->element_class);
  g_free (node->pseudo_class);
  g_free (node->inline_style);

  if (node->properties)
    {
      g_free (node->properties);
      node->properties = NULL;
      node->n_properties = 0;
    }

  if (node->inline_properties)
    {
      /* This destroys the list, not just the head of the list */
      cr_declaration_destroy (node->inline_properties);
    }

  if (node->font_desc)
    {
      pango_font_description_free (node->font_desc);
      node->font_desc = NULL;
    }

  if (node->background_theme_image)
    {
      g_object_unref (node->background_theme_image);
      node->background_theme_image = NULL;
    }

  if (node->background_image)
    g_free (node->background_image);

  G_OBJECT_CLASS (st_theme_node_parent_class)->finalize (object);
}

/**
 * st_theme_node_new:
 * @context: the context representing global state for this themed tree
 * @parent_node: (allow-none): the parent node of this node
 * @theme: (allow-none): a theme (stylesheet set) that overrides the
 *   theme inherited from the parent node
 * @element_type: the type of the GObject represented by this node
 *  in the tree (corresponding to an element if we were theming an XML
 *  document. %G_TYPE_NONE means this style was created for the stage
 * actor and matches a selector element name of 'stage'.
 * @element_id: (allow-none): the ID to match CSS rules against
 * @element_class: (allow-none): a whitespace-separated list of classes
 *   to match CSS rules against
 * @pseudo_class: (allow-none): a whitespace-separated list of pseudo-classes
 *   (like 'hover' or 'visited') to match CSS rules against
 *
 * Creates a new #StThemeNode. Once created, a node is immutable. Of any
 * of the attributes of the node (like the @element_class) change the node
 * and its child nodes must be destroyed and recreated.
 *
 * Return value: (transfer full): the theme node
 */
StThemeNode *
st_theme_node_new (StThemeContext    *context,
                   StThemeNode       *parent_node,
                   StTheme           *theme,
                   GType              element_type,
                   const char        *element_id,
                   const char        *element_class,
                   const char        *pseudo_class,
                   const char        *inline_style)
{
  StThemeNode *node;

  g_return_val_if_fail (ST_IS_THEME_CONTEXT (context), NULL);
  g_return_val_if_fail (parent_node == NULL || ST_IS_THEME_NODE (parent_node), NULL);

  node = g_object_new (ST_TYPE_THEME_NODE, NULL);

  node->context = g_object_ref (context);
  if (parent_node != NULL)
    node->parent_node = g_object_ref (parent_node);
  else
    node->parent_node = NULL;

  if (theme == NULL && parent_node != NULL)
    theme = parent_node->theme;

  if (theme != NULL)
    node->theme = g_object_ref (theme);

  node->element_type = element_type;
  node->element_id = g_strdup (element_id);
  node->element_class = g_strdup (element_class);
  node->pseudo_class = g_strdup (pseudo_class);
  node->inline_style = g_strdup (inline_style);

  return node;
}

/**
 * st_theme_node_get_parent:
 * @node: a #StThemeNode
 *
 * Gets the parent themed element node.
 *
 * Return value: (transfer none): the parent #StThemeNode, or %NULL if this
 *  is the root node of the tree of theme elements.
 */
StThemeNode *
st_theme_node_get_parent (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  return node->parent_node;
}

/**
 * st_theme_node_get_theme:
 * @node: a #StThemeNode
 *
 * Gets the theme stylesheet set that styles this node
 *
 * Return value: (transfer none): the theme stylesheet set
 */
StTheme *
st_theme_node_get_theme (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  return node->theme;
}

GType
st_theme_node_get_element_type (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), G_TYPE_NONE);

  return node->element_type;
}

const char *
st_theme_node_get_element_id (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  return node->element_id;
}

const char *
st_theme_node_get_element_class (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  return node->element_class;
}

const char *
st_theme_node_get_pseudo_class (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  return node->pseudo_class;
}

static void
ensure_properties (StThemeNode *node)
{
  if (!node->properties_computed)
    {
      GPtrArray *properties = NULL;

      node->properties_computed = TRUE;

      if (node->theme)
        properties = _st_theme_get_matched_properties (node->theme, node);

      if (node->inline_style)
        {
          CRDeclaration *cur_decl;

          if (!properties)
            properties = g_ptr_array_new ();

          node->inline_properties = cr_declaration_parse_list_from_buf ((const guchar *)node->inline_style,
                                                                        CR_UTF_8);

          for (cur_decl = node->inline_properties; cur_decl; cur_decl = cur_decl->next)
            g_ptr_array_add (properties, cur_decl);
        }

      if (properties)
        {
          node->n_properties = properties->len;
          node->properties = (CRDeclaration **)g_ptr_array_free (properties, FALSE);
        }
    }
}

typedef enum {
  VALUE_FOUND,
  VALUE_NOT_FOUND,
  VALUE_INHERIT
} GetFromTermResult;

static gboolean
term_is_inherit (CRTerm *term)
{
  return (term->type == TERM_IDENT &&
          strcmp (term->content.str->stryng->str, "inherit") == 0);
}

static gboolean
term_is_none (CRTerm *term)
{
  return (term->type == TERM_IDENT &&
          strcmp (term->content.str->stryng->str, "none") == 0);
}

static gboolean
term_is_transparent (CRTerm *term)
{
  return (term->type == TERM_IDENT &&
          strcmp (term->content.str->stryng->str, "transparent") == 0);
}

static int
color_component_from_double (double component)
{
  /* We want to spread the range 0-1 equally over 0..255, but
   * 1.0 should map to 255 not 256, so we need to special-case it.
   * See http://people.redhat.com/otaylor/pixel-converting.html
   * for (very) detailed discussion of related issues. */
  if (component >= 1.0)
    return 255;
  else
    return (int)(component * 256);
}

static GetFromTermResult
get_color_from_rgba_term (CRTerm       *term,
                          ClutterColor *color)
{
  CRTerm *arg = term->ext_content.func_param;
  CRNum *num;
  double r = 0, g = 0, b = 0, a = 0;
  int i;

  for (i = 0; i < 4; i++)
    {
      double value;

      if (arg == NULL)
        return VALUE_NOT_FOUND;

      if ((i == 0 && arg->the_operator != NO_OP) ||
          (i > 0 && arg->the_operator != COMMA))
        return VALUE_NOT_FOUND;

      if (arg->type != TERM_NUMBER)
        return VALUE_NOT_FOUND;

      num = arg->content.num;

      /* For simplicity, we convert a,r,g,b to [0,1.0] floats and then
       * convert them back below. Then when we set them on a cairo content
       * we convert them back to floats, and then cairo converts them
       * back to integers to pass them to X, and so forth...
       */
      if (i < 3)
        {
          if (num->type == NUM_PERCENTAGE)
            value = num->val / 100;
          else if (num->type == NUM_GENERIC)
            value = num->val / 255;
          else
            return VALUE_NOT_FOUND;
        }
      else
        {
          if (num->type != NUM_GENERIC)
            return VALUE_NOT_FOUND;

          value = num->val;
        }

      value = CLAMP (value, 0, 1);

      switch (i)
        {
        case 0:
          r = value;
          break;
        case 1:
          g = value;
          break;
        case 2:
          b = value;
          break;
        case 3:
          a = value;
          break;
        }

      arg = arg->next;
    }

  color->red = color_component_from_double (r);
  color->green = color_component_from_double (g);
  color->blue = color_component_from_double (b);
  color->alpha = color_component_from_double (a);

  return VALUE_FOUND;
}

static GetFromTermResult
get_color_from_term (StThemeNode  *node,
                     CRTerm       *term,
                     ClutterColor *color)
{
  CRRgb rgb;
  enum CRStatus status;

  /* Since libcroco doesn't know about rgba colors, it can't handle
   * the transparent keyword
   */
  if (term_is_transparent (term))
    {
      *color = TRANSPARENT_COLOR;
      return VALUE_FOUND;
    }
  /* rgba () colors - a CSS3 addition, are not supported by libcroco,
   * but they are parsed as a "function", so we can emulate the
   * functionality.
   */
  else if (term->type == TERM_FUNCTION &&
           term->content.str &&
           term->content.str->stryng &&
           term->content.str->stryng->str &&
           strcmp (term->content.str->stryng->str, "rgba") == 0)
    {
      return get_color_from_rgba_term (term, color);
    }

  status = cr_rgb_set_from_term (&rgb, term);
  if (status != CR_OK)
    return VALUE_NOT_FOUND;

  if (rgb.inherit)
    return VALUE_INHERIT;

  if (rgb.is_percentage)
    cr_rgb_compute_from_percentage (&rgb);

  color->red = rgb.red;
  color->green = rgb.green;
  color->blue = rgb.blue;
  color->alpha = 0xff;

  return VALUE_FOUND;
}

/**
 * st_theme_node_get_color:
 * @node: a #StThemeNode
 * @property_name: The name of the color property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @color: (out): location to store the color that was determined.
 *   If the property is not found, the value in this location
 *   will not be changed.
 *
 * Generically looks up a property containing a single color value. When
 * specific getters (like st_theme_node_get_background_color()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_get_color (StThemeNode  *node,
                         const char   *property_name,
                         gboolean      inherit,
                         ClutterColor *color)
{

  int i;

  ensure_properties (node);

  for (i = node->n_properties - 1; i >= 0; i--)
    {
      CRDeclaration *decl = node->properties[i];

      if (strcmp (decl->property->stryng->str, property_name) == 0)
        {
          GetFromTermResult result = get_color_from_term (node, decl->value, color);
          if (result == VALUE_FOUND)
            {
              return TRUE;
            }
          else if (result == VALUE_INHERIT)
            {
              if (node->parent_node)
                return st_theme_node_get_color (node->parent_node, property_name, inherit, color);
              else
                break;
            }
        }
    }

  return FALSE;
}

/**
 * st_theme_node_get_double:
 * @node: a #StThemeNode
 * @property_name: The name of the numeric property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @value: (out): location to store the value that was determined.
 *   If the property is not found, the value in this location
 *   will not be changed.
 *
 * Generically looks up a property containing a single numeric value
 *  without units.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_get_double (StThemeNode *node,
                          const char  *property_name,
                          gboolean     inherit,
                          double      *value)
{
  gboolean result = FALSE;
  int i;

  ensure_properties (node);

  for (i = node->n_properties - 1; i >= 0; i--)
    {
      CRDeclaration *decl = node->properties[i];

      if (strcmp (decl->property->stryng->str, property_name) == 0)
        {
          CRTerm *term = decl->value;

          if (term->type != TERM_NUMBER || term->content.num->type != NUM_GENERIC)
            continue;

          *value = term->content.num->val;
          result = TRUE;
          break;
        }
    }

  if (!result && inherit && node->parent_node)
    result = st_theme_node_get_double (node->parent_node, property_name, inherit, value);

  return result;
}

static const PangoFontDescription *
get_parent_font (StThemeNode *node)
{
  if (node->parent_node)
    return st_theme_node_get_font (node->parent_node);
  else
    return st_theme_context_get_font (node->context);
}

static GetFromTermResult
get_length_from_term (StThemeNode *node,
                      CRTerm      *term,
                      gboolean     use_parent_font,
                      gdouble     *length)
{
  CRNum *num;

  enum {
    ABSOLUTE,
    POINTS,
    FONT_RELATIVE,
  } type = ABSOLUTE;

  double multiplier = 1.0;

  if (term->type != TERM_NUMBER)
    {
      g_warning ("Ignoring length property that isn't a number");
      return FALSE;
    }

  num = term->content.num;

  switch (num->type)
    {
    case NUM_LENGTH_PX:
      type = ABSOLUTE;
      multiplier = 1;
      break;
    case NUM_LENGTH_PT:
      type = POINTS;
      multiplier = 1;
      break;
    case NUM_LENGTH_IN:
      type = POINTS;
      multiplier = 72;
      break;
    case NUM_LENGTH_CM:
      type = POINTS;
      multiplier = 72. / 2.54;
      break;
    case NUM_LENGTH_MM:
      type = POINTS;
      multiplier = 72. / 25.4;
      break;
    case NUM_LENGTH_PC:
      type = POINTS;
      multiplier = 12. / 25.4;
      break;
    case NUM_LENGTH_EM:
      {
        type = FONT_RELATIVE;
        multiplier = 1;
        break;
      }
    case NUM_LENGTH_EX:
      {
        /* Doing better would require actually resolving the font description
         * to a specific font, and Pango doesn't have an ex metric anyways,
         * so we'd have to try and synthesize it by complicated means.
         *
         * The 0.5em is the CSS spec suggested thing to use when nothing
         * better is available.
         */
        type = FONT_RELATIVE;
        multiplier = 0.5;
        break;
      }

    case NUM_INHERIT:
      return VALUE_INHERIT;

    case NUM_AUTO:
      g_warning ("'auto' not supported for lengths");
      return VALUE_NOT_FOUND;

    case NUM_GENERIC:
      g_warning ("length values must specify a unit");
      return VALUE_NOT_FOUND;

    case NUM_PERCENTAGE:
      g_warning ("percentage lengths not currently supported");
      return VALUE_NOT_FOUND;

    case NUM_ANGLE_DEG:
    case NUM_ANGLE_RAD:
    case NUM_ANGLE_GRAD:
    case NUM_TIME_MS:
    case NUM_TIME_S:
    case NUM_FREQ_HZ:
    case NUM_FREQ_KHZ:
    case NUM_UNKNOWN_TYPE:
    case NB_NUM_TYPE:
      g_warning ("Ignoring invalid type of number of length property");
      return VALUE_NOT_FOUND;
    }

  switch (type)
    {
    case ABSOLUTE:
      *length = num->val * multiplier;
      break;
    case POINTS:
      {
        double resolution = st_theme_context_get_resolution (node->context);
        *length = num->val * multiplier * (resolution / 72.);
      }
      break;
    case FONT_RELATIVE:
      {
        const PangoFontDescription *desc;
        double font_size;

        if (use_parent_font)
          desc = get_parent_font (node);
        else
          desc = st_theme_node_get_font (node);

        font_size = (double)pango_font_description_get_size (desc) / PANGO_SCALE;

        if (pango_font_description_get_size_is_absolute (desc))
          {
            *length = num->val * multiplier * font_size;
          }
        else
          {
            double resolution = st_theme_context_get_resolution (node->context);
            *length = num->val * multiplier * (resolution / 72.) * font_size;
          }
      }
      break;
    default:
      g_assert_not_reached ();
    }

  return VALUE_FOUND;
}

static GetFromTermResult
get_length_internal (StThemeNode *node,
                     const char  *property_name,
                     const char  *suffixed,
                     gdouble     *length)
{
  int i;

  ensure_properties (node);

  for (i = node->n_properties - 1; i >= 0; i--)
    {
      CRDeclaration *decl = node->properties[i];

      if (strcmp (decl->property->stryng->str, property_name) == 0 ||
          (suffixed != NULL && strcmp (decl->property->stryng->str, suffixed) == 0))
        {
          GetFromTermResult result = get_length_from_term (node, decl->value, FALSE, length);
          if (result != VALUE_NOT_FOUND)
            return result;
        }
    }

  return VALUE_NOT_FOUND;
}

/**
 * st_theme_node_get_length:
 * @node: a #StThemeNode
 * @property_name: The name of the length property
 * @inherit: if %TRUE, if a value is not found for the property on the
 *   node, then it will be looked up on the parent node, and then on the
 *   parent's parent, and so forth. Note that if the property has a
 *   value of 'inherit' it will be inherited even if %FALSE is passed
 *   in for @inherit; this only affects the default behavior for inheritance.
 * @length: (out): location to store the length that was determined.
 *   If the property is not found, the value in this location
 *   will not be changed. The returned length is resolved
 *   to pixels.
 *
 * Generically looks up a property containing a single length value. When
 * specific getters (like st_theme_node_get_border_width()) exist, they
 * should be used instead. They are cached, so more efficient, and have
 * handling for shortcut properties and other details of CSS.
 *
 * Return value: %TRUE if the property was found in the properties for this
 *  theme node (or in the properties of parent nodes when inheriting.)
 */
gboolean
st_theme_node_get_length (StThemeNode *node,
                          const char  *property_name,
                          gboolean     inherit,
                          gdouble     *length)
{
  GetFromTermResult result = get_length_internal (node, property_name, NULL, length);
  if (result == VALUE_FOUND)
    return TRUE;
  else if (result == VALUE_INHERIT)
    inherit = TRUE;

  if (inherit && node->parent_node &&
      st_theme_node_get_length (node->parent_node, property_name, inherit, length))
    return TRUE;
  else
    return FALSE;
}

static void
do_border_property (StThemeNode   *node,
                    CRDeclaration *decl)
{
  const char *property_name = decl->property->stryng->str + 6; /* Skip 'border' */
  StSide side = (StSide)-1;
  ClutterColor color;
  gboolean color_set = FALSE;
  double width;
  gboolean width_set = FALSE;
  int j;

  if (g_str_has_prefix (property_name, "-left"))
    {
      side = ST_SIDE_LEFT;
      property_name += 5;
    }
  else if (g_str_has_prefix (property_name, "-right"))
    {
      side = ST_SIDE_RIGHT;
      property_name += 6;
    }
  else if (g_str_has_prefix (property_name, "-top"))
    {
      side = ST_SIDE_TOP;
      property_name += 4;
    }
  else if (g_str_has_prefix (property_name, "-bottom"))
    {
      side = ST_SIDE_BOTTOM;
      property_name += 7;
    }

  if (strcmp (property_name, "") == 0)
    {
      /* Set value for width/color/node in any order */
      CRTerm *term;

      for (term = decl->value; term; term = term->next)
        {
          GetFromTermResult result;

          if (term->type == TERM_IDENT)
            {
              const char *ident = term->content.str->stryng->str;
              if (strcmp (ident, "none") == 0 || strcmp (ident, "hidden") == 0)
                {
                  width = 0.;
                  continue;
                }
              else if (strcmp (ident, "solid") == 0)
                {
                  /* The only thing we support */
                  continue;
                }
              else if (strcmp (ident, "dotted") == 0 ||
                       strcmp (ident, "dashed") == 0 ||
                       strcmp (ident, "solid") == 0 ||
                       strcmp (ident, "double") == 0 ||
                       strcmp (ident, "groove") == 0 ||
                       strcmp (ident, "ridge") == 0 ||
                       strcmp (ident, "inset") == 0 ||
                       strcmp (ident, "outset") == 0)
                {
                  /* Treat the same as solid */
                  continue;
                }

              /* Presumably a color, fall through */
            }

          if (term->type == TERM_NUMBER)
            {
              result = get_length_from_term (node, term, FALSE, &width);
              if (result != VALUE_NOT_FOUND)
                {
                  width_set = result == VALUE_FOUND;
                  continue;
                }
            }

          result = get_color_from_term (node, term, &color);
          if (result != VALUE_NOT_FOUND)
            {
              color_set = result == VALUE_FOUND;
              continue;
            }
        }

    }
  else if (strcmp (property_name, "-color") == 0)
    {
      if (decl->value == NULL || decl->value->next != NULL)
        return;

      if (get_color_from_term (node, decl->value, &color) == VALUE_FOUND)
        /* Ignore inherit */
        color_set = TRUE;
    }
  else if (strcmp (property_name, "-width") == 0)
    {
      if (decl->value == NULL || decl->value->next != NULL)
        return;

      if (get_length_from_term (node, decl->value, FALSE, &width) == VALUE_FOUND)
        /* Ignore inherit */
        width_set = TRUE;
    }

  if (side == (StSide)-1)
    {
      for (j = 0; j < 4; j++)
        {
          if (color_set)
            node->border_color[j] = color;
          if (width_set)
            node->border_width[j] = width;
        }
    }
  else
    {
      if (color_set)
        node->border_color[side] = color;
      if (width_set)
        node->border_width[side] = width;
    }
}

static void
do_padding_property_term (StThemeNode *node,
                          CRTerm      *term,
                          gboolean     left,
                          gboolean     right,
                          gboolean     top,
                          gboolean     bottom)
{
  gdouble value;

  if (get_length_from_term (node, term, FALSE, &value) != VALUE_FOUND)
    return;

  if (left)
    node->padding[ST_SIDE_LEFT] = value;
  if (right)
    node->padding[ST_SIDE_RIGHT] = value;
  if (top)
    node->padding[ST_SIDE_TOP] = value;
  if (bottom)
    node->padding[ST_SIDE_BOTTOM] = value;
}

static void
do_padding_property (StThemeNode   *node,
                     CRDeclaration *decl)
{
  const char *property_name = decl->property->stryng->str + 7; /* Skip 'padding' */

  if (strcmp (property_name, "") == 0)
    {
      /* Slight deviation ... if we don't understand some of the terms and understand others,
       * then we set the ones we understand and ignore the others instead of ignoring the
       * whole thing
       */
      if (decl->value == NULL) /* 0 values */
        return;
      else if (decl->value->next == NULL)
        { /* 1 value */
          do_padding_property_term (node, decl->value, TRUE, TRUE, TRUE, TRUE); /* left/right/top/bottom */
          return;
        }
      else if (decl->value->next->next == NULL)
        { /* 2 values */
          do_padding_property_term (node, decl->value,       FALSE, FALSE, TRUE,  TRUE);  /* top/bottom */
          do_padding_property_term (node, decl->value->next, TRUE, TRUE,   FALSE, FALSE); /* left/right */
        }
      else if (decl->value->next->next->next == NULL)
        { /* 3 values */
          do_padding_property_term (node, decl->value,             FALSE, FALSE, TRUE,  FALSE); /* top */
          do_padding_property_term (node, decl->value->next,       TRUE,  TRUE,  FALSE, FALSE); /* left/right */
          do_padding_property_term (node, decl->value->next->next, FALSE, FALSE, FALSE, TRUE);  /* bottom */
        }
      else if (decl->value->next->next->next == NULL)
        { /* 4 values */
          do_padding_property_term (node, decl->value,                   FALSE, FALSE, TRUE,  FALSE);
          do_padding_property_term (node, decl->value->next,             FALSE, TRUE, FALSE, FALSE); /* left */
          do_padding_property_term (node, decl->value->next->next,       FALSE, FALSE, FALSE, TRUE);
          do_padding_property_term (node, decl->value->next->next->next, TRUE,  FALSE, FALSE, TRUE); /* left */
        }
      else
        {
          g_warning ("Too many values for padding property");
          return;
        }
    }
  else
    {
      if (decl->value == NULL || decl->value->next != NULL)
        return;

      if (strcmp (property_name, "-left") == 0)
        do_padding_property_term (node, decl->value, TRUE,  FALSE, FALSE, FALSE);
      else if (strcmp (property_name, "-right") == 0)
        do_padding_property_term (node, decl->value, FALSE, TRUE,  FALSE, FALSE);
      else if (strcmp (property_name, "-top") == 0)
        do_padding_property_term (node, decl->value, FALSE, FALSE, TRUE,  FALSE);
      else if (strcmp (property_name, "-bottom") == 0)
        do_padding_property_term (node, decl->value, FALSE, FALSE, FALSE, TRUE);
    }
}

static void
ensure_borders (StThemeNode *node)
{
  int i, j;

  if (node->borders_computed)
    return;

  node->borders_computed = TRUE;

  ensure_properties (node);

  for (j = 0; j < 4; j++)
    {
      node->border_width[j] = 0;
      node->border_color[j] = TRANSPARENT_COLOR;
    }

  for (i = 0; i < node->n_properties; i++)
    {
      CRDeclaration *decl = node->properties[i];
      const char *property_name = decl->property->stryng->str;

      if (g_str_has_prefix (property_name, "border"))
        do_border_property (node, decl);
      else if (g_str_has_prefix (property_name, "padding"))
        do_padding_property (node, decl);
    }
}

double
st_theme_node_get_border_width (StThemeNode *node,
                                StSide       side)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0.);
  g_return_val_if_fail (side >= ST_SIDE_LEFT && side <= ST_SIDE_BOTTOM, 0.);

  ensure_borders (node);

  return node->border_width[side];
}

static GetFromTermResult
get_background_color_from_term (StThemeNode  *node,
                                CRTerm       *term,
                                ClutterColor *color)
{
  GetFromTermResult result = get_color_from_term (node, term, color);
  if (result == VALUE_NOT_FOUND)
    {
      if (term_is_transparent (term))
        {
          *color = TRANSPARENT_COLOR;
          return VALUE_FOUND;
        }
    }

  return result;
}

static void
ensure_background (StThemeNode *node)
{
  int i;

  if (node->background_computed)
    return;

  node->background_computed = TRUE;
  node->background_color = TRANSPARENT_COLOR;

  ensure_properties (node);

  for (i = 0; i < node->n_properties; i++)
    {
      CRDeclaration *decl = node->properties[i];
      const char *property_name = decl->property->stryng->str;

      if (g_str_has_prefix (property_name, "background"))
        property_name += 10;
      else
        continue;

      if (strcmp (property_name, "") == 0)
        {
          /* We're very liberal here ... if we recognize any term in the expression we take it, and
           * we ignore the rest. The actual specification is:
           *
           * background: [<'background-color'> || <'background-image'> || <'background-repeat'> || <'background-attachment'> || <'background-position'>] | inherit
           */

          CRTerm *term;
          /* background: property sets all terms to specified or default values */
          node->background_color = TRANSPARENT_COLOR;
          g_free (node->background_image);
          node->background_image = NULL;

          for (term = decl->value; term; term = term->next)
            {
              GetFromTermResult result = get_background_color_from_term (node, term, &node->background_color);
              if (result == VALUE_FOUND)
                {
                  /* color stored in node->background_color */
                }
              else if (result == VALUE_INHERIT)
                {
                  if (node->parent_node)
                    {
                      st_theme_node_get_background_color (node->parent_node, &node->background_color);
                      node->background_image = g_strdup (st_theme_node_get_background_image (node->parent_node));
                    }
                }
              else if (term_is_none (term))
                {
                  /* leave node->background_color as transparent */
                }
              else if (term->type == TERM_URI)
                {
                  node->background_image = _st_theme_resolve_url (node->theme,
                                                                     decl->parent_statement->parent_sheet,
                                                                     term->content.str->stryng->str);
                }
            }
        }
      else if (strcmp (property_name, "-color") == 0)
        {
          GetFromTermResult result;

          if (decl->value == NULL || decl->value->next != NULL)
            continue;

          result = get_background_color_from_term (node, decl->value, &node->background_color);
          if (result == VALUE_FOUND)
            {
              /* color stored in node->background_color */
            }
          else if (result == VALUE_INHERIT)
            {
              if (node->parent_node)
                st_theme_node_get_background_color (node->parent_node, &node->background_color);
            }
        }
      else if (strcmp (property_name, "-image") == 0)
        {
          if (decl->value == NULL || decl->value->next != NULL)
            continue;

          if (decl->value->type == TERM_URI)
            {
              g_free (node->background_image);
              node->background_image = _st_theme_resolve_url (node->theme,
                                                              decl->parent_statement->parent_sheet,
                                                              decl->value->content.str->stryng->str);
            }
          else if (term_is_inherit (decl->value))
            {
              g_free (node->background_image);
              node->background_image = g_strdup (st_theme_node_get_background_image (node->parent_node));
            }
          else if (term_is_none (decl->value))
            {
              g_free (node->background_image);
              node->background_image = NULL;
            }
        }
    }
}

void
st_theme_node_get_background_color (StThemeNode  *node,
                                    ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));

  ensure_background (node);

  *color = node->background_color;
}

const char *
st_theme_node_get_background_image (StThemeNode *node)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), NULL);

  ensure_background (node);

  return node->background_image;
}

void
st_theme_node_get_foreground_color (StThemeNode  *node,
                                    ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));

  if (!node->foreground_computed)
    {
      int i;

      node->foreground_computed = TRUE;

      ensure_properties (node);

      for (i = node->n_properties - 1; i >= 0; i--)
        {
          CRDeclaration *decl = node->properties[i];

          if (strcmp (decl->property->stryng->str, "color") == 0)
            {
              GetFromTermResult result = get_color_from_term (node, decl->value, &node->foreground_color);
              if (result == VALUE_FOUND)
                goto out;
              else if (result == VALUE_INHERIT)
                break;
            }
        }

      if (node->parent_node)
        st_theme_node_get_foreground_color (node->parent_node, &node->foreground_color);
      else
        node->foreground_color = BLACK_COLOR; /* default to black */
    }

 out:
  *color = node->foreground_color;
}

void
st_theme_node_get_border_color (StThemeNode  *node,
                                StSide        side,
                                ClutterColor *color)
{
  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (side >= ST_SIDE_LEFT && side <= ST_SIDE_BOTTOM);

  ensure_borders (node);

  *color = node->border_color[side];
}

double
st_theme_node_get_padding (StThemeNode *node,
                           StSide       side)
{
  g_return_val_if_fail (ST_IS_THEME_NODE (node), 0.);
  g_return_val_if_fail (side >= ST_SIDE_LEFT && side <= ST_SIDE_BOTTOM, 0.);

  ensure_borders (node);

  return node->padding[side];
}

StTextDecoration
st_theme_node_get_text_decoration (StThemeNode *node)
{
  int i;

  ensure_properties (node);

  for (i = node->n_properties - 1; i >= 0; i--)
    {
      CRDeclaration *decl = node->properties[i];

      if (strcmp (decl->property->stryng->str, "text-decoration") == 0)
        {
          CRTerm *term = decl->value;
          StTextDecoration decoration = 0;

          /* Specification is none | [ underline || overline || line-through || blink ] | inherit
           *
           * We're a bit more liberal, and for example treat 'underline none' as the same as
           * none.
           */
          for (; term; term = term->next)
            {
              if (term->type != TERM_IDENT)
                goto next_decl;

              if (strcmp (term->content.str->stryng->str, "none") == 0)
                {
                  return 0;
                }
              else if (strcmp (term->content.str->stryng->str, "inherit") == 0)
                {
                  if (node->parent_node)
                    return st_theme_node_get_text_decoration (node->parent_node);
                }
              else if (strcmp (term->content.str->stryng->str, "underline") == 0)
                {
                  decoration |= ST_TEXT_DECORATION_UNDERLINE;
                }
              else if (strcmp (term->content.str->stryng->str, "overline") == 0)
                {
                  decoration |= ST_TEXT_DECORATION_OVERLINE;
                }
              else if (strcmp (term->content.str->stryng->str, "line-through") == 0)
                {
                  decoration |= ST_TEXT_DECORATION_LINE_THROUGH;
                }
              else if (strcmp (term->content.str->stryng->str, "blink") == 0)
                {
                  decoration |= ST_TEXT_DECORATION_BLINK;
                }
              else
                {
                  goto next_decl;
                }
            }

          return decoration;
        }

    next_decl:
      ;
    }

  return 0;
}

static gboolean
font_family_from_terms (CRTerm *term,
                        char  **family)
{
  GString *family_string;
  gboolean result = FALSE;
  gboolean last_was_quoted = FALSE;

  if (!term)
    return FALSE;

  family_string = g_string_new (NULL);

  while (term)
    {
      if (term->type != TERM_STRING && term->type != TERM_IDENT)
        {
          goto out;
        }

      if (family_string->len > 0)
        {
          if (term->the_operator != COMMA && term->the_operator != NO_OP)
            goto out;
          /* Can concatenate two bare words, but not two quoted strings */
          if ((term->the_operator == NO_OP && last_was_quoted) || term->type == TERM_STRING)
            goto out;

          if (term->the_operator == NO_OP)
            g_string_append (family_string, " ");
          else
            g_string_append (family_string, ", ");
        }
      else
        {
          if (term->the_operator != NO_OP)
            goto out;
        }

      g_string_append (family_string, term->content.str->stryng->str);

      term = term->next;
    }

  result = TRUE;

 out:
  if (result)
    {
      *family = g_string_free (family_string, FALSE);
      return TRUE;
    }
  else
    {
      *family = g_string_free (family_string, TRUE);
      return FALSE;
    }
}

/* In points */
static int font_sizes[] = {
  6 * 1024,   /* xx-small */
  8 * 1024,   /* x-small */
  10 * 1024,  /* small */
  12 * 1024,  /* medium */
  16 * 1024,  /* large */
  20 * 1024,  /* x-large */
  24 * 1024,  /* xx-large */
};

static gboolean
font_size_from_term (StThemeNode *node,
                     CRTerm      *term,
                     double      *size)
{
  if (term->type == TERM_IDENT)
    {
      double resolution = st_theme_context_get_resolution (node->context);
      /* We work in integers to avoid double comparisons when converting back
       * from a size in pixels to a logical size.
       */
      int size_points = (int)(0.5 + *size * (72. / resolution));

      if (strcmp (term->content.str->stryng->str, "xx-small") == 0)
        size_points = font_sizes[0];
      else if (strcmp (term->content.str->stryng->str, "x-small") == 0)
        size_points = font_sizes[1];
      else if (strcmp (term->content.str->stryng->str, "small") == 0)
        size_points = font_sizes[2];
      else if (strcmp (term->content.str->stryng->str, "medium") == 0)
        size_points = font_sizes[3];
      else if (strcmp (term->content.str->stryng->str, "large") == 0)
        size_points = font_sizes[4];
      else if (strcmp (term->content.str->stryng->str, "x-large") == 0)
        size_points = font_sizes[5];
      else if (strcmp (term->content.str->stryng->str, "xx-large") == 0)
        size_points = font_sizes[6];
      else if (strcmp (term->content.str->stryng->str, "smaller") == 0)
        {
          /* Find the standard size equal to or smaller than the current size */
          int i = 0;

          while (i <= 6 && font_sizes[i] < size_points)
            i++;

          if (i > 6)
            {
              /* original size greater than any standard size */
              size_points = (int)(0.5 + size_points / 1.2);
            }
          else
            {
              /* Go one smaller than that, if possible */
              if (i > 0)
                i--;

              size_points = font_sizes[i];
            }
        }
      else if (strcmp (term->content.str->stryng->str, "larger") == 0)
        {
          /* Find the standard size equal to or larger than the current size */
          int i = 6;

          while (i >= 0 && font_sizes[i] > size_points)
            i--;

          if (i < 0) /* original size smaller than any standard size */
            i = 0;

          /* Go one larger than that, if possible */
          if (i < 6)
            i++;

          size_points = font_sizes[i];
        }
      else
        {
          return FALSE;
        }

      *size = size_points * (resolution / 72.);
      return TRUE;

    }
  else if (term->type == TERM_NUMBER && term->content.num->type == NUM_PERCENTAGE)
    {
      *size *= term->content.num->val / 100.;
      return TRUE;
    }
  else if (get_length_from_term (node, term, TRUE, size) == VALUE_FOUND)
    {
      /* Convert from pixels to Pango units */
      *size *= 1024;
      return TRUE;
    }

  return FALSE;
}

static gboolean
font_weight_from_term (CRTerm      *term,
                       PangoWeight *weight,
                       gboolean    *weight_absolute)
{
  if (term->type == TERM_NUMBER)
    {
      int weight_int;

      /* The spec only allows numeric weights from 100-900, though Pango
       * will handle any number. We just let anything through.
       */
      if (term->content.num->type != NUM_GENERIC)
        return FALSE;

      weight_int = (int)(term->content.num->val + 0.5);

      *weight = weight_int;
      *weight_absolute = TRUE;

    }
  else if (term->type == TERM_IDENT)
    {
      /* FIXME: handle INHERIT */

      if (strcmp (term->content.str->stryng->str, "bold") == 0)
        {
          *weight = PANGO_WEIGHT_BOLD;
          *weight_absolute = TRUE;
        }
      else if (strcmp (term->content.str->stryng->str, "normal") == 0)
        {
          *weight = PANGO_WEIGHT_NORMAL;
          *weight_absolute = TRUE;
        }
      else if (strcmp (term->content.str->stryng->str, "bolder") == 0)
        {
          *weight = PANGO_WEIGHT_BOLD;
          *weight_absolute = FALSE;
        }
      else if (strcmp (term->content.str->stryng->str, "lighter") == 0)
        {
          *weight = PANGO_WEIGHT_LIGHT;
          *weight_absolute = FALSE;
        }
      else
        {
          return FALSE;
        }

    }
  else
    {
      return FALSE;
    }

  return TRUE;
}

static gboolean
font_style_from_term (CRTerm     *term,
                      PangoStyle *style)
{
  if (term->type != TERM_IDENT)
    return FALSE;

  /* FIXME: handle INHERIT */

  if (strcmp (term->content.str->stryng->str, "normal") == 0)
    *style = PANGO_STYLE_NORMAL;
  else if (strcmp (term->content.str->stryng->str, "oblique") == 0)
    *style = PANGO_STYLE_OBLIQUE;
  else if (strcmp (term->content.str->stryng->str, "italic") == 0)
    *style = PANGO_STYLE_ITALIC;
  else
    return FALSE;

  return TRUE;
}

static gboolean
font_variant_from_term (CRTerm       *term,
                        PangoVariant *variant)
{
  if (term->type != TERM_IDENT)
    return FALSE;

  /* FIXME: handle INHERIT */

  if (strcmp (term->content.str->stryng->str, "normal") == 0)
    *variant = PANGO_VARIANT_NORMAL;
  else if (strcmp (term->content.str->stryng->str, "small-caps") == 0)
    *variant = PANGO_VARIANT_SMALL_CAPS;
  else
    return FALSE;

  return TRUE;
}

const PangoFontDescription *
st_theme_node_get_font (StThemeNode *node)
{
  PangoStyle font_style;
  gboolean font_style_set = FALSE;
  PangoVariant variant;
  gboolean variant_set = FALSE;
  PangoWeight weight;
  gboolean weight_absolute;
  gboolean weight_set = FALSE;
  double parent_size;
  double size = 0.; /* Suppress warning */
  gboolean size_set = FALSE;
  char *family = NULL;
  int i;

  if (node->font_desc)
    return node->font_desc;

  node->font_desc = pango_font_description_copy (get_parent_font (node));
  parent_size = pango_font_description_get_size (node->font_desc);
  if (!pango_font_description_get_size_is_absolute (node->font_desc))
    {
      double resolution = st_theme_context_get_resolution (node->context);
      parent_size *= (resolution / 72.);
    }

  ensure_properties (node);

  for (i = 0; i < node->n_properties; i++)
    {
      CRDeclaration *decl = node->properties[i];

      if (strcmp (decl->property->stryng->str, "font") == 0)
        {
          PangoStyle tmp_style = PANGO_STYLE_NORMAL;
          PangoVariant tmp_variant = PANGO_VARIANT_NORMAL;
          PangoWeight tmp_weight = PANGO_WEIGHT_NORMAL;
          gboolean tmp_weight_absolute = TRUE;
          double tmp_size;
          CRTerm *term = decl->value;

          /* A font specification starts with node/variant/weight
           * in any order. Each is allowed to be specified only once,
           * but we don't enforce that.
           */
          for (; term; term = term->next)
            {
              if (font_style_from_term (term, &tmp_style))
                continue;
              if (font_variant_from_term (term, &tmp_variant))
                continue;
              if (font_weight_from_term (term, &tmp_weight, &tmp_weight_absolute))
                continue;

              break;
            }

          /* The size is mandatory */

          if (term == NULL || term->type != TERM_NUMBER)
            {
              g_warning ("Size missing from font property");
              continue;
            }

          tmp_size = parent_size;
          if (!font_size_from_term (node, term, &tmp_size))
            {
              g_warning ("Couldn't parse size in font property");
              continue;
            }

          term = term->next;

          if (term != NULL && term->type && TERM_NUMBER && term->the_operator == DIVIDE)
            {
              /* Ignore line-height specification */
              term = term->next;
            }

          /* the font family is mandatory - it is a comma-separated list of
           * names.
           */
          if (!font_family_from_terms (term, &family))
            {
              g_warning ("Couldn't parse family in font property");
              continue;
            }

          font_style = tmp_style;
          font_style_set = TRUE;
          weight = tmp_weight;
          weight_absolute = tmp_weight_absolute;
          weight_set = TRUE;
          variant = tmp_variant;
          variant_set = TRUE;

          size = tmp_size;
          size_set = TRUE;

        }
      else if (strcmp (decl->property->stryng->str, "font-family") == 0)
        {
          if (!font_family_from_terms (decl->value, &family))
            {
              g_warning ("Couldn't parse family in font property");
              continue;
            }
        }
      else if (strcmp (decl->property->stryng->str, "font-weight") == 0)
        {
          if (decl->value == NULL || decl->value->next != NULL)
            continue;

          if (font_weight_from_term (decl->value, &weight, &weight_absolute))
            weight_set = TRUE;
        }
      else if (strcmp (decl->property->stryng->str, "font-style") == 0)
        {
          if (decl->value == NULL || decl->value->next != NULL)
            continue;

          if (font_style_from_term (decl->value, &font_style))
            font_style_set = TRUE;
        }
      else if (strcmp (decl->property->stryng->str, "font-variant") == 0)
        {
          if (decl->value == NULL || decl->value->next != NULL)
            continue;

          if (font_variant_from_term (decl->value, &variant))
            variant_set = TRUE;
        }
      else if (strcmp (decl->property->stryng->str, "font-size") == 0)
        {
          gdouble tmp_size;
          if (decl->value == NULL || decl->value->next != NULL)
            continue;

          tmp_size = parent_size;
          if (font_size_from_term (node, decl->value, &tmp_size))
            {
              size = tmp_size;
              size_set = TRUE;
            }
        }
    }

  if (family)
    pango_font_description_set_family (node->font_desc, family);

  if (size_set)
    pango_font_description_set_absolute_size (node->font_desc, size);

  if (weight_set)
    {
      if (!weight_absolute)
        {
          /* bolder/lighter are supposed to switch between available styles, but with
           * font substitution, that gets to be a pretty fuzzy concept. So we use
           * a fixed step of 200. (The spec says 100, but that might not take us from
           * normal to bold.
           */

          PangoWeight old_weight = pango_font_description_get_weight (node->font_desc);
          if (weight == PANGO_WEIGHT_BOLD)
            weight = old_weight + 200;
          else
            weight = old_weight - 200;

          if (weight < 100)
            weight = 100;
          if (weight > 900)
            weight = 900;
        }

      pango_font_description_set_weight (node->font_desc, weight);
    }

  if (font_style_set)
    pango_font_description_set_style (node->font_desc, font_style);
  if (variant_set)
    pango_font_description_set_variant (node->font_desc, variant);

  return node->font_desc;
}

/**
 * st_theme_node_get_background_theme_image:
 * @node: a #StThemeNode
 *
 * Gets the value for the -st-background-image style property
 *
 * Return value: (transfer none): the background image, or %NULL
 *   if there is no background theme image.
 */
StThemeImage *
st_theme_node_get_background_theme_image (StThemeNode *node)
{
  int i;

  if (node->background_theme_image_computed)
    return node->background_theme_image;

  node->background_theme_image = NULL;
  node->background_theme_image_computed = TRUE;

  ensure_properties (node);

  for (i = node->n_properties - 1; i >= 0; i--)
    {
      CRDeclaration *decl = node->properties[i];

      if (strcmp (decl->property->stryng->str, "-st-background-image") == 0)
        {
          CRTerm *term = decl->value;
          int lengths[4];
          int n_lengths = 0;
          int i;

          const char *url;
          int border_top;
          int border_right;
          int border_bottom;
          int border_left;

          char *filename;

          /* First term must be the URL to the image */
          if (term->type != TERM_URI)
            goto next_property;

          url = term->content.str->stryng->str;

          term = term->next;

          /* Followed by 0 to 4 lengths */
          for (i = 0; i < 4; i++)
            {
              double value;

              if (term == NULL)
                break;

              if (get_length_from_term (node, term, FALSE, &value) != VALUE_FOUND)
                goto next_property;

              lengths[n_lengths] = (int)(0.5 + value);
              n_lengths++;

              term = term->next;
            }

          switch (n_lengths)
            {
            case 0:
              border_top = border_right = border_bottom = border_left = 0;
              break;
            case 1:
              border_top = border_right = border_bottom = border_left = lengths[0];
              break;
            case 2:
              border_top = border_bottom = lengths[0];
              border_left = border_right = lengths[1];
              break;
            case 3:
              border_top = lengths[0];
              border_left = border_right = lengths[1];
              border_bottom = lengths[2];
              break;
            case 4:
            default:
              border_top = lengths[0];
              border_right = lengths[1];
              border_bottom = lengths[2];
              border_left = lengths[3];
              break;
            }

          filename = _st_theme_resolve_url (node->theme, decl->parent_statement->parent_sheet, url);
          if (filename == NULL)
            goto next_property;

          node->background_theme_image = st_theme_image_new (filename,
                                                             border_top, border_right, border_bottom, border_left);

          g_free (filename);

          return node->background_theme_image;
        }

    next_property:
      ;
    }

  return NULL;
}
