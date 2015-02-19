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

#include <stdio.h>

#include <gst/gl/gl.h>
#include "gstgluploadelement.h"

#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gsteglimagememory.h>
#endif

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

GST_DEBUG_CATEGORY_STATIC (gst_gl_upload_element_debug);
#define GST_CAT_DEFAULT gst_gl_upload_element_debug

typedef struct _UploadMethod UploadMethod;

struct _GstGLUploadElementPrivate
{
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  GstCaps *in_caps;
  GstCaps *out_caps;

  GstBuffer *outbuf;

  /* all method impl pointers */
  gpointer *upload_impl;

  /* current method */
  const UploadMethod *method;
  gpointer method_impl;
  int method_i;
};

static GstCaps *
_set_caps_features (const GstCaps * caps, const gchar * feature_name)
{
  GstCaps *tmp = gst_caps_copy (caps);
  guint n = gst_caps_get_size (tmp);
  guint i = 0;

  for (i = 0; i < n; i++)
    gst_caps_set_features (tmp, i,
        gst_caps_features_from_string (feature_name));

  return tmp;
}

static inline GstGLContext *
_get_context (GstGLUploadElement * upload)
{
  return GST_GL_BASE_FILTER (upload)->context;
}

typedef enum
{
  METHOD_FLAG_CAN_SHARE_CONTEXT = 1,
} GstGLUploadElementMethodFlags;

struct _UploadMethod
{
  const gchar *name;
  GstGLUploadElementMethodFlags flags;

    gpointer (*new) (GstGLUploadElement * upload);
  GstCaps *(*transform_caps) (GstGLContext * context,
      GstPadDirection direction, GstCaps * caps);
    gboolean (*accept) (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
      GstCaps * out_caps);
    gboolean (*propose_allocation) (gpointer impl, GstQuery * decide_query,
      GstQuery * query);
    GstGLUploadElementReturn (*perform) (gpointer impl, GstBuffer * buffer,
      GstBuffer ** outbuf);
  void (*release) (gpointer impl, GstBuffer * buffer);
  void (*free) (gpointer impl);
} _UploadMethod;

struct GLMemoryUpload
{
  GstGLUploadElement *upload;
};

static gpointer
_gl_memory_upload_new (GstGLUploadElement * upload)
{
  struct GLMemoryUpload *mem = g_new0 (struct GLMemoryUpload, 1);

  mem->upload = upload;

  return mem;
}

static GstCaps *
_gl_memory_upload_transform_caps (GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  return _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
}

static gboolean
_gl_memory_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct GLMemoryUpload *upload = impl;
  GstCapsFeatures *features, *gl_features;
  gboolean ret = TRUE;
  int i;

  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY);

  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_is_equal (features, gl_features))
    ret = FALSE;

  features = gst_caps_get_features (in_caps, 0);
  if (!gst_caps_features_is_equal (features, gl_features)
      && !gst_caps_features_is_equal (features,
          GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))
    ret = FALSE;

  gst_caps_features_free (gl_features);

  if (!ret)
    return FALSE;

  if (buffer) {
    if (gst_buffer_n_memory (buffer) !=
        GST_VIDEO_INFO_N_PLANES (&upload->upload->priv->in_info))
      return FALSE;

    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&upload->upload->priv->in_info);
        i++) {
      GstMemory *mem = gst_buffer_peek_memory (buffer, i);

      if (!gst_is_gl_memory (mem))
        return FALSE;
    }
  }

  return TRUE;
}

