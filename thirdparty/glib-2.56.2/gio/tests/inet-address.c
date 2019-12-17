/* Unit tests for GInetAddress
 * Copyright (C) 2012 Red Hat, Inc
 * Author: Matthias Clasen
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>

static void
test_parse (void)
{
  GInetAddress *addr;

  addr = g_inet_address_new_from_string ("0:0:0:0:0:0:0:0");
  g_assert (addr != NULL);
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("1:0:0:0:0:0:0:8");
  g_assert (addr != NULL);
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("0:0:0:0:0:FFFF:204.152.189.116");
  g_assert (addr != NULL);
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("::1");
  g_assert (addr != NULL);
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("::");
  g_assert (addr != NULL);
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("::FFFF:204.152.189.116");
  g_assert (addr != NULL);
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("204.152.189.116");
  g_assert (addr != NULL);
  g_object_unref (addr);

  addr = g_inet_address_new_from_string ("::1::2");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("2001:1:2:3:4:5:6:7]");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("[2001:1:2:3:4:5:6:7");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("[2001:1:2:3:4:5:6:7]");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("[2001:1:2:3:4:5:6:7]:80");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("0:1:2:3:4:5:6:7:8:9");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("::FFFFFFF");
  g_assert (addr == NULL);
  addr = g_inet_address_new_from_string ("204.152.189.116:80");
  g_assert (addr == NULL);
}

static void
test_any (void)
{
  GInetAddress *addr;
  GSocketFamily family[2] = { G_SOCKET_FAMILY_IPV4, G_SOCKET_FAMILY_IPV6 };
  gsize size[2] = { 4, 16 };
  gint i;

  for (i = 0; i < 2; i++)
    {
      addr = g_inet_address_new_any (family[i]);

      g_assert (g_inet_address_get_is_any (addr));
      g_assert (g_inet_address_get_family (addr) == family[i]);
      g_assert (g_inet_address_get_native_size (addr) == size[i]);
      g_assert (!g_inet_address_get_is_loopback (addr));
      g_assert (!g_inet_address_get_is_link_local (addr));
      g_assert (!g_inet_address_get_is_site_local (addr));
      g_assert (!g_inet_address_get_is_multicast (addr));
      g_assert (!g_inet_address_get_is_mc_global (addr));
      g_assert (!g_inet_address_get_is_mc_link_local (addr));
      g_assert (!g_inet_address_get_is_mc_node_local (addr));
      g_assert (!g_inet_address_get_is_mc_org_local (addr));
      g_assert (!g_inet_address_get_is_mc_site_local (addr));

      g_object_unref (addr);
   }
}

static void
test_loopback (void)
{
  GInetAddress *addr;

  addr = g_inet_address_new_from_string ("::1");
  g_assert (g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV6);
  g_assert (g_inet_address_get_is_loopback (addr));
  g_object_unref (addr);

  addr = g_inet_address_new_from_string ("127.0.0.0");
  g_assert (g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV4);
  g_assert (g_inet_address_get_is_loopback (addr));
  g_object_unref (addr);
}

static void
test_bytes (void)
{
  GInetAddress *addr1, *addr2, *addr3;
  const guint8 *bytes;

  addr1 = g_inet_address_new_from_string ("192.168.0.100");
  addr2 = g_inet_address_new_from_string ("192.168.0.101");
  bytes = g_inet_address_to_bytes (addr1);
  addr3 = g_inet_address_new_from_bytes (bytes, G_SOCKET_FAMILY_IPV4);

  g_assert (!g_inet_address_equal (addr1, addr2));
  g_assert (g_inet_address_equal (addr1, addr3));

  g_object_unref (addr1);
  g_object_unref (addr2);
  g_object_unref (addr3);
}

static void
test_property (void)
{
  GInetAddress *addr;
  GSocketFamily family;
  const guint8 *bytes;
  gboolean any;
  gboolean loopback;
  gboolean link_local;
  gboolean site_local;
  gboolean multicast;
  gboolean mc_global;
  gboolean mc_link_local;
  gboolean mc_node_local;
  gboolean mc_org_local;
  gboolean mc_site_local;

  addr = g_inet_address_new_from_string ("ff85::");
  g_object_get (addr,
                "family", &family,
                "bytes", &bytes,
                "is-any", &any,
                "is-loopback", &loopback,
                "is-link-local", &link_local,
                "is-site-local", &site_local,
                "is-multicast", &multicast,
                "is-mc-global", &mc_global,
                "is-mc-link-local", &mc_link_local,
                "is-mc-node-local", &mc_node_local,
                "is-mc-org-local", &mc_org_local,
                "is-mc-site-local", &mc_site_local,
                NULL);

  g_assert (family == G_SOCKET_FAMILY_IPV6);
  g_assert (!any);
  g_assert (!loopback);
  g_assert (!link_local);
  g_assert (!site_local);
  g_assert (multicast);
  g_assert (!mc_global);
  g_assert (!mc_link_local);
  g_assert (!mc_node_local);
  g_assert (!mc_org_local);
  g_assert (mc_site_local);

  g_object_unref (addr);
}

static void
test_socket_address (void)
{
  GInetAddress *addr;
  GInetSocketAddress *saddr;
  guint port;
  guint32 flowinfo;
  guint32 scope_id;
  GSocketFamily family;

  addr = g_inet_address_new_from_string ("::ffff:125.1.15.5");
  saddr = G_INET_SOCKET_ADDRESS (g_inet_socket_address_new (addr, 308));

  g_assert (g_inet_address_equal (addr, g_inet_socket_address_get_address (saddr)));
  g_object_unref (addr);

  g_assert (g_inet_socket_address_get_port (saddr) == 308);
  g_assert (g_inet_socket_address_get_flowinfo (saddr) == 0);
  g_assert (g_inet_socket_address_get_scope_id (saddr) == 0);

  g_object_unref (saddr);

  addr = g_inet_address_new_from_string ("::1");
  saddr = G_INET_SOCKET_ADDRESS (g_object_new (G_TYPE_INET_SOCKET_ADDRESS,
                                               "address", addr,
                                               "port", 308,
                                               "flowinfo", 10,
                                               "scope-id", 25,
                                               NULL));
  g_object_unref (addr);

  g_assert (g_inet_socket_address_get_port (saddr) == 308);
  g_assert (g_inet_socket_address_get_flowinfo (saddr) == 10);
  g_assert (g_inet_socket_address_get_scope_id (saddr) == 25);

  g_object_get (saddr,
                "family", &family,
                "address", &addr,
                "port", &port,
                "flowinfo", &flowinfo,
                "scope-id", &scope_id,
                NULL);

  g_assert (family == G_SOCKET_FAMILY_IPV6);
  g_assert (addr != NULL);
  g_assert (port == 308);
  g_assert (flowinfo == 10);
  g_assert (scope_id == 25);

  g_object_unref (addr);
  g_object_unref (saddr);
}

static void
test_socket_address_to_string (void)
{
  GSocketAddress *sa = NULL;
  GInetAddress *ia = NULL;
  gchar *str = NULL;

  /* IPv4. */
  ia = g_inet_address_new_from_string ("123.1.123.1");
  sa = g_inet_socket_address_new (ia, 80);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (sa));
  g_assert_cmpstr (str, ==, "123.1.123.1:80");
  g_free (str);
  g_object_unref (sa);
  g_object_unref (ia);

  /* IPv6. */
  ia = g_inet_address_new_from_string ("fe80::80");
  sa = g_inet_socket_address_new (ia, 80);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (sa));
  g_assert_cmpstr (str, ==, "[fe80::80]:80");
  g_free (str);
  g_object_unref (sa);
  g_object_unref (ia);

  /* IPv6 without port. */
  ia = g_inet_address_new_from_string ("fe80::80");
  sa = g_inet_socket_address_new (ia, 0);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (sa));
  g_assert_cmpstr (str, ==, "fe80::80");
  g_free (str);
  g_object_unref (sa);
  g_object_unref (ia);

  /* IPv6 with scope. */
  ia = g_inet_address_new_from_string ("::1");
  sa = G_SOCKET_ADDRESS (g_object_new (G_TYPE_INET_SOCKET_ADDRESS,
                                       "address", ia,
                                       "port", 123,
                                       "flowinfo", 10,
                                       "scope-id", 25,
                                       NULL));
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (sa));
  g_assert_cmpstr (str, ==, "[::1%25]:123");
  g_free (str);
  g_object_unref (sa);
  g_object_unref (ia);
}

