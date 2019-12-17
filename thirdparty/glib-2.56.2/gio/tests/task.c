/*
 * Copyright 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include <gio/gio.h>
#include <string.h>

static GMainLoop *loop;
static GThread *main_thread;
static gssize magic;

/* We need objects for a few tests where we don't care what type
 * they are, just that they're GObjects.
 */
#define g_dummy_object_new g_socket_client_new

static gboolean
idle_quit_loop (gpointer user_data)
{
  g_main_loop_quit (loop);
  return FALSE;
}

static void
completed_cb (GObject    *gobject,
              GParamSpec *pspec,
              gpointer    user_data)
{
	gboolean *notification_emitted = user_data;
	*notification_emitted = TRUE;
}

static void
wait_for_completed_notification (GTask *task)
{
  gboolean notification_emitted = FALSE;
  gboolean is_completed = FALSE;

  /* Hold a ref. so we can check the :completed property afterwards. */
  g_object_ref (task);

  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);
  g_idle_add (idle_quit_loop, NULL);
  g_main_loop_run (loop);
  g_assert_true (notification_emitted);

  g_assert_true (g_task_get_completed (task));
  g_object_get (G_OBJECT (task), "completed", &is_completed, NULL);
  g_assert_true (is_completed);

  g_object_unref (task);
}

/* test_basic */

