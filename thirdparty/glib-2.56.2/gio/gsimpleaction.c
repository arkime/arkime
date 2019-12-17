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

#include "gsimpleaction.h"

#include "gaction.h"
#include "glibintl.h"

/**
 * SECTION:gsimpleaction
 * @title: GSimpleAction
 * @short_description: A simple GAction implementation
 * @include: gio/gio.h
 *
 * A #GSimpleAction is the obvious simple implementation of the #GAction
 * interface. This is the easiest way to create an action for purposes of
 * adding it to a #GSimpleActionGroup.
 *
 * See also #GtkAction.
 */

/**
 * GSimpleAction:
 *
 * #GSimpleAction is an opaque data structure and can only be accessed
 * using the following functions.
 **/

struct _GSimpleAction
{
  GObject       parent_instance;

  gchar        *name;
  GVariantType *parameter_type;
  gboolean      enabled;
  GVariant     *state;
  GVariant     *state_hint;
  gboolean      state_set_already;
};

typedef GObjectClass GSimpleActionClass;

static void g_simple_action_iface_init (GActionInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GSimpleAction, g_simple_action, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION, g_simple_action_iface_init))

enum
{
  PROP_NONE,
  PROP_NAME,
  PROP_PARAMETER_TYPE,
  PROP_ENABLED,
  PROP_STATE_TYPE,
  PROP_STATE
};

enum
{
  SIGNAL_CHANGE_STATE,
  SIGNAL_ACTIVATE,
  NR_SIGNALS
};

static guint g_simple_action_signals[NR_SIGNALS];

static const gchar *
g_simple_action_get_name (GAction *action)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  return simple->name;
}

static const GVariantType *
g_simple_action_get_parameter_type (GAction *action)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  return simple->parameter_type;
}

static const GVariantType *
g_simple_action_get_state_type (GAction *action)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  if (simple->state != NULL)
    return g_variant_get_type (simple->state);
  else
    return NULL;
}

static GVariant *
g_simple_action_get_state_hint (GAction *action)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  if (simple->state_hint != NULL)
    return g_variant_ref (simple->state_hint);
  else
    return NULL;
}

static gboolean
g_simple_action_get_enabled (GAction *action)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  return simple->enabled;
}

static void
g_simple_action_change_state (GAction  *action,
                              GVariant *value)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  /* If the user connected a signal handler then they are responsible
   * for handling state changes.
   */
  if (g_signal_has_handler_pending (action, g_simple_action_signals[SIGNAL_CHANGE_STATE], 0, TRUE))
    g_signal_emit (action, g_simple_action_signals[SIGNAL_CHANGE_STATE], 0, value);

  /* If not, then the default behaviour is to just set the state. */
  else
    g_simple_action_set_state (simple, value);
}

/**
 * g_simple_action_set_state:
 * @simple: a #GSimpleAction
 * @value: the new #GVariant for the state
 *
 * Sets the state of the action.
 *
 * This directly updates the 'state' property to the given value.
 *
 * This should only be called by the implementor of the action.  Users
 * of the action should not attempt to directly modify the 'state'
 * property.  Instead, they should call g_action_change_state() to
 * request the change.
 *
 * If the @value GVariant is floating, it is consumed.
 *
 * Since: 2.30
 **/
void
g_simple_action_set_state (GSimpleAction *simple,
                           GVariant      *value)
{
  g_return_if_fail (G_IS_SIMPLE_ACTION (simple));
  g_return_if_fail (value != NULL);

  {
    const GVariantType *state_type;

    state_type = simple->state ?
                   g_variant_get_type (simple->state) : NULL;
    g_return_if_fail (state_type != NULL);
    g_return_if_fail (g_variant_is_of_type (value, state_type));
  }

  g_variant_ref_sink (value);

  if (!simple->state || !g_variant_equal (simple->state, value))
    {
      if (simple->state)
        g_variant_unref (simple->state);

      simple->state = g_variant_ref (value);

      g_object_notify (G_OBJECT (simple), "state");
    }

  g_variant_unref (value);
}

static GVariant *
g_simple_action_get_state (GAction *action)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  return simple->state ? g_variant_ref (simple->state) : NULL;
}

static void
g_simple_action_activate (GAction  *action,
                          GVariant *parameter)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (action);

  g_return_if_fail (simple->parameter_type == NULL ?
                      parameter == NULL :
                    (parameter != NULL &&
                     g_variant_is_of_type (parameter,
                                           simple->parameter_type)));

  if (parameter != NULL)
    g_variant_ref_sink (parameter);

  if (simple->enabled)
    {
      /* If the user connected a signal handler then they are responsible
       * for handling activation.
       */
      if (g_signal_has_handler_pending (action, g_simple_action_signals[SIGNAL_ACTIVATE], 0, TRUE))
        g_signal_emit (action, g_simple_action_signals[SIGNAL_ACTIVATE], 0, parameter);

      /* If not, do some reasonable defaults for stateful actions. */
      else if (simple->state)
        {
          /* If we have no parameter and this is a boolean action, toggle. */
          if (parameter == NULL && g_variant_is_of_type (simple->state, G_VARIANT_TYPE_BOOLEAN))
            {
              gboolean was_enabled = g_variant_get_boolean (simple->state);
              g_simple_action_change_state (action, g_variant_new_boolean (!was_enabled));
            }

          /* else, if the parameter and state type are the same, do a change-state */
          else if (g_variant_is_of_type (simple->state, g_variant_get_type (parameter)))
            g_simple_action_change_state (action, parameter);
        }
    }

  if (parameter != NULL)
    g_variant_unref (parameter);
}

