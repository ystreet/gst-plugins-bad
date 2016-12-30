/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
 *
 * gst3dvertexconverter.c: Convert 3D vertices to different formats automatically
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

#include <math.h>
#include <string.h>

#include "gst3dvertexconverter.h"
#include "gst3dvertexpack.h"
#include "gst3dvertexmeta.h"

/**
 * SECTION:3dvertexconverter
 * @short_description: Generic vertex conversion
 *
 * <refsect2>
 * <para>
 * This object is used to convert vertices from one format to another.
 * The object can perform conversion of:
 * <itemizedlist>
 *  <listitem><para>
 *    vertex format
 *  </para></listitem>
 *  <listitem><para>
 *    vertex channels
 *  </para></listitem>
 * </para>
 * </refsect2>
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("3dvertex-converter", 0,
        "3dvertex-converter object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

typedef struct _VertexAttributeConverter VertexAttributeConverter;


/*                           int/int    int/float  float/int float/float
 *
 *  unpack                     S32          S32         F64       F64
 *  convert                               S32->F64
 *  channel mix                S32          F64         F64       F64
 *  convert                                           F64->S32
 *  quantize                   S32                      S32
 *  pack                       S32          F64         S32       F64
 *
 *
 *  interleave
 *  deinterleave
 *  revertice
 */

struct _VertexAttributeConverter
{
  Gst3DVertexAttribute in;
  Gst3DVertexAttribute out;

  gboolean in_writable;
  guint8 *in_data;
  guint8 *out_data;

  gsize num_vertices;
  gsize src_stride;
  gsize dst_stride;
};

struct _Gst3DVertexConverter
{
  Gst3DVertexInfo in;
  Gst3DVertexInfo out;

  GstStructure *config;

  gsize num_vertices;

  VertexAttributeConverter *convert[GST_3D_VERTEX_INFO_MAX_ATTRIBUTES];
};

/*
static guint
get_opt_uint (Gst3DVertexConverter * convert, const gchar * opt, guint def)
{
  guint res;
  if (!gst_structure_get_uint (convert->config, opt, &res))
    res = def;
  return res;
}

static gint
get_opt_enum (Gst3DVertexConverter * convert, const gchar * opt, GType type,
    gint def)
{
  gint res;
  if (!gst_structure_get_enum (convert->config, opt, type, &res))
    res = def;
  return res;
}
*/

static gboolean
copy_config (GQuark field_id, const GValue * value, gpointer user_data)
{
  Gst3DVertexConverter *convert = user_data;

  gst_structure_id_set_value (convert->config, field_id, value);

  return TRUE;
}

/**
 * gst_3d_vertex_converter_update_config:
 * @convert: a #Gst3DVertexConverter
 * @in_rate: input rate
 * @out_rate: output rate
 * @config: (transfer full) (allow-none): a #GstStructure or %NULL
 *
 * Set @in_rate, @out_rate and @config as extra configuration for @convert.
 *
 * @in_rate and @out_rate specify the new vertice rates of input and output
 * formats. A value of 0 leaves the vertice rate unchanged.
 *
 * @config can be %NULL, in which case, the current configuration is not
 * changed.
 *
 * If the parameters in @config can not be set exactly, this function returns
 * %FALSE and will try to update as much state as possible. The new state can
 * then be retrieved and refined with gst_3d_vertex_converter_get_config().
 *
 * Look at the #GST_3D_VERTEX_CONVERTER_OPT_* fields to check valid configuration
 * option and values.
 *
 * Returns: %TRUE when the new parameters could be set
 */
gboolean
gst_3d_vertex_converter_update_config (Gst3DVertexConverter * convert,
    GstStructure * config)
{
  g_return_val_if_fail (convert != NULL, FALSE);

  if (config) {
    gst_structure_foreach (config, copy_config, convert);
    gst_structure_free (config);
  }

  return TRUE;
}

/**
 * gst_3d_vertex_converter_get_config:
 * @convert: a #Gst3DVertexConverter
 * @in_rate: result input rate
 * @out_rate: result output rate
 *
 * Get the current configuration of @convert.
 *
 * Returns: a #GstStructure that remains valid for as long as @convert is valid
 *   or until gst_3d_vertex_converter_update_config() is called.
 */
const GstStructure *
gst_3d_vertex_converter_get_config (Gst3DVertexConverter * convert)
{
  g_return_val_if_fail (convert != NULL, NULL);

  return convert->config;
}

