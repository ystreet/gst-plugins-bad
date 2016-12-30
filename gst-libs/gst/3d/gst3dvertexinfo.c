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

#include "gst3dvertexinfo.h"

GST_DEBUG_CATEGORY_STATIC (trid_vertex_info_debug);
#define GST_CAT_DEFAULT _init_3d_vertex_info_debug()

static GstDebugCategory *
_init_3d_vertex_info_debug (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (trid_vertex_info_debug, "3dvertexinfo", 0,
        "3D Vertex Info");
    g_once_init_leave (&_init, 1);
  }

  return trid_vertex_info_debug;
}

/* *INDENT-OFF* */
static const struct
{
  Gst3DVertexType type;
  const gchar *name;
} type_names[] = {
  {GST_3D_VERTEX_TYPE_CUSTOM, "custom"},
  {GST_3D_VERTEX_TYPE_POSITION, "position"},
  {GST_3D_VERTEX_TYPE_NORMAL, "normal"},
  {GST_3D_VERTEX_TYPE_TEXTURE, "texture"},
};
/* *INDENT-ON* */

const gchar *
gst_3d_vertex_type_to_string (Gst3DVertexType type)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (type_names); i++) {
    if (type == type_names[i].type)
      return type_names[i].name;
  }

  return "unknown";
}

Gst3DVertexType
gst_3d_vertex_type_from_string (const gchar * name)
{
  int i;

  g_return_val_if_fail (name != NULL, GST_3D_VERTEX_TYPE_UNKNOWN);

  for (i = 0; i < G_N_ELEMENTS (type_names); i++) {
    if (g_strcmp0 (name, type_names[i].name) == 0)
      return type_names[i].type;
  }

  return GST_3D_VERTEX_TYPE_UNKNOWN;
}

/* *INDENT-OFF* */
static const struct
{
  Gst3DVertexPrimitiveMode mode;
  const gchar *name;
} prim_mode_names[] = {
  {GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLES, "triangles"},
  {GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLE_STRIP, "triangle-strip"},
  {GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLE_FAN, "triangle-fan"},
};
/* *INDENT-ON* */

const gchar *
gst_3d_vertex_primitive_mode_to_string (Gst3DVertexPrimitiveMode mode)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (prim_mode_names); i++) {
    if (mode == prim_mode_names[i].mode)
      return prim_mode_names[i].name;
  }

  return "unknown";
}

Gst3DVertexPrimitiveMode
gst_3d_vertex_primitive_mode_from_string (const gchar * name)
{
  int i;

  g_return_val_if_fail (name != NULL, GST_3D_VERTEX_PRIMITIVE_MODE_UNKNOWN);

  for (i = 0; i < G_N_ELEMENTS (prim_mode_names); i++) {
    if (g_strcmp0 (name, prim_mode_names[i].name) == 0)
      return prim_mode_names[i].mode;
  }

  return GST_3D_VERTEX_TYPE_UNKNOWN;
}

void
gst_3d_vertex_attribute_init (Gst3DVertexAttribute * attrib)
{
  g_return_if_fail (attrib != NULL);

  memset (attrib, 0, sizeof (*attrib));

  attrib->finfo = gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_UNKNOWN);
}

void
gst_3d_vertex_attribute_unset (Gst3DVertexAttribute * attrib)
{
  g_return_if_fail (attrib != NULL);

  g_free (attrib->custom_name);
  attrib->custom_name = NULL;
}

void
gst_3d_vertex_attribute_copy_into (Gst3DVertexAttribute * src,
    Gst3DVertexAttribute * dest)
{
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  dest->finfo = src->finfo;
  dest->type = src->type;
  dest->custom_name = g_strdup (src->custom_name);
  dest->channels = src->channels;
}

void
gst_3d_vertex_info_init (Gst3DVertexInfo * info, guint n_attributes)
{
  g_return_if_fail (info != NULL);

  memset (info, 0, sizeof (*info));
  info->max_vertices = -1;

  gst_3d_vertex_info_set_n_attributes (info, n_attributes);
}

void
gst_3d_vertex_info_set_n_attributes (Gst3DVertexInfo * info, guint n_attributes)
{
  int i;

  g_return_if_fail (info != NULL);

  if (n_attributes <= 0)
    return;

  info->attributes =
      g_renew (Gst3DVertexAttribute, info->attributes, n_attributes);
  info->offsets = g_renew (gsize, info->offsets, n_attributes);
  info->strides = g_renew (gsize, info->strides, n_attributes);
  for (i = info->n_attribs; i < n_attributes; i++) {
    gst_3d_vertex_attribute_init (&info->attributes[i]);
    info->offsets[i] = 0;
    info->strides[i] = 0;
  }
  info->n_attribs = n_attributes;
}

