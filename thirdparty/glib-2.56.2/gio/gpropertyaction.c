/*
 * Copyright © 2013 Canonical Limited
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gpropertyaction.h"

#include "gsettings-mapping.h"
#include "gaction.h"
#include "glibintl.h"

/**
 * SECTION:gpropertyaction
 * @title: GPropertyAction
 * @short_description: A GAction reflecting a GObject property
 * @include: gio/gio.h
 *
 * A #GPropertyAction is a way to get a #GAction with a state value
 * reflecting and controlling the value of a #GObject property.
 *
 * The state of the action will correspond to the value of the property.
 * Changing it will change the property (assuming the requested value
 * matches the requirements as specified in the #GParamSpec).
 *
 * Only the most common types are presently supported.  Booleans are
 * mapped to booleans, strings to strings, signed/unsigned integers to
 * int32/uint32 and floats and doubles to doubles.
 *
 * If the property is an enum then the state will be string-typed and
 * conversion will automatically be performed between the enum value and
 * "nick" string as per the #GEnumValue table.
 *
 * Flags types are not currently supported.
 *
 * Properties of object types, boxed types and pointer types are not
 * supported and probably never will be.
 *
 * Properties of #GVariant types are not currently supported.
 *
 * If the property is boolean-valued then the action will have a NULL
 * parameter type, and activating the action (with no parameter) will
 * toggle the value of the property.
 *
 * In all other cases, the parameter type will correspond to the type of
 * the property.
 *
 * The general idea here is to reduce the number of locations where a
 * particular piece of state is kept (and therefore has to be synchronised
 * between). #GPropertyAction does not have a separate state that is kept
 * in sync with the property value -- its state is the property value.
 *
 * For example, it might be useful to create a #GAction corresponding to
 * the "visible-child-name" property of a #GtkStack so that the current
 * page can be switched from a menu.  The active radio indication in the
 * menu is then directly determined from the active page of the
 * #GtkStack.
 *
 * An anti-example would be binding the "active-id" property on a
 * #GtkComboBox.  This is because the state of the combobox itself is
 * probably uninteresting and is actually being used to control
 * something else.
 *
 * Another anti-example would be to bind to the "visible-child-name"
 * property of a #GtkStack if this value is actually stored in
 * #GSettings.  In that case, the real source of the value is
 * #GSettings.  If you want a #GAction to control a setting stored in
 * #GSettings, see g_settings_create_action() instead, and possibly
 * combine its use with g_settings_bind().
 *
 * Since: 2.38
 **/
struct _GPropertyAction
{
  GObject     parent_instance;

  gchar              *name;
  gpointer            object;
  GParamSpec         *pspec;
  const GVariantType *state_type;
  gboolean            invert_boolean;
};

/**
 * GPropertyAction:
 *
 * This type is opaque.
 *
 * Since: 2.38
 **/

typedef GObjectClass GPropertyActionClass;

static void g_property_action_iface_init (GActionInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GPropertyAction, g_property_action, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION, g_property_action_iface_init))

enum
{
  PROP_NONE,
  PROP_NAME,
  PROP_PARAMETER_TYPE,
  PROP_ENABLED,
  PROP_STATE_TYPE,
  PROP_STATE,
  PROP_OBJECT,
  PROP_PROPERTY_NAME,
  PROP_INVERT_BOOLEAN
};

static gboolean
g_property_action_get_invert_boolean (GAction *action)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);

  return paction->invert_boolean;
}

static const gchar *
g_property_action_get_name (GAction *action)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);

  return paction->name;
}

static const GVariantType *
g_property_action_get_parameter_type (GAction *action)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);

  return paction->pspec->value_type == G_TYPE_BOOLEAN ? NULL : paction->state_type;
}

static const GVariantType *
g_property_action_get_state_type (GAction *action)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);

  return paction->state_type;
}

static GVariant *
g_property_action_get_state_hint (GAction *action)
{
  return NULL;
}

static gboolean
g_property_action_get_enabled (GAction *action)
{
  return TRUE;
}

static void
g_property_action_set_state (GPropertyAction *paction,
                             GVariant        *variant)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, paction->pspec->value_type);
  g_settings_get_mapping (&value, variant, NULL);

  if (paction->pspec->value_type == G_TYPE_BOOLEAN && paction->invert_boolean)
    g_value_set_boolean (&value, !g_value_get_boolean (&value));

  g_object_set_property (paction->object, paction->pspec->name, &value);
  g_value_unset (&value);
}

