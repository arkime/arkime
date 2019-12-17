/*
 * Copyright © 2010 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Vincent Untz <vuntz@gnome.org>
 */

#include "config.h"

#include "gsettings-mapping.h"

static GVariant *
g_settings_set_mapping_int (const GValue       *value,
                            const GVariantType *expected_type)
{
  GVariant *variant = NULL;
  gint64 l;

  if (G_VALUE_HOLDS_INT (value))
    l = g_value_get_int (value);
  else if (G_VALUE_HOLDS_INT64 (value))
    l = g_value_get_int64 (value);
  else
    return NULL;

  if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT16))
    {
      if (G_MININT16 <= l && l <= G_MAXINT16)
        variant = g_variant_new_int16 ((gint16) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT16))
    {
      if (0 <= l && l <= G_MAXUINT16)
        variant = g_variant_new_uint16 ((guint16) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT32))
    {
      if (G_MININT32 <= l && l <= G_MAXINT32)
        variant = g_variant_new_int32 ((gint) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT32))
    {
      if (0 <= l && l <= G_MAXUINT32)
        variant = g_variant_new_uint32 ((guint) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT64))
    {
      if (G_MININT64 <= l && l <= G_MAXINT64)
        variant = g_variant_new_int64 ((gint64) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT64))
    {
      if (0 <= l && l <= G_MAXUINT64)
        variant = g_variant_new_uint64 ((guint64) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_HANDLE))
    {
      if (0 <= l && l <= G_MAXUINT32)
        variant = g_variant_new_handle ((guint) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_DOUBLE))
    variant = g_variant_new_double ((gdouble) l);

  return variant;
}

static GVariant *
g_settings_set_mapping_float (const GValue       *value,
                              const GVariantType *expected_type)
{
  GVariant *variant = NULL;
  gdouble d;
  gint64 l;

  if (G_VALUE_HOLDS_DOUBLE (value))
    d = g_value_get_double (value);
  else
    return NULL;

  l = (gint64) d;
  if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT16))
    {
      if (G_MININT16 <= l && l <= G_MAXINT16)
        variant = g_variant_new_int16 ((gint16) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT16))
    {
      if (0 <= l && l <= G_MAXUINT16)
        variant = g_variant_new_uint16 ((guint16) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT32))
    {
      if (G_MININT32 <= l && l <= G_MAXINT32)
        variant = g_variant_new_int32 ((gint) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT32))
    {
      if (0 <= l && l <= G_MAXUINT32)
        variant = g_variant_new_uint32 ((guint) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT64))
    {
      if (G_MININT64 <= l && l <= G_MAXINT64)
        variant = g_variant_new_int64 ((gint64) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT64))
    {
      if (0 <= l && l <= G_MAXUINT64)
        variant = g_variant_new_uint64 ((guint64) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_HANDLE))
    {
      if (0 <= l && l <= G_MAXUINT32)
        variant = g_variant_new_handle ((guint) l);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_DOUBLE))
    variant = g_variant_new_double ((gdouble) d);

  return variant;
}
static GVariant *
g_settings_set_mapping_unsigned_int (const GValue       *value,
                                     const GVariantType *expected_type)
{
  GVariant *variant = NULL;
  guint64 u;

  if (G_VALUE_HOLDS_UINT (value))
    u = g_value_get_uint (value);
  else if (G_VALUE_HOLDS_UINT64 (value))
    u = g_value_get_uint64 (value);
  else
    return NULL;

  if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT16))
    {
      if (u <= G_MAXINT16)
        variant = g_variant_new_int16 ((gint16) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT16))
    {
      if (u <= G_MAXUINT16)
        variant = g_variant_new_uint16 ((guint16) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT32))
    {
      if (u <= G_MAXINT32)
        variant = g_variant_new_int32 ((gint) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT32))
    {
      if (u <= G_MAXUINT32)
        variant = g_variant_new_uint32 ((guint) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_INT64))
    {
      if (u <= G_MAXINT64)
        variant = g_variant_new_int64 ((gint64) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT64))
    {
      if (u <= G_MAXUINT64)
        variant = g_variant_new_uint64 ((guint64) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_HANDLE))
    {
      if (u <= G_MAXUINT32)
        variant = g_variant_new_handle ((guint) u);
    }
  else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_DOUBLE))
    variant = g_variant_new_double ((gdouble) u);

  return variant;
}