static void
basic_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  gssize *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  *result_out = g_task_propagate_int (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static gboolean
basic_return (gpointer user_data)
{
  GTask *task = user_data;

  g_task_return_int (task, magic);
  g_object_unref (task);

  return FALSE;
}

static void
basic_destroy_notify (gpointer user_data)
{
  gboolean *destroyed = user_data;

  *destroyed = TRUE;
}

static void
test_basic (void)
{
  GTask *task;
  gssize result;
  gboolean task_data_destroyed = FALSE;
  gboolean notification_emitted = FALSE;

  task = g_task_new (NULL, NULL, basic_callback, &result);
  g_task_set_task_data (task, &task_data_destroyed, basic_destroy_notify);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_idle_add (basic_return, task);
  g_main_loop_run (loop);

  g_assert_cmpint (result, ==, magic);
  g_assert (task_data_destroyed == TRUE);
  g_assert_true (notification_emitted);
  g_assert (task == NULL);
}

/* test_error */

static void
error_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  gssize *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  *result_out = g_task_propagate_int (G_TASK (result), &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_error_free (error);

  g_assert (g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static gboolean
error_return (gpointer user_data)
{
  GTask *task = user_data;

  g_task_return_new_error (task,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed");
  g_object_unref (task);

  return FALSE;
}

static void
error_destroy_notify (gpointer user_data)
{
  gboolean *destroyed = user_data;

  *destroyed = TRUE;
}

static void
test_error (void)
{
  GTask *task;
  gssize result;
  gboolean first_task_data_destroyed = FALSE;
  gboolean second_task_data_destroyed = FALSE;
  gboolean notification_emitted = FALSE;

  task = g_task_new (NULL, NULL, error_callback, &result);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_assert (first_task_data_destroyed == FALSE);
  g_task_set_task_data (task, &first_task_data_destroyed, error_destroy_notify);
  g_assert (first_task_data_destroyed == FALSE);

  /* Calling g_task_set_task_data() again will destroy the first data */
  g_task_set_task_data (task, &second_task_data_destroyed, error_destroy_notify);
  g_assert (first_task_data_destroyed == TRUE);
  g_assert (second_task_data_destroyed == FALSE);

  g_idle_add (error_return, task);
  g_main_loop_run (loop);

  g_assert_cmpint (result, ==, -1);
  g_assert (second_task_data_destroyed == TRUE);
  g_assert_true (notification_emitted);
  g_assert (task == NULL);
}

/* test_return_from_same_iteration: calling g_task_return_* from the
 * loop iteration the task was created in defers completion until the
 * next iteration.
 */
gboolean same_result = FALSE;
gboolean same_notification_emitted = FALSE;

static void
same_callback (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  gboolean *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  *result_out = g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static gboolean
same_start (gpointer user_data)
{
  gpointer *weak_pointer = user_data;
  GTask *task;

  task = g_task_new (NULL, NULL, same_callback, &same_result);
  *weak_pointer = task;
  g_object_add_weak_pointer (G_OBJECT (task), weak_pointer);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &same_notification_emitted);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);

  /* same_callback should not have been invoked yet */
  g_assert (same_result == FALSE);
  g_assert (*weak_pointer == task);
  g_assert_false (same_notification_emitted);

  return FALSE;
}

static void
test_return_from_same_iteration (void)
{
  gpointer weak_pointer;

  g_idle_add (same_start, &weak_pointer);
  g_main_loop_run (loop);

  g_assert (same_result == TRUE);
  g_assert (weak_pointer == NULL);
  g_assert_true (same_notification_emitted);
}

/* test_return_from_toplevel: calling g_task_return_* from outside any
 * main loop completes the task inside the main loop.
 */
gboolean toplevel_notification_emitted = FALSE;

static void
toplevel_callback (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  gboolean *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  *result_out = g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static void
test_return_from_toplevel (void)
{
  GTask *task;
  gboolean result = FALSE;

  task = g_task_new (NULL, NULL, toplevel_callback, &result);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &toplevel_notification_emitted);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);

  /* toplevel_callback should not have been invoked yet */
  g_assert (result == FALSE);
  g_assert (task != NULL);
  g_assert_false (toplevel_notification_emitted);

  g_main_loop_run (loop);

  g_assert (result == TRUE);
  g_assert (task == NULL);
  g_assert_true (toplevel_notification_emitted);
}

/* test_return_from_anon_thread: calling g_task_return_* from a
 * thread with no thread-default main context will complete the
 * task in the task's context/thread.
 */

gboolean anon_thread_notification_emitted = FALSE;
GThread *anon_thread;

static void
anon_callback (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  gssize *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  g_assert (g_thread_self () == main_thread);

  *result_out = g_task_propagate_int (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static gpointer
anon_thread_func (gpointer user_data)
{
  GTask *task = user_data;

  g_task_return_int (task, magic);
  g_object_unref (task);

  return NULL;
}

static gboolean
anon_start (gpointer user_data)
{
  GTask *task = user_data;

  anon_thread = g_thread_new ("test_return_from_anon_thread",
                              anon_thread_func, task);
  return FALSE;
}

static void
test_return_from_anon_thread (void)
{
  GTask *task;
  gssize result = 0;

  task = g_task_new (NULL, NULL, anon_callback, &result);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb,
                    &anon_thread_notification_emitted);

  g_idle_add (anon_start, task);
  g_main_loop_run (loop);

  g_thread_join (anon_thread);

  g_assert_cmpint (result, ==, magic);
  g_assert (task == NULL);
  g_assert_true (anon_thread_notification_emitted);
}

/* test_return_from_wrong_thread: calling g_task_return_* from a
 * thread with its own thread-default main context will complete the
 * task in the task's context/thread.
 */

gboolean wrong_thread_notification_emitted = FALSE;
GThread *wrong_thread;

static void
wrong_callback (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  gssize *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  g_assert (g_thread_self () == main_thread);

  *result_out = g_task_propagate_int (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static gpointer
wrong_thread_func (gpointer user_data)
{
  GTask *task = user_data;
  GMainContext *context;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  g_assert (g_task_get_context (task) != context);

  g_task_return_int (task, magic);
  g_object_unref (task);

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);

  return NULL;
}

static gboolean
wrong_start (gpointer user_data)
{
  GTask *task = user_data;

  wrong_thread = g_thread_new ("test_return_from_anon_thread",
                               wrong_thread_func, task);
  return FALSE;
}

static void
test_return_from_wrong_thread (void)
{
  GTask *task;
  gssize result = 0;

  task = g_task_new (NULL, NULL, wrong_callback, &result);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb,
                    &wrong_thread_notification_emitted);

  g_idle_add (wrong_start, task);
  g_main_loop_run (loop);

  g_thread_join (wrong_thread);

  g_assert_cmpint (result, ==, magic);
  g_assert (task == NULL);
  g_assert_true (wrong_thread_notification_emitted);
}

/* test_no_callback */

static void
test_no_callback (void)
{
  GTask *task;

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);

  /* Even though there’s no callback, the :completed notification has to
   * happen in an idle handler. */
  g_assert_nonnull (task);
  wait_for_completed_notification (task);
  g_assert_null (task);
}

/* test_report_error */

static void test_report_error (void);
gboolean error_notification_emitted = FALSE;

static void
report_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  gpointer *weak_pointer = user_data;
  GError *error = NULL;
  gssize ret;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (g_async_result_is_tagged (result, test_report_error));
  g_assert (g_task_get_source_tag (G_TASK (result)) == test_report_error);
  g_assert (g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  ret = g_task_propagate_int (G_TASK (result), &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpint (ret, ==, -1);
  g_error_free (error);

  g_assert (g_task_had_error (G_TASK (result)));

  *weak_pointer = result;
  g_object_add_weak_pointer (G_OBJECT (result), weak_pointer);
  g_signal_connect (result, "notify::completed",
                    (GCallback) completed_cb, &error_notification_emitted);

  g_main_loop_quit (loop);
}

static void
test_report_error (void)
{
  gpointer weak_pointer = (gpointer)-1;

  g_task_report_new_error (NULL, report_callback, &weak_pointer,
                           test_report_error,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed");
  g_main_loop_run (loop);

  g_assert (weak_pointer == NULL);
  g_assert_true (error_notification_emitted);
}

/* test_priority: tasks complete in priority order */

static int counter = 0;

static void
priority_callback (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  gssize *ret_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  *ret_out = ++counter;

  if (counter == 3)
    g_main_loop_quit (loop);
}

static void
test_priority (void)
{
  GTask *t1, *t2, *t3;
  gssize ret1, ret2, ret3;

  /* t2 has higher priority than either t1 or t3, so we can't
   * accidentally pass the test just by completing the tasks in the
   * order they were created (or in reverse order).
   */

  t1 = g_task_new (NULL, NULL, priority_callback, &ret1);
  g_task_set_priority (t1, G_PRIORITY_DEFAULT);
  g_task_return_boolean (t1, TRUE);
  g_object_unref (t1);

  t2 = g_task_new (NULL, NULL, priority_callback, &ret2);
  g_task_set_priority (t2, G_PRIORITY_HIGH);
  g_task_return_boolean (t2, TRUE);
  g_object_unref (t2);

  t3 = g_task_new (NULL, NULL, priority_callback, &ret3);
  g_task_set_priority (t3, G_PRIORITY_LOW);
  g_task_return_boolean (t3, TRUE);
  g_object_unref (t3);

  g_main_loop_run (loop);

  g_assert_cmpint (ret2, ==, 1);
  g_assert_cmpint (ret1, ==, 2);
  g_assert_cmpint (ret3, ==, 3);
}

/* test_check_cancellable: cancellation overrides return value */

enum {
  CANCEL_BEFORE     = (1 << 1),
  CANCEL_AFTER      = (1 << 2),
  CHECK_CANCELLABLE = (1 << 3)
};
#define NUM_CANCEL_TESTS (CANCEL_BEFORE | CANCEL_AFTER | CHECK_CANCELLABLE)

static void
cancel_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  int state = GPOINTER_TO_INT (user_data);
  GTask *task;
  GCancellable *cancellable;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);

  task = G_TASK (result);
  cancellable = g_task_get_cancellable (task);
  g_assert (G_IS_CANCELLABLE (cancellable));

  if (state & (CANCEL_BEFORE | CANCEL_AFTER))
    g_assert (g_cancellable_is_cancelled (cancellable));
  else
    g_assert (!g_cancellable_is_cancelled (cancellable));

  if (state & CHECK_CANCELLABLE)
    g_assert (g_task_get_check_cancellable (task));
  else
    g_assert (!g_task_get_check_cancellable (task));

  if (g_task_propagate_boolean (task, &error))
    {
      g_assert (!g_cancellable_is_cancelled (cancellable) ||
                !g_task_get_check_cancellable (task));
    }
  else
    {
      g_assert (g_cancellable_is_cancelled (cancellable) &&
                g_task_get_check_cancellable (task));
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_error_free (error);
    }

  g_main_loop_quit (loop);
}

static void
test_check_cancellable (void)
{
  GTask *task;
  GCancellable *cancellable;
  int state;

  cancellable = g_cancellable_new ();

  for (state = 0; state <= NUM_CANCEL_TESTS; state++)
    {
      task = g_task_new (NULL, cancellable, cancel_callback,
                         GINT_TO_POINTER (state));
      g_task_set_check_cancellable (task, (state & CHECK_CANCELLABLE) != 0);

      if (state & CANCEL_BEFORE)
        g_cancellable_cancel (cancellable);
      g_task_return_boolean (task, TRUE);
      if (state & CANCEL_AFTER)
        g_cancellable_cancel (cancellable);

      g_main_loop_run (loop);
      g_object_unref (task);
      g_cancellable_reset (cancellable);
    }

  g_object_unref (cancellable);
}

/* test_return_if_cancelled */

static void
return_if_cancelled_callback (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);

  g_assert (g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static void
test_return_if_cancelled (void)
{
  GTask *task;
  GCancellable *cancellable;
  gboolean cancelled;
  gboolean notification_emitted = FALSE;

  cancellable = g_cancellable_new ();

  task = g_task_new (NULL, cancellable, return_if_cancelled_callback, NULL);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_cancellable_cancel (cancellable);
  cancelled = g_task_return_error_if_cancelled (task);
  g_assert (cancelled);
  g_assert_false (notification_emitted);
  g_main_loop_run (loop);
  g_object_unref (task);
  g_assert_true (notification_emitted);
  g_cancellable_reset (cancellable);

  notification_emitted = FALSE;

  task = g_task_new (NULL, cancellable, return_if_cancelled_callback, NULL);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_task_set_check_cancellable (task, FALSE);
  g_cancellable_cancel (cancellable);
  cancelled = g_task_return_error_if_cancelled (task);
  g_assert (cancelled);
  g_assert_false (notification_emitted);
  g_main_loop_run (loop);
  g_object_unref (task);
  g_assert_true (notification_emitted);
  g_object_unref (cancellable);
}

/* test_run_in_thread */

static GMutex run_in_thread_mutex;
static GCond run_in_thread_cond;

static void
task_weak_notify (gpointer  user_data,
                  GObject  *ex_task)
{
  gboolean *weak_notify_ran = user_data;

  g_mutex_lock (&run_in_thread_mutex);
  *weak_notify_ran = TRUE;
  g_cond_signal (&run_in_thread_cond);
  g_mutex_unlock (&run_in_thread_mutex);
}

static void
run_in_thread_callback (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  gboolean *done = user_data;
  GError *error = NULL;
  gssize ret;

  g_assert (g_thread_self () == main_thread);

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  ret = g_task_propagate_int (G_TASK (result), &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, magic);

  g_assert (!g_task_had_error (G_TASK (result)));

  *done = TRUE;
  g_main_loop_quit (loop);
}

static void
run_in_thread_thread (GTask        *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  gboolean *thread_ran = task_data;

  g_assert (source_object == g_task_get_source_object (task));
  g_assert (task_data == g_task_get_task_data (task));
  g_assert (cancellable == g_task_get_cancellable (task));
  g_assert_false (g_task_get_completed (task));

  g_assert (g_thread_self () != main_thread);

  g_mutex_lock (&run_in_thread_mutex);
  *thread_ran = TRUE;
  g_cond_signal (&run_in_thread_cond);
  g_mutex_unlock (&run_in_thread_mutex);

  g_task_return_int (task, magic);
}

static void
test_run_in_thread (void)
{
  GTask *task;
  volatile gboolean thread_ran = FALSE;
  volatile gboolean weak_notify_ran = FALSE;
  gboolean notification_emitted = FALSE;
  gboolean done = FALSE;

  task = g_task_new (NULL, NULL, run_in_thread_callback, &done);
  g_object_weak_ref (G_OBJECT (task), task_weak_notify, (gpointer)&weak_notify_ran);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_task_set_task_data (task, (gpointer)&thread_ran, NULL);
  g_task_run_in_thread (task, run_in_thread_thread);
  g_object_unref (task);

  g_mutex_lock (&run_in_thread_mutex);
  while (!thread_ran)
    g_cond_wait (&run_in_thread_cond, &run_in_thread_mutex);
  g_mutex_unlock (&run_in_thread_mutex);

  g_assert (done == FALSE);
  g_assert (weak_notify_ran == FALSE);

  g_main_loop_run (loop);

  g_assert (done == TRUE);
  g_assert_true (notification_emitted);

  g_mutex_lock (&run_in_thread_mutex);
  while (!weak_notify_ran)
    g_cond_wait (&run_in_thread_cond, &run_in_thread_mutex);
  g_mutex_unlock (&run_in_thread_mutex);
}

/* test_run_in_thread_sync */

static void
run_in_thread_sync_callback (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  /* g_task_run_in_thread_sync() does not invoke the task's callback */
  g_assert_not_reached ();
}

static void
run_in_thread_sync_thread (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  gboolean *thread_ran = task_data;

  g_assert (source_object == g_task_get_source_object (task));
  g_assert (task_data == g_task_get_task_data (task));
  g_assert (cancellable == g_task_get_cancellable (task));
  g_assert_false (g_task_get_completed (task));

  g_assert (g_thread_self () != main_thread);

  *thread_ran = TRUE;
  g_task_return_int (task, magic);
}

static void
test_run_in_thread_sync (void)
{
  GTask *task;
  gboolean thread_ran = FALSE;
  gssize ret;
  gboolean notification_emitted = FALSE;
  GError *error = NULL;

  task = g_task_new (NULL, NULL, run_in_thread_sync_callback, NULL);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb,
                    &notification_emitted);

  g_task_set_task_data (task, &thread_ran, NULL);
  g_task_run_in_thread_sync (task, run_in_thread_sync_thread);

  g_assert (thread_ran == TRUE);
  g_assert (task != NULL);
  g_assert (!g_task_had_error (task));
  g_assert_true (g_task_get_completed (task));
  g_assert_true (notification_emitted);

  ret = g_task_propagate_int (task, &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, magic);

  g_assert (!g_task_had_error (task));

  g_object_unref (task);
}

/* test_run_in_thread_priority */

static GMutex fake_task_mutex, last_fake_task_mutex;
static gint sequence_number = 0;

static void
quit_main_loop_callback (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GError *error = NULL;
  gboolean ret;

  g_assert (g_thread_self () == main_thread);

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  ret = g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, TRUE);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static void
set_sequence_number_thread (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  gint *seq_no_p = task_data;

  *seq_no_p = ++sequence_number;
  g_task_return_boolean (task, TRUE);
}

static void
fake_task_thread (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  GMutex *mutex = task_data;

  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);
  g_task_return_boolean (task, TRUE);
}

#define G_TASK_THREAD_POOL_SIZE 10
static int fake_tasks_running;

static void
fake_task_callback (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  if (--fake_tasks_running == 0)
    g_main_loop_quit (loop);
}

static void
clog_up_thread_pool (void)
{
  GTask *task;
  int i;

  g_thread_pool_stop_unused_threads ();

  g_mutex_lock (&fake_task_mutex);
  for (i = 0; i < G_TASK_THREAD_POOL_SIZE - 1; i++)
    {
      task = g_task_new (NULL, NULL, fake_task_callback, NULL);
      g_task_set_task_data (task, &fake_task_mutex, NULL);
      g_assert_cmpint (g_task_get_priority (task), ==, G_PRIORITY_DEFAULT);
      g_task_set_priority (task, G_PRIORITY_HIGH * 2);
      g_assert_cmpint (g_task_get_priority (task), ==, G_PRIORITY_HIGH * 2);
      g_task_run_in_thread (task, fake_task_thread);
      g_object_unref (task);
      fake_tasks_running++;
    }

  g_mutex_lock (&last_fake_task_mutex);
  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_task_data (task, &last_fake_task_mutex, NULL);
  g_task_set_priority (task, G_PRIORITY_HIGH * 2);
  g_task_run_in_thread (task, fake_task_thread);
  g_object_unref (task);
}

static void
unclog_thread_pool (void)
{
  g_mutex_unlock (&fake_task_mutex);
  g_main_loop_run (loop);
}

static void
test_run_in_thread_priority (void)
{
  GTask *task;
  GCancellable *cancellable;
  int seq_a, seq_b, seq_c, seq_d;

  clog_up_thread_pool ();

  /* Queue three more tasks that we'll arrange to have run serially */
  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_task_data (task, &seq_a, NULL);
  g_task_run_in_thread (task, set_sequence_number_thread);
  g_object_unref (task);
  
  task = g_task_new (NULL, NULL, quit_main_loop_callback, NULL);
  g_task_set_task_data (task, &seq_b, NULL);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_run_in_thread (task, set_sequence_number_thread);
  g_object_unref (task);
  
  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_task_data (task, &seq_c, NULL);
  g_task_set_priority (task, G_PRIORITY_HIGH);
  g_task_run_in_thread (task, set_sequence_number_thread);
  g_object_unref (task);
  
  cancellable = g_cancellable_new ();
  task = g_task_new (NULL, cancellable, NULL, NULL);
  g_task_set_task_data (task, &seq_d, NULL);
  g_task_run_in_thread (task, set_sequence_number_thread);
  g_cancellable_cancel (cancellable);
  g_object_unref (cancellable);
  g_object_unref (task);

  /* Let the last fake task complete; the four other tasks will then
   * complete serially, in the order D, C, A, B, and B will quit the
   * main loop.
   */
  g_mutex_unlock (&last_fake_task_mutex);
  g_main_loop_run (loop);

  g_assert_cmpint (seq_d, ==, 1);
  g_assert_cmpint (seq_c, ==, 2);
  g_assert_cmpint (seq_a, ==, 3);
  g_assert_cmpint (seq_b, ==, 4);

  unclog_thread_pool ();
}

/* test_run_in_thread_nested: task threads that block waiting on
 * other task threads will not cause the thread pool to starve.
 */

static void
run_nested_task_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  GTask *nested;
  int *nested_tasks_left = task_data;

  if ((*nested_tasks_left)--)
    {
      nested = g_task_new (NULL, NULL, NULL, NULL);
      g_task_set_task_data (nested, nested_tasks_left, NULL);
      g_task_run_in_thread_sync (nested, run_nested_task_thread);
      g_object_unref (nested);
    }

  g_task_return_boolean (task, TRUE);
}

