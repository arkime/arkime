/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gapplicationcommandline.h"

#include "glibintl.h"
#include "gfile.h"

#include <string.h>
#include <stdio.h>

#ifdef G_OS_UNIX
#include "gunixinputstream.h"
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#undef environ
#include "gwin32inputstream.h"
#endif

/**
 * SECTION:gapplicationcommandline
 * @title: GApplicationCommandLine
 * @short_description: A command-line invocation of an application
 * @include: gio/gio.h
 * @see_also: #GApplication
 *
 * #GApplicationCommandLine represents a command-line invocation of
 * an application.  It is created by #GApplication and emitted
 * in the #GApplication::command-line signal and virtual function.
 *
 * The class contains the list of arguments that the program was invoked
 * with.  It is also possible to query if the commandline invocation was
 * local (ie: the current process is running in direct response to the
 * invocation) or remote (ie: some other process forwarded the
 * commandline to this process).
 *
 * The GApplicationCommandLine object can provide the @argc and @argv
 * parameters for use with the #GOptionContext command-line parsing API,
 * with the g_application_command_line_get_arguments() function. See
 * [gapplication-example-cmdline3.c][gapplication-example-cmdline3]
 * for an example.
 *
 * The exit status of the originally-invoked process may be set and
 * messages can be printed to stdout or stderr of that process.  The
 * lifecycle of the originally-invoked process is tied to the lifecycle
 * of this object (ie: the process exits when the last reference is
 * dropped).
 *
 * The main use for #GApplicationCommandLine (and the
 * #GApplication::command-line signal) is 'Emacs server' like use cases:
 * You can set the `EDITOR` environment variable to have e.g. git use
 * your favourite editor to edit commit messages, and if you already
 * have an instance of the editor running, the editing will happen
 * in the running instance, instead of opening a new one. An important
 * aspect of this use case is that the process that gets started by git
 * does not return until the editing is done.
 *
 * Normally, the commandline is completely handled in the
 * #GApplication::command-line handler. The launching instance exits
 * once the signal handler in the primary instance has returned, and
 * the return value of the signal handler becomes the exit status
 * of the launching instance.
 * |[<!-- language="C" -->
 * static int
 * command_line (GApplication            *application,
 *               GApplicationCommandLine *cmdline)
 * {
 *   gchar **argv;
 *   gint argc;
 *   gint i;
 *
 *   argv = g_application_command_line_get_arguments (cmdline, &argc);
 *
 *   g_application_command_line_print (cmdline,
 *                                     "This text is written back\n"
 *                                     "to stdout of the caller\n");
 *
 *   for (i = 0; i < argc; i++)
 *     g_print ("argument %d: %s\n", i, argv[i]);
 *
 *   g_strfreev (argv);
 *
 *   return 0;
 * }
 * ]|
 * The complete example can be found here: 
 * [gapplication-example-cmdline.c](https://git.gnome.org/browse/glib/tree/gio/tests/gapplication-example-cmdline.c)
 *
 * In more complicated cases, the handling of the comandline can be
 * split between the launcher and the primary instance.
 * |[<!-- language="C" -->
 * static gboolean
 *  test_local_cmdline (GApplication   *application,
 *                      gchar        ***arguments,
 *                      gint           *exit_status)
 * {
 *   gint i, j;
 *   gchar **argv;
 *
 *   argv = *arguments;
 *
 *   i = 1;
 *   while (argv[i])
 *     {
 *       if (g_str_has_prefix (argv[i], "--local-"))
 *         {
 *           g_print ("handling argument %s locally\n", argv[i]);
 *           g_free (argv[i]);
 *           for (j = i; argv[j]; j++)
 *             argv[j] = argv[j + 1];
 *         }
 *       else
 *         {
 *           g_print ("not handling argument %s locally\n", argv[i]);
 *           i++;
 *         }
 *     }
 *
 *   *exit_status = 0;
 *
 *   return FALSE;
 * }
 *
 * static void
 * test_application_class_init (TestApplicationClass *class)
 * {
 *   G_APPLICATION_CLASS (class)->local_command_line = test_local_cmdline;
 *
 *   ...
 * }
 * ]|
 * In this example of split commandline handling, options that start
 * with `--local-` are handled locally, all other options are passed
 * to the #GApplication::command-line handler which runs in the primary
 * instance.
 *
 * The complete example can be found here:
 * [gapplication-example-cmdline2.c](https://git.gnome.org/browse/glib/tree/gio/tests/gapplication-example-cmdline2.c)
 *
 * If handling the commandline requires a lot of work, it may
 * be better to defer it.
 * |[<!-- language="C" -->
 * static gboolean
 * my_cmdline_handler (gpointer data)
 * {
 *   GApplicationCommandLine *cmdline = data;
 *
 *   // do the heavy lifting in an idle
 *
 *   g_application_command_line_set_exit_status (cmdline, 0);
 *   g_object_unref (cmdline); // this releases the application
 *
 *   return G_SOURCE_REMOVE;
 * }
 *
 * static int
 * command_line (GApplication            *application,
 *               GApplicationCommandLine *cmdline)
 * {
 *   // keep the application running until we are done with this commandline
 *   g_application_hold (application);
 *
 *   g_object_set_data_full (G_OBJECT (cmdline),
 *                           "application", application,
 *                           (GDestroyNotify)g_application_release);
 *
 *   g_object_ref (cmdline);
 *   g_idle_add (my_cmdline_handler, cmdline);
 *
 *   return 0;
 * }
 * ]|
 * In this example the commandline is not completely handled before
 * the #GApplication::command-line handler returns. Instead, we keep
 * a reference to the #GApplicationCommandLine object and handle it
 * later (in this example, in an idle). Note that it is necessary to
 * hold the application until you are done with the commandline.
 *
 * The complete example can be found here:
 * [gapplication-example-cmdline3.c](https://git.gnome.org/browse/glib/tree/gio/tests/gapplication-example-cmdline3.c)
 */

