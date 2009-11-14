/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_DRAWING_AREA_H__
#define __ST_DRAWING_AREA_H__

#include "st-bin.h"

#define ST_TYPE_DRAWING_AREA                 (st_drawing_area_get_type ())
#define ST_DRAWING_AREA(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ST_TYPE_DRAWING_AREA, StDrawingArea))
#define ST_DRAWING_AREA_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ST_TYPE_DRAWING_AREA, StDrawingAreaClass))
#define ST_IS_DRAWING_AREA(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ST_TYPE_DRAWING_AREA))
#define ST_IS_DRAWING_AREA_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ST_TYPE_DRAWING_AREA))
#define ST_DRAWING_AREA_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ST_TYPE_DRAWING_AREA, StDrawingAreaClass))

typedef struct _StDrawingArea        StDrawingArea;
typedef struct _StDrawingAreaClass   StDrawingAreaClass;

typedef struct _StDrawingAreaPrivate StDrawingAreaPrivate;

struct _StDrawingArea
{
    StBin parent;

    StDrawingAreaPrivate *priv;
};

struct _StDrawingAreaClass
{
    StBinClass parent_class;

    void (*redraw) (StDrawingArea *area, ClutterCairoTexture *texture);
};

GType st_drawing_area_get_type (void) G_GNUC_CONST;

ClutterCairoTexture *st_drawing_area_get_texture (StDrawingArea *area);

void st_drawing_area_emit_redraw (StDrawingArea *area);

#endif /* __ST_DRAWING_AREA_H__ */
