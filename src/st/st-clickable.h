/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_CLICKABLE_H__
#define __ST_CLICKABLE_H__

#include "st-bin.h"

#define ST_TYPE_CLICKABLE                 (st_clickable_get_type ())
#define ST_CLICKABLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_CLICKABLE, StClickable))
#define ST_CLICKABLE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_CLICKABLE, StClickableClass))
#define ST_IS_CLICKABLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_CLICKABLE))
#define ST_IS_CLICKABLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_CLICKABLE))
#define ST_CLICKABLE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_CLICKABLE, StClickableClass))

typedef struct _StClickable        StClickable;
typedef struct _StClickableClass   StClickableClass;

typedef struct _StClickablePrivate StClickablePrivate;

struct _StClickable
{
    StBin parent;

    StClickablePrivate *priv;
};

struct _StClickableClass
{
    StBinClass parent_class;
};

GType st_clickable_get_type (void) G_GNUC_CONST;

void st_clickable_fake_release (StClickable *box);

#endif /* __ST_CLICKABLE_H__ */
