/*
 * Copyright © 2013 Lars Uebernickel
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
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#include "config.h"

#include "gnotification-private.h"
#include "gdbusutils.h"
#include "gicon.h"
#include "gaction.h"
#include "gioenumtypes.h"

/**
 * SECTION:gnotification
 * @short_description: User Notifications (pop up messages)
 * @include: gio/gio.h
 *
 * #GNotification is a mechanism for creating a notification to be shown
 * to the user -- typically as a pop-up notification presented by the
 * desktop environment shell.
 *
 * The key difference between #GNotification and other similar APIs is
 * that, if supported by the desktop environment, notifications sent
 * with #GNotification will persist after the application has exited,
 * and even across system reboots.
 *
 * Since the user may click on a notification while the application is
 * not running, applications using #GNotification should be able to be
 * started as a D-Bus service, using #GApplication.
 *
 * User interaction with a notification (either the default action, or
 * buttons) must be associated with actions on the application (ie:
 * "app." actions).  It is not possible to route user interaction
 * through the notification itself, because the object will not exist if
 * the application is autostarted as a result of a notification being
 * clicked.
 *
 * A notification can be sent with g_application_send_notification().
 *
 * Since: 2.40
 **/

/**
 * GNotification:
 *
 * This structure type is private and should only be accessed using the
 * public APIs.
 *
 * Since: 2.40
 **/

typedef GObjectClass GNotificationClass;

struct _GNotification
{
  GObject parent;

  gchar *title;
  gchar *body;
  GIcon *icon;
  GNotificationPriority priority;
  GPtrArray *buttons;
  gchar *default_action;
  GVariant *default_action_target;
};

typedef struct
{
  gchar *label;
  gchar *action_name;
  GVariant *target;
} Button;

G_DEFINE_TYPE (GNotification, g_notification, G_TYPE_OBJECT)

static void
button_free (gpointer data)
{
  Button *button = data;

  g_free (button->label);
  g_free (button->action_name);
  if (button->target)
    g_variant_unref (button->target);

  g_slice_free (Button, button);
}

static void
g_notification_dispose (GObject *object)
{
  GNotification *notification = G_NOTIFICATION (object);

  g_clear_object (&notification->icon);

  G_OBJECT_CLASS (g_notification_parent_class)->dispose (object);
}

static void
g_notification_finalize (GObject *object)
{
  GNotification *notification = G_NOTIFICATION (object);

  g_free (notification->title);
  g_free (notification->body);
  g_free (notification->default_action);
  if (notification->default_action_target)
    g_variant_unref (notification->default_action_target);
  g_ptr_array_free (notification->buttons, TRUE);

  G_OBJECT_CLASS (g_notification_parent_class)->finalize (object);
}

static void
g_notification_class_init (GNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = g_notification_dispose;
  object_class->finalize = g_notification_finalize;
}

static void
g_notification_init (GNotification *notification)
{
  notification->buttons = g_ptr_array_new_full (2, button_free);
}

/**
 * g_notification_new:
 * @title: the title of the notification
 *
 * Creates a new #GNotification with @title as its title.
 *
 * After populating @notification with more details, it can be sent to
 * the desktop shell with g_application_send_notification(). Changing
 * any properties after this call will not have any effect until
 * resending @notification.
 *
 * Returns: a new #GNotification instance
 *
 * Since: 2.40
 */
GNotification *
g_notification_new (const gchar *title)
{
  GNotification *notification;

  g_return_val_if_fail (title != NULL, NULL);

  notification = g_object_new (G_TYPE_NOTIFICATION, NULL);
  notification->title = g_strdup (title);

  return notification;
}

/*< private >
 * g_notification_get_title:
 * @notification: a #GNotification
 *
 * Gets the title of @notification.
 *
 * Returns: the title of @notification
 *
 * Since: 2.40
 */
const gchar *
g_notification_get_title (GNotification *notification)
{
  g_return_val_if_fail (G_IS_NOTIFICATION (notification), NULL);

  return notification->title;
}

/**
 * g_notification_set_title:
 * @notification: a #GNotification
 * @title: the new title for @notification
 *
 * Sets the title of @notification to @title.
 *
 * Since: 2.40
 */
void
g_notification_set_title (GNotification *notification,
                          const gchar   *title)
{
  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (title != NULL);

  g_free (notification->title);

  notification->title = g_strdup (title);
}

/*< private >
 * g_notification_get_body:
 * @notification: a #GNotification
 *
 * Gets the current body of @notification.
 *
 * Returns: (nullable): the body of @notification
 *
 * Since: 2.40
 */
const gchar *
g_notification_get_body (GNotification *notification)
{
  g_return_val_if_fail (G_IS_NOTIFICATION (notification), NULL);

  return notification->body;
}