void
gst_3d_vertex_info_copy_into (Gst3DVertexInfo * src, Gst3DVertexInfo * dest)
{
  int i;

  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  gst_3d_vertex_info_init (dest, src->n_attribs);
  dest->primitive_mode = src->primitive_mode;
  dest->index_finfo = src->index_finfo;
  dest->max_vertices = src->max_vertices;
  for (i = 0; i < src->n_attribs; i++) {
    gst_3d_vertex_attribute_copy_into (&src->attributes[i],
        &dest->attributes[i]);
    dest->offsets[i] = src->offsets[i];
    dest->strides[i] = src->strides[i];
  }
}

void
gst_3d_vertex_info_unset (Gst3DVertexInfo * info)
{
  gint i;

  g_return_if_fail (info != NULL);

  for (i = 0; i < info->n_attribs; i++)
    gst_3d_vertex_attribute_unset (&info->attributes[i]);
  g_free (info->attributes);
  info->attributes = NULL;
  g_free (info->offsets);
  info->offsets = NULL;
  g_free (info->strides);
  info->strides = NULL;
}

static gboolean
parse_attribute_structure_value (Gst3DVertexAttribute * attr,
    const GValue * structure_val)
{
  Gst3DVertexFormat format;
  const GstStructure *str;
  const gchar *s;

  if (!structure_val)
    goto invalid_gvalue;

  str = gst_value_get_structure (structure_val);
  if (!str)
    goto no_contents;

  if (!(s = gst_structure_get_string (str, "format")))
    goto no_format;
  format = gst_3d_vertex_format_from_string (s);
  if (!(attr->finfo = gst_3d_vertex_format_get_info (format)))
    goto unknown_format;

  if (!(s = gst_structure_get_string (str, "type")))
    goto no_type;

  attr->type = gst_3d_vertex_type_from_string (s);
  if (attr->type == GST_3D_VERTEX_TYPE_UNKNOWN) {
    goto unknown_type;
  } else if (attr->type == GST_3D_VERTEX_TYPE_CUSTOM) {
    if (!(s = gst_structure_get_string (str, "custom-name")))
      goto no_custom_name;
    attr->custom_name = g_strdup (s);
  } else {
    attr->custom_name =
        g_strdup (gst_structure_get_string (str, "custom-name"));
  }

  if (!gst_structure_has_field_typed (str, "channels", G_TYPE_INT))
    goto wrong_channels;
  gst_structure_get_int (str, "channels", &attr->channels);

  /* if !complex, assert(channels == 1)? */

  return TRUE;

invalid_gvalue:
  GST_ERROR ("invalid structure GValue");
  return FALSE;

no_contents:
  GST_ERROR ("Value has no contents");
  return FALSE;

no_format:
  GST_ERROR ("structure has no \'format\' field");
  return FALSE;

unknown_format:
  GST_ERROR ("%s in \'format\' field is not recognized", s);
  return FALSE;

no_type:
  GST_ERROR ("structure has no \'type\' field");
  return FALSE;

unknown_type:
  GST_ERROR ("value in \'type\' field is not recognized");
  return FALSE;

no_custom_name:
  GST_ERROR ("no \'custom-name\' field as required by type=custom");
  return FALSE;

wrong_channels:
  GST_ERROR ("missing or incorrect type in field \'channels\'");
  return FALSE;
}

static void
fill_planes (Gst3DVertexInfo * info)
{
  gsize stride = 0, offset = 0;
  gint i, len;

  /* default is interleaved vertices */
  len = info->n_attribs;
  for (i = 0; i < len; i++)
    stride +=
        info->attributes[i].finfo->width / 8 * info->attributes[i].channels;
  for (i = 0; i < len; i++) {
    info->strides[i] = stride;
    info->offsets[i] = offset;
    offset +=
        info->attributes[i].finfo->width / 8 * info->attributes[i].channels;
  }
  info->max_vertices = -1;
}

