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

#include "gnotificationbackend.h"

#include "gapplication.h"
#include "giomodule-priv.h"
#include "gnotification-private.h"
#include "gdbusconnection.h"
#include "gactiongroup.h"
#include "gaction.h"
#include "gthemedicon.h"
#include "gfileicon.h"
#include "gfile.h"
#include "gdbusutils.h"

#define G_TYPE_FDO_NOTIFICATION_BACKEND  (g_fdo_notification_backend_get_type ())
#define G_FDO_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_FDO_NOTIFICATION_BACKEND, GFdoNotificationBackend))

typedef struct _GFdoNotificationBackend GFdoNotificationBackend;
typedef GNotificationBackendClass       GFdoNotificationBackendClass;

struct _GFdoNotificationBackend
{
  GNotificationBackend parent;

  guint   notify_subscription;
  GSList *notifications;
};

GType g_fdo_notification_backend_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GFdoNotificationBackend, g_fdo_notification_backend, G_TYPE_NOTIFICATION_BACKEND,
  _g_io_modules_ensure_extension_points_registered ();
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME,
                                 g_define_type_id, "freedesktop", 0))

typedef struct
{
  GFdoNotificationBackend *backend;
  gchar *id;
  guint32 notify_id;
  gchar *default_action;
  GVariant *default_action_target;
} FreedesktopNotification;

static void
freedesktop_notification_free (gpointer data)
{
  FreedesktopNotification *n = data;

  g_free (n->id);
  g_free (n->default_action);
  if (n->default_action_target)
    g_variant_unref (n->default_action_target);

  g_slice_free (FreedesktopNotification, n);
}

static FreedesktopNotification *
freedesktop_notification_new (GFdoNotificationBackend *backend,
                              const gchar             *id,
                              GNotification           *notification)
{
  FreedesktopNotification *n;

  n = g_slice_new0 (FreedesktopNotification);
  n->backend = backend;
  n->id = g_strdup (id);
  n->notify_id = 0;
  g_notification_get_default_action (notification,
                                     &n->default_action,
                                     &n->default_action_target);

  return n;
}

static FreedesktopNotification *
g_fdo_notification_backend_find_notification (GFdoNotificationBackend *backend,
                                              const gchar             *id)
{
  GSList *it;

  for (it = backend->notifications; it != NULL; it = it->next)
    {
      FreedesktopNotification *n = it->data;
      if (g_str_equal (n->id, id))
        return n;
    }

  return NULL;
}

static FreedesktopNotification *
g_fdo_notification_backend_find_notification_by_notify_id (GFdoNotificationBackend *backend,
                                                           guint32                  id)
{
  GSList *it;

  for (it = backend->notifications; it != NULL; it = it->next)
    {
      FreedesktopNotification *n = it->data;
      if (n->notify_id == id)
        return n;
    }

  return NULL;
}

static void
activate_action (GFdoNotificationBackend *backend,
                 const gchar             *name,
                 GVariant                *parameter)
{
  GNotificationBackend *g_backend = G_NOTIFICATION_BACKEND (backend);

  if (name)
    {
      if (g_str_has_prefix (name, "app."))
        g_action_group_activate_action (G_ACTION_GROUP (g_backend->application), name + 4, parameter);
    }
  else
    {
      g_application_activate (g_backend->application);
    }
}