/**
 * g_notification_set_body:
 * @notification: a #GNotification
 * @body: (nullable): the new body for @notification, or %NULL
 *
 * Sets the body of @notification to @body.
 *
 * Since: 2.40
 */
void
g_notification_set_body (GNotification *notification,
                         const gchar   *body)
{
  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (body != NULL);

  g_free (notification->body);

  notification->body = g_strdup (body);
}

/*< private >
 * g_notification_get_icon:
 * @notification: a #GNotification
 *
 * Gets the icon currently set on @notification.
 *
 * Returns: (transfer none): the icon associated with @notification
 *
 * Since: 2.40
 */
GIcon *
g_notification_get_icon (GNotification *notification)
{
  g_return_val_if_fail (G_IS_NOTIFICATION (notification), NULL);

  return notification->icon;
}

/**
 * g_notification_set_icon:
 * @notification: a #GNotification
 * @icon: the icon to be shown in @notification, as a #GIcon
 *
 * Sets the icon of @notification to @icon.
 *
 * Since: 2.40
 */
void
g_notification_set_icon (GNotification *notification,
                         GIcon         *icon)
{
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  if (notification->icon)
    g_object_unref (notification->icon);

  notification->icon = g_object_ref (icon);
}

/*< private >
 * g_notification_get_priority:
 * @notification: a #GNotification
 *
 * Returns the priority of @notification
 *
 * Since: 2.42
 */
GNotificationPriority
g_notification_get_priority (GNotification *notification)
{
  g_return_val_if_fail (G_IS_NOTIFICATION (notification), G_NOTIFICATION_PRIORITY_NORMAL);

  return notification->priority;
}

/**
 * g_notification_set_urgent:
 * @notification: a #GNotification
 * @urgent: %TRUE if @notification is urgent
 *
 * Deprecated in favor of g_notification_set_priority().
 *
 * Since: 2.40
 * Deprecated: 2.42: Since 2.42, this has been deprecated in favour of
 *    g_notification_set_priority().
 */
void
g_notification_set_urgent (GNotification *notification,
                           gboolean       urgent)
{
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  notification->priority = urgent ?
      G_NOTIFICATION_PRIORITY_URGENT :
      G_NOTIFICATION_PRIORITY_NORMAL;
}

/**
 * g_notification_set_priority:
 * @notification: a #GNotification
 * @priority: a #GNotificationPriority
 *
 * Sets the priority of @notification to @priority. See
 * #GNotificationPriority for possible values.
 */
void
g_notification_set_priority (GNotification         *notification,
                             GNotificationPriority  priority)
{
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  notification->priority = priority;
}

/**
 * g_notification_add_button:
 * @notification: a #GNotification
 * @label: label of the button
 * @detailed_action: a detailed action name
 *
 * Adds a button to @notification that activates the action in
 * @detailed_action when clicked. That action must be an
 * application-wide action (starting with "app."). If @detailed_action
 * contains a target, the action will be activated with that target as
 * its parameter.
 *
 * See g_action_parse_detailed_name() for a description of the format
 * for @detailed_action.
 *
 * Since: 2.40
 */
void
g_notification_add_button (GNotification *notification,
                           const gchar   *label,
                           const gchar   *detailed_action)
{
  gchar *action;
  GVariant *target;
  GError *error = NULL;

  g_return_if_fail (detailed_action != NULL);

  if (!g_action_parse_detailed_name (detailed_action, &action, &target, &error))
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      g_error_free (error);
      return;
    }

  g_notification_add_button_with_target_value (notification, label, action, target);

  g_free (action);
  if (target)
    g_variant_unref (target);
}

/**
 * g_notification_add_button_with_target: (skip)
 * @notification: a #GNotification
 * @label: label of the button
 * @action: an action name
 * @target_format: (nullable): a #GVariant format string, or %NULL
 * @...: positional parameters, as determined by @target_format
 *
 * Adds a button to @notification that activates @action when clicked.
 * @action must be an application-wide action (it must start with "app.").
 *
 * If @target_format is given, it is used to collect remaining
 * positional parameters into a #GVariant instance, similar to
 * g_variant_new(). @action will be activated with that #GVariant as its
 * parameter.
 *
 * Since: 2.40
 */
void
g_notification_add_button_with_target (GNotification *notification,
                                       const gchar   *label,
                                       const gchar   *action,
                                       const gchar   *target_format,
                                       ...)
{
  va_list args;
  GVariant *target = NULL;

  if (target_format)
    {
      va_start (args, target_format);
      target = g_variant_new_va (target_format, NULL, &args);
      va_end (args);
    }

  g_notification_add_button_with_target_value (notification, label, action, target);
}