static void
test_run_in_thread_nested (void)
{
  GTask *task;
  int nested_tasks_left = 2;

  clog_up_thread_pool ();

  task = g_task_new (NULL, NULL, quit_main_loop_callback, NULL);
  g_task_set_task_data (task, &nested_tasks_left, NULL);
  g_task_run_in_thread (task, run_nested_task_thread);
  g_object_unref (task);

  g_mutex_unlock (&last_fake_task_mutex);
  g_main_loop_run (loop);

  unclog_thread_pool ();
}

/* test_run_in_thread_overflow: if you queue lots and lots and lots of
 * tasks, they won't all run at once.
 */
static GMutex overflow_mutex;

static void
run_overflow_task_thread (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  gchar *result = task_data;

  if (g_task_return_error_if_cancelled (task))
    {
      *result = 'X';
      return;
    }

  /* Block until the main thread is ready. */
  g_mutex_lock (&overflow_mutex);
  g_mutex_unlock (&overflow_mutex);

  *result = '.';

  g_task_return_boolean (task, TRUE);
}

#define NUM_OVERFLOW_TASKS 1024

static void
test_run_in_thread_overflow (void)
{
  GCancellable *cancellable;
  GTask *task;
  gchar buf[NUM_OVERFLOW_TASKS + 1];
  gint i;

  /* Queue way too many tasks and then sleep for a bit. The first 10
   * tasks will be dispatched to threads and will then block on
   * overflow_mutex, so more threads will be created while this thread
   * is sleeping. Then we cancel the cancellable, unlock the mutex,
   * wait for all of the tasks to complete, and make sure that we got
   * the behavior we expected.
   */

  memset (buf, 0, sizeof (buf));
  cancellable = g_cancellable_new ();

  g_mutex_lock (&overflow_mutex);

  for (i = 0; i < NUM_OVERFLOW_TASKS; i++)
    {
      task = g_task_new (NULL, cancellable, NULL, NULL);
      g_task_set_task_data (task, buf + i, NULL);
      g_task_run_in_thread (task, run_overflow_task_thread);
      g_object_unref (task);
    }

  if (g_test_slow ())
    g_usleep (5000000); /* 5 s */
  else
    g_usleep (500000);  /* 0.5 s */
  g_cancellable_cancel (cancellable);
  g_object_unref (cancellable);

  g_mutex_unlock (&overflow_mutex);

  /* Wait for all tasks to complete. */
  while (strlen (buf) != NUM_OVERFLOW_TASKS)
    g_usleep (1000);

  i = strspn (buf, ".");
  /* Given the sleep times above, i should be 14 for normal, 40 for
   * slow. But if the machine is too slow/busy then the scheduling
   * might get messed up and we'll get more or fewer threads than
   * expected. But there are limits to how messed up it could
   * plausibly get (and we hope that if gtask is actually broken then
   * it will exceed those limits).
   */
  g_assert_cmpint (i, >=, 10);
  if (g_test_slow ())
    g_assert_cmpint (i, <, 50);
  else
    g_assert_cmpint (i, <, 20);

  g_assert_cmpint (i + strspn (buf + i, "X"), ==, NUM_OVERFLOW_TASKS);
}

