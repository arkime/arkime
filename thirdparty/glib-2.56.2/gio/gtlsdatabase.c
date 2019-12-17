/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Collabora, Ltd.
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

#include "gtlsdatabase.h"

#include "gasyncresult.h"
#include "gcancellable.h"
#include "glibintl.h"
#include "gsocketconnectable.h"
#include "gtask.h"
#include "gtlscertificate.h"
#include "gtlsinteraction.h"

/**
 * SECTION:gtlsdatabase
 * @short_description: TLS database type
 * @include: gio/gio.h
 *
 * #GTlsDatabase is used to lookup certificates and other information
 * from a certificate or key store. It is an abstract base class which
 * TLS library specific subtypes override.
 *
 * Most common client applications will not directly interact with
 * #GTlsDatabase. It is used internally by #GTlsConnection.
 *
 * Since: 2.30
 */

/**
 * GTlsDatabase:
 *
 * Abstract base class for the backend-specific database types.
 *
 * Since: 2.30
 */

/**
 * GTlsDatabaseClass:
 * @verify_chain: Virtual method implementing
 *  g_tls_database_verify_chain().
 * @verify_chain_async: Virtual method implementing
 *  g_tls_database_verify_chain_async().
 * @verify_chain_finish: Virtual method implementing
 *  g_tls_database_verify_chain_finish().
 * @create_certificate_handle: Virtual method implementing
 *  g_tls_database_create_certificate_handle().
 * @lookup_certificate_for_handle: Virtual method implementing
 *  g_tls_database_lookup_certificate_for_handle().
 * @lookup_certificate_for_handle_async: Virtual method implementing
 *  g_tls_database_lookup_certificate_for_handle_async().
 * @lookup_certificate_for_handle_finish: Virtual method implementing
 *  g_tls_database_lookup_certificate_for_handle_finish().
 * @lookup_certificate_issuer: Virtual method implementing
 *  g_tls_database_lookup_certificate_issuer().
 * @lookup_certificate_issuer_async: Virtual method implementing
 *  g_tls_database_lookup_certificate_issuer_async().
 * @lookup_certificate_issuer_finish: Virtual method implementing
 *  g_tls_database_lookup_certificate_issuer_finish().
 * @lookup_certificates_issued_by: Virtual method implementing
 *  g_tls_database_lookup_certificates_issued_by().
 * @lookup_certificates_issued_by_async: Virtual method implementing
 *  g_tls_database_lookup_certificates_issued_by_async().
 * @lookup_certificates_issued_by_finish: Virtual method implementing
 *  g_tls_database_lookup_certificates_issued_by_finish().
 *
 * The class for #GTlsDatabase. Derived classes should implement the various
 * virtual methods. _async and _finish methods have a default
 * implementation that runs the corresponding sync method in a thread.
 *
 * Since: 2.30
 */

G_DEFINE_ABSTRACT_TYPE (GTlsDatabase, g_tls_database, G_TYPE_OBJECT)

enum {
  UNLOCK_REQUIRED,

  LAST_SIGNAL
};

/**
 * G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER:
 *
 * The purpose used to verify the server certificate in a TLS connection. This
 * is the most common purpose in use. Used by TLS clients.
 */

/**
 * G_TLS_DATABASE_PURPOSE_AUTHENTICATE_CLIENT:
 *
 * The purpose used to verify the client certificate in a TLS connection.
 * Used by TLS servers.
 */

static void
g_tls_database_init (GTlsDatabase *cert)
{

}

typedef struct _AsyncVerifyChain {
  GTlsCertificate *chain;
  gchar *purpose;
  GSocketConnectable *identity;
  GTlsInteraction *interaction;
  GTlsDatabaseVerifyFlags flags;
} AsyncVerifyChain;

static void
async_verify_chain_free (gpointer data)
{
  AsyncVerifyChain *args = data;
  g_clear_object (&args->chain);
  g_free (args->purpose);
  g_clear_object (&args->identity);
  g_clear_object (&args->interaction);
  g_slice_free (AsyncVerifyChain, args);
}

static void
async_verify_chain_thread (GTask         *task,
			   gpointer       object,
			   gpointer       task_data,
			   GCancellable  *cancellable)
{
  AsyncVerifyChain *args = task_data;
  GTlsCertificateFlags verify_result;
  GError *error = NULL;

  verify_result = g_tls_database_verify_chain (G_TLS_DATABASE (object),
					       args->chain,
					       args->purpose,
					       args->identity,
					       args->interaction,
					       args->flags,
					       cancellable,
					       &error);
  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, (gssize)verify_result);
}

