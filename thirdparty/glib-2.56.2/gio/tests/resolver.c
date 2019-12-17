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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#include <gio/gio.h>

static GResolver *resolver;
static GCancellable *cancellable;
static GMainLoop *loop;
static int nlookups = 0;
static gboolean synchronous = FALSE;
static guint connectable_count = 0;
static GResolverRecordType record_type = 0;

static void G_GNUC_NORETURN
usage (void)
{
	fprintf (stderr, "Usage: resolver [-s] [hostname | IP | service/protocol/domain ] ...\n");
	fprintf (stderr, "Usage: resolver [-s] [-t MX|TXT|NS|SOA] rrname ...\n");
	fprintf (stderr, "       resolver [-s] -c NUMBER [hostname | IP | service/protocol/domain ]\n");
	fprintf (stderr, "       Use -s to do synchronous lookups.\n");
	fprintf (stderr, "       Use -c NUMBER (and only a single resolvable argument) to test GSocketConnectable.\n");
	fprintf (stderr, "       The given NUMBER determines how many times the connectable will be enumerated.\n");
	fprintf (stderr, "       Use -t with MX, TXT, NS or SOA to lookup DNS records of those types.\n");
	exit (1);
}

G_LOCK_DEFINE_STATIC (response);

static void
done_lookup (void)
{
  nlookups--;
  if (nlookups == 0)
    {
      /* In the sync case we need to make sure we don't call
       * g_main_loop_quit before the loop is actually running...
       */
      g_idle_add ((GSourceFunc)g_main_loop_quit, loop);
    }
}

