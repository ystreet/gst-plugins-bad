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

#include "gstglmodelrender.h"

#define GST_CAT_DEFAULT gst_gl_model_render_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

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
  guint vbo;
  guint vao;
  guint vbo_indices;
  gint draw_attr_position_loc;
  gint draw_attr_texture0_loc;

  GstGLModelRenderBin *model;
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
  model->model->priv->model = model;
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

static gboolean gst_gl_model_render_gl_start (GstGLBaseFilter * base_filter);
static void gst_gl_model_render_gl_stop (GstGLBaseFilter * base_filter);

/* vertex source */
static const gchar *cube_v_src =
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texcoord0;\n"
    "varying vec2 v_texcoord0;\n"
    "const mat4 scale = \n"
    "   mat4 (0.75, 0.,   0.,   0.,\n"
    "         0.,  -0.75, 0.,   0.,\n"
    "         0.,   0.,   0.75, 0.,\n"
    "         0.,   0.,   0.,   1.);\n"
    "void main()\n"
    "{\n"
    "   gl_Position = scale * a_position;\n"
    "   v_texcoord0 = a_texcoord0;\n"
    "}\n";

/* fragment source */
static const gchar *cube_f_src =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 v_texcoord0;\n"
    "uniform sampler2D tex0;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = texture2D(tex0, v_texcoord0);\n"
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

  base_filter_class->supported_gl_api = GST_GL_API_ANY;
  base_filter_class->gl_start = gst_gl_model_render_gl_start;
  base_filter_class->gl_stop = gst_gl_model_render_gl_stop;
}

static void
gst_gl_model_render_init (GstGLModelRender * model)
{
  model->priv = GST_GL_MODEL_RENDER_GET_PRIVATE (model);
}

static void
gst_gl_model_render_finalize (GObject * object)
{
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
_bind_buffer (GstGLModelRender * model, Gst3DVertexInfo * vinfo)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (model)->context->gl_vtable;
  int pos_idx = gst_3d_vertex_info_find_attribute_index (vinfo,
      GST_3D_VERTEX_TYPE_POSITION, NULL);
  int tex_idx = gst_3d_vertex_info_find_attribute_index (vinfo,
      GST_3D_VERTEX_TYPE_TEXTURE, NULL);
  g_assert (pos_idx >= 0 && tex_idx >= 0);

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, model->priv->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, model->priv->vbo);

  /* Load the vertex position */
  /* FIXME: hardcoded format */
  gl->VertexAttribPointer (model->priv->draw_attr_position_loc, 3, GL_FLOAT,
      GL_FALSE, vinfo->strides[pos_idx], (void *) vinfo->offsets[pos_idx]);
  gl->VertexAttribPointer (model->priv->draw_attr_texture0_loc, 2, GL_FLOAT,
      GL_FALSE, vinfo->strides[tex_idx], (void *) vinfo->offsets[tex_idx]);

  gl->EnableVertexAttribArray (model->priv->draw_attr_position_loc);
  gl->EnableVertexAttribArray (model->priv->draw_attr_texture0_loc);
}

static void
_unbind_buffer (GstGLModelRender * model)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (model)->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (model->priv->draw_attr_position_loc);
  gl->DisableVertexAttribArray (model->priv->draw_attr_texture0_loc);
}