static void
g_tls_database_real_verify_chain_async (GTlsDatabase           *self,
                                        GTlsCertificate        *chain,
                                        const gchar            *purpose,
                                        GSocketConnectable     *identity,
                                        GTlsInteraction        *interaction,
                                        GTlsDatabaseVerifyFlags flags,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
  GTask *task;
  AsyncVerifyChain *args;

  args = g_slice_new0 (AsyncVerifyChain);
  args->chain = g_object_ref (chain);
  args->purpose = g_strdup (purpose);
  args->identity = identity ? g_object_ref (identity) : NULL;
  args->interaction = interaction ? g_object_ref (interaction) : NULL;
  args->flags = flags;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_tls_database_real_verify_chain_async);
  g_task_set_task_data (task, args, async_verify_chain_free);
  g_task_run_in_thread (task, async_verify_chain_thread);
  g_object_unref (task);
}

static GTlsCertificateFlags
g_tls_database_real_verify_chain_finish (GTlsDatabase          *self,
                                         GAsyncResult          *result,
                                         GError               **error)
{
  GTlsCertificateFlags ret;

  g_return_val_if_fail (g_task_is_valid (result, self), G_TLS_CERTIFICATE_GENERIC_ERROR);

  ret = (GTlsCertificateFlags)g_task_propagate_int (G_TASK (result), error);
  if (ret == (GTlsCertificateFlags)-1)
    return G_TLS_CERTIFICATE_GENERIC_ERROR;
  else
    return ret;
}

typedef struct {
  gchar *handle;
  GTlsInteraction *interaction;
  GTlsDatabaseLookupFlags flags;
} AsyncLookupCertificateForHandle;

static void
async_lookup_certificate_for_handle_free (gpointer data)
{
  AsyncLookupCertificateForHandle *args = data;

  g_free (args->handle);
  g_clear_object (&args->interaction);
  g_slice_free (AsyncLookupCertificateForHandle, args);
}

static void
async_lookup_certificate_for_handle_thread (GTask         *task,
					    gpointer       object,
					    gpointer       task_data,
					    GCancellable  *cancellable)
{
  AsyncLookupCertificateForHandle *args = task_data;
  GTlsCertificate *result;
  GError *error = NULL;

  result = g_tls_database_lookup_certificate_for_handle (G_TLS_DATABASE (object),
							 args->handle,
							 args->interaction,
							 args->flags,
							 cancellable,
							 &error);
  if (result)
    g_task_return_pointer (task, result, g_object_unref);
  else
    g_task_return_error (task, error);
}

static void
g_tls_database_real_lookup_certificate_for_handle_async (GTlsDatabase           *self,
                                                         const gchar            *handle,
                                                         GTlsInteraction        *interaction,
                                                         GTlsDatabaseLookupFlags flags,
                                                         GCancellable           *cancellable,
                                                         GAsyncReadyCallback     callback,
                                                         gpointer                user_data)
{
  GTask *task;
  AsyncLookupCertificateForHandle *args;

  args = g_slice_new0 (AsyncLookupCertificateForHandle);
  args->handle = g_strdup (handle);
  args->interaction = interaction ? g_object_ref (interaction) : NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task,
                         g_tls_database_real_lookup_certificate_for_handle_async);
  g_task_set_task_data (task, args, async_lookup_certificate_for_handle_free);
  g_task_run_in_thread (task, async_lookup_certificate_for_handle_thread);
  g_object_unref (task);
}

static GTlsCertificate*
g_tls_database_real_lookup_certificate_for_handle_finish (GTlsDatabase          *self,
                                                          GAsyncResult          *result,
                                                          GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}


typedef struct {
  GTlsCertificate *certificate;
  GTlsInteraction *interaction;
  GTlsDatabaseLookupFlags flags;
} AsyncLookupCertificateIssuer;

static void
async_lookup_certificate_issuer_free (gpointer data)
{
  AsyncLookupCertificateIssuer *args = data;

  g_clear_object (&args->certificate);
  g_clear_object (&args->interaction);
  g_slice_free (AsyncLookupCertificateIssuer, args);
}

