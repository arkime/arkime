/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
#include <glib.h>
#include "glibintl.h"

#include "gresolver.h"
#include "gnetworkingprivate.h"
#include "gasyncresult.h"
#include "ginetaddress.h"
#include "gtask.h"
#include "gsrvtarget.h"
#include "gthreadedresolver.h"
#include "gioerror.h"

#ifdef G_OS_UNIX
#include <sys/stat.h>
#endif

#include <stdlib.h>


/**
 * SECTION:gresolver
 * @short_description: Asynchronous and cancellable DNS resolver
 * @include: gio/gio.h
 *
 * #GResolver provides cancellable synchronous and asynchronous DNS
 * resolution, for hostnames (g_resolver_lookup_by_address(),
 * g_resolver_lookup_by_name() and their async variants) and SRV
 * (service) records (g_resolver_lookup_service()).
 *
 * #GNetworkAddress and #GNetworkService provide wrappers around
 * #GResolver functionality that also implement #GSocketConnectable,
 * making it easy to connect to a remote host/service.
 */

enum {
  RELOAD,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _GResolverPrivate {
#ifdef G_OS_UNIX
  time_t resolv_conf_timestamp;
#else
  int dummy;
#endif
};

/**
 * GResolver:
 *
 * The object that handles DNS resolution. Use g_resolver_get_default()
 * to get the default resolver.
 *
 * This is an abstract type; subclasses of it implement different resolvers for
 * different platforms and situations.
 */
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GResolver, g_resolver, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (GResolver)
                                  g_networking_init ();)

static GList *
srv_records_to_targets (GList *records)
{
  const gchar *hostname;
  guint16 port, priority, weight;
  GSrvTarget *target;
  GList *l;

  for (l = records; l != NULL; l = g_list_next (l))
    {
      g_variant_get (l->data, "(qqq&s)", &priority, &weight, &port, &hostname);
      target = g_srv_target_new (hostname, port, priority, weight);
      g_variant_unref (l->data);
      l->data = target;
    }

  return g_srv_target_list_sort (records);
}

static GList *
g_resolver_real_lookup_service (GResolver            *resolver,
                                const gchar          *rrname,
                                GCancellable         *cancellable,
                                GError              **error)
{
  GList *records;

  records = G_RESOLVER_GET_CLASS (resolver)->lookup_records (resolver,
                                                             rrname,
                                                             G_RESOLVER_RECORD_SRV,
                                                             cancellable,
                                                             error);

  return srv_records_to_targets (records);
}

static void
g_resolver_real_lookup_service_async (GResolver            *resolver,
                                      const gchar          *rrname,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  G_RESOLVER_GET_CLASS (resolver)->lookup_records_async (resolver,
                                                         rrname,
                                                         G_RESOLVER_RECORD_SRV,
                                                         cancellable,
                                                         callback,
                                                         user_data);
}

static GList *
g_resolver_real_lookup_service_finish (GResolver            *resolver,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  GList *records;

  records = G_RESOLVER_GET_CLASS (resolver)->lookup_records_finish (resolver,
                                                                    result,
                                                                    error);

  return srv_records_to_targets (records);
}

