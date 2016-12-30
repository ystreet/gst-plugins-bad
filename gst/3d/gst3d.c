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
 * SECTION:plugin-3d
 *
 * 3D vertex/model plugin.
 * <refsect2>
 * <title>Debugging</title>
 * </refsect2>
 * <refsect2>
 * <title>Examples</title>
 * |[
 * GST_DEBUG=gl*:6 gst-launch-1.0 3dmodelsrc ! fakesink
 * ]| A debugging pipelines related to shaders.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gst3dvertexsrc.h"
#include "gst3dvertexconvert.h"

#define GST_CAT_DEFAULT gst_3d_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* Register filters that make up the gstgl plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "3d", 0, "gst3d");

  if (!gst_element_register (plugin, "3dvertextestsrc",
          GST_RANK_NONE, GST_TYPE_3D_VERTEX_SRC)) {
    return FALSE;
  }

  if (!gst_element_register (plugin, "3dvertexconvert",
          GST_RANK_NONE, GST_TYPE_3D_VERTEX_CONVERT)) {
    return FALSE;
  }

  return TRUE;
}

/* *INDENT-OFF* */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    3d,
    "3d plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
/* *INDENT-ON* */
