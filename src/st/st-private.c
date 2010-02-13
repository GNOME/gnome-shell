/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include "st-private.h"

/**
 * _st_actor_get_preferred_width:
 * @actor: a #ClutterActor
 * @for_height: as with clutter_actor_get_preferred_width()
 * @y_fill: %TRUE if @actor will fill its allocation vertically
 * @min_width_p: as with clutter_actor_get_preferred_width()
 * @natural_width_p: as with clutter_actor_get_preferred_width()
 *
 * Like clutter_actor_get_preferred_width(), but if @y_fill is %FALSE,
 * then it will compute a width request based on the assumption that
 * @actor will be given an allocation no taller than its natural
 * height.
 */
void
_st_actor_get_preferred_width  (ClutterActor *actor,
                                gfloat        for_height,
                                gboolean      y_fill,
                                gfloat       *min_width_p,
                                gfloat       *natural_width_p)
{
  if (!y_fill && for_height != -1)
    {
      ClutterRequestMode mode;
      gfloat natural_height;

      g_object_get (G_OBJECT (actor), "request-mode", &mode, NULL);
      if (mode == CLUTTER_REQUEST_WIDTH_FOR_HEIGHT)
        {
          clutter_actor_get_preferred_height (actor, -1, NULL, &natural_height);
          if (for_height > natural_height)
            for_height = natural_height;
        }
    }

  clutter_actor_get_preferred_width (actor, for_height, min_width_p, natural_width_p);
}

/**
 * _st_actor_get_preferred_height:
 * @actor: a #ClutterActor
 * @for_width: as with clutter_actor_get_preferred_height()
 * @x_fill: %TRUE if @actor will fill its allocation horizontally
 * @min_height_p: as with clutter_actor_get_preferred_height()
 * @natural_height_p: as with clutter_actor_get_preferred_height()
 *
 * Like clutter_actor_get_preferred_height(), but if @x_fill is
 * %FALSE, then it will compute a height request based on the
 * assumption that @actor will be given an allocation no wider than
 * its natural width.
 */
