/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gdbusutils.h"

#include "glibintl.h"

/**
 * SECTION:gdbusutils
 * @title: D-Bus Utilities
 * @short_description: Various utilities related to D-Bus
 * @include: gio/gio.h
 *
 * Various utility routines related to D-Bus.
 */

static gboolean
is_valid_bus_name_character (gint c,
                             gboolean allow_hyphen)
{
  return
    (c >= '0' && c <= '9') ||
    (c >= 'A' && c <= 'Z') ||
    (c >= 'a' && c <= 'z') ||
    (c == '_') ||
    (allow_hyphen && c == '-');
}

static gboolean
is_valid_initial_bus_name_character (gint c,
                                     gboolean allow_initial_digit,
                                     gboolean allow_hyphen)
{
  if (allow_initial_digit)
    return is_valid_bus_name_character (c, allow_hyphen);
  else
    return
      (c >= 'A' && c <= 'Z') ||
      (c >= 'a' && c <= 'z') ||
      (c == '_') ||
      (allow_hyphen && c == '-');
}

static gboolean
is_valid_name (const gchar *start,
               guint len,
               gboolean allow_initial_digit,
               gboolean allow_hyphen)
{
  gboolean ret;
  const gchar *s;
  const gchar *end;
  gboolean has_dot;

  ret = FALSE;

  if (len == 0)
    goto out;

  s = start;
  end = s + len;
  has_dot = FALSE;
  while (s != end)
    {
      if (*s == '.')
        {
          s += 1;
          if (G_UNLIKELY (!is_valid_initial_bus_name_character (*s, allow_initial_digit, allow_hyphen)))
            goto out;
          has_dot = TRUE;
        }
      else if (G_UNLIKELY (!is_valid_bus_name_character (*s, allow_hyphen)))
        {
          goto out;
        }
      s += 1;
    }

  if (G_UNLIKELY (!has_dot))
    goto out;

  ret = TRUE;

 out:
  return ret;
}

/**
 * g_dbus_is_name:
 * @string: The string to check.
 *
 * Checks if @string is a valid D-Bus bus name (either unique or well-known).
 *
 * Returns: %TRUE if valid, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_name (const gchar *string)
{
  guint len;
  gboolean ret;
  const gchar *s;

  g_return_val_if_fail (string != NULL, FALSE);

  ret = FALSE;

  len = strlen (string);
  if (G_UNLIKELY (len == 0 || len > 255))
    goto out;

  s = string;
  if (*s == ':')
    {
      /* handle unique name */
      if (!is_valid_name (s + 1, len - 1, TRUE, TRUE))
        goto out;
      ret = TRUE;
      goto out;
    }
  else if (G_UNLIKELY (*s == '.'))
    {
      /* can't start with a . */
      goto out;
    }
  else if (G_UNLIKELY (!is_valid_initial_bus_name_character (*s, FALSE, TRUE)))
    goto out;

  ret = is_valid_name (s + 1, len - 1, FALSE, TRUE);

 out:
  return ret;
}

