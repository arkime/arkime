/* 
 * Copyright (C) 2008 Red Hat, Inc
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

#include <glib/glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
  const gchar *ascii_name, *unicode_name;
} idn_test_domains[] = {
  /* "example.test" in various languages */
  { "xn--mgbh0fb.xn--kgbechtv", "\xd9\x85\xd8\xab\xd8\xa7\xd9\x84.\xd8\xa5\xd8\xae\xd8\xaa\xd8\xa8\xd8\xa7\xd8\xb1" },
  { "xn--fsqu00a.xn--0zwm56d", "\xe4\xbe\x8b\xe5\xad\x90.\xe6\xb5\x8b\xe8\xaf\x95" },
  { "xn--fsqu00a.xn--g6w251d", "\xe4\xbe\x8b\xe5\xad\x90.\xe6\xb8\xac\xe8\xa9\xa6" },
  { "xn--hxajbheg2az3al.xn--jxalpdlp", "\xcf\x80\xce\xb1\xcf\x81\xce\xac\xce\xb4\xce\xb5\xce\xb9\xce\xb3\xce\xbc\xce\xb1.\xce\xb4\xce\xbf\xce\xba\xce\xb9\xce\xbc\xce\xae" },
  { "xn--p1b6ci4b4b3a.xn--11b5bs3a9aj6g", "\xe0\xa4\x89\xe0\xa4\xa6\xe0\xa4\xbe\xe0\xa4\xb9\xe0\xa4\xb0\xe0\xa4\xa3.\xe0\xa4\xaa\xe0\xa4\xb0\xe0\xa5\x80\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7\xe0\xa4\xbe" },
  { "xn--r8jz45g.xn--zckzah", "\xe4\xbe\x8b\xe3\x81\x88.\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88" },
  { "xn--9n2bp8q.xn--9t4b11yi5a", "\xec\x8b\xa4\xeb\xa1\x80.\xed\x85\x8c\xec\x8a\xa4\xed\x8a\xb8" },
  { "xn--mgbh0fb.xn--hgbk6aj7f53bba", "\xd9\x85\xd8\xab\xd8\xa7\xd9\x84.\xd8\xa2\xd8\xb2\xd9\x85\xd8\xa7\xdb\x8c\xd8\xb4\xdb\x8c" },
  { "xn--e1afmkfd.xn--80akhbyknj4f", "\xd0\xbf\xd1\x80\xd0\xb8\xd0\xbc\xd0\xb5\xd1\x80.\xd0\xb8\xd1\x81\xd0\xbf\xd1\x8b\xd1\x82\xd0\xb0\xd0\xbd\xd0\xb8\xd0\xb5" },
  { "xn--zkc6cc5bi7f6e.xn--hlcj6aya9esc7a", "\xe0\xae\x89\xe0\xae\xa4\xe0\xae\xbe\xe0\xae\xb0\xe0\xae\xa3\xe0\xae\xae\xe0\xaf\x8d.\xe0\xae\xaa\xe0\xae\xb0\xe0\xae\xbf\xe0\xae\x9f\xe0\xaf\x8d\xe0\xae\x9a\xe0\xaf\x88" },
  { "xn--fdbk5d8ap9b8a8d.xn--deba0ad", "\xd7\x91\xd7\xb2\xd6\xb7\xd7\xa9\xd7\xa4\xd6\xbc\xd7\x99\xd7\x9c.\xd7\x98\xd7\xa2\xd7\xa1\xd7\x98" },

  /* further examples without their own IDN-ized TLD */
  { "xn--1xd0bwwra.idn.icann.org", "\xe1\x8a\xa0\xe1\x88\x9b\xe1\x88\xad\xe1\x8a\x9b.idn.icann.org" },
  { "xn--54b7fta0cc.idn.icann.org", "\xe0\xa6\xac\xe0\xa6\xbe\xe0\xa6\x82\xe0\xa6\xb2\xe0\xa6\xbe.idn.icann.org" },
  { "xn--5dbqzzl.idn.icann.org", "\xd7\xa2\xd7\x91\xd7\xa8\xd7\x99\xd7\xaa.idn.icann.org" },
  { "xn--j2e7beiw1lb2hqg.idn.icann.org", "\xe1\x9e\x97\xe1\x9e\xb6\xe1\x9e\x9f\xe1\x9e\xb6\xe1\x9e\x81\xe1\x9f\x92\xe1\x9e\x98\xe1\x9f\x82\xe1\x9e\x9a.idn.icann.org" },
  { "xn--o3cw4h.idn.icann.org", "\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2.idn.icann.org" },
  { "xn--mgbqf7g.idn.icann.org", "\xd8\xa7\xd8\xb1\xd8\xaf\xd9\x88.idn.icann.org" }
};
static const gint num_idn_test_domains = G_N_ELEMENTS (idn_test_domains);

