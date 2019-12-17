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
 * MT safe
 */

#include "config.h"

#include <string.h>

#include "gparam.h"
#include "gparamspecs.h"
#include "gvaluecollector.h"
#include "gtype-private.h"

/**
 * SECTION:gparamspec
 * @short_description: Metadata for parameter specifications
 * @see_also: g_object_class_install_property(), g_object_set(),
 *     g_object_get(), g_object_set_property(), g_object_get_property(),
 *     g_value_register_transform_func()
 * @title: GParamSpec
 *
 * #GParamSpec is an object structure that encapsulates the metadata
 * required to specify parameters, such as e.g. #GObject properties.
 *
 * ## Parameter names # {#canonical-parameter-names}
 *
 * Parameter names need to start with a letter (a-z or A-Z).
 * Subsequent characters can be letters, numbers or a '-'.
 * All other characters are replaced by a '-' during construction.
 * The result of this replacement is called the canonical name of
 * the parameter.
 */


/* --- defines --- */
#define PARAM_FLOATING_FLAG                     0x2
#define	G_PARAM_USER_MASK			(~0U << G_PARAM_USER_SHIFT)
#define PSPEC_APPLIES_TO_VALUE(pspec, value)	(G_TYPE_CHECK_VALUE_TYPE ((value), G_PARAM_SPEC_VALUE_TYPE (pspec)))

/* --- prototypes --- */
static void	g_param_spec_class_base_init	 (GParamSpecClass	*class);
static void	g_param_spec_class_base_finalize (GParamSpecClass	*class);
static void	g_param_spec_class_init		 (GParamSpecClass	*class,
						  gpointer               class_data);
static void	g_param_spec_init		 (GParamSpec		*pspec,
						  GParamSpecClass	*class);
static void	g_param_spec_finalize		 (GParamSpec		*pspec);
static void	value_param_init		(GValue		*value);
static void	value_param_free_value		(GValue		*value);
static void	value_param_copy_value		(const GValue	*src_value,
						 GValue		*dest_value);
static void	value_param_transform_value	(const GValue	*src_value,
						 GValue		*dest_value);
static gpointer	value_param_peek_pointer	(const GValue	*value);
static gchar*	value_param_collect_value	(GValue		*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);
static gchar*	value_param_lcopy_value		(const GValue	*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);

typedef struct
{
  GValue default_value;
  GQuark name_quark;
} GParamSpecPrivate;

static gint g_param_private_offset;

/* --- functions --- */
static inline GParamSpecPrivate *
g_param_spec_get_private (GParamSpec *pspec)
{
  return &G_STRUCT_MEMBER (GParamSpecPrivate, pspec, g_param_private_offset);
}

void
_g_param_type_init (void)
{
  static const GTypeFundamentalInfo finfo = {
    (G_TYPE_FLAG_CLASSED |
     G_TYPE_FLAG_INSTANTIATABLE |
     G_TYPE_FLAG_DERIVABLE |
     G_TYPE_FLAG_DEEP_DERIVABLE),
  };
  static const GTypeValueTable param_value_table = {
    value_param_init,           /* value_init */
    value_param_free_value,     /* value_free */
    value_param_copy_value,     /* value_copy */
    value_param_peek_pointer,   /* value_peek_pointer */
    "p",			/* collect_format */
    value_param_collect_value,  /* collect_value */
    "p",			/* lcopy_format */
    value_param_lcopy_value,    /* lcopy_value */
  };
  const GTypeInfo param_spec_info = {
    sizeof (GParamSpecClass),

    (GBaseInitFunc) g_param_spec_class_base_init,
    (GBaseFinalizeFunc) g_param_spec_class_base_finalize,
    (GClassInitFunc) g_param_spec_class_init,
    (GClassFinalizeFunc) NULL,
    NULL,	/* class_data */

    sizeof (GParamSpec),
    0,		/* n_preallocs */
    (GInstanceInitFunc) g_param_spec_init,

    &param_value_table,
  };
  GType type;

  /* This should be registered as GParamSpec instead of GParam, for
   * consistency sake, so that type name can be mapped to struct name,
   * However, some language bindings, most noticeable the python ones
   * depends on the "GParam" identifier, see #548689
   */
  type = g_type_register_fundamental (G_TYPE_PARAM, g_intern_static_string ("GParam"), &param_spec_info, &finfo, G_TYPE_FLAG_ABSTRACT);
  g_assert (type == G_TYPE_PARAM);
  g_param_private_offset = g_type_add_instance_private (type, sizeof (GParamSpecPrivate));
  g_value_register_transform_func (G_TYPE_PARAM, G_TYPE_PARAM, value_param_transform_value);
}

static void
g_param_spec_class_base_init (GParamSpecClass *class)
{
}

static void
g_param_spec_class_base_finalize (GParamSpecClass *class)
{
}

static void
g_param_spec_class_init (GParamSpecClass *class,
			 gpointer         class_data)
{
  class->value_type = G_TYPE_NONE;
  class->finalize = g_param_spec_finalize;
  class->value_set_default = NULL;
  class->value_validate = NULL;
  class->values_cmp = NULL;

  g_type_class_adjust_private_offset (class, &g_param_private_offset);
}

static void
g_param_spec_init (GParamSpec      *pspec,
		   GParamSpecClass *class)
{
  pspec->name = NULL;
  pspec->_nick = NULL;
  pspec->_blurb = NULL;
  pspec->flags = 0;
  pspec->value_type = class->value_type;
  pspec->owner_type = 0;
  pspec->qdata = NULL;
  g_datalist_set_flags (&pspec->qdata, PARAM_FLOATING_FLAG);
  pspec->ref_count = 1;
  pspec->param_id = 0;
}