static gboolean
_gl_memory_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  struct GLMemoryUpload *upload = impl;
  GstAllocationParams params;
  GstAllocator *allocator;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps, *decide_caps;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!decide_query)
    return FALSE;

  gst_query_parse_allocation (decide_query, &decide_caps, NULL);
  if (!decide_caps)
    goto no_caps;

  if (!_gl_memory_upload_accept (impl, NULL, caps, decide_caps))
    return FALSE;

  if (pool == NULL && need_pool) {
    GstVideoInfo info;
    GstBufferPool *decide_pool = NULL;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    gst_query_parse_allocation (decide_query, &decide_caps, NULL);
    decide_pool =
        gst_base_transform_get_buffer_pool (GST_BASE_TRANSFORM
        (upload->upload));

    if (decide_pool && GST_IS_GL_BUFFER_POOL (decide_pool)
        && gst_caps_is_equal_fixed (decide_caps, caps)) {
      config = gst_buffer_pool_get_config (decide_pool);
      gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
      gst_structure_free (config);

      pool = decide_pool;
    } else {
      GST_DEBUG_OBJECT (upload->upload, "create new pool");
      if (decide_pool)
        gst_object_unref (decide_pool);
      pool = gst_gl_buffer_pool_new (_get_context (upload->upload));

      /* the normal size of a frame */
      size = info.size;

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
      if (!gst_buffer_pool_set_config (pool, config))
        goto config_failed;
    }
  }

  if (pool) {
    gst_query_add_allocation_pool (query, pool, size, 1, 0);
    gst_object_unref (pool);
  }

  gst_allocation_params_init (&params);

  allocator = gst_allocator_find (GST_GL_MEMORY_ALLOCATOR);
  gst_query_add_allocation_param (query, allocator, &params);
  gst_object_unref (allocator);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (upload->upload, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (upload->upload, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (upload->upload, "failed setting config");
    return FALSE;
  }
}

static GstGLUploadElementReturn
_gl_memory_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct GLMemoryUpload *upload = impl;
  GstGLMemory *gl_mem;
  int i;

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&upload->upload->priv->in_info); i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);

    gl_mem = (GstGLMemory *) mem;
    if (!gst_gl_context_can_share (_get_context (upload->upload),
            gl_mem->context))
      return GST_GL_UPLOAD_ELEMENT_UNSHARED_GL_CONTEXT;
  }

  *outbuf = buffer;

  return GST_GL_UPLOAD_ELEMENT_DONE;
}

static void
_gl_memory_upload_release (gpointer impl, GstBuffer * buffer)
{
}

static void
_gl_memory_upload_free (gpointer impl)
{
  g_free (impl);
}

static const UploadMethod _gl_memory_upload = {
  "GLMemory",
  METHOD_FLAG_CAN_SHARE_CONTEXT,
  &_gl_memory_upload_new,
  &_gl_memory_upload_transform_caps,
  &_gl_memory_upload_accept,
  &_gl_memory_upload_propose_allocation,
  &_gl_memory_upload_perform,
  &_gl_memory_upload_release,
  &_gl_memory_upload_free
};

#if GST_GL_HAVE_PLATFORM_EGL
struct EGLImageUpload
{
  GstGLUploadElement *upload;
};

static gpointer
_egl_image_upload_new (GstGLUploadElement * upload)
{
  struct EGLImageUpload *image = g_new0 (struct EGLImageUpload, 1);

  image->upload = upload;

  return image;
}

static GstCaps *
_egl_image_upload_transform_caps (GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    ret = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  } else {
    ret = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_EGL_IMAGE);
    gst_caps_set_simple (ret, "format", G_TYPE_STRING, "RGBA", NULL);
  }

  return ret;
}

static gboolean
_egl_image_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct EGLImageUpload *image = impl;
  GstCapsFeatures *features, *gl_features;
  gboolean ret = TRUE;
  int i;

  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_EGL_IMAGE);
  features = gst_caps_get_features (in_caps, 0);
  if (!gst_caps_features_is_equal (features, gl_features))
    ret = FALSE;

  gst_caps_features_free (gl_features);

  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_is_equal (features, gl_features))
    ret = FALSE;

  gst_caps_features_free (gl_features);

  if (!ret)
    return FALSE;

  if (buffer) {
    if (gst_buffer_n_memory (buffer) !=
        GST_VIDEO_INFO_N_PLANES (&image->upload->priv->in_info))
      return FALSE;

    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&image->upload->priv->in_info);
        i++) {
      GstMemory *mem = gst_buffer_peek_memory (buffer, i);

      if (!gst_is_egl_image_memory (mem))
        return FALSE;
    }
  }

  return TRUE;
}

