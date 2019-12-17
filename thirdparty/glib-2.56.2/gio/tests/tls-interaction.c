/* GLib testing framework examples and tests
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * Author: Stef Walter <stefw@collobora.co.uk>
 */

#include "config.h"

#include <gio/gio.h>

#include "gtesttlsbackend.h"

static GPtrArray *fixtures = NULL;

typedef struct {
  /* Class virtual interaction methods */
  gpointer ask_password_func;
  gpointer ask_password_async_func;
  gpointer ask_password_finish_func;
  gpointer request_certificate_func;
  gpointer request_certificate_async_func;
  gpointer request_certificate_finish_func;

  /* Expected results */
  GTlsInteractionResult result;
  GQuark error_domain;
  gint error_code;
  const gchar *error_message;
} Fixture;

typedef struct {
  GTlsInteraction *interaction;
  GTlsPassword *password;
  GTlsConnection *connection;
  GMainLoop *loop;
  GThread *interaction_thread;
  GThread *test_thread;
  GThread *loop_thread;
  const Fixture *fixture;
} Test;

typedef struct {
  GTlsInteraction parent;
  Test *test;
} TestInteraction;

typedef struct {
  GTlsInteractionClass parent;
} TestInteractionClass;

static GType test_interaction_get_type (void);
G_DEFINE_TYPE (TestInteraction, test_interaction, G_TYPE_TLS_INTERACTION)

#define TEST_TYPE_INTERACTION         (test_interaction_get_type ())
#define TEST_INTERACTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_INTERACTION, TestInteraction))
#define TEST_IS_INTERACTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_INTERACTION))

static void
test_interaction_init (TestInteraction *self)
{

}

static void
test_interaction_class_init (TestInteractionClass *klass)
{
  /* By default no virtual methods */
}

static void
test_interaction_ask_password_async_success (GTlsInteraction    *interaction,
                                             GTlsPassword       *password,
                                             GCancellable       *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer            user_data)
{
  GTask *task;
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_PASSWORD (password));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  /* Don't do this in real life. Include a null terminator for testing */
  g_tls_password_set_value (password, (const guchar *)"the password", 13);
  g_task_return_int (task, G_TLS_INTERACTION_HANDLED);
  g_object_unref (task);
}


static GTlsInteractionResult
test_interaction_ask_password_finish_success (GTlsInteraction    *interaction,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (g_task_is_valid (result, interaction));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
test_interaction_ask_password_async_failure (GTlsInteraction    *interaction,
                                             GTlsPassword       *password,
                                             GCancellable       *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer            user_data)
{
  GTask *task;
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_PASSWORD (password));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  g_task_return_new_error (task, G_FILE_ERROR, G_FILE_ERROR_ACCES, "The message");
  g_object_unref (task);
}

static GTlsInteractionResult
test_interaction_ask_password_finish_failure (GTlsInteraction    *interaction,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (g_task_is_valid (result, interaction));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  if (g_task_propagate_int (G_TASK (result), error) != -1)
    g_assert_not_reached ();

  return G_TLS_INTERACTION_FAILED;
}


static GTlsInteractionResult
test_interaction_ask_password_sync_success (GTlsInteraction    *interaction,
                                            GTlsPassword       *password,
                                            GCancellable       *cancellable,
                                            GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_PASSWORD (password));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  /* Don't do this in real life. Include a null terminator for testing */
  g_tls_password_set_value (password, (const guchar *)"the password", 13);
  return G_TLS_INTERACTION_HANDLED;
}

static GTlsInteractionResult
test_interaction_ask_password_sync_failure (GTlsInteraction    *interaction,
                                            GTlsPassword       *password,
                                            GCancellable       *cancellable,
                                            GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_PASSWORD (password));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_ACCES, "The message");
  return G_TLS_INTERACTION_FAILED;
}

static void
test_interaction_request_certificate_async_success (GTlsInteraction    *interaction,
                                                    GTlsConnection     *connection,
                                                    gint                unused_flags,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data)
{
  GTask *task;
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_CONNECTION (connection));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (unused_flags == 0);

  task = g_task_new (self, cancellable, callback, user_data);

  /*
   * IRL would call g_tls_connection_set_certificate(). But here just touch
   * the connection in a detectable way.
   */
  g_object_set_data (G_OBJECT (connection), "chosen-certificate", "my-certificate");
  g_task_return_int (task, G_TLS_INTERACTION_HANDLED);
  g_object_unref (task);
}