static void
g_param_spec_finalize (GParamSpec *pspec)
{
  GParamSpecPrivate *priv = g_param_spec_get_private (pspec);

  if (priv->default_value.g_type)
    g_value_reset (&priv->default_value);

  g_datalist_clear (&pspec->qdata);

  if (!(pspec->flags & G_PARAM_STATIC_NICK))
    g_free (pspec->_nick);

  if (!(pspec->flags & G_PARAM_STATIC_BLURB))
    g_free (pspec->_blurb);

  g_type_free_instance ((GTypeInstance*) pspec);
}

/**
 * g_param_spec_ref: (skip)
 * @pspec: a valid #GParamSpec
 *
 * Increments the reference count of @pspec.
 *
 * Returns: the #GParamSpec that was passed into this function
 */
GParamSpec*
g_param_spec_ref (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  g_atomic_int_inc ((int *)&pspec->ref_count);

  return pspec;
}

/**
 * g_param_spec_unref: (skip)
 * @pspec: a valid #GParamSpec
 *
 * Decrements the reference count of a @pspec.
 */
void
g_param_spec_unref (GParamSpec *pspec)
{
  gboolean is_zero;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  is_zero = g_atomic_int_dec_and_test ((int *)&pspec->ref_count);

  if (G_UNLIKELY (is_zero))
    {
      G_PARAM_SPEC_GET_CLASS (pspec)->finalize (pspec);
    }
}

/**
 * g_param_spec_sink:
 * @pspec: a valid #GParamSpec
 *
 * The initial reference count of a newly created #GParamSpec is 1,
 * even though no one has explicitly called g_param_spec_ref() on it
 * yet. So the initial reference count is flagged as "floating", until
 * someone calls `g_param_spec_ref (pspec); g_param_spec_sink
 * (pspec);` in sequence on it, taking over the initial
 * reference count (thus ending up with a @pspec that has a reference
 * count of 1 still, but is not flagged "floating" anymore).
 */
void
g_param_spec_sink (GParamSpec *pspec)
{
  gsize oldvalue;
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  oldvalue = g_atomic_pointer_and (&pspec->qdata, ~(gsize)PARAM_FLOATING_FLAG);
  if (oldvalue & PARAM_FLOATING_FLAG)
    g_param_spec_unref (pspec);
}

/**
 * g_param_spec_ref_sink: (skip)
 * @pspec: a valid #GParamSpec
 *
 * Convenience function to ref and sink a #GParamSpec.
 *
 * Since: 2.10
 * Returns: the #GParamSpec that was passed into this function
 */
GParamSpec*
g_param_spec_ref_sink (GParamSpec *pspec)
{
  gsize oldvalue;
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  oldvalue = g_atomic_pointer_and (&pspec->qdata, ~(gsize)PARAM_FLOATING_FLAG);
  if (!(oldvalue & PARAM_FLOATING_FLAG))
    g_param_spec_ref (pspec);

  return pspec;
}

/**
 * g_param_spec_get_name:
 * @pspec: a valid #GParamSpec
 *
 * Get the name of a #GParamSpec.
 *
 * The name is always an "interned" string (as per g_intern_string()).
 * This allows for pointer-value comparisons.
 *
 * Returns: the name of @pspec.
 */
const gchar *
g_param_spec_get_name (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  return pspec->name;
}

/**
 * g_param_spec_get_nick:
 * @pspec: a valid #GParamSpec
 *
 * Get the nickname of a #GParamSpec.
 *
 * Returns: the nickname of @pspec.
 */
const gchar *
g_param_spec_get_nick (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  if (pspec->_nick)
    return pspec->_nick;
  else
    {
      GParamSpec *redirect_target;

      redirect_target = g_param_spec_get_redirect_target (pspec);
      if (redirect_target && redirect_target->_nick)
	return redirect_target->_nick;
    }

  return pspec->name;
}

/**
 * g_param_spec_get_blurb:
 * @pspec: a valid #GParamSpec
 *
 * Get the short description of a #GParamSpec.
 *
 * Returns: the short description of @pspec.
 */
const gchar *
g_param_spec_get_blurb (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  if (pspec->_blurb)
    return pspec->_blurb;
  else
    {
      GParamSpec *redirect_target;

      redirect_target = g_param_spec_get_redirect_target (pspec);
      if (redirect_target && redirect_target->_blurb)
	return redirect_target->_blurb;
    }

  return NULL;
}

static void
canonicalize_key (gchar *key)
{
  gchar *p;
  
  for (p = key; *p != 0; p++)
    {
      gchar c = *p;
      
      if (c != '-' &&
	  (c < '0' || c > '9') &&
	  (c < 'A' || c > 'Z') &&
	  (c < 'a' || c > 'z'))
	*p = '-';
    }
}

static gboolean
is_canonical (const gchar *key)
{
  const gchar *p;

  for (p = key; *p != 0; p++)
    {
      gchar c = *p;
      
      if (c != '-' &&
	  (c < '0' || c > '9') &&
	  (c < 'A' || c > 'Z') &&
	  (c < 'a' || c > 'z'))
	return FALSE;
    }

  return TRUE;
}

/**
 * g_param_spec_internal: (skip)
 * @param_type: the #GType for the property; must be derived from #G_TYPE_PARAM
 * @name: the canonical name of the property
 * @nick: the nickname of the property
 * @blurb: a short description of the property
 * @flags: a combination of #GParamFlags
 *
 * Creates a new #GParamSpec instance.
 *
 * A property name consists of segments consisting of ASCII letters and
 * digits, separated by either the '-' or '_' character. The first
 * character of a property name must be a letter. Names which violate these
 * rules lead to undefined behaviour.
 *
 * When creating and looking up a #GParamSpec, either separator can be
 * used, but they cannot be mixed. Using '-' is considerably more
 * efficient and in fact required when using property names as detail
 * strings for signals.
 *
 * Beyond the name, #GParamSpecs have two more descriptive
 * strings associated with them, the @nick, which should be suitable
 * for use as a label for the property in a property editor, and the
 * @blurb, which should be a somewhat longer description, suitable for
 * e.g. a tooltip. The @nick and @blurb should ideally be localized.
 *
 * Returns: (type GObject.ParamSpec): a newly allocated #GParamSpec instance
 */
