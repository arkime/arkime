/* GLib testing framework examples and tests
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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

/* We want to distinguish between messages originating from libglib
 * and messages originating from this program.
 */
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "testing"

#include <glib.h>

#include <stdlib.h>
#include <string.h>

/* test assertion variants */
static void
test_assertions_bad_cmpstr (void)
{
  g_assert_cmpstr ("fzz", !=, "fzz");
  exit (0);
}

static void
test_assertions_bad_cmpint (void)
{
  g_assert_cmpint (4, !=, 4);
  exit (0);
}

static void
test_assertions_bad_cmpmem_len (void)
{
  g_assert_cmpmem ("foo", 3, "foot", 4);
  exit (0);
}

static void
test_assertions_bad_cmpmem_data (void)
{
  g_assert_cmpmem ("foo", 3, "fzz", 3);
  exit (0);
}

static void
test_assertions (void)
{
  gchar *fuu;
  g_assert_cmpint (1, >, 0);
  g_assert_cmphex (2, ==, 2);
  g_assert_cmpfloat (3.3, !=, 7);
  g_assert_cmpfloat (7, <=, 3 + 4);
  g_assert (TRUE);
  g_assert_cmpstr ("foo", !=, "faa");
  fuu = g_strdup_printf ("f%s", "uu");
  g_test_queue_free (fuu);
  g_assert_cmpstr ("foo", !=, fuu);
  g_assert_cmpstr ("fuu", ==, fuu);
  g_assert_cmpstr (NULL, <, "");
  g_assert_cmpstr (NULL, ==, NULL);
  g_assert_cmpstr ("", >, NULL);
  g_assert_cmpstr ("foo", <, "fzz");
  g_assert_cmpstr ("fzz", >, "faa");
  g_assert_cmpstr ("fzz", ==, "fzz");
  g_assert_cmpmem ("foo", 3, "foot", 3);

  g_test_trap_subprocess ("/misc/assertions/subprocess/bad_cmpstr", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*");

  g_test_trap_subprocess ("/misc/assertions/subprocess/bad_cmpint", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*");

  g_test_trap_subprocess ("/misc/assertions/subprocess/bad_cmpmem_len", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*len*");

  g_test_trap_subprocess ("/misc/assertions/subprocess/bad_cmpmem_data", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*");
  g_test_trap_assert_stderr_unmatched ("*assertion failed*len*");
}

/* test g_test_timer* API */
static void
test_timer (void)
{
  double ttime;
  g_test_timer_start();
  g_assert_cmpfloat (g_test_timer_last(), ==, 0);
  g_usleep (25 * 1000);
  ttime = g_test_timer_elapsed();
  g_assert_cmpfloat (ttime, >, 0);
  g_assert_cmpfloat (g_test_timer_last(), ==, ttime);
  g_test_minimized_result (ttime, "timer-test-time: %fsec", ttime);
  g_test_maximized_result (5, "bogus-quantity: %ddummies", 5); /* simple API test */
}

#ifdef G_OS_UNIX
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* fork out for a failing test */
static void
test_fork_fail (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_assert_not_reached();
    }
  g_test_trap_assert_failed();
  g_test_trap_assert_stderr ("*ERROR*test_fork_fail*should not be reached*");
}

/* fork out to assert stdout and stderr patterns */
static void
test_fork_patterns (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      g_print ("some stdout text: somagic17\n");
      g_printerr ("some stderr text: semagic43\n");
      exit (0);
    }
  g_test_trap_assert_passed();
  g_test_trap_assert_stdout ("*somagic17*");
  g_test_trap_assert_stderr ("*semagic43*");
}

/* fork out for a timeout test */
static void
test_fork_timeout (void)
{
  /* allow child to run for only a fraction of a second */
  if (g_test_trap_fork (0.11 * 1000000, 0))
    {
      /* loop and sleep forever */
      while (TRUE)
        g_usleep (1000 * 1000);
    }
  g_test_trap_assert_failed();
  g_assert (g_test_trap_reached_timeout());
}

G_GNUC_END_IGNORE_DEPRECATIONS
#endif /* G_OS_UNIX */

static void
test_subprocess_fail (void)
{
  if (g_test_subprocess ())
    {
      g_assert_not_reached ();
      return;
    }

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*ERROR*test_subprocess_fail*should not be reached*");
}

static void
test_subprocess_no_such_test (void)
{
  if (g_test_subprocess ())
    {
      g_test_trap_subprocess ("/trap_subprocess/this-test-does-not-exist", 0, 0);
      g_assert_not_reached ();
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*test does not exist*");
  g_test_trap_assert_stderr_unmatched ("*should not be reached*");
}

static void
test_subprocess_patterns (void)
{
  if (g_test_subprocess ())
    {
      g_print ("some stdout text: somagic17\n");
      g_printerr ("some stderr text: semagic43\n");
      exit (0);
    }
  g_test_trap_subprocess (NULL, 0,  0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*somagic17*");
  g_test_trap_assert_stderr ("*semagic43*");
}

static void
test_subprocess_timeout (void)
{
  if (g_test_subprocess ())
    {
      /* loop and sleep forever */
      while (TRUE)
        g_usleep (1000 * 1000);
      return;
    }
  /* allow child to run for only a fraction of a second */
  g_test_trap_subprocess (NULL, 0.11 * 1000000, 0);
  g_test_trap_assert_failed ();
  g_assert (g_test_trap_reached_timeout ());
}

/* run a test with fixture setup and teardown */
typedef struct {
  guint  seed;
  guint  prime;
  gchar *msg;
} Fixturetest;
static void
fixturetest_setup (Fixturetest  *fix,
                   gconstpointer test_data)
{
  g_assert (test_data == (void*) 0xc0cac01a);
  fix->seed = 18;
  fix->prime = 19;
  fix->msg = g_strdup_printf ("%d", fix->prime);
}
static void
fixturetest_test (Fixturetest  *fix,
                  gconstpointer test_data)
{
  guint prime = g_spaced_primes_closest (fix->seed);
  g_assert_cmpint (prime, ==, fix->prime);
  prime = g_ascii_strtoull (fix->msg, NULL, 0);
  g_assert_cmpint (prime, ==, fix->prime);
  g_assert (test_data == (void*) 0xc0cac01a);
}
static void
fixturetest_teardown (Fixturetest  *fix,
                      gconstpointer test_data)
{
  g_assert (test_data == (void*) 0xc0cac01a);
  g_free (fix->msg);
}

static struct {
  int bit, vint1, vint2, irange;
  long double vdouble, drange;
} shared_rand_state;

static void
test_rand1 (void)
{
  shared_rand_state.bit = g_test_rand_bit();
  shared_rand_state.vint1 = g_test_rand_int();
  shared_rand_state.vint2 = g_test_rand_int();
  g_assert_cmpint (shared_rand_state.vint1, !=, shared_rand_state.vint2);
  shared_rand_state.irange = g_test_rand_int_range (17, 35);
  g_assert_cmpint (shared_rand_state.irange, >=, 17);
  g_assert_cmpint (shared_rand_state.irange, <=, 35);
  shared_rand_state.vdouble = g_test_rand_double();
  shared_rand_state.drange = g_test_rand_double_range (-999, +17);
  g_assert_cmpfloat (shared_rand_state.drange, >=, -999);
  g_assert_cmpfloat (shared_rand_state.drange, <=, +17);
}

static void
test_rand2 (void)
{
  /* this test only works if run after test1.
   * we do this to check that random number generators
   * are reseeded upon fixture setup.
   */
  g_assert_cmpint (shared_rand_state.bit, ==, g_test_rand_bit());
  g_assert_cmpint (shared_rand_state.vint1, ==, g_test_rand_int());
  g_assert_cmpint (shared_rand_state.vint2, ==, g_test_rand_int());
  g_assert_cmpint (shared_rand_state.irange, ==, g_test_rand_int_range (17, 35));
  g_assert_cmpfloat (shared_rand_state.vdouble, ==, g_test_rand_double());
  g_assert_cmpfloat (shared_rand_state.drange, ==, g_test_rand_double_range (-999, +17));
}

static void
test_data_test (gconstpointer test_data)
{
  g_assert (test_data == (void*) 0xc0c0baba);
}

static void
test_random_conversions (void)
{
  /* very simple conversion test using random numbers */
  int vint = g_test_rand_int();
  char *err, *str = g_strdup_printf ("%d", vint);
  gint64 vint64 = g_ascii_strtoll (str, &err, 10);
  g_assert_cmphex (vint, ==, vint64);
  g_assert (!err || *err == 0);
  g_free (str);
}

static gboolean
fatal_handler (const gchar    *log_domain,
               GLogLevelFlags  log_level,
               const gchar    *message,
               gpointer        user_data)
{
  return FALSE;
}

static void
test_fatal_log_handler_critical_pass (void)
{
  g_test_log_set_fatal_handler (fatal_handler, NULL);
  g_str_has_prefix (NULL, "file://");
  g_critical ("Test passing");
  exit (0);
}

static void
test_fatal_log_handler_error_fail (void)
{
  g_error ("Test failing");
  exit (0);
}

static void
test_fatal_log_handler_critical_fail (void)
{
  g_str_has_prefix (NULL, "file://");
  g_critical ("Test passing");
  exit (0);
}

static void
test_fatal_log_handler (void)
{
  g_test_trap_subprocess ("/misc/fatal-log-handler/subprocess/critical-pass", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*CRITICAL*g_str_has_prefix*");
  g_test_trap_assert_stderr ("*CRITICAL*Test passing*");

  g_test_trap_subprocess ("/misc/fatal-log-handler/subprocess/error-fail", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*ERROR*Test failing*");

  g_test_trap_subprocess ("/misc/fatal-log-handler/subprocess/critical-fail", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*g_str_has_prefix*");
  g_test_trap_assert_stderr_unmatched ("*CRITICAL*Test passing*");
}

static void
test_expected_messages_warning (void)
{
  g_warning ("This is a %d warning", g_random_int ());
  g_return_if_reached ();
}

static void
test_expected_messages_expect_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  test_expected_messages_warning ();
}

static void
test_expected_messages_wrong_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be *");
  test_expected_messages_warning ();
}

static void
test_expected_messages_expected (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be reached");

  test_expected_messages_warning ();

  g_test_assert_expected_messages ();
  exit (0);
}

static void
test_expected_messages_null_domain (void)
{
  g_test_expect_message (NULL, G_LOG_LEVEL_WARNING, "no domain");
  g_log (NULL, G_LOG_LEVEL_WARNING, "no domain");
  g_test_assert_expected_messages ();
}

static void
test_expected_messages_expect_error (void)
{
  /* make sure we can't try to expect a g_error() */
  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL, "*G_LOG_LEVEL_ERROR*");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "this won't work");
  g_test_assert_expected_messages ();
}

static void
test_expected_messages_extra_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be reached");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "nope");

  test_expected_messages_warning ();

  /* If we don't assert, it won't notice the missing message */
  exit (0);
}

static void
test_expected_messages_unexpected_extra_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be reached");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "nope");

  test_expected_messages_warning ();

  g_test_assert_expected_messages ();
  exit (0);
}

static void
test_expected_messages (void)
{
  g_test_trap_subprocess ("/misc/expected-messages/subprocess/warning", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*This is a * warning*");
  g_test_trap_assert_stderr_unmatched ("*should not be reached*");

  g_test_trap_subprocess ("/misc/expected-messages/subprocess/expect-warning", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr_unmatched ("*This is a * warning*");
  g_test_trap_assert_stderr ("*should not be reached*");

  g_test_trap_subprocess ("/misc/expected-messages/subprocess/wrong-warning", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr_unmatched ("*should not be reached*");
  g_test_trap_assert_stderr ("*GLib-CRITICAL*Did not see expected message testing-CRITICAL*should not be *WARNING*This is a * warning*");

  g_test_trap_subprocess ("/misc/expected-messages/subprocess/expected", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");

  g_test_trap_subprocess ("/misc/expected-messages/subprocess/null-domain", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");

  g_test_trap_subprocess ("/misc/expected-messages/subprocess/extra-warning", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");

  g_test_trap_subprocess ("/misc/expected-messages/subprocess/unexpected-extra-warning", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*GLib:ERROR*Did not see expected message testing-CRITICAL*nope*");
}

static void
test_expected_messages_debug (void)
{
  g_test_expect_message ("Test", G_LOG_LEVEL_WARNING, "warning message");
  g_log ("Test", G_LOG_LEVEL_DEBUG, "should be ignored");
  g_log ("Test", G_LOG_LEVEL_WARNING, "warning message");
  g_test_assert_expected_messages ();

  g_test_expect_message ("Test", G_LOG_LEVEL_DEBUG, "debug message");
  g_log ("Test", G_LOG_LEVEL_DEBUG, "debug message");
  g_test_assert_expected_messages ();
}

static void
test_dash_p_hidden (void)
{
  if (!g_test_subprocess ())
    g_assert_not_reached ();

  g_print ("Test /misc/dash-p/subprocess/hidden ran\n");
}

static void
test_dash_p_hidden_sub (void)
{
  if (!g_test_subprocess ())
    g_assert_not_reached ();

  g_print ("Test /misc/dash-p/subprocess/hidden/sub ran\n");
}

/* The rest of the dash_p tests will get run by the toplevel test
 * process, but they shouldn't do anything there.
 */

static void
test_dash_p_child (void)
{
  if (!g_test_subprocess ())
    return;

  g_print ("Test /misc/dash-p/child ran\n");
}

static void
test_dash_p_child_sub (void)
{
  if (!g_test_subprocess ())
    return;

  g_print ("Test /misc/dash-p/child/sub ran\n");
}

static void
test_dash_p_child_sub2 (void)
{
  if (!g_test_subprocess ())
    return;

  g_print ("Test /misc/dash-p/child/sub2 ran\n");
}

static void
test_dash_p_child_sub_child (void)
{
  if (!g_test_subprocess ())
    return;

  g_print ("Test /misc/dash-p/child/subprocess ran\n");
}

static void
test_dash_p (void)
{
  g_test_trap_subprocess ("/misc/dash-p/subprocess/hidden", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*Test /misc/dash-p/subprocess/hidden ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden/sub ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden/sub2 ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden/sub/subprocess ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/child*");

  g_test_trap_subprocess ("/misc/dash-p/subprocess/hidden/sub", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*Test /misc/dash-p/subprocess/hidden/sub ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden/sub2 ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden/subprocess ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/child*");

  g_test_trap_subprocess ("/misc/dash-p/child", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*Test /misc/dash-p/child ran*");
  g_test_trap_assert_stdout ("*Test /misc/dash-p/child/sub ran*");
  g_test_trap_assert_stdout ("*Test /misc/dash-p/child/sub2 ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/child/subprocess ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden*");

  g_test_trap_subprocess ("/misc/dash-p/child/sub", 0, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*Test /misc/dash-p/child/sub ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/child ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/child/sub2 ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/child/subprocess ran*");
  g_test_trap_assert_stdout_unmatched ("*Test /misc/dash-p/subprocess/hidden*");
}

static void
test_nonfatal (void)
{
  if (g_test_subprocess ())
    {
      g_test_set_nonfatal_assertions ();
      g_assert_cmpint (4, ==, 5);
      g_print ("The End\n");
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*4 == 5*");
  g_test_trap_assert_stdout ("*The End*");
}

static void
test_skip (void)
{
  g_test_skip ("Skipped should count as passed, not failed");
}

static void
test_pass (void)
{
}

static void
test_fail (void)
{
  if (g_test_subprocess ())
    {
      g_test_fail ();
      g_assert (g_test_failed ());
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
test_incomplete (void)
{
  if (g_test_subprocess ())
    {
      g_test_incomplete ("not done");
      g_assert (g_test_failed ());
      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
test_subprocess_timed_out (void)
{
  if (g_test_subprocess ())
    {
      g_usleep (1000000);
      return;
    }
  g_test_trap_subprocess (NULL, 50000, 0);
  g_assert (g_test_trap_reached_timeout ());
}

static const char *argv0;

static void
test_skip_all (void)
{
  GPtrArray *argv;
  GError *error = NULL;
  int status;

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, (char *) argv0);
  g_ptr_array_add (argv, "--GTestSubprocess");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip");
  g_ptr_array_add (argv, NULL);

  g_spawn_sync (NULL, (char **) argv->pdata, NULL,
                G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                NULL, NULL, NULL, NULL, &status,
                &error);
  g_assert_no_error (error);

  g_spawn_check_exit_status (status, &error);
  g_assert_error (error, G_SPAWN_EXIT_ERROR, 77);
  g_clear_error (&error);

  g_ptr_array_set_size (argv, 0);
  g_ptr_array_add (argv, (char *) argv0);
  g_ptr_array_add (argv, "--GTestSubprocess");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip-all/subprocess/skip1");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip-all/subprocess/skip2");
  g_ptr_array_add (argv, NULL);

  g_spawn_sync (NULL, (char **) argv->pdata, NULL,
                G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                NULL, NULL, NULL, NULL, &status,
                &error);
  g_assert_no_error (error);

  g_spawn_check_exit_status (status, &error);
  g_assert_error (error, G_SPAWN_EXIT_ERROR, 77);
  g_clear_error (&error);

  g_ptr_array_set_size (argv, 0);
  g_ptr_array_add (argv, (char *) argv0);
  g_ptr_array_add (argv, "--GTestSubprocess");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip-all/subprocess/pass");
  g_ptr_array_add (argv, "-p");
  g_ptr_array_add (argv, "/misc/skip-all/subprocess/skip1");
  g_ptr_array_add (argv, NULL);

  g_spawn_sync (NULL, (char **) argv->pdata, NULL,
                G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                NULL, NULL, NULL, NULL, &status,
                &error);
  g_assert_no_error (error);

  g_spawn_check_exit_status (status, &error);
  g_assert_no_error (error);

  g_ptr_array_unref (argv);
}

int
main (int   argc,
      char *argv[])
{
  argv0 = argv[0];

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/random-generator/rand-1", test_rand1);
  g_test_add_func ("/random-generator/rand-2", test_rand2);
  g_test_add_func ("/random-generator/random-conversions", test_random_conversions);
  g_test_add_func ("/misc/assertions", test_assertions);
  g_test_add_func ("/misc/assertions/subprocess/bad_cmpstr", test_assertions_bad_cmpstr);
  g_test_add_func ("/misc/assertions/subprocess/bad_cmpint", test_assertions_bad_cmpint);
  g_test_add_func ("/misc/assertions/subprocess/bad_cmpmem_len", test_assertions_bad_cmpmem_len);
  g_test_add_func ("/misc/assertions/subprocess/bad_cmpmem_data", test_assertions_bad_cmpmem_data);
  g_test_add_data_func ("/misc/test-data", (void*) 0xc0c0baba, test_data_test);
  g_test_add ("/misc/primetoul", Fixturetest, (void*) 0xc0cac01a, fixturetest_setup, fixturetest_test, fixturetest_teardown);
  if (g_test_perf())
    g_test_add_func ("/misc/timer", test_timer);

#ifdef G_OS_UNIX
  g_test_add_func ("/forking/fail assertion", test_fork_fail);
  g_test_add_func ("/forking/patterns", test_fork_patterns);
  if (g_test_slow())
    g_test_add_func ("/forking/timeout", test_fork_timeout);
#endif

  g_test_add_func ("/trap_subprocess/fail", test_subprocess_fail);
  g_test_add_func ("/trap_subprocess/no-such-test", test_subprocess_no_such_test);
  if (g_test_slow ())
    g_test_add_func ("/trap_subprocess/timeout", test_subprocess_timeout);

  g_test_add_func ("/trap_subprocess/patterns", test_subprocess_patterns);

  g_test_add_func ("/misc/fatal-log-handler", test_fatal_log_handler);
  g_test_add_func ("/misc/fatal-log-handler/subprocess/critical-pass", test_fatal_log_handler_critical_pass);
  g_test_add_func ("/misc/fatal-log-handler/subprocess/error-fail", test_fatal_log_handler_error_fail);
  g_test_add_func ("/misc/fatal-log-handler/subprocess/critical-fail", test_fatal_log_handler_critical_fail);

  g_test_add_func ("/misc/expected-messages", test_expected_messages);
  g_test_add_func ("/misc/expected-messages/subprocess/warning", test_expected_messages_warning);
  g_test_add_func ("/misc/expected-messages/subprocess/expect-warning", test_expected_messages_expect_warning);
  g_test_add_func ("/misc/expected-messages/subprocess/wrong-warning", test_expected_messages_wrong_warning);
  g_test_add_func ("/misc/expected-messages/subprocess/expected", test_expected_messages_expected);
  g_test_add_func ("/misc/expected-messages/subprocess/null-domain", test_expected_messages_null_domain);
  g_test_add_func ("/misc/expected-messages/subprocess/extra-warning", test_expected_messages_extra_warning);
  g_test_add_func ("/misc/expected-messages/subprocess/unexpected-extra-warning", test_expected_messages_unexpected_extra_warning);
  g_test_add_func ("/misc/expected-messages/expect-error", test_expected_messages_expect_error);
  g_test_add_func ("/misc/expected-messages/skip-debug", test_expected_messages_debug);

  g_test_add_func ("/misc/dash-p", test_dash_p);
  g_test_add_func ("/misc/dash-p/child", test_dash_p_child);
  g_test_add_func ("/misc/dash-p/child/sub", test_dash_p_child_sub);
  g_test_add_func ("/misc/dash-p/child/sub/subprocess", test_dash_p_child_sub_child);
  g_test_add_func ("/misc/dash-p/child/sub/subprocess/child", test_dash_p_child_sub_child);
  g_test_add_func ("/misc/dash-p/child/sub2", test_dash_p_child_sub2);
  g_test_add_func ("/misc/dash-p/subprocess/hidden", test_dash_p_hidden);
  g_test_add_func ("/misc/dash-p/subprocess/hidden/sub", test_dash_p_hidden_sub);

  g_test_add_func ("/misc/nonfatal", test_nonfatal);

  g_test_add_func ("/misc/skip", test_skip);
  g_test_add_func ("/misc/skip-all", test_skip_all);
  g_test_add_func ("/misc/skip-all/subprocess/skip1", test_skip);
  g_test_add_func ("/misc/skip-all/subprocess/skip2", test_skip);
  g_test_add_func ("/misc/skip-all/subprocess/pass", test_pass);
  g_test_add_func ("/misc/fail", test_fail);
  g_test_add_func ("/misc/incomplete", test_incomplete);
  g_test_add_func ("/misc/timeout", test_subprocess_timed_out);

  return g_test_run();
}
