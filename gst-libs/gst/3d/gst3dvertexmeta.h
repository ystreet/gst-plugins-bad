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

#ifndef __GST_3D_VERTEX_META_H__
#define __GST_3D_VERTEX_META_H__

#include <gst/gst.h>

#include "gst3dvertexinfo.h"

G_BEGIN_DECLS

#define GST_3D_VERTEX_META_API_TYPE (gst_3d_vertex_meta_api_get_type())
#define GST_3D_VERTEX_META_INFO     (gst_3d_vertex_meta_get_info())

typedef struct _Gst3DVertexMeta Gst3DVertexMeta;

struct _Gst3DVertexMeta
{
  GstMeta            meta;

  GstBuffer         *buffer;
  gint               id;

  Gst3DVertexInfo    info;
  gsize              n_indices;
  gsize              index_offset;          /* where in @buffer the indices start */
  gsize              n_vertices;
  gsize              vertex_offset;         /* where in @buffer the vertices start */
};

typedef struct _Gst3DVertexMapInfo Gst3DVertexMapInfo;

struct _Gst3DVertexMapInfo
{
  GstMapInfo index_map;
  guint8 *index_data;
  gsize index_offset;       /* byte offset into index_map.data the indices start */

  GstMapInfo vertex_map;
  guint8 *vertex_data;
  gsize vertex_offset;      /* byte offset into vertex_map.data the vertices start */
};

GType               gst_3d_vertex_meta_api_get_type (void);
const GstMetaInfo * gst_3d_vertex_meta_get_info (void);

Gst3DVertexMeta *   gst_buffer_get_3d_vertex_meta       (GstBuffer * buffer,
                                                         gint id);
Gst3DVertexMeta *   gst_buffer_add_3d_vertex_meta       (GstBuffer * buffer,
                                                         gint id,
                                                         Gst3DVertexInfo * info,
                                                         gsize n_indices,
                                                         gsize n_vertices);
gboolean            gst_3d_vertex_meta_map              (Gst3DVertexMeta * meta,
                                                         Gst3DVertexMapInfo * map_info,
                                                         GstMapFlags map_flags);
void                gst_3d_vertex_meta_unmap            (Gst3DVertexMeta * meta,
                                                         Gst3DVertexMapInfo * map_info);

G_END_DECLS

#endif /* __GST_3D_VERTEX_META_H__ */
