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

#include <sys/stat.h>
#include <stdio.h>

#include "gstassimp.h"

/* TODO: lights, animation, textures stored in the model */

#define GST_CAT_DEFAULT gst_assimp_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define BLOCKSIZE 1024 * 16

static struct aiVector3D Zero3D = { 0.0, 0.0, 0.0 };
static struct aiColor4D ZeroColor4D = { 0.0, 0.0, 0.0, 0.0 };

struct sample_load;
typedef void (*LinkDecodedPad) (struct sample_load * load, GstElement * decoder,
    GstPad * new_pad);

struct sample_load
{
  GstAssimp *ai;
  GstElement *bin;
  GstElement *src, *typefind, *decoder, *sink;
  GMutex lock;
  GCond cond;
  gboolean ready;
  GstSample *sample;

  LinkDecodedPad link;
};

typedef void (*ai_base_close) (gpointer user_data);

struct ai_base
{
  ai_base_close destroy;
};

struct buffer_file_user_data
{
  struct ai_base base;

  GstBuffer *buffer;
  gboolean mapped;
  GstMapInfo map_info;
  gsize file_cursor;
};

typedef struct _AIBin AIBin;
typedef struct _AIBinClass AIBinClass;

struct _AIBin
{
  GstBin parent;

  struct sample_load *wait;
};

struct _AIBinClass
{
  GstBinClass parent_class;
};

GType ai_bin_get_type (void);
G_DEFINE_TYPE (AIBin, ai_bin, GST_TYPE_BIN);

static void
ai_bin_handle_message (GstBin * bin, GstMessage * message)
{
  AIBin *ai_bin = (AIBin *) bin;
  GstMessage *msg;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *error = NULL;
      gchar *dbg_info;

      gst_message_parse_error (message, &error, &dbg_info);
      msg = gst_message_new_warning (message->src, error, dbg_info);
//      gst_message_unref (message);
      g_mutex_lock (&ai_bin->wait->lock);
      ai_bin->wait->ready = TRUE;
      g_cond_broadcast (&ai_bin->wait->cond);
      g_mutex_unlock (&ai_bin->wait->lock);
      break;
    }
    default:
      msg = message;
      break;
  }

  GST_BIN_CLASS (ai_bin_parent_class)->handle_message (bin, msg);
}

static AIBin *
ai_bin_new (struct sample_load *wait)
{
  AIBin *bin = (AIBin *) g_object_new (ai_bin_get_type (), NULL);

  bin->wait = wait;

  return bin;
}

static void
ai_bin_class_init (AIBinClass * klass)
{
  GstBinClass *bin_class = (GstBinClass *) klass;

  bin_class->handle_message = ai_bin_handle_message;
}

static void
ai_bin_init (AIBin * bin)
{
}

static size_t
_ai_buffer_write (struct aiFile *file, const char *buffer, size_t size,
    size_t count)
{
  GST_FIXME ("implement writing");
  g_assert_not_reached ();
  return 0;
}

static size_t
_ai_buffer_read (struct aiFile *file, char *buffer, size_t size, size_t count)
{
  struct buffer_file_user_data *user =
      (struct buffer_file_user_data *) file->UserData;

  g_return_val_if_fail (user != NULL, 0);
  GST_TRACE ("%p reading %" G_GSIZE_FORMAT " bytes from offset %"
      G_GSIZE_FORMAT, file, size * count, user->file_cursor);

  g_return_val_if_fail (user->mapped == TRUE, 0);
  g_return_val_if_fail (user->map_info.maxsize >=
      user->file_cursor + size * count, 0);

  memcpy (buffer, &user->map_info.data[user->file_cursor], size * count);

  user->file_cursor += size * count;

  return size * count;
}

static size_t
_ai_buffer_tell (struct aiFile *file)
{
  struct buffer_file_user_data *user =
      (struct buffer_file_user_data *) file->UserData;
  GST_TRACE ("%p tell %" G_GSIZE_FORMAT, file, user->file_cursor);

  return user->file_cursor;
}

static size_t
_ai_buffer_size (struct aiFile *file)
{
  struct buffer_file_user_data *user =
      (struct buffer_file_user_data *) file->UserData;

  GST_TRACE ("%p size %" G_GSIZE_FORMAT, file, user->map_info.size);

  return user->map_info.size;
}

static void
_ai_buffer_flush (struct aiFile *file)
{
}

static aiReturn
_ai_buffer_seek (struct aiFile *file, size_t offset, enum aiOrigin origin)
{
  struct buffer_file_user_data *user =
      (struct buffer_file_user_data *) file->UserData;
  gsize current = user->file_cursor;

  if (origin == aiOrigin_SET)
    user->file_cursor = offset;
  else if (origin == aiOrigin_CUR)
    user->file_cursor += offset;
  else if (origin == aiOrigin_END)
    user->file_cursor = user->map_info.size - offset;
  else {
    GST_ERROR ("%p unknown origin", file);
    return aiReturn_FAILURE;
  }

  GST_TRACE ("%p seek from %" G_GSIZE_FORMAT " to %" G_GSIZE_FORMAT
      " using offset %" G_GSIZE_FORMAT, file, current, user->file_cursor,
      offset);

  return aiReturn_SUCCESS;
}

static void
_ai_buffer_free (struct buffer_file_user_data *data)
{
  if (!data)
    return;

  if (data->mapped)
    gst_buffer_unmap (data->buffer, &data->map_info);

  gst_buffer_unref (data->buffer);

  g_free (data);
}

static struct aiFile *
_ai_buffer_new (GstBuffer * buffer)
{
  struct buffer_file_user_data *data;
  struct aiFile *file;

  file = g_new0 (struct aiFile, 1);
  file->ReadProc = _ai_buffer_read;
  file->WriteProc = _ai_buffer_write;
  file->TellProc = _ai_buffer_tell;
  file->FileSizeProc = _ai_buffer_size;
  file->SeekProc = _ai_buffer_seek;
  file->FlushProc = _ai_buffer_flush;

  data = g_new0 (struct buffer_file_user_data, 1);
  data->base.destroy = (ai_base_close) _ai_buffer_free;
  data->buffer = gst_buffer_ref (buffer);
  data->file_cursor = 0;
  data->mapped = gst_buffer_map (buffer, &data->map_info, GST_MAP_READ);

  GST_TRACE ("%p new buffer %" GST_PTR_FORMAT " reader of size: %"
      G_GSIZE_FORMAT, file, buffer, data->map_info.size);

  file->UserData = (char *) data;

  return file;
}

static void
_decode_pad_added (GstElement * decoder, GstPad * new_pad,
    struct sample_load *wait)
{
  wait->link (wait, decoder, new_pad);
}

static void
_on_have_type (GstElement * typefind, guint probability, GstCaps * caps,
    struct sample_load *wait)
{
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const gchar *name = gst_structure_get_name (s);

  if (g_str_has_prefix (name, "video/") || g_str_has_prefix (name, "image/")
      || g_str_has_prefix (name, "audio/")) {
    wait->decoder = gst_element_factory_make ("decodebin", NULL);
    g_signal_connect (wait->decoder, "pad-added", (GCallback) _decode_pad_added,
        wait);
    gst_bin_add (GST_BIN (wait->bin), wait->decoder);
    gst_element_link (wait->typefind, wait->decoder);
    gst_element_sync_state_with_parent (wait->decoder);
  } else {
    gst_element_link (wait->typefind, wait->sink);
  }
}