gpointer
g_param_spec_internal (GType        param_type,
		       const gchar *name,
		       const gchar *nick,
		       const gchar *blurb,
		       GParamFlags  flags)
{
  GParamSpec *pspec;
  GParamSpecPrivate *priv;
  
  g_return_val_if_fail (G_TYPE_IS_PARAM (param_type) && param_type != G_TYPE_PARAM, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'), NULL);
  g_return_val_if_fail (!(flags & G_PARAM_STATIC_NAME) || is_canonical (name), NULL);
  
  pspec = (gpointer) g_type_create_instance (param_type);

  if (flags & G_PARAM_STATIC_NAME)
    {
      /* pspec->name is not freed if (flags & G_PARAM_STATIC_NAME) */
      pspec->name = (gchar *) g_intern_static_string (name);
      if (!is_canonical (pspec->name))
        g_warning ("G_PARAM_STATIC_NAME used with non-canonical pspec name: %s", pspec->name);
    }
  else
    {
      if (is_canonical (name))
        pspec->name = (gchar *) g_intern_string (name);
      else
        {
          gchar *tmp = g_strdup (name);
          canonicalize_key (tmp);
          pspec->name = (gchar *) g_intern_string (tmp);
          g_free (tmp);
        }
    }

  priv = g_param_spec_get_private (pspec);
  priv->name_quark = g_quark_from_string (pspec->name);

  if (flags & G_PARAM_STATIC_NICK)
    pspec->_nick = (gchar*) nick;
  else
    pspec->_nick = g_strdup (nick);

  if (flags & G_PARAM_STATIC_BLURB)
    pspec->_blurb = (gchar*) blurb;
  else
    pspec->_blurb = g_strdup (blurb);

  pspec->flags = (flags & G_PARAM_USER_MASK) | (flags & G_PARAM_MASK);
  
  return pspec;
}

/**
 * g_param_spec_get_qdata:
 * @pspec: a valid #GParamSpec
 * @quark: a #GQuark, naming the user data pointer
 *
 * Gets back user data pointers stored via g_param_spec_set_qdata().
 *
 * Returns: (transfer none): the user data pointer set, or %NULL
 */
gpointer
g_param_spec_get_qdata (GParamSpec *pspec,
			GQuark      quark)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  
  return quark ? g_datalist_id_get_data (&pspec->qdata, quark) : NULL;
}

/**
 * g_param_spec_set_qdata:
 * @pspec: the #GParamSpec to set store a user data pointer
 * @quark: a #GQuark, naming the user data pointer
 * @data: an opaque user data pointer
 *
 * Sets an opaque, named pointer on a #GParamSpec. The name is
 * specified through a #GQuark (retrieved e.g. via
 * g_quark_from_static_string()), and the pointer can be gotten back
 * from the @pspec with g_param_spec_get_qdata().  Setting a
 * previously set user data pointer, overrides (frees) the old pointer
 * set, using %NULL as pointer essentially removes the data stored.
 */
void
g_param_spec_set_qdata (GParamSpec *pspec,
			GQuark      quark,
			gpointer    data)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (quark > 0);

  g_datalist_id_set_data (&pspec->qdata, quark, data);
}

/**
 * g_param_spec_set_qdata_full: (skip)
 * @pspec: the #GParamSpec to set store a user data pointer
 * @quark: a #GQuark, naming the user data pointer
 * @data: an opaque user data pointer
 * @destroy: function to invoke with @data as argument, when @data needs to
 *  be freed
 *
 * This function works like g_param_spec_set_qdata(), but in addition,
 * a `void (*destroy) (gpointer)` function may be
 * specified which is called with @data as argument when the @pspec is
 * finalized, or the data is being overwritten by a call to
 * g_param_spec_set_qdata() with the same @quark.
 */
void
g_param_spec_set_qdata_full (GParamSpec    *pspec,
			     GQuark         quark,
			     gpointer       data,
			     GDestroyNotify destroy)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (quark > 0);

  g_datalist_id_set_data_full (&pspec->qdata, quark, data, data ? destroy : (GDestroyNotify) NULL);
}

/**
 * g_param_spec_steal_qdata:
 * @pspec: the #GParamSpec to get a stored user data pointer from
 * @quark: a #GQuark, naming the user data pointer
 *
 * Gets back user data pointers stored via g_param_spec_set_qdata()
 * and removes the @data from @pspec without invoking its destroy()
 * function (if any was set).  Usually, calling this function is only
 * required to update user data pointers with a destroy notifier.
 *
 * Returns: (transfer none): the user data pointer set, or %NULL
 */
gpointer
g_param_spec_steal_qdata (GParamSpec *pspec,
			  GQuark      quark)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (quark > 0, NULL);
  
  return g_datalist_id_remove_no_notify (&pspec->qdata, quark);
}

/**
 * g_param_spec_get_redirect_target:
 * @pspec: a #GParamSpec
 *
 * If the paramspec redirects operations to another paramspec,
 * returns that paramspec. Redirect is used typically for
 * providing a new implementation of a property in a derived
 * type while preserving all the properties from the parent
 * type. Redirection is established by creating a property
 * of type #GParamSpecOverride. See g_object_class_override_property()
 * for an example of the use of this capability.
 *
 * Since: 2.4
 *
 * Returns: (transfer none): paramspec to which requests on this
 *          paramspec should be redirected, or %NULL if none.
 */
GParamSpec*
g_param_spec_get_redirect_target (GParamSpec *pspec)
{
  GTypeInstance *inst = (GTypeInstance *)pspec;

  if (inst && inst->g_class && inst->g_class->g_type == G_TYPE_PARAM_OVERRIDE)
    return ((GParamSpecOverride*)pspec)->overridden;
  else
    return NULL;
}

