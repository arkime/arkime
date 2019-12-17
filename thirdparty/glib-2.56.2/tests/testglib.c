/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef GLIB_COMPILATION

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "glib.h"
#include <glib/gstdio.h>

#include <stdlib.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#include <io.h>			/* For read(), write() etc */
#endif


#define GLIB_TEST_STRING "el dorado "
#define GLIB_TEST_STRING_5 "el do"


/* --- variables --- */
static gint test_nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static gint more_nums[10] = { 8, 9, 7, 0, 3, 2, 5, 1, 4, 6};

/* --- functions --- */
static gint
my_list_compare_one (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return one-two;
}

static gint
my_list_compare_two (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return two-one;
}

/* static void
my_list_print (gpointer a, gpointer b)
{
  gint three = *((gint*)a);
  g_printerr ("%d", three);
}; */

static void
glist_test (void)
{
  GList *list = NULL;
  guint i;

  for (i = 0; i < 10; i++)
    list = g_list_append (list, &test_nums[i]);
  list = g_list_reverse (list);

  for (i = 0; i < 10; i++)
    {
      GList *t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
	g_error ("Regular insert failed");
    }

  for (i = 0; i < 10; i++)
    if (g_list_position (list, g_list_nth (list, i)) != i)
      g_error ("g_list_position does not seem to be the inverse of g_list_nth\n");

  g_list_free (list);
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_insert_sorted (list, &more_nums[i], my_list_compare_one);

  /*
  g_printerr ("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      GList *t = g_list_nth (list, i);
      if (*((gint*) t->data) != i)
         g_error ("Sorted insert failed");
    }

  g_list_free (list);
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_insert_sorted (list, &more_nums[i], my_list_compare_two);

  /*
  g_printerr ("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      GList *t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
         g_error ("Sorted insert failed");
    }

  g_list_free (list);
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_prepend (list, &more_nums[i]);

  list = g_list_sort (list, my_list_compare_two);

  /*
  g_printerr ("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      GList *t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
         g_error ("Merge sort failed");
    }

  g_list_free (list);
}

static void
gslist_test (void)
{
  GSList *slist = NULL;
  guint i;

  for (i = 0; i < 10; i++)
    slist = g_slist_append (slist, &test_nums[i]);
  slist = g_slist_reverse (slist);

  for (i = 0; i < 10; i++)
    {
      GSList *st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
	g_error ("failed");
    }

  g_slist_free (slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_insert_sorted (slist, &more_nums[i], my_list_compare_one);

  /*
  g_printerr ("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      GSList *st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != i)
        g_error ("Sorted insert failed");
    }

  g_slist_free (slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_insert_sorted (slist, &more_nums[i], my_list_compare_two);

  /*
  g_printerr ("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      GSList *st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
        g_error("Sorted insert failed");
    }

  g_slist_free(slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_prepend (slist, &more_nums[i]);

  slist = g_slist_sort (slist, my_list_compare_two);

  /*
  g_printerr ("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      GSList *st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
        g_error("Sorted insert failed");
    }

  g_slist_free(slist);
}

static gboolean
node_build_string (GNode    *node,
		   gpointer  data)
{
  gchar **p = data;
  gchar *string;
  gchar c[2] = "_";

  c[0] = ((gchar) ((gintptr) (node->data)));

  string = g_strconcat (*p ? *p : "", c, NULL);
  g_free (*p);
  *p = string;

  return FALSE;
}

static void
gnode_test (void)
{
#define	C2P(c)		((gpointer) ((long) (c)))
#define	P2C(p)		((gchar) ((gintptr) (p)))
  GNode *root;
  GNode *node;
  GNode *node_B;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  guint i;
  gchar *tstring, *cstring;

  root = g_node_new (C2P ('A'));
  g_assert (g_node_depth (root) == 1 && g_node_max_height (root) == 1);

  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  g_assert (root->children == node_B);

  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  g_node_insert (node_B, 1, g_node_new (C2P ('D')));

  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  g_assert (root->children->next == node_F);

  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_prepend (node_G, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  g_node_insert_data (node_G, 0, C2P ('H'));
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

  g_assert (g_node_depth (root) == 1);
  g_assert (g_node_max_height (root) == 4);
  g_assert (g_node_depth (node_G->children->next) == 4);
  g_assert (g_node_n_nodes (root, G_TRAVERSE_LEAFS) == 7);
  g_assert (g_node_n_nodes (root, G_TRAVERSE_NON_LEAFS) == 4);
  g_assert (g_node_n_nodes (root, G_TRAVERSE_ALL) == 11);
  g_assert (g_node_max_height (node_F) == 3);
  g_assert (g_node_n_children (node_G) == 4);
  g_assert (g_node_find_child (root, G_TRAVERSE_ALL, C2P ('F')) == node_F);
  g_assert (g_node_find (root, G_LEVEL_ORDER, G_TRAVERSE_NON_LEAFS, C2P ('I')) == NULL);
  g_assert (g_node_find (root, G_IN_ORDER, G_TRAVERSE_LEAFS, C2P ('J')) == node_J);

  for (i = 0; i < g_node_n_children (node_B); i++)
    {
      node = g_node_nth_child (node_B, i);
      g_assert (P2C (node->data) == ('C' + i));
    }
  
  for (i = 0; i < g_node_n_children (node_G); i++)
    g_assert (g_node_child_position (node_G, g_node_nth_child (node_G, i)) == i);

  /* we have built:                    A
   *                                 /   \
   *                               B       F
   *                             / | \       \
   *                           C   D   E       G
   *                                         / /\ \
   *                                       H  I  J  K
   *
   * for in-order traversal, 'G' is considered to be the "left"
   * child of 'F', which will cause 'F' to be the last node visited.
   */

  tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABCDEFGHIJK");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_POST_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "CDEBHIJKGFA");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "CBDEAHGIJKF");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFCDEGHIJK");
  g_free (tstring); tstring = NULL;
  
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_LEAFS, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "CDEHIJK");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_NON_LEAFS, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFG");
  g_free (tstring); tstring = NULL;

  g_node_reverse_children (node_B);
  g_node_reverse_children (node_G);

  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFEDCGKJIH");
  g_free (tstring); tstring = NULL;

  cstring = NULL;
  node = g_node_copy (root);
  g_assert (g_node_n_nodes (root, G_TRAVERSE_ALL) == g_node_n_nodes (node, G_TRAVERSE_ALL));
  g_assert (g_node_max_height (root) == g_node_max_height (node));
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &cstring);
  g_assert_cmpstr (tstring, ==, cstring);
  g_free (tstring); tstring = NULL;
  g_free (cstring); cstring = NULL;
  g_node_destroy (node);

  g_node_destroy (root);

  /* allocation tests */

  root = g_node_new (NULL);
  node = root;

  for (i = 0; i < 2048; i++)
    {
      g_node_append (node, g_node_new (NULL));
      if ((i%5) == 4)
	node = node->children->next;
    }
  g_assert (g_node_max_height (root) > 100);
  g_assert (g_node_n_nodes (root, G_TRAVERSE_ALL) == 1 + 2048);

  g_node_destroy (root);