gboolean
gst_3d_vertex_info_from_caps (Gst3DVertexInfo * info, const GstCaps * caps)
{
  GstStructure *str;
  const GValue *val;
  const gchar *s;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  memset (info, 0, sizeof (*info));

  str = gst_caps_get_structure (caps, 0);

  if (!gst_structure_has_name (str, "application/3d-vertices"))
    goto wrong_name;

  if (!(s = gst_structure_get_string (str, "primitive-mode")))
    goto no_primitive_mode;

  info->primitive_mode = gst_3d_vertex_primitive_mode_from_string (s);
  if (info->primitive_mode == GST_3D_VERTEX_PRIMITIVE_MODE_UNKNOWN)
    goto unknown_primitive_mode;

  if ((s = gst_structure_get_string (str, "index-format"))) {
    if (!g_str_equal (s, "none")) {
      Gst3DVertexFormat index_format = gst_3d_vertex_format_from_string (s);
      if (index_format == GST_3D_VERTEX_FORMAT_UNKNOWN)
        goto unknown_index_format;
      info->index_finfo = gst_3d_vertex_format_get_info (index_format);
      if (!info->index_finfo)
        goto unknown_index_format;
    }
  }

  if (!(val = gst_structure_get_value (str, "attributes")))
    goto no_attributes;

  if (GST_VALUE_HOLDS_ARRAY (val)) {
    gint i, len;

    len = gst_value_array_get_size (val);
    if (len > GST_3D_VERTEX_INFO_MAX_ATTRIBUTES)
      goto attribute_array_too_large;

    gst_3d_vertex_info_set_n_attributes (info, len);
    for (i = 0; i < len; i++) {
      const GValue *attr = gst_value_array_get_value (val, i);
      int j;

      if (!parse_attribute_structure_value (&info->attributes[i], attr)) {
        gst_3d_vertex_info_unset (info);
        return FALSE;
      }

      for (j = 0; j < i; j++) {
        if (info->attributes[i].type == info->attributes[j].type) {
          if (info->attributes[i].type == GST_3D_VERTEX_TYPE_POSITION
              || info->attributes[i].type == GST_3D_VERTEX_TYPE_NORMAL) {
            GST_ERROR ("multiple %s attributes in caps!",
                gst_3d_vertex_type_to_string (info->attributes[i].type));
            gst_3d_vertex_info_unset (info);
            return FALSE;
          }
          if (info->attributes[i].custom_name == NULL) {
            GST_ERROR ("%s attribute at index %d does not have a required "
                "custom name as required with multiple attributes of the same "
                "type", gst_3d_vertex_type_to_string (info->attributes[i].type),
                i);
            gst_3d_vertex_info_unset (info);
            return FALSE;
          }
          if (info->attributes[j].custom_name == NULL) {
            GST_ERROR ("%s attribute at index %d does not have a required "
                "custom name as required with multiple attributes of the same "
                "type", gst_3d_vertex_type_to_string (info->attributes[j].type),
                j);
            gst_3d_vertex_info_unset (info);
            return FALSE;
          }
        }
      }
    }
  } else if (GST_VALUE_HOLDS_STRUCTURE (val)) {
    gst_3d_vertex_info_set_n_attributes (info, 1);
    if (!parse_attribute_structure_value (&info->attributes[0], val)) {
      gst_3d_vertex_info_unset (info);
      return FALSE;
    }
  } else {
    goto wrong_attributes_type;
  }

  /* calculate offsets/strides */
  fill_planes (info);

  return TRUE;

wrong_name:
  GST_ERROR ("caps has the wrong structure name");
  return FALSE;

no_primitive_mode:
  GST_ERROR ("field \'primitive-mode\' is missing");
  return FALSE;

unknown_primitive_mode:
  GST_ERROR ("unknown value in \'primitive-mode\'");
  return FALSE;

unknown_index_format:
  GST_ERROR ("field \'index-format\' has unknown value");
  return FALSE;

no_attributes:
  GST_ERROR ("field \'attributes\' is missing");
  return FALSE;

wrong_attributes_type:
  GST_ERROR ("field \'attributes\' contains the wrong type of value");
  return FALSE;

attribute_array_too_large:
  GST_ERROR ("field \'attributes\' has too many values");
  return FALSE;
}

static GstStructure *
attribute_to_structure (const Gst3DVertexAttribute * attrib)
{
  GstStructure *str = gst_structure_new_empty ("attribute");
  const gchar *s;

  gst_structure_set (str, "format", G_TYPE_STRING, attrib->finfo->name, NULL);

  if (!(s = gst_3d_vertex_type_to_string (attrib->type))) {
    gst_structure_free (str);
    return NULL;
  }

  gst_structure_set (str, "type", G_TYPE_STRING, s, "channels", G_TYPE_INT,
      attrib->channels, NULL);

  return str;
}

