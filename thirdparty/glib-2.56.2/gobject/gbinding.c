/* gbinding.c: Binding for object properties
 *
 * Copyright (C) 2010  Intel Corp.
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:gbinding
 * @Title: GBinding
 * @Short_Description: Bind two object properties
 *
 * #GBinding is the representation of a binding between a property on a
 * #GObject instance (or source) and another property on another #GObject
 * instance (or target). Whenever the source property changes, the same
 * value is applied to the target property; for instance, the following
 * binding:
 *
 * |[<!-- language="C" --> 
 *   g_object_bind_property (object1, "property-a",
 *                           object2, "property-b",
 *                           G_BINDING_DEFAULT);
 * ]|
 *
 * will cause the property named "property-b" of @object2 to be updated
 * every time g_object_set() or the specific accessor changes the value of
 * the property "property-a" of @object1.
 *
 * It is possible to create a bidirectional binding between two properties
 * of two #GObject instances, so that if either property changes, the
 * other is updated as well, for instance:
 *
 * |[<!-- language="C" --> 
 *   g_object_bind_property (object1, "property-a",
 *                           object2, "property-b",
 *                           G_BINDING_BIDIRECTIONAL);
 * ]|
 *
 * will keep the two properties in sync.
 *
 * It is also possible to set a custom transformation function (in both
 * directions, in case of a bidirectional binding) to apply a custom
 * transformation from the source value to the target value before
 * applying it; for instance, the following binding:
 *
 * |[<!-- language="C" --> 
 *   g_object_bind_property_full (adjustment1, "value",
 *                                adjustment2, "value",
 *                                G_BINDING_BIDIRECTIONAL,
 *                                celsius_to_fahrenheit,
 *                                fahrenheit_to_celsius,
 *                                NULL, NULL);
 * ]|
 *
 * will keep the "value" property of the two adjustments in sync; the
 * @celsius_to_fahrenheit function will be called whenever the "value"
 * property of @adjustment1 changes and will transform the current value
 * of the property before applying it to the "value" property of @adjustment2.
 *
 * Vice versa, the @fahrenheit_to_celsius function will be called whenever
 * the "value" property of @adjustment2 changes, and will transform the
 * current value of the property before applying it to the "value" property
 * of @adjustment1.
 *
 * Note that #GBinding does not resolve cycles by itself; a cycle like
 *
 * |[
 *   object1:propertyA -> object2:propertyB
 *   object2:propertyB -> object3:propertyC
 *   object3:propertyC -> object1:propertyA
 * ]|
 *
 * might lead to an infinite loop. The loop, in this particular case,
 * can be avoided if the objects emit the #GObject::notify signal only
 * if the value has effectively been changed. A binding is implemented
 * using the #GObject::notify signal, so it is susceptible to all the
 * various ways of blocking a signal emission, like g_signal_stop_emission()
 * or g_signal_handler_block().
 *
 * A binding will be severed, and the resources it allocates freed, whenever
 * either one of the #GObject instances it refers to are finalized, or when
 * the #GBinding instance loses its last reference.
 *
 * Bindings for languages with garbage collection can use
 * g_binding_unbind() to explicitly release a binding between the source
 * and target properties, instead of relying on the last reference on the
 * binding, source, and target instances to drop.
 *
 * #GBinding is available since GObject 2.26
 */

#include "config.h"

#include <string.h>

#include "gbinding.h"
#include "genums.h"
#include "gmarshal.h"
#include "gobject.h"
#include "gsignal.h"
#include "gparamspecs.h"
#include "gvaluetypes.h"

#include "glibintl.h"


