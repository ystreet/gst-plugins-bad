/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <matthew@centricular.com>
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

#include "gl.h"
#include "gstglsyncmeta.h"

#define GST_CAT_DEFAULT gst_gl_sync_meta_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#endif
#ifndef GL_TIMEOUT_EXPIRED
#define GL_TIMEOUT_EXPIRED 0x911B
#endif

static void
_default_set_sync (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (gl->FenceSync) {
    if (sync_meta->data) {
      GST_LOG ("deleting sync object %p", sync_meta->data);
      gl->DeleteSync ((GLsync) sync_meta->data);
    }
    sync_meta->data =
        (gpointer) gl->FenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    gl->Flush ();
    GST_LOG ("setting sync object %p", sync_meta->data);
  } else {
    gl->Finish ();
  }
}

static void
_default_wait (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;
  GLenum res;

  if (gl->ClientWaitSync) {
    do {
      GST_LOG ("waiting on sync object %p", sync_meta->data);
      res =
          gl->ClientWaitSync ((GLsync) sync_meta->data,
          GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000 /* 1s */ );
    } while (res == GL_TIMEOUT_EXPIRED);
  }
}

static void
_default_copy (GstGLSyncMeta * src, GstBuffer * sbuffer, GstGLSyncMeta * dest,
    GstBuffer * dbuffer)
{
  GST_LOG ("copy sync object %p from meta %p to %p", src->data, src, dest);

  /* Setting a sync point here relies on GstBuffer copying
   * metas after data */
  _default_set_sync (src, src->context);
}

static void
_default_free (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (sync_meta->data) {
    GST_LOG ("deleting sync object %p", sync_meta->data);
    gl->DeleteSync ((GLsync) sync_meta->data);
    sync_meta->data = NULL;
  }
}

GstGLSyncMeta *
gst_buffer_add_gl_sync_meta_full (GstGLContext * context, GstBuffer * buffer,
    gpointer sync_func, gpointer wait_func, gpointer copy_func,
    gpointer free_func, gpointer data)
{
  GstGLSyncMeta *meta;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  meta =
      (GstGLSyncMeta *) gst_buffer_add_meta ((buffer), GST_GL_SYNC_META_INFO,
      NULL);

  if (!meta)
    return NULL;

  meta->context = gst_object_ref (context);
  meta->set_sync = sync_func;
  meta->wait = wait_func;
  meta->copy = copy_func;
  meta->free = free_func;
  meta->data = data;

  return meta;
}

GstGLSyncMeta *
gst_buffer_add_gl_sync_meta (GstGLContext * context, GstBuffer * buffer)
{
  return gst_buffer_add_gl_sync_meta_full (context, buffer, _default_set_sync,
      _default_wait, _default_copy, _default_free, NULL);
}

static void
_set_sync_point (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->set_sync != NULL);

  GST_LOG ("setting sync point %p", sync_meta);
  sync_meta->set_sync (sync_meta, context);
}

void
gst_gl_sync_meta_set_sync_point (GstGLSyncMeta * sync_meta,
    GstGLContext * context)
{
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _set_sync_point, sync_meta);
}

static void
_wait (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->wait != NULL);

  GST_LOG ("waiting %p", sync_meta);
  sync_meta->wait (sync_meta, context);
}

void
gst_gl_sync_meta_wait (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _wait, sync_meta);
}

static gboolean
_gst_gl_sync_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstGLSyncMeta *dmeta, *smeta;

  smeta = (GstGLSyncMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    g_assert (smeta->copy != NULL);

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta = gst_buffer_add_gl_sync_meta_full (smeta->context, dest,
          smeta->set_sync, smeta->wait, smeta->copy, smeta->free, NULL);

      if (!dmeta)
        return FALSE;

      GST_LOG ("copying sync meta %p into %p", smeta, dmeta);
      smeta->copy (smeta, buffer, dmeta, dest);
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

static void
_free_gl_sync_meta (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->free != NULL);

  GST_LOG ("free sync meta %p", sync_meta);
  sync_meta->free (sync_meta, context);
}

static void
_gst_gl_sync_meta_free (GstGLSyncMeta * sync_meta, GstBuffer * buffer)
{
  gst_gl_context_thread_add (sync_meta->context,
      (GstGLContextThreadFunc) _free_gl_sync_meta, sync_meta);

  gst_object_unref (sync_meta->context);
}

static gboolean
_gst_gl_sync_meta_init (GstGLSyncMeta * sync_meta, gpointer params,
    GstBuffer * buffer)
{
  static volatile gsize _init;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_gl_sync_meta_debug, "glsyncmeta", 0,
        "glsyncmeta");
    g_once_init_leave (&_init, 1);
  }

  sync_meta->context = NULL;
  sync_meta->data = NULL;

  return TRUE;
}

GType
gst_gl_sync_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstGLSyncMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }

  return type;
}

const GstMetaInfo *
gst_gl_sync_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_GL_SYNC_META_API_TYPE, "GstGLSyncMeta",
        sizeof (GstGLSyncMeta), (GstMetaInitFunction) _gst_gl_sync_meta_init,
        (GstMetaFreeFunction) _gst_gl_sync_meta_free,
        _gst_gl_sync_meta_transform);
    g_once_init_leave (&meta_info, meta);
  }

  return meta_info;
}