static void
notify_signal (GDBusConnection *connection,
               const gchar     *sender_name,
               const gchar     *object_path,
               const gchar     *interface_name,
               const gchar     *signal_name,
               GVariant        *parameters,
               gpointer         user_data)
{
  GFdoNotificationBackend *backend = user_data;
  guint32 id = 0;
  const gchar *action = NULL;
  FreedesktopNotification *n;

  if (g_str_equal (signal_name, "NotificationClosed") &&
      g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(uu)")))
    {
      g_variant_get (parameters, "(uu)", &id, NULL);
    }
  else if (g_str_equal (signal_name, "ActionInvoked") &&
           g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(us)")))
    {
      g_variant_get (parameters, "(u&s)", &id, &action);
    }
  else
    return;

  n = g_fdo_notification_backend_find_notification_by_notify_id (backend, id);
  if (n == NULL)
    return;

  if (action)
    {
      if (g_str_equal (action, "default"))
        {
          activate_action (backend, n->default_action, n->default_action_target);
        }
      else
        {
          gchar *name;
          GVariant *target;

          if (g_action_parse_detailed_name (action, &name, &target, NULL))
            {
              activate_action (backend, name, target);
              g_free (name);
              if (target)
                g_variant_unref (target);
            }
        }
    }

  /* Get the notification again in case the action redrew it */
  n = g_fdo_notification_backend_find_notification_by_notify_id (backend, id);
  if (n != NULL)
    {
      backend->notifications = g_slist_remove (backend->notifications, n);
      freedesktop_notification_free (n);
    }
}

/* Converts a GNotificationPriority to an urgency level as defined by
 * the freedesktop spec (0: low, 1: normal, 2: critical).
 */
static guchar
urgency_from_priority (GNotificationPriority priority)
{
  switch (priority)
    {
    case G_NOTIFICATION_PRIORITY_LOW:
      return 0;

    default:
    case G_NOTIFICATION_PRIORITY_NORMAL:
    case G_NOTIFICATION_PRIORITY_HIGH:
      return 1;

    case G_NOTIFICATION_PRIORITY_URGENT:
      return 2;
    }
}

static void
call_notify (GDBusConnection     *con,
             GApplication        *app,
             guint32              replace_id,
             GNotification       *notification,
             GAsyncReadyCallback  callback,
             gpointer             user_data)
{
  GVariantBuilder action_builder;
  guint n_buttons;
  guint i;
  GVariantBuilder hints_builder;
  GIcon *icon;
  GVariant *parameters;
  const gchar *body;
  guchar urgency;

  g_variant_builder_init (&action_builder, G_VARIANT_TYPE_STRING_ARRAY);
  if (g_notification_get_default_action (notification, NULL, NULL))
    {
      g_variant_builder_add (&action_builder, "s", "default");
      g_variant_builder_add (&action_builder, "s", "");
    }

  n_buttons = g_notification_get_n_buttons (notification);
  for (i = 0; i < n_buttons; i++)
    {
      gchar *label;
      gchar *action;
      GVariant *target;
      gchar *detailed_name;

      g_notification_get_button (notification, i, &label, &action, &target);
      detailed_name = g_action_print_detailed_name (action, target);

      /* Actions named 'default' collide with libnotify's naming of the
       * default action. Rewriting them to something unique is enough,
       * because those actions can never be activated (they aren't
       * prefixed with 'app.').
       */
      if (g_str_equal (detailed_name, "default"))
        {
          g_free (detailed_name);
          detailed_name = g_dbus_generate_guid ();
        }

      g_variant_builder_add_value (&action_builder, g_variant_new_take_string (detailed_name));
      g_variant_builder_add_value (&action_builder, g_variant_new_take_string (label));

      g_free (action);
      if (target)
        g_variant_unref (target);
    }

  g_variant_builder_init (&hints_builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&hints_builder, "{sv}", "desktop-entry",
                         g_variant_new_string (g_application_get_application_id (app)));
  urgency = urgency_from_priority (g_notification_get_priority (notification));
  g_variant_builder_add (&hints_builder, "{sv}", "urgency", g_variant_new_byte (urgency));
  icon = g_notification_get_icon (notification);
  if (icon != NULL)
    {
      if (G_IS_FILE_ICON (icon))
        {
           GFile *file;

           file = g_file_icon_get_file (G_FILE_ICON (icon));
           g_variant_builder_add (&hints_builder, "{sv}", "image-path",
                                  g_variant_new_take_string (g_file_get_path (file)));
        }
      else if (G_IS_THEMED_ICON (icon))
        {
           const gchar* const* icon_names = g_themed_icon_get_names(G_THEMED_ICON (icon));
           /* Take first name from GThemedIcon */
           g_variant_builder_add (&hints_builder, "{sv}", "image-path",
                                  g_variant_new_string (icon_names[0]));
        }
    }

  body = g_notification_get_body (notification);

  parameters = g_variant_new ("(susssasa{sv}i)",
                              "",           /* app name */
                              replace_id,
                              "",           /* app icon */
                              g_notification_get_title (notification),
                              body ? body : "",
                              &action_builder,
                              &hints_builder,
                              -1);          /* expire_timeout */

  g_dbus_connection_call (con, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications", "Notify",
                          parameters, G_VARIANT_TYPE ("(u)"),
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                          callback, user_data);
}

static void
notification_sent (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  FreedesktopNotification *n = user_data;
  GVariant *val;
  GError *error = NULL;
  static gboolean warning_printed = FALSE;

  val = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), result, &error);
  if (val)
    {
      GFdoNotificationBackend *backend = n->backend;
      FreedesktopNotification *match;

      g_variant_get (val, "(u)", &n->notify_id);
      g_variant_unref (val);

      match = g_fdo_notification_backend_find_notification_by_notify_id (backend, n->notify_id);
      if (match != NULL)
        {
          backend->notifications = g_slist_remove (backend->notifications, match);
          freedesktop_notification_free (match);
        }
      backend->notifications = g_slist_prepend (backend->notifications, n);
    }
  else
    {
      if (!warning_printed)
        {
          g_warning ("unable to send notifications through org.freedesktop.Notifications: %s",
                     error->message);
          warning_printed = TRUE;
        }

      freedesktop_notification_free (n);
      g_error_free (error);
    }
}

