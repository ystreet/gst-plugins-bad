/* GStreamer
 *
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
# include "config.h"
#endif

#include <gst/check/gstcheck.h>

#include <gst/3d/3d.h>

#include <stdio.h>

#define FORMAT(f_) GST_3D_VERTEX_FORMAT_ ## f_
#define F_FLAG(flag) GST_3D_VERTEX_FORMAT_FLAG_ ## flag

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define NE(f_, f2_) f_ ## LE ## f2_
#else
#define NE(f_, f2_) f_ ## BE ## f2_
#endif

/* keep in sync with Gst3DVertexFormat in gst3dvertexformat.h */
static Gst3DVertexFormat vertex_formats[] = {
  GST_3D_VERTEX_FORMAT_S8,
  GST_3D_VERTEX_FORMAT_S8_NORM,
  GST_3D_VERTEX_FORMAT_U8,
  GST_3D_VERTEX_FORMAT_U8_NORM,
  GST_3D_VERTEX_FORMAT_S16LE,
  GST_3D_VERTEX_FORMAT_S16LE_NORM,
  GST_3D_VERTEX_FORMAT_S16BE,
  GST_3D_VERTEX_FORMAT_S16BE_NORM,
  GST_3D_VERTEX_FORMAT_U16LE,
  GST_3D_VERTEX_FORMAT_U16LE_NORM,
  GST_3D_VERTEX_FORMAT_U16BE,
  GST_3D_VERTEX_FORMAT_U16BE_NORM,
  GST_3D_VERTEX_FORMAT_S32LE,
  GST_3D_VERTEX_FORMAT_S32LE_NORM,
  GST_3D_VERTEX_FORMAT_S32BE,
  GST_3D_VERTEX_FORMAT_S32BE_NORM,
  GST_3D_VERTEX_FORMAT_U32LE,
  GST_3D_VERTEX_FORMAT_U32LE_NORM,
  GST_3D_VERTEX_FORMAT_U32BE,
  GST_3D_VERTEX_FORMAT_U32BE_NORM,
/*  GST_3D_VERTEX_FORMAT_F16LE,
  GST_3D_VERTEX_FORMAT_F16BE,*/
  GST_3D_VERTEX_FORMAT_F32LE,
  GST_3D_VERTEX_FORMAT_F32BE,
  GST_3D_VERTEX_FORMAT_F64LE,
  GST_3D_VERTEX_FORMAT_F64BE,
};