#undef C2P
#undef P2C
}

static gint
my_compare (gconstpointer a,
	    gconstpointer b)
{
  const char *cha = a;
  const char *chb = b;

  return *cha - *chb;
}

static gint
my_traverse (gpointer key,
	     gpointer value,
	     gpointer data)
{
  char *ch = key;
  g_printerr ("%c ", *ch);
  return FALSE;
}

static void
binary_tree_test (void)
{
  GTree *tree;
  char chars[62];
  guint i, j;

  tree = g_tree_new (my_compare);
  i = 0;
  for (j = 0; j < 10; j++, i++)
    {
      chars[i] = '0' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }
  for (j = 0; j < 26; j++, i++)
    {
      chars[i] = 'A' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }
  for (j = 0; j < 26; j++, i++)
    {
      chars[i] = 'a' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }

  g_assert_cmpint (g_tree_nnodes (tree), ==, 10 + 26 + 26);
  g_assert_cmpint (g_tree_height (tree), ==, 6);

  if (g_test_verbose())
    {
      g_printerr ("tree: ");
      g_tree_foreach (tree, my_traverse, NULL);
      g_printerr ("\n");
    }

  for (i = 0; i < 10; i++)
    g_tree_remove (tree, &chars[i]);

  g_assert_cmpint (g_tree_nnodes (tree), ==, 26 + 26);
  g_assert_cmpint (g_tree_height (tree), ==, 6);

  if (g_test_verbose())
    {
      g_printerr ("tree: ");
      g_tree_foreach (tree, my_traverse, NULL);
      g_printerr ("\n");
    }

  g_tree_unref (tree);
}

static gboolean
my_hash_callback_remove (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static void
my_hash_callback_remove_test (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    g_error ("hash table entry %d should have been removed already\n", *d);
}

static void
my_hash_callback (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
  int *d = value;
  *d = 1;
}

static guint
my_hash (gconstpointer key)
{
  return (guint) *((const gint*) key);
}

static gboolean
my_hash_equal (gconstpointer a,
	       gconstpointer b)
{
  return *((const gint*) a) == *((const gint*) b);
}

static gboolean 
find_first_that(gpointer key, 
		gpointer value, 
		gpointer user_data)
{
  gint *v = value;
  gint *test = user_data;
  return (*v == *test);
}

static void
test_g_parse_debug_string (void)
{
  GDebugKey keys[] = { 
    { "foo", 1 },
    { "bar", 2 },
    { "baz", 4 },
    { "weird", 8 },
  };
  guint n_keys = G_N_ELEMENTS (keys);
  guint result;
  
  result = g_parse_debug_string ("bar:foo:blubb", keys, n_keys);
  g_assert (result == 3);

  result = g_parse_debug_string (":baz::_E@~!_::", keys, n_keys);
  g_assert (result == 4);

  result = g_parse_debug_string ("", keys, n_keys);
  g_assert (result == 0);

  result = g_parse_debug_string (" : ", keys, n_keys);
  g_assert (result == 0);

  result = g_parse_debug_string ("all", keys, n_keys);
  g_assert_cmpuint (result, ==, (1 << n_keys) - 1);

  /* Test subtracting debug flags from "all" */
  result = g_parse_debug_string ("all:foo", keys, n_keys);
  g_assert_cmpuint (result, ==, 2 | 4 | 8);

  result = g_parse_debug_string ("foo baz,all", keys, n_keys);
  g_assert_cmpuint (result, ==, 2 | 8);

  result = g_parse_debug_string ("all,fooo,baz", keys, n_keys);
  g_assert_cmpuint (result, ==, 1 | 2 | 8);

  result = g_parse_debug_string ("all:weird", keys, n_keys);
  g_assert_cmpuint (result, ==, 1 | 2 | 4);
}

static void
log_warning_error_tests (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE,
                         "*is a g_message test*");
  g_message ("this is a g_message test.");
  g_test_assert_expected_messages ();

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE,
                         "*non-printable UTF-8*");
  g_message ("non-printable UTF-8: \"\xc3\xa4\xda\x85\"");
  g_test_assert_expected_messages ();

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE,
                         "*unsafe chars*");
  g_message ("unsafe chars: \"\x10\x11\x12\n\t\x7f\x81\x82\x83\"");
  g_test_assert_expected_messages ();

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "*harmless warning*");
  g_warning ("harmless warning with parameters: %d %s %#x", 42, "Boo", 12345);
  g_test_assert_expected_messages ();

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*g_print*assertion*failed*");
  g_print (NULL);
  g_test_assert_expected_messages ();
}