static void
test_mask_parse (void)
{
  GInetAddressMask *mask;
  GError *error = NULL;

  mask = g_inet_address_mask_new_from_string ("10.0.0.0/8", &error);
  g_assert_no_error (error);
  g_assert (mask != NULL);
  g_object_unref (mask);

  mask = g_inet_address_mask_new_from_string ("fe80::/10", &error);
  g_assert_no_error (error);
  g_assert (mask != NULL);
  g_object_unref (mask);

  mask = g_inet_address_mask_new_from_string ("::", &error);
  g_assert_no_error (error);
  g_assert (mask != NULL);
  g_object_unref (mask);

  mask = g_inet_address_mask_new_from_string ("::/abc", &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert (mask == NULL);
  g_clear_error (&error);

  mask = g_inet_address_mask_new_from_string ("127.0.0.1/128", &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert (mask == NULL);
  g_clear_error (&error);
}

static void
test_mask_property (void)
{
  GInetAddressMask *mask;
  GInetAddress *addr;
  GSocketFamily family;
  guint len;

  addr = g_inet_address_new_from_string ("fe80::");
  mask = g_inet_address_mask_new_from_string ("fe80::/10", NULL);
  g_assert (g_inet_address_mask_get_family (mask) == G_SOCKET_FAMILY_IPV6);
  g_assert (g_inet_address_equal (addr, g_inet_address_mask_get_address (mask)));
  g_assert (g_inet_address_mask_get_length (mask) == 10);
  g_object_unref (addr);

  g_object_get (mask,
                "family", &family,
                "address", &addr,
                "length", &len,
                NULL);
  g_assert (family == G_SOCKET_FAMILY_IPV6);
  g_assert (addr != NULL);
  g_assert (len == 10);
  g_object_unref (addr);

  g_object_unref (mask);
}

static void
test_mask_equal (void)
{
  GInetAddressMask *mask;
  GInetAddressMask *mask2;
  gchar *str;

  mask = g_inet_address_mask_new_from_string ("fe80:0:0::/10", NULL);
  str = g_inet_address_mask_to_string (mask);
  g_assert_cmpstr (str, ==, "fe80::/10");
  mask2 = g_inet_address_mask_new_from_string (str, NULL);
  g_assert (g_inet_address_mask_equal (mask, mask2));
  g_object_unref (mask2);
  g_free (str);

  mask2 = g_inet_address_mask_new_from_string ("fe80::/12", NULL);
  g_assert (!g_inet_address_mask_equal (mask, mask2));
  g_object_unref (mask2);

  mask2 = g_inet_address_mask_new_from_string ("ff80::/10", NULL);
  g_assert (!g_inet_address_mask_equal (mask, mask2));
  g_object_unref (mask2);

  g_object_unref (mask);
}

static void
test_mask_match (void)
{
  GInetAddressMask *mask;
  GInetAddress *addr;

  mask = g_inet_address_mask_new_from_string ("1.2.0.0/16", NULL);

  addr = g_inet_address_new_from_string ("1.2.0.0");
  g_assert (g_inet_address_mask_matches (mask, addr));
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("1.2.3.4");
  g_assert (g_inet_address_mask_matches (mask, addr));
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("1.3.1.1");
  g_assert (!g_inet_address_mask_matches (mask, addr));
  g_object_unref (addr);

  g_object_unref (mask);

  mask = g_inet_address_mask_new_from_string ("1.2.0.0/24", NULL);

  addr = g_inet_address_new_from_string ("1.2.0.0");
  g_assert (g_inet_address_mask_matches (mask, addr));
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("1.2.3.4");
  g_assert (!g_inet_address_mask_matches (mask, addr));
  g_object_unref (addr);
  addr = g_inet_address_new_from_string ("1.2.0.24");
  g_assert (g_inet_address_mask_matches (mask, addr));
  g_object_unref (addr);

  g_object_unref (mask);

}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/inet-address/parse", test_parse);
  g_test_add_func ("/inet-address/any", test_any);
  g_test_add_func ("/inet-address/loopback", test_loopback);
  g_test_add_func ("/inet-address/bytes", test_bytes);
  g_test_add_func ("/inet-address/property", test_property);
  g_test_add_func ("/socket-address/basic", test_socket_address);
  g_test_add_func ("/socket-address/to-string", test_socket_address_to_string);
  g_test_add_func ("/address-mask/parse", test_mask_parse);
  g_test_add_func ("/address-mask/property", test_mask_property);
  g_test_add_func ("/address-mask/equal", test_mask_equal);
  g_test_add_func ("/address-mask/match", test_mask_match);

  return g_test_run ();
}