static gboolean
_egl_image_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  struct EGLImageUpload *image = impl;
  GstAllocationParams params;
  GstAllocator *allocator;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps, *decide_caps;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (!decide_query)
    return FALSE;

  gst_query_parse_allocation (decide_query, &decide_caps, NULL);
  if (!decide_caps)
    goto no_caps;

  if (!_egl_image_upload_accept (impl, NULL, caps, decide_caps))
    return FALSE;

  if (pool == NULL && need_pool) {
    GstVideoInfo info;
    GstBufferPool *decide_pool = NULL;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    gst_query_parse_allocation (decide_query, &decide_caps, NULL);
    decide_pool =
        gst_base_transform_get_buffer_pool (GST_BASE_TRANSFORM (image->upload));

    if (decide_pool && GST_IS_GL_BUFFER_POOL (decide_pool)
        && gst_caps_is_equal_fixed (decide_caps, caps)) {
      config = gst_buffer_pool_get_config (decide_pool);
      gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
      gst_structure_free (config);

      pool = decide_pool;
    } else {
      GST_DEBUG_OBJECT (image->upload, "create new pool");
      if (decide_pool)
        gst_object_unref (decide_pool);
      pool = gst_gl_buffer_pool_new (_get_context (image->upload));

      /* the normal size of a frame */
      size = info.size;

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
      if (!gst_buffer_pool_set_config (pool, config))
        goto config_failed;
    }
  }

  if (pool) {
    gst_query_add_allocation_pool (query, pool, size, 1, 0);
    gst_object_unref (pool);
  }

  gst_allocation_params_init (&params);

  if (gst_gl_context_check_feature (_get_context (image->upload),
          "EGL_KHR_image_base")) {
    allocator = gst_allocator_find (GST_EGL_IMAGE_MEMORY_TYPE);
    gst_query_add_allocation_param (query, allocator, &params);
    gst_object_unref (allocator);
  }

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (image->upload, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (image->upload, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (image->upload, "failed setting config");
    return FALSE;
  }
}

static GstGLUploadElementReturn
_egl_image_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct EGLImageUpload *image = impl;
  GstBufferPool *pool;
  GstFlowReturn ret;
  guint i;

  pool =
      gst_base_transform_get_buffer_pool (GST_BASE_TRANSFORM (image->upload));

  if (!pool)
    return GST_GL_UPLOAD_ELEMENT_ERROR;

  if (!gst_buffer_pool_is_active (pool)) {
    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      GST_ELEMENT_ERROR (image->upload, RESOURCE, SETTINGS,
          ("failed to activate bufferpool"), ("failed to activate bufferpool"));
      gst_object_unref (pool);
      return GST_GL_UPLOAD_ELEMENT_ERROR;
    }
  }

  ret = gst_buffer_pool_acquire_buffer (pool, outbuf, NULL);
  gst_object_unref (pool);

  if (ret < GST_FLOW_OK)
    return GST_GL_UPLOAD_ELEMENT_ERROR;

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&image->upload->priv->in_info); i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);
    GstGLMemory *out_gl_mem =
        (GstGLMemory *) gst_buffer_peek_memory (*outbuf, i);
    const GstGLFuncs *gl = NULL;

    gl = GST_GL_CONTEXT (((GstEGLImageMemory *) mem)->context)->gl_vtable;

    gl->ActiveTexture (GL_TEXTURE0 + i);
    gl->BindTexture (GL_TEXTURE_2D, out_gl_mem->tex_id);
    gl->EGLImageTargetTexture2D (GL_TEXTURE_2D,
        gst_egl_image_memory_get_image (mem));
  }

  if (GST_IS_GL_BUFFER_POOL (buffer->pool))
    gst_gl_buffer_pool_replace_last_buffer (GST_GL_BUFFER_POOL (buffer->pool),
        buffer);

  return GST_GL_UPLOAD_ELEMENT_DONE;
}

