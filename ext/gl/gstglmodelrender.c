/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#include <graphene.h>

#include "gstglmodelrender.h"

#define GST_CAT_DEFAULT gst_gl_model_render_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define MAX_LIGHTS 3
#define MAX_TEXTURES 11
#define MAX_TEXTURES_PER_STACK 4
#define MAX_UV 4

#define OP_ADD 1

#define SHADING_NONE 0
#define SHADING_PHONG 1
#define SHADING_GOURAD 2

struct Vec2
{
  float x, y;
};

struct Vec3
{
  float x, y, z;
};

struct Vec4
{
  float x, y, z, w;
};

struct Light
{
  struct Vec4 position;
  struct Vec3 color;
  float attuention;
  float ambient_coefficient;
  float cone_angle;
  struct Vec3 cone_direction;
};

struct MaterialItem
{
  struct Vec4 base_color;
  int n_textures;
  int texture_unit;
  int uv_index;
  float blend;
  int op;
};

struct Material
{
  struct MaterialItem diffuse;
};

struct Model
{
  struct Material material;
  guint position_loc;
  guint color_loc;
  guint vao;
  guint vbo;
  guint vbo_indices;
  guint uv_loc[MAX_UV];
};

typedef struct _GstGLModelRenderBin GstGLModelRenderBin;
typedef struct _GstGLModelRenderBinClass GstGLModelRenderBinClass;

#define GST_GL_MODEL_RENDER_BIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_MODEL_RENDER_BIN,GstGLModelRenderBin))
#define GST_IS_GL_MODEL_RENDER_BIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_MODEL_RENDER_BIN))
#define GST_GL_MODEL_RENDER_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_MODEL_RENDER_BIN,GstGLModelRenderBinClass))
#define GST_IS_GL_MODEL_RENDER_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_MODEL_RENDER_BIN))
#define GST_GL_MODEL_RENDER_BIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_MODEL_RENDER_BIN,GstGLModelRenderBinClass))

struct _GstGLModelRenderBin
{
  GstBin parent;

  GstGLModelRender *model;
  GstElement *gl_conversion;
  GstElement *src;
  GstElement *sink;
  gboolean pulled_preroll;
};

struct _GstGLModelRenderBinClass
{
  GstBinClass parent_class;
};

struct _GstGLModelRenderPrivate
{
  GstGLModelRenderBin *parent;

  graphene_matrix_t model_matrix;
  graphene_matrix_t mvp;
  graphene_vec3_t center_to_eye;

  struct Model model;
  struct Light lights[MAX_LIGHTS];
};

G_DEFINE_TYPE (GstGLModelRenderBin, gst_gl_model_render_bin, GST_TYPE_BIN);

/* *INDENT-OFF* */
static GstStaticPadTemplate src_pad_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(memory:GLMemory), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ","
        "texture-target = (string) 2D"));

#define MODEL_CAPS \
    "application/3d-model, "                                                \
    "primitive-mode = (string) triangles, "                                \
    "index-format = (string) U16LE "

#define POSITION_ATTRIBUTE \
    "attribute, "                                                           \
    "format = (string) F32LE, "                                             \
    "type = (string) position , "                                           \
    "channels = (int) 3 "
#define COLOR_ATTRIBUTE \
    "attribute, "                                                           \
    "format = (string) F32LE, "                                             \
    "type = (string) color , "                                             \
    "channels = (int) 4 "
#define TEXCOORD_ATTRIBUTE \
    "attribute, "                                                           \
    "format = (string) F32LE, "                                             \
    "type = (string) texture , "                                            \
    "channels = (int) 2 "
#define NORMAL_ATTRIBUTE \
    "attribute, "                                                           \
    "format = (string) F32LE, "                                             \
    "type = (string) normal , "                                             \
    "channels = (int) 3 "

static GstCaps *
_create_sink_pad_template_caps (void)
{
  GstCaps *ret = gst_caps_new_empty ();
  GstStructure *s = gst_structure_from_string (MODEL_CAPS, NULL);
  GstStructure *attrib;
  GValue attrib_val = G_VALUE_INIT;
  GValue attributes = G_VALUE_INIT;

  g_value_init (&attributes, GST_TYPE_ARRAY);
  g_value_init (&attrib_val, GST_TYPE_STRUCTURE);

  attrib = gst_structure_from_string (POSITION_ATTRIBUTE, NULL);
  gst_value_set_structure (&attrib_val, attrib);
  gst_structure_free (attrib);
  gst_value_array_append_value (&attributes, &attrib_val);

  attrib = gst_structure_from_string (COLOR_ATTRIBUTE, NULL);
  gst_value_set_structure (&attrib_val, attrib);
  gst_structure_free (attrib);
  gst_value_array_append_value (&attributes, &attrib_val);

  attrib = gst_structure_from_string (NORMAL_ATTRIBUTE, NULL);
  gst_value_set_structure (&attrib_val, attrib);
  gst_structure_free (attrib);
  gst_value_array_append_value (&attributes, &attrib_val);

  attrib = gst_structure_from_string (TEXCOORD_ATTRIBUTE, NULL);
  gst_value_set_structure (&attrib_val, attrib);
  gst_structure_free (attrib);
  gst_value_array_append_value (&attributes, &attrib_val);

  gst_structure_set_value (s, "attributes", &attributes);

  g_value_unset (&attributes);
  g_value_unset (&attrib_val);

  gst_caps_append_structure (ret, s);

  return ret;
}