static void
timer_tests (void)
{
  GTimer *timer, *timer2;
  gdouble elapsed;

  /* basic testing */
  timer = g_timer_new ();
  g_timer_start (timer);
  elapsed = g_timer_elapsed (timer, NULL);
  g_timer_stop (timer);
  g_assert_cmpfloat (elapsed, <=, g_timer_elapsed (timer, NULL));
  g_timer_destroy (timer);

  if (g_test_slow())
    {
      if (g_test_verbose())
        g_printerr ("checking timers...\n");
      timer = g_timer_new ();
      if (g_test_verbose())
        g_printerr ("  spinning for 3 seconds...\n");
      g_timer_start (timer);
      while (g_timer_elapsed (timer, NULL) < 3)
        ;
      g_timer_stop (timer);
      g_timer_destroy (timer);
      if (g_test_verbose())
        g_printerr ("ok\n");
    }

  if (g_test_slow())
    {
      gulong elapsed_usecs;
      if (g_test_verbose())
        g_printerr ("checking g_timer_continue...\n");
      timer2 = g_timer_new ();
      if (g_test_verbose())
        g_printerr ("\trun for 1 second...\n");
      timer = g_timer_new();
      g_usleep (G_USEC_PER_SEC); /* run timer for 1 second */
      g_timer_stop (timer);
      if (g_test_verbose())
        g_printerr ("\tstop for 1 second...\n");
      g_usleep (G_USEC_PER_SEC); /* wait for 1 second */
      if (g_test_verbose())
        g_printerr ("\trun for 2 seconds...\n");
      g_timer_continue (timer);
      g_usleep (2 * G_USEC_PER_SEC); /* run timer for 2 seconds */
      g_timer_stop(timer);
      if (g_test_verbose())
        g_printerr ("\tstop for 1.5 seconds...\n");
      g_usleep ((3 * G_USEC_PER_SEC) / 2); /* wait for 1.5 seconds */
      if (g_test_verbose())
        g_printerr ("\trun for 0.2 seconds...\n");
      g_timer_continue (timer);
      g_usleep (G_USEC_PER_SEC / 5); /* run timer for 0.2 seconds */
      g_timer_stop (timer);
      if (g_test_verbose())
        g_printerr ("\tstop for 4 seconds...\n");
      g_usleep (4 * G_USEC_PER_SEC); /* wait for 4 seconds */
      if (g_test_verbose())
        g_printerr ("\trun for 5.8 seconds...\n");
      g_timer_continue (timer);
      g_usleep ((29 * G_USEC_PER_SEC) / 5); /* run timer for 5.8 seconds */
      g_timer_stop(timer);
      elapsed = g_timer_elapsed (timer, &elapsed_usecs);
      if (g_test_verbose())
        g_printerr ("\t=> timer = %.6f = %d.%06ld (should be: 9.000000) (%.6f off)\n", elapsed, (int) elapsed, elapsed_usecs, ABS (elapsed - 9.));
      g_assert_cmpfloat (elapsed, >, 8.8);
      g_assert_cmpfloat (elapsed, <, 9.2);
      if (g_test_verbose())
        g_printerr ("g_timer_continue ... ok\n\n");
      g_timer_stop (timer2);
      elapsed = g_timer_elapsed (timer2, &elapsed_usecs);
      if (g_test_verbose())
        g_printerr ("\t=> timer2 = %.6f = %d.%06ld (should be: %.6f) (%.6f off)\n\n", elapsed, (int) elapsed, elapsed_usecs, 9.+6.5, ABS (elapsed - (9.+6.5)));
      g_assert_cmpfloat (elapsed, >, 8.8 + 6.5);
      g_assert_cmpfloat (elapsed, <, 9.2 + 6.5);
      if (g_test_verbose())
        g_printerr ("timer2 ... ok\n\n");
      g_timer_destroy (timer);
      g_timer_destroy (timer2);
    }
}

static void
type_sizes (void)
{
  guint16 gu16t1 = 0x44afU, gu16t2 = 0xaf44U;
  guint32 gu32t1 = 0x02a7f109U, gu32t2 = 0x09f1a702U;
  guint64 gu64t1 = G_GINT64_CONSTANT(0x1d636b02300a7aa7U),
	  gu64t2 = G_GINT64_CONSTANT(0xa77a0a30026b631dU);
  /* type sizes */
  g_assert_cmpint (sizeof (gint8), ==, 1);
  g_assert_cmpint (sizeof (gint16), ==, 2);
  g_assert_cmpint (sizeof (gint32), ==, 4);
  g_assert_cmpint (sizeof (gint64), ==, 8);
  /* endian macros */
  if (g_test_verbose())
    g_printerr ("checking endian macros (host is %s)...\n",
             G_BYTE_ORDER == G_BIG_ENDIAN ? "big endian" : "little endian");
  g_assert (GUINT16_SWAP_LE_BE (gu16t1) == gu16t2);
  g_assert (GUINT32_SWAP_LE_BE (gu32t1) == gu32t2);
  g_assert (GUINT64_SWAP_LE_BE (gu64t1) == gu64t2);
}

static void
test_info (void)
{
  const gchar *un, *rn, *hn;
  const gchar *tmpdir, *homedir, *userdatadir, *uconfdir, *ucachedir;
  const gchar *uddesktop, *udddocs, *uddpubshare, *uruntimedir;
  gchar **sv, *cwd, *sdatadirs, *sconfdirs, *langnames;
  const gchar *charset;
  gboolean charset_is_utf8;
  if (g_test_verbose())
    g_printerr ("TestGLib v%u.%u.%u (i:%u b:%u)\n",
             glib_major_version,
             glib_minor_version,
             glib_micro_version,
             glib_interface_age,
             glib_binary_age);

  cwd = g_get_current_dir ();
  un = g_get_user_name();
  rn = g_get_real_name();
  hn = g_get_host_name();
  if (g_test_verbose())
    {
      g_printerr ("cwd: %s\n", cwd);
      g_printerr ("user: %s\n", un);
      g_printerr ("real: %s\n", rn);
      g_printerr ("host: %s\n", hn);
    }
  g_free (cwd);

  /* reload, just for fun */
  g_reload_user_special_dirs_cache ();
  g_reload_user_special_dirs_cache ();

  tmpdir = g_get_tmp_dir();
  g_assert (tmpdir != NULL);
  homedir = g_get_home_dir ();
  g_assert (homedir != NULL);
  userdatadir = g_get_user_data_dir ();
  g_assert (userdatadir != NULL);
  uconfdir = g_get_user_config_dir ();
  g_assert (uconfdir != NULL);
  ucachedir = g_get_user_cache_dir ();
  g_assert (ucachedir != NULL);

  uddesktop = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_assert (uddesktop != NULL);
  udddocs = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
  uddpubshare = g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE);
  uruntimedir = g_get_user_runtime_dir ();
  g_assert (uruntimedir != NULL);

  sv = (gchar **) g_get_system_data_dirs ();
  sdatadirs = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, sv);
  sv = (gchar **) g_get_system_config_dirs ();
  sconfdirs = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, sv);
  sv = (gchar **) g_get_language_names ();
  langnames = g_strjoinv (":", sv);

  if (g_test_verbose())
    {
      g_printerr ("tmp-dir: %s\n", tmpdir);
      g_printerr ("home: %s\n", homedir);
      g_printerr ("user_data: %s\n", userdatadir);
      g_printerr ("user_config: %s\n", uconfdir);
      g_printerr ("user_cache: %s\n", ucachedir);
      g_printerr ("user_runtime: %s\n", uruntimedir);
      g_printerr ("system_data: %s\n", sdatadirs);
      g_printerr ("system_config: %s\n", sconfdirs);
      g_printerr ("languages: %s\n", langnames);
      g_printerr ("user_special[DESKTOP]: %s\n", uddesktop);
      g_printerr ("user_special[DOCUMENTS]: %s\n", udddocs);
      g_printerr ("user_special[PUBLIC_SHARE]: %s\n", uddpubshare);
    }
  g_free (sdatadirs);
  g_free (sconfdirs);
  g_free (langnames);

  charset_is_utf8 = g_get_charset ((const char**)&charset);

  if (g_test_verbose())
    {
      if (charset_is_utf8)
        g_printerr ("current charset is UTF-8: %s\n", charset);
      else
        g_printerr ("current charset is not UTF-8: %s\n", charset);
    }

  if (g_test_verbose())
    {
#ifdef G_PLATFORM_WIN32
      g_printerr ("current locale: %s\n", g_win32_getlocale ());

      g_printerr ("found more.com as %s\n", g_find_program_in_path ("more.com"));
      g_printerr ("found regedit as %s\n", g_find_program_in_path ("regedit"));

      g_printerr ("a Win32 error message: %s\n", g_win32_error_message (2));
#endif
    }
}

