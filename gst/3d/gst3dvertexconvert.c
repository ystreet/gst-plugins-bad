/* GStreamer
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
 * SECTION:element-3dvertexconvert
 *
 * Audioconvert converts raw audio buffers between various possible formats.
 * It supports integer to float conversion, width/depth conversion,
 * signedness and endianness conversion and channel transformations.
 *
 * It can generate default values for channels when the source does not
 * provide them.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v -m audiotestsrc ! audioconvert ! audio/x-raw,format=S8,channels=2 ! level ! fakesink silent=TRUE
 * ]| This pipeline converts audio to 8-bit.  The level element shows that
 * the output levels still match the one for a sine wave.
 * |[
 * gst-launch-1.0 -v -m uridecodebin uri=file:///path/to/audio.flac ! audioconvert ! vorbisenc ! oggmux ! filesink location=audio.ogg
 * ]| The vorbis encoder takes float audio data instead of the integer data
 * output by most other audio elements. This pipeline decodes a FLAC audio file
 * (or any other audio file for which decoders are installed) and re-encodes
 * it into an Ogg/Vorbis audio file.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gst3dvertexconvert.h"

GST_DEBUG_CATEGORY (trid_vertex_convert_debug);
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);
#define GST_CAT_DEFAULT (trid_vertex_convert_debug)

/*** DEFINITIONS **************************************************************/

/* type functions */
static void gst_3d_vertex_convert_dispose (GObject * obj);
static void gst_3d_vertex_convert_finalize (GObject * obj);

/* gstreamer functions */
static GstCaps *gst_3d_vertex_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_3d_vertex_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_3d_vertex_convert_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn
gst_3d_vertex_convert_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf);
static GstFlowReturn gst_3d_vertex_convert_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_3d_vertex_convert_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);
static void gst_3d_vertex_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_3d_vertex_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* 3DVertexConvert signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (trid_vertex_convert_debug, "3dvertexconvert", 0, "3D vertex conversion element"); \
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");
#define gst_3d_vertex_convert_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (Gst3DVertexConvert, gst_3d_vertex_convert,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

/*** GSTREAMER PROTOTYPES *****************************************************/

#define VERTICES_CAPS \
    "application/3d-vertices, "                                             \
    "primitive-mode = (string) { " GST_3D_VERTEX_PRIMITIVE_MODE_ALL "} , "  \
    "index-format = (string) { none, " GST_3D_VERTEX_FORMAT_ALL " } "

#define ATTRIBUTE \
    "attribute, "                                                           \
    "format = (string) { " GST_3D_VERTEX_FORMAT_ALL "} , "                  \
    "type = (string) { " GST_3D_VERTEX_TYPE_ALL "} , "                      \
    "channels = (int) [ 1, 4 ] "

static GstCaps *
_create_pad_template_caps (void)
{
  GstCaps *ret = gst_caps_new_empty ();
  GstStructure *s = gst_structure_from_string (VERTICES_CAPS, NULL);
  GstStructure *attrib = gst_structure_from_string (ATTRIBUTE, NULL);
  GValue attrib_val = G_VALUE_INIT;
  GValue attributes = G_VALUE_INIT;
  gint i;

  g_value_init (&attributes, GST_TYPE_ARRAY);
  g_value_init (&attrib_val, GST_TYPE_STRUCTURE);

  gst_value_set_structure (&attrib_val, attrib);
  gst_structure_free (attrib);
  gst_structure_set_value (s, "attributes", &attrib_val);
  gst_caps_append_structure (ret, gst_structure_copy (s));

  gst_value_array_append_value (&attributes, &attrib_val);
  gst_structure_set_value (s, "attributes", &attributes);

  for (i = 1; i < 4; i++) {
    gst_value_array_append_value (&attributes, &attrib_val);

    gst_structure_set_value (s, "attributes", &attributes);

    gst_caps_append_structure (ret, gst_structure_copy (s));
  }
  g_value_unset (&attributes);
  g_value_unset (&attrib_val);

  gst_structure_free (s);

  return ret;
}