static void
gst_gl_model_render_bin_init (GstGLModelRenderBin * model)
{
  GstPad *ghost, *pad;

  model->model = g_object_new (GST_TYPE_GL_MODEL_RENDER, NULL);
  model->model->priv->parent = model;
  gst_bin_add (GST_BIN (model), GST_ELEMENT (model->model));

  pad = gst_element_get_static_pad (GST_ELEMENT (model->model), "src");
  ghost = gst_ghost_pad_new ("src", pad);
  gst_element_add_pad (GST_ELEMENT (model), ghost);

  pad = gst_element_get_static_pad (GST_ELEMENT (model->model), "sink");
  ghost = gst_ghost_pad_new ("sink", pad);
  gst_element_add_pad (GST_ELEMENT (model), ghost);
}

static void
gst_gl_model_render_bin_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_gl_model_render_bin_parent_class)->finalize (object);
}

static void
gst_gl_model_render_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_model_render_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_gl_model_render_bin_change_state (GstElement * element,
    GstStateChange transition)
{
  GstGLModelRenderBin *model = GST_GL_MODEL_RENDER_BIN (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (model, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GError *error = NULL;
      model->gl_conversion = gst_parse_bin_from_description ("appsrc name=src block=true ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGBA,texture-target=2D ! appsink sync=false async=false name=sink", FALSE, &error);
      if (!model->gl_conversion) {
        GST_ELEMENT_ERROR (model, CORE, MISSING_PLUGIN,
            ("%s", "Missing an element"), ("%s", error->message));
        return GST_STATE_CHANGE_FAILURE;
      }
      gst_bin_add (GST_BIN (model), GST_ELEMENT (model->gl_conversion));

      model->src = gst_bin_get_by_name (GST_BIN (model->gl_conversion), "src");
      model->sink = gst_bin_get_by_name (GST_BIN (model->gl_conversion), "sink");
      model->pulled_preroll = FALSE;
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_gl_model_render_bin_parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_bin_remove (GST_BIN (model), GST_ELEMENT (model->gl_conversion));
      gst_object_unref (model->src);
      gst_object_unref (model->sink);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_gl_model_render_bin_class_init (GstGLModelRenderBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstCaps *sink_caps_templ = _create_sink_pad_template_caps ();

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_gl_model_render_bin_finalize;
  gobject_class->set_property = gst_gl_model_render_bin_set_property;
  gobject_class->get_property = gst_gl_model_render_bin_get_property;

  element_class->change_state = gst_gl_model_render_bin_change_state;

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          sink_caps_templ));
  gst_element_class_add_static_pad_template (element_class, &src_pad_template);

  gst_element_class_set_metadata (element_class, "OpenGL model render",
      "Filter/Effect/Video/3D/Render", "OpenGL model render",
      "Matthew Waters <matthew@centricular.com>");
}

#define gst_gl_model_render_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLModelRender, gst_gl_model_render,
    GST_TYPE_GL_BASE_FILTER, GST_DEBUG_CATEGORY_INIT (gst_gl_model_render_debug,
        "glmodelrender", 0, "glmodelrender element"););

#define GST_GL_MODEL_RENDER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE((o), GST_TYPE_GL_MODEL_RENDER, GstGLModelRenderPrivate))

/* Properties */
enum
{
  PROP_0,
  PROP_CONTEXT
};

static void gst_gl_model_render_finalize (GObject * object);
static void gst_gl_model_render_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_model_render_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_gl_model_render_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_gl_model_render_start (GstBaseTransform * btrans);
static gboolean gst_gl_model_render_stop (GstBaseTransform * btrans);
static GstCaps *gst_gl_model_render_fixate_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static GstCaps *gst_gl_model_render_transform_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_gl_model_render_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_gl_model_render_decide_allocation (GstBaseTransform *
    btrans, GstQuery * query);