static void
_egl_image_upload_release (gpointer impl, GstBuffer * buffer)
{
}

static void
_egl_image_upload_free (gpointer impl)
{
  g_free (impl);
}

static const UploadMethod _egl_image_upload = {
  "EGLImage",
  0,
  &_egl_image_upload_new,
  &_egl_image_upload_transform_caps,
  &_egl_image_upload_accept,
  &_egl_image_upload_propose_allocation,
  &_egl_image_upload_perform,
  &_egl_image_upload_release,
  &_egl_image_upload_free
};
#endif

struct GLUploadElementMeta
{
  GstGLUploadElement *upload;

  gboolean result;
  GstVideoGLTextureUploadMeta *meta;
  guint texture_ids[GST_VIDEO_MAX_PLANES];
};

static gpointer
_upload_meta_upload_new (GstGLUploadElement * upload)
{
  struct GLUploadElementMeta *meta = g_new0 (struct GLUploadElementMeta, 1);

  meta->upload = upload;

  return meta;
}

static GstCaps *
_upload_meta_upload_transform_caps (GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    ret = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  } else {
    ret =
        _set_caps_features (caps,
        GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META);
    gst_caps_set_simple (ret, "format", G_TYPE_STRING, "RGBA", NULL);
  }

  return ret;
}

static gboolean
_upload_meta_upload_accept (gpointer impl, GstBuffer * buffer,
    GstCaps * in_caps, GstCaps * out_caps)
{
  struct GLUploadElementMeta *upload = impl;
  GstCapsFeatures *features, *gl_features;
  GstVideoGLTextureUploadMeta *meta;
  gboolean ret = TRUE;

  features = gst_caps_get_features (in_caps, 0);
  gl_features =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META);

  if (!gst_caps_features_is_equal (features, gl_features))
    ret = FALSE;

  gst_caps_features_free (gl_features);

  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_is_equal (features, gl_features))
    ret = FALSE;

  gst_caps_features_free (gl_features);

  if (!ret)
    return ret;

  if (buffer) {
    if ((meta = gst_buffer_get_video_gl_texture_upload_meta (buffer)) == NULL)
      return FALSE;

    if (meta->texture_type[0] != GST_VIDEO_GL_TEXTURE_TYPE_RGBA) {
      GST_FIXME_OBJECT (upload, "only single rgba texture supported");
      return FALSE;
    }

    if (meta->texture_orientation !=
        GST_VIDEO_GL_TEXTURE_ORIENTATION_X_NORMAL_Y_NORMAL) {
      GST_FIXME_OBJECT (upload, "only x-normal, y-normal textures supported");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
_upload_meta_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  struct GLUploadElementMeta *upload = impl;
  GstStructure *gl_context;
  gchar *platform, *gl_apis;
  gpointer handle;

  gl_apis =
      gst_gl_api_to_string (gst_gl_context_get_gl_api (_get_context
          (upload->upload)));
  platform =
      gst_gl_platform_to_string (gst_gl_context_get_gl_platform (_get_context
          (upload->upload)));
  handle =
      (gpointer) gst_gl_context_get_gl_context (_get_context (upload->upload));

  gl_context =
      gst_structure_new ("GstVideoGLTextureUploadMeta", "gst.gl.GstGLContext",
      GST_GL_TYPE_CONTEXT, _get_context (upload->upload),
      "gst.gl.context.handle", G_TYPE_POINTER, handle, "gst.gl.context.type",
      G_TYPE_STRING, platform, "gst.gl.context.apis", G_TYPE_STRING, gl_apis,
      NULL);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, gl_context);

  g_free (gl_apis);
  g_free (platform);
  gst_structure_free (gl_context);

  return TRUE;
}

