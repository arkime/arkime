/* GStaticProxyResolver tests
 *
 * Copyright 2011, 2013 Red Hat, Inc.
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
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

static void
async_result_cb (GObject      *obj,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GAsyncResult **result_out = user_data;
  *result_out = g_object_ref (result);
}

static void
test_uris (void)
{
  GProxyResolver *resolver;
  gchar *ignore_hosts[2] = { "127.0.0.1", NULL };
  gchar **proxies;
  GError *error = NULL;
  const gchar *uri;
  gchar *str = NULL;
  GAsyncResult *result = NULL;

  /* Valid URI. */
  uri = "http://%E0%B4%A8%E0%B4%B2:80/";
  resolver = g_simple_proxy_resolver_new (NULL, ignore_hosts);

  proxies = g_proxy_resolver_lookup (resolver, uri, NULL, &error);
  g_assert_no_error (error);
  g_strfreev (proxies);

  g_proxy_resolver_lookup_async (resolver, uri, NULL, async_result_cb, &result);
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  proxies = g_proxy_resolver_lookup_finish (resolver, result, &error);
  g_assert_no_error (error);
  g_strfreev (proxies);
  g_clear_object (&result);

  g_object_unref (resolver);

  /* Invalid URI. */
  uri = "%E0%B4%A8%E0%B4%B2";
  str = g_strdup_printf ("Invalid URI ‘%s’", uri);
  resolver = g_simple_proxy_resolver_new (NULL, ignore_hosts);

  proxies = g_proxy_resolver_lookup (resolver, uri, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, str);
  g_clear_error (&error);
  g_assert_null (proxies);
  g_clear_object (&result);

  g_proxy_resolver_lookup_async (resolver, uri, NULL, async_result_cb, &result);
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  proxies = g_proxy_resolver_lookup_finish (resolver, result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, str);
  g_clear_error (&error);
  g_assert_null (proxies);

  g_object_unref (resolver);
  g_free (str);

  resolver = g_simple_proxy_resolver_new ("default://", ignore_hosts);
  g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver),
                                         "http", "http://proxy.example.com");
  g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver),
                                         "ftp", "ftp://proxy.example.com");

  proxies = g_proxy_resolver_lookup (resolver, "http://one.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "http://proxy.example.com");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "HTTP://uppercase.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "http://proxy.example.com");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "htt://missing-letter.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "default://");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "https://extra-letter.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "default://");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "ftp://five.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "ftp://proxy.example.com");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "http://127.0.0.1/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "direct://");
  g_strfreev (proxies);

  g_object_unref (resolver);
}

static void
test_socks (void)
{
  GProxyResolver *resolver;
  gchar *ignore_hosts[2] = { "127.0.0.1", NULL };
  gchar **proxies;
  GError *error = NULL;

  resolver = g_simple_proxy_resolver_new ("socks://proxy.example.com", ignore_hosts);

  proxies = g_proxy_resolver_lookup (resolver, "http://one.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 3);
  g_assert_cmpstr (proxies[0], ==, "socks5://proxy.example.com");
  g_assert_cmpstr (proxies[1], ==, "socks4a://proxy.example.com");
  g_assert_cmpstr (proxies[2], ==, "socks4://proxy.example.com");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "http://127.0.0.1/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "direct://");
  g_strfreev (proxies);

  g_object_unref (resolver);

  resolver = g_simple_proxy_resolver_new ("default-proxy://", ignore_hosts);
  g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver),
                                         "http", "socks://proxy.example.com");

  proxies = g_proxy_resolver_lookup (resolver, "http://one.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 3);
  g_assert_cmpstr (proxies[0], ==, "socks5://proxy.example.com");
  g_assert_cmpstr (proxies[1], ==, "socks4a://proxy.example.com");
  g_assert_cmpstr (proxies[2], ==, "socks4://proxy.example.com");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "ftp://two.example.com/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "default-proxy://");
  g_strfreev (proxies);

  proxies = g_proxy_resolver_lookup (resolver, "http://127.0.0.1/",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (proxies), ==, 1);
  g_assert_cmpstr (proxies[0], ==, "direct://");
  g_strfreev (proxies);

  g_object_unref (resolver);
}