static GstFlowReturn gst_gl_model_render_transform (GstBaseTransform * btrans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_gl_model_render_src_event (GstBaseTransform * btrans,
    GstEvent * event);

static gboolean gst_gl_model_render_gl_start (GstGLBaseFilter * base_filter);
static void gst_gl_model_render_gl_stop (GstGLBaseFilter * base_filter);
static void _update_mvp (GstGLModelRender * model);

/* vertex source */
static const gchar *cube_v_src =
    "#version 150\n"
    "in vec4 a_position;\n"
    "in vec4 a_color;\n"
    "in vec2 uv[" G_STRINGIFY (MAX_UV) "];\n"
    "out vec2 out_uv[" G_STRINGIFY (MAX_UV) "];\n"
    "out vec4 out_color;\n"
    "uniform mat4 mvp;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = mvp * a_position;\n"
    "  for (int i = 0; i < " G_STRINGIFY (MAX_UV) "; i++) {\n"
    "    out_uv[i] = uv[i];\n"
    "  }\n"
    "  out_color = a_color;\n"
    "}\n";

/* fragment source */
static const gchar *cube_f_src =
    "#version 150\n"
    "in vec2 out_uv[" G_STRINGIFY (MAX_UV) "];\n"
    "in vec4 out_color;\n"
    "uniform int uv_index[" G_STRINGIFY (MAX_TEXTURES) "];\n"
    "uniform sampler2D tex[" G_STRINGIFY (MAX_TEXTURES) "];\n"
    "vec4 tex_color[" G_STRINGIFY (MAX_TEXTURES) "];\n"
    "struct MaterialItem {\n"
    "  vec3 base_color;\n"
    "  int n_textures;\n"
    "  int tex_idx[" G_STRINGIFY (MAX_TEXTURES_PER_STACK) "];\n"
    "  float blend[" G_STRINGIFY (MAX_TEXTURES_PER_STACK) "];\n"
    "  int op[" G_STRINGIFY (MAX_TEXTURES_PER_STACK) "];\n"
    "};\n"
    "struct Material {\n"
    "  MaterialItem diffuse;\n"
    "};\n"
    "struct Light {\n"
    "  vec4 position;\n"
    "  vec3 color;\n"
    "  float attuentation;\n"
    "  float ambient_coefficient;\n"
    "  float cone_angle;\n"
    "  vec3 cone_direction;\n"
    "};\n"
    "uniform Material material;\n"
    "uniform int n_lights;\n"
    "uniform Light lights[" G_STRINGIFY(MAX_LIGHTS) "];\n"
    "uniform int shading_model;\n"
    "vec3 evaluate_texture_stack(MaterialItem item) {\n"
    "  vec3 color;\n"
    "  if (true) {\n"
    "    color = item.base_color;\n"
    "  } else {\n"
    "    color = out_color.rgb;\n"
    "  }\n"
    "  for (int i = 0; i < item.n_textures; i++) {\n"
    "    vec3 c = tex_color[item.tex_idx[i]].rgb;\n"
    "    c *= item.blend[i];\n"
    "    int op = item.op[i];\n"
    "    if (op == " G_STRINGIFY (OP_ADD) ") {\n"
    "      color += c;\n"
    "    }\n"
    "  }\n"
    "  return color;\n"
    "}\n"
    "vec3 calculate_diffuse() {\n"
    "  if (shading_model == " G_STRINGIFY(SHADING_NONE) ")\n"
    "    return evaluate_texture_stack(material.diffuse);\n"
    "  vec3 intensity = vec3(0.0);\n"
    "  for (int i = 0; i < n_lights; i++) {\n"
    "    float fac = 1.0;\n"
    "    if (shading_model == " G_STRINGIFY (SHADING_PHONG) ")\n"
    "      return vec3(0.5);\n" /* FIXME */
    "  }\n"
    "  return vec3(0.5);\n" /* FIXME */
    "}\n"
    "vec3 calculate_specular() {\n"
    "  return vec3(0.0);\n"
    "}\n"
    "vec3 calculate_ambient() {\n"
    "  if (shading_model == " G_STRINGIFY(SHADING_NONE) ")\n"
    "    return vec3 (0.0);\n"
    "  return vec3(0.0);\n"
    "}\n"
    "vec4 pimp_the_pixel(vec4 prev) {\n"
    "  vec3 diff = calculate_diffuse ();\n"
    "  vec3 spec = calculate_specular ();\n"
    "  vec3 amb = calculate_ambient ();\n"
    "  vec3 color = diff + spec + amb;\n"
    "  float opacity = 1.0;\n"
    "  return vec4(color, opacity);\n"
    "}\n"
    "void main()\n"
    "{\n"
    "  %s\n"
    "  gl_FragColor = pimp_the_pixel(vec4 (vec3(0.0), 1.0));\n"
    "}\n";
/* *INDENT-ON* */

static void
gst_gl_model_render_class_init (GstGLModelRenderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *base_transform_class;
  GstGLBaseFilterClass *base_filter_class;
  GstCaps *sink_caps_templ = _create_sink_pad_template_caps ();

  g_type_class_add_private (klass, sizeof (GstGLModelRenderPrivate));

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  base_transform_class = (GstBaseTransformClass *) klass;
  base_filter_class = (GstGLBaseFilterClass *) klass;

  gobject_class->finalize = gst_gl_model_render_finalize;
  gobject_class->set_property = gst_gl_model_render_set_property;
  gobject_class->get_property = gst_gl_model_render_get_property;

  element_class->change_state = gst_gl_model_render_change_state;

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          sink_caps_templ));
  gst_element_class_add_static_pad_template (element_class, &src_pad_template);

  gst_element_class_set_metadata (element_class, "OpenGL model render",
      "Filter/Effect/Video/3D/Render", "OpenGL model render",
      "Matthew Waters <matthew@centricular.com>");

  base_transform_class->start = gst_gl_model_render_start;
  base_transform_class->stop = gst_gl_model_render_stop;
  base_transform_class->transform_caps = gst_gl_model_render_transform_caps;
  base_transform_class->fixate_caps = gst_gl_model_render_fixate_caps;
  base_transform_class->set_caps = gst_gl_model_render_set_caps;
  base_transform_class->decide_allocation =
      gst_gl_model_render_decide_allocation;
  base_transform_class->transform = gst_gl_model_render_transform;
  base_transform_class->src_event = gst_gl_model_render_src_event;

  base_filter_class->supported_gl_api = GST_GL_API_ANY;
  base_filter_class->gl_start = gst_gl_model_render_gl_start;
  base_filter_class->gl_stop = gst_gl_model_render_gl_stop;
}

