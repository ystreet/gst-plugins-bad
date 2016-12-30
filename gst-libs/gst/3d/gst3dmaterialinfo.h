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

#ifndef __GST_3D_MATERIAL_INFO_H__
#define __GST_3D_MATERIAL_INFO_H__

#include <gst/gst.h>

#include <gst/video/video.h>

G_BEGIN_DECLS

typedef enum
{
  GST_3D_MATERIAL_ELEMENT_NONE          = 0,
  GST_3D_MATERIAL_ELEMENT_DIFFUSE       = (1 << 0),
  GST_3D_MATERIAL_ELEMENT_SPECULAR      = (1 << 1),
  GST_3D_MATERIAL_ELEMENT_AMBIENT       = (1 << 2),
  GST_3D_MATERIAL_ELEMENT_EMISSIVE      = (1 << 3),
  GST_3D_MATERIAL_ELEMENT_SHININESS     = (1 << 4),
  GST_3D_MATERIAL_ELEMENT_OPACITY       = (1 << 5),
  GST_3D_MATERIAL_ELEMENT_NORMAL_MAP    = (1 << 6),
  GST_3D_MATERIAL_ELEMENT_HEIGHT_MAP    = (1 << 7),
  GST_3D_MATERIAL_ELEMENT_LIGHT_MAP     = (1 << 8),

  GST_3D_MATERIAL_ELEMENT_CUSTOM1       = (1 << 28),           /* FIXME: support */
  GST_3D_MATERIAL_ELEMENT_CUSTOM2       = (1 << 29),           /* FIXME: support */
  GST_3D_MATERIAL_ELEMENT_CUSTOM3       = (1 << 30),           /* FIXME: support */
  GST_3D_MATERIAL_ELEMENT_CUSTOM4       = (1 << 31),           /* FIXME: support */
} Gst3DMaterialElement;

GType gst_3d_material_element_get_type (void);
#define GST_TYPE_3D_MATERIAL_ELEMENT (gst_3d_material_element_get_type())
GType gst_3d_material_element_flagset_get_type (void);
#define GST_TYPE_3D_MATERIAL_ELEMENT_FLAGSET (gst_3d_material_element_flagset_get_type())
const gchar *           gst_3d_material_element_to_string        (Gst3DMaterialElement element);
Gst3DMaterialElement    gst_3d_material_element_from_string      (const gchar * name);

typedef enum
{
  GST_3D_MATERIAL_SHADING_UNKNOWN,
  GST_3D_MATERIAL_SHADING_NONE,
  GST_3D_MATERIAL_SHADING_FLAT,
  GST_3D_MATERIAL_SHADING_GOURAUD,
  GST_3D_MATERIAL_SHADING_PHONG,
  GST_3D_MATERIAL_SHADING_PHONG_BLINN,
  GST_3D_MATERIAL_SHADING_TOON,
  GST_3D_MATERIAL_SHADING_OREN_NAYAR,
  GST_3D_MATERIAL_SHADING_MINNAERT,
  GST_3D_MATERIAL_SHADING_COOK_TORRANCE,
  GST_3D_MATERIAL_SHADING_FRESNEL,
} Gst3DMaterialShading;

#define GST_3D_MATERIAL_SHADING_ALL " none, flat, gouraud, phong, " \
    "blinn, toon, oren-nayar, minnaert, cook-torrance, fresnel "

const gchar *               gst_3d_material_shading_to_string          (Gst3DMaterialShading type);
Gst3DMaterialShading        gst_3d_material_shading_from_string        (const gchar * name);

typedef enum
{
  GST_3D_MATERIAL_COMBINE_OVERWRITE,
  GST_3D_MATERIAL_COMBINE_ADD,
  GST_3D_MATERIAL_COMBINE_MULTIPLY,
} Gst3DMaterialCombineFunction;

const gchar *                   gst_3d_material_combine_function_to_string          (Gst3DMaterialCombineFunction function);
Gst3DMaterialCombineFunction    gst_3d_material_combine_function_from_string        (const gchar * name);

typedef enum
{
  GST_3D_MATERIAL_WRAP_MODE_NONE,
  GST_3D_MATERIAL_WRAP_MODE_WRAP,
  GST_3D_MATERIAL_WRAP_MODE_CLAMP,
  GST_3D_MATERIAL_WRAP_MODE_DECAL,
  GST_3D_MATERIAL_WRAP_MODE_MIRROR,
} Gst3DMaterialTextureWrapMode;

const gchar *                   gst_3d_material_texture_wrap_mode_to_string          (Gst3DMaterialTextureWrapMode function);
Gst3DMaterialTextureWrapMode    gst_3d_material_texture_wrap_mode_from_string        (const gchar * name);

typedef enum
{
  GST_3D_MATERIAL_TEXTURE_FLAG_NONE,
  GST_3D_MATERIAL_TEXTURE_FLAG_INVERT           = (1 << 0),
  GST_3D_MATERIAL_TEXTURE_FLAG_IGNORE_ALPHA     = (1 << 1),
  GST_3D_MATERIAL_TEXTURE_FLAG_USE_ALPHA        = (1 << 2),
} Gst3DMaterialTextureFlags;

typedef struct _Gst3DMaterialTexture Gst3DMaterialTexture;
typedef struct _Gst3DMaterialStack Gst3DMaterialStack;
typedef struct _Gst3DMaterialInfo Gst3DMaterialInfo;

struct _Gst3DMaterialTexture
{
  float                         multiplier;
  Gst3DMaterialCombineFunction  combiner;
  Gst3DMaterialTextureFlags     flags;
  Gst3DMaterialTextureWrapMode  wrap_mode;

  /* XXX: maybe map/unmap functions for the @sample*/
  GstSample                    *sample;
};

struct _Gst3DMaterialStack
{
  Gst3DMaterialElement type;
  gchar *name;                      /* used if @type is CUSTOM */

  float x, y, z;

  GArray *textures;
};

void                                gst_3d_material_stack_init                  (Gst3DMaterialStack * stack);
void                                gst_3d_material_stack_unset                 (Gst3DMaterialStack * stack);
void                                gst_3d_material_stack_copy_into             (Gst3DMaterialStack * src,
                                                                                 Gst3DMaterialStack * dest);
Gst3DMaterialTexture *              gst_3d_material_stack_get_texture           (Gst3DMaterialStack * stack,
                                                                                 int i);
void                                gst_3d_material_stack_set_n_textures        (Gst3DMaterialStack * stack,
                                                                                 guint n_textures);

struct _Gst3DMaterialInfo
{
  Gst3DMaterialShading       shading;
  Gst3DMaterialElement       elements;
  GstCaps                   *caps;
};

void                            gst_3d_material_info_init                   (Gst3DMaterialInfo * info);
void                            gst_3d_material_info_unset                  (Gst3DMaterialInfo * info);
void                            gst_3d_material_info_copy_into              (Gst3DMaterialInfo * src,
                                                                             Gst3DMaterialInfo * dest);

gboolean                        gst_3d_material_info_from_caps              (Gst3DMaterialInfo * info,
                                                                             const GstCaps * caps);
GstCaps *                       gst_3d_material_info_to_caps                (const Gst3DMaterialInfo * info);


G_END_DECLS

#endif /* __GST_3D_MATERIAL_INFO_H__ */