/**
 * GApplicationCommandLine:
 *
 * #GApplicationCommandLine is an opaque data structure and can only be accessed
 * using the following functions.
 */

/**
 * GApplicationCommandLineClass:
 *
 * The #GApplicationCommandLineClass-struct 
 * contains private data only.
 *
 * Since: 2.28
 **/
enum
{
  PROP_NONE,
  PROP_ARGUMENTS,
  PROP_OPTIONS,
  PROP_PLATFORM_DATA,
  PROP_IS_REMOTE
};

struct _GApplicationCommandLinePrivate
{
  GVariant *platform_data;
  GVariant *arguments;
  GVariant *options;
  GVariantDict *options_dict;
  gchar *cwd;  /* in GLib filename encoding, not UTF-8 */

  gchar **environ;
  gint exit_status;
};

G_DEFINE_TYPE_WITH_PRIVATE (GApplicationCommandLine, g_application_command_line, G_TYPE_OBJECT)

/* All subclasses represent remote invocations of some kind. */
#define IS_REMOTE(cmdline) (G_TYPE_FROM_INSTANCE (cmdline) != \
                            G_TYPE_APPLICATION_COMMAND_LINE)

static void
grok_platform_data (GApplicationCommandLine *cmdline)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_variant_iter_init (&iter, cmdline->priv->platform_data);

  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    if (strcmp (key, "cwd") == 0)
      {
        if (!cmdline->priv->cwd)
          cmdline->priv->cwd = g_variant_dup_bytestring (value, NULL);
      }

    else if (strcmp (key, "environ") == 0)
      {
        if (!cmdline->priv->environ)
          cmdline->priv->environ =
            g_variant_dup_bytestring_array (value, NULL);
      }

    else if (strcmp (key, "options") == 0)
      {
        if (!cmdline->priv->options)
          cmdline->priv->options = g_variant_ref (value);
      }
}

static void
g_application_command_line_real_print_literal (GApplicationCommandLine *cmdline,
                                               const gchar             *message)
{
  g_print ("%s", message);
}

static void
g_application_command_line_real_printerr_literal (GApplicationCommandLine *cmdline,
                                                  const gchar             *message)
{
  g_printerr ("%s", message);
}

static GInputStream *
g_application_command_line_real_get_stdin (GApplicationCommandLine *cmdline)
{
#ifdef G_OS_UNIX
  return g_unix_input_stream_new (0, FALSE);
#else
  return g_win32_input_stream_new (GetStdHandle (STD_INPUT_HANDLE), FALSE);
#endif
}

