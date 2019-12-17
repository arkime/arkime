/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 * Copyright © 2010 Christian Persch
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
#include <stdlib.h> /* qsort() */

#include "gvaluetypes.h"
#include "gtype-private.h"
#include "gvaluecollector.h"
#include "gobject.h"
#include "gparam.h"
#include "gboxed.h"
#include "genums.h"


/* --- value functions --- */
static void
value_init_long0 (GValue *value)
{
  value->data[0].v_long = 0;
}

static void
value_copy_long0 (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_long = src_value->data[0].v_long;
}

static gchar*
value_lcopy_char (const GValue *value,
		  guint         n_collect_values,
		  GTypeCValue  *collect_values,
		  guint         collect_flags)
{
  gint8 *int8_p = collect_values[0].v_pointer;
  
  if (!int8_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int8_p = value->data[0].v_int;
  
  return NULL;
}

static gchar*
value_lcopy_boolean (const GValue *value,
		     guint         n_collect_values,
		     GTypeCValue  *collect_values,
		     guint         collect_flags)
{
  gboolean *bool_p = collect_values[0].v_pointer;
  
  if (!bool_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *bool_p = value->data[0].v_int;
  
  return NULL;
}

static gchar*
value_collect_int (GValue      *value,
		   guint        n_collect_values,
		   GTypeCValue *collect_values,
		   guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;
  
  return NULL;
}

static gchar*
value_lcopy_int (const GValue *value,
		 guint         n_collect_values,
		 GTypeCValue  *collect_values,
		 guint         collect_flags)
{
  gint *int_p = collect_values[0].v_pointer;
  
  if (!int_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int_p = value->data[0].v_int;
  
  return NULL;
}

static gchar*
value_collect_long (GValue      *value,
		    guint        n_collect_values,
		    GTypeCValue *collect_values,
		    guint        collect_flags)
{
  value->data[0].v_long = collect_values[0].v_long;
  
  return NULL;
}

static gchar*
value_lcopy_long (const GValue *value,
		  guint         n_collect_values,
		  GTypeCValue  *collect_values,
		  guint         collect_flags)
{
  glong *long_p = collect_values[0].v_pointer;
  
  if (!long_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *long_p = value->data[0].v_long;
  
  return NULL;
}

static void
value_init_int64 (GValue *value)
{
  value->data[0].v_int64 = 0;
}

static void
value_copy_int64 (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_int64 = src_value->data[0].v_int64;
}

static gchar*
value_collect_int64 (GValue      *value,
		     guint        n_collect_values,
		     GTypeCValue *collect_values,
		     guint        collect_flags)
{
  value->data[0].v_int64 = collect_values[0].v_int64;
  
  return NULL;
}

static gchar*
value_lcopy_int64 (const GValue *value,
		   guint         n_collect_values,
		   GTypeCValue  *collect_values,
		   guint         collect_flags)
{
  gint64 *int64_p = collect_values[0].v_pointer;
  
  if (!int64_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *int64_p = value->data[0].v_int64;
  
  return NULL;
}

static void
value_init_float (GValue *value)
{
  value->data[0].v_float = 0.0;
}

static void
value_copy_float (const GValue *src_value,
		  GValue       *dest_value)
{
  dest_value->data[0].v_float = src_value->data[0].v_float;
}

static gchar*
value_collect_float (GValue      *value,
		     guint        n_collect_values,
		     GTypeCValue *collect_values,
		     guint        collect_flags)
{
  value->data[0].v_float = collect_values[0].v_double;
  
  return NULL;
}

static gchar*
value_lcopy_float (const GValue *value,
		   guint         n_collect_values,
		   GTypeCValue  *collect_values,
		   guint         collect_flags)
{
  gfloat *float_p = collect_values[0].v_pointer;
  
  if (!float_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *float_p = value->data[0].v_float;
  
  return NULL;
}

static void
value_init_double (GValue *value)
{
  value->data[0].v_double = 0.0;
}

static void
value_copy_double (const GValue *src_value,
		   GValue	*dest_value)
{
  dest_value->data[0].v_double = src_value->data[0].v_double;
}

static gchar*
value_collect_double (GValue	  *value,
		      guint        n_collect_values,
		      GTypeCValue *collect_values,
		      guint        collect_flags)
{
  value->data[0].v_double = collect_values[0].v_double;
  
  return NULL;
}

static gchar*
value_lcopy_double (const GValue *value,
		    guint         n_collect_values,
		    GTypeCValue  *collect_values,
		    guint         collect_flags)
{
  gdouble *double_p = collect_values[0].v_pointer;
  
  if (!double_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  *double_p = value->data[0].v_double;
  
  return NULL;
}

static void
value_init_string (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_free_string (GValue *value)
{
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    g_free (value->data[0].v_pointer);
}

static void
value_copy_string (const GValue *src_value,
		   GValue	*dest_value)
{
  dest_value->data[0].v_pointer = g_strdup (src_value->data[0].v_pointer);
}

static gchar*
value_collect_string (GValue	  *value,
		      guint        n_collect_values,
		      GTypeCValue *collect_values,
		      guint        collect_flags)
{
  if (!collect_values[0].v_pointer)
    value->data[0].v_pointer = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    {
      value->data[0].v_pointer = collect_values[0].v_pointer;
      value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
    }
  else
    value->data[0].v_pointer = g_strdup (collect_values[0].v_pointer);
  
  return NULL;
}

static gchar*
value_lcopy_string (const GValue *value,
		    guint         n_collect_values,
		    GTypeCValue  *collect_values,
		    guint         collect_flags)
{
  gchar **string_p = collect_values[0].v_pointer;
  
  if (!string_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));
  
  if (!value->data[0].v_pointer)
    *string_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *string_p = value->data[0].v_pointer;
  else
    *string_p = g_strdup (value->data[0].v_pointer);
  
  return NULL;
}

static void
value_init_pointer (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_copy_pointer (const GValue *src_value,
		    GValue       *dest_value)
{
  dest_value->data[0].v_pointer = src_value->data[0].v_pointer;
}

static gpointer
value_peek_pointer0 (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
value_collect_pointer (GValue      *value,
		       guint        n_collect_values,
		       GTypeCValue *collect_values,
		       guint        collect_flags)
{
  value->data[0].v_pointer = collect_values[0].v_pointer;

  return NULL;
}

static gchar*
value_lcopy_pointer (const GValue *value,
		     guint         n_collect_values,
		     GTypeCValue  *collect_values,
		     guint         collect_flags)
{
  gpointer *pointer_p = collect_values[0].v_pointer;

  if (!pointer_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  *pointer_p = value->data[0].v_pointer;

  return NULL;
}

static void
value_free_variant (GValue *value)
{
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS) &&
      value->data[0].v_pointer)
    g_variant_unref (value->data[0].v_pointer);
}

static void
value_copy_variant (const GValue *src_value,
		   GValue	*dest_value)
{
  if (src_value->data[0].v_pointer)
    dest_value->data[0].v_pointer = g_variant_ref_sink (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static gchar*
value_collect_variant (GValue	  *value,
		      guint        n_collect_values,
		      GTypeCValue *collect_values,
		      guint        collect_flags)
{
  if (!collect_values[0].v_pointer)
    value->data[0].v_pointer = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    {
      value->data[0].v_pointer = collect_values[0].v_pointer;
      value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
    }
  else
    value->data[0].v_pointer = g_variant_ref_sink (collect_values[0].v_pointer);

  return NULL;
}

static gchar*
value_lcopy_variant (const GValue *value,
		    guint         n_collect_values,
		    GTypeCValue  *collect_values,
		    guint         collect_flags)
{
  GVariant **variant_p = collect_values[0].v_pointer;

  if (!variant_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *variant_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *variant_p = value->data[0].v_pointer;
  else
    *variant_p = g_variant_ref_sink (value->data[0].v_pointer);

  return NULL;
}

/* --- type initialization --- */
void
_g_value_types_init (void)
{
  GTypeInfo info = {
    0,				/* class_size */
    NULL,			/* base_init */
    NULL,			/* base_destroy */
    NULL,			/* class_init */
    NULL,			/* class_destroy */
    NULL,			/* class_data */
    0,				/* instance_size */
    0,				/* n_preallocs */
    NULL,			/* instance_init */
    NULL,			/* value_table */
  };
  const GTypeFundamentalInfo finfo = { G_TYPE_FLAG_DERIVABLE, };
  GType type;
  
  /* G_TYPE_CHAR / G_TYPE_UCHAR
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		/* value_init */
      NULL,			/* value_free */
      value_copy_long0,		/* value_copy */
      NULL,			/* value_peek_pointer */
      "i",			/* collect_format */
      value_collect_int,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_char,		/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_CHAR, g_intern_static_string ("gchar"), &info, &finfo, 0);
    g_assert (type == G_TYPE_CHAR);
    type = g_type_register_fundamental (G_TYPE_UCHAR, g_intern_static_string ("guchar"), &info, &finfo, 0);
    g_assert (type == G_TYPE_UCHAR);
  }

  /* G_TYPE_BOOLEAN
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		 /* value_init */
      NULL,			 /* value_free */
      value_copy_long0,		 /* value_copy */
      NULL,                      /* value_peek_pointer */
      "i",			 /* collect_format */
      value_collect_int,	 /* collect_value */
      "p",			 /* lcopy_format */
      value_lcopy_boolean,	 /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_BOOLEAN, g_intern_static_string ("gboolean"), &info, &finfo, 0);
    g_assert (type == G_TYPE_BOOLEAN);
  }
  
  /* G_TYPE_INT / G_TYPE_UINT
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		/* value_init */
      NULL,			/* value_free */
      value_copy_long0,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "i",			/* collect_format */
      value_collect_int,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_int,		/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_INT, g_intern_static_string ("gint"), &info, &finfo, 0);
    g_assert (type == G_TYPE_INT);
    type = g_type_register_fundamental (G_TYPE_UINT, g_intern_static_string ("guint"), &info, &finfo, 0);
    g_assert (type == G_TYPE_UINT);
  }

  /* G_TYPE_LONG / G_TYPE_ULONG
   */
  {
    static const GTypeValueTable value_table = {
      value_init_long0,		/* value_init */
      NULL,			/* value_free */
      value_copy_long0,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "l",			/* collect_format */
      value_collect_long,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_long,		/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_LONG, g_intern_static_string ("glong"), &info, &finfo, 0);
    g_assert (type == G_TYPE_LONG);
    type = g_type_register_fundamental (G_TYPE_ULONG, g_intern_static_string ("gulong"), &info, &finfo, 0);
    g_assert (type == G_TYPE_ULONG);
  }
  
  /* G_TYPE_INT64 / G_TYPE_UINT64
   */
  {
    static const GTypeValueTable value_table = {
      value_init_int64,		/* value_init */
      NULL,			/* value_free */
      value_copy_int64,		/* value_copy */
      NULL,                     /* value_peek_pointer */
      "q",			/* collect_format */
      value_collect_int64,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_int64,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_INT64, g_intern_static_string ("gint64"), &info, &finfo, 0);
    g_assert (type == G_TYPE_INT64);
    type = g_type_register_fundamental (G_TYPE_UINT64, g_intern_static_string ("guint64"), &info, &finfo, 0);
    g_assert (type == G_TYPE_UINT64);
  }
  
  /* G_TYPE_FLOAT
   */
  {
    static const GTypeValueTable value_table = {
      value_init_float,		 /* value_init */
      NULL,			 /* value_free */
      value_copy_float,		 /* value_copy */
      NULL,                      /* value_peek_pointer */
      "d",			 /* collect_format */
      value_collect_float,	 /* collect_value */
      "p",			 /* lcopy_format */
      value_lcopy_float,	 /* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_FLOAT, g_intern_static_string ("gfloat"), &info, &finfo, 0);
    g_assert (type == G_TYPE_FLOAT);
  }
  
  /* G_TYPE_DOUBLE
   */
  {
    static const GTypeValueTable value_table = {
      value_init_double,	/* value_init */
      NULL,			/* value_free */
      value_copy_double,	/* value_copy */
      NULL,                     /* value_peek_pointer */
      "d",			/* collect_format */
      value_collect_double,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_double,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_DOUBLE, g_intern_static_string ("gdouble"), &info, &finfo, 0);
    g_assert (type == G_TYPE_DOUBLE);
  }

  /* G_TYPE_STRING
   */
  {
    static const GTypeValueTable value_table = {
      value_init_string,	/* value_init */
      value_free_string,	/* value_free */
      value_copy_string,	/* value_copy */
      value_peek_pointer0,	/* value_peek_pointer */
      "p",			/* collect_format */
      value_collect_string,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_string,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_STRING, g_intern_static_string ("gchararray"), &info, &finfo, 0);
    g_assert (type == G_TYPE_STRING);
  }

  /* G_TYPE_POINTER
   */
  {
    static const GTypeValueTable value_table = {
      value_init_pointer,	/* value_init */
      NULL,			/* value_free */
      value_copy_pointer,	/* value_copy */
      value_peek_pointer0,	/* value_peek_pointer */
      "p",			/* collect_format */
      value_collect_pointer,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_pointer,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_POINTER, g_intern_static_string ("gpointer"), &info, &finfo, 0);
    g_assert (type == G_TYPE_POINTER);
  }

  /* G_TYPE_VARIANT
   */
  {
    static const GTypeValueTable value_table = {
      value_init_pointer,	/* value_init */
      value_free_variant,	/* value_free */
      value_copy_variant,	/* value_copy */
      value_peek_pointer0,	/* value_peek_pointer */
      "p",			/* collect_format */
      value_collect_variant,	/* collect_value */
      "p",			/* lcopy_format */
      value_lcopy_variant,	/* lcopy_value */
    };
    info.value_table = &value_table;
    type = g_type_register_fundamental (G_TYPE_VARIANT, g_intern_static_string ("GVariant"), &info, &finfo, 0);
    g_assert (type == G_TYPE_VARIANT);
  }
}


/* --- GValue functions --- */
/**
 * g_value_set_char:
 * @value: a valid #GValue of type %G_TYPE_CHAR
 * @v_char: character value to be set
 *
 * Set the contents of a %G_TYPE_CHAR #GValue to @v_char.
 * Deprecated: 2.32: This function's input type is broken, see g_value_set_schar()
 */
void
g_value_set_char (GValue *value,
		  gchar	  v_char)
{
  g_return_if_fail (G_VALUE_HOLDS_CHAR (value));
  
  value->data[0].v_int = v_char;
}

/**
 * g_value_get_char:
 * @value: a valid #GValue of type %G_TYPE_CHAR
 *
 * Do not use this function; it is broken on platforms where the %char
 * type is unsigned, such as ARM and PowerPC.  See g_value_get_schar().
 *
 * Get the contents of a %G_TYPE_CHAR #GValue.  
 * 
 * Returns: character contents of @value
 * Deprecated: 2.32: This function's return type is broken, see g_value_get_schar()
 */
gchar
g_value_get_char (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_CHAR (value), 0);
  
  return value->data[0].v_int;
}

/**
 * g_value_set_schar:
 * @value: a valid #GValue of type %G_TYPE_CHAR
 * @v_char: signed 8 bit integer to be set
 *
 * Set the contents of a %G_TYPE_CHAR #GValue to @v_char.
 *
 * Since: 2.32
 */
void
g_value_set_schar (GValue *value,
		   gint8   v_char)
{
  g_return_if_fail (G_VALUE_HOLDS_CHAR (value));
  
  value->data[0].v_int = v_char;
}

/**
 * g_value_get_schar:
 * @value: a valid #GValue of type %G_TYPE_CHAR
 *
 * Get the contents of a %G_TYPE_CHAR #GValue.
 * 
 * Returns: signed 8 bit integer contents of @value
 * Since: 2.32
 */
gint8
g_value_get_schar (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_CHAR (value), 0);
  
  return value->data[0].v_int;
}

/**
 * g_value_set_uchar:
 * @value: a valid #GValue of type %G_TYPE_UCHAR
 * @v_uchar: unsigned character value to be set
 *
 * Set the contents of a %G_TYPE_UCHAR #GValue to @v_uchar.
 */
void
g_value_set_uchar (GValue *value,
		   guchar  v_uchar)
{
  g_return_if_fail (G_VALUE_HOLDS_UCHAR (value));
  
  value->data[0].v_uint = v_uchar;
}

/**
 * g_value_get_uchar:
 * @value: a valid #GValue of type %G_TYPE_UCHAR
 *
 * Get the contents of a %G_TYPE_UCHAR #GValue.
 *
 * Returns: unsigned character contents of @value
 */
guchar
g_value_get_uchar (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UCHAR (value), 0);
  
  return value->data[0].v_uint;
}

/**
 * g_value_set_boolean:
 * @value: a valid #GValue of type %G_TYPE_BOOLEAN
 * @v_boolean: boolean value to be set
 *
 * Set the contents of a %G_TYPE_BOOLEAN #GValue to @v_boolean.
 */
void
g_value_set_boolean (GValue  *value,
		     gboolean v_boolean)
{
  g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
  
  value->data[0].v_int = v_boolean != FALSE;
}

/**
 * g_value_get_boolean:
 * @value: a valid #GValue of type %G_TYPE_BOOLEAN
 *
 * Get the contents of a %G_TYPE_BOOLEAN #GValue.
 *
 * Returns: boolean contents of @value
 */
gboolean
g_value_get_boolean (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (value), 0);
  
  return value->data[0].v_int;
}

/**
 * g_value_set_int:
 * @value: a valid #GValue of type %G_TYPE_INT
 * @v_int: integer value to be set
 *
 * Set the contents of a %G_TYPE_INT #GValue to @v_int.
 */
void
g_value_set_int (GValue *value,
		 gint	 v_int)
{
  g_return_if_fail (G_VALUE_HOLDS_INT (value));
  
  value->data[0].v_int = v_int;
}

/**
 * g_value_get_int:
 * @value: a valid #GValue of type %G_TYPE_INT
 *
 * Get the contents of a %G_TYPE_INT #GValue.
 *
 * Returns: integer contents of @value
 */
gint
g_value_get_int (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_INT (value), 0);
  
  return value->data[0].v_int;
}

/**
 * g_value_set_uint:
 * @value: a valid #GValue of type %G_TYPE_UINT
 * @v_uint: unsigned integer value to be set
 *
 * Set the contents of a %G_TYPE_UINT #GValue to @v_uint.
 */
void
g_value_set_uint (GValue *value,
		  guint	  v_uint)
{
  g_return_if_fail (G_VALUE_HOLDS_UINT (value));
  
  value->data[0].v_uint = v_uint;
}

/**
 * g_value_get_uint:
 * @value: a valid #GValue of type %G_TYPE_UINT
 *
 * Get the contents of a %G_TYPE_UINT #GValue.
 *
 * Returns: unsigned integer contents of @value
 */
guint
g_value_get_uint (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UINT (value), 0);
  
  return value->data[0].v_uint;
}

/**
 * g_value_set_long:
 * @value: a valid #GValue of type %G_TYPE_LONG
 * @v_long: long integer value to be set
 *
 * Set the contents of a %G_TYPE_LONG #GValue to @v_long.
 */
void
g_value_set_long (GValue *value,
		  glong	  v_long)
{
  g_return_if_fail (G_VALUE_HOLDS_LONG (value));
  
  value->data[0].v_long = v_long;
}

/**
 * g_value_get_long:
 * @value: a valid #GValue of type %G_TYPE_LONG
 *
 * Get the contents of a %G_TYPE_LONG #GValue.
 *
 * Returns: long integer contents of @value
 */
glong
g_value_get_long (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_LONG (value), 0);
  
  return value->data[0].v_long;
}

