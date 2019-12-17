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

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "glib.h"

#define C2P(c)          ((gpointer) ((long) (c)))
#define P2C(p)          ((gchar) ((long) (p)))

typedef struct {
  GString *s;
  gint count;
} CallbackData;

static gboolean
node_build_string (GNode    *node,
                   gpointer  data)
{
  CallbackData *d = data;

  g_string_append_c (d->s, P2C (node->data));

  d->count--;

  if (d->count == 0)
    return TRUE;

  return FALSE;
}

typedef struct {
    GTraverseType   traverse;
    GTraverseFlags  flags;
    gint            depth;
    gint            limit;
    const gchar    *expected;
} TraverseData;

static void
traversal_test (void)
{
  GNode *root;
  GNode *node_B;
  GNode *node_C;
  GNode *node_D;
  GNode *node_E;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  GNode *n;
  TraverseData orders[] = {
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1, -1, "ABCDEFGHIJK" },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        1, -1, "A"           },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        2, -1, "ABF"         },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3, -1, "ABCDEFG"     },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3, -1, "ABCDEFG"     },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1, -1, "CDEBHIJKGFA" },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        1, -1, "A"           },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        2, -1, "BFA"         },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3, -1, "CDEBGFA"     },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1, -1, "CBDEAHGIJKF" },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        1, -1, "A"           },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        2, -1, "BAF"         },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3, -1, "CBDEAGF"     },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1, -1, "ABFCDEGHIJK" },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        1, -1, "A"           },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        2, -1, "ABF"         },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3, -1, "ABFCDEG"     },
    { G_LEVEL_ORDER, G_TRAVERSE_LEAFS,     -1, -1, "CDEHIJK"     },
    { G_LEVEL_ORDER, G_TRAVERSE_NON_LEAFS, -1, -1, "ABFG"        },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  1, "A"           },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  2, "AB"          },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  3, "ABC"         },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  4, "ABCD"        },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  5, "ABCDE"       },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  6, "ABCDEF"      },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  7, "ABCDEFG"     },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  8, "ABCDEFGH"    },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1,  9, "ABCDEFGHI"   },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,       -1, 10, "ABCDEFGHIJ"  },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  1, "A"           },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  2, "AB"          },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  3, "ABC"         },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  4, "ABCD"        },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  5, "ABCDE"       },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  6, "ABCDEF"      },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  7, "ABCDEFG"     },
    { G_PRE_ORDER,   G_TRAVERSE_ALL,        3,  8, "ABCDEFG"     },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  1, "C"           },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  2, "CD"          },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  3, "CDE"         },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  4, "CDEB"        },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  5, "CDEBH"       },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  6, "CDEBHI"      },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  7, "CDEBHIJ"     },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  8, "CDEBHIJK"    },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1,  9, "CDEBHIJKG"   },
    { G_POST_ORDER,  G_TRAVERSE_ALL,       -1, 10, "CDEBHIJKGF"  },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  1, "C"           },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  2, "CD"          },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  3, "CDE"         },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  4, "CDEB"        },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  5, "CDEBG"       },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  6, "CDEBGF"      },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  7, "CDEBGFA"     },
    { G_POST_ORDER,  G_TRAVERSE_ALL,        3,  8, "CDEBGFA"     },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  1, "C"           },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  2, "CB"          },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  3, "CBD"         },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  4, "CBDE"        },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  5, "CBDEA"       },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  6, "CBDEAH"      },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  7, "CBDEAHG"     },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  8, "CBDEAHGI"    },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1,  9, "CBDEAHGIJ"   },
    { G_IN_ORDER,    G_TRAVERSE_ALL,       -1, 10, "CBDEAHGIJK"  },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  1, "C"           },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  2, "CB"          },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  3, "CBD"         },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  4, "CBDE"        },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  5, "CBDEA"       },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  6, "CBDEAG"      },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  7, "CBDEAGF"     },
    { G_IN_ORDER,    G_TRAVERSE_ALL,        3,  8, "CBDEAGF"     },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  1, "A"           },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  2, "AB"          },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  3, "ABF"         },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  4, "ABFC"        },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  5, "ABFCD"       },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  6, "ABFCDE"      },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  7, "ABFCDEG"     },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  8, "ABFCDEGH"    },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1,  9, "ABFCDEGHI"   },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,       -1, 10, "ABFCDEGHIJ"  },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  1, "A"           },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  2, "AB"          },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  3, "ABF"         },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  4, "ABFC"        },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  5, "ABFCD"       },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  6, "ABFCDE"      },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  7, "ABFCDEG"     },
    { G_LEVEL_ORDER, G_TRAVERSE_ALL,        3,  8, "ABFCDEG"     },
  };
  gint i;
  CallbackData data;

  root = g_node_new (C2P ('A'));
  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  node_D = g_node_new (C2P ('D'));
  g_node_insert (node_B, 1, node_D);
  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_prepend (node_G, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  g_node_insert_data (node_G, 0, C2P ('H'));
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

  /* we have built:                    A
   *                                 /   \
   *                               B       F
   *                             / | \       \
   *                           C   D   E       G
   *                                         / /\ \
   *                                        H I J  K
   *
   * for in-order traversal, 'G' is considered to be the "left"
   * child of 'F', which will cause 'F' to be the last node visited.
   */

  node_C = node_B->children;
  node_E = node_D->next;

  n = g_node_last_sibling (node_C);
  g_assert (n == node_E);
  n = g_node_last_sibling (node_D);
  g_assert (n == node_E);
  n = g_node_last_sibling (node_E);
  g_assert (n == node_E);

  data.s = g_string_new ("");  
  for (i = 0; i < G_N_ELEMENTS (orders); i++)
    {
      g_string_set_size (data.s, 0);
      data.count = orders[i].limit;
      g_node_traverse (root, orders[i].traverse, orders[i].flags, orders[i].depth, node_build_string, &data);
      g_assert_cmpstr (data.s->str, ==,  orders[i].expected);
    }

  g_node_reverse_children (node_B);
  g_node_reverse_children (node_G);

  g_string_set_size (data.s, 0);
  data.count = -1;
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &data);
  g_assert_cmpstr (data.s->str, ==, "ABFEDCGKJIH");
  
  g_node_append (node_D, g_node_new (C2P ('L')));
  g_node_insert (node_D, -1, g_node_new (C2P ('M')));

  g_string_set_size (data.s, 0);
  data.count = -1;
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &data);
  g_assert_cmpstr (data.s->str, ==, "ABFEDCGLMKJIH");

  g_node_destroy (root);
  g_string_free (data.s, TRUE);
}

