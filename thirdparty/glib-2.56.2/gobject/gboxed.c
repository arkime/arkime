/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Red Hat, Inc.
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
 */

#include "config.h"

#include <string.h>

/* for GValueArray */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gboxed.h"
#include "gclosure.h"
#include "gtype-private.h"
#include "gvalue.h"
#include "gvaluearray.h"
#include "gvaluecollector.h"


/**
 * SECTION:gboxed
 * @short_description: A mechanism to wrap opaque C structures registered
 *     by the type system
 * @see_also: #GParamSpecBoxed, g_param_spec_boxed()
 * @title: Boxed Types
 *
 * GBoxed is a generic wrapper mechanism for arbitrary C structures. The only
 * thing the type system needs to know about the structures is how to copy and
 * free them, beyond that they are treated as opaque chunks of memory.
 *
 * Boxed types are useful for simple value-holder structures like rectangles or
 * points. They can also be used for wrapping structures defined in non-GObject
 * based libraries.
 */

static inline void              /* keep this function in sync with gvalue.c */
value_meminit (GValue *value,
	       GType   value_type)
{
  value->g_type = value_type;
  memset (value->data, 0, sizeof (value->data));
}

static GValue *
value_copy (GValue *src_value)
{
  GValue *dest_value = g_new0 (GValue, 1);

  if (G_VALUE_TYPE (src_value))
    {
      g_value_init (dest_value, G_VALUE_TYPE (src_value));
      g_value_copy (src_value, dest_value);
    }
  return dest_value;
}

static void
value_free (GValue *value)
{
  if (G_VALUE_TYPE (value))
    g_value_unset (value);
  g_free (value);
}

static GPollFD *
pollfd_copy (GPollFD *src)
{
  GPollFD *dest = g_new0 (GPollFD, 1);
  /* just a couple of integers */
  memcpy (dest, src, sizeof (GPollFD));
  return dest;
}

void
_g_boxed_type_init (void)
{
  const GTypeInfo info = {
    0,                          /* class_size */
    NULL,                       /* base_init */
    NULL,                       /* base_destroy */
    NULL,                       /* class_init */
    NULL,                       /* class_destroy */
    NULL,                       /* class_data */
    0,                          /* instance_size */
    0,                          /* n_preallocs */
    NULL,                       /* instance_init */
    NULL,                       /* value_table */
  };
  const GTypeFundamentalInfo finfo = { G_TYPE_FLAG_DERIVABLE, };
  GType type;

  /* G_TYPE_BOXED
   */
  type = g_type_register_fundamental (G_TYPE_BOXED, g_intern_static_string ("GBoxed"), &info, &finfo,
				      G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT);
  g_assert (type == G_TYPE_BOXED);
}

static GString *
gstring_copy (GString *src_gstring)
{
  return g_string_new_len (src_gstring->str, src_gstring->len);
}

static void
gstring_free (GString *gstring)
{
  g_string_free (gstring, TRUE);
}

G_DEFINE_BOXED_TYPE (GClosure, g_closure, g_closure_ref, g_closure_unref)
G_DEFINE_BOXED_TYPE (GValue, g_value, value_copy, value_free)
G_DEFINE_BOXED_TYPE (GValueArray, g_value_array, g_value_array_copy, g_value_array_free)
G_DEFINE_BOXED_TYPE (GDate, g_date, g_date_copy, g_date_free)
/* the naming is a bit odd, but GString is obviously not G_TYPE_STRING */
G_DEFINE_BOXED_TYPE (GString, g_gstring, gstring_copy, gstring_free)
G_DEFINE_BOXED_TYPE (GHashTable, g_hash_table, g_hash_table_ref, g_hash_table_unref)
G_DEFINE_BOXED_TYPE (GArray, g_array, g_array_ref, g_array_unref)
G_DEFINE_BOXED_TYPE (GPtrArray, g_ptr_array,g_ptr_array_ref, g_ptr_array_unref)
G_DEFINE_BOXED_TYPE (GByteArray, g_byte_array, g_byte_array_ref, g_byte_array_unref)
G_DEFINE_BOXED_TYPE (GBytes, g_bytes, g_bytes_ref, g_bytes_unref)

G_DEFINE_BOXED_TYPE (GRegex, g_regex, g_regex_ref, g_regex_unref)
G_DEFINE_BOXED_TYPE (GMatchInfo, g_match_info, g_match_info_ref, g_match_info_unref)