static void
test_paths (void)
{
  struct {
    gchar *filename;
    gchar *dirname;
  } dirname_checks[] = {
    { "/", "/" },
    { "////", "/" },
    { ".////", "." },
    { "../", ".." },
    { "..////", ".." },
    { "a/b", "a" },
    { "a/b/", "a/b" },
    { "c///", "c" },
#ifdef G_OS_WIN32
    { "\\", "\\" },
    { ".\\\\\\\\", "." },
    { "..\\", ".." },
    { "..\\\\\\\\", ".." },
    { "a\\b", "a" },
    { "a\\b/", "a\\b" },
    { "a/b\\", "a/b" },
    { "c\\\\/", "c" },
    { "//\\", "/" },
#endif
#ifdef G_WITH_CYGWIN
    { "//server/share///x", "//server/share" },
#endif
    { ".", "." },
    { "..", "." },
    { "", "." },
  };
  const guint n_dirname_checks = G_N_ELEMENTS (dirname_checks);
  struct {
    gchar *filename;
    gchar *without_root;
  } skip_root_checks[] = {
    { "/", "" },
    { "//", "" },
    { "/foo", "foo" },
    { "//foo", "foo" },
    { "a/b", NULL },
#ifdef G_OS_WIN32
    { "\\", "" },
    { "\\foo", "foo" },
    { "\\\\server\\foo", "" },
    { "\\\\server\\foo\\bar", "bar" },
    { "a\\b", NULL },
#endif
#ifdef G_WITH_CYGWIN
    { "//server/share///x", "//x" },
#endif
    { ".", NULL },
    { "", NULL },
  };
  const guint n_skip_root_checks = G_N_ELEMENTS (skip_root_checks);
  gchar *string;
  guint i;
  if (g_test_verbose())
    g_printerr ("checking g_path_get_basename()...");
  string = g_path_get_basename (G_DIR_SEPARATOR_S "foo" G_DIR_SEPARATOR_S "dir" G_DIR_SEPARATOR_S);
  g_assert (strcmp (string, "dir") == 0);
  g_free (string);
  string = g_path_get_basename (G_DIR_SEPARATOR_S "foo" G_DIR_SEPARATOR_S "file");
  g_assert (strcmp (string, "file") == 0);
  g_free (string);
  if (g_test_verbose())
    g_printerr ("ok\n");

#ifdef G_OS_WIN32
  string = g_path_get_basename ("/foo/dir/");
  g_assert (strcmp (string, "dir") == 0);
  g_free (string);
  string = g_path_get_basename ("/foo/file");
  g_assert (strcmp (string, "file") == 0);
  g_free (string);
#endif

  if (g_test_verbose())
    g_printerr ("checking g_path_get_dirname()...");
  for (i = 0; i < n_dirname_checks; i++)
    {
      gchar *dirname = g_path_get_dirname (dirname_checks[i].filename);
      if (strcmp (dirname, dirname_checks[i].dirname) != 0)
	{
	  g_error ("\nfailed for \"%s\"==\"%s\" (returned: \"%s\")\n",
		   dirname_checks[i].filename,
		   dirname_checks[i].dirname,
		   dirname);
	}
      g_free (dirname);
    }
  if (g_test_verbose())
    g_printerr ("ok\n");

  if (g_test_verbose())
    g_printerr ("checking g_path_skip_root()...");
  for (i = 0; i < n_skip_root_checks; i++)
    {
      const gchar *skipped = g_path_skip_root (skip_root_checks[i].filename);
      if ((skipped && !skip_root_checks[i].without_root) ||
	  (!skipped && skip_root_checks[i].without_root) ||
	  ((skipped && skip_root_checks[i].without_root) &&
	   strcmp (skipped, skip_root_checks[i].without_root)))
	{
	  g_error ("\nfailed for \"%s\"==\"%s\" (returned: \"%s\")\n",
		   skip_root_checks[i].filename,
		   (skip_root_checks[i].without_root ?
		    skip_root_checks[i].without_root : "<NULL>"),
		   (skipped ? skipped : "<NULL>"));
	}
    }
  if (g_test_verbose())
    g_printerr ("ok\n");
}