static void
g_simple_action_set_property (GObject    *object,
                              guint       prop_id,
                              const GValue     *value,
                              GParamSpec *pspec)
{
  GSimpleAction *action = G_SIMPLE_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      action->name = g_strdup (g_value_get_string (value));
      break;

    case PROP_PARAMETER_TYPE:
      action->parameter_type = g_value_dup_boxed (value);
      break;

    case PROP_ENABLED:
      action->enabled = g_value_get_boolean (value);
      break;

    case PROP_STATE:
      /* The first time we see this (during construct) we should just
       * take the state as it was handed to us.
       *
       * After that, we should make sure we go through the same checks
       * as the C API.
       */
      if (!action->state_set_already)
        {
          action->state = g_value_dup_variant (value);
          action->state_set_already = TRUE;
        }
      else
        g_simple_action_set_state (action, g_value_get_variant (value));

      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_simple_action_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GAction *action = G_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, g_simple_action_get_name (action));
      break;

    case PROP_PARAMETER_TYPE:
      g_value_set_boxed (value, g_simple_action_get_parameter_type (action));
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, g_simple_action_get_enabled (action));
      break;

    case PROP_STATE_TYPE:
      g_value_set_boxed (value, g_simple_action_get_state_type (action));
      break;

    case PROP_STATE:
      g_value_take_variant (value, g_simple_action_get_state (action));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_simple_action_finalize (GObject *object)
{
  GSimpleAction *simple = G_SIMPLE_ACTION (object);

  g_free (simple->name);
  if (simple->parameter_type)
    g_variant_type_free (simple->parameter_type);
  if (simple->state)
    g_variant_unref (simple->state);
  if (simple->state_hint)
    g_variant_unref (simple->state_hint);

  G_OBJECT_CLASS (g_simple_action_parent_class)
    ->finalize (object);
}

void
g_simple_action_init (GSimpleAction *simple)
{
  simple->enabled = TRUE;
}

void
g_simple_action_iface_init (GActionInterface *iface)
{
  iface->get_name = g_simple_action_get_name;
  iface->get_parameter_type = g_simple_action_get_parameter_type;
  iface->get_state_type = g_simple_action_get_state_type;
  iface->get_state_hint = g_simple_action_get_state_hint;
  iface->get_enabled = g_simple_action_get_enabled;
  iface->get_state = g_simple_action_get_state;
  iface->change_state = g_simple_action_change_state;
  iface->activate = g_simple_action_activate;
}

