#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static gboolean
my_cmdline_handler (gpointer data)
{
  GApplicationCommandLine *cmdline = data;
  gchar **args;
  gchar **argv;
  gint argc;
  gint arg1;
  gboolean arg2;
  gboolean help;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "arg1", 0, 0, G_OPTION_ARG_INT, &arg1, NULL, NULL },
    { "arg2", 0, 0, G_OPTION_ARG_NONE, &arg2, NULL, NULL },
    { "help", '?', 0, G_OPTION_ARG_NONE, &help, NULL, NULL },
    { NULL }
  };
  GError *error;
  gint i;

  args = g_application_command_line_get_arguments (cmdline, &argc);

  /* We have to make an extra copy of the array, since g_option_context_parse()
   * assumes that it can remove strings from the array without freeing them.
   */
  argv = g_new (gchar*, argc + 1);
  for (i = 0; i <= argc; i++)
    argv[i] = args[i];

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);

  arg1 = 0;
  arg2 = FALSE;
  help = FALSE;
  error = NULL;
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_application_command_line_printerr (cmdline, "%s\n", error->message);
      g_error_free (error);
      g_application_command_line_set_exit_status (cmdline, 1);
    }
  else if (help)
    {
      gchar *text;
      text = g_option_context_get_help (context, FALSE, NULL);
      g_application_command_line_print (cmdline, "%s",  text);
      g_free (text);
    }
  else
    {
      g_application_command_line_print (cmdline, "arg1 is %d and arg2 is %s\n",
                                        arg1, arg2 ? "TRUE" : "FALSE");
      g_application_command_line_set_exit_status (cmdline, 0);
    }

  g_free (argv);
  g_strfreev (args);

  g_option_context_free (context);

  /* we are done handling this commandline */
  g_object_unref (cmdline);

  return G_SOURCE_REMOVE;
}

static int
command_line (GApplication            *application,
              GApplicationCommandLine *cmdline)
{
  /* keep the application running until we are done with this commandline */
  g_application_hold (application);

  g_object_set_data_full (G_OBJECT (cmdline),
                          "application", application,
                          (GDestroyNotify)g_application_release);

  g_object_ref (cmdline);
  g_idle_add (my_cmdline_handler, cmdline);

  return 0;
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);
  g_application_set_inactivity_timeout (app, 10000);

  status = g_application_run (app, argc, argv);

  g_object_unref (app);

  return status;
}