/* test_return_on_cancel */

GMutex roc_init_mutex, roc_finish_mutex;
GCond roc_init_cond, roc_finish_cond;

typedef enum {
  THREAD_STARTING,
  THREAD_RUNNING,
  THREAD_CANCELLED,
  THREAD_COMPLETED
} ThreadState;

static void
return_on_cancel_callback (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  gboolean *callback_ran = user_data;
  GError *error = NULL;
  gssize ret;

  g_assert (g_thread_self () == main_thread);

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  ret = g_task_propagate_int (G_TASK (result), &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_cmpint (ret, ==, -1);

  g_assert (g_task_had_error (G_TASK (result)));

  *callback_ran = TRUE;
  g_main_loop_quit (loop);
}

static void
return_on_cancel_thread (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  ThreadState *state = task_data;

  g_assert (source_object == g_task_get_source_object (task));
  g_assert (task_data == g_task_get_task_data (task));
  g_assert (cancellable == g_task_get_cancellable (task));

  g_assert (g_thread_self () != main_thread);

  g_mutex_lock (&roc_init_mutex);
  *state = THREAD_RUNNING;
  g_cond_signal (&roc_init_cond);
  g_mutex_unlock (&roc_init_mutex);

  g_mutex_lock (&roc_finish_mutex);

  if (!g_task_get_return_on_cancel (task) ||
      g_task_set_return_on_cancel (task, FALSE))
    {
      *state = THREAD_COMPLETED;
      g_task_return_int (task, magic);
    }
  else
    *state = THREAD_CANCELLED;

  g_cond_signal (&roc_finish_cond);
  g_mutex_unlock (&roc_finish_mutex);
}

static void
test_return_on_cancel (void)
{
  GTask *task;
  GCancellable *cancellable;
  volatile ThreadState thread_state;
  volatile gboolean weak_notify_ran = FALSE;
  gboolean callback_ran;
  gboolean notification_emitted = FALSE;

  cancellable = g_cancellable_new ();

  /* If return-on-cancel is FALSE (default), the task does not return
   * early.
   */
  callback_ran = FALSE;
  thread_state = THREAD_STARTING;
  task = g_task_new (NULL, cancellable, return_on_cancel_callback, &callback_ran);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_task_set_task_data (task, (gpointer)&thread_state, NULL);
  g_mutex_lock (&roc_init_mutex);
  g_mutex_lock (&roc_finish_mutex);
  g_task_run_in_thread (task, return_on_cancel_thread);
  g_object_unref (task);

  while (thread_state == THREAD_STARTING)
    g_cond_wait (&roc_init_cond, &roc_init_mutex);
  g_mutex_unlock (&roc_init_mutex);

  g_assert (thread_state == THREAD_RUNNING);
  g_assert (callback_ran == FALSE);

  g_cancellable_cancel (cancellable);
  g_mutex_unlock (&roc_finish_mutex);
  g_main_loop_run (loop);

  g_assert (thread_state == THREAD_COMPLETED);
  g_assert (callback_ran == TRUE);
  g_assert_true (notification_emitted);

  g_cancellable_reset (cancellable);

  /* If return-on-cancel is TRUE, it does return early */
  callback_ran = FALSE;
  notification_emitted = FALSE;
  thread_state = THREAD_STARTING;
  task = g_task_new (NULL, cancellable, return_on_cancel_callback, &callback_ran);
  g_object_weak_ref (G_OBJECT (task), task_weak_notify, (gpointer)&weak_notify_ran);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);
  g_task_set_return_on_cancel (task, TRUE);

  g_task_set_task_data (task, (gpointer)&thread_state, NULL);
  g_mutex_lock (&roc_init_mutex);
  g_mutex_lock (&roc_finish_mutex);
  g_task_run_in_thread (task, return_on_cancel_thread);
  g_object_unref (task);

  while (thread_state == THREAD_STARTING)
    g_cond_wait (&roc_init_cond, &roc_init_mutex);
  g_mutex_unlock (&roc_init_mutex);

  g_assert (thread_state == THREAD_RUNNING);
  g_assert (callback_ran == FALSE);

  g_cancellable_cancel (cancellable);
  g_main_loop_run (loop);
  g_assert (thread_state == THREAD_RUNNING);
  g_assert (callback_ran == TRUE);

  g_assert (weak_notify_ran == FALSE);

  while (thread_state == THREAD_RUNNING)
    g_cond_wait (&roc_finish_cond, &roc_finish_mutex);
  g_mutex_unlock (&roc_finish_mutex);

  g_assert (thread_state == THREAD_CANCELLED);
  g_mutex_lock (&run_in_thread_mutex);
  while (!weak_notify_ran)
    g_cond_wait (&run_in_thread_cond, &run_in_thread_mutex);
  g_mutex_unlock (&run_in_thread_mutex);

  g_assert_true (notification_emitted);
  g_cancellable_reset (cancellable);

  /* If the task is already cancelled before it starts, it returns
   * immediately, but the thread func still runs.
   */
  callback_ran = FALSE;
  notification_emitted = FALSE;
  thread_state = THREAD_STARTING;
  task = g_task_new (NULL, cancellable, return_on_cancel_callback, &callback_ran);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);
  g_task_set_return_on_cancel (task, TRUE);

  g_cancellable_cancel (cancellable);

  g_task_set_task_data (task, (gpointer)&thread_state, NULL);
  g_mutex_lock (&roc_init_mutex);
  g_mutex_lock (&roc_finish_mutex);
  g_task_run_in_thread (task, return_on_cancel_thread);
  g_object_unref (task);

  g_main_loop_run (loop);
  g_assert (callback_ran == TRUE);

  while (thread_state == THREAD_STARTING)
    g_cond_wait (&roc_init_cond, &roc_init_mutex);
  g_mutex_unlock (&roc_init_mutex);

  g_assert (thread_state == THREAD_RUNNING);

  while (thread_state == THREAD_RUNNING)
    g_cond_wait (&roc_finish_cond, &roc_finish_mutex);
  g_mutex_unlock (&roc_finish_mutex);

  g_assert (thread_state == THREAD_CANCELLED);
  g_assert_true (notification_emitted);

  g_object_unref (cancellable);
}

