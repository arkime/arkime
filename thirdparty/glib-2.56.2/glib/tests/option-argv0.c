/* 
 * Copyright (C) 2011 Red Hat, Inc.
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
 *
 * Authors: Colin Walters <walters@verbum.org>
 */

#include <glib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined __linux || defined __OpenBSD__
static void
test_platform_argv0 (void)
{
  GOptionContext *context;
  gboolean arg;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };
  gboolean retval;

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);
  
  retval = g_option_context_parse (context, NULL, NULL, NULL);
  g_assert (retval == TRUE);
  g_assert (strcmp (g_get_prgname(), "option-argv0") == 0
	    || strcmp (g_get_prgname (), "lt-option-argv0") == 0);

  g_option_context_free (context);
}
#endif

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, "no_g_set_prgname", NULL);

#if defined __linux || defined __OpenBSD__
  g_test_add_func ("/option/argv0", test_platform_argv0);
#endif

  return g_test_run ();
}