GstCaps *
gst_3d_vertex_info_to_caps (const Gst3DVertexInfo * info)
{
  GstStructure *str;
  GstCaps *ret;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->primitive_mode !=
      GST_3D_VERTEX_PRIMITIVE_MODE_UNKNOWN, NULL);
  g_return_val_if_fail (info->n_attribs <= GST_3D_VERTEX_INFO_MAX_ATTRIBUTES,
      NULL);
  g_return_val_if_fail (info->n_attribs > 0, NULL);

  ret = gst_caps_new_empty_simple ("application/3d-vertices");
  str = gst_caps_get_structure (ret, 0);

  gst_structure_set (str, "primitive-mode", G_TYPE_STRING,
      gst_3d_vertex_primitive_mode_to_string (info->primitive_mode), NULL);

  if (info->index_finfo)
    gst_structure_set (str, "index-format", G_TYPE_STRING,
        info->index_finfo->name, NULL);
  else
    gst_structure_set (str, "index-format", G_TYPE_STRING, "none", NULL);

  if (info->n_attribs == 1) {
    GstStructure *attrib;

    if (!(attrib = attribute_to_structure (&info->attributes[0]))) {
      gst_caps_unref (ret);
      return NULL;
    }

    gst_structure_set (str, "attributes", GST_TYPE_STRUCTURE, attrib, NULL);
    gst_structure_free (attrib);
  } else {
    GValue attribs = G_VALUE_INIT;
    gint i;

    g_value_init (&attribs, GST_TYPE_ARRAY);
    for (i = 0; i < info->n_attribs; i++) {
      GstStructure *attrib;
      GValue attrib_value = G_VALUE_INIT;
      int j;

      for (j = 0; j < i; j++) {
        if (info->attributes[i].type == info->attributes[j].type) {
          if (info->attributes[i].type == GST_3D_VERTEX_TYPE_POSITION
              || info->attributes[i].type == GST_3D_VERTEX_TYPE_NORMAL) {
            GST_ERROR ("multiple %s attributes present in info!",
                gst_3d_vertex_type_to_string (info->attributes[i].type));
            g_value_unset (&attribs);
            gst_caps_unref (ret);
            return NULL;
          }
        }
      }

      attrib = attribute_to_structure (&info->attributes[i]);
      if (!attrib) {
        g_value_unset (&attribs);
        gst_caps_unref (ret);
        return NULL;
      }

      g_value_init (&attrib_value, GST_TYPE_STRUCTURE);
      gst_value_set_structure (&attrib_value, attrib);
      gst_structure_free (attrib);
      gst_value_array_append_value (&attribs, &attrib_value);
      g_value_unset (&attrib_value);
    }
    gst_structure_set_value (str, "attributes", &attribs);
    g_value_unset (&attribs);
  }

  return ret;
}

/* return how many vertices can fit in @n_bytes */
gsize
gst_3d_vertex_info_get_n_vertices (Gst3DVertexInfo * info, gsize n_bytes)
{
  gsize vertices = -1;
  gint i;

  g_return_val_if_fail (info != NULL, 0);

  if (info->n_attribs <= 0)
    return 0;

  for (i = 0; i < info->n_attribs; i++) {
    gsize block, vert_size, vertices_in_block;

    /* n_bytes is not large enough to contain all attributes */
    if (info->offsets[i] > n_bytes)
      return 0;

    vert_size =
        info->attributes[i].channels * info->attributes[i].finfo->width / 8;

    block = n_bytes - info->offsets[i];
    vertices_in_block = block / info->strides[i];
    /* can we fit another vertex in? */
    if (vertices_in_block * info->strides[i] + vert_size <= block)
      vertices_in_block++;

    vertices = MIN (vertices, vertices_in_block);
  }

  /* too many vertices */
  if (info->max_vertices != -1 && vertices > info->max_vertices)
    return info->max_vertices;

  return vertices;
}

gsize
gst_3d_vertex_info_get_size (Gst3DVertexInfo * info, gsize n_vertices)
{
  gsize size = 0;
  gint i;

  g_return_val_if_fail (info != NULL, 0);

  if (n_vertices <= 0)
    return 0;
  /* if n_vertices in one attribute overruns into another attribute. */
  if (info->max_vertices != -1 && n_vertices > info->max_vertices)
    return 0;

  for (i = 0; i < info->n_attribs; i++) {
    gsize vert_size =
        info->attributes[i].channels * info->attributes[i].finfo->width / 8;
    gsize block =
        info->offsets[i] + info->strides[i] * (n_vertices - 1) + vert_size;

    size = MAX (size, block);
  }

  return size;
}

int
gst_3d_vertex_info_find_attribute_index (Gst3DVertexInfo * info,
    Gst3DVertexType type, const gchar * custom_name)
{
  int i, tmp_res = -1;

  for (i = 0; i < info->n_attribs; i++) {
    if (type == info->attributes[i].type) {
      /* have multiple of the same type without a custom name.
       * Cannot distiguish between them */
      if (custom_name == NULL && tmp_res != -1)
        return -1;
      tmp_res = i;

      if (custom_name == NULL && info->attributes[i].custom_name == NULL)
        return i;
      else if (custom_name == NULL || info->attributes[i].custom_name == NULL)
        continue;
      else if (g_str_equal (custom_name, info->attributes[i].custom_name))
        return i;
    }
  }

  return i;
}