void
g_simple_action_class_init (GSimpleActionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = g_simple_action_set_property;
  object_class->get_property = g_simple_action_get_property;
  object_class->finalize = g_simple_action_finalize;

  /**
   * GSimpleAction::activate:
   * @simple: the #GSimpleAction
   * @parameter: (nullable): the parameter to the activation
   *
   * Indicates that the action was just activated.
   *
   * @parameter will always be of the expected type.  In the event that
   * an incorrect type was given, no signal will be emitted.
   *
   * Since GLib 2.40, if no handler is connected to this signal then the
   * default behaviour for boolean-stated actions with a %NULL parameter
   * type is to toggle them via the #GSimpleAction::change-state signal.
   * For stateful actions where the state type is equal to the parameter
   * type, the default is to forward them directly to
   * #GSimpleAction::change-state.  This should allow almost all users
   * of #GSimpleAction to connect only one handler or the other.
   *
   * Since: 2.28
   */
  g_simple_action_signals[SIGNAL_ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_SIMPLE_ACTION,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VARIANT,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);

  /**
   * GSimpleAction::change-state:
   * @simple: the #GSimpleAction
   * @value: (nullable): the requested value for the state
   *
   * Indicates that the action just received a request to change its
   * state.
   *
   * @value will always be of the correct state type.  In the event that
   * an incorrect type was given, no signal will be emitted.
   *
   * If no handler is connected to this signal then the default
   * behaviour is to call g_simple_action_set_state() to set the state
   * to the requested value. If you connect a signal handler then no
   * default action is taken. If the state should change then you must
   * call g_simple_action_set_state() from the handler.
   *
   * An example of a 'change-state' handler:
   * |[<!-- language="C" -->
   * static void
   * change_volume_state (GSimpleAction *action,
   *                      GVariant      *value,
   *                      gpointer       user_data)
   * {
   *   gint requested;
   *
   *   requested = g_variant_get_int32 (value);
   *
   *   // Volume only goes from 0 to 10
   *   if (0 <= requested && requested <= 10)
   *     g_simple_action_set_state (action, value);
   * }
   * ]|
   *
   * The handler need not set the state to the requested value.
   * It could set it to any value at all, or take some other action.
   *
   * Since: 2.30
   */
  g_simple_action_signals[SIGNAL_CHANGE_STATE] =
    g_signal_new (I_("change-state"),
                  G_TYPE_SIMPLE_ACTION,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VARIANT,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);

  /**
   * GSimpleAction:name:
   *
   * The name of the action. This is mostly meaningful for identifying
   * the action once it has been added to a #GSimpleActionGroup.
   *
   * Since: 2.28
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
   * GSimpleAction:parameter-type:
   *
   * The type of the parameter that must be given when activating the
   * action.
   *
   * Since: 2.28
   **/
  g_object_class_install_property (object_class, PROP_PARAMETER_TYPE,
                                   g_param_spec_boxed ("parameter-type",
                                                       P_("Parameter Type"),
                                                       P_("The type of GVariant passed to activate()"),
                                                       G_TYPE_VARIANT_TYPE,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GSimpleAction:enabled:
   *
   * If @action is currently enabled.
   *
   * If the action is disabled then calls to g_action_activate() and
   * g_action_change_state() have no effect.
   *
   * Since: 2.28
   **/
  g_object_class_install_property (object_class, PROP_ENABLED,
                                   g_param_spec_boolean ("enabled",
                                                         P_("Enabled"),
                                                         P_("If the action can be activated"),
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GSimpleAction:state-type:
   *
   * The #GVariantType of the state that the action has, or %NULL if the
   * action is stateless.
   *
   * Since: 2.28
   **/
  g_object_class_install_property (object_class, PROP_STATE_TYPE,
                                   g_param_spec_boxed ("state-type",
                                                       P_("State Type"),
                                                       P_("The type of the state kept by the action"),
                                                       G_TYPE_VARIANT_TYPE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GSimpleAction:state:
   *
   * The state of the action, or %NULL if the action is stateless.
   *
   * Since: 2.28
   **/
  g_object_class_install_property (object_class, PROP_STATE,
                                   g_param_spec_variant ("state",
                                                         P_("State"),
                                                         P_("The state the action is in"),
                                                         G_VARIANT_TYPE_ANY,
                                                         NULL,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                         G_PARAM_STATIC_STRINGS));
}

/**
 * g_simple_action_set_enabled:
 * @simple: a #GSimpleAction
 * @enabled: whether the action is enabled
 *
 * Sets the action as enabled or not.
 *
 * An action must be enabled in order to be activated or in order to
 * have its state changed from outside callers.
 *
 * This should only be called by the implementor of the action.  Users
 * of the action should not attempt to modify its enabled flag.
 *
 * Since: 2.28
 **/
void
g_simple_action_set_enabled (GSimpleAction *simple,
                             gboolean       enabled)
{
  g_return_if_fail (G_IS_SIMPLE_ACTION (simple));

  enabled = !!enabled;

  if (simple->enabled != enabled)
    {
      simple->enabled = enabled;
      g_object_notify (G_OBJECT (simple), "enabled");
    }
}

/**
 * g_simple_action_set_state_hint:
 * @simple: a #GSimpleAction
 * @state_hint: (nullable): a #GVariant representing the state hint
 *
 * Sets the state hint for the action.
 *
 * See g_action_get_state_hint() for more information about
 * action state hints.
 *
 * Since: 2.44
 **/
void
g_simple_action_set_state_hint (GSimpleAction *simple,
                                GVariant      *state_hint)
{
  g_return_if_fail (G_IS_SIMPLE_ACTION (simple));

  if (simple->state_hint != NULL)
    {
      g_variant_unref (simple->state_hint);
      simple->state_hint = NULL;
    }

  if (state_hint != NULL)
    simple->state_hint = g_variant_ref (state_hint);
}

/**
 * g_simple_action_new:
 * @name: the name of the action
 * @parameter_type: (nullable): the type of parameter to the activate function
 *
 * Creates a new action.
 *
 * The created action is stateless.  See g_simple_action_new_stateful().
 *
 * Returns: a new #GSimpleAction
 *
 * Since: 2.28
 **/
GSimpleAction *
g_simple_action_new (const gchar        *name,
                     const GVariantType *parameter_type)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (G_TYPE_SIMPLE_ACTION,
                       "name", name,
                       "parameter-type", parameter_type,
                       NULL);
}

/**
 * g_simple_action_new_stateful:
 * @name: the name of the action
 * @parameter_type: (nullable): the type of the parameter to the activate function
 * @state: the initial state of the action
 *
 * Creates a new stateful action.
 *
 * @state is the initial state of the action.  All future state values
 * must have the same #GVariantType as the initial state.
 *
 * If the @state GVariant is floating, it is consumed.
 *
 * Returns: a new #GSimpleAction
 *
 * Since: 2.28
 **/
GSimpleAction *
g_simple_action_new_stateful (const gchar        *name,
                              const GVariantType *parameter_type,
                              GVariant           *state)
{
  return g_object_new (G_TYPE_SIMPLE_ACTION,
                       "name", name,
                       "parameter-type", parameter_type,
                       "state", state,
                       NULL);
}