static void
g_resolver_class_init (GResolverClass *resolver_class)
{
  /* Automatically pass these over to the lookup_records methods */
  resolver_class->lookup_service = g_resolver_real_lookup_service;
  resolver_class->lookup_service_async = g_resolver_real_lookup_service_async;
  resolver_class->lookup_service_finish = g_resolver_real_lookup_service_finish;

  /**
   * GResolver::reload:
   * @resolver: a #GResolver
   *
   * Emitted when the resolver notices that the system resolver
   * configuration has changed.
   **/
  signals[RELOAD] =
    g_signal_new (I_("reload"),
		  G_TYPE_RESOLVER,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GResolverClass, reload),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
g_resolver_init (GResolver *resolver)
{
#ifdef G_OS_UNIX
  struct stat st;
#endif

  resolver->priv = g_resolver_get_instance_private (resolver);

#ifdef G_OS_UNIX
  if (stat (_PATH_RESCONF, &st) == 0)
    resolver->priv->resolv_conf_timestamp = st.st_mtime;
#endif
}

G_LOCK_DEFINE_STATIC (default_resolver);
static GResolver *default_resolver;

/**
 * g_resolver_get_default:
 *
 * Gets the default #GResolver. You should unref it when you are done
 * with it. #GResolver may use its reference count as a hint about how
 * many threads it should allocate for concurrent DNS resolutions.
 *
 * Returns: (transfer full): the default #GResolver.
 *
 * Since: 2.22
 */
GResolver *
g_resolver_get_default (void)
{
  GResolver *ret;

  G_LOCK (default_resolver);
  if (!default_resolver)
    default_resolver = g_object_new (G_TYPE_THREADED_RESOLVER, NULL);
  ret = g_object_ref (default_resolver);
  G_UNLOCK (default_resolver);

  return ret;
}

/**
 * g_resolver_set_default:
 * @resolver: the new default #GResolver
 *
 * Sets @resolver to be the application's default resolver (reffing
 * @resolver, and unreffing the previous default resolver, if any).
 * Future calls to g_resolver_get_default() will return this resolver.
 *
 * This can be used if an application wants to perform any sort of DNS
 * caching or "pinning"; it can implement its own #GResolver that
 * calls the original default resolver for DNS operations, and
 * implements its own cache policies on top of that, and then set
 * itself as the default resolver for all later code to use.
 *
 * Since: 2.22
 */
void
g_resolver_set_default (GResolver *resolver)
{
  G_LOCK (default_resolver);
  if (default_resolver)
    g_object_unref (default_resolver);
  default_resolver = g_object_ref (resolver);
  G_UNLOCK (default_resolver);
}

/* Bionic has res_init() but it's not in any header */
#ifdef __BIONIC__
int res_init (void);
#endif

static void
g_resolver_maybe_reload (GResolver *resolver)
{
#ifdef G_OS_UNIX
  struct stat st;

  if (stat (_PATH_RESCONF, &st) == 0)
    {
      if (st.st_mtime != resolver->priv->resolv_conf_timestamp)
        {
          resolver->priv->resolv_conf_timestamp = st.st_mtime;
#ifdef HAVE_RES_INIT
          res_init ();
#endif
          g_signal_emit (resolver, signals[RELOAD], 0);
        }
    }
#endif
}

/* filter out duplicates, cf. https://bugzilla.gnome.org/show_bug.cgi?id=631379 */
static void
remove_duplicates (GList *addrs)
{
  GList *l;
  GList *ll;
  GList *lll;

  /* TODO: if this is too slow (it's O(n^2) but n is typically really
   * small), we can do something more clever but note that we must not
   * change the order of elements...
   */
  for (l = addrs; l != NULL; l = l->next)
    {
      GInetAddress *address = G_INET_ADDRESS (l->data);
      for (ll = l->next; ll != NULL; ll = lll)
        {
          GInetAddress *other_address = G_INET_ADDRESS (ll->data);
          lll = ll->next;
          if (g_inet_address_equal (address, other_address))
            {
              g_object_unref (other_address);
              /* we never return the first element */
              g_warn_if_fail (g_list_delete_link (addrs, ll) == addrs);
            }
        }
    }
}

/* Note that this does not follow the "FALSE means @error is set"
 * convention. The return value tells the caller whether it should
 * return @addrs and @error to the caller right away, or if it should
 * continue and trying to resolve the name as a hostname.
 */
static gboolean
handle_ip_address (const char  *hostname,
                   GList      **addrs,
                   GError     **error)
{
  GInetAddress *addr;

#ifndef G_OS_WIN32
  struct in_addr ip4addr;
#endif

  addr = g_inet_address_new_from_string (hostname);
  if (addr)
    {
      *addrs = g_list_append (NULL, addr);
      return TRUE;
    }

  *addrs = NULL;

#ifdef G_OS_WIN32

  /* Reject IPv6 addresses that have brackets ('[' or ']') and/or port numbers,
   * as no valid addresses should contain these at this point.
   * Non-standard IPv4 addresses would be rejected during the call to
   * getaddrinfo() later.
   */
  if (strrchr (hostname, '[') != NULL ||
      strrchr (hostname, ']') != NULL)
#else

  /* Reject non-standard IPv4 numbers-and-dots addresses.
   * g_inet_address_new_from_string() will have accepted any "real" IP
   * address, so if inet_aton() succeeds, then it's an address we want
   * to reject.
   */
  if (inet_aton (hostname, &ip4addr))
#endif
    {
      g_set_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND,
                   _("Error resolving “%s”: %s"),
                   hostname, gai_strerror (EAI_NONAME));
      return TRUE;
    }

  return FALSE;
}

