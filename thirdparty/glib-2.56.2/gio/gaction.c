/*
 * Copyright © 2010 Codethink Limited
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
#include "gaction.h"
#include "glibintl.h"

#include <string.h>

G_DEFINE_INTERFACE (GAction, g_action, G_TYPE_OBJECT)

/**
 * SECTION:gaction
 * @title: GAction
 * @short_description: An action interface
 * @include: gio/gio.h
 *
 * #GAction represents a single named action.
 *
 * The main interface to an action is that it can be activated with
 * g_action_activate().  This results in the 'activate' signal being
 * emitted.  An activation has a #GVariant parameter (which may be
 * %NULL).  The correct type for the parameter is determined by a static
 * parameter type (which is given at construction time).
 *
 * An action may optionally have a state, in which case the state may be
 * set with g_action_change_state().  This call takes a #GVariant.  The
 * correct type for the state is determined by a static state type
 * (which is given at construction time).
 *
 * The state may have a hint associated with it, specifying its valid
 * range.
 *
 * #GAction is merely the interface to the concept of an action, as
 * described above.  Various implementations of actions exist, including
 * #GSimpleAction.
 *
 * In all cases, the implementing class is responsible for storing the
 * name of the action, the parameter type, the enabled state, the
 * optional state type and the state and emitting the appropriate
 * signals when these change.  The implementor is responsible for filtering
 * calls to g_action_activate() and g_action_change_state() for type
 * safety and for the state being enabled.
 *
 * Probably the only useful thing to do with a #GAction is to put it
 * inside of a #GSimpleActionGroup.
 **/

/**
 * GAction:
 *
 * #GAction is an opaque data structure and can only be accessed
 * using the following functions.
 **/

/**
 * GActionInterface:
 * @get_name: the virtual function pointer for g_action_get_name()
 * @get_parameter_type: the virtual function pointer for g_action_get_parameter_type()
 * @get_state_type: the virtual function pointer for g_action_get_state_type()
 * @get_state_hint: the virtual function pointer for g_action_get_state_hint()
 * @get_enabled: the virtual function pointer for g_action_get_enabled()
 * @get_state: the virtual function pointer for g_action_get_state()
 * @change_state: the virtual function pointer for g_action_change_state()
 * @activate: the virtual function pointer for g_action_activate().  Note that #GAction does not have an
 *            'activate' signal but that implementations of it may have one.
 *
 * The virtual function table for #GAction.
 *
 * Since: 2.28
 */

void
g_action_default_init (GActionInterface *iface)
{
  /**
   * GAction:name:
   *
   * The name of the action.  This is mostly meaningful for identifying
   * the action once it has been added to a #GActionGroup. It is immutable.
   *
   * Since: 2.28
   **/
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("name",
                                                            P_("Action Name"),
                                                            P_("The name used to invoke the action"),
                                                            NULL,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));

  /**
   * GAction:parameter-type:
   *
   * The type of the parameter that must be given when activating the
   * action. This is immutable, and may be %NULL if no parameter is needed when
   * activating the action.
   *
   * Since: 2.28
   **/
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("parameter-type",
                                                           P_("Parameter Type"),
                                                           P_("The type of GVariant passed to activate()"),
                                                           G_TYPE_VARIANT_TYPE,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS));

  /**
   * GAction:enabled:
   *
   * If @action is currently enabled.
   *
   * If the action is disabled then calls to g_action_activate() and
   * g_action_change_state() have no effect.
   *
   * Since: 2.28
   **/
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("enabled",
                                                             P_("Enabled"),
                                                             P_("If the action can be activated"),
                                                             TRUE,
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));

  /**
   * GAction:state-type:
   *
   * The #GVariantType of the state that the action has, or %NULL if the
   * action is stateless. This is immutable.
   *
   * Since: 2.28
   **/
  g_object_interface_install_property (iface,
                                       g_param_spec_boxed ("state-type",
                                                           P_("State Type"),
                                                           P_("The type of the state kept by the action"),
                                                           G_TYPE_VARIANT_TYPE,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS));

  /**
   * GAction:state:
   *
   * The state of the action, or %NULL if the action is stateless.
   *
   * Since: 2.28
   **/
  g_object_interface_install_property (iface,
                                       g_param_spec_variant ("state",
                                                             P_("State"),
                                                             P_("The state the action is in"),
                                                             G_VARIANT_TYPE_ANY,
                                                             NULL,
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));
}

/**
 * g_action_change_state:
 * @action: a #GAction
 * @value: the new state
 *
 * Request for the state of @action to be changed to @value.
 *
 * The action must be stateful and @value must be of the correct type.
 * See g_action_get_state_type().
 *
 * This call merely requests a change.  The action may refuse to change
 * its state or may change its state to something other than @value.
 * See g_action_get_state_hint().
 *
 * If the @value GVariant is floating, it is consumed.
 *
 * Since: 2.30
 **/