static gboolean
is_intermediate_format (Gst3DVertexFormat format)
{
  return format == GST_3D_VERTEX_FORMAT_S16
      || format == GST_3D_VERTEX_FORMAT_S16_NORM
      || format == GST_3D_VERTEX_FORMAT_S32
      || format == GST_3D_VERTEX_FORMAT_S32_NORM
      || format == GST_3D_VERTEX_FORMAT_F32
      || format == GST_3D_VERTEX_FORMAT_F64;
}

static gboolean
vertex_attribute_converter_vertices (VertexAttributeConverter * convert,
    gsize n_vertices, guint8 * src, guint8 * dest)
{
  /* 4 components * 64-bit value */
  guint8 tmp1[4 * 8], tmp2[4 * 8];
  guint8 *tmp1_p, *tmp2_p;
  gsize src_offset, dst_offset, i;
  gboolean same_format;

  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  if (n_vertices == 0) {
    GST_DEBUG ("skipping empty buffer");
    return TRUE;
  }

  same_format = convert->in.finfo->format == convert->out.finfo->format;
  src_offset = dst_offset = 0;

  for (i = 0; i < n_vertices; i++) {
    const Gst3DVertexFormatInfo *info;
    gsize byte_width;
    gint channels;

    /* 1. unpack the vertex (if necessary) */
    info = convert->in.finfo;
    byte_width = info->width / 8;
    channels = convert->in.channels;

    /* do not unpack if we have the same input format as the output format
     * and it is a possible intermediate format */
    if (convert->in.finfo->unpack_format == GST_3D_VERTEX_FORMAT_UNKNOWN ||
        (same_format && is_intermediate_format (convert->in.finfo->format))) {
      tmp1_p = &src[src_offset];
    } else {
      channels = MAX (convert->in.finfo->unpack_channels, convert->in.channels);

      convert->in.finfo->unpack_func (convert->in.finfo,
          0 /*GST_AUDIO_PACK_FLAG_TRUNCATE_RANGE */ , tmp1,
          &src[src_offset], channels);

      info = gst_3d_vertex_format_get_info (convert->in.finfo->unpack_format);
      byte_width = info->width / 8;
      tmp1_p = tmp1;
    }

    /* 2. synthesize extra channels (if necessary) */
    {
      gint need_channels =
          MAX (convert->out.finfo->unpack_channels, convert->out.channels);

      if (need_channels > channels)
        memcpy (&tmp1_p[channels * byte_width],
            &info->default_vec4[channels * byte_width],
            (need_channels - channels) * byte_width);
      channels = need_channels;
    }

    /* 3. convert between float/integer representations (if necessary) */
    {
      gboolean in_int =
          GST_3D_VERTEX_FORMAT_INFO_IS_INTEGER (convert->in.finfo);
      gboolean out_int =
          GST_3D_VERTEX_FORMAT_INFO_IS_INTEGER (convert->out.finfo);

      if (in_int && !out_int) {
        if (info->flags & GST_3D_VERTEX_FORMAT_FLAG_NORMALIZED) {
          vertex_orc_s32norm_to_double ((gdouble *) tmp2, (gint32 *) tmp1_p,
              channels);
        } else {
          vertex_orc_s32_to_double ((gdouble *) tmp2, (gint32 *) tmp1_p,
              channels);
        }
        info = gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F64);
        byte_width = info->width / 8;
        tmp2_p = tmp2;
      } else if (!in_int && out_int) {
        if (convert->out.finfo->flags & GST_3D_VERTEX_FORMAT_FLAG_NORMALIZED) {
          info = gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_S32_NORM);
          vertex_orc_double_to_s32norm ((gint32 *) tmp2, (gdouble *) tmp1_p,
              channels);
        } else {
          info = gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_S32);
          vertex_orc_double_to_s32 ((gint32 *) tmp2, (gdouble *) tmp1_p,
              channels);
        }
        byte_width = info->width / 8;
        tmp2_p = tmp2;
      } else {
        tmp2_p = tmp1_p;
      }
    }

    /* 4. pack the vertex into the destination */
    if (convert->out.finfo->unpack_format == GST_3D_VERTEX_FORMAT_UNKNOWN ||
        (same_format && is_intermediate_format (convert->out.finfo->format))) {
      memcpy (&dest[dst_offset], tmp2_p, byte_width * convert->out.channels);
    } else {
      convert->out.finfo->pack_func (convert->out.finfo,
          0 /*GST_AUDIO_PACK_FLAG_TRUNCATE_RANGE */ , tmp2_p,
          &dest[dst_offset], channels);
    }
    src_offset += convert->src_stride;
    dst_offset += convert->dst_stride;
  }
  return TRUE;
}