/**
 * g_value_set_ulong:
 * @value: a valid #GValue of type %G_TYPE_ULONG
 * @v_ulong: unsigned long integer value to be set
 *
 * Set the contents of a %G_TYPE_ULONG #GValue to @v_ulong.
 */
void
g_value_set_ulong (GValue *value,
		   gulong  v_ulong)
{
  g_return_if_fail (G_VALUE_HOLDS_ULONG (value));
  
  value->data[0].v_ulong = v_ulong;
}

/**
 * g_value_get_ulong:
 * @value: a valid #GValue of type %G_TYPE_ULONG
 *
 * Get the contents of a %G_TYPE_ULONG #GValue.
 *
 * Returns: unsigned long integer contents of @value
 */
gulong
g_value_get_ulong (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_ULONG (value), 0);
  
  return value->data[0].v_ulong;
}

/**
 * g_value_get_int64:
 * @value: a valid #GValue of type %G_TYPE_INT64
 *
 * Get the contents of a %G_TYPE_INT64 #GValue.
 *
 * Returns: 64bit integer contents of @value
 */
void
g_value_set_int64 (GValue *value,
		   gint64  v_int64)
{
  g_return_if_fail (G_VALUE_HOLDS_INT64 (value));
  
  value->data[0].v_int64 = v_int64;
}

