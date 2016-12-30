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

#ifndef __GST_3D_VERTEX_FORMAT_H__
#define __GST_3D_VERTEX_FORMAT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _Gst3DVertexFormatInfo Gst3DVertexFormatInfo;

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define _GST_3D_VERTEX_FORMAT_NE(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## BE
#define _GST_3D_VERTEX_FORMAT_OE(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## LE
#define _GST_3D_VERTEX_FORMAT_NE_NORM(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## BE_NORM
#define _GST_3D_VERTEX_FORMAT_OE_NORM(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## LE_NORM
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
#define _GST_3D_VERTEX_FORMAT_NE(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## LE
#define _GST_3D_VERTEX_FORMAT_OE(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## BE
#define _GST_3D_VERTEX_FORMAT_NE_NORM(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## LE_NORM
#define _GST_3D_VERTEX_FORMAT_OE_NORM(fmt) GST_3D_VERTEX_FORMAT_ ## fmt ## BE_NORM
#endif

/*
 * For a given bit depth, b:
 *
 * signed formats                   [-s^(b-1) - 1, s^(b-1) - 1]
 * signed normalized formats        [-1, 1] note: The lowest value, -2^(b-1)
 *                                          is outside the represented range
 *                                          and must be clamped
 * unsigned formats                 [0, s^b - 1]
 * unsigned normalized formats      [0, 1]
 */

typedef enum
{
  GST_3D_VERTEX_FORMAT_UNKNOWN,
  /* 8 bit */
  GST_3D_VERTEX_FORMAT_S8,
  GST_3D_VERTEX_FORMAT_S8_NORM,
  GST_3D_VERTEX_FORMAT_U8,
  GST_3D_VERTEX_FORMAT_U8_NORM,
  /* 16 bit */
  GST_3D_VERTEX_FORMAT_S16LE,
  GST_3D_VERTEX_FORMAT_S16LE_NORM,
  GST_3D_VERTEX_FORMAT_S16BE,
  GST_3D_VERTEX_FORMAT_S16BE_NORM,
  GST_3D_VERTEX_FORMAT_U16LE,
  GST_3D_VERTEX_FORMAT_U16LE_NORM,
  GST_3D_VERTEX_FORMAT_U16BE,
  GST_3D_VERTEX_FORMAT_U16BE_NORM,
  /* 32 bit */
  GST_3D_VERTEX_FORMAT_S32LE,
  GST_3D_VERTEX_FORMAT_S32LE_NORM,
  GST_3D_VERTEX_FORMAT_S32BE,
  GST_3D_VERTEX_FORMAT_S32BE_NORM,
  GST_3D_VERTEX_FORMAT_U32LE,
  GST_3D_VERTEX_FORMAT_U32LE_NORM,
  GST_3D_VERTEX_FORMAT_U32BE,
  GST_3D_VERTEX_FORMAT_U32BE_NORM,
  /* float */
/*  GST_3D_VERTEX_FORMAT_F16LE,
  GST_3D_VERTEX_FORMAT_F16BE,*/
  GST_3D_VERTEX_FORMAT_F32LE,
  GST_3D_VERTEX_FORMAT_F32BE,
  GST_3D_VERTEX_FORMAT_F64LE,
  GST_3D_VERTEX_FORMAT_F64BE,
  /* native endianness equivalents */
  GST_3D_VERTEX_FORMAT_S16 = _GST_3D_VERTEX_FORMAT_NE(S16),
  GST_3D_VERTEX_FORMAT_S16_NORM = _GST_3D_VERTEX_FORMAT_NE_NORM(S16),
  GST_3D_VERTEX_FORMAT_U16 = _GST_3D_VERTEX_FORMAT_NE(U16),
  GST_3D_VERTEX_FORMAT_U16_NORM = _GST_3D_VERTEX_FORMAT_NE_NORM(U16),
  GST_3D_VERTEX_FORMAT_S32 = _GST_3D_VERTEX_FORMAT_NE(S32),
  GST_3D_VERTEX_FORMAT_S32_NORM = _GST_3D_VERTEX_FORMAT_NE_NORM(S32),
  GST_3D_VERTEX_FORMAT_U32 = _GST_3D_VERTEX_FORMAT_NE(U32),
  GST_3D_VERTEX_FORMAT_U32_NORM = _GST_3D_VERTEX_FORMAT_NE_NORM(U32),
/*  GST_3D_VERTEX_FORMAT_F16 = _GST_3D_VERTEX_FORMAT_NE(F16),*/
  GST_3D_VERTEX_FORMAT_F32 = _GST_3D_VERTEX_FORMAT_NE(F32),
  GST_3D_VERTEX_FORMAT_F64 = _GST_3D_VERTEX_FORMAT_NE(F64)
} Gst3DVertexFormat;

const gchar *       gst_3d_vertex_format_to_string      (Gst3DVertexFormat format);
Gst3DVertexFormat   gst_3d_vertex_format_from_string    (const gchar * s);

#if 0
guint           gst_3d_vertex_format_to_gl (Gst3DVertexFormat format);
#endif

/**
 * Gst3DFormatFlags:
 * @GST_3D_VERTEX_FORMAT_FLAG_INTEGER: integer samples
 * @GST_3D_VERTEX_FORMAT_FLAG_FLOAT: float samples
 * @GST_3D_VERTEX_FORMAT_FLAG_SIGNED: signed samples
 * @GST_3D_VERTEX_FORMAT_FLAG_NORMALIZED: normalized representation
 *      i.e. indicates values in the range from [0, 1] (or [-1, 1] if signed)
 *      the value (2^bit_depth -1) is out of range for normalized signed integers.
 * @GST_3D_VERTEX_FORMAT_FLAG_COMPLEX: complex layout
 * @GST_3D_VERTEX_FORMAT_FLAG_LE: the format is in little endian byte order for multi-byte data types.
 *
 * The different 3D flags that a format info can have.
 */
typedef enum
{
  GST_3D_VERTEX_FORMAT_FLAG_INTEGER     = (1 << 0),
  GST_3D_VERTEX_FORMAT_FLAG_FLOAT       = (1 << 1),
  GST_3D_VERTEX_FORMAT_FLAG_SIGNED      = (1 << 2),
  GST_3D_VERTEX_FORMAT_FLAG_NORMALIZED  = (1 << 3),
  GST_3D_VERTEX_FORMAT_FLAG_COMPLEX     = (1 << 4),
  GST_3D_VERTEX_FORMAT_FLAG_LE          = (1 << 5),
} Gst3DVertexFormatFlags;

/**
 * Gst3DVertexFormatUnpack:
 * @info: a #Gst3DVertexFormatInfo
 * @dest: (array) (element-type guint8): a destination array
 * @data: (array) (element-type guint8): pointer to the vertex data
 * @channels: the number of channels to unpack.
 *
 * Unpacks @length samples from the given data of format @info.
 * The samples will be unpacked into @dest which each channel
 * interleaved. @dest should at least be big enough to hold @channels *
 * size(unpack_format) bytes.
 */
typedef void (*Gst3DVertexFormatUnpack)     (const Gst3DVertexFormatInfo *info,
                                             guint flags, gpointer dest,
                                             const gpointer data, gint channels);

/**
 * Gst3DVertexFormatPack:
 * @info: a #Gst3DVertexFormatInfo
 * @src: (array) (element-type guint8): a source array
 * @data: (array) (element-type guint8): pointer to the destination
 *   data
 * @length: the amount of channels to pack.
 *
 * Packs @channel's from @src to the data array in format @info.
 */
typedef void (*Gst3DVertexFormatPack)       (const Gst3DVertexFormatInfo *info,
                                             guint flags, const gpointer src,
                                             gpointer data, gint channels);

#define GST_3D_VERTEX_FORMAT_MAX_COMPONENTS 4
struct _Gst3DVertexFormatInfo
{
  Gst3DVertexFormat format;
  const gchar *name;
  const gchar *description;

  Gst3DVertexFormatFlags flags;
  gint width;
  gint n_components;
  guint shift[GST_3D_VERTEX_FORMAT_MAX_COMPONENTS];
  guint depth[GST_3D_VERTEX_FORMAT_MAX_COMPONENTS];

  const guint8 *default_vec4; /* vec4(0, 0, 0, 1) for this format */

  Gst3DVertexFormat unpack_format;
  gint unpack_channels;
  Gst3DVertexFormatUnpack unpack_func;
  Gst3DVertexFormatPack pack_func;

  /* <private> */
  gpointer _padding[GST_PADDING];
};

#define GST_3D_VERTEX_FORMAT_INFO_FORMAT(info)           ((info)->format)
#define GST_3D_VERTEX_FORMAT_INFO_NAME(info)             ((info)->name)
#define GST_3D_VERTEX_FORMAT_INFO_FLAGS(info)            ((info)->flags)

#define GST_3D_VERTEX_FORMAT_INFO_IS_INTEGER(info)       !!((info)->flags & GST_3D_VERTEX_FORMAT_FLAG_INTEGER)
#define GST_3D_VERTEX_FORMAT_INFO_IS_FLOAT(info)         !!((info)->flags & GST_3D_VERTEX_FORMAT_FLAG_FLOAT)
#define GST_3D_VERTEX_FORMAT_INFO_IS_SIGNED(info)        !!((info)->flags & GST_3D_VERTEX_FORMAT_FLAG_SIGNED)
#define GST_3D_VERTEX_FORMAT_INFO_IS_LITTLE_ENDIAN(info) !!((info)->flags & GST_3D_VERTEX_FORMAT_FLAG_LE)
#define GST_3D_VERTEX_FORMAT_INFO_IS_BIG_ENDIAN(info)    (((info)->flags & GST_3D_VERTEX_FORMAT_FLAG_LE) == 0)

#define GST_3D_VERTEX_FORMAT_INFO_WIDTH(info)            ((info)->width)
#define GST_3D_VERTEX_FORMAT_INFO_N_COMPONENTS(info)     ((info)->n_components)
#define GST_3D_VERTEX_FORMAT_INFO_SHIFT(info,i)          ((info)->shift[i])
#define GST_3D_VERTEX_FORMAT_INFO_DEPTH(info,i)          ((info)->depth[i])

#define GST_3D_VERTEX_FORMAT_ALL "S8, U8, " \
    "S8_NORM, U8_NORM, " \
    "S16LE, S16BE, U16LE, U16BE, " \
    "S16LE_NORM, S16BE_NORM, U16LE_NORM, U16BE_NORM, " \
    "S32LE, S32BE, U32LE, U32BE, " \
    "S32LE_NORM, S32BE_NORM, U32LE_NORM, U32BE_NORM, " \
    "F32LE, F32BE, F64LE, F64BE"

const Gst3DVertexFormatInfo *       gst_3d_vertex_format_get_info       (Gst3DVertexFormat format);

G_END_DECLS

#endif /* __GST_3D_VERTEX_FORMAT_H__ */
