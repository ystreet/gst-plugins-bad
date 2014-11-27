/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <ystreet00@gmail.com>
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

#include <gst/gl/egl/gstgldisplay_egl.h>
#include <gst/gl/egl/gsteglimagememory.h>

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

#ifndef EGL_PLATFORM_X11
#define EGL_PLATFORM_X11 0x31D5
#endif
#ifndef EGL_PLATFORM_WAYLAND
#define EGL_PLATFORM_WAYLAND 0x31D8
#endif
#ifndef EGL_PLATFORM_ANDROID
#define EGL_PLATFORM_ANDROID 0x3141
#endif

typedef EGLDisplay (*_gst_eglGetPlatformDisplay_type) (EGLenum platform,
    void *native_display, const EGLint * attrib_list);

G_DEFINE_TYPE (GstGLDisplayEGL, gst_gl_display_egl, GST_TYPE_GL_DISPLAY);

static void gst_gl_display_egl_finalize (GObject * object);
static guintptr gst_gl_display_egl_get_handle (GstGLDisplay * display);

static void
gst_gl_display_egl_class_init (GstGLDisplayEGLClass * klass)
{
  GST_GL_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_egl_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_egl_finalize;
}

static void
gst_gl_display_egl_init (GstGLDisplayEGL * display_egl)
{
  GstGLDisplay *display = (GstGLDisplay *) display_egl;

  display->type = GST_GL_DISPLAY_TYPE_EGL;
  display_egl->foreign_display = FALSE;

  gst_egl_image_memory_init ();
}

static void
gst_gl_display_egl_finalize (GObject * object)
{
  GstGLDisplayEGL *display_egl = GST_GL_DISPLAY_EGL (object);

  if (display_egl->display && !display_egl->foreign_display) {
    eglTerminate (display_egl->display);
    display_egl->display = NULL;
  }

  G_OBJECT_CLASS (gst_gl_display_egl_parent_class)->finalize (object);
}

/**
 * gst_gl_display_egl__get_egl_display():
 * @type: a #GstGLDisplayType
 * @display: pointer to a display (or 0)
 *
 * Attempts to create a new #EGLDisplay from @display.  If @type is
 * %GST_GL_DISPLAY_TYPE_ANY, then @display must be 0.
 *
 * Returns: A new #EGLDisplay or %EGL_NO_DISPLAY
 */
EGLDisplay
gst_gl_display_egl_get_from_native (GstGLDisplayType type, guintptr display)
{
  const gchar *egl_exts;
  EGLint maj, min;
  EGLDisplay ret = EGL_NO_DISPLAY;
  _gst_eglGetPlatformDisplay_type _gst_eglGetPlatformDisplay;

  g_return_val_if_fail (type == GST_GL_DISPLAY_TYPE_ANY
      && type != 0, EGL_NO_DISPLAY);

  /* given an EGLDisplay already */
  if (type == GST_GL_DISPLAY_TYPE_EGL)
    return (EGLDisplay) display;

  if (type == GST_GL_DISPLAY_TYPE_NONE)
    type = GST_GL_DISPLAY_TYPE_ANY;

  if (!eglInitialize (EGL_NO_DISPLAY, &maj, &min))
    return EGL_NO_DISPLAY;

  egl_exts = eglQueryString (EGL_NO_DISPLAY, EGL_EXTENSIONS);
  GST_DEBUG ("egl no display extensions: %s", egl_exts);

  if (eglGetError () != EGL_SUCCESS || !egl_exts)
    goto default_display;

  /* check if we can actually choose the egl display type */
  if (maj == 1 && min < 5) {
    if (!gst_gl_check_extension ("EGL_KHR_client_get_all_proc_addresses",
            egl_exts))
      goto default_display;
    if (!gst_gl_check_extension ("EGL_EXT_platform_base", egl_exts))
      goto default_display;
  }

  _gst_eglGetPlatformDisplay = (_gst_eglGetPlatformDisplay_type)
      eglGetProcAddress ("eglGetPlatformDisplay");
  if (!_gst_eglGetPlatformDisplay)
    _gst_eglGetPlatformDisplay = (_gst_eglGetPlatformDisplay_type)
        eglGetProcAddress ("eglGetPlatformDisplayEXT");
  if (!_gst_eglGetPlatformDisplay)
    goto default_display;

  /* try each platform in turn */
#if GST_GL_HAVE_WINDOW_X11
  if (ret == EGL_NO_DISPLAY && (type & GST_GL_DISPLAY_TYPE_X11) &&
      (gst_gl_check_extension ("EGL_KHR_platform_x11", egl_exts) ||
          gst_gl_check_extension ("EGL_EXT_platform_x11", egl_exts))) {
    ret = _gst_eglGetPlatformDisplay (EGL_PLATFORM_X11, (gpointer) display,
        NULL);
  }
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND
  if (ret == EGL_NO_DISPLAY && (type & GST_GL_DISPLAY_TYPE_WAYLAND) &&
      (gst_gl_check_extension ("EGL_KHR_platform_wayland", egl_exts) ||
          gst_gl_check_extension ("EGL_EXT_platform_wayland", egl_exts))) {
    ret = _gst_eglGetPlatformDisplay (EGL_PLATFORM_WAYLAND, (gpointer) display,
        NULL);
  }
#endif
#if GST_GL_HAVE_WINDOW_WAYLAND
  if (ret == EGL_NO_DISPLAY && (type & GST_GL_DISPLAY_TYPE_WAYLAND) &&
      (gst_gl_check_extension ("EGL_KHR_platform_wayland", egl_exts) ||
          gst_gl_check_extension ("EGL_EXT_platform_wayland", egl_exts))) {
    ret = _gst_eglGetPlatformDisplay (EGL_PLATFORM_WAYLAND, (gpointer) display,
        NULL);
  }
#endif

  if (ret != EGL_NO_DISPLAY)
    return ret;

  /* otherwise rely on the implementation to choose the correct display
   * based on the pointer */
default_display:
  return eglGetDisplay ((EGLNativeDisplayType) display);
}