/**
 * g_value_set_int64:
 * @value: a valid #GValue of type %G_TYPE_INT64
 * @v_int64: 64bit integer value to be set
 *
 * Set the contents of a %G_TYPE_INT64 #GValue to @v_int64.
 */
gint64
g_value_get_int64 (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_INT64 (value), 0);
  
  return value->data[0].v_int64;
}

/**
 * g_value_set_uint64:
 * @value: a valid #GValue of type %G_TYPE_UINT64
 * @v_uint64: unsigned 64bit integer value to be set
 *
 * Set the contents of a %G_TYPE_UINT64 #GValue to @v_uint64.
 */
void
g_value_set_uint64 (GValue *value,
		    guint64 v_uint64)
{
  g_return_if_fail (G_VALUE_HOLDS_UINT64 (value));
  
  value->data[0].v_uint64 = v_uint64;
}

/**
 * g_value_get_uint64:
 * @value: a valid #GValue of type %G_TYPE_UINT64
 *
 * Get the contents of a %G_TYPE_UINT64 #GValue.
 *
 * Returns: unsigned 64bit integer contents of @value
 */
guint64
g_value_get_uint64 (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_UINT64 (value), 0);
  
  return value->data[0].v_uint64;
}

/**
 * g_value_set_float:
 * @value: a valid #GValue of type %G_TYPE_FLOAT
 * @v_float: float value to be set
 *
 * Set the contents of a %G_TYPE_FLOAT #GValue to @v_float.
 */
