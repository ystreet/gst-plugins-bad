/*
 * GStreamer Plugins VR
 * Copyright (C) 2016 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
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

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include <gst/video/navigation.h>
#include "gst3dcamera.h"

#define GST_CAT_DEFAULT gst_3d_camera_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEFAULT_FOV 45.0
#define DEFAULT_ASPECT (16.0 / 9.0)
#define DEFAULT_ZNEAR 0.01
#define DEFAULT_ZFAR 1000.0

static void gst_3d_camera_default_update_view (Gst3DCamera * self);

G_DEFINE_TYPE_WITH_CODE (Gst3DCamera, gst_3d_camera, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_3d_camera_debug, "3dcamera", 0, "camera"));

Gst3DCamera *
gst_3d_camera_new (void)
{
  return g_object_new (GST_TYPE_3D_CAMERA, NULL);
}

void
gst_3d_camera_init (Gst3DCamera * self)
{
  self->fov = DEFAULT_FOV;
  self->aspect = DEFAULT_ASPECT;
  self->znear = DEFAULT_ZNEAR;
  self->zfar = DEFAULT_ZFAR;

  graphene_vec3_init (&self->eye, 0.f, 0.f, 1.f);
  graphene_vec3_init (&self->center, 0.f, 0.f, 0.f);
  graphene_vec3_init (&self->up, 0.f, 1.f, 0.f);

  self->cursor_last_x = 0;
  self->cursor_last_y = 0;
  self->pressed_mouse_button = 0;
}

static void
gst_3d_camera_class_init (Gst3DCameraClass * klass)
{
  klass->update_view = gst_3d_camera_default_update_view;
}

void
gst_3d_camera_update_view (Gst3DCamera * self)
{
  Gst3DCameraClass *camera_class = GST_3D_CAMERA_GET_CLASS (self);

  camera_class->update_view (self);
}

static void
gst_3d_camera_default_update_view (Gst3DCamera * self)
{
  graphene_matrix_t view_matrix, projection_matrix;

  if (self->ortho) {
    graphene_matrix_init_ortho (&projection_matrix, -1., 1., -1., 1.,
        self->znear, self->zfar);
  } else {
    graphene_matrix_init_perspective (&projection_matrix,
        self->fov, self->aspect, self->znear, self->zfar);
  }

  graphene_matrix_init_look_at (&view_matrix, &self->eye, &self->center,
      &self->up);

  graphene_matrix_multiply (&projection_matrix, &view_matrix,
      &self->view_projection);
}

static void
_press_key (Gst3DCamera * self, const gchar * key)
{
  GList *l;
  gboolean already_pushed = FALSE;

  GST_DEBUG ("Event: Press %s", key);

  for (l = self->pushed_buttons; l != NULL; l = l->next)
    if (g_strcmp0 (l->data, key) == 0)
      already_pushed = TRUE;

  if (!already_pushed)
    self->pushed_buttons = g_list_append (self->pushed_buttons, g_strdup (key));
}

static void
_release_key (Gst3DCamera * self, const gchar * key)
{
  GList *l = self->pushed_buttons;

  GST_DEBUG ("Event: Release %s", key);

  while (l != NULL) {
    GList *next = l->next;
    if (g_strcmp0 (l->data, key) == 0) {
      g_free (l->data);
      self->pushed_buttons = g_list_delete_link (self->pushed_buttons, l);
    }
    l = next;
  }
}

void
gst_3d_camera_navigation_event (Gst3DCamera * self, GstEvent * event)
{
  Gst3DCameraClass *camera_class = GST_3D_CAMERA_GET_CLASS (self);
  GstNavigationEventType event_type = gst_navigation_event_get_type (event);
  const GstStructure *structure = gst_event_get_structure (event);

  switch (event_type) {
    case GST_NAVIGATION_EVENT_KEY_PRESS:{
      const gchar *key = gst_structure_get_string (structure, "key");
      if (key != NULL)
        _press_key (self, key);
      break;
    }
    case GST_NAVIGATION_EVENT_KEY_RELEASE:{
      const gchar *key = gst_structure_get_string (structure, "key");
      if (key != NULL)
        _release_key (self, key);
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_MOVE:{
      /* handle the mouse motion for zooming and rotating the view */
      gdouble x, y;

      gst_structure_get_double (structure, "pointer_x", &x);
      gst_structure_get_double (structure, "pointer_y", &y);

      self->cursor_last_x = x;
      self->cursor_last_y = y;
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:{
      gint button;

      gst_structure_get_int (structure, "button", &button);
      self->pressed_mouse_button = 0;

      if (button == 1) {
        /* first mouse button release */
        gst_structure_get_double (structure, "pointer_x", &self->cursor_last_x);
        gst_structure_get_double (structure, "pointer_y", &self->cursor_last_y);
      }
      break;
    }
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:{
      gint button;

      gst_structure_get_int (structure, "button", &button);
      self->pressed_mouse_button = button;
      break;
    }
    default:
      break;
  }

  if (camera_class->navigation_event)
    camera_class->navigation_event (self, event);
}

GList *
gst_3d_camera_get_pressed_keys (Gst3DCamera * self)
{
  GList *ret;

  ret = g_list_copy_deep (self->pushed_buttons, (GCopyFunc) g_strdup, NULL);

  return ret;
}
