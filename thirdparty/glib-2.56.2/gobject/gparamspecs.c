/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 * Copyright (C) 2010 Christian Persch
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

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gparamspecs.h"
#include "gtype-private.h"
#include "gvaluecollector.h"

#include "gvaluearray.h"


/**
 * SECTION:param_value_types
 * @short_description: Standard Parameter and Value Types
 * @see_also: #GParamSpec, #GValue, g_object_class_install_property().
 * @title: Parameters and Values
 *
 * #GValue provides an abstract container structure which can be
 * copied, transformed and compared while holding a value of any
 * (derived) type, which is registered as a #GType with a
 * #GTypeValueTable in its #GTypeInfo structure.  Parameter
 * specifications for most value types can be created as #GParamSpec
 * derived instances, to implement e.g. #GObject properties which
 * operate on #GValue containers.
 *
 * Parameter names need to start with a letter (a-z or A-Z). Subsequent
 * characters can be letters, numbers or a '-'.
 * All other characters are replaced by a '-' during construction.
 */


#define	G_FLOAT_EPSILON		(1e-30)
#define	G_DOUBLE_EPSILON	(1e-90)


/* --- param spec functions --- */
static void
param_char_init (GParamSpec *pspec)
{
  GParamSpecChar *cspec = G_PARAM_SPEC_CHAR (pspec);
  
  cspec->minimum = 0x7f;
  cspec->maximum = 0x80;
  cspec->default_value = 0;
}

static void
param_char_set_default (GParamSpec *pspec,
			GValue	   *value)
{
  value->data[0].v_int = G_PARAM_SPEC_CHAR (pspec)->default_value;
}

static gboolean
param_char_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecChar *cspec = G_PARAM_SPEC_CHAR (pspec);
  gint oval = value->data[0].v_int;
  
  value->data[0].v_int = CLAMP (value->data[0].v_int, cspec->minimum, cspec->maximum);
  
  return value->data[0].v_int != oval;
}

static void
param_uchar_init (GParamSpec *pspec)
{
  GParamSpecUChar *uspec = G_PARAM_SPEC_UCHAR (pspec);
  
  uspec->minimum = 0;
  uspec->maximum = 0xff;
  uspec->default_value = 0;
}

static void
param_uchar_set_default (GParamSpec *pspec,
			 GValue	    *value)
{
  value->data[0].v_uint = G_PARAM_SPEC_UCHAR (pspec)->default_value;
}

static gboolean
param_uchar_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecUChar *uspec = G_PARAM_SPEC_UCHAR (pspec);
  guint oval = value->data[0].v_uint;
  
  value->data[0].v_uint = CLAMP (value->data[0].v_uint, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_uint != oval;
}

static void
param_boolean_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  value->data[0].v_int = G_PARAM_SPEC_BOOLEAN (pspec)->default_value;
}

static gboolean
param_boolean_validate (GParamSpec *pspec,
			GValue     *value)
{
  gint oval = value->data[0].v_int;
  
  value->data[0].v_int = value->data[0].v_int != FALSE;
  
  return value->data[0].v_int != oval;
}

static void
param_int_init (GParamSpec *pspec)
{
  GParamSpecInt *ispec = G_PARAM_SPEC_INT (pspec);
  
  ispec->minimum = 0x7fffffff;
  ispec->maximum = 0x80000000;
  ispec->default_value = 0;
}

static void
param_int_set_default (GParamSpec *pspec,
		       GValue     *value)
{
  value->data[0].v_int = G_PARAM_SPEC_INT (pspec)->default_value;
}

static gboolean
param_int_validate (GParamSpec *pspec,
		    GValue     *value)
{
  GParamSpecInt *ispec = G_PARAM_SPEC_INT (pspec);
  gint oval = value->data[0].v_int;
  
  value->data[0].v_int = CLAMP (value->data[0].v_int, ispec->minimum, ispec->maximum);
  
  return value->data[0].v_int != oval;
}

static gint
param_int_values_cmp (GParamSpec   *pspec,
		      const GValue *value1,
		      const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

static void
param_uint_init (GParamSpec *pspec)
{
  GParamSpecUInt *uspec = G_PARAM_SPEC_UINT (pspec);
  
  uspec->minimum = 0;
  uspec->maximum = 0xffffffff;
  uspec->default_value = 0;
}

static void
param_uint_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_uint = G_PARAM_SPEC_UINT (pspec)->default_value;
}

static gboolean
param_uint_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecUInt *uspec = G_PARAM_SPEC_UINT (pspec);
  guint oval = value->data[0].v_uint;
  
  value->data[0].v_uint = CLAMP (value->data[0].v_uint, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_uint != oval;
}

static gint
param_uint_values_cmp (GParamSpec   *pspec,
		       const GValue *value1,
		       const GValue *value2)
{
  if (value1->data[0].v_uint < value2->data[0].v_uint)
    return -1;
  else
    return value1->data[0].v_uint > value2->data[0].v_uint;
}

static void
param_long_init (GParamSpec *pspec)
{
  GParamSpecLong *lspec = G_PARAM_SPEC_LONG (pspec);
  
#if SIZEOF_LONG == 4
  lspec->minimum = 0x7fffffff;
  lspec->maximum = 0x80000000;
#else /* SIZEOF_LONG != 4 (8) */
  lspec->minimum = 0x7fffffffffffffff;
  lspec->maximum = 0x8000000000000000;
#endif
  lspec->default_value = 0;
}

static void
param_long_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_long = G_PARAM_SPEC_LONG (pspec)->default_value;
}

static gboolean
param_long_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecLong *lspec = G_PARAM_SPEC_LONG (pspec);
  glong oval = value->data[0].v_long;
  
  value->data[0].v_long = CLAMP (value->data[0].v_long, lspec->minimum, lspec->maximum);
  
  return value->data[0].v_long != oval;
}

static gint
param_long_values_cmp (GParamSpec   *pspec,
		       const GValue *value1,
		       const GValue *value2)
{
  if (value1->data[0].v_long < value2->data[0].v_long)
    return -1;
  else
    return value1->data[0].v_long > value2->data[0].v_long;
}

static void
param_ulong_init (GParamSpec *pspec)
{
  GParamSpecULong *uspec = G_PARAM_SPEC_ULONG (pspec);
  
  uspec->minimum = 0;
#if SIZEOF_LONG == 4
  uspec->maximum = 0xffffffff;
#else /* SIZEOF_LONG != 4 (8) */
  uspec->maximum = 0xffffffffffffffff;
#endif
  uspec->default_value = 0;
}

static void
param_ulong_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_ulong = G_PARAM_SPEC_ULONG (pspec)->default_value;
}

static gboolean
param_ulong_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecULong *uspec = G_PARAM_SPEC_ULONG (pspec);
  gulong oval = value->data[0].v_ulong;
  
  value->data[0].v_ulong = CLAMP (value->data[0].v_ulong, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_ulong != oval;
}

static gint
param_ulong_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  if (value1->data[0].v_ulong < value2->data[0].v_ulong)
    return -1;
  else
    return value1->data[0].v_ulong > value2->data[0].v_ulong;
}