/*
 * Uploads using gst_video_gl_texture_upload_meta_upload().
 * i.e. consumer of GstVideoGLTextureUploadMeta
 */
static void
_do_upload_with_meta (GstGLContext * context,
    struct GLUploadElementMeta *upload)
{
  if (!gst_video_gl_texture_upload_meta_upload (upload->meta,
          upload->texture_ids)) {
    upload->result = FALSE;
    return;
  }

  upload->result = TRUE;
}

static GstGLUploadElementReturn
_upload_meta_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct GLUploadElementMeta *upload = impl;
  GstBufferPool *pool;
  GstFlowReturn ret;
  guint i;

  GST_LOG_OBJECT (upload, "Attempting upload with GstVideoGLTextureUploadMeta");

  upload->meta = gst_buffer_get_video_gl_texture_upload_meta (buffer);

  pool =
      gst_base_transform_get_buffer_pool (GST_BASE_TRANSFORM (upload->upload));

  if (!pool)
    return GST_GL_UPLOAD_ELEMENT_ERROR;

  if (!gst_buffer_pool_is_active (pool)) {
    if (!gst_buffer_pool_set_active (pool, TRUE)) {
      GST_ELEMENT_ERROR (upload->upload, RESOURCE, SETTINGS,
          ("failed to activate bufferpool"), ("failed to activate bufferpool"));
      gst_object_unref (pool);
      return GST_GL_UPLOAD_ELEMENT_ERROR;
    }
  }

  ret = gst_buffer_pool_acquire_buffer (pool, outbuf, NULL);
  gst_object_unref (pool);

  if (ret < GST_FLOW_OK)
    return GST_GL_UPLOAD_ELEMENT_ERROR;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    guint tex_id = 0;

    if (i < GST_VIDEO_INFO_N_PLANES (&upload->upload->priv->in_info)) {
      GstMemory *mem = gst_buffer_peek_memory (*outbuf, i);
      tex_id = ((GstGLMemory *) mem)->tex_id;
    }

    upload->texture_ids[i] = tex_id;
  }

  GST_LOG ("Uploading with GLTextureUploadMeta with textures %i,%i,%i,%i",
      upload->texture_ids[0], upload->texture_ids[1], upload->texture_ids[2],
      upload->texture_ids[3]);

  gst_gl_context_thread_add (_get_context (upload->upload),
      (GstGLContextThreadFunc) _do_upload_with_meta, upload);

  if (!upload->result) {
    gst_buffer_unref (*outbuf);
    *outbuf = NULL;
    return GST_GL_UPLOAD_ELEMENT_ERROR;
  }

  return GST_GL_UPLOAD_ELEMENT_DONE;
}

static void
_upload_meta_upload_release (gpointer impl, GstBuffer * buffer)
{
}

static void
_upload_meta_upload_free (gpointer impl)
{
  struct GLUploadElementMeta *upload = impl;
  gint i;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    if (upload->texture_ids[i])
      gst_gl_context_del_texture (_get_context (upload->upload),
          &upload->texture_ids[i]);
  }
  g_free (upload);
}

static const UploadMethod _upload_meta_upload = {
  "UploadMeta",
  METHOD_FLAG_CAN_SHARE_CONTEXT,
  &_upload_meta_upload_new,
  &_upload_meta_upload_transform_caps,
  &_upload_meta_upload_accept,
  &_upload_meta_upload_propose_allocation,
  &_upload_meta_upload_perform,
  &_upload_meta_upload_release,
  &_upload_meta_upload_free
};

struct RawUpload
{
  GstGLUploadElement *upload;
  GstGLMemory *in_tex[GST_VIDEO_MAX_PLANES];
  GstVideoFrame in_frame;
};

static gpointer
_raw_data_upload_new (GstGLUploadElement * upload)
{
  struct RawUpload *raw = g_new0 (struct RawUpload, 1);

  raw->upload = upload;

  return raw;
}

