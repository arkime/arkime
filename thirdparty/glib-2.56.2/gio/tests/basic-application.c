#include <gio/gio.h>
#include <string.h>

static void
new_activated (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GApplication *app = user_data;

  g_application_activate (app);
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GApplication *app = user_data;

  g_application_quit (app);
}

static void
action1_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  g_print ("activate action1\n");
}

static void
action2_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GVariant *state;

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_print ("activate action2 %d\n", !g_variant_get_boolean (state));
  g_variant_unref (state);
}

static void
change_action2 (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
  g_print ("change action2 %d\n", g_variant_get_boolean (state));
}

static void
startup (GApplication *app)
{
  static GActionEntry actions[] = {
    { "new", new_activated, NULL, NULL, NULL },
    { "quit", quit_activated, NULL, NULL, NULL },
    { "action1", action1_activated, NULL, NULL, NULL },
    { "action2", action2_activated, "b", "false", change_action2 }
  };

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   actions, G_N_ELEMENTS (actions),
                                   app);
}

static void
activate (GApplication *application)
{
  g_application_hold (application);
  g_print ("activated\n");
  g_application_release (application);
}

static void
open (GApplication  *application,
      GFile        **files,
      gint           n_files,
      const gchar   *hint)
{
  gint i;

  g_application_hold (application);

  g_print ("open");
  for (i = 0; i < n_files; i++)
    {
      gchar *uri = g_file_get_uri (files[i]);
      g_print (" %s", uri);
      g_free (uri);
    }
  g_print ("\n");

  g_application_release (application);
}

static int
command_line (GApplication            *application,
              GApplicationCommandLine *cmdline)
{
  gchar **argv;
  gint argc;
  gint i;

  g_application_hold (application);
  argv = g_application_command_line_get_arguments (cmdline, &argc);

  if (argc > 1)
    {
      if (g_strcmp0 (argv[1], "echo") == 0)
        {
          g_print ("cmdline");
          for (i = 0; i < argc; i++)
            g_print (" %s", argv[i]);
          g_print ("\n");
        }
      else if (g_strcmp0 (argv[1], "env") == 0)
        {
          const gchar * const *env;

          env = g_application_command_line_get_environ (cmdline);
          g_print ("environment");
          for (i = 0; env[i]; i++)
            if (g_str_has_prefix (env[i], "TEST="))
              g_print (" %s", env[i]);      
          g_print ("\n");
        }
      else if (g_strcmp0 (argv[1], "getenv") == 0)
        {
          g_print ("getenv TEST=%s\n", g_application_command_line_getenv (cmdline, "TEST"));
        }
      else if (g_strcmp0 (argv[1], "print") == 0)
        {
          g_application_command_line_print (cmdline, "print %s\n", argv[2]);
        }
      else if (g_strcmp0 (argv[1], "printerr") == 0)
        {
          g_application_command_line_printerr (cmdline, "printerr %s\n", argv[2]);
        }
      else if (g_strcmp0 (argv[1], "file") == 0)
        {
          GFile *file;

          file = g_application_command_line_create_file_for_arg (cmdline, argv[2]);         
          g_print ("file %s\n", g_file_get_path (file));
          g_object_unref (file);
        }
      else if (g_strcmp0 (argv[1], "properties") == 0)
        {
          gboolean remote;
          GVariant *data;

          g_object_get (cmdline,
                        "is-remote", &remote,
                        NULL);

          data = g_application_command_line_get_platform_data (cmdline);
          g_assert (remote);
          g_assert (g_variant_is_of_type (data, G_VARIANT_TYPE ("a{sv}")));
          g_variant_unref (data);
          g_print ("properties ok\n");
        }
      else if (g_strcmp0 (argv[1], "cwd") == 0)
        {
          g_print ("cwd %s\n", g_application_command_line_get_cwd (cmdline));
        }
      else if (g_strcmp0 (argv[1], "busy") == 0)
        {
          g_application_mark_busy (g_application_get_default ());
          g_print ("busy\n");
        }
      else if (g_strcmp0 (argv[1], "idle") == 0)
        {
          g_application_unmark_busy (g_application_get_default ());
          g_print ("idle\n");
        }
      else if (g_strcmp0 (argv[1], "stdin") == 0)
        {
          GInputStream *stream;

          stream = g_application_command_line_get_stdin (cmdline);

          g_assert (stream == NULL || G_IS_INPUT_STREAM (stream));
          g_object_unref (stream);

          g_print ("stdin ok\n");
        }
      else
        g_print ("unexpected command: %s\n", argv[1]);
    }
  else
    g_print ("got ./cmd %d\n", g_application_command_line_get_is_remote (cmdline));

  g_strfreev (argv);
  g_application_release (application);

  return 0;
}

static gboolean
action_cb (gpointer data)
{
  gchar **argv = data;
  GApplication *app;
  gchar **actions;
  gint i;

  if (g_strcmp0 (argv[1], "./actions") == 0)
    {
      app = g_application_get_default ();

      if (g_strcmp0 (argv[2], "list") == 0)
        {
          g_print ("actions");
          actions = g_action_group_list_actions (G_ACTION_GROUP (app));
          for (i = 0; actions[i]; i++)
            g_print (" %s", actions[i]);
          g_print ("\n");
          g_strfreev (actions);
        }
      else if (g_strcmp0 (argv[2], "activate") == 0)
        {
          g_action_group_activate_action (G_ACTION_GROUP (app),
                                          "action1", NULL);
        }
      else if (g_strcmp0 (argv[2], "set-state") == 0)
        {
          g_action_group_change_action_state (G_ACTION_GROUP (app),
                                              "action2",
                                              g_variant_new_boolean (TRUE));
        }
      g_application_release (app);
    }

  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_SEND_ENVIRONMENT |
                           (g_strcmp0 (argv[1], "./cmd") == 0
                             ? G_APPLICATION_HANDLES_COMMAND_LINE
                             : G_APPLICATION_HANDLES_OPEN));
  g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);
#ifdef STANDALONE
  g_application_set_inactivity_timeout (app, 10000);
#else
  g_application_set_inactivity_timeout (app, 1000);
#endif

  if (g_strcmp0 (argv[1], "./actions") == 0)
    {
      g_application_set_inactivity_timeout (app, 0);
      g_application_hold (app);
      g_idle_add (action_cb, argv);
    }

  status = g_application_run (app, argc - 1, argv + 1);

  g_object_unref (app);

  g_print ("exit status: %d\n", status);

  return 0;
}
