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

#include <glib.h>

#include "gnotification-server.h"
#include "gdbus-sessionbus.h"

static void
activate_app (GApplication *application,
              gpointer      user_data)
{
  GNotification *notification;
  GIcon *icon;

  notification = g_notification_new ("Test");

  g_application_send_notification (application, "test1", notification);
  g_application_send_notification (application, "test2", notification);
  g_application_withdraw_notification (application, "test1");
  g_application_send_notification (application, "test3", notification);

  icon = g_themed_icon_new ("i-c-o-n");
  g_notification_set_icon (notification, icon);
  g_object_unref (icon);

  g_notification_set_body (notification, "body");
  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_URGENT);
  g_notification_set_default_action_and_target (notification, "app.action", "i", 42);
  g_notification_add_button_with_target (notification, "label", "app.action2", "s", "bla");

  g_application_send_notification (application, "test4", notification);
  g_application_send_notification (application, NULL, notification);

  g_dbus_connection_flush_sync (g_application_get_dbus_connection (application), NULL, NULL);

  g_object_unref (notification);
}

static void
notification_received (GNotificationServer *server,
                       const gchar         *app_id,
                       const gchar         *notification_id,
                       GVariant            *notification,
                       gpointer             user_data)
{
  gint *count = user_data;
  const gchar *title;

  g_assert_cmpstr (app_id, ==, "org.gtk.TestApplication");

  switch (*count)
    {
    case 0:
      g_assert_cmpstr (notification_id, ==, "test1");
      g_assert (g_variant_lookup (notification, "title", "&s", &title));
      g_assert_cmpstr (title, ==, "Test");
      break;

    case 1:
      g_assert_cmpstr (notification_id, ==, "test2");
      break;

    case 2:
      g_assert_cmpstr (notification_id, ==, "test3");
      break;

    case 3:
      g_assert_cmpstr (notification_id, ==, "test4");
      break;

    case 4:
      g_assert (g_dbus_is_guid (notification_id));
 
      g_notification_server_stop (server);
      break;
    }

  (*count)++;
}

static void
notification_removed (GNotificationServer *server,
                      const gchar         *app_id,
                      const gchar         *notification_id,
                      gpointer             user_data)
{
  gint *count = user_data;

  g_assert_cmpstr (app_id, ==, "org.gtk.TestApplication");
  g_assert_cmpstr (notification_id, ==, "test1");

  (*count)++;
}

static void
server_notify_is_running (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  GMainLoop *loop = user_data;
  GNotificationServer *server = G_NOTIFICATION_SERVER (object);

  if (g_notification_server_get_is_running (server))
    {
      GApplication *app;

      app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_FLAGS_NONE);
      g_signal_connect (app, "activate", G_CALLBACK (activate_app), NULL);

      g_application_run (app, 0, NULL);

      g_object_unref (app);
    }
  else
    {
      g_main_loop_quit (loop);
    }
}

static gboolean
timeout (gpointer user_data)
{
  GNotificationServer *server = user_data;

  g_notification_server_stop (server);

  return G_SOURCE_REMOVE;
}

static void
basic (void)
{
  GNotificationServer *server;
  GMainLoop *loop;
  gint received_count = 0;
  gint removed_count = 0;

  session_bus_up ();

  loop = g_main_loop_new (NULL, FALSE);

  server = g_notification_server_new ();
  g_signal_connect (server, "notification-received", G_CALLBACK (notification_received), &received_count);
  g_signal_connect (server, "notification-removed", G_CALLBACK (notification_removed), &removed_count);
  g_signal_connect (server, "notify::is-running", G_CALLBACK (server_notify_is_running), loop);
  g_timeout_add_seconds (1, timeout, server);

  g_main_loop_run (loop);

  g_assert_cmpint (received_count, ==, 5);
  g_assert_cmpint (removed_count, ==, 1);

  g_object_unref (server);
  g_main_loop_unref (loop);
  session_bus_stop ();
}

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

static void
test_properties (void)
{
  GNotification *n;
  struct _GNotification *rn;
  GIcon *icon;
  const gchar * const *names;
  Button *b;

  n = g_notification_new ("Test");

  g_notification_set_title (n, "title");
  g_notification_set_body (n, "body");
  icon = g_themed_icon_new ("i-c-o-n");
  g_notification_set_icon (n, icon);
  g_object_unref (icon);
  g_notification_set_priority (n, G_NOTIFICATION_PRIORITY_HIGH);
  g_notification_add_button (n, "label1", "app.action1::target1");
  g_notification_set_default_action (n, "app.action2::target2");

  rn = (struct _GNotification *)n;

  g_assert_cmpstr (rn->title, ==, "title");
  g_assert_cmpstr (rn->body, ==, "body");
  g_assert (G_IS_THEMED_ICON (rn->icon));
  names = g_themed_icon_get_names (G_THEMED_ICON (rn->icon));
  g_assert_cmpstr (names[0], ==, "i-c-o-n");
  g_assert (names[1] == NULL);
  g_assert (rn->priority == G_NOTIFICATION_PRIORITY_HIGH);

  g_assert_cmpint (rn->buttons->len, ==, 1);
  b = (Button*)rn->buttons->pdata[0];
  g_assert_cmpstr (b->label, ==, "label1");
  g_assert_cmpstr (b->action_name, ==, "app.action1");
  g_assert_cmpstr (g_variant_get_string (b->target, NULL), ==, "target1");

  g_assert_cmpstr (rn->default_action, ==, "app.action2");
  g_assert_cmpstr (g_variant_get_string (rn->default_action_target, NULL), ==, "target2");

  g_object_unref (n);
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gnotification/basic", basic);
  g_test_add_func ("/gnotification/properties", test_properties);

  return g_test_run ();
}