static GstSample *
_wait_for_sample (GstElement * sink, struct sample_load *wait)
{
  g_mutex_lock (&wait->lock);
  while (!wait->ready)
    g_cond_wait (&wait->cond, &wait->lock);
  g_mutex_unlock (&wait->lock);

  GST_DEBUG_OBJECT (sink, "received sample %" GST_PTR_FORMAT, wait->sample);

  return wait->sample;
}

static GstFlowReturn
_sink_new_preroll (GstElement * sink, struct sample_load *wait)
{
  GstSample *sample;

  g_signal_emit_by_name (sink, "pull-preroll", &sample, NULL);

  g_mutex_lock (&wait->lock);
  g_cond_broadcast (&wait->cond);
  wait->sample = gst_sample_ref (sample);
  wait->ready = TRUE;
  g_mutex_unlock (&wait->lock);
  gst_sample_unref (sample);

  return GST_FLOW_EOS;
}

static GstSample *
_gst_sample_from_uri_full (GstAssimp * ai, const gchar * uri,
    struct sample_load *wait)
{
  GError *error = NULL;
  GstSample *sample;

  GST_INFO ("Attempting to open, %s", uri);

  wait->src = gst_element_make_from_uri (GST_URI_SRC, uri, NULL, &error);
  if (!wait->src) {
    GST_ERROR ("Failed to create source element for uri: %s", uri);
    return NULL;
  }

  wait->ai = ai;
  wait->sample = NULL;
  wait->ready = FALSE;
  g_mutex_init (&wait->lock);
  g_cond_init (&wait->cond);

  wait->typefind = gst_element_factory_make ("typefind", NULL);
  g_signal_connect (wait->typefind, "have-type", (GCallback) _on_have_type,
      wait);

  wait->sink = gst_element_factory_make ("appsink", NULL);
  g_signal_connect (wait->sink, "new-preroll", (GCallback) _sink_new_preroll,
      wait);
  g_object_set (G_OBJECT (wait->sink), "emit-signals", TRUE, NULL);

  wait->bin = GST_ELEMENT (ai_bin_new (wait));
  gst_element_set_locked_state (wait->bin, TRUE);
  gst_bin_add (GST_BIN (ai), wait->bin);
  gst_bin_add_many (GST_BIN (wait->bin), wait->src, wait->typefind, wait->sink,
      NULL);
  gst_element_link (wait->src, wait->typefind);

  if (!gst_element_set_state (wait->bin, GST_STATE_PLAYING)) {
    GST_ERROR_OBJECT (ai, "Failed to set pipeline for \'%s\' to playing", uri);
    return NULL;
  }

  sample = _wait_for_sample (wait->sink, wait);

  if (!gst_element_set_state (wait->bin, GST_STATE_NULL)) {
    GST_ERROR_OBJECT (ai, "Failed to shutdown pipeline for \'%s\'", uri);
    gst_sample_unref (sample);
    return NULL;
  }
  gst_bin_remove (GST_BIN (ai), wait->bin);

  g_cond_clear (&wait->cond);
  g_mutex_clear (&wait->lock);

  return sample;
}

static void
_default_link_decoded_pad (struct sample_load *load, GstElement * decoder,
    GstPad * new_pad)
{
  GstPad *sink_pad = gst_element_get_static_pad (load->sink, "sink");

  if (gst_pad_link (new_pad, sink_pad) != GST_PAD_LINK_OK)
    g_assert_not_reached ();
  gst_object_unref (sink_pad);
}

static GstSample *
_gst_sample_from_uri (GstAssimp * ai, const gchar * uri)
{
  struct sample_load wait = { 0, };

  wait.link = _default_link_decoded_pad;

  return _gst_sample_from_uri_full (ai, uri, &wait);
}

struct buffer_fileio_user_data
{
  GstAssimp *ai;
  GstBuffer *buffer;
};

static struct aiFile *
_ai_buffer_open (struct aiFileIO *fileio, const char *filename,
    const char *access)
{
  struct buffer_fileio_user_data *data =
      (struct buffer_fileio_user_data *) fileio->UserData;

  GST_TRACE_OBJECT (data->ai, "%p opening: %s with access: %s", fileio,
      filename, access);

  if (g_strcmp0 (filename, "gst.buffer") == 0) {
    return _ai_buffer_new (GST_BUFFER (data->buffer));
  } else {
    GstSample *sample;
    struct aiFile *file = NULL;
    gchar *fullpath;

    if (!g_path_is_absolute (filename)) {
      gchar *dirname = g_path_get_dirname (data->ai->obj_uri);
      fullpath = g_build_filename (dirname, filename, NULL);
      g_free (dirname);
    } else {
      fullpath = g_strdup (filename);
    }

    sample = _gst_sample_from_uri (data->ai, fullpath);
    if (sample) {
      GstBuffer *buffer = gst_sample_get_buffer (sample);
      file = _ai_buffer_new (gst_buffer_ref (buffer));
      gst_sample_unref (sample);
    } else {
      GST_WARNING_OBJECT (data->ai, "Failed to open path: %s", fullpath);
    }
    g_free (fullpath);

    return file;
  }
}

static void
_ai_buffer_close (struct aiFileIO *fileio, struct aiFile *file)
{
  struct buffer_fileio_user_data *io_data =
      (struct buffer_fileio_user_data *) fileio->UserData;

  if (file) {
    struct ai_base *data = (struct ai_base *) file->UserData;

    if (data && data->destroy)
      data->destroy (data);

    GST_TRACE_OBJECT (io_data->ai, "%p closing file %p", fileio, file);

    g_free (file);
  }
}

static gboolean gst_assimp_handle_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_assimp_handle_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

GType gst_assimp_src_pad_get_type (void);
G_DEFINE_TYPE (GstAssimpSrcPad, gst_assimp_src_pad, GST_TYPE_PAD);

static void
gst_assimp_src_pad_constructed (GObject * object)
{
  GstPad *pad = GST_PAD (object);

  gst_pad_set_event_full_function_full (pad,
      GST_DEBUG_FUNCPTR (gst_assimp_handle_src_event), NULL, NULL);
  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_assimp_handle_src_query));
}

static void
gst_assimp_src_pad_finalize (GObject * object)
{
//  GstAssimpSrcPad *pad = (GstAssimpSrcPad *) object;

  G_OBJECT_CLASS (gst_assimp_src_pad_parent_class)->finalize (object);
}

static void
gst_assimp_src_pad_dispose (GObject * object)
{
//  GstAssimpSrcPad *pad = (GstAssimpSrcPad *) object;

  G_OBJECT_CLASS (gst_assimp_src_pad_parent_class)->dispose (object);
}

static void
gst_assimp_src_pad_class_init (GstAssimpSrcPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_assimp_src_pad_constructed;
  gobject_class->finalize = gst_assimp_src_pad_finalize;
  gobject_class->dispose = gst_assimp_src_pad_dispose;
}

static void
gst_assimp_src_pad_init (GstAssimpSrcPad * pad)
{
  pad->send_stream_start = TRUE;
  pad->send_segment = TRUE;
  pad->send_tags = TRUE;
  gst_segment_init (&pad->segment, GST_FORMAT_TIME);
}

static GObjectClass *parent_class = NULL;
static void gst_assimp_init (GstAssimp * ai, GstAssimpClass * klass);
static void gst_assimp_class_init (GstAssimpClass * klass);