static void
gst_gl_model_render_init (GstGLModelRender * model)
{
  model->priv = GST_GL_MODEL_RENDER_GET_PRIVATE (model);

//  model->priv->lights[0]

  model->camera = gst_3d_camera_new ();
  model->camera->ortho = TRUE;
  model->camera->znear = 1.;
  model->camera->zfar = -1.;
  graphene_vec3_init_from_vec3 (&model->priv->center_to_eye,
      &model->camera->eye);
  graphene_matrix_init_scale (&model->priv->model_matrix, 0.75f, 0.75f, 0.75f);
  _update_mvp (model);
}

static void
gst_gl_model_render_finalize (GObject * object)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (object);

  if (model->camera)
    gst_object_unref (model->camera);
  model->camera = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_model_render_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_model_render_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_model_render_start (GstBaseTransform * btrans)
{
  return GST_BASE_TRANSFORM_CLASS (parent_class)->start (btrans);
}

static gboolean
gst_gl_model_render_stop (GstBaseTransform * btrans)
{
  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (btrans);
}

static gboolean
gst_gl_model_render_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (base_filter);
  guint out_width = GST_VIDEO_INFO_WIDTH (&model->vinfo);
  guint out_height = GST_VIDEO_INFO_HEIGHT (&model->vinfo);

  if (!(model->fbo =
          gst_gl_framebuffer_new_with_default_depth (base_filter->context,
              out_width, out_height)))
    return FALSE;

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter);
}

static void
gst_gl_model_render_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (base_filter);

  if (model->fbo)
    gst_object_unref (model->fbo);
  model->fbo = NULL;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static gboolean
gst_gl_model_render_decide_allocation (GstBaseTransform * btrans,
    GstQuery * query)
{
  GstGLContext *context;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps)
    return FALSE;

  /* get gl context */
  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (btrans,
          query))
    return FALSE;

  context = GST_GL_BASE_FILTER (btrans)->context;

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, caps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool || !GST_IS_GL_BUFFER_POOL (pool)) {
    if (pool)
      gst_object_unref (pool);
    pool = gst_gl_buffer_pool_new (context);
  }

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (gst_query_find_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL))
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);

  gst_buffer_pool_set_config (pool, config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;
}

static GstCaps *
gst_gl_model_render_transform_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  if (direction == GST_PAD_SRC) {
    tmp = gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SINK_PAD (btrans));
  } else if (direction == GST_PAD_SINK) {
    tmp = gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SRC_PAD (btrans));
  } else {
    g_assert_not_reached ();
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (btrans, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static GstCaps *
gst_gl_model_render_fixate_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  /* FIXME: proper fixation */
  GstStructure *other_s /*, *s */ ;

  othercaps = gst_caps_make_writable (othercaps);
  other_s = gst_caps_get_structure (othercaps, 0);
//  s = gst_caps_get_structure (caps, 0);

  if (direction == GST_PAD_SINK) {
    gst_structure_fixate_field_string (other_s, "format", "RGBA");
    gst_structure_fixate_field_nearest_int (other_s, "width", 320);
    gst_structure_fixate_field_nearest_int (other_s, "height", 240);
    if (gst_structure_has_field (other_s, "pixel-aspect-ratio"))
      gst_structure_fixate_field_nearest_fraction (other_s,
          "pixel-aspect-ratio", 1, 1);
  } else {
    /* FIXME */
  }

  othercaps = gst_caps_fixate (othercaps);

  return othercaps;
}

static gboolean
gst_gl_model_render_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (btrans);

  if (!gst_video_info_from_caps (&model->vinfo, outcaps))
    return FALSE;

  return TRUE;
}

