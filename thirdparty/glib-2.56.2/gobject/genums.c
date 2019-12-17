/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe
 */

#include "config.h"

#include <string.h>

#include "genums.h"
#include "gtype-private.h"
#include "gvalue.h"
#include "gvaluecollector.h"


/**
 * SECTION:enumerations_flags
 * @short_description: Enumeration and flags types
 * @title: Enumeration and Flag Types
 * @see_also:#GParamSpecEnum, #GParamSpecFlags, g_param_spec_enum(),
 * g_param_spec_flags()
 *
 * The GLib type system provides fundamental types for enumeration and
 * flags types. (Flags types are like enumerations, but allow their
 * values to be combined by bitwise or). A registered enumeration or
 * flags type associates a name and a nickname with each allowed
 * value, and the methods g_enum_get_value_by_name(),
 * g_enum_get_value_by_nick(), g_flags_get_value_by_name() and
 * g_flags_get_value_by_nick() can look up values by their name or
 * nickname.  When an enumeration or flags type is registered with the
 * GLib type system, it can be used as value type for object
 * properties, using g_param_spec_enum() or g_param_spec_flags().
 *
 * GObject ships with a utility called [glib-mkenums][glib-mkenums],
 * that can construct suitable type registration functions from C enumeration
 * definitions.
 *
 * Example of how to get a string representation of an enum value:
 * |[<!-- language="C" -->
 * GEnumClass *enum_class;
 * GEnumValue *enum_value;
 *
 * enum_class = g_type_class_ref (MAMAN_TYPE_MY_ENUM);
 * enum_value = g_enum_get_value (enum_class, MAMAN_MY_ENUM_FOO);
 *
 * g_print ("Name: %s\n", enum_value->value_name);
 *
 * g_type_class_unref (enum_class);
 * ]|
 */


/* --- prototypes --- */
static void	g_enum_class_init		(GEnumClass	*class,
						 gpointer	 class_data);
static void	g_flags_class_init		(GFlagsClass	*class,
						 gpointer	 class_data);
static void	value_flags_enum_init		(GValue		*value);
static void	value_flags_enum_copy_value	(const GValue	*src_value,
						 GValue		*dest_value);
static gchar*	value_flags_enum_collect_value  (GValue		*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);
static gchar*	value_flags_enum_lcopy_value	(const GValue	*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);

/* --- functions --- */
void
_g_enum_types_init (void)
{
  static gboolean initialized = FALSE;
  static const GTypeValueTable flags_enum_value_table = {
    value_flags_enum_init,	    /* value_init */
    NULL,			    /* value_free */
    value_flags_enum_copy_value,    /* value_copy */
    NULL,			    /* value_peek_pointer */
    "i",			    /* collect_format */
    value_flags_enum_collect_value, /* collect_value */
    "p",			    /* lcopy_format */
    value_flags_enum_lcopy_value,   /* lcopy_value */
  };
  GTypeInfo info = {
    0,                          /* class_size */
    NULL,                       /* base_init */
    NULL,                       /* base_destroy */
    NULL,                       /* class_init */
    NULL,                       /* class_destroy */
    NULL,                       /* class_data */
    0,                          /* instance_size */
    0,                          /* n_preallocs */
    NULL,                       /* instance_init */
    &flags_enum_value_table,    /* value_table */
  };
  static const GTypeFundamentalInfo finfo = {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_DERIVABLE,
  };
  GType type;
  
  g_return_if_fail (initialized == FALSE);
  initialized = TRUE;
  
  /* G_TYPE_ENUM
   */
  info.class_size = sizeof (GEnumClass);
  type = g_type_register_fundamental (G_TYPE_ENUM, g_intern_static_string ("GEnum"), &info, &finfo,
				      G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT);
  g_assert (type == G_TYPE_ENUM);
  
  /* G_TYPE_FLAGS
   */
  info.class_size = sizeof (GFlagsClass);
  type = g_type_register_fundamental (G_TYPE_FLAGS, g_intern_static_string ("GFlags"), &info, &finfo,
				      G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT);
  g_assert (type == G_TYPE_FLAGS);
}

