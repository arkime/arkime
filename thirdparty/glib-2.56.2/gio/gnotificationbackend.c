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

#include "gnotificationbackend.h"

#include "gnotification.h"
#include "gapplication.h"
#include "gactiongroup.h"
#include "giomodule-priv.h"

G_DEFINE_TYPE (GNotificationBackend, g_notification_backend, G_TYPE_OBJECT)

static void
g_notification_backend_class_init (GNotificationBackendClass *class)
{
}

static void
g_notification_backend_init (GNotificationBackend *backend)
{
}

GNotificationBackend *
g_notification_backend_new_default (GApplication *application)
{
  GType backend_type;
  GNotificationBackend *backend;

  g_return_val_if_fail (G_IS_APPLICATION (application), NULL);

  backend_type = _g_io_module_get_default_type (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME,
                                                "GNOTIFICATION_BACKEND",
                                                G_STRUCT_OFFSET (GNotificationBackendClass, is_supported));

  backend = g_object_new (backend_type, NULL);

  /* Avoid ref cycle by not taking a ref to the application at all. The
   * backend only lives as long as the application does.
   */
  backend->application = application;

  backend->dbus_connection = g_application_get_dbus_connection (application);
  if (backend->dbus_connection)
    g_object_ref (backend->dbus_connection);

  return backend;
}

void
g_notification_backend_send_notification (GNotificationBackend *backend,
                                          const gchar          *id,
                                          GNotification        *notification)
{
  g_return_if_fail (G_IS_NOTIFICATION_BACKEND (backend));
  g_return_if_fail (G_IS_NOTIFICATION (notification));

  G_NOTIFICATION_BACKEND_GET_CLASS (backend)->send_notification (backend, id, notification);
}

void
g_notification_backend_withdraw_notification (GNotificationBackend *backend,
                                              const gchar          *id)
{
  g_return_if_fail (G_IS_NOTIFICATION_BACKEND (backend));
  g_return_if_fail (id != NULL);

  G_NOTIFICATION_BACKEND_GET_CLASS (backend)->withdraw_notification (backend, id);
}