/**
 * g_param_value_set_default:
 * @pspec: a valid #GParamSpec
 * @value: a #GValue of correct type for @pspec
 *
 * Sets @value to its default value as specified in @pspec.
 */
void
g_param_value_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value));

  g_value_reset (value);
  G_PARAM_SPEC_GET_CLASS (pspec)->value_set_default (pspec, value);
}

/**
 * g_param_value_defaults:
 * @pspec: a valid #GParamSpec
 * @value: a #GValue of correct type for @pspec
 *
 * Checks whether @value contains the default value as specified in @pspec.
 *
 * Returns: whether @value contains the canonical default for this @pspec
 */
gboolean
g_param_value_defaults (GParamSpec *pspec,
			GValue     *value)
{
  GValue dflt_value = G_VALUE_INIT;
  gboolean defaults;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value), FALSE);

  g_value_init (&dflt_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  G_PARAM_SPEC_GET_CLASS (pspec)->value_set_default (pspec, &dflt_value);
  defaults = G_PARAM_SPEC_GET_CLASS (pspec)->values_cmp (pspec, value, &dflt_value) == 0;
  g_value_unset (&dflt_value);

  return defaults;
}

/**
 * g_param_value_validate:
 * @pspec: a valid #GParamSpec
 * @value: a #GValue of correct type for @pspec
 *
 * Ensures that the contents of @value comply with the specifications
 * set out by @pspec. For example, a #GParamSpecInt might require
 * that integers stored in @value may not be smaller than -42 and not be
 * greater than +42. If @value contains an integer outside of this range,
 * it is modified accordingly, so the resulting value will fit into the
 * range -42 .. +42.
 *
 * Returns: whether modifying @value was necessary to ensure validity
 */
gboolean
g_param_value_validate (GParamSpec *pspec,
			GValue     *value)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value), FALSE);

  if (G_PARAM_SPEC_GET_CLASS (pspec)->value_validate)
    {
      GValue oval = *value;

      if (G_PARAM_SPEC_GET_CLASS (pspec)->value_validate (pspec, value) ||
	  memcmp (&oval.data, &value->data, sizeof (oval.data)))
	return TRUE;
    }

  return FALSE;
}

/**
 * g_param_value_convert:
 * @pspec: a valid #GParamSpec
 * @src_value: souce #GValue
 * @dest_value: destination #GValue of correct type for @pspec
 * @strict_validation: %TRUE requires @dest_value to conform to @pspec
 * without modifications
 *
 * Transforms @src_value into @dest_value if possible, and then
 * validates @dest_value, in order for it to conform to @pspec.  If
 * @strict_validation is %TRUE this function will only succeed if the
 * transformed @dest_value complied to @pspec without modifications.
 *
 * See also g_value_type_transformable(), g_value_transform() and
 * g_param_value_validate().
 *
 * Returns: %TRUE if transformation and validation were successful,
 *  %FALSE otherwise and @dest_value is left untouched.
 */
gboolean
g_param_value_convert (GParamSpec   *pspec,
		       const GValue *src_value,
		       GValue       *dest_value,
		       gboolean	     strict_validation)
{
  GValue tmp_value = G_VALUE_INIT;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (src_value), FALSE);
  g_return_val_if_fail (G_IS_VALUE (dest_value), FALSE);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, dest_value), FALSE);

  /* better leave dest_value untouched when returning FALSE */

  g_value_init (&tmp_value, G_VALUE_TYPE (dest_value));
  if (g_value_transform (src_value, &tmp_value) &&
      (!g_param_value_validate (pspec, &tmp_value) || !strict_validation))
    {
      g_value_unset (dest_value);
      
      /* values are relocatable */
      memcpy (dest_value, &tmp_value, sizeof (tmp_value));
      
      return TRUE;
    }
  else
    {
      g_value_unset (&tmp_value);
      
      return FALSE;
    }
}

/**
 * g_param_values_cmp:
 * @pspec: a valid #GParamSpec
 * @value1: a #GValue of correct type for @pspec
 * @value2: a #GValue of correct type for @pspec
 *
 * Compares @value1 with @value2 according to @pspec, and return -1, 0 or +1,
 * if @value1 is found to be less than, equal to or greater than @value2,
 * respectively.
 *
 * Returns: -1, 0 or +1, for a less than, equal to or greater than result
 */
gint
g_param_values_cmp (GParamSpec   *pspec,
		    const GValue *value1,
		    const GValue *value2)
{
  gint cmp;

  /* param_values_cmp() effectively does: value1 - value2
   * so the return values are:
   * -1)  value1 < value2
   *  0)  value1 == value2
   *  1)  value1 > value2
   */
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), 0);
  g_return_val_if_fail (G_IS_VALUE (value1), 0);
  g_return_val_if_fail (G_IS_VALUE (value2), 0);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value1), 0);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value2), 0);

  cmp = G_PARAM_SPEC_GET_CLASS (pspec)->values_cmp (pspec, value1, value2);

  return CLAMP (cmp, -1, 1);
}

static void
value_param_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_param_free_value (GValue *value)
{
  if (value->data[0].v_pointer)
    g_param_spec_unref (value->data[0].v_pointer);
}

