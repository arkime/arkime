/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997-1999, 2000-2001 Tim Janik and Red Hat, Inc.
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

/*
 * FIXME: MT-safety
 */

#include "config.h"

#include <string.h>

#include "gvalue.h"
#include "gvaluecollector.h"
#include "gbsearcharray.h"
#include "gtype-private.h"


/**
 * SECTION:generic_values
 * @short_description: A polymorphic type that can hold values of any
 *     other type
 * @see_also: The fundamental types which all support #GValue
 *     operations and thus can be used as a type initializer for
 *     g_value_init() are defined by a separate interface.  See the
 *     [standard values API][gobject-Standard-Parameter-and-Value-Types]
 *     for details
 * @title: Generic values
 *
 * The #GValue structure is basically a variable container that consists
 * of a type identifier and a specific value of that type.
 * The type identifier within a #GValue structure always determines the
 * type of the associated value.
 * To create a undefined #GValue structure, simply create a zero-filled
 * #GValue structure. To initialize the #GValue, use the g_value_init()
 * function. A #GValue cannot be used until it is initialized.
 * The basic type operations (such as freeing and copying) are determined
 * by the #GTypeValueTable associated with the type ID stored in the #GValue.
 * Other #GValue operations (such as converting values between types) are
 * provided by this interface.
 *
 * The code in the example program below demonstrates #GValue's
 * features.
 *
 * |[<!-- language="C" --> 
 * #include <glib-object.h>
 *
 * static void
 * int2string (const GValue *src_value,
 *             GValue       *dest_value)
 * {
 *   if (g_value_get_int (src_value) == 42)
 *     g_value_set_static_string (dest_value, "An important number");
 *   else
 *     g_value_set_static_string (dest_value, "What's that?");
 * }
 *
 * int
 * main (int   argc,
 *       char *argv[])
 * {
 *   // GValues must be initialized
 *   GValue a = G_VALUE_INIT;
 *   GValue b = G_VALUE_INIT;
 *   const gchar *message;
 *
 *   // The GValue starts empty
 *   g_assert (!G_VALUE_HOLDS_STRING (&a));
 *
 *   // Put a string in it
 *   g_value_init (&a, G_TYPE_STRING);
 *   g_assert (G_VALUE_HOLDS_STRING (&a));
 *   g_value_set_static_string (&a, "Hello, world!");
 *   g_printf ("%s\n", g_value_get_string (&a));
 *
 *   // Reset it to its pristine state
 *   g_value_unset (&a);
 *
 *   // It can then be reused for another type
 *   g_value_init (&a, G_TYPE_INT);
 *   g_value_set_int (&a, 42);
 *
 *   // Attempt to transform it into a GValue of type STRING
 *   g_value_init (&b, G_TYPE_STRING);
 *
 *   // An INT is transformable to a STRING
 *   g_assert (g_value_type_transformable (G_TYPE_INT, G_TYPE_STRING));
 *
 *   g_value_transform (&a, &b);
 *   g_printf ("%s\n", g_value_get_string (&b));
 *
 *   // Attempt to transform it again using a custom transform function
 *   g_value_register_transform_func (G_TYPE_INT, G_TYPE_STRING, int2string);
 *   g_value_transform (&a, &b);
 *   g_printf ("%s\n", g_value_get_string (&b));
 *   return 0;
 * }
 * ]|
 */


/* --- typedefs & structures --- */
typedef struct {
  GType src_type;
  GType dest_type;
  GValueTransform func;
} TransformEntry;


/* --- prototypes --- */
static gint	transform_entries_cmp	(gconstpointer bsearch_node1,
					 gconstpointer bsearch_node2);


/* --- variables --- */
static GBSearchArray *transform_array = NULL;
static GBSearchConfig transform_bconfig = {
  sizeof (TransformEntry),
  transform_entries_cmp,
  G_BSEARCH_ARRAY_ALIGN_POWER2,
};


/* --- functions --- */
void
_g_value_c_init (void)
{
  transform_array = g_bsearch_array_create (&transform_bconfig);
}

static inline void		/* keep this function in sync with gvaluecollector.h and gboxed.c */
value_meminit (GValue *value,
	       GType   value_type)
{
  value->g_type = value_type;
  memset (value->data, 0, sizeof (value->data));
}

/**
 * g_value_init:
 * @value: A zero-filled (uninitialized) #GValue structure.
 * @g_type: Type the #GValue should hold values of.
 *
 * Initializes @value with the default value of @type.
 *
 * Returns: (transfer none): the #GValue structure that has been passed in
 */