static void
construct_test (void)
{
  GNode *root;
  GNode *node;
  GNode *node_B;
  GNode *node_D;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  GNode *node_H;
  guint i;

  root = g_node_new (C2P ('A'));
  g_assert_cmpint (g_node_depth (root), ==, 1);
  g_assert_cmpint (g_node_max_height (root), ==, 1);

  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  g_assert (root->children == node_B);

  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  node_D = g_node_new (C2P ('D'));
  g_node_insert (node_B, 1, node_D);

  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  g_assert (root->children->next == node_F);

  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_insert_after (node_G, NULL, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  node_H = g_node_new (C2P ('H'));
  g_node_insert_after (node_G, NULL, node_H);
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

  /* we have built:                    A
   *                                 /   \
   *                               B       F
   *                             / | \       \
   *                           C   D   E       G
   *                                         / /\ \
   *                                       H  I  J  K
   */
  g_assert_cmpint (g_node_depth (root), ==, 1);
  g_assert_cmpint (g_node_max_height (root), ==, 4);
  g_assert_cmpint (g_node_depth (node_G->children->next), ==, 4);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_LEAFS), ==, 7);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_NON_LEAFS), ==, 4);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_ALL), ==, 11);
  g_assert_cmpint (g_node_max_height (node_F), ==, 3);
  g_assert_cmpint (g_node_n_children (node_G), ==, 4);
  g_assert (g_node_find_child (root, G_TRAVERSE_ALL, C2P ('F')) == node_F);
  g_assert (g_node_find_child (node_G, G_TRAVERSE_LEAFS, C2P ('H')) == node_H);
  g_assert (g_node_find_child (root, G_TRAVERSE_ALL, C2P ('H')) == NULL);
  g_assert (g_node_find (root, G_LEVEL_ORDER, G_TRAVERSE_NON_LEAFS, C2P ('I')) == NULL);
  g_assert (g_node_find (root, G_IN_ORDER, G_TRAVERSE_LEAFS, C2P ('J')) == node_J);

  for (i = 0; i < g_node_n_children (node_B); i++)
    {
      node = g_node_nth_child (node_B, i);
      g_assert_cmpint (P2C (node->data), ==, ('C' + i));
    }

  for (i = 0; i < g_node_n_children (node_G); i++)
    g_assert_cmpint (g_node_child_position (node_G, g_node_nth_child (node_G, i)), ==, i);

  g_node_destroy (root);
}

static void
allocation_test (void)
{
  GNode *root;
  GNode *node;
  gint i;

  root = g_node_new (NULL);
  node = root;

  for (i = 0; i < 2048; i++)
    {
      g_node_append (node, g_node_new (NULL));
      if ((i % 5) == 4)
        node = node->children->next;
    }
  g_assert_cmpint (g_node_max_height (root), >, 100);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_ALL), ==, 1 + 2048);

  g_node_destroy (root);
}