#define g_variant_type_get_type g_variant_type_get_gtype
G_DEFINE_BOXED_TYPE (GVariantType, g_variant_type, g_variant_type_copy, g_variant_type_free)
#undef g_variant_type_get_type

G_DEFINE_BOXED_TYPE (GVariantBuilder, g_variant_builder, g_variant_builder_ref, g_variant_builder_unref)
G_DEFINE_BOXED_TYPE (GVariantDict, g_variant_dict, g_variant_dict_ref, g_variant_dict_unref)

G_DEFINE_BOXED_TYPE (GError, g_error, g_error_copy, g_error_free)

G_DEFINE_BOXED_TYPE (GDateTime, g_date_time, g_date_time_ref, g_date_time_unref)
G_DEFINE_BOXED_TYPE (GTimeZone, g_time_zone, g_time_zone_ref, g_time_zone_unref)
G_DEFINE_BOXED_TYPE (GKeyFile, g_key_file, g_key_file_ref, g_key_file_unref)
G_DEFINE_BOXED_TYPE (GMappedFile, g_mapped_file, g_mapped_file_ref, g_mapped_file_unref)

G_DEFINE_BOXED_TYPE (GMainLoop, g_main_loop, g_main_loop_ref, g_main_loop_unref)
G_DEFINE_BOXED_TYPE (GMainContext, g_main_context, g_main_context_ref, g_main_context_unref)
G_DEFINE_BOXED_TYPE (GSource, g_source, g_source_ref, g_source_unref)
G_DEFINE_BOXED_TYPE (GPollFD, g_pollfd, pollfd_copy, g_free)
G_DEFINE_BOXED_TYPE (GMarkupParseContext, g_markup_parse_context, g_markup_parse_context_ref, g_markup_parse_context_unref)

G_DEFINE_BOXED_TYPE (GThread, g_thread, g_thread_ref, g_thread_unref)
G_DEFINE_BOXED_TYPE (GChecksum, g_checksum, g_checksum_copy, g_checksum_free)

G_DEFINE_BOXED_TYPE (GOptionGroup, g_option_group, g_option_group_ref, g_option_group_unref)

/* This one can't use G_DEFINE_BOXED_TYPE (GStrv, g_strv, g_strdupv, g_strfreev) */
GType
g_strv_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      GType g_define_type_id =
        g_boxed_type_register_static (g_intern_static_string ("GStrv"),
                                      (GBoxedCopyFunc) g_strdupv,
                                      (GBoxedFreeFunc) g_strfreev);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_variant_get_gtype (void)
{
  return G_TYPE_VARIANT;
}

static void
boxed_proxy_value_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
boxed_proxy_value_free (GValue *value)
{
  if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    _g_type_boxed_free (G_VALUE_TYPE (value), value->data[0].v_pointer);
}