GST_START_TEST (test_format_description)
{
  int i, n;

  n = G_N_ELEMENTS (vertex_formats);
  GST_INFO ("Validating %d vertex formats", n);

  for (i = 0; i < G_N_ELEMENTS (vertex_formats); i++) {
    const Gst3DVertexFormatInfo *format;
    gsize accum = 0;
    gint j;

    GST_INFO ("validating format %s",
        gst_3d_vertex_format_to_string (vertex_formats[i]));

    format = gst_3d_vertex_format_get_info (vertex_formats[i]);
    fail_unless (format != NULL, "get_info(0x%x/%s) returns NULL",
        vertex_formats[i], gst_3d_vertex_format_to_string (vertex_formats[i]));
    fail_unless (format->format == vertex_formats[i],
        "get_info(0x%x/%s) results in different format", vertex_formats[i],
        gst_3d_vertex_format_to_string (vertex_formats[i]));

    fail_unless (format->name != NULL, "format number %d doesn't have a name!",
        i);

    /* formats */
    fail_if (format->format == GST_3D_VERTEX_FORMAT_UNKNOWN,
        "%s:%d is \'unknown\'!", format->name, i);
    fail_if (g_strcmp0 (gst_3d_vertex_format_to_string (format->format),
            "unknown") == 0, "%s to_string() is \'unknown\'!", format->name);
    fail_unless (gst_3d_vertex_format_from_string
        (gst_3d_vertex_format_to_string (format->format)) == format->format,
        "%s from_string(to_string()) results in different format!",
        format->name);

    fail_unless (format->description != NULL, "%s no description!",
        format->name);
    fail_if (format->width <= 0, "%s <= 0 width!", format->name);
    fail_if (format->n_components <= 0, "%s <= 0 components!", format->name);

    for (j = 0; j < format->n_components; j++) {
      fail_if (format->shift[j] > 64,
          "%s shift count (%d) greater than reasonable (64)!", format->name,
          format->shift[j]);

      fail_if (format->shift[j] > format->width,
          "%s shift count (%d) greater than the format's width!", format->name,
          format->shift[j]);

      fail_if (format->depth[j] == 0, "%s 0 depth!", format->name);
      accum += format->depth[j];

      fail_if (format->shift[j] + format->depth[j] > format->width,
          "%s shift + depth is greater than width!", format->name);
    }
    fail_if (accum > format->width,
        "%s sum of depth's greater than the format width", format->name);

    if (format->unpack_format == GST_3D_VERTEX_FORMAT_UNKNOWN) {
      fail_if (format->format != GST_3D_VERTEX_FORMAT_F64
          && format->format != GST_3D_VERTEX_FORMAT_S32
          && format->format != GST_3D_VERTEX_FORMAT_S32_NORM,
          "%s unpack format is unknown", format->name);
      fail_if (format->unpack_channels != 0,
          "%s unpack channels is not 0 for unknown unpack format",
          format->name);
    } else {
      const Gst3DVertexFormatInfo *uinfo;

      fail_if (format->format == GST_3D_VERTEX_FORMAT_F64
          || format->format == GST_3D_VERTEX_FORMAT_S32
          || format->format == GST_3D_VERTEX_FORMAT_S32_NORM,
          "%s unpack format has unpack format", format->name);

      uinfo = gst_3d_vertex_format_get_info (format->unpack_format);
      fail_unless (uinfo != NULL,
          "%s could not find unpack format \'%s\'",
          format->name, gst_3d_vertex_format_to_string (format->unpack_format));
      fail_if ((format->flags & F_FLAG (NORMALIZED)) !=
          (uinfo->flags & F_FLAG (NORMALIZED)),
          "%s unpack \'%s\' has different normalization",
          format->name, gst_3d_vertex_format_to_string (format->unpack_format));
      fail_if ((format->flags & F_FLAG (INTEGER)) !=
          (uinfo->flags & F_FLAG (INTEGER)),
          "%s unpack \'%s\' has different integer flag",
          format->name, gst_3d_vertex_format_to_string (format->unpack_format));
      fail_if ((format->flags & F_FLAG (FLOAT)) !=
          (uinfo->flags & F_FLAG (FLOAT)),
          "%s unpack \'%s\' has different float flag",
          format->name, gst_3d_vertex_format_to_string (format->unpack_format));
      fail_if ((format->unpack_channels >
              GST_3D_VERTEX_FORMAT_MAX_COMPONENTS),
          "%s unpack \'%s\' has large number of channels", format->name,
          gst_3d_vertex_format_to_string (format->unpack_format));
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_n_attributes_change)
{
  Gst3DVertexInfo orig, info;
  gsize vert_size;

  gst_3d_vertex_info_init (&orig, 1);
  orig.attributes[0].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  orig.attributes[0].type = GST_3D_VERTEX_TYPE_POSITION;
  orig.attributes[0].channels = 4;
  orig.offsets[0] = 0;
  vert_size = orig.attributes[0].channels * orig.attributes[0].finfo->width / 8;
  orig.strides[0] = vert_size;

  gst_3d_vertex_info_copy_into (&orig, &info);
  gst_3d_vertex_info_set_n_attributes (&info, 2);

  fail_unless (info.attributes[0].finfo == orig.attributes[0].finfo);
  fail_unless (info.attributes[0].type == orig.attributes[0].type);
  fail_unless (info.attributes[0].channels == orig.attributes[0].channels);
  fail_unless (info.offsets[0] == orig.offsets[0]);
  fail_unless (info.strides[0] == orig.strides[0]);

  gst_3d_vertex_info_unset (&orig);
  gst_3d_vertex_info_unset (&info);
}

GST_END_TEST;

GST_START_TEST (test_sizes)
{
  Gst3DVertexInfo info;

  gst_3d_vertex_info_init (&info, 2);

  /* interleaved */
  info.attributes[0].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  info.attributes[0].type = GST_3D_VERTEX_TYPE_POSITION;
  info.attributes[0].channels = 4;
  info.offsets[0] = 0;
  info.strides[0] =
      info.attributes[0].channels * info.attributes[0].finfo->width / 8;

  info.attributes[1].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_S16_NORM);
  info.attributes[1].type = GST_3D_VERTEX_TYPE_NORMAL;
  info.attributes[1].channels = 3;
  info.offsets[1] = info.strides[0];
  info.strides[0] = info.strides[1] =
      info.strides[0] +
      info.attributes[1].channels * info.attributes[1].finfo->width / 8;

  fail_unless (gst_3d_vertex_info_get_size (&info, 1) == info.strides[0]);
  fail_unless (gst_3d_vertex_info_get_n_vertices (&info, info.strides[0]) == 1);

  /* 10 planar vertices */
  info.max_vertices = 10;
  info.strides[0] =
      info.attributes[0].channels * info.attributes[0].finfo->width / 8;
  info.strides[1] =
      info.attributes[1].channels * info.attributes[1].finfo->width / 8;
  info.offsets[1] =
      10 * info.attributes[0].channels * info.attributes[0].finfo->width / 8;

  fail_unless (gst_3d_vertex_info_get_size (&info,
          1) == info.offsets[1] + 1 * info.strides[1]);
  fail_unless (gst_3d_vertex_info_get_n_vertices (&info,
          info.offsets[1] + 1 * info.strides[1]) == 1);
  fail_unless (gst_3d_vertex_info_get_size (&info,
          2) == info.offsets[1] + 2 * info.strides[1]);
  fail_unless (gst_3d_vertex_info_get_size (&info,
          10) == info.offsets[1] + 10 * info.strides[1]);

  /* failure */
  /* too small */
  fail_unless (gst_3d_vertex_info_get_n_vertices (&info, info.strides[1]) == 0);
  /* too many vertices */
  fail_unless (gst_3d_vertex_info_get_n_vertices (&info,
          info.offsets[1] + 11 * info.strides[1]) == 10);
  fail_unless (gst_3d_vertex_info_get_size (&info, 11) == 0);

  gst_3d_vertex_info_unset (&info);
}

GST_END_TEST;

#define U8_0            { 0x0, }
#define U8_1            { 0x1, 0 }
#define U8_NORM_0       U8_0
#define U8_NORM_1       { 0xff, 0, }
#define U16LE_0         { 0x0, }
#define U16LE_1         { 0x1, 0x0, 0, }
#define U16LE_NORM_0    U16LE_0
#define U16LE_NORM_1    { 0xff, 0xff, 0, }
#define U16BE_0         U32LE_0
#define U16BE_1         { 0x0, 0x1, 0, }
#define U16BE_NORM_0    U16LE_0
#define U16BE_NORM_1    U16LE_NORM_1
#define U32LE_0         { 0, }
#define U32LE_1         { 0x1, 0x0, 0x0, 0x0, 0}
#define U32LE_NORM_0    U32BE_0
#define U32LE_NORM_1    { 0xff, 0xff, 0xff, 0xff, 0}
#define U32BE_0         U32LE_0
#define U32BE_1         { 0x0, 0x0, 0x0, 0x1, 0}
#define U32BE_NORM_0    U32LE_0
#define U32BE_NORM_1    U32LE_NORM_1

#define S8_0            { 0x00, 0, }
#define S8_1            { 0x01, 0, }
#define S8_N1           { 0xff, 0, }
#define S8_NORM_0       S8_0
#define S8_NORM_1       { 0x7f, 0, }
#define S8_NORM_N1      { 0x81, 0, }
#define S16LE_0         { 0x00, 0, }
#define S16LE_1         { 0x01, 0x00, 0, }
#define S16LE_N1        { 0xff, 0xff, 0, }
#define S16LE_NORM_0    S16LE_0
#define S16LE_NORM_1    { 0xff, 0x7f, 0, }
#define S16LE_NORM_N1   { 0x01, 0x80, 0, }
#define S16BE_0         S16LE_0
#define S16BE_1         { 0x0, 0x1, 0, }
#define S16BE_N1        S16LE_N1
#define S16BE_NORM_0    S16BE_0
#define S16BE_NORM_1    { 0x7f, 0xff, 0, }
#define S16BE_NORM_N1   { 0x80, 0x01, 0, }
#define S32LE_0         { 0, }
#define S32LE_1         { 0x01, 0x00, 0x00, 0x00, 0 }
#define S32LE_N1        { 0xff, 0xff, 0xff, 0xff, 0 }
#define S32LE_NORM_0    S32LE_0
#define S32LE_NORM_1    { 0xff, 0xff, 0xff, 0x7f, 0, }
#define S32LE_NORM_N1   { 0x01, 0x00, 0x00, 0x80, 0, }
#define S32BE_0         S32LE_0
#define S32BE_1         { 0x00, 0x00, 0x00, 0x01, 0 }
#define S32BE_N1        S32LE_N1
#define S32BE_NORM_0    S32LE_0
#define S32BE_NORM_1    { 0x7f, 0xff, 0xff, 0xff, 0, }
#define S32BE_NORM_N1   { 0x80, 0x00, 0x00, 0x01, 0, }

#define F32LE_0         { 0x00, 0x00, 0x00, 0x00, 0, }
#define F32LE_1         { 0x00, 0x00, 0x80, 0x3f, 0, }
#define F32LE_N1        { 0x00, 0x00, 0x80, 0xbf, 0, }
#define F32BE_0         { 0x00, 0x00, 0x00, 0x00, 0, }
#define F32BE_1         { 0x3f, 0x80, 0x00, 0x00, 0, }
#define F32BE_N1        { 0xbf, 0x80, 0x00, 0x00, 0, }
#define F64LE_0         { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0, }
#define F64LE_1         { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0, }
#define F64LE_N1        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xbf, 0, }
#define F64BE_0         { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0, }
#define F64BE_1         { 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0, }
#define F64BE_N1        { 0xbf, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0, }

#define MAX_VALUE_SIZE (sizeof(double)+1)
#define DEF_F64_EPSILON 0.000000001

#define BYTES_TO_INT32(arr)         (arr[0] | arr[1] << 8 | arr[2] << 16 | arr[3] << 24)
#define SWAP_BYTES_TO_INT32(arr)    (arr[3] | arr[2] << 8 | arr[1] << 16 | arr[0] << 24)
#define BYTES_TO_INT64(arr)         ((guint64) arr[0] | (guint64) arr[1] << 8 | \
                                     (guint64) arr[2] << 16 | (guint64) arr[3] << 24 | \
                                     (guint64) arr[4] << 32 | (guint64) arr[5] << 48 | \
                                     (guint64) arr[6] << 48 | (guint64) arr[7] << 54)