static const char *ignore_hosts[] = {
  ".bbb.xx",
  "*.ccc.xx",
  "ddd.xx",
  "*.eee.xx:8000",
  "127.0.0.0/24",
  "10.0.0.1:8000",
  "::1",
  "fe80::/10",
  NULL
};

static const struct {
  const char *uri;
  const char *proxy;
} ignore_tests[] = {
  { "http://aaa.xx/",          	 "http://localhost:8080" },
  { "http://aaa.xx:8000/",     	 "http://localhost:8080" },
  { "http://www.aaa.xx/",      	 "http://localhost:8080" },
  { "http://www.aaa.xx:8000/", 	 "http://localhost:8080" },
  { "https://aaa.xx/",         	 "http://localhost:8080" },
  { "http://bbb.xx/",          	 "direct://" },
  { "http://www.bbb.xx/",      	 "direct://" },
  { "http://bbb.xx:8000/",     	 "direct://" },
  { "http://www.bbb.xx:8000/", 	 "direct://" },
  { "https://bbb.xx/",         	 "direct://" },
  { "http://nobbb.xx/",          "http://localhost:8080" },
  { "http://www.nobbb.xx/",      "http://localhost:8080" },
  { "http://nobbb.xx:8000/",     "http://localhost:8080" },
  { "http://www.nobbb.xx:8000/", "http://localhost:8080" },
  { "https://nobbb.xx/",         "http://localhost:8080" },
  { "http://ccc.xx/",          	 "direct://" },
  { "http://www.ccc.xx/",      	 "direct://" },
  { "http://ccc.xx:8000/",     	 "direct://" },
  { "http://www.ccc.xx:8000/", 	 "direct://" },
  { "https://ccc.xx/",         	 "direct://" },
  { "http://ddd.xx/",          	 "direct://" },
  { "http://ddd.xx:8000/",     	 "direct://" },
  { "http://www.ddd.xx/",      	 "direct://" },
  { "http://www.ddd.xx:8000/", 	 "direct://" },
  { "https://ddd.xx/",         	 "direct://" },
  { "http://eee.xx/",          	 "http://localhost:8080" },
  { "http://eee.xx:8000/",     	 "direct://" },
  { "http://www.eee.xx/",      	 "http://localhost:8080" },
  { "http://www.eee.xx:8000/", 	 "direct://" },
  { "https://eee.xx/",         	 "http://localhost:8080" },
  { "http://1.2.3.4/",         	 "http://localhost:8080" },
  { "http://127.0.0.1/",       	 "direct://" },
  { "http://127.0.0.2/",       	 "direct://" },
  { "http://127.0.0.255/",     	 "direct://" },
  { "http://127.0.1.0/",       	 "http://localhost:8080" },
  { "http://10.0.0.1/",        	 "http://localhost:8080" },
  { "http://10.0.0.1:8000/",   	 "direct://" },
  { "http://[::1]/",           	 "direct://" },
  { "http://[::1]:80/",        	 "direct://" },
  { "http://[::1:1]/",         	 "http://localhost:8080" },
  { "http://[::1:1]:80/",      	 "http://localhost:8080" },
  { "http://[fe80::1]/",       	 "direct://" },
  { "http://[fe80::1]:80/",    	 "direct://" },
  { "http://[fec0::1]/",       	 "http://localhost:8080" },
  { "http://[fec0::1]:80/",    	 "http://localhost:8080" }
};
static const int n_ignore_tests = G_N_ELEMENTS (ignore_tests);

static void
test_ignore (void)
{
  GProxyResolver *resolver;
  GError *error = NULL;
  char **proxies;
  int i;

  resolver = g_simple_proxy_resolver_new ("http://localhost:8080",
                                          (char **)ignore_hosts);

  for (i = 0; i < n_ignore_tests; i++)
    {
      proxies = g_proxy_resolver_lookup (resolver, ignore_tests[i].uri,
					 NULL, &error);
      g_assert_no_error (error);

      g_assert_cmpstr (proxies[0], ==, ignore_tests[i].proxy);
      g_strfreev (proxies);
    }

  g_object_unref (resolver);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/static-proxy/uri", test_uris);
  g_test_add_func ("/static-proxy/socks", test_socks);
  g_test_add_func ("/static-proxy/ignore", test_ignore);

  return g_test_run();
}
