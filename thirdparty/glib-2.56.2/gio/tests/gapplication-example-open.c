#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static void
activate (GApplication *application)
{
  g_print ("activated\n");

  /* Note: when doing a longer-lasting action here that returns
   * to the mainloop, you should use g_application_hold() and
   * g_application_release() to keep the application alive until
   * the action is completed.
   */
}

static void
open (GApplication  *application,
      GFile        **files,
      gint           n_files,
      const gchar   *hint)
{
  gint i;

  for (i = 0; i < n_files; i++)
    {
      gchar *uri = g_file_get_uri (files[i]);
      g_print ("open %s\n", uri);
      g_free (uri);
    }

  /* Note: when doing a longer-lasting action here that returns
   * to the mainloop, you should use g_application_hold() and
   * g_application_release() to keep the application alive until
   * the action is completed.
   */
}

int
main (int argc, char **argv)
{
  GApplication *app;
  int status;

  app = g_application_new ("org.gtk.TestApplication",
                           G_APPLICATION_HANDLES_OPEN);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
  g_application_set_inactivity_timeout (app, 10000);

  status = g_application_run (app, argc, argv);

  g_object_unref (app);

  return status;
}