/* we can't use G_DEFINE_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_assimp_get_type (void)
{
  static volatile gsize assimp_type = 0;

  if (g_once_init_enter (&assimp_type)) {
    GType _type;
    static const GTypeInfo assimp_info = {
      sizeof (GstAssimpClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_assimp_class_init,
      NULL,
      NULL,
      sizeof (GstAssimp),
      0,
      (GInstanceInitFunc) gst_assimp_init,
    };

    _type = g_type_register_static (GST_TYPE_BIN, "GstAssimp", &assimp_info, 0);
    GST_DEBUG_CATEGORY_INIT (gst_assimp_debug, "glassimp", 0,
        "Assimp model loader");
    g_once_init_leave (&assimp_type, _type);
  }
  return assimp_type;
}

static void gst_assimp_finalize (GObject * object);
static void gst_assimp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_assimp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_assimp_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_assimp_sink_activate (GstPad * sinkpad, GstObject * parent);
static gboolean gst_assimp_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);
static GstFlowReturn gst_assimp_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_assimp_handle_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_assimp_handle_sink_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static void gst_assimp_loop (gpointer data);
static GstFlowReturn _gst_assimp_generate_model (GstAssimp * ai,
    GstBuffer * buffer);
static GstFlowReturn _gst_assimp_render_scene (GstAssimp * ai);

static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    /* FIXME: define some caps for encoded models */
    GST_STATIC_CAPS_ANY);

#define MODEL_CAPS \
    "application/3d-model, "                                                \
    "primitive-mode = (string) triangles , "                                \
    "index-format = (string) U16LE, "                                       \
    "shading = (string) { " GST_3D_MATERIAL_SHADING_ALL " } "

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
_create_vertices_pad_template_caps (void)
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
gst_assimp_class_init (GstAssimpClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstCaps *src_caps_templ = _create_vertices_pad_template_caps ();

  parent_class = g_type_class_peek_parent (klass);
  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_assimp_finalize;
  gobject_class->set_property = gst_assimp_set_property;
  gobject_class->get_property = gst_assimp_get_property;

  element_class->change_state = gst_assimp_change_state;

  gst_element_class_add_static_pad_template (element_class, &sink_templ);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          src_caps_templ));
  gst_caps_unref (src_caps_templ);

  gst_element_class_set_metadata (element_class,
      "Assimp 3D model loader", "Decoder/3D/Model",
      "Load 3D models using Assimp",
      "Matthew Waters <matthew@centricular.com>");
}

static void
_on_ai_log (const gchar * v1, gchar * v2)
{
  GstAssimp *ai = (GstAssimp *) v2;

  GST_DEBUG_OBJECT (ai, "%s", v1);
}

static GstSample *
_create_null_sample (GstAssimp * ai)
{
  GstVideoInfo v_info;
  GstCaps *caps;
  GstBuffer *buffer;
  GstMapInfo map_info;
  GstSample *sample;

  gst_video_info_init (&v_info);
  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 1, 1);
  caps = gst_video_info_to_caps (&v_info);
  buffer = gst_buffer_new_allocate (NULL, 4, NULL);

  gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
  memset (map_info.data, 0, map_info.size);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  map_info.data[0] = 0xff;
#else
  map_info.data[3] = 0xff;
#endif
  gst_buffer_unmap (buffer, &map_info);

  sample = gst_sample_new (buffer, caps, NULL, NULL);

  gst_buffer_unref (buffer);
  gst_caps_unref (caps);

  return sample;
}

static void
gst_assimp_init (GstAssimp * ai, GstAssimpClass * klass)
{
  GstPadTemplate *src_templ;

  ai->log_stream.callback = (aiLogStreamCallback) _on_ai_log;
  ai->log_stream.user = (char *) ai;
  aiAttachLogStream (&ai->log_stream);
  aiEnableVerboseLogging (AI_TRUE);

  ai->sinkpad = gst_pad_new_from_static_template (&sink_templ, "sink");
  gst_pad_set_activate_function (ai->sinkpad,
      GST_DEBUG_FUNCPTR (gst_assimp_sink_activate));
  gst_pad_set_activatemode_function (ai->sinkpad,
      GST_DEBUG_FUNCPTR (gst_assimp_sink_activate_mode));
  gst_pad_set_chain_function (ai->sinkpad,
      GST_DEBUG_FUNCPTR (gst_assimp_chain));
  gst_pad_set_event_function (ai->sinkpad,
      GST_DEBUG_FUNCPTR (gst_assimp_handle_sink_event));
  gst_pad_set_query_function (ai->sinkpad,
      GST_DEBUG_FUNCPTR (gst_assimp_handle_sink_query));
  gst_element_add_pad (GST_ELEMENT (ai), ai->sinkpad);

  src_templ =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  ai->srcpad = g_object_new (gst_assimp_src_pad_get_type (), "name",
      "src", "direction", GST_PAD_SRC, "template", src_templ, NULL);
  gst_pad_set_event_function (ai->srcpad,
      GST_DEBUG_FUNCPTR (gst_assimp_handle_src_event));
  gst_pad_set_query_function (ai->srcpad,
      GST_DEBUG_FUNCPTR (gst_assimp_handle_src_query));
  gst_element_add_pad (GST_ELEMENT (ai), ai->srcpad);

  ai->adapter = gst_adapter_new ();
  gst_3d_vertex_info_init (&ai->out_vertex_info, 4);
  ai->out_vertex_info.primitive_mode = GST_3D_VERTEX_PRIMITIVE_MODE_TRIANGLES;
  ai->out_vertex_info.index_finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_U16);
  ai->out_vertex_info.max_vertices = -1;
  ai->out_vertex_info.attributes[0].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  ai->out_vertex_info.attributes[0].type = GST_3D_VERTEX_TYPE_POSITION;
  ai->out_vertex_info.attributes[0].channels = 3;
  ai->out_vertex_info.attributes[1].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  ai->out_vertex_info.attributes[1].type = GST_3D_VERTEX_TYPE_COLOR;
  ai->out_vertex_info.attributes[1].channels = 4;
  ai->out_vertex_info.attributes[2].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  ai->out_vertex_info.attributes[2].type = GST_3D_VERTEX_TYPE_NORMAL;
  ai->out_vertex_info.attributes[2].channels = 3;
  ai->out_vertex_info.attributes[3].finfo =
      gst_3d_vertex_format_get_info (GST_3D_VERTEX_FORMAT_F32);
  ai->out_vertex_info.attributes[3].type = GST_3D_VERTEX_TYPE_TEXTURE;
  ai->out_vertex_info.attributes[3].channels = 2;
  ai->out_vertex_info.offsets[0] = 0;
  ai->out_vertex_info.offsets[1] = ai->out_vertex_info.offsets[0] +
      ai->out_vertex_info.attributes[0].channels *
      ai->out_vertex_info.attributes[0].finfo->width / 8;
  ai->out_vertex_info.offsets[2] = ai->out_vertex_info.offsets[1] +
      ai->out_vertex_info.attributes[1].channels *
      ai->out_vertex_info.attributes[1].finfo->width / 8;
  ai->out_vertex_info.offsets[3] = ai->out_vertex_info.offsets[2] +
      ai->out_vertex_info.attributes[2].channels *
      ai->out_vertex_info.attributes[2].finfo->width / 8;
  ai->out_vertex_info.strides[0] = ai->out_vertex_info.strides[1] =
      ai->out_vertex_info.strides[2] = ai->out_vertex_info.strides[3] =
      sizeof (struct GstAIVertex);
  gst_3d_material_info_init (&ai->out_material_info);

  ai->null_sample = _create_null_sample (ai);
}