static void
boxed_proxy_value_copy (const GValue *src_value,
			GValue       *dest_value)
{
  if (src_value->data[0].v_pointer)
    dest_value->data[0].v_pointer = _g_type_boxed_copy (G_VALUE_TYPE (src_value), src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = src_value->data[0].v_pointer;
}

static gpointer
boxed_proxy_value_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
boxed_proxy_collect_value (GValue      *value,
			   guint        n_collect_values,
			   GTypeCValue *collect_values,
			   guint        collect_flags)
{
  if (!collect_values[0].v_pointer)
    value->data[0].v_pointer = NULL;
  else
    {
      if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
	{
	  value->data[0].v_pointer = collect_values[0].v_pointer;
	  value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
	}
      else
	value->data[0].v_pointer = _g_type_boxed_copy (G_VALUE_TYPE (value), collect_values[0].v_pointer);
    }

  return NULL;
}

static gchar*
boxed_proxy_lcopy_value (const GValue *value,
			 guint         n_collect_values,
			 GTypeCValue  *collect_values,
			 guint         collect_flags)
{
  gpointer *boxed_p = collect_values[0].v_pointer;

  if (!boxed_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *boxed_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *boxed_p = value->data[0].v_pointer;
  else
    *boxed_p = _g_type_boxed_copy (G_VALUE_TYPE (value), value->data[0].v_pointer);

  return NULL;
}

/**
 * g_boxed_type_register_static:
 * @name: Name of the new boxed type.
 * @boxed_copy: Boxed structure copy function.
 * @boxed_free: Boxed structure free function.
 *
 * This function creates a new %G_TYPE_BOXED derived type id for a new
 * boxed type with name @name. Boxed type handling functions have to be
 * provided to copy and free opaque boxed structures of this type.
 *
 * Returns: New %G_TYPE_BOXED derived type id for @name.
 */
GType
g_boxed_type_register_static (const gchar   *name,
			      GBoxedCopyFunc boxed_copy,
			      GBoxedFreeFunc boxed_free)
{
  static const GTypeValueTable vtable = {
    boxed_proxy_value_init,
    boxed_proxy_value_free,
    boxed_proxy_value_copy,
    boxed_proxy_value_peek_pointer,
    "p",
    boxed_proxy_collect_value,
    "p",
    boxed_proxy_lcopy_value,
  };
  GTypeInfo type_info = {
    0,			/* class_size */
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    NULL,		/* class_init */
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    0,			/* instance_size */
    0,			/* n_preallocs */
    NULL,		/* instance_init */
    &vtable,		/* value_table */
  };
  GType type;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (boxed_copy != NULL, 0);
  g_return_val_if_fail (boxed_free != NULL, 0);
  g_return_val_if_fail (g_type_from_name (name) == 0, 0);

  type = g_type_register_static (G_TYPE_BOXED, name, &type_info, 0);

  /* install proxy functions upon successful registration */
  if (type)
    _g_type_boxed_init (type, boxed_copy, boxed_free);

  return type;
}

/**
 * g_boxed_copy:
 * @boxed_type: The type of @src_boxed.
 * @src_boxed: (not nullable): The boxed structure to be copied.
 * 
 * Provide a copy of a boxed structure @src_boxed which is of type @boxed_type.
 * 
 * Returns: (transfer full) (not nullable): The newly created copy of the boxed
 *    structure.
 */
gpointer
g_boxed_copy (GType         boxed_type,
	      gconstpointer src_boxed)
{
  GTypeValueTable *value_table;
  gpointer dest_boxed;

  g_return_val_if_fail (G_TYPE_IS_BOXED (boxed_type), NULL);
  g_return_val_if_fail (G_TYPE_IS_ABSTRACT (boxed_type) == FALSE, NULL);
  g_return_val_if_fail (src_boxed != NULL, NULL);

  value_table = g_type_value_table_peek (boxed_type);
  if (!value_table)
    g_return_val_if_fail (G_TYPE_IS_VALUE_TYPE (boxed_type), NULL);

  /* check if our proxying implementation is used, we can short-cut here */
  if (value_table->value_copy == boxed_proxy_value_copy)
    dest_boxed = _g_type_boxed_copy (boxed_type, (gpointer) src_boxed);
  else
    {
      GValue src_value, dest_value;

      /* we heavily rely on third-party boxed type value vtable
       * implementations to follow normal boxed value storage
       * (data[0].v_pointer is the boxed struct, and
       * data[1].v_uint holds the G_VALUE_NOCOPY_CONTENTS flag,
       * rest zero).
       * but then, we can expect that since we laid out the
       * g_boxed_*() API.
       * data[1].v_uint&G_VALUE_NOCOPY_CONTENTS shouldn't be set
       * after a copy.
       */
      /* equiv. to g_value_set_static_boxed() */
      value_meminit (&src_value, boxed_type);
      src_value.data[0].v_pointer = (gpointer) src_boxed;
      src_value.data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;

      /* call third-party code copy function, fingers-crossed */
      value_meminit (&dest_value, boxed_type);
      value_table->value_copy (&src_value, &dest_value);

      /* double check and grouse if things went wrong */
      if (dest_value.data[1].v_ulong)
	g_warning ("the copy_value() implementation of type '%s' seems to make use of reserved GValue fields",
		   g_type_name (boxed_type));

      dest_boxed = dest_value.data[0].v_pointer;
    }

  return dest_boxed;
}

/**
 * g_boxed_free:
 * @boxed_type: The type of @boxed.
 * @boxed: (not nullable): The boxed structure to be freed.
 *
 * Free the boxed structure @boxed which is of type @boxed_type.
 */
void
g_boxed_free (GType    boxed_type,
	      gpointer boxed)
{
  GTypeValueTable *value_table;

  g_return_if_fail (G_TYPE_IS_BOXED (boxed_type));
  g_return_if_fail (G_TYPE_IS_ABSTRACT (boxed_type) == FALSE);
  g_return_if_fail (boxed != NULL);

  value_table = g_type_value_table_peek (boxed_type);
  if (!value_table)
    g_return_if_fail (G_TYPE_IS_VALUE_TYPE (boxed_type));

  /* check if our proxying implementation is used, we can short-cut here */
  if (value_table->value_free == boxed_proxy_value_free)
    _g_type_boxed_free (boxed_type, boxed);
  else
    {
      GValue value;

      /* see g_boxed_copy() on why we think we can do this */
      value_meminit (&value, boxed_type);
      value.data[0].v_pointer = boxed;
      value_table->value_free (&value);
    }
}

/**
 * g_value_get_boxed:
 * @value: a valid #GValue of %G_TYPE_BOXED derived type
 *
 * Get the contents of a %G_TYPE_BOXED derived #GValue.
 *
 * Returns: (transfer none): boxed contents of @value
 */
gpointer
g_value_get_boxed (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (value), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)), NULL);

  return value->data[0].v_pointer;
}

