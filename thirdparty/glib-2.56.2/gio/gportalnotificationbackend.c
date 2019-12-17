/*
* Copyright © 2016 Red Hat, Inc.
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
* Author: Matthias Clasen
*/

#include "config.h"
#include "gnotificationbackend.h"

#include "giomodule-priv.h"
#include "gdbusconnection.h"
#include "gapplication.h"
#include "gnotification-private.h"
#include "gportalsupport.h"

#define G_TYPE_PORTAL_NOTIFICATION_BACKEND  (g_portal_notification_backend_get_type ())
#define G_PORTAL_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_PORTAL_NOTIFICATION_BACKEND, GPortalNotificationBackend))

typedef struct _GPortalNotificationBackend GPortalNotificationBackend;
typedef GNotificationBackendClass       GPortalNotificationBackendClass;

struct _GPortalNotificationBackend
{
  GNotificationBackend parent;
};

GType g_portal_notification_backend_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GPortalNotificationBackend, g_portal_notification_backend, G_TYPE_NOTIFICATION_BACKEND,
  _g_io_modules_ensure_extension_points_registered ();
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME,
                                 g_define_type_id, "portal", 110))

static gboolean
g_portal_notification_backend_is_supported (void)
{
  return glib_should_use_portal ();
}

static void
g_portal_notification_backend_send_notification (GNotificationBackend *backend,
                                                 const gchar          *id,
                                                 GNotification        *notification)
{
  g_dbus_connection_call (backend->dbus_connection,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.Notification",
                          "AddNotification",
                          g_variant_new ("(s@a{sv})",
                                         id,
                                         g_notification_serialize (notification)),
                          G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_portal_notification_backend_withdraw_notification (GNotificationBackend *backend,
                                                     const gchar          *id)
{
  g_dbus_connection_call (backend->dbus_connection,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.Notification",
                          "RemoveNotification",
                          g_variant_new ("(s)", id),
                          G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_portal_notification_backend_init (GPortalNotificationBackend *backend)
{
}

static void
g_portal_notification_backend_class_init (GPortalNotificationBackendClass *class)
{
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS (class);

  backend_class->is_supported = g_portal_notification_backend_is_supported;
  backend_class->send_notification = g_portal_notification_backend_send_notification;
  backend_class->withdraw_notification = g_portal_notification_backend_withdraw_notification;
}
