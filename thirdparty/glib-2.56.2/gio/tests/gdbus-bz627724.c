/* GLib testing framework examples and tests
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

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

#include "gdbus-tests.h"

static GDBusConnection *the_connection = NULL;

#define MY_TYPE_OBJECT   (my_object_get_type ())
#define MY_OBJECT(o)     (G_TYPE_CHECK_INSTANCE_CAST ((o), MY_TYPE_OBJECT, MyObject))
#define MY_IS_OBJECT(o)  (G_TYPE_CHECK_INSTANCE_TYPE ((o), MY_TYPE_OBJECT))

typedef struct {
  GObject parent_instance;
} MyObject;

typedef struct {
  GObjectClass parent_class;
} MyObjectClass;

GType my_object_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MyObject, my_object, G_TYPE_OBJECT)

static void
my_object_init (MyObject *object)
{
}

static void
my_object_class_init (MyObjectClass *klass)
{
  GError *error;
  error = NULL;
  the_connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                   NULL, /* GCancellable* */
                                   &error);
  g_assert_no_error (error);
  g_assert (G_IS_DBUS_CONNECTION (the_connection));
}

static void
test_bz627724 (void)
{
  MyObject *object;

  session_bus_up ();
  g_assert (the_connection == NULL);
  object = g_object_new (MY_TYPE_OBJECT, NULL);
  g_assert (the_connection != NULL);
  g_object_unref (the_connection);
  g_object_unref (object);
  session_bus_down ();
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_dbus_unset ();

  g_test_add_func ("/gdbus/bz627724", test_bz627724);
  return g_test_run();
}