static void
async_lookup_certificate_issuer_thread (GTask         *task,
					gpointer       object,
					gpointer       task_data,
					GCancellable  *cancellable)
{
  AsyncLookupCertificateIssuer *args = task_data;
  GTlsCertificate *issuer;
  GError *error = NULL;

  issuer = g_tls_database_lookup_certificate_issuer (G_TLS_DATABASE (object),
						     args->certificate,
						     args->interaction,
						     args->flags,
						     cancellable,
						     &error);
  if (issuer)
    g_task_return_pointer (task, issuer, g_object_unref);
  else
    g_task_return_error (task, error);
}

static void
g_tls_database_real_lookup_certificate_issuer_async (GTlsDatabase           *self,
                                                     GTlsCertificate        *certificate,
                                                     GTlsInteraction        *interaction,
                                                     GTlsDatabaseLookupFlags flags,
                                                     GCancellable           *cancellable,
                                                     GAsyncReadyCallback     callback,
                                                     gpointer                user_data)
{
  GTask *task;
  AsyncLookupCertificateIssuer *args;

  args = g_slice_new0 (AsyncLookupCertificateIssuer);
  args->certificate = g_object_ref (certificate);
  args->flags = flags;
  args->interaction = interaction ? g_object_ref (interaction) : NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task,
                         g_tls_database_real_lookup_certificate_issuer_async);
  g_task_set_task_data (task, args, async_lookup_certificate_issuer_free);
  g_task_run_in_thread (task, async_lookup_certificate_issuer_thread);
  g_object_unref (task);
}

static GTlsCertificate *
g_tls_database_real_lookup_certificate_issuer_finish (GTlsDatabase          *self,
                                                      GAsyncResult          *result,
                                                      GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct {
  GByteArray *issuer;
  GTlsInteraction *interaction;
  GTlsDatabaseLookupFlags flags;
} AsyncLookupCertificatesIssuedBy;

static void
async_lookup_certificates_issued_by_free (gpointer data)
{
  AsyncLookupCertificatesIssuedBy *args = data;

  g_byte_array_unref (args->issuer);
  g_clear_object (&args->interaction);
  g_slice_free (AsyncLookupCertificatesIssuedBy, args);
}

static void
async_lookup_certificates_free_certificates (gpointer data)
{
  GList *list = data;

  g_list_free_full (list, g_object_unref);
}

static void
async_lookup_certificates_issued_by_thread (GTask         *task,
					    gpointer       object,
					    gpointer       task_data,
                                            GCancellable  *cancellable)
{
  AsyncLookupCertificatesIssuedBy *args = task_data;
  GList *results;
  GError *error = NULL;

  results = g_tls_database_lookup_certificates_issued_by (G_TLS_DATABASE (object),
							  args->issuer,
							  args->interaction,
							  args->flags,
							  cancellable,
							  &error);
  if (results)
    g_task_return_pointer (task, results, async_lookup_certificates_free_certificates);
  else
    g_task_return_error (task, error);
}

static void
g_tls_database_real_lookup_certificates_issued_by_async (GTlsDatabase           *self,
                                                         GByteArray             *issuer,
                                                         GTlsInteraction        *interaction,
                                                         GTlsDatabaseLookupFlags flags,
                                                         GCancellable           *cancellable,
                                                         GAsyncReadyCallback     callback,
                                                         gpointer                user_data)
{
  GTask *task;
  AsyncLookupCertificatesIssuedBy *args;

  args = g_slice_new0 (AsyncLookupCertificatesIssuedBy);
  args->issuer = g_byte_array_ref (issuer);
  args->flags = flags;
  args->interaction = interaction ? g_object_ref (interaction) : NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task,
                         g_tls_database_real_lookup_certificates_issued_by_async);
  g_task_set_task_data (task, args, async_lookup_certificates_issued_by_free);
  g_task_run_in_thread (task, async_lookup_certificates_issued_by_thread);
  g_object_unref (task);
}