static void
misc_test (void)
{
  GNode *root;
  GNode *node_B;
  GNode *node_C;
  GNode *node_D;
  GNode *node_E;
  CallbackData data;

  root = g_node_new (C2P ('A'));
  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  node_D = g_node_new (C2P ('D'));
  g_node_append (root, node_D);
  node_C = g_node_new (C2P ('C'));
  g_node_insert_after (root, node_B, node_C);
  node_E = g_node_new (C2P ('E'));
  g_node_append (node_C, node_E);

  g_assert (g_node_get_root (node_E) == root);
  g_assert (g_node_is_ancestor (root, node_B));
  g_assert (g_node_is_ancestor (root, node_E));
  g_assert (!g_node_is_ancestor (node_B, node_D));
  g_assert (g_node_first_sibling (node_D) == node_B);
  g_assert (g_node_first_sibling (node_E) == node_E);
  g_assert (g_node_first_sibling (root) == root);
  g_assert_cmpint (g_node_child_index (root, C2P ('B')), ==, 0);
  g_assert_cmpint (g_node_child_index (root, C2P ('C')), ==, 1);
  g_assert_cmpint (g_node_child_index (root, C2P ('D')), ==, 2);
  g_assert_cmpint (g_node_child_index (root, C2P ('E')), ==, -1);

  data.s = g_string_new ("");
  data.count = -1;
  g_node_children_foreach (root, G_TRAVERSE_ALL, (GNodeForeachFunc)node_build_string, &data);
  g_assert_cmpstr (data.s->str, ==, "BCD");

  g_string_set_size (data.s, 0);
  data.count = -1;
  g_node_children_foreach (root, G_TRAVERSE_LEAVES, (GNodeForeachFunc)node_build_string, &data);
  g_assert_cmpstr (data.s->str, ==, "BD");

  g_string_set_size (data.s, 0);
  data.count = -1;
  g_node_children_foreach (root, G_TRAVERSE_NON_LEAVES, (GNodeForeachFunc)node_build_string, &data);
  g_assert_cmpstr (data.s->str, ==, "C");
  g_string_free (data.s, TRUE);

  g_node_destroy (root);
}

static gboolean
check_order (GNode    *node,
             gpointer  data)
{
  gchar **expected = data;
  gchar d;

  d = GPOINTER_TO_INT (node->data);
  g_assert_cmpint (d, ==, **expected);
  (*expected)++;

  return FALSE;
}

static void
unlink_test (void)
{
  GNode *root;
  GNode *node;
  GNode *bnode;
  GNode *cnode;
  gchar *expected;

  /*
   *        -------- a --------
   *       /         |          \
   *     b           c           d
   *   / | \       / | \       / | \
   * e   f   g   h   i   j   k   l   m
   *
   */

  root = g_node_new (C2P ('a'));
  node = bnode = g_node_append_data (root, C2P ('b'));
  g_node_append_data (node, C2P ('e'));
  g_node_append_data (node, C2P ('f'));
  g_node_append_data (node, C2P ('g'));

  node = cnode = g_node_append_data (root, C2P ('c'));
  g_node_append_data (node, C2P ('h'));
  g_node_append_data (node, C2P ('i'));
  g_node_append_data (node, C2P ('j'));

  node = g_node_append_data (root, C2P ('d'));
  g_node_append_data (node, C2P ('k'));
  g_node_append_data (node, C2P ('l'));
  g_node_append_data (node, C2P ('m'));

  g_node_unlink (cnode);

  expected = "abdefgklm";
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, check_order, &expected);

  expected = "abd";
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, 1, check_order, &expected);

  expected = "chij";
  g_node_traverse (cnode, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, check_order, &expected);

  g_node_destroy (bnode);

  expected = "adklm";
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, check_order, &expected);

  g_node_destroy (root);
  g_node_destroy (cnode);
}

static gpointer
copy_up (gconstpointer src,
         gpointer      data)
{
  gchar l, u;

  l = GPOINTER_TO_INT (src);
  u = g_ascii_toupper (l);

  return GINT_TO_POINTER ((int)u);
}

static void
copy_test (void)
{
  GNode *root;
  GNode *copy;
  gchar *expected;

  root = g_node_new (C2P ('a'));
  g_node_append_data (root, C2P ('b'));
  g_node_append_data (root, C2P ('c'));
  g_node_append_data (root, C2P ('d'));

  expected = "abcd";
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, check_order, &expected);

  copy = g_node_copy (root);

  expected = "abcd";
  g_node_traverse (copy, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, check_order, &expected);

  g_node_destroy (copy);

  copy = g_node_copy_deep (root, copy_up, NULL);

  expected = "ABCD";
  g_node_traverse (copy, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, check_order, &expected);

  g_node_destroy (copy);

  g_node_destroy (root);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/node/allocation", allocation_test);
  g_test_add_func ("/node/construction", construct_test);
  g_test_add_func ("/node/traversal", traversal_test);
  g_test_add_func ("/node/misc", misc_test);
  g_test_add_func ("/node/unlink", unlink_test);
  g_test_add_func ("/node/copy", copy_test);

  return g_test_run ();
}

