/*
 * Copyright (C) 2016, Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_AMC_SURFACE_TEXTURE_NATIVE_H__
#define __GST_AMC_SURFACE_TEXTURE_NATIVE_H__

#include <gst/gst.h>
#include "gstamcsurfacetexture.h"

typedef void (*GstAmcFrameAvailableNotify) (gpointer user_data);

G_BEGIN_DECLS

gboolean    gst_amc_surface_texture_set_native_listener     (GstAmcSurfaceTexture * texture,
                                                             GstAmcFrameAvailableNotify frame_available,
                                                             gpointer user_data,
                                                             GDestroyNotify notify);
void        gst_amc_native_listener_unref                   (gpointer obj);

G_END_DECLS
#endif