static GList *
g_tls_database_real_lookup_certificates_issued_by_finish (GTlsDatabase          *self,
                                                          GAsyncResult          *result,
                                                          GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
g_tls_database_class_init (GTlsDatabaseClass *klass)
{
  klass->verify_chain_async = g_tls_database_real_verify_chain_async;
  klass->verify_chain_finish = g_tls_database_real_verify_chain_finish;
  klass->lookup_certificate_for_handle_async = g_tls_database_real_lookup_certificate_for_handle_async;
  klass->lookup_certificate_for_handle_finish = g_tls_database_real_lookup_certificate_for_handle_finish;
  klass->lookup_certificate_issuer_async = g_tls_database_real_lookup_certificate_issuer_async;
  klass->lookup_certificate_issuer_finish = g_tls_database_real_lookup_certificate_issuer_finish;
  klass->lookup_certificates_issued_by_async = g_tls_database_real_lookup_certificates_issued_by_async;
  klass->lookup_certificates_issued_by_finish = g_tls_database_real_lookup_certificates_issued_by_finish;
}

/**
 * g_tls_database_verify_chain:
 * @self: a #GTlsDatabase
 * @chain: a #GTlsCertificate chain
 * @purpose: the purpose that this certificate chain will be used for.
 * @identity: (nullable): the expected peer identity
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: additional verify flags
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: (nullable): a #GError, or %NULL
 *
 * Determines the validity of a certificate chain after looking up and
 * adding any missing certificates to the chain.
 *
 * @chain is a chain of #GTlsCertificate objects each pointing to the next
 * certificate in the chain by its #GTlsCertificate:issuer property. The chain may initially
 * consist of one or more certificates. After the verification process is
 * complete, @chain may be modified by adding missing certificates, or removing
 * extra certificates. If a certificate anchor was found, then it is added to
 * the @chain.
 *
 * @purpose describes the purpose (or usage) for which the certificate
 * is being used. Typically @purpose will be set to #G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER
 * which means that the certificate is being used to authenticate a server
 * (and we are acting as the client).
 *
 * The @identity is used to check for pinned certificates (trust exceptions)
 * in the database. These will override the normal verification process on a
 * host by host basis.
 *
 * Currently there are no @flags, and %G_TLS_DATABASE_VERIFY_NONE should be
 * used.
 *
 * If @chain is found to be valid, then the return value will be 0. If
 * @chain is found to be invalid, then the return value will indicate
 * the problems found. If the function is unable to determine whether
 * @chain is valid or not (eg, because @cancellable is triggered
 * before it completes) then the return value will be
 * %G_TLS_CERTIFICATE_GENERIC_ERROR and @error will be set
 * accordingly. @error is not set when @chain is successfully analyzed
 * but found to be invalid.
 *
 * This function can block, use g_tls_database_verify_chain_async() to perform
 * the verification operation asynchronously.
 *
 * Returns: the appropriate #GTlsCertificateFlags which represents the
 * result of verification.
 *
 * Since: 2.30
 */
GTlsCertificateFlags
g_tls_database_verify_chain (GTlsDatabase           *self,
                             GTlsCertificate        *chain,
                             const gchar            *purpose,
                             GSocketConnectable     *identity,
                             GTlsInteraction        *interaction,
                             GTlsDatabaseVerifyFlags flags,
                             GCancellable           *cancellable,
                             GError                **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (G_IS_TLS_DATABASE (self),
                        G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (chain),
                        G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (purpose, G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction),
                        G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (identity == NULL || G_IS_SOCKET_CONNECTABLE (identity),
                        G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (error == NULL || *error == NULL, G_TLS_CERTIFICATE_GENERIC_ERROR);

  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->verify_chain,
                        G_TLS_CERTIFICATE_GENERIC_ERROR);

  return G_TLS_DATABASE_GET_CLASS (self)->verify_chain (self,
                                                        chain,
                                                        purpose,
                                                        identity,
                                                        interaction,
                                                        flags,
                                                        cancellable,
                                                        error);
}

/**
 * g_tls_database_verify_chain_async:
 * @self: a #GTlsDatabase
 * @chain: a #GTlsCertificate chain
 * @purpose: the purpose that this certificate chain will be used for.
 * @identity: (nullable): the expected peer identity
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: additional verify flags
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the operation completes
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously determines the validity of a certificate chain after
 * looking up and adding any missing certificates to the chain. See
 * g_tls_database_verify_chain() for more information.
 *
 * Since: 2.30
 */
void
g_tls_database_verify_chain_async (GTlsDatabase           *self,
                                   GTlsCertificate        *chain,
                                   const gchar            *purpose,
                                   GSocketConnectable     *identity,
                                   GTlsInteraction        *interaction,
                                   GTlsDatabaseVerifyFlags flags,
                                   GCancellable           *cancellable,
                                   GAsyncReadyCallback     callback,
                                   gpointer                user_data)
{
  g_return_if_fail (G_IS_TLS_DATABASE (self));
  g_return_if_fail (G_IS_TLS_CERTIFICATE (chain));
  g_return_if_fail (purpose != NULL);
  g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (identity == NULL || G_IS_SOCKET_CONNECTABLE (identity));
  g_return_if_fail (callback != NULL);

  g_return_if_fail (G_TLS_DATABASE_GET_CLASS (self)->verify_chain_async);
  G_TLS_DATABASE_GET_CLASS (self)->verify_chain_async (self,
                                                       chain,
                                                       purpose,
                                                       identity,
                                                       interaction,
                                                       flags,
                                                       cancellable,
                                                       callback,
                                                       user_data);
}

/**
 * g_tls_database_verify_chain_finish:
 * @self: a #GTlsDatabase
 * @result: a #GAsyncResult.
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous verify chain operation. See
 * g_tls_database_verify_chain() for more information.
 *
 * If @chain is found to be valid, then the return value will be 0. If
 * @chain is found to be invalid, then the return value will indicate
 * the problems found. If the function is unable to determine whether
 * @chain is valid or not (eg, because @cancellable is triggered
 * before it completes) then the return value will be
 * %G_TLS_CERTIFICATE_GENERIC_ERROR and @error will be set
 * accordingly. @error is not set when @chain is successfully analyzed
 * but found to be invalid.
 *
 * Returns: the appropriate #GTlsCertificateFlags which represents the
 * result of verification.
 *
 * Since: 2.30
 */
GTlsCertificateFlags
g_tls_database_verify_chain_finish (GTlsDatabase          *self,
                                    GAsyncResult          *result,
                                    GError               **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (error == NULL || *error == NULL, G_TLS_CERTIFICATE_GENERIC_ERROR);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->verify_chain_finish,
                        G_TLS_CERTIFICATE_GENERIC_ERROR);
  return G_TLS_DATABASE_GET_CLASS (self)->verify_chain_finish (self,
                                                               result,
                                                               error);
}

/**
 * g_tls_database_create_certificate_handle:
 * @self: a #GTlsDatabase
 * @certificate: certificate for which to create a handle.
 *
 * Create a handle string for the certificate. The database will only be able
 * to create a handle for certificates that originate from the database. In
 * cases where the database cannot create a handle for a certificate, %NULL
 * will be returned.
 *
 * This handle should be stable across various instances of the application,
 * and between applications. If a certificate is modified in the database,
 * then it is not guaranteed that this handle will continue to point to it.
 *
 * Returns: (nullable): a newly allocated string containing the
 * handle.
 *
 * Since: 2.30
 */
gchar*
g_tls_database_create_certificate_handle (GTlsDatabase            *self,
                                          GTlsCertificate         *certificate)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->create_certificate_handle, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->create_certificate_handle (self,
                                                                     certificate);
}

