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

/**
 * SECTION:element-3dvertexsrc
 *
 * <refsect2>
 * <para>
 * The gltestsrc element is used to produce test video texture.
 * The video test produced can be controlled with the "pattern"
 * property.
 * </para>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch-1.0 -v gltestsrc pattern=smpte ! glimagesink
 * </programlisting>
 * Shows original SMPTE color bars in a window.
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gst3dvertexsrc.h"

GST_DEBUG_CATEGORY_STATIC (trid_vertex_src_debug);
#define GST_CAT_DEFAULT trid_vertex_src_debug

enum
{
  PROP_0,
  PROP_PATTERN,
  PROP_TIMESTAMP_OFFSET,
  PROP_IS_LIVE
      /* FILL ME */
};

/* *INDENT-OFF* */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/3d-vertices, "
        "primitive-mode = (string) triangles, "
        "index-format = (string) U16LE, "
        "attributes = (structure) <\"attribute, "
            "format = (string) F32LE, "
            "type = (string) position, "
            "channels = (int) 3 ;\", "
            "\"attribute, "
            "format = (string) F32LE, "
            "type = (string) texture, "
            "channels = (int) 2 ;\">")
    );
/* *INDENT-ON* */

#define gst_3d_vertex_src_parent_class parent_class
G_DEFINE_TYPE (Gst3DVertexSrc, gst_3d_vertex_src, GST_TYPE_PUSH_SRC);

static void gst_3d_vertex_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_3d_vertex_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_3d_vertex_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);
static GstCaps *gst_3d_vertex_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);

static gboolean gst_3d_vertex_src_is_seekable (GstBaseSrc * psrc);
static gboolean gst_3d_vertex_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static gboolean gst_3d_vertex_src_query (GstBaseSrc * bsrc, GstQuery * query);
static GstStateChangeReturn gst_3d_vertex_src_change_state (GstElement *
    element, GstStateChange transition);

static void gst_3d_vertex_src_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static GstFlowReturn gst_3d_vertex_src_create (GstPushSrc * psrc,
    GstBuffer ** outbuf);
static gboolean gst_3d_vertex_src_start (GstBaseSrc * basesrc);
static gboolean gst_3d_vertex_src_stop (GstBaseSrc * basesrc);

static void
gst_3d_vertex_src_class_init (Gst3DVertexSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;
  GstElementClass *element_class;

  GST_DEBUG_CATEGORY_INIT (trid_vertex_src_debug, "gltestsrc", 0,
      "Video Test Source");

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_3d_vertex_src_set_property;
  gobject_class->get_property = gst_3d_vertex_src_get_property;

  g_object_class_install_property (gobject_class,
      PROP_TIMESTAMP_OFFSET, g_param_spec_int64 ("timestamp-offset",
          "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "Model test source",
      "Source/Model", "Creates a test model stream",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);

  element_class->change_state = gst_3d_vertex_src_change_state;

  gstbasesrc_class->set_caps = gst_3d_vertex_src_setcaps;
  gstbasesrc_class->is_seekable = gst_3d_vertex_src_is_seekable;
  gstbasesrc_class->do_seek = gst_3d_vertex_src_do_seek;
  gstbasesrc_class->query = gst_3d_vertex_src_query;
  gstbasesrc_class->get_times = gst_3d_vertex_src_get_times;
  gstbasesrc_class->start = gst_3d_vertex_src_start;
  gstbasesrc_class->stop = gst_3d_vertex_src_stop;
  gstbasesrc_class->fixate = gst_3d_vertex_src_fixate;

  gstpushsrc_class->create = gst_3d_vertex_src_create;
}

static void
gst_3d_vertex_src_init (Gst3DVertexSrc * src)
{
  src->timestamp_offset = 0;

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), FALSE);
}

static GstCaps *
gst_3d_vertex_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GST_DEBUG ("fixate");

  caps = gst_caps_fixate (caps);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static void
gst_3d_vertex_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (object);

  switch (prop_id) {
    case PROP_TIMESTAMP_OFFSET:
      src->timestamp_offset = g_value_get_int64 (value);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
      break;
    default:
      break;
  }
}

static void
gst_3d_vertex_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (object);

  switch (prop_id) {
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, src->timestamp_offset);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_3d_vertex_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (bsrc);

  GST_DEBUG ("setcaps");

  src->negotiated = TRUE;

  GST_DEBUG_OBJECT (src, "set caps %" GST_PTR_FORMAT, caps);

  if (!gst_3d_vertex_info_from_caps (&src->vinfo, caps))
    return FALSE;
  src->vinfo.max_vertices = 24;
  src->vinfo.offsets[0] = 0;
  src->vinfo.strides[0] = 5 * sizeof (float);
  src->vinfo.offsets[1] = src->vinfo.offsets[0] + 3 * sizeof (float);
  src->vinfo.strides[1] = 5 * sizeof (float);
  src->vinfo.offsets[2] =
      src->vinfo.offsets[1] + src->vinfo.max_vertices * src->vinfo.strides[1];
  src->vinfo.strides[2] = 1 * sizeof (short);

  gst_caps_replace (&src->out_caps, caps);

  return TRUE;

/* ERRORS */
}

static gboolean
gst_3d_vertex_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
#if 0
  gboolean res = FALSE;
  Gst3DVertexSrc *src;

  src = GST_3D_VERTEX_SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      res =
          gst_video_info_convert (&src->out_info, src_fmt, src_val, dest_fmt,
          &dest_val);
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      return res;
    }
    default:
      break;
  }
#endif
  return GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
}