static GTlsInteractionResult
test_interaction_request_certificate_finish_success (GTlsInteraction    *interaction,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (g_task_is_valid (result, interaction));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
test_interaction_request_certificate_async_failure (GTlsInteraction    *interaction,
                                                    GTlsConnection     *connection,
                                                    gint                unused_flags,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data)
{
  GTask *task;
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_CONNECTION (connection));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (unused_flags == 0);

  task = g_task_new (self, cancellable, callback, user_data);

  g_task_return_new_error (task, G_FILE_ERROR, G_FILE_ERROR_NOENT, "Another message");
  g_object_unref (task);
}

static GTlsInteractionResult
test_interaction_request_certificate_finish_failure (GTlsInteraction    *interaction,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (g_task_is_valid (result, interaction));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  if (g_task_propagate_int (G_TASK (result), error) != -1)
    g_assert_not_reached ();

  return G_TLS_INTERACTION_FAILED;
}

static GTlsInteractionResult
test_interaction_request_certificate_sync_success (GTlsInteraction    *interaction,
                                                   GTlsConnection      *connection,
                                                   gint                 unused_flags,
                                                   GCancellable        *cancellable,
                                                   GError             **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_CONNECTION (connection));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error != NULL);
  g_assert (*error == NULL);

  /*
   * IRL would call g_tls_connection_set_certificate(). But here just touch
   * the connection in a detectable way.
   */
  g_object_set_data (G_OBJECT (connection), "chosen-certificate", "my-certificate");
  return G_TLS_INTERACTION_HANDLED;
}

static GTlsInteractionResult
test_interaction_request_certificate_sync_failure (GTlsInteraction    *interaction,
                                                   GTlsConnection     *connection,
                                                   gint                unused_flags,
                                                   GCancellable       *cancellable,
                                                   GError            **error)
{
  TestInteraction *self;

  g_assert (TEST_IS_INTERACTION (interaction));
  self = TEST_INTERACTION (interaction);

  g_assert (g_thread_self () == self->test->interaction_thread);

  g_assert (G_IS_TLS_CONNECTION (connection));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (unused_flags == 0);
  g_assert (error != NULL);
  g_assert (*error == NULL);

  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "Another message");
  return G_TLS_INTERACTION_FAILED;
}

/* ----------------------------------------------------------------------------
 * ACTUAL TESTS
 */

static void
on_ask_password_async_call (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  Test *test = user_data;
  GTlsInteractionResult res;
  GError *error = NULL;

  g_assert (G_IS_TLS_INTERACTION (source));
  g_assert (G_TLS_INTERACTION (source) == test->interaction);

  /* Check that this callback is being run in the right place */
  g_assert (g_thread_self () == test->interaction_thread);

  res = g_tls_interaction_ask_password_finish (test->interaction, result,
                                               &error);

  /* Check that the results match the fixture */
  g_assert_cmpuint (test->fixture->result, ==, res);
  switch (test->fixture->result)
    {
      case G_TLS_INTERACTION_HANDLED:
        g_assert_no_error (error);
        g_assert_cmpstr ((const gchar *)g_tls_password_get_value (test->password, NULL), ==, "the password");
        break;
      case G_TLS_INTERACTION_FAILED:
        g_assert_error (error, test->fixture->error_domain, test->fixture->error_code);
        g_assert_cmpstr (error->message, ==, test->fixture->error_message);
        g_clear_error (&error);
        break;
      case G_TLS_INTERACTION_UNHANDLED:
        g_assert_no_error (error);
        break;
      default:
        g_assert_not_reached ();
    }

  /* Signal the end of the test */
  g_main_loop_quit (test->loop);
}

static void
test_ask_password_async (Test            *test,
                         gconstpointer    unused)
{
  /* This test only works with a main loop */
  g_assert (test->loop);

  g_tls_interaction_ask_password_async (test->interaction,
                                        test->password, NULL,
                                        on_ask_password_async_call,
                                        test);

  /* teardown waits until g_main_loop_quit(). called from callback */
}