/**
 * g_tls_database_lookup_certificate_for_handle:
 * @self: a #GTlsDatabase
 * @handle: a certificate handle
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: Flags which affect the lookup.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: (nullable): a #GError, or %NULL
 *
 * Lookup a certificate by its handle.
 *
 * The handle should have been created by calling
 * g_tls_database_create_certificate_handle() on a #GTlsDatabase object of
 * the same TLS backend. The handle is designed to remain valid across
 * instantiations of the database.
 *
 * If the handle is no longer valid, or does not point to a certificate in
 * this database, then %NULL will be returned.
 *
 * This function can block, use g_tls_database_lookup_certificate_for_handle_async() to perform
 * the lookup operation asynchronously.
 *
 * Returns: (transfer full) (nullable): a newly allocated
 * #GTlsCertificate, or %NULL. Use g_object_unref() to release the certificate.
 *
 * Since: 2.30
 */
GTlsCertificate*
g_tls_database_lookup_certificate_for_handle (GTlsDatabase            *self,
                                              const gchar             *handle,
                                              GTlsInteraction         *interaction,
                                              GTlsDatabaseLookupFlags  flags,
                                              GCancellable            *cancellable,
                                              GError                 **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (handle != NULL, NULL);
  g_return_val_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_for_handle, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_for_handle (self,
                                                                         handle,
                                                                         interaction,
                                                                         flags,
                                                                         cancellable,
                                                                         error);
}


