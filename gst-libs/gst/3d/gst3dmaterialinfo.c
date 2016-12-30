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

#include <string.h>

#include "gst3dmaterialinfo.h"

GST_DEBUG_CATEGORY_STATIC (trid_material_info_debug);
#define GST_CAT_DEFAULT _init_3d_material_info_debug()

static GstDebugCategory *
_init_3d_material_info_debug (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (trid_material_info_debug, "3dmaterialinfo", 0,
        "3D Material Info");
    g_once_init_leave (&_init, 1);
  }

  return trid_material_info_debug;
}

/* *INDENT-OFF* */
GType
gst_3d_material_element_get_type (void)
{
  static volatile gsize type_id = 0;
  if (g_once_init_enter (&type_id)) {
    static const GFlagsValue values[] = {
      {GST_3D_MATERIAL_ELEMENT_NONE, "GST_3D_MATERIAL_ELEMENT_NONE", "none"},
      {GST_3D_MATERIAL_ELEMENT_DIFFUSE, "GST_3D_MATERIAL_ELEMENT_DIFFUSE", "diffuse"},
      {GST_3D_MATERIAL_ELEMENT_SPECULAR, "GST_3D_MATERIAL_ELEMENT_SPECULAR", "specular"},
      {GST_3D_MATERIAL_ELEMENT_AMBIENT, "GST_3D_MATERIAL_ELEMENT_AMBIENT", "ambient"},
      {GST_3D_MATERIAL_ELEMENT_EMISSIVE, "GST_3D_MATERIAL_ELEMENT_EMISSIVE", "emissive"},
      {GST_3D_MATERIAL_ELEMENT_SHININESS, "GST_3D_MATERIAL_ELEMENT_SHININESS", "shininess"},
      {GST_3D_MATERIAL_ELEMENT_OPACITY, "GST_3D_MATERIAL_ELEMENT_OPACITY", "opacity"},
      {GST_3D_MATERIAL_ELEMENT_NORMAL_MAP, "GST_3D_MATERIAL_ELEMENT_NORMAL_MAP", "normal-map"},
      {GST_3D_MATERIAL_ELEMENT_HEIGHT_MAP, "GST_3D_MATERIAL_ELEMENT_HEIGHT_MAP", "height-map"},
      {GST_3D_MATERIAL_ELEMENT_LIGHT_MAP, "GST_3D_MATERIAL_ELEMENT_LIGHT_MAP", "light-map"},
      {GST_3D_MATERIAL_ELEMENT_CUSTOM1, "GST_3D_MATERIAL_ELEMENT_CUSTOM1", "custom1"},
      {GST_3D_MATERIAL_ELEMENT_CUSTOM2, "GST_3D_MATERIAL_ELEMENT_CUSTOM2", "custom2"},
      {GST_3D_MATERIAL_ELEMENT_CUSTOM3, "GST_3D_MATERIAL_ELEMENT_CUSTOM3", "custom3"},
      {GST_3D_MATERIAL_ELEMENT_CUSTOM4, "GST_3D_MATERIAL_ELEMENT_CUSTOM4", "custom4"},
      {0, NULL, NULL}
    };
    GType _type_id = g_flags_register_static ("Gst3DMaterialElement", values);
    g_once_init_leave (&type_id, _type_id);
  }
  return type_id;
}
/* *INDENT-ON* */

GType
gst_3d_material_element_flagset_get_type (void)
{
  static volatile GType type = 0;

  if (g_once_init_enter (&type)) {
    GType _type = gst_flagset_register (GST_TYPE_3D_MATERIAL_ELEMENT);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/* *INDENT-OFF* */
static const struct
{
  Gst3DMaterialElement element;
  const gchar *name;
} element_names[] = {
  {GST_3D_MATERIAL_ELEMENT_DIFFUSE, "diffuse"},
  {GST_3D_MATERIAL_ELEMENT_SPECULAR, "specular"},
  {GST_3D_MATERIAL_ELEMENT_AMBIENT, "ambient"},
  {GST_3D_MATERIAL_ELEMENT_EMISSIVE, "emissive"},
  {GST_3D_MATERIAL_ELEMENT_SHININESS, "shininess"},
  {GST_3D_MATERIAL_ELEMENT_OPACITY, "opacity"},
  {GST_3D_MATERIAL_ELEMENT_NORMAL_MAP, "normal-map"},
  {GST_3D_MATERIAL_ELEMENT_HEIGHT_MAP, "height-map"},
  {GST_3D_MATERIAL_ELEMENT_LIGHT_MAP, "light-map"},
  {GST_3D_MATERIAL_ELEMENT_CUSTOM1, "custom-1"},
  {GST_3D_MATERIAL_ELEMENT_CUSTOM2, "custom-2"},
  {GST_3D_MATERIAL_ELEMENT_CUSTOM3, "custom-3"},
  {GST_3D_MATERIAL_ELEMENT_CUSTOM4, "custom-4"},
};
/* *INDENT-ON* */

const gchar *
gst_3d_material_element_to_string (Gst3DMaterialElement element)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (element_names); i++) {
    if (element == element_names[i].element)
      return element_names[i].name;
  }

  return "unknown";
}