static void
test_invoke_ask_password (Test         *test,
                          gconstpointer unused)
{
  GTlsInteractionResult res;
  GError *error = NULL;

  res = g_tls_interaction_invoke_ask_password (test->interaction, test->password,
                                               NULL, &error);

  /* Check that the results match the fixture */
  g_assert_cmpuint (test->fixture->result, ==, res);
  switch (test->fixture->result)
    {
      case G_TLS_INTERACTION_HANDLED:
        g_assert_no_error (error);
        g_assert_cmpstr ((const gchar *)g_tls_password_get_value (test->password, NULL), ==, "the password");
        break;
      case G_TLS_INTERACTION_FAILED:
        g_assert_error (error, test->fixture->error_domain, test->fixture->error_code);
        g_assert_cmpstr (error->message, ==, test->fixture->error_message);
        g_clear_error (&error);
        break;
      case G_TLS_INTERACTION_UNHANDLED:
        g_assert_no_error (error);
        break;
      default:
        g_assert_not_reached ();
    }

  /* This allows teardown to stop if running with loop */
  if (test->loop)
    g_main_loop_quit (test->loop);
}

static void
test_ask_password (Test         *test,
                   gconstpointer unused)
{
  GTlsInteractionResult res;
  GError *error = NULL;

  res = g_tls_interaction_ask_password (test->interaction, test->password,
                                        NULL, &error);

  /* Check that the results match the fixture */
  g_assert_cmpuint (test->fixture->result, ==, res);
  switch (test->fixture->result)
    {
      case G_TLS_INTERACTION_HANDLED:
        g_assert_no_error (error);
        g_assert_cmpstr ((const gchar *)g_tls_password_get_value (test->password, NULL), ==, "the password");
        break;
      case G_TLS_INTERACTION_FAILED:
        g_assert_error (error, test->fixture->error_domain, test->fixture->error_code);
        g_assert_cmpstr (error->message, ==, test->fixture->error_message);
        g_clear_error (&error);
        break;
      case G_TLS_INTERACTION_UNHANDLED:
        g_assert_no_error (error);
        break;
      default:
        g_assert_not_reached ();
    }

  /* This allows teardown to stop if running with loop */
  if (test->loop)
    g_main_loop_quit (test->loop);
}

static void
on_request_certificate_async_call (GObject      *source,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  Test *test = user_data;
  GTlsInteractionResult res;
  GError *error = NULL;

  g_assert (G_IS_TLS_INTERACTION (source));
  g_assert (G_TLS_INTERACTION (source) == test->interaction);

  /* Check that this callback is being run in the right place */
  g_assert (g_thread_self () == test->interaction_thread);

  res = g_tls_interaction_request_certificate_finish (test->interaction, result, &error);

  /* Check that the results match the fixture */
  g_assert_cmpuint (test->fixture->result, ==, res);
  switch (test->fixture->result)
    {
      case G_TLS_INTERACTION_HANDLED:
        g_assert_no_error (error);
        g_assert_cmpstr (g_object_get_data (G_OBJECT (test->connection), "chosen-certificate"), ==, "my-certificate");
        break;
      case G_TLS_INTERACTION_FAILED:
        g_assert_error (error, test->fixture->error_domain, test->fixture->error_code);
        g_assert_cmpstr (error->message, ==, test->fixture->error_message);
        g_clear_error (&error);
        break;
      case G_TLS_INTERACTION_UNHANDLED:
        g_assert_no_error (error);
        break;
      default:
        g_assert_not_reached ();
    }

  /* Signal the end of the test */
  g_main_loop_quit (test->loop);
}

static void
test_request_certificate_async (Test            *test,
                                gconstpointer    unused)
{
  /* This test only works with a main loop */
  g_assert (test->loop);

  g_tls_interaction_request_certificate_async (test->interaction,
                                               test->connection, 0, NULL,
                                               on_request_certificate_async_call,
                                               test);

  /* teardown waits until g_main_loop_quit(). called from callback */
}

