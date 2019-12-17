/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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

#ifndef __TESTS_H__
#define __TESTS_H__

#include <gio/gio.h>
#include "gdbus-sessionbus.h"

G_BEGIN_DECLS

/* TODO: clean up and move to gtestutils.c
 *
 * This is needed because libdbus-1 does not give predictable error messages - e.g. you
 * get a different error message on connecting to a bus if the socket file is there vs
 * if the socket file is missing.
 */

#define _g_assert_error_domain(err, dom)	do { if (!err || (err)->domain != dom) \
      g_assertion_message_error (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                 #err, err, dom, -1); } while (0)

#define _g_assert_property_notify(object, property_name)                \
  do                                                                    \
    {                                                                   \
      if (!G_IS_OBJECT (object))                                        \
        {                                                               \
          g_assertion_message (G_LOG_DOMAIN,                            \
                               __FILE__,                                \
                               __LINE__,                                \
                               G_STRFUNC,                               \
                               "Not a GObject instance");               \
        }                                                               \
      if (g_object_class_find_property (G_OBJECT_GET_CLASS (object),    \
                                        property_name) == NULL)         \
        {                                                               \
          g_assertion_message (G_LOG_DOMAIN,                            \
                               __FILE__,                                \
                               __LINE__,                                \
                               G_STRFUNC,                               \
                               "Property " property_name " does not "   \
                               "exist on object");                      \
        }                                                               \
      if (_g_assert_property_notify_run (object, property_name))        \
        {                                                               \
          g_assertion_message (G_LOG_DOMAIN,                            \
                               __FILE__,                                \
                               __LINE__,                                \
                               G_STRFUNC,                               \
                               "Timed out waiting for notification "    \
                               "on property " property_name);           \
        }                                                               \
    }                                                                   \
  while (FALSE)

#define _g_assert_signal_received(object, signal_name)                  \
  do                                                                    \
    {                                                                   \
      if (!G_IS_OBJECT (object))                                        \
        {                                                               \
          g_assertion_message (G_LOG_DOMAIN,                            \
                               __FILE__,                                \
                               __LINE__,                                \
                               G_STRFUNC,                               \
                               "Not a GObject instance");               \
        }                                                               \
      if (g_signal_lookup (signal_name,                                 \
                           G_TYPE_FROM_INSTANCE (object)) == 0)         \
        {                                                               \
          g_assertion_message (G_LOG_DOMAIN,                            \
                               __FILE__,                                \
                               __LINE__,                                \
                               G_STRFUNC,                               \
                               "Signal '" signal_name "' does not "     \
                               "exist on object");                      \
        }                                                               \
      if (_g_assert_signal_received_run (object, signal_name))          \
        {                                                               \
          g_assertion_message (G_LOG_DOMAIN,                            \
                               __FILE__,                                \
                               __LINE__,                                \
                               G_STRFUNC,                               \
                               "Timed out waiting for signal '"         \
                               signal_name "'");                        \
        }                                                               \
    }                                                                   \
  while (FALSE)

gboolean _g_assert_property_notify_run (gpointer     object,
                                        const gchar *property_name);


gboolean _g_assert_signal_received_run (gpointer     object,
                                        const gchar *signal_name);

GDBusConnection *_g_bus_get_priv (GBusType            bus_type,
                                  GCancellable       *cancellable,
                                  GError            **error);

void ensure_gdbus_testserver_up (void);

G_END_DECLS

#endif /* __TESTS_H__ */