static void
param_int64_init (GParamSpec *pspec)
{
  GParamSpecInt64 *lspec = G_PARAM_SPEC_INT64 (pspec);
  
  lspec->minimum = G_MININT64;
  lspec->maximum = G_MAXINT64;
  lspec->default_value = 0;
}

static void
param_int64_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_int64 = G_PARAM_SPEC_INT64 (pspec)->default_value;
}

static gboolean
param_int64_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecInt64 *lspec = G_PARAM_SPEC_INT64 (pspec);
  gint64 oval = value->data[0].v_int64;
  
  value->data[0].v_int64 = CLAMP (value->data[0].v_int64, lspec->minimum, lspec->maximum);
  
  return value->data[0].v_int64 != oval;
}

static gint
param_int64_values_cmp (GParamSpec   *pspec,
		       const GValue *value1,
		       const GValue *value2)
{
  if (value1->data[0].v_int64 < value2->data[0].v_int64)
    return -1;
  else
    return value1->data[0].v_int64 > value2->data[0].v_int64;
}

static void
param_uint64_init (GParamSpec *pspec)
{
  GParamSpecUInt64 *uspec = G_PARAM_SPEC_UINT64 (pspec);
  
  uspec->minimum = 0;
  uspec->maximum = G_MAXUINT64;
  uspec->default_value = 0;
}

static void
param_uint64_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_uint64 = G_PARAM_SPEC_UINT64 (pspec)->default_value;
}

static gboolean
param_uint64_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecUInt64 *uspec = G_PARAM_SPEC_UINT64 (pspec);
  guint64 oval = value->data[0].v_uint64;
  
  value->data[0].v_uint64 = CLAMP (value->data[0].v_uint64, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_uint64 != oval;
}

static gint
param_uint64_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  if (value1->data[0].v_uint64 < value2->data[0].v_uint64)
    return -1;
  else
    return value1->data[0].v_uint64 > value2->data[0].v_uint64;
}

static void
param_unichar_init (GParamSpec *pspec)
{
  GParamSpecUnichar *uspec = G_PARAM_SPEC_UNICHAR (pspec);
  
  uspec->default_value = 0;
}

static void
param_unichar_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_uint = G_PARAM_SPEC_UNICHAR (pspec)->default_value;
}

static gboolean
param_unichar_validate (GParamSpec *pspec,
		        GValue     *value)
{
  gunichar oval = value->data[0].v_uint;
  gboolean changed = FALSE;

  if (!g_unichar_validate (oval))
    {
      value->data[0].v_uint = 0;
      changed = TRUE;
    }

  return changed;
}

static gint
param_unichar_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  if (value1->data[0].v_uint < value2->data[0].v_uint)
    return -1;
  else
    return value1->data[0].v_uint > value2->data[0].v_uint;
}

static void
param_enum_init (GParamSpec *pspec)
{
  GParamSpecEnum *espec = G_PARAM_SPEC_ENUM (pspec);
  
  espec->enum_class = NULL;
  espec->default_value = 0;
}

static void
param_enum_finalize (GParamSpec *pspec)
{
  GParamSpecEnum *espec = G_PARAM_SPEC_ENUM (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_ENUM));
  
  if (espec->enum_class)
    {
      g_type_class_unref (espec->enum_class);
      espec->enum_class = NULL;
    }
  
  parent_class->finalize (pspec);
}

static void
param_enum_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_long = G_PARAM_SPEC_ENUM (pspec)->default_value;
}

static gboolean
param_enum_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecEnum *espec = G_PARAM_SPEC_ENUM (pspec);
  glong oval = value->data[0].v_long;
  
  if (!espec->enum_class ||
      !g_enum_get_value (espec->enum_class, value->data[0].v_long))
    value->data[0].v_long = espec->default_value;
  
  return value->data[0].v_long != oval;
}

static void
param_flags_init (GParamSpec *pspec)
{
  GParamSpecFlags *fspec = G_PARAM_SPEC_FLAGS (pspec);
  
  fspec->flags_class = NULL;
  fspec->default_value = 0;
}

static void
param_flags_finalize (GParamSpec *pspec)
{
  GParamSpecFlags *fspec = G_PARAM_SPEC_FLAGS (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_FLAGS));
  
  if (fspec->flags_class)
    {
      g_type_class_unref (fspec->flags_class);
      fspec->flags_class = NULL;
    }
  
  parent_class->finalize (pspec);
}

static void
param_flags_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_ulong = G_PARAM_SPEC_FLAGS (pspec)->default_value;
}

static gboolean
param_flags_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecFlags *fspec = G_PARAM_SPEC_FLAGS (pspec);
  gulong oval = value->data[0].v_ulong;
  
  if (fspec->flags_class)
    value->data[0].v_ulong &= fspec->flags_class->mask;
  else
    value->data[0].v_ulong = fspec->default_value;
  
  return value->data[0].v_ulong != oval;
}

static void
param_float_init (GParamSpec *pspec)
{
  GParamSpecFloat *fspec = G_PARAM_SPEC_FLOAT (pspec);
  
  fspec->minimum = -G_MAXFLOAT;
  fspec->maximum = G_MAXFLOAT;
  fspec->default_value = 0;
  fspec->epsilon = G_FLOAT_EPSILON;
}

static void
param_float_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_float = G_PARAM_SPEC_FLOAT (pspec)->default_value;
}

static gboolean
param_float_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecFloat *fspec = G_PARAM_SPEC_FLOAT (pspec);
  gfloat oval = value->data[0].v_float;
  
  value->data[0].v_float = CLAMP (value->data[0].v_float, fspec->minimum, fspec->maximum);
  
  return value->data[0].v_float != oval;
}

static gint
param_float_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  gfloat epsilon = G_PARAM_SPEC_FLOAT (pspec)->epsilon;
  
  if (value1->data[0].v_float < value2->data[0].v_float)
    return - (value2->data[0].v_float - value1->data[0].v_float > epsilon);
  else
    return value1->data[0].v_float - value2->data[0].v_float > epsilon;
}

static void
param_double_init (GParamSpec *pspec)
{
  GParamSpecDouble *dspec = G_PARAM_SPEC_DOUBLE (pspec);
  
  dspec->minimum = -G_MAXDOUBLE;
  dspec->maximum = G_MAXDOUBLE;
  dspec->default_value = 0;
  dspec->epsilon = G_DOUBLE_EPSILON;
}

static void
param_double_set_default (GParamSpec *pspec,
			  GValue     *value)
{
  value->data[0].v_double = G_PARAM_SPEC_DOUBLE (pspec)->default_value;
}

static gboolean
param_double_validate (GParamSpec *pspec,
		       GValue     *value)
{
  GParamSpecDouble *dspec = G_PARAM_SPEC_DOUBLE (pspec);
  gdouble oval = value->data[0].v_double;
  
  value->data[0].v_double = CLAMP (value->data[0].v_double, dspec->minimum, dspec->maximum);
  
  return value->data[0].v_double != oval;
}

static gint
param_double_values_cmp (GParamSpec   *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  gdouble epsilon = G_PARAM_SPEC_DOUBLE (pspec)->epsilon;
  
  if (value1->data[0].v_double < value2->data[0].v_double)
    return - (value2->data[0].v_double - value1->data[0].v_double > epsilon);
  else
    return value1->data[0].v_double - value2->data[0].v_double > epsilon;
}