/**
 * g_dbus_is_unique_name:
 * @string: The string to check.
 *
 * Checks if @string is a valid D-Bus unique bus name.
 *
 * Returns: %TRUE if valid, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_unique_name (const gchar *string)
{
  gboolean ret;
  guint len;

  g_return_val_if_fail (string != NULL, FALSE);

  ret = FALSE;

  len = strlen (string);
  if (G_UNLIKELY (len == 0 || len > 255))
    goto out;

  if (G_UNLIKELY (*string != ':'))
    goto out;

  if (G_UNLIKELY (!is_valid_name (string + 1, len - 1, TRUE, TRUE)))
    goto out;

  ret = TRUE;

 out:
  return ret;
}

/**
 * g_dbus_is_member_name:
 * @string: The string to check.
 *
 * Checks if @string is a valid D-Bus member (e.g. signal or method) name.
 *
 * Returns: %TRUE if valid, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_member_name (const gchar *string)
{
  gboolean ret;
  guint n;

  ret = FALSE;
  if (G_UNLIKELY (string == NULL))
    goto out;

  if (G_UNLIKELY (!is_valid_initial_bus_name_character (string[0], FALSE, FALSE)))
    goto out;

  for (n = 1; string[n] != '\0'; n++)
    {
      if (G_UNLIKELY (!is_valid_bus_name_character (string[n], FALSE)))
        {
          goto out;
        }
    }

  ret = TRUE;

 out:
  return ret;
}

/**
 * g_dbus_is_interface_name:
 * @string: The string to check.
 *
 * Checks if @string is a valid D-Bus interface name.
 *
 * Returns: %TRUE if valid, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_interface_name (const gchar *string)
{
  guint len;
  gboolean ret;
  const gchar *s;

  g_return_val_if_fail (string != NULL, FALSE);

  ret = FALSE;

  len = strlen (string);
  if (G_UNLIKELY (len == 0 || len > 255))
    goto out;

  s = string;
  if (G_UNLIKELY (*s == '.'))
    {
      /* can't start with a . */
      goto out;
    }
  else if (G_UNLIKELY (!is_valid_initial_bus_name_character (*s, FALSE, FALSE)))
    goto out;

  ret = is_valid_name (s + 1, len - 1, FALSE, FALSE);

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* TODO: maybe move to glib? if so, it should conform to http://en.wikipedia.org/wiki/Guid and/or
 *       http://tools.ietf.org/html/rfc4122 - specifically it should have hyphens then.
 */

/**
 * g_dbus_generate_guid:
 *
 * Generate a D-Bus GUID that can be used with
 * e.g. g_dbus_connection_new().
 *
 * See the D-Bus specification regarding what strings are valid D-Bus
 * GUID (for example, D-Bus GUIDs are not RFC-4122 compliant).
 *
 * Returns: A valid D-Bus GUID. Free with g_free().
 *
 * Since: 2.26
 */
gchar *
g_dbus_generate_guid (void)
{
  GString *s;
  GTimeVal now;
  guint32 r1;
  guint32 r2;
  guint32 r3;

  s = g_string_new (NULL);

  r1 = g_random_int ();
  r2 = g_random_int ();
  r3 = g_random_int ();
  g_get_current_time (&now);

  g_string_append_printf (s, "%08x", r1);
  g_string_append_printf (s, "%08x", r2);
  g_string_append_printf (s, "%08x", r3);
  g_string_append_printf (s, "%08x", (guint32) now.tv_sec);

  return g_string_free (s, FALSE);
}

/**
 * g_dbus_is_guid:
 * @string: The string to check.
 *
 * Checks if @string is a D-Bus GUID.
 *
 * See the D-Bus specification regarding what strings are valid D-Bus
 * GUID (for example, D-Bus GUIDs are not RFC-4122 compliant).
 *
 * Returns: %TRUE if @string is a guid, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_guid (const gchar *string)
{
  gboolean ret;
  guint n;

  g_return_val_if_fail (string != NULL, FALSE);

  ret = FALSE;

  for (n = 0; n < 32; n++)
    {
      if (!g_ascii_isxdigit (string[n]))
        goto out;
    }
  if (string[32] != '\0')
    goto out;

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_gvariant_to_gvalue:
 * @value: A #GVariant.
 * @out_gvalue: (out): Return location pointing to a zero-filled (uninitialized) #GValue.
 *
 * Converts a #GVariant to a #GValue. If @value is floating, it is consumed.
 *
 * The rules specified in the g_dbus_gvalue_to_gvariant() function are
 * used - this function is essentially its reverse form. So, a #GVariant
 * containing any basic or string array type will be converted to a #GValue
 * containing a basic value or string array. Any other #GVariant (handle,
 * variant, tuple, dict entry) will be converted to a #GValue containing that
 * #GVariant.
 *
 * The conversion never fails - a valid #GValue is always returned in
 * @out_gvalue.
 *
 * Since: 2.30
 */