static GstCaps *
_raw_data_upload_transform_caps (GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    ret = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  } else {
    ret = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
  }

  return ret;
}

static gboolean
_raw_data_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct RawUpload *raw = impl;
  GstCapsFeatures *features, *gl_features;
  gboolean ret = TRUE;

  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_is_equal (features, gl_features))
    ret = FALSE;

  gst_caps_features_free (gl_features);

  if (!ret)
    return ret;

  if (buffer) {
    if (!gst_video_frame_map (&raw->in_frame, &raw->upload->priv->in_info,
            buffer, GST_MAP_READ))
      return FALSE;

    raw->upload->priv->in_info = raw->in_frame.info;
  }

  return TRUE;
}

static gboolean
_raw_data_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, 0);

  return TRUE;
}

static GstGLUploadElementReturn
_raw_data_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct RawUpload *raw = impl;
  int i;

  if (!raw->in_tex[0])
    gst_gl_memory_setup_wrapped (_get_context (raw->upload),
        &raw->upload->priv->in_info, NULL, raw->in_frame.data, raw->in_tex);

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    if (raw->in_tex[i]) {
      raw->in_tex[i]->data = raw->in_frame.data[i];
      GST_GL_MEMORY_FLAG_SET (raw->in_tex[i], GST_GL_MEMORY_FLAG_NEED_UPLOAD);
    }
  }

  *outbuf = gst_buffer_new ();
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&raw->upload->priv->in_info); i++) {
    gst_buffer_append_memory (*outbuf,
        gst_memory_ref ((GstMemory *) raw->in_tex[i]));
  }

  return GST_GL_UPLOAD_ELEMENT_DONE;
}

static void
_raw_data_upload_release (gpointer impl, GstBuffer * buffer)
{
  struct RawUpload *raw = impl;

  gst_video_frame_unmap (&raw->in_frame);
}

static void
_raw_data_upload_free (gpointer impl)
{
  struct RawUpload *raw = impl;
  int i;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    if (raw->in_tex[i])
      gst_memory_unref ((GstMemory *) raw->in_tex[i]);
  }
  g_free (raw);
}

static const UploadMethod _raw_data_upload = {
  "Raw Data",
  0,
  &_raw_data_upload_new,
  &_raw_data_upload_transform_caps,
  &_raw_data_upload_accept,
  &_raw_data_upload_propose_allocation,
  &_raw_data_upload_perform,
  &_raw_data_upload_release,
  &_raw_data_upload_free
};

static const UploadMethod *upload_methods[] = { &_gl_memory_upload,
#if GST_GL_HAVE_PLATFORM_EGL
  &_egl_image_upload,
#endif
  &_upload_meta_upload, &_raw_data_upload
};

#define gst_gl_upload_element_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLUploadElement, gst_gl_upload_element,
    GST_TYPE_GL_BASE_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_gl_upload_element_debug, "gluploadelement", 0,
        "glupload"););
#define GST_GL_UPLOAD_ELEMENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
    GST_TYPE_GL_UPLOAD_ELEMENT, GstGLUploadElementPrivate))
static void gst_gl_upload_element_finalize (GObject * object);

static gboolean gst_gl_upload_element_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static gboolean gst_gl_upload_element_query (GstBaseTransform * bt,
    GstPadDirection direction, GstQuery * query);
static GstCaps *_gst_gl_upload_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean _gst_gl_upload_element_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);
static gboolean _gst_gl_upload_element_propose_allocation (GstBaseTransform *
    bt, GstQuery * decide_query, GstQuery * query);
static GstFlowReturn
gst_gl_upload_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf);
static GstFlowReturn gst_gl_upload_element_transform (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer * outbuf);

static GstStaticPadTemplate gst_gl_upload_element_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

static GstStaticPadTemplate gst_gl_upload_element_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

