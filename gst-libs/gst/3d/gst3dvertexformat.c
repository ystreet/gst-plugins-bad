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

#include "gst3dvertexformat.h"
#include "gst3dvertexpack.h"

#ifdef HAVE_ORC
#include <orc/orcfunctions.h>
#else
#define orc_memset memset
#endif

GST_DEBUG_CATEGORY_STATIC (trid_vertex_format_debug);
#define GST_CAT_DEFAULT trid_vertex_format_debug

#define vertex_orc_unpack_S8 vertex_orc_unpack_s8
/*#define vertex_orc_unpack_S8_NORM vertex_orc_unpack_s8norm */
#define vertex_orc_unpack_S8_NORM_trunc vertex_orc_unpack_s8norm_trunc
#define vertex_orc_unpack_U8 vertex_orc_unpack_u8
#define vertex_orc_unpack_U8_NORM vertex_orc_unpack_u8norm
#define vertex_orc_unpack_U8_NORM_trunc vertex_orc_unpack_u8norm_trunc
#define vertex_orc_pack_S8 vertex_orc_pack_s8
#define vertex_orc_pack_S8_NORM vertex_orc_pack_s8norm
#define vertex_orc_pack_U8 vertex_orc_pack_u8
#define vertex_orc_pack_U8_NORM vertex_orc_pack_u8norm
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define vertex_orc_unpack_S16LE vertex_orc_unpack_s16
/*# define vertex_orc_unpack_S16LE_NORM vertex_orc_unpack_s16norm */
# define vertex_orc_unpack_S16LE_NORM vertex_orc_unpack_S16NE_NORM
# define vertex_orc_unpack_S16LE_NORM_trunc vertex_orc_unpack_s16norm_trunc
# define vertex_orc_unpack_S16BE vertex_orc_unpack_s16_swap
/*# define vertex_orc_unpack_S16BE_NORM vertex_orc_unpack_s16norm_swap */
# define vertex_orc_unpack_S16BE_NORM vertex_orc_unpack_S16OE_NORM
# define vertex_orc_unpack_S16BE_NORM_trunc vertex_orc_unpack_s16norm_swap_trunc
# define vertex_orc_unpack_U16LE vertex_orc_unpack_u16
# define vertex_orc_unpack_U16LE_NORM vertex_orc_unpack_u16norm
# define vertex_orc_unpack_U16LE_NORM_trunc vertex_orc_unpack_u16norm_trunc
# define vertex_orc_unpack_U16BE vertex_orc_unpack_u16_swap
# define vertex_orc_unpack_U16BE_NORM vertex_orc_unpack_u16norm_swap
# define vertex_orc_unpack_U16BE_NORM_trunc vertex_orc_unpack_u16norm_swap_trunc
# define vertex_orc_unpack_S32LE vertex_orc_unpack_s32
# define vertex_orc_unpack_S32LE_NORM vertex_orc_unpack_s32
# define vertex_orc_unpack_S32BE vertex_orc_unpack_s32_swap
# define vertex_orc_unpack_S32BE_NORM vertex_orc_unpack_s32_swap
# define vertex_orc_unpack_U32LE vertex_orc_unpack_u32
# define vertex_orc_unpack_U32LE_NORM vertex_orc_unpack_u32norm
# define vertex_orc_unpack_U32BE vertex_orc_unpack_u32_swap
# define vertex_orc_unpack_U32BE_NORM vertex_orc_unpack_u32norm_swap
# define vertex_orc_unpack_F32LE vertex_orc_unpack_f32
# define vertex_orc_unpack_F32BE vertex_orc_unpack_f32_swap
# define vertex_orc_unpack_F64LE vertex_orc_unpack_f64
# define vertex_orc_unpack_F64BE vertex_orc_unpack_f64_swap
# define vertex_orc_pack_S16LE vertex_orc_pack_s16
# define vertex_orc_pack_S16LE_NORM vertex_orc_pack_s16norm
# define vertex_orc_pack_S16BE vertex_orc_pack_s16_swap
# define vertex_orc_pack_S16BE_NORM vertex_orc_pack_s16norm_swap
# define vertex_orc_pack_U16LE vertex_orc_pack_u16
# define vertex_orc_pack_U16LE_NORM vertex_orc_pack_u16norm
# define vertex_orc_pack_U16BE vertex_orc_pack_u16_swap
# define vertex_orc_pack_U16BE_NORM vertex_orc_pack_u16norm_swap
# define vertex_orc_pack_S32LE vertex_orc_pack_s32
# define vertex_orc_pack_S32LE_NORM vertex_orc_pack_s32
# define vertex_orc_pack_S32BE vertex_orc_pack_s32_swap
# define vertex_orc_pack_S32BE_NORM vertex_orc_pack_s32_swap
# define vertex_orc_pack_U32LE vertex_orc_pack_u32
# define vertex_orc_pack_U32LE_NORM vertex_orc_pack_u32norm
# define vertex_orc_pack_U32BE vertex_orc_pack_u32_swap
# define vertex_orc_pack_U32BE_NORM vertex_orc_pack_u32norm_swap
# define vertex_orc_pack_F32LE vertex_orc_pack_f32
# define vertex_orc_pack_F32BE vertex_orc_pack_f32_swap
# define vertex_orc_pack_F64LE vertex_orc_pack_f64
# define vertex_orc_pack_F64BE vertex_orc_pack_f64_swap
#else
# define vertex_orc_unpack_S16LE vertex_orc_unpack_s16_swap
/*# define vertex_orc_unpack_S16LE_NORM vertex_orc_unpack_s16norm_swap */
# define vertex_orc_unpack_S16LE_NORM vertex_orc_unpack_S16OE_NORM
# define vertex_orc_unpack_S16LE_NORM_trunc vertex_orc_unpack_s16norm_swap_trunc
# define vertex_orc_unpack_S16BE vertex_orc_unpack_s16
/*# define vertex_orc_unpack_S16BE_NORM vertex_orc_unpack_s16norm */
# define vertex_orc_unpack_S16BE_NORM vertex_orc_unpack_S16NE_NORM
# define vertex_orc_unpack_S16BE_NORM_trunc vertex_orc_unpack_s16norm_trunc
# define vertex_orc_unpack_U16LE vertex_orc_unpack_u16_swap
# define vertex_orc_unpack_U16LE_NORM vertex_orc_unpack_u16norm_swap
# define vertex_orc_unpack_U16LE_NORM_trunc vertex_orc_unpack_u16norm_swap_trunc
# define vertex_orc_unpack_U16BE vertex_orc_unpack_u16
# define vertex_orc_unpack_U16BE_NORM vertex_orc_unpack_u16norm
# define vertex_orc_unpack_U16BE_NORM_trunc vertex_orc_unpack_u16norm_trunc
# define vertex_orc_unpack_S32LE vertex_orc_unpack_s32_swap
# define vertex_orc_unpack_S32LE_NORM vertex_orc_unpack_s32_swap
# define vertex_orc_unpack_S32BE vertex_orc_unpack_s32
# define vertex_orc_unpack_S32BE_NORM vertex_orc_unpack_s32
# define vertex_orc_unpack_U32LE vertex_orc_unpack_u32_swap
# define vertex_orc_unpack_U32LE_NORM vertex_orc_unpack_u32norm_swap
# define vertex_orc_unpack_U32BE vertex_orc_unpack_u32
# define vertex_orc_unpack_U32BE_NORM vertex_orc_unpack_u32norm
# define vertex_orc_unpack_F32LE vertex_orc_unpack_f32_swap
# define vertex_orc_unpack_F32BE vertex_orc_unpack_f32
# define vertex_orc_unpack_F64LE vertex_orc_unpack_f64_swap
# define vertex_orc_unpack_F64BE vertex_orc_unpack_f64
# define vertex_orc_pack_S16LE vertex_orc_pack_s16_swap
# define vertex_orc_pack_S16LE_NORM vertex_orc_pack_s16norm_swap
# define vertex_orc_pack_S16BE vertex_orc_pack_s16
# define vertex_orc_pack_S16BE_NORM vertex_orc_pack_s16norm
# define vertex_orc_pack_U16LE vertex_orc_pack_u16_swap
# define vertex_orc_pack_U16LE_NORM vertex_orc_pack_u16norm_swap
# define vertex_orc_pack_U16BE vertex_orc_pack_u16
# define vertex_orc_pack_U16BE_NORM vertex_orc_pack_u16norm
# define vertex_orc_pack_S32LE vertex_orc_pack_s32_swap
# define vertex_orc_pack_S32LE_NORM vertex_orc_pack_s32_swap
# define vertex_orc_pack_S32BE vertex_orc_pack_s32
# define vertex_orc_pack_S32BE_NORM vertex_orc_pack_s32
# define vertex_orc_pack_U32LE vertex_orc_pack_u32_swap
# define vertex_orc_pack_U32LE_NORM vertex_orc_pack_u32norm_swap
# define vertex_orc_pack_U32BE vertex_orc_pack_u32
# define vertex_orc_pack_U32BE_NORM vertex_orc_pack_u32norm
# define vertex_orc_pack_F32LE vertex_orc_pack_f32_swap
# define vertex_orc_pack_F32BE vertex_orc_pack_f32
# define vertex_orc_pack_F64LE vertex_orc_pack_f64_swap
# define vertex_orc_pack_F64BE vertex_orc_pack_f64
#endif