static gboolean
g_settings_get_mapping_int (GValue   *value,
                            GVariant *variant)
{
  const GVariantType *type;
  gint64 l;

  type = g_variant_get_type (variant);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
    l = g_variant_get_int16 (variant);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    l = g_variant_get_int32 (variant);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
    l = g_variant_get_int64 (variant);
  else
    return FALSE;

  if (G_VALUE_HOLDS_INT (value))
    {
      g_value_set_int (value, l);
      return (G_MININT32 <= l && l <= G_MAXINT32);
    }
  else if (G_VALUE_HOLDS_UINT (value))
    {
      g_value_set_uint (value, l);
      return (0 <= l && l <= G_MAXUINT32);
    }
  else if (G_VALUE_HOLDS_INT64 (value))
    {
      g_value_set_int64 (value, l);
      return (G_MININT64 <= l && l <= G_MAXINT64);
    }
  else if (G_VALUE_HOLDS_UINT64 (value))
    {
      g_value_set_uint64 (value, l);
      return (0 <= l && l <= G_MAXUINT64);
    }
  else if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_set_double (value, l);
      return TRUE;
    }

  return FALSE;
}

static gboolean
g_settings_get_mapping_float (GValue   *value,
                              GVariant *variant)
{
  const GVariantType *type;
  gdouble d;
  gint64 l;

  type = g_variant_get_type (variant);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
    d = g_variant_get_double (variant);
  else
    return FALSE;

  l = (gint64)d;
  if (G_VALUE_HOLDS_INT (value))
    {
      g_value_set_int (value, l);
      return (G_MININT32 <= l && l <= G_MAXINT32);
    }
  else if (G_VALUE_HOLDS_UINT (value))
    {
      g_value_set_uint (value, l);
      return (0 <= l && l <= G_MAXUINT32);
    }
  else if (G_VALUE_HOLDS_INT64 (value))
    {
      g_value_set_int64 (value, l);
      return (G_MININT64 <= l && l <= G_MAXINT64);
    }
  else if (G_VALUE_HOLDS_UINT64 (value))
    {
      g_value_set_uint64 (value, l);
      return (0 <= l && l <= G_MAXUINT64);
    }
  else if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_set_double (value, d);
      return TRUE;
    }

  return FALSE;
}
static gboolean
g_settings_get_mapping_unsigned_int (GValue   *value,
                                     GVariant *variant)
{
  const GVariantType *type;
  guint64 u;

  type = g_variant_get_type (variant);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
    u = g_variant_get_uint16 (variant);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    u = g_variant_get_uint32 (variant);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
    u = g_variant_get_uint64 (variant);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_HANDLE))
    u = g_variant_get_handle (variant);
  else
    return FALSE;

  if (G_VALUE_HOLDS_INT (value))
    {
      g_value_set_int (value, u);
      return (u <= G_MAXINT32);
    }
  else if (G_VALUE_HOLDS_UINT (value))
    {
      g_value_set_uint (value, u);
      return (u <= G_MAXUINT32);
    }
  else if (G_VALUE_HOLDS_INT64 (value))
    {
      g_value_set_int64 (value, u);
      return (u <= G_MAXINT64);
    }
  else if (G_VALUE_HOLDS_UINT64 (value))
    {
      g_value_set_uint64 (value, u);
      return (u <= G_MAXUINT64);
    }
  else if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_set_double (value, u);
      return TRUE;
    }

  return FALSE;
}

