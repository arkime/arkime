/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2011 Red Hat, Inc.
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

#include "gio.h"

/* hack */
#define GIO_COMPILATION
#include "gnetworkmonitorbase.h"

#include <string.h>

/* Test data; the GInetAddresses and GInetAddressMasks get filled in
 * in main(). Each address in a TestAddress matches the mask in its
 * corresponding TestMask, and none of them match any of the other
 * masks. The addresses in unmatched don't match any of the masks.
 */

typedef struct {
  const char *string;
  GInetAddress *address;
} TestAddress;

typedef struct {
  const char *mask_string;
  GInetAddressMask *mask;
  TestAddress *addresses;
} TestMask;

TestAddress net127addrs[] = {
  { "127.0.0.1", NULL },
  { "127.0.0.2", NULL },
  { "127.0.0.255", NULL },
  { "127.0.1.0", NULL },
  { "127.0.255.0", NULL },
  { "127.0.255.0", NULL },
  { "127.255.255.255", NULL },
  { NULL, NULL }
};
TestMask net127 = { "127.0.0.0/8", NULL, net127addrs };

TestAddress net10addrs[] = {
  { "10.0.0.1", NULL },
  { "10.0.0.2", NULL },
  { "10.0.0.255", NULL },
  { NULL, NULL }
};
TestMask net10 = { "10.0.0.0/24", NULL, net10addrs };

TestAddress net192addrs[] = {
  { "192.168.0.1", NULL },
  { "192.168.0.2", NULL },
  { "192.168.0.255", NULL },
  { "192.168.1.0", NULL },
  { "192.168.15.0", NULL },
  { NULL, NULL }
};
TestMask net192 = { "192.168.0.0/20", NULL, net192addrs };

TestAddress netlocal6addrs[] = {
  { "::1", NULL },
  { NULL, NULL }
};
TestMask netlocal6 = { "::1/128", NULL, netlocal6addrs };

TestAddress netfe80addrs[] = {
  { "fe80::", NULL },
  { "fe80::1", NULL },
  { "fe80::21b:77ff:fea2:972a", NULL },
  { NULL, NULL }
};
TestMask netfe80 = { "fe80::/64", NULL, netfe80addrs };

TestAddress unmatched[] = {
  { "10.0.1.0", NULL },
  { "10.0.255.0", NULL },
  { "10.255.255.255", NULL },
  { "192.168.16.0", NULL },
  { "192.168.255.0", NULL },
  { "192.169.0.0", NULL },
  { "192.255.255.255", NULL },
  { "::2", NULL },
  { "1::1", NULL },
  { "fe80::1:0:0:0:0", NULL },
  { "fe80:8000::0:0:0:0", NULL },
  { NULL, NULL }
};

GInetAddressMask *ip4_default, *ip6_default;

static void
notify_handler (GObject    *object,
                GParamSpec *pspec,
                gpointer    user_data)
{
  gboolean *emitted = user_data;

  *emitted = TRUE;
}

static void
network_changed_handler (GNetworkMonitor *monitor,
                         gboolean         available,
                         gpointer         user_data)
{
  gboolean *emitted = user_data;

  *emitted = TRUE;
}

static void
assert_signals (GNetworkMonitor *monitor,
                gboolean         should_emit_notify,
                gboolean         should_emit_network_changed,
                gboolean         expected_network_available)
{
  gboolean emitted_notify = FALSE, emitted_network_changed = FALSE;
  guint h1, h2;

  h1 = g_signal_connect (monitor, "notify::network-available",
                         G_CALLBACK (notify_handler),
                         &emitted_notify);
  h2 = g_signal_connect (monitor, "network-changed",
                         G_CALLBACK (network_changed_handler),
                         &emitted_network_changed);

  g_main_context_iteration (NULL, FALSE);

  g_signal_handler_disconnect (monitor, h1);
  g_signal_handler_disconnect (monitor, h2);

  g_assert (emitted_notify == should_emit_notify);
  g_assert (emitted_network_changed == should_emit_network_changed);

  g_assert (g_network_monitor_get_network_available (monitor) == expected_network_available);
}

typedef struct {
  GNetworkMonitor *monitor;
  GMainLoop       *loop;
  GSocketAddress  *sockaddr;
  gboolean         should_be_reachable;
} CanReachData;

static void
reach_cb (GObject      *source,
          GAsyncResult *res,
          gpointer      user_data)
{
  GError *error = NULL;
  gboolean reachable;
  CanReachData *data = user_data;

  reachable = g_network_monitor_can_reach_finish (data->monitor, res, &error);

  if (data->should_be_reachable)
    g_assert_no_error (error);
  else
    {
      g_assert (error != NULL);
      g_clear_error (&error);
    }
  g_assert (reachable == data->should_be_reachable);

  g_main_loop_quit (data->loop);
}

static gboolean
test_reach_async (gpointer user_data)
{
  CanReachData *data = user_data;

  g_network_monitor_can_reach_async (data->monitor,
                                     G_SOCKET_CONNECTABLE (data->sockaddr),
                                     NULL,
                                     reach_cb,
                                     data);
  return G_SOURCE_REMOVE;
}