void
g_value_set_float (GValue *value,
		   gfloat  v_float)
{
  g_return_if_fail (G_VALUE_HOLDS_FLOAT (value));
  
  value->data[0].v_float = v_float;
}

/**
 * g_value_get_float:
 * @value: a valid #GValue of type %G_TYPE_FLOAT
 *
 * Get the contents of a %G_TYPE_FLOAT #GValue.
 *
 * Returns: float contents of @value
 */
gfloat
g_value_get_float (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_FLOAT (value), 0);
  
  return value->data[0].v_float;
}

/**
 * g_value_set_double:
 * @value: a valid #GValue of type %G_TYPE_DOUBLE
 * @v_double: double value to be set
 *
 * Set the contents of a %G_TYPE_DOUBLE #GValue to @v_double.
 */
void
g_value_set_double (GValue *value,
		    gdouble v_double)
{
  g_return_if_fail (G_VALUE_HOLDS_DOUBLE (value));
  
  value->data[0].v_double = v_double;
}

/**
 * g_value_get_double:
 * @value: a valid #GValue of type %G_TYPE_DOUBLE
 *
 * Get the contents of a %G_TYPE_DOUBLE #GValue.
 *
 * Returns: double contents of @value
 */
gdouble
g_value_get_double (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_DOUBLE (value), 0);
  
  return value->data[0].v_double;
}