GVariant *
g_settings_set_mapping (const GValue       *value,
                        const GVariantType *expected_type,
                        gpointer            user_data)
{
  gchar *type_string;

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_BOOLEAN))
        return g_variant_new_boolean (g_value_get_boolean (value));
    }

  else if (G_VALUE_HOLDS_CHAR (value)  ||
           G_VALUE_HOLDS_UCHAR (value))
    {
      if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_BYTE))
        {
          if (G_VALUE_HOLDS_CHAR (value))
            return g_variant_new_byte (g_value_get_schar (value));
          else
            return g_variant_new_byte (g_value_get_uchar (value));
        }
    }

  else if (G_VALUE_HOLDS_INT (value)   ||
           G_VALUE_HOLDS_INT64 (value))
    return g_settings_set_mapping_int (value, expected_type);

  else if (G_VALUE_HOLDS_DOUBLE (value))
    return g_settings_set_mapping_float (value, expected_type);

  else if (G_VALUE_HOLDS_UINT (value)  ||
           G_VALUE_HOLDS_UINT64 (value))
    return g_settings_set_mapping_unsigned_int (value, expected_type);

  else if (G_VALUE_HOLDS_STRING (value))
    {
      if (g_value_get_string (value) == NULL)
        return NULL;
      else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_STRING))
        return g_variant_new_string (g_value_get_string (value));
      else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_BYTESTRING))
        return g_variant_new_bytestring (g_value_get_string (value));
      else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_OBJECT_PATH))
        return g_variant_new_object_path (g_value_get_string (value));
      else if (g_variant_type_equal (expected_type, G_VARIANT_TYPE_SIGNATURE))
        return g_variant_new_signature (g_value_get_string (value));
    }

  else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
    {
      if (g_value_get_boxed (value) == NULL)
        return NULL;
      return g_variant_new_strv ((const gchar **) g_value_get_boxed (value),
                                 -1);
    }

  else if (G_VALUE_HOLDS_ENUM (value))
    {
      GEnumValue *enumval;
      GEnumClass *eclass;

      /* GParamSpecEnum holds a ref on the class so we just peek... */
      eclass = g_type_class_peek (G_VALUE_TYPE (value));
      enumval = g_enum_get_value (eclass, g_value_get_enum (value));

      if (enumval)
        return g_variant_new_string (enumval->value_nick);
      else
        return NULL;
    }

  else if (G_VALUE_HOLDS_FLAGS (value))
    {
      GVariantBuilder builder;
      GFlagsValue *flagsval;
      GFlagsClass *fclass;
      guint flags;

      fclass = g_type_class_peek (G_VALUE_TYPE (value));
      flags = g_value_get_flags (value);

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
      while (flags)
        {
          flagsval = g_flags_get_first_value (fclass, flags);

          if (flagsval == NULL)
            {
              g_variant_builder_clear (&builder);
              return NULL;
            }

          g_variant_builder_add (&builder, "s", flagsval->value_nick);
          flags &= ~flagsval->value;
        }

      return g_variant_builder_end (&builder);
    }

  type_string = g_variant_type_dup_string (expected_type);
  g_critical ("No GSettings bind handler for type \"%s\".", type_string);
  g_free (type_string);

  return NULL;
}