#define SWAP_BYTES_TO_INT64(arr)    ((guint64) arr[7] | (guint64) arr[6] << 8 | \
                                     (guint64) arr[5] << 16 | (guint64) arr[4] << 24 | \
                                     (guint64) arr[3] << 32 | (guint64) arr[2] << 48 | \
                                     (guint64) arr[1] << 48 | (guint64) arr[0] << 54)
#define BYTES_TO_F32(res, arr)      memcpy(res, arr, sizeof (float))
#define SWAP_BYTES_TO_F32(res, arr) \
    G_STMT_START { \
      guint32 tmp = SWAP_BYTES_TO_INT32(arr); \
      BYTES_TO_F32(res, &tmp); \
    } G_STMT_END
#define BYTES_TO_F64(res, arr)      memcpy(res, arr, sizeof (double))
#define SWAP_BYTES_TO_F64(res, arr) \
    G_STMT_START { \
      guint64 tmp = SWAP_BYTES_TO_INT64(arr); \
      BYTES_TO_F64(res, &tmp); \
    } G_STMT_END
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define LE_BYTES_TO_INT32(arr)      BYTES_TO_INT32(arr)
#define BE_BYTES_TO_INT32(arr)      SWAP_BYTES_TO_INT32(arr)
#define LE_BYTES_TO_F32(res, arr)   BYTES_TO_F32(res,arr)
#define BE_BYTES_TO_F32(res, arr)   SWAP_BYTES_TO_F32(res,arr)
#define LE_BYTES_TO_F64(res, arr)   BYTES_TO_F64(res,arr)
#define BE_BYTES_TO_F64(res, arr)   SWAP_BYTES_TO_F64(res,arr)
#else
#define BE_BYTES_TO_INT32(arr)      BYTES_TO_INT32(arr)
#define LE_BYTES_TO_INT32(arr)      SWAP_BYTES_TO_INT32(arr)
#define BE_BYTES_TO_F32(res, arr)   BYTES_TO_F32(res,arr)
#define LE_BYTES_TO_F32(res, arr)   SWAP_BYTES_TO_F32(res,arr)
#define BE_BYTES_TO_F64(res, arr)   BYTES_TO_F64(res,arr)
#define LE_BYTES_TO_F64(res, arr)   SWAP_BYTES_TO_F64(res,arr)
#endif

static gint32
read_s32_as_native (const Gst3DVertexFormatInfo * finfo, const guint8 * integer)
{
  if (finfo->format == FORMAT (S8) || finfo->format == FORMAT (S8_NORM)
      || finfo->format == FORMAT (U8) || finfo->format == FORMAT (U8_NORM))
    return BYTES_TO_INT32 (integer);

  if (finfo->flags & F_FLAG (LE))
    return LE_BYTES_TO_INT32 (integer);
  else
    return BE_BYTES_TO_INT32 (integer);
}

static double
read_f64_as_native (const Gst3DVertexFormatInfo * finfo, const guint8 * f)
{
  if (finfo->width == 32) {
    float res;
    if (finfo->flags & F_FLAG (LE))
      LE_BYTES_TO_F32 (&res, f);
    else
      BE_BYTES_TO_F32 (&res, f);
    return (double) res;
  } else if (finfo->width == 64) {
    double res;
    if (finfo->flags & F_FLAG (LE))
      LE_BYTES_TO_F64 (&res, f);
    else
      BE_BYTES_TO_F64 (&res, f);
    return res;
  }

  return 0.;
}