static void
param_string_init (GParamSpec *pspec)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  
  sspec->default_value = NULL;
  sspec->cset_first = NULL;
  sspec->cset_nth = NULL;
  sspec->substitutor = '_';
  sspec->null_fold_if_empty = FALSE;
  sspec->ensure_non_null = FALSE;
}

static void
param_string_finalize (GParamSpec *pspec)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_STRING));
  
  g_free (sspec->default_value);
  g_free (sspec->cset_first);
  g_free (sspec->cset_nth);
  sspec->default_value = NULL;
  sspec->cset_first = NULL;
  sspec->cset_nth = NULL;
  
  parent_class->finalize (pspec);
}

static void
param_string_set_default (GParamSpec *pspec,
			  GValue     *value)
{
  value->data[0].v_pointer = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
}

static gboolean
param_string_validate (GParamSpec *pspec,
		       GValue     *value)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  gchar *string = value->data[0].v_pointer;
  guint changed = 0;
  
  if (string && string[0])
    {
      gchar *s;
      
      if (sspec->cset_first && !strchr (sspec->cset_first, string[0]))
	{
          if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
            {
              value->data[0].v_pointer = g_strdup (string);
              string = value->data[0].v_pointer;
              value->data[1].v_uint &= ~G_VALUE_NOCOPY_CONTENTS;
            }
	  string[0] = sspec->substitutor;
	  changed++;
	}
      if (sspec->cset_nth)
	for (s = string + 1; *s; s++)
	  if (!strchr (sspec->cset_nth, *s))
	    {
              if (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS)
                {
                  value->data[0].v_pointer = g_strdup (string);
                  s = (gchar*) value->data[0].v_pointer + (s - string);
                  string = value->data[0].v_pointer;
                  value->data[1].v_uint &= ~G_VALUE_NOCOPY_CONTENTS;
                }
	      *s = sspec->substitutor;
	      changed++;
	    }
    }
  if (sspec->null_fold_if_empty && string && string[0] == 0)
    {
      if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
        g_free (value->data[0].v_pointer);
      else
        value->data[1].v_uint &= ~G_VALUE_NOCOPY_CONTENTS;
      value->data[0].v_pointer = NULL;
      changed++;
      string = value->data[0].v_pointer;
    }
  if (sspec->ensure_non_null && !string)
    {
      value->data[1].v_uint &= ~G_VALUE_NOCOPY_CONTENTS;
      value->data[0].v_pointer = g_strdup ("");
      changed++;
      string = value->data[0].v_pointer;
    }

  return changed;
}

static gint
param_string_values_cmp (GParamSpec   *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  if (!value1->data[0].v_pointer)
    return value2->data[0].v_pointer != NULL ? -1 : 0;
  else if (!value2->data[0].v_pointer)
    return value1->data[0].v_pointer != NULL;
  else
    return strcmp (value1->data[0].v_pointer, value2->data[0].v_pointer);
}

static void
param_param_init (GParamSpec *pspec)
{
  /* GParamSpecParam *spec = G_PARAM_SPEC_PARAM (pspec); */
}