void
_st_actor_get_preferred_height (ClutterActor *actor,
                                gfloat        for_width,
                                gboolean      x_fill,
                                gfloat       *min_height_p,
                                gfloat       *natural_height_p)
{
  if (!x_fill && for_width != -1)
    {
      ClutterRequestMode mode;
      gfloat natural_width;

      g_object_get (G_OBJECT (actor), "request-mode", &mode, NULL);
      if (mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
        {
          clutter_actor_get_preferred_width (actor, -1, NULL, &natural_width);
          if (for_width > natural_width)
            for_width = natural_width;
        }
    }

  clutter_actor_get_preferred_height (actor, for_width, min_height_p, natural_height_p);
}

/**
 * _st_allocate_fill:
 * @parent: the parent #StWidget
 * @child: the child (not necessarily an #StWidget)
 * @childbox: total space that could be allocated to @child
 * @x_alignment: horizontal alignment within @childbox
 * @y_alignment: vertical alignment within @childbox
 * @x_fill: whether or not to fill @childbox horizontally
 * @y_fill: whether or not to fill @childbox vertically
 *
 * Given @childbox, containing the initial allocation of @child, this
 * adjusts the horizontal allocation if @x_fill is %FALSE, and the
 * vertical allocation if @y_fill is %FALSE, by:
 *
 *     - reducing the allocation if it is larger than @child's natural
 *       size.
 *
 *     - adjusting the position of the child within the allocation
 *       according to @x_alignment/@y_alignment (and flipping
 *       @x_alignment if @parent has %ST_TEXT_DIRECTION_RTL)
 *
 * If @x_fill and @y_fill are both %TRUE, or if @child's natural size
 * is larger than the initial allocation in @childbox, then @childbox
 * will be unchanged.
 *
 * If you are allocating children with _st_allocate_fill(), you should
 * determine their preferred sizes using
 * _st_actor_get_preferred_width() and
 * _st_actor_get_preferred_height(), not with the corresponding
 * Clutter methods.
 */
void
_st_allocate_fill (StWidget        *parent,
                   ClutterActor    *child,
                   ClutterActorBox *childbox,
                   StAlign          x_alignment,
                   StAlign          y_alignment,
                   gboolean         x_fill,
                   gboolean         y_fill)
{
  gfloat natural_width, natural_height;
  gfloat min_width, min_height;
  gfloat child_width, child_height;
  gfloat available_width, available_height;
  ClutterRequestMode request;
  gdouble x_align, y_align;

  available_width  = childbox->x2 - childbox->x1;
  available_height = childbox->y2 - childbox->y1;

  if (available_width < 0)
    {
      available_width = 0;
      childbox->x2 = childbox->x1;
    }

  if (available_height < 0)
    {
      available_height = 0;
      childbox->y2 = childbox->y1;
    }

  /* If we are filling both horizontally and vertically then we don't
   * need to do anything else.
   */
  if (x_fill && y_fill)
    return;

  _st_get_align_factors (parent, x_alignment, y_alignment,
                         &x_align, &y_align);

  /* The following is based on clutter_actor_get_preferred_size(), but
   * modified to cope with the fact that the available size may be
   * less than the preferred size.
   */
  request = CLUTTER_REQUEST_HEIGHT_FOR_WIDTH;
  g_object_get (G_OBJECT (child), "request-mode", &request, NULL);

  if (request == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
    {
      clutter_actor_get_preferred_width (child, available_height,
                                         &min_width,
                                         &natural_width);

      child_width = CLAMP (natural_width, min_width, available_width);

      clutter_actor_get_preferred_height (child, child_width,
                                          &min_height,
                                          &natural_height);

      child_height = CLAMP (natural_height, min_height, available_height);
    }
  else
    {
      clutter_actor_get_preferred_height (child, available_width,
                                          &min_height,
                                          &natural_height);

      child_height = CLAMP (natural_height, min_height, available_height);

      clutter_actor_get_preferred_width (child, child_height,
                                         &min_width,
                                         &natural_width);

      child_width = CLAMP (natural_width, min_width, available_width);
    }

  if (!x_fill)
    {
      childbox->x1 += (int)((available_width - child_width) * x_align);
      childbox->x2 = childbox->x1 + (int) child_width;
    }

  if (!y_fill)
    {
      childbox->y1 += (int)((available_height - child_height) * y_align);
      childbox->y2 = childbox->y1 + (int) child_height;
    }
}

/**
 * _st_get_align_factors:
 * @widget: an #StWidget
 * @x_align: an #StAlign
 * @y_align: an #StAlign
 * @x_align_out: (out) (allow-none): @x_align as a #gdouble
 * @y_align_out: (out) (allow-none): @y_align as a #gdouble
 *
 * Converts @x_align and @y_align to #gdouble values. If @widget has
 * %ST_TEXT_DIRECTION_RTL, the @x_align_out value will be flipped
 * relative to @x_align.
 */
void
_st_get_align_factors (StWidget *widget,
                       StAlign   x_align,
                       StAlign   y_align,
                       gdouble  *x_align_out,
                       gdouble  *y_align_out)
{
  if (x_align_out)
    {
      switch (x_align)
        {
        case ST_ALIGN_START:
          *x_align_out = 0.0;
          break;

        case ST_ALIGN_MIDDLE:
          *x_align_out = 0.5;
          break;

        case ST_ALIGN_END:
          *x_align_out = 1.0;
          break;

        default:
          g_warn_if_reached ();
          break;
        }

      if (st_widget_get_direction (widget) == ST_TEXT_DIRECTION_RTL)
        *x_align_out = 1.0 - *x_align_out;
    }

  if (y_align_out)
    {
      switch (y_align)
        {
        case ST_ALIGN_START:
          *y_align_out = 0.0;
          break;

        case ST_ALIGN_MIDDLE:
          *y_align_out = 0.5;
          break;

        case ST_ALIGN_END:
          *y_align_out = 1.0;
          break;

        default:
          g_warn_if_reached ();
          break;
        }
    }
}

/**
 * _st_set_text_from_style:
 * @text: Target #ClutterText
 * @theme_node: Source #StThemeNode
 *
 * Set various GObject properties of the @text object using
 * CSS information from @theme_node.
 */
void
_st_set_text_from_style (ClutterText *text,
                         StThemeNode *theme_node)
{

  ClutterColor color;
  StTextDecoration decoration;
  PangoAttrList *attribs;
  const PangoFontDescription *font;
  gchar *font_string;

  st_theme_node_get_foreground_color (theme_node, &color);
  clutter_text_set_color (text, &color);

  font = st_theme_node_get_font (theme_node);
  font_string = pango_font_description_to_string (font);
  clutter_text_set_font_name (text, font_string);
  g_free (font_string);

  attribs = pango_attr_list_new ();

  decoration = st_theme_node_get_text_decoration (theme_node);
  if (decoration & ST_TEXT_DECORATION_UNDERLINE)
    {
      PangoAttribute *underline = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      pango_attr_list_insert (attribs, underline);
    }
  if (decoration & ST_TEXT_DECORATION_LINE_THROUGH)
    {
      PangoAttribute *strikethrough = pango_attr_strikethrough_new (TRUE);
      pango_attr_list_insert (attribs, strikethrough);
    }
  /* Pango doesn't have an equivalent attribute for _OVERLINE, and we deliberately
   * skip BLINK (for now...)
   */

  clutter_text_set_attributes (text, attribs);

  pango_attr_list_unref (attribs);
}
