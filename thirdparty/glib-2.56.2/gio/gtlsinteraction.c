/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora, Ltd.
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include <string.h>

#include "gtlscertificate.h"
#include "gtlsconnection.h"
#include "gtlsinteraction.h"
#include "gtlspassword.h"
#include "gasyncresult.h"
#include "gcancellable.h"
#include "gtask.h"
#include "gioenumtypes.h"
#include "glibintl.h"


/**
 * SECTION:gtlsinteraction
 * @short_description: Interaction with the user during TLS operations.
 * @include: gio/gio.h
 *
 * #GTlsInteraction provides a mechanism for the TLS connection and database
 * code to interact with the user. It can be used to ask the user for passwords.
 *
 * To use a #GTlsInteraction with a TLS connection use
 * g_tls_connection_set_interaction().
 *
 * Callers should instantiate a derived class that implements the various
 * interaction methods to show the required dialogs.
 *
 * Callers should use the 'invoke' functions like
 * g_tls_interaction_invoke_ask_password() to run interaction methods. These
 * functions make sure that the interaction is invoked in the main loop
 * and not in the current thread, if the current thread is not running the
 * main loop.
 *
 * Derived classes can choose to implement whichever interactions methods they'd
 * like to support by overriding those virtual methods in their class
 * initialization function. Any interactions not implemented will return
 * %G_TLS_INTERACTION_UNHANDLED. If a derived class implements an async method,
 * it must also implement the corresponding finish method.
 */

/**
 * GTlsInteraction:
 *
 * An object representing interaction that the TLS connection and database
 * might have with the user.
 *
 * Since: 2.30
 */

/**
 * GTlsInteractionClass:
 * @ask_password: ask for a password synchronously. If the implementation
 *     returns %G_TLS_INTERACTION_HANDLED, then the password argument should
 *     have been filled in by using g_tls_password_set_value() or a similar
 *     function.
 * @ask_password_async: ask for a password asynchronously.
 * @ask_password_finish: complete operation to ask for a password asynchronously.
 *     If the implementation returns %G_TLS_INTERACTION_HANDLED, then the
 *     password argument of the async method should have been filled in by using
 *     g_tls_password_set_value() or a similar function.
 * @request_certificate: ask for a certificate synchronously. If the
 *     implementation returns %G_TLS_INTERACTION_HANDLED, then the connection
 *     argument should have been filled in by using
 *     g_tls_connection_set_certificate().
 * @request_certificate_async: ask for a certificate asynchronously.
 * @request_certificate_finish: complete operation to ask for a certificate
 *     asynchronously. If the implementation returns %G_TLS_INTERACTION_HANDLED,
 *     then the connection argument of the async method should have been
 *     filled in by using g_tls_connection_set_certificate().
 *
 * The class for #GTlsInteraction. Derived classes implement the various
 * virtual interaction methods to handle TLS interactions.
 *
 * Derived classes can choose to implement whichever interactions methods they'd
 * like to support by overriding those virtual methods in their class
 * initialization function. If a derived class implements an async method,
 * it must also implement the corresponding finish method.
 *
 * The synchronous interaction methods should implement to display modal dialogs,
 * and the asynchronous methods to display modeless dialogs.
 *
 * If the user cancels an interaction, then the result should be
 * %G_TLS_INTERACTION_FAILED and the error should be set with a domain of
 * %G_IO_ERROR and code of %G_IO_ERROR_CANCELLED.
 *
 * Since: 2.30
 */

struct _GTlsInteractionPrivate {
  GMainContext *context;
};

G_DEFINE_TYPE_WITH_PRIVATE (GTlsInteraction, g_tls_interaction, G_TYPE_OBJECT)

typedef struct {
  GMutex mutex;

  /* Input arguments */
  GTlsInteraction *interaction;
  GObject *argument;
  GCancellable *cancellable;

  /* Used when we're invoking async interactions */
  GAsyncReadyCallback callback;
  gpointer user_data;

  /* Used when we expect results */
  GTlsInteractionResult result;
  GError *error;
  gboolean complete;
  GCond cond;
} InvokeClosure;

