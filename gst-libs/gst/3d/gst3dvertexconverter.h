/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
 *
 * gst3dvertexconverter.h: audio format conversion library
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

#ifndef __GST_3D_VERTEX_CONVERTER_H__
#define __GST_3D_VERTEX_CONVERTER_H__

#include <gst/gst.h>
#include "gst3dvertexinfo.h"

typedef struct _Gst3DVertexConverter Gst3DVertexConverter;

Gst3DVertexConverter *  gst_3d_vertex_converter_new                 (Gst3DVertexInfo *in_info,
                                                                     Gst3DVertexInfo *out_info,
                                                                     GstStructure *config);

void                    gst_3d_vertex_converter_free                (Gst3DVertexConverter * convert);

gboolean                gst_3d_vertex_converter_update_config       (Gst3DVertexConverter * convert,
                                                                     GstStructure *config);
const GstStructure *    gst_3d_vertex_converter_get_config          (Gst3DVertexConverter * convert);

gboolean                gst_3d_vertex_converter_vertices            (Gst3DVertexConverter * convert,
                                                                     GstBuffer *input,
                                                                     GstBuffer *output);

#endif /* __GST_3D_VERTEX_CONVERTER_H__ */
