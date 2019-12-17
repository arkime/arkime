/* GLib testing framework examples and tests
 * Copyright (C) 2009 Red Hat, Inc.
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

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_TRIALS 250000

struct {
  const char *order;
  int expected, seen;
} ordering[] = {
  /* There are 32 legitimate orderings; the result always has to start
   * with either "fe" (usually) or "ef" (rarely). For the remaining
   * letters, "cbda" is the most likely, with various other orders
   * possible, down to "adbc" being the most improbable. However,
   * almost all "fe" orderings are more likely than almost any "ef"
   * orderings. The complete probability ordering, from most-likely
   * to least-likely is something roughly like:
   */
  { "fecbda", 0.2468 * NUM_TRIALS, 0},
  { "febcda", 0.1885 * NUM_TRIALS, 0},
  { "fecdba", 0.1346 * NUM_TRIALS, 0},
  { "fedcba", 0.0830 * NUM_TRIALS, 0},
  { "febdca", 0.0706 * NUM_TRIALS, 0},
  { "fedbca", 0.0571 * NUM_TRIALS, 0},
  { "fecbad", 0.0496 * NUM_TRIALS, 0},
  { "febcad", 0.0374 * NUM_TRIALS, 0},
  { "fecabd", 0.0185 * NUM_TRIALS, 0},
  { "fecdab", 0.0136 * NUM_TRIALS, 0},
  { "fecadb", 0.0110 * NUM_TRIALS, 0},
  { "febacd", 0.0108 * NUM_TRIALS, 0},
  { "feacbd", 0.0096 * NUM_TRIALS, 0},
  { "fedcab", 0.0083 * NUM_TRIALS, 0},
  { "feabcd", 0.0073 * NUM_TRIALS, 0},
  { "feacdb", 0.0058 * NUM_TRIALS, 0},
  { "efcbda", 0.0049 * NUM_TRIALS, 0},
  { "febdac", 0.0048 * NUM_TRIALS, 0},
  { "febadc", 0.0043 * NUM_TRIALS, 0},
  { "fedbac", 0.0038 * NUM_TRIALS, 0},
  { "efbcda", 0.0038 * NUM_TRIALS, 0},
  { "feadcb", 0.0036 * NUM_TRIALS, 0},
  { "fedacb", 0.0035 * NUM_TRIALS, 0},
  { "feabdc", 0.0029 * NUM_TRIALS, 0},
  { "feadbc", 0.0026 * NUM_TRIALS, 0},
  { "fedabc", 0.0026 * NUM_TRIALS, 0},
  { "efcdba", 0.0026 * NUM_TRIALS, 0},
  { "efdcba", 0.0017 * NUM_TRIALS, 0},
  { "efbdca", 0.0014 * NUM_TRIALS, 0},
  { "efdbca", 0.0011 * NUM_TRIALS, 0},
  { "efcbad", 0.0010 * NUM_TRIALS, 0},
  { "efbcad", 0.0008 * NUM_TRIALS, 0},
  { "efcabd", 0.0004 * NUM_TRIALS, 0},
  { "efcdab", 0.0003 * NUM_TRIALS, 0},
  { "efcadb", 0.0002 * NUM_TRIALS, 0},
  { "efbacd", 0.0002 * NUM_TRIALS, 0},
  { "efacbd", 0.0002 * NUM_TRIALS, 0},
  { "efdcab", 0.0002 * NUM_TRIALS, 0},
  { "efabcd", 0.0002 * NUM_TRIALS, 0},
  { "efacdb", 0.0001 * NUM_TRIALS, 0},
  { "efbdac", 0.0001 * NUM_TRIALS, 0},
  { "efadcb", 0.0001 * NUM_TRIALS, 0},
  { "efdbac", 0.0001 * NUM_TRIALS, 0},
  { "efbadc", 0.0001 * NUM_TRIALS, 0},
  { "efdacb", 0.0001 * NUM_TRIALS, 0},
  { "efabdc", 0.0001 * NUM_TRIALS, 0},
  { "efadbc", 0.00005 * NUM_TRIALS, 0},
  { "efdabc", 0.00005 * NUM_TRIALS, 0}
};
#define NUM_ORDERINGS G_N_ELEMENTS (ordering)

static void
test_srv_target_ordering (void)
{
  GList *targets, *sorted, *t;
  char result[7], *p;
  int i;
  guint o;

  targets = NULL;
  /*                                 name, port, priority, weight */
  targets = g_list_append (targets, g_srv_target_new ("a", 0, 2, 0));
  targets = g_list_append (targets, g_srv_target_new ("b", 0, 2, 10));
  targets = g_list_append (targets, g_srv_target_new ("c", 0, 2, 15));
  targets = g_list_append (targets, g_srv_target_new ("d", 0, 2, 5));
  targets = g_list_append (targets, g_srv_target_new ("e", 0, 1, 0));
  targets = g_list_append (targets, g_srv_target_new ("f", 0, 1, 50));

  for (i = 0; i < NUM_TRIALS; i++)
    {
      g_random_set_seed (i);

      sorted = g_srv_target_list_sort (g_list_copy (targets));

      for (t = sorted, p = result; t; t = t->next)
	*(p++) = *g_srv_target_get_hostname (t->data);
      *p = '\0';
      g_list_free (sorted);

      for (o = 0; o < NUM_ORDERINGS; o++)
	{
	  if (!strcmp (result, ordering[o].order))
	    {
	      ordering[o].seen++;
	      break;
	    }
	}

      /* Assert that @result matched one of the valid orderings */
      if (o == NUM_ORDERINGS)
	{
	  char *msg = g_strdup_printf ("result '%s' is invalid", result);
	  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, msg);
	}
    }

  /* Assert that each ordering appeared roughly the expected
   * number of times.
   */
  for (o = 0; o < NUM_ORDERINGS; o++)
    {
      g_assert_cmpint (ordering[o].seen, >, ordering[o].expected / 2);
      g_assert_cmpint (ordering[o].seen, <, ordering[o].expected * 2);
    }

  g_resolver_free_targets (targets);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/srvtarget/srv-target-ordering", test_srv_target_ordering);

  return g_test_run();
}