static void
param_param_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_param_validate (GParamSpec *pspec,
		      GValue     *value)
{
  /* GParamSpecParam *spec = G_PARAM_SPEC_PARAM (pspec); */
  GParamSpec *param = value->data[0].v_pointer;
  guint changed = 0;
  
  if (param && !g_value_type_compatible (G_PARAM_SPEC_TYPE (param), G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
      g_param_spec_unref (param);
      value->data[0].v_pointer = NULL;
      changed++;
    }
  
  return changed;
}

static void
param_boxed_init (GParamSpec *pspec)
{
  /* GParamSpecBoxed *bspec = G_PARAM_SPEC_BOXED (pspec); */
}

static void
param_boxed_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_boxed_validate (GParamSpec *pspec,
		      GValue     *value)
{
  /* GParamSpecBoxed *bspec = G_PARAM_SPEC_BOXED (pspec); */
  guint changed = 0;

  /* can't do a whole lot here since we haven't even G_BOXED_TYPE() */
  
  return changed;
}

static gint
param_boxed_values_cmp (GParamSpec    *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

static void
param_pointer_init (GParamSpec *pspec)
{
  /* GParamSpecPointer *spec = G_PARAM_SPEC_POINTER (pspec); */
}

static void
param_pointer_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_pointer_validate (GParamSpec *pspec,
			GValue     *value)
{
  /* GParamSpecPointer *spec = G_PARAM_SPEC_POINTER (pspec); */
  guint changed = 0;
  
  return changed;
}

static gint
param_pointer_values_cmp (GParamSpec   *pspec,
			  const GValue *value1,
			  const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

static void
param_value_array_init (GParamSpec *pspec)
{
  GParamSpecValueArray *aspec = G_PARAM_SPEC_VALUE_ARRAY (pspec);

  aspec->element_spec = NULL;
  aspec->fixed_n_elements = 0; /* disable */
}

static inline guint
value_array_ensure_size (GValueArray *value_array,
			 guint        fixed_n_elements)
{
  guint changed = 0;

  if (fixed_n_elements)
    {
      while (value_array->n_values < fixed_n_elements)
	{
	  g_value_array_append (value_array, NULL);
	  changed++;
	}
      while (value_array->n_values > fixed_n_elements)
	{
	  g_value_array_remove (value_array, value_array->n_values - 1);
	  changed++;
	}
    }
  return changed;
}

static void
param_value_array_finalize (GParamSpec *pspec)
{
  GParamSpecValueArray *aspec = G_PARAM_SPEC_VALUE_ARRAY (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_VALUE_ARRAY));

  if (aspec->element_spec)
    {
      g_param_spec_unref (aspec->element_spec);
      aspec->element_spec = NULL;
    }

  parent_class->finalize (pspec);
}

static void
param_value_array_set_default (GParamSpec *pspec,
			       GValue     *value)
{
  GParamSpecValueArray *aspec = G_PARAM_SPEC_VALUE_ARRAY (pspec);

  if (!value->data[0].v_pointer && aspec->fixed_n_elements)
    value->data[0].v_pointer = g_value_array_new (aspec->fixed_n_elements);

  if (value->data[0].v_pointer)
    {
      /* g_value_reset (value);  already done */
      value_array_ensure_size (value->data[0].v_pointer, aspec->fixed_n_elements);
    }
}

static gboolean
param_value_array_validate (GParamSpec *pspec,
			    GValue     *value)
{
  GParamSpecValueArray *aspec = G_PARAM_SPEC_VALUE_ARRAY (pspec);
  GValueArray *value_array = value->data[0].v_pointer;
  guint changed = 0;

  if (!value->data[0].v_pointer && aspec->fixed_n_elements)
    value->data[0].v_pointer = g_value_array_new (aspec->fixed_n_elements);

  if (value->data[0].v_pointer)
    {
      /* ensure array size validity */
      changed += value_array_ensure_size (value_array, aspec->fixed_n_elements);
      
      /* ensure array values validity against a present element spec */
      if (aspec->element_spec)
	{
	  GParamSpec *element_spec = aspec->element_spec;
	  guint i;
	  
	  for (i = 0; i < value_array->n_values; i++)
	    {
	      GValue *element = value_array->values + i;
	      
	      /* need to fixup value type, or ensure that the array value is initialized at all */
	      if (!g_value_type_compatible (G_VALUE_TYPE (element), G_PARAM_SPEC_VALUE_TYPE (element_spec)))
		{
		  if (G_VALUE_TYPE (element) != 0)
		    g_value_unset (element);
		  g_value_init (element, G_PARAM_SPEC_VALUE_TYPE (element_spec));
		  g_param_value_set_default (element_spec, element);
		  changed++;
		}
	      /* validate array value against element_spec */
	      changed += g_param_value_validate (element_spec, element);
	    }
	}
    }

  return changed;
}

static gint
param_value_array_values_cmp (GParamSpec   *pspec,
			      const GValue *value1,
			      const GValue *value2)
{
  GParamSpecValueArray *aspec = G_PARAM_SPEC_VALUE_ARRAY (pspec);
  GValueArray *value_array1 = value1->data[0].v_pointer;
  GValueArray *value_array2 = value2->data[0].v_pointer;

  if (!value_array1 || !value_array2)
    return value_array2 ? -1 : value_array1 != value_array2;

  if (value_array1->n_values != value_array2->n_values)
    return value_array1->n_values < value_array2->n_values ? -1 : 1;
  else if (!aspec->element_spec)
    {
      /* we need an element specification for comparisons, so there's not much
       * to compare here, try to at least provide stable lesser/greater result
       */
      return value_array1->n_values < value_array2->n_values ? -1 : value_array1->n_values > value_array2->n_values;
    }
  else /* value_array1->n_values == value_array2->n_values */
    {
      guint i;

      for (i = 0; i < value_array1->n_values; i++)
	{
	  GValue *element1 = value_array1->values + i;
	  GValue *element2 = value_array2->values + i;
	  gint cmp;

	  /* need corresponding element types, provide stable result otherwise */
	  if (G_VALUE_TYPE (element1) != G_VALUE_TYPE (element2))
	    return G_VALUE_TYPE (element1) < G_VALUE_TYPE (element2) ? -1 : 1;
	  cmp = g_param_values_cmp (aspec->element_spec, element1, element2);
	  if (cmp)
	    return cmp;
	}
      return 0;
    }
}

static void
param_object_init (GParamSpec *pspec)
{
  /* GParamSpecObject *ospec = G_PARAM_SPEC_OBJECT (pspec); */
}

static void
param_object_set_default (GParamSpec *pspec,
			  GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_object_validate (GParamSpec *pspec,
		       GValue     *value)
{
  GParamSpecObject *ospec = G_PARAM_SPEC_OBJECT (pspec);
  GObject *object = value->data[0].v_pointer;
  guint changed = 0;
  
  if (object && !g_value_type_compatible (G_OBJECT_TYPE (object), G_PARAM_SPEC_VALUE_TYPE (ospec)))
    {
      g_object_unref (object);
      value->data[0].v_pointer = NULL;
      changed++;
    }
  
  return changed;
}

static gint
param_object_values_cmp (GParamSpec   *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

static void
param_override_init (GParamSpec *pspec)
{
  /* GParamSpecOverride *ospec = G_PARAM_SPEC_OVERRIDE (pspec); */
}

static void
param_override_finalize (GParamSpec *pspec)
{
  GParamSpecOverride *ospec = G_PARAM_SPEC_OVERRIDE (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_OVERRIDE));
  
  if (ospec->overridden)
    {
      g_param_spec_unref (ospec->overridden);
      ospec->overridden = NULL;
    }
  
  parent_class->finalize (pspec);
}

static void
param_override_set_default (GParamSpec *pspec,
			    GValue     *value)
{
  GParamSpecOverride *ospec = G_PARAM_SPEC_OVERRIDE (pspec);

  g_param_value_set_default (ospec->overridden, value);
}

static gboolean
param_override_validate (GParamSpec *pspec,
			 GValue     *value)
{
  GParamSpecOverride *ospec = G_PARAM_SPEC_OVERRIDE (pspec);
  
  return g_param_value_validate (ospec->overridden, value);
}

static gint
param_override_values_cmp (GParamSpec   *pspec,
			   const GValue *value1,
			   const GValue *value2)
{
  GParamSpecOverride *ospec = G_PARAM_SPEC_OVERRIDE (pspec);

  return g_param_values_cmp (ospec->overridden, value1, value2);
}

static void
param_gtype_init (GParamSpec *pspec)
{
}

static void
param_gtype_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  GParamSpecGType *tspec = G_PARAM_SPEC_GTYPE (pspec);

  value->data[0].v_pointer = GSIZE_TO_POINTER (tspec->is_a_type);
}

static gboolean
param_gtype_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecGType *tspec = G_PARAM_SPEC_GTYPE (pspec);
  GType gtype = GPOINTER_TO_SIZE (value->data[0].v_pointer);
  guint changed = 0;
  
  if (tspec->is_a_type != G_TYPE_NONE && !g_type_is_a (gtype, tspec->is_a_type))
    {
      value->data[0].v_pointer = GSIZE_TO_POINTER (tspec->is_a_type);
      changed++;
    }
  
  return changed;
}

static gint
param_gtype_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  GType p1 = GPOINTER_TO_SIZE (value1->data[0].v_pointer);
  GType p2 = GPOINTER_TO_SIZE (value2->data[0].v_pointer);

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

static void
param_variant_init (GParamSpec *pspec)
{
  GParamSpecVariant *vspec = G_PARAM_SPEC_VARIANT (pspec);

  vspec->type = NULL;
  vspec->default_value = NULL;
}

static void
param_variant_finalize (GParamSpec *pspec)
{
  GParamSpecVariant *vspec = G_PARAM_SPEC_VARIANT (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_VARIANT));

  if (vspec->default_value)
    g_variant_unref (vspec->default_value);
  g_variant_type_free (vspec->type);

  parent_class->finalize (pspec);
}

static void
param_variant_set_default (GParamSpec *pspec,
                           GValue     *value)
{
  value->data[0].v_pointer = G_PARAM_SPEC_VARIANT (pspec)->default_value;
  value->data[1].v_uint |= G_VALUE_NOCOPY_CONTENTS;
}

static gboolean
param_variant_validate (GParamSpec *pspec,
                        GValue     *value)
{
  GParamSpecVariant *vspec = G_PARAM_SPEC_VARIANT (pspec);
  GVariant *variant = value->data[0].v_pointer;

  if ((variant == NULL && vspec->default_value != NULL) ||
      (variant != NULL && !g_variant_is_of_type (variant, vspec->type)))
    {
      g_param_value_set_default (pspec, value);
      return TRUE;
    }

  return FALSE;
}

/* g_variant_compare() can only be used with scalar types. */
static gboolean
variant_is_incomparable (GVariant *v)
{
  GVariantClass v_class = g_variant_classify (v);

  return (v_class == G_VARIANT_CLASS_HANDLE ||
          v_class == G_VARIANT_CLASS_VARIANT ||
          v_class ==  G_VARIANT_CLASS_MAYBE||
          v_class == G_VARIANT_CLASS_ARRAY ||
          v_class ==  G_VARIANT_CLASS_TUPLE ||
          v_class == G_VARIANT_CLASS_DICT_ENTRY);
}