/**
 * g_resolver_lookup_by_name:
 * @resolver: a #GResolver
 * @hostname: the hostname to look up
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously resolves @hostname to determine its associated IP
 * address(es). @hostname may be an ASCII-only or UTF-8 hostname, or
 * the textual form of an IP address (in which case this just becomes
 * a wrapper around g_inet_address_new_from_string()).
 *
 * On success, g_resolver_lookup_by_name() will return a non-empty #GList of
 * #GInetAddress, sorted in order of preference and guaranteed to not
 * contain duplicates. That is, if using the result to connect to
 * @hostname, you should attempt to connect to the first address
 * first, then the second if the first fails, etc. If you are using
 * the result to listen on a socket, it is appropriate to add each
 * result using e.g. g_socket_listener_add_address().
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to a
 * value from #GResolverError and %NULL will be returned.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * If you are planning to connect to a socket on the resolved IP
 * address, it may be easier to create a #GNetworkAddress and use its
 * #GSocketConnectable interface.
 *
 * Returns: (element-type GInetAddress) (transfer full): a non-empty #GList
 * of #GInetAddress, or %NULL on error. You
 * must unref each of the addresses and free the list when you are
 * done with it. (You can use g_resolver_free_addresses() to do this.)
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_by_name (GResolver     *resolver,
                           const gchar   *hostname,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GList *addrs;
  gchar *ascii_hostname = NULL;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (hostname != NULL, NULL);

  /* Check if @hostname is just an IP address */
  if (handle_ip_address (hostname, &addrs, error))
    return addrs;

  if (g_hostname_is_non_ascii (hostname))
    hostname = ascii_hostname = g_hostname_to_ascii (hostname);

  if (!hostname)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Invalid hostname"));
      return NULL;
    }

  g_resolver_maybe_reload (resolver);
  addrs = G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_name (resolver, hostname, cancellable, error);

  remove_duplicates (addrs);

  g_free (ascii_hostname);
  return addrs;
}

/**
 * g_resolver_lookup_by_name_async:
 * @resolver: a #GResolver
 * @hostname: the hostname to look up the address of
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously resolving @hostname to determine its
 * associated IP address(es), and eventually calls @callback, which
 * must call g_resolver_lookup_by_name_finish() to get the result.
 * See g_resolver_lookup_by_name() for more details.
 *
 * Since: 2.22
 */