static GstStateChangeReturn
gst_gl_model_render_change_state (GstElement * element,
    GstStateChange transition)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (model, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
_update_mvp (GstGLModelRender * model)
{
  graphene_vec3_t translation;
  gfloat fast_modifier = 1.0, distance;
  gfloat xtranslation = 0.0f, ytranslation = 0.0f, ztranslation = 0.0f;
  GList *keys, *l;

  keys = gst_3d_camera_get_pressed_keys (model->camera);

  for (l = keys; l != NULL; l = l->next)
    if (g_strcmp0 (l->data, "Shift_L") == 0)
      fast_modifier = 5.0;

  distance = 0.01 * fast_modifier;

  for (l = keys; l != NULL; l = l->next) {
    g_print ("%s\n", (gchar *) l->data);
    if (g_strcmp0 (l->data, "w") == 0) {
      ztranslation = -distance;
      continue;
    } else if (g_strcmp0 (l->data, "s") == 0) {
      ztranslation = distance;
      continue;
    }

    if (g_strcmp0 (l->data, "a") == 0) {
      xtranslation = -distance;
      continue;
    } else if (g_strcmp0 (l->data, "d") == 0) {
      xtranslation = distance;
      continue;
    }

    if (g_strcmp0 (l->data, "space") == 0) {
      ytranslation = -distance;
      continue;
    } else if (g_strcmp0 (l->data, "Control_L") == 0) {
      ytranslation = distance;
      continue;
    }
  }

  graphene_vec3_init (&translation, xtranslation, ytranslation, ztranslation);
  graphene_vec3_add (&model->camera->center, &translation,
      &model->camera->center);
  GST_ERROR ("translation %f, %f, %f", graphene_vec3_get_x (&translation),
      graphene_vec3_get_y (&translation), graphene_vec3_get_z (&translation));
  GST_ERROR ("center %f, %f, %f", graphene_vec3_get_x (&model->camera->center),
      graphene_vec3_get_y (&model->camera->center),
      graphene_vec3_get_z (&model->camera->center));

  graphene_vec3_add (&model->camera->center, &model->priv->center_to_eye,
      &model->camera->eye);
  gst_3d_camera_update_view (model->camera);

  graphene_matrix_multiply (&model->priv->model_matrix,
      &model->camera->view_projection, &model->priv->mvp);
  graphene_matrix_print (&model->priv->mvp);

  g_list_free_full (l, (GDestroyNotify) g_free);
}

static void
_on_navigation (GstGLModelRender * model, GstEvent * event)
{
  GstNavigationEventType event_type = gst_navigation_event_get_type (event);
//  GstStructure *structure = (GstStructure *) gst_event_get_structure (event);

  switch (event_type) {
    case GST_NAVIGATION_EVENT_MOUSE_MOVE:{
      /* handle the mouse motion for zooming and rotating the view */
/*      gdouble x, y;
      gdouble dx, dy;

      gst_structure_get_double (structure, "pointer_x", &x);
      gst_structure_get_double (structure, "pointer_y", &y);

      dx = x - model->camera->cursor_last_x;
      dy = y - model->camera->cursor_last_y;

      if (model->camera->pressed_mouse_button == 1) {
        GST_DEBUG ("Rotating [%fx%f]", dx, dy);
      }*/
      break;
    }
    default:
      break;
  }

  gst_3d_camera_navigation_event (model->camera, event);
  _update_mvp (model);
}

static gboolean
gst_gl_model_render_src_event (GstBaseTransform * btrans, GstEvent * event)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (btrans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
      event =
          GST_EVENT (gst_mini_object_make_writable (GST_MINI_OBJECT (event)));
      _on_navigation (model, event);
      break;
    default:
      break;
  }
  return GST_BASE_TRANSFORM_CLASS (parent_class)->src_event (btrans, event);
}

static gboolean
_create_shader (GstGLModelRender * model, int gl_texture_count)
{
  GstGLContext *context = GST_GL_BASE_FILTER (model)->context;
  GstGLSLStage *vert, *frag;
  GError *error = NULL;
  gchar *frag_str;
  int i;

  if (model->shader)
    return TRUE;

  vert = gst_glsl_stage_new_with_string (context, GL_VERTEX_SHADER,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, cube_v_src);
  if (!gst_glsl_stage_compile (vert, &error)) {
    GST_ERROR_OBJECT (vert, "%s", error->message);
    g_error_free (error);
    gst_object_unref (vert);
    return FALSE;
  }

  {
    int i;
    gchar *sampling;

    sampling = g_strdup ("");

    for (i = 0; i < gl_texture_count; i++) {
      gchar *tmp =
          g_strdup_printf
          ("%s\n  tex_color[%d] = texture2D(tex[%d], out_uv[uv_index[%d]]);",
          sampling, i, i, i);
      g_free (sampling);
      sampling = tmp;
    }

    frag_str = g_strdup_printf (cube_f_src, sampling);
  }

  frag = gst_glsl_stage_new_with_string (context, GL_FRAGMENT_SHADER,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, frag_str);
  if (!gst_glsl_stage_compile (frag, &error)) {
    GST_ERROR_OBJECT (frag, "%s", error->message);
    g_error_free (error);
    gst_object_unref (vert);
    gst_object_unref (frag);
    g_free (frag_str);
    return FALSE;
  }
  g_free (frag_str);

  model->shader = gst_gl_shader_new (context);
  if (!gst_gl_shader_attach (model->shader, vert)) {
    gst_object_unref (vert);
    gst_object_unref (frag);
    gst_object_unref (model->shader);
    model->shader = NULL;
    return FALSE;
  }
  if (!gst_gl_shader_attach (model->shader, frag)) {
    gst_object_unref (frag);
    gst_object_unref (model->shader);
    model->shader = NULL;
    return FALSE;
  }

  if (!gst_gl_shader_link (model->shader, &error)) {
    GST_ERROR_OBJECT (model, "%s", error->message);
    g_error_free (error);
    error = NULL;
    gst_gl_context_clear_shader (context);
    gst_object_unref (model->shader);
    model->shader = NULL;
    return FALSE;
  }

  gst_gl_shader_use (model->shader);
  model->priv->model.position_loc =
      gst_gl_shader_get_attribute_location (model->shader, "a_position");
  model->priv->model.color_loc =
      gst_gl_shader_get_attribute_location (model->shader, "a_color");

  for (i = 0; i < MAX_UV; i++) {
    gchar *name;

    name = g_strdup_printf ("uv[%u]", i);
    model->priv->model.uv_loc[i] =
        gst_gl_shader_get_attribute_location (model->shader, name);
    g_free (name);
  }
  gst_gl_context_clear_shader (context);

  return TRUE;
}