void
g_action_change_state (GAction  *action,
                       GVariant *value)
{
  const GVariantType *state_type;

  g_return_if_fail (G_IS_ACTION (action));
  g_return_if_fail (value != NULL);
  state_type = g_action_get_state_type (action);
  g_return_if_fail (state_type != NULL);
  g_return_if_fail (g_variant_is_of_type (value, state_type));

  g_variant_ref_sink (value);

  G_ACTION_GET_IFACE (action)
    ->change_state (action, value);

  g_variant_unref (value);
}

/**
 * g_action_get_state:
 * @action: a #GAction
 *
 * Queries the current state of @action.
 *
 * If the action is not stateful then %NULL will be returned.  If the
 * action is stateful then the type of the return value is the type
 * given by g_action_get_state_type().
 *
 * The return value (if non-%NULL) should be freed with
 * g_variant_unref() when it is no longer required.
 *
 * Returns: (transfer full): the current state of the action
 *
 * Since: 2.28
 **/
GVariant *
g_action_get_state (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return G_ACTION_GET_IFACE (action)
    ->get_state (action);
}

/**
 * g_action_get_name:
 * @action: a #GAction
 *
 * Queries the name of @action.
 *
 * Returns: the name of the action
 *
 * Since: 2.28
 **/
const gchar *
g_action_get_name (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return G_ACTION_GET_IFACE (action)
    ->get_name (action);
}

/**
 * g_action_get_parameter_type:
 * @action: a #GAction
 *
 * Queries the type of the parameter that must be given when activating
 * @action.
 *
 * When activating the action using g_action_activate(), the #GVariant
 * given to that function must be of the type returned by this function.
 *
 * In the case that this function returns %NULL, you must not give any
 * #GVariant, but %NULL instead.
 *
 * Returns: (nullable): the parameter type
 *
 * Since: 2.28
 **/
const GVariantType *
g_action_get_parameter_type (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return G_ACTION_GET_IFACE (action)
    ->get_parameter_type (action);
}

/**
 * g_action_get_state_type:
 * @action: a #GAction
 *
 * Queries the type of the state of @action.
 *
 * If the action is stateful (e.g. created with
 * g_simple_action_new_stateful()) then this function returns the
 * #GVariantType of the state.  This is the type of the initial value
 * given as the state. All calls to g_action_change_state() must give a
 * #GVariant of this type and g_action_get_state() will return a
 * #GVariant of the same type.
 *
 * If the action is not stateful (e.g. created with g_simple_action_new())
 * then this function will return %NULL. In that case, g_action_get_state()
 * will return %NULL and you must not call g_action_change_state().
 *
 * Returns: (nullable): the state type, if the action is stateful
 *
 * Since: 2.28
 **/
const GVariantType *
g_action_get_state_type (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return G_ACTION_GET_IFACE (action)
    ->get_state_type (action);
}

/**
 * g_action_get_state_hint:
 * @action: a #GAction
 *
 * Requests a hint about the valid range of values for the state of
 * @action.
 *
 * If %NULL is returned it either means that the action is not stateful
 * or that there is no hint about the valid range of values for the
 * state of the action.
 *
 * If a #GVariant array is returned then each item in the array is a
 * possible value for the state.  If a #GVariant pair (ie: two-tuple) is
 * returned then the tuple specifies the inclusive lower and upper bound
 * of valid values for the state.
 *
 * In any case, the information is merely a hint.  It may be possible to
 * have a state value outside of the hinted range and setting a value
 * within the range may fail.
 *
 * The return value (if non-%NULL) should be freed with
 * g_variant_unref() when it is no longer required.
 *
 * Returns: (nullable) (transfer full): the state range hint
 *
 * Since: 2.28
 **/
GVariant *
g_action_get_state_hint (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return G_ACTION_GET_IFACE (action)
    ->get_state_hint (action);
}

/**
 * g_action_get_enabled:
 * @action: a #GAction
 *
 * Checks if @action is currently enabled.
 *
 * An action must be enabled in order to be activated or in order to
 * have its state changed from outside callers.
 *
 * Returns: whether the action is enabled
 *
 * Since: 2.28
 **/
gboolean
g_action_get_enabled (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), FALSE);

  return G_ACTION_GET_IFACE (action)
    ->get_enabled (action);
}

/**
 * g_action_activate:
 * @action: a #GAction
 * @parameter: (nullable): the parameter to the activation
 *
 * Activates the action.
 *
 * @parameter must be the correct type of parameter for the action (ie:
 * the parameter type given at construction time).  If the parameter
 * type was %NULL then @parameter must also be %NULL.
 *
 * If the @parameter GVariant is floating, it is consumed.
 *
 * Since: 2.28
 **/
void
g_action_activate (GAction  *action,
                   GVariant *parameter)
{
  g_return_if_fail (G_IS_ACTION (action));

  if (parameter != NULL)
    g_variant_ref_sink (parameter);

  G_ACTION_GET_IFACE (action)
    ->activate (action, parameter);

  if (parameter != NULL)
    g_variant_unref (parameter);
}