/**
 * g_value_set_string:
 * @value: a valid #GValue of type %G_TYPE_STRING
 * @v_string: (nullable): caller-owned string to be duplicated for the #GValue
 *
 * Set the contents of a %G_TYPE_STRING #GValue to @v_string.
 */
void
g_value_set_string (GValue	*value,
		    const gchar *v_string)
{
  gchar *new_val;

  g_return_if_fail (G_VALUE_HOLDS_STRING (value));

  new_val = g_strdup (v_string);

  if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
    value->data[1].v_uint = 0;
  else
    g_free (value->data[0].v_pointer);

  value->data[0].v_pointer = new_val;
}

/**
 * g_value_set_static_string:
 * @value: a valid #GValue of type %G_TYPE_STRING
 * @v_string: (nullable): static string to be set
 *
 * Set the contents of a %G_TYPE_STRING #GValue to @v_string.
 * The string is assumed to be static, and is thus not duplicated
 * when setting the #GValue.
 */
void
g_value_set_static_string (GValue      *value,
			   const gchar *v_string)
{
  g_return_if_fail (G_VALUE_HOLDS_STRING (value));
  
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    g_free (value->data[0].v_pointer);
  value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
  value->data[0].v_pointer = (gchar*) v_string;
}

/**
 * g_value_set_string_take_ownership:
 * @value: a valid #GValue of type %G_TYPE_STRING
 * @v_string: (nullable): duplicated unowned string to be set
 *
 * This is an internal function introduced mainly for C marshallers.
 *
 * Deprecated: 2.4: Use g_value_take_string() instead.
 */