Gst3DMaterialElement
gst_3d_material_element_from_string (const gchar * name)
{
  int i;

  g_return_val_if_fail (name != NULL, GST_3D_MATERIAL_ELEMENT_NONE);

  for (i = 0; i < G_N_ELEMENTS (element_names); i++) {
    if (g_strcmp0 (name, element_names[i].name) == 0)
      return element_names[i].element;
  }

  return GST_3D_MATERIAL_ELEMENT_NONE;
}

/* *INDENT-OFF* */
static const struct
{
  Gst3DMaterialShading shading;
  const gchar *name;
} shading_names[] = {
  {GST_3D_MATERIAL_SHADING_NONE, "none"},
  {GST_3D_MATERIAL_SHADING_FLAT, "flat"},
  {GST_3D_MATERIAL_SHADING_GOURAUD, "gouraud"},
  {GST_3D_MATERIAL_SHADING_PHONG, "phong"},
  {GST_3D_MATERIAL_SHADING_PHONG_BLINN, "phong-blinn"},
  {GST_3D_MATERIAL_SHADING_TOON, "toon"},
  {GST_3D_MATERIAL_SHADING_OREN_NAYAR, "oren-nayar"},
  {GST_3D_MATERIAL_SHADING_MINNAERT, "minnaert"},
  {GST_3D_MATERIAL_SHADING_COOK_TORRANCE, "cook-torrance"},
  {GST_3D_MATERIAL_SHADING_FRESNEL, "fresnel"},
};
/* *INDENT-ON* */

const gchar *
gst_3d_material_shading_to_string (Gst3DMaterialShading shading)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (shading_names); i++) {
    if (shading == shading_names[i].shading)
      return shading_names[i].name;
  }

  return "unknown";
}

Gst3DMaterialShading
gst_3d_material_shading_from_string (const gchar * name)
{
  int i;

  g_return_val_if_fail (name != NULL, GST_3D_MATERIAL_SHADING_NONE);

  for (i = 0; i < G_N_ELEMENTS (shading_names); i++) {
    if (g_strcmp0 (name, shading_names[i].name) == 0)
      return shading_names[i].shading;
  }

  return GST_3D_MATERIAL_SHADING_NONE;
}

/* *INDENT-OFF* */
static const struct
{
  Gst3DMaterialTextureWrapMode wrap_mode;
  const gchar *name;
} wrap_mode_names[] = {
  {GST_3D_MATERIAL_WRAP_MODE_WRAP, "wrap"},
  {GST_3D_MATERIAL_WRAP_MODE_CLAMP, "clamp"},
  {GST_3D_MATERIAL_WRAP_MODE_DECAL, "decal"},
  {GST_3D_MATERIAL_WRAP_MODE_MIRROR, "mirror"},
};
/* *INDENT-ON* */

const gchar *
gst_3d_material_texture_wrap_mode_to_string (Gst3DMaterialTextureWrapMode
    wrap_mode)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (wrap_mode_names); i++) {
    if (wrap_mode == wrap_mode_names[i].wrap_mode)
      return wrap_mode_names[i].name;
  }

  return "unknown";
}

Gst3DMaterialTextureWrapMode
gst_3d_material_texture_wrap_mode_from_string (const gchar * name)
{
  int i;

  g_return_val_if_fail (name != NULL, GST_3D_MATERIAL_SHADING_NONE);

  for (i = 0; i < G_N_ELEMENTS (wrap_mode_names); i++) {
    if (g_strcmp0 (name, wrap_mode_names[i].name) == 0)
      return wrap_mode_names[i].wrap_mode;
  }

  return GST_3D_MATERIAL_WRAP_MODE_NONE;
}

#define nth_texture(stack, i) (&g_array_index (stack->textures, Gst3DMaterialTexture, i))

static void
gst_3d_material_texture_init (Gst3DMaterialTexture * texture)
{
  g_return_if_fail (texture != NULL);

  memset (texture, 0, sizeof (*texture));

  texture->multiplier = 1.0;
}

static void
gst_3d_material_texture_unset (Gst3DMaterialTexture * texture)
{
  if (texture->sample)
    gst_sample_unref (texture->sample);
  texture->sample = NULL;
}

static void
gst_3d_material_texture_copy_into (Gst3DMaterialTexture * src,
    Gst3DMaterialTexture * dest)
{
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  dest->multiplier = src->multiplier;
  dest->combiner = src->combiner;
  dest->flags = src->flags;
  dest->wrap_mode = src->wrap_mode;
  dest->sample = src->sample ? gst_sample_ref (src->sample) : NULL;
}

void
gst_3d_material_stack_init (Gst3DMaterialStack * stack)
{
  memset (stack, 0, sizeof (*stack));

  stack->textures = g_array_new (FALSE, TRUE, sizeof (Gst3DMaterialTexture));
  g_array_set_clear_func (stack->textures,
      (GDestroyNotify) gst_3d_material_texture_unset);
}

