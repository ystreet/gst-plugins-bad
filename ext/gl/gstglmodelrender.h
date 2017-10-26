/*
 * GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifndef _GST_GL_MODEL_RENDER_H_
#define _GST_GL_MODEL_RENDER_H_

#include <gst/gst.h>
#include <gst/video/video.h>

#include <gst/gl/gl.h>
#include <gst/3d/3d.h>

G_BEGIN_DECLS

GType gst_gl_model_render_bin_get_type(void);
#define GST_TYPE_GL_MODEL_RENDER_BIN            (gst_gl_model_render_bin_get_type())

GType gst_gl_model_render_get_type(void);
#define GST_TYPE_GL_MODEL_RENDER            (gst_gl_model_render_get_type())
#define GST_GL_MODEL_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_MODEL_RENDER,GstGLModelRender))
#define GST_IS_GL_MODEL_RENDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_MODEL_RENDER))
#define GST_GL_MODEL_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_MODEL_RENDER,GstGLModelRenderClass))
#define GST_IS_GL_MODEL_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_MODEL_RENDER))
#define GST_GL_MODEL_RENDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_MODEL_RENDER,GstGLModelRenderClass))

typedef struct _GstGLModelRender GstGLModelRender;
typedef struct _GstGLModelRenderClass GstGLModelRenderClass;
typedef struct _GstGLModelRenderPrivate GstGLModelRenderPrivate;

/**
 * GstGLModelRender:
 * @parent: parent #GstBaseTransform
 * @display: the currently configured #GstGLDisplay
 * @context: the currently configured #GstGLContext
 * @out_caps: the currently configured output #GstCaps
 */
struct _GstGLModelRender
{
  GstGLBaseFilter    parent;

  GstVideoInfo       vinfo;

  GstGLShader       *shader;
  Gst3DCamera       *camera;

  /* <private> */
  GstBuffer         *out_buffer;
  GstBuffer         *in_buffer;
  GstGLFramebuffer  *fbo;

  GstGLModelRenderPrivate *priv;
};

/**
 * GstGLModelRenderClass:
 * @parent_class: parent class
 * @supported_gl_api: the logical-OR of #GstGLAPI's supported by this element
 * @gl_start: called in the GL thread to setup the element GL state.
 * @gl_stop: called in the GL thread to setup the element GL state.
 */
struct _GstGLModelRenderClass
{
  GstGLBaseFilterClass parent_class;

  /* <private> */
  gpointer _padding[GST_PADDING];
};

G_END_DECLS

#endif /* _GST_GL_MODEL_RENDER_H_ */