static void
test_invoke_request_certificate (Test         *test,
                                 gconstpointer unused)
{
  GTlsInteractionResult res;
  GError *error = NULL;

  res = g_tls_interaction_invoke_request_certificate (test->interaction,
                                                      test->connection,
                                                      0, NULL, &error);

  /* Check that the results match the fixture */
  g_assert_cmpuint (test->fixture->result, ==, res);
  switch (test->fixture->result)
    {
      case G_TLS_INTERACTION_HANDLED:
        g_assert_no_error (error);
        g_assert_cmpstr (g_object_get_data (G_OBJECT (test->connection), "chosen-certificate"), ==, "my-certificate");
        break;
      case G_TLS_INTERACTION_FAILED:
        g_assert_error (error, test->fixture->error_domain, test->fixture->error_code);
        g_assert_cmpstr (error->message, ==, test->fixture->error_message);
        g_clear_error (&error);
        break;
      case G_TLS_INTERACTION_UNHANDLED:
        g_assert_no_error (error);
        break;
      default:
        g_assert_not_reached ();
    }

  /* This allows teardown to stop if running with loop */
  if (test->loop)
    g_main_loop_quit (test->loop);
}

static void
test_request_certificate (Test         *test,
                          gconstpointer unused)
{
  GTlsInteractionResult res;
  GError *error = NULL;

  res = g_tls_interaction_request_certificate (test->interaction, test->connection,
                                               0, NULL, &error);

  /* Check that the results match the fixture */
  g_assert_cmpuint (test->fixture->result, ==, res);
  switch (test->fixture->result)
    {
      case G_TLS_INTERACTION_HANDLED:
        g_assert_no_error (error);
        g_assert_cmpstr (g_object_get_data (G_OBJECT (test->connection), "chosen-certificate"), ==, "my-certificate");
        break;
      case G_TLS_INTERACTION_FAILED:
        g_assert_error (error, test->fixture->error_domain, test->fixture->error_code);
        g_assert_cmpstr (error->message, ==, test->fixture->error_message);
        g_clear_error (&error);
        break;
      case G_TLS_INTERACTION_UNHANDLED:
        g_assert_no_error (error);
        break;
      default:
        g_assert_not_reached ();
    }

  /* This allows teardown to stop if running with loop */
  if (test->loop)
    g_main_loop_quit (test->loop);
}

/* ----------------------------------------------------------------------------
 * TEST SETUP
 */

static void
setup_without_loop (Test           *test,
                    gconstpointer   user_data)
{
  const Fixture *fixture = user_data;
  GTlsInteractionClass *klass;
  GTlsBackend *backend;
  GError *error = NULL;

  test->fixture = fixture;

  test->interaction = g_object_new (TEST_TYPE_INTERACTION, NULL);
  g_assert (TEST_IS_INTERACTION (test->interaction));

  TEST_INTERACTION (test->interaction)->test = test;

  klass =  G_TLS_INTERACTION_GET_CLASS (test->interaction);
  klass->ask_password = fixture->ask_password_func;
  klass->ask_password_async = fixture->ask_password_async_func;
  klass->ask_password_finish = fixture->ask_password_finish_func;
  klass->request_certificate = fixture->request_certificate_func;
  klass->request_certificate_async = fixture->request_certificate_async_func;
  klass->request_certificate_finish = fixture->request_certificate_finish_func;

  backend = g_object_new (G_TYPE_TEST_TLS_BACKEND, NULL);
  test->connection = g_object_new (g_tls_backend_get_server_connection_type (backend), NULL);
  g_assert_no_error (error);
  g_object_unref (backend);

  test->password = g_tls_password_new (0, "Description");
  test->test_thread = g_thread_self ();

  /*
   * If no loop is running then interaction should happen in the same
   * thread that the tests are running in.
   */
  test->interaction_thread = test->test_thread;
}

static void
teardown_without_loop (Test            *test,
                       gconstpointer    unused)
{
  gpointer weak_pointer = test->interaction;

  g_object_add_weak_pointer (weak_pointer, &weak_pointer);

  g_object_unref (test->connection);

  g_object_unref (test->password);

  g_object_unref (test->interaction);

  g_assert (weak_pointer == NULL);

}

typedef struct {
  GMutex loop_mutex;
  GCond loop_started;
  gboolean started;
  Test *test;
} ThreadLoop;