static void
g_fdo_notification_backend_dispose (GObject *object)
{
  GFdoNotificationBackend *backend = G_FDO_NOTIFICATION_BACKEND (object);

  if (backend->notify_subscription)
    {
      GDBusConnection *session_bus;

      session_bus = G_NOTIFICATION_BACKEND (backend)->dbus_connection;
      g_dbus_connection_signal_unsubscribe (session_bus, backend->notify_subscription);
      backend->notify_subscription = 0;
    }

  if (backend->notifications)
    {
      g_slist_free_full (backend->notifications, freedesktop_notification_free);
      backend->notifications = NULL;
    }

  G_OBJECT_CLASS (g_fdo_notification_backend_parent_class)->dispose (object);
}

static gboolean
g_fdo_notification_backend_is_supported (void)
{
  /* This is the fallback backend with the lowest priority. To avoid an
   * unnecessary synchronous dbus call to check for
   * org.freedesktop.Notifications, this function always succeeds. A
   * warning will be printed when sending the first notification fails.
   */
  return TRUE;
}

static void
g_fdo_notification_backend_send_notification (GNotificationBackend *backend,
                                              const gchar          *id,
                                              GNotification        *notification)
{
  GFdoNotificationBackend *self = G_FDO_NOTIFICATION_BACKEND (backend);
  FreedesktopNotification *n, *tmp;

  if (self->notify_subscription == 0)
    {
      self->notify_subscription =
        g_dbus_connection_signal_subscribe (backend->dbus_connection,
                                            "org.freedesktop.Notifications",
                                            "org.freedesktop.Notifications", NULL,
                                            "/org/freedesktop/Notifications", NULL,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            notify_signal, backend, NULL);
    }

  n = freedesktop_notification_new (self, id, notification);

  tmp = g_fdo_notification_backend_find_notification (self, id);
  if (tmp)
    n->notify_id = tmp->notify_id;

  call_notify (backend->dbus_connection, backend->application, n->notify_id, notification, notification_sent, n);
}

static void
g_fdo_notification_backend_withdraw_notification (GNotificationBackend *backend,
                                                  const gchar          *id)
{
  GFdoNotificationBackend *self = G_FDO_NOTIFICATION_BACKEND (backend);
  FreedesktopNotification *n;

  n = g_fdo_notification_backend_find_notification (self, id);
  if (n)
    {
      if (n->notify_id > 0)
        {
          g_dbus_connection_call (backend->dbus_connection,
                                  "org.freedesktop.Notifications",
                                  "/org/freedesktop/Notifications",
                                  "org.freedesktop.Notifications", "CloseNotification",
                                  g_variant_new ("(u)", n->notify_id), NULL,
                                  G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
        }

      self->notifications = g_slist_remove (self->notifications, n);
      freedesktop_notification_free (n);
    }
}

static void
g_fdo_notification_backend_init (GFdoNotificationBackend *backend)
{
}

static void
g_fdo_notification_backend_class_init (GFdoNotificationBackendClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS (class);

  object_class->dispose = g_fdo_notification_backend_dispose;

  backend_class->is_supported = g_fdo_notification_backend_is_supported;
  backend_class->send_notification = g_fdo_notification_backend_send_notification;
  backend_class->withdraw_notification = g_fdo_notification_backend_withdraw_notification;
}