void
g_resolver_lookup_by_name_async (GResolver           *resolver,
                                 const gchar         *hostname,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  gchar *ascii_hostname = NULL;
  GList *addrs;
  GError *error = NULL;

  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (hostname != NULL);

  /* Check if @hostname is just an IP address */
  if (handle_ip_address (hostname, &addrs, &error))
    {
      GTask *task;

      task = g_task_new (resolver, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_resolver_lookup_by_name_async);
      if (addrs)
        g_task_return_pointer (task, addrs, (GDestroyNotify) g_resolver_free_addresses);
      else
        g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (g_hostname_is_non_ascii (hostname))
    hostname = ascii_hostname = g_hostname_to_ascii (hostname);

  if (!hostname)
    {
      GTask *task;

      g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Invalid hostname"));
      task = g_task_new (resolver, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_resolver_lookup_by_name_async);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_name_async (resolver, hostname, cancellable, callback, user_data);

  g_free (ascii_hostname);
}

/**
 * g_resolver_lookup_by_name_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a call to
 * g_resolver_lookup_by_name_async().
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Returns: (element-type GInetAddress) (transfer full): a #GList
 * of #GInetAddress, or %NULL on error. See g_resolver_lookup_by_name()
 * for more details.
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_by_name_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  GList *addrs;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;
  else if (g_async_result_is_tagged (result, g_resolver_lookup_by_name_async))
    {
      /* Handle the stringified-IP-addr case */
      return g_task_propagate_pointer (G_TASK (result), error);
    }

  addrs = G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_name_finish (resolver, result, error);

  remove_duplicates (addrs);

  return addrs;
}

/**
 * g_resolver_free_addresses: (skip)
 * @addresses: a #GList of #GInetAddress
 *
 * Frees @addresses (which should be the return value from
 * g_resolver_lookup_by_name() or g_resolver_lookup_by_name_finish()).
 * (This is a convenience method; you can also simply free the results
 * by hand.)
 *
 * Since: 2.22
 */
void
g_resolver_free_addresses (GList *addresses)
{
  GList *a;

  for (a = addresses; a; a = a->next)
    g_object_unref (a->data);
  g_list_free (addresses);
}

/**
 * g_resolver_lookup_by_address:
 * @resolver: a #GResolver
 * @address: the address to reverse-resolve
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously reverse-resolves @address to determine its
 * associated hostname.
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to
 * a value from #GResolverError.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * Returns: a hostname (either ASCII-only, or in ASCII-encoded
 *     form), or %NULL on error.
 *
 * Since: 2.22
 */
gchar *
g_resolver_lookup_by_address (GResolver     *resolver,
                              GInetAddress  *address,
                              GCancellable  *cancellable,
                              GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), NULL);

  g_resolver_maybe_reload (resolver);
  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_address (resolver, address, cancellable, error);
}

/**
 * g_resolver_lookup_by_address_async:
 * @resolver: a #GResolver
 * @address: the address to reverse-resolve
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously reverse-resolving @address to determine its
 * associated hostname, and eventually calls @callback, which must
 * call g_resolver_lookup_by_address_finish() to get the final result.
 *
 * Since: 2.22
 */
void
g_resolver_lookup_by_address_async (GResolver           *resolver,
                                    GInetAddress        *address,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (G_IS_INET_ADDRESS (address));

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_address_async (resolver, address, cancellable, callback, user_data);
}

/**
 * g_resolver_lookup_by_address_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a previous call to
 * g_resolver_lookup_by_address_async().
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Returns: a hostname (either ASCII-only, or in ASCII-encoded
 * form), or %NULL on error.
 *
 * Since: 2.22
 */
gchar *
g_resolver_lookup_by_address_finish (GResolver     *resolver,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;

  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_by_address_finish (resolver, result, error);
}

static gchar *
g_resolver_get_service_rrname (const char *service,
                               const char *protocol,
                               const char *domain)
{
  gchar *rrname, *ascii_domain = NULL;

  if (g_hostname_is_non_ascii (domain))
    domain = ascii_domain = g_hostname_to_ascii (domain);
  if (!domain)
    return NULL;

  rrname = g_strdup_printf ("_%s._%s.%s", service, protocol, domain);

  g_free (ascii_domain);
  return rrname;
}