/**
 * gst_gl_display_egl_new:
 * @name: (allow-none): a display name
 *
 * Create a new #GstGLDisplayEGL from the x11 display name.  See XOpenDisplay()
 * for details on what is a valid name.
 *
 * Returns: (transfer full): a new #GstGLDisplayEGL or %NULL
 */
GstGLDisplayEGL *
gst_gl_display_egl_new (void)
{
  GstGLDisplayEGL *ret;

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_EGL, NULL);
  ret->display =
      gst_gl_display_egl_get_from_native (GST_GL_DISPLAY_TYPE_ANY, 0);

  if (!ret->display) {
    GST_ERROR ("Failed to open EGL display connection");
  }

  return ret;
}

/**
 * gst_gl_display_egl_new_with_egl_display:
 * @display: an existing, x11 display
 *
 * Creates a new display connection from a X11 Display.
 *
 * Returns: (transfer full): a new #GstGLDisplayEGL
 */
GstGLDisplayEGL *
gst_gl_display_egl_new_with_egl_display (EGLDisplay display)
{
  GstGLDisplayEGL *ret;

  g_return_val_if_fail (display != NULL, NULL);

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_EGL, NULL);

  ret->display = display;
  ret->foreign_display = TRUE;

  return ret;
}

static gpointer
_ref_if_set (gpointer data, gpointer user_data)
{
  if (data)
    gst_object_ref (data);
  return data;
}

/**
 * gst_gl_display_egl_new_with_display:
 * @display: an existing, native display
 *
 * Creates a new display connection from a native Display.
 *
 * Returns: (transfer full): a new #GstGLDisplayEGL
 */
GstGLDisplayEGL *
gst_gl_display_egl_new_with_display (GstGLDisplay * display)
{
  GstGLDisplayEGL *ret;
  GstGLDisplayType display_type;
  guintptr native_display;

  g_return_val_if_fail (GST_IS_GL_DISPLAY (display), NULL);

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  if (GST_IS_GL_DISPLAY_EGL (display)) {
    GST_LOG_OBJECT (display, "display %" GST_PTR_FORMAT "is already a "
        "GstGLDisplayEGL", display);
    return gst_object_ref (display);
  }
#if GLIB_CHECK_VERSION(2, 34, 0)
  /* try to get a previously set GstGLDisplayEGL */
  ret = g_object_dup_data (G_OBJECT (display), "gst.gl.display.egl",
      (GDuplicateFunc) _ref_if_set, NULL);
  if (ret && GST_IS_GL_DISPLAY_EGL (ret)) {
    GST_LOG_OBJECT (display, "display %" GST_PTR_FORMAT "already has a "
        "GstGLDisplayEGL %" GST_PTR_FORMAT, display, ret);
    return ret;
  }

  if (ret)
    gst_object_unref (ret);
#endif

  display_type = gst_gl_display_get_handle_type (display);
  native_display = gst_gl_display_get_handle (display);

  g_return_val_if_fail (native_display, NULL);
  g_return_val_if_fail (display_type != GST_GL_DISPLAY_TYPE_NONE, NULL);

  ret = g_object_new (GST_TYPE_GL_DISPLAY_EGL, NULL);

  ret->display =
      gst_gl_display_egl_get_from_native (display_type, native_display);

  if (!ret->display) {
    GST_WARNING_OBJECT (ret, "failed to get EGLDisplay from native display");
    gst_object_unref (ret);
    return NULL;
  }
#if GLIB_CHECK_VERSION(2, 34, 0)
  g_object_set_data_full (G_OBJECT (display), "gst.gl.display.egl",
      gst_object_ref (ret), (GDestroyNotify) gst_object_unref);
#endif

  return ret;
}

static guintptr
gst_gl_display_egl_get_handle (GstGLDisplay * display)
{
  return (guintptr) GST_GL_DISPLAY_EGL (display)->display;
}