static gint
param_variant_values_cmp (GParamSpec   *pspec,
                          const GValue *value1,
                          const GValue *value2)
{
  GVariant *v1 = value1->data[0].v_pointer;
  GVariant *v2 = value2->data[0].v_pointer;

  if (v1 == NULL && v2 == NULL)
    return 0;
  else if (v1 == NULL && v2 != NULL)
    return -1;
  else if (v1 != NULL && v2 == NULL)
    return 1;

  if (!g_variant_type_equal (g_variant_get_type (v1), g_variant_get_type (v2)) ||
      variant_is_incomparable (v1) ||
      variant_is_incomparable (v2))
    return g_variant_equal (v1, v2) ? 0 : (v1 < v2 ? -1 : 1);

  return g_variant_compare (v1, v2);
}

/* --- type initialization --- */
GType *g_param_spec_types = NULL;

void
_g_param_spec_types_init (void)	
{
  const guint n_types = 23;
  GType type, *spec_types, *spec_types_bound;

  g_param_spec_types = g_new0 (GType, n_types);
  spec_types = g_param_spec_types;
  spec_types_bound = g_param_spec_types + n_types;
  
  /* G_TYPE_PARAM_CHAR
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecChar),	/* instance_size */
      16,			/* n_preallocs */
      param_char_init,		/* instance_init */
      G_TYPE_CHAR,		/* value_type */
      NULL,			/* finalize */
      param_char_set_default,	/* value_set_default */
      param_char_validate,	/* value_validate */
      param_int_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamChar"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_CHAR);
  }
  
  /* G_TYPE_PARAM_UCHAR
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecUChar), /* instance_size */
      16,                       /* n_preallocs */
      param_uchar_init,         /* instance_init */
      G_TYPE_UCHAR,		/* value_type */
      NULL,			/* finalize */
      param_uchar_set_default,	/* value_set_default */
      param_uchar_validate,	/* value_validate */
      param_uint_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamUChar"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_UCHAR);
  }
  
  /* G_TYPE_PARAM_BOOLEAN
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecBoolean), /* instance_size */
      16,                         /* n_preallocs */
      NULL,			  /* instance_init */
      G_TYPE_BOOLEAN,             /* value_type */
      NULL,                       /* finalize */
      param_boolean_set_default,  /* value_set_default */
      param_boolean_validate,     /* value_validate */
      param_int_values_cmp,       /* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamBoolean"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_BOOLEAN);
  }
  
  /* G_TYPE_PARAM_INT
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecInt),   /* instance_size */
      16,                       /* n_preallocs */
      param_int_init,           /* instance_init */
      G_TYPE_INT,		/* value_type */
      NULL,			/* finalize */
      param_int_set_default,	/* value_set_default */
      param_int_validate,	/* value_validate */
      param_int_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamInt"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_INT);
  }
  
  /* G_TYPE_PARAM_UINT
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecUInt),  /* instance_size */
      16,                       /* n_preallocs */
      param_uint_init,          /* instance_init */
      G_TYPE_UINT,		/* value_type */
      NULL,			/* finalize */
      param_uint_set_default,	/* value_set_default */
      param_uint_validate,	/* value_validate */
      param_uint_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamUInt"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_UINT);
  }
  
  /* G_TYPE_PARAM_LONG
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecLong),  /* instance_size */
      16,                       /* n_preallocs */
      param_long_init,          /* instance_init */
      G_TYPE_LONG,		/* value_type */
      NULL,			/* finalize */
      param_long_set_default,	/* value_set_default */
      param_long_validate,	/* value_validate */
      param_long_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamLong"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_LONG);
  }
  
  /* G_TYPE_PARAM_ULONG
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecULong), /* instance_size */
      16,                       /* n_preallocs */
      param_ulong_init,         /* instance_init */
      G_TYPE_ULONG,		/* value_type */
      NULL,			/* finalize */
      param_ulong_set_default,	/* value_set_default */
      param_ulong_validate,	/* value_validate */
      param_ulong_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamULong"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_ULONG);
  }

  /* G_TYPE_PARAM_INT64
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecInt64),  /* instance_size */
      16,                       /* n_preallocs */
      param_int64_init,         /* instance_init */
      G_TYPE_INT64,		/* value_type */
      NULL,			/* finalize */
      param_int64_set_default,	/* value_set_default */
      param_int64_validate,	/* value_validate */
      param_int64_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamInt64"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_INT64);
  }
  
  /* G_TYPE_PARAM_UINT64
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecUInt64), /* instance_size */
      16,                       /* n_preallocs */
      param_uint64_init,        /* instance_init */
      G_TYPE_UINT64,		/* value_type */
      NULL,			/* finalize */
      param_uint64_set_default,	/* value_set_default */
      param_uint64_validate,	/* value_validate */
      param_uint64_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamUInt64"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_UINT64);
  }

  /* G_TYPE_PARAM_UNICHAR
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecUnichar), /* instance_size */
      16,                        /* n_preallocs */
      param_unichar_init,	 /* instance_init */
      G_TYPE_UINT,		 /* value_type */
      NULL,			 /* finalize */
      param_unichar_set_default, /* value_set_default */
      param_unichar_validate,	 /* value_validate */
      param_unichar_values_cmp,	 /* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamUnichar"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_UNICHAR);
  }

 /* G_TYPE_PARAM_ENUM
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecEnum),  /* instance_size */
      16,                       /* n_preallocs */
      param_enum_init,          /* instance_init */
      G_TYPE_ENUM,		/* value_type */
      param_enum_finalize,	/* finalize */
      param_enum_set_default,	/* value_set_default */
      param_enum_validate,	/* value_validate */
      param_long_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamEnum"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_ENUM);
  }
  
  /* G_TYPE_PARAM_FLAGS
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecFlags),	/* instance_size */
      16,			/* n_preallocs */
      param_flags_init,		/* instance_init */
      G_TYPE_FLAGS,		/* value_type */
      param_flags_finalize,	/* finalize */
      param_flags_set_default,	/* value_set_default */
      param_flags_validate,	/* value_validate */
      param_ulong_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamFlags"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_FLAGS);
  }
  
  /* G_TYPE_PARAM_FLOAT
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecFloat), /* instance_size */
      16,                       /* n_preallocs */
      param_float_init,         /* instance_init */
      G_TYPE_FLOAT,		/* value_type */
      NULL,			/* finalize */
      param_float_set_default,	/* value_set_default */
      param_float_validate,	/* value_validate */
      param_float_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamFloat"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_FLOAT);
  }
  
  /* G_TYPE_PARAM_DOUBLE
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecDouble),	/* instance_size */
      16,				/* n_preallocs */
      param_double_init,		/* instance_init */
      G_TYPE_DOUBLE,			/* value_type */
      NULL,				/* finalize */
      param_double_set_default,		/* value_set_default */
      param_double_validate,		/* value_validate */
      param_double_values_cmp,		/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamDouble"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_DOUBLE);
  }
  
  /* G_TYPE_PARAM_STRING
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecString),	/* instance_size */
      16,				/* n_preallocs */
      param_string_init,		/* instance_init */
      G_TYPE_STRING,			/* value_type */
      param_string_finalize,		/* finalize */
      param_string_set_default,		/* value_set_default */
      param_string_validate,		/* value_validate */
      param_string_values_cmp,		/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamString"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_STRING);
  }
  
  /* G_TYPE_PARAM_PARAM
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecParam),	/* instance_size */
      16,			/* n_preallocs */
      param_param_init,		/* instance_init */
      G_TYPE_PARAM,		/* value_type */
      NULL,			/* finalize */
      param_param_set_default,	/* value_set_default */
      param_param_validate,	/* value_validate */
      param_pointer_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamParam"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_PARAM);
  }
  
  /* G_TYPE_PARAM_BOXED
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecBoxed),	/* instance_size */
      4,			/* n_preallocs */
      param_boxed_init,		/* instance_init */
      G_TYPE_BOXED,		/* value_type */
      NULL,			/* finalize */
      param_boxed_set_default,	/* value_set_default */
      param_boxed_validate,	/* value_validate */
      param_boxed_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamBoxed"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_BOXED);
  }

  /* G_TYPE_PARAM_POINTER
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecPointer),  /* instance_size */
      0,                           /* n_preallocs */
      param_pointer_init,	   /* instance_init */
      G_TYPE_POINTER,  		   /* value_type */
      NULL,			   /* finalize */
      param_pointer_set_default,   /* value_set_default */
      param_pointer_validate,	   /* value_validate */
      param_pointer_values_cmp,	   /* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamPointer"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_POINTER);
  }
  
  /* G_TYPE_PARAM_VALUE_ARRAY
   */
  {
    /* const */ GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecValueArray),	/* instance_size */
      0,				/* n_preallocs */
      param_value_array_init,		/* instance_init */
      0xdeadbeef,			/* value_type, assigned further down */
      param_value_array_finalize,	/* finalize */
      param_value_array_set_default,	/* value_set_default */
      param_value_array_validate,	/* value_validate */
      param_value_array_values_cmp,	/* values_cmp */
    };
    pspec_info.value_type = G_TYPE_VALUE_ARRAY;
    type = g_param_type_register_static (g_intern_static_string ("GParamValueArray"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_VALUE_ARRAY);
  }

  /* G_TYPE_PARAM_OBJECT
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecObject), /* instance_size */
      16,                        /* n_preallocs */
      param_object_init,	 /* instance_init */
      G_TYPE_OBJECT,		 /* value_type */
      NULL,			 /* finalize */
      param_object_set_default,	 /* value_set_default */
      param_object_validate,	 /* value_validate */
      param_object_values_cmp,	 /* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamObject"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_OBJECT);
  }

  /* G_TYPE_PARAM_OVERRIDE
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecOverride), /* instance_size */
      16,                        /* n_preallocs */
      param_override_init,	 /* instance_init */
      G_TYPE_NONE,		 /* value_type */
      param_override_finalize,	 /* finalize */
      param_override_set_default, /* value_set_default */
      param_override_validate,	  /* value_validate */
      param_override_values_cmp,  /* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamOverride"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_OVERRIDE);
  }

  /* G_TYPE_PARAM_GTYPE
   */
  {
    GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecGType),	/* instance_size */
      0,			/* n_preallocs */
      param_gtype_init,		/* instance_init */
      0xdeadbeef,		/* value_type, assigned further down */
      NULL,			/* finalize */
      param_gtype_set_default,	/* value_set_default */
      param_gtype_validate,	/* value_validate */
      param_gtype_values_cmp,	/* values_cmp */
    };
    pspec_info.value_type = G_TYPE_GTYPE;
    type = g_param_type_register_static (g_intern_static_string ("GParamGType"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_GTYPE);
  }

  /* G_TYPE_PARAM_VARIANT
   */
  {
    const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecVariant), /* instance_size */
      0,                          /* n_preallocs */
      param_variant_init,         /* instance_init */
      G_TYPE_VARIANT,             /* value_type */
      param_variant_finalize,     /* finalize */
      param_variant_set_default,  /* value_set_default */
      param_variant_validate,     /* value_validate */
      param_variant_values_cmp,   /* values_cmp */
    };
    type = g_param_type_register_static (g_intern_static_string ("GParamVariant"), &pspec_info);
    *spec_types++ = type;
    g_assert (type == G_TYPE_PARAM_VARIANT);
  }

  g_assert (spec_types == spec_types_bound);
}

