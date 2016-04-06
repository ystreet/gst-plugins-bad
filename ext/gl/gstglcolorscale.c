/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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

/**
 * SECTION:element-glcolorscale
 *
 * video frame scaling and colorspace conversion.
 *
 * <refsect2>
 * <title>Scaling and Color space conversion</title>
 * <para>
 * Equivalent to glupload ! gldownload.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Examples</title>
 * |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw ! glcolorscale ! ximagesink
 * ]| A pipeline to test colorspace conversion.
 * FBO is required.
  |[
 * gst-launch-1.0 -v videotestsrc ! video/x-raw, width=640, height=480, format=AYUV ! glcolorscale ! \
 *   video/x-raw, width=320, height=240, format=YV12 ! videoconvert ! autovideosink
 * ]| A pipeline to test hardware scaling and colorspace conversion.
 * FBO and GLSL are required.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglcolorscale.h"
#include <gst/video/gstvideoaffinetransformationmeta.h>


#define GST_CAT_DEFAULT gst_gl_colorscale_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* Properties */
enum
{
  PROP_0
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_colorscale_debug, "glcolorscale", 0, "glcolorscale element");
#define gst_gl_colorscale_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLColorscale, gst_gl_colorscale,
    GST_TYPE_GL_FILTER, DEBUG_INIT);

static void gst_gl_colorscale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_colorscale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_colorscale_gl_start (GstGLBaseFilter * base_filter);
static void gst_gl_colorscale_gl_stop (GstGLBaseFilter * base_filter);

static gboolean gst_gl_colorscale_filter_texture (GstGLFilter * filter,
    guint in_tex, guint out_tex);
static gboolean gst_gl_colorscale_filter (GstGLFilter * filter,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_gl_colorscale_class_init (GstGLColorscaleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *basetransform_class;
  GstGLBaseFilterClass *base_filter_class;
  GstGLFilterClass *filter_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  basetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  base_filter_class = GST_GL_BASE_FILTER_CLASS (klass);
  filter_class = GST_GL_FILTER_CLASS (klass);

  gobject_class->set_property = gst_gl_colorscale_set_property;
  gobject_class->get_property = gst_gl_colorscale_get_property;

  gst_element_class_set_metadata (element_class, "OpenGL color scale",
      "Filter/Effect/Video", "Colorspace converter and video scaler",
      "Julien Isorce <julien.isorce@gmail.com>\n"
      "Matthew Waters <matthew@centricular.com>");

  basetransform_class->passthrough_on_same_caps = TRUE;

  base_filter_class->gl_start = GST_DEBUG_FUNCPTR (gst_gl_colorscale_gl_start);
  base_filter_class->gl_stop = GST_DEBUG_FUNCPTR (gst_gl_colorscale_gl_stop);
  base_filter_class->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;

  filter_class->filter = gst_gl_colorscale_filter;
  filter_class->filter_texture = gst_gl_colorscale_filter_texture;
}

static void
gst_gl_colorscale_init (GstGLColorscale * colorscale)
{
}

static void
gst_gl_colorscale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_colorscale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gfloat identity_matrix[] = {
  1.0, 0.0f, 0.0, 0.0,
  0.0, 1.0f, 0.0, 0.0,
  0.0, 0.0f, 1.0, 0.0,
  0.0, 0.0f, 0.0, 1.0,
};

static gboolean
gst_gl_colorscale_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (base_filter);
  GstGLFilter *filter = GST_GL_FILTER (base_filter);
  GstGLSLStage *frag_stage, *vert_stage;
  GstGLShader *shader;
  GError *error = NULL;
  if (!(vert_stage = gst_glsl_stage_new_with_string (base_filter->context,
              GL_VERTEX_SHADER, GST_GLSL_VERSION_NONE,
              GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY,
              gst_gl_shader_string_vertex_mat4_texture_transform))) {
    GST_ERROR_OBJECT (colorscale, "Failed to create vertex shader");
    return FALSE;
  }

  if (!(frag_stage =
          gst_glsl_stage_new_default_fragment (base_filter->context))) {
    GST_ERROR_OBJECT (colorscale, "Failed to create vertex shader");
    return FALSE;
  }

  if (!(shader =
          gst_gl_shader_new_link_with_stages (base_filter->context, &error,
              vert_stage, frag_stage, NULL))) {
    GST_ERROR_OBJECT (colorscale, "Failed to initialize shader: %s",
        error->message);
    return FALSE;
  }

  filter->draw_attr_position_loc =
      gst_gl_shader_get_attribute_location (shader, "a_position");
  filter->draw_attr_texture_loc =
      gst_gl_shader_get_attribute_location (shader, "a_texcoord");

  gst_gl_shader_use (shader);
  gst_gl_shader_set_uniform_matrix_4fv (shader,
      "u_transformation", 1, FALSE, identity_matrix);
  gst_gl_context_clear_shader (base_filter->context);

  colorscale->shader = shader;

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter);
}

static void
gst_gl_colorscale_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (base_filter);

  if (colorscale->shader) {
    gst_object_unref (colorscale->shader);
    colorscale->shader = NULL;
  }

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static void
_colorscale_cb (gint width, gint height, guint texture, gpointer stuff)
{
  GstGLFilter *filter = GST_GL_FILTER (stuff);
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (stuff);
  GstGLContext *context = GST_GL_BASE_FILTER (filter)->context;
  const GstGLFuncs *gl = context->gl_vtable;

#if GST_GL_HAVE_OPENGL
  if (gst_gl_context_get_gl_api (context) & GST_GL_API_OPENGL) {
    gl->MatrixMode (GL_PROJECTION);
    gl->LoadIdentity ();
  }
#endif

  gst_gl_shader_use (colorscale->shader);

  gl->ActiveTexture (GL_TEXTURE1);
  gl->BindTexture (GL_TEXTURE_2D, texture);

  gst_gl_shader_set_uniform_1i (colorscale->shader, "tex", 1);
  gst_gl_shader_set_uniform_1f (colorscale->shader, "width", width);
  gst_gl_shader_set_uniform_1f (colorscale->shader, "height", height);
  gst_gl_shader_set_uniform_matrix_4fv (colorscale->shader,
      "u_transformation", 1, FALSE, colorscale->transformation_matrix);

  gst_gl_filter_draw_texture (filter, texture, width, height);
}

static gboolean
gst_gl_colorscale_filter_texture (GstGLFilter * filter, guint in_tex,
    guint out_tex)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (filter);

  gst_gl_filter_render_to_target (filter, TRUE, in_tex, out_tex,
      (GLCB) _colorscale_cb, colorscale);

  return TRUE;
}

static gboolean
gst_gl_colorscale_filter (GstGLFilter * filter, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (filter);
  GstVideoAffineTransformationMeta *af_meta;

  af_meta = gst_buffer_get_video_affine_transformation_meta (inbuf);
  if (af_meta)
    colorscale->transformation_matrix = af_meta->matrix;
  else
    colorscale->transformation_matrix = identity_matrix;

  return gst_gl_filter_filter_texture (filter, inbuf, outbuf);
}