void
gst_3d_material_stack_unset (Gst3DMaterialStack * stack)
{
  if (stack->type >= GST_3D_MATERIAL_ELEMENT_CUSTOM1)
    g_free (stack->name);
  g_array_free (stack->textures, TRUE);
}

void
gst_3d_material_stack_copy_into (Gst3DMaterialStack * src,
    Gst3DMaterialStack * dest)
{
  gint i;

  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  dest->type = src->type;
  dest->name = g_strdup (src->name);
  dest->x = src->x;
  dest->y = src->y;
  dest->z = src->z;
  dest->textures = g_array_new (FALSE, TRUE, sizeof (Gst3DMaterialTexture));
  g_array_set_clear_func (dest->textures,
      (GDestroyNotify) gst_3d_material_texture_unset);
  g_array_set_size (dest->textures, src->textures->len);
  for (i = 0; i < dest->textures->len; i++)
    gst_3d_material_texture_copy_into (nth_texture (src, i), nth_texture (dest,
            i));
}

void
gst_3d_material_stack_set_n_textures (Gst3DMaterialStack * stack,
    guint n_textures)
{
  guint old_size;
  int i;

  g_return_if_fail (stack != NULL);

  old_size = stack->textures->len;
  if (stack->textures->len > 0) {
    for (i = stack->textures->len - 1; i >= n_textures; i--) {
      Gst3DMaterialTexture *texture =
          &g_array_index (stack->textures, Gst3DMaterialTexture, i);
      gst_3d_material_texture_unset (texture);
    }
  }

  g_array_set_size (stack->textures, n_textures);
  for (i = old_size; i < stack->textures->len; i++) {
    Gst3DMaterialTexture *texture =
        &g_array_index (stack->textures, Gst3DMaterialTexture, i);
    gst_3d_material_texture_init (texture);
  }
}

Gst3DMaterialTexture *
gst_3d_material_stack_get_texture (Gst3DMaterialStack * stack, int i)
{
  return nth_texture (stack, i);
}

void
gst_3d_material_info_init (Gst3DMaterialInfo * info)
{
  g_return_if_fail (info != NULL);

  memset (info, 0, sizeof (*info));
}

void
gst_3d_material_info_unset (Gst3DMaterialInfo * info)
{
  g_return_if_fail (info != NULL);

  if (info->caps)
    gst_caps_unref (info->caps);
  info->caps = NULL;
}

void
gst_3d_material_info_copy_into (Gst3DMaterialInfo * src,
    Gst3DMaterialInfo * dest)
{
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  dest->shading = src->shading;
  dest->elements = src->elements;
  dest->caps = src->caps ? gst_caps_ref (src->caps) : NULL;
}

gboolean
gst_3d_material_info_from_caps (Gst3DMaterialInfo * info, const GstCaps * caps)
{
  GstStructure *str;
  const GValue *val;
  const gchar *s;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  memset (info, 0, sizeof (*info));

  str = gst_caps_get_structure (caps, 0);

  if (!gst_structure_has_name (str, "application/3d-material"))
    goto wrong_name;

  if (!(s = gst_structure_get_string (str, "shading")))
    goto no_shading;

  info->shading = gst_3d_material_shading_from_string (s);
  if (info->shading == GST_3D_MATERIAL_SHADING_UNKNOWN)
    goto unknown_shading;

  if (!(val = gst_structure_get_value (str, "elements")))
    goto no_elements;

  info->elements = gst_value_get_flagset_flags (val);
  if (info->elements == GST_3D_MATERIAL_ELEMENT_NONE)
    goto no_elements;

  if ((val = gst_structure_get_value (str, "material-caps"))) {
    info->caps = (GstCaps *) gst_value_get_caps (val);
    if (info->caps)
      gst_caps_ref (info->caps);
  }

  return TRUE;

wrong_name:
  GST_ERROR ("caps has the wrong structure name");
  return FALSE;

no_shading:
  GST_ERROR ("field \'shading\' is missing");
  return FALSE;

unknown_shading:
  GST_ERROR ("unknown value in \'shading\'");
  return FALSE;

no_elements:
  GST_ERROR ("field \'elements\' is missing");
  return FALSE;
}

GstCaps *
gst_3d_material_info_to_caps (const Gst3DMaterialInfo * info)
{
  GstStructure *str;
  GstCaps *ret;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->shading != GST_3D_MATERIAL_SHADING_NONE, NULL);
  g_return_val_if_fail (info->elements != GST_3D_MATERIAL_ELEMENT_NONE, NULL);

  ret = gst_caps_new_empty_simple ("application/3d-material");
  str = gst_caps_get_structure (ret, 0);

  gst_structure_set (str, "shading", G_TYPE_STRING,
      gst_3d_material_shading_to_string (info->shading), NULL);

  gst_structure_set (str, "elements", GST_TYPE_FLAG_SET, info->elements,
      GST_FLAG_SET_MASK_EXACT, NULL);

  if (info->caps)
    gst_structure_set (str, "caps", GST_TYPE_CAPS, info->caps, NULL);

  return ret;
}