static void
vertex_orc_unpack_S8_NORM (gint32 * dest, const gint8 * src, gint n)
{
  int sign_mask = 0x00808080;
  int i;

  for (i = 0; i < n; i++) {
    gint8 s = src[i];
    guint8 u32 = s;

    dest[i] = (u32 | u32 << 8 | u32 << 16 | u32 << 24);
    /* this condition doesn't exist in the orc code */
    if (ABS (s) > 0x3F)
      dest[i] = dest[i] ^ sign_mask;
  }
}

static void
vertex_orc_unpack_S16NE_NORM (gint32 * dest, const gint16 * src, gint n)
{
  int sign_mask = 0x00008000;
  int i;

  for (i = 0; i < n; i++) {
    gint16 s = src[i];
    guint16 u32 = s;

    dest[i] = (u32 | u32 << 16);
    /* this condition doesn't exist in the orc code */
    if (ABS (s) > 0x3FFF)
      dest[i] = dest[i] ^ sign_mask;
  }
}

static void
vertex_orc_unpack_S16OE_NORM (gint32 * dest, const gint16 * src, gint n)
{
  int sign_mask = 0x00008000;
  int i;

  for (i = 0; i < n; i++) {
    gint16 s = (src[i] & 0xff) << 8 | (src[i] & 0xff00) >> 8;
    guint16 u32 = s;

    dest[i] = (u32 | u32 << 16);
    /* this condition doesn't exist in the orc code */
    if (ABS (s) > 0x3FFF)
      dest[i] = dest[i] ^ sign_mask;
  }
}