static gpointer
thread_loop (gpointer user_data)
{
  GMainContext *context = g_main_context_default ();
  ThreadLoop *closure = user_data;
  Test *test = closure->test;

  g_mutex_lock (&closure->loop_mutex);

  g_assert (test->loop_thread == g_thread_self ());
  g_assert (test->loop == NULL);
  test->loop = g_main_loop_new (context, TRUE);

  g_main_context_acquire (context);
  closure->started = TRUE;
  g_cond_signal (&closure->loop_started);
  g_mutex_unlock (&closure->loop_mutex);

  while (g_main_loop_is_running (test->loop))
    g_main_context_iteration (context, TRUE);

  g_main_context_release (context);
  return test;
}

static void
setup_with_thread_loop (Test            *test,
                        gconstpointer    user_data)
{
  ThreadLoop closure;

  setup_without_loop (test, user_data);

  g_mutex_init (&closure.loop_mutex);
  g_cond_init (&closure.loop_started);
  closure.started = FALSE;
  closure.test = test;

  g_mutex_lock (&closure.loop_mutex);
  test->loop_thread = g_thread_new ("loop", thread_loop, &closure);
  while (!closure.started)
    g_cond_wait (&closure.loop_started, &closure.loop_mutex);
  g_mutex_unlock (&closure.loop_mutex);

  /*
   * When a loop is running then interaction should always occur in the main
   * context of that loop.
   */
  test->interaction_thread = test->loop_thread;

  g_mutex_clear (&closure.loop_mutex);
  g_cond_clear (&closure.loop_started);
}

static void
teardown_with_thread_loop (Test            *test,
                           gconstpointer    unused)
{
  gpointer check;

  g_assert (test->loop_thread);
  check = g_thread_join (test->loop_thread);
  g_assert (check == test);
  test->loop_thread = NULL;

  g_main_loop_unref (test->loop);

  teardown_without_loop (test, unused);
}

static void
setup_with_normal_loop (Test            *test,
                        gconstpointer    user_data)
{
  GMainContext *context;

  setup_without_loop (test, user_data);

  context = g_main_context_default ();
  if (!g_main_context_acquire (context))
    g_assert_not_reached ();

  test->loop = g_main_loop_new (context, TRUE);
  g_assert (g_main_loop_is_running (test->loop));
}

static void
teardown_with_normal_loop (Test            *test,
                           gconstpointer    unused)
{
  GMainContext *context;

  context = g_main_context_default ();
  while (g_main_loop_is_running (test->loop))
    g_main_context_iteration (context, TRUE);

  g_main_context_release (context);

  /* Run test until complete */
  g_main_loop_unref (test->loop);
  test->loop = NULL;

  teardown_without_loop (test, unused);
}

typedef void (*TestFunc) (Test *test, gconstpointer data);

static void
test_with_async_ask_password (const gchar *name,
                              TestFunc     setup,
                              TestFunc     func,
                              TestFunc     teardown)
{
  gchar *test_name;
  Fixture *fixture;

  /* Async implementation that succeeds */
  fixture = g_new0 (Fixture, 1);
  fixture->ask_password_async_func = test_interaction_ask_password_async_success;
  fixture->ask_password_finish_func = test_interaction_ask_password_finish_success;
  fixture->ask_password_func = NULL;
  fixture->result = G_TLS_INTERACTION_HANDLED;
  test_name = g_strdup_printf ("%s/async-implementation-success", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);

  /* Async implementation that fails */
  fixture = g_new0 (Fixture, 1);
  fixture->ask_password_async_func = test_interaction_ask_password_async_failure;
  fixture->ask_password_finish_func = test_interaction_ask_password_finish_failure;
  fixture->ask_password_func = NULL;
  fixture->result = G_TLS_INTERACTION_FAILED;
  fixture->error_domain = G_FILE_ERROR;
  fixture->error_code = G_FILE_ERROR_ACCES;
  fixture->error_message = "The message";
  test_name = g_strdup_printf ("%s/async-implementation-failure", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);
}