static void
g_property_action_change_state (GAction  *action,
                                GVariant *value)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);

  g_return_if_fail (g_variant_is_of_type (value, paction->state_type));

  g_property_action_set_state (paction, value);
}

static GVariant *
g_property_action_get_state (GAction *action)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);
  GValue value = G_VALUE_INIT;
  GVariant *result;

  g_value_init (&value, paction->pspec->value_type);
  g_object_get_property (paction->object, paction->pspec->name, &value);

  if (paction->pspec->value_type == G_TYPE_BOOLEAN && paction->invert_boolean)
    g_value_set_boolean (&value, !g_value_get_boolean (&value));

  result = g_settings_set_mapping (&value, paction->state_type, NULL);
  g_value_unset (&value);

  return g_variant_ref_sink (result);
}

static void
g_property_action_activate (GAction  *action,
                            GVariant *parameter)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (action);

  if (paction->pspec->value_type == G_TYPE_BOOLEAN)
    {
      gboolean value;

      g_return_if_fail (paction->pspec->value_type == G_TYPE_BOOLEAN && parameter == NULL);

      g_object_get (paction->object, paction->pspec->name, &value, NULL);
      value = !value;
      g_object_set (paction->object, paction->pspec->name, value, NULL);
    }
  else
    {
      g_return_if_fail (parameter != NULL && g_variant_is_of_type (parameter, paction->state_type));

      g_property_action_set_state (paction, parameter);
    }
}

static const GVariantType *
g_property_action_determine_type (GParamSpec *pspec)
{
  if (G_TYPE_IS_ENUM (pspec->value_type))
    return G_VARIANT_TYPE_STRING;

  switch (pspec->value_type)
    {
    case G_TYPE_BOOLEAN:
      return G_VARIANT_TYPE_BOOLEAN;

    case G_TYPE_INT:
      return G_VARIANT_TYPE_INT32;

    case G_TYPE_UINT:
      return G_VARIANT_TYPE_UINT32;

    case G_TYPE_DOUBLE:
    case G_TYPE_FLOAT:
      return G_VARIANT_TYPE_DOUBLE;

    case G_TYPE_STRING:
      return G_VARIANT_TYPE_STRING;

    default:
      g_critical ("Unable to use GPropertyAction with property '%s::%s' of type '%s'",
                  g_type_name (pspec->owner_type), pspec->name, g_type_name (pspec->value_type));
      return NULL;
    }
}

static void
g_property_action_notify (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  GPropertyAction *paction = user_data;

  g_assert (object == paction->object);
  g_assert (pspec == paction->pspec);

  g_object_notify (G_OBJECT (paction), "state");
}

static void
g_property_action_set_property_name (GPropertyAction *paction,
                                     const gchar     *property_name)
{
  GParamSpec *pspec;
  gchar *detailed;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (paction->object), property_name);

  if (pspec == NULL)
    {
      g_critical ("Attempted to use non-existent property '%s::%s' for GPropertyAction",
                  G_OBJECT_TYPE_NAME (paction->object), property_name);
      return;
    }

  if (~pspec->flags & G_PARAM_READABLE || ~pspec->flags & G_PARAM_WRITABLE || pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_critical ("Property '%s::%s' used with GPropertyAction must be readable, writable, and not construct-only",
                  G_OBJECT_TYPE_NAME (paction->object), property_name);
      return;
    }

  paction->pspec = pspec;

  detailed = g_strconcat ("notify::", paction->pspec->name, NULL);
  paction->state_type = g_property_action_determine_type (paction->pspec);
  g_signal_connect (paction->object, detailed, G_CALLBACK (g_property_action_notify), paction);
  g_free (detailed);
}

static void
g_property_action_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      paction->name = g_value_dup_string (value);
      break;

    case PROP_OBJECT:
      paction->object = g_value_dup_object (value);
      break;

    case PROP_PROPERTY_NAME:
      g_property_action_set_property_name (paction, g_value_get_string (value));
      break;

    case PROP_INVERT_BOOLEAN:
      paction->invert_boolean = g_value_get_boolean (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_property_action_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GAction *action = G_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, g_property_action_get_name (action));
      break;

    case PROP_PARAMETER_TYPE:
      g_value_set_boxed (value, g_property_action_get_parameter_type (action));
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, g_property_action_get_enabled (action));
      break;

    case PROP_STATE_TYPE:
      g_value_set_boxed (value, g_property_action_get_state_type (action));
      break;

    case PROP_STATE:
      g_value_take_variant (value, g_property_action_get_state (action));
      break;

    case PROP_INVERT_BOOLEAN:
      g_value_set_boolean (value, g_property_action_get_invert_boolean (action));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_property_action_finalize (GObject *object)
{
  GPropertyAction *paction = G_PROPERTY_ACTION (object);

  g_signal_handlers_disconnect_by_func (paction->object, g_property_action_notify, paction);
  g_object_unref (paction->object);
  g_free (paction->name);

  G_OBJECT_CLASS (g_property_action_parent_class)
    ->finalize (object);
}