static void
g_application_command_line_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  switch (prop_id)
    {
    case PROP_ARGUMENTS:
      g_value_set_variant (value, cmdline->priv->arguments);
      break;

    case PROP_PLATFORM_DATA:
      g_value_set_variant (value, cmdline->priv->platform_data);
      break;

    case PROP_IS_REMOTE:
      g_value_set_boolean (value, IS_REMOTE (cmdline));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_application_command_line_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  switch (prop_id)
    {
    case PROP_ARGUMENTS:
      g_assert (cmdline->priv->arguments == NULL);
      cmdline->priv->arguments = g_value_dup_variant (value);
      break;

    case PROP_OPTIONS:
      g_assert (cmdline->priv->options == NULL);
      cmdline->priv->options = g_value_dup_variant (value);
      break;

    case PROP_PLATFORM_DATA:
      g_assert (cmdline->priv->platform_data == NULL);
      cmdline->priv->platform_data = g_value_dup_variant (value);
      if (cmdline->priv->platform_data != NULL)
        grok_platform_data (cmdline);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_application_command_line_finalize (GObject *object)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  if (cmdline->priv->options_dict)
    g_variant_dict_unref (cmdline->priv->options_dict);

  if (cmdline->priv->options)
      g_variant_unref (cmdline->priv->options);

  if (cmdline->priv->platform_data)
    g_variant_unref (cmdline->priv->platform_data);
  if (cmdline->priv->arguments)
    g_variant_unref (cmdline->priv->arguments);

  g_free (cmdline->priv->cwd);
  g_strfreev (cmdline->priv->environ);

  G_OBJECT_CLASS (g_application_command_line_parent_class)
    ->finalize (object);
}

static void
g_application_command_line_init (GApplicationCommandLine *cmdline)
{
  cmdline->priv = g_application_command_line_get_instance_private (cmdline);
}

static void
g_application_command_line_constructed (GObject *object)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);

  if (IS_REMOTE (cmdline))
    return;

  /* In the local case, set cmd and environ */
  if (!cmdline->priv->cwd)
    cmdline->priv->cwd = g_get_current_dir ();

  if (!cmdline->priv->environ)
    cmdline->priv->environ = g_get_environ ();
}