#define MAKE_ORC_PACK_UNPACK(fmt,fmt_trunc)                     \
static void unpack_ ##fmt (const Gst3DVertexFormatInfo *info,   \
    guint flags, gpointer dest,                                 \
    const gpointer data, gint channels) {                       \
  if (FALSE)                                                    \
    vertex_orc_unpack_ ##fmt_trunc (dest, data, channels);      \
  else                                                          \
    vertex_orc_unpack_ ##fmt (dest, data, channels);            \
}                                                               \
static void pack_ ##fmt (const Gst3DVertexFormatInfo *info,     \
    guint flags, const gpointer src,                            \
    gpointer data, gint channels) {                             \
  vertex_orc_pack_ ##fmt (data, src, channels);                 \
}

#define MAKE_ASSERT_ORC_PACK_UNPACK(fmt)                        \
static void unpack_ ##fmt (const Gst3DVertexFormatInfo *info,   \
    guint flags, gpointer dest,                                 \
    const gpointer data, gint channels) {                       \
  g_assert_not_reached ();                                      \
}                                                               \
static void pack_ ##fmt (const Gst3DVertexFormatInfo *info,     \
    guint flags, const gpointer src,                            \
    gpointer data, gint channels) {                             \
  g_assert_not_reached ();                                      \
}