gboolean
g_settings_get_mapping (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN))
    {
      if (!G_VALUE_HOLDS_BOOLEAN (value))
        return FALSE;
      g_value_set_boolean (value, g_variant_get_boolean (variant));
      return TRUE;
    }

  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BYTE))
    {
      if (G_VALUE_HOLDS_UCHAR (value))
        g_value_set_uchar (value, g_variant_get_byte (variant));
      else if (G_VALUE_HOLDS_CHAR (value))
        g_value_set_schar (value, (gint8)g_variant_get_byte (variant));
      else
        return FALSE;
      return TRUE;
    }

  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT16)  ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32)  ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_INT64))
    return g_settings_get_mapping_int (value, variant);

  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_DOUBLE))
    return g_settings_get_mapping_float (value, variant);

  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT16) ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT32) ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_UINT64) ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_HANDLE))
    return g_settings_get_mapping_unsigned_int (value, variant);

  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING)      ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_OBJECT_PATH) ||
           g_variant_is_of_type (variant, G_VARIANT_TYPE_SIGNATURE))
    {
      if (G_VALUE_HOLDS_STRING (value))
        {
          g_value_set_string (value, g_variant_get_string (variant, NULL));
          return TRUE;
        }

      else if (G_VALUE_HOLDS_ENUM (value))
        {
          GEnumClass *eclass;
          GEnumValue *evalue;
          const gchar *nick;

          /* GParamSpecEnum holds a ref on the class so we just peek... */
          eclass = g_type_class_peek (G_VALUE_TYPE (value));
          nick = g_variant_get_string (variant, NULL);
          evalue = g_enum_get_value_by_nick (eclass, nick);

          if (evalue)
            {
             g_value_set_enum (value, evalue->value);
             return TRUE;
            }

          g_warning ("Unable to lookup enum nick '%s' via GType\n", nick);
          return FALSE;
        }
    }
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE ("as")))
    {
      if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          g_value_take_boxed (value, g_variant_dup_strv (variant, NULL));
          return TRUE;
        }

      else if (G_VALUE_HOLDS_FLAGS (value))
        {
          GFlagsClass *fclass;
          GFlagsValue *fvalue;
          const gchar *nick;
          GVariantIter iter;
          guint flags = 0;

          fclass = g_type_class_peek (G_VALUE_TYPE (value));

          g_variant_iter_init (&iter, variant);
          while (g_variant_iter_next (&iter, "&s", &nick))
            {
              fvalue = g_flags_get_value_by_nick (fclass, nick);

              if (fvalue)
                flags |= fvalue->value;

              else
                {
                  g_warning ("Unable to lookup flags nick '%s' via GType\n",
                             nick);
                  return FALSE;
                }
            }

          g_value_set_flags (value, flags);
          return TRUE;
        }
    }
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BYTESTRING))
    {
      g_value_set_string (value, g_variant_get_bytestring (variant));
      return TRUE;
    }

  g_critical ("No GSettings bind handler for type \"%s\".",
              g_variant_get_type_string (variant));

  return FALSE;
}

gboolean
g_settings_mapping_is_compatible (GType               gvalue_type,
                                  const GVariantType *variant_type)
{
  gboolean ok = FALSE;

  if (gvalue_type == G_TYPE_BOOLEAN)
    ok = g_variant_type_equal (variant_type, G_VARIANT_TYPE_BOOLEAN);
  else if (gvalue_type == G_TYPE_CHAR  ||
           gvalue_type == G_TYPE_UCHAR)
    ok = g_variant_type_equal (variant_type, G_VARIANT_TYPE_BYTE);
  else if (gvalue_type == G_TYPE_INT    ||
           gvalue_type == G_TYPE_UINT   ||
           gvalue_type == G_TYPE_INT64  ||
           gvalue_type == G_TYPE_UINT64 ||
           gvalue_type == G_TYPE_DOUBLE)
    ok = (g_variant_type_equal (variant_type, G_VARIANT_TYPE_INT16)  ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_UINT16) ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_INT32)  ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_UINT32) ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_INT64)  ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_UINT64) ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_HANDLE) ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_DOUBLE));
  else if (gvalue_type == G_TYPE_STRING)
    ok = (g_variant_type_equal (variant_type, G_VARIANT_TYPE_STRING)      ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE ("ay")) ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_OBJECT_PATH) ||
          g_variant_type_equal (variant_type, G_VARIANT_TYPE_SIGNATURE));
  else if (gvalue_type == G_TYPE_STRV)
    ok = g_variant_type_equal (variant_type, G_VARIANT_TYPE ("as"));
  else if (G_TYPE_IS_ENUM (gvalue_type))
    ok = g_variant_type_equal (variant_type, G_VARIANT_TYPE_STRING);
  else if (G_TYPE_IS_FLAGS (gvalue_type))
    ok = g_variant_type_equal (variant_type, G_VARIANT_TYPE ("as"));

  return ok;
}