static void
test_with_unhandled_ask_password (const gchar *name,
                                  TestFunc     setup,
                                  TestFunc     func,
                                  TestFunc     teardown)
{
  gchar *test_name;
  Fixture *fixture;

  /* Unhandled implementation */
  fixture = g_new0 (Fixture, 1);
  fixture->ask_password_async_func = NULL;
  fixture->ask_password_finish_func = NULL;
  fixture->ask_password_func = NULL;
  fixture->result = G_TLS_INTERACTION_UNHANDLED;
  test_name = g_strdup_printf ("%s/unhandled-implementation", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);
}

static void
test_with_sync_ask_password (const gchar *name,
                                             TestFunc     setup,
                                             TestFunc     func,
                                             TestFunc     teardown)
{
  gchar *test_name;
  Fixture *fixture;

  /* Sync implementation that succeeds */
  fixture = g_new0 (Fixture, 1);
  fixture->ask_password_async_func = NULL;
  fixture->ask_password_finish_func = NULL;
  fixture->ask_password_func = test_interaction_ask_password_sync_success;
  fixture->result = G_TLS_INTERACTION_HANDLED;
  test_name = g_strdup_printf ("%s/sync-implementation-success", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);

  /* Async implementation that fails */
  fixture = g_new0 (Fixture, 1);
  fixture->ask_password_async_func = NULL;
  fixture->ask_password_finish_func = NULL;
  fixture->ask_password_func = test_interaction_ask_password_sync_failure;
  fixture->result = G_TLS_INTERACTION_FAILED;
  fixture->error_domain = G_FILE_ERROR;
  fixture->error_code = G_FILE_ERROR_ACCES;
  fixture->error_message = "The message";
  test_name = g_strdup_printf ("%s/sync-implementation-failure", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);
}

static void
test_with_all_ask_password (const gchar *name,
                            TestFunc setup,
                            TestFunc func,
                            TestFunc teardown)
{
  test_with_unhandled_ask_password (name, setup, func, teardown);
  test_with_async_ask_password (name, setup, func, teardown);
  test_with_sync_ask_password (name, setup, func, teardown);
}

static void
test_with_async_request_certificate (const gchar *name,
                                     TestFunc     setup,
                                     TestFunc     func,
                                     TestFunc     teardown)
{
  gchar *test_name;
  Fixture *fixture;

  /* Async implementation that succeeds */
  fixture = g_new0 (Fixture, 1);
  fixture->request_certificate_async_func = test_interaction_request_certificate_async_success;
  fixture->request_certificate_finish_func = test_interaction_request_certificate_finish_success;
  fixture->request_certificate_func = NULL;
  fixture->result = G_TLS_INTERACTION_HANDLED;
  test_name = g_strdup_printf ("%s/async-implementation-success", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);

  /* Async implementation that fails */
  fixture = g_new0 (Fixture, 1);
  fixture->request_certificate_async_func = test_interaction_request_certificate_async_failure;
  fixture->request_certificate_finish_func = test_interaction_request_certificate_finish_failure;
  fixture->request_certificate_func = NULL;
  fixture->result = G_TLS_INTERACTION_FAILED;
  fixture->error_domain = G_FILE_ERROR;
  fixture->error_code = G_FILE_ERROR_NOENT;
  fixture->error_message = "Another message";
  test_name = g_strdup_printf ("%s/async-implementation-failure", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);
}

static void
test_with_unhandled_request_certificate (const gchar *name,
                                         TestFunc     setup,
                                         TestFunc     func,
                                         TestFunc     teardown)
{
  gchar *test_name;
  Fixture *fixture;

  /* Unhandled implementation */
  fixture = g_new0 (Fixture, 1);
  fixture->request_certificate_async_func = NULL;
  fixture->request_certificate_finish_func = NULL;
  fixture->request_certificate_func = NULL;
  fixture->result = G_TLS_INTERACTION_UNHANDLED;
  test_name = g_strdup_printf ("%s/unhandled-implementation", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);
}