/**
 * g_tls_database_lookup_certificate_for_handle_async:
 * @self: a #GTlsDatabase
 * @handle: a certificate handle
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: Flags which affect the lookup.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the operation completes
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously lookup a certificate by its handle in the database. See
 * g_tls_database_lookup_certificate_for_handle() for more information.
 *
 * Since: 2.30
 */
void
g_tls_database_lookup_certificate_for_handle_async (GTlsDatabase            *self,
                                                    const gchar             *handle,
                                                    GTlsInteraction         *interaction,
                                                    GTlsDatabaseLookupFlags  flags,
                                                    GCancellable            *cancellable,
                                                    GAsyncReadyCallback      callback,
                                                    gpointer                 user_data)
{
  g_return_if_fail (G_IS_TLS_DATABASE (self));
  g_return_if_fail (handle != NULL);
  g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_for_handle_async);
  G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_for_handle_async (self,
                                                                               handle,
                                                                               interaction,
                                                                               flags,
                                                                               cancellable,
                                                                               callback,
                                                                               user_data);
}

/**
 * g_tls_database_lookup_certificate_for_handle_finish:
 * @self: a #GTlsDatabase
 * @result: a #GAsyncResult.
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous lookup of a certificate by its handle. See
 * g_tls_database_lookup_certificate_by_handle() for more information.
 *
 * If the handle is no longer valid, or does not point to a certificate in
 * this database, then %NULL will be returned.
 *
 * Returns: (transfer full): a newly allocated #GTlsCertificate object.
 * Use g_object_unref() to release the certificate.
 *
 * Since: 2.30
 */
GTlsCertificate*
g_tls_database_lookup_certificate_for_handle_finish (GTlsDatabase            *self,
                                                     GAsyncResult            *result,
                                                     GError                 **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_for_handle_finish, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_for_handle_finish (self,
                                                                                result,
                                                                                error);
}

/**
 * g_tls_database_lookup_certificate_issuer:
 * @self: a #GTlsDatabase
 * @certificate: a #GTlsCertificate
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: flags which affect the lookup operation
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: (nullable): a #GError, or %NULL
 *
 * Lookup the issuer of @certificate in the database.
 *
 * The %issuer property
 * of @certificate is not modified, and the two certificates are not hooked
 * into a chain.
 *
 * This function can block, use g_tls_database_lookup_certificate_issuer_async() to perform
 * the lookup operation asynchronously.
 *
 * Returns: (transfer full): a newly allocated issuer #GTlsCertificate,
 * or %NULL. Use g_object_unref() to release the certificate.
 *
 * Since: 2.30
 */
GTlsCertificate*
g_tls_database_lookup_certificate_issuer (GTlsDatabase           *self,
                                          GTlsCertificate        *certificate,
                                          GTlsInteraction        *interaction,
                                          GTlsDatabaseLookupFlags flags,
                                          GCancellable           *cancellable,
                                          GError                **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);
  g_return_val_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_issuer, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_issuer (self,
                                                                     certificate,
                                                                     interaction,
                                                                     flags,
                                                                     cancellable,
                                                                     error);
}

/**
 * g_tls_database_lookup_certificate_issuer_async:
 * @self: a #GTlsDatabase
 * @certificate: a #GTlsCertificate
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: flags which affect the lookup operation
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the operation completes
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously lookup the issuer of @certificate in the database. See
 * g_tls_database_lookup_certificate_issuer() for more information.
 *
 * Since: 2.30
 */
void
g_tls_database_lookup_certificate_issuer_async (GTlsDatabase           *self,
                                                GTlsCertificate        *certificate,
                                                GTlsInteraction        *interaction,
                                                GTlsDatabaseLookupFlags flags,
                                                GCancellable           *cancellable,
                                                GAsyncReadyCallback     callback,
                                                gpointer                user_data)
{
  g_return_if_fail (G_IS_TLS_DATABASE (self));
  g_return_if_fail (G_IS_TLS_CERTIFICATE (certificate));
  g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);
  g_return_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_issuer_async);
  G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_issuer_async (self,
                                                        certificate,
                                                        interaction,
                                                        flags,
                                                        cancellable,
                                                        callback,
                                                        user_data);
}