static VertexAttributeConverter *
vertex_attribute_converter_new (Gst3DVertexAttribute * in,
    Gst3DVertexAttribute * out)
{
  VertexAttributeConverter *convert;

  g_return_val_if_fail (in != NULL, FALSE);
  g_return_val_if_fail (out != NULL, FALSE);

  convert = g_new0 (VertexAttributeConverter, 1);

  gst_3d_vertex_attribute_copy_into (in, &convert->in);
  gst_3d_vertex_attribute_copy_into (out, &convert->out);
  convert->src_stride = 0;
  convert->dst_stride = 0;

  return convert;
}

static void
vertex_attribute_converter_free (VertexAttributeConverter * convert)
{
  g_return_if_fail (convert != NULL);

  gst_3d_vertex_attribute_unset (&convert->in);
  gst_3d_vertex_attribute_unset (&convert->out);

  g_free (convert);
}

/**
 * gst_3d_vertex_converter_new: (skip)
 * @flags: extra #Gst3DVertexConverterFlags
 * @in_info: a source #Gst3DVertexInfo
 * @out_info: a destination #Gst3DVertexInfo
 * @config: (transfer full): a #GstStructure with configuration options
 *
 * Create a new #Gst3DVertexConverter that is able to convert between @in and @out
 * audio formats.
 *
 * @config contains extra configuration options, see #GST_VIDEO_CONVERTER_OPT_*
 * parameters for details about the options and values.
 *
 * Returns: a #Gst3DVertexConverter or %NULL if conversion is not possible.
 */
Gst3DVertexConverter *
gst_3d_vertex_converter_new (Gst3DVertexInfo * in_info,
    Gst3DVertexInfo * out_info, GstStructure * config)
{
  Gst3DVertexConverter *convert;
  gint i;

  g_return_val_if_fail (in_info != NULL, FALSE);
  g_return_val_if_fail (out_info != NULL, FALSE);
  g_return_val_if_fail (in_info->primitive_mode == out_info->primitive_mode,
      FALSE);

  convert = g_new0 (Gst3DVertexConverter, 1);

  gst_3d_vertex_info_copy_into (in_info, &convert->in);
  gst_3d_vertex_info_copy_into (out_info, &convert->out);

  /* default config */
  convert->config = gst_structure_new_empty ("Gst3DVertexConverter");
  if (config)
    gst_3d_vertex_converter_update_config (convert, config);

#if 0
  /* TODO: optimize */
  if (gst_3d_vertex_attribute_is_equal (out_info, in_info)) {
    GST_INFO ("same formats, passthrough");
    convert->passthrough = TRUE;
    return convert;
  }
#endif

  for (i = 0; i < out_info->n_attribs; i++) {
    gint j;

    for (j = 0; j < in_info->n_attribs; j++) {
      if (in_info->attributes[j].type == out_info->attributes[i].type) {
        const gchar *in_name = in_info->attributes[j].custom_name;
        const gchar *out_name = out_info->attributes[i].custom_name;

        /* Are the names the same? NULL is equal to NULL */
        if (in_name == NULL || out_name == NULL) {
          if (in_name != NULL || out_name != NULL)
            continue;
        } else if (!g_str_equal (in_name, out_name)) {
          continue;
        }
      }
      convert->convert[i] =
          vertex_attribute_converter_new (&in_info->attributes[j],
          &out_info->attributes[i]);
      break;
    }

    if (j >= in_info->n_attribs) {
      GST_ERROR ("could not find output attribute %u in input info", i);
      gst_3d_vertex_converter_free (convert);
      return NULL;
    }
  }

  return convert;
}

/**
 * gst_3d_vertex_converter_free:
 * @convert: a #Gst3DVertexConverter
 *
 * Free a previously allocated @convert instance.
 */
void
gst_3d_vertex_converter_free (Gst3DVertexConverter * convert)
{
  int i;

  g_return_if_fail (convert != NULL);

  for (i = 0; i < GST_3D_VERTEX_INFO_MAX_ATTRIBUTES; i++) {
    if (convert->convert[i])
      vertex_attribute_converter_free (convert->convert[i]);
    convert->convert[i] = NULL;
  }

  gst_3d_vertex_info_unset (&convert->in);
  gst_3d_vertex_info_unset (&convert->out);

  gst_structure_free (convert->config);

  g_free (convert);
}