static void
invoke_closure_free (gpointer data)
{
  InvokeClosure *closure = data;
  g_assert (closure);
  g_object_unref (closure->interaction);
  g_clear_object (&closure->argument);
  g_clear_object (&closure->cancellable);
  g_cond_clear (&closure->cond);
  g_mutex_clear (&closure->mutex);
  g_clear_error (&closure->error);

  /* Insurance that we've actually used these before freeing */
  g_assert (closure->callback == NULL);
  g_assert (closure->user_data == NULL);

  g_free (closure);
}

static InvokeClosure *
invoke_closure_new (GTlsInteraction *interaction,
                    GObject         *argument,
                    GCancellable    *cancellable)
{
  InvokeClosure *closure = g_new0 (InvokeClosure, 1);
  closure->interaction = g_object_ref (interaction);
  closure->argument = argument ? g_object_ref (argument) : NULL;
  closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  g_mutex_init (&closure->mutex);
  g_cond_init (&closure->cond);
  closure->result = G_TLS_INTERACTION_UNHANDLED;
  return closure;
}

static GTlsInteractionResult
invoke_closure_wait_and_free (InvokeClosure *closure,
                              GError       **error)
{
  GTlsInteractionResult result;

  g_mutex_lock (&closure->mutex);

  while (!closure->complete)
    g_cond_wait (&closure->cond, &closure->mutex);

  g_mutex_unlock (&closure->mutex);

  if (closure->error)
    {
      g_propagate_error (error, closure->error);
      closure->error = NULL;
    }
  result = closure->result;

  invoke_closure_free (closure);
  return result;
}

static GTlsInteractionResult
invoke_closure_complete_and_free (GTlsInteraction *interaction,
                                  InvokeClosure *closure,
                                  GError **error)
{
  GTlsInteractionResult result;
  gboolean complete;

  /*
   * Handle the case where we've been called from within the main context
   * or in the case where the main context is not running. This approximates
   * the behavior of a modal dialog.
   */
  if (g_main_context_acquire (interaction->priv->context))
    {
      for (;;)
        {
          g_mutex_lock (&closure->mutex);
          complete = closure->complete;
          g_mutex_unlock (&closure->mutex);
          if (complete)
            break;
          g_main_context_iteration (interaction->priv->context, TRUE);
        }

      g_main_context_release (interaction->priv->context);

      if (closure->error)
        {
          g_propagate_error (error, closure->error);
          closure->error = NULL;
        }

      result = closure->result;
      invoke_closure_free (closure);
    }

  /*
   * Handle the case where we're in a different thread than the main
   * context and a main loop is running.
   */
  else
    {
      result = invoke_closure_wait_and_free (closure, error);
    }

  return result;
}

static void
g_tls_interaction_init (GTlsInteraction *interaction)
{
  interaction->priv = g_tls_interaction_get_instance_private (interaction);
  interaction->priv->context = g_main_context_ref_thread_default ();
}

static void
g_tls_interaction_finalize (GObject *object)
{
  GTlsInteraction *interaction = G_TLS_INTERACTION (object);

  g_main_context_unref (interaction->priv->context);

  G_OBJECT_CLASS (g_tls_interaction_parent_class)->finalize (object);
}

static void
g_tls_interaction_class_init (GTlsInteractionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_tls_interaction_finalize;
}

static gboolean
on_invoke_ask_password_sync (gpointer user_data)
{
  InvokeClosure *closure = user_data;
  GTlsInteractionClass *klass;

  g_mutex_lock (&closure->mutex);

  klass = G_TLS_INTERACTION_GET_CLASS (closure->interaction);
  g_assert (klass->ask_password);

  closure->result = klass->ask_password (closure->interaction,
                                         G_TLS_PASSWORD (closure->argument),
                                         closure->cancellable,
                                         &closure->error);

  closure->complete = TRUE;
  g_cond_signal (&closure->cond);
  g_mutex_unlock (&closure->mutex);

  return FALSE; /* don't call again */
}

static void
on_ask_password_complete (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  InvokeClosure *closure = user_data;
  GTlsInteractionClass *klass;

  g_mutex_lock (&closure->mutex);

  klass = G_TLS_INTERACTION_GET_CLASS (closure->interaction);
  g_assert (klass->ask_password_finish);

  closure->result = klass->ask_password_finish (closure->interaction,
                                                result,
                                                &closure->error);

  closure->complete = TRUE;
  g_cond_signal (&closure->cond);
  g_mutex_unlock (&closure->mutex);
}