void
g_value_set_string_take_ownership (GValue *value,
				   gchar  *v_string)
{
  g_value_take_string (value, v_string);
}

/**
 * g_value_take_string:
 * @value: a valid #GValue of type %G_TYPE_STRING
 * @v_string: (nullable): string to take ownership of
 *
 * Sets the contents of a %G_TYPE_STRING #GValue to @v_string.
 *
 * Since: 2.4
 */
void
g_value_take_string (GValue *value,
		     gchar  *v_string)
{
  g_return_if_fail (G_VALUE_HOLDS_STRING (value));
  
  if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
    value->data[1].v_uint = 0;
  else
    g_free (value->data[0].v_pointer);
  value->data[0].v_pointer = v_string;
}

/**
 * g_value_get_string:
 * @value: a valid #GValue of type %G_TYPE_STRING
 *
 * Get the contents of a %G_TYPE_STRING #GValue.
 *
 * Returns: string content of @value
 */
const gchar*
g_value_get_string (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (value), NULL);
  
  return value->data[0].v_pointer;
}

/**
 * g_value_dup_string:
 * @value: a valid #GValue of type %G_TYPE_STRING
 *
 * Get a copy the contents of a %G_TYPE_STRING #GValue.
 *
 * Returns: a newly allocated copy of the string content of @value
 */