static void
test_file_functions (void)
{
  const char hello[] = "Hello, World";
  const int hellolen = sizeof (hello) - 1;
  GError *error;
  char template[32];
  char *name_used, chars[62];
  gint fd, n;
  int errsv;
  
  strcpy (template, "foobar");
  fd = g_mkstemp (template);
  if (g_test_verbose() && fd != -1)
    g_printerr ("g_mkstemp works even if template doesn't end in XXXXXX\n");
  if (fd != -1)
    close (fd);
  strcpy (template, "fooXXXXXX");
  fd = g_mkstemp (template);
  if (fd == -1)
    g_error ("g_mkstemp didn't work for template %s\n", template);
  n = write (fd, hello, hellolen);
  errsv = errno;
  if (n == -1)
    g_error ("write() failed: %s\n", g_strerror (errsv));
  else if (n != hellolen)
    g_error ("write() should have written %d bytes, wrote %d\n", hellolen, n);

  lseek (fd, 0, 0);
  n = read (fd, chars, sizeof (chars));
  errsv = errno;
  if (n == -1)
    g_error ("read() failed: %s\n", g_strerror (errsv));
  else if (n != hellolen)
    g_error ("read() should have read %d bytes, got %d\n", hellolen, n);

  chars[n] = 0;
  if (strcmp (chars, hello) != 0)
    g_error ("wrote '%s', but got '%s'\n", hello, chars);
  if (fd != -1)
    close (fd);
  remove (template);

  error = NULL;
  name_used = NULL;
  strcpy (template, "zap" G_DIR_SEPARATOR_S "barXXXXXX");
  fd = g_file_open_tmp (template, &name_used, &error);
  if (g_test_verbose())
    {
      if (fd != -1)
        g_printerr ("g_file_open_tmp works even if template contains '%s'\n", G_DIR_SEPARATOR_S);
      else
        g_printerr ("g_file_open_tmp correctly returns error: %s\n", error->message);
    }
  if (fd != -1)
    close (fd);
  g_clear_error (&error);
  g_free (name_used);

#ifdef G_OS_WIN32
  name_used = NULL;
  strcpy (template, "zap/barXXXXXX");
  fd = g_file_open_tmp (template, &name_used, &error);
  if (g_test_verbose())
    {
      if (fd != -1)
        g_printerr ("g_file_open_tmp works even if template contains '/'\n");
      else
        g_printerr ("g_file_open_tmp correctly returns error: %s\n", error->message);
    }
  if (fd != -1)
    close (fd);
  g_clear_error (&error);
  g_free (name_used);
#endif

  strcpy (template, "zapXXXXXX");
  name_used = NULL;
  fd = g_file_open_tmp (template, &name_used, &error);
  if (fd == -1)
    g_error ("g_file_open_tmp didn't work for template '%s': %s\n", template, error->message);
  else if (g_test_verbose())
    g_printerr ("g_file_open_tmp for template '%s' used name '%s'\n", template, name_used);
  if (fd != -1)
    close (fd);
  g_clear_error (&error);
  remove (name_used);
  g_free (name_used);

  name_used = NULL;
  fd = g_file_open_tmp (NULL, &name_used, &error);
  if (fd == -1)
    g_error ("g_file_open_tmp didn't work for a NULL template: %s\n", error->message);
  else
    close (fd);
  g_clear_error (&error);
  remove (name_used);
  g_free (name_used);
}

static void
test_arrays (void)
{
  GByteArray *gbarray;
  GPtrArray *gparray;
  GArray *garray;
  guint i;

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));
  for (i = 0; i < 10000; i++)
    if (g_ptr_array_index (gparray, i) != GINT_TO_POINTER (i))
      g_error ("array fails: %p ( %p )\n", g_ptr_array_index (gparray, i), GINT_TO_POINTER (i));
  g_ptr_array_free (gparray, TRUE);

  gbarray = g_byte_array_new ();
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);
  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }
  g_byte_array_free (gbarray, TRUE);

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, i);
  for (i = 0; i < 10000; i++)
    if (g_array_index (garray, gint, i) != i)
      g_error ("failure: %d ( %d )\n", g_array_index (garray, gint, i), i);
  g_array_free (garray, TRUE);

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);
  for (i = 0; i < 100; i++)
    if (g_array_index (garray, gint, i) != (100 - i - 1))
      g_error ("failure: %d ( %d )\n", g_array_index (garray, gint, i), 100 - i - 1);
  g_array_free (garray, TRUE);
}

static void
hash_table_tests (void)
{
  GHashTable *hash_table;
  int array[10000];
  gint *pvalue = NULL;
  gint value = 120;
  guint i;

  hash_table = g_hash_table_new (my_hash, my_hash_equal);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  pvalue = g_hash_table_find (hash_table, find_first_that, &value);
  if (*pvalue != value)
    g_error ("g_hash_table_find failed");
  g_hash_table_foreach (hash_table, my_hash_callback, NULL);
  for (i = 0; i < 10000; i++)
    if (array[i] == 0)
      g_error ("hashtable-test: wrong value: %d\n", i);
  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_error ("hashtable removal failed\n");
  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);
  g_hash_table_destroy (hash_table);
}

#ifndef G_DISABLE_DEPRECATED
static void
relation_test (void)
{
  GRelation *relation = g_relation_new (2);
  GTuples *tuples;
  gint data [1024];
  guint i;

  g_relation_index (relation, 0, g_int_hash, g_int_equal);
  g_relation_index (relation, 1, g_int_hash, g_int_equal);

  for (i = 0; i < 1024; i += 1)
    data[i] = i;

  for (i = 1; i < 1023; i += 1)
    {
      g_relation_insert (relation, data + i, data + i + 1);
      g_relation_insert (relation, data + i, data + i - 1);
    }

  for (i = 2; i < 1022; i += 1)
    {
      g_assert (! g_relation_exists (relation, data + i, data + i));
      g_assert (! g_relation_exists (relation, data + i, data + i + 2));
      g_assert (! g_relation_exists (relation, data + i, data + i - 2));
    }

  for (i = 1; i < 1023; i += 1)
    {
      g_assert (g_relation_exists (relation, data + i, data + i + 1));
      g_assert (g_relation_exists (relation, data + i, data + i - 1));
    }

  for (i = 2; i < 1022; i += 1)
    {
      g_assert (g_relation_count (relation, data + i, 0) == 2);
      g_assert (g_relation_count (relation, data + i, 1) == 2);
    }

  g_assert (g_relation_count (relation, data, 0) == 0);

  g_assert (g_relation_count (relation, data + 42, 0) == 2);
  g_assert (g_relation_count (relation, data + 43, 1) == 2);
  g_assert (g_relation_count (relation, data + 41, 1) == 2);
  g_relation_delete (relation, data + 42, 0);
  g_assert (g_relation_count (relation, data + 42, 0) == 0);
  g_assert (g_relation_count (relation, data + 43, 1) == 1);
  g_assert (g_relation_count (relation, data + 41, 1) == 1);

  tuples = g_relation_select (relation, data + 200, 0);

  g_assert (tuples->len == 2);

#if 0
  for (i = 0; i < tuples->len; i += 1)
    {
      printf ("%d %d\n",
	      *(gint*) g_tuples_index (tuples, i, 0),
	      *(gint*) g_tuples_index (tuples, i, 1));
    }
#endif

  g_assert (g_relation_exists (relation, data + 300, data + 301));
  g_relation_delete (relation, data + 300, 0);
  g_assert (!g_relation_exists (relation, data + 300, data + 301));

  g_tuples_destroy (tuples);

  g_relation_destroy (relation);

  relation = NULL;
}
#endif