static guint
_material_texture_count (Gst3DMaterialMeta * mmeta)
{
  Gst3DMaterialStack *stack;
  guint n;

  stack =
      gst_3d_material_meta_get_stack (mmeta, GST_3D_MATERIAL_ELEMENT_DIFFUSE);
  n = gst_3d_material_stack_get_n_textures (stack);

  /* FIXME: other stacks */
  return n;
}

static guint
_3d_vertex_format_to_gl_enum (Gst3DVertexFormat format)
{
  switch (format) {
    case GST_3D_VERTEX_FORMAT_F32:
      return GL_FLOAT;
    case GST_3D_VERTEX_FORMAT_U16:
      return GL_UNSIGNED_SHORT;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static void
_bind_buffer (GstGLModelRender * model, Gst3DVertexInfo * vinfo)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (model)->context->gl_vtable;
  Gst3DVertexAttribute *attrib;
  int idx, i, n;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, model->priv->model.vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, model->priv->model.vbo);

  /* Load the vertex position */
  /* FIXME: hardcoded format */
  idx = gst_3d_vertex_info_find_attribute_index (vinfo,
      GST_3D_VERTEX_TYPE_POSITION, NULL);
  attrib = GST_3D_VERTEX_INFO_ATTRIBUTE (vinfo, idx);
  gl->VertexAttribPointer (model->priv->model.position_loc, attrib->channels,
      GL_FLOAT, GL_FALSE, vinfo->strides[idx], (void *) vinfo->offsets[idx]);
  gl->EnableVertexAttribArray (model->priv->model.position_loc);

  idx = gst_3d_vertex_info_find_attribute_index (vinfo,
      GST_3D_VERTEX_TYPE_COLOR, NULL);
  attrib = GST_3D_VERTEX_INFO_ATTRIBUTE (vinfo, idx);
  gl->VertexAttribPointer (model->priv->model.color_loc, attrib->channels,
      GL_FLOAT, GL_FALSE, vinfo->strides[idx], (void *) vinfo->offsets[idx]);
  gl->EnableVertexAttribArray (model->priv->model.color_loc);

  n = gst_3d_vertex_info_get_n_attributes_of_type (vinfo,
      GST_3D_VERTEX_TYPE_TEXTURE);
  for (i = 0; i < n; i++) {
    idx = gst_3d_vertex_info_find_attribute_nth_index (vinfo,
        GST_3D_VERTEX_TYPE_TEXTURE, i);

    attrib = GST_3D_VERTEX_INFO_ATTRIBUTE (vinfo, idx);

    if (model->priv->model.uv_loc[i] != -1) {
      gl->VertexAttribPointer (model->priv->model.uv_loc[i],
          attrib->channels,
          _3d_vertex_format_to_gl_enum (attrib->finfo->format), GL_FALSE,
          vinfo->strides[idx], (void *) vinfo->offsets[idx]);
      gl->EnableVertexAttribArray (model->priv->model.uv_loc[i]);
    }
  }
}

static void
_unbind_buffer (GstGLModelRender * model)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (model)->context->gl_vtable;
  int i, n;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (model->priv->model.position_loc);
  n = MAX_UV;
  for (i = 0; i < n; i++) {
    if (model->priv->model.uv_loc[i] != -1)
      gl->DisableVertexAttribArray (model->priv->model.uv_loc[i]);
  }
}

static gboolean
_update_vertex_buffer (GstGLModelRender * model, Gst3DModelMeta * meta,
    guint * n_indices, Gst3DVertexInfo ** vinfo)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (model)->context->gl_vtable;
  Gst3DVertexMeta *vmeta;
  Gst3DMaterialMeta *mmeta;
  Gst3DVertexMapInfo map_info;
  int gl_texture_count;

  mmeta = gst_buffer_get_3d_material_meta (meta->buffer, meta->material_id);

  gl_texture_count = _material_texture_count (mmeta);

  if (!_create_shader (model, gl_texture_count))
    return FALSE;

  vmeta = gst_buffer_get_3d_vertex_meta (meta->buffer, meta->vertex_id);
  if (!meta) {
    GST_ERROR_OBJECT (model, "input buffer is missing Gst3DVertexMeta");
    return FALSE;
  }
  *vinfo = &vmeta->info;

  if (!gst_3d_vertex_meta_map (vmeta, &map_info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (model, "failed to map input buffer");
    return FALSE;
  }

  if (gl->GenVertexArrays)
    gl->BindVertexArray (model->priv->model.vao);
  _bind_buffer (model, &vmeta->info);

  gl->BufferData (GL_ARRAY_BUFFER, map_info.vertex_map.size,
      map_info.vertex_data, GL_STATIC_DRAW);

  gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, map_info.index_map.size,
      map_info.index_data, GL_STATIC_DRAW);
  *n_indices = vmeta->n_indices;

  return TRUE;
}

static GQuark
_gl_memory_quark (void)
{
  static GQuark quark = 0;

  if (!quark)
    quark = g_quark_from_static_string ("GstGLModelRenderGLMemory");

  return quark;
}

static GstMemory *
_get_cached_gl_memory (GstBuffer * buf)
{
  return gst_mini_object_get_qdata (GST_MINI_OBJECT (buf), _gl_memory_quark ());
}