struct convert_info
{
  Gst3DVertexFormat from, to;
  guint8 from_value[MAX_VALUE_SIZE];
  guint8 to_value[MAX_VALUE_SIZE];
  guint32 epsilon_s32;
  double epsilon_f64;
};

/* *INDENT-OFF* */
static const struct convert_info pack_unpack[] =
{
  {FORMAT(U8),          FORMAT(S32),        U8_0,           NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(U8),          FORMAT(S32),        U8_1,           NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(U8_NORM),     FORMAT(S32_NORM),   U8_0,           NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(U8_NORM),     FORMAT(S32_NORM),   U8_NORM_1,      NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S8),          FORMAT(S32),        S8_0,           NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S8),          FORMAT(S32),        S8_1,           NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S8),          FORMAT(S32),        S8_N1,          NE(S32,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(S8_NORM),     FORMAT(S32_NORM),   S8_NORM_0,      NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(S8_NORM),     FORMAT(S32_NORM),   S8_NORM_1,      NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S8_NORM),     FORMAT(S32_NORM),   S8_NORM_N1,     NE(S32,_NORM_N1),   0x01010100, DEF_F64_EPSILON},
  {FORMAT(U16LE),       FORMAT(S32),        U16LE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(U16LE),       FORMAT(S32),        U16LE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(U16LE_NORM),  FORMAT(S32_NORM),   U16LE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(U16LE_NORM),  FORMAT(S32_NORM),   U16LE_NORM_1,   NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(U16BE),       FORMAT(S32),        U16BE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(U16BE),       FORMAT(S32),        U16BE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(U16BE_NORM),  FORMAT(S32_NORM),   U16BE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(U16BE_NORM),  FORMAT(S32_NORM),   U16BE_NORM_1,   NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S32),        S16LE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S32),        S16LE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S32),        S16LE_N1,       NE(S32,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S32_NORM),   S16LE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S32_NORM),   S16LE_NORM_1,   NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S32_NORM),   S16LE_NORM_N1,  NE(S32,_NORM_N1),   0x00010000, DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S32),        S16BE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S32),        S16BE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S32),        S16BE_N1,       NE(S32,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S32_NORM),   S16BE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S32_NORM),   S16BE_NORM_1,   NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S32_NORM),   S16BE_NORM_N1,  NE(S32,_NORM_N1),   0x00010000, DEF_F64_EPSILON},
  {FORMAT(U32LE),       FORMAT(S32),        U32LE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(U32LE),       FORMAT(S32),        U32LE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(U32LE_NORM),  FORMAT(S32_NORM),   U32LE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(U32LE_NORM),  FORMAT(S32_NORM),   U32LE_NORM_1,   NE(S32,_NORM_1),    0x00000001, DEF_F64_EPSILON},
  {FORMAT(U32BE),       FORMAT(S32),        U32BE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(U32BE),       FORMAT(S32),        U32BE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(U32BE_NORM),  FORMAT(S32_NORM),   U32BE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(U32BE_NORM),  FORMAT(S32_NORM),   U32BE_NORM_1,   NE(S32,_NORM_1),    0x00000001, DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32),        S32LE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32),        S32LE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32),        S32LE_N1,       NE(S32,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32_NORM),   S32LE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32_NORM),   S32LE_NORM_1,   NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32_NORM),   S32LE_NORM_N1,  NE(S32,_NORM_N1),   0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32),        S32BE_0,        NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32),        S32BE_1,        NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32),        S32BE_N1,       NE(S32,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32_NORM),   S32BE_NORM_0,   NE(S32,_NORM_0),    0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32_NORM),   S32BE_NORM_1,   NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32_NORM),   S32BE_NORM_N1,  NE(S32,_NORM_N1),   0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F64),        F32LE_0,        NE(F64,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F64),        F32LE_1,        NE(F64,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F64),        F32LE_N1,       NE(F64,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F64),        F32BE_0,        NE(F64,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F64),        F32BE_1,        NE(F64,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F64),        F32BE_N1,       NE(F64,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64),        F64LE_0,        NE(F64,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64),        F64LE_1,        NE(F64,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64),        F64LE_N1,       NE(F64,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64),        F64BE_0,        NE(F64,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64),        F64BE_1,        NE(F64,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64),        F64BE_N1,       NE(F64,_N1),        0,          DEF_F64_EPSILON},
};
/* *INDENT-ON* */