/* --- GParamSpec initialization --- */

/**
 * g_param_spec_char:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecChar instance specifying a %G_TYPE_CHAR property.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_char (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   gint8	minimum,
		   gint8	maximum,
		   gint8	default_value,
		   GParamFlags	flags)
{
  GParamSpecChar *cspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  cspec = g_param_spec_internal (G_TYPE_PARAM_CHAR,
				 name,
				 nick,
				 blurb,
				 flags);
  if (cspec == NULL)
    return NULL;
  
  cspec->minimum = minimum;
  cspec->maximum = maximum;
  cspec->default_value = default_value;
  
  return G_PARAM_SPEC (cspec);
}

/**
 * g_param_spec_uchar:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecUChar instance specifying a %G_TYPE_UCHAR property.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_uchar (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    guint8	 minimum,
		    guint8	 maximum,
		    guint8	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecUChar *uspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  uspec = g_param_spec_internal (G_TYPE_PARAM_UCHAR,
				 name,
				 nick,
				 blurb,
				 flags);
  if (uspec == NULL)
    return NULL;
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

/**
 * g_param_spec_boolean:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecBoolean instance specifying a %G_TYPE_BOOLEAN
 * property. In many cases, it may be more appropriate to use an enum with
 * g_param_spec_enum(), both to improve code clarity by using explicitly named
 * values, and to allow for more values to be added in future without breaking
 * API.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_boolean (const gchar *name,
		      const gchar *nick,
		      const gchar *blurb,
		      gboolean	   default_value,
		      GParamFlags  flags)
{
  GParamSpecBoolean *bspec;

  g_return_val_if_fail (default_value == TRUE || default_value == FALSE, NULL);

  bspec = g_param_spec_internal (G_TYPE_PARAM_BOOLEAN,
				 name,
				 nick,
				 blurb,
				 flags);
  if (bspec == NULL)
    return NULL;
  
  bspec->default_value = default_value;
  
  return G_PARAM_SPEC (bspec);
}

/**
 * g_param_spec_int:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecInt instance specifying a %G_TYPE_INT property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_int (const gchar *name,
		  const gchar *nick,
		  const gchar *blurb,
		  gint	       minimum,
		  gint	       maximum,
		  gint	       default_value,
		  GParamFlags  flags)
{
  GParamSpecInt *ispec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  ispec = g_param_spec_internal (G_TYPE_PARAM_INT,
				 name,
				 nick,
				 blurb,
				 flags);
  if (ispec == NULL)
    return NULL;
  
  ispec->minimum = minimum;
  ispec->maximum = maximum;
  ispec->default_value = default_value;
  
  return G_PARAM_SPEC (ispec);
}

/**
 * g_param_spec_uint:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecUInt instance specifying a %G_TYPE_UINT property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_uint (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   guint	minimum,
		   guint	maximum,
		   guint	default_value,
		   GParamFlags	flags)
{
  GParamSpecUInt *uspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  uspec = g_param_spec_internal (G_TYPE_PARAM_UINT,
				 name,
				 nick,
				 blurb,
				 flags);
  if (uspec == NULL)
    return NULL;
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

/**
 * g_param_spec_long:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecLong instance specifying a %G_TYPE_LONG property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_long (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   glong	minimum,
		   glong	maximum,
		   glong	default_value,
		   GParamFlags	flags)
{
  GParamSpecLong *lspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  lspec = g_param_spec_internal (G_TYPE_PARAM_LONG,
				 name,
				 nick,
				 blurb,
				 flags);
  if (lspec == NULL)
    return NULL;
  
  lspec->minimum = minimum;
  lspec->maximum = maximum;
  lspec->default_value = default_value;
  
  return G_PARAM_SPEC (lspec);
}

/**
 * g_param_spec_ulong:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecULong instance specifying a %G_TYPE_ULONG
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_ulong (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    gulong	 minimum,
		    gulong	 maximum,
		    gulong	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecULong *uspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  uspec = g_param_spec_internal (G_TYPE_PARAM_ULONG,
				 name,
				 nick,
				 blurb,
				 flags);
  if (uspec == NULL)
    return NULL;
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

/**
 * g_param_spec_int64:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecInt64 instance specifying a %G_TYPE_INT64 property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_int64 (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    gint64	 minimum,
		    gint64	 maximum,
		    gint64	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecInt64 *lspec;
  
  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  lspec = g_param_spec_internal (G_TYPE_PARAM_INT64,
				 name,
				 nick,
				 blurb,
				 flags);
  if (lspec == NULL)
    return NULL;
  
  lspec->minimum = minimum;
  lspec->maximum = maximum;
  lspec->default_value = default_value;
  
  return G_PARAM_SPEC (lspec);
}

/**
 * g_param_spec_uint64:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecUInt64 instance specifying a %G_TYPE_UINT64
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_uint64 (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     guint64	  minimum,
		     guint64	  maximum,
		     guint64	  default_value,
		     GParamFlags  flags)
{
  GParamSpecUInt64 *uspec;
  
  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);
  
  uspec = g_param_spec_internal (G_TYPE_PARAM_UINT64,
				 name,
				 nick,
				 blurb,
				 flags);
  if (uspec == NULL)
    return NULL;
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

/**
 * g_param_spec_unichar:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecUnichar instance specifying a %G_TYPE_UINT
 * property. #GValue structures for this property can be accessed with
 * g_value_set_uint() and g_value_get_uint().
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_unichar (const gchar *name,
		      const gchar *nick,
		      const gchar *blurb,
		      gunichar	   default_value,
		      GParamFlags  flags)
{
  GParamSpecUnichar *uspec;

  uspec = g_param_spec_internal (G_TYPE_PARAM_UNICHAR,
				 name,
				 nick,
				 blurb,
				 flags);
  if (uspec == NULL)
    return NULL;
  
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

/**
 * g_param_spec_enum:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @enum_type: a #GType derived from %G_TYPE_ENUM
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecEnum instance specifying a %G_TYPE_ENUM
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_enum (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   GType	enum_type,
		   gint		default_value,
		   GParamFlags	flags)
{
  GParamSpecEnum *espec;
  GEnumClass *enum_class;
  
  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);

  enum_class = g_type_class_ref (enum_type);

  g_return_val_if_fail (g_enum_get_value (enum_class, default_value) != NULL, NULL);
  
  espec = g_param_spec_internal (G_TYPE_PARAM_ENUM,
				 name,
				 nick,
				 blurb,
				 flags);
  if (espec == NULL)
    return NULL;
  
  espec->enum_class = enum_class;
  espec->default_value = default_value;
  G_PARAM_SPEC (espec)->value_type = enum_type;
  
  return G_PARAM_SPEC (espec);
}

/**
 * g_param_spec_flags:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @flags_type: a #GType derived from %G_TYPE_FLAGS
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecFlags instance specifying a %G_TYPE_FLAGS
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_flags (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType	 flags_type,
		    guint	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecFlags *fspec;
  GFlagsClass *flags_class;
  
  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), NULL);

  flags_class = g_type_class_ref (flags_type);

  g_return_val_if_fail ((default_value & flags_class->mask) == default_value, NULL);
  
  fspec = g_param_spec_internal (G_TYPE_PARAM_FLAGS,
				 name,
				 nick,
				 blurb,
				 flags);
  if (fspec == NULL)
    return NULL;
  
  fspec->flags_class = flags_class;
  fspec->default_value = default_value;
  G_PARAM_SPEC (fspec)->value_type = flags_type;
  
  return G_PARAM_SPEC (fspec);
}

/**
 * g_param_spec_float:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecFloat instance specifying a %G_TYPE_FLOAT property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_float (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    gfloat	 minimum,
		    gfloat	 maximum,
		    gfloat	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecFloat *fspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  fspec = g_param_spec_internal (G_TYPE_PARAM_FLOAT,
				 name,
				 nick,
				 blurb,
				 flags);
  if (fspec == NULL)
    return NULL;
  
  fspec->minimum = minimum;
  fspec->maximum = maximum;
  fspec->default_value = default_value;
  
  return G_PARAM_SPEC (fspec);
}

/**
 * g_param_spec_double:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @minimum: minimum value for the property specified
 * @maximum: maximum value for the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecDouble instance specifying a %G_TYPE_DOUBLE
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_double (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     gdouble	  minimum,
		     gdouble	  maximum,
		     gdouble	  default_value,
		     GParamFlags  flags)
{
  GParamSpecDouble *dspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum, NULL);

  dspec = g_param_spec_internal (G_TYPE_PARAM_DOUBLE,
				 name,
				 nick,
				 blurb,
				 flags);
  if (dspec == NULL)
    return NULL;
  
  dspec->minimum = minimum;
  dspec->maximum = maximum;
  dspec->default_value = default_value;
  
  return G_PARAM_SPEC (dspec);
}

/**
 * g_param_spec_string:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @default_value: (nullable): default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecString instance.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_string (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     const gchar *default_value,
		     GParamFlags  flags)
{
  GParamSpecString *sspec = g_param_spec_internal (G_TYPE_PARAM_STRING,
						   name,
						   nick,
						   blurb,
						   flags);
  if (sspec == NULL)
    return NULL;

  g_free (sspec->default_value);
  sspec->default_value = g_strdup (default_value);
  
  return G_PARAM_SPEC (sspec);
}

/**
 * g_param_spec_param:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @param_type: a #GType derived from %G_TYPE_PARAM
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecParam instance specifying a %G_TYPE_PARAM
 * property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_param (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType	 param_type,
		    GParamFlags  flags)
{
  GParamSpecParam *pspec;
  
  g_return_val_if_fail (G_TYPE_IS_PARAM (param_type), NULL);
  
  pspec = g_param_spec_internal (G_TYPE_PARAM_PARAM,
				 name,
				 nick,
				 blurb,
				 flags);
  if (pspec == NULL)
    return NULL;

  G_PARAM_SPEC (pspec)->value_type = param_type;
  
  return G_PARAM_SPEC (pspec);
}

/**
 * g_param_spec_boxed:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @boxed_type: %G_TYPE_BOXED derived type of this property
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecBoxed instance specifying a %G_TYPE_BOXED
 * derived property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_boxed (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType	 boxed_type,
		    GParamFlags  flags)
{
  GParamSpecBoxed *bspec;
  
  g_return_val_if_fail (G_TYPE_IS_BOXED (boxed_type), NULL);
  g_return_val_if_fail (G_TYPE_IS_VALUE_TYPE (boxed_type), NULL);
  
  bspec = g_param_spec_internal (G_TYPE_PARAM_BOXED,
				 name,
				 nick,
				 blurb,
				 flags);
  if (bspec == NULL)
    return NULL;

  G_PARAM_SPEC (bspec)->value_type = boxed_type;
  
  return G_PARAM_SPEC (bspec);
}

/**
 * g_param_spec_pointer:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecPointer instance specifying a pointer property.
 * Where possible, it is better to use g_param_spec_object() or
 * g_param_spec_boxed() to expose memory management information.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_pointer (const gchar *name,
		      const gchar *nick,
		      const gchar *blurb,
		      GParamFlags  flags)
{
  GParamSpecPointer *pspec;
  
  pspec = g_param_spec_internal (G_TYPE_PARAM_POINTER,
				 name,
				 nick,
				 blurb,
				 flags);
  if (pspec == NULL)
    return NULL;

  return G_PARAM_SPEC (pspec);
}

/**
 * g_param_spec_gtype:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @is_a_type: a #GType whose subtypes are allowed as values
 *  of the property (use %G_TYPE_NONE for any type)
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecGType instance specifying a
 * %G_TYPE_GTYPE property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Since: 2.10
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_gtype (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType        is_a_type,
		    GParamFlags  flags)
{
  GParamSpecGType *tspec;
  
  tspec = g_param_spec_internal (G_TYPE_PARAM_GTYPE,
				 name,
				 nick,
				 blurb,
				 flags);
  if (tspec == NULL)
    return NULL;

  tspec->is_a_type = is_a_type;

  return G_PARAM_SPEC (tspec);
}

/**
 * g_param_spec_value_array: (skip)
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @element_spec: a #GParamSpec describing the elements contained in
 *  arrays of this property, may be %NULL
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecValueArray instance specifying a
 * %G_TYPE_VALUE_ARRAY property. %G_TYPE_VALUE_ARRAY is a
 * %G_TYPE_BOXED type, as such, #GValue structures for this property
 * can be accessed with g_value_set_boxed() and g_value_get_boxed().
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: a newly created parameter specification
 */