static void
value_flags_enum_init (GValue *value)
{
  value->data[0].v_long = 0;
}

static void
value_flags_enum_copy_value (const GValue *src_value,
			     GValue	  *dest_value)
{
  dest_value->data[0].v_long = src_value->data[0].v_long;
}

static gchar*
value_flags_enum_collect_value (GValue      *value,
				guint        n_collect_values,
				GTypeCValue *collect_values,
				guint        collect_flags)
{
  value->data[0].v_long = collect_values[0].v_int;

  return NULL;
}

static gchar*
value_flags_enum_lcopy_value (const GValue *value,
			      guint         n_collect_values,
			      GTypeCValue  *collect_values,
			      guint         collect_flags)
{
  gint *int_p = collect_values[0].v_pointer;
  
  if (!int_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int_p = value->data[0].v_long;
  
  return NULL;
}

/**
 * g_enum_register_static:
 * @name: A nul-terminated string used as the name of the new type.
 * @const_static_values: An array of #GEnumValue structs for the possible
 *  enumeration values. The array is terminated by a struct with all
 *  members being 0. GObject keeps a reference to the data, so it cannot
 *  be stack-allocated.
 *
 * Registers a new static enumeration type with the name @name.
 *
 * It is normally more convenient to let [glib-mkenums][glib-mkenums],
 * generate a my_enum_get_type() function from a usual C enumeration
 * definition  than to write one yourself using g_enum_register_static().
 *
 * Returns: The new type identifier.
 */
GType
g_enum_register_static (const gchar	 *name,
			const GEnumValue *const_static_values)
{
  GTypeInfo enum_type_info = {
    sizeof (GEnumClass), /* class_size */
    NULL,                /* base_init */
    NULL,                /* base_finalize */
    (GClassInitFunc) g_enum_class_init,
    NULL,                /* class_finalize */
    NULL,                /* class_data */
    0,                   /* instance_size */
    0,                   /* n_preallocs */
    NULL,                /* instance_init */
    NULL,		 /* value_table */
  };
  GType type;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);
  
  enum_type_info.class_data = const_static_values;
  
  type = g_type_register_static (G_TYPE_ENUM, name, &enum_type_info, 0);
  
  return type;
}

/**
 * g_flags_register_static:
 * @name: A nul-terminated string used as the name of the new type.
 * @const_static_values: An array of #GFlagsValue structs for the possible
 *  flags values. The array is terminated by a struct with all members being 0.
 *  GObject keeps a reference to the data, so it cannot be stack-allocated.
 *
 * Registers a new static flags type with the name @name.
 *
 * It is normally more convenient to let [glib-mkenums][glib-mkenums]
 * generate a my_flags_get_type() function from a usual C enumeration
 * definition than to write one yourself using g_flags_register_static().
 *
 * Returns: The new type identifier.
 */
GType
g_flags_register_static (const gchar	   *name,
			 const GFlagsValue *const_static_values)
{
  GTypeInfo flags_type_info = {
    sizeof (GFlagsClass), /* class_size */
    NULL,                 /* base_init */
    NULL,                 /* base_finalize */
    (GClassInitFunc) g_flags_class_init,
    NULL,                 /* class_finalize */
    NULL,                 /* class_data */
    0,                    /* instance_size */
    0,                    /* n_preallocs */
    NULL,                 /* instance_init */
    NULL,		  /* value_table */
  };
  GType type;
  
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);
  
  flags_type_info.class_data = const_static_values;
  
  type = g_type_register_static (G_TYPE_FLAGS, name, &flags_type_info, 0);
  
  return type;
}

/**
 * g_enum_complete_type_info:
 * @g_enum_type: the type identifier of the type being completed
 * @info: (out callee-allocates): the #GTypeInfo struct to be filled in
 * @const_values: An array of #GEnumValue structs for the possible
 *  enumeration values. The array is terminated by a struct with all
 *  members being 0.
 *
 * This function is meant to be called from the `complete_type_info`
 * function of a #GTypePlugin implementation, as in the following
 * example:
 *
 * |[<!-- language="C" --> 
 * static void
 * my_enum_complete_type_info (GTypePlugin     *plugin,
 *                             GType            g_type,
 *                             GTypeInfo       *info,
 *                             GTypeValueTable *value_table)
 * {
 *   static const GEnumValue values[] = {
 *     { MY_ENUM_FOO, "MY_ENUM_FOO", "foo" },
 *     { MY_ENUM_BAR, "MY_ENUM_BAR", "bar" },
 *     { 0, NULL, NULL }
 *   };
 *
 *   g_enum_complete_type_info (type, info, values);
 * }
 * ]|
 */