static void
run_tests (GNetworkMonitor *monitor,
           TestAddress     *addresses,
           gboolean         should_be_reachable)
{
  GError *error = NULL;
  int i;
  gboolean reachable;
  GSocketAddress *sockaddr;
  CanReachData data;

  data.monitor = monitor;
  data.loop = g_main_loop_new (NULL, FALSE);

  for (i = 0; addresses[i].address; i++)
    {
      sockaddr = g_inet_socket_address_new (addresses[i].address, 0);
      reachable = g_network_monitor_can_reach (monitor,
                                               G_SOCKET_CONNECTABLE (sockaddr),
                                               NULL, &error);
      data.sockaddr = sockaddr;
      data.should_be_reachable = should_be_reachable;

      g_idle_add (test_reach_async, &data);
      g_main_loop_run (data.loop);

      g_object_unref (sockaddr);
      g_assert_cmpint (reachable, ==, should_be_reachable);
      if (should_be_reachable)
        g_assert_no_error (error);
      else
        {
          g_assert (error != NULL);
          g_clear_error (&error);
        }
    }

  g_main_loop_unref (data.loop);
}

static void
test_default (void)
{
  GNetworkMonitor *monitor, *m;
  GError *error = NULL;

  m = g_network_monitor_get_default ();
  g_assert (G_IS_NETWORK_MONITOR (m));

  monitor = g_object_new (G_TYPE_NETWORK_MONITOR_BASE, NULL);
  g_initable_init (G_INITABLE (monitor), NULL, &error);
  g_assert_no_error (error);

  /* In the default configuration, all addresses are reachable */
  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, TRUE);

  assert_signals (monitor, FALSE, FALSE, TRUE);

  g_object_unref (monitor);
}

static void
test_remove_default (void)
{
  GNetworkMonitor *monitor;
  GError *error = NULL;

  monitor = g_initable_new (G_TYPE_NETWORK_MONITOR_BASE, NULL, &error, NULL);
  g_assert_no_error (error);
  assert_signals (monitor, FALSE, FALSE, TRUE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         ip4_default);
  assert_signals (monitor, FALSE, TRUE, TRUE);
  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         ip6_default);
  assert_signals (monitor, TRUE, TRUE, FALSE);

  /* Now nothing should be reachable */
  run_tests (monitor, net127.addresses, FALSE);
  run_tests (monitor, net10.addresses, FALSE);
  run_tests (monitor, net192.addresses, FALSE);
  run_tests (monitor, netlocal6.addresses, FALSE);
  run_tests (monitor, netfe80.addresses, FALSE);
  run_tests (monitor, unmatched, FALSE);

  g_object_unref (monitor);
}

static void
test_add_networks (void)
{
  GNetworkMonitor *monitor;
  GError *error = NULL;

  monitor = g_initable_new (G_TYPE_NETWORK_MONITOR_BASE, NULL, &error, NULL);
  g_assert_no_error (error);
  assert_signals (monitor, FALSE, FALSE, TRUE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         ip4_default);
  assert_signals (monitor, FALSE, TRUE, TRUE);
  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         ip6_default);
  assert_signals (monitor, TRUE, TRUE, FALSE);

  /* Now add the masks one by one */

  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      net127.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);

  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, FALSE);
  run_tests (monitor, net192.addresses, FALSE);
  run_tests (monitor, netlocal6.addresses, FALSE);
  run_tests (monitor, netfe80.addresses, FALSE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      net10.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, FALSE);
  run_tests (monitor, netlocal6.addresses, FALSE);
  run_tests (monitor, netfe80.addresses, FALSE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      net192.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, FALSE);
  run_tests (monitor, netfe80.addresses, FALSE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      netlocal6.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, FALSE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      netfe80.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, FALSE);

  g_object_unref (monitor);
}

static void
test_remove_networks (void)
{
  GNetworkMonitor *monitor;
  GError *error = NULL;

  monitor = g_initable_new (G_TYPE_NETWORK_MONITOR_BASE, NULL, &error, NULL);
  g_assert_no_error (error);
  assert_signals (monitor, FALSE, FALSE, TRUE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         ip4_default);
  assert_signals (monitor, FALSE, TRUE, TRUE);
  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         ip6_default);
  assert_signals (monitor, TRUE, TRUE, FALSE);

  /* First add them */
  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      net127.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      net10.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      net192.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      netlocal6.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (monitor),
                                      netfe80.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);

  run_tests (monitor, net127.addresses, TRUE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, FALSE);

  /* Now remove them one by one */
  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         net127.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, FALSE);
  run_tests (monitor, net10.addresses, TRUE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         net10.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, FALSE);
  run_tests (monitor, net10.addresses, FALSE);
  run_tests (monitor, net192.addresses, TRUE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         net192.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, FALSE);
  run_tests (monitor, net10.addresses, FALSE);
  run_tests (monitor, net192.addresses, FALSE);
  run_tests (monitor, netlocal6.addresses, TRUE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         netlocal6.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, FALSE);
  run_tests (monitor, net10.addresses, FALSE);
  run_tests (monitor, net192.addresses, FALSE);
  run_tests (monitor, netlocal6.addresses, FALSE);
  run_tests (monitor, netfe80.addresses, TRUE);
  run_tests (monitor, unmatched, FALSE);

  g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (monitor),
                                         netfe80.mask);
  assert_signals (monitor, FALSE, TRUE, FALSE);
  run_tests (monitor, net127.addresses, FALSE);
  run_tests (monitor, net10.addresses, FALSE);
  run_tests (monitor, net192.addresses, FALSE);
  run_tests (monitor, netlocal6.addresses, FALSE);
  run_tests (monitor, netfe80.addresses, FALSE);
  run_tests (monitor, unmatched, FALSE);

  g_object_unref (monitor);
}