/**
 * g_tls_database_lookup_certificate_issuer_finish:
 * @self: a #GTlsDatabase
 * @result: a #GAsyncResult.
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous lookup issuer operation. See
 * g_tls_database_lookup_certificate_issuer() for more information.
 *
 * Returns: (transfer full): a newly allocated issuer #GTlsCertificate,
 * or %NULL. Use g_object_unref() to release the certificate.
 *
 * Since: 2.30
 */
GTlsCertificate*
g_tls_database_lookup_certificate_issuer_finish (GTlsDatabase          *self,
                                                 GAsyncResult          *result,
                                                 GError               **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_issuer_finish, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->lookup_certificate_issuer_finish (self,
                                                                result,
                                                                error);
}

/**
 * g_tls_database_lookup_certificates_issued_by:
 * @self: a #GTlsDatabase
 * @issuer_raw_dn: a #GByteArray which holds the DER encoded issuer DN.
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: Flags which affect the lookup operation.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: (nullable): a #GError, or %NULL
 *
 * Lookup certificates issued by this issuer in the database.
 *
 * This function can block, use g_tls_database_lookup_certificates_issued_by_async() to perform
 * the lookup operation asynchronously.
 *
 * Returns: (transfer full) (element-type GTlsCertificate): a newly allocated list of #GTlsCertificate
 * objects. Use g_object_unref() on each certificate, and g_list_free() on the release the list.
 *
 * Since: 2.30
 */
GList*
g_tls_database_lookup_certificates_issued_by (GTlsDatabase           *self,
                                              GByteArray             *issuer_raw_dn,
                                              GTlsInteraction        *interaction,
                                              GTlsDatabaseLookupFlags flags,
                                              GCancellable           *cancellable,
                                              GError                **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (issuer_raw_dn, NULL);
  g_return_val_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificates_issued_by, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->lookup_certificates_issued_by (self,
                                                                         issuer_raw_dn,
                                                                         interaction,
                                                                         flags,
                                                                         cancellable,
                                                                         error);
}

/**
 * g_tls_database_lookup_certificates_issued_by_async:
 * @self: a #GTlsDatabase
 * @issuer_raw_dn: a #GByteArray which holds the DER encoded issuer DN.
 * @interaction: (nullable): used to interact with the user if necessary
 * @flags: Flags which affect the lookup operation.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the operation completes
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously lookup certificates issued by this issuer in the database. See
 * g_tls_database_lookup_certificates_issued_by() for more information.
 *
 * The database may choose to hold a reference to the issuer byte array for the duration
 * of of this asynchronous operation. The byte array should not be modified during
 * this time.
 *
 * Since: 2.30
 */
void
g_tls_database_lookup_certificates_issued_by_async (GTlsDatabase           *self,
                                                    GByteArray             *issuer_raw_dn,
                                                    GTlsInteraction        *interaction,
                                                    GTlsDatabaseLookupFlags flags,
                                                    GCancellable           *cancellable,
                                                    GAsyncReadyCallback     callback,
                                                    gpointer                user_data)
{
  g_return_if_fail (G_IS_TLS_DATABASE (self));
  g_return_if_fail (issuer_raw_dn != NULL);
  g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);
  g_return_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificates_issued_by_async);
  G_TLS_DATABASE_GET_CLASS (self)->lookup_certificates_issued_by_async (self,
                                                                        issuer_raw_dn,
                                                                        interaction,
                                                                        flags,
                                                                        cancellable,
                                                                        callback,
                                                                        user_data);
}

/**
 * g_tls_database_lookup_certificates_issued_by_finish:
 * @self: a #GTlsDatabase
 * @result: a #GAsyncResult.
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous lookup of certificates. See
 * g_tls_database_lookup_certificates_issued_by() for more information.
 *
 * Returns: (transfer full) (element-type GTlsCertificate): a newly allocated list of #GTlsCertificate
 * objects. Use g_object_unref() on each certificate, and g_list_free() on the release the list.
 *
 * Since: 2.30
 */
GList*
g_tls_database_lookup_certificates_issued_by_finish (GTlsDatabase          *self,
                                                     GAsyncResult          *result,
                                                     GError               **error)
{
  g_return_val_if_fail (G_IS_TLS_DATABASE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_TLS_DATABASE_GET_CLASS (self)->lookup_certificates_issued_by_finish, NULL);
  return G_TLS_DATABASE_GET_CLASS (self)->lookup_certificates_issued_by_finish (self,
                                                                                result,
                                                                                error);
}