/**
 * g_resolver_lookup_service:
 * @resolver: a #GResolver
 * @service: the service type to look up (eg, "ldap")
 * @protocol: the networking protocol to use for @service (eg, "tcp")
 * @domain: the DNS domain to look up the service in
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously performs a DNS SRV lookup for the given @service and
 * @protocol in the given @domain and returns an array of #GSrvTarget.
 * @domain may be an ASCII-only or UTF-8 hostname. Note also that the
 * @service and @protocol arguments do not include the leading underscore
 * that appears in the actual DNS entry.
 *
 * On success, g_resolver_lookup_service() will return a non-empty #GList of
 * #GSrvTarget, sorted in order of preference. (That is, you should
 * attempt to connect to the first target first, then the second if
 * the first fails, etc.)
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to
 * a value from #GResolverError and %NULL will be returned.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * If you are planning to connect to the service, it is usually easier
 * to create a #GNetworkService and use its #GSocketConnectable
 * interface.
 *
 * Returns: (element-type GSrvTarget) (transfer full): a non-empty #GList of
 * #GSrvTarget, or %NULL on error. You must free each of the targets and the
 * list when you are done with it. (You can use g_resolver_free_targets() to do
 * this.)
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_service (GResolver     *resolver,
                           const gchar   *service,
                           const gchar   *protocol,
                           const gchar   *domain,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GList *targets;
  gchar *rrname;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (service != NULL, NULL);
  g_return_val_if_fail (protocol != NULL, NULL);
  g_return_val_if_fail (domain != NULL, NULL);

  rrname = g_resolver_get_service_rrname (service, protocol, domain);
  if (!rrname)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Invalid domain"));
      return NULL;
    }

  g_resolver_maybe_reload (resolver);
  targets = G_RESOLVER_GET_CLASS (resolver)->
    lookup_service (resolver, rrname, cancellable, error);

  g_free (rrname);
  return targets;
}

/**
 * g_resolver_lookup_service_async:
 * @resolver: a #GResolver
 * @service: the service type to look up (eg, "ldap")
 * @protocol: the networking protocol to use for @service (eg, "tcp")
 * @domain: the DNS domain to look up the service in
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously performing a DNS SRV lookup for the given
 * @service and @protocol in the given @domain, and eventually calls
 * @callback, which must call g_resolver_lookup_service_finish() to
 * get the final result. See g_resolver_lookup_service() for more
 * details.
 *
 * Since: 2.22
 */
void
g_resolver_lookup_service_async (GResolver           *resolver,
                                 const gchar         *service,
                                 const gchar         *protocol,
                                 const gchar         *domain,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  gchar *rrname;

  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (service != NULL);
  g_return_if_fail (protocol != NULL);
  g_return_if_fail (domain != NULL);

  rrname = g_resolver_get_service_rrname (service, protocol, domain);
  if (!rrname)
    {
      g_task_report_new_error (resolver, callback, user_data,
                               g_resolver_lookup_service_async,
                               G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Invalid domain"));
      return;
    }

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_service_async (resolver, rrname, cancellable, callback, user_data);

  g_free (rrname);
}

/**
 * g_resolver_lookup_service_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a previous call to
 * g_resolver_lookup_service_async().
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Returns: (element-type GSrvTarget) (transfer full): a non-empty #GList of
 * #GSrvTarget, or %NULL on error. See g_resolver_lookup_service() for more
 * details.
 *
 * Since: 2.22
 */
GList *
g_resolver_lookup_service_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;

  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_service_finish (resolver, result, error);
}

/**
 * g_resolver_free_targets: (skip)
 * @targets: a #GList of #GSrvTarget
 *
 * Frees @targets (which should be the return value from
 * g_resolver_lookup_service() or g_resolver_lookup_service_finish()).
 * (This is a convenience method; you can also simply free the
 * results by hand.)
 *
 * Since: 2.22
 */
void
g_resolver_free_targets (GList *targets)
{
  GList *t;

  for (t = targets; t; t = t->next)
    g_srv_target_free (t->data);
  g_list_free (targets);
}

