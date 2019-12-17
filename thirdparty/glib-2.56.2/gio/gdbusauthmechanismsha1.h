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

#ifndef __G_DBUS_AUTH_MECHANISM_SHA1_H__
#define __G_DBUS_AUTH_MECHANISM_SHA1_H__

#if !defined (GIO_COMPILATION)
#error "gdbusauthmechanismsha1.h is a private header file."
#endif

#include <gio/giotypes.h>
#include <gio/gdbusauthmechanism.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_AUTH_MECHANISM_SHA1         (_g_dbus_auth_mechanism_sha1_get_type ())
#define G_DBUS_AUTH_MECHANISM_SHA1(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_AUTH_MECHANISM_SHA1, GDBusAuthMechanismSha1))
#define G_DBUS_AUTH_MECHANISM_SHA1_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_AUTH_MECHANISM_SHA1, GDBusAuthMechanismSha1Class))
#define G_DBUS_AUTH_MECHANISM_SHA1_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_AUTH_MECHANISM_SHA1, GDBusAuthMechanismSha1Class))
#define G_IS_DBUS_AUTH_MECHANISM_SHA1(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_AUTH_MECHANISM_SHA1))
#define G_IS_DBUS_AUTH_MECHANISM_SHA1_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_AUTH_MECHANISM_SHA1))

typedef struct _GDBusAuthMechanismSha1        GDBusAuthMechanismSha1;
typedef struct _GDBusAuthMechanismSha1Class   GDBusAuthMechanismSha1Class;
typedef struct _GDBusAuthMechanismSha1Private GDBusAuthMechanismSha1Private;

struct _GDBusAuthMechanismSha1Class
{
  /*< private >*/
  GDBusAuthMechanismClass parent_class;
};

struct _GDBusAuthMechanismSha1
{
  GDBusAuthMechanism parent_instance;
  GDBusAuthMechanismSha1Private *priv;
};

GType _g_dbus_auth_mechanism_sha1_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __G_DBUS_AUTH_MECHANISM_SHA1_H__ */
