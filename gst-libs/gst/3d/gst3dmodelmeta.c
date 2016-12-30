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

#include "gst3dmodelmeta.h"

GST_DEBUG_CATEGORY_STATIC (trid_model_meta_debug);
#define GST_CAT_DEFAULT _init_3d_model_meta_debug()

static GstDebugCategory *
_init_3d_model_meta_debug (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (trid_model_meta_debug, "3dmodelmeta", 0,
        "3D Model Meta");
    g_once_init_leave (&_init, 1);
  }

  return trid_model_meta_debug;
}

static gboolean
gst_3d_model_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  Gst3DModelMeta *emeta = (Gst3DModelMeta *) meta;

  emeta->buffer = NULL;
  emeta->id = 0;
  emeta->vertex_id = -1;
  emeta->material_id = -1;

  return TRUE;
}

static void
gst_3d_model_meta_free (GstMeta * meta, GstBuffer * buffer)
{
}

static gboolean
gst_3d_model_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  Gst3DModelMeta *dmeta, *smeta;

  smeta = (Gst3DModelMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta =
          gst_buffer_add_3d_model_meta (dest, smeta->id, smeta->vertex_id,
          smeta->material_id);
      if (!dmeta)
        return FALSE;

      GST_TRACE ("copy meta from buffer %p to %p", buffer, dest);

      dmeta->buffer = dest;
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

GType
gst_3d_model_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { NULL, };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("Gst3DModelMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/* video metadata */
const GstMetaInfo *
gst_3d_model_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_3D_MODEL_META_API_TYPE, "Gst3DModelMeta",
        sizeof (Gst3DModelMeta), (GstMetaInitFunction) gst_3d_model_meta_init,
        (GstMetaFreeFunction) gst_3d_model_meta_free,
        gst_3d_model_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) meta);
  }
  return meta_info;
}

Gst3DModelMeta *
gst_buffer_add_3d_model_meta (GstBuffer * buffer, gint id, gint vertex_id,
    gint material_id)
{
  Gst3DModelMeta *meta;

  meta =
      (Gst3DModelMeta *) gst_buffer_add_meta (buffer, GST_3D_MODEL_META_INFO,
      NULL);
  meta->vertex_id = vertex_id;
  meta->material_id = material_id;

  return meta;
}

Gst3DModelMeta *
gst_buffer_get_3d_model_meta (GstBuffer * buffer, gint id)
{
  gpointer state = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_3D_MODEL_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DModelMeta *vmeta = (Gst3DModelMeta *) meta;
      if (vmeta->id == id)
        return vmeta;
    }
  }
  return NULL;
}

gboolean
gst_3d_model_meta_get_vertices_material (Gst3DModelMeta * meta,
    Gst3DVertexMeta ** vmeta, Gst3DMaterialMeta ** mmeta)
{
  if (vmeta) {
    if (meta->vertex_id == -1)
      *vmeta = NULL;
    else if (!(*vmeta =
            gst_buffer_get_3d_vertex_meta (meta->buffer, meta->vertex_id)))
      return FALSE;
  }
  if (mmeta) {
    if (meta->material_id == -1)
      *mmeta = NULL;
    else if (!(*mmeta =
            gst_buffer_get_3d_material_meta (meta->buffer, meta->material_id)))
      return FALSE;
  }

  return TRUE;
}

static gboolean
_copy_fields (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstStructure *s = user_data;

  gst_structure_set_value (s, g_quark_to_string (field_id), value);

  return TRUE;
}

GstCaps *
gst_3d_model_info_to_caps (Gst3DVertexInfo * vinfo, Gst3DMaterialInfo * minfo)
{
  GstCaps *ret, *caps = NULL;
  GstStructure *s, *ret_s;
  Gst3DMaterialInfo tmp_minfo;

  g_return_val_if_fail (vinfo != NULL, NULL);

  ret = gst_caps_new_empty_simple ("application/3d-model");
  ret_s = gst_caps_get_structure (ret, 0);
  if (!(caps = gst_3d_vertex_info_to_caps (vinfo)))
    goto error;
  s = gst_caps_get_structure (caps, 0);
  gst_structure_foreach (s, _copy_fields, ret_s);
  gst_caps_unref (caps);

  if (!minfo) {
    gst_3d_material_info_init (&tmp_minfo);
    minfo = &tmp_minfo;
  }

  if (!(caps = gst_3d_material_info_to_caps (minfo)))
    goto error;
  s = gst_caps_get_structure (caps, 0);
  gst_structure_foreach (s, _copy_fields, ret_s);
  gst_caps_unref (caps);

  return ret;

error:
  if (ret)
    gst_caps_unref (ret);
  if (caps)
    gst_caps_unref (caps);
  return NULL;
}

gboolean
gst_3d_model_info_from_caps (GstCaps * caps, Gst3DVertexInfo * vinfo,
    Gst3DMaterialInfo * minfo)
{
  GstCaps *tmp;
  GstStructure *s;

  g_return_val_if_fail (vinfo != NULL, FALSE);
  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);

  tmp = gst_caps_copy (caps);
  s = gst_caps_get_structure (tmp, 0);

  gst_structure_set_name (s, "application/3d-vertices");
  if (!gst_3d_vertex_info_from_caps (vinfo, tmp))
    goto error;

  if (minfo) {
    gst_structure_set_name (s, "application/3d-material");
    if (!gst_3d_material_info_from_caps (minfo, tmp))
      goto error;
  }

  gst_caps_unref (tmp);
  return TRUE;

error:
  gst_caps_unref (tmp);
  return FALSE;
}