/**
 * g_notification_add_button_with_target_value: (rename-to g_notification_add_button_with_target)
 * @notification: a #GNotification
 * @label: label of the button
 * @action: an action name
 * @target: (nullable): a #GVariant to use as @action's parameter, or %NULL
 *
 * Adds a button to @notification that activates @action when clicked.
 * @action must be an application-wide action (it must start with "app.").
 *
 * If @target is non-%NULL, @action will be activated with @target as
 * its parameter.
 *
 * Since: 2.40
 */
void
g_notification_add_button_with_target_value (GNotification *notification,
                                             const gchar   *label,
                                             const gchar   *action,
                                             GVariant      *target)
{
  Button *button;

  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (label != NULL);
  g_return_if_fail (action != NULL && g_action_name_is_valid (action));

  if (!g_str_has_prefix (action, "app."))
    {
      g_warning ("%s: action '%s' does not start with 'app.'."
                 "This is unlikely to work properly.", G_STRFUNC, action);
    }

  button =  g_slice_new0 (Button);
  button->label = g_strdup (label);
  button->action_name = g_strdup (action);

  if (target)
    button->target = g_variant_ref_sink (target);

  g_ptr_array_add (notification->buttons, button);
}

/*< private >
 * g_notification_get_n_buttons:
 * @notification: a #GNotification
 *
 * Returns: the amount of buttons added to @notification.
 */
guint
g_notification_get_n_buttons (GNotification *notification)
{
  return notification->buttons->len;
}

/*< private >
 * g_notification_get_button:
 * @notification: a #GNotification
 * @index: index of the button
 * @label: (): return location for the button's label
 * @action: (): return location for the button's associated action
 * @target: (): return location for the target @action should be
 * activated with
 *
 * Returns a description of a button that was added to @notification
 * with g_notification_add_button().
 *
 * @index must be smaller than the value returned by
 * g_notification_get_n_buttons().
 */
void
g_notification_get_button (GNotification  *notification,
                           gint            index,
                           gchar         **label,
                           gchar         **action,
                           GVariant      **target)
{
  Button *button;

  button = g_ptr_array_index (notification->buttons, index);

  if (label)
    *label = g_strdup (button->label);

  if (action)
    *action = g_strdup (button->action_name);

  if (target)
    *target = button->target ? g_variant_ref (button->target) : NULL;
}

/*< private >
 * g_notification_get_button_with_action:
 * @notification: a #GNotification
 * @action: an action name
 *
 * Returns the index of the button in @notification that is associated
 * with @action, or -1 if no such button exists.
 */
gint
g_notification_get_button_with_action (GNotification *notification,
                                       const gchar   *action)
{
  guint i;

  for (i = 0; i < notification->buttons->len; i++)
    {
      Button *button;

      button = g_ptr_array_index (notification->buttons, i);
      if (g_str_equal (action, button->action_name))
        return i;
    }

  return -1;
}


/*< private >
 * g_notification_get_default_action:
 * @notification: a #GNotification
 * @action: (nullable): return location for the default action
 * @target: (nullable): return location for the target of the default action
 *
 * Gets the action and target for the default action of @notification.
 *
 * Returns: %TRUE if @notification has a default action
 */
gboolean
g_notification_get_default_action (GNotification  *notification,
                                   gchar         **action,
                                   GVariant      **target)
{
  if (notification->default_action == NULL)
    return FALSE;

  if (action)
    *action = g_strdup (notification->default_action);

  if (target)
    {
      if (notification->default_action_target)
        *target = g_variant_ref (notification->default_action_target);
      else
        *target = NULL;
    }

  return TRUE;
}

/**
 * g_notification_set_default_action:
 * @notification: a #GNotification
 * @detailed_action: a detailed action name
 *
 * Sets the default action of @notification to @detailed_action. This
 * action is activated when the notification is clicked on.
 *
 * The action in @detailed_action must be an application-wide action (it
 * must start with "app."). If @detailed_action contains a target, the
 * given action will be activated with that target as its parameter.
 * See g_action_parse_detailed_name() for a description of the format
 * for @detailed_action.
 *
 * When no default action is set, the application that the notification
 * was sent on is activated.
 *
 * Since: 2.40
 */
void
g_notification_set_default_action (GNotification *notification,
                                   const gchar   *detailed_action)
{
  gchar *action;
  GVariant *target;
  GError *error = NULL;

  if (!g_action_parse_detailed_name (detailed_action, &action, &target, &error))
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      g_error_free (error);
      return;
    }

  g_notification_set_default_action_and_target_value (notification, action, target);

  g_free (action);
  if (target)
    g_variant_unref (target);
}

