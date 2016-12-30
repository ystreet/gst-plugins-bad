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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gst3dvertexmeta.h"

GST_DEBUG_CATEGORY_STATIC (trid_vertex_meta_debug);
#define GST_CAT_DEFAULT _init_3d_vertex_meta_debug()

static GstDebugCategory *
_init_3d_vertex_meta_debug (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (trid_vertex_meta_debug, "3dvertexmeta", 0,
        "3D Vertex Meta");
    g_once_init_leave (&_init, 1);
  }

  return trid_vertex_meta_debug;
}

static gboolean
gst_3d_vertex_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  Gst3DVertexMeta *emeta = (Gst3DVertexMeta *) meta;

  emeta->buffer = NULL;
  emeta->id = 0;
  gst_3d_vertex_info_init (&emeta->info, 0);
  emeta->n_indices = 0;
  emeta->n_vertices = 0;
  emeta->index_offset = 0;
  emeta->vertex_offset = 0;

  return TRUE;
}

static void
gst_3d_vertex_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  Gst3DVertexMeta *emeta = (Gst3DVertexMeta *) meta;

  gst_3d_vertex_info_unset (&emeta->info);
}

static gboolean
gst_3d_vertex_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  Gst3DVertexMeta *dmeta, *smeta;

  smeta = (Gst3DVertexMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta = gst_buffer_add_3d_vertex_meta (dest, smeta->id, &smeta->info,
          smeta->n_indices, smeta->n_vertices);
      if (!dmeta)
        return FALSE;

      GST_TRACE ("copy meta from buffer %p to %p", buffer, dest);

      dmeta->buffer = dest;
      dmeta->index_offset = smeta->index_offset;
      dmeta->vertex_offset = smeta->vertex_offset;
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

GType
gst_3d_vertex_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { NULL, };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("Gst3DVertexMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/* video metadata */
const GstMetaInfo *
gst_3d_vertex_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_3D_VERTEX_META_API_TYPE, "Gst3DVertexMeta",
        sizeof (Gst3DVertexMeta), (GstMetaInitFunction) gst_3d_vertex_meta_init,
        (GstMetaFreeFunction) gst_3d_vertex_meta_free,
        gst_3d_vertex_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) meta);
  }
  return meta_info;
}

Gst3DVertexMeta *
gst_buffer_add_3d_vertex_meta (GstBuffer * buffer, gint id,
    Gst3DVertexInfo * info, gsize n_indices, gsize n_vertices)
{
  Gst3DVertexMeta *meta;

  meta =
      (Gst3DVertexMeta *) gst_buffer_add_meta (buffer, GST_3D_VERTEX_META_INFO,
      NULL);
  meta->buffer = buffer;
  meta->id = id;
  meta->n_indices = n_indices;
  meta->n_vertices = n_vertices;

  gst_3d_vertex_info_unset (&meta->info);
  gst_3d_vertex_info_copy_into (info, &meta->info);

  meta->index_offset = 0;
  if (meta->info.index_finfo)
    meta->vertex_offset = meta->info.index_finfo->width / 8 * n_indices;
  else
    meta->vertex_offset = 0;

  return meta;
}

Gst3DVertexMeta *
gst_buffer_get_3d_vertex_meta (GstBuffer * buffer, gint id)
{
  gpointer state = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_3D_VERTEX_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DVertexMeta *vmeta = (Gst3DVertexMeta *) meta;
      if (vmeta->id == id)
        return vmeta;
    }
  }
  return NULL;
}

gboolean
gst_3d_vertex_meta_map (Gst3DVertexMeta * meta, Gst3DVertexMapInfo * map_info,
    GstMapFlags map_flags)
{
  guint idx, length;
  gsize offset, skip, size;
  GstBuffer *buffer = meta->buffer;

  if (meta->info.n_attribs <= 0)
    goto no_vertices;

  memset (&map_info->index_map, 0, sizeof (map_info->index_map));
  map_info->index_data = NULL;
  map_info->index_offset = 0;
  memset (&map_info->vertex_map, 0, sizeof (map_info->vertex_map));
  map_info->vertex_data = NULL;
  map_info->vertex_offset = 0;

  if (meta->info.index_finfo) {
    offset = meta->index_offset;
    size = meta->n_indices * meta->info.index_finfo->width / 8;

    /* find the memory block for this plane, this is the memory block containing
     * the offset. */
    if (!gst_buffer_find_memory (buffer, offset, size, &idx, &length, &skip))
      goto no_memory;

    if (!gst_buffer_map_range (buffer, idx, length, &map_info->index_map,
            map_flags))
      goto cannot_map;

    map_info->index_data = (guint8 *) map_info->index_map.data + skip;
    map_info->index_offset = skip;
  }

  offset = meta->vertex_offset;
  size = gst_3d_vertex_info_get_size (&meta->info, meta->n_vertices);
  if (size == 0)
    goto no_vertex_size;

  /* find the memory block for this plane, this is the memory block containing
   * the offset. */
  if (!gst_buffer_find_memory (buffer, offset, size, &idx, &length, &skip))
    goto no_memory;

  if (!gst_buffer_map_range (buffer, idx, length, &map_info->vertex_map,
          map_flags))
    goto cannot_map;

  map_info->vertex_data = (guint8 *) map_info->vertex_map.data + skip;
  map_info->vertex_offset = skip;

  return TRUE;

  /* ERRORS */
no_vertices:
  {
    GST_WARNING ("meta doesn't have any vertices to map");
    return FALSE;
  }
no_vertex_size:
  {
    GST_WARNING ("Could not calculate size of vertex data");
    return FALSE;
  }
no_memory:
  {
    GST_WARNING ("no memory at offset %" G_GSIZE_FORMAT, offset);
    return FALSE;
  }
cannot_map:
  {
    GST_WARNING ("cannot map memory range %u-%u", idx, length);
    return FALSE;
  }
}

void
gst_3d_vertex_meta_unmap (Gst3DVertexMeta * meta, Gst3DVertexMapInfo * map_info)
{
  if (map_info->vertex_data)
    gst_buffer_unmap (meta->buffer, &map_info->index_map);
  if (map_info->index_data)
    gst_buffer_unmap (meta->buffer, &map_info->vertex_map);
}