static void
gst_assimp_finalize (GObject * object)
{
  GstAssimp *ai = GST_ASSIMP (object);

  aiDetachLogStream (&ai->log_stream);

  gst_3d_vertex_info_unset (&ai->out_vertex_info);
  gst_3d_material_info_unset (&ai->out_material_info);

  if (ai->adapter) {
    gst_object_unref (ai->adapter);
    ai->adapter = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_assimp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_assimp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_assimp_handle_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
//  GstAssimp *ai = GST_ASSIMP (parent);

  switch (GST_QUERY_TYPE (query)) {
    default:
      break;
  }

  return gst_pad_query_default (pad, parent, query);
}

static gboolean
gst_assimp_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  GstAssimp *ai = GST_ASSIMP (parent);
  GstQuery *query;
  gboolean pull_mode = FALSE;

  query = gst_query_new_scheduling ();

  if (gst_pad_peer_query (sinkpad, query))
    pull_mode = gst_query_has_scheduling_mode (query, GST_PAD_MODE_PULL);

  gst_query_unref (query);

  if (pull_mode) {
    GST_DEBUG_OBJECT (ai, "going to pull mode");
    ai->streaming = FALSE;
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);
  } else {
    GST_DEBUG_OBJECT (ai, "going to push (streaming) mode");
    ai->streaming = TRUE;
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }
}

static gboolean
gst_assimp_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  switch (mode) {
    case GST_PAD_MODE_PULL:
      if (active) {
        gst_pad_start_task (sinkpad, (GstTaskFunction) gst_assimp_loop,
            sinkpad, NULL);
      } else {
        gst_pad_stop_task (sinkpad);
      }
      return TRUE;
    case GST_PAD_MODE_PUSH:
      return TRUE;
    default:
      return FALSE;
  }
}

static GstFlowReturn
gst_assimp_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstAssimp *ai = GST_ASSIMP (parent);

  if (G_UNLIKELY (GST_BUFFER_IS_DISCONT (buffer))) {
    GST_DEBUG_OBJECT (ai, "got DISCONT");
    gst_adapter_clear (ai->adapter);
    GST_OBJECT_LOCK (ai);
    /* FIXME: reset scene */
    GST_OBJECT_UNLOCK (ai);
  }

  gst_adapter_push (ai->adapter, buffer);

  return GST_FLOW_OK;
}

static GstFlowReturn
_on_eos (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAssimp *ai = GST_ASSIMP (parent);
  GstQuery *query;
  gboolean query_res;
  gsize available;
  GstBuffer *buffer;
  GstFlowReturn ret;

  available = gst_adapter_available (ai->adapter);
  if (available == 0) {
    GST_WARNING_OBJECT (ai, "Received EOS without any data.");
    gst_pad_event_default (pad, parent, event);
    return GST_FLOW_FLUSHING;
  }

  /* Need to get the URI to use it as a base to generate the models's
   * uris */
  query = gst_query_new_uri ();
  query_res = gst_pad_peer_query (pad, query);
  if (query_res) {
    gchar *uri, *redirect_uri;
    gboolean permanent;

    gst_query_parse_uri (query, &uri);
    gst_query_parse_uri_redirection (query, &redirect_uri);
    gst_query_parse_uri_redirection_permanent (query, &permanent);

    if (permanent && redirect_uri) {
      ai->obj_uri = redirect_uri;
      ai->obj_base_uri = NULL;
      g_free (uri);
    } else {
      ai->obj_uri = uri;
      ai->obj_base_uri = redirect_uri;
    }

    GST_DEBUG_OBJECT (ai, "Fetched URI: %s (base: %s)",
        ai->obj_uri, GST_STR_NULL (ai->obj_base_uri));
  } else {
    GST_WARNING_OBJECT (ai, "Upstream URI query failed.");
  }
  gst_query_unref (query);

  buffer = gst_adapter_take_buffer (ai->adapter, available);

  ret = _gst_assimp_generate_model (ai, buffer);

  gst_buffer_unref (buffer);
  gst_event_unref (event);
  return ret;
}

static gboolean
gst_assimp_handle_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAssimp *ai = GST_ASSIMP (parent);
  gboolean res = TRUE;

  GST_DEBUG_OBJECT (ai, "have event type %s: %p on sink pad",
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      _on_eos (pad, parent, event);
      return TRUE;
    case GST_EVENT_SEGMENT:
      /* Swallow segments, we'll push our own */
      gst_event_unref (event);
      return TRUE;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static gboolean
gst_assimp_handle_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
//  GstAssimp *ai = GST_ASSIMP (parent);

  switch (GST_QUERY_TYPE (query)) {
    default:
      break;
  }

  return gst_pad_query_default (pad, parent, query);
}