/**
 * g_notification_set_default_action_and_target: (skip)
 * @notification: a #GNotification
 * @action: an action name
 * @target_format: (nullable): a #GVariant format string, or %NULL
 * @...: positional parameters, as determined by @target_format
 *
 * Sets the default action of @notification to @action. This action is
 * activated when the notification is clicked on. It must be an
 * application-wide action (it must start with "app.").
 *
 * If @target_format is given, it is used to collect remaining
 * positional parameters into a #GVariant instance, similar to
 * g_variant_new(). @action will be activated with that #GVariant as its
 * parameter.
 *
 * When no default action is set, the application that the notification
 * was sent on is activated.
 *
 * Since: 2.40
 */
void
g_notification_set_default_action_and_target (GNotification *notification,
                                              const gchar   *action,
                                              const gchar   *target_format,
                                              ...)
{
  va_list args;
  GVariant *target = NULL;

  if (target_format)
    {
      va_start (args, target_format);
      target = g_variant_new_va (target_format, NULL, &args);
      va_end (args);
    }

  g_notification_set_default_action_and_target_value (notification, action, target);
}

/**
 * g_notification_set_default_action_and_target_value: (rename-to g_notification_set_default_action_and_target)
 * @notification: a #GNotification
 * @action: an action name
 * @target: (nullable): a #GVariant to use as @action's parameter, or %NULL
 *
 * Sets the default action of @notification to @action. This action is
 * activated when the notification is clicked on. It must be an
 * application-wide action (start with "app.").
 *
 * If @target is non-%NULL, @action will be activated with @target as
 * its parameter.
 *
 * When no default action is set, the application that the notification
 * was sent on is activated.
 *
 * Since: 2.40
 */
void
g_notification_set_default_action_and_target_value (GNotification *notification,
                                                    const gchar   *action,
                                                    GVariant      *target)
{
  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (action != NULL && g_action_name_is_valid (action));

  if (!g_str_has_prefix (action, "app."))
    {
      g_warning ("%s: action '%s' does not start with 'app.'."
                 "This is unlikely to work properly.", G_STRFUNC, action);
    }

  g_free (notification->default_action);
  g_clear_pointer (&notification->default_action_target, g_variant_unref);

  notification->default_action = g_strdup (action);

  if (target)
    notification->default_action_target = g_variant_ref_sink (target);
}

static GVariant *
g_notification_serialize_button (Button *button)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "label", g_variant_new_string (button->label));
  g_variant_builder_add (&builder, "{sv}", "action", g_variant_new_string (button->action_name));

  if (button->target)
    g_variant_builder_add (&builder, "{sv}", "target", button->target);

  return g_variant_builder_end (&builder);
}

static GVariant *
g_notification_get_priority_nick (GNotification *notification)
{
  GEnumClass *enum_class;
  GEnumValue *value;
  GVariant *nick;

  enum_class = g_type_class_ref (G_TYPE_NOTIFICATION_PRIORITY);
  value = g_enum_get_value (enum_class, g_notification_get_priority (notification));
  g_assert (value != NULL);
  nick = g_variant_new_string (value->value_nick);
  g_type_class_unref (enum_class);

  return nick;
}

/*< private >
 * g_notification_serialize:
 *
 * Serializes @notification into an floating variant of type a{sv}.
 *
 * Returns: the serialized @notification as a floating variant.
 */
GVariant *
g_notification_serialize (GNotification *notification)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  if (notification->title)
    g_variant_builder_add (&builder, "{sv}", "title", g_variant_new_string (notification->title));

  if (notification->body)
    g_variant_builder_add (&builder, "{sv}", "body", g_variant_new_string (notification->body));

  if (notification->icon)
    {
      GVariant *serialized_icon;

      if ((serialized_icon = g_icon_serialize (notification->icon)))
        {
          g_variant_builder_add (&builder, "{sv}", "icon", serialized_icon);
          g_variant_unref (serialized_icon);
        }
    }

  g_variant_builder_add (&builder, "{sv}", "priority", g_notification_get_priority_nick (notification));

  if (notification->default_action)
    {
      g_variant_builder_add (&builder, "{sv}", "default-action",
                                               g_variant_new_string (notification->default_action));

      if (notification->default_action_target)
        g_variant_builder_add (&builder, "{sv}", "default-action-target",
                                                  notification->default_action_target);
    }

  if (notification->buttons->len > 0)
    {
      GVariantBuilder actions_builder;
      guint i;

      g_variant_builder_init (&actions_builder, G_VARIANT_TYPE ("aa{sv}"));

      for (i = 0; i < notification->buttons->len; i++)
        {
          Button *button = g_ptr_array_index (notification->buttons, i);
          g_variant_builder_add (&actions_builder, "@a{sv}", g_notification_serialize_button (button));
        }

      g_variant_builder_add (&builder, "{sv}", "buttons", g_variant_builder_end (&actions_builder));
    }

  return g_variant_builder_end (&builder);
}