static void
g_application_command_line_class_init (GApplicationCommandLineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = g_application_command_line_get_property;
  object_class->set_property = g_application_command_line_set_property;
  object_class->finalize = g_application_command_line_finalize;
  object_class->constructed = g_application_command_line_constructed;

  class->printerr_literal = g_application_command_line_real_printerr_literal;
  class->print_literal = g_application_command_line_real_print_literal;
  class->get_stdin = g_application_command_line_real_get_stdin;

  g_object_class_install_property (object_class, PROP_ARGUMENTS,
    g_param_spec_variant ("arguments",
                          P_("Commandline arguments"),
                          P_("The commandline that caused this ::command-line signal emission"),
                          G_VARIANT_TYPE_BYTESTRING_ARRAY, NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_OPTIONS,
    g_param_spec_variant ("options",
                          P_("Options"),
                          P_("The options sent along with the commandline"),
                          G_VARIANT_TYPE_VARDICT, NULL, G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PLATFORM_DATA,
    g_param_spec_variant ("platform-data",
                          P_("Platform data"),
                          P_("Platform-specific data for the commandline"),
                          G_VARIANT_TYPE ("a{sv}"), NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IS_REMOTE,
    g_param_spec_boolean ("is-remote",
                          P_("Is remote"),
                          P_("TRUE if this is a remote commandline"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}


/**
 * g_application_command_line_get_arguments:
 * @cmdline: a #GApplicationCommandLine
 * @argc: (out) (optional): the length of the arguments array, or %NULL
 *
 * Gets the list of arguments that was passed on the command line.
 *
 * The strings in the array may contain non-UTF-8 data on UNIX (such as
 * filenames or arguments given in the system locale) but are always in
 * UTF-8 on Windows.
 *
 * If you wish to use the return value with #GOptionContext, you must
 * use g_option_context_parse_strv().
 *
 * The return value is %NULL-terminated and should be freed using
 * g_strfreev().
 *
 * Returns: (array length=argc) (element-type filename) (transfer full)
 *      the string array containing the arguments (the argv)
 *
 * Since: 2.28
 **/
gchar **
g_application_command_line_get_arguments (GApplicationCommandLine *cmdline,
                                          int                     *argc)
{
  gchar **argv;
  gsize len;

  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);

  argv = g_variant_dup_bytestring_array (cmdline->priv->arguments, &len);

  if (argc)
    *argc = len;

  return argv;
}

/**
 * g_application_command_line_get_options_dict:
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the options there were passed to g_application_command_line().
 *
 * If you did not override local_command_line() then these are the same
 * options that were parsed according to the #GOptionEntrys added to the
 * application with g_application_add_main_option_entries() and possibly
 * modified from your GApplication::handle-local-options handler.
 *
 * If no options were sent then an empty dictionary is returned so that
 * you don't need to check for %NULL.
 *
 * Returns: (transfer none): a #GVariantDict with the options
 *
 * Since: 2.40
 **/
GVariantDict *
g_application_command_line_get_options_dict (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);

  if (!cmdline->priv->options_dict)
    cmdline->priv->options_dict = g_variant_dict_new (cmdline->priv->options);

  return cmdline->priv->options_dict;
}

/**
 * g_application_command_line_get_stdin:
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the stdin of the invoking process.
 *
 * The #GInputStream can be used to read data passed to the standard
 * input of the invoking process.
 * This doesn't work on all platforms.  Presently, it is only available
 * on UNIX when using a DBus daemon capable of passing file descriptors.
 * If stdin is not available then %NULL will be returned.  In the
 * future, support may be expanded to other platforms.
 *
 * You must only call this function once per commandline invocation.
 *
 * Returns: (transfer full): a #GInputStream for stdin
 *
 * Since: 2.34
 **/
GInputStream *
g_application_command_line_get_stdin (GApplicationCommandLine *cmdline)
{
  return G_APPLICATION_COMMAND_LINE_GET_CLASS (cmdline)->get_stdin (cmdline);
}

/**
 * g_application_command_line_get_cwd:
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the working directory of the command line invocation.
 * The string may contain non-utf8 data.
 *
 * It is possible that the remote application did not send a working
 * directory, so this may be %NULL.
 *
 * The return value should not be modified or freed and is valid for as
 * long as @cmdline exists.
 *
 * Returns: (nullable) (type filename): the current directory, or %NULL
 *
 * Since: 2.28
 **/
const gchar *
g_application_command_line_get_cwd (GApplicationCommandLine *cmdline)
{
  return cmdline->priv->cwd;
}

/**
 * g_application_command_line_get_environ:
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the contents of the 'environ' variable of the command line
 * invocation, as would be returned by g_get_environ(), ie as a
 * %NULL-terminated list of strings in the form 'NAME=VALUE'.
 * The strings may contain non-utf8 data.
 *
 * The remote application usually does not send an environment.  Use
 * %G_APPLICATION_SEND_ENVIRONMENT to affect that.  Even with this flag
 * set it is possible that the environment is still not available (due
 * to invocation messages from other applications).
 *
 * The return value should not be modified or freed and is valid for as
 * long as @cmdline exists.
 *
 * See g_application_command_line_getenv() if you are only interested
 * in the value of a single environment variable.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer none):
 *     the environment strings, or %NULL if they were not sent
 *
 * Since: 2.28
 **/
const gchar * const *
g_application_command_line_get_environ (GApplicationCommandLine *cmdline)
{
  return (const gchar **)cmdline->priv->environ;
}

/**
 * g_application_command_line_getenv:
 * @cmdline: a #GApplicationCommandLine
 * @name: (type filename): the environment variable to get
 *
 * Gets the value of a particular environment variable of the command
 * line invocation, as would be returned by g_getenv().  The strings may
 * contain non-utf8 data.
 *
 * The remote application usually does not send an environment.  Use
 * %G_APPLICATION_SEND_ENVIRONMENT to affect that.  Even with this flag
 * set it is possible that the environment is still not available (due
 * to invocation messages from other applications).
 *
 * The return value should not be modified or freed and is valid for as
 * long as @cmdline exists.
 *
 * Returns: the value of the variable, or %NULL if unset or unsent
 *
 * Since: 2.28
 **/
const gchar *
g_application_command_line_getenv (GApplicationCommandLine *cmdline,
                                   const gchar             *name)
{
  gint length = strlen (name);
  gint i;

  /* TODO: expand on windows */
  if (cmdline->priv->environ)
    for (i = 0; cmdline->priv->environ[i]; i++)
      if (strncmp (cmdline->priv->environ[i], name, length) == 0 &&
          cmdline->priv->environ[i][length] == '=')
        return cmdline->priv->environ[i] + length + 1;

  return NULL;
}

/**
 * g_application_command_line_get_is_remote:
 * @cmdline: a #GApplicationCommandLine
 *
 * Determines if @cmdline represents a remote invocation.
 *
 * Returns: %TRUE if the invocation was remote
 *
 * Since: 2.28
 **/
gboolean
g_application_command_line_get_is_remote (GApplicationCommandLine *cmdline)
{
  return IS_REMOTE (cmdline);
}

/**
 * g_application_command_line_print:
 * @cmdline: a #GApplicationCommandLine
 * @format: a printf-style format string
 * @...: arguments, as per @format
 *
 * Formats a message and prints it using the stdout print handler in the
 * invoking process.
 *
 * If @cmdline is a local invocation then this is exactly equivalent to
 * g_print().  If @cmdline is remote then this is equivalent to calling
 * g_print() in the invoking process.
 *
 * Since: 2.28
 **/
void
g_application_command_line_print (GApplicationCommandLine *cmdline,
                                  const gchar             *format,
                                  ...)
{
  gchar *message;
  va_list ap;

  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));
  g_return_if_fail (format != NULL);

  va_start (ap, format);
  message = g_strdup_vprintf (format, ap);
  va_end (ap);

  G_APPLICATION_COMMAND_LINE_GET_CLASS (cmdline)
    ->print_literal (cmdline, message);
  g_free (message);
}

/**
 * g_application_command_line_printerr:
 * @cmdline: a #GApplicationCommandLine
 * @format: a printf-style format string
 * @...: arguments, as per @format
 *
 * Formats a message and prints it using the stderr print handler in the
 * invoking process.
 *
 * If @cmdline is a local invocation then this is exactly equivalent to
 * g_printerr().  If @cmdline is remote then this is equivalent to
 * calling g_printerr() in the invoking process.
 *
 * Since: 2.28
 **/
void
g_application_command_line_printerr (GApplicationCommandLine *cmdline,
                                     const gchar             *format,
                                     ...)
{
  gchar *message;
  va_list ap;

  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));
  g_return_if_fail (format != NULL);

  va_start (ap, format);
  message = g_strdup_vprintf (format, ap);
  va_end (ap);

  G_APPLICATION_COMMAND_LINE_GET_CLASS (cmdline)
    ->printerr_literal (cmdline, message);
  g_free (message);
}