static void
_set_cached_gl_memory (GstBuffer * buf, GstMemory * gl_mem)
{
  return gst_mini_object_set_qdata (GST_MINI_OBJECT (buf),
      _gl_memory_quark (), gl_mem, (GDestroyNotify) gst_memory_unref);
}

static GstGLMemory *
_create_upload_texture (GstGLModelRender * model, Gst3DMaterialTexture * tex)
{
  GstFlowReturn flow_ret;
  GstSample *gl_sample;
  GstBuffer *buf, *gl_buf;
  GstMemory *ret;

  buf = gst_sample_get_buffer (tex->sample);
  ret = _get_cached_gl_memory (buf);
  if (ret)
    return (GstGLMemory *) gst_memory_ref (ret);

  GST_ERROR ("%p", gst_sample_get_buffer (tex->sample));
  g_signal_emit_by_name (model->priv->parent->src, "push-sample", tex->sample,
      &flow_ret);
  GST_ERROR ("%p", gst_sample_get_buffer (tex->sample));
  if (flow_ret != GST_FLOW_OK)
    return NULL;

  GST_ERROR ("%p", gst_sample_get_buffer (tex->sample));
  if (model->priv->parent->pulled_preroll)
    g_signal_emit_by_name (model->priv->parent->sink, "pull-sample",
        &gl_sample);
  else
    g_signal_emit_by_name (model->priv->parent->sink, "pull-preroll",
        &gl_sample);
  GST_ERROR ("%p", gst_sample_get_buffer (tex->sample));
  if (!gl_sample)
    return NULL;

  gl_buf = gst_sample_get_buffer (gl_sample);
  if (!gl_buf) {
    gst_sample_unref (gl_sample);
    return NULL;
  }

  ret = gst_buffer_peek_memory (gl_buf, 0);
  if (!gst_is_gl_memory (ret)) {
    gst_sample_unref (gl_sample);
    return NULL;
  }

  _set_cached_gl_memory (buf, gst_memory_ref (ret));
  gst_sample_unref (gl_sample);

  return (GstGLMemory *) gst_memory_ref (ret);
}

static gboolean
_update_materials (GstGLModelRender * model, Gst3DModelMeta * meta)
{
  GstGLContext *context = GST_GL_BASE_FILTER (model)->context;
  const GstGLFuncs *gl = context->gl_vtable;
  Gst3DMaterialMeta *mmeta;
  Gst3DMaterialStack *diffuse;
  int i, n;
  int gl_texture_count, tex_i = 0;
  gchar *name;
  const gchar *base_name;

  mmeta = gst_buffer_get_3d_material_meta (meta->buffer, meta->material_id);
  diffuse =
      gst_3d_material_meta_get_stack (mmeta, GST_3D_MATERIAL_ELEMENT_DIFFUSE);
  n = gst_3d_material_stack_get_n_textures (diffuse);

  base_name = "material.diffuse";

  gl_texture_count = _material_texture_count (mmeta);

  if (!_create_shader (model, gl_texture_count))
    return FALSE;
  gst_gl_shader_use (model->shader);

  name = g_strdup_printf ("%s.%s", base_name, "base_color");
  gst_gl_shader_set_uniform_3fv (model->shader, name, 1, &diffuse->x);
  g_free (name);

  name = g_strdup_printf ("%s.n_textures", base_name);
  gst_gl_shader_set_uniform_1i (model->shader, name, n);
  g_free (name);

  for (i = 0; i < n; i++) {
    Gst3DMaterialTexture *tex;
    GstBuffer *buf;
    GstGLMemory *ret;

    tex = gst_3d_material_stack_get_texture (diffuse, i);
    if (!tex)
      continue;
    buf = gst_sample_get_buffer (tex->sample);
    ret = (GstGLMemory *) _get_cached_gl_memory (buf);
    g_assert (ret);             /* no funny business now */

    gl->ActiveTexture (GL_TEXTURE0 + tex_i);
    gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (ret));

    name = g_strdup_printf ("tex[%u]", i);
    gst_gl_shader_set_uniform_1i (model->shader, name, tex_i);
    g_free (name);

    name = g_strdup_printf ("uv_index[%d]", i);
    gst_gl_shader_set_uniform_1i (model->shader, name, 0);      /* FIXME */
    g_free (name);

    name = g_strdup_printf ("%s.tex_idx[%u]", base_name, i);
    gst_gl_shader_set_uniform_1i (model->shader, name, tex_i);
    g_free (name);

    name = g_strdup_printf ("%s.%s[%d]", base_name, "blend", i);
    gst_gl_shader_set_uniform_1f (model->shader, name, 1.0);
    g_free (name);

    name = g_strdup_printf ("%s.%s[%d]", base_name, "op", i);
    gst_gl_shader_set_uniform_1i (model->shader, name, OP_ADD);
    g_free (name);

    tex_i++;
  }

  return TRUE;
}