void
g_enum_complete_type_info (GType	     g_enum_type,
			   GTypeInfo	    *info,
			   const GEnumValue *const_values)
{
  g_return_if_fail (G_TYPE_IS_ENUM (g_enum_type));
  g_return_if_fail (info != NULL);
  g_return_if_fail (const_values != NULL);
  
  info->class_size = sizeof (GEnumClass);
  info->base_init = NULL;
  info->base_finalize = NULL;
  info->class_init = (GClassInitFunc) g_enum_class_init;
  info->class_finalize = NULL;
  info->class_data = const_values;
}

/**
 * g_flags_complete_type_info:
 * @g_flags_type: the type identifier of the type being completed
 * @info: (out callee-allocates): the #GTypeInfo struct to be filled in
 * @const_values: An array of #GFlagsValue structs for the possible
 *  enumeration values. The array is terminated by a struct with all
 *  members being 0.
 *
 * This function is meant to be called from the complete_type_info()
 * function of a #GTypePlugin implementation, see the example for
 * g_enum_complete_type_info() above.
 */
void
g_flags_complete_type_info (GType	       g_flags_type,
			    GTypeInfo	      *info,
			    const GFlagsValue *const_values)
{
  g_return_if_fail (G_TYPE_IS_FLAGS (g_flags_type));
  g_return_if_fail (info != NULL);
  g_return_if_fail (const_values != NULL);
  
  info->class_size = sizeof (GFlagsClass);
  info->base_init = NULL;
  info->base_finalize = NULL;
  info->class_init = (GClassInitFunc) g_flags_class_init;
  info->class_finalize = NULL;
  info->class_data = const_values;
}

static void
g_enum_class_init (GEnumClass *class,
		   gpointer    class_data)
{
  g_return_if_fail (G_IS_ENUM_CLASS (class));
  
  class->minimum = 0;
  class->maximum = 0;
  class->n_values = 0;
  class->values = class_data;
  
  if (class->values)
    {
      GEnumValue *values;
      
      class->minimum = class->values->value;
      class->maximum = class->values->value;
      for (values = class->values; values->value_name; values++)
	{
	  class->minimum = MIN (class->minimum, values->value);
	  class->maximum = MAX (class->maximum, values->value);
	  class->n_values++;
	}
    }
}

static void
g_flags_class_init (GFlagsClass *class,
		    gpointer	 class_data)
{
  g_return_if_fail (G_IS_FLAGS_CLASS (class));
  
  class->mask = 0;
  class->n_values = 0;
  class->values = class_data;
  
  if (class->values)
    {
      GFlagsValue *values;
      
      for (values = class->values; values->value_name; values++)
	{
	  class->mask |= values->value;
	  class->n_values++;
	}
    }
}

/**
 * g_enum_get_value_by_name:
 * @enum_class: a #GEnumClass
 * @name: the name to look up
 *
 * Looks up a #GEnumValue by name.
 *
 * Returns: (transfer none): the #GEnumValue with name @name,
 *          or %NULL if the enumeration doesn't have a member
 *          with that name
 */
GEnumValue*
g_enum_get_value_by_name (GEnumClass  *enum_class,
			  const gchar *name)
{
  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  if (enum_class->n_values)
    {
      GEnumValue *enum_value;
      
      for (enum_value = enum_class->values; enum_value->value_name; enum_value++)
	if (strcmp (name, enum_value->value_name) == 0)
	  return enum_value;
    }
  
  return NULL;
}

/**
 * g_flags_get_value_by_name:
 * @flags_class: a #GFlagsClass
 * @name: the name to look up
 *
 * Looks up a #GFlagsValue by name.
 *
 * Returns: (transfer none): the #GFlagsValue with name @name,
 *          or %NULL if there is no flag with that name
 */