/**
 * g_application_command_line_set_exit_status:
 * @cmdline: a #GApplicationCommandLine
 * @exit_status: the exit status
 *
 * Sets the exit status that will be used when the invoking process
 * exits.
 *
 * The return value of the #GApplication::command-line signal is
 * passed to this function when the handler returns.  This is the usual
 * way of setting the exit status.
 *
 * In the event that you want the remote invocation to continue running
 * and want to decide on the exit status in the future, you can use this
 * call.  For the case of a remote invocation, the remote process will
 * typically exit when the last reference is dropped on @cmdline.  The
 * exit status of the remote process will be equal to the last value
 * that was set with this function.
 *
 * In the case that the commandline invocation is local, the situation
 * is slightly more complicated.  If the commandline invocation results
 * in the mainloop running (ie: because the use-count of the application
 * increased to a non-zero value) then the application is considered to
 * have been 'successful' in a certain sense, and the exit status is
 * always zero.  If the application use count is zero, though, the exit
 * status of the local #GApplicationCommandLine is used.
 *
 * Since: 2.28
 **/
void
g_application_command_line_set_exit_status (GApplicationCommandLine *cmdline,
                                            int                      exit_status)
{
  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  cmdline->priv->exit_status = exit_status;
}

/**
 * g_application_command_line_get_exit_status:
 * @cmdline: a #GApplicationCommandLine
 *
 * Gets the exit status of @cmdline.  See
 * g_application_command_line_set_exit_status() for more information.
 *
 * Returns: the exit status
 *
 * Since: 2.28
 **/
int
g_application_command_line_get_exit_status (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), -1);

  return cmdline->priv->exit_status;
}

/**
 * g_application_command_line_get_platform_data:
 * @cmdline: #GApplicationCommandLine
 *
 * Gets the platform data associated with the invocation of @cmdline.
 *
 * This is a #GVariant dictionary containing information about the
 * context in which the invocation occurred.  It typically contains
 * information like the current working directory and the startup
 * notification ID.
 *
 * For local invocation, it will be %NULL.
 *
 * Returns: (nullable): the platform data, or %NULL
 *
 * Since: 2.28
 **/
GVariant *
g_application_command_line_get_platform_data (GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), NULL);

  if (cmdline->priv->platform_data)
    return g_variant_ref (cmdline->priv->platform_data);
  else
      return NULL;
}

/**
 * g_application_command_line_create_file_for_arg:
 * @cmdline: a #GApplicationCommandLine
 * @arg: (type filename): an argument from @cmdline
 *
 * Creates a #GFile corresponding to a filename that was given as part
 * of the invocation of @cmdline.
 *
 * This differs from g_file_new_for_commandline_arg() in that it
 * resolves relative pathnames using the current working directory of
 * the invoking process rather than the local process.
 *
 * Returns: (transfer full): a new #GFile
 *
 * Since: 2.36
 **/
GFile *
g_application_command_line_create_file_for_arg (GApplicationCommandLine *cmdline,
                                                const gchar             *arg)
{
  g_return_val_if_fail (arg != NULL, NULL);

  if (cmdline->priv->cwd)
    return g_file_new_for_commandline_arg_and_cwd (arg, cmdline->priv->cwd);

  g_warning ("Requested creation of GFile for commandline invocation that did not send cwd. "
             "Using cwd of local process to resolve relative path names.");

  return g_file_new_for_commandline_arg (arg);
}
