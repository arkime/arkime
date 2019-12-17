/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#ifndef __G_DBUS_AUTH_H__
#define __G_DBUS_AUTH_H__

#if !defined (GIO_COMPILATION)
#error "gdbusauth.h is a private header file."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_AUTH         (_g_dbus_auth_get_type ())
#define G_DBUS_AUTH(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_AUTH, GDBusAuth))
#define G_DBUS_AUTH_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_AUTH, GDBusAuthClass))
#define G_DBUS_AUTH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_AUTH, GDBusAuthClass))
#define G_IS_DBUS_AUTH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_AUTH))
#define G_IS_DBUS_AUTH_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_AUTH))

typedef struct _GDBusAuth        GDBusAuth;
typedef struct _GDBusAuthClass   GDBusAuthClass;
typedef struct _GDBusAuthPrivate GDBusAuthPrivate;

struct _GDBusAuthClass
{
  /*< private >*/
  GObjectClass parent_class;
};

struct _GDBusAuth
{
  GObject parent_instance;
  GDBusAuthPrivate *priv;
};

GType       _g_dbus_auth_get_type (void) G_GNUC_CONST;
GDBusAuth  *_g_dbus_auth_new      (GIOStream *stream);

/* TODO: need a way to set allowed authentication mechanisms */

/* TODO: need a way to convey credentials etc. */

/* TODO: need a way to convey negotiated features (e.g. returning flags from e.g. GDBusConnectionFeatures) */

/* TODO: need to expose encode()/decode() from the AuthMechanism (and whether it is needed at all) */

gboolean    _g_dbus_auth_run_server (GDBusAuth             *auth,
                                     GDBusAuthObserver     *observer,
                                     const gchar           *guid,
                                     gboolean               allow_anonymous,
                                     GDBusCapabilityFlags   offered_capabilities,
                                     GDBusCapabilityFlags  *out_negotiated_capabilities,
                                     GCredentials         **out_received_credentials,
                                     GCancellable          *cancellable,
                                     GError               **error);

gchar      *_g_dbus_auth_run_client (GDBusAuth     *auth,
                                     GDBusAuthObserver     *observer,
                                     GDBusCapabilityFlags offered_capabilities,
                                     GDBusCapabilityFlags *out_negotiated_capabilities,
                                     GCancellable  *cancellable,
                                     GError       **error);

G_END_DECLS

#endif /* __G_DBUS_AUTH_H__ */