/**
 * g_resolver_lookup_records:
 * @resolver: a #GResolver
 * @rrname: the DNS name to lookup the record for
 * @record_type: the type of DNS record to lookup
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronously performs a DNS record lookup for the given @rrname and returns
 * a list of records as #GVariant tuples. See #GResolverRecordType for
 * information on what the records contain for each @record_type.
 *
 * If the DNS resolution fails, @error (if non-%NULL) will be set to
 * a value from #GResolverError and %NULL will be returned.
 *
 * If @cancellable is non-%NULL, it can be used to cancel the
 * operation, in which case @error (if non-%NULL) will be set to
 * %G_IO_ERROR_CANCELLED.
 *
 * Returns: (element-type GVariant) (transfer full): a non-empty #GList of
 * #GVariant, or %NULL on error. You must free each of the records and the list
 * when you are done with it. (You can use g_list_free_full() with
 * g_variant_unref() to do this.)
 *
 * Since: 2.34
 */
GList *
g_resolver_lookup_records (GResolver            *resolver,
                           const gchar          *rrname,
                           GResolverRecordType   record_type,
                           GCancellable         *cancellable,
                           GError              **error)
{
  GList *records;

  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  g_return_val_if_fail (rrname != NULL, NULL);

  g_resolver_maybe_reload (resolver);
  records = G_RESOLVER_GET_CLASS (resolver)->
    lookup_records (resolver, rrname, record_type, cancellable, error);

  return records;
}

/**
 * g_resolver_lookup_records_async:
 * @resolver: a #GResolver
 * @rrname: the DNS name to lookup the record for
 * @record_type: the type of DNS record to lookup
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async): callback to call after resolution completes
 * @user_data: (closure): data for @callback
 *
 * Begins asynchronously performing a DNS lookup for the given
 * @rrname, and eventually calls @callback, which must call
 * g_resolver_lookup_records_finish() to get the final result. See
 * g_resolver_lookup_records() for more details.
 *
 * Since: 2.34
 */
void
g_resolver_lookup_records_async (GResolver           *resolver,
                                 const gchar         *rrname,
                                 GResolverRecordType  record_type,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_return_if_fail (G_IS_RESOLVER (resolver));
  g_return_if_fail (rrname != NULL);

  g_resolver_maybe_reload (resolver);
  G_RESOLVER_GET_CLASS (resolver)->
    lookup_records_async (resolver, rrname, record_type, cancellable, callback, user_data);
}

/**
 * g_resolver_lookup_records_finish:
 * @resolver: a #GResolver
 * @result: the result passed to your #GAsyncReadyCallback
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the result of a previous call to
 * g_resolver_lookup_records_async(). Returns a non-empty list of records as
 * #GVariant tuples. See #GResolverRecordType for information on what the
 * records contain.
 *
 * If the DNS resolution failed, @error (if non-%NULL) will be set to
 * a value from #GResolverError. If the operation was cancelled,
 * @error will be set to %G_IO_ERROR_CANCELLED.
 *
 * Returns: (element-type GVariant) (transfer full): a non-empty #GList of
 * #GVariant, or %NULL on error. You must free each of the records and the list
 * when you are done with it. (You can use g_list_free_full() with
 * g_variant_unref() to do this.)
 *
 * Since: 2.34
 */
GList *
g_resolver_lookup_records_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), NULL);
  return G_RESOLVER_GET_CLASS (resolver)->
    lookup_records_finish (resolver, result, error);
}

guint64
g_resolver_get_serial (GResolver *resolver)
{
  g_return_val_if_fail (G_IS_RESOLVER (resolver), 0);

  g_resolver_maybe_reload (resolver);

#ifdef G_OS_UNIX
  return (guint64) resolver->priv->resolv_conf_timestamp;
#else
  return 1;
#endif
}

/**
 * g_resolver_error_quark:
 *
 * Gets the #GResolver Error Quark.
 *
 * Returns: a #GQuark.
 *
 * Since: 2.22
 */
G_DEFINE_QUARK (g-resolver-error-quark, g_resolver_error)