/* *INDENT-OFF* */
#define UNPACK_NULL_0 0, 0
#define UNPACK_S32_1 GST_3D_VERTEX_FORMAT_S32, 1
#define UNPACK_S32_NORM_1 GST_3D_VERTEX_FORMAT_S32_NORM, 1
#define UNPACK_F64_1 GST_3D_VERTEX_FORMAT_F64, 1
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
MAKE_ASSERT_ORC_PACK_UNPACK(F64LE)
MAKE_ASSERT_ORC_PACK_UNPACK(S32LE)
MAKE_ASSERT_ORC_PACK_UNPACK(S32LE_NORM)
MAKE_ORC_PACK_UNPACK(F64BE,         F64BE)
MAKE_ORC_PACK_UNPACK(S32BE,         S32BE)
MAKE_ORC_PACK_UNPACK(S32BE_NORM,    S32BE)
#elif G_BYTE_ORDER == G_BIG_ENDIAN
MAKE_ASSERT_ORC_PACK_UNPACK(F64BE)
MAKE_ASSERT_ORC_PACK_UNPACK(S32BE)
MAKE_ASSERT_ORC_PACK_UNPACK(S32BE_NORM)
MAKE_ORC_PACK_UNPACK(F64LE,         F64LE)
MAKE_ORC_PACK_UNPACK(S32LE,         S32LE)
MAKE_ORC_PACK_UNPACK(S32LE_NORM,    S32LE)
#endif
MAKE_ORC_PACK_UNPACK(S8,            S8)
MAKE_ORC_PACK_UNPACK(S8_NORM,       S8_NORM_trunc)
MAKE_ORC_PACK_UNPACK(S16LE,         S16LE)
MAKE_ORC_PACK_UNPACK(S16LE_NORM,    S16LE_NORM_trunc)
MAKE_ORC_PACK_UNPACK(S16BE,         S16BE)
MAKE_ORC_PACK_UNPACK(S16BE_NORM,    S16BE_NORM_trunc)
MAKE_ORC_PACK_UNPACK(U8,            U8)
MAKE_ORC_PACK_UNPACK(U8_NORM,       U8_NORM_trunc)
MAKE_ORC_PACK_UNPACK(U16LE,         U16LE)
MAKE_ORC_PACK_UNPACK(U16LE_NORM,    U16LE_NORM_trunc)
MAKE_ORC_PACK_UNPACK(U16BE,         U16BE)
MAKE_ORC_PACK_UNPACK(U16BE_NORM,    U16BE_NORM_trunc)
MAKE_ORC_PACK_UNPACK(U32LE,         U32LE)
MAKE_ORC_PACK_UNPACK(U32LE_NORM,    U32LE)
MAKE_ORC_PACK_UNPACK(U32BE,         U32BE)
MAKE_ORC_PACK_UNPACK(U32BE_NORM,    U32BE)
MAKE_ORC_PACK_UNPACK(F32LE,         F32LE)
MAKE_ORC_PACK_UNPACK(F32BE,         F32BE)

#define DEFAULT_VEC4_NE_TYPE(name, type, zero, one)                         \
    static const type default_vec4_ ## name[] = {                           \
        (type)zero, (type)zero, (type)zero, (type)one,                      \
    };