static gboolean
_update_vertex_buffer (GstGLModelRender * model, Gst3DModelMeta * meta,
    guint * n_indices, Gst3DVertexInfo ** vinfo)
{
  const GstGLFuncs *gl = GST_GL_BASE_FILTER (model)->context->gl_vtable;
  Gst3DVertexMeta *vmeta;
  Gst3DVertexMapInfo map_info;

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
    gl->BindVertexArray (model->priv->vao);
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
  g_signal_emit_by_name (model->priv->model->src, "push-sample", tex->sample,
      &flow_ret);
  GST_ERROR ("%p", gst_sample_get_buffer (tex->sample));
  if (flow_ret != GST_FLOW_OK)
    return NULL;

  GST_ERROR ("%p", gst_sample_get_buffer (tex->sample));
  if (model->priv->model->pulled_preroll)
    g_signal_emit_by_name (model->priv->model->sink, "pull-sample", &gl_sample);
  else
    g_signal_emit_by_name (model->priv->model->sink, "pull-preroll",
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

static GstGLMemory *
_update_materials (GstGLModelRender * model, Gst3DModelMeta * meta)
{
  GstGLContext *context = GST_GL_BASE_FILTER (model)->context;
  const GstGLFuncs *gl = context->gl_vtable;
  Gst3DMaterialMeta *mmeta;
  Gst3DMaterialStack *diffuse;
  Gst3DMaterialTexture *tex;
  GstBuffer *buf;
  GstGLMemory *ret;

  mmeta = gst_buffer_get_3d_material_meta (meta->buffer, meta->material_id);
  diffuse =
      gst_3d_material_meta_get_stack (mmeta, GST_3D_MATERIAL_ELEMENT_DIFFUSE);
  tex = gst_3d_material_stack_get_texture (diffuse, 0);
  if (!tex)
    return NULL;
  buf = gst_sample_get_buffer (tex->sample);
  ret = (GstGLMemory *) _get_cached_gl_memory (buf);
  g_assert (ret);               /* no funny business now */

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, gst_gl_memory_get_texture_id (ret));

  gst_gl_shader_set_uniform_1i (model->shader, "tex0", 0);

  return ret;
}

static gboolean
_create_shader (GstGLModelRender * model)
{
  GstGLContext *context = GST_GL_BASE_FILTER (model)->context;
  GstGLSLStage *vert, *frag;
  GError *error = NULL;

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

  frag = gst_glsl_stage_new_with_string (context, GL_FRAGMENT_SHADER,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, cube_f_src);
  if (!gst_glsl_stage_compile (frag, &error)) {
    GST_ERROR_OBJECT (frag, "%s", error->message);
    g_error_free (error);
    gst_object_unref (vert);
    gst_object_unref (frag);
    return FALSE;
  }

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
  model->priv->draw_attr_position_loc =
      gst_gl_shader_get_attribute_location (model->shader, "a_position");
  model->priv->draw_attr_texture0_loc =
      gst_gl_shader_get_attribute_location (model->shader, "a_texcoord0");
  gst_gl_context_clear_shader (context);

  return TRUE;
}

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

  if (!_create_shader (model))
    return FALSE;
  gst_gl_shader_use (model->shader);

  if (!model->priv->vbo) {
    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &model->priv->vao);
    }

    gl->GenBuffers (1, &model->priv->vbo);
    gl->GenBuffers (1, &model->priv->vbo_indices);
  }

  if (gl->GenVertexArrays)
    gl->BindVertexArray (model->priv->vao);

  gl->Enable (GL_DEPTH_TEST);
  gl->ClearColor (0.0, 0.0, 0.0, 0.0);
  gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  while ((meta = gst_buffer_iterate_meta (model->in_buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DModelMeta *mmeta = (Gst3DModelMeta *) meta;
      GstGLMemory *tex;

      /* XXX: terribly inefficient */
      if (!_update_vertex_buffer (model, mmeta, &n_indices, &vinfo))
        return FALSE;
      /* XXX: terribly inefficient */
      if (!(tex = _update_materials (model, mmeta)))
        return FALSE;

      /* FIXME: hardcoded index format */
      gl->DrawElements (GL_TRIANGLES, n_indices, GL_UNSIGNED_SHORT, 0);

      gst_memory_unref ((GstMemory *) tex);
    }
  }

  gl->Disable (GL_DEPTH_TEST);

  gst_gl_context_clear_shader (context);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  _unbind_buffer (model);

  return TRUE;
}

static GstGLMemory *
_upload_material (GstGLModelRender * model, Gst3DModelMeta * meta)
{
  Gst3DMaterialMeta *mmeta;
  Gst3DMaterialStack *diffuse;
  Gst3DMaterialTexture *tex;
  GstGLMemory *ret;

  mmeta = gst_buffer_get_3d_material_meta (meta->buffer, meta->material_id);
  diffuse =
      gst_3d_material_meta_get_stack (mmeta, GST_3D_MATERIAL_ELEMENT_DIFFUSE);
  if (!(tex = gst_3d_material_stack_get_texture (diffuse, 0)))
    return NULL;

  ret = _create_upload_texture (model, tex);

  return ret;
}

static gboolean
_upload_materials (GstGLModelRender * model)
{
  const GstMetaInfo *info = GST_3D_MODEL_META_INFO;
  gpointer state = NULL;
  GstMeta *meta;

  while ((meta = gst_buffer_iterate_meta (model->in_buffer, &state))) {
    if (meta->info->api == info->api) {
      Gst3DModelMeta *mmeta = (Gst3DModelMeta *) meta;
      GstGLMemory *tex;

      /* XXX: terribly inefficient */
      if (!(tex = _upload_material (model, mmeta)))
        return FALSE;
    }
  }

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

  model->in_buffer = inbuf;
  model->out_buffer = outbuf;

  _upload_materials (model);

  gst_gl_context_thread_add (GST_GL_BASE_FILTER (model)->context,
      (GstGLContextThreadFunc) _process_model_gl, model);

  return GST_FLOW_OK;
}
