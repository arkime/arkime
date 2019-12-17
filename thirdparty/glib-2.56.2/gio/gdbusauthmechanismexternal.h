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

#ifndef __G_DBUS_AUTH_MECHANISM_EXTERNAL_H__
#define __G_DBUS_AUTH_MECHANISM_EXTERNAL_H__

#if !defined (GIO_COMPILATION)
#error "gdbusauthmechanismexternal.h is a private header file."
#endif

#include <gio/giotypes.h>
#include <gio/gdbusauthmechanism.h>

G_BEGIN_DECLS

#define G_TYPE_DBUS_AUTH_MECHANISM_EXTERNAL         (_g_dbus_auth_mechanism_external_get_type ())
#define G_DBUS_AUTH_MECHANISM_EXTERNAL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DBUS_AUTH_MECHANISM_EXTERNAL, GDBusAuthMechanismExternal))
#define G_DBUS_AUTH_MECHANISM_EXTERNAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DBUS_AUTH_MECHANISM_EXTERNAL, GDBusAuthMechanismExternalClass))
#define G_DBUS_AUTH_MECHANISM_EXTERNAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DBUS_AUTH_MECHANISM_EXTERNAL, GDBusAuthMechanismExternalClass))
#define G_IS_DBUS_AUTH_MECHANISM_EXTERNAL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DBUS_AUTH_MECHANISM_EXTERNAL))
#define G_IS_DBUS_AUTH_MECHANISM_EXTERNAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DBUS_AUTH_MECHANISM_EXTERNAL))

typedef struct _GDBusAuthMechanismExternal        GDBusAuthMechanismExternal;
typedef struct _GDBusAuthMechanismExternalClass   GDBusAuthMechanismExternalClass;
typedef struct _GDBusAuthMechanismExternalPrivate GDBusAuthMechanismExternalPrivate;

struct _GDBusAuthMechanismExternalClass
{
  /*< private >*/
  GDBusAuthMechanismClass parent_class;
};

struct _GDBusAuthMechanismExternal
{
  GDBusAuthMechanism parent_instance;
  GDBusAuthMechanismExternalPrivate *priv;
};

GType _g_dbus_auth_mechanism_external_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __G_DBUS_AUTH_MECHANISM_EXTERNAL_H__ */
