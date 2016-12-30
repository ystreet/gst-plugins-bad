/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
 *
 * gst3dvertexconvert.h: Convert vertices to different formats automatically
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

#ifndef __GST_3D_VERTEX_CONVERT_H__
#define __GST_3D_VERTEX_CONVERT_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/3d/3d.h>

GType gst_3d_vertex_convert_get_type (void);
#define GST_TYPE_3D_VERTEX_CONVERT            (gst_3d_vertex_convert_get_type())
#define GST_3D_VERTEX_CONVERT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_3D_VERTEX_CONVERT,Gst3DVertexConvert))
#define GST_3D_VERTEX_CONVERT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_3D_VERTEX_CONVERT,Gst3DVertexConvertClass))
#define GST_IS_3D_VERTEX_CONVERT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_3D_VERTEX_CONVERT))
#define GST_IS_3D_VERTEX_CONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_3D_VERTEX_CONVERT))

typedef struct _Gst3DVertexConvert Gst3DVertexConvert;
typedef struct _Gst3DVertexConvertClass Gst3DVertexConvertClass;

/**
 * Gst3DVertexConvert:
 *
 * The vertex convert object structure.
 */
struct _Gst3DVertexConvert
{
  GstBaseTransform element;

  Gst3DVertexInfo in_info;
  Gst3DVertexInfo out_info;
  Gst3DVertexConverter *convert;
};

struct _Gst3DVertexConvertClass
{
  GstBaseTransformClass parent_class;
};

#endif /* __GST_3D_VERTEX_CONVERT_H__ */
