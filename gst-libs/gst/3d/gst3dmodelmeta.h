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

#ifndef __GST_3D_MODEL_META_H__
#define __GST_3D_MODEL_META_H__

#include <gst/gst.h>

#include <gst/3d/gst3dvertexmeta.h>
#include <gst/3d/gst3dmaterialmeta.h>

G_BEGIN_DECLS

#define GST_3D_MODEL_META_API_TYPE (gst_3d_model_meta_api_get_type())
#define GST_3D_MODEL_META_INFO     (gst_3d_model_meta_get_info())

typedef struct _Gst3DModelMeta Gst3DModelMeta;

struct _Gst3DModelMeta
{
  GstMeta            meta;

  GstBuffer         *buffer;
  gint               id;

  gint               vertex_id;
  gint               material_id;
};

GType               gst_3d_model_meta_api_get_type (void);
const GstMetaInfo * gst_3d_model_meta_get_info (void);

Gst3DModelMeta *    gst_buffer_get_3d_model_meta            (GstBuffer * buffer,
                                                             gint id);
Gst3DModelMeta *    gst_buffer_add_3d_model_meta            (GstBuffer * buffer,
                                                             gint id,
                                                             gint vertex_id,
                                                             gint material_id);

gboolean            gst_3d_model_meta_get_vertices_material (Gst3DModelMeta * meta,
                                                             Gst3DVertexMeta ** vmeta,
                                                             Gst3DMaterialMeta ** mmeta);

GstCaps *           gst_3d_model_info_to_caps               (Gst3DVertexInfo * vinfo,
                                                             Gst3DMaterialInfo * minfo);
gboolean            gst_3d_model_info_from_caps             (GstCaps * caps,
                                                             Gst3DVertexInfo * vinfo,
                                                             Gst3DMaterialInfo * minfo);

G_END_DECLS

#endif /* __GST_3D_MODEL_META_H__ */