/**
 * g_action_name_is_valid:
 * @action_name: an potential action name
 *
 * Checks if @action_name is valid.
 *
 * @action_name is valid if it consists only of alphanumeric characters,
 * plus '-' and '.'.  The empty string is not a valid action name.
 *
 * It is an error to call this function with a non-utf8 @action_name.
 * @action_name must not be %NULL.
 *
 * Returns: %TRUE if @action_name is valid
 *
 * Since: 2.38
 **/
gboolean
g_action_name_is_valid (const gchar *action_name)
{
  gchar c;
  gint i;

  g_return_val_if_fail (action_name != NULL, FALSE);

  for (i = 0; (c = action_name[i]); i++)
    if (!g_ascii_isalnum (c) && c != '.' && c != '-')
      return FALSE;

  return i > 0;
}

/**
 * g_action_parse_detailed_name:
 * @detailed_name: a detailed action name
 * @action_name: (out): the action name
 * @target_value: (out): the target value, or %NULL for no target
 * @error: a pointer to a %NULL #GError, or %NULL
 *
 * Parses a detailed action name into its separate name and target
 * components.
 *
 * Detailed action names can have three formats.
 *
 * The first format is used to represent an action name with no target
 * value and consists of just an action name containing no whitespace
 * nor the characters ':', '(' or ')'.  For example: "app.action".
 *
 * The second format is used to represent an action with a target value
 * that is a non-empty string consisting only of alphanumerics, plus '-'
 * and '.'.  In that case, the action name and target value are
 * separated by a double colon ("::").  For example:
 * "app.action::target".
 *
 * The third format is used to represent an action with any type of
 * target value, including strings.  The target value follows the action
 * name, surrounded in parens.  For example: "app.action(42)".  The
 * target value is parsed using g_variant_parse().  If a tuple-typed
 * value is desired, it must be specified in the same way, resulting in
 * two sets of parens, for example: "app.action((1,2,3))".  A string
 * target can be specified this way as well: "app.action('target')".
 * For strings, this third format must be used if * target value is
 * empty or contains characters other than alphanumerics, '-' and '.'.
 *
 * Returns: %TRUE if successful, else %FALSE with @error set
 *
 * Since: 2.38
 **/
gboolean
g_action_parse_detailed_name (const gchar  *detailed_name,
                              gchar       **action_name,
                              GVariant    **target_value,
                              GError      **error)
{
  const gchar *target;
  gsize target_len;
  gsize base_len;

  /* For historical (compatibility) reasons, this function accepts some
   * cases of invalid action names as long as they don't interfere with
   * the separation of the action from the target value.
   *
   * We decide which format we have based on which we see first between
   * '::' '(' and '\0'.
   */

  if (*detailed_name == '\0' || *detailed_name == ' ')
    goto bad_fmt;

  base_len = strcspn (detailed_name, ": ()");
  target = detailed_name + base_len;
  target_len = strlen (target);

  switch (target[0])
    {
    case ' ':
    case ')':
      goto bad_fmt;

    case ':':
      if (target[1] != ':')
        goto bad_fmt;

      *target_value = g_variant_ref_sink (g_variant_new_string (target + 2));
      break;

    case '(':
      {
        if (target[target_len - 1] != ')')
          goto bad_fmt;

        *target_value = g_variant_parse (NULL, target + 1, target + target_len - 1, NULL, error);
        if (*target_value == NULL)
          goto bad_fmt;
      }
      break;

    case '\0':
      *target_value = NULL;
      break;
    }

  *action_name = g_strndup (detailed_name, base_len);

  return TRUE;

bad_fmt:
  if (error)
    {
      if (*error == NULL)
        g_set_error (error, G_VARIANT_PARSE_ERROR, G_VARIANT_PARSE_ERROR_FAILED,
                     "Detailed action name '%s' has invalid format", detailed_name);
      else
        g_prefix_error (error, "Detailed action name '%s' has invalid format: ", detailed_name);
    }

  return FALSE;
}

/**
 * g_action_print_detailed_name:
 * @action_name: a valid action name
 * @target_value: (nullable): a #GVariant target value, or %NULL
 *
 * Formats a detailed action name from @action_name and @target_value.
 *
 * It is an error to call this function with an invalid action name.
 *
 * This function is the opposite of g_action_parse_detailed_name().
 * It will produce a string that can be parsed back to the @action_name
 * and @target_value by that function.
 *
 * See that function for the types of strings that will be printed by
 * this function.
 *
 * Returns: a detailed format string
 *
 * Since: 2.38
 **/
gchar *
g_action_print_detailed_name (const gchar *action_name,
                              GVariant    *target_value)
{
  g_return_val_if_fail (g_action_name_is_valid (action_name), NULL);

  if (target_value == NULL)
    return g_strdup (action_name);

  if (g_variant_is_of_type (target_value, G_VARIANT_TYPE_STRING))
    {
      const gchar *str = g_variant_get_string (target_value, NULL);

      if (g_action_name_is_valid (str))
        return g_strconcat (action_name, "::", str, NULL);
    }

  {
    GString *result = g_string_new (action_name);
    g_string_append_c (result, '(');
    g_variant_print_string (target_value, result, TRUE);
    g_string_append_c (result, ')');

    return g_string_free (result, FALSE);
  }
}
