/* module-test.c - test program for GMODULE
 * Copyright (C) 1998 Tim Janik
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

#include <gmodule.h>
#include <string.h>

gchar* global_state;

G_MODULE_EXPORT void g_clash_func (void);

G_MODULE_EXPORT void
g_clash_func (void)
{
  global_state = "global clash";
}

typedef	void (*SimpleFunc) (void);
typedef	void (*GModuleFunc) (GModule *);

static gchar **gplugin_a_state;
static gchar **gplugin_b_state;

static void
compare (const gchar *desc, const gchar *expected, const gchar *found)
{
  if (!expected && !found)
    return;
  
  if (expected && found && strcmp (expected, found) == 0)
    return;
    
  g_error ("error: %s state should have been \"%s\", but is \"%s\"",
	   desc, expected ? expected : "NULL", found ? found : "NULL");
}

static void 
test_states (const gchar *global, const gchar *gplugin_a, 
	     const gchar *gplugin_b)
{	
  compare ("global", global, global_state);
  compare ("Plugin A", gplugin_a, *gplugin_a_state);
  compare ("Plugin B", gplugin_b, *gplugin_b_state);
  
  global_state = *gplugin_a_state = *gplugin_b_state = NULL;
}
 
static SimpleFunc plugin_clash_func = NULL;

int
main (int    argc,
      char **argv)
{
  GModule *module_self, *module_a, *module_b;
  gchar *plugin_a, *plugin_b;
  SimpleFunc f_a, f_b, f_self;
  GModuleFunc gmod_f;

  g_test_init (&argc, &argv, NULL);

  if (!g_module_supported ())
    g_error ("dynamic modules not supported");

  plugin_a = g_test_build_filename (G_TEST_BUILT, "libmoduletestplugin_a", NULL);
  plugin_b = g_test_build_filename (G_TEST_BUILT, "libmoduletestplugin_b", NULL);

  /* module handles */
  
  module_self = g_module_open (NULL, G_MODULE_BIND_LAZY);
  if (!module_self)
    g_error ("error: %s", g_module_error ());

  if (!g_module_symbol (module_self, "g_module_close", (gpointer *) &f_self))
    g_error ("error: %s", g_module_error ());

  module_a = g_module_open (plugin_a, G_MODULE_BIND_LAZY);
  if (!module_a)
    g_error ("error: %s", g_module_error ());

  module_b = g_module_open (plugin_b, G_MODULE_BIND_LAZY);
  if (!module_b)
    g_error ("error: %s", g_module_error ());

  /* get plugin state vars */

  if (!g_module_symbol (module_a, "gplugin_a_state", 
			(gpointer *) &gplugin_a_state))
    g_error ("error: %s", g_module_error ());
  
  if (!g_module_symbol (module_b, "gplugin_b_state", 
			(gpointer *) &gplugin_b_state))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, "check-init");
  
  /* get plugin specific symbols and call them
   */
  if (!g_module_symbol (module_a, "gplugin_a_func", (gpointer *) &f_a))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);
 
  if (!g_module_symbol (module_b, "gplugin_b_func", (gpointer *) &f_b))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);
 
  f_a ();
  test_states (NULL, "Hello world", NULL);
  
  f_b ();
  test_states (NULL, NULL, "Hello world");
  
  /* get and call globally clashing functions
   */
 
  if (!g_module_symbol (module_self, "g_clash_func", (gpointer *) &f_self))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);

  if (!g_module_symbol (module_a, "g_clash_func", (gpointer *) &f_a))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);
 
  if (!g_module_symbol (module_b, "g_clash_func", (gpointer *) &f_b))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);
 
  f_self ();
  test_states ("global clash", NULL, NULL);
  
  f_a ();
  test_states (NULL, "global clash", NULL);

  f_b ();
  test_states (NULL, NULL, "global clash");

  /* get and call clashing plugin functions  */

  if (!g_module_symbol (module_a, "gplugin_clash_func", (gpointer *) &f_a))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);

  if (!g_module_symbol (module_b, "gplugin_clash_func", (gpointer *) &f_b))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);

  plugin_clash_func = f_a;
  plugin_clash_func ();
  test_states (NULL, "plugin clash", NULL);

  plugin_clash_func = f_b;
  plugin_clash_func ();
  test_states (NULL, NULL, "plugin clash");

  /* call gmodule function from A  */

  if (!g_module_symbol (module_a, "gplugin_a_module_func", (gpointer *) &gmod_f))
    g_error ("error: %s", g_module_error ());
  test_states (NULL, NULL, NULL);

  gmod_f (module_b);
  test_states (NULL, NULL, "BOOH");
 
  gmod_f (module_a);
  test_states (NULL, "BOOH", NULL);

  /* unload plugins  */

  if (!g_module_close (module_a))
    g_error ("error: %s", g_module_error ());

  if (!g_module_close (module_b))
    g_error ("error: %s", g_module_error ());

  g_free (plugin_a);
  g_free (plugin_b);
  g_module_close (module_self);
  return 0;
}