GParamSpec*
g_param_spec_value_array (const gchar *name,
			  const gchar *nick,
			  const gchar *blurb,
			  GParamSpec  *element_spec,
			  GParamFlags  flags)
{
  GParamSpecValueArray *aspec;
  
  if (element_spec)
    g_return_val_if_fail (G_IS_PARAM_SPEC (element_spec), NULL);
  
  aspec = g_param_spec_internal (G_TYPE_PARAM_VALUE_ARRAY,
				 name,
				 nick,
				 blurb,
				 flags);
  if (aspec == NULL)
    return NULL;

  if (element_spec)
    {
      aspec->element_spec = g_param_spec_ref (element_spec);
      g_param_spec_sink (element_spec);
    }

  return G_PARAM_SPEC (aspec);
}

/**
 * g_param_spec_object:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @object_type: %G_TYPE_OBJECT derived type of this property
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecBoxed instance specifying a %G_TYPE_OBJECT
 * derived property.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): a newly created parameter specification
 */
GParamSpec*
g_param_spec_object (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     GType	  object_type,
		     GParamFlags  flags)
{
  GParamSpecObject *ospec;
  
  g_return_val_if_fail (g_type_is_a (object_type, G_TYPE_OBJECT), NULL);
  
  ospec = g_param_spec_internal (G_TYPE_PARAM_OBJECT,
				 name,
				 nick,
				 blurb,
				 flags);
  if (ospec == NULL)
    return NULL;

  G_PARAM_SPEC (ospec)->value_type = object_type;
  
  return G_PARAM_SPEC (ospec);
}