static gboolean
on_invoke_ask_password_async_as_sync (gpointer user_data)
{
  InvokeClosure *closure = user_data;
  GTlsInteractionClass *klass;

  g_mutex_lock (&closure->mutex);

  klass = G_TLS_INTERACTION_GET_CLASS (closure->interaction);
  g_assert (klass->ask_password_async);

  klass->ask_password_async (closure->interaction,
                             G_TLS_PASSWORD (closure->argument),
                             closure->cancellable,
                             on_ask_password_complete,
                             closure);

  /* Note that we've used these */
  closure->callback = NULL;
  closure->user_data = NULL;

  g_mutex_unlock (&closure->mutex);

  return FALSE; /* don't call again */
}

/**
 * g_tls_interaction_invoke_ask_password:
 * @interaction: a #GTlsInteraction object
 * @password: a #GTlsPassword object
 * @cancellable: an optional #GCancellable cancellation object
 * @error: an optional location to place an error on failure
 *
 * Invoke the interaction to ask the user for a password. It invokes this
 * interaction in the main loop, specifically the #GMainContext returned by
 * g_main_context_get_thread_default() when the interaction is created. This
 * is called by called by #GTlsConnection or #GTlsDatabase to ask the user
 * for a password.
 *
 * Derived subclasses usually implement a password prompt, although they may
 * also choose to provide a password from elsewhere. The @password value will
 * be filled in and then @callback will be called. Alternatively the user may
 * abort this password request, which will usually abort the TLS connection.
 *
 * The implementation can either be a synchronous (eg: modal dialog) or an
 * asynchronous one (eg: modeless dialog). This function will take care of
 * calling which ever one correctly.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code. Certain implementations may
 * not support immediate cancellation.
 *
 * Returns: The status of the ask password interaction.
 *
 * Since: 2.30
 */
GTlsInteractionResult
g_tls_interaction_invoke_ask_password (GTlsInteraction    *interaction,
                                       GTlsPassword       *password,
                                       GCancellable       *cancellable,
                                       GError            **error)
{
  GTlsInteractionResult result;
  InvokeClosure *closure;
  GTlsInteractionClass *klass;

  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_TLS_PASSWORD (password), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), G_TLS_INTERACTION_UNHANDLED);

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);

  if (klass->ask_password)
    {
      closure = invoke_closure_new (interaction, G_OBJECT (password), cancellable);
      g_main_context_invoke (interaction->priv->context,
                             on_invoke_ask_password_sync, closure);
      result = invoke_closure_wait_and_free (closure, error);
    }
  else if (klass->ask_password_async)
    {
      g_return_val_if_fail (klass->ask_password_finish, G_TLS_INTERACTION_UNHANDLED);

      closure = invoke_closure_new (interaction, G_OBJECT (password), cancellable);
      g_main_context_invoke (interaction->priv->context,
                             on_invoke_ask_password_async_as_sync, closure);

      result = invoke_closure_complete_and_free (interaction, closure, error);
    }
  else
    {
      result = G_TLS_INTERACTION_UNHANDLED;
    }

  return result;
}

/**
 * g_tls_interaction_ask_password:
 * @interaction: a #GTlsInteraction object
 * @password: a #GTlsPassword object
 * @cancellable: an optional #GCancellable cancellation object
 * @error: an optional location to place an error on failure
 *
 * Run synchronous interaction to ask the user for a password. In general,
 * g_tls_interaction_invoke_ask_password() should be used instead of this
 * function.
 *
 * Derived subclasses usually implement a password prompt, although they may
 * also choose to provide a password from elsewhere. The @password value will
 * be filled in and then @callback will be called. Alternatively the user may
 * abort this password request, which will usually abort the TLS connection.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code. Certain implementations may
 * not support immediate cancellation.
 *
 * Returns: The status of the ask password interaction.
 *
 * Since: 2.30
 */