static const struct {
  const gchar *orig_name, *ascii_name;
  gboolean orig_is_unicode, ascii_is_encoded;
} non_round_trip_names[] = {
  /* uppercase characters */
  { "EXAMPLE.COM", "example.com", FALSE, FALSE },
  { "\xc3\x89XAMPLE.COM", "xn--xample-9ua.com", TRUE, TRUE },

  /* unicode that decodes to ascii */
  { "\xe2\x93\x94\xe2\x93\xa7\xe2\x93\x90\xe2\x93\x9c\xe2\x93\x9f\xe2\x93\x9b\xe2\x93\x94.com", "example.com", TRUE, FALSE },

  /* non-standard dot characters */
  { "example\xe3\x80\x82" "com", "example.com", TRUE, FALSE },
  { "\xc3\xa9xample\xe3\x80\x82" "com", "xn--xample-9ua.com", TRUE, TRUE },
  { "Å.idn.icann.org", "xn--5ca.idn.icann.org", TRUE, TRUE },
  { "ℵℶℷ\xcd\x8f.idn.icann.org", "xn--4dbcd.idn.icann.org", TRUE, TRUE }
};
static const gint num_non_round_trip_names = G_N_ELEMENTS (non_round_trip_names);

static const gchar *bad_names[] = {
  "disallowed\xef\xbf\xbd" "character",
  "non-utf\x88",
  "xn--mixed-\xc3\xbcp"
};
static const gint num_bad_names = G_N_ELEMENTS (bad_names);

static void
test_to_ascii (void)
{
  gint i;
  gchar *ascii;

  for (i = 0; i < num_idn_test_domains; i++)
    {
      g_assert (g_hostname_is_non_ascii (idn_test_domains[i].unicode_name));
      ascii = g_hostname_to_ascii (idn_test_domains[i].unicode_name);
      g_assert_cmpstr (idn_test_domains[i].ascii_name, ==, ascii);
      g_free (ascii);

      ascii = g_hostname_to_ascii (idn_test_domains[i].ascii_name);
      g_assert_cmpstr (idn_test_domains[i].ascii_name, ==, ascii);
      g_free (ascii);
    }

  for (i = 0; i < num_non_round_trip_names; i++)
    {
      if (non_round_trip_names[i].orig_is_unicode)
	g_assert (g_hostname_is_non_ascii (non_round_trip_names[i].orig_name));
      else
	g_assert (!g_hostname_is_non_ascii (non_round_trip_names[i].orig_name));

      if (non_round_trip_names[i].ascii_is_encoded)
	g_assert (g_hostname_is_ascii_encoded (non_round_trip_names[i].ascii_name));
      else
	g_assert (!g_hostname_is_ascii_encoded (non_round_trip_names[i].ascii_name));

      ascii = g_hostname_to_ascii (non_round_trip_names[i].orig_name);
      g_assert_cmpstr (non_round_trip_names[i].ascii_name, ==, ascii);
      g_free (ascii);

      ascii = g_hostname_to_ascii (non_round_trip_names[i].ascii_name);
      g_assert_cmpstr (non_round_trip_names[i].ascii_name, ==, ascii);
      g_free (ascii);
    }

  for (i = 0; i < num_bad_names; i++)
    {
      ascii = g_hostname_to_ascii (bad_names[i]);
      g_assert_cmpstr (ascii, ==, NULL);
    }
}