GType
g_binding_flags_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { G_BINDING_DEFAULT, "G_BINDING_DEFAULT", "default" },
        { G_BINDING_BIDIRECTIONAL, "G_BINDING_BIDIRECTIONAL", "bidirectional" },
        { G_BINDING_SYNC_CREATE, "G_BINDING_SYNC_CREATE", "sync-create" },
        { G_BINDING_INVERT_BOOLEAN, "G_BINDING_INVERT_BOOLEAN", "invert-boolean" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("GBindingFlags"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

#define G_BINDING_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_BINDING, GBindingClass))
#define G_IS_BINDING_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_BINDING))
#define G_BINDING_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_BINDING, GBindingClass))

typedef struct _GBindingClass           GBindingClass;

struct _GBinding
{
  GObject parent_instance;

  /* no reference is held on the objects, to avoid cycles */
  GObject *source;
  GObject *target;

  /* the property names are interned, so they should not be freed */
  const gchar *source_property;
  const gchar *target_property;

  GParamSpec *source_pspec;
  GParamSpec *target_pspec;

  GBindingTransformFunc transform_s2t;
  GBindingTransformFunc transform_t2s;

  GBindingFlags flags;

  guint source_notify;
  guint target_notify;

  gpointer transform_data;
  GDestroyNotify notify;

  /* a guard, to avoid loops */
  guint is_frozen : 1;
};

struct _GBindingClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_TARGET,
  PROP_SOURCE_PROPERTY,
  PROP_TARGET_PROPERTY,
  PROP_FLAGS
};

static guint gobject_notify_signal_id;

G_DEFINE_TYPE (GBinding, g_binding, G_TYPE_OBJECT)

/* the basic assumption is that if either the source or the target
 * goes away then the binding does not exist any more and it should
 * be reaped as well
 */
static void
weak_unbind (gpointer  user_data,
             GObject  *where_the_object_was)
{
  GBinding *binding = user_data;

  /* if what went away was the source, unset it so that GBinding::finalize
   * does not try to access it; otherwise, disconnect everything and remove
   * the GBinding instance from the object's qdata
   */
  if (binding->source == where_the_object_was)
    binding->source = NULL;
  else
    {
      if (binding->source_notify != 0)
        g_signal_handler_disconnect (binding->source, binding->source_notify);

      g_object_weak_unref (binding->source, weak_unbind, user_data);

      binding->source_notify = 0;
      binding->source = NULL;
    }

  /* as above, but with the target */
  if (binding->target == where_the_object_was)
    binding->target = NULL;
  else
    {
      if (binding->target_notify != 0)
        g_signal_handler_disconnect (binding->target, binding->target_notify);

      g_object_weak_unref (binding->target, weak_unbind, user_data);

      binding->target_notify = 0;
      binding->target = NULL;
    }

  /* this will take care of the binding itself */
  g_object_unref (binding);
}

static gboolean
default_transform (GBinding     *binding,
                   const GValue *value_a,
                   GValue       *value_b,
                   gpointer      user_data G_GNUC_UNUSED)
{
  /* if it's not the same type, try to convert it using the GValue
   * transformation API; otherwise just copy it
   */
  if (!g_type_is_a (G_VALUE_TYPE (value_a), G_VALUE_TYPE (value_b)))
    {
      /* are these two types compatible (can be directly copied)? */
      if (g_value_type_compatible (G_VALUE_TYPE (value_a),
                                   G_VALUE_TYPE (value_b)))
        {
          g_value_copy (value_a, value_b);
          return TRUE;
        }

      if (g_value_type_transformable (G_VALUE_TYPE (value_a),
                                      G_VALUE_TYPE (value_b)))
        {
          if (g_value_transform (value_a, value_b))
            return TRUE;
        }

      g_warning ("%s: Unable to convert a value of type %s to a "
                 "value of type %s",
                 G_STRLOC,
                 g_type_name (G_VALUE_TYPE (value_a)),
                 g_type_name (G_VALUE_TYPE (value_b)));

      return FALSE;
    }

  g_value_copy (value_a, value_b);
  return TRUE;
}

static gboolean
default_invert_boolean_transform (GBinding     *binding,
                                  const GValue *value_a,
                                  GValue       *value_b,
                                  gpointer      user_data G_GNUC_UNUSED)
{
  gboolean value;

  g_assert (G_VALUE_HOLDS_BOOLEAN (value_a));
  g_assert (G_VALUE_HOLDS_BOOLEAN (value_b));

  value = g_value_get_boolean (value_a);
  value = !value;

  g_value_set_boolean (value_b, value);

  return TRUE;
}

static void
on_source_notify (GObject    *gobject,
                  GParamSpec *pspec,
                  GBinding   *binding)
{
  GValue from_value = G_VALUE_INIT;
  GValue to_value = G_VALUE_INIT;
  gboolean res;

  if (binding->is_frozen)
    return;

  g_value_init (&from_value, G_PARAM_SPEC_VALUE_TYPE (binding->source_pspec));
  g_value_init (&to_value, G_PARAM_SPEC_VALUE_TYPE (binding->target_pspec));

  g_object_get_property (binding->source, binding->source_pspec->name, &from_value);

  res = binding->transform_s2t (binding,
                                &from_value,
                                &to_value,
                                binding->transform_data);
  if (res)
    {
      binding->is_frozen = TRUE;

      g_param_value_validate (binding->target_pspec, &to_value);
      g_object_set_property (binding->target, binding->target_pspec->name, &to_value);

      binding->is_frozen = FALSE;
    }

  g_value_unset (&from_value);
  g_value_unset (&to_value);
}

static void
on_target_notify (GObject    *gobject,
                  GParamSpec *pspec,
                  GBinding   *binding)
{
  GValue from_value = G_VALUE_INIT;
  GValue to_value = G_VALUE_INIT;
  gboolean res;

  if (binding->is_frozen)
    return;

  g_value_init (&from_value, G_PARAM_SPEC_VALUE_TYPE (binding->target_pspec));
  g_value_init (&to_value, G_PARAM_SPEC_VALUE_TYPE (binding->source_pspec));

  g_object_get_property (binding->target, binding->target_pspec->name, &from_value);

  res = binding->transform_t2s (binding,
                                &from_value,
                                &to_value,
                                binding->transform_data);
  if (res)
    {
      binding->is_frozen = TRUE;

      g_param_value_validate (binding->source_pspec, &to_value);
      g_object_set_property (binding->source, binding->source_pspec->name, &to_value);

      binding->is_frozen = FALSE;
    }

  g_value_unset (&from_value);
  g_value_unset (&to_value);
}

static inline void
g_binding_unbind_internal (GBinding *binding,
                           gboolean  unref_binding)
{
  gboolean source_is_target = binding->source == binding->target;

  /* dispose of the transformation data */
  if (binding->notify != NULL)
    {
      binding->notify (binding->transform_data);

      binding->transform_data = NULL;
      binding->notify = NULL;
    }

  if (binding->source != NULL)
    {
      if (binding->source_notify != 0)
        g_signal_handler_disconnect (binding->source, binding->source_notify);

      g_object_weak_unref (binding->source, weak_unbind, binding);

      binding->source_notify = 0;
      binding->source = NULL;
    }

  if (binding->target != NULL)
    {
      if (binding->target_notify != 0)
        g_signal_handler_disconnect (binding->target, binding->target_notify);

      if (!source_is_target)
        g_object_weak_unref (binding->target, weak_unbind, binding);

      binding->target_notify = 0;
      binding->target = NULL;
    }

  if (unref_binding)
    g_object_unref (binding);
}

static void
g_binding_finalize (GObject *gobject)
{
  GBinding *binding = G_BINDING (gobject);

  g_binding_unbind_internal (binding, FALSE);

  G_OBJECT_CLASS (g_binding_parent_class)->finalize (gobject);
}

static void
g_binding_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GBinding *binding = G_BINDING (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      binding->source = g_value_get_object (value);
      break;

    case PROP_SOURCE_PROPERTY:
      binding->source_property = g_intern_string (g_value_get_string (value));
      break;

    case PROP_TARGET:
      binding->target = g_value_get_object (value);
      break;

    case PROP_TARGET_PROPERTY:
      binding->target_property = g_intern_string (g_value_get_string (value));
      break;

    case PROP_FLAGS:
      binding->flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
g_binding_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GBinding *binding = G_BINDING (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, binding->source);
      break;

    case PROP_SOURCE_PROPERTY:
      g_value_set_string (value, binding->source_property);
      break;

    case PROP_TARGET:
      g_value_set_object (value, binding->target);
      break;

    case PROP_TARGET_PROPERTY:
      g_value_set_string (value, binding->target_property);
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, binding->flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
g_binding_constructed (GObject *gobject)
{
  GBinding *binding = G_BINDING (gobject);
  GBindingTransformFunc transform_func = default_transform;
  GQuark source_property_detail;
  GClosure *source_notify_closure;

  /* assert that we were constructed correctly */
  g_assert (binding->source != NULL);
  g_assert (binding->target != NULL);
  g_assert (binding->source_property != NULL);
  g_assert (binding->target_property != NULL);

  /* we assume a check was performed prior to construction - since
   * g_object_bind_property_full() does it; we cannot fail construction
   * anyway, so it would be hard for use to properly warn here
   */
  binding->source_pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (binding->source), binding->source_property);
  binding->target_pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (binding->target), binding->target_property);
  g_assert (binding->source_pspec != NULL);
  g_assert (binding->target_pspec != NULL);

  /* switch to the invert boolean transform if needed */
  if (binding->flags & G_BINDING_INVERT_BOOLEAN)
    transform_func = default_invert_boolean_transform;

  /* set the default transformation functions here */
  binding->transform_s2t = transform_func;
  binding->transform_t2s = transform_func;

  binding->transform_data = NULL;
  binding->notify = NULL;

  source_property_detail = g_quark_from_string (binding->source_property);
  source_notify_closure = g_cclosure_new (G_CALLBACK (on_source_notify),
                                          binding, NULL);
  binding->source_notify = g_signal_connect_closure_by_id (binding->source,
                                                           gobject_notify_signal_id,
                                                           source_property_detail,
                                                           source_notify_closure,
                                                           FALSE);

  g_object_weak_ref (binding->source, weak_unbind, binding);

  if (binding->flags & G_BINDING_BIDIRECTIONAL)
    {
      GQuark target_property_detail;
      GClosure *target_notify_closure;

      target_property_detail = g_quark_from_string (binding->target_property);
      target_notify_closure = g_cclosure_new (G_CALLBACK (on_target_notify),
                                              binding, NULL);
      binding->target_notify = g_signal_connect_closure_by_id (binding->target,
                                                               gobject_notify_signal_id,
                                                               target_property_detail,
                                                               target_notify_closure,
                                                               FALSE);
    }

  if (binding->target != binding->source)
    g_object_weak_ref (binding->target, weak_unbind, binding);
}

