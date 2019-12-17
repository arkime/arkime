/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2005 Red Hat, Inc.
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

#undef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN "TestReferences"

#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS

#include	<glib-object.h>

/* This test tests weak and toggle references
 */

static GObject *global_object;

static gboolean object_destroyed;
static gboolean weak_ref1_notified;
static gboolean weak_ref2_notified;
static gboolean toggle_ref1_weakened;
static gboolean toggle_ref1_strengthened;
static gboolean toggle_ref2_weakened;
static gboolean toggle_ref2_strengthened;
static gboolean toggle_ref3_weakened;
static gboolean toggle_ref3_strengthened;

/*
 * TestObject, a parent class for TestObject
 */
static GType test_object_get_type (void);
#define TEST_TYPE_OBJECT          (test_object_get_type ())
typedef struct _TestObject        TestObject;
typedef struct _TestObjectClass   TestObjectClass;

struct _TestObject
{
  GObject parent_instance;
};
struct _TestObjectClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT)

static void
test_object_finalize (GObject *object)
{
  object_destroyed = TRUE;
  
  G_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_class_init (TestObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = test_object_finalize;
}

static void
test_object_init (TestObject *test_object)
{
}

static void
clear_flags (void)
{
  object_destroyed = FALSE;
  weak_ref1_notified = FALSE;
  weak_ref2_notified = FALSE;
  toggle_ref1_weakened = FALSE;
  toggle_ref1_strengthened = FALSE;
  toggle_ref2_weakened = FALSE;
  toggle_ref2_strengthened = FALSE;
  toggle_ref3_weakened = FALSE;
  toggle_ref3_strengthened = FALSE;
}

static void
weak_ref1 (gpointer data,
	   GObject *object)
{
  g_assert (object == global_object);
  g_assert (data == GUINT_TO_POINTER (42));

  weak_ref1_notified = TRUE;
}

static void
weak_ref2 (gpointer data,
	   GObject *object)
{
  g_assert (object == global_object);
  g_assert (data == GUINT_TO_POINTER (24));

  weak_ref2_notified = TRUE;
}

static void
toggle_ref1 (gpointer data,
	     GObject *object,
	     gboolean is_last_ref)
{
  g_assert (object == global_object);
  g_assert (data == GUINT_TO_POINTER (42));

  if (is_last_ref)
    toggle_ref1_weakened = TRUE;
  else
    toggle_ref1_strengthened = TRUE;
}

static void
toggle_ref2 (gpointer data,
	     GObject *object,
	     gboolean is_last_ref)
{
  g_assert (object == global_object);
  g_assert (data == GUINT_TO_POINTER (24));

  if (is_last_ref)
    toggle_ref2_weakened = TRUE;
  else
    toggle_ref2_strengthened = TRUE;
}

static void
toggle_ref3 (gpointer data,
	     GObject *object,
	     gboolean is_last_ref)
{
  g_assert (object == global_object);
  g_assert (data == GUINT_TO_POINTER (34));

  if (is_last_ref)
    {
      toggle_ref3_weakened = TRUE;
      g_object_remove_toggle_ref (object, toggle_ref3, GUINT_TO_POINTER (34));
    }
  else
    toggle_ref3_strengthened = TRUE;
}

int
main (int   argc,
      char *argv[])
{
  GObject *object;
	
  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
			  G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_CRITICAL);

  /* Test basic weak reference operation
   */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);
  
  g_object_weak_ref (object, weak_ref1, GUINT_TO_POINTER (42));

  clear_flags ();
  g_object_unref (object);
  g_assert (weak_ref1_notified == TRUE);
  g_assert (object_destroyed == TRUE);

  /* Test two weak references at once
   */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);
  
  g_object_weak_ref (object, weak_ref1, GUINT_TO_POINTER (42));
  g_object_weak_ref (object, weak_ref2, GUINT_TO_POINTER (24));

  clear_flags ();
  g_object_unref (object);
  g_assert (weak_ref1_notified == TRUE);
  g_assert (weak_ref2_notified == TRUE);
  g_assert (object_destroyed == TRUE);

  /* Test remove weak references
   */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);
  
  g_object_weak_ref (object, weak_ref1, GUINT_TO_POINTER (42));
  g_object_weak_ref (object, weak_ref2, GUINT_TO_POINTER (24));
  g_object_weak_unref (object, weak_ref1, GUINT_TO_POINTER (42));

  clear_flags ();
  g_object_unref (object);
  g_assert (weak_ref1_notified == FALSE);
  g_assert (weak_ref2_notified == TRUE);
  g_assert (object_destroyed == TRUE);

  /* Test basic toggle reference operation
   */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);
  
  g_object_add_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));

  clear_flags ();
  g_object_unref (object);
  g_assert (toggle_ref1_weakened == TRUE);
  g_assert (toggle_ref1_strengthened == FALSE);
  g_assert (object_destroyed == FALSE);

  clear_flags ();
  g_object_ref (object);
  g_assert (toggle_ref1_weakened == FALSE);
  g_assert (toggle_ref1_strengthened == TRUE);
  g_assert (object_destroyed == FALSE);

  g_object_unref (object);

  clear_flags ();
  g_object_remove_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));
  g_assert (toggle_ref1_weakened == FALSE);
  g_assert (toggle_ref1_strengthened == FALSE);
  g_assert (object_destroyed == TRUE);

  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);

  /* Test two toggle references at once
   */
  g_object_add_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));
  g_object_add_toggle_ref (object, toggle_ref2, GUINT_TO_POINTER (24));

  clear_flags ();
  g_object_unref (object);
  g_assert (toggle_ref1_weakened == FALSE);
  g_assert (toggle_ref1_strengthened == FALSE);
  g_assert (toggle_ref2_weakened == FALSE);
  g_assert (toggle_ref2_strengthened == FALSE);
  g_assert (object_destroyed == FALSE);

  clear_flags ();
  g_object_remove_toggle_ref (object, toggle_ref1, GUINT_TO_POINTER (42));
  g_assert (toggle_ref1_weakened == FALSE);
  g_assert (toggle_ref1_strengthened == FALSE);
  g_assert (toggle_ref2_weakened == TRUE);
  g_assert (toggle_ref2_strengthened == FALSE);
  g_assert (object_destroyed == FALSE);

  clear_flags ();
  g_object_remove_toggle_ref (object, toggle_ref2, GUINT_TO_POINTER (24));
  g_assert (toggle_ref1_weakened == FALSE);
  g_assert (toggle_ref1_strengthened == FALSE);
  g_assert (toggle_ref2_weakened == FALSE);
  g_assert (toggle_ref2_strengthened == FALSE);
  g_assert (object_destroyed == TRUE);
  
  /* Test a toggle reference that removes itself
   */
  global_object = object = g_object_new (TEST_TYPE_OBJECT, NULL);
  
  g_object_add_toggle_ref (object, toggle_ref3, GUINT_TO_POINTER (34));

  clear_flags ();
  g_object_unref (object);
  g_assert (toggle_ref3_weakened == TRUE);
  g_assert (toggle_ref3_strengthened == FALSE);
  g_assert (object_destroyed == TRUE);

  return 0;
}