/**
 * gst_3d_vertex_converter_vertices:
 * @convert: a #Gst3DVertexConverter
 * @flags: extra #Gst3DVertexConverterFlags
 * @n_vertices: number of input frames
 * @src: input vertices
 * @dest: output vertices
 *
 * Perform the conversion with @n_vertices in @src to @n_vertices in @dest
 * using @convert.
 *
 * @src may be %NULL, in which case @n_vertices of default vertices are produced
 * by the converter. FIXME!
 *
 * This function always produces @n_vertices of output and consumes
 * @n_vertices of input. Use gst_3d_vertex_info_get_n_vertices() and
 * gst_3d_vertex_info_get_size() to make sure @src and @dest point to memory
 * large enough to accomodate @n_vertices.
 *
 * Returns: %TRUE if the conversion could be performed.
 */
gboolean
gst_3d_vertex_converter_vertices (Gst3DVertexConverter * convert,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  Gst3DVertexMeta *in_meta, *out_meta;
  Gst3DVertexMapInfo srcmap, dstmap;
  gsize insize, outsize;
  gboolean ret;
  gint i;

  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (outbuf), FALSE);

  in_meta = gst_buffer_get_3d_vertex_meta (inbuf, 0);
  out_meta = gst_buffer_get_3d_vertex_meta (outbuf, 0);
  g_return_val_if_fail (in_meta != NULL, FALSE);
  g_return_val_if_fail (out_meta != NULL, FALSE);
  g_return_val_if_fail (in_meta->n_vertices == out_meta->n_vertices, FALSE);

  if (in_meta->n_vertices == 0 && out_meta->n_vertices == 0)
    /* nothing to convert */
    return TRUE;

  /* get in/output sizes, to see if the buffers we got are of correct
   * sizes */
  insize = gst_3d_vertex_info_get_size (&in_meta->info, in_meta->n_vertices);
  outsize = gst_3d_vertex_info_get_size (&out_meta->info, out_meta->n_vertices);

  if (insize == 0 || outsize == 0) {
    GST_ERROR ("Could not determine size of buffers");
    return FALSE;
  }

  /* get src and dst data */
  if (!gst_3d_vertex_meta_map (in_meta, &srcmap, GST_MAP_READ)) {
    GST_ERROR ("Failed to map input buffer");
    return FALSE;
  }

  if (!gst_3d_vertex_meta_map (out_meta, &dstmap, GST_MAP_WRITE)) {
    gst_3d_vertex_meta_unmap (in_meta, &dstmap);
    GST_ERROR ("Failed to map output buffer");
    return FALSE;
  }

  /* check in and outsize */
  if (srcmap.vertex_map.size < insize)
    goto wrong_size;
  if (dstmap.vertex_map.size < outsize)
    goto wrong_size;

  /* and convert the samples */
  if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_GAP)) {
    for (i = 0; i < convert->out.n_attribs; i++) {
      guint8 *src_vertex = NULL, *dst_vertex = NULL;
      gint j;

      for (j = 0; j < convert->in.n_attribs; j++) {
        if (convert->in.attributes[j].type == convert->out.attributes[i].type) {
          src_vertex = &srcmap.vertex_data[in_meta->info.offsets[j]];
          break;
        }
      }
      dst_vertex = &dstmap.vertex_data[out_meta->info.offsets[i]];

      convert->convert[i]->src_stride = in_meta->info.strides[j];
      convert->convert[i]->dst_stride = out_meta->info.strides[i];
      if (!vertex_attribute_converter_vertices (convert->convert[i],
              in_meta->n_vertices, src_vertex, dst_vertex))
        goto convert_error;
    }
  } else {
    ret = FALSE;
  }
  ret = TRUE;

done:
  gst_3d_vertex_meta_unmap (in_meta, &srcmap);
  gst_3d_vertex_meta_unmap (out_meta, &dstmap);

  return ret;

  /* ERRORS */
wrong_size:
  {
    GST_ERROR ("input/output buffers are of wrong size in: %" G_GSIZE_FORMAT
        " < %" G_GSIZE_FORMAT " or out: %" G_GSIZE_FORMAT " < %" G_GSIZE_FORMAT,
        srcmap.vertex_map.size, insize, dstmap.vertex_map.size, outsize);
    ret = FALSE;
    goto done;
  }
convert_error:
  {
    GST_ERROR ("error while converting");
    ret = FALSE;
    goto done;
  }
}
