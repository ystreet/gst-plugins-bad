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

#ifndef __GST_3D_MATERIAL_META_H__
#define __GST_3D_MATERIAL_META_H__

#include <gst/gst.h>

#include <gst/3d/gst3dmaterialinfo.h>

G_BEGIN_DECLS

#define GST_3D_MATERIAL_META_API_TYPE (gst_3d_material_meta_api_get_type())
#define GST_3D_MATERIAL_META_INFO     (gst_3d_material_meta_get_info())

typedef struct _Gst3DMaterialMeta Gst3DMaterialMeta;

struct _Gst3DMaterialMeta
{
  GstMeta            meta;

  GstBuffer         *buffer;
  gint               id;

  Gst3DMaterialInfo  info;
  GArray            *stacks;                /* array of Gst3DMaterialStack's */
};

GType               gst_3d_material_meta_api_get_type (void);
const GstMetaInfo * gst_3d_material_meta_get_info (void);

Gst3DMaterialMeta *     gst_buffer_get_3d_material_meta     (GstBuffer * buffer,
                                                             gint id);
Gst3DMaterialMeta *     gst_buffer_add_3d_material_meta     (GstBuffer * buffer,
                                                             gint id,
                                                             Gst3DMaterialInfo * info);
Gst3DMaterialStack *    gst_3d_material_meta_get_stack      (Gst3DMaterialMeta * meta,
                                                             Gst3DMaterialElement element);

G_END_DECLS

#endif /* __GST_3D_MATERIAL_META_H__ */