/*** TYPE FUNCTIONS ***********************************************************/
static void
gst_3d_vertex_convert_class_init (Gst3DVertexConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCaps *caps = _create_pad_template_caps ();

  gobject_class->dispose = gst_3d_vertex_convert_dispose;
  gobject_class->finalize = gst_3d_vertex_convert_finalize;
  gobject_class->set_property = gst_3d_vertex_convert_set_property;
  gobject_class->get_property = gst_3d_vertex_convert_get_property;

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  gst_element_class_set_static_metadata (element_class, "Vertex converter",
      "Filter/Converter/Vertex", "Convert 3D vertices to different formats",
      "Matthew Waters <matthew@centricular.com>");

  basetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_3d_vertex_convert_transform_caps);
  basetransform_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_3d_vertex_convert_fixate_caps);
  basetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_3d_vertex_convert_set_caps);
  basetransform_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_3d_vertex_convert_prepare_output_buffer);
  basetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_3d_vertex_convert_transform);
  basetransform_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_3d_vertex_convert_transform_meta);

  basetransform_class->passthrough_on_same_caps = TRUE;
}

static void
gst_3d_vertex_convert_init (Gst3DVertexConvert * this)
{
  gst_3d_vertex_info_init (&this->in_info, 0);
  gst_3d_vertex_info_init (&this->out_info, 0);
}