static void
gstring_tests (void)
{
  GString *string1, *string2;
  guint i;

  if (g_test_verbose())
    g_printerr ("test GString basics\n");

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new ("");

  g_assert (strcmp ("hi pete!", string1->str) == 0);

  for (i = 0; i < 10000; i++)
    g_string_append_c (string1, 'a'+(i%26));

#ifndef G_OS_WIN32
  /* MSVC, mingw32 and LCC use the same run-time C library, which doesn't like
     the %10000.10000f format... */
  g_string_printf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%10000.10000f",
		   "this pete guy sure is a wuss, like he's the number ",
		   1,
		   " wuss.  everyone agrees.\n",
		   string1->str,
		   10, 666, 15, 15, 666.666666666, 666.666666666);
#else
  g_string_printf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%100.100f",
		   "this pete guy sure is a wuss, like he's the number ",
		   1,
		   " wuss.  everyone agrees.\n",
		   string1->str,
		   10, 666, 15, 15, 666.666666666, 666.666666666);
#endif

  if (g_test_verbose())
    g_printerr ("string2 length = %lu...\n", (gulong)string2->len);
  string2->str[70] = '\0';
  if (g_test_verbose())
    g_printerr ("first 70 chars:\n%s\n", string2->str);
  string2->str[141] = '\0';
  if (g_test_verbose())
    g_printerr ("next 70 chars:\n%s\n", string2->str+71);
  string2->str[212] = '\0';
  if (g_test_verbose())
    g_printerr ("and next 70:\n%s\n", string2->str+142);
  if (g_test_verbose())
    g_printerr ("last 70 chars:\n%s\n", string2->str+string2->len - 70);

  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);

  /* append */
  string1 = g_string_new ("firsthalf");
  g_string_append (string1, "lasthalf");
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);

  /* append_len */
  string1 = g_string_new ("firsthalf");
  g_string_append_len (string1, "lasthalfjunkjunk", strlen ("lasthalf"));
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);

  /* prepend */
  string1 = g_string_new ("lasthalf");
  g_string_prepend (string1, "firsthalf");
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);

  /* prepend_len */
  string1 = g_string_new ("lasthalf");
  g_string_prepend_len (string1, "firsthalfjunkjunk", strlen ("firsthalf"));
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);

  /* insert */
  string1 = g_string_new ("firstlast");
  g_string_insert (string1, 5, "middle");
  g_assert (strcmp (string1->str, "firstmiddlelast") == 0);
  g_string_free (string1, TRUE);

  /* insert with pos == end of the string */
  string1 = g_string_new ("firstmiddle");
  g_string_insert (string1, strlen ("firstmiddle"), "last");
  g_assert (strcmp (string1->str, "firstmiddlelast") == 0);
  g_string_free (string1, TRUE);

  /* insert_len */
  string1 = g_string_new ("firstlast");
  g_string_insert_len (string1, 5, "middlejunkjunk", strlen ("middle"));
  g_assert (strcmp (string1->str, "firstmiddlelast") == 0);
  g_string_free (string1, TRUE);

  /* insert_len with magic -1 pos for append */
  string1 = g_string_new ("first");
  g_string_insert_len (string1, -1, "lastjunkjunk", strlen ("last"));
  g_assert (strcmp (string1->str, "firstlast") == 0);
  g_string_free (string1, TRUE);

  /* insert_len with magic -1 len for strlen-the-string */
  string1 = g_string_new ("first");
  g_string_insert_len (string1, 5, "last", -1);
  g_assert (strcmp (string1->str, "firstlast") == 0);
  g_string_free (string1, TRUE);

  /* g_string_equal */
  string1 = g_string_new ("test");
  string2 = g_string_new ("te");
  g_assert (! g_string_equal(string1, string2));
  g_string_append (string2, "st");
  g_assert (g_string_equal(string1, string2));
  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);

  /* Check handling of embedded ASCII 0 (NUL) characters in GString. */
  if (g_test_verbose())
    g_printerr ("test embedded ASCII 0 (NUL) characters in GString\n");
  string1 = g_string_new ("fiddle");
  string2 = g_string_new ("fiddle");
  g_assert (g_string_equal(string1, string2));
  g_string_append_c(string1, '\0');
  g_assert (! g_string_equal(string1, string2));
  g_string_append_c(string2, '\0');
  g_assert (g_string_equal(string1, string2));
  g_string_append_c(string1, 'x');
  g_string_append_c(string2, 'y');
  g_assert (! g_string_equal(string1, string2));
  g_assert (string1->len == 8);
  g_string_append(string1, "yzzy");
  g_assert (string1->len == 12);
  g_assert ( memcmp(string1->str, "fiddle\0xyzzy", 13) == 0);
  g_string_insert(string1, 1, "QED");
  g_assert ( memcmp(string1->str, "fQEDiddle\0xyzzy", 16) == 0);
  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);
}