gchar*
g_value_dup_string (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (value), NULL);
  
  return g_strdup (value->data[0].v_pointer);
}

/**
 * g_value_set_pointer:
 * @value: a valid #GValue of %G_TYPE_POINTER
 * @v_pointer: pointer value to be set
 *
 * Set the contents of a pointer #GValue to @v_pointer.
 */
void
g_value_set_pointer (GValue  *value,
		     gpointer v_pointer)
{
  g_return_if_fail (G_VALUE_HOLDS_POINTER (value));

  value->data[0].v_pointer = v_pointer;
}

/**
 * g_value_get_pointer:
 * @value: a valid #GValue of %G_TYPE_POINTER
 *
 * Get the contents of a pointer #GValue.
 *
 * Returns: (transfer none): pointer contents of @value
 */
gpointer
g_value_get_pointer (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_POINTER (value), NULL);

  return value->data[0].v_pointer;
}

G_DEFINE_POINTER_TYPE (GType, g_gtype)

/**
 * g_value_set_gtype:
 * @value: a valid #GValue of type %G_TYPE_GTYPE
 * @v_gtype: #GType to be set
 *
 * Set the contents of a %G_TYPE_GTYPE #GValue to @v_gtype.
 *
 * Since: 2.12
 */
void
g_value_set_gtype (GValue *value,
		   GType   v_gtype)
{
  g_return_if_fail (G_VALUE_HOLDS_GTYPE (value));

  value->data[0].v_pointer = GSIZE_TO_POINTER (v_gtype);
  
}

/**
 * g_value_get_gtype:
 * @value: a valid #GValue of type %G_TYPE_GTYPE
 *
 * Get the contents of a %G_TYPE_GTYPE #GValue.
 *
 * Since: 2.12
 *
 * Returns: the #GType stored in @value
 */
GType
g_value_get_gtype (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_GTYPE (value), 0);

  return GPOINTER_TO_SIZE (value->data[0].v_pointer);
}

/**
 * g_value_set_variant:
 * @value: a valid #GValue of type %G_TYPE_VARIANT
 * @variant: (nullable): a #GVariant, or %NULL
 *
 * Set the contents of a variant #GValue to @variant.
 * If the variant is floating, it is consumed.
 *
 * Since: 2.26
 */
void
g_value_set_variant (GValue   *value,
                     GVariant *variant)
{
  GVariant *old_variant;

  g_return_if_fail (G_VALUE_HOLDS_VARIANT (value));

  old_variant = value->data[0].v_pointer;

  if (variant)
    value->data[0].v_pointer = g_variant_ref_sink (variant);
  else
    value->data[0].v_pointer = NULL;

  if (old_variant)
    g_variant_unref (old_variant);
}

/**
 * g_value_take_variant:
 * @value: a valid #GValue of type %G_TYPE_VARIANT
 * @variant: (nullable) (transfer full): a #GVariant, or %NULL
 *
 * Set the contents of a variant #GValue to @variant, and takes over
 * the ownership of the caller's reference to @variant;
 * the caller doesn't have to unref it any more (i.e. the reference
 * count of the variant is not increased).
 *
 * If @variant was floating then its floating reference is converted to
 * a hard reference.
 *
 * If you want the #GValue to hold its own reference to @variant, use
 * g_value_set_variant() instead.
 *
 * This is an internal function introduced mainly for C marshallers.
 *
 * Since: 2.26
 */
void
g_value_take_variant (GValue   *value,
                      GVariant *variant)
{
  GVariant *old_variant;

  g_return_if_fail (G_VALUE_HOLDS_VARIANT (value));

  old_variant = value->data[0].v_pointer;

  if (variant)
    value->data[0].v_pointer = g_variant_take_ref (variant);
  else
    value->data[0].v_pointer = NULL;

  if (old_variant)
    g_variant_unref (old_variant);
}

/**
 * g_value_get_variant:
 * @value: a valid #GValue of type %G_TYPE_VARIANT
 *
 * Get the contents of a variant #GValue.
 *
 * Returns: (nullable): variant contents of @value (may be %NULL)
 *
 * Since: 2.26
 */
