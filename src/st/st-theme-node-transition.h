/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_THEME_NODE_TRANSITION_H__
#define __ST_THEME_NODE_TRANSITION_H__

#include <clutter/clutter.h>

#include "st-widget.h"
#include "st-theme-node.h"

G_BEGIN_DECLS

#define ST_TYPE_THEME_NODE_TRANSITION         (st_theme_node_transition_get_type ())
#define ST_THEME_NODE_TRANSITION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ST_TYPE_THEME_NODE_TRANSITION, StThemeNodeTransition))
#define ST_IS_THEME_NODE_TRANSITION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ST_TYPE_THEME_NODE_TRANSITION))
#define ST_THEME_NODE_TRANSITION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    ST_TYPE_THEME_NODE_TRANSITION, StThemeNodeTransitionClass))
#define ST_IS_THEME_NODE_TRANSITION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    ST_TYPE_THEME_NODE_TRANSITION))
#define ST_THEME_NODE_TRANSITION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  ST_THEME_NODE_TRANSITION, StThemeNodeTransitionClass))

typedef struct _StThemeNodeTransition        StThemeNodeTransition;
typedef struct _StThemeNodeTransitionClass   StThemeNodeTransitionClass;
typedef struct _StThemeNodeTransitionPrivate StThemeNodeTransitionPrivate;

struct _StThemeNodeTransition {
  GObject parent;

  StThemeNodeTransitionPrivate *priv;
};

struct _StThemeNodeTransitionClass {
  GObjectClass parent_class;

  void (*completed) (StThemeNodeTransition *transition);
  void (*new_frame) (StThemeNodeTransition *transition);
};

GType st_theme_node_transition_get_type (void) G_GNUC_CONST;

StThemeNodeTransition *st_theme_node_transition_new (StThemeNode *from_node,
                                                     StThemeNode *to_node,
                                                     guint        duration);

void  st_theme_node_transition_update   (StThemeNodeTransition *transition,
                                         StThemeNode           *new_node);

void  st_theme_node_transition_paint    (StThemeNodeTransition *transition,
                                         ClutterActorBox       *allocation,
                                         guint8                 paint_opacity);

G_END_DECLS

#endif