GTlsInteractionResult
g_tls_interaction_ask_password (GTlsInteraction    *interaction,
                                GTlsPassword       *password,
                                GCancellable       *cancellable,
                                GError            **error)
{
  GTlsInteractionClass *klass;

  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_TLS_PASSWORD (password), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), G_TLS_INTERACTION_UNHANDLED);

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);
  if (klass->ask_password)
    return (klass->ask_password) (interaction, password, cancellable, error);
  else
    return G_TLS_INTERACTION_UNHANDLED;
}

/**
 * g_tls_interaction_ask_password_async:
 * @interaction: a #GTlsInteraction object
 * @password: a #GTlsPassword object
 * @cancellable: an optional #GCancellable cancellation object
 * @callback: (nullable): will be called when the interaction completes
 * @user_data: (nullable): data to pass to the @callback
 *
 * Run asynchronous interaction to ask the user for a password. In general,
 * g_tls_interaction_invoke_ask_password() should be used instead of this
 * function.
 *
 * Derived subclasses usually implement a password prompt, although they may
 * also choose to provide a password from elsewhere. The @password value will
 * be filled in and then @callback will be called. Alternatively the user may
 * abort this password request, which will usually abort the TLS connection.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code. Certain implementations may
 * not support immediate cancellation.
 *
 * Certain implementations may not support immediate cancellation.
 *
 * Since: 2.30
 */
void
g_tls_interaction_ask_password_async (GTlsInteraction    *interaction,
                                      GTlsPassword       *password,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data)
{
  GTlsInteractionClass *klass;
  GTask *task;

  g_return_if_fail (G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (G_IS_TLS_PASSWORD (password));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);
  if (klass->ask_password_async)
    {
      g_return_if_fail (klass->ask_password_finish);
      (klass->ask_password_async) (interaction, password, cancellable,
                                   callback, user_data);
    }
  else
    {
      task = g_task_new (interaction, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_tls_interaction_ask_password_async);
      g_task_return_int (task, G_TLS_INTERACTION_UNHANDLED);
      g_object_unref (task);
    }
}

/**
 * g_tls_interaction_ask_password_finish:
 * @interaction: a #GTlsInteraction object
 * @result: the result passed to the callback
 * @error: an optional location to place an error on failure
 *
 * Complete an ask password user interaction request. This should be once
 * the g_tls_interaction_ask_password_async() completion callback is called.
 *
 * If %G_TLS_INTERACTION_HANDLED is returned, then the #GTlsPassword passed
 * to g_tls_interaction_ask_password() will have its password filled in.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code.
 *
 * Returns: The status of the ask password interaction.
 *
 * Since: 2.30
 */
GTlsInteractionResult
g_tls_interaction_ask_password_finish (GTlsInteraction    *interaction,
                                       GAsyncResult       *result,
                                       GError            **error)
{
  GTlsInteractionClass *klass;

  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), G_TLS_INTERACTION_UNHANDLED);

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);
  if (klass->ask_password_finish)
    {
      g_return_val_if_fail (klass->ask_password_async != NULL, G_TLS_INTERACTION_UNHANDLED);

      return (klass->ask_password_finish) (interaction, result, error);
    }
  else
    {
      g_return_val_if_fail (g_async_result_is_tagged (result, g_tls_interaction_ask_password_async), G_TLS_INTERACTION_UNHANDLED);

      return g_task_propagate_int (G_TASK (result), error);
    }
}

static gboolean
on_invoke_request_certificate_sync (gpointer user_data)
{
  InvokeClosure *closure = user_data;
  GTlsInteractionClass *klass;

  g_mutex_lock (&closure->mutex);

  klass = G_TLS_INTERACTION_GET_CLASS (closure->interaction);
  g_assert (klass->request_certificate != NULL);

  closure->result = klass->request_certificate (closure->interaction,
                                                G_TLS_CONNECTION (closure->argument),
                                                0,
                                                closure->cancellable,
                                                &closure->error);

  closure->complete = TRUE;
  g_cond_signal (&closure->cond);
  g_mutex_unlock (&closure->mutex);

  return FALSE; /* don't call again */
}