GValue*
g_value_init (GValue *value,
	      GType   g_type)
{
  /* g_return_val_if_fail (G_TYPE_IS_VALUE (g_type), NULL);	be more elaborate below */
  g_return_val_if_fail (value != NULL, NULL);
  /* g_return_val_if_fail (G_VALUE_TYPE (value) == 0, NULL);	be more elaborate below */

  if (G_TYPE_IS_VALUE (g_type) && G_VALUE_TYPE (value) == 0)
    {
      GTypeValueTable *value_table = g_type_value_table_peek (g_type);

      /* setup and init */
      value_meminit (value, g_type);
      value_table->value_init (value);
    }
  else if (G_VALUE_TYPE (value))
    g_warning ("%s: cannot initialize GValue with type '%s', the value has already been initialized as '%s'",
	       G_STRLOC,
	       g_type_name (g_type),
	       g_type_name (G_VALUE_TYPE (value)));
  else /* !G_TYPE_IS_VALUE (g_type) */
    g_warning ("%s: cannot initialize GValue with type '%s', %s",
	       G_STRLOC,
	       g_type_name (g_type),
	       g_type_value_table_peek (g_type) ?
	       "this type is abstract with regards to GValue use, use a more specific (derived) type" :
	       "this type has no GTypeValueTable implementation");
  return value;
}

/**
 * g_value_copy:
 * @src_value: An initialized #GValue structure.
 * @dest_value: An initialized #GValue structure of the same type as @src_value.
 *
 * Copies the value of @src_value into @dest_value.
 */
void
g_value_copy (const GValue *src_value,
	      GValue       *dest_value)
{
  g_return_if_fail (G_IS_VALUE (src_value));
  g_return_if_fail (G_IS_VALUE (dest_value));
  g_return_if_fail (g_value_type_compatible (G_VALUE_TYPE (src_value), G_VALUE_TYPE (dest_value)));
  
  if (src_value != dest_value)
    {
      GType dest_type = G_VALUE_TYPE (dest_value);
      GTypeValueTable *value_table = g_type_value_table_peek (dest_type);

      /* make sure dest_value's value is free()d */
      if (value_table->value_free)
	value_table->value_free (dest_value);

      /* setup and copy */
      value_meminit (dest_value, dest_type);
      value_table->value_copy (src_value, dest_value);
    }
}

/**
 * g_value_reset:
 * @value: An initialized #GValue structure.
 *
 * Clears the current value in @value and resets it to the default value
 * (as if the value had just been initialized).
 *
 * Returns: the #GValue structure that has been passed in
 */
GValue*
g_value_reset (GValue *value)
{
  GTypeValueTable *value_table;
  GType g_type;
  
  g_return_val_if_fail (G_IS_VALUE (value), NULL);
  
  g_type = G_VALUE_TYPE (value);
  value_table = g_type_value_table_peek (g_type);

  /* make sure value's value is free()d */
  if (value_table->value_free)
    value_table->value_free (value);

  /* setup and init */
  value_meminit (value, g_type);
  value_table->value_init (value);

  return value;
}

/**
 * g_value_unset:
 * @value: An initialized #GValue structure.
 *
 * Clears the current value in @value (if any) and "unsets" the type,
 * this releases all resources associated with this GValue. An unset
 * value is the same as an uninitialized (zero-filled) #GValue
 * structure.
 */
void
g_value_unset (GValue *value)
{
  GTypeValueTable *value_table;
  
  if (value->g_type == 0)
    return;

  g_return_if_fail (G_IS_VALUE (value));

  value_table = g_type_value_table_peek (G_VALUE_TYPE (value));

  if (value_table->value_free)
    value_table->value_free (value);
  memset (value, 0, sizeof (*value));
}

/**
 * g_value_fits_pointer:
 * @value: An initialized #GValue structure.
 *
 * Determines if @value will fit inside the size of a pointer value.
 * This is an internal function introduced mainly for C marshallers.
 *
 * Returns: %TRUE if @value will fit inside a pointer value.
 */
gboolean
g_value_fits_pointer (const GValue *value)
{
  GTypeValueTable *value_table;

  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  value_table = g_type_value_table_peek (G_VALUE_TYPE (value));

  return value_table->value_peek_pointer != NULL;
}

/**
 * g_value_peek_pointer:
 * @value: An initialized #GValue structure
 *
 * Returns the value contents as pointer. This function asserts that
 * g_value_fits_pointer() returned %TRUE for the passed in value.
 * This is an internal function introduced mainly for C marshallers.
 *
 * Returns: (transfer none): the value contents as pointer
 */
gpointer
g_value_peek_pointer (const GValue *value)
{
  GTypeValueTable *value_table;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  value_table = g_type_value_table_peek (G_VALUE_TYPE (value));
  if (!value_table->value_peek_pointer)
    {
      g_return_val_if_fail (g_value_fits_pointer (value) == TRUE, NULL);
      return NULL;
    }

  return value_table->value_peek_pointer (value);
}