static void
test_pack_unpack_info (const struct convert_info *info)
{
  const Gst3DVertexFormatInfo *finfo, *unpack_finfo;
//  double res_f64;

  finfo = gst_3d_vertex_format_get_info (info->from);
  unpack_finfo = gst_3d_vertex_format_get_info (finfo->unpack_format);
  if (!unpack_finfo)
    return;

  if (finfo->flags & F_FLAG (INTEGER)) {
    gint32 res = 0, to = 0, from = 0, native_res;

    from = read_s32_as_native (finfo, info->from_value);
    to = read_s32_as_native (unpack_finfo, info->to_value);

    GST_INFO ("testing unpack of %s from 0x%x to 0x%08x with epsilon 0x%08x",
        finfo->name, from, to, info->epsilon_s32);
    finfo->unpack_func (finfo, 0, &res, (guint8 *) info->from_value, 1);
    native_res = read_s32_as_native (unpack_finfo, (guint8 *) & res);
    GST_LOG ("result: 0x%08x, native: 0x%08x, change: 0x%08x", res, native_res,
        ABS (native_res - to));
    fail_unless (ABS (native_res - to) <= info->epsilon_s32);

    res = 0;
    GST_INFO ("testing pack of %s from 0x%08x to 0x%x with epsilon 0x%08x",
        finfo->name, to, from, info->epsilon_s32);
    finfo->pack_func (finfo, 0, (guint8 *) info->to_value, (guint8 *) & res, 1);
    native_res = read_s32_as_native (finfo, (guint8 *) & res);
    GST_LOG ("result: 0x%08x, native: 0x%08x, change: 0x%08x", res, native_res,
        ABS (native_res - from));
    fail_unless (ABS (native_res - from) <= info->epsilon_s32);
  } else if (finfo->flags & F_FLAG (FLOAT)) {
    double res = 0, to = 0, from = 0, native_res;

    from = read_f64_as_native (finfo, info->from_value);
    to = read_f64_as_native (unpack_finfo, info->to_value);

    GST_INFO ("testing unpack of %s from %f to %f with epsilon %f", finfo->name,
        from, to, info->epsilon_f64);
    finfo->unpack_func (finfo, 0, &res, (guint8 *) info->from_value, 1);
    native_res = read_f64_as_native (unpack_finfo, (guint8 *) & res);
    GST_LOG ("result: %f, change: %f", native_res, ABS (native_res - to));
    fail_unless (ABS (native_res - to) <= info->epsilon_f64);

    res = 0;
    GST_INFO ("testing pack of %s from %f to %f with epsilon %f", finfo->name,
        to, from, info->epsilon_f64);
    finfo->pack_func (finfo, 0, (guint8 *) info->to_value, (guint8 *) & res, 1);
    native_res = read_f64_as_native (finfo, (guint8 *) & res);
    GST_LOG ("result: %f, change: %f", native_res, ABS (native_res - from));
    fail_unless (ABS (native_res - from) <= info->epsilon_f64);
  } else {
    g_assert_not_reached ();
  }
}

GST_START_TEST (test_pack_unpack)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (pack_unpack); i++)
    test_pack_unpack_info (&pack_unpack[i]);
}

GST_END_TEST;

/* *INDENT-OFF* */
static const struct convert_info passthrough[] = {
  {FORMAT(U8),          FORMAT(U8),         U8_0,           U8_0,           0,          DEF_F64_EPSILON},
  {FORMAT(U8),          FORMAT(U8),         U8_1,           U8_1,           0,          DEF_F64_EPSILON},
  {FORMAT(U8_NORM),     FORMAT(U8_NORM),    U8_NORM_0,      U8_NORM_0,      0,          DEF_F64_EPSILON},
  {FORMAT(U8_NORM),     FORMAT(U8_NORM),    U8_NORM_1,      U8_NORM_1,      0,          DEF_F64_EPSILON},
  {FORMAT(S8),          FORMAT(S8),         S8_0,           S8_0,           0,          DEF_F64_EPSILON},
  {FORMAT(S8),          FORMAT(S8),         S8_1,           S8_1,           0,          DEF_F64_EPSILON},
  {FORMAT(S8),          FORMAT(S8),         S8_N1,          S8_N1,          0,          DEF_F64_EPSILON},
  {FORMAT(S8_NORM),     FORMAT(S8_NORM),    S8_NORM_0,      S8_NORM_0,      0,          DEF_F64_EPSILON},
  {FORMAT(S8_NORM),     FORMAT(S8_NORM),    S8_NORM_1,      S8_NORM_1,      0,          DEF_F64_EPSILON},
  {FORMAT(S8_NORM),     FORMAT(S8_NORM),    S8_NORM_N1,     S8_NORM_N1,     0,          DEF_F64_EPSILON},
  {FORMAT(U16LE),       FORMAT(U16LE),      U16LE_0,        U16LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U16LE),       FORMAT(U16LE),      U16LE_1,        U16LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U16LE_NORM),  FORMAT(U16LE_NORM), U16LE_NORM_0,   U16LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U16LE_NORM),  FORMAT(U16LE_NORM), U16LE_NORM_1,   U16LE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(U16BE),       FORMAT(U16BE),      U16BE_0,        U16BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U16BE),       FORMAT(U16BE),      U16BE_1,        U16BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U16BE_NORM),  FORMAT(U16BE_NORM), U16BE_NORM_0,   U16BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U16BE_NORM),  FORMAT(U16BE_NORM), U16BE_NORM_1,   U16BE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S16LE),      S16LE_0,        S16LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S16LE),      S16LE_1,        S16LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S16LE),      S16LE_N1,       S16LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S16LE_NORM), S16LE_NORM_0,   S16LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S16LE_NORM), S16LE_NORM_1,   S16LE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S16LE_NORM), S16LE_NORM_N1,  S16LE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S16BE),      S16BE_0,        S16BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S16BE),      S16BE_1,        S16BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S16BE),      S16BE_N1,       S16BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S16BE_NORM), S16BE_NORM_0,   S16BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S16BE_NORM), S16BE_NORM_1,   S16BE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S16BE_NORM), S16BE_NORM_N1,  S16BE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(U32LE),       FORMAT(U32LE),      U32LE_0,        U32LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U32LE),       FORMAT(U32LE),      U32LE_1,        U32LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U32LE_NORM),  FORMAT(U32LE_NORM), U32LE_NORM_0,   U32LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U32LE_NORM),  FORMAT(U32LE_NORM), U32LE_NORM_1,   U32LE_NORM_1,   0x00000001, DEF_F64_EPSILON},
  {FORMAT(U32BE),       FORMAT(U32BE),      U32BE_0,        U32BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U32BE),       FORMAT(U32BE),      U32BE_1,        U32BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U32BE_NORM),  FORMAT(U32BE_NORM), U32BE_NORM_0,   U32BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U32BE_NORM),  FORMAT(U32BE_NORM), U32BE_NORM_1,   U32BE_NORM_1,   0x00000001, DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32LE),      S32LE_0,        S32LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32LE),      S32LE_1,        S32LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32LE),      S32LE_N1,       S32LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32LE_NORM), S32LE_NORM_0,   S32LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32LE_NORM), S32LE_NORM_1,   S32LE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32LE_NORM), S32LE_NORM_N1,  S32LE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32BE),      S32BE_0,        S32BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32BE),      S32BE_1,        S32BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32BE),      S32BE_N1,       S32BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32BE_NORM), S32BE_NORM_0,   S32BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32BE_NORM), S32BE_NORM_1,   S32BE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32BE_NORM), S32BE_NORM_N1,  S32BE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F32LE),      F32LE_0,        F32LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F32LE),      F32LE_1,        F32LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F32LE),      F32LE_N1,       F32LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F32BE),      F32BE_0,        F32BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F32BE),      F32BE_1,        F32BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F32BE),      F32BE_N1,       F32BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64LE),      F64LE_0,        F64LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64LE),      F64LE_1,        F64LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64LE),      F64LE_N1,       F64LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64BE),      F64BE_0,        F64BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64BE),      F64BE_1,        F64BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64BE),      F64BE_N1,       F64BE_N1,       0,          DEF_F64_EPSILON},
};
/* *INDENT-ON* */