static void
gst_3d_vertex_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration))
        *end = timestamp + duration;
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gboolean
gst_3d_vertex_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
#if 0
  GstClockTime time;
  Gst3DVertexSrc *src;

  src = GST_3D_VERTEX_SRC (bsrc);

  segment->time = segment->start;
  time = segment->position;

  /* now move to the time indicated */
  if (src->out_info.fps_n) {
    src->n_frames = gst_util_uint64_scale (time,
        src->out_info.fps_n, src->out_info.fps_d * GST_SECOND);
  } else
    src->n_frames = 0;

  if (src->out_info.fps_n) {
    src->running_time = gst_util_uint64_scale (src->n_frames,
        src->out_info.fps_d * GST_SECOND, src->out_info.fps_n);
  } else {
    /* FIXME : Not sure what to set here */
    src->running_time = 0;
  }

  g_return_val_if_fail (src->running_time <= time, FALSE);
#endif
  return TRUE;
}

static gboolean
gst_3d_vertex_src_is_seekable (GstBaseSrc * psrc)
{
  /* we're seekable... */
  return TRUE;
}

/* *INDENT-OFF* */
static const float cube[] = {
 /*|     Vertex     | TexCoord |*/ 
    /* front face */
     1.0,  1.0, -1.0, 1.0, 0.0,
     1.0, -1.0, -1.0, 1.0, 1.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
    -1.0,  1.0, -1.0, 0.0, 0.0,
    /* back face */
     1.0,  1.0,  1.0, 1.0, 0.0,
    -1.0,  1.0,  1.0, 0.0, 0.0,
    -1.0, -1.0,  1.0, 0.0, 1.0,
     1.0, -1.0,  1.0, 1.0, 1.0,
    /* right face */
     1.0,  1.0,  1.0, 1.0, 0.0,
     1.0, -1.0,  1.0, 0.0, 0.0,
     1.0, -1.0, -1.0, 0.0, 1.0,
     1.0,  1.0, -1.0, 1.0, 1.0,
    /* left face */
    -1.0,  1.0,  1.0, 1.0, 0.0,
    -1.0,  1.0, -1.0, 1.0, 1.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
    -1.0, -1.0,  1.0, 0.0, 0.0,
    /* top face */
     1.0, -1.0,  1.0, 1.0, 0.0,
    -1.0, -1.0,  1.0, 0.0, 0.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
     1.0, -1.0, -1.0, 1.0, 1.0,
    /* bottom face */
     1.0,  1.0,  1.0, 1.0, 0.0,
     1.0,  1.0, -1.0, 1.0, 1.0,
    -1.0,  1.0, -1.0, 0.0, 1.0,
    -1.0,  1.0,  1.0, 0.0, 0.0
};

static const short indices[] = {
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23
};
/* *INDENT-ON* */

static GstFlowReturn
gst_3d_vertex_src_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (psrc);
  GstClockTime next_time;
  GstBuffer *buffer;

  if (G_UNLIKELY (!src->negotiated))
    goto not_negotiated;

  if (!src->buffer) {
    GstMemory *index_mem, *vert_mem;
    GstMapInfo map_info;

    src->buffer = gst_buffer_new ();

    index_mem = gst_allocator_alloc (NULL, sizeof (indices), NULL);
    vert_mem = gst_allocator_alloc (NULL, sizeof (cube), NULL);
    gst_buffer_append_memory (src->buffer, index_mem);
    gst_buffer_append_memory (src->buffer, vert_mem);

    if (!gst_memory_map (index_mem, &map_info, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (src, "Failed to map memory for writing");
      return GST_FLOW_NOT_NEGOTIATED;
    }
    memcpy (map_info.data, indices, sizeof (indices));
    gst_memory_unmap (index_mem, &map_info);

    if (!gst_memory_map (vert_mem, &map_info, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (src, "Failed to map memory for writing");
      return GST_FLOW_NOT_NEGOTIATED;
    }
    memcpy (map_info.data, cube, sizeof (cube));
    gst_memory_unmap (vert_mem, &map_info);

    gst_buffer_add_3d_vertex_meta (src->buffer, 0, &src->vinfo, 36, 24);
  }

  buffer = gst_buffer_make_writable (gst_buffer_ref (src->buffer));

  GST_BUFFER_TIMESTAMP (buffer) = src->timestamp_offset + src->running_time;
  GST_BUFFER_OFFSET (buffer) = src->n_frames;
  src->n_frames++;
  GST_BUFFER_OFFSET_END (buffer) = src->n_frames;
  if (TRUE) {
    next_time = gst_util_uint64_scale_int (src->n_frames * GST_SECOND,
        /* denom */ 1, /* numer */ 10);
    GST_BUFFER_DURATION (buffer) = next_time - src->running_time;
  } else {
    next_time = src->timestamp_offset;
    /* NONE means forever */
    GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  }

  src->running_time = next_time;

  *outbuf = buffer;
  return GST_FLOW_OK;

not_negotiated:
  {
    GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL),
        ("format wasn't negotiated before get function"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static gboolean
gst_3d_vertex_src_start (GstBaseSrc * basesrc)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (basesrc);

  src->running_time = 0;
  src->n_frames = 0;
  src->negotiated = FALSE;

  return TRUE;
}

static gboolean
gst_3d_vertex_src_stop (GstBaseSrc * basesrc)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (basesrc);

  gst_caps_replace (&src->out_caps, NULL);
  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  return TRUE;
}

static GstStateChangeReturn
gst_3d_vertex_src_change_state (GstElement * element, GstStateChange transition)
{
  Gst3DVertexSrc *src = GST_3D_VERTEX_SRC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (src, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
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