static void
gst_3d_vertex_convert_dispose (GObject * obj)
{
  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (obj);

  if (this->convert) {
    gst_3d_vertex_converter_free (this->convert);
    this->convert = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_3d_vertex_convert_finalize (GObject * obj)
{
  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (obj);

  gst_3d_vertex_info_unset (&this->in_info);
  gst_3d_vertex_info_unset (&this->out_info);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

/*** GSTREAMER FUNCTIONS ******************************************************/

static gboolean
_remove_attribute_fields (GQuark field_id, GValue * value, gpointer user_data)
{
  if (field_id == g_quark_from_static_string ("format"))
    return FALSE;
  if (field_id == g_quark_from_static_string ("channels"))
    return FALSE;
  return TRUE;
}

static gboolean
_transform_remove_attribute_fields (const GValue * attr, GValue * res_value)
{
  if (GST_VALUE_HOLDS_STRUCTURE (attr)) {
    const GstStructure *old = gst_value_get_structure (attr);
    GstStructure *new = gst_structure_copy (old);

    gst_structure_filter_and_map_in_place (new, _remove_attribute_fields, NULL);
    g_value_init (res_value, GST_TYPE_STRUCTURE);
    gst_value_set_structure (res_value, new);
    gst_structure_free (new);
    return TRUE;
  } else if (GST_VALUE_HOLDS_ARRAY (attr)) {
    int i, len;

    g_value_init (res_value, GST_TYPE_ARRAY);
    len = gst_value_array_get_size (attr);
    for (i = 0; i < len; i++) {
      const GValue *item;
      GValue new = G_VALUE_INIT;

      item = gst_value_array_get_value (attr, i);
      if (!_transform_remove_attribute_fields (item, &new)) {
        g_value_unset (res_value);
        g_value_unset (&new);
        return FALSE;
      }
      gst_value_array_append_value (res_value, &new);
      g_value_unset (&new);
    }
    return TRUE;
  } else if (GST_VALUE_HOLDS_LIST (attr)) {
    int i, len;

    g_value_init (res_value, GST_TYPE_LIST);
    len = gst_value_list_get_size (attr);
    for (i = 0; i < len; i++) {
      const GValue *item;
      GValue new = G_VALUE_INIT;

      item = gst_value_list_get_value (attr, i);
      if (!_transform_remove_attribute_fields (item, &new)) {
        g_value_unset (res_value);
        g_value_unset (&new);
        return FALSE;
      }
      gst_value_list_append_value (res_value, &new);
      g_value_unset (&new);
    }
    return TRUE;
  }
  return FALSE;
}

/* next lexicographical permutation
 * from: http://rosettacode.org/wiki/Permutations */
static gboolean
_next_lex_perm (int *array, int n)
{
  int k, l;

#define swap(array__, i__, j__) {int t__ = array__[i__]; array__[i__] = array__[j__]; array__[j__] = t__;}
  /* 1. Find the largest index k such that a[k] < a[k + 1]. If no such
     index exists, the permutation is the last permutation. */
  for (k = n - 1; k && array[k - 1] >= array[k]; k--);
  if (!k--)
    return FALSE;

  /* 2. Find the largest index l such that a[k] < a[l]. Since k + 1 is
     such an index, l is well defined */
  for (l = n - 1; array[l] <= array[k]; l--);

  /* 3. Swap a[k] with a[l] */
  swap (array, k, l);

  /* 4. Reverse the sequence from a[k + 1] to the end */
  for (k++, l = n - 1; l > k; l--, k++)
    swap (array, k, l);
  return TRUE;
#undef swap
}

static gboolean
_transform_attribute_permutations (const GValue * attr, GValue * res_value)
{
  if (GST_VALUE_HOLDS_STRUCTURE (attr)) {
    g_value_init (res_value, GST_TYPE_STRUCTURE);
    g_value_copy (attr, res_value);
    return TRUE;
  } else if (GST_VALUE_HOLDS_ARRAY (attr)) {
    int i, len;
    int *permutations;

    g_value_init (res_value, GST_TYPE_LIST);
    len = gst_value_array_get_size (attr);

    permutations = g_new0 (int, len);
    for (i = 0; i < len; i++)
      permutations[i] = i;

    /* permutations of all possible ordering and number of attributes */
    do {
      GValue new = G_VALUE_INIT;
      const GValue *item;

      item = gst_value_array_get_value (attr, permutations[0]);
      if (!_transform_attribute_permutations (item, &new)) {
        g_value_unset (res_value);
        g_value_unset (&new);
        g_free (permutations);
        return FALSE;
      }
      gst_value_list_append_and_take_value (res_value, &new);

      for (i = 1; i < len; i++) {
        GValue new_array = G_VALUE_INIT;
        int j;

        g_value_init (&new_array, GST_TYPE_ARRAY);
        for (j = 0; j < i; j++) {
          item = gst_value_array_get_value (attr, permutations[j]);
          if (!_transform_attribute_permutations (item, &new)) {
            g_value_unset (res_value);
            g_value_unset (&new_array);
            g_value_unset (&new);
            g_free (permutations);
            return FALSE;
          }
          gst_value_array_append_and_take_value (&new_array, &new);
        }

        if (!gst_value_is_subset (&new_array, res_value))
          gst_value_list_append_and_take_value (res_value, &new_array);
      }
    } while (_next_lex_perm (permutations, len));

    g_free (permutations);
    return TRUE;
  } else if (GST_VALUE_HOLDS_LIST (attr)) {
    int i, len;

    g_value_init (res_value, GST_TYPE_LIST);
    len = gst_value_list_get_size (attr);
    for (i = 0; i < len; i++) {
      const GValue *item;
      GValue new = G_VALUE_INIT;

      item = gst_value_list_get_value (attr, i);
      if (!_transform_attribute_permutations (item, &new)) {
        g_value_unset (res_value);
        g_value_unset (&new);
        return FALSE;
      }
      gst_value_list_append_value (res_value, &new);
      g_value_unset (&new);
    }
    return TRUE;
  }
  return FALSE;
}

/* BaseTransform vmethods */
/* copies the given caps */
static GstCaps *
gst_3d_vertex_convert_caps_remove_format_info (GstCaps * caps,
    GstPadDirection direction)
{
  GstStructure *st;
  GstCaps *res;
  gint i, n;

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    const GValue *attr;
    GValue new_attr = G_VALUE_INIT;
    st = gst_caps_get_structure (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure (res, st))
      continue;

    if (direction == GST_PAD_SINK || TRUE) {
      st = gst_structure_copy (st);
      /* FIXME: convert indices as well */
/*      gst_structure_remove_field (st, "index-format"); */
      gst_structure_remove_field (st, "attributes");
      gst_caps_append_structure (res, st);
    } else {
      /* FIXME: this doesn't add extra attributes */
      attr = gst_structure_get_value (st, "attributes");

      if (_transform_remove_attribute_fields (attr, &new_attr)) {
        GValue permutated = G_VALUE_INIT;

        if (_transform_attribute_permutations (&new_attr, &permutated)) {
          st = gst_structure_copy (st);
          /* FIXME: convert indices as well */
/*          gst_structure_remove_field (st, "index-format"); */
          gst_structure_take_value (st, "attributes", &permutated);
          gst_caps_append_structure (res, st);
        }
        g_value_unset (&new_attr);
      }
    }
  }

  return res;
}

/* The caps can be transformed into any other caps with format info removed.
 * However, we should prefer passthrough, so if passthrough is possible,
 * put it first in the list. */
static GstCaps *
gst_3d_vertex_convert_transform_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2;
  GstCaps *result;

  /* Get all possible caps that we can transform to */
  tmp = gst_3d_vertex_convert_caps_remove_format_info (caps, direction);

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (btrans, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

#define SCORE_FORMAT_CHANGE       1
#define SCORE_DEPTH_CHANGE        1
#define SCORE_CHANNEL_CHANGE      1
#define SCORE_COMPLEX_CHANGE      1

#define SCORE_COMPLEX_GAIN        2     /* change into a complex format */
#define SCORE_DEPTH_LOSS          4     /* change bit depth */
#define SCORE_CHANNEL_LOSS        8     /* lose at least one channel */

/* calculate how much loss a conversion would be */
static void
score_value (Gst3DVertexConvert * convert,
    const Gst3DVertexFormatInfo * in_info, gint in_channels,
    const GValue * format, const GValue * channels_val, gint * min_loss,
    const Gst3DVertexFormatInfo ** out_info, gint * out_channels)
{
  const gchar *fname;
  const Gst3DVertexFormatInfo *t_info;
  Gst3DVertexFormatFlags in_flags, t_flags;
  gint n_channels, channels;
  gint loss;

  fname = g_value_get_string (format);
  t_info =
      gst_3d_vertex_format_get_info (gst_3d_vertex_format_from_string (fname));
  if (!t_info)
    return;
  n_channels = g_value_get_int (channels_val);
  channels = MAX (n_channels, t_info->n_components);

  /* accept input format immediately without loss */
  if (in_info == t_info && in_channels == n_channels) {
    *min_loss = 0;
    *out_info = t_info;
    *out_channels = n_channels;
    return;
  }

  loss = SCORE_FORMAT_CHANGE;

  in_flags = GST_3D_VERTEX_FORMAT_INFO_FLAGS (in_info);
  in_flags &= ~GST_3D_VERTEX_FORMAT_FLAG_LE;
//  in_flags &= ~GST_3D_VERTEX_FORMAT_FLAG_UNPACK;

  t_flags = GST_3D_VERTEX_FORMAT_INFO_FLAGS (t_info);
  t_flags &= ~GST_3D_VERTEX_FORMAT_FLAG_LE;
//  t_flags &= ~GST_3D_VERTEX_FORMAT_FLAG_UNPACK;

  if ((t_flags & GST_3D_VERTEX_FORMAT_FLAG_COMPLEX) !=
      (in_flags & GST_3D_VERTEX_FORMAT_FLAG_COMPLEX)) {
    loss += SCORE_COMPLEX_CHANGE;
    if ((t_flags & GST_3D_VERTEX_FORMAT_FLAG_COMPLEX))
      loss += SCORE_COMPLEX_GAIN;
  }

  if ((in_info->width) != (t_info->width)) {
    loss += SCORE_DEPTH_CHANGE;
    if ((in_info->width) > (t_info->width))
      loss += SCORE_DEPTH_LOSS;
  }

  if ((in_channels) != (channels)) {
    loss += SCORE_CHANNEL_CHANGE;
    if ((in_channels) > (channels))
      loss += SCORE_CHANNEL_LOSS;
  }

  GST_DEBUG_OBJECT (convert, "score %s,%d -> %s,%d = %d",
      GST_3D_VERTEX_FORMAT_INFO_NAME (in_info), in_channels,
      GST_3D_VERTEX_FORMAT_INFO_NAME (t_info), channels, loss);

  if (loss < *min_loss) {
    GST_DEBUG_OBJECT (convert, "found new best %d", loss);
    *out_info = t_info;
    *min_loss = loss;
    *out_channels = n_channels;
  }
}

static void
score_channels (Gst3DVertexConvert * convert,
    const Gst3DVertexFormatInfo * in_info, gint in_channels,
    const GValue * format, const GValue * channels, gint * min_loss,
    const Gst3DVertexFormatInfo ** out_info, gint * out_channels)
{
  if (GST_VALUE_HOLDS_LIST (channels)) {
    gint i, n;

    n = gst_value_list_get_size (channels);
    for (i = 0; i < n; i++) {
      const GValue *val = gst_value_list_get_value (channels, i);

      score_value (convert, in_info, in_channels, format, val, min_loss,
          out_info, out_channels);
      if (min_loss == 0)
        break;
    }
  } else if (GST_VALUE_HOLDS_ARRAY (channels)) {
    gint i, n;

    n = gst_value_array_get_size (channels);
    for (i = 0; i < n; i++) {
      const GValue *val = gst_value_array_get_value (channels, i);

      score_value (convert, in_info, in_channels, format, val, min_loss,
          out_info, out_channels);
      if (min_loss == 0)
        break;
    }
  } else if (G_VALUE_HOLDS_INT (channels)) {
    score_value (convert, in_info, in_channels, format, channels, min_loss,
        out_info, out_channels);
  }
}

static void
fixate_attribute (Gst3DVertexConvert * convert, const GstStructure * ins,
    GstStructure * outs)
{
  const gchar *in_format;
  const GValue *format, *channels;
  const Gst3DVertexFormatInfo *in_info, *out_info = NULL;
  gint in_channels, out_channels;
  gint min_loss;

  if (!ins) {
    in_format = "unknown";
    in_channels = 4;
  } else {
    in_format = gst_structure_get_string (ins, "format");
    if (!in_format)
      return;
    if (!gst_structure_has_field_typed (ins, "channels", G_TYPE_INT))
      return;
    gst_structure_get_int (ins, "channels", &in_channels);
  }

  format = gst_structure_get_value (outs, "format");
  channels = gst_structure_get_value (outs, "channels");
  /* should not happen */
  if (format == NULL || channels == NULL)
    return;

  /* nothing to fixate? */
  if (!GST_VALUE_HOLDS_LIST (format) && !GST_VALUE_HOLDS_LIST (channels)
      && !GST_VALUE_HOLDS_INT_RANGE (channels))
    return;

  in_info =
      gst_3d_vertex_format_get_info (gst_3d_vertex_format_from_string
      (in_format));
  if (!in_info)
    return;

  if (GST_VALUE_HOLDS_LIST (format)) {
    gint i, n;

    n = gst_value_list_get_size (format);
    for (i = 0; i < n; i++) {
      const GValue *val = gst_value_list_get_value (format, i);
      score_channels (convert, in_info, in_channels, val, channels, &min_loss,
          &out_info, &out_channels);
      if (min_loss == 0)
        break;
    }
  } else if (G_VALUE_HOLDS_STRING (format)) {
    score_channels (convert, in_info, in_channels, format, channels, &min_loss,
        &out_info, &out_channels);
  }

  if (out_info && out_channels > 0)
    gst_structure_set (outs, "format", G_TYPE_STRING,
        GST_3D_VERTEX_FORMAT_INFO_NAME (out_info), "channels", out_channels,
        NULL);
}

static const GstStructure *
find_corresponding_attribute (const GstStructure * needle,
    const GValue * haystack)
{
  const gchar *type_str = gst_structure_get_string (needle, "type");

  if (!type_str)
    return NULL;

  if (GST_VALUE_HOLDS_STRUCTURE (haystack)) {
    const GstStructure *str = gst_value_get_structure (haystack);
    const gchar *s = gst_structure_get_string (str, "type");

    if (g_str_equal (type_str, s))
      return str;
  } else if (GST_VALUE_HOLDS_ARRAY (haystack)) {
    gint i, n;

    n = gst_value_array_get_size (haystack);
    for (i = 0; i < n; i++) {
      const GValue *attr = gst_value_array_get_value (haystack, i);
      const GstStructure *str = gst_value_get_structure (attr);
      const gchar *s = gst_structure_get_string (str, "type");

      if (g_str_equal (type_str, s)) {
        const gchar *n_n = gst_structure_get_string (needle, "custom-name");
        const gchar *a_n = gst_structure_get_string (str, "custom-name");
        if (a_n == NULL && n_n == NULL)
          return str;
        if (g_str_equal (a_n, n_n))
          return str;
      }
    }
  } else {
    g_assert_not_reached ();
  }

  return NULL;
}

static void
gst_3d_vertex_convert_fixate (GstBaseTransform * base, GstStructure * ins,
    GstStructure * outs)
{
  Gst3DVertexConvert *convert = GST_3D_VERTEX_CONVERT (base);
  const GValue *in_attributes, *attributes;

  in_attributes = gst_structure_get_value (ins, "attributes");
  if (!in_attributes)
    return;

  attributes = gst_structure_get_value (outs, "attributes");
  /* should not happen */
  if (attributes == NULL)
    return;

  /* nothing to fixate? */
//  if (gst_value_is_fixed (attributes))
//    return;

  /* FIXME: primitive-mode */

  if (GST_VALUE_HOLDS_STRUCTURE (attributes)) {
    GstStructure *str = (GstStructure *) gst_value_get_structure (attributes);
    const GstStructure *in_str =
        find_corresponding_attribute (str, in_attributes);

    fixate_attribute (convert, in_str, str);
  } else if (GST_VALUE_HOLDS_ARRAY (attributes)) {
    gint i, n;

    n = gst_value_array_get_size (attributes);
    for (i = 0; i < n; i++) {
      const GValue *attr = gst_value_array_get_value (attributes, i);
      GstStructure *str = (GstStructure *) gst_value_get_structure (attr);
      const GstStructure *in_str =
          find_corresponding_attribute (str, in_attributes);

      fixate_attribute (convert, in_str, str);
    }
  } else {
    g_assert_not_reached ();
  }
}

/* try to keep as many of the structure members the same by fixating the
 * possible ranges; this way we convert the least amount of things as possible
 */
static GstCaps *
gst_3d_vertex_convert_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *ins, *outs;
  GstCaps *result;

  GST_DEBUG_OBJECT (base, "trying to fixate othercaps %" GST_PTR_FORMAT
      " based on caps %" GST_PTR_FORMAT, othercaps, caps);

/*  result = gst_caps_intersect (othercaps, caps);
  if (gst_caps_is_empty (result)) {
    if (result)
      gst_caps_unref (result);*/
  result = othercaps;
/*  } else {
    gst_caps_unref (othercaps);
  }*/

  GST_DEBUG_OBJECT (base, "now fixating %" GST_PTR_FORMAT, result);

  /* fixate remaining fields */
  result = gst_caps_make_writable (result);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (result, 0);

  gst_3d_vertex_convert_fixate (base, ins, outs);

  GST_ERROR ("result: %" GST_PTR_FORMAT, result);

  /* fixate remaining */
  result = gst_caps_fixate (result);

  GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_3d_vertex_convert_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (base);
  Gst3DVertexInfo in_info;
  Gst3DVertexInfo out_info;

  GST_DEBUG_OBJECT (base, "incaps %" GST_PTR_FORMAT ", outcaps %"
      GST_PTR_FORMAT, incaps, outcaps);

  if (this->convert) {
    gst_3d_vertex_converter_free (this->convert);
    this->convert = NULL;
  }

  if (!gst_3d_vertex_info_from_caps (&in_info, incaps))
    goto invalid_in;
  if (!gst_3d_vertex_info_from_caps (&out_info, outcaps))
    goto invalid_out;

  this->convert = gst_3d_vertex_converter_new (&in_info, &out_info, NULL);

  if (this->convert == NULL)
    goto no_converter;

  gst_3d_vertex_info_unset (&this->in_info);
  gst_3d_vertex_info_unset (&this->out_info);

  gst_3d_vertex_info_copy_into (&in_info, &this->in_info);
  gst_3d_vertex_info_copy_into (&out_info, &this->out_info);

  gst_3d_vertex_info_unset (&in_info);
  gst_3d_vertex_info_unset (&out_info);

  return TRUE;

  /* ERRORS */
invalid_in:
  {
    GST_ERROR_OBJECT (base, "invalid input caps");
    return FALSE;
  }
invalid_out:
  {
    GST_ERROR_OBJECT (base, "invalid output caps");
    gst_3d_vertex_info_unset (&in_info);
    return FALSE;
  }
no_converter:
  {
    GST_ERROR_OBJECT (base, "could not make converter");
    gst_3d_vertex_info_unset (&in_info);
    gst_3d_vertex_info_unset (&out_info);
    return FALSE;
  }
}

static GstFlowReturn
gst_3d_vertex_convert_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (base);
  gsize out_size;
  GstMemory *index_mem = NULL;
  Gst3DVertexMeta *in_meta;
  Gst3DVertexMapInfo map_info;

  in_meta = gst_buffer_get_3d_vertex_meta (inbuf, 0);
  if (!in_meta) {
    GST_ELEMENT_ERROR (this, STREAM, FORMAT,
        ("Input buffer doesn't have Gst3DVertexMeta"), (NULL));
    return GST_FLOW_ERROR;
  }

  out_size = gst_3d_vertex_info_get_size (&this->out_info, in_meta->n_vertices);
  if (out_size == 0) {
    GST_ELEMENT_ERROR (this, CORE, NEGOTIATION,
        ("Failed to calculate size of output buffer"), (NULL));
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (this->in_info.index_finfo) {
    if (!gst_3d_vertex_meta_map (in_meta, &map_info, GST_MAP_READ)) {
      GST_ELEMENT_ERROR (this, STREAM, FORMAT,
          ("Failed to map input buffer"), (NULL));
      return GST_FLOW_ERROR;
    }

    index_mem = gst_memory_share (map_info.index_map.memory,
        map_info.index_offset, map_info.index_map.size);

    gst_3d_vertex_meta_unmap (in_meta, &map_info);
  }

  *outbuf = gst_buffer_new ();
  if (index_mem)
    gst_buffer_append_memory (*outbuf, index_mem);
  gst_buffer_append_memory (*outbuf, gst_allocator_alloc (NULL, out_size,
          NULL));
  gst_buffer_add_3d_vertex_meta (*outbuf, 0, &this->out_info,
      in_meta->n_indices, in_meta->n_vertices);

  /* basetransform doesn't unref if they're the same */
  if (inbuf == *outbuf)
    gst_buffer_unref (*outbuf);
  else
    GST_BASE_TRANSFORM_CLASS (parent_class)->copy_metadata (base, inbuf,
        *outbuf);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_3d_vertex_convert_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (base);

  if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_GAP)) {
    if (!gst_3d_vertex_converter_vertices (this->convert, inbuf, outbuf))
      goto convert_error;
  } else {
    return GST_FLOW_ERROR;
  }
  return GST_FLOW_OK;

  /* ERRORS */
convert_error:
  {
    GST_ELEMENT_ERROR (this, STREAM, FORMAT,
        (NULL), ("error while converting"));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_3d_vertex_convert_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf)
{
  return FALSE;
}

static void
gst_3d_vertex_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
//  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_3d_vertex_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
//  Gst3DVertexConvert *this = GST_3D_VERTEX_CONVERT (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