GFlagsValue*
g_flags_get_value_by_name (GFlagsClass *flags_class,
			   const gchar *name)
{
  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  if (flags_class->n_values)
    {
      GFlagsValue *flags_value;
      
      for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
	if (strcmp (name, flags_value->value_name) == 0)
	  return flags_value;
    }
  
  return NULL;
}

/**
 * g_enum_get_value_by_nick:
 * @enum_class: a #GEnumClass
 * @nick: the nickname to look up
 *
 * Looks up a #GEnumValue by nickname.
 *
 * Returns: (transfer none): the #GEnumValue with nickname @nick,
 *          or %NULL if the enumeration doesn't have a member
 *          with that nickname
 */
GEnumValue*
g_enum_get_value_by_nick (GEnumClass  *enum_class,
			  const gchar *nick)
{
  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);
  g_return_val_if_fail (nick != NULL, NULL);
  
  if (enum_class->n_values)
    {
      GEnumValue *enum_value;
      
      for (enum_value = enum_class->values; enum_value->value_name; enum_value++)
	if (enum_value->value_nick && strcmp (nick, enum_value->value_nick) == 0)
	  return enum_value;
    }
  
  return NULL;
}

/**
 * g_flags_get_value_by_nick:
 * @flags_class: a #GFlagsClass
 * @nick: the nickname to look up
 *
 * Looks up a #GFlagsValue by nickname.
 *
 * Returns: (transfer none): the #GFlagsValue with nickname @nick,
 *          or %NULL if there is no flag with that nickname
 */
GFlagsValue*
g_flags_get_value_by_nick (GFlagsClass *flags_class,
			   const gchar *nick)
{
  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  g_return_val_if_fail (nick != NULL, NULL);
  
  if (flags_class->n_values)
    {
      GFlagsValue *flags_value;
      
      for (flags_value = flags_class->values; flags_value->value_nick; flags_value++)
	if (flags_value->value_nick && strcmp (nick, flags_value->value_nick) == 0)
	  return flags_value;
    }
  
  return NULL;
}

/**
 * g_enum_get_value:
 * @enum_class: a #GEnumClass
 * @value: the value to look up
 *
 * Returns the #GEnumValue for a value.
 *
 * Returns: (transfer none): the #GEnumValue for @value, or %NULL
 *          if @value is not a member of the enumeration
 */
GEnumValue*
g_enum_get_value (GEnumClass *enum_class,
		  gint	      value)
{
  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);
  
  if (enum_class->n_values)
    {
      GEnumValue *enum_value;
      
      for (enum_value = enum_class->values; enum_value->value_name; enum_value++)
	if (enum_value->value == value)
	  return enum_value;
    }
  
  return NULL;
}

/**
 * g_flags_get_first_value:
 * @flags_class: a #GFlagsClass
 * @value: the value
 *
 * Returns the first #GFlagsValue which is set in @value.
 *
 * Returns: (transfer none): the first #GFlagsValue which is set in
 *          @value, or %NULL if none is set
 */
GFlagsValue*
g_flags_get_first_value (GFlagsClass *flags_class,
			 guint	      value)
{
  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  
  if (flags_class->n_values)
    {
      GFlagsValue *flags_value;

      if (value == 0)
        {
          for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
            if (flags_value->value == 0)
              return flags_value;
        }
      else
        {
          for (flags_value = flags_class->values; flags_value->value_name; flags_value++)
            if (flags_value->value != 0 && (flags_value->value & value) == flags_value->value)
              return flags_value;
        }      
    }
  
  return NULL;
}

/**
 * g_enum_to_string:
 * @g_enum_type: the type identifier of a #GEnumClass type
 * @value: the value
 *
 * Pretty-prints @value in the form of the enum’s name.
 *
 * This is intended to be used for debugging purposes. The format of the output
 * may change in the future.
 *
 * Returns: (transfer full): a newly-allocated text string
 *
 * Since: 2.54
 */