GVariant*
g_value_get_variant (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_VARIANT (value), NULL);

  return value->data[0].v_pointer;
}

/**
 * g_value_dup_variant:
 * @value: a valid #GValue of type %G_TYPE_VARIANT
 *
 * Get the contents of a variant #GValue, increasing its refcount. The returned
 * #GVariant is never floating.
 *
 * Returns: (transfer full) (nullable): variant contents of @value (may be %NULL);
 *    should be unreffed using g_variant_unref() when no longer needed
 *
 * Since: 2.26
 */
GVariant*
g_value_dup_variant (const GValue *value)
{
  GVariant *variant;

  g_return_val_if_fail (G_VALUE_HOLDS_VARIANT (value), NULL);

  variant = value->data[0].v_pointer;
  if (variant)
    g_variant_ref_sink (variant);

  return variant;
}

/**
 * g_strdup_value_contents:
 * @value: #GValue which contents are to be described.
 *
 * Return a newly allocated string, which describes the contents of a
 * #GValue.  The main purpose of this function is to describe #GValue
 * contents for debugging output, the way in which the contents are
 * described may change between different GLib versions.
 *
 * Returns: Newly allocated string.
 */
gchar*
g_strdup_value_contents (const GValue *value)
{
  const gchar *src;
  gchar *contents;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);
  
  if (G_VALUE_HOLDS_STRING (value))
    {
      src = g_value_get_string (value);
      
      if (!src)
	contents = g_strdup ("NULL");
      else
	{
	  gchar *s = g_strescape (src, NULL);

	  contents = g_strdup_printf ("\"%s\"", s);
	  g_free (s);
	}
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING))
    {
      GValue tmp_value = G_VALUE_INIT;
      gchar *s;

      g_value_init (&tmp_value, G_TYPE_STRING);
      g_value_transform (value, &tmp_value);
      s = g_strescape (g_value_get_string (&tmp_value), NULL);
      g_value_unset (&tmp_value);
      if (G_VALUE_HOLDS_ENUM (value) || G_VALUE_HOLDS_FLAGS (value))
	contents = g_strdup_printf ("((%s) %s)",
				    g_type_name (G_VALUE_TYPE (value)),
				    s);
      else
	contents = g_strdup (s ? s : "NULL");
      g_free (s);
    }
  else if (g_value_fits_pointer (value))
    {
      gpointer p = g_value_peek_pointer (value);

      if (!p)
	contents = g_strdup ("NULL");
      else if (G_VALUE_HOLDS_OBJECT (value))
	contents = g_strdup_printf ("((%s*) %p)", G_OBJECT_TYPE_NAME (p), p);
      else if (G_VALUE_HOLDS_PARAM (value))
	contents = g_strdup_printf ("((%s*) %p)", G_PARAM_SPEC_TYPE_NAME (p), p);
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          GStrv strv = g_value_get_boxed (value);
          GString *tmp = g_string_new ("[");

          while (*strv != NULL)
            {
              gchar *escaped = g_strescape (*strv, NULL);

              g_string_append_printf (tmp, "\"%s\"", escaped);
              g_free (escaped);

              if (*++strv != NULL)
                g_string_append (tmp, ", ");
            }

          g_string_append (tmp, "]");
          contents = g_string_free (tmp, FALSE);
        }
      else if (G_VALUE_HOLDS_BOXED (value))
	contents = g_strdup_printf ("((%s*) %p)", g_type_name (G_VALUE_TYPE (value)), p);
      else if (G_VALUE_HOLDS_POINTER (value))
	contents = g_strdup_printf ("((gpointer) %p)", p);
      else
	contents = g_strdup ("???");
    }
  else
    contents = g_strdup ("???");

  return contents;
}

/**
 * g_pointer_type_register_static:
 * @name: the name of the new pointer type.
 *
 * Creates a new %G_TYPE_POINTER derived type id for a new
 * pointer type with name @name.
 *
 * Returns: a new %G_TYPE_POINTER derived type id for @name.
 */
GType
g_pointer_type_register_static (const gchar *name)
{
  const GTypeInfo type_info = {
    0,			/* class_size */
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    NULL,		/* class_init */
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    0,			/* instance_size */
    0,			/* n_preallocs */
    NULL,		/* instance_init */
    NULL		/* value_table */
  };
  GType type;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (g_type_from_name (name) == 0, 0);

  type = g_type_register_static (G_TYPE_POINTER, name, &type_info, 0);

  return type;
}