static void
_convert_data (Gst3DVertexConverter * convert, Gst3DVertexInfo * in_info,
    Gst3DVertexInfo * out_info, guint8 * src, gsize in_size,
    guint8 * dst, gsize out_size)
{
  GstBuffer *in, *out;

  in = gst_buffer_new_wrapped_full (0, src, in_size, 0, in_size, NULL, NULL);
  gst_buffer_add_3d_vertex_meta (in, 0, in_info, 0, 1);
  out = gst_buffer_new_wrapped_full (0, dst, out_size, 0, out_size, NULL, NULL);
  gst_buffer_add_3d_vertex_meta (out, 0, out_info, 0, 1);

  fail_unless (gst_3d_vertex_converter_vertices (convert, in, out));

  gst_buffer_unref (in);
  gst_buffer_unref (out);
}

static void
test_conversion_info (const struct convert_info *info)
{
  Gst3DVertexInfo in_info, out_info;
  Gst3DVertexConverter *convert;

  gst_3d_vertex_info_init (&in_info, 1);
  in_info.attributes[0].finfo = gst_3d_vertex_format_get_info (info->from);
  in_info.attributes[0].type = GST_3D_VERTEX_TYPE_POSITION;
  in_info.attributes[0].channels = 1;
  in_info.strides[0] = in_info.attributes[0].finfo->width / 8;
  gst_3d_vertex_info_init (&out_info, 1);
  out_info.attributes[0].finfo = gst_3d_vertex_format_get_info (info->to);
  out_info.attributes[0].type = GST_3D_VERTEX_TYPE_POSITION;
  out_info.attributes[0].channels = 1;
  out_info.strides[0] = out_info.attributes[0].finfo->width / 8;

  if ((in_info.attributes[0].finfo->flags & F_FLAG (INTEGER))
      && (out_info.attributes[0].finfo->flags & F_FLAG (INTEGER))) {
    gint32 res = 0, to = 0, from = 0, native_res;

    from = read_s32_as_native (in_info.attributes[0].finfo, info->from_value);
    to = read_s32_as_native (out_info.attributes[0].finfo, info->to_value);

    GST_INFO ("testing conversion from %s to %s from 0x%x to 0x%08x "
        "with epsilon 0x%08x", in_info.attributes[0].finfo->name,
        out_info.attributes[0].finfo->name, from, to, info->epsilon_s32);
    convert = gst_3d_vertex_converter_new (&in_info, &out_info, NULL);
    fail_unless (convert != NULL);

    _convert_data (convert, &in_info, &out_info, (guint8 *) info->from_value,
        in_info.strides[0], (guint8 *) & res, out_info.strides[0]);
    native_res =
        read_s32_as_native (out_info.attributes[0].finfo, (guint8 *) & res);
    GST_LOG ("result: 0x%08x, native: 0x%08x, change: 0x%08x", res, native_res,
        ABS (native_res - to));
    fail_unless (ABS (native_res - to) <= info->epsilon_s32);
  } else if ((in_info.attributes[0].finfo->flags & F_FLAG (FLOAT))
      && (out_info.attributes[0].finfo->flags & F_FLAG (FLOAT))) {
    double res = 0, to = 0, from = 0, native_res;

    from = read_f64_as_native (in_info.attributes[0].finfo, info->from_value);
    to = read_f64_as_native (out_info.attributes[0].finfo, info->to_value);

    GST_INFO ("testing conversion from %s to %s from %f to %f "
        "with epsilon %.15f", in_info.attributes[0].finfo->name,
        out_info.attributes[0].finfo->name, from, to, info->epsilon_f64);
    convert = gst_3d_vertex_converter_new (&in_info, &out_info, NULL);
    fail_unless (convert != NULL);

    _convert_data (convert, &in_info, &out_info, (guint8 *) info->from_value,
        in_info.strides[0], (guint8 *) & res, out_info.strides[0]);
    native_res =
        read_f64_as_native (out_info.attributes[0].finfo, (guint8 *) & res);
    GST_LOG ("result: %f, change: %.15f", native_res, ABS (native_res - to));
    fail_unless (ABS (native_res - to) <= info->epsilon_f64);
  } else if ((in_info.attributes[0].finfo->flags & F_FLAG (FLOAT))
      && (out_info.attributes[0].finfo->flags & F_FLAG (INTEGER))) {
    gint32 res, native_res, to;
    double from;

    from = read_f64_as_native (in_info.attributes[0].finfo, info->from_value);
    to = read_s32_as_native (out_info.attributes[0].finfo, info->to_value);

    GST_INFO ("testing conversion from %s to %s from %f to 0x%08x "
        "with epsilon 0x%08x", in_info.attributes[0].finfo->name,
        out_info.attributes[0].finfo->name, from, to, info->epsilon_s32);
    convert = gst_3d_vertex_converter_new (&in_info, &out_info, NULL);
    fail_unless (convert != NULL);

    _convert_data (convert, &in_info, &out_info, (guint8 *) info->from_value,
        in_info.strides[0], (guint8 *) & res, out_info.strides[0]);
    native_res =
        read_s32_as_native (out_info.attributes[0].finfo, (guint8 *) & res);
    GST_LOG ("result: 0x%08x, native: 0x%08x, change: 0x%08x", res, native_res,
        ABS (native_res - to));
    fail_unless (ABS (native_res - to) <= info->epsilon_s32);
  } else if ((in_info.attributes[0].finfo->flags & F_FLAG (INTEGER))
      && (out_info.attributes[0].finfo->flags & F_FLAG (FLOAT))) {
    double res, native_res, to;
    gint32 from;

    from = read_s32_as_native (in_info.attributes[0].finfo, info->from_value);
    to = read_f64_as_native (out_info.attributes[0].finfo, info->to_value);

    GST_INFO ("testing conversion from %s to %s from 0x%08x to %f "
        "with epsilon %.15f", in_info.attributes[0].finfo->name,
        out_info.attributes[0].finfo->name, from, to, info->epsilon_f64);
    convert = gst_3d_vertex_converter_new (&in_info, &out_info, NULL);
    fail_unless (convert != NULL);

    _convert_data (convert, &in_info, &out_info, (guint8 *) info->from_value,
        in_info.strides[0], (guint8 *) & res, out_info.strides[0]);
    native_res =
        read_f64_as_native (out_info.attributes[0].finfo, (guint8 *) & res);
    GST_LOG ("result: %f, change: %.15f", native_res, ABS (native_res - to));
    fail_unless (ABS (native_res - to) <= info->epsilon_f64);
  } else {
    g_assert_not_reached ();
  }

  gst_3d_vertex_info_unset (&in_info);
  gst_3d_vertex_info_unset (&out_info);
  gst_3d_vertex_converter_free (convert);
}