gchar *
g_enum_to_string (GType g_enum_type,
                  gint  value)
{
  gchar *result;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  g_return_val_if_fail (G_TYPE_IS_ENUM (g_enum_type), NULL);

  enum_class = g_type_class_ref (g_enum_type);

  /* Already warned */
  if (enum_class == NULL)
    return g_strdup_printf ("%d", value);

  enum_value = g_enum_get_value (enum_class, value);

  if (enum_value == NULL)
    result = g_strdup_printf ("%d", value);
  else
    result = g_strdup (enum_value->value_name);

  g_type_class_unref (enum_class);
  return result;
}

/*
 * g_flags_get_value_string:
 * @flags_class: a #GFlagsClass
 * @value: the value
 *
 * Pretty-prints @value in the form of the flag names separated by ` | ` and
 * sorted. Any extra bits will be shown at the end as a hexadecimal number.
 *
 * This is intended to be used for debugging purposes. The format of the output
 * may change in the future.
 *
 * Returns: (transfer full): a newly-allocated text string
 *
 * Since: 2.54
 */
static gchar *
g_flags_get_value_string (GFlagsClass *flags_class,
                          guint        value)
{
  GString *str;
  GFlagsValue *flags_value;

  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);

  str = g_string_new (NULL);

  while ((str->len == 0 || value != 0) &&
         (flags_value = g_flags_get_first_value (flags_class, value)) != NULL)
    {
      if (str->len > 0)
        g_string_append (str, " | ");

      g_string_append (str, flags_value->value_name);

      value &= ~flags_value->value;
    }

  /* Show the extra bits */
  if (value != 0 || str->len == 0)
    {
      if (str->len > 0)
        g_string_append (str, " | ");

      g_string_append_printf (str, "0x%x", value);
    }

  return g_string_free (str, FALSE);
}

/**
 * g_flags_to_string:
 * @flags_type: the type identifier of a #GFlagsClass type
 * @value: the value
 *
 * Pretty-prints @value in the form of the flag names separated by ` | ` and
 * sorted. Any extra bits will be shown at the end as a hexadecimal number.
 *
 * This is intended to be used for debugging purposes. The format of the output
 * may change in the future.
 *
 * Returns: (transfer full): a newly-allocated text string
 *
 * Since: 2.54
 */
gchar *
g_flags_to_string (GType flags_type,
                   guint value)
{
  gchar *result;
  GFlagsClass *flags_class;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), NULL);

  flags_class = g_type_class_ref (flags_type);

  /* Already warned */
  if (flags_class == NULL)
    return NULL;

  result = g_flags_get_value_string (flags_class, value);

  g_type_class_unref (flags_class);
  return result;
}


/**
 * g_value_set_enum:
 * @value: a valid #GValue whose type is derived from %G_TYPE_ENUM
 * @v_enum: enum value to be set
 *
 * Set the contents of a %G_TYPE_ENUM #GValue to @v_enum.
 */
void
g_value_set_enum (GValue *value,
		  gint    v_enum)
{
  g_return_if_fail (G_VALUE_HOLDS_ENUM (value));
  
  value->data[0].v_long = v_enum;
}

/**
 * g_value_get_enum:
 * @value: a valid #GValue whose type is derived from %G_TYPE_ENUM
 *
 * Get the contents of a %G_TYPE_ENUM #GValue.
 *
 * Returns: enum contents of @value
 */
gint
g_value_get_enum (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_ENUM (value), 0);
  
  return value->data[0].v_long;
}

/**
 * g_value_set_flags:
 * @value: a valid #GValue whose type is derived from %G_TYPE_FLAGS
 * @v_flags: flags value to be set
 *
 * Set the contents of a %G_TYPE_FLAGS #GValue to @v_flags.
 */
void
g_value_set_flags (GValue *value,
		   guint   v_flags)
{
  g_return_if_fail (G_VALUE_HOLDS_FLAGS (value));
  
  value->data[0].v_ulong = v_flags;
}

/**
 * g_value_get_flags:
 * @value: a valid #GValue whose type is derived from %G_TYPE_FLAGS
 *
 * Get the contents of a %G_TYPE_FLAGS #GValue.
 *
 * Returns: flags contents of @value
 */
guint
g_value_get_flags (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (value), 0);
  
  return value->data[0].v_ulong;
}
