/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2008 Red Hat, Inc.
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

#include "config.h"
#include "gsocketaddressenumerator.h"
#include "glibintl.h"

#include "gtask.h"


G_DEFINE_ABSTRACT_TYPE (GSocketAddressEnumerator, g_socket_address_enumerator, G_TYPE_OBJECT)

static void            g_socket_address_enumerator_real_next_async  (GSocketAddressEnumerator  *enumerator,
								     GCancellable              *cancellable,
								     GAsyncReadyCallback        callback,
								     gpointer                   user_data);
static GSocketAddress *g_socket_address_enumerator_real_next_finish (GSocketAddressEnumerator  *enumerator,
								     GAsyncResult              *result,
								     GError                   **error);

static void
g_socket_address_enumerator_init (GSocketAddressEnumerator *enumerator)
{
}

static void
g_socket_address_enumerator_class_init (GSocketAddressEnumeratorClass *enumerator_class)
{
  enumerator_class->next_async = g_socket_address_enumerator_real_next_async;
  enumerator_class->next_finish = g_socket_address_enumerator_real_next_finish;
}

/**
 * g_socket_address_enumerator_next:
 * @enumerator: a #GSocketAddressEnumerator
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: a #GError.
 *
 * Retrieves the next #GSocketAddress from @enumerator. Note that this
 * may block for some amount of time. (Eg, a #GNetworkAddress may need
 * to do a DNS lookup before it can return an address.) Use
 * g_socket_address_enumerator_next_async() if you need to avoid
 * blocking.
 *
 * If @enumerator is expected to yield addresses, but for some reason
 * is unable to (eg, because of a DNS error), then the first call to
 * g_socket_address_enumerator_next() will return an appropriate error
 * in *@error. However, if the first call to
 * g_socket_address_enumerator_next() succeeds, then any further
 * internal errors (other than @cancellable being triggered) will be
 * ignored.
 *
 * Returns: (transfer full): a #GSocketAddress (owned by the caller), or %NULL on
 *     error (in which case *@error will be set) or if there are no
 *     more addresses.
 */
GSocketAddress *
g_socket_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
				  GCancellable              *cancellable,
				  GError                   **error)
{
  GSocketAddressEnumeratorClass *klass;

  g_return_val_if_fail (G_IS_SOCKET_ADDRESS_ENUMERATOR (enumerator), NULL);

  klass = G_SOCKET_ADDRESS_ENUMERATOR_GET_CLASS (enumerator);

  return (* klass->next) (enumerator, cancellable, error);
}

/* Default implementation just calls the synchronous method; this can
 * be used if the implementation already knows all of its addresses,
 * and so the synchronous method will never block.
 */
static void
g_socket_address_enumerator_real_next_async (GSocketAddressEnumerator *enumerator,
					     GCancellable             *cancellable,
					     GAsyncReadyCallback       callback,
					     gpointer                  user_data)
{
  GTask *task;
  GSocketAddress *address;
  GError *error = NULL;

  task = g_task_new (enumerator, NULL, callback, user_data);
  g_task_set_source_tag (task, g_socket_address_enumerator_real_next_async);

  address = g_socket_address_enumerator_next (enumerator, cancellable, &error);
  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, address, g_object_unref);

  g_object_unref (task);
}

/**
 * g_socket_address_enumerator_next_async:
 * @enumerator: a #GSocketAddressEnumerator
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request
 *     is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously retrieves the next #GSocketAddress from @enumerator
 * and then calls @callback, which must call
 * g_socket_address_enumerator_next_finish() to get the result.
 */
void
g_socket_address_enumerator_next_async (GSocketAddressEnumerator *enumerator,
					GCancellable             *cancellable,
					GAsyncReadyCallback       callback,
					gpointer                  user_data)
{
  GSocketAddressEnumeratorClass *klass;

  g_return_if_fail (G_IS_SOCKET_ADDRESS_ENUMERATOR (enumerator));

  klass = G_SOCKET_ADDRESS_ENUMERATOR_GET_CLASS (enumerator);

  (* klass->next_async) (enumerator, cancellable, callback, user_data);
}

static GSocketAddress *
g_socket_address_enumerator_real_next_finish (GSocketAddressEnumerator  *enumerator,
					      GAsyncResult              *result,
					      GError                   **error)
{
  g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * g_socket_address_enumerator_next_finish:
 * @enumerator: a #GSocketAddressEnumerator
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Retrieves the result of a completed call to
 * g_socket_address_enumerator_next_async(). See
 * g_socket_address_enumerator_next() for more information about
 * error handling.
 *
 * Returns: (transfer full): a #GSocketAddress (owned by the caller), or %NULL on
 *     error (in which case *@error will be set) or if there are no
 *     more addresses.
 */
GSocketAddress *
g_socket_address_enumerator_next_finish (GSocketAddressEnumerator  *enumerator,
					 GAsyncResult              *result,
					 GError                   **error)
{
  GSocketAddressEnumeratorClass *klass;

  g_return_val_if_fail (G_IS_SOCKET_ADDRESS_ENUMERATOR (enumerator), NULL);

  klass = G_SOCKET_ADDRESS_ENUMERATOR_GET_CLASS (enumerator);

  return (* klass->next_finish) (enumerator, result, error);
}
