#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>


static gint
handle_local_options (GApplication      *application,
                      GVariantDict      *options,
                      gpointer           user_data)
{
  guint32 count;

  /* Deal (locally) with version option */
  if (g_variant_dict_lookup (options, "version", "b", &count))
    {
      g_print ("This is example-cmdline4, version 1.2.3\n");
      return EXIT_SUCCESS;
    }

  return -1;

}

static gint
command_line (GApplication                *application,
              GApplicationCommandLine     *cmdline,
              gpointer                     user_data)
{
  guint32 count;

  GVariantDict *options = g_application_command_line_get_options_dict (cmdline);

  /* Deal with arg option */
  if (g_variant_dict_lookup (options, "flag", "b", &count))
    {
      g_application_command_line_print (cmdline, "flag is set\n");
    }

  return EXIT_SUCCESS;
}


int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  GOptionEntry entries[] = {
    /* A version flag option, to be handled locally */
    { "version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "Show the application version", NULL },

    /* A dummy flag option, to be handled in primary */
    { "flag", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "A flag argument", NULL },

    { NULL }
  };

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_HANDLES_COMMAND_LINE);

  g_application_add_main_option_entries (app, entries);

  g_application_set_option_context_parameter_string (app, "- a simple command line example");
  g_application_set_option_context_summary (app,
                                            "Summary:\n"
                                            "This is a simple command line --help example.");
  g_application_set_option_context_description (app,
                                                "Description:\n"
                                                "This example illustrates the use of "
                                                "g_application command line --help functionalities "
                                                "(parameter string, summary, description). "
                                                "It does nothing at all except displaying information "
                                                "when invoked with --help argument...\n");

  g_signal_connect (app, "handle-local-options", G_CALLBACK (handle_local_options), NULL);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);

  /* This application does absolutely nothing, except if a command line is given */
  status = g_application_run (app, argc, argv);

  g_object_unref (app);

  return status;
}