static void
on_request_certificate_complete (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  InvokeClosure *closure = user_data;
  GTlsInteractionClass *klass;

  g_mutex_lock (&closure->mutex);

  klass = G_TLS_INTERACTION_GET_CLASS (closure->interaction);
  g_assert (klass->request_certificate_finish != NULL);

  closure->result = klass->request_certificate_finish (closure->interaction,
                                                       result, &closure->error);

  closure->complete = TRUE;
  g_cond_signal (&closure->cond);
  g_mutex_unlock (&closure->mutex);
}

static gboolean
on_invoke_request_certificate_async_as_sync (gpointer user_data)
{
  InvokeClosure *closure = user_data;
  GTlsInteractionClass *klass;

  g_mutex_lock (&closure->mutex);

  klass = G_TLS_INTERACTION_GET_CLASS (closure->interaction);
  g_assert (klass->request_certificate_async);

  klass->request_certificate_async (closure->interaction,
                                    G_TLS_CONNECTION (closure->argument), 0,
                                    closure->cancellable,
                                    on_request_certificate_complete,
                                    closure);

  /* Note that we've used these */
  closure->callback = NULL;
  closure->user_data = NULL;

  g_mutex_unlock (&closure->mutex);

  return FALSE; /* don't call again */
}

/**
 * g_tls_interaction_invoke_request_certificate:
 * @interaction: a #GTlsInteraction object
 * @connection: a #GTlsConnection object
 * @flags: flags providing more information about the request
 * @cancellable: an optional #GCancellable cancellation object
 * @error: an optional location to place an error on failure
 *
 * Invoke the interaction to ask the user to choose a certificate to
 * use with the connection. It invokes this interaction in the main
 * loop, specifically the #GMainContext returned by
 * g_main_context_get_thread_default() when the interaction is
 * created. This is called by called by #GTlsConnection when the peer
 * requests a certificate during the handshake.
 *
 * Derived subclasses usually implement a certificate selector,
 * although they may also choose to provide a certificate from
 * elsewhere. Alternatively the user may abort this certificate
 * request, which may or may not abort the TLS connection.
 *
 * The implementation can either be a synchronous (eg: modal dialog) or an
 * asynchronous one (eg: modeless dialog). This function will take care of
 * calling which ever one correctly.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code. Certain implementations may
 * not support immediate cancellation.
 *
 * Returns: The status of the certificate request interaction.
 *
 * Since: 2.40
 */
GTlsInteractionResult
g_tls_interaction_invoke_request_certificate (GTlsInteraction    *interaction,
                                              GTlsConnection               *connection,
                                              GTlsCertificateRequestFlags   flags,
                                              GCancellable       *cancellable,
                                              GError            **error)
{
  GTlsInteractionResult result;
  InvokeClosure *closure;
  GTlsInteractionClass *klass;

  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_TLS_CONNECTION (connection), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), G_TLS_INTERACTION_UNHANDLED);

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);

  if (klass->request_certificate)
    {
      closure = invoke_closure_new (interaction, G_OBJECT (connection), cancellable);
      g_main_context_invoke (interaction->priv->context,
                             on_invoke_request_certificate_sync, closure);
      result = invoke_closure_wait_and_free (closure, error);
    }
  else if (klass->request_certificate_async)
    {
      g_return_val_if_fail (klass->request_certificate_finish, G_TLS_INTERACTION_UNHANDLED);

      closure = invoke_closure_new (interaction, G_OBJECT (connection), cancellable);
      g_main_context_invoke (interaction->priv->context,
                             on_invoke_request_certificate_async_as_sync, closure);

      result = invoke_closure_complete_and_free (interaction, closure, error);
    }
  else
    {
      result = G_TLS_INTERACTION_UNHANDLED;
    }

  return result;
}

/**
 * g_tls_interaction_request_certificate:
 * @interaction: a #GTlsInteraction object
 * @connection: a #GTlsConnection object
 * @flags: flags providing more information about the request
 * @cancellable: an optional #GCancellable cancellation object
 * @error: an optional location to place an error on failure
 *
 * Run synchronous interaction to ask the user to choose a certificate to use
 * with the connection. In general, g_tls_interaction_invoke_request_certificate()
 * should be used instead of this function.
 *
 * Derived subclasses usually implement a certificate selector, although they may
 * also choose to provide a certificate from elsewhere. Alternatively the user may
 * abort this certificate request, which will usually abort the TLS connection.
 *
 * If %G_TLS_INTERACTION_HANDLED is returned, then the #GTlsConnection
 * passed to g_tls_interaction_request_certificate() will have had its
 * #GTlsConnection:certificate filled in.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code. Certain implementations may
 * not support immediate cancellation.
 *
 * Returns: The status of the request certificate interaction.
 *
 * Since: 2.40
 */