/**
 * g_value_set_instance:
 * @value: An initialized #GValue structure.
 * @instance: (nullable): the instance
 *
 * Sets @value from an instantiatable type via the
 * value_table's collect_value() function.
 */
void
g_value_set_instance (GValue  *value,
		      gpointer instance)
{
  GType g_type;
  GTypeValueTable *value_table;
  GTypeCValue cvalue;
  gchar *error_msg;
  
  g_return_if_fail (G_IS_VALUE (value));
  if (instance)
    {
      g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
      g_return_if_fail (g_value_type_compatible (G_TYPE_FROM_INSTANCE (instance), G_VALUE_TYPE (value)));
    }
  
  g_type = G_VALUE_TYPE (value);
  value_table = g_type_value_table_peek (g_type);
  
  g_return_if_fail (strcmp (value_table->collect_format, "p") == 0);
  
  memset (&cvalue, 0, sizeof (cvalue));
  cvalue.v_pointer = instance;
  
  /* make sure value's value is free()d */
  if (value_table->value_free)
    value_table->value_free (value);

  /* setup and collect */
  value_meminit (value, g_type);
  error_msg = value_table->collect_value (value, 1, &cvalue, 0);
  if (error_msg)
    {
      g_warning ("%s: %s", G_STRLOC, error_msg);
      g_free (error_msg);
      
      /* we purposely leak the value here, it might not be
       * in a sane state if an error condition occoured
       */
      value_meminit (value, g_type);
      value_table->value_init (value);
    }
}

/**
 * g_value_init_from_instance:
 * @value: An uninitialized #GValue structure.
 * @instance: (type GObject.TypeInstance): the instance
 *
 * Initializes and sets @value from an instantiatable type via the
 * value_table's collect_value() function.
 *
 * Note: The @value will be initialised with the exact type of
 * @instance.  If you wish to set the @value's type to a different GType
 * (such as a parent class GType), you need to manually call
 * g_value_init() and g_value_set_instance().
 *
 * Since: 2.42
 */
void
g_value_init_from_instance (GValue  *value,
                            gpointer instance)
{
  g_return_if_fail (value != NULL && G_VALUE_TYPE(value) == 0);

  if (G_IS_OBJECT (instance))
    {
      /* Fast-path.
       * If G_IS_OBJECT() succeeds we know:
       * * that instance is present and valid
       * * that it is a GObject, and therefore we can directly
       *   use the collect implementation (g_object_ref) */
      value_meminit (value, G_TYPE_FROM_INSTANCE (instance));
      value->data[0].v_pointer = g_object_ref (instance);
    }
  else
    {  
      GType g_type;
      GTypeValueTable *value_table;
      GTypeCValue cvalue;
      gchar *error_msg;

      g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));

      g_type = G_TYPE_FROM_INSTANCE (instance);
      value_table = g_type_value_table_peek (g_type);
      g_return_if_fail (strcmp (value_table->collect_format, "p") == 0);

      memset (&cvalue, 0, sizeof (cvalue));
      cvalue.v_pointer = instance;

      /* setup and collect */
      value_meminit (value, g_type);
      value_table->value_init (value);
      error_msg = value_table->collect_value (value, 1, &cvalue, 0);
      if (error_msg)
        {
          g_warning ("%s: %s", G_STRLOC, error_msg);
          g_free (error_msg);

          /* we purposely leak the value here, it might not be
           * in a sane state if an error condition occoured
           */
          value_meminit (value, g_type);
          value_table->value_init (value);
        }
    }
}

static GValueTransform
transform_func_lookup (GType src_type,
		       GType dest_type)
{
  TransformEntry entry;

  entry.src_type = src_type;
  do
    {
      entry.dest_type = dest_type;
      do
	{
	  TransformEntry *e;
	  
	  e = g_bsearch_array_lookup (transform_array, &transform_bconfig, &entry);
	  if (e)
	    {
	      /* need to check that there hasn't been a change in value handling */
	      if (g_type_value_table_peek (entry.dest_type) == g_type_value_table_peek (dest_type) &&
		  g_type_value_table_peek (entry.src_type) == g_type_value_table_peek (src_type))
		return e->func;
	    }
	  entry.dest_type = g_type_parent (entry.dest_type);
	}
      while (entry.dest_type);
      
      entry.src_type = g_type_parent (entry.src_type);
    }
  while (entry.src_type);

  return NULL;
}