static gboolean
_upload_textures (GstGLModelRender * model, Gst3DModelMeta * meta)
{
  Gst3DMaterialMeta *mmeta;
  Gst3DMaterialStack *diffuse;
  int i, n;

  mmeta = gst_buffer_get_3d_material_meta (meta->buffer, meta->material_id);
  diffuse =
      gst_3d_material_meta_get_stack (mmeta, GST_3D_MATERIAL_ELEMENT_DIFFUSE);
  n = gst_3d_material_stack_get_n_textures (diffuse);

  for (i = 0; i < n; i++) {
    Gst3DMaterialTexture *tex;
    GstBuffer *buf;
    GstGLMemory *ret;

    tex = gst_3d_material_stack_get_texture (diffuse, i);
    if (!tex)
      continue;
    buf = gst_sample_get_buffer (tex->sample);
    ret = (GstGLMemory *) _get_cached_gl_memory (buf);
    if (!ret) {
      _create_upload_texture (model, tex);
      ret = (GstGLMemory *) _get_cached_gl_memory (buf);
    }
    g_assert (ret);             /* no funny business now */
  }

  return TRUE;
}

#if 0
static void
_set_shader_light_uniforms (GstGLShader * shader, int i, struct Light *light)
{
  gchar *light_base_str = g_strdup_printf ("lights[%u]", i);
  gchar *name;

  name = g_strdup_printf ("%s.%s", light_base_str, "position");
  gst_gl_shader_set_uniform_4fv (shader, name, 1, &light->position.x);
  g_free (name);

  name = g_strdup_printf ("%s.%s", light_base_str, "color");
  gst_gl_shader_set_uniform_3fv (shader, name, 1, &light->color.x);
  g_free (name);

  name = g_strdup_printf ("%s.%s", light_base_str, "attuention");
  gst_gl_shader_set_uniform_1f (shader, name, light->attuention);
  g_free (name);

  name = g_strdup_printf ("%s.%s", light_base_str, "ambient_coefficient");
  gst_gl_shader_set_uniform_1f (shader, name, light->ambient_coefficient);
  g_free (name);

  name = g_strdup_printf ("%s.%s", light_base_str, "cone_angle");
  gst_gl_shader_set_uniform_1f (shader, name, light->cone_angle);
  g_free (name);

  name = g_strdup_printf ("%s.%s", light_base_str, "cone_direction");
  gst_gl_shader_set_uniform_3fv (shader, name, 1, &light->cone_direction.x);
  g_free (name);

  g_free (light_base_str);
}
#endif
static gboolean
gst_gl_model_render_draw (gpointer data)
{
  GstGLModelRender *model = data;
  GstGLContext *context = GST_GL_BASE_FILTER (model)->context;
  const GstGLFuncs *gl = context->gl_vtable;
  const GstMetaInfo *info = GST_3D_MODEL_META_INFO;
  gpointer state = NULL;
  GstMeta *meta;
  Gst3DVertexInfo *vinfo;
  guint n_indices;

  gl->Enable (GL_DEPTH_TEST);
  gl->ClearColor (1.0, 0.0, 0.0, 1.0);
  gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  while ((meta = gst_buffer_iterate_meta (model->in_buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DModelMeta *mmeta = (Gst3DModelMeta *) meta;
      float m[16];

      if (!model->priv->model.vbo) {
        if (gl->GenVertexArrays) {
          gl->GenVertexArrays (1, &model->priv->model.vao);
        }

        gl->GenBuffers (1, &model->priv->model.vbo);
        gl->GenBuffers (1, &model->priv->model.vbo_indices);
      }

      if (gl->GenVertexArrays)
        gl->BindVertexArray (model->priv->model.vao);

      /* XXX: terribly inefficient */
      if (!_update_vertex_buffer (model, mmeta, &n_indices, &vinfo))
        return FALSE;
      /* XXX: terribly inefficient */
      if (!_update_materials (model, mmeta))
        return FALSE;

      gst_gl_shader_use (model->shader);
      graphene_matrix_to_float (&model->priv->mvp, m);

      gst_gl_shader_set_uniform_matrix_4fv (model->shader, "mvp", 1, FALSE, m);

      /* FIXME: hardcoded index format */
      gl->DrawElements (GL_TRIANGLES, n_indices, GL_UNSIGNED_SHORT, 0);

      if (gl->GenVertexArrays)
        gl->BindVertexArray (0);
      _unbind_buffer (model);

      gst_gl_context_clear_shader (context);
      /* FIXME: inefficent */
      gst_object_unref (model->shader);
      model->shader = NULL;
    }
  }

  gl->Disable (GL_DEPTH_TEST);

  return TRUE;
}

static void
_process_model_gl (GstGLContext * context, GstGLModelRender * model)
{
  GstGLMemory *out_tex =
      (GstGLMemory *) gst_buffer_peek_memory (model->out_buffer, 0);

  gst_gl_framebuffer_draw_to_texture (model->fbo, out_tex,
      gst_gl_model_render_draw, model);
}

static GstFlowReturn
gst_gl_model_render_transform (GstBaseTransform * btrans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstGLModelRender *model = GST_GL_MODEL_RENDER (btrans);
  const GstMetaInfo *info = GST_3D_MODEL_META_INFO;
  gpointer state = NULL;
  GstMeta *meta;

  model->in_buffer = inbuf;
  model->out_buffer = outbuf;

  while ((meta = gst_buffer_iterate_meta (model->in_buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DModelMeta *mmeta = (Gst3DModelMeta *) meta;

      _upload_textures (model, mmeta);
    }
  }

  gst_gl_context_thread_add (GST_GL_BASE_FILTER (model)->context,
      (GstGLContextThreadFunc) _process_model_gl, model);

  return GST_FLOW_OK;
}
