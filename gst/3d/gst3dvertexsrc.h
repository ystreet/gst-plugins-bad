/* 
 * GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_3D_VERTEX_SRC_H__
#define __GST_3D_VERTEX_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <gst/3d/3d.h>

G_BEGIN_DECLS

#define GST_TYPE_3D_VERTEX_SRC \
    (gst_3d_vertex_src_get_type())
#define GST_3D_VERTEX_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_3D_VERTEX_SRC,Gst3DVertexSrc))
#define GST_3D_VERTEX_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_3D_VERTEX_SRC,Gst3DVertexSrcClass))
#define GST_IS_3D_VERTEX_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_3D_VERTEX_SRC))
#define GST_IS_3D_VERTEX_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_3D_VERTEX_SRC))

typedef struct _Gst3DVertexSrc Gst3DVertexSrc;
typedef struct _Gst3DVertexSrcClass Gst3DVertexSrcClass;

/**
 * Gst3DVertexSrc:
 *
 * Opaque data structure.
 */
struct _Gst3DVertexSrc {
    GstPushSrc element;

    /*< private >*/
    gint64 timestamp_offset;              /* base offset */
    GstClockTime running_time;            /* total running time */
    gint64 n_frames;                      /* total frames sent */
    gboolean negotiated;

    Gst3DVertexInfo vinfo;

    GstCaps *out_caps;

    GstBuffer *buffer;
};

struct _Gst3DVertexSrcClass {
    GstPushSrcClass parent_class;
};

GType gst_3d_vertex_src_get_type (void);

G_END_DECLS

#endif /* __GST_3D_VERTEX_SRC_H__ */