static void
value_param_copy_value (const GValue *src_value,
			GValue       *dest_value)
{
  if (src_value->data[0].v_pointer)
    dest_value->data[0].v_pointer = g_param_spec_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static void
value_param_transform_value (const GValue *src_value,
			     GValue       *dest_value)
{
  if (src_value->data[0].v_pointer &&
      g_type_is_a (G_PARAM_SPEC_TYPE (dest_value->data[0].v_pointer), G_VALUE_TYPE (dest_value)))
    dest_value->data[0].v_pointer = g_param_spec_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static gpointer
value_param_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
value_param_collect_value (GValue      *value,
			   guint        n_collect_values,
			   GTypeCValue *collect_values,
			   guint        collect_flags)
{
  if (collect_values[0].v_pointer)
    {
      GParamSpec *param = collect_values[0].v_pointer;

      if (param->g_type_instance.g_class == NULL)
	return g_strconcat ("invalid unclassed param spec pointer for value type '",
			    G_VALUE_TYPE_NAME (value),
			    "'",
			    NULL);
      else if (!g_value_type_compatible (G_PARAM_SPEC_TYPE (param), G_VALUE_TYPE (value)))
	return g_strconcat ("invalid param spec type '",
			    G_PARAM_SPEC_TYPE_NAME (param),
			    "' for value type '",
			    G_VALUE_TYPE_NAME (value),
			    "'",
			    NULL);
      value->data[0].v_pointer = g_param_spec_ref (param);
    }
  else
    value->data[0].v_pointer = NULL;

  return NULL;
}

static gchar*
value_param_lcopy_value (const GValue *value,
			 guint         n_collect_values,
			 GTypeCValue  *collect_values,
			 guint         collect_flags)
{
  GParamSpec **param_p = collect_values[0].v_pointer;

  if (!param_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *param_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *param_p = value->data[0].v_pointer;
  else
    *param_p = g_param_spec_ref (value->data[0].v_pointer);

  return NULL;
}


/* --- param spec pool --- */
/**
 * GParamSpecPool:
 *
 * A #GParamSpecPool maintains a collection of #GParamSpecs which can be
 * quickly accessed by owner and name. The implementation of the #GObject property
 * system uses such a pool to store the #GParamSpecs of the properties all object
 * types.
 */
struct _GParamSpecPool
{
  GMutex       mutex;
  gboolean     type_prefixing;
  GHashTable  *hash_table;
};

static guint
param_spec_pool_hash (gconstpointer key_spec)
{
  const GParamSpec *key = key_spec;
  const gchar *p;
  guint h = key->owner_type;

  for (p = key->name; *p; p++)
    h = (h << 5) - h + *p;

  return h;
}

static gboolean
param_spec_pool_equals (gconstpointer key_spec_1,
			gconstpointer key_spec_2)
{
  const GParamSpec *key1 = key_spec_1;
  const GParamSpec *key2 = key_spec_2;

  return (key1->owner_type == key2->owner_type &&
	  strcmp (key1->name, key2->name) == 0);
}

/**
 * g_param_spec_pool_new:
 * @type_prefixing: Whether the pool will support type-prefixed property names.
 *
 * Creates a new #GParamSpecPool.
 *
 * If @type_prefixing is %TRUE, lookups in the newly created pool will
 * allow to specify the owner as a colon-separated prefix of the
 * property name, like "GtkContainer:border-width". This feature is
 * deprecated, so you should always set @type_prefixing to %FALSE.
 *
 * Returns: (transfer none): a newly allocated #GParamSpecPool.
 */
GParamSpecPool*
g_param_spec_pool_new (gboolean type_prefixing)
{
  static GMutex init_mutex;
  GParamSpecPool *pool = g_new (GParamSpecPool, 1);

  memcpy (&pool->mutex, &init_mutex, sizeof (init_mutex));
  pool->type_prefixing = type_prefixing != FALSE;
  pool->hash_table = g_hash_table_new (param_spec_pool_hash, param_spec_pool_equals);

  return pool;
}

/**
 * g_param_spec_pool_insert:
 * @pool: a #GParamSpecPool.
 * @pspec: the #GParamSpec to insert
 * @owner_type: a #GType identifying the owner of @pspec
 *
 * Inserts a #GParamSpec in the pool.
 */
void
g_param_spec_pool_insert (GParamSpecPool *pool,
			  GParamSpec     *pspec,
			  GType           owner_type)
{
  const gchar *p;
  
  if (pool && pspec && owner_type > 0 && pspec->owner_type == 0)
    {
      for (p = pspec->name; *p; p++)
	{
	  if (!strchr (G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-_", *p))
	    {
	      g_warning (G_STRLOC ": pspec name \"%s\" contains invalid characters", pspec->name);
	      return;
	    }
	}
      g_mutex_lock (&pool->mutex);
      pspec->owner_type = owner_type;
      g_param_spec_ref (pspec);
      g_hash_table_add (pool->hash_table, pspec);
      g_mutex_unlock (&pool->mutex);
    }
  else
    {
      g_return_if_fail (pool != NULL);
      g_return_if_fail (pspec);
      g_return_if_fail (owner_type > 0);
      g_return_if_fail (pspec->owner_type == 0);
    }
}

/**
 * g_param_spec_pool_remove:
 * @pool: a #GParamSpecPool
 * @pspec: the #GParamSpec to remove
 *
 * Removes a #GParamSpec from the pool.
 */
void
g_param_spec_pool_remove (GParamSpecPool *pool,
			  GParamSpec     *pspec)
{
  if (pool && pspec)
    {
      g_mutex_lock (&pool->mutex);
      if (g_hash_table_remove (pool->hash_table, pspec))
	g_param_spec_unref (pspec);
      else
	g_warning (G_STRLOC ": attempt to remove unknown pspec '%s' from pool", pspec->name);
      g_mutex_unlock (&pool->mutex);
    }
  else
    {
      g_return_if_fail (pool != NULL);
      g_return_if_fail (pspec);
    }
}

static inline GParamSpec*
param_spec_ht_lookup (GHashTable  *hash_table,
		      const gchar *param_name,
		      GType        owner_type,
		      gboolean     walk_ancestors)
{
  GParamSpec key, *pspec;

  key.owner_type = owner_type;
  key.name = (gchar*) param_name;
  if (walk_ancestors)
    do
      {
	pspec = g_hash_table_lookup (hash_table, &key);
	if (pspec)
	  return pspec;
	key.owner_type = g_type_parent (key.owner_type);
      }
    while (key.owner_type);
  else
    pspec = g_hash_table_lookup (hash_table, &key);

  if (!pspec && !is_canonical (param_name))
    {
      gchar *canonical;

      canonical = g_strdup (key.name);
      canonicalize_key (canonical);

      /* try canonicalized form */
      key.name = canonical;
      key.owner_type = owner_type;

      if (walk_ancestors)
        do
          {
            pspec = g_hash_table_lookup (hash_table, &key);
            if (pspec)
              {
                g_free (canonical);
                return pspec;
              }
            key.owner_type = g_type_parent (key.owner_type);
          }
        while (key.owner_type);
      else
        pspec = g_hash_table_lookup (hash_table, &key);

      g_free (canonical);
    }

  return pspec;
}

/**
 * g_param_spec_pool_lookup:
 * @pool: a #GParamSpecPool
 * @param_name: the name to look for
 * @owner_type: the owner to look for
 * @walk_ancestors: If %TRUE, also try to find a #GParamSpec with @param_name
 *  owned by an ancestor of @owner_type.
 *
 * Looks up a #GParamSpec in the pool.
 *
 * Returns: (transfer none): The found #GParamSpec, or %NULL if no
 * matching #GParamSpec was found.
 */
GParamSpec*
g_param_spec_pool_lookup (GParamSpecPool *pool,
			  const gchar    *param_name,
			  GType           owner_type,
			  gboolean        walk_ancestors)
{
  GParamSpec *pspec;
  gchar *delim;

  g_return_val_if_fail (pool != NULL, NULL);
  g_return_val_if_fail (param_name != NULL, NULL);

  g_mutex_lock (&pool->mutex);

  delim = pool->type_prefixing ? strchr (param_name, ':') : NULL;

  /* try quick and away, i.e. without prefix */
  if (!delim)
    {
      pspec = param_spec_ht_lookup (pool->hash_table, param_name, owner_type, walk_ancestors);
      g_mutex_unlock (&pool->mutex);

      return pspec;
    }

  /* strip type prefix */
  if (pool->type_prefixing && delim[1] == ':')
    {
      guint l = delim - param_name;
      gchar stack_buffer[32], *buffer = l < 32 ? stack_buffer : g_new (gchar, l + 1);
      GType type;
      
      strncpy (buffer, param_name, delim - param_name);
      buffer[l] = 0;
      type = g_type_from_name (buffer);
      if (l >= 32)
	g_free (buffer);
      if (type)		/* type==0 isn't a valid type pefix */
	{
	  /* sanity check, these cases don't make a whole lot of sense */
	  if ((!walk_ancestors && type != owner_type) || !g_type_is_a (owner_type, type))
	    {
	      g_mutex_unlock (&pool->mutex);

	      return NULL;
	    }
	  owner_type = type;
	  param_name += l + 2;
	  pspec = param_spec_ht_lookup (pool->hash_table, param_name, owner_type, walk_ancestors);
	  g_mutex_unlock (&pool->mutex);

	  return pspec;
	}
    }
  /* malformed param_name */

  g_mutex_unlock (&pool->mutex);

  return NULL;
}

static void
pool_list (gpointer key,
	   gpointer value,
	   gpointer user_data)
{
  GParamSpec *pspec = value;
  gpointer *data = user_data;
  GType owner_type = (GType) data[1];

  if (owner_type == pspec->owner_type)
    data[0] = g_list_prepend (data[0], pspec);
}

/**
 * g_param_spec_pool_list_owned:
 * @pool: a #GParamSpecPool
 * @owner_type: the owner to look for
 *
 * Gets an #GList of all #GParamSpecs owned by @owner_type in
 * the pool.
 *
 * Returns: (transfer container) (element-type GObject.ParamSpec): a
 *          #GList of all #GParamSpecs owned by @owner_type in
 *          the pool#GParamSpecs.
 */
GList*
g_param_spec_pool_list_owned (GParamSpecPool *pool,
			      GType           owner_type)
{
  gpointer data[2];

  g_return_val_if_fail (pool != NULL, NULL);
  g_return_val_if_fail (owner_type > 0, NULL);
  
  g_mutex_lock (&pool->mutex);
  data[0] = NULL;
  data[1] = (gpointer) owner_type;
  g_hash_table_foreach (pool->hash_table, pool_list, &data);
  g_mutex_unlock (&pool->mutex);

  return data[0];
}

static gint
pspec_compare_id (gconstpointer a,
		  gconstpointer b)
{
  const GParamSpec *pspec1 = a, *pspec2 = b;

  if (pspec1->param_id < pspec2->param_id)
    return -1;

  if (pspec1->param_id > pspec2->param_id)
    return 1;

  return strcmp (pspec1->name, pspec2->name);
}

static inline GSList*
pspec_list_remove_overridden_and_redirected (GSList     *plist,
					     GHashTable *ht,
					     GType       owner_type,
					     guint      *n_p)
{
  GSList *rlist = NULL;

  while (plist)
    {
      GSList *tmp = plist->next;
      GParamSpec *pspec = plist->data;
      GParamSpec *found;
      gboolean remove = FALSE;

      /* Remove paramspecs that are redirected, and also paramspecs
       * that have are overridden by non-redirected properties.
       * The idea is to get the single paramspec for each name that
       * best corresponds to what the application sees.
       */
      if (g_param_spec_get_redirect_target (pspec))
	remove = TRUE;
      else
	{
	  found = param_spec_ht_lookup (ht, pspec->name, owner_type, TRUE);
	  if (found != pspec)
	    {
	      GParamSpec *redirect = g_param_spec_get_redirect_target (found);
	      if (redirect != pspec)
		remove = TRUE;
	    }
	}

      if (remove)
	{
	  g_slist_free_1 (plist);
	}
      else
	{
	  plist->next = rlist;
	  rlist = plist;
	  *n_p += 1;
	}
      plist = tmp;
    }
  return rlist;
}

static void
pool_depth_list (gpointer key,
		 gpointer value,
		 gpointer user_data)
{
  GParamSpec *pspec = value;
  gpointer *data = user_data;
  GSList **slists = data[0];
  GType owner_type = (GType) data[1];

  if (g_type_is_a (owner_type, pspec->owner_type))
    {
      if (G_TYPE_IS_INTERFACE (pspec->owner_type))
	{
	  slists[0] = g_slist_prepend (slists[0], pspec);
	}
      else
	{
	  guint d = g_type_depth (pspec->owner_type);

	  slists[d - 1] = g_slist_prepend (slists[d - 1], pspec);
	}
    }
}

/* We handle interfaces specially since we don't want to
 * count interface prerequisites like normal inheritance;
 * the property comes from the direct inheritance from
 * the prerequisite class, not from the interface that
 * prerequires it.
 * 
 * also 'depth' isn't a meaningful concept for interface
 * prerequites.
 */
static void
pool_depth_list_for_interface (gpointer key,
			       gpointer value,
			       gpointer user_data)
{
  GParamSpec *pspec = value;
  gpointer *data = user_data;
  GSList **slists = data[0];
  GType owner_type = (GType) data[1];

  if (pspec->owner_type == owner_type)
    slists[0] = g_slist_prepend (slists[0], pspec);
}

/**
 * g_param_spec_pool_list:
 * @pool: a #GParamSpecPool
 * @owner_type: the owner to look for
 * @n_pspecs_p: (out): return location for the length of the returned array
 *
 * Gets an array of all #GParamSpecs owned by @owner_type in
 * the pool.
 *
 * Returns: (array length=n_pspecs_p) (transfer container): a newly
 *          allocated array containing pointers to all #GParamSpecs
 *          owned by @owner_type in the pool
 */
GParamSpec**
g_param_spec_pool_list (GParamSpecPool *pool,
			GType           owner_type,
			guint          *n_pspecs_p)
{
  GParamSpec **pspecs, **p;
  GSList **slists, *node;
  gpointer data[2];
  guint d, i;

  g_return_val_if_fail (pool != NULL, NULL);
  g_return_val_if_fail (owner_type > 0, NULL);
  g_return_val_if_fail (n_pspecs_p != NULL, NULL);
  
  g_mutex_lock (&pool->mutex);
  *n_pspecs_p = 0;
  d = g_type_depth (owner_type);
  slists = g_new0 (GSList*, d);
  data[0] = slists;
  data[1] = (gpointer) owner_type;

  g_hash_table_foreach (pool->hash_table,
			G_TYPE_IS_INTERFACE (owner_type) ?
			   pool_depth_list_for_interface :
			   pool_depth_list,
			&data);
  
  for (i = 0; i < d; i++)
    slists[i] = pspec_list_remove_overridden_and_redirected (slists[i], pool->hash_table, owner_type, n_pspecs_p);
  pspecs = g_new (GParamSpec*, *n_pspecs_p + 1);
  p = pspecs;
  for (i = 0; i < d; i++)
    {
      slists[i] = g_slist_sort (slists[i], pspec_compare_id);
      for (node = slists[i]; node; node = node->next)
	*p++ = node->data;
      g_slist_free (slists[i]);
    }
  *p++ = NULL;
  g_free (slists);
  g_mutex_unlock (&pool->mutex);

  return pspecs;
}


/* --- auxiliary functions --- */
typedef struct
{
  /* class portion */
  GType           value_type;
  void          (*finalize)             (GParamSpec   *pspec);
  void          (*value_set_default)    (GParamSpec   *pspec,
					 GValue       *value);
  gboolean      (*value_validate)       (GParamSpec   *pspec,
					 GValue       *value);
  gint          (*values_cmp)           (GParamSpec   *pspec,
					 const GValue *value1,
					 const GValue *value2);
} ParamSpecClassInfo;

static void
param_spec_generic_class_init (gpointer g_class,
			       gpointer class_data)
{
  GParamSpecClass *class = g_class;
  ParamSpecClassInfo *info = class_data;

  class->value_type = info->value_type;
  if (info->finalize)
    class->finalize = info->finalize;			/* optional */
  class->value_set_default = info->value_set_default;
  if (info->value_validate)
    class->value_validate = info->value_validate;	/* optional */
  class->values_cmp = info->values_cmp;
  g_free (class_data);
}

static void
default_value_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  /* value is already zero initialized */
}

static gint
default_values_cmp (GParamSpec   *pspec,
		    const GValue *value1,
		    const GValue *value2)
{
  return memcmp (&value1->data, &value2->data, sizeof (value1->data));
}

/**
 * g_param_type_register_static:
 * @name: 0-terminated string used as the name of the new #GParamSpec type.
 * @pspec_info: The #GParamSpecTypeInfo for this #GParamSpec type.
 *
 * Registers @name as the name of a new static type derived from
 * #G_TYPE_PARAM. The type system uses the information contained in
 * the #GParamSpecTypeInfo structure pointed to by @info to manage the
 * #GParamSpec type and its instances.
 *
 * Returns: The new type identifier.
 */
GType
g_param_type_register_static (const gchar              *name,
			      const GParamSpecTypeInfo *pspec_info)
{
  GTypeInfo info = {
    sizeof (GParamSpecClass),      /* class_size */
    NULL,                          /* base_init */
    NULL,                          /* base_destroy */
    param_spec_generic_class_init, /* class_init */
    NULL,                          /* class_destroy */
    NULL,                          /* class_data */
    0,                             /* instance_size */
    16,                            /* n_preallocs */
    NULL,                          /* instance_init */
  };
  ParamSpecClassInfo *cinfo;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (pspec_info != NULL, 0);
  g_return_val_if_fail (g_type_from_name (name) == 0, 0);
  g_return_val_if_fail (pspec_info->instance_size >= sizeof (GParamSpec), 0);
  g_return_val_if_fail (g_type_name (pspec_info->value_type) != NULL, 0);
  /* default: g_return_val_if_fail (pspec_info->value_set_default != NULL, 0); */
  /* optional: g_return_val_if_fail (pspec_info->value_validate != NULL, 0); */
  /* default: g_return_val_if_fail (pspec_info->values_cmp != NULL, 0); */

  info.instance_size = pspec_info->instance_size;
  info.n_preallocs = pspec_info->n_preallocs;
  info.instance_init = (GInstanceInitFunc) pspec_info->instance_init;
  cinfo = g_new (ParamSpecClassInfo, 1);
  cinfo->value_type = pspec_info->value_type;
  cinfo->finalize = pspec_info->finalize;
  cinfo->value_set_default = pspec_info->value_set_default ? pspec_info->value_set_default : default_value_set_default;
  cinfo->value_validate = pspec_info->value_validate;
  cinfo->values_cmp = pspec_info->values_cmp ? pspec_info->values_cmp : default_values_cmp;
  info.class_data = cinfo;

  return g_type_register_static (G_TYPE_PARAM, name, &info, 0);
}

/**
 * g_value_set_param:
 * @value: a valid #GValue of type %G_TYPE_PARAM
 * @param: (nullable): the #GParamSpec to be set
 *
 * Set the contents of a %G_TYPE_PARAM #GValue to @param.
 */
void
g_value_set_param (GValue     *value,
		   GParamSpec *param)
{
  g_return_if_fail (G_VALUE_HOLDS_PARAM (value));
  if (param)
    g_return_if_fail (G_IS_PARAM_SPEC (param));

  if (value->data[0].v_pointer)
    g_param_spec_unref (value->data[0].v_pointer);
  value->data[0].v_pointer = param;
  if (value->data[0].v_pointer)
    g_param_spec_ref (value->data[0].v_pointer);
}

/**
 * g_value_set_param_take_ownership: (skip)
 * @value: a valid #GValue of type %G_TYPE_PARAM
 * @param: (nullable): the #GParamSpec to be set
 *
 * This is an internal function introduced mainly for C marshallers.
 *
 * Deprecated: 2.4: Use g_value_take_param() instead.
 */
void
g_value_set_param_take_ownership (GValue     *value,
				  GParamSpec *param)
{
  g_value_take_param (value, param);
}

/**
 * g_value_take_param: (skip)
 * @value: a valid #GValue of type %G_TYPE_PARAM
 * @param: (nullable): the #GParamSpec to be set
 *
 * Sets the contents of a %G_TYPE_PARAM #GValue to @param and takes
 * over the ownership of the callers reference to @param; the caller
 * doesn't have to unref it any more.
 *
 * Since: 2.4
 */
void
g_value_take_param (GValue     *value,
		    GParamSpec *param)
{
  g_return_if_fail (G_VALUE_HOLDS_PARAM (value));
  if (param)
    g_return_if_fail (G_IS_PARAM_SPEC (param));

  if (value->data[0].v_pointer)
    g_param_spec_unref (value->data[0].v_pointer);
  value->data[0].v_pointer = param; /* we take over the reference count */
}

/**
 * g_value_get_param:
 * @value: a valid #GValue whose type is derived from %G_TYPE_PARAM
 *
 * Get the contents of a %G_TYPE_PARAM #GValue.
 *
 * Returns: (transfer none): #GParamSpec content of @value
 */
GParamSpec*
g_value_get_param (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_PARAM (value), NULL);

  return value->data[0].v_pointer;
}

/**
 * g_value_dup_param: (skip)
 * @value: a valid #GValue whose type is derived from %G_TYPE_PARAM
 *
 * Get the contents of a %G_TYPE_PARAM #GValue, increasing its
 * reference count.
 *
 * Returns: #GParamSpec content of @value, should be unreferenced when
 *          no longer needed.
 */
GParamSpec*
g_value_dup_param (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_PARAM (value), NULL);

  return value->data[0].v_pointer ? g_param_spec_ref (value->data[0].v_pointer) : NULL;
}

/**
 * g_param_spec_get_default_value:
 * @pspec: a #GParamSpec
 *
 * Gets the default value of @pspec as a pointer to a #GValue.
 *
 * The #GValue will remain valid for the life of @pspec.
 *
 * Returns: a pointer to a #GValue which must not be modified
 *
 * Since: 2.38
 **/
const GValue *
g_param_spec_get_default_value (GParamSpec *pspec)
{
  GParamSpecPrivate *priv = g_param_spec_get_private (pspec);

  /* We use the type field of the GValue as the key for the once because
   * it will be zero before it is initialised and non-zero after.  We
   * have to take care that we don't write a non-zero value to the type
   * field before we are completely done, however, because then another
   * thread could come along and find the value partially-initialised.
   *
   * In order to accomplish this we store the default value in a
   * stack-allocated GValue.  We then set the type field in that value
   * to zero and copy the contents into place.  We then end by storing
   * the type as the last step in order to ensure that we're completely
   * done before a g_once_init_enter() could take the fast path in
   * another thread.
   */
  if (g_once_init_enter (&priv->default_value.g_type))
    {
      GValue default_value = G_VALUE_INIT;

      g_value_init (&default_value, pspec->value_type);
      g_param_value_set_default (pspec, &default_value);

      /* store all but the type */
      default_value.g_type = 0;
      priv->default_value = default_value;

      g_once_init_leave (&priv->default_value.g_type, pspec->value_type);
    }

  return &priv->default_value;
}

/**
 * g_param_spec_get_name_quark:
 * @pspec: a #GParamSpec
 *
 * Gets the GQuark for the name.
 *
 * Returns: the GQuark for @pspec->name.
 *
 * Since: 2.46
 */
GQuark
g_param_spec_get_name_quark (GParamSpec *pspec)
{
  GParamSpecPrivate *priv = g_param_spec_get_private (pspec);

  /* Return the quark that we've stashed away at creation time.
   * This lets us avoid a lock and a hash table lookup when
   * dispatching property change notification.
   */

  return priv->name_quark;
}