static void
init_test (TestMask *test)
{
  GError *error = NULL;
  int i;

  test->mask = g_inet_address_mask_new_from_string (test->mask_string, &error);
  g_assert_no_error (error);

  for (i = 0; test->addresses[i].string; i++)
    {
      test->addresses[i].address = g_inet_address_new_from_string (test->addresses[i].string);
      if (strchr (test->addresses[i].string, ':'))
        g_assert_cmpint (g_inet_address_get_family (test->addresses[i].address), ==, G_SOCKET_FAMILY_IPV6);
      else
        g_assert_cmpint (g_inet_address_get_family (test->addresses[i].address), ==, G_SOCKET_FAMILY_IPV4);
    }
}

static void
cleanup_test (TestMask *test)
{
  int i;

  g_object_unref (test->mask);
  for (i = 0; test->addresses[i].string; i++)
    g_object_unref (test->addresses[i].address);
}

static void
watch_network_changed (GNetworkMonitor *monitor,
                       gboolean         available,
                       gpointer         user_data)
{
  g_print ("Network is %s\n", available ? "up" : "down");
}

static void
watch_connectivity_changed (GNetworkMonitor *monitor,
			    GParamSpec      *pspec,
			    gpointer         user_data)
{
  g_print ("Connectivity is %d\n", g_network_monitor_get_connectivity (monitor));
}

static void
watch_metered_changed (GNetworkMonitor *monitor,
                       GParamSpec      *pspec,
                       gpointer         user_data)
{
  g_print ("Metered is %d\n", g_network_monitor_get_network_metered (monitor));
}

static void
do_watch_network (void)
{
  GNetworkMonitor *monitor = g_network_monitor_get_default ();
  GMainLoop *loop;

  g_print ("Monitoring via %s\n", g_type_name_from_instance ((GTypeInstance *) monitor));

  g_signal_connect (monitor, "network-changed",
                    G_CALLBACK (watch_network_changed), NULL);
  g_signal_connect (monitor, "notify::connectivity",
                    G_CALLBACK (watch_connectivity_changed), NULL);
  g_signal_connect (monitor, "notify::network-metered",
                    G_CALLBACK (watch_metered_changed), NULL);
  watch_network_changed (monitor, g_network_monitor_get_network_available (monitor), NULL);
  watch_connectivity_changed (monitor, NULL, NULL);
  watch_metered_changed (monitor, NULL, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
}

int
main (int argc, char **argv)
{
  int ret;

  if (argc == 2 && !strcmp (argv[1], "--watch"))
    {
      do_watch_network ();
      return 0;
    }

  g_test_init (&argc, &argv, NULL);

  /* GNetworkMonitor will resolve addresses through a proxy if one is set and a
   * GIO module is available to handle it. In these tests we deliberately
   * change the idea of a reachable network to exclude the proxy, which will
   * lead to negative results. We're not trying to test the proxy-resolving
   * functionality (that would be for e.g. glib-networking's testsuite), so
   * let's just use the dummy proxy resolver, which always pretends the
   * passed-in URL is directly resolvable.
   */
  g_setenv ("GIO_USE_PROXY_RESOLVER", "dummy", TRUE);

  init_test (&net127);
  init_test (&net10);
  init_test (&net192);
  init_test (&netlocal6);
  init_test (&netfe80);
  ip4_default = g_inet_address_mask_new_from_string ("0.0.0.0/0", NULL);
  ip6_default = g_inet_address_mask_new_from_string ("::/0", NULL);

  g_test_add_func ("/network-monitor/default", test_default);
  g_test_add_func ("/network-monitor/remove_default", test_remove_default);
  g_test_add_func ("/network-monitor/add_networks", test_add_networks);
  g_test_add_func ("/network-monitor/remove_networks", test_remove_networks);

  ret = g_test_run ();

  cleanup_test (&net127);
  cleanup_test (&net10);
  cleanup_test (&net192);
  cleanup_test (&netlocal6);
  cleanup_test (&netfe80);
  g_object_unref (ip4_default);
  g_object_unref (ip6_default);

  return ret;
}
