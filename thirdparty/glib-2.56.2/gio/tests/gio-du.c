#include <gio/gio.h>
#include <locale.h>

static gboolean option_use_async;
static gint     option_format_size;

static gint     outstanding_asyncs;

static void
print_result (const gchar *filename,
              guint64      disk_usage,
              guint64      num_dirs,
              guint64      num_files,
              GError      *error,
              gchar        nl)
{
  if (!error)
    {
      if (option_format_size)
        {
          GFormatSizeFlags format_flags;
          gchar *str;

          format_flags = (option_format_size > 1) ? G_FORMAT_SIZE_LONG_FORMAT : G_FORMAT_SIZE_DEFAULT;
          str = g_format_size_full (disk_usage, format_flags);
          g_print ("%s: %s (%"G_GUINT64_FORMAT" dirs, %"G_GUINT64_FORMAT" files)%c",
                   filename, str, num_dirs, num_files, nl);
          g_free (str);
        }
      else
        g_print ("%s: %"G_GUINT64_FORMAT" (%"G_GUINT64_FORMAT" dirs, %"G_GUINT64_FORMAT" files)%c",
                 filename, disk_usage, num_dirs, num_files, nl);
    }
  else
    {
      g_printerr ("%s: %s\n", filename, error->message);
      g_error_free (error);
    }
}

static void
async_ready_func (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  gchar *filename = user_data;
  GError *error = NULL;
  guint64 disk_usage;
  guint64 num_dirs;
  guint64 num_files;

  g_file_measure_disk_usage_finish (G_FILE (source), result, &disk_usage, &num_dirs, &num_files, &error);
  print_result (filename, disk_usage, num_dirs, num_files, error, '\n');
  outstanding_asyncs--;
  g_free (filename);
}

static void
report_progress (gboolean reporting,
                 guint64  disk_usage,
                 guint64  num_dirs,
                 guint64  num_files,
                 gpointer user_data)
{
  const gchar *filename = user_data;

  if (!reporting)
    g_printerr ("%s: warning: does not support progress reporting\n", filename);

  print_result (filename, disk_usage, num_dirs, num_files, NULL, '\r');
}

int
main (int argc, char **argv)
{
  GFileMeasureProgressCallback progress = NULL;
  GFileMeasureFlags flags = 0;
  gint i;

#ifdef G_OS_WIN32
  argv = g_win32_get_command_line ();
  argc = g_strv_length (argv);
#endif

  setlocale (LC_ALL, "");



  for (i = 1; argv[i] && argv[i][0] == '-'; i++)
    {
      if (g_str_equal (argv[i], "--"))
        break;

      if (g_str_equal (argv[i], "--help"))
        {
          g_print ("usage: du [--progress] [--async] [-x] [-h] [-h] [--apparent-size] [--any-error] [--] files...\n");
#ifdef G_OS_WIN32
          g_strfreev (argv);
#endif
          return 0;
        }
      else if (g_str_equal (argv[i], "-x"))
        flags |= G_FILE_MEASURE_NO_XDEV;
      else if (g_str_equal (argv[i], "-h"))
        option_format_size++;
      else if (g_str_equal (argv[i], "--apparent-size"))
        flags |= G_FILE_MEASURE_APPARENT_SIZE;
      else if (g_str_equal (argv[i], "--any-error"))
        flags |= G_FILE_MEASURE_REPORT_ANY_ERROR;
      else if (g_str_equal (argv[i], "--async"))
        option_use_async = TRUE;
      else if (g_str_equal (argv[i], "--progress"))
        progress = report_progress;
      else
        {
          g_printerr ("unrecognised flag %s\n", argv[i]);
        }
    }

  if (!argv[i])
    {
      g_printerr ("usage: du [--progress] [--async] [-x] [-h] [-h] [--apparent-size] [--any-error] [--] files...\n");
#ifdef G_OS_WIN32
      g_strfreev (argv);
#endif
      return 1;
    }

  while (argv[i])
  {
    GFile *file = g_file_new_for_commandline_arg (argv[i]);

    if (option_use_async)
    {
      g_file_measure_disk_usage_async (file, flags, G_PRIORITY_DEFAULT, NULL,
                                       progress, argv[i], async_ready_func, argv[i]);
      outstanding_asyncs++;
    }
    else
    {
      GError *error = NULL;
      guint64 disk_usage;
      guint64 num_dirs;
      guint64 num_files;

      g_file_measure_disk_usage (file, flags, NULL, progress, argv[i],
                                 &disk_usage, &num_dirs, &num_files, &error);
      print_result (argv[i], disk_usage, num_dirs, num_files, error, '\n');
    }

    g_object_unref (file);

    i++;
  }

  while (outstanding_asyncs)
    g_main_context_iteration (NULL, TRUE);

#ifdef G_OS_WIN32
  g_strfreev (argv);
#endif

  return 0;
}
