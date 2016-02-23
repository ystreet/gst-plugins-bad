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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gmodule.h>

#include "gstjniutils.h"
#include "gstamcsurfacetexture_nativelistener.hpp"

/*typedef jobject (*android_view_Surface_createFromIGraphicBufferProducer_PFN) (JNIEnv* env,
        const sp<IGraphicBufferProducer>& bufferProducer);*/
typedef void (*ConsumerBase_setOnFrameAvailableListener_PFN) (ConsumerBase * base,
        const wp<ConsumerBase::FrameAvailableListener>& listener);
typedef sp<ConsumerBase> (*SurfaceTexture_getSurfaceTexture_PFN) (JNIEnv* env, jobject thiz);

gboolean
gst_amc_surface_texture_set_native_listener (GstAmcSurfaceTexture * texture,
    GstAmcFrameAvailableNotify frame_available,
    gpointer user_data, GDestroyNotify notify)
{
/*  android_view_Surface_createFromIGraphicBufferProducer_PFN surface_create_from_producer; */
  ConsumerBase_setOnFrameAvailableListener_PFN consumer_set_frame_listener;
  SurfaceTexture_getSurfaceTexture_PFN get_native_surface_texture;
  GstAmcNativeListener *old_listener = NULL;
  JNIEnv* env;
  GModule *self;

  g_return_val_if_fail (GST_IS_AMC_SURFACE_TEXTURE (texture), FALSE);
  env = gst_amc_jni_get_env ();
  g_return_val_if_fail (env != NULL, FALSE);

  self = g_module_open (NULL, (GModuleFlags) 0);

/*  g_module_symbol (self,
      "_ZN7android53android_view_Surface_createFromIGraphicBufferProducerEP7_JNIEnvRKNS_2spINS_22IGraphicBufferProducerEEE", 
      &surface_create_from_producer);
  g_return_val_if_fail (surface_create_from_producer != NULL, FALSE); */

  g_module_symbol (self,
      "_ZN7android12ConsumerBase25setFrameAvailableListenerERKNS_2wpINS0_22FrameAvailableListenerEEE",
      (gpointer *) &consumer_set_frame_listener);
  g_return_val_if_fail (consumer_set_frame_listener != NULL, FALSE);

  g_module_symbol (self,
      "_ZN7android32SurfaceTexture_getSurfaceTextureEP7_JNIEnvP8_jobject",
      (gpointer *) &get_native_surface_texture);
  g_return_val_if_fail (get_native_surface_texture != NULL, FALSE);

  if (texture->listener) {
    old_listener = reinterpret_cast<GstAmcNativeListener *> (texture->listener);
    texture->listener = NULL;
  }

  {
    sp<ConsumerBase> surface_tex = get_native_surface_texture (env, texture->jobject);
    sp<GstAmcNativeListener> listener (new GstAmcNativeListener (frame_available, user_data, notify));

    listener->incStrong((void *) gst_amc_surface_texture_set_native_listener);
    consumer_set_frame_listener (surface_tex.get(), listener);
    texture->listener = (GstAmcNativeListener *) listener.get();
  }

  if (old_listener)
    gst_amc_native_listener_unref (old_listener);

  return TRUE;
}

void
gst_amc_native_listener_unref (gpointer obj)
{
  GstAmcNativeListener *listener = static_cast<GstAmcNativeListener *> (obj);

  if (listener)
    listener->decStrong ((void *) gst_amc_surface_texture_set_native_listener);
}

GstAmcNativeListener::GstAmcNativeListener
(GstAmcFrameAvailableNotify frame_available, gpointer user_data,
    GDestroyNotify notify) :
  frame_available (frame_available),
  user_data (user_data),
  destroy (notify)
{
}

GstAmcNativeListener::~GstAmcNativeListener ()
{
  if (this->destroy)
    this->destroy (this->user_data);
}

void
GstAmcNativeListener::onFrameAvailable (const BufferItem& item)
{
  if (this->frame_available)
    this->frame_available (this->user_data);
}