GST_START_TEST (test_convert_value_passthrough)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (passthrough); i++)
    test_conversion_info (&passthrough[i]);
}

GST_END_TEST;

/* *INDENT-OFF* */
static const struct convert_info endianess[] =
{
  {FORMAT(U16LE),       FORMAT(U16BE),      U16LE_0,        U16BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U16LE),       FORMAT(U16BE),      U16LE_1,        U16BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U16LE_NORM),  FORMAT(U16BE_NORM), U16LE_NORM_0,   U16BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U16LE_NORM),  FORMAT(U16BE_NORM), U16LE_NORM_1,   U16BE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(U16BE),       FORMAT(U16LE),      U16BE_0,        U16LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U16BE),       FORMAT(U16LE),      U16BE_1,        U16LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U16BE_NORM),  FORMAT(U16LE_NORM), U16BE_NORM_0,   U16LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U16BE_NORM),  FORMAT(U16LE_NORM), U16BE_NORM_1,   U16LE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S16BE),      S16LE_0,        S16BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S16BE),      S16LE_1,        S16BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S16LE),       FORMAT(S16BE),      S16LE_N1,       S16BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S16BE_NORM), S16LE_NORM_0,   S16BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S16BE_NORM), S16LE_NORM_1,   S16BE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S16LE_NORM),  FORMAT(S16BE_NORM), S16LE_NORM_N1,  S16BE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S16LE),      S16BE_0,        S16LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S16LE),      S16BE_1,        S16LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S16BE),       FORMAT(S16LE),      S16BE_N1,       S16LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S16LE_NORM), S16BE_NORM_0,   S16LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S16LE_NORM), S16BE_NORM_1,   S16LE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S16BE_NORM),  FORMAT(S16LE_NORM), S16BE_NORM_N1,  S16LE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(U32LE),       FORMAT(U32BE),      U32LE_0,        U32BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U32LE),       FORMAT(U32BE),      U32LE_1,        U32BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U32LE_NORM),  FORMAT(U32BE_NORM), U32LE_NORM_0,   U32BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U32LE_NORM),  FORMAT(U32BE_NORM), U32LE_NORM_1,   U32BE_NORM_1,   0x00000001, DEF_F64_EPSILON},
  {FORMAT(U32BE),       FORMAT(U32LE),      U32BE_0,        U32LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(U32BE),       FORMAT(U32LE),      U32BE_1,        U32LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(U32BE_NORM),  FORMAT(U32LE_NORM), U32BE_NORM_0,   U32LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(U32BE_NORM),  FORMAT(U32LE_NORM), U32BE_NORM_1,   U32LE_NORM_1,   0x00000001, DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32BE),      S32LE_0,        S32BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32BE),      S32LE_1,        S32BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S32LE),       FORMAT(S32BE),      S32LE_N1,       S32BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32BE_NORM), S32LE_NORM_0,   S32BE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32BE_NORM), S32LE_NORM_1,   S32BE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S32LE_NORM),  FORMAT(S32BE_NORM), S32LE_NORM_N1,  S32BE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32LE),      S32BE_0,        S32LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32LE),      S32BE_1,        S32LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(S32BE),       FORMAT(S32LE),      S32BE_N1,       S32LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32LE_NORM), S32BE_NORM_0,   S32LE_NORM_0,   0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32LE_NORM), S32BE_NORM_1,   S32LE_NORM_1,   0,          DEF_F64_EPSILON},
  {FORMAT(S32BE_NORM),  FORMAT(S32LE_NORM), S32BE_NORM_N1,  S32LE_NORM_N1,  0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F32BE),      F32LE_0,        F32BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F32BE),      F32LE_1,        F32BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F32LE),       FORMAT(F32BE),      F32LE_N1,       F32BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F32LE),      F32BE_0,        F32LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F32LE),      F32BE_1,        F32LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F32BE),       FORMAT(F32LE),      F32BE_N1,       F32LE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64BE),      F64LE_0,        F64BE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64BE),      F64LE_1,        F64BE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F64LE),       FORMAT(F64BE),      F64LE_N1,       F64BE_N1,       0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64LE),      F64BE_0,        F64LE_0,        0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64LE),      F64BE_1,        F64LE_1,        0,          DEF_F64_EPSILON},
  {FORMAT(F64BE),       FORMAT(F64LE),      F64BE_N1,       F64LE_N1,       0,          DEF_F64_EPSILON},
};
/* *INDENT-ON* */

GST_START_TEST (test_convert_value_endianess)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (endianess); i++)
    test_conversion_info (&endianess[i]);
}

GST_END_TEST;