/**
 * g_value_dup_boxed: (skip)
 * @value: a valid #GValue of %G_TYPE_BOXED derived type
 *
 * Get the contents of a %G_TYPE_BOXED derived #GValue.  Upon getting,
 * the boxed value is duplicated and needs to be later freed with
 * g_boxed_free(), e.g. like: g_boxed_free (G_VALUE_TYPE (@value),
 * return_value);
 *
 * Returns: boxed contents of @value
 */
gpointer
g_value_dup_boxed (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (value), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)), NULL);

  return value->data[0].v_pointer ? g_boxed_copy (G_VALUE_TYPE (value), value->data[0].v_pointer) : NULL;
}

static inline void
value_set_boxed_internal (GValue       *value,
			  gconstpointer boxed,
			  gboolean      need_copy,
			  gboolean      need_free)
{
  if (!boxed)
    {
      /* just resetting to NULL might not be desired, need to
       * have value reinitialized also (for values defaulting
       * to other default value states than a NULL data pointer),
       * g_value_reset() will handle this
       */
      g_value_reset (value);
      return;
    }

  if (value->data[0].v_pointer && !(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    g_boxed_free (G_VALUE_TYPE (value), value->data[0].v_pointer);
  value->data[1].v_uint = need_free ? 0 : G_VALUE_NOCOPY_CONTENTS;
  value->data[0].v_pointer = need_copy ? g_boxed_copy (G_VALUE_TYPE (value), boxed) : (gpointer) boxed;
}

/**
 * g_value_set_boxed:
 * @value: a valid #GValue of %G_TYPE_BOXED derived type
 * @v_boxed: (nullable): boxed value to be set
 *
 * Set the contents of a %G_TYPE_BOXED derived #GValue to @v_boxed.
 */
void
g_value_set_boxed (GValue       *value,
		   gconstpointer boxed)
{
  g_return_if_fail (G_VALUE_HOLDS_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  value_set_boxed_internal (value, boxed, TRUE, TRUE);
}

/**
 * g_value_set_static_boxed:
 * @value: a valid #GValue of %G_TYPE_BOXED derived type
 * @v_boxed: (nullable): static boxed value to be set
 *
 * Set the contents of a %G_TYPE_BOXED derived #GValue to @v_boxed.
 * The boxed value is assumed to be static, and is thus not duplicated
 * when setting the #GValue.
 */
void
g_value_set_static_boxed (GValue       *value,
			  gconstpointer boxed)
{
  g_return_if_fail (G_VALUE_HOLDS_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  value_set_boxed_internal (value, boxed, FALSE, FALSE);
}

/**
 * g_value_set_boxed_take_ownership:
 * @value: a valid #GValue of %G_TYPE_BOXED derived type
 * @v_boxed: (nullable): duplicated unowned boxed value to be set
 *
 * This is an internal function introduced mainly for C marshallers.
 *
 * Deprecated: 2.4: Use g_value_take_boxed() instead.
 */
void
g_value_set_boxed_take_ownership (GValue       *value,
				  gconstpointer boxed)
{
  g_value_take_boxed (value, boxed);
}

/**
 * g_value_take_boxed:
 * @value: a valid #GValue of %G_TYPE_BOXED derived type
 * @v_boxed: (nullable): duplicated unowned boxed value to be set
 *
 * Sets the contents of a %G_TYPE_BOXED derived #GValue to @v_boxed
 * and takes over the ownership of the callers reference to @v_boxed;
 * the caller doesn't have to unref it any more.
 *
 * Since: 2.4
 */
void
g_value_take_boxed (GValue       *value,
		    gconstpointer boxed)
{
  g_return_if_fail (G_VALUE_HOLDS_BOXED (value));
  g_return_if_fail (G_TYPE_IS_VALUE (G_VALUE_TYPE (value)));

  value_set_boxed_internal (value, boxed, FALSE, TRUE);
}