static void
various_string_tests (void)
{
  GStringChunk *string_chunk;
  GTimeVal ref_date, date;
  gchar *tmp_string = NULL, *tmp_string_2, *string, *date_str;
  guint i;
  const gchar *tz;

  if (g_test_verbose())
    g_printerr ("checking string chunks...");
  string_chunk = g_string_chunk_new (1024);
  for (i = 0; i < 100000; i ++)
    {
      tmp_string = g_string_chunk_insert (string_chunk, "hi pete");
      if (strcmp ("hi pete", tmp_string) != 0)
	g_error ("string chunks are broken.\n");
    }
  tmp_string_2 = g_string_chunk_insert_const (string_chunk, tmp_string);
  g_assert (tmp_string_2 != tmp_string && strcmp (tmp_string_2, tmp_string) == 0);
  tmp_string = g_string_chunk_insert_const (string_chunk, tmp_string);
  g_assert (tmp_string_2 == tmp_string);
  g_string_chunk_free (string_chunk);

  if (g_test_verbose())
    g_printerr ("test positional printf formats (not supported):");
  string = g_strdup_printf ("%.*s%s", 5, "a", "b");
  tmp_string = g_strdup_printf ("%2$*1$s", 5, "c");
  if (g_test_verbose())
    g_printerr ("%s%s\n", string, tmp_string);
  g_free (tmp_string);
  g_free (string);

#define REF_INVALID1      "Wed Dec 19 17:20:20 GMT 2007"
#define REF_INVALID2      "1980-02-22T10:36:00Zulu"
#define REF_INVALID3      "1980-02-22T"
#define REF_SEC_UTC       320063760
#define REF_STR_UTC       "1980-02-22T10:36:00Z"
#define REF_STR_LOCAL     "1980-02-22T13:36:00"
#define REF_STR_CEST      "1980-02-22T12:36:00+02:00"
#define REF_STR_EST       "19800222T053600-0500"
#define REF_STR_NST       "1980-02-22T07:06:00-03:30"
#define REF_USEC_UTC      50000
#define REF_STR_USEC_UTC  "1980-02-22T10:36:00.050000Z"
#define REF_STR_USEC_CEST "19800222T123600.050000000+0200"
#define REF_STR_USEC_EST  "1980-02-22T05:36:00,05-05:00"
#define REF_STR_USEC_NST  "19800222T070600,0500-0330"
#define REF_STR_DATE_ONLY "1980-02-22"

  if (g_test_verbose())
    g_printerr ("checking g_time_val_from_iso8601...\n");
  ref_date.tv_sec = REF_SEC_UTC;
  ref_date.tv_usec = 0;
  g_assert (g_time_val_from_iso8601 (REF_INVALID1, &date) == FALSE);
  g_assert (g_time_val_from_iso8601 (REF_INVALID2, &date) == FALSE);
  g_assert (g_time_val_from_iso8601 (REF_INVALID3, &date) == FALSE);
  g_assert (g_time_val_from_iso8601 (REF_STR_DATE_ONLY, &date) == FALSE);
  g_assert (g_time_val_from_iso8601 (REF_STR_UTC, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> UTC stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  /* predefine time zone */
  tz = g_getenv("TZ");
  g_setenv("TZ", "UTC-03:00", 1);
  tzset();

  g_assert (g_time_val_from_iso8601 (REF_STR_LOCAL, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> LOCAL stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  /* revert back user defined time zone */
  if (tz)
    g_setenv("TZ", tz, TRUE);
  else
    g_unsetenv("TZ");
  tzset();

  g_assert (g_time_val_from_iso8601 (REF_STR_CEST, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> CEST stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  g_assert (g_time_val_from_iso8601 (REF_STR_EST, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> EST stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  g_assert (g_time_val_from_iso8601 (REF_STR_NST, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> NST stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  ref_date.tv_usec = REF_USEC_UTC;
  g_assert (g_time_val_from_iso8601 (REF_STR_USEC_UTC, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> UTC stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  g_assert (g_time_val_from_iso8601 (REF_STR_USEC_CEST, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> CEST stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  g_assert (g_time_val_from_iso8601 (REF_STR_USEC_EST, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> EST stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  g_assert (g_time_val_from_iso8601 (REF_STR_USEC_NST, &date) != FALSE);
  if (g_test_verbose())
    g_printerr ("\t=> NST stamp = %ld.%06ld (should be: %ld.%06ld) (%ld.%06ld off)\n",
             date.tv_sec, date.tv_usec, ref_date.tv_sec, ref_date.tv_usec,
             date.tv_sec - ref_date.tv_sec, date.tv_usec - ref_date.tv_usec);
  g_assert (date.tv_sec == ref_date.tv_sec && date.tv_usec == ref_date.tv_usec);

  if (g_test_verbose())
    g_printerr ("checking g_time_val_to_iso8601...\n");
  ref_date.tv_sec = REF_SEC_UTC;
  ref_date.tv_usec = 0;
  date_str = g_time_val_to_iso8601 (&ref_date);
  g_assert (date_str != NULL);
  if (g_test_verbose())
    g_printerr ("\t=> date string = %s (should be: %s)\n", date_str, REF_STR_UTC);
  g_assert (strcmp (date_str, REF_STR_UTC) == 0);
  g_free (date_str);

  ref_date.tv_usec = REF_USEC_UTC;
  date_str = g_time_val_to_iso8601 (&ref_date);
  g_assert (date_str != NULL);
  if (g_test_verbose())
    g_printerr ("\t=> date string = %s (should be: %s)\n", date_str, REF_STR_USEC_UTC);
  g_assert (strcmp (date_str, REF_STR_USEC_UTC) == 0);
  g_free (date_str);

  if (g_test_verbose())
    g_printerr ("checking g_ascii_strcasecmp...");
  g_assert (g_ascii_strcasecmp ("FroboZZ", "frobozz") == 0);
  g_assert (g_ascii_strcasecmp ("frobozz", "frobozz") == 0);
  g_assert (g_ascii_strcasecmp ("frobozz", "FROBOZZ") == 0);
  g_assert (g_ascii_strcasecmp ("FROBOZZ", "froboz") > 0);
  g_assert (g_ascii_strcasecmp ("", "") == 0);
  g_assert (g_ascii_strcasecmp ("!#%&/()", "!#%&/()") == 0);
  g_assert (g_ascii_strcasecmp ("a", "b") < 0);
  g_assert (g_ascii_strcasecmp ("a", "B") < 0);
  g_assert (g_ascii_strcasecmp ("A", "b") < 0);
  g_assert (g_ascii_strcasecmp ("A", "B") < 0);
  g_assert (g_ascii_strcasecmp ("b", "a") > 0);
  g_assert (g_ascii_strcasecmp ("b", "A") > 0);
  g_assert (g_ascii_strcasecmp ("B", "a") > 0);
  g_assert (g_ascii_strcasecmp ("B", "A") > 0);

  if (g_test_verbose())
    g_printerr ("checking g_strdup...\n");
  g_assert (g_strdup (NULL) == NULL);
  string = g_strdup (GLIB_TEST_STRING);
  g_assert (string != NULL);
  g_assert (strcmp(string, GLIB_TEST_STRING) == 0);
  g_free (string);

  if (g_test_verbose())
    g_printerr ("checking g_strconcat...\n");
  string = g_strconcat (GLIB_TEST_STRING, NULL);
  g_assert (string != NULL);
  g_assert (strcmp (string, GLIB_TEST_STRING) == 0);
  g_free (string);
  string = g_strconcat (GLIB_TEST_STRING, GLIB_TEST_STRING, 
                        GLIB_TEST_STRING, NULL);
  g_assert (string != NULL);
  g_assert (strcmp (string, GLIB_TEST_STRING GLIB_TEST_STRING
                    GLIB_TEST_STRING) == 0);
  g_free (string);

  if (g_test_verbose())
    g_printerr ("checking g_strlcpy/g_strlcat...");
  /* The following is a torture test for strlcpy/strlcat, with lots of
   * checking; normal users wouldn't use them this way!
   */
  string = g_malloc (6);
  *(string + 5) = 'Z'; /* guard value, shouldn't change during test */
  *string = 'q';
  g_assert (g_strlcpy(string, "" , 5) == 0);
  g_assert ( *string == '\0' );
  *string = 'q';
  g_assert (g_strlcpy(string, "abc" , 5) == 3);
  g_assert ( *(string + 3) == '\0' );
  g_assert (g_str_equal(string, "abc"));
  g_assert (g_strlcpy(string, "abcd" , 5) == 4);
  g_assert ( *(string + 4) == '\0' );
  g_assert ( *(string + 5) == 'Z' );
  g_assert (g_str_equal(string, "abcd"));
  g_assert (g_strlcpy(string, "abcde" , 5) == 5);
  g_assert ( *(string + 4) == '\0' );
  g_assert ( *(string + 5) == 'Z' );
  g_assert (g_str_equal(string, "abcd"));
  g_assert (g_strlcpy(string, "abcdef" , 5) == 6);
  g_assert ( *(string + 4) == '\0' );
  g_assert ( *(string + 5) == 'Z' );
  g_assert (g_str_equal(string, "abcd"));
  *string = 'Y';
  *(string + 1)= '\0';
  g_assert (g_strlcpy(string, "Hello" , 0) == 5);
  g_assert (*string == 'Y');
  *string = '\0';
  g_assert (g_strlcat(string, "123" , 5) == 3);
  g_assert ( *(string + 3) == '\0' );
  g_assert (g_str_equal(string, "123"));
  g_assert (g_strlcat(string, "" , 5) == 3);
  g_assert ( *(string + 3) == '\0' );
  g_assert (g_str_equal(string, "123"));
  g_assert (g_strlcat(string, "4", 5) == 4);
  g_assert (g_str_equal(string, "1234"));
  g_assert (g_strlcat(string, "5", 5) == 5);
  g_assert ( *(string + 4) == '\0' );
  g_assert (g_str_equal(string, "1234"));
  g_assert ( *(string + 5) == 'Z' );
  *string = 'Y';
  *(string + 1)= '\0';
  g_assert (g_strlcat(string, "123" , 0) == 3);
  g_assert (*string == 'Y');

  /* A few more tests, demonstrating more "normal" use  */
  g_assert (g_strlcpy(string, "hi", 5) == 2);
  g_assert (g_str_equal(string, "hi"));
  g_assert (g_strlcat(string, "t", 5) == 3);
  g_assert (g_str_equal(string, "hit"));
  g_free(string);

  if (g_test_verbose())
    g_printerr ("checking g_strdup_printf...\n");
  string = g_strdup_printf ("%05d %-5s", 21, "test");
  g_assert (string != NULL);
  g_assert (strcmp(string, "00021 test ") == 0);
  g_free (string);

  /* g_debug (argv[0]); */
}

#ifndef G_DISABLE_DEPRECATED
static void
test_mem_chunks (void)
{
  GMemChunk *mem_chunk = g_mem_chunk_new ("test mem chunk", 50, 100, G_ALLOC_AND_FREE);
  gchar *mem[10000];
  guint i;
  for (i = 0; i < 10000; i++)
    {
      guint j;
      mem[i] = g_chunk_new (gchar, mem_chunk);
      for (j = 0; j < 50; j++)
	mem[i][j] = i * j;
    }
  for (i = 0; i < 10000; i++)
    g_mem_chunk_free (mem_chunk, mem[i]);

  g_mem_chunk_destroy (mem_chunk);
}
#endif

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/testglib/Infos", test_info);
  g_test_add_func ("/testglib/Types Sizes", type_sizes);
  g_test_add_func ("/testglib/GStrings", gstring_tests);
  g_test_add_func ("/testglib/Various Strings", various_string_tests);
  g_test_add_func ("/testglib/GList", glist_test);
  g_test_add_func ("/testglib/GSList", gslist_test);
  g_test_add_func ("/testglib/GNode", gnode_test);
  g_test_add_func ("/testglib/GTree", binary_tree_test);
  g_test_add_func ("/testglib/Arrays", test_arrays);
  g_test_add_func ("/testglib/GHashTable", hash_table_tests);
#ifndef G_DISABLE_DEPRECATED
  g_test_add_func ("/testglib/Relation (deprecated)", relation_test);
#endif
  g_test_add_func ("/testglib/File Paths", test_paths);
  g_test_add_func ("/testglib/File Functions", test_file_functions);
  g_test_add_func ("/testglib/Parse Debug Strings", test_g_parse_debug_string);
#ifndef G_DISABLE_DEPRECATED
  g_test_add_func ("/testglib/GMemChunk (deprecated)", test_mem_chunks);
#endif
  g_test_add_func ("/testglib/Warnings & Errors", log_warning_error_tests);
  g_test_add_func ("/testglib/Timers (slow)", timer_tests);

  return g_test_run();
}