static void
test_to_unicode (void)
{
  gint i;
  gchar *unicode;

  for (i = 0; i < num_idn_test_domains; i++)
    {
      g_assert (g_hostname_is_ascii_encoded (idn_test_domains[i].ascii_name));
      unicode = g_hostname_to_unicode (idn_test_domains[i].ascii_name);
      g_assert_cmpstr (idn_test_domains[i].unicode_name, ==, unicode);
      g_free (unicode);

      unicode = g_hostname_to_unicode (idn_test_domains[i].unicode_name);
      g_assert_cmpstr (idn_test_domains[i].unicode_name, ==, unicode);
      g_free (unicode);
    }

  for (i = 0; i < num_bad_names; i++)
    {
      unicode = g_hostname_to_unicode (bad_names[i]);
      g_assert_cmpstr (unicode, ==, NULL);
    }
}

static const struct {
  const gchar *addr;
  gboolean is_addr;
} ip_addr_tests[] = {
  /* IPv6 tests */

  { "0123:4567:89AB:cdef:3210:7654:ba98:FeDc", TRUE },

  { "0123:4567:89AB:cdef:3210:7654:ba98::", TRUE },
  { "0123:4567:89AB:cdef:3210:7654::", TRUE },
  { "0123:4567:89AB:cdef:3210::", TRUE },
  { "0123:4567:89AB:cdef::", TRUE },
  { "0123:4567:89AB::", TRUE },
  { "0123:4567::", TRUE },
  { "0123::", TRUE },

  { "::4567:89AB:cdef:3210:7654:ba98:FeDc", TRUE },
  { "::89AB:cdef:3210:7654:ba98:FeDc", TRUE },
  { "::cdef:3210:7654:ba98:FeDc", TRUE },
  { "::3210:7654:ba98:FeDc", TRUE },
  { "::7654:ba98:FeDc", TRUE },
  { "::ba98:FeDc", TRUE },
  { "::FeDc", TRUE },

  { "0123::89AB:cdef:3210:7654:ba98:FeDc", TRUE },
  { "0123:4567::cdef:3210:7654:ba98:FeDc", TRUE },
  { "0123:4567:89AB::3210:7654:ba98:FeDc", TRUE },
  { "0123:4567:89AB:cdef::7654:ba98:FeDc", TRUE },
  { "0123:4567:89AB:cdef:3210::ba98:FeDc", TRUE },
  { "0123:4567:89AB:cdef:3210:7654::FeDc", TRUE },

  { "0123::cdef:3210:7654:ba98:FeDc", TRUE },
  { "0123:4567::3210:7654:ba98:FeDc", TRUE },
  { "0123:4567:89AB::7654:ba98:FeDc", TRUE },
  { "0123:4567:89AB:cdef::ba98:FeDc", TRUE },
  { "0123:4567:89AB:cdef:3210::FeDc", TRUE },

  { "0123::3210:7654:ba98:FeDc", TRUE },
  { "0123:4567::7654:ba98:FeDc", TRUE },
  { "0123:4567:89AB::ba98:FeDc", TRUE },
  { "0123:4567:89AB:cdef::FeDc", TRUE },

  { "0123::7654:ba98:FeDc", TRUE },
  { "0123:4567::ba98:FeDc", TRUE },
  { "0123:4567:89AB::FeDc", TRUE },

  { "0123::ba98:FeDc", TRUE },
  { "0123:4567::FeDc", TRUE },

  { "0123::FeDc", TRUE },

  { "::", TRUE },

  { "0:12:345:6789:a:bc:def::", TRUE },

  { "0123:4567:89AB:cdef:3210:7654:123.45.67.89", TRUE },
  { "0123:4567:89AB:cdef:3210::123.45.67.89", TRUE },
  { "0123:4567:89AB:cdef::123.45.67.89", TRUE },
  { "0123:4567:89AB::123.45.67.89", TRUE },
  { "0123:4567::123.45.67.89", TRUE },
  { "0123::123.45.67.89", TRUE },
  { "::123.45.67.89", TRUE },

  /* Contain non-hex chars */
  { "012x:4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:45x7:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:8xAB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:xdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:321;:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:76*4:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:b-98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:ba98:+eDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:ba98:FeDc and some trailing junk", FALSE },
  { " 123:4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "012 :4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123: 567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:ba98:FeD ", FALSE },

  /* Contains too-long/out-of-range segments */
  { "00123:4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:04567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:189AB:cdef:3210:7654:ba98:FeDc", FALSE },

  /* Too short */
  { "0123:4567:89AB:cdef:3210:7654:ba98", FALSE },
  { "0123:4567:89AB:cdef:3210:7654", FALSE },
  { "0123:4567:89AB:cdef:3210", FALSE },
  { "0123", FALSE },
  { "", FALSE },

  /* Too long */
  { "0123:4567:89AB:cdef:3210:7654:ba98:FeDc:9999", FALSE },
  { "0123::4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567::89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB::cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef::3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210::7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654::ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:ba98::FeDc", FALSE },

  /* Invalid use of ":"s */
  { "0123::89AB::3210:7654:ba98:FeDc", FALSE },
  { "::4567:89AB:cdef:3210:7654::FeDc", FALSE },
  { "0123::89AB:cdef:3210:7654:ba98::", FALSE },
  { ":4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:ba98:", FALSE },
  { "0123:::cdef:3210:7654:ba98:FeDc", FALSE },
  { "0123:4567:89AB:cdef:3210:7654:ba98:FeDc:", FALSE },
  { ":0123:4567:89AB:cdef:3210:7654:ba98:FeDc", FALSE },
  { ":::", FALSE },

  /* IPv4 address at wrong place */
  { "0123:4567:89AB:cdef:3210:123.45.67.89", FALSE },
  { "0123:4567:89AB:cdef:3210:7654::123.45.67.89", FALSE },
  { "0123:4567:89AB:cdef:123.45.67.89", FALSE },
  { "0123:4567:89AB:cdef:3210:123.45.67.89:FeDc", FALSE },


  /* IPv4 tests */

  { "123.45.67.89", TRUE },
  { "1.2.3.4", TRUE },
  { "1.2.3.0", TRUE },

  { "023.045.067.089", FALSE },
  { "1234.5.67.89", FALSE },
  { "123.45.67.00", FALSE },
  { " 1.2.3.4", FALSE },
  { "1 .2.3.4", FALSE },
  { "1. 2.3.4", FALSE },
  { "1.2.3.4 ", FALSE },
  { "1.2.3", FALSE },
  { "1.2.3.4.5", FALSE },
  { "1.b.3.4", FALSE },
  { "1.2.3:4", FALSE },
  { "1.2.3.4, etc", FALSE },
  { "1,2,3,4", FALSE },
  { "1.2.3.com", FALSE },
  { "1.2.3.4.", FALSE },
  { "1.2.3.", FALSE },
  { ".1.2.3.4", FALSE },
  { ".2.3.4", FALSE },
  { "1..2.3.4", FALSE },
  { "1..3.4", FALSE }
};
static const gint num_ip_addr_tests = G_N_ELEMENTS (ip_addr_tests);

static void
test_is_ip_addr (void)
{
  gint i;

  for (i = 0; i < num_ip_addr_tests; i++)
    {
      if (g_hostname_is_ip_address (ip_addr_tests[i].addr) != ip_addr_tests[i].is_addr)
	{
	  char *msg = g_strdup_printf ("g_hostname_is_ip_address (\"%s\") == %s",
				       ip_addr_tests[i].addr,
				       ip_addr_tests[i].is_addr ? "TRUE" : "FALSE");
	  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, msg);
	}
    }
}

/* FIXME: test names with both unicode and ACE-encoded labels */
/* FIXME: test invalid unicode names */

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  
  if (argc == 2 && argv[1][0] != '-')
    {
      const gchar *hostname = argv[1];
      gchar *converted;

      if (g_hostname_is_non_ascii (hostname))
	{
	  converted = g_hostname_to_ascii (hostname);
	  printf ("to_ascii: %s\n", converted);
	  g_free (converted);
	}
      else if (g_hostname_is_ascii_encoded (hostname))
	{
	  converted = g_hostname_to_unicode (hostname);
	  printf ("to_unicode: %s\n", converted);
	  g_free (converted);
	}
      else
	printf ("hostname is neither unicode nor ACE encoded\n");
      return 0;
    }

  g_test_add_func ("/hostutils/to_ascii", test_to_ascii);
  g_test_add_func ("/hostutils/to_unicode", test_to_unicode);
  g_test_add_func ("/hostutils/is_ip_addr", test_is_ip_addr);

  return g_test_run ();
}
