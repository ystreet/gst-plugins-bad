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

#include "gst3dmaterialmeta.h"

GST_DEBUG_CATEGORY_STATIC (trid_material_meta_debug);
#define GST_CAT_DEFAULT _init_3d_material_meta_debug()

static GstDebugCategory *
_init_3d_material_meta_debug (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (trid_material_meta_debug, "3dmaterialmeta", 0,
        "3D Material Meta");
    g_once_init_leave (&_init, 1);
  }

  return trid_material_meta_debug;
}

static gboolean
gst_3d_material_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  Gst3DMaterialMeta *emeta = (Gst3DMaterialMeta *) meta;

  emeta->buffer = NULL;
  emeta->id = 0;
  gst_3d_material_info_init (&emeta->info);
  emeta->stacks = g_array_new (FALSE, TRUE, sizeof (Gst3DMaterialStack));
  g_array_set_clear_func (emeta->stacks,
      (GDestroyNotify) gst_3d_material_stack_unset);

  return TRUE;
}

static void
gst_3d_material_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  Gst3DMaterialMeta *emeta = (Gst3DMaterialMeta *) meta;

  gst_3d_material_info_unset (&emeta->info);
  g_array_free (emeta->stacks, TRUE);
}

static gboolean
gst_3d_material_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  Gst3DMaterialMeta *dmeta, *smeta;

  smeta = (Gst3DMaterialMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (!copy->region) {
      int i;

      /* only copy if the complete data is copied as well */
      dmeta = gst_buffer_add_3d_material_meta (dest, smeta->id, &smeta->info);
      if (!dmeta)
        return FALSE;

      GST_TRACE ("copy meta from buffer %p to %p", buffer, dest);

      for (i = 0; i < smeta->stacks->len; i++) {
        Gst3DMaterialStack *stack_s =
            &g_array_index (smeta->stacks, Gst3DMaterialStack, i);
        Gst3DMaterialStack *stack_d =
            &g_array_index (dmeta->stacks, Gst3DMaterialStack, i);
        gst_3d_material_stack_copy_into (stack_s, stack_d);
      }

      dmeta->buffer = dest;
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }
  return TRUE;
}

GType
gst_3d_material_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { NULL, };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("Gst3DMaterialMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/* video metadata */
const GstMetaInfo *
gst_3d_material_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_3D_MATERIAL_META_API_TYPE, "Gst3DMaterialMeta",
        sizeof (Gst3DMaterialMeta),
        (GstMetaInitFunction) gst_3d_material_meta_init,
        (GstMetaFreeFunction) gst_3d_material_meta_free,
        gst_3d_material_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & meta_info, (GstMetaInfo *) meta);
  }
  return meta_info;
}

Gst3DMaterialMeta *
gst_buffer_add_3d_material_meta (GstBuffer * buffer, gint id,
    Gst3DMaterialInfo * info)
{
  Gst3DMaterialMeta *meta;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->elements != GST_3D_MATERIAL_ELEMENT_NONE, NULL);

  meta =
      (Gst3DMaterialMeta *) gst_buffer_add_meta (buffer,
      GST_3D_MATERIAL_META_INFO, NULL);
  meta->buffer = buffer;
  meta->id = id;

  gst_3d_material_info_unset (&meta->info);
  gst_3d_material_info_copy_into (info, &meta->info);

  {
    Gst3DMaterialElement elements = info->elements;
    int n = 0, i = 0;

    while (elements &= (elements - 1))
      n++;
    n++;

    g_array_set_size (meta->stacks, n);

    elements = info->elements;
    while (elements) {
      Gst3DMaterialElement element;
      Gst3DMaterialStack *stack;

      element = elements - (elements & (elements - 1));
      stack = &g_array_index (meta->stacks, Gst3DMaterialStack, i);

      gst_3d_material_stack_init (stack);
      stack->type = element;

      elements -= element;
      i++;
    }
  }

  return meta;
}

Gst3DMaterialMeta *
gst_buffer_get_3d_material_meta (GstBuffer * buffer, gint id)
{
  gpointer state = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_3D_MATERIAL_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DMaterialMeta *vmeta = (Gst3DMaterialMeta *) meta;
      if (vmeta->id == id)
        return vmeta;
    }
  }
  return NULL;
}

Gst3DMaterialStack *
gst_3d_material_meta_get_stack (Gst3DMaterialMeta * meta,
    Gst3DMaterialElement element)
{
  int i;

  for (i = 0; i < meta->stacks->len; i++) {
    Gst3DMaterialStack *stack =
        &g_array_index (meta->stacks, Gst3DMaterialStack, i);

    if (stack->type == element)
      return stack;
  }

  return NULL;
}
