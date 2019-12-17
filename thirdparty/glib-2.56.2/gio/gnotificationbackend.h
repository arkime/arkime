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

#ifndef __G_NOTIFICATION_BACKEND_H__
#define __G_NOTIFICATION_BACKEND_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_NOTIFICATION_BACKEND         (g_notification_backend_get_type ())
#define G_NOTIFICATION_BACKEND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NOTIFICATION_BACKEND, GNotificationBackend))
#define G_IS_NOTIFICATION_BACKEND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NOTIFICATION_BACKEND))
#define G_NOTIFICATION_BACKEND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_NOTIFICATION_BACKEND, GNotificationBackendClass))
#define G_NOTIFICATION_BACKEND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_NOTIFICATION_BACKEND, GNotificationBackendClass))

#define G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME "gnotification-backend"

typedef struct _GNotificationBackend      GNotificationBackend;
typedef struct _GNotificationBackendClass GNotificationBackendClass;

struct _GNotificationBackend
{
  GObject parent_instance;

  GApplication    *application;
  GDBusConnection *dbus_connection;
};

struct _GNotificationBackendClass
{
  GObjectClass parent_class;

  gboolean  (*is_supported)             (void);

  void      (*send_notification)        (GNotificationBackend *backend,
                                         const gchar          *id,
                                         GNotification        *notification);

  void      (*withdraw_notification)    (GNotificationBackend *backend,
                                         const gchar          *id);
};

GType                   g_notification_backend_get_type                 (void);

GNotificationBackend *  g_notification_backend_new_default              (GApplication         *application);

void                    g_notification_backend_send_notification        (GNotificationBackend *backend,
                                                                         const gchar          *id,
                                                                         GNotification        *notification);

void                    g_notification_backend_withdraw_notification    (GNotificationBackend *backend,
                                                                         const gchar          *id);

G_END_DECLS

#endif