/* *INDENT-OFF* */
static const struct convert_info representations[] = {
  {FORMAT(S32),         FORMAT(F64),        NE(S32,_0),         NE(F64,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S32),         FORMAT(F64),        NE(S32,_1),         NE(F64,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S32),         FORMAT(F64),        NE(S32,_N1),        NE(F64,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(F64),         FORMAT(S32),        NE(F64,_0),         NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(F64),         FORMAT(S32),        NE(F64,_1),         NE(S32,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(F64),         FORMAT(S32),        NE(F64,_N1),        NE(S32,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(S32_NORM),    FORMAT(F64),        NE(S32,_0),         NE(F64,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(S32_NORM),    FORMAT(F64),        NE(S32,_NORM_1),    NE(F64,_1),         0,          DEF_F64_EPSILON},
  {FORMAT(S32_NORM),    FORMAT(F64),        NE(S32,_NORM_N1),   NE(F64,_N1),        0,          DEF_F64_EPSILON},
  {FORMAT(F64),         FORMAT(S32_NORM),   NE(F64,_0),         NE(S32,_0),         0,          DEF_F64_EPSILON},
  {FORMAT(F64),         FORMAT(S32_NORM),   NE(F64,_1),         NE(S32,_NORM_1),    0,          DEF_F64_EPSILON},
  {FORMAT(F64),         FORMAT(S32_NORM),   NE(F64,_N1),        NE(S32,_NORM_N1),   0x00000001, DEF_F64_EPSILON},
};
/* *INDENT-ON* */

GST_START_TEST (test_convert_value_representation)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (representations); i++)
    test_conversion_info (&representations[i]);
}

GST_END_TEST;

GST_START_TEST (test_caps)
{
  Gst3DVertexInfo info, tmp;
  GstCaps *caps;

  gst_3d_vertex_info_init (&info, 1);

  /* interleaved */
  info.primitive_mode = GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLES;
  info.attributes[0].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  info.attributes[0].type = GST_3D_VERTEX_TYPE_POSITION;
  info.attributes[0].channels = 4;
  info.offsets[0] = 0;
  info.strides[0] =
      info.attributes[0].channels * info.attributes[0].finfo->width / 8;

  caps = gst_3d_vertex_info_to_caps (&info);
  fail_unless (caps != NULL);
  fail_unless (gst_3d_vertex_info_from_caps (&tmp, caps));
  fail_unless (info.n_attribs == tmp.n_attribs);
  fail_unless (info.attributes[0].finfo == tmp.attributes[0].finfo);
  fail_unless (info.attributes[0].type == tmp.attributes[0].type);
  fail_unless (info.attributes[0].channels == tmp.attributes[0].channels);
  fail_unless (info.offsets[0] == tmp.offsets[0]);
  fail_unless (info.strides[0] == tmp.strides[0]);

  gst_3d_vertex_info_unset (&tmp);
  gst_caps_unref (caps);
  gst_3d_vertex_info_set_n_attributes (&info, 2);

  info.attributes[1].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_S16_NORM);
  info.attributes[1].type = GST_3D_VERTEX_TYPE_NORMAL;
  info.attributes[1].channels = 3;
  info.offsets[1] = info.strides[0];
  info.strides[0] = info.strides[1] =
      info.strides[0] +
      info.attributes[1].channels * info.attributes[1].finfo->width / 8;

  caps = gst_3d_vertex_info_to_caps (&info);
  fail_unless (caps != NULL);
  fail_unless (gst_3d_vertex_info_from_caps (&tmp, caps));
  fail_unless (info.n_attribs == tmp.n_attribs);
  fail_unless (info.attributes[0].finfo == tmp.attributes[0].finfo);
  fail_unless (info.attributes[0].type == tmp.attributes[0].type);
  fail_unless (info.attributes[0].channels == tmp.attributes[0].channels);
  fail_unless (info.offsets[0] == tmp.offsets[0]);
  fail_unless (info.strides[0] == tmp.strides[0]);
  fail_unless (info.attributes[1].finfo == tmp.attributes[1].finfo);
  fail_unless (info.attributes[1].type == tmp.attributes[1].type);
  fail_unless (info.attributes[1].channels == tmp.attributes[1].channels);
  fail_unless (info.offsets[1] == tmp.offsets[1]);
  fail_unless (info.strides[1] == tmp.strides[1]);

  gst_3d_vertex_info_unset (&info);
  gst_3d_vertex_info_unset (&tmp);
  gst_caps_unref (caps);
}

GST_END_TEST;

static Suite *
gst_3d_vertex_suite (void)
{
  Suite *s = suite_create ("Gst3DVertex");
  TCase *tc_chain = tcase_create ("general");
#if 0
  guint8 tmp[sizeof (double)];
  double d;
  float f;
#endif

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_format_description);
  tcase_add_test (tc_chain, test_n_attributes_change);
  tcase_add_test (tc_chain, test_caps);
  tcase_add_test (tc_chain, test_sizes);
  tcase_add_test (tc_chain, test_pack_unpack);
  tcase_add_test (tc_chain, test_convert_value_passthrough);
  tcase_add_test (tc_chain, test_convert_value_endianess);
  tcase_add_test (tc_chain, test_convert_value_representation);

#if 0
#define BYTE_FMT "%02x"
#define F_ARR_FMT BYTE_FMT BYTE_FMT BYTE_FMT BYTE_FMT
#define F_ARR_ARG(arr) arr[0], arr[1], arr[2], arr[3]
  f = -1.f;
  memcpy (tmp, &f, sizeof (f));
  GST_ERROR ("%f - 0x" F_ARR_FMT, f, F_ARR_ARG (tmp));
#define D_ARR_FMT BYTE_FMT BYTE_FMT BYTE_FMT BYTE_FMT BYTE_FMT BYTE_FMT BYTE_FMT BYTE_FMT
#define D_ARR_ARG(arr) arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7]
  d = -1.;
  memcpy (tmp, &d, sizeof (d));
  GST_ERROR ("%f - 0x" D_ARR_FMT, d, D_ARR_ARG (tmp));
#endif

  return s;
}

GST_CHECK_MAIN (gst_3d_vertex);