static void
g_binding_class_init (GBindingClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_notify_signal_id = g_signal_lookup ("notify", G_TYPE_OBJECT);
  g_assert (gobject_notify_signal_id != 0);

  gobject_class->constructed = g_binding_constructed;
  gobject_class->set_property = g_binding_set_property;
  gobject_class->get_property = g_binding_get_property;
  gobject_class->finalize = g_binding_finalize;

  /**
   * GBinding:source:
   *
   * The #GObject that should be used as the source of the binding
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_SOURCE,
                                   g_param_spec_object ("source",
                                                        P_("Source"),
                                                        P_("The source of the binding"),
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GBinding:target:
   *
   * The #GObject that should be used as the target of the binding
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_TARGET,
                                   g_param_spec_object ("target",
                                                        P_("Target"),
                                                        P_("The target of the binding"),
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GBinding:source-property:
   *
   * The name of the property of #GBinding:source that should be used
   * as the source of the binding
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_SOURCE_PROPERTY,
                                   g_param_spec_string ("source-property",
                                                        P_("Source Property"),
                                                        P_("The property on the source to bind"),
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GBinding:target-property:
   *
   * The name of the property of #GBinding:target that should be used
   * as the target of the binding
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_TARGET_PROPERTY,
                                   g_param_spec_string ("target-property",
                                                        P_("Target Property"),
                                                        P_("The property on the target to bind"),
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GBinding:flags:
   *
   * Flags to be used to control the #GBinding
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       P_("Flags"),
                                                       P_("The binding flags"),
                                                       G_TYPE_BINDING_FLAGS,
                                                       G_BINDING_DEFAULT,
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
}

static void
g_binding_init (GBinding *binding)
{
}

/**
 * g_binding_get_flags:
 * @binding: a #GBinding
 *
 * Retrieves the flags passed when constructing the #GBinding.
 *
 * Returns: the #GBindingFlags used by the #GBinding
 *
 * Since: 2.26
 */
GBindingFlags
g_binding_get_flags (GBinding *binding)
{
  g_return_val_if_fail (G_IS_BINDING (binding), G_BINDING_DEFAULT);

  return binding->flags;
}

/**
 * g_binding_get_source:
 * @binding: a #GBinding
 *
 * Retrieves the #GObject instance used as the source of the binding.
 *
 * Returns: (transfer none): the source #GObject
 *
 * Since: 2.26
 */
GObject *
g_binding_get_source (GBinding *binding)
{
  g_return_val_if_fail (G_IS_BINDING (binding), NULL);

  return binding->source;
}

/**
 * g_binding_get_target:
 * @binding: a #GBinding
 *
 * Retrieves the #GObject instance used as the target of the binding.
 *
 * Returns: (transfer none): the target #GObject
 *
 * Since: 2.26
 */
GObject *
g_binding_get_target (GBinding *binding)
{
  g_return_val_if_fail (G_IS_BINDING (binding), NULL);

  return binding->target;
}

/**
 * g_binding_get_source_property:
 * @binding: a #GBinding
 *
 * Retrieves the name of the property of #GBinding:source used as the source
 * of the binding.
 *
 * Returns: the name of the source property
 *
 * Since: 2.26
 */
const gchar *
g_binding_get_source_property (GBinding *binding)
{
  g_return_val_if_fail (G_IS_BINDING (binding), NULL);

  return binding->source_property;
}

/**
 * g_binding_get_target_property:
 * @binding: a #GBinding
 *
 * Retrieves the name of the property of #GBinding:target used as the target
 * of the binding.
 *
 * Returns: the name of the target property
 *
 * Since: 2.26
 */
const gchar *
g_binding_get_target_property (GBinding *binding)
{
  g_return_val_if_fail (G_IS_BINDING (binding), NULL);

  return binding->target_property;
}

/**
 * g_binding_unbind:
 * @binding: a #GBinding
 *
 * Explicitly releases the binding between the source and the target
 * property expressed by @binding.
 *
 * This function will release the reference that is being held on
 * the @binding instance; if you want to hold on to the #GBinding instance
 * after calling g_binding_unbind(), you will need to hold a reference
 * to it.
 *
 * Since: 2.38
 */
void
g_binding_unbind (GBinding *binding)
{
  g_return_if_fail (G_IS_BINDING (binding));

  g_binding_unbind_internal (binding, TRUE);
}

/**
 * g_object_bind_property_full:
 * @source: (type GObject.Object): the source #GObject
 * @source_property: the property on @source to bind
 * @target: (type GObject.Object): the target #GObject
 * @target_property: the property on @target to bind
 * @flags: flags to pass to #GBinding
 * @transform_to: (scope notified) (nullable): the transformation function
 *     from the @source to the @target, or %NULL to use the default
 * @transform_from: (scope notified) (nullable): the transformation function
 *     from the @target to the @source, or %NULL to use the default
 * @user_data: custom data to be passed to the transformation functions,
 *     or %NULL
 * @notify: (nullable): a function to call when disposing the binding, to free
 *     resources used by the transformation functions, or %NULL if not required
 *
 * Complete version of g_object_bind_property().
 *
 * Creates a binding between @source_property on @source and @target_property
 * on @target, allowing you to set the transformation functions to be used by
 * the binding.
 *
 * If @flags contains %G_BINDING_BIDIRECTIONAL then the binding will be mutual:
 * if @target_property on @target changes then the @source_property on @source
 * will be updated as well. The @transform_from function is only used in case
 * of bidirectional bindings, otherwise it will be ignored
 *
 * The binding will automatically be removed when either the @source or the
 * @target instances are finalized. To remove the binding without affecting the
 * @source and the @target you can just call g_object_unref() on the returned
 * #GBinding instance.
 *
 * A #GObject can have multiple bindings.
 *
 * The same @user_data parameter will be used for both @transform_to
 * and @transform_from transformation functions; the @notify function will
 * be called once, when the binding is removed. If you need different data
 * for each transformation function, please use
 * g_object_bind_property_with_closures() instead.
 *
 * Returns: (transfer none): the #GBinding instance representing the
 *     binding between the two #GObject instances. The binding is released
 *     whenever the #GBinding reference count reaches zero.
 *
 * Since: 2.26
 */
GBinding *
g_object_bind_property_full (gpointer               source,
                             const gchar           *source_property,
                             gpointer               target,
                             const gchar           *target_property,
                             GBindingFlags          flags,
                             GBindingTransformFunc  transform_to,
                             GBindingTransformFunc  transform_from,
                             gpointer               user_data,
                             GDestroyNotify         notify)
{
  GParamSpec *pspec;
  GBinding *binding;

  g_return_val_if_fail (G_IS_OBJECT (source), NULL);
  g_return_val_if_fail (source_property != NULL, NULL);
  g_return_val_if_fail (G_IS_OBJECT (target), NULL);
  g_return_val_if_fail (target_property != NULL, NULL);

  if (source == target && g_strcmp0 (source_property, target_property) == 0)
    {
      g_warning ("Unable to bind the same property on the same instance");
      return NULL;
    }

  /* remove the G_BINDING_INVERT_BOOLEAN flag in case we have
   * custom transformation functions
   */
  if ((flags & G_BINDING_INVERT_BOOLEAN) &&
      (transform_to != NULL || transform_from != NULL))
    {
      flags &= ~G_BINDING_INVERT_BOOLEAN;
    }

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (source), source_property);
  if (pspec == NULL)
    {
      g_warning ("%s: The source object of type %s has no property called '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (source),
                 source_property);
      return NULL;
    }

  if (!(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("%s: The source object of type %s has no readable property called '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (source),
                 source_property);
      return NULL;
    }

  if ((flags & G_BINDING_BIDIRECTIONAL) &&
      ((pspec->flags & G_PARAM_CONSTRUCT_ONLY) || !(pspec->flags & G_PARAM_WRITABLE)))
    {
      g_warning ("%s: The source object of type %s has no writable property called '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (source),
                 source_property);
      return NULL;
    }

  if ((flags & G_BINDING_INVERT_BOOLEAN) &&
      !(G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN))
    {
      g_warning ("%s: The G_BINDING_INVERT_BOOLEAN flag can only be used "
                 "when binding boolean properties; the source property '%s' "
                 "is of type '%s'",
                 G_STRLOC,
                 source_property,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      return NULL;
    }

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (target), target_property);
  if (pspec == NULL)
    {
      g_warning ("%s: The target object of type %s has no property called '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (target),
                 target_property);
      return NULL;
    }

  if ((pspec->flags & G_PARAM_CONSTRUCT_ONLY) || !(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("%s: The target object of type %s has no writable property called '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (target),
                 target_property);
      return NULL;
    }

  if ((flags & G_BINDING_BIDIRECTIONAL) &&
      !(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("%s: The target object of type %s has no readable property called '%s'",
                 G_STRLOC,
                 G_OBJECT_TYPE_NAME (target),
                 target_property);
      return NULL;
    }

  if ((flags & G_BINDING_INVERT_BOOLEAN) &&
      !(G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN))
    {
      g_warning ("%s: The G_BINDING_INVERT_BOOLEAN flag can only be used "
                 "when binding boolean properties; the target property '%s' "
                 "is of type '%s'",
                 G_STRLOC,
                 target_property,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      return NULL;
    }

  binding = g_object_new (G_TYPE_BINDING,
                          "source", source,
                          "source-property", source_property,
                          "target", target,
                          "target-property", target_property,
                          "flags", flags,
                          NULL);

  if (transform_to != NULL)
    binding->transform_s2t = transform_to;

  if (transform_from != NULL)
    binding->transform_t2s = transform_from;

  binding->transform_data = user_data;
  binding->notify = notify;

  /* synchronize the target with the source by faking an emission of
   * the ::notify signal for the source property; this will also take
   * care of the bidirectional binding case because the eventual change
   * will emit a notification on the target
   */
  if (flags & G_BINDING_SYNC_CREATE)
    on_source_notify (binding->source, binding->source_pspec, binding);

  return binding;
}

/**
 * g_object_bind_property:
 * @source: (type GObject.Object): the source #GObject
 * @source_property: the property on @source to bind
 * @target: (type GObject.Object): the target #GObject
 * @target_property: the property on @target to bind
 * @flags: flags to pass to #GBinding
 *
 * Creates a binding between @source_property on @source and @target_property
 * on @target. Whenever the @source_property is changed the @target_property is
 * updated using the same value. For instance:
 *
 * |[
 *   g_object_bind_property (action, "active", widget, "sensitive", 0);
 * ]|
 *
 * Will result in the "sensitive" property of the widget #GObject instance to be
 * updated with the same value of the "active" property of the action #GObject
 * instance.
 *
 * If @flags contains %G_BINDING_BIDIRECTIONAL then the binding will be mutual:
 * if @target_property on @target changes then the @source_property on @source
 * will be updated as well.
 *
 * The binding will automatically be removed when either the @source or the
 * @target instances are finalized. To remove the binding without affecting the
 * @source and the @target you can just call g_object_unref() on the returned
 * #GBinding instance.
 *
 * A #GObject can have multiple bindings.
 *
 * Returns: (transfer none): the #GBinding instance representing the
 *     binding between the two #GObject instances. The binding is released
 *     whenever the #GBinding reference count reaches zero.
 *
 * Since: 2.26
 */
GBinding *
g_object_bind_property (gpointer       source,
                        const gchar   *source_property,
                        gpointer       target,
                        const gchar   *target_property,
                        GBindingFlags  flags)
{
  /* type checking is done in g_object_bind_property_full() */

  return g_object_bind_property_full (source, source_property,
                                      target, target_property,
                                      flags,
                                      NULL,
                                      NULL,
                                      NULL, NULL);
}

typedef struct _TransformData
{
  GClosure *transform_to_closure;
  GClosure *transform_from_closure;
} TransformData;

static gboolean
bind_with_closures_transform_to (GBinding     *binding,
                                 const GValue *source,
                                 GValue       *target,
                                 gpointer      data)
{
  TransformData *t_data = data;
  GValue params[3] = { G_VALUE_INIT, G_VALUE_INIT, G_VALUE_INIT };
  GValue retval = G_VALUE_INIT;
  gboolean res;

  g_value_init (&params[0], G_TYPE_BINDING);
  g_value_set_object (&params[0], binding);

  g_value_init (&params[1], G_TYPE_VALUE);
  g_value_set_boxed (&params[1], source);

  g_value_init (&params[2], G_TYPE_VALUE);
  g_value_set_boxed (&params[2], target);

  g_value_init (&retval, G_TYPE_BOOLEAN);
  g_value_set_boolean (&retval, FALSE);

  g_closure_invoke (t_data->transform_to_closure, &retval, 3, params, NULL);

  res = g_value_get_boolean (&retval);
  if (res)
    {
      const GValue *out_value = g_value_get_boxed (&params[2]);

      g_assert (out_value != NULL);

      g_value_copy (out_value, target);
    }

  g_value_unset (&params[0]);
  g_value_unset (&params[1]);
  g_value_unset (&params[2]);
  g_value_unset (&retval);

  return res;
}

static gboolean
bind_with_closures_transform_from (GBinding     *binding,
                                   const GValue *source,
                                   GValue       *target,
                                   gpointer      data)
{
  TransformData *t_data = data;
  GValue params[3] = { G_VALUE_INIT, G_VALUE_INIT, G_VALUE_INIT };
  GValue retval = G_VALUE_INIT;
  gboolean res;

  g_value_init (&params[0], G_TYPE_BINDING);
  g_value_set_object (&params[0], binding);

  g_value_init (&params[1], G_TYPE_VALUE);
  g_value_set_boxed (&params[1], source);

  g_value_init (&params[2], G_TYPE_VALUE);
  g_value_set_boxed (&params[2], target);

  g_value_init (&retval, G_TYPE_BOOLEAN);
  g_value_set_boolean (&retval, FALSE);

  g_closure_invoke (t_data->transform_from_closure, &retval, 3, params, NULL);

  res = g_value_get_boolean (&retval);
  if (res)
    {
      const GValue *out_value = g_value_get_boxed (&params[2]);

      g_assert (out_value != NULL);

      g_value_copy (out_value, target);
    }

  g_value_unset (&params[0]);
  g_value_unset (&params[1]);
  g_value_unset (&params[2]);
  g_value_unset (&retval);

  return res;
}

static void
bind_with_closures_free_func (gpointer data)
{
  TransformData *t_data = data;

  if (t_data->transform_to_closure != NULL)
    g_closure_unref (t_data->transform_to_closure);

  if (t_data->transform_from_closure != NULL)
    g_closure_unref (t_data->transform_from_closure);

  g_slice_free (TransformData, t_data);
}

/**
 * g_object_bind_property_with_closures: (rename-to g_object_bind_property_full)
 * @source: (type GObject.Object): the source #GObject
 * @source_property: the property on @source to bind
 * @target: (type GObject.Object): the target #GObject
 * @target_property: the property on @target to bind
 * @flags: flags to pass to #GBinding
 * @transform_to: a #GClosure wrapping the transformation function
 *     from the @source to the @target, or %NULL to use the default
 * @transform_from: a #GClosure wrapping the transformation function
 *     from the @target to the @source, or %NULL to use the default
 *
 * Creates a binding between @source_property on @source and @target_property
 * on @target, allowing you to set the transformation functions to be used by
 * the binding.
 *
 * This function is the language bindings friendly version of
 * g_object_bind_property_full(), using #GClosures instead of
 * function pointers.
 *
 * Returns: (transfer none): the #GBinding instance representing the
 *     binding between the two #GObject instances. The binding is released
 *     whenever the #GBinding reference count reaches zero.
 *
 * Since: 2.26
 */
GBinding *
g_object_bind_property_with_closures (gpointer       source,
                                      const gchar   *source_property,
                                      gpointer       target,
                                      const gchar   *target_property,
                                      GBindingFlags  flags,
                                      GClosure      *transform_to,
                                      GClosure      *transform_from)
{
  TransformData *data;

  data = g_slice_new0 (TransformData);

  if (transform_to != NULL)
    {
      if (G_CLOSURE_NEEDS_MARSHAL (transform_to))
        g_closure_set_marshal (transform_to, g_cclosure_marshal_BOOLEAN__BOXED_BOXED);

      data->transform_to_closure = g_closure_ref (transform_to);
      g_closure_sink (data->transform_to_closure);
    }

  if (transform_from != NULL)
    {
      if (G_CLOSURE_NEEDS_MARSHAL (transform_from))
        g_closure_set_marshal (transform_from, g_cclosure_marshal_BOOLEAN__BOXED_BOXED);

      data->transform_from_closure = g_closure_ref (transform_from);
      g_closure_sink (data->transform_from_closure);
    }

  return g_object_bind_property_full (source, source_property,
                                      target, target_property,
                                      flags,
                                      transform_to != NULL ? bind_with_closures_transform_to : NULL,
                                      transform_from != NULL ? bind_with_closures_transform_from : NULL,
                                      data,
                                      bind_with_closures_free_func);
}