/**
 * g_param_spec_override: (skip)
 * @name: the name of the property.
 * @overridden: The property that is being overridden
 *
 * Creates a new property of type #GParamSpecOverride. This is used
 * to direct operations to another paramspec, and will not be directly
 * useful unless you are implementing a new base type similar to GObject.
 *
 * Since: 2.4
 *
 * Returns: the newly created #GParamSpec
 */
GParamSpec*
g_param_spec_override (const gchar *name,
		       GParamSpec  *overridden)
{
  GParamSpec *pspec;
  
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (G_IS_PARAM_SPEC (overridden), NULL);
  
  /* Dereference further redirections for property that was passed in
   */
  while (TRUE)
    {
      GParamSpec *indirect = g_param_spec_get_redirect_target (overridden);
      if (indirect)
	overridden = indirect;
      else
	break;
    }

  pspec = g_param_spec_internal (G_TYPE_PARAM_OVERRIDE,
				 name, NULL, NULL,
				 overridden->flags);
  if (pspec == NULL)
    return NULL;
  
  pspec->value_type = G_PARAM_SPEC_VALUE_TYPE (overridden);
  G_PARAM_SPEC_OVERRIDE (pspec)->overridden = g_param_spec_ref (overridden);

  return pspec;
}

/**
 * g_param_spec_variant:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @type: a #GVariantType
 * @default_value: (nullable) (transfer full): a #GVariant of type @type to
 *                 use as the default value, or %NULL
 * @flags: flags for the property specified
 *
 * Creates a new #GParamSpecVariant instance specifying a #GVariant
 * property.
 *
 * If @default_value is floating, it is consumed.
 *
 * See g_param_spec_internal() for details on property names.
 *
 * Returns: (transfer full): the newly created #GParamSpec
 *
 * Since: 2.26
 */
GParamSpec*
g_param_spec_variant (const gchar        *name,
                      const gchar        *nick,
                      const gchar        *blurb,
                      const GVariantType *type,
                      GVariant           *default_value,
                      GParamFlags         flags)
{
  GParamSpecVariant *vspec;

  g_return_val_if_fail (type != NULL, NULL);
  g_return_val_if_fail (default_value == NULL ||
                        g_variant_is_of_type (default_value, type), NULL);

  vspec = g_param_spec_internal (G_TYPE_PARAM_VARIANT,
                                 name,
                                 nick,
                                 blurb,
                                 flags);
  if (vspec == NULL)
    return NULL;

  vspec->type = g_variant_type_copy (type);
  if (default_value)
    vspec->default_value = g_variant_ref_sink (default_value);

  return G_PARAM_SPEC (vspec);
}