static void
print_resolved_name (const char *phys,
                     char       *name,
                     GError     *error)
{
  G_LOCK (response);
  printf ("Address: %s\n", phys);
  if (error)
    {
      printf ("Error:   %s\n", error->message);
      g_error_free (error);
    }
  else
    {
      printf ("Name:    %s\n", name);
      g_free (name);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
print_resolved_addresses (const char *name,
                          GList      *addresses,
			  GError     *error)
{
  char *phys;
  GList *a;

  G_LOCK (response);
  printf ("Name:    %s\n", name);
  if (error)
    {
      printf ("Error:   %s\n", error->message);
      g_error_free (error);
    }
  else
    {
      for (a = addresses; a; a = a->next)
	{
	  phys = g_inet_address_to_string (a->data);
	  printf ("Address: %s\n", phys);
	  g_free (phys);
          g_object_unref (a->data);
	}
      g_list_free (addresses);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
print_resolved_service (const char *service,
                        GList      *targets,
			GError     *error)
{
  GList *t;  

  G_LOCK (response);
  printf ("Service: %s\n", service);
  if (error)
    {
      printf ("Error: %s\n", error->message);
      g_error_free (error);
    }
  else
    {
      for (t = targets; t; t = t->next)
	{
	  printf ("%s:%u (pri %u, weight %u)\n",
		  g_srv_target_get_hostname (t->data),
		  (guint)g_srv_target_get_port (t->data),
		  (guint)g_srv_target_get_priority (t->data),
		  (guint)g_srv_target_get_weight (t->data));
          g_srv_target_free (t->data);
	}
      g_list_free (targets);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
print_resolved_mx (const char *rrname,
                   GList      *records,
                   GError     *error)
{
  const gchar *hostname;
  guint16 priority;
  GList *t;

  G_LOCK (response);
  printf ("Domain: %s\n", rrname);
  if (error)
    {
      printf ("Error: %s\n", error->message);
      g_error_free (error);
    }
  else if (!records)
    {
      printf ("no MX records\n");
    }
  else
    {
      for (t = records; t; t = t->next)
        {
          g_variant_get (t->data, "(q&s)", &priority, &hostname);
          printf ("%s (pri %u)\n", hostname, (guint)priority);
          g_variant_unref (t->data);
        }
      g_list_free (records);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
print_resolved_txt (const char *rrname,
                    GList      *records,
                    GError     *error)
{
  const gchar **contents;
  GList *t;
  gint i;

  G_LOCK (response);
  printf ("Domain: %s\n", rrname);
  if (error)
    {
      printf ("Error: %s\n", error->message);
      g_error_free (error);
    }
  else if (!records)
    {
      printf ("no TXT records\n");
    }
  else
    {
      for (t = records; t; t = t->next)
        {
          if (t != records)
            printf ("\n");
          g_variant_get (t->data, "(^a&s)", &contents);
          for (i = 0; contents[i] != NULL; i++)
            printf ("%s\n", contents[i]);
          g_variant_unref (t->data);
        }
      g_list_free (records);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
print_resolved_soa (const char *rrname,
                    GList      *records,
                    GError     *error)
{
  GList *t;
  const gchar *primary_ns;
  const gchar *administrator;
  guint32 serial, refresh, retry, expire, ttl;

  G_LOCK (response);
  printf ("Zone: %s\n", rrname);
  if (error)
    {
      printf ("Error: %s\n", error->message);
      g_error_free (error);
    }
  else if (!records)
    {
      printf ("no SOA records\n");
    }
  else
    {
      for (t = records; t; t = t->next)
        {
          g_variant_get (t->data, "(&s&suuuuu)", &primary_ns, &administrator,
                         &serial, &refresh, &retry, &expire, &ttl);
          printf ("%s %s (serial %u, refresh %u, retry %u, expire %u, ttl %u)\n",
                  primary_ns, administrator, (guint)serial, (guint)refresh,
                  (guint)retry, (guint)expire, (guint)ttl);
          g_variant_unref (t->data);
        }
      g_list_free (records);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
print_resolved_ns (const char *rrname,
                    GList      *records,
                    GError     *error)
{
  GList *t;
  const gchar *hostname;

  G_LOCK (response);
  printf ("Zone: %s\n", rrname);
  if (error)
    {
      printf ("Error: %s\n", error->message);
      g_error_free (error);
    }
  else if (!records)
    {
      printf ("no NS records\n");
    }
  else
    {
      for (t = records; t; t = t->next)
        {
          g_variant_get (t->data, "(&s)", &hostname);
          printf ("%s\n", hostname);
          g_variant_unref (t->data);
        }
      g_list_free (records);
    }
  printf ("\n");

  done_lookup ();
  G_UNLOCK (response);
}

static void
lookup_one_sync (const char *arg)
{
  GError *error = NULL;

  if (record_type != 0)
    {
      GList *records;

      records = g_resolver_lookup_records (resolver, arg, record_type, cancellable, &error);
      switch (record_type)
      {
        case G_RESOLVER_RECORD_MX:
          print_resolved_mx (arg, records, error);
          break;
        case G_RESOLVER_RECORD_SOA:
          print_resolved_soa (arg, records, error);
          break;
        case G_RESOLVER_RECORD_NS:
          print_resolved_ns (arg, records, error);
          break;
        case G_RESOLVER_RECORD_TXT:
          print_resolved_txt (arg, records, error);
          break;
        default:
          g_warn_if_reached ();
          break;
      }
    }
  else if (strchr (arg, '/'))
    {
      GList *targets;
      /* service/protocol/domain */
      char **parts = g_strsplit (arg, "/", 3);

      if (!parts || !parts[2])
	usage ();

      targets = g_resolver_lookup_service (resolver,
                                           parts[0], parts[1], parts[2],
                                           cancellable, &error);
      print_resolved_service (arg, targets, error);
    }
  else if (g_hostname_is_ip_address (arg))
    {
      GInetAddress *addr = g_inet_address_new_from_string (arg);
      char *name;

      name = g_resolver_lookup_by_address (resolver, addr, cancellable, &error);
      print_resolved_name (arg, name, error);
      g_object_unref (addr);
    }
  else
    {
      GList *addresses;

      addresses = g_resolver_lookup_by_name (resolver, arg, cancellable, &error);
      print_resolved_addresses (arg, addresses, error);
    }
}

static gpointer
lookup_thread (gpointer arg)
{
  lookup_one_sync (arg);
  return NULL;
}

static void
start_sync_lookups (char **argv, int argc)
{
  int i;

  for (i = 0; i < argc; i++)
    {
      GThread *thread;
      thread = g_thread_new ("lookup", lookup_thread, argv[i]);
      g_thread_unref (thread);
    }
}

static void
lookup_by_addr_callback (GObject *source, GAsyncResult *result,
                         gpointer user_data)
{
  const char *phys = user_data;
  GError *error = NULL;
  char *name;

  name = g_resolver_lookup_by_address_finish (resolver, result, &error);
  print_resolved_name (phys, name, error);
}

static void
lookup_by_name_callback (GObject *source, GAsyncResult *result,
                         gpointer user_data)
{
  const char *name = user_data;
  GError *error = NULL;
  GList *addresses;

  addresses = g_resolver_lookup_by_name_finish (resolver, result, &error);
  print_resolved_addresses (name, addresses, error);
}

static void
lookup_service_callback (GObject *source, GAsyncResult *result,
			 gpointer user_data)
{
  const char *service = user_data;
  GError *error = NULL;
  GList *targets;

  targets = g_resolver_lookup_service_finish (resolver, result, &error);
  print_resolved_service (service, targets, error);
}

static void
lookup_records_callback (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  const char *arg = user_data;
  GError *error = NULL;
  GList *records;

  records = g_resolver_lookup_records_finish (resolver, result, &error);

  switch (record_type)
  {
    case G_RESOLVER_RECORD_MX:
      print_resolved_mx (arg, records, error);
      break;
    case G_RESOLVER_RECORD_SOA:
      print_resolved_soa (arg, records, error);
      break;
    case G_RESOLVER_RECORD_NS:
      print_resolved_ns (arg, records, error);
      break;
    case G_RESOLVER_RECORD_TXT:
      print_resolved_txt (arg, records, error);
      break;
    default:
      g_warn_if_reached ();
      break;
  }
}

static void
start_async_lookups (char **argv, int argc)
{
  int i;

  for (i = 0; i < argc; i++)
    {
      if (record_type != 0)
	{
	  g_resolver_lookup_records_async (resolver, argv[i], record_type,
	                                   cancellable, lookup_records_callback, argv[i]);
	}
      else if (strchr (argv[i], '/'))
	{
	  /* service/protocol/domain */
	  char **parts = g_strsplit (argv[i], "/", 3);

	  if (!parts || !parts[2])
	    usage ();

	  g_resolver_lookup_service_async (resolver,
					   parts[0], parts[1], parts[2],
					   cancellable,
					   lookup_service_callback, argv[i]);
	}
      else if (g_hostname_is_ip_address (argv[i]))
	{
          GInetAddress *addr = g_inet_address_new_from_string (argv[i]);

	  g_resolver_lookup_by_address_async (resolver, addr, cancellable,
                                              lookup_by_addr_callback, argv[i]);
	  g_object_unref (addr);
	}
      else
	{
	  g_resolver_lookup_by_name_async (resolver, argv[i], cancellable,
                                           lookup_by_name_callback,
                                           argv[i]);
	}

      /* Stress-test the reloading code */
      g_signal_emit_by_name (resolver, "reload");
    }
}

static void
print_connectable_sockaddr (GSocketAddress *sockaddr,
                            GError         *error)
{
  char *phys;

  if (error)
    {
      printf ("Error:   %s\n", error->message);
      g_error_free (error);
    }
  else if (!G_IS_INET_SOCKET_ADDRESS (sockaddr))
    {
      printf ("Error:   Unexpected sockaddr type '%s'\n", g_type_name_from_instance ((GTypeInstance *)sockaddr));
      g_object_unref (sockaddr);
    }
  else
    {
      GInetSocketAddress *isa = G_INET_SOCKET_ADDRESS (sockaddr);
      phys = g_inet_address_to_string (g_inet_socket_address_get_address (isa));
      printf ("Address: %s%s%s:%d\n",
              strchr (phys, ':') ? "[" : "", phys, strchr (phys, ':') ? "]" : "",
              g_inet_socket_address_get_port (isa));
      g_free (phys);
      g_object_unref (sockaddr);
    }
}

static void
do_sync_connectable (GSocketAddressEnumerator *enumerator)
{
  GSocketAddress *sockaddr;
  GError *error = NULL;

  while ((sockaddr = g_socket_address_enumerator_next (enumerator, cancellable, &error)))
    print_connectable_sockaddr (sockaddr, error);

  g_object_unref (enumerator);
  done_lookup ();
}

static void do_async_connectable (GSocketAddressEnumerator *enumerator);

static void
got_next_async (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GSocketAddressEnumerator *enumerator = G_SOCKET_ADDRESS_ENUMERATOR (source);
  GSocketAddress *sockaddr;
  GError *error = NULL;

  sockaddr = g_socket_address_enumerator_next_finish (enumerator, result, &error);
  if (sockaddr || error)
    print_connectable_sockaddr (sockaddr, error);
  if (sockaddr)
    do_async_connectable (enumerator);
  else
    {
      g_object_unref (enumerator);
      done_lookup ();
    }
}

static void
do_async_connectable (GSocketAddressEnumerator *enumerator)
{
  g_socket_address_enumerator_next_async (enumerator, cancellable,
                                          got_next_async, NULL);
}

static void
do_connectable (const char *arg, gboolean synchronous, guint count)
{
  char **parts;
  GSocketConnectable *connectable;
  GSocketAddressEnumerator *enumerator;

  if (strchr (arg, '/'))
    {
      /* service/protocol/domain */
      parts = g_strsplit (arg, "/", 3);
      if (!parts || !parts[2])
	usage ();

      connectable = g_network_service_new (parts[0], parts[1], parts[2]);
    }
  else
    {
      guint16 port;

      parts = g_strsplit (arg, ":", 2);
      if (parts && parts[1])
	{
	  arg = parts[0];
	  port = strtoul (parts[1], NULL, 10);
	}
      else
	port = 0;

      if (g_hostname_is_ip_address (arg))
	{
	  GInetAddress *addr = g_inet_address_new_from_string (arg);
	  GSocketAddress *sockaddr = g_inet_socket_address_new (addr, port);

	  g_object_unref (addr);
	  connectable = G_SOCKET_CONNECTABLE (sockaddr);
	}
      else
        connectable = g_network_address_new (arg, port);
    }

  while (count--)
    {
      enumerator = g_socket_connectable_enumerate (connectable);

      if (synchronous)
        do_sync_connectable (enumerator);
      else
        do_async_connectable (enumerator);
    }
  
  g_object_unref (connectable);
}

#ifdef G_OS_UNIX
static int cancel_fds[2];

static void
interrupted (int sig)
{
  gssize c;

  signal (SIGINT, SIG_DFL);
  c = write (cancel_fds[1], "x", 1);
  g_assert_cmpint(c, ==, 1);
}

static gboolean
async_cancel (GIOChannel *source, GIOCondition cond, gpointer cancel)
{
  g_cancellable_cancel (cancel);
  return FALSE;
}
#endif


static gboolean
record_type_arg (const gchar *option_name,
                 const gchar *value,
                 gpointer data,
                 GError **error)
{
  if (g_ascii_strcasecmp (value, "MX") == 0) {
    record_type = G_RESOLVER_RECORD_MX;
  } else if (g_ascii_strcasecmp (value, "TXT") == 0) {
    record_type = G_RESOLVER_RECORD_TXT;
  } else if (g_ascii_strcasecmp (value, "SOA") == 0) {
    record_type = G_RESOLVER_RECORD_SOA;
  } else if (g_ascii_strcasecmp (value, "NS") == 0) {
    record_type = G_RESOLVER_RECORD_NS;
  } else {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   "Specify MX, TXT, NS or SOA for the special record lookup types");
      return FALSE;
  }

  return TRUE;
}

static const GOptionEntry option_entries[] = {
  { "synchronous", 's', 0, G_OPTION_ARG_NONE, &synchronous, "Synchronous connections", NULL },
  { "connectable", 'c', 0, G_OPTION_ARG_INT, &connectable_count, "Connectable count", "C" },
  { "special-type", 't', 0, G_OPTION_ARG_CALLBACK, record_type_arg, "Record type like MX, TXT, NS or SOA", "RR" },
  { NULL },
};

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GError *error = NULL;
#ifdef G_OS_UNIX
  GIOChannel *chan;
  guint watch;
#endif

  context = g_option_context_new ("lookups ...");
  g_option_context_add_main_entries (context, option_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      usage();
    }

  if (argc < 2 || (argc > 2 && connectable_count))
    usage ();

  resolver = g_resolver_get_default ();

  cancellable = g_cancellable_new ();

#ifdef G_OS_UNIX
  /* Set up cancellation; we want to cancel if the user ^C's the
   * program, but we can't cancel directly from an interrupt.
   */
  signal (SIGINT, interrupted);

  if (pipe (cancel_fds) == -1)
    {
      perror ("pipe");
      exit (1);
    }
  chan = g_io_channel_unix_new (cancel_fds[0]);
  watch = g_io_add_watch (chan, G_IO_IN, async_cancel, cancellable);
  g_io_channel_unref (chan);
#endif

  nlookups = argc - 1;
  loop = g_main_loop_new (NULL, TRUE);

  if (connectable_count)
    {
      nlookups = connectable_count;
      do_connectable (argv[1], synchronous, connectable_count);
    }
  else
    {
      if (synchronous)
        start_sync_lookups (argv + 1, argc - 1);
      else
        start_async_lookups (argv + 1, argc - 1);
    }

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

#ifdef G_OS_UNIX
  g_source_remove (watch);
#endif
  g_object_unref (cancellable);

  return 0;
}