static void
gst_gl_upload_element_class_init (GstGLUploadElementClass * klass)
{
  GstBaseTransformClass *bt_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstGLUploadElementPrivate));

  bt_class->query = gst_gl_upload_element_query;
  bt_class->transform_caps = _gst_gl_upload_element_transform_caps;
  bt_class->set_caps = _gst_gl_upload_element_set_caps;
  bt_class->propose_allocation = _gst_gl_upload_element_propose_allocation;
  bt_class->get_unit_size = gst_gl_upload_element_get_unit_size;
  bt_class->prepare_output_buffer = gst_gl_upload_element_prepare_output_buffer;
  bt_class->transform = gst_gl_upload_element_transform;

  bt_class->passthrough_on_same_caps = TRUE;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_gl_upload_element_src_pad_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_gl_upload_element_sink_pad_template));

  gst_element_class_set_metadata (element_class,
      "OpenGL uploader", "Filter/Video",
      "Uploads data into OpenGL", "Matthew Waters <matthew@centricular.com>");

  G_OBJECT_CLASS (klass)->finalize = gst_gl_upload_element_finalize;
}

static void
gst_gl_upload_element_init (GstGLUploadElement * upload)
{
  upload->priv = GST_GL_UPLOAD_ELEMENT_GET_PRIVATE (upload);

  gst_base_transform_set_prefer_passthrough (GST_BASE_TRANSFORM (upload), TRUE);
}

static void
_init_upload_methods (GstGLUploadElement * upload)
{
  gint i, n;

  if (upload->priv->upload_impl)
    return;

  n = G_N_ELEMENTS (upload_methods);
  upload->priv->upload_impl = g_malloc (sizeof (gpointer) * n);
  for (i = 0; i < n; i++) {
    upload->priv->upload_impl[i] = upload_methods[i]->new (upload);
  }
}

static void
gst_gl_upload_element_finalize (GObject * object)
{
  GstGLUploadElement *upload;
  gint i, n;

  upload = GST_GL_UPLOAD_ELEMENT (object);

  if (upload->priv->method_impl) {
    upload->priv->method->release (upload->priv->method_impl,
        upload->priv->outbuf);
    gst_buffer_replace (&upload->priv->outbuf, NULL);
    upload->priv->method->free (upload->priv->method_impl);
  }
  upload->priv->method_i = 0;
  if (upload->priv->in_caps) {
    gst_caps_unref (upload->priv->in_caps);
    upload->priv->in_caps = NULL;
  }

  if (upload->priv->out_caps) {
    gst_caps_unref (upload->priv->out_caps);
    upload->priv->out_caps = NULL;
  }

  if (upload->priv->upload_impl) {
    n = G_N_ELEMENTS (upload_methods);
    for (i = 0; i < n; i++) {
      if (upload->priv->upload_impl[i])
        upload_methods[i]->free (upload->priv->upload_impl[i]);
    }
    g_free (upload->priv->upload_impl);
  }

  G_OBJECT_CLASS (gst_gl_upload_element_parent_class)->finalize (object);
}

static gboolean
gst_gl_upload_element_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    gsize * size)
{
  gboolean ret = FALSE;
  GstVideoInfo info;

  ret = gst_video_info_from_caps (&info, caps);
  if (ret)
    *size = GST_VIDEO_INFO_SIZE (&info);

  return TRUE;
}

static GstCaps *
_gst_gl_upload_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);
  GstGLContext *context = GST_GL_BASE_FILTER (bt)->context;
  GstCaps *result, *tmp;
  gint i;

  tmp = gst_caps_new_empty ();

  _init_upload_methods (upload);

  for (i = 0; i < G_N_ELEMENTS (upload_methods); i++) {
    GstCaps *tmp2 =
        upload_methods[i]->transform_caps (context, direction, caps);

    if (tmp2)
      tmp = gst_caps_merge (tmp, tmp2);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  return result;
}

static gboolean
_gst_gl_upload_element_propose_allocation (GstBaseTransform * bt,
    GstQuery * decide_query, GstQuery * query)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);
  gboolean ret = FALSE;
  gint i;

  /* we only need one to succeed */
  for (i = 0; i < G_N_ELEMENTS (upload_methods); i++)
    ret |= upload_methods[i]->propose_allocation (upload->priv->upload_impl[i],
        decide_query, query);

  return ret;
}