static gboolean
gst_assimp_handle_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
//  GstAssimp *ai = GST_ASSIMP (parent);
  gboolean res = TRUE;

  GST_DEBUG_OBJECT (pad, "have event type %s: %p on src pad",
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static void
gst_assimp_loop (gpointer data)
{
  GstAssimp *ai = GST_ASSIMP (GST_OBJECT_PARENT (data));
  GstBuffer *buffer = NULL;
  GstFlowReturn ret;

  ret = gst_pad_pull_range (ai->sinkpad, ai->offset, BLOCKSIZE, &buffer);
  if (buffer)
    ai->offset += gst_buffer_get_size (buffer);

  if (ret == GST_FLOW_OK) {
    gst_adapter_push (ai->adapter, buffer);
    return;
  } else if (ret == GST_FLOW_EOS) {
    if ((ret =
            _on_eos (ai->sinkpad, GST_OBJECT (ai),
                gst_event_new_eos ())) < GST_FLOW_OK)
      goto beach;
  } else {
    GST_ERROR_OBJECT (ai, "Upstream returned %s", gst_flow_get_name (ret));
  }

beach:
  gst_pad_pause_task (ai->sinkpad);
}

static void
_deinit_scene (GstAssimp * ai)
{
  guint i;

  if (ai->meshes) {
    for (i = 0; i < ai->meshes->len; i++) {
      struct GstAIMeshNode *node =
          &g_array_index (ai->meshes, struct GstAIMeshNode, i);

      if (node->vertex_buffer) {
        gst_memory_unref (GST_MEMORY_CAST (node->vertex_buffer));
        node->vertex_buffer = NULL;
      }

      if (node->index_buffer) {
        gst_memory_unref (GST_MEMORY_CAST (node->index_buffer));
        node->index_buffer = NULL;
      }
    }

    g_array_free (ai->meshes, TRUE);
    ai->meshes = NULL;
  }

  if (ai->materials) {
/*
    for (i = 0; i < ai->materials->len; i++) {
      struct GstAIMaterial *material =
          &g_array_index (ai->materials, struct GstAIMaterial, i);

      if (material->sample) {
        gst_sample_unref (texture->sample);
        texture->sample = NULL;
      }
    }*/

    g_array_free (ai->materials, TRUE);
    ai->materials = NULL;
  }

  if (ai->out_buffer) {
    gst_buffer_unref (ai->out_buffer);
    ai->out_buffer = NULL;
  }

  gst_segment_init (&ai->segment, GST_FORMAT_TIME);

  if (ai->scene) {
    aiReleaseImport (ai->scene);
    ai->scene = NULL;
  }
}

static const gchar *
_tex_type_string (enum aiTextureType type)
{
  switch (type) {
    case aiTextureType_NONE:
      return "none";
    case aiTextureType_DIFFUSE:
      return "diffuse";
    case aiTextureType_SPECULAR:
      return "specular";
    case aiTextureType_AMBIENT:
      return "ambient";
    case aiTextureType_EMISSIVE:
      return "emissive";
    case aiTextureType_HEIGHT:
      return "height";
    case aiTextureType_NORMALS:
      return "normal";
    case aiTextureType_SHININESS:
      return "shininess";
    case aiTextureType_OPACITY:
      return "opacity";
    case aiTextureType_DISPLACEMENT:
      return "displacement";
    case aiTextureType_LIGHTMAP:
      return "lightmap";
    case aiTextureType_REFLECTION:
      return "reflection";
    default:
      return "unknown";
  }
}

static gboolean
_load_texture (GstAssimp * ai, struct GstAIMaterial *material,
    enum aiTextureType type, gint idx, struct GstAITexture *texture)
{
  struct aiString path;
  gchar *dirname;

  dirname = g_path_get_dirname (ai->obj_uri);

  texture->type = type;
  texture->multiplier = 1.;
  texture->operation = aiTextureOp_Add;
  texture->flags = 0;
  texture->wrapping[0] = aiTextureMapMode_Wrap;
  texture->wrapping[1] = aiTextureMapMode_Wrap;
  texture->mapping = aiTextureMapping_UV;
  texture->uv_index = 0;
  texture->axis.x = 1.;
  texture->axis.y = 0.;
  texture->axis.z = 0.;

  if (aiGetMaterialTexture (material->mat, type, idx, &path, &texture->mapping,
          &texture->uv_index, &texture->multiplier, &texture->operation,
          texture->wrapping, &texture->flags) == AI_SUCCESS) {
    gchar *path2, **split;
    gchar *new_path;

    /* TODO: only load textures referenced multiple times in the materials once */
    /* TODO: possibly load textures from the model instead of from a file */

    /* what is this windows path nonsense!? */
    /* FIXME: perform better path reconstruction */
    split = g_strsplit (path.data, "\\", -1);
    path2 = g_strjoinv ("/", split);
    g_strfreev (split);
    new_path = g_build_filename (dirname, path2, NULL);
    g_free (path2);

    GST_LOG_OBJECT (ai, "have material %p, %s texture index %u path %s "
        "multiplier %f, operation 0x%x, flags 0x%x wrap_u 0x%x, wrap_v 0x%x, "
        "mapping 0x%x, uv index %u", material, _tex_type_string (type), idx,
        new_path, texture->multiplier, texture->operation, texture->flags,
        texture->wrapping[0], texture->wrapping[1], texture->mapping,
        texture->uv_index);

    texture->sample = _gst_sample_from_uri (ai, new_path);
    if (!texture->sample)
      GST_ELEMENT_WARNING (ai, RESOURCE, NOT_FOUND,
          (NULL), ("Failed to open path: %s", new_path));
    g_free (new_path);
    /* ignore errors
       } else {
       return FALSE;
     */
  }

  /* Load a white texture in case the model does not include its own texture */
  if (!texture->sample) {
    GST_DEBUG ("using dummy texture for material %p", material);
    texture->sample = gst_sample_ref (ai->null_sample);
  } else {
    GST_DEBUG ("have sample %" GST_PTR_FORMAT " for material %p",
        texture->sample, material);
  }

  return TRUE;
}

static gboolean
_load_textures (GstAssimp * ai, struct GstAIMaterial *material,
    enum aiTextureType type, struct GstAITextureList *texture)
{
  gint n_textures, i;

  n_textures = aiGetMaterialTextureCount (material->mat, type);
  GST_DEBUG_OBJECT (ai, "material %p has %i %s textures", material->mat,
      n_textures, _tex_type_string (type));

  texture->n_textures = n_textures;
  texture->textures = g_new0 (struct GstAITexture, texture->n_textures);
  for (i = 0; i < n_textures; i++) {
    if (!_load_texture (ai, material, type, i, &texture->textures[i]))
      return FALSE;
  }

  return TRUE;
}

static gboolean
_load_color3d (GstAssimp * ai, struct GstAIMaterial *material,
    const gchar * key, unsigned int type, unsigned int index, struct Vec3F *ret)
{
  struct aiColor4D color = { 0., 0., 0., 1. };
  enum aiReturn res;

  res = aiGetMaterialColor (material->mat, key, type, index, &color);

  ret->x = color.r;
  ret->y = color.g;
  ret->z = color.b;

  return res == AI_SUCCESS;
}

static gboolean
_load_float (GstAssimp * ai, struct GstAIMaterial *material,
    const gchar * key, unsigned int type, unsigned int index, float *ret,
    float default_value)
{
  *ret = default_value;
  return AI_SUCCESS == aiGetMaterialFloatArray (material->mat, key, type, index,
      ret, NULL);
}

static gboolean
_load_int (GstAssimp * ai, struct GstAIMaterial *material,
    const gchar * key, unsigned int type, unsigned int index, int *ret,
    int default_value)
{
  *ret = default_value;
  return AI_SUCCESS == aiGetMaterialIntegerArray (material->mat, key, type,
      index, ret, NULL);
}

static gboolean
_load_material (GstAssimp * ai, const struct aiMaterial *mat,
    struct GstAIMaterial *material)
{
  memset (material, 0, sizeof (*material));

  material->mat = mat;
  material->elements = 0;

  _load_int (ai, material, AI_MATKEY_SHADING_MODEL,
      (int *) &material->shading_model, (int) aiShadingMode_Gouraud);
  {
    GstAIMaterialFlags flags = 0;
    int val;

    _load_int (ai, material, AI_MATKEY_TWOSIDED, &val, 0);
    if (val)
      flags |= MATERIAL_TWO_SIDED;

    _load_int (ai, material, AI_MATKEY_ENABLE_WIREFRAME, &val, 0);
    if (val)
      flags |= MATERIAL_WIREFRAME;

    material->flags = flags;
  }

#define LOAD_TEXTURES(ai_type, gst_type, textures_) \
  if (!_load_textures (ai, material, aiTextureType_ ## ai_type, &material->textures_)) \
    return FALSE; \
  if (material->textures_.n_textures) \
    material->elements |= GST_3D_MATERIAL_ELEMENT_ ## gst_type;

#define LOAD_BASE_COLOR3_TEXTURES(ai_type, gst_type,textures_) \
  if (_load_color3d (ai, material, AI_MATKEY_COLOR_ ## ai_type , &material->textures_.color)) \
    material->elements |= GST_3D_MATERIAL_ELEMENT_ ## gst_type; \
  LOAD_TEXTURES(ai_type, gst_type, textures_.textures)

  LOAD_BASE_COLOR3_TEXTURES (DIFFUSE, DIFFUSE, diffuse)
      LOAD_BASE_COLOR3_TEXTURES (SPECULAR, SPECULAR, specular)
      LOAD_BASE_COLOR3_TEXTURES (AMBIENT, AMBIENT, ambient)
      LOAD_BASE_COLOR3_TEXTURES (EMISSIVE, EMISSIVE, emissive)
      LOAD_TEXTURES (HEIGHT, HEIGHT_MAP, height_maps)
      LOAD_TEXTURES (NORMALS, NORMAL_MAP, normal_maps)
      LOAD_TEXTURES (LIGHTMAP, LIGHT_MAP, light_maps)
      LOAD_TEXTURES (SHININESS, SHININESS, shininess_textures)
      LOAD_TEXTURES (OPACITY, OPACITY, opacity_textures)

      if (_load_float (ai, material, AI_MATKEY_SHININESS, &material->shininess,
          0.))
    material->elements |= GST_3D_MATERIAL_ELEMENT_SHININESS;
  if (_load_float (ai, material, AI_MATKEY_SHININESS_STRENGTH,
          &material->shininess_strength, 1.))
    material->elements |= GST_3D_MATERIAL_ELEMENT_SHININESS;
  if (_load_float (ai, material, AI_MATKEY_OPACITY, &material->opacity, 1.))
    material->elements |= GST_3D_MATERIAL_ELEMENT_OPACITY;

#if 0
  /* TODO displacement, reflection */
  LOAD_TEXTURES (DISPLACEMENT, NONE, displacments)
      LOAD_TEXTURES (REFLECTIONS, NONE, reflections)
      if (_load_float (ai, material, AI_MATKEY_REFLECTIVITY,
          &material->reflectivity, 0.))
    material->elements |= GST_3D_MATERIAL_ELEMENT_NONE;
  if (_load_float (ai, material, AI_MATKEY_REFRACTI,
          &material->refractive_index, 0.))
    material->elements |= GST_3D_MATERIAL_ELEMENT_NONE;
#endif

  return TRUE;
}

static void
_mangle_material_uvwsrc (GstAssimp * ai, struct GstAIMaterial *material,
    guint n_uv_channels)
{
  int i;

#define ASSIGN(list) \
  for (i = 0; i < list.n_textures; i++) {                               \
    if (n_uv_channels <= 0) {                                           \
      list.textures[i].uv_index = 0;     /* assign the only channel */  \
    } else if (list.textures[i].uv_index == -1) {   /* no uvwsrc */     \
      list.textures[i].uv_index = i;                       \
    }                                                                   \
  }

  ASSIGN (material->diffuse.textures)
      ASSIGN (material->specular.textures)
      ASSIGN (material->ambient.textures)
      ASSIGN (material->emissive.textures)
      ASSIGN (material->height_maps)
      ASSIGN (material->normal_maps)
      ASSIGN (material->light_maps)
      ASSIGN (material->shininess_textures)
      ASSIGN (material->opacity_textures)
      /* TODO
         ASSIGN (material->displacements)
         ASSIGN (material->reflections) */
#undef ASSIGN
}

static Gst3DMaterialShading
_ai_shading_to_gst_shading (enum aiShadingMode mode)
{
  switch (mode) {
    case aiShadingMode_NoShading:
      return GST_3D_MATERIAL_SHADING_NONE;
    case aiShadingMode_Flat:
      return GST_3D_MATERIAL_SHADING_FLAT;
    case aiShadingMode_Gouraud:
      return GST_3D_MATERIAL_SHADING_GOURAUD;
    case aiShadingMode_Phong:
      return GST_3D_MATERIAL_SHADING_PHONG;
    case aiShadingMode_Blinn:
      return GST_3D_MATERIAL_SHADING_PHONG_BLINN;
    case aiShadingMode_Toon:
      return GST_3D_MATERIAL_SHADING_TOON;
    case aiShadingMode_OrenNayar:
      return GST_3D_MATERIAL_SHADING_OREN_NAYAR;
    case aiShadingMode_Minnaert:
      return GST_3D_MATERIAL_SHADING_MINNAERT;
    case aiShadingMode_CookTorrance:
      return GST_3D_MATERIAL_SHADING_COOK_TORRANCE;
    case aiShadingMode_Fresnel:
      return GST_3D_MATERIAL_SHADING_FRESNEL;
    default:
      GST_WARNING ("unknown shading mode 0x%x", mode);
      return GST_3D_MATERIAL_SHADING_UNKNOWN;
  }
}

static struct GstAITextureList *
_ai_texture_list_from_material (struct GstAIMaterial *mat,
    Gst3DMaterialElement element)
{
  switch (element) {
    case GST_3D_MATERIAL_ELEMENT_DIFFUSE:
      return &mat->diffuse.textures;
    case GST_3D_MATERIAL_ELEMENT_SPECULAR:
      return &mat->specular.textures;
    case GST_3D_MATERIAL_ELEMENT_AMBIENT:
      return &mat->ambient.textures;
    case GST_3D_MATERIAL_ELEMENT_EMISSIVE:
      return &mat->emissive.textures;
    case GST_3D_MATERIAL_ELEMENT_HEIGHT_MAP:
      return &mat->height_maps;
    case GST_3D_MATERIAL_ELEMENT_NORMAL_MAP:
      return &mat->normal_maps;
    case GST_3D_MATERIAL_ELEMENT_LIGHT_MAP:
      return &mat->light_maps;
    case GST_3D_MATERIAL_ELEMENT_SHININESS:
      return &mat->shininess_textures;
    case GST_3D_MATERIAL_ELEMENT_OPACITY:
      return &mat->opacity_textures;
    default:
      GST_WARNING ("unhandled element 0x%x", element);
      g_assert_not_reached ();
      return NULL;
  }
}

static Gst3DMaterialCombineFunction
_ai_operation_to_combiner (enum aiTextureOp op)
{
  switch (op) {
    case aiTextureOp_Multiply:
      return GST_3D_MATERIAL_COMBINE_MULTIPLY;
    case aiTextureOp_Add:
      return GST_3D_MATERIAL_COMBINE_ADD;
    case aiTextureOp_Subtract:
    case aiTextureOp_Divide:
    case aiTextureOp_SmoothAdd:
    case aiTextureOp_SignedAdd:
    default:
      g_assert_not_reached ();
      return GST_3D_MATERIAL_COMBINE_OVERWRITE;
  }
}

static Gst3DMaterialTextureWrapMode
_ai_wrap_to_gst (enum aiTextureMapMode mode)
{
  switch (mode) {
    case aiTextureMapMode_Wrap:
      return GST_3D_MATERIAL_WRAP_MODE_WRAP;
    case aiTextureMapMode_Clamp:
      return GST_3D_MATERIAL_WRAP_MODE_CLAMP;
    case aiTextureMapMode_Decal:
      return GST_3D_MATERIAL_WRAP_MODE_DECAL;
    case aiTextureMapMode_Mirror:
      return GST_3D_MATERIAL_WRAP_MODE_MIRROR;
    default:
      g_assert_not_reached ();
      return GST_3D_MATERIAL_WRAP_MODE_NONE;
  }
}

static gboolean
_add_textures_to_stack (Gst3DMaterialStack * stack, struct GstAIMaterial *mat)
{
  struct GstAITextureList *list;
  int i;

  list = _ai_texture_list_from_material (mat, stack->type);
  gst_3d_material_stack_set_n_textures (stack, list->n_textures);

  for (i = 0; i < list->n_textures; i++) {
    struct GstAITexture *src = &list->textures[i];
    Gst3DMaterialTexture *dst = gst_3d_material_stack_get_texture (stack, i);

    dst->multiplier = src->multiplier;
    dst->combiner = _ai_operation_to_combiner (src->operation);
    dst->flags = 0;
    if (src->flags & aiTextureFlags_Invert)
      dst->flags |= GST_3D_MATERIAL_TEXTURE_FLAG_INVERT;
    if (src->flags & aiTextureFlags_IgnoreAlpha)
      dst->flags |= GST_3D_MATERIAL_TEXTURE_FLAG_IGNORE_ALPHA;
    if (src->flags & aiTextureFlags_UseAlpha)
      dst->flags |= GST_3D_MATERIAL_TEXTURE_FLAG_USE_ALPHA;
    dst->wrap_mode = _ai_wrap_to_gst (src->wrapping[0]);
    dst->sample = gst_sample_ref (src->sample);
  }

  return TRUE;
}

static void
_ai_material_to_gst_info (GstAssimp * ai, struct GstAIMaterial *mat,
    Gst3DMaterialInfo * minfo)
{
  gst_3d_material_info_init (minfo);
  minfo->shading = _ai_shading_to_gst_shading (mat->shading_model);
  minfo->elements = mat->elements;
  minfo->caps = gst_caps_from_string ("video/x-raw");
}

static Gst3DMaterialMeta *
_add_material_meta_to_buffer (GstAssimp * ai, gint id,
    struct GstAIMaterial *mat, GstBuffer * buffer, Gst3DMaterialInfo * out_info)
{
  Gst3DMaterialMeta *meta;
  int i = 1;

  _ai_material_to_gst_info (ai, mat, out_info);
  meta = gst_buffer_add_3d_material_meta (buffer, id, out_info);

  while (i < meta->info.elements) {
    if (i & meta->info.elements) {
      Gst3DMaterialStack *stack = gst_3d_material_meta_get_stack (meta, i);
      if (!stack)
        return NULL;

      if (!_add_textures_to_stack (stack, mat))
        return NULL;
    }
    i <<= 1;
  }

  return meta;
}

static gboolean
_init_scene (GstAssimp * ai, GstBuffer * input)
{
  struct buffer_fileio_user_data user_data = { 0, };
  struct aiFileIO fileio = { 0, };
  float max_coord = -9999999., min_coord = -max_coord, offset, scale;
  guint i, j;

  user_data.ai = ai;
  user_data.buffer = input;

  fileio.OpenProc = _ai_buffer_open;
  fileio.CloseProc = _ai_buffer_close;
  fileio.UserData = (char *) &user_data;

  _deinit_scene (ai);

  ai->scene =
      aiImportFileEx ("gst.buffer",
      aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_GenUVCoords
      | aiProcess_FlipUVs, &fileio);
  if (!ai->scene) {
    GST_ELEMENT_ERROR (ai, LIBRARY, INIT,
        ("%s", "Failed to create scene from buffer"),
        ("%s", aiGetErrorString ()));
    return FALSE;
  }

  if (ai->scene->mNumMeshes != 1) {
    GST_ELEMENT_WARNING (ai, STREAM, NOT_IMPLEMENTED,
        ("%s", "More than one mesh in scene"), (NULL));
  }

  GST_DEBUG_OBJECT (ai, "scene has %i materials", ai->scene->mNumMaterials);

  ai->materials =
      g_array_sized_new (FALSE, TRUE, sizeof (struct GstAIMaterial),
      ai->scene->mNumMaterials);
  ai->materials->len = ai->scene->mNumMaterials;

  /* Initialize the materials */
  for (i = 0; i < ai->scene->mNumMaterials; i++) {
    const struct aiMaterial *mat = ai->scene->mMaterials[i];
    struct GstAIMaterial *material =
        &g_array_index (ai->materials, struct GstAIMaterial, i);

    if (!_load_material (ai, mat, material))
      return FALSE;
  }

  GST_INFO_OBJECT (ai, "scene has %d meshes", ai->scene->mNumMeshes);

  ai->out_buffer = gst_buffer_new ();

  ai->meshes =
      g_array_sized_new (FALSE, TRUE, sizeof (struct GstAIMeshNode),
      ai->scene->mNumMeshes);
  ai->meshes->len = ai->scene->mNumMeshes;

  for (i = 0; i < ai->scene->mNumMeshes; i++) {
    const struct aiMesh *mesh = ai->scene->mMeshes[i];
    struct GstAIMaterial *material =
        &g_array_index (ai->materials, struct GstAIMaterial,
        mesh->mMaterialIndex);
    struct GstAIMeshNode *node;
    GstMapInfo map_info;
    guint n_uv_channels = 0;

    node = &g_array_index (ai->meshes, struct GstAIMeshNode, i);
    node->material_index = mesh->mMaterialIndex;

    node->vertex_buffer =
        gst_allocator_alloc (NULL,
        sizeof (struct GstAIVertex) * mesh->mNumVertices, NULL);

    if (!gst_memory_map (GST_MEMORY_CAST (node->vertex_buffer), &map_info,
            GST_MAP_WRITE)) {
      GST_ELEMENT_ERROR (ai, LIBRARY, INIT, ("%s", "Failed to map memory"),
          (NULL));
      _deinit_scene (ai);
      return FALSE;
    }

    GST_INFO_OBJECT (ai, "mesh %d has %d vertices", i, mesh->mNumVertices);
    node->num_vertices = mesh->mNumVertices;

    while (n_uv_channels < AI_MAX_NUMBER_OF_TEXTURECOORDS
        && mesh->mTextureCoords[n_uv_channels])
      ++n_uv_channels;
    _mangle_material_uvwsrc (ai, material, n_uv_channels);

    /* build the list of vertices */
    for (j = 0; j < mesh->mNumVertices; j++) {
      struct GstAIVertex *node =
          (struct GstAIVertex *) (map_info.data +
          sizeof (struct GstAIVertex) * j);
      const struct aiVector3D *pos = &(mesh->mVertices[j]);
      const struct aiVector3D *normal = &(mesh->mNormals[j]);
      const struct aiColor4D *color;
      const struct aiVector3D *tex_coord;
      int k;

      node->px = pos->x;
      node->py = pos->y;
      node->pz = pos->z;

      for (k = 0; k < AI_MAX_NUMBER_OF_COLOR_SETS; k++) {
        color = &ZeroColor4D;
        if (k >= 1 || !mesh->mColors[k]) {
          break;
        } else {
          color = &(mesh->mColors[k][j]);
        }

        node->c.x = color->r;
        node->c.y = color->g;
        node->c.z = color->b;
        node->c.w = color->a;
      }

      node->nx = normal->x;
      node->ny = normal->y;
      node->nz = normal->z;

      for (k = 0; k < AI_MAX_NUMBER_OF_TEXTURECOORDS; k++) {
        if (k >= n_uv_channels) {
          tex_coord = &Zero3D;
        } else {
          tex_coord = &(mesh->mTextureCoords[k][j]);
        }

        node->t[k].x = tex_coord->x;
        node->t[k].y = tex_coord->y;
      }

      if (node->px > max_coord)
        max_coord = node->px;
      if (node->px < min_coord)
        min_coord = node->px;
      if (node->py > max_coord)
        max_coord = node->py;
      if (node->py < min_coord)
        min_coord = node->py;
      if (node->pz > max_coord)
        max_coord = node->pz;
      if (node->pz < min_coord)
        min_coord = node->pz;
    }
    gst_memory_unmap (GST_MEMORY_CAST (node->vertex_buffer), &map_info);

    node->index_buffer =
        gst_allocator_alloc (NULL, 3 * sizeof (short) * mesh->mNumFaces, NULL);
    node->num_indices = 3 * mesh->mNumFaces;

    if (!gst_memory_map (GST_MEMORY_CAST (node->index_buffer), &map_info,
            GST_MAP_WRITE)) {
      GST_ELEMENT_ERROR (ai, LIBRARY, INIT, ("%s", "Failed to map GL buffer"),
          (NULL));
      _deinit_scene (ai);
      return FALSE;
    }

    GST_INFO_OBJECT (ai, "mesh %d has %d faces", i, mesh->mNumFaces);

    /* build the list of indices for the list of vertices */
    for (j = 0; j < mesh->mNumFaces; j++) {
      short *idx = (short *) (map_info.data + 3 * sizeof (short) * j);
      const struct aiFace *face = &mesh->mFaces[j];

      g_assert (face->mNumIndices == 3);
      idx[0] = face->mIndices[0];
      idx[1] = face->mIndices[1];
      idx[2] = face->mIndices[2];
    }
    gst_memory_unmap (GST_MEMORY_CAST (node->index_buffer), &map_info);
  }

  /* FIXME: separate this into a separate element */
  /* scale to [-1, 1] */
  scale = ABS (max_coord - min_coord) * 0.5;
  offset = 0.0;                 //-(max_coord + min_coord) / 2.;
  for (i = 0; i < ai->scene->mNumMeshes; i++) {
    const struct aiMesh *mesh = ai->scene->mMeshes[i];
    struct GstAIMeshNode *node;
    GstMapInfo map_info;

    node = &g_array_index (ai->meshes, struct GstAIMeshNode, i);

    if (!gst_memory_map (GST_MEMORY_CAST (node->vertex_buffer), &map_info,
            GST_MAP_READWRITE)) {
      GST_ELEMENT_ERROR (ai, LIBRARY, INIT, ("%s", "Failed to map GL buffer"),
          (NULL));
      _deinit_scene (ai);
      return FALSE;
    }
    /* normalize vertices to [-1, 1] */
    for (j = 0; j < mesh->mNumVertices; j++) {
      struct GstAIVertex *node =
          (struct GstAIVertex *) (map_info.data +
          sizeof (struct GstAIVertex) * j);

      node->px = (node->px + offset) / scale;
      node->py = (node->py + offset) / scale;
      node->pz = (node->pz + offset) / scale;
    }
    gst_memory_unmap (GST_MEMORY_CAST (node->vertex_buffer), &map_info);
  }

  {
    gsize prev_offset = 0;

    gst_3d_material_info_unset (&ai->out_material_info);
    gst_3d_material_info_init (&ai->out_material_info);

    for (i = 0; i < ai->meshes->len; i++) {
      struct GstAIMeshNode *mesh =
          &g_array_index (ai->meshes, struct GstAIMeshNode, i);
      Gst3DVertexMeta *vmeta;
      gsize index_size, vertex_size;
      gint material_id = -1;

      index_size = gst_memory_get_sizes (mesh->index_buffer, NULL, NULL);
      vertex_size = gst_memory_get_sizes (mesh->vertex_buffer, NULL, NULL);
      gst_buffer_append_memory (ai->out_buffer,
          gst_memory_share (mesh->index_buffer, 0, -1));
      gst_buffer_append_memory (ai->out_buffer,
          gst_memory_share (mesh->vertex_buffer, 0, -1));

      vmeta = gst_buffer_add_3d_vertex_meta (ai->out_buffer, i,
          &ai->out_vertex_info, mesh->num_indices, mesh->num_vertices);
      vmeta->index_offset = prev_offset;
      prev_offset += index_size;
      vmeta->vertex_offset = prev_offset;
      prev_offset += vertex_size;

      if (mesh->material_index < ai->materials->len) {
        struct GstAIMaterial *material =
            &g_array_index (ai->materials, struct GstAIMaterial,
            mesh->material_index);
        Gst3DMaterialInfo minfo;

        material_id = mesh->material_index;
        _add_material_meta_to_buffer (ai, mesh->material_index, material,
            ai->out_buffer, &minfo);

        /* merge the material into the global material info */
        ai->out_material_info.elements |= minfo.elements;
        ai->out_material_info.shading |= minfo.shading;
        if (ai->out_material_info.caps) {
          if (minfo.caps) {
            /* XXX: is intersect right? we kinda want union but removing
             * unfixed fields */
            GstCaps *tmp =
                gst_caps_intersect (ai->out_material_info.caps, minfo.caps);
            g_assert (tmp && !gst_caps_is_empty (tmp));
            gst_caps_unref (ai->out_material_info.caps);
            ai->out_material_info.caps = tmp;
          }
        } else if (minfo.caps) {
          ai->out_material_info.caps = gst_caps_ref (minfo.caps);
        }
      }

      gst_buffer_add_3d_model_meta (ai->out_buffer, i, i, material_id);
    }
  }

  return TRUE;
}

static gboolean
negotiate_caps (GstAssimp * ai)
{
  GstAssimpSrcPad *ai_pad = (GstAssimpSrcPad *) ai->srcpad;
  GstCaps *caps;

  caps =
      gst_3d_model_info_to_caps (&ai->out_vertex_info, &ai->out_material_info);

  if (ai_pad->srccaps)
    gst_caps_unref (ai_pad->srccaps);
  ai_pad->srccaps = caps;

  return ai_pad->srccaps != NULL;
}

static void
_srcpad_push_mandatory_events (GstAssimpSrcPad * ai_pad)
{
  GstPad *srcpad = GST_PAD (ai_pad);
  GstEvent *segment = NULL;
  GstEvent *tags = NULL;

  if (ai_pad->send_stream_start) {
    GstEvent *ss;
    gchar s_id[32];

    GST_INFO_OBJECT (ai_pad, "pushing stream start");
    /* stream-start (FIXME: create id based on input ids) */
    g_snprintf (s_id, sizeof (s_id), "agg-%08x", g_random_int ());
    ss = gst_event_new_stream_start (s_id);
    if (!gst_pad_push_event (srcpad, ss)) {
      GST_WARNING_OBJECT (ai_pad, "Sending stream start event failed");
    }
    ai_pad->send_stream_start = FALSE;
  }

  if (ai_pad->srccaps) {
    GST_INFO_OBJECT (ai_pad, "pushing caps: %" GST_PTR_FORMAT, ai_pad->srccaps);
    if (!gst_pad_push_event (srcpad, gst_event_new_caps (ai_pad->srccaps))) {
      GST_WARNING_OBJECT (ai_pad, "Sending caps event failed");
    }
    gst_caps_unref (ai_pad->srccaps);
    ai_pad->srccaps = NULL;
  }

  GST_OBJECT_LOCK (ai_pad);
  if (ai_pad->send_segment && !ai_pad->flush_seeking) {
    segment = gst_event_new_segment (&ai_pad->segment);

    if (!ai_pad->seqnum)
      ai_pad->seqnum = gst_event_get_seqnum (segment);
    else
      gst_event_set_seqnum (segment, ai_pad->seqnum);
    ai_pad->send_segment = FALSE;

    GST_DEBUG_OBJECT (ai_pad, "pushing segment %" GST_PTR_FORMAT, segment);
  }

  if (ai_pad->tags && ai_pad->send_tags && !ai_pad->flush_seeking) {
    tags = gst_event_new_tag (gst_tag_list_ref (ai_pad->tags));
    ai_pad->send_tags = FALSE;
  }
  GST_OBJECT_UNLOCK (ai_pad);

  if (segment)
    gst_pad_push_event (srcpad, segment);
  if (tags)
    gst_pad_push_event (srcpad, tags);
}

static GstFlowReturn
_gst_assimp_generate_model (GstAssimp * ai, GstBuffer * buffer)
{
  GstAssimpSrcPad *srcpad = (GstAssimpSrcPad *) ai->srcpad;
  GstFlowReturn ret;

  if (!_init_scene (ai, buffer))
    return GST_FLOW_ERROR;

  do {
    if (gst_pad_check_reconfigure (ai->srcpad)) {
      if (!negotiate_caps (ai))
        return GST_FLOW_NOT_NEGOTIATED;

      _srcpad_push_mandatory_events (srcpad);
    }
  } while ((ret = _gst_assimp_render_scene (ai)) == GST_FLOW_OK);

  return ret;
}

static GstFlowReturn
_gst_assimp_render_scene (GstAssimp * ai)
{
  GstBuffer *out_buffer;
  GstFlowReturn ret;

  /* FIXME: animation */
  out_buffer = gst_buffer_copy (ai->out_buffer);

  GST_BUFFER_PTS (out_buffer) = ai->segment.position;
  GST_BUFFER_DTS (out_buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (out_buffer) = GST_SECOND;

  ai->segment.position += GST_SECOND;

  ret = gst_pad_push (ai->srcpad, out_buffer);

  return ret;
}

static GstStateChangeReturn
gst_assimp_change_state (GstElement * element, GstStateChange transition)
{
  GstAssimp *ai = GST_ASSIMP (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (ai, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      _deinit_scene (ai);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "assimp",
          GST_RANK_NONE, gst_assimp_get_type ())) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    assimp,
    "Assimp plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