static gint
transform_entries_cmp (gconstpointer bsearch_node1,
		       gconstpointer bsearch_node2)
{
  const TransformEntry *e1 = bsearch_node1;
  const TransformEntry *e2 = bsearch_node2;
  gint cmp = G_BSEARCH_ARRAY_CMP (e1->src_type, e2->src_type);

  if (cmp)
    return cmp;
  else
    return G_BSEARCH_ARRAY_CMP (e1->dest_type, e2->dest_type);
}

/**
 * g_value_register_transform_func: (skip)
 * @src_type: Source type.
 * @dest_type: Target type.
 * @transform_func: a function which transforms values of type @src_type
 *  into value of type @dest_type
 *
 * Registers a value transformation function for use in g_value_transform().
 * A previously registered transformation function for @src_type and @dest_type
 * will be replaced.
 */
void
g_value_register_transform_func (GType           src_type,
				 GType           dest_type,
				 GValueTransform transform_func)
{
  TransformEntry entry;

  /* these checks won't pass for dynamic types.
   * g_return_if_fail (G_TYPE_HAS_VALUE_TABLE (src_type));
   * g_return_if_fail (G_TYPE_HAS_VALUE_TABLE (dest_type));
   */
  g_return_if_fail (transform_func != NULL);

  entry.src_type = src_type;
  entry.dest_type = dest_type;

#if 0 /* let transform function replacement be a valid operation */
  if (g_bsearch_array_lookup (transform_array, &transform_bconfig, &entry))
    g_warning ("reregistering value transformation function (%p) for '%s' to '%s'",
	       transform_func,
	       g_type_name (src_type),
	       g_type_name (dest_type));
#endif

  entry.func = transform_func;
  transform_array = g_bsearch_array_replace (transform_array, &transform_bconfig, &entry);
}

/**
 * g_value_type_transformable:
 * @src_type: Source type.
 * @dest_type: Target type.
 *
 * Check whether g_value_transform() is able to transform values
 * of type @src_type into values of type @dest_type. Note that for
 * the types to be transformable, they must be compatible or a
 * transformation function must be registered.
 *
 * Returns: %TRUE if the transformation is possible, %FALSE otherwise.
 */
gboolean
g_value_type_transformable (GType src_type,
			    GType dest_type)
{
  g_return_val_if_fail (G_TYPE_IS_VALUE (src_type), FALSE);
  g_return_val_if_fail (G_TYPE_IS_VALUE (dest_type), FALSE);

  return (g_value_type_compatible (src_type, dest_type) ||
	  transform_func_lookup (src_type, dest_type) != NULL);
}

/**
 * g_value_type_compatible:
 * @src_type: source type to be copied.
 * @dest_type: destination type for copying.
 *
 * Returns whether a #GValue of type @src_type can be copied into
 * a #GValue of type @dest_type.
 *
 * Returns: %TRUE if g_value_copy() is possible with @src_type and @dest_type.
 */
gboolean
g_value_type_compatible (GType src_type,
			 GType dest_type)
{
  g_return_val_if_fail (G_TYPE_IS_VALUE (src_type), FALSE);
  g_return_val_if_fail (G_TYPE_IS_VALUE (dest_type), FALSE);

  return (g_type_is_a (src_type, dest_type) &&
	  g_type_value_table_peek (dest_type) == g_type_value_table_peek (src_type));
}

/**
 * g_value_transform:
 * @src_value: Source value.
 * @dest_value: Target value.
 *
 * Tries to cast the contents of @src_value into a type appropriate
 * to store in @dest_value, e.g. to transform a %G_TYPE_INT value
 * into a %G_TYPE_FLOAT value. Performing transformations between
 * value types might incur precision lossage. Especially
 * transformations into strings might reveal seemingly arbitrary
 * results and shouldn't be relied upon for production code (such
 * as rcfile value or object property serialization).
 *
 * Returns: Whether a transformation rule was found and could be applied.
 *  Upon failing transformations, @dest_value is left untouched.
 */
gboolean
g_value_transform (const GValue *src_value,
		   GValue       *dest_value)
{
  GType dest_type;

  g_return_val_if_fail (G_IS_VALUE (src_value), FALSE);
  g_return_val_if_fail (G_IS_VALUE (dest_value), FALSE);

  dest_type = G_VALUE_TYPE (dest_value);
  if (g_value_type_compatible (G_VALUE_TYPE (src_value), dest_type))
    {
      g_value_copy (src_value, dest_value);
      
      return TRUE;
    }
  else
    {
      GValueTransform transform = transform_func_lookup (G_VALUE_TYPE (src_value), dest_type);

      if (transform)
	{
	  g_value_unset (dest_value);
	  
	  /* setup and transform */
	  value_meminit (dest_value, dest_type);
	  transform (src_value, dest_value);
	  
	  return TRUE;
	}
    }
  return FALSE;
}