GTlsInteractionResult
g_tls_interaction_request_certificate (GTlsInteraction              *interaction,
                                       GTlsConnection               *connection,
                                       GTlsCertificateRequestFlags   flags,
                                       GCancellable                 *cancellable,
                                       GError                      **error)
{
  GTlsInteractionClass *klass;

  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_TLS_CONNECTION (connection), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), G_TLS_INTERACTION_UNHANDLED);

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);
  if (klass->request_certificate)
    return (klass->request_certificate) (interaction, connection, flags, cancellable, error);
  else
    return G_TLS_INTERACTION_UNHANDLED;
}

/**
 * g_tls_interaction_request_certificate_async:
 * @interaction: a #GTlsInteraction object
 * @connection: a #GTlsConnection object
 * @flags: flags providing more information about the request
 * @cancellable: an optional #GCancellable cancellation object
 * @callback: (nullable): will be called when the interaction completes
 * @user_data: (nullable): data to pass to the @callback
 *
 * Run asynchronous interaction to ask the user for a certificate to use with
 * the connection. In general, g_tls_interaction_invoke_request_certificate() should
 * be used instead of this function.
 *
 * Derived subclasses usually implement a certificate selector, although they may
 * also choose to provide a certificate from elsewhere. @callback will be called
 * when the operation completes. Alternatively the user may abort this certificate
 * request, which will usually abort the TLS connection.
 *
 * Since: 2.40
 */
void
g_tls_interaction_request_certificate_async (GTlsInteraction              *interaction,
                                             GTlsConnection               *connection,
                                             GTlsCertificateRequestFlags   flags,
                                             GCancellable                 *cancellable,
                                             GAsyncReadyCallback           callback,
                                             gpointer                      user_data)
{
  GTlsInteractionClass *klass;
  GTask *task;

  g_return_if_fail (G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (G_IS_TLS_CONNECTION (connection));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);
  if (klass->request_certificate_async)
    {
      g_return_if_fail (klass->request_certificate_finish);
      (klass->request_certificate_async) (interaction, connection, flags,
                                          cancellable, callback, user_data);
    }
  else
    {
      task = g_task_new (interaction, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_tls_interaction_request_certificate_async);
      g_task_return_int (task, G_TLS_INTERACTION_UNHANDLED);
      g_object_unref (task);
    }
}

/**
 * g_tls_interaction_request_certificate_finish:
 * @interaction: a #GTlsInteraction object
 * @result: the result passed to the callback
 * @error: an optional location to place an error on failure
 *
 * Complete an request certificate user interaction request. This should be once
 * the g_tls_interaction_request_certificate_async() completion callback is called.
 *
 * If %G_TLS_INTERACTION_HANDLED is returned, then the #GTlsConnection
 * passed to g_tls_interaction_request_certificate_async() will have had its
 * #GTlsConnection:certificate filled in.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_FAILED will be returned with an error that
 * contains a %G_IO_ERROR_CANCELLED error code.
 *
 * Returns: The status of the request certificate interaction.
 *
 * Since: 2.40
 */
GTlsInteractionResult
g_tls_interaction_request_certificate_finish (GTlsInteraction    *interaction,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  GTlsInteractionClass *klass;

  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), G_TLS_INTERACTION_UNHANDLED);

  klass = G_TLS_INTERACTION_GET_CLASS (interaction);
  if (klass->request_certificate_finish)
    {
      g_return_val_if_fail (klass->request_certificate_async != NULL, G_TLS_INTERACTION_UNHANDLED);

      return (klass->request_certificate_finish) (interaction, result, error);
    }
  else
    {
      g_return_val_if_fail (g_async_result_is_tagged (result, g_tls_interaction_request_certificate_async), G_TLS_INTERACTION_UNHANDLED);

      return g_task_propagate_int (G_TASK (result), error);
    }
}