void
g_property_action_init (GPropertyAction *property)
{
}

void
g_property_action_iface_init (GActionInterface *iface)
{
  iface->get_name = g_property_action_get_name;
  iface->get_parameter_type = g_property_action_get_parameter_type;
  iface->get_state_type = g_property_action_get_state_type;
  iface->get_state_hint = g_property_action_get_state_hint;
  iface->get_enabled = g_property_action_get_enabled;
  iface->get_state = g_property_action_get_state;
  iface->change_state = g_property_action_change_state;
  iface->activate = g_property_action_activate;
}

void
g_property_action_class_init (GPropertyActionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = g_property_action_set_property;
  object_class->get_property = g_property_action_get_property;
  object_class->finalize = g_property_action_finalize;

  /**
   * GPropertyAction:name:
   *
   * The name of the action.  This is mostly meaningful for identifying
   * the action once it has been added to a #GActionMap.
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("Action Name"),
                                                        P_("The name used to invoke the action"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:parameter-type:
   *
   * The type of the parameter that must be given when activating the
   * action.
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_PARAMETER_TYPE,
                                   g_param_spec_boxed ("parameter-type",
                                                       P_("Parameter Type"),
                                                       P_("The type of GVariant passed to activate()"),
                                                       G_TYPE_VARIANT_TYPE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:enabled:
   *
   * If @action is currently enabled.
   *
   * If the action is disabled then calls to g_action_activate() and
   * g_action_change_state() have no effect.
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_ENABLED,
                                   g_param_spec_boolean ("enabled",
                                                         P_("Enabled"),
                                                         P_("If the action can be activated"),
                                                         TRUE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:state-type:
   *
   * The #GVariantType of the state that the action has, or %NULL if the
   * action is stateless.
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_STATE_TYPE,
                                   g_param_spec_boxed ("state-type",
                                                       P_("State Type"),
                                                       P_("The type of the state kept by the action"),
                                                       G_TYPE_VARIANT_TYPE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:state:
   *
   * The state of the action, or %NULL if the action is stateless.
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_STATE,
                                   g_param_spec_variant ("state",
                                                         P_("State"),
                                                         P_("The state the action is in"),
                                                         G_VARIANT_TYPE_ANY,
                                                         NULL,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:object:
   *
   * The object to wrap a property on.
   *
   * The object must be a non-%NULL #GObject with properties.
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_OBJECT,
                                   g_param_spec_object ("object",
                                                        P_("Object"),
                                                        P_("The object with the property to wrap"),
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:property-name:
   *
   * The name of the property to wrap on the object.
   *
   * The property must exist on the passed-in object and it must be
   * readable and writable (and not construct-only).
   *
   * Since: 2.38
   **/
  g_object_class_install_property (object_class, PROP_PROPERTY_NAME,
                                   g_param_spec_string ("property-name",
                                                        P_("Property name"),
                                                        P_("The name of the property to wrap"),
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GPropertyAction:invert-boolean:
   *
   * If %TRUE, the state of the action will be the negation of the
   * property value, provided the property is boolean.
   *
   * Since: 2.46
   */
  g_object_class_install_property (object_class, PROP_INVERT_BOOLEAN,
                                   g_param_spec_boolean ("invert-boolean",
                                                         P_("Invert boolean"),
                                                         P_("Whether to invert the value of a boolean property"),
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
}

/**
 * g_property_action_new:
 * @name: the name of the action to create
 * @object: (type GObject.Object): the object that has the property
 *   to wrap
 * @property_name: the name of the property
 *
 * Creates a #GAction corresponding to the value of property
 * @property_name on @object.
 *
 * The property must be existent and readable and writable (and not
 * construct-only).
 *
 * This function takes a reference on @object and doesn't release it
 * until the action is destroyed.
 *
 * Returns: a new #GPropertyAction
 *
 * Since: 2.38
 **/
GPropertyAction *
g_property_action_new (const gchar *name,
                       gpointer     object,
                       const gchar *property_name)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_object_new (G_TYPE_PROPERTY_ACTION,
                       "name", name,
                       "object", object,
                       "property-name", property_name,
                       NULL);
}
