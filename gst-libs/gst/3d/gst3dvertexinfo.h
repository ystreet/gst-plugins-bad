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

#ifndef __GST_3D_VERTEX_INFO_H__
#define __GST_3D_VERTEX_INFO_H__

#include <gst/gst.h>

#include "gst3dvertexformat.h"

G_BEGIN_DECLS

/* Provides information about 3D vertices as a list of up to 4 values.
 */

typedef enum
{
  GST_3D_VERTEX_TYPE_UNKNOWN,
  GST_3D_VERTEX_TYPE_CUSTOM,
  GST_3D_VERTEX_TYPE_POSITION,
  GST_3D_VERTEX_TYPE_NORMAL,
  GST_3D_VERTEX_TYPE_TEXTURE,
  /* FIXME: more explicit texture channels ?? */
} Gst3DVertexType;

#define GST_3D_VERTEX_TYPE_ALL " custom, position, normal, texture "

const gchar *       gst_3d_vertex_type_to_string        (Gst3DVertexType type);
Gst3DVertexType     gst_3d_vertex_type_from_string      (const gchar * name);

typedef enum
{
  GST_3D_VERTEX_PRIMITIVE_MODE_UNKNOWN,
  GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLES,
  GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLE_STRIP,
  GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLE_FAN,
  /* etc... */
} Gst3DVertexPrimitiveMode;

#define GST_3D_VERTEX_PRIMITIVE_MODE_ALL " triangles, triangle-strip, triangle-fan "

const gchar *               gst_3d_vertex_primitive_mode_to_string          (Gst3DVertexPrimitiveMode type);
Gst3DVertexPrimitiveMode    gst_3d_vertex_primitive_mode_from_string        (const gchar * name);

typedef struct _Gst3DVertexAttribute Gst3DVertexAttribute;

#define GST_3D_VERTEX_INFO_MAX_ATTRIBUTES 32
struct _Gst3DVertexAttribute
{
  const Gst3DVertexFormatInfo      *finfo;
  Gst3DVertexType                   type;
  gchar                            *custom_name; /* required if @type == GST_3D_VERTEX_TYPE_CUSTOM or if there are multiple @type's in the #Gst3DVertexInfo */
  gint                              channels;   /* how many @finfo's in this attribute */
};

#define GST_3D_VERTEX_ATTRIBUTE_FORMAT_INFO(info)           ((info)->finfo)
#define GST_3D_VERTEX_ATTRIBUTE_TYPE(info)                  ((info)->type)
#define GST_3D_VERTEX_ATTRIBUTE_CHANNELS(info)              ((info)->channels)

void            gst_3d_vertex_attribute_init        (Gst3DVertexAttribute * info);
void            gst_3d_vertex_attribute_unset       (Gst3DVertexAttribute * info);
void            gst_3d_vertex_attribute_copy_into   (Gst3DVertexAttribute * src, Gst3DVertexAttribute * dest);

typedef struct _Gst3DVertexInfo Gst3DVertexInfo;

struct _Gst3DVertexInfo
{
  Gst3DVertexPrimitiveMode       primitive_mode;
  const Gst3DVertexFormatInfo   *index_finfo;       /* the vertices may not have an index */
  guint                          n_attribs;
  Gst3DVertexAttribute          *attributes;
  gssize                         max_vertices;      /* -1 if no maximum*/
  gsize                         *offsets;           /* offset to the first vertex */
  gsize                         *strides;           /* number of bytes between successive vertices */
};

#define GST_3D_VERTEX_INFO_N_ATTRIBS(info)                  ((info)->n_attribs)
#define GST_3D_VERTEX_INFO_ATTRIBUTE(info, i)               (&(info)->attributes[i])

void                gst_3d_vertex_info_init                 (Gst3DVertexInfo * info, guint n_attributes);
void                gst_3d_vertex_info_unset                (Gst3DVertexInfo * info);
void                gst_3d_vertex_info_copy_into            (Gst3DVertexInfo * src, Gst3DVertexInfo * dest);

void                gst_3d_vertex_info_set_n_attributes     (Gst3DVertexInfo * info, guint n_attributes);

gboolean            gst_3d_vertex_info_from_caps            (Gst3DVertexInfo * info, const GstCaps * caps);
GstCaps *           gst_3d_vertex_info_to_caps              (const Gst3DVertexInfo * info);

gsize               gst_3d_vertex_info_get_size             (Gst3DVertexInfo * info, gsize n_vertices);
gsize               gst_3d_vertex_info_get_n_vertices       (Gst3DVertexInfo * info, gsize n_bytes);
int                 gst_3d_vertex_info_find_attribute_index (Gst3DVertexInfo * info, Gst3DVertexType type, const gchar * custom_name);
int                 gst_3d_vertex_info_find_attribute_nth_index (Gst3DVertexInfo * info, Gst3DVertexType type, int n);
int                 gst_3d_vertex_info_get_n_attributes_of_type (Gst3DVertexInfo * info, Gst3DVertexType type);


G_END_DECLS

#endif /* __GST_3D_VERTEX_INFO_H__ */
