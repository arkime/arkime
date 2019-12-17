/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2013 Red Hat, Inc.
 * Copy and pasted from accumulator.c and modified.
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
 */

#undef        G_LOG_DOMAIN
#define       G_LOG_DOMAIN "TestSignals"

#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS

#include <glib-object.h>

#include "testcommon.h"

/* What this test tests is the behavior of signal disconnection
 * from within a signal handler for the signal being disconnected.
 *
 * The test demonstrates that signal handlers disconnected from a signal
 * from an earlier handler in the same emission will not be run.
 *
 * It also demonstrates that signal handlers connected from a signal
 * from an earlier handler in the same emission will not be run.
 */

/*
 * TestObject, a parent class for TestObject
 */
#define TEST_TYPE_OBJECT         (test_object_get_type ())
typedef struct _TestObject        TestObject;
typedef struct _TestObjectClass   TestObjectClass;
static gboolean callback1_ran = FALSE, callback2_ran = FALSE, callback3_ran = FALSE, default_handler_ran = FALSE;

struct _TestObject
{
  GObject parent_instance;
};
struct _TestObjectClass
{
  GObjectClass parent_class;

  void   (*test_signal) (TestObject *object);
};

static GType test_object_get_type (void);

static void
test_object_real_signal (TestObject *object)
{
  default_handler_ran = TRUE;
}

static void
test_object_signal_callback3 (TestObject *object,
                              gpointer    data)
{
  callback3_ran = TRUE;
}

static void
test_object_signal_callback2 (TestObject *object,
                              gpointer    data)
{
  callback2_ran = TRUE;
}

static void
test_object_signal_callback1 (TestObject *object,
                              gpointer    data)
{
  callback1_ran = TRUE;
  g_signal_handlers_disconnect_by_func (G_OBJECT (object),
                                        test_object_signal_callback2,
                                        data);
  g_signal_connect (object, "test-signal",
                    G_CALLBACK (test_object_signal_callback3), NULL);
}

static void
test_object_class_init (TestObjectClass *class)
{
  class->test_signal = test_object_real_signal;

  g_signal_new ("test-signal",
                G_OBJECT_CLASS_TYPE (class),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestObjectClass, test_signal),
                NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static DEFINE_TYPE(TestObject, test_object,
                   test_object_class_init, NULL, NULL,
                   G_TYPE_OBJECT)

int
main (int   argc,
      char *argv[])
{
  TestObject *object;

  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
                          G_LOG_LEVEL_WARNING |
                          G_LOG_LEVEL_CRITICAL);

  object = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_signal_connect (object, "test-signal",
                    G_CALLBACK (test_object_signal_callback1), NULL);
  g_signal_connect (object, "test-signal",
                    G_CALLBACK (test_object_signal_callback2), NULL);
  g_signal_emit_by_name (object, "test-signal");

  g_assert (callback1_ran);
  g_assert (!callback2_ran);
  g_assert (!callback3_ran);
  g_assert (default_handler_ran);

  g_object_unref (object);
  return 0;
}