static gboolean
_gst_gl_upload_element_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);

  if (upload->priv->in_caps && upload->priv->out_caps
      && gst_caps_is_equal (upload->priv->in_caps, in_caps)
      && gst_caps_is_equal (upload->priv->out_caps, out_caps))
    return TRUE;

  gst_caps_replace (&upload->priv->in_caps, in_caps);
  gst_caps_replace (&upload->priv->out_caps, out_caps);

  gst_video_info_from_caps (&upload->priv->in_info, in_caps);
  gst_video_info_from_caps (&upload->priv->out_info, out_caps);

  if (upload->priv->method_impl)
    upload->priv->method->free (upload->priv->method_impl);
  upload->priv->method_impl = NULL;
  upload->priv->method_i = 0;

  return TRUE;
}

static gboolean
gst_gl_upload_element_query (GstBaseTransform * bt, GstPadDirection direction,
    GstQuery * query)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DRAIN:
      if (upload->priv->outbuf && upload->priv->method_impl) {
        upload->priv->method->release (upload->priv->method_impl,
            upload->priv->outbuf);
        gst_buffer_replace (&upload->priv->outbuf, NULL);
      }
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (bt, direction, query);
}

static gboolean
_upload_find_method (GstGLUploadElement * upload)
{
  if (upload->priv->method_i >= G_N_ELEMENTS (upload_methods))
    return FALSE;

  if (upload->priv->method_impl)
    upload->priv->method->free (upload->priv->method_impl);

  upload->priv->method = upload_methods[upload->priv->method_i];
  upload->priv->method_impl = upload->priv->method->new (upload);

  GST_DEBUG_OBJECT (upload, "attempting upload with uploader %s",
      upload->priv->method->name);

  upload->priv->method_i++;

  return TRUE;
}

GstFlowReturn
gst_gl_upload_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);
  GstGLUploadElementReturn ret = GST_GL_UPLOAD_ELEMENT_ERROR;

  if (gst_base_transform_is_passthrough (bt)) {
    *outbuf = buffer;
    return GST_FLOW_OK;
  }
#define NEXT_METHOD \
do { \
  if (!_upload_find_method (upload)) { \
    GST_WARNING_OBJECT (upload, "failed to find suitable upload method"); \
    return GST_FLOW_NOT_NEGOTIATED; \
  } \
  goto restart; \
} while (0)

  if (!upload->priv->method_impl)
    _upload_find_method (upload);

restart:
  if (!upload->priv->method->accept (upload->priv->method_impl, buffer,
          upload->priv->in_caps, upload->priv->out_caps))
    NEXT_METHOD;

  ret =
      upload->priv->method->perform (upload->priv->method_impl, buffer, outbuf);
  if (ret == GST_GL_UPLOAD_ELEMENT_UNSHARED_GL_CONTEXT) {
    upload->priv->method->free (upload->priv->method_impl);
    upload->priv->method = &_raw_data_upload;
    upload->priv->method_impl = upload->priv->method->new (upload);
    goto restart;
  } else if (ret == GST_GL_UPLOAD_ELEMENT_DONE) {
    /* we are done */
  } else {
    upload->priv->method->release (upload->priv->method_impl, *outbuf);
    gst_buffer_replace (outbuf, NULL);
    upload->priv->method->free (upload->priv->method_impl);
    NEXT_METHOD;
  }

  return ret == GST_GL_UPLOAD_ELEMENT_DONE ? GST_FLOW_OK : GST_FLOW_ERROR;

#undef NEXT_METHOD
}

static GstFlowReturn
gst_gl_upload_element_transform (GstBaseTransform * bt, GstBuffer * buffer,
    GstBuffer * outbuf)
{
  return GST_FLOW_OK;
}