static void
test_with_sync_request_certificate (const gchar *name,
                                    TestFunc     setup,
                                    TestFunc     func,
                                    TestFunc     teardown)
{
  gchar *test_name;
  Fixture *fixture;

  /* Sync implementation that succeeds */
  fixture = g_new0 (Fixture, 1);
  fixture->request_certificate_async_func = NULL;
  fixture->request_certificate_finish_func = NULL;
  fixture->request_certificate_func = test_interaction_request_certificate_sync_success;
  fixture->result = G_TLS_INTERACTION_HANDLED;
  test_name = g_strdup_printf ("%s/sync-implementation-success", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);

  /* Async implementation that fails */
  fixture = g_new0 (Fixture, 1);
  fixture->request_certificate_async_func = NULL;
  fixture->request_certificate_finish_func = NULL;
  fixture->request_certificate_func = test_interaction_request_certificate_sync_failure;
  fixture->result = G_TLS_INTERACTION_FAILED;
  fixture->error_domain = G_FILE_ERROR;
  fixture->error_code = G_FILE_ERROR_NOENT;
  fixture->error_message = "Another message";
  test_name = g_strdup_printf ("%s/sync-implementation-failure", name);
  g_test_add (test_name, Test, fixture, setup, func, teardown);
  g_free (test_name);
  g_ptr_array_add (fixtures, fixture);
}

static void
test_with_all_request_certificate (const gchar *name,
                                   TestFunc setup,
                                   TestFunc func,
                                   TestFunc teardown)
{
  test_with_unhandled_request_certificate (name, setup, func, teardown);
  test_with_async_request_certificate (name, setup, func, teardown);
  test_with_sync_request_certificate (name, setup, func, teardown);
}
int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_test_init (&argc, &argv, NULL);

  fixtures = g_ptr_array_new_with_free_func (g_free);

  /* Tests for g_tls_interaction_invoke_ask_password */
  test_with_all_ask_password ("/tls-interaction/ask-password/invoke-with-loop",
                              setup_with_thread_loop, test_invoke_ask_password, teardown_with_thread_loop);
  test_with_all_ask_password ("/tls-interaction/ask-password/invoke-without-loop",
                              setup_without_loop, test_invoke_ask_password, teardown_without_loop);
  test_with_all_ask_password ("/tls-interaction/ask-password/invoke-in-loop",
                                              setup_with_normal_loop, test_invoke_ask_password, teardown_with_normal_loop);

  /* Tests for g_tls_interaction_ask_password */
  test_with_unhandled_ask_password ("/tls-interaction/ask-password/sync",
                                    setup_without_loop, test_ask_password, teardown_without_loop);
  test_with_sync_ask_password ("/tls-interaction/ask-password/sync",
                               setup_without_loop, test_ask_password, teardown_without_loop);

  /* Tests for g_tls_interaction_ask_password_async */
  test_with_unhandled_ask_password ("/tls-interaction/ask-password/async",
                                    setup_with_normal_loop, test_ask_password_async, teardown_with_normal_loop);
  test_with_async_ask_password ("/tls-interaction/ask-password/async",
                                setup_with_normal_loop, test_ask_password_async, teardown_with_normal_loop);

  /* Tests for g_tls_interaction_invoke_request_certificate */
  test_with_all_request_certificate ("/tls-interaction/request-certificate/invoke-with-loop",
                                     setup_with_thread_loop, test_invoke_request_certificate, teardown_with_thread_loop);
  test_with_all_request_certificate ("/tls-interaction/request-certificate/invoke-without-loop",
                                     setup_without_loop, test_invoke_request_certificate, teardown_without_loop);
  test_with_all_request_certificate ("/tls-interaction/request-certificate/invoke-in-loop",
                              setup_with_normal_loop, test_invoke_request_certificate, teardown_with_normal_loop);

  /* Tests for g_tls_interaction_ask_password */
  test_with_unhandled_request_certificate ("/tls-interaction/request-certificate/sync",
                                           setup_without_loop, test_request_certificate, teardown_without_loop);
  test_with_sync_request_certificate ("/tls-interaction/request-certificate/sync",
                                      setup_without_loop, test_request_certificate, teardown_without_loop);

  /* Tests for g_tls_interaction_ask_password_async */
  test_with_unhandled_request_certificate ("/tls-interaction/request-certificate/async",
                                           setup_with_normal_loop, test_request_certificate_async, teardown_with_normal_loop);
  test_with_async_request_certificate ("/tls-interaction/request-certificate/async",
                                       setup_with_normal_loop, test_request_certificate_async, teardown_with_normal_loop);

  ret = g_test_run();
  g_ptr_array_free (fixtures, TRUE);
  return ret;
}