/* test_return_on_cancel_sync */

static gpointer
cancel_sync_runner_thread (gpointer task)
{
  g_task_run_in_thread_sync (task, return_on_cancel_thread);
  return NULL;
}

static void
test_return_on_cancel_sync (void)
{
  GTask *task;
  GCancellable *cancellable;
  volatile ThreadState thread_state;
  GThread *runner_thread;
  gssize ret;
  GError *error = NULL;

  cancellable = g_cancellable_new ();

  /* If return-on-cancel is FALSE, the task does not return early.
   */
  thread_state = THREAD_STARTING;
  task = g_task_new (NULL, cancellable, run_in_thread_sync_callback, NULL);

  g_task_set_task_data (task, (gpointer)&thread_state, NULL);
  g_mutex_lock (&roc_init_mutex);
  g_mutex_lock (&roc_finish_mutex);
  runner_thread = g_thread_new ("return-on-cancel-sync runner thread",
                                cancel_sync_runner_thread, task);

  while (thread_state == THREAD_STARTING)
    g_cond_wait (&roc_init_cond, &roc_init_mutex);
  g_mutex_unlock (&roc_init_mutex);

  g_assert (thread_state == THREAD_RUNNING);

  g_cancellable_cancel (cancellable);
  g_mutex_unlock (&roc_finish_mutex);
  g_thread_join (runner_thread);
  g_assert (thread_state == THREAD_COMPLETED);

  ret = g_task_propagate_int (task, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_cmpint (ret, ==, -1);

  g_object_unref (task);

  g_cancellable_reset (cancellable);

  /* If return-on-cancel is TRUE, it does return early */
  thread_state = THREAD_STARTING;
  task = g_task_new (NULL, cancellable, run_in_thread_sync_callback, NULL);
  g_task_set_return_on_cancel (task, TRUE);

  g_task_set_task_data (task, (gpointer)&thread_state, NULL);
  g_mutex_lock (&roc_init_mutex);
  g_mutex_lock (&roc_finish_mutex);
  runner_thread = g_thread_new ("return-on-cancel-sync runner thread",
                                cancel_sync_runner_thread, task);

  while (thread_state == THREAD_STARTING)
    g_cond_wait (&roc_init_cond, &roc_init_mutex);
  g_mutex_unlock (&roc_init_mutex);

  g_assert (thread_state == THREAD_RUNNING);

  g_cancellable_cancel (cancellable);
  g_thread_join (runner_thread);
  g_assert (thread_state == THREAD_RUNNING);

  ret = g_task_propagate_int (task, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_cmpint (ret, ==, -1);

  g_object_unref (task);

  while (thread_state == THREAD_RUNNING)
    g_cond_wait (&roc_finish_cond, &roc_finish_mutex);
  g_mutex_unlock (&roc_finish_mutex);

  g_assert (thread_state == THREAD_CANCELLED);

  g_cancellable_reset (cancellable);

  /* If the task is already cancelled before it starts, it returns
   * immediately, but the thread func still runs.
   */
  thread_state = THREAD_STARTING;
  task = g_task_new (NULL, cancellable, run_in_thread_sync_callback, NULL);
  g_task_set_return_on_cancel (task, TRUE);

  g_cancellable_cancel (cancellable);

  g_task_set_task_data (task, (gpointer)&thread_state, NULL);
  g_mutex_lock (&roc_init_mutex);
  g_mutex_lock (&roc_finish_mutex);
  runner_thread = g_thread_new ("return-on-cancel-sync runner thread",
                                cancel_sync_runner_thread, task);

  g_thread_join (runner_thread);
  g_assert (thread_state == THREAD_STARTING);

  ret = g_task_propagate_int (task, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_cmpint (ret, ==, -1);

  g_object_unref (task);

  while (thread_state == THREAD_STARTING)
    g_cond_wait (&roc_init_cond, &roc_init_mutex);
  g_mutex_unlock (&roc_init_mutex);

  g_assert (thread_state == THREAD_RUNNING);

  while (thread_state == THREAD_RUNNING)
    g_cond_wait (&roc_finish_cond, &roc_finish_mutex);
  g_mutex_unlock (&roc_finish_mutex);

  g_assert (thread_state == THREAD_CANCELLED);

  g_object_unref (cancellable);
}

/* test_return_on_cancel_atomic: turning return-on-cancel on/off is
 * non-racy
 */

GMutex roca_mutex_1, roca_mutex_2;
GCond roca_cond_1, roca_cond_2;

static void
return_on_cancel_atomic_callback (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  gboolean *callback_ran = user_data;
  GError *error = NULL;
  gssize ret;

  g_assert (g_thread_self () == main_thread);

  g_assert (object == NULL);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  ret = g_task_propagate_int (G_TASK (result), &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_cmpint (ret, ==, -1);

  g_assert (g_task_had_error (G_TASK (result)));

  *callback_ran = TRUE;
  g_main_loop_quit (loop);
}

static void
return_on_cancel_atomic_thread (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  gint *state = task_data;

  g_assert (source_object == g_task_get_source_object (task));
  g_assert (task_data == g_task_get_task_data (task));
  g_assert (cancellable == g_task_get_cancellable (task));
  g_assert_false (g_task_get_completed (task));

  g_assert (g_thread_self () != main_thread);
  g_assert_cmpint (*state, ==, 0);

  g_mutex_lock (&roca_mutex_1);
  *state = 1;
  g_cond_signal (&roca_cond_1);
  g_mutex_unlock (&roca_mutex_1);

  g_mutex_lock (&roca_mutex_2);
  if (g_task_set_return_on_cancel (task, FALSE))
    *state = 2;
  else
    *state = 3;
  g_cond_signal (&roca_cond_2);
  g_mutex_unlock (&roca_mutex_2);

  g_mutex_lock (&roca_mutex_1);
  if (g_task_set_return_on_cancel (task, TRUE))
    *state = 4;
  else
    *state = 5;
  g_cond_signal (&roca_cond_1);
  g_mutex_unlock (&roca_mutex_1);

  g_mutex_lock (&roca_mutex_2);
  if (g_task_set_return_on_cancel (task, TRUE))
    *state = 6;
  else
    *state = 7;
  g_cond_signal (&roca_cond_2);
  g_mutex_unlock (&roca_mutex_2);

  g_task_return_int (task, magic);
}

static void
test_return_on_cancel_atomic (void)
{
  GTask *task;
  GCancellable *cancellable;
  volatile gint state;
  gboolean notification_emitted = FALSE;
  gboolean callback_ran;

  cancellable = g_cancellable_new ();
  g_mutex_lock (&roca_mutex_1);
  g_mutex_lock (&roca_mutex_2);

  /* If we don't cancel it, each set_return_on_cancel() call will succeed */
  state = 0;
  callback_ran = FALSE;
  task = g_task_new (NULL, cancellable, return_on_cancel_atomic_callback, &callback_ran);
  g_task_set_return_on_cancel (task, TRUE);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_task_set_task_data (task, (gpointer)&state, NULL);
  g_task_run_in_thread (task, return_on_cancel_atomic_thread);
  g_object_unref (task);

  g_assert_cmpint (state, ==, 0);

  while (state == 0)
    g_cond_wait (&roca_cond_1, &roca_mutex_1);
  g_assert (state == 1);

  while (state == 1)
    g_cond_wait (&roca_cond_2, &roca_mutex_2);
  g_assert (state == 2);

  while (state == 2)
    g_cond_wait (&roca_cond_1, &roca_mutex_1);
  g_assert (state == 4);

  while (state == 4)
    g_cond_wait (&roca_cond_2, &roca_mutex_2);
  g_assert (state == 6);

  /* callback assumes there'll be a cancelled error */
  g_cancellable_cancel (cancellable);

  g_assert (callback_ran == FALSE);
  g_main_loop_run (loop);
  g_assert (callback_ran == TRUE);
  g_assert_true (notification_emitted);

  g_cancellable_reset (cancellable);


  /* If we cancel while it's temporarily not return-on-cancel, the
   * task won't complete right away, and further
   * g_task_set_return_on_cancel() calls will return FALSE.
   */
  state = 0;
  callback_ran = FALSE;
  notification_emitted = FALSE;
  task = g_task_new (NULL, cancellable, return_on_cancel_atomic_callback, &callback_ran);
  g_task_set_return_on_cancel (task, TRUE);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  g_task_set_task_data (task, (gpointer)&state, NULL);
  g_task_run_in_thread (task, return_on_cancel_atomic_thread);
  g_object_unref (task);

  g_assert_cmpint (state, ==, 0);

  while (state == 0)
    g_cond_wait (&roca_cond_1, &roca_mutex_1);
  g_assert (state == 1);
  g_assert (g_task_get_return_on_cancel (task));

  while (state == 1)
    g_cond_wait (&roca_cond_2, &roca_mutex_2);
  g_assert (state == 2);
  g_assert (!g_task_get_return_on_cancel (task));

  g_cancellable_cancel (cancellable);
  g_idle_add (idle_quit_loop, NULL);
  g_main_loop_run (loop);
  g_assert (callback_ran == FALSE);

  while (state == 2)
    g_cond_wait (&roca_cond_1, &roca_mutex_1);
  g_assert (state == 5);
  g_assert (!g_task_get_return_on_cancel (task));

  g_main_loop_run (loop);
  g_assert (callback_ran == TRUE);
  g_assert_true (notification_emitted);

  while (state == 5)
    g_cond_wait (&roca_cond_2, &roca_mutex_2);
  g_assert (state == 7);

  g_object_unref (cancellable);
  g_mutex_unlock (&roca_mutex_1);
  g_mutex_unlock (&roca_mutex_2);
}

/* test_return_pointer: memory management of pointer returns */

static void
test_return_pointer (void)
{
  GObject *object, *ret;
  GTask *task;
  GCancellable *cancellable;
  GError *error = NULL;

  /* If we don't read back the return value, the task will
   * run its destroy notify.
   */
  object = (GObject *)g_dummy_object_new ();
  g_assert_cmpint (object->ref_count, ==, 1);
  g_object_add_weak_pointer (object, (gpointer *)&object);

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_task_return_pointer (task, object, g_object_unref);
  g_assert_cmpint (object->ref_count, ==, 1);

  /* Task and object are reffed until the :completed notification in idle. */
  g_object_unref (task);
  g_assert_nonnull (task);
  g_assert_nonnull (object);

  wait_for_completed_notification (task);

  g_assert_null (task);
  g_assert_null (object);

  /* Likewise, if the return value is overwritten by an error */
  object = (GObject *)g_dummy_object_new ();
  g_assert_cmpint (object->ref_count, ==, 1);
  g_object_add_weak_pointer (object, (gpointer *)&object);

  cancellable = g_cancellable_new ();
  task = g_task_new (NULL, cancellable, NULL, NULL);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_task_return_pointer (task, object, g_object_unref);
  g_assert_cmpint (object->ref_count, ==, 1);
  g_cancellable_cancel (cancellable);
  g_assert_cmpint (object->ref_count, ==, 1);

  ret = g_task_propagate_pointer (task, &error);
  g_assert (ret == NULL);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_cmpint (object->ref_count, ==, 1);

  g_object_unref (task);
  g_object_unref (cancellable);
  g_assert_nonnull (task);
  g_assert_nonnull (object);

  wait_for_completed_notification (task);

  g_assert_null (task);
  g_assert_null (object);
  
  /* If we read back the return value, we steal its ref */
  object = (GObject *)g_dummy_object_new ();
  g_assert_cmpint (object->ref_count, ==, 1);
  g_object_add_weak_pointer (object, (gpointer *)&object);

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_task_return_pointer (task, object, g_object_unref);
  g_assert_cmpint (object->ref_count, ==, 1);

  ret = g_task_propagate_pointer (task, &error);
  g_assert_no_error (error);
  g_assert (ret == object);
  g_assert_cmpint (object->ref_count, ==, 1);

  g_object_unref (task);
  g_assert_nonnull (task);
  g_assert_cmpint (object->ref_count, ==, 1);
  g_object_unref (object);
  g_assert (object == NULL);

  wait_for_completed_notification (task);
  g_assert_null (task);
}

/* test_object_keepalive: GTask takes a ref on its source object */

static GObject *keepalive_object;

static void
keepalive_callback (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  gssize *result_out = user_data;
  GError *error = NULL;

  g_assert (object == keepalive_object);
  g_assert (g_task_is_valid (result, object));
  g_assert (g_async_result_get_user_data (result) == user_data);
  g_assert (!g_task_had_error (G_TASK (result)));
  g_assert_false (g_task_get_completed (G_TASK (result)));

  *result_out = g_task_propagate_int (G_TASK (result), &error);
  g_assert_no_error (error);

  g_assert (!g_task_had_error (G_TASK (result)));

  g_main_loop_quit (loop);
}

static void
test_object_keepalive (void)
{
  GObject *object;
  GTask *task;
  gssize result;
  int ref_count;
  gboolean notification_emitted = FALSE;

  keepalive_object = object = (GObject *)g_dummy_object_new ();
  g_object_add_weak_pointer (object, (gpointer *)&object);

  task = g_task_new (object, NULL, keepalive_callback, &result);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_signal_connect (task, "notify::completed",
                    (GCallback) completed_cb, &notification_emitted);

  ref_count = object->ref_count;
  g_assert_cmpint (ref_count, >, 1);

  g_assert (g_task_get_source_object (task) == object);
  g_assert (g_async_result_get_source_object (G_ASYNC_RESULT (task)) == object);
  g_assert_cmpint (object->ref_count, ==, ref_count + 1);
  g_object_unref (object);

  g_object_unref (object);
  g_assert (object != NULL);

  g_task_return_int (task, magic);
  g_main_loop_run (loop);

  g_assert (object != NULL);
  g_assert_cmpint (result, ==, magic);
  g_assert_true (notification_emitted);

  g_object_unref (task);
  g_assert (task == NULL);
  g_assert (object == NULL);
}

/* test_legacy_error: legacy GSimpleAsyncResult handling */
static void test_legacy_error (void);

static void
legacy_error_callback (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  gssize *result_out = user_data;
  GError *error = NULL;

  g_assert (object == NULL);
  g_assert (g_async_result_is_tagged (result, test_legacy_error));
  g_assert (g_async_result_get_user_data (result) == user_data);

  if (g_async_result_legacy_propagate_error (result, &error))
    {
      g_assert (!g_task_is_valid (result, object));
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_assert (g_simple_async_result_is_valid (result, object, test_legacy_error));
      G_GNUC_END_IGNORE_DEPRECATIONS;

      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
      *result_out = -2;
    }
  else
    {
      g_assert (g_task_is_valid (result, object));

      *result_out = g_task_propagate_int (G_TASK (result), NULL);
      /* Might be error, might not */
    }

  g_main_loop_quit (loop);
}

static gboolean
legacy_error_return (gpointer user_data)
{
  if (G_IS_TASK (user_data))
    {
      GTask *task = user_data;

      g_task_return_int (task, magic);
      g_object_unref (task);
    }
  else
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_simple_async_result_set_error (simple,
                                       G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       "Failed");
      g_simple_async_result_complete (simple);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      g_object_unref (simple);
    }

  return FALSE;
}

static void
test_legacy_error (void)
{
  GTask *task;
  GSimpleAsyncResult *simple;
  gssize result;

  /* GTask success */
  task = g_task_new (NULL, NULL, legacy_error_callback, &result);
  g_task_set_source_tag (task, test_legacy_error);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);

  g_idle_add (legacy_error_return, task);
  g_main_loop_run (loop);

  g_assert_cmpint (result, ==, magic);
  g_assert (task == NULL);

  /* GTask error */
  task = g_task_new (NULL, NULL, legacy_error_callback, &result);
  g_task_set_source_tag (task, test_legacy_error);
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Failed");
  g_object_unref (task);
  g_main_loop_run (loop);

  g_assert_cmpint (result, ==, -1);
  g_assert (task == NULL);

  /* GSimpleAsyncResult error */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  simple = g_simple_async_result_new (NULL, legacy_error_callback, &result,
                                      test_legacy_error);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  g_object_add_weak_pointer (G_OBJECT (simple), (gpointer *)&simple);

  g_idle_add (legacy_error_return, simple);
  g_main_loop_run (loop);

  g_assert_cmpint (result, ==, -2);
  g_assert (simple == NULL);
}


int
main (int argc, char **argv)
{
  int ret;

  g_test_init (&argc, &argv, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  main_thread = g_thread_self ();
  magic = g_get_monotonic_time ();

  g_test_add_func ("/gtask/basic", test_basic);
  g_test_add_func ("/gtask/error", test_error);
  g_test_add_func ("/gtask/return-from-same-iteration", test_return_from_same_iteration);
  g_test_add_func ("/gtask/return-from-toplevel", test_return_from_toplevel);
  g_test_add_func ("/gtask/return-from-anon-thread", test_return_from_anon_thread);
  g_test_add_func ("/gtask/return-from-wrong-thread", test_return_from_wrong_thread);
  g_test_add_func ("/gtask/no-callback", test_no_callback);
  g_test_add_func ("/gtask/report-error", test_report_error);
  g_test_add_func ("/gtask/priority", test_priority);
  g_test_add_func ("/gtask/check-cancellable", test_check_cancellable);
  g_test_add_func ("/gtask/return-if-cancelled", test_return_if_cancelled);
  g_test_add_func ("/gtask/run-in-thread", test_run_in_thread);
  g_test_add_func ("/gtask/run-in-thread-sync", test_run_in_thread_sync);
  g_test_add_func ("/gtask/run-in-thread-priority", test_run_in_thread_priority);
  g_test_add_func ("/gtask/run-in-thread-nested", test_run_in_thread_nested);
  g_test_add_func ("/gtask/run-in-thread-overflow", test_run_in_thread_overflow);
  g_test_add_func ("/gtask/return-on-cancel", test_return_on_cancel);
  g_test_add_func ("/gtask/return-on-cancel-sync", test_return_on_cancel_sync);
  g_test_add_func ("/gtask/return-on-cancel-atomic", test_return_on_cancel_atomic);
  g_test_add_func ("/gtask/return-pointer", test_return_pointer);
  g_test_add_func ("/gtask/object-keepalive", test_object_keepalive);
  g_test_add_func ("/gtask/legacy-error", test_legacy_error);

  ret = g_test_run();

  g_main_loop_unref (loop);

  return ret;
}
