/*
 * Copyright © 2013 Canonical Limited
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
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "gdbus-sessionbus.h"

static GDesktopAppInfo *appinfo;
static int current_state;
static gboolean saw_startup_id;
static gboolean requested_startup_id;


static GType test_app_launch_context_get_type (void);
typedef GAppLaunchContext TestAppLaunchContext;
typedef GAppLaunchContextClass TestAppLaunchContextClass;
G_DEFINE_TYPE (TestAppLaunchContext, test_app_launch_context, G_TYPE_APP_LAUNCH_CONTEXT)

static gchar *
test_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                               GAppInfo          *info,
                                               GList             *uris)
{
  requested_startup_id = TRUE;
  return g_strdup ("expected startup id");
}

static void
test_app_launch_context_init (TestAppLaunchContext *ctx)
{
}

static void
test_app_launch_context_class_init (GAppLaunchContextClass *class)
{
  class->get_startup_notify_id = test_app_launch_context_get_startup_notify_id;
}

static GType test_application_get_type (void);
typedef GApplication TestApplication;
typedef GApplicationClass TestApplicationClass;
G_DEFINE_TYPE (TestApplication, test_application, G_TYPE_APPLICATION)

static void
saw_action (const gchar *action)
{
  /* This is the main driver of the test.  It's a bit of a state
   * machine.
   *
   * Each time some event arrives on the app, it calls here to report
   * which event it was.  The initial activation of the app is what
   * starts everything in motion (starting from state 0).  At each
   * state, we assert that we receive the expected event, send the next
   * event, then update the current_state variable so we do the correct
   * thing next time.
   */

  switch (current_state)
    {
      case 0: g_assert_cmpstr (action, ==, "activate");

      /* Let's try another activation... */
      g_app_info_launch (G_APP_INFO (appinfo), NULL, NULL, NULL);
      current_state = 1; return; case 1: g_assert_cmpstr (action, ==, "activate");


      /* Now let's try opening some files... */
      {
        GList *files;

        files = g_list_prepend (NULL, g_file_new_for_uri ("file:///a/b"));
        files = g_list_append (files, g_file_new_for_uri ("file:///c/d"));
        g_app_info_launch (G_APP_INFO (appinfo), files, NULL, NULL);
        g_list_free_full (files, g_object_unref);
      }
      current_state = 2; return; case 2: g_assert_cmpstr (action, ==, "open");

      /* Now action activations... */
      g_desktop_app_info_launch_action (appinfo, "frob", NULL);
      current_state = 3; return; case 3: g_assert_cmpstr (action, ==, "frob");

      g_desktop_app_info_launch_action (appinfo, "tweak", NULL);
      current_state = 4; return; case 4: g_assert_cmpstr (action, ==, "tweak");

      g_desktop_app_info_launch_action (appinfo, "twiddle", NULL);
      current_state = 5; return; case 5: g_assert_cmpstr (action, ==, "twiddle");

      /* Now launch the app with startup notification */
      {
        GAppLaunchContext *ctx;

        g_assert (saw_startup_id == FALSE);
        ctx = g_object_new (test_app_launch_context_get_type (), NULL);
        g_app_info_launch (G_APP_INFO (appinfo), NULL, ctx, NULL);
        g_assert (requested_startup_id);
        requested_startup_id = FALSE;
        g_object_unref (ctx);
      }
      current_state = 6; return; case 6: g_assert_cmpstr (action, ==, "activate"); g_assert (saw_startup_id);
      saw_startup_id = FALSE;

      /* Now do the same for an action */
      {
        GAppLaunchContext *ctx;

        g_assert (saw_startup_id == FALSE);
        ctx = g_object_new (test_app_launch_context_get_type (), NULL);
        g_desktop_app_info_launch_action (appinfo, "frob", ctx);
        g_assert (requested_startup_id);
        requested_startup_id = FALSE;
        g_object_unref (ctx);
      }
      current_state = 7; return; case 7: g_assert_cmpstr (action, ==, "frob"); g_assert (saw_startup_id);
      saw_startup_id = FALSE;

      /* Now quit... */
      g_desktop_app_info_launch_action (appinfo, "quit", NULL);
      current_state = 8; return; case 8: g_assert_not_reached ();
    }
}

static void
test_application_frob (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  g_assert (parameter == NULL);
  saw_action ("frob");
}

static void
test_application_tweak (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  g_assert (parameter == NULL);
  saw_action ("tweak");
}

static void
test_application_twiddle (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  g_assert (parameter == NULL);
  saw_action ("twiddle");
}

static void
test_application_quit (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  GApplication *application = user_data;

  g_application_quit (application);
}

static const GActionEntry app_actions[] = {
  { "frob",         test_application_frob              },
  { "tweak",        test_application_tweak             },
  { "twiddle",      test_application_twiddle           },
  { "quit",         test_application_quit              }
};

static void
test_application_activate (GApplication *application)
{
  /* Unbalanced, but that's OK because we will quit() */
  g_application_hold (application);

  saw_action ("activate");
}

static void
test_application_open (GApplication  *application,
                       GFile        **files,
                       gint           n_files,
                       const gchar   *hint)
{
  GFile *f;

  g_assert_cmpstr (hint, ==, "");

  g_assert_cmpint (n_files, ==, 2);
  f = g_file_new_for_uri ("file:///a/b");
  g_assert (g_file_equal (files[0], f));
  g_object_unref (f);
  f = g_file_new_for_uri ("file:///c/d");
  g_assert (g_file_equal (files[1], f));
  g_object_unref (f);

  saw_action ("open");
}

static void
test_application_startup (GApplication *application)
{
  G_APPLICATION_CLASS (test_application_parent_class)
    ->startup (application);

  g_action_map_add_action_entries (G_ACTION_MAP (application), app_actions, G_N_ELEMENTS (app_actions), application);
}

static void
test_application_before_emit (GApplication *application,
                              GVariant     *platform_data)
{
  const gchar *startup_id;

  g_assert (!saw_startup_id);

  if (!g_variant_lookup (platform_data, "desktop-startup-id", "&s", &startup_id))
    return;

  g_assert_cmpstr (startup_id, ==, "expected startup id");
  saw_startup_id = TRUE;
}

static void
test_application_init (TestApplication *app)
{
}

static void
test_application_class_init (GApplicationClass *class)
{
  class->before_emit = test_application_before_emit;
  class->startup = test_application_startup;
  class->activate = test_application_activate;
  class->open = test_application_open;
}

static void
test_dbus_appinfo (void)
{
  const gchar *argv[] = { "myapp", NULL };
  TestApplication *app;
  int status;

  appinfo = g_desktop_app_info_new_from_filename (g_test_build_filename (G_TEST_DIST,
                                                                         "org.gtk.test.dbusappinfo.desktop",
                                                                         NULL));
  g_assert (appinfo != NULL);

  app = g_object_new (test_application_get_type (),
                      "application-id", "org.gtk.test.dbusappinfo",
                      "flags", G_APPLICATION_HANDLES_OPEN,
                      NULL);
  status = g_application_run (app, 1, (gchar **) argv);

  g_assert_cmpint (status, ==, 0);
  g_assert_cmpint (current_state, ==, 8);

  g_object_unref (appinfo);
  g_object_unref (app);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/appinfo/dbusappinfo", test_dbus_appinfo);

  return session_bus_run ();
}
