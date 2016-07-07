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

#ifndef _GST_ASSIMP_H_
#define _GST_ASSIMP_H_

#include <assimp/cimport.h>
#include <assimp/cfileio.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include <gst/base/gstadapter.h>
#include <gst/video/video.h>
#include <gst/3d/3d.h>

G_BEGIN_DECLS

GType gst_assimp_get_type (void);
#define GST_TYPE_ASSIMP            (gst_assimp_get_type())
#define GST_ASSIMP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ASSIMP,GstAssimp))
#define GST_IS_ASSIMP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ASSIMP))
#define GST_ASSIMP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_ASSIMP,GstAssimpClass))
#define GST_IS_ASSIMP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_ASSIMP))
#define GST_ASSIMP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_ASSIMP,GstAssimpClass))
typedef struct _GstAssimp GstAssimp;
typedef struct _GstAssimpClass GstAssimpClass;

struct Vec3F
{
  float x, y, z;
};

struct Vec2F
{
  float x, y;
};

struct GstAIVertex
{
  float px, py, pz;
  float nx, ny, nz;
  struct Vec2F t[AI_MAX_NUMBER_OF_TEXTURECOORDS];
};

struct GstAITexture
{
  enum aiTextureType        type;

  float                     multiplier;
  enum aiTextureOp          operation;
  enum aiTextureFlags       flags;
  enum aiTextureMapMode     wrapping[2];    /* u, v */
  enum aiTextureMapping     mapping;        /* u, v coordinates or generated coordinates */
  guint                     uv_index;       /* UV channel for @mapping = aiTextureMapping_UV */
  struct Vec3F              axis;           /* vector for generating with @mapping */

  gboolean                  new_sample;
  GstSample                *sample;
};

struct GstAITextureList
{
  guint n_textures;
  struct GstAITexture *textures;
};

struct GstAIMaterialColorComponent
{
  struct Vec3F color;
  struct GstAITextureList textures;
};

typedef enum
{
  MATERIAL_NONE = 0,
  MATERIAL_TWO_SIDED = (1 << 0),
  MATERIAL_WIREFRAME = (1 << 1),
} GstAIMaterialFlags;

struct GstAIMaterial
{
  const struct aiMaterial *mat;

  Gst3DMaterialElement elements;

  enum aiShadingMode shading_model;
  GstAIMaterialFlags flags;

  struct GstAIMaterialColorComponent diffuse;
  struct GstAIMaterialColorComponent specular;
  struct GstAIMaterialColorComponent ambient;
  struct GstAIMaterialColorComponent emissive;
  struct GstAITextureList height_maps;
  struct GstAITextureList normal_maps;
  struct GstAITextureList displacements;
  struct GstAITextureList light_maps;
  struct GstAITextureList reflections;
  float shininess;
  float shininess_strength;
  struct GstAITextureList shininess_textures;
  float opacity;
  struct GstAITextureList opacity_textures;

  struct Vec3F transparent_color;
  float refractive_index;
  float reflectivity;
};

struct GstAIMeshNode
{
  GstMemory        *vertex_buffer;
  GstMemory        *index_buffer;

  guint             num_indices;
  guint             num_vertices;
  unsigned int      material_index;
};

typedef struct _GstAssimpSrcPad GstAssimpSrcPad;
typedef struct _GstAssimpSrcPadClass GstAssimpSrcPadClass;
typedef struct _GstAssimpSrcPadPrivate GstAssimpSrcPadPrivate;

struct _GstAssimpSrcPad
{
  GstPad                parent;

  struct GstAIMaterial *material;

  gboolean              flush_seeking;
  gboolean              send_stream_start;
  GstCaps              *srccaps;
  gboolean              send_segment;
  GstSegment            segment;
  gsize                 seqnum;
  gboolean              send_tags;
  GstTagList           *tags;
};

struct _GstAssimpSrcPadClass
{
  GstPadClass           parent_class;
};

struct _GstAssimp
{
  GstBin                parent;

  GstPad               *sinkpad;
  GstPad               *srcpad;

  /* whether we're in push mode or not */
  gboolean              streaming;

  /* offset for pull mode */
  guint64               offset;

  /* hold all collected buffers */
  GstAdapter           *adapter;

  /* for getting the textures later */
  gchar                *obj_uri;
  gchar                *obj_base_uri;

  GstSegment            segment;
  Gst3DVertexInfo       out_vertex_info;
  Gst3DMaterialInfo     out_material_info;
  GstBuffer            *out_buffer;

  GstSample            *null_sample;

  struct aiLogStream    log_stream;
  const struct aiScene *scene;
  GArray               *meshes;
  GArray               *materials;
};

struct _GstAssimpClass
{
  GstBinClass element_class;
};

G_END_DECLS

#endif /* _GST_ASSIMP_H_ */