void
g_dbus_gvariant_to_gvalue (GVariant  *value,
                           GValue    *out_gvalue)
{
  const GVariantType *type;
  gchar **array;

  g_return_if_fail (value != NULL);
  g_return_if_fail (out_gvalue != NULL);

  memset (out_gvalue, '\0', sizeof (GValue));

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_BOOLEAN:
      g_value_init (out_gvalue, G_TYPE_BOOLEAN);
      g_value_set_boolean (out_gvalue, g_variant_get_boolean (value));
      break;

    case G_VARIANT_CLASS_BYTE:
      g_value_init (out_gvalue, G_TYPE_UCHAR);
      g_value_set_uchar (out_gvalue, g_variant_get_byte (value));
      break;

    case G_VARIANT_CLASS_INT16:
      g_value_init (out_gvalue, G_TYPE_INT);
      g_value_set_int (out_gvalue, g_variant_get_int16 (value));
      break;

    case G_VARIANT_CLASS_UINT16:
      g_value_init (out_gvalue, G_TYPE_UINT);
      g_value_set_uint (out_gvalue, g_variant_get_uint16 (value));
      break;

    case G_VARIANT_CLASS_INT32:
      g_value_init (out_gvalue, G_TYPE_INT);
      g_value_set_int (out_gvalue, g_variant_get_int32 (value));
      break;

    case G_VARIANT_CLASS_UINT32:
      g_value_init (out_gvalue, G_TYPE_UINT);
      g_value_set_uint (out_gvalue, g_variant_get_uint32 (value));
      break;

    case G_VARIANT_CLASS_INT64:
      g_value_init (out_gvalue, G_TYPE_INT64);
      g_value_set_int64 (out_gvalue, g_variant_get_int64 (value));
      break;

    case G_VARIANT_CLASS_UINT64:
      g_value_init (out_gvalue, G_TYPE_UINT64);
      g_value_set_uint64 (out_gvalue, g_variant_get_uint64 (value));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      g_value_init (out_gvalue, G_TYPE_DOUBLE);
      g_value_set_double (out_gvalue, g_variant_get_double (value));
      break;

    case G_VARIANT_CLASS_STRING:
      g_value_init (out_gvalue, G_TYPE_STRING);
      g_value_set_string (out_gvalue, g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_OBJECT_PATH:
      g_value_init (out_gvalue, G_TYPE_STRING);
      g_value_set_string (out_gvalue, g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_SIGNATURE:
      g_value_init (out_gvalue, G_TYPE_STRING);
      g_value_set_string (out_gvalue, g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_ARRAY:
      type = g_variant_get_type (value);
      switch (g_variant_type_peek_string (type)[1])
        {
        case G_VARIANT_CLASS_BYTE:
          g_value_init (out_gvalue, G_TYPE_STRING);
          g_value_set_string (out_gvalue, g_variant_get_bytestring (value));
          break;

        case G_VARIANT_CLASS_STRING:
          g_value_init (out_gvalue, G_TYPE_STRV);
          array = g_variant_dup_strv (value, NULL);
          g_value_take_boxed (out_gvalue, array);
          break;

        case G_VARIANT_CLASS_OBJECT_PATH:
          g_value_init (out_gvalue, G_TYPE_STRV);
          array = g_variant_dup_objv (value, NULL);
          g_value_take_boxed (out_gvalue, array);
          break;

        case G_VARIANT_CLASS_ARRAY:
          switch (g_variant_type_peek_string (type)[2])
            {
            case G_VARIANT_CLASS_BYTE:
              g_value_init (out_gvalue, G_TYPE_STRV);
              array = g_variant_dup_bytestring_array (value, NULL);
              g_value_take_boxed (out_gvalue, array);
              break;

            default:
              g_value_init (out_gvalue, G_TYPE_VARIANT);
              g_value_set_variant (out_gvalue, value);
              break;
            }
          break;

        default:
          g_value_init (out_gvalue, G_TYPE_VARIANT);
          g_value_set_variant (out_gvalue, value);
          break;
        }
      break;

    case G_VARIANT_CLASS_HANDLE:
    case G_VARIANT_CLASS_VARIANT:
    case G_VARIANT_CLASS_MAYBE:
    case G_VARIANT_CLASS_TUPLE:
    case G_VARIANT_CLASS_DICT_ENTRY:
      g_value_init (out_gvalue, G_TYPE_VARIANT);
      g_value_set_variant (out_gvalue, value);
      break;
    }
}


/**
 * g_dbus_gvalue_to_gvariant:
 * @gvalue: A #GValue to convert to a #GVariant
 * @type: A #GVariantType
 *
 * Converts a #GValue to a #GVariant of the type indicated by the @type
 * parameter.
 *
 * The conversion is using the following rules:
 *
 * - #G_TYPE_STRING: 's', 'o', 'g' or 'ay'
 * - #G_TYPE_STRV: 'as', 'ao' or 'aay'
 * - #G_TYPE_BOOLEAN: 'b'
 * - #G_TYPE_UCHAR: 'y'
 * - #G_TYPE_INT: 'i', 'n'
 * - #G_TYPE_UINT: 'u', 'q'
 * - #G_TYPE_INT64 'x'
 * - #G_TYPE_UINT64: 't'
 * - #G_TYPE_DOUBLE: 'd'
 * - #G_TYPE_VARIANT: Any #GVariantType
 *
 * This can fail if e.g. @gvalue is of type #G_TYPE_STRING and @type
 * is ['i'][G-VARIANT-TYPE-INT32:CAPS]. It will also fail for any #GType
 * (including e.g. #G_TYPE_OBJECT and #G_TYPE_BOXED derived-types) not
 * in the table above.
 *
 * Note that if @gvalue is of type #G_TYPE_VARIANT and its value is
 * %NULL, the empty #GVariant instance (never %NULL) for @type is
 * returned (e.g. 0 for scalar types, the empty string for string types,
 * '/' for object path types, the empty array for any array type and so on).
 *
 * See the g_dbus_gvariant_to_gvalue() function for how to convert a
 * #GVariant to a #GValue.
 *
 * Returns: A #GVariant (never floating) of #GVariantType @type holding
 *     the data from @gvalue or %NULL in case of failure. Free with
 *     g_variant_unref().
 *
 * Since: 2.30
 */
GVariant *
g_dbus_gvalue_to_gvariant (const GValue       *gvalue,
                           const GVariantType *type)
{
  GVariant *ret;
  const gchar *s;
  const gchar * const *as;
  const gchar *empty_strv[1] = {NULL};

  g_return_val_if_fail (gvalue != NULL, NULL);
  g_return_val_if_fail (type != NULL, NULL);

  ret = NULL;

  /* @type can easily be e.g. "s" with the GValue holding a GVariant - for example this
   * can happen when using the org.gtk.GDBus.C.ForceGVariant annotation with the
   * gdbus-codegen(1) tool.
   */
  if (G_VALUE_TYPE (gvalue) == G_TYPE_VARIANT)
    {
      ret = g_value_dup_variant (gvalue);
    }
  else
    {
      switch (g_variant_type_peek_string (type)[0])
        {
        case G_VARIANT_CLASS_BOOLEAN:
          ret = g_variant_ref_sink (g_variant_new_boolean (g_value_get_boolean (gvalue)));
          break;

        case G_VARIANT_CLASS_BYTE:
          ret = g_variant_ref_sink (g_variant_new_byte (g_value_get_uchar (gvalue)));
          break;

        case G_VARIANT_CLASS_INT16:
          ret = g_variant_ref_sink (g_variant_new_int16 (g_value_get_int (gvalue)));
          break;

        case G_VARIANT_CLASS_UINT16:
          ret = g_variant_ref_sink (g_variant_new_uint16 (g_value_get_uint (gvalue)));
          break;

        case G_VARIANT_CLASS_INT32:
          ret = g_variant_ref_sink (g_variant_new_int32 (g_value_get_int (gvalue)));
          break;

        case G_VARIANT_CLASS_UINT32:
          ret = g_variant_ref_sink (g_variant_new_uint32 (g_value_get_uint (gvalue)));
          break;

        case G_VARIANT_CLASS_INT64:
          ret = g_variant_ref_sink (g_variant_new_int64 (g_value_get_int64 (gvalue)));
          break;

        case G_VARIANT_CLASS_UINT64:
          ret = g_variant_ref_sink (g_variant_new_uint64 (g_value_get_uint64 (gvalue)));
          break;

        case G_VARIANT_CLASS_DOUBLE:
          ret = g_variant_ref_sink (g_variant_new_double (g_value_get_double (gvalue)));
          break;

        case G_VARIANT_CLASS_STRING:
          s = g_value_get_string (gvalue);
          if (s == NULL)
            s = "";
          ret = g_variant_ref_sink (g_variant_new_string (s));
          break;

        case G_VARIANT_CLASS_OBJECT_PATH:
          s = g_value_get_string (gvalue);
          if (s == NULL)
            s = "/";
          ret = g_variant_ref_sink (g_variant_new_object_path (s));
          break;

        case G_VARIANT_CLASS_SIGNATURE:
          s = g_value_get_string (gvalue);
          if (s == NULL)
            s = "";
          ret = g_variant_ref_sink (g_variant_new_signature (s));
          break;

        case G_VARIANT_CLASS_ARRAY:
          switch (g_variant_type_peek_string (type)[1])
            {
            case G_VARIANT_CLASS_BYTE:
              s = g_value_get_string (gvalue);
              if (s == NULL)
                s = "";
              ret = g_variant_ref_sink (g_variant_new_bytestring (s));
              break;

            case G_VARIANT_CLASS_STRING:
              as = g_value_get_boxed (gvalue);
              if (as == NULL)
                as = empty_strv;
              ret = g_variant_ref_sink (g_variant_new_strv (as, -1));
              break;

            case G_VARIANT_CLASS_OBJECT_PATH:
              as = g_value_get_boxed (gvalue);
              if (as == NULL)
                as = empty_strv;
              ret = g_variant_ref_sink (g_variant_new_objv (as, -1));
              break;

            case G_VARIANT_CLASS_ARRAY:
              switch (g_variant_type_peek_string (type)[2])
                {
                case G_VARIANT_CLASS_BYTE:
                  as = g_value_get_boxed (gvalue);
                  if (as == NULL)
                    as = empty_strv;
                  ret = g_variant_ref_sink (g_variant_new_bytestring_array (as, -1));
                  break;

                default:
                  ret = g_value_dup_variant (gvalue);
                  break;
                }
              break;

            default:
              ret = g_value_dup_variant (gvalue);
              break;
            }
          break;

        case G_VARIANT_CLASS_HANDLE:
        case G_VARIANT_CLASS_VARIANT:
        case G_VARIANT_CLASS_MAYBE:
        case G_VARIANT_CLASS_TUPLE:
        case G_VARIANT_CLASS_DICT_ENTRY:
          ret = g_value_dup_variant (gvalue);
          break;
        }
    }

  /* Could be that the GValue is holding a NULL GVariant - in that case,
   * we return an "empty" GVariant instead of a NULL GVariant
   */
  if (ret == NULL)
    {
      GVariant *untrusted_empty;
      untrusted_empty = g_variant_new_from_data (type, NULL, 0, FALSE, NULL, NULL);
      ret = g_variant_take_ref (g_variant_get_normal_form (untrusted_empty));
      g_variant_unref (untrusted_empty);
    }

  g_assert (!g_variant_is_floating (ret));

  return ret;
}