#define DEFAULT_VEC4_OE_TYPE(name, other, type)                             \
    static type default_vec4_ ## name[4];                                   \
    static void                                                             \
    swap_default_vec4_ ## name (void)                                       \
    {                                                                       \
      if (sizeof(type) == 2)                                                \
        vertex_orc_swap_16 ((guint16 *) default_vec4_ ## name,              \
            (guint16 *) default_vec4_ ## other, 4);                         \
      else if (sizeof(type) == 4)                                           \
        vertex_orc_swap_32 ((guint32 *) default_vec4_ ## name,              \
            (guint32 *) default_vec4_ ## other, 4);                         \
      else if (sizeof(type) == 8)                                           \
        vertex_orc_swap_64 ((guint64 *) default_vec4_ ## name,              \
            (guint64 *) default_vec4_ ## other, 4);                         \
    }

DEFAULT_VEC4_NE_TYPE(S8,            gint8,          0,          1)
DEFAULT_VEC4_NE_TYPE(S8_NORM,       gint8,          0,          0x7f)
DEFAULT_VEC4_NE_TYPE(U8,            guint8,         0,          1)
DEFAULT_VEC4_NE_TYPE(U8_NORM,       guint8,         0,          0xff)
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
DEFAULT_VEC4_NE_TYPE(S16LE,         gint16,         0,          1)
DEFAULT_VEC4_NE_TYPE(S16LE_NORM,    gint16,         0,          0x7fff)
DEFAULT_VEC4_NE_TYPE(S32LE,         gint32,         0,          1)
DEFAULT_VEC4_NE_TYPE(S32LE_NORM,    gint32,         0,          0x7fffffff)
DEFAULT_VEC4_NE_TYPE(U16LE,         guint16,        0,          1)
DEFAULT_VEC4_NE_TYPE(U16LE_NORM,    guint16,        0,          0xffff)
DEFAULT_VEC4_NE_TYPE(U32LE,         guint32,        0,          1)
DEFAULT_VEC4_NE_TYPE(U32LE_NORM,    guint32,        0,          0xffffffff)
DEFAULT_VEC4_NE_TYPE(F32LE,         float,          0.0f,       1.0f)
DEFAULT_VEC4_NE_TYPE(F64LE,         double,         0.0,        1.0)
DEFAULT_VEC4_OE_TYPE(S16BE,         S16LE,          gint16)
DEFAULT_VEC4_OE_TYPE(S16BE_NORM,    S16LE_NORM,     gint16)
DEFAULT_VEC4_OE_TYPE(S32BE,         S32LE,          gint32)
DEFAULT_VEC4_OE_TYPE(S32BE_NORM,    S32LE_NORM,     gint32)
DEFAULT_VEC4_OE_TYPE(U16BE,         U16LE,          guint16)
DEFAULT_VEC4_OE_TYPE(U16BE_NORM,    U16LE_NORM,     guint16)
DEFAULT_VEC4_OE_TYPE(U32BE,         U32LE,          guint32)
DEFAULT_VEC4_OE_TYPE(U32BE_NORM,    U32LE_NORM,     guint32)
DEFAULT_VEC4_OE_TYPE(F32BE,         F32LE,          float)
DEFAULT_VEC4_OE_TYPE(F64BE,         F64LE,          double)
#elif G_BYTE_ORDER == G_BIG_ENDIAN
DEFAULT_VEC4_NE_TYPE(S16BE,         gint16,         0,          1)
DEFAULT_VEC4_NE_TYPE(S16BE_NORM,    gint16,         0,          0xffff)
DEFAULT_VEC4_NE_TYPE(S32BE,         gint32,         0,          1)
DEFAULT_VEC4_NE_TYPE(S32BE_NORM,    gint32,         0,          0x7fffffff)
DEFAULT_VEC4_NE_TYPE(U16BE,         guint16,        0,          1)
DEFAULT_VEC4_NE_TYPE(U16BE_NORM,    guint16,        0,          0x7fff)
DEFAULT_VEC4_NE_TYPE(U32BE,         guint32,        0,          1)
DEFAULT_VEC4_NE_TYPE(U32BE_NORM,    guint32,        0,          0xffffffff)
DEFAULT_VEC4_NE_TYPE(F32BE,         float,          0.0f,       1.0f)
DEFAULT_VEC4_NE_TYPE(F64BE,         double,         0.0,        1.0)
DEFAULT_VEC4_OE_TYPE(S16LE,         S16BE,          gint16)
DEFAULT_VEC4_OE_TYPE(S16LE_NORM,    S16BE_NORM,     gint16)
DEFAULT_VEC4_OE_TYPE(S32LE,         S32BE,          gint32)
DEFAULT_VEC4_OE_TYPE(S32LE_NORM,    S32BE_NORM,     gint32)
DEFAULT_VEC4_OE_TYPE(U16LE,         U16BE,          guint16)
DEFAULT_VEC4_OE_TYPE(U16LE_NORM,    U16BE_NORM,     guint16)
DEFAULT_VEC4_OE_TYPE(U32LE,         U32BE,          guint32)
DEFAULT_VEC4_OE_TYPE(U32LE_NORM,    U32BE_NORM,     guint32)
DEFAULT_VEC4_OE_TYPE(F32LE,         F32BE,          float)
DEFAULT_VEC4_OE_TYPE(F64LE,         F64BE,          double)
#endif
/* *INDENT-ON* */

#define F_FLAG(flag) GST_3D_VERTEX_FORMAT_FLAG_ ## flag

#define SHIFT0 {0, 0, 0, 0}

#define DEPTH8 {8, 0, 0, 0}
#define DEPTH16 {16, 0, 0, 0}
#define DEPTH32 {32, 0, 0, 0}
#define DEPTH64 {64, 0, 0, 0}

#define MAKE_FORMAT_INFO(format, desc, flags, bits, n_components, shift, depth, unpack_format) \
    {GST_3D_VERTEX_FORMAT_ ## format, G_STRINGIFY(format), desc, flags, \
        bits, n_components, shift, depth, (guint8 *) default_vec4_ ## format, \
        unpack_format, unpack_ ## format, pack_ ## format}

static const Gst3DVertexFormatInfo vertex_formats[] = {
  MAKE_FORMAT_INFO (S8, "8-bit signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED),
      8, 1, SHIFT0, DEPTH8, UNPACK_S32_1),
  MAKE_FORMAT_INFO (S8_NORM, "8-bit normalized signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (NORMALIZED),
      8, 1, SHIFT0, DEPTH8, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (U8, "8-bit unsigned integer",
      F_FLAG (INTEGER),
      8, 1, SHIFT0, DEPTH8, UNPACK_S32_1),
  MAKE_FORMAT_INFO (U8_NORM, "8-bit normalized unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (NORMALIZED),
      8, 1, SHIFT0, DEPTH8, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (S16LE, "16-bit little endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (LE),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_1),
  MAKE_FORMAT_INFO (S16LE_NORM,
      "16-bit normalized little endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (LE) | F_FLAG (NORMALIZED),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (S16BE, "16-bit big endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_1),
  MAKE_FORMAT_INFO (S16BE_NORM, "16-bit normalized big endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (NORMALIZED),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (U16LE, "16-bit little endian unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (LE),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_1),
  MAKE_FORMAT_INFO (U16LE_NORM,
      "16-bit normalized little endian unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (LE) | F_FLAG (NORMALIZED),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (U16BE, "16-bit big endian unsigned integer",
      F_FLAG (INTEGER),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_1),
  MAKE_FORMAT_INFO (U16BE_NORM, "16-bit normalized big endian unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (NORMALIZED),
      16, 1, SHIFT0, DEPTH16, UNPACK_S32_NORM_1),
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  MAKE_FORMAT_INFO (S32LE, "32-bit little endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (LE),
      32, 1, SHIFT0, DEPTH32, UNPACK_NULL_0),
  MAKE_FORMAT_INFO (S32LE_NORM,
      "32-bit normalized little endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (LE) | F_FLAG (NORMALIZED),
      32, 1, SHIFT0, DEPTH32, UNPACK_NULL_0),
  MAKE_FORMAT_INFO (S32BE, "32-bit big endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_1),
  MAKE_FORMAT_INFO (S32BE_NORM, "32-bit normalized big endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (NORMALIZED),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_NORM_1),
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  MAKE_FORMAT_INFO (S32LE, "32-bit little endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (LE),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_1),
  MAKE_FORMAT_INFO (S32LE_NORM,
      "32-bit normalized little endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (LE) | F_FLAG (NORMALIZED),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (S32BE, "32-bit big endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED),
      32, 1, SHIFT0, DEPTH32, UNPACK_NULL_0),
  MAKE_FORMAT_INFO (S32BE_NORM, "32-bit normalized big endian signed integer",
      F_FLAG (INTEGER) | F_FLAG (SIGNED) | F_FLAG (NORMALIZED),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_NORM_1),
#endif
  MAKE_FORMAT_INFO (U32LE, "32-bit little endian unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (LE),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_1),
  MAKE_FORMAT_INFO (U32LE_NORM,
      "32-bit normalized little endian unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (LE) | F_FLAG (NORMALIZED),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (U32BE, "32-bit big endian unsigned integer",
      F_FLAG (INTEGER),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_1),
  MAKE_FORMAT_INFO (U32BE_NORM, "32-bit normalized big endian unsigned integer",
      F_FLAG (INTEGER) | F_FLAG (NORMALIZED),
      32, 1, SHIFT0, DEPTH32, UNPACK_S32_NORM_1),
  MAKE_FORMAT_INFO (F32LE, "IEEE 32-bit little endian floating point",
      F_FLAG (FLOAT) | F_FLAG (LE),
      32, 1, SHIFT0, DEPTH32, UNPACK_F64_1),
  MAKE_FORMAT_INFO (F32BE, "IEEE 32-bit big endian floating point",
      F_FLAG (FLOAT), 32, 1, SHIFT0, DEPTH32, UNPACK_F64_1),
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  MAKE_FORMAT_INFO (F64LE, "IEEE 64-bit little endian floating point",
      F_FLAG (FLOAT) | F_FLAG (LE),
      64, 1, SHIFT0, DEPTH64, UNPACK_NULL_0),
  MAKE_FORMAT_INFO (F64BE, "IEEE 64-bit big endian floating point",
      F_FLAG (FLOAT),
      64, 1, SHIFT0, DEPTH64, UNPACK_F64_1),
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  MAKE_FORMAT_INFO (F64LE, "IEEE 64-bit little endian floating point",
      F_FLAG (FLOAT) | F_FLAG (LE),
      64, 1, SHIFT0, DEPTH64, UNPACK_F64_1),
  MAKE_FORMAT_INFO (F64BE, "IEEE 64-bit big endian floating point",
      F_FLAG (FLOAT),
      64, 1, SHIFT0, DEPTH64, UNPACK_NULL_0),
#endif
};

static void
_init_formats (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (trid_vertex_format_debug, "3dvertexformat", 0,
        "3D Vertex Format");

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    swap_default_vec4_S16BE ();
    swap_default_vec4_S16BE_NORM ();
    swap_default_vec4_S32BE ();
    swap_default_vec4_S32BE_NORM ();
    swap_default_vec4_U16BE ();
    swap_default_vec4_U16BE_NORM ();
    swap_default_vec4_U32BE ();
    swap_default_vec4_U32BE_NORM ();
    swap_default_vec4_F32BE ();
    swap_default_vec4_F64BE ();
#elif G_BYTE_ORDER == G_BIG_ENDIAN
    swap_default_vec4_S16LE ();
    swap_default_vec4_S16LE_NORM ();
    swap_default_vec4_S32LE ();
    swap_default_vec4_S32LE_NORM ();
    swap_default_vec4_U16LE ();
    swap_default_vec4_U16LE_NORM ();
    swap_default_vec4_U32LE ();
    swap_default_vec4_U32LE_NORM ();
    swap_default_vec4_F32LE ();
    swap_default_vec4_F64LE ();
#endif
    g_once_init_leave (&_init, 1);
  }
}

const Gst3DVertexFormatInfo *
gst_3d_vertex_format_get_info (Gst3DVertexFormat format)
{
  int i;

  _init_formats ();

  for (i = 0; i < G_N_ELEMENTS (vertex_formats); i++) {
    if (vertex_formats[i].format == format)
      return &vertex_formats[i];
  }

  return NULL;
}

Gst3DVertexFormat
gst_3d_vertex_format_from_string (const gchar * s)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (vertex_formats); i++) {
    if (g_strcmp0 (vertex_formats[i].name, s) == 0)
      return vertex_formats[i].format;
  }

  return GST_3D_VERTEX_FORMAT_UNKNOWN;
}

const gchar *
gst_3d_vertex_format_to_string (Gst3DVertexFormat format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (vertex_formats); i++) {
    if (vertex_formats[i].format == format)
      return vertex_formats[i].name;
  }

  return "unknown";
}

#if 0
#define MAKE_GL_FORMAT(format, gl_format) \
    {GST_3D_VERTEX_FORMAT_ ## format, GL_ ## gl_format, ((G_BYTE_ORDER == G_LITTLE_ENDIAN) * GST_3D_VERTEX_FORMAT_FLAG_LE)}

static const struct
{
  Gst3DVertexFormat format;
  guint gl_format;
guint flags} gl_format_table = {
  {GST_3D_VERTEX_FORMAT_S8, GL_BYTE, 0},
  {GST_3D_VERTEX_FORMAT_U8, GL_UNSIGNED_BYTE, 0},
  MAKE_GL_FORMAT (S16LE, SHORT),
  MAKE_GL_FORMAT (S16BE, SHORT),
  MAKE_GL_FORMAT (U16LE, UNSIGNED_SHORT),
  MAKE_GL_FORMAT (U16BE, UNSIGNED_SHORT),
  MAKE_GL_FORMAT (S32LE, INT),
  MAKE_GL_FORMAT (S32BE, INT),
  MAKE_GL_FORMAT (U32LE, UNSIGNED_INT),
  MAKE_GL_FORMAT (U32BE, UNSIGNED_INT),
  MAKE_GL_FORMAT (F16LE, HALF_FLOAT),
  MAKE_GL_FORMAT (F16BE, HALF_FLOAT),
  MAKE_GL_FORMAT (F32LE, FLOAT),
  MAKE_GL_FORMAT (F32BE, FLOAT),
  MAKE_GL_FORMAT (F64LE, DOUBLE),
  MAKE_GL_FORMAT (F64BE, DOUBLE),
  MAKE_GL_FORMAT (S16, SHORT),
  MAKE_GL_FORMAT (U16, UNSIGNED_SHORT),
  MAKE_GL_FORMAT (S32, INT),
  MAKE_GL_FORMAT (U32, UNSIGNED_INT),
  MAKE_GL_FORMAT (F16, HALF_FLOAT),
  MAKE_GL_FORMAT (F32, FLOAT),
  MAKE_GL_FORMAT (F64, DOUBLE),
};

guint
gst_3d_vertex_format_to_gl (Gst3DVertexFormat format)
{
  int i;

  g_return_val_if_fail (format != GST_3D_VERTEX_FORMAT_UNKNOWN, 0);

  for (i = 0; i < G_N_ELEMENTS (gl_format_table); i++) {
    if (gl_format_table[i].format == format) {
      if ((G_BYTE_ORDER == G_LITTLE_ENDIAN &&
              gl_format_table[i].flags & GST_3D_VERTEX_FORMAT_FLAG_LE))
        return gl_format_table[i].gl_format;
      if (G_BYTE_ORDER == G_BIG_ENDIAN &&
          (gl_format_table[i].flags & GST_3D_VERTEX_FORMAT_FLAG_LE) == 0) {
        return gl_format_table[i].gl_format;
      }
    }
  }

  return 0;
}
#endif
