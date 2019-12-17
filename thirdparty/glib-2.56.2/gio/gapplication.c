/*
 * Copyright © 2010 Codethink Limited
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

/* Prologue {{{1 */
#include "config.h"

#include "gapplication.h"

#include "gapplicationcommandline.h"
#include "gsimpleactiongroup.h"
#include "gremoteactiongroup.h"
#include "gapplicationimpl.h"
#include "gactiongroup.h"
#include "gactionmap.h"
#include "gsettings.h"
#include "gnotification-private.h"
#include "gnotificationbackend.h"
#include "gdbusutils.h"

#include "gioenumtypes.h"
#include "gioenums.h"
#include "gfile.h"

#include "glibintl.h"

#include <string.h>

/**
 * SECTION:gapplication
 * @title: GApplication
 * @short_description: Core application class
 * @include: gio/gio.h
 *
 * A #GApplication is the foundation of an application.  It wraps some
 * low-level platform-specific services and is intended to act as the
 * foundation for higher-level application classes such as
 * #GtkApplication or #MxApplication.  In general, you should not use
 * this class outside of a higher level framework.
 *
 * GApplication provides convenient life cycle management by maintaining
 * a "use count" for the primary application instance. The use count can
 * be changed using g_application_hold() and g_application_release(). If
 * it drops to zero, the application exits. Higher-level classes such as
 * #GtkApplication employ the use count to ensure that the application
 * stays alive as long as it has any opened windows.
 *
 * Another feature that GApplication (optionally) provides is process
 * uniqueness. Applications can make use of this functionality by
 * providing a unique application ID. If given, only one application
 * with this ID can be running at a time per session. The session
 * concept is platform-dependent, but corresponds roughly to a graphical
 * desktop login. When your application is launched again, its
 * arguments are passed through platform communication to the already
 * running program. The already running instance of the program is
 * called the "primary instance"; for non-unique applications this is
 * the always the current instance. On Linux, the D-Bus session bus
 * is used for communication.
 *
 * The use of #GApplication differs from some other commonly-used
 * uniqueness libraries (such as libunique) in important ways. The
 * application is not expected to manually register itself and check
 * if it is the primary instance. Instead, the main() function of a
 * #GApplication should do very little more than instantiating the
 * application instance, possibly connecting signal handlers, then
 * calling g_application_run(). All checks for uniqueness are done
 * internally. If the application is the primary instance then the
 * startup signal is emitted and the mainloop runs. If the application
 * is not the primary instance then a signal is sent to the primary
 * instance and g_application_run() promptly returns. See the code
 * examples below.
 *
 * If used, the expected form of an application identifier is the same as
 * that of of a
 * [D-Bus well-known bus name](https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names-bus).
 * Examples include: `com.example.MyApp`, `org.example.internal_apps.Calculator`,
 * `org._7_zip.Archiver`.
 * For details on valid application identifiers, see g_application_id_is_valid().
 *
 * On Linux, the application identifier is claimed as a well-known bus name
 * on the user's session bus.  This means that the uniqueness of your
 * application is scoped to the current session.  It also means that your
 * application may provide additional services (through registration of other
 * object paths) at that bus name.  The registration of these object paths
 * should be done with the shared GDBus session bus.  Note that due to the
 * internal architecture of GDBus, method calls can be dispatched at any time
 * (even if a main loop is not running).  For this reason, you must ensure that
 * any object paths that you wish to register are registered before #GApplication
 * attempts to acquire the bus name of your application (which happens in
 * g_application_register()).  Unfortunately, this means that you cannot use
 * g_application_get_is_remote() to decide if you want to register object paths.
 *
 * GApplication also implements the #GActionGroup and #GActionMap
 * interfaces and lets you easily export actions by adding them with
 * g_action_map_add_action(). When invoking an action by calling
 * g_action_group_activate_action() on the application, it is always
 * invoked in the primary instance. The actions are also exported on
 * the session bus, and GIO provides the #GDBusActionGroup wrapper to
 * conveniently access them remotely. GIO provides a #GDBusMenuModel wrapper
 * for remote access to exported #GMenuModels.
 *
 * There is a number of different entry points into a GApplication:
 *
 * - via 'Activate' (i.e. just starting the application)
 *
 * - via 'Open' (i.e. opening some files)
 *
 * - by handling a command-line
 *
 * - via activating an action
 *
 * The #GApplication::startup signal lets you handle the application
 * initialization for all of these in a single place.
 *
 * Regardless of which of these entry points is used to start the
 * application, GApplication passes some "platform data from the
 * launching instance to the primary instance, in the form of a
 * #GVariant dictionary mapping strings to variants. To use platform
 * data, override the @before_emit or @after_emit virtual functions
 * in your #GApplication subclass. When dealing with
 * #GApplicationCommandLine objects, the platform data is
 * directly available via g_application_command_line_get_cwd(),
 * g_application_command_line_get_environ() and
 * g_application_command_line_get_platform_data().
 *
 * As the name indicates, the platform data may vary depending on the
 * operating system, but it always includes the current directory (key
 * "cwd"), and optionally the environment (ie the set of environment
 * variables and their values) of the calling process (key "environ").
 * The environment is only added to the platform data if the
 * %G_APPLICATION_SEND_ENVIRONMENT flag is set. #GApplication subclasses
 * can add their own platform data by overriding the @add_platform_data
 * virtual function. For instance, #GtkApplication adds startup notification
 * data in this way.
 *
 * To parse commandline arguments you may handle the
 * #GApplication::command-line signal or override the local_command_line()
 * vfunc, to parse them in either the primary instance or the local instance,
 * respectively.
 *
 * For an example of opening files with a GApplication, see
 * [gapplication-example-open.c](https://git.gnome.org/browse/glib/tree/gio/tests/gapplication-example-open.c).
 *
 * For an example of using actions with GApplication, see
 * [gapplication-example-actions.c](https://git.gnome.org/browse/glib/tree/gio/tests/gapplication-example-actions.c).
 *
 * For an example of using extra D-Bus hooks with GApplication, see
 * [gapplication-example-dbushooks.c](https://git.gnome.org/browse/glib/tree/gio/tests/gapplication-example-dbushooks.c).
 */

/**
 * GApplication:
 *
 * #GApplication is an opaque data structure and can only be accessed
 * using the following functions.
 * Since: 2.28
 */

/**
 * GApplicationClass:
 * @startup: invoked on the primary instance immediately after registration
 * @shutdown: invoked only on the registered primary instance immediately
 *      after the main loop terminates
 * @activate: invoked on the primary instance when an activation occurs
 * @open: invoked on the primary instance when there are files to open
 * @command_line: invoked on the primary instance when a command-line is
 *   not handled locally
 * @local_command_line: invoked (locally). The virtual function has the chance
 *     to inspect (and possibly replace) command line arguments. See
 *     g_application_run() for more information. Also see the
 *     #GApplication::handle-local-options signal, which is a simpler
 *     alternative to handling some commandline options locally
 * @before_emit: invoked on the primary instance before 'activate', 'open',
 *     'command-line' or any action invocation, gets the 'platform data' from
 *     the calling instance
 * @after_emit: invoked on the primary instance after 'activate', 'open',
 *     'command-line' or any action invocation, gets the 'platform data' from
 *     the calling instance
 * @add_platform_data: invoked (locally) to add 'platform data' to be sent to
 *     the primary instance when activating, opening or invoking actions
 * @quit_mainloop: Used to be invoked on the primary instance when the use
 *     count of the application drops to zero (and after any inactivity
 *     timeout, if requested). Not used anymore since 2.32
 * @run_mainloop: Used to be invoked on the primary instance from
 *     g_application_run() if the use-count is non-zero. Since 2.32,
 *     GApplication is iterating the main context directly and is not
 *     using @run_mainloop anymore
 * @dbus_register: invoked locally during registration, if the application is
 *     using its D-Bus backend. You can use this to export extra objects on the
 *     bus, that need to exist before the application tries to own the bus name.
 *     The function is passed the #GDBusConnection to to session bus, and the
 *     object path that #GApplication will use to export is D-Bus API.
 *     If this function returns %TRUE, registration will proceed; otherwise
 *     registration will abort. Since: 2.34
 * @dbus_unregister: invoked locally during unregistration, if the application
 *     is using its D-Bus backend. Use this to undo anything done by the
 *     @dbus_register vfunc. Since: 2.34
 * @handle_local_options: invoked locally after the parsing of the commandline
 *  options has occurred. Since: 2.40
 *
 * Virtual function table for #GApplication.
 *
 * Since: 2.28
 */

struct _GApplicationPrivate
{
  GApplicationFlags  flags;
  gchar             *id;
  gchar             *resource_path;

  GActionGroup      *actions;

  guint              inactivity_timeout_id;
  guint              inactivity_timeout;
  guint              use_count;
  guint              busy_count;

  guint              is_registered : 1;
  guint              is_remote : 1;
  guint              did_startup : 1;
  guint              did_shutdown : 1;
  guint              must_quit_now : 1;

  GRemoteActionGroup *remote_actions;
  GApplicationImpl   *impl;

  GNotificationBackend *notifications;

  /* GOptionContext support */
  GOptionGroup       *main_options;
  GSList             *option_groups;
  GHashTable         *packed_options;
  gboolean            options_parsed;
  gchar              *parameter_string;
  gchar              *summary;
  gchar              *description;

  /* Allocated option strings, from g_application_add_main_option() */
  GSList             *option_strings;
};

enum
{
  PROP_NONE,
  PROP_APPLICATION_ID,
  PROP_FLAGS,
  PROP_RESOURCE_BASE_PATH,
  PROP_IS_REGISTERED,
  PROP_IS_REMOTE,
  PROP_INACTIVITY_TIMEOUT,
  PROP_ACTION_GROUP,
  PROP_IS_BUSY
};

enum
{
  SIGNAL_STARTUP,
  SIGNAL_SHUTDOWN,
  SIGNAL_ACTIVATE,
  SIGNAL_OPEN,
  SIGNAL_ACTION,
  SIGNAL_COMMAND_LINE,
  SIGNAL_HANDLE_LOCAL_OPTIONS,
  NR_SIGNALS
};

static guint g_application_signals[NR_SIGNALS];

static void g_application_action_group_iface_init (GActionGroupInterface *);
static void g_application_action_map_iface_init (GActionMapInterface *);
G_DEFINE_TYPE_WITH_CODE (GApplication, g_application, G_TYPE_OBJECT,
 G_ADD_PRIVATE (GApplication)
 G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_application_action_group_iface_init)
 G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, g_application_action_map_iface_init))

/* GApplicationExportedActions {{{1 */

/* We create a subclass of GSimpleActionGroup that implements
 * GRemoteActionGroup and deals with the platform data using
 * GApplication's before/after_emit vfuncs.  This is the action group we
 * will be exporting.
 *
 * We could implement GRemoteActionGroup on GApplication directly, but
 * this would be potentially extremely confusing to have exposed as part
 * of the public API of GApplication.  We certainly don't want anyone in
 * the same process to be calling these APIs...
 */
typedef GSimpleActionGroupClass GApplicationExportedActionsClass;
typedef struct
{
  GSimpleActionGroup parent_instance;
  GApplication *application;
} GApplicationExportedActions;

static GType g_application_exported_actions_get_type   (void);
static void  g_application_exported_actions_iface_init (GRemoteActionGroupInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GApplicationExportedActions, g_application_exported_actions, G_TYPE_SIMPLE_ACTION_GROUP,
                         G_IMPLEMENT_INTERFACE (G_TYPE_REMOTE_ACTION_GROUP, g_application_exported_actions_iface_init))

static void
g_application_exported_actions_activate_action_full (GRemoteActionGroup *remote,
                                                     const gchar        *action_name,
                                                     GVariant           *parameter,
                                                     GVariant           *platform_data)
{
  GApplicationExportedActions *exported = (GApplicationExportedActions *) remote;

  G_APPLICATION_GET_CLASS (exported->application)
    ->before_emit (exported->application, platform_data);

  g_action_group_activate_action (G_ACTION_GROUP (exported), action_name, parameter);

  G_APPLICATION_GET_CLASS (exported->application)
    ->after_emit (exported->application, platform_data);
}

static void
g_application_exported_actions_change_action_state_full (GRemoteActionGroup *remote,
                                                         const gchar        *action_name,
                                                         GVariant           *value,
                                                         GVariant           *platform_data)
{
  GApplicationExportedActions *exported = (GApplicationExportedActions *) remote;

  G_APPLICATION_GET_CLASS (exported->application)
    ->before_emit (exported->application, platform_data);

  g_action_group_change_action_state (G_ACTION_GROUP (exported), action_name, value);

  G_APPLICATION_GET_CLASS (exported->application)
    ->after_emit (exported->application, platform_data);
}

static void
g_application_exported_actions_init (GApplicationExportedActions *actions)
{
}

static void
g_application_exported_actions_iface_init (GRemoteActionGroupInterface *iface)
{
  iface->activate_action_full = g_application_exported_actions_activate_action_full;
  iface->change_action_state_full = g_application_exported_actions_change_action_state_full;
}

static void
g_application_exported_actions_class_init (GApplicationExportedActionsClass *class)
{
}

static GActionGroup *
g_application_exported_actions_new (GApplication *application)
{
  GApplicationExportedActions *actions;

  actions = g_object_new (g_application_exported_actions_get_type (), NULL);
  actions->application = application;

  return G_ACTION_GROUP (actions);
}

/* Command line option handling {{{1 */

static void
free_option_entry (gpointer data)
{
  GOptionEntry *entry = data;

  switch (entry->arg)
    {
    case G_OPTION_ARG_STRING:
    case G_OPTION_ARG_FILENAME:
      g_free (*(gchar **) entry->arg_data);
      break;

    case G_OPTION_ARG_STRING_ARRAY:
    case G_OPTION_ARG_FILENAME_ARRAY:
      g_strfreev (*(gchar ***) entry->arg_data);
      break;

    default:
      /* most things require no free... */
      break;
    }

  /* ...except for the space that we allocated for it ourselves */
  g_free (entry->arg_data);

  g_slice_free (GOptionEntry, entry);
}

static void
g_application_pack_option_entries (GApplication *application,
                                   GVariantDict *dict)
{
  GHashTableIter iter;
  gpointer item;

  g_hash_table_iter_init (&iter, application->priv->packed_options);
  while (g_hash_table_iter_next (&iter, NULL, &item))
    {
      GOptionEntry *entry = item;
      GVariant *value = NULL;

      switch (entry->arg)
        {
        case G_OPTION_ARG_NONE:
          if (*(gboolean *) entry->arg_data != 2)
            value = g_variant_new_boolean (*(gboolean *) entry->arg_data);
          break;

        case G_OPTION_ARG_STRING:
          if (*(gchar **) entry->arg_data)
            value = g_variant_new_string (*(gchar **) entry->arg_data);
          break;

        case G_OPTION_ARG_INT:
          if (*(gint32 *) entry->arg_data)
            value = g_variant_new_int32 (*(gint32 *) entry->arg_data);
          break;

        case G_OPTION_ARG_FILENAME:
          if (*(gchar **) entry->arg_data)
            value = g_variant_new_bytestring (*(gchar **) entry->arg_data);
          break;

        case G_OPTION_ARG_STRING_ARRAY:
          if (*(gchar ***) entry->arg_data)
            value = g_variant_new_strv (*(const gchar ***) entry->arg_data, -1);
          break;

        case G_OPTION_ARG_FILENAME_ARRAY:
          if (*(gchar ***) entry->arg_data)
            value = g_variant_new_bytestring_array (*(const gchar ***) entry->arg_data, -1);
          break;

        case G_OPTION_ARG_DOUBLE:
          if (*(gdouble *) entry->arg_data)
            value = g_variant_new_double (*(gdouble *) entry->arg_data);
          break;

        case G_OPTION_ARG_INT64:
          if (*(gint64 *) entry->arg_data)
            value = g_variant_new_int64 (*(gint64 *) entry->arg_data);
          break;

        default:
          g_assert_not_reached ();
        }

      if (value)
        g_variant_dict_insert_value (dict, entry->long_name, value);
    }
}

static GVariantDict *
g_application_parse_command_line (GApplication   *application,
                                  gchar        ***arguments,
                                  GError        **error)
{
  gboolean become_service = FALSE;
  gchar *app_id = NULL;
  GVariantDict *dict = NULL;
  GOptionContext *context;
  GOptionGroup *gapplication_group;

  /* Due to the memory management of GOptionGroup we can only parse
   * options once.  That's because once you add a group to the
   * GOptionContext there is no way to get it back again.  This is fine:
   * local_command_line() should never get invoked more than once
   * anyway.  Add a sanity check just to be sure.
   */
  g_return_val_if_fail (!application->priv->options_parsed, NULL);

  context = g_option_context_new (application->priv->parameter_string);
  g_option_context_set_summary (context, application->priv->summary);
  g_option_context_set_description (context, application->priv->description);

  gapplication_group = g_option_group_new ("gapplication",
                                           _("GApplication options"), _("Show GApplication options"),
                                           NULL, NULL);
  g_option_group_set_translation_domain (gapplication_group, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gapplication_group);

  /* If the application has not registered local options and it has
   * G_APPLICATION_HANDLES_COMMAND_LINE then we have to assume that
   * their primary instance commandline handler may want to deal with
   * the arguments.  We must therefore ignore them.
   *
   * We must also ignore --help in this case since some applications
   * will try to handle this from the remote side.  See #737869.
   */
  if (application->priv->main_options == NULL && (application->priv->flags & G_APPLICATION_HANDLES_COMMAND_LINE))
    {
      g_option_context_set_ignore_unknown_options (context, TRUE);
      g_option_context_set_help_enabled (context, FALSE);
    }

  /* Add the main option group, if it exists */
  if (application->priv->main_options)
    {
      /* This consumes the main_options */
      g_option_context_set_main_group (context, application->priv->main_options);
      application->priv->main_options = NULL;
    }

  /* Add any other option groups if they exist.  Adding them to the
   * context will consume them, so we free the list as we go...
   */
  while (application->priv->option_groups)
    {
      g_option_context_add_group (context, application->priv->option_groups->data);
      application->priv->option_groups = g_slist_delete_link (application->priv->option_groups,
                                                              application->priv->option_groups);
    }

  /* In the case that we are not explicitly marked as a service or a
   * launcher then we want to add the "--gapplication-service" option to
   * allow the process to be made into a service.
   */
  if ((application->priv->flags & (G_APPLICATION_IS_SERVICE | G_APPLICATION_IS_LAUNCHER)) == 0)
    {
      GOptionEntry entries[] = {
        { "gapplication-service", '\0', 0, G_OPTION_ARG_NONE, &become_service,
          N_("Enter GApplication service mode (use from D-Bus service files)") },
        { NULL }
      };

      g_option_group_add_entries (gapplication_group, entries);
    }

  /* Allow overriding the ID if the application allows it */
  if (application->priv->flags & G_APPLICATION_CAN_OVERRIDE_APP_ID)
    {
      GOptionEntry entries[] = {
        { "gapplication-app-id", '\0', 0, G_OPTION_ARG_STRING, &app_id,
          N_("Override the application’s ID") },
        { NULL }
      };

      g_option_group_add_entries (gapplication_group, entries);
    }

  /* Now we parse... */
  if (!g_option_context_parse_strv (context, arguments, error))
    goto out;

  /* Check for --gapplication-service */
  if (become_service)
    application->priv->flags |= G_APPLICATION_IS_SERVICE;

  /* Check for --gapplication-app-id */
  if (app_id)
    g_application_set_application_id (application, app_id);

  dict = g_variant_dict_new (NULL);
  if (application->priv->packed_options)
    {
      g_application_pack_option_entries (application, dict);
      g_hash_table_unref (application->priv->packed_options);
      application->priv->packed_options = NULL;
    }

out:
  /* Make sure we don't run again */
  application->priv->options_parsed = TRUE;

  g_option_context_free (context);
  g_free (app_id);

  return dict;
}

static void
add_packed_option (GApplication *application,
                   GOptionEntry *entry)
{
  switch (entry->arg)
    {
    case G_OPTION_ARG_NONE:
      entry->arg_data = g_new (gboolean, 1);
      *(gboolean *) entry->arg_data = 2;
      break;

    case G_OPTION_ARG_INT:
      entry->arg_data = g_new0 (gint, 1);
      break;

    case G_OPTION_ARG_STRING:
    case G_OPTION_ARG_FILENAME:
    case G_OPTION_ARG_STRING_ARRAY:
    case G_OPTION_ARG_FILENAME_ARRAY:
      entry->arg_data = g_new0 (gpointer, 1);
      break;

    case G_OPTION_ARG_INT64:
      entry->arg_data = g_new0 (gint64, 1);
      break;

    case G_OPTION_ARG_DOUBLE:
      entry->arg_data = g_new0 (gdouble, 1);
      break;

    default:
      g_return_if_reached ();
    }

  if (!application->priv->packed_options)
    application->priv->packed_options = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, free_option_entry);

  g_hash_table_insert (application->priv->packed_options,
                       g_strdup (entry->long_name),
                       g_slice_dup (GOptionEntry, entry));
}

/**
 * g_application_add_main_option_entries:
 * @application: a #GApplication
 * @entries: (array zero-terminated=1) (element-type GOptionEntry) a
 *           %NULL-terminated list of #GOptionEntrys
 *
 * Adds main option entries to be handled by @application.
 *
 * This function is comparable to g_option_context_add_main_entries().
 *
 * After the commandline arguments are parsed, the
 * #GApplication::handle-local-options signal will be emitted.  At this
 * point, the application can inspect the values pointed to by @arg_data
 * in the given #GOptionEntrys.
 *
 * Unlike #GOptionContext, #GApplication supports giving a %NULL
 * @arg_data for a non-callback #GOptionEntry.  This results in the
 * argument in question being packed into a #GVariantDict which is also
 * passed to #GApplication::handle-local-options, where it can be
 * inspected and modified.  If %G_APPLICATION_HANDLES_COMMAND_LINE is
 * set, then the resulting dictionary is sent to the primary instance,
 * where g_application_command_line_get_options_dict() will return it.
 * This "packing" is done according to the type of the argument --
 * booleans for normal flags, strings for strings, bytestrings for
 * filenames, etc.  The packing only occurs if the flag is given (ie: we
 * do not pack a "false" #GVariant in the case that a flag is missing).
 *
 * In general, it is recommended that all commandline arguments are
 * parsed locally.  The options dictionary should then be used to
 * transmit the result of the parsing to the primary instance, where
 * g_variant_dict_lookup() can be used.  For local options, it is
 * possible to either use @arg_data in the usual way, or to consult (and
 * potentially remove) the option from the options dictionary.
 *
 * This function is new in GLib 2.40.  Before then, the only real choice
 * was to send all of the commandline arguments (options and all) to the
 * primary instance for handling.  #GApplication ignored them completely
 * on the local side.  Calling this function "opts in" to the new
 * behaviour, and in particular, means that unrecognised options will be
 * treated as errors.  Unrecognised options have never been ignored when
 * %G_APPLICATION_HANDLES_COMMAND_LINE is unset.
 *
 * If #GApplication::handle-local-options needs to see the list of
 * filenames, then the use of %G_OPTION_REMAINING is recommended.  If
 * @arg_data is %NULL then %G_OPTION_REMAINING can be used as a key into
 * the options dictionary.  If you do use %G_OPTION_REMAINING then you
 * need to handle these arguments for yourself because once they are
 * consumed, they will no longer be visible to the default handling
 * (which treats them as filenames to be opened).
 *
 * It is important to use the proper GVariant format when retrieving
 * the options with g_variant_dict_lookup():
 * - for %G_OPTION_ARG_NONE, use b
 * - for %G_OPTION_ARG_STRING, use &s
 * - for %G_OPTION_ARG_INT, use i
 * - for %G_OPTION_ARG_INT64, use x
 * - for %G_OPTION_ARG_DOUBLE, use d
 * - for %G_OPTION_ARG_FILENAME, use ^ay
 * - for %G_OPTION_ARG_STRING_ARRAY, use &as
 * - for %G_OPTION_ARG_FILENAME_ARRAY, use ^aay
 *
 * Since: 2.40
 */
void
g_application_add_main_option_entries (GApplication       *application,
                                       const GOptionEntry *entries)
{
  gint i;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (entries != NULL);

  if (!application->priv->main_options)
    {
      application->priv->main_options = g_option_group_new (NULL, NULL, NULL, NULL, NULL);
      g_option_group_set_translation_domain (application->priv->main_options, NULL);
    }

  for (i = 0; entries[i].long_name; i++)
    {
      GOptionEntry my_entries[2] = { { NULL }, { NULL } };
      my_entries[0] = entries[i];

      if (!my_entries[0].arg_data)
        add_packed_option (application, &my_entries[0]);

      g_option_group_add_entries (application->priv->main_options, my_entries);
    }
}

/**
 * g_application_add_main_option:
 * @application: the #GApplication
 * @long_name: the long name of an option used to specify it in a commandline
 * @short_name: the short name of an option
 * @flags: flags from #GOptionFlags
 * @arg: the type of the option, as a #GOptionArg
 * @description: the description for the option in `--help` output
 * @arg_description: (nullable): the placeholder to use for the extra argument
 *    parsed by the option in `--help` output
 *
 * Add an option to be handled by @application.
 *
 * Calling this function is the equivalent of calling
 * g_application_add_main_option_entries() with a single #GOptionEntry
 * that has its arg_data member set to %NULL.
 *
 * The parsed arguments will be packed into a #GVariantDict which
 * is passed to #GApplication::handle-local-options. If
 * %G_APPLICATION_HANDLES_COMMAND_LINE is set, then it will also
 * be sent to the primary instance. See
 * g_application_add_main_option_entries() for more details.
 *
 * See #GOptionEntry for more documentation of the arguments.
 *
 * Since: 2.42
 **/
void
g_application_add_main_option (GApplication *application,
                               const char   *long_name,
                               char          short_name,
                               GOptionFlags  flags,
                               GOptionArg    arg,
                               const char   *description,
                               const char   *arg_description)
{
  gchar *dup_string;
  GOptionEntry my_entry[2] = {
    { NULL, short_name, flags, arg, NULL, NULL, NULL },
    { NULL }
  };

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (long_name != NULL);
  g_return_if_fail (description != NULL);

  my_entry[0].long_name = dup_string = g_strdup (long_name);
  application->priv->option_strings = g_slist_prepend (application->priv->option_strings, dup_string);

  my_entry[0].description = dup_string = g_strdup (description);
  application->priv->option_strings = g_slist_prepend (application->priv->option_strings, dup_string);

  my_entry[0].arg_description = dup_string = g_strdup (arg_description);
  application->priv->option_strings = g_slist_prepend (application->priv->option_strings, dup_string);

  g_application_add_main_option_entries (application, my_entry);
}

/**
 * g_application_add_option_group:
 * @application: the #GApplication
 * @group: (transfer full): a #GOptionGroup
 *
 * Adds a #GOptionGroup to the commandline handling of @application.
 *
 * This function is comparable to g_option_context_add_group().
 *
 * Unlike g_application_add_main_option_entries(), this function does
 * not deal with %NULL @arg_data and never transmits options to the
 * primary instance.
 *
 * The reason for that is because, by the time the options arrive at the
 * primary instance, it is typically too late to do anything with them.
 * Taking the GTK option group as an example: GTK will already have been
 * initialised by the time the #GApplication::command-line handler runs.
 * In the case that this is not the first-running instance of the
 * application, the existing instance may already have been running for
 * a very long time.
 *
 * This means that the options from #GOptionGroup are only really usable
 * in the case that the instance of the application being run is the
 * first instance.  Passing options like `--display=` or `--gdk-debug=`
 * on future runs will have no effect on the existing primary instance.
 *
 * Calling this function will cause the options in the supplied option
 * group to be parsed, but it does not cause you to be "opted in" to the
 * new functionality whereby unrecognised options are rejected even if
 * %G_APPLICATION_HANDLES_COMMAND_LINE was given.
 *
 * Since: 2.40
 **/
void
g_application_add_option_group (GApplication *application,
                                GOptionGroup *group)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (group != NULL);

  application->priv->option_groups = g_slist_prepend (application->priv->option_groups, group);
}

/**
 * g_application_set_option_context_parameter_string:
 * @application: the #GApplication
 * @parameter_string: (nullable): a string which is displayed
 *   in the first line of `--help` output, after the usage summary `programname [OPTION...]`.
 *
 * Sets the parameter string to be used by the commandline handling of @application.
 *
 * This function registers the argument to be passed to g_option_context_new()
 * when the internal #GOptionContext of @application is created.
 *
 * See g_option_context_new() for more information about @parameter_string.
 *
 * Since: 2.56
 */
void
g_application_set_option_context_parameter_string (GApplication *application,
                                                   const gchar  *parameter_string)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  g_free (application->priv->parameter_string);
  application->priv->parameter_string = g_strdup (parameter_string);
}

/**
 * g_application_set_option_context_summary:
 * @application: the #GApplication
 * @summary: (nullable): a string to be shown in `--help` output
 *  before the list of options, or %NULL
 *
 * Adds a summary to the @application option context.
 *
 * See g_option_context_set_summary() for more information.
 *
 * Since: 2.56
 */
void
g_application_set_option_context_summary (GApplication *application,
                                          const gchar  *summary)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  g_free (application->priv->summary);
  application->priv->summary = g_strdup (summary);
}

/**
 * g_application_set_option_context_description:
 * @application: the #GApplication
 * @description: (nullable): a string to be shown in `--help` output
 *  after the list of options, or %NULL
 *
 * Adds a description to the @application option context.
 *
 * See g_option_context_set_description() for more information.
 *
 * Since: 2.56
 */
void
g_application_set_option_context_description (GApplication *application,
                                              const gchar  *description)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  g_free (application->priv->description);
  application->priv->description = g_strdup (description);

}


/* vfunc defaults {{{1 */
static void
g_application_real_before_emit (GApplication *application,
                                GVariant     *platform_data)
{
}

static void
g_application_real_after_emit (GApplication *application,
                               GVariant     *platform_data)
{
}

static void
g_application_real_startup (GApplication *application)
{
  application->priv->did_startup = TRUE;
}

static void
g_application_real_shutdown (GApplication *application)
{
  application->priv->did_shutdown = TRUE;
}

static void
g_application_real_activate (GApplication *application)
{
  if (!g_signal_has_handler_pending (application,
                                     g_application_signals[SIGNAL_ACTIVATE],
                                     0, TRUE) &&
      G_APPLICATION_GET_CLASS (application)->activate == g_application_real_activate)
    {
      static gboolean warned;

      if (warned)
        return;

      g_warning ("Your application does not implement "
                 "g_application_activate() and has no handlers connected "
                 "to the 'activate' signal.  It should do one of these.");
      warned = TRUE;
    }
}

static void
g_application_real_open (GApplication  *application,
                         GFile        **files,
                         gint           n_files,
                         const gchar   *hint)
{
  if (!g_signal_has_handler_pending (application,
                                     g_application_signals[SIGNAL_OPEN],
                                     0, TRUE) &&
      G_APPLICATION_GET_CLASS (application)->open == g_application_real_open)
    {
      static gboolean warned;

      if (warned)
        return;

      g_warning ("Your application claims to support opening files "
                 "but does not implement g_application_open() and has no "
                 "handlers connected to the 'open' signal.");
      warned = TRUE;
    }
}

static int
g_application_real_command_line (GApplication            *application,
                                 GApplicationCommandLine *cmdline)
{
  if (!g_signal_has_handler_pending (application,
                                     g_application_signals[SIGNAL_COMMAND_LINE],
                                     0, TRUE) &&
      G_APPLICATION_GET_CLASS (application)->command_line == g_application_real_command_line)
    {
      static gboolean warned;

      if (warned)
        return 1;

      g_warning ("Your application claims to support custom command line "
                 "handling but does not implement g_application_command_line() "
                 "and has no handlers connected to the 'command-line' signal.");

      warned = TRUE;
    }

    return 1;
}

static gint
g_application_real_handle_local_options (GApplication *application,
                                         GVariantDict *options)
{
  return -1;
}

static GVariant *
get_platform_data (GApplication *application,
                   GVariant     *options)
{
  GVariantBuilder *builder;
  GVariant *result;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

  {
    gchar *cwd = g_get_current_dir ();
    g_variant_builder_add (builder, "{sv}", "cwd",
                           g_variant_new_bytestring (cwd));
    g_free (cwd);
  }

  if (application->priv->flags & G_APPLICATION_SEND_ENVIRONMENT)
    {
      GVariant *array;
      gchar **envp;

      envp = g_get_environ ();
      array = g_variant_new_bytestring_array ((const gchar **) envp, -1);
      g_strfreev (envp);

      g_variant_builder_add (builder, "{sv}", "environ", array);
    }

  if (options)
    g_variant_builder_add (builder, "{sv}", "options", options);

  G_APPLICATION_GET_CLASS (application)->
    add_platform_data (application, builder);

  result = g_variant_builder_end (builder);
  g_variant_builder_unref (builder);

  return result;
}

static void
g_application_call_command_line (GApplication        *application,
                                 const gchar * const *arguments,
                                 GVariant            *options,
                                 gint                *exit_status)
{
  if (application->priv->is_remote)
    {
      GVariant *platform_data;

      platform_data = get_platform_data (application, options);
      *exit_status = g_application_impl_command_line (application->priv->impl, arguments, platform_data);
    }
  else
    {
      GApplicationCommandLine *cmdline;
      GVariant *v;

      v = g_variant_new_bytestring_array ((const gchar **) arguments, -1);
      cmdline = g_object_new (G_TYPE_APPLICATION_COMMAND_LINE,
                              "arguments", v,
                              "options", options,
                              NULL);
      g_signal_emit (application, g_application_signals[SIGNAL_COMMAND_LINE], 0, cmdline, exit_status);
      g_object_unref (cmdline);
    }
}

static gboolean
g_application_real_local_command_line (GApplication   *application,
                                       gchar        ***arguments,
                                       int            *exit_status)
{
  GError *error = NULL;
  GVariantDict *options;
  gint n_args;

  options = g_application_parse_command_line (application, arguments, &error);
  if (!options)
    {
      g_printerr ("%s\n", error->message);
      *exit_status = 1;
      return TRUE;
    }

  g_signal_emit (application, g_application_signals[SIGNAL_HANDLE_LOCAL_OPTIONS], 0, options, exit_status);

  if (*exit_status >= 0)
    {
      g_variant_dict_unref (options);
      return TRUE;
    }

  if (!g_application_register (application, NULL, &error))
    {
      g_printerr ("Failed to register: %s\n", error->message);
      g_variant_dict_unref (options);
      g_error_free (error);
      *exit_status = 1;
      return TRUE;
    }

  n_args = g_strv_length (*arguments);

  if (application->priv->flags & G_APPLICATION_IS_SERVICE)
    {
      if ((*exit_status = n_args > 1))
        {
          g_printerr ("GApplication service mode takes no arguments.\n");
          application->priv->flags &= ~G_APPLICATION_IS_SERVICE;
          *exit_status = 1;
        }
      else
        *exit_status = 0;
    }
  else if (application->priv->flags & G_APPLICATION_HANDLES_COMMAND_LINE)
    {
      g_application_call_command_line (application,
                                       (const gchar **) *arguments,
                                       g_variant_dict_end (options),
                                       exit_status);
    }
  else
    {
      if (n_args <= 1)
        {
          g_application_activate (application);
          *exit_status = 0;
        }

      else
        {
          if (~application->priv->flags & G_APPLICATION_HANDLES_OPEN)
            {
              g_critical ("This application can not open files.");
              *exit_status = 1;
            }
          else
            {
              GFile **files;
              gint n_files;
              gint i;

              n_files = n_args - 1;
              files = g_new (GFile *, n_files);

              for (i = 0; i < n_files; i++)
                files[i] = g_file_new_for_commandline_arg ((*arguments)[i + 1]);

              g_application_open (application, files, n_files, "");

              for (i = 0; i < n_files; i++)
                g_object_unref (files[i]);
              g_free (files);

              *exit_status = 0;
            }
        }
    }

  g_variant_dict_unref (options);

  return TRUE;
}

static void
g_application_real_add_platform_data (GApplication    *application,
                                      GVariantBuilder *builder)
{
}

static gboolean
g_application_real_dbus_register (GApplication    *application,
                                  GDBusConnection *connection,
                                  const gchar     *object_path,
                                  GError         **error)
{
  return TRUE;
}

static void
g_application_real_dbus_unregister (GApplication    *application,
                                    GDBusConnection *connection,
                                    const gchar     *object_path)
{
}

/* GObject implementation stuff {{{1 */
static void
g_application_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GApplication *application = G_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_application_set_application_id (application,
                                        g_value_get_string (value));
      break;

    case PROP_FLAGS:
      g_application_set_flags (application, g_value_get_flags (value));
      break;

    case PROP_RESOURCE_BASE_PATH:
      g_application_set_resource_base_path (application, g_value_get_string (value));
      break;

    case PROP_INACTIVITY_TIMEOUT:
      g_application_set_inactivity_timeout (application,
                                            g_value_get_uint (value));
      break;

    case PROP_ACTION_GROUP:
      g_clear_object (&application->priv->actions);
      application->priv->actions = g_value_dup_object (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * g_application_set_action_group:
 * @application: a #GApplication
 * @action_group: (nullable): a #GActionGroup, or %NULL
 *
 * This used to be how actions were associated with a #GApplication.
 * Now there is #GActionMap for that.
 *
 * Since: 2.28
 *
 * Deprecated:2.32:Use the #GActionMap interface instead.  Never ever
 * mix use of this API with use of #GActionMap on the same @application
 * or things will go very badly wrong.  This function is known to
 * introduce buggy behaviour (ie: signals not emitted on changes to the
 * action group), so you should really use #GActionMap instead.
 **/
void
g_application_set_action_group (GApplication *application,
                                GActionGroup *action_group)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (!application->priv->is_registered);

  if (application->priv->actions != NULL)
    g_object_unref (application->priv->actions);

  application->priv->actions = action_group;

  if (application->priv->actions != NULL)
    g_object_ref (application->priv->actions);
}

static void
g_application_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GApplication *application = G_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_value_set_string (value,
                          g_application_get_application_id (application));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value,
                         g_application_get_flags (application));
      break;

    case PROP_RESOURCE_BASE_PATH:
      g_value_set_string (value, g_application_get_resource_base_path (application));
      break;

    case PROP_IS_REGISTERED:
      g_value_set_boolean (value,
                           g_application_get_is_registered (application));
      break;

    case PROP_IS_REMOTE:
      g_value_set_boolean (value,
                           g_application_get_is_remote (application));
      break;

    case PROP_INACTIVITY_TIMEOUT:
      g_value_set_uint (value,
                        g_application_get_inactivity_timeout (application));
      break;

    case PROP_IS_BUSY:
      g_value_set_boolean (value, g_application_get_is_busy (application));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_application_constructed (GObject *object)
{
  GApplication *application = G_APPLICATION (object);

  if (g_application_get_default () == NULL)
    g_application_set_default (application);

  /* People should not set properties from _init... */
  g_assert (application->priv->resource_path == NULL);

  if (application->priv->id != NULL)
    {
      gint i;

      application->priv->resource_path = g_strconcat ("/", application->priv->id, NULL);

      for (i = 1; application->priv->resource_path[i]; i++)
        if (application->priv->resource_path[i] == '.')
          application->priv->resource_path[i] = '/';
    }
}

static void
g_application_dispose (GObject *object)
{
  GApplication *application = G_APPLICATION (object);

  if (application->priv->impl != NULL &&
      G_APPLICATION_GET_CLASS (application)->dbus_unregister != g_application_real_dbus_unregister)
    {
      static gboolean warned;

      if (!warned)
        {
          g_warning ("Your application did not unregister from D-Bus before destruction. "
                     "Consider using g_application_run().");
        }

      warned = TRUE;
    }

  G_OBJECT_CLASS (g_application_parent_class)->dispose (object);
}

static void
g_application_finalize (GObject *object)
{
  GApplication *application = G_APPLICATION (object);

  g_slist_free_full (application->priv->option_groups, (GDestroyNotify) g_option_group_unref);
  if (application->priv->main_options)
    g_option_group_unref (application->priv->main_options);
  if (application->priv->packed_options)
    g_hash_table_unref (application->priv->packed_options);

  g_free (application->priv->parameter_string);
  g_free (application->priv->summary);
  g_free (application->priv->description);

  g_slist_free_full (application->priv->option_strings, g_free);

  if (application->priv->impl)
    g_application_impl_destroy (application->priv->impl);
  g_free (application->priv->id);

  if (g_application_get_default () == application)
    g_application_set_default (NULL);

  if (application->priv->actions)
    g_object_unref (application->priv->actions);

  if (application->priv->notifications)
    g_object_unref (application->priv->notifications);

  g_free (application->priv->resource_path);

  G_OBJECT_CLASS (g_application_parent_class)
    ->finalize (object);
}

static void
g_application_init (GApplication *application)
{
  application->priv = g_application_get_instance_private (application);

  application->priv->actions = g_application_exported_actions_new (application);

  /* application->priv->actions is the one and only ref on the group, so when
   * we dispose, the action group will die, disconnecting all signals.
   */
  g_signal_connect_swapped (application->priv->actions, "action-added",
                            G_CALLBACK (g_action_group_action_added), application);
  g_signal_connect_swapped (application->priv->actions, "action-enabled-changed",
                            G_CALLBACK (g_action_group_action_enabled_changed), application);
  g_signal_connect_swapped (application->priv->actions, "action-state-changed",
                            G_CALLBACK (g_action_group_action_state_changed), application);
  g_signal_connect_swapped (application->priv->actions, "action-removed",
                            G_CALLBACK (g_action_group_action_removed), application);
}

static gboolean
g_application_handle_local_options_accumulator (GSignalInvocationHint *ihint,
                                                GValue                *return_accu,
                                                const GValue          *handler_return,
                                                gpointer               dummy)
{
  gint value;

  value = g_value_get_int (handler_return);
  g_value_set_int (return_accu, value);

  return value < 0;
}

static void
g_application_class_init (GApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = g_application_constructed;
  object_class->dispose = g_application_dispose;
  object_class->finalize = g_application_finalize;
  object_class->get_property = g_application_get_property;
  object_class->set_property = g_application_set_property;

  class->before_emit = g_application_real_before_emit;
  class->after_emit = g_application_real_after_emit;
  class->startup = g_application_real_startup;
  class->shutdown = g_application_real_shutdown;
  class->activate = g_application_real_activate;
  class->open = g_application_real_open;
  class->command_line = g_application_real_command_line;
  class->local_command_line = g_application_real_local_command_line;
  class->handle_local_options = g_application_real_handle_local_options;
  class->add_platform_data = g_application_real_add_platform_data;
  class->dbus_register = g_application_real_dbus_register;
  class->dbus_unregister = g_application_real_dbus_unregister;

  g_object_class_install_property (object_class, PROP_APPLICATION_ID,
    g_param_spec_string ("application-id",
                         P_("Application identifier"),
                         P_("The unique identifier for the application"),
                         NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FLAGS,
    g_param_spec_flags ("flags",
                        P_("Application flags"),
                        P_("Flags specifying the behaviour of the application"),
                        G_TYPE_APPLICATION_FLAGS, G_APPLICATION_FLAGS_NONE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_RESOURCE_BASE_PATH,
    g_param_spec_string ("resource-base-path",
                         P_("Resource base path"),
                         P_("The base resource path for the application"),
                         NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IS_REGISTERED,
    g_param_spec_boolean ("is-registered",
                          P_("Is registered"),
                          P_("If g_application_register() has been called"),
                          FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IS_REMOTE,
    g_param_spec_boolean ("is-remote",
                          P_("Is remote"),
                          P_("If this application instance is remote"),
                          FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_INACTIVITY_TIMEOUT,
    g_param_spec_uint ("inactivity-timeout",
                       P_("Inactivity timeout"),
                       P_("Time (ms) to stay alive after becoming idle"),
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ACTION_GROUP,
    g_param_spec_object ("action-group",
                         P_("Action group"),
                         P_("The group of actions that the application exports"),
                         G_TYPE_ACTION_GROUP,
                         G_PARAM_DEPRECATED | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GApplication:is-busy:
   *
   * Whether the application is currently marked as busy through
   * g_application_mark_busy() or g_application_bind_busy_property().
   *
   * Since: 2.44
   */
  g_object_class_install_property (object_class, PROP_IS_BUSY,
    g_param_spec_boolean ("is-busy",
                          P_("Is busy"),
                          P_("If this application is currently marked busy"),
                          FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GApplication::startup:
   * @application: the application
   *
   * The ::startup signal is emitted on the primary instance immediately
   * after registration. See g_application_register().
   */
  g_application_signals[SIGNAL_STARTUP] =
    g_signal_new (I_("startup"), G_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GApplicationClass, startup),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GApplication::shutdown:
   * @application: the application
   *
   * The ::shutdown signal is emitted only on the registered primary instance
   * immediately after the main loop terminates.
   */
  g_application_signals[SIGNAL_SHUTDOWN] =
    g_signal_new (I_("shutdown"), G_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, shutdown),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GApplication::activate:
   * @application: the application
   *
   * The ::activate signal is emitted on the primary instance when an
   * activation occurs. See g_application_activate().
   */
  g_application_signals[SIGNAL_ACTIVATE] =
    g_signal_new (I_("activate"), G_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, activate),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);


  /**
   * GApplication::open:
   * @application: the application
   * @files: (array length=n_files) (element-type GFile): an array of #GFiles
   * @n_files: the length of @files
   * @hint: a hint provided by the calling instance
   *
   * The ::open signal is emitted on the primary instance when there are
   * files to open. See g_application_open() for more information.
   */
  g_application_signals[SIGNAL_OPEN] =
    g_signal_new (I_("open"), G_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, open),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_STRING);

  /**
   * GApplication::command-line:
   * @application: the application
   * @command_line: a #GApplicationCommandLine representing the
   *     passed commandline
   *
   * The ::command-line signal is emitted on the primary instance when
   * a commandline is not handled locally. See g_application_run() and
   * the #GApplicationCommandLine documentation for more information.
   *
   * Returns: An integer that is set as the exit status for the calling
   *   process. See g_application_command_line_set_exit_status().
   */
  g_application_signals[SIGNAL_COMMAND_LINE] =
    g_signal_new (I_("command-line"), G_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, command_line),
                  g_signal_accumulator_first_wins, NULL,
                  NULL,
                  G_TYPE_INT, 1, G_TYPE_APPLICATION_COMMAND_LINE);

  /**
   * GApplication::handle-local-options:
   * @application: the application
   * @options: the options dictionary
   *
   * The ::handle-local-options signal is emitted on the local instance
   * after the parsing of the commandline options has occurred.
   *
   * You can add options to be recognised during commandline option
   * parsing using g_application_add_main_option_entries() and
   * g_application_add_option_group().
   *
   * Signal handlers can inspect @options (along with values pointed to
   * from the @arg_data of an installed #GOptionEntrys) in order to
   * decide to perform certain actions, including direct local handling
   * (which may be useful for options like --version).
   *
   * In the event that the application is marked
   * %G_APPLICATION_HANDLES_COMMAND_LINE the "normal processing" will
   * send the @options dictionary to the primary instance where it can be
   * read with g_application_command_line_get_options_dict().  The signal
   * handler can modify the dictionary before returning, and the
   * modified dictionary will be sent.
   *
   * In the event that %G_APPLICATION_HANDLES_COMMAND_LINE is not set,
   * "normal processing" will treat the remaining uncollected command
   * line arguments as filenames or URIs.  If there are no arguments,
   * the application is activated by g_application_activate().  One or
   * more arguments results in a call to g_application_open().
   *
   * If you want to handle the local commandline arguments for yourself
   * by converting them to calls to g_application_open() or
   * g_action_group_activate_action() then you must be sure to register
   * the application first.  You should probably not call
   * g_application_activate() for yourself, however: just return -1 and
   * allow the default handler to do it for you.  This will ensure that
   * the `--gapplication-service` switch works properly (i.e. no activation
   * in that case).
   *
   * Note that this signal is emitted from the default implementation of
   * local_command_line().  If you override that function and don't
   * chain up then this signal will never be emitted.
   *
   * You can override local_command_line() if you need more powerful
   * capabilities than what is provided here, but this should not
   * normally be required.
   *
   * Returns: an exit code. If you have handled your options and want
   * to exit the process, return a non-negative option, 0 for success,
   * and a positive value for failure. To continue, return -1 to let
   * the default option processing continue.
   *
   * Since: 2.40
   **/
  g_application_signals[SIGNAL_HANDLE_LOCAL_OPTIONS] =
    g_signal_new (I_("handle-local-options"), G_TYPE_APPLICATION, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GApplicationClass, handle_local_options),
                  g_application_handle_local_options_accumulator, NULL, NULL,
                  G_TYPE_INT, 1, G_TYPE_VARIANT_DICT);

}

/* Application ID validity {{{1 */

/**
 * g_application_id_is_valid:
 * @application_id: a potential application identifier
 *
 * Checks if @application_id is a valid application identifier.
 *
 * A valid ID is required for calls to g_application_new() and
 * g_application_set_application_id().
 *
 * Application identifiers follow the same format as
 * [D-Bus well-known bus names](https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names-bus).
 * For convenience, the restrictions on application identifiers are
 * reproduced here:
 *
 * - Application identifiers are composed of 1 or more elements separated by a
 *   period (`.`) character. All elements must contain at least one character.
 *
 * - Each element must only contain the ASCII characters `[A-Z][a-z][0-9]_-`,
 *   with `-` discouraged in new application identifiers. Each element must not
 *   begin with a digit.
 *
 * - Application identifiers must contain at least one `.` (period) character
 *   (and thus at least two elements).
 *
 * - Application identifiers must not begin with a `.` (period) character.
 *
 * - Application identifiers must not exceed 255 characters.
 *
 * Note that the hyphen (`-`) character is allowed in application identifiers,
 * but is problematic or not allowed in various specifications and APIs that
 * refer to D-Bus, such as
 * [Flatpak application IDs](http://docs.flatpak.org/en/latest/introduction.html#identifiers),
 * the
 * [`DBusActivatable` interface in the Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#dbus),
 * and the convention that an application's "main" interface and object path
 * resemble its application identifier and bus name. To avoid situations that
 * require special-case handling, it is recommended that new application
 * identifiers consistently replace hyphens with underscores.
 *
 * Like D-Bus interface names, application identifiers should start with the
 * reversed DNS domain name of the author of the interface (in lower-case), and
 * it is conventional for the rest of the application identifier to consist of
 * words run together, with initial capital letters.
 *
 * As with D-Bus interface names, if the author's DNS domain name contains
 * hyphen/minus characters they should be replaced by underscores, and if it
 * contains leading digits they should be escaped by prepending an underscore.
 * For example, if the owner of 7-zip.org used an application identifier for an
 * archiving application, it might be named `org._7_zip.Archiver`.
 *
 * Returns: %TRUE if @application_id is valid
 */
gboolean
g_application_id_is_valid (const gchar *application_id)
{
  return g_dbus_is_name (application_id) &&
         !g_dbus_is_unique_name (application_id);
}

/* Public Constructor {{{1 */
/**
 * g_application_new:
 * @application_id: (nullable): the application id
 * @flags: the application flags
 *
 * Creates a new #GApplication instance.
 *
 * If non-%NULL, the application id must be valid.  See
 * g_application_id_is_valid().
 *
 * If no application ID is given then some features of #GApplication
 * (most notably application uniqueness) will be disabled.
 *
 * Returns: a new #GApplication instance
 **/
GApplication *
g_application_new (const gchar       *application_id,
                   GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id == NULL || g_application_id_is_valid (application_id), NULL);

  return g_object_new (G_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

/* Simple get/set: application id, flags, inactivity timeout {{{1 */
/**
 * g_application_get_application_id:
 * @application: a #GApplication
 *
 * Gets the unique identifier for @application.
 *
 * Returns: the identifier for @application, owned by @application
 *
 * Since: 2.28
 **/
const gchar *
g_application_get_application_id (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), NULL);

  return application->priv->id;
}

/**
 * g_application_set_application_id:
 * @application: a #GApplication
 * @application_id: (nullable): the identifier for @application
 *
 * Sets the unique identifier for @application.
 *
 * The application id can only be modified if @application has not yet
 * been registered.
 *
 * If non-%NULL, the application id must be valid.  See
 * g_application_id_is_valid().
 *
 * Since: 2.28
 **/
void
g_application_set_application_id (GApplication *application,
                                  const gchar  *application_id)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  if (g_strcmp0 (application->priv->id, application_id) != 0)
    {
      g_return_if_fail (application_id == NULL || g_application_id_is_valid (application_id));
      g_return_if_fail (!application->priv->is_registered);

      g_free (application->priv->id);
      application->priv->id = g_strdup (application_id);

      g_object_notify (G_OBJECT (application), "application-id");
    }
}

/**
 * g_application_get_flags:
 * @application: a #GApplication
 *
 * Gets the flags for @application.
 *
 * See #GApplicationFlags.
 *
 * Returns: the flags for @application
 *
 * Since: 2.28
 **/
GApplicationFlags
g_application_get_flags (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), 0);

  return application->priv->flags;
}

/**
 * g_application_set_flags:
 * @application: a #GApplication
 * @flags: the flags for @application
 *
 * Sets the flags for @application.
 *
 * The flags can only be modified if @application has not yet been
 * registered.
 *
 * See #GApplicationFlags.
 *
 * Since: 2.28
 **/
void
g_application_set_flags (GApplication      *application,
                         GApplicationFlags  flags)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  if (application->priv->flags != flags)
    {
      g_return_if_fail (!application->priv->is_registered);

      application->priv->flags = flags;

      g_object_notify (G_OBJECT (application), "flags");
    }
}

/**
 * g_application_get_resource_base_path:
 * @application: a #GApplication
 *
 * Gets the resource base path of @application.
 *
 * See g_application_set_resource_base_path() for more information.
 *
 * Returns: (nullable): the base resource path, if one is set
 *
 * Since: 2.42
 */
const gchar *
g_application_get_resource_base_path (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), NULL);

  return application->priv->resource_path;
}

/**
 * g_application_set_resource_base_path:
 * @application: a #GApplication
 * @resource_path: (nullable): the resource path to use
 *
 * Sets (or unsets) the base resource path of @application.
 *
 * The path is used to automatically load various [application
 * resources][gresource] such as menu layouts and action descriptions.
 * The various types of resources will be found at fixed names relative
 * to the given base path.
 *
 * By default, the resource base path is determined from the application
 * ID by prefixing '/' and replacing each '.' with '/'.  This is done at
 * the time that the #GApplication object is constructed.  Changes to
 * the application ID after that point will not have an impact on the
 * resource base path.
 *
 * As an example, if the application has an ID of "org.example.app" then
 * the default resource base path will be "/org/example/app".  If this
 * is a #GtkApplication (and you have not manually changed the path)
 * then Gtk will then search for the menus of the application at
 * "/org/example/app/gtk/menus.ui".
 *
 * See #GResource for more information about adding resources to your
 * application.
 *
 * You can disable automatic resource loading functionality by setting
 * the path to %NULL.
 *
 * Changing the resource base path once the application is running is
 * not recommended.  The point at which the resource path is consulted
 * for forming paths for various purposes is unspecified.  When writing
 * a sub-class of #GApplication you should either set the
 * #GApplication:resource-base-path property at construction time, or call
 * this function during the instance initialization. Alternatively, you
 * can call this function in the #GApplicationClass.startup virtual function,
 * before chaining up to the parent implementation.
 *
 * Since: 2.42
 */
void
g_application_set_resource_base_path (GApplication *application,
                                      const gchar  *resource_path)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (resource_path == NULL || g_str_has_prefix (resource_path, "/"));

  if (g_strcmp0 (application->priv->resource_path, resource_path) != 0)
    {
      g_free (application->priv->resource_path);

      application->priv->resource_path = g_strdup (resource_path);

      g_object_notify (G_OBJECT (application), "resource-base-path");
    }
}

/**
 * g_application_get_inactivity_timeout:
 * @application: a #GApplication
 *
 * Gets the current inactivity timeout for the application.
 *
 * This is the amount of time (in milliseconds) after the last call to
 * g_application_release() before the application stops running.
 *
 * Returns: the timeout, in milliseconds
 *
 * Since: 2.28
 **/
guint
g_application_get_inactivity_timeout (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), 0);

  return application->priv->inactivity_timeout;
}

/**
 * g_application_set_inactivity_timeout:
 * @application: a #GApplication
 * @inactivity_timeout: the timeout, in milliseconds
 *
 * Sets the current inactivity timeout for the application.
 *
 * This is the amount of time (in milliseconds) after the last call to
 * g_application_release() before the application stops running.
 *
 * This call has no side effects of its own.  The value set here is only
 * used for next time g_application_release() drops the use count to
 * zero.  Any timeouts currently in progress are not impacted.
 *
 * Since: 2.28
 **/
void
g_application_set_inactivity_timeout (GApplication *application,
                                      guint         inactivity_timeout)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  if (application->priv->inactivity_timeout != inactivity_timeout)
    {
      application->priv->inactivity_timeout = inactivity_timeout;

      g_object_notify (G_OBJECT (application), "inactivity-timeout");
    }
}
/* Read-only property getters (is registered, is remote, dbus stuff) {{{1 */
/**
 * g_application_get_is_registered:
 * @application: a #GApplication
 *
 * Checks if @application is registered.
 *
 * An application is registered if g_application_register() has been
 * successfully called.
 *
 * Returns: %TRUE if @application is registered
 *
 * Since: 2.28
 **/
gboolean
g_application_get_is_registered (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);

  return application->priv->is_registered;
}

/**
 * g_application_get_is_remote:
 * @application: a #GApplication
 *
 * Checks if @application is remote.
 *
 * If @application is remote then it means that another instance of
 * application already exists (the 'primary' instance).  Calls to
 * perform actions on @application will result in the actions being
 * performed by the primary instance.
 *
 * The value of this property cannot be accessed before
 * g_application_register() has been called.  See
 * g_application_get_is_registered().
 *
 * Returns: %TRUE if @application is remote
 *
 * Since: 2.28
 **/
gboolean
g_application_get_is_remote (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (application->priv->is_registered, FALSE);

  return application->priv->is_remote;
}

/**
 * g_application_get_dbus_connection:
 * @application: a #GApplication
 *
 * Gets the #GDBusConnection being used by the application, or %NULL.
 *
 * If #GApplication is using its D-Bus backend then this function will
 * return the #GDBusConnection being used for uniqueness and
 * communication with the desktop environment and other instances of the
 * application.
 *
 * If #GApplication is not using D-Bus then this function will return
 * %NULL.  This includes the situation where the D-Bus backend would
 * normally be in use but we were unable to connect to the bus.
 *
 * This function must not be called before the application has been
 * registered.  See g_application_get_is_registered().
 *
 * Returns: (transfer none): a #GDBusConnection, or %NULL
 *
 * Since: 2.34
 **/
GDBusConnection *
g_application_get_dbus_connection (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (application->priv->is_registered, FALSE);

  return g_application_impl_get_dbus_connection (application->priv->impl);
}

/**
 * g_application_get_dbus_object_path:
 * @application: a #GApplication
 *
 * Gets the D-Bus object path being used by the application, or %NULL.
 *
 * If #GApplication is using its D-Bus backend then this function will
 * return the D-Bus object path that #GApplication is using.  If the
 * application is the primary instance then there is an object published
 * at this path.  If the application is not the primary instance then
 * the result of this function is undefined.
 *
 * If #GApplication is not using D-Bus then this function will return
 * %NULL.  This includes the situation where the D-Bus backend would
 * normally be in use but we were unable to connect to the bus.
 *
 * This function must not be called before the application has been
 * registered.  See g_application_get_is_registered().
 *
 * Returns: the object path, or %NULL
 *
 * Since: 2.34
 **/
const gchar *
g_application_get_dbus_object_path (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (application->priv->is_registered, FALSE);

  return g_application_impl_get_dbus_object_path (application->priv->impl);
}


/* Register {{{1 */
/**
 * g_application_register:
 * @application: a #GApplication
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a pointer to a NULL #GError, or %NULL
 *
 * Attempts registration of the application.
 *
 * This is the point at which the application discovers if it is the
 * primary instance or merely acting as a remote for an already-existing
 * primary instance.  This is implemented by attempting to acquire the
 * application identifier as a unique bus name on the session bus using
 * GDBus.
 *
 * If there is no application ID or if %G_APPLICATION_NON_UNIQUE was
 * given, then this process will always become the primary instance.
 *
 * Due to the internal architecture of GDBus, method calls can be
 * dispatched at any time (even if a main loop is not running).  For
 * this reason, you must ensure that any object paths that you wish to
 * register are registered before calling this function.
 *
 * If the application has already been registered then %TRUE is
 * returned with no work performed.
 *
 * The #GApplication::startup signal is emitted if registration succeeds
 * and @application is the primary instance (including the non-unique
 * case).
 *
 * In the event of an error (such as @cancellable being cancelled, or a
 * failure to connect to the session bus), %FALSE is returned and @error
 * is set appropriately.
 *
 * Note: the return value of this function is not an indicator that this
 * instance is or is not the primary instance of the application.  See
 * g_application_get_is_remote() for that.
 *
 * Returns: %TRUE if registration succeeded
 *
 * Since: 2.28
 **/
gboolean
g_application_register (GApplication  *application,
                        GCancellable  *cancellable,
                        GError       **error)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);

  if (!application->priv->is_registered)
    {
      if (application->priv->id == NULL)
        application->priv->flags |= G_APPLICATION_NON_UNIQUE;

      application->priv->impl =
        g_application_impl_register (application, application->priv->id,
                                     application->priv->flags,
                                     application->priv->actions,
                                     &application->priv->remote_actions,
                                     cancellable, error);

      if (application->priv->impl == NULL)
        return FALSE;

      application->priv->is_remote = application->priv->remote_actions != NULL;
      application->priv->is_registered = TRUE;

      g_object_notify (G_OBJECT (application), "is-registered");

      if (!application->priv->is_remote)
        {
          g_signal_emit (application, g_application_signals[SIGNAL_STARTUP], 0);

          if (!application->priv->did_startup)
            g_critical ("GApplication subclass '%s' failed to chain up on"
                        " ::startup (from start of override function)",
                        G_OBJECT_TYPE_NAME (application));
        }
    }

  return TRUE;
}

/* Hold/release {{{1 */
/**
 * g_application_hold:
 * @application: a #GApplication
 *
 * Increases the use count of @application.
 *
 * Use this function to indicate that the application has a reason to
 * continue to run.  For example, g_application_hold() is called by GTK+
 * when a toplevel window is on the screen.
 *
 * To cancel the hold, call g_application_release().
 **/
void
g_application_hold (GApplication *application)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  if (application->priv->inactivity_timeout_id)
    {
      g_source_remove (application->priv->inactivity_timeout_id);
      application->priv->inactivity_timeout_id = 0;
    }

  application->priv->use_count++;
}

static gboolean
inactivity_timeout_expired (gpointer data)
{
  GApplication *application = G_APPLICATION (data);

  application->priv->inactivity_timeout_id = 0;

  return G_SOURCE_REMOVE;
}


/**
 * g_application_release:
 * @application: a #GApplication
 *
 * Decrease the use count of @application.
 *
 * When the use count reaches zero, the application will stop running.
 *
 * Never call this function except to cancel the effect of a previous
 * call to g_application_hold().
 **/
void
g_application_release (GApplication *application)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (application->priv->use_count > 0);

  application->priv->use_count--;

  if (application->priv->use_count == 0 && application->priv->inactivity_timeout)
    application->priv->inactivity_timeout_id = g_timeout_add (application->priv->inactivity_timeout,
                                                              inactivity_timeout_expired, application);
}

/* Activate, Open {{{1 */
/**
 * g_application_activate:
 * @application: a #GApplication
 *
 * Activates the application.
 *
 * In essence, this results in the #GApplication::activate signal being
 * emitted in the primary instance.
 *
 * The application must be registered before calling this function.
 *
 * Since: 2.28
 **/
void
g_application_activate (GApplication *application)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (application->priv->is_registered);

  if (application->priv->is_remote)
    g_application_impl_activate (application->priv->impl,
                                 get_platform_data (application, NULL));

  else
    g_signal_emit (application, g_application_signals[SIGNAL_ACTIVATE], 0);
}

/**
 * g_application_open:
 * @application: a #GApplication
 * @files: (array length=n_files): an array of #GFiles to open
 * @n_files: the length of the @files array
 * @hint: a hint (or ""), but never %NULL
 *
 * Opens the given files.
 *
 * In essence, this results in the #GApplication::open signal being emitted
 * in the primary instance.
 *
 * @n_files must be greater than zero.
 *
 * @hint is simply passed through to the ::open signal.  It is
 * intended to be used by applications that have multiple modes for
 * opening files (eg: "view" vs "edit", etc).  Unless you have a need
 * for this functionality, you should use "".
 *
 * The application must be registered before calling this function
 * and it must have the %G_APPLICATION_HANDLES_OPEN flag set.
 *
 * Since: 2.28
 **/
void
g_application_open (GApplication  *application,
                    GFile        **files,
                    gint           n_files,
                    const gchar   *hint)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (application->priv->flags &
                    G_APPLICATION_HANDLES_OPEN);
  g_return_if_fail (application->priv->is_registered);

  if (application->priv->is_remote)
    g_application_impl_open (application->priv->impl,
                             files, n_files, hint,
                             get_platform_data (application, NULL));

  else
    g_signal_emit (application, g_application_signals[SIGNAL_OPEN],
                   0, files, n_files, hint);
}

/* Run {{{1 */
/**
 * g_application_run:
 * @application: a #GApplication
 * @argc: the argc from main() (or 0 if @argv is %NULL)
 * @argv: (array length=argc) (element-type filename) (nullable):
 *     the argv from main(), or %NULL
 *
 * Runs the application.
 *
 * This function is intended to be run from main() and its return value
 * is intended to be returned by main(). Although you are expected to pass
 * the @argc, @argv parameters from main() to this function, it is possible
 * to pass %NULL if @argv is not available or commandline handling is not
 * required.  Note that on Windows, @argc and @argv are ignored, and
 * g_win32_get_command_line() is called internally (for proper support
 * of Unicode commandline arguments).
 *
 * #GApplication will attempt to parse the commandline arguments.  You
 * can add commandline flags to the list of recognised options by way of
 * g_application_add_main_option_entries().  After this, the
 * #GApplication::handle-local-options signal is emitted, from which the
 * application can inspect the values of its #GOptionEntrys.
 *
 * #GApplication::handle-local-options is a good place to handle options
 * such as `--version`, where an immediate reply from the local process is
 * desired (instead of communicating with an already-running instance).
 * A #GApplication::handle-local-options handler can stop further processing
 * by returning a non-negative value, which then becomes the exit status of
 * the process.
 *
 * What happens next depends on the flags: if
 * %G_APPLICATION_HANDLES_COMMAND_LINE was specified then the remaining
 * commandline arguments are sent to the primary instance, where a
 * #GApplication::command-line signal is emitted.  Otherwise, the
 * remaining commandline arguments are assumed to be a list of files.
 * If there are no files listed, the application is activated via the
 * #GApplication::activate signal.  If there are one or more files, and
 * %G_APPLICATION_HANDLES_OPEN was specified then the files are opened
 * via the #GApplication::open signal.
 *
 * If you are interested in doing more complicated local handling of the
 * commandline then you should implement your own #GApplication subclass
 * and override local_command_line(). In this case, you most likely want
 * to return %TRUE from your local_command_line() implementation to
 * suppress the default handling. See
 * [gapplication-example-cmdline2.c][gapplication-example-cmdline2]
 * for an example.
 *
 * If, after the above is done, the use count of the application is zero
 * then the exit status is returned immediately.  If the use count is
 * non-zero then the default main context is iterated until the use count
 * falls to zero, at which point 0 is returned.
 *
 * If the %G_APPLICATION_IS_SERVICE flag is set, then the service will
 * run for as much as 10 seconds with a use count of zero while waiting
 * for the message that caused the activation to arrive.  After that,
 * if the use count falls to zero the application will exit immediately,
 * except in the case that g_application_set_inactivity_timeout() is in
 * use.
 *
 * This function sets the prgname (g_set_prgname()), if not already set,
 * to the basename of argv[0].
 *
 * Much like g_main_loop_run(), this function will acquire the main context
 * for the duration that the application is running.
 *
 * Since 2.40, applications that are not explicitly flagged as services
 * or launchers (ie: neither %G_APPLICATION_IS_SERVICE or
 * %G_APPLICATION_IS_LAUNCHER are given as flags) will check (from the
 * default handler for local_command_line) if "--gapplication-service"
 * was given in the command line.  If this flag is present then normal
 * commandline processing is interrupted and the
 * %G_APPLICATION_IS_SERVICE flag is set.  This provides a "compromise"
 * solution whereby running an application directly from the commandline
 * will invoke it in the normal way (which can be useful for debugging)
 * while still allowing applications to be D-Bus activated in service
 * mode.  The D-Bus service file should invoke the executable with
 * "--gapplication-service" as the sole commandline argument.  This
 * approach is suitable for use by most graphical applications but
 * should not be used from applications like editors that need precise
 * control over when processes invoked via the commandline will exit and
 * what their exit status will be.
 *
 * Returns: the exit status
 *
 * Since: 2.28
 **/
int
g_application_run (GApplication  *application,
                   int            argc,
                   char         **argv)
{
  gchar **arguments;
  int status;
  GMainContext *context;
  gboolean acquired_context;

  g_return_val_if_fail (G_IS_APPLICATION (application), 1);
  g_return_val_if_fail (argc == 0 || argv != NULL, 1);
  g_return_val_if_fail (!application->priv->must_quit_now, 1);

#ifdef G_OS_WIN32
  {
    gint new_argc = 0;

    arguments = g_win32_get_command_line ();

    /*
     * CommandLineToArgvW(), which is called by g_win32_get_command_line(),
     * pulls in the whole command line that is used to call the program.  This is
     * fine in cases where the program is a .exe program, but in the cases where the
     * program is a called via a script, such as PyGObject's gtk-demo.py, which is normally
     * called using 'python gtk-demo.py' on Windows, the program name (argv[0])
     * returned by g_win32_get_command_line() will not be the argv[0] that ->local_command_line()
     * would expect, causing the program to fail with "This application can not open files."
     */
    new_argc = g_strv_length (arguments);

    if (new_argc > argc)
      {
        gint i;

        for (i = 0; i < new_argc - argc; i++)
          g_free (arguments[i]);

        memmove (&arguments[0],
                 &arguments[new_argc - argc],
                 sizeof (arguments[0]) * (argc + 1));
      }
  }
#else
  {
    gint i;

    arguments = g_new (gchar *, argc + 1);
    for (i = 0; i < argc; i++)
      arguments[i] = g_strdup (argv[i]);
    arguments[i] = NULL;
  }
#endif

  if (g_get_prgname () == NULL && argc > 0)
    {
      gchar *prgname;

      prgname = g_path_get_basename (argv[0]);
      g_set_prgname (prgname);
      g_free (prgname);
    }

  context = g_main_context_default ();
  acquired_context = g_main_context_acquire (context);
  g_return_val_if_fail (acquired_context, 0);

  if (!G_APPLICATION_GET_CLASS (application)
        ->local_command_line (application, &arguments, &status))
    {
      GError *error = NULL;

      if (!g_application_register (application, NULL, &error))
        {
          g_printerr ("Failed to register: %s\n", error->message);
          g_error_free (error);
          return 1;
        }

      g_application_call_command_line (application, (const gchar **) arguments, NULL, &status);
    }

  g_strfreev (arguments);

  if (application->priv->flags & G_APPLICATION_IS_SERVICE &&
      application->priv->is_registered &&
      !application->priv->use_count &&
      !application->priv->inactivity_timeout_id)
    {
      application->priv->inactivity_timeout_id =
        g_timeout_add (10000, inactivity_timeout_expired, application);
    }

  while (application->priv->use_count || application->priv->inactivity_timeout_id)
    {
      if (application->priv->must_quit_now)
        break;

      g_main_context_iteration (context, TRUE);
      status = 0;
    }

  if (application->priv->is_registered && !application->priv->is_remote)
    {
      g_signal_emit (application, g_application_signals[SIGNAL_SHUTDOWN], 0);

      if (!application->priv->did_shutdown)
        g_critical ("GApplication subclass '%s' failed to chain up on"
                    " ::shutdown (from end of override function)",
                    G_OBJECT_TYPE_NAME (application));
    }

  if (application->priv->impl)
    {
      g_application_impl_flush (application->priv->impl);
      g_application_impl_destroy (application->priv->impl);
      application->priv->impl = NULL;
    }

  g_settings_sync ();

  if (!application->priv->must_quit_now)
    while (g_main_context_iteration (context, FALSE))
      ;

  g_main_context_release (context);

  return status;
}

static gchar **
g_application_list_actions (GActionGroup *action_group)
{
  GApplication *application = G_APPLICATION (action_group);

  g_return_val_if_fail (application->priv->is_registered, NULL);

  if (application->priv->remote_actions != NULL)
    return g_action_group_list_actions (G_ACTION_GROUP (application->priv->remote_actions));

  else if (application->priv->actions != NULL)
    return g_action_group_list_actions (application->priv->actions);

  else
    /* empty string array */
    return g_new0 (gchar *, 1);
}

static gboolean
g_application_query_action (GActionGroup        *group,
                            const gchar         *action_name,
                            gboolean            *enabled,
                            const GVariantType **parameter_type,
                            const GVariantType **state_type,
                            GVariant           **state_hint,
                            GVariant           **state)
{
  GApplication *application = G_APPLICATION (group);

  g_return_val_if_fail (application->priv->is_registered, FALSE);

  if (application->priv->remote_actions != NULL)
    return g_action_group_query_action (G_ACTION_GROUP (application->priv->remote_actions),
                                        action_name,
                                        enabled,
                                        parameter_type,
                                        state_type,
                                        state_hint,
                                        state);

  if (application->priv->actions != NULL)
    return g_action_group_query_action (application->priv->actions,
                                        action_name,
                                        enabled,
                                        parameter_type,
                                        state_type,
                                        state_hint,
                                        state);

  return FALSE;
}

static void
g_application_change_action_state (GActionGroup *action_group,
                                   const gchar  *action_name,
                                   GVariant     *value)
{
  GApplication *application = G_APPLICATION (action_group);

  g_return_if_fail (application->priv->is_remote ||
                    application->priv->actions != NULL);
  g_return_if_fail (application->priv->is_registered);

  if (application->priv->remote_actions)
    g_remote_action_group_change_action_state_full (application->priv->remote_actions,
                                                    action_name, value, get_platform_data (application, NULL));

  else
    g_action_group_change_action_state (application->priv->actions, action_name, value);
}

static void
g_application_activate_action (GActionGroup *action_group,
                               const gchar  *action_name,
                               GVariant     *parameter)
{
  GApplication *application = G_APPLICATION (action_group);

  g_return_if_fail (application->priv->is_remote ||
                    application->priv->actions != NULL);
  g_return_if_fail (application->priv->is_registered);

  if (application->priv->remote_actions)
    g_remote_action_group_activate_action_full (application->priv->remote_actions,
                                                action_name, parameter, get_platform_data (application, NULL));

  else
    g_action_group_activate_action (application->priv->actions, action_name, parameter);
}

static GAction *
g_application_lookup_action (GActionMap  *action_map,
                             const gchar *action_name)
{
  GApplication *application = G_APPLICATION (action_map);

  g_return_val_if_fail (G_IS_ACTION_MAP (application->priv->actions), NULL);

  return g_action_map_lookup_action (G_ACTION_MAP (application->priv->actions), action_name);
}

static void
g_application_add_action (GActionMap *action_map,
                          GAction    *action)
{
  GApplication *application = G_APPLICATION (action_map);

  g_return_if_fail (G_IS_ACTION_MAP (application->priv->actions));

  g_action_map_add_action (G_ACTION_MAP (application->priv->actions), action);
}

static void
g_application_remove_action (GActionMap  *action_map,
                             const gchar *action_name)
{
  GApplication *application = G_APPLICATION (action_map);

  g_return_if_fail (G_IS_ACTION_MAP (application->priv->actions));

  g_action_map_remove_action (G_ACTION_MAP (application->priv->actions), action_name);
}

static void
g_application_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_application_list_actions;
  iface->query_action = g_application_query_action;
  iface->change_action_state = g_application_change_action_state;
  iface->activate_action = g_application_activate_action;
}

static void
g_application_action_map_iface_init (GActionMapInterface *iface)
{
  iface->lookup_action = g_application_lookup_action;
  iface->add_action = g_application_add_action;
  iface->remove_action = g_application_remove_action;
}

/* Default Application {{{1 */

static GApplication *default_app;

/**
 * g_application_get_default:
 *
 * Returns the default #GApplication instance for this process.
 *
 * Normally there is only one #GApplication per process and it becomes
 * the default when it is created.  You can exercise more control over
 * this by using g_application_set_default().
 *
 * If there is no default application then %NULL is returned.
 *
 * Returns: (transfer none): the default application for this process, or %NULL
 *
 * Since: 2.32
 **/
GApplication *
g_application_get_default (void)
{
  return default_app;
}

/**
 * g_application_set_default:
 * @application: (nullable): the application to set as default, or %NULL
 *
 * Sets or unsets the default application for the process, as returned
 * by g_application_get_default().
 *
 * This function does not take its own reference on @application.  If
 * @application is destroyed then the default application will revert
 * back to %NULL.
 *
 * Since: 2.32
 **/
void
g_application_set_default (GApplication *application)
{
  default_app = application;
}

/**
 * g_application_quit:
 * @application: a #GApplication
 *
 * Immediately quits the application.
 *
 * Upon return to the mainloop, g_application_run() will return,
 * calling only the 'shutdown' function before doing so.
 *
 * The hold count is ignored.
 * Take care if your code has called g_application_hold() on the application and
 * is therefore still expecting it to exist.
 * (Note that you may have called g_application_hold() indirectly, for example
 * through gtk_application_add_window().)
 *
 * The result of calling g_application_run() again after it returns is
 * unspecified.
 *
 * Since: 2.32
 **/
void
g_application_quit (GApplication *application)
{
  g_return_if_fail (G_IS_APPLICATION (application));

  application->priv->must_quit_now = TRUE;
}

/**
 * g_application_mark_busy:
 * @application: a #GApplication
 *
 * Increases the busy count of @application.
 *
 * Use this function to indicate that the application is busy, for instance
 * while a long running operation is pending.
 *
 * The busy state will be exposed to other processes, so a session shell will
 * use that information to indicate the state to the user (e.g. with a
 * spinner).
 *
 * To cancel the busy indication, use g_application_unmark_busy().
 *
 * Since: 2.38
 **/
void
g_application_mark_busy (GApplication *application)
{
  gboolean was_busy;

  g_return_if_fail (G_IS_APPLICATION (application));

  was_busy = (application->priv->busy_count > 0);
  application->priv->busy_count++;

  if (!was_busy)
    {
      g_application_impl_set_busy_state (application->priv->impl, TRUE);
      g_object_notify (G_OBJECT (application), "is-busy");
    }
}

/**
 * g_application_unmark_busy:
 * @application: a #GApplication
 *
 * Decreases the busy count of @application.
 *
 * When the busy count reaches zero, the new state will be propagated
 * to other processes.
 *
 * This function must only be called to cancel the effect of a previous
 * call to g_application_mark_busy().
 *
 * Since: 2.38
 **/
void
g_application_unmark_busy (GApplication *application)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (application->priv->busy_count > 0);

  application->priv->busy_count--;

  if (application->priv->busy_count == 0)
    {
      g_application_impl_set_busy_state (application->priv->impl, FALSE);
      g_object_notify (G_OBJECT (application), "is-busy");
    }
}

/**
 * g_application_get_is_busy:
 * @application: a #GApplication
 *
 * Gets the application's current busy state, as set through
 * g_application_mark_busy() or g_application_bind_busy_property().
 *
 * Returns: %TRUE if @application is currenty marked as busy
 *
 * Since: 2.44
 */
gboolean
g_application_get_is_busy (GApplication *application)
{
  g_return_val_if_fail (G_IS_APPLICATION (application), FALSE);

  return application->priv->busy_count > 0;
}

/* Notifications {{{1 */

/**
 * g_application_send_notification:
 * @application: a #GApplication
 * @id: (nullable): id of the notification, or %NULL
 * @notification: the #GNotification to send
 *
 * Sends a notification on behalf of @application to the desktop shell.
 * There is no guarantee that the notification is displayed immediately,
 * or even at all.
 *
 * Notifications may persist after the application exits. It will be
 * D-Bus-activated when the notification or one of its actions is
 * activated.
 *
 * Modifying @notification after this call has no effect. However, the
 * object can be reused for a later call to this function.
 *
 * @id may be any string that uniquely identifies the event for the
 * application. It does not need to be in any special format. For
 * example, "new-message" might be appropriate for a notification about
 * new messages.
 *
 * If a previous notification was sent with the same @id, it will be
 * replaced with @notification and shown again as if it was a new
 * notification. This works even for notifications sent from a previous
 * execution of the application, as long as @id is the same string.
 *
 * @id may be %NULL, but it is impossible to replace or withdraw
 * notifications without an id.
 *
 * If @notification is no longer relevant, it can be withdrawn with
 * g_application_withdraw_notification().
 *
 * Since: 2.40
 */
void
g_application_send_notification (GApplication  *application,
                                 const gchar   *id,
                                 GNotification *notification)
{
  gchar *generated_id = NULL;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (G_IS_NOTIFICATION (notification));
  g_return_if_fail (g_application_get_is_registered (application));
  g_return_if_fail (!g_application_get_is_remote (application));

  if (application->priv->notifications == NULL)
    application->priv->notifications = g_notification_backend_new_default (application);

  if (id == NULL)
    {
      generated_id = g_dbus_generate_guid ();
      id = generated_id;
    }

  g_notification_backend_send_notification (application->priv->notifications, id, notification);

  g_free (generated_id);
}

/**
 * g_application_withdraw_notification:
 * @application: a #GApplication
 * @id: id of a previously sent notification
 *
 * Withdraws a notification that was sent with
 * g_application_send_notification().
 *
 * This call does nothing if a notification with @id doesn't exist or
 * the notification was never sent.
 *
 * This function works even for notifications sent in previous
 * executions of this application, as long @id is the same as it was for
 * the sent notification.
 *
 * Note that notifications are dismissed when the user clicks on one
 * of the buttons in a notification or triggers its default action, so
 * there is no need to explicitly withdraw the notification in that case.
 *
 * Since: 2.40
 */
void
g_application_withdraw_notification (GApplication *application,
                                     const gchar  *id)
{
  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (id != NULL);

  if (application->priv->notifications == NULL)
    application->priv->notifications = g_notification_backend_new_default (application);

  g_notification_backend_withdraw_notification (application->priv->notifications, id);
}

/* Busy binding {{{1 */

typedef struct
{
  GApplication *app;
  gboolean is_busy;
} GApplicationBusyBinding;

static void
g_application_busy_binding_destroy (gpointer  data,
                                    GClosure *closure)
{
  GApplicationBusyBinding *binding = data;

  if (binding->is_busy)
    g_application_unmark_busy (binding->app);

  g_object_unref (binding->app);
  g_slice_free (GApplicationBusyBinding, binding);
}

static void
g_application_notify_busy_binding (GObject    *object,
                                   GParamSpec *pspec,
                                   gpointer    user_data)
{
  GApplicationBusyBinding *binding = user_data;
  gboolean is_busy;

  g_object_get (object, pspec->name, &is_busy, NULL);

  if (is_busy && !binding->is_busy)
    g_application_mark_busy (binding->app);
  else if (!is_busy && binding->is_busy)
    g_application_unmark_busy (binding->app);

  binding->is_busy = is_busy;
}

/**
 * g_application_bind_busy_property:
 * @application: a #GApplication
 * @object: (type GObject.Object): a #GObject
 * @property: the name of a boolean property of @object
 *
 * Marks @application as busy (see g_application_mark_busy()) while
 * @property on @object is %TRUE.
 *
 * The binding holds a reference to @application while it is active, but
 * not to @object. Instead, the binding is destroyed when @object is
 * finalized.
 *
 * Since: 2.44
 */
void
g_application_bind_busy_property (GApplication *application,
                                  gpointer      object,
                                  const gchar  *property)
{
  guint notify_id;
  GQuark property_quark;
  GParamSpec *pspec;
  GApplicationBusyBinding *binding;
  GClosure *closure;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  notify_id = g_signal_lookup ("notify", G_TYPE_OBJECT);
  property_quark = g_quark_from_string (property);
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), property);

  g_return_if_fail (pspec != NULL && pspec->value_type == G_TYPE_BOOLEAN);

  if (g_signal_handler_find (object, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DETAIL | G_SIGNAL_MATCH_FUNC,
                             notify_id, property_quark, NULL, g_application_notify_busy_binding, NULL) > 0)
    {
      g_critical ("%s: '%s' is already bound to the busy state of the application", G_STRFUNC, property);
      return;
    }

  binding = g_slice_new (GApplicationBusyBinding);
  binding->app = g_object_ref (application);
  binding->is_busy = FALSE;

  closure = g_cclosure_new (G_CALLBACK (g_application_notify_busy_binding), binding,
                            g_application_busy_binding_destroy);
  g_signal_connect_closure_by_id (object, notify_id, property_quark, closure, FALSE);

  /* fetch the initial value */
  g_application_notify_busy_binding (object, pspec, binding);
}

/**
 * g_application_unbind_busy_property:
 * @application: a #GApplication
 * @object: (type GObject.Object): a #GObject
 * @property: the name of a boolean property of @object
 *
 * Destroys a binding between @property and the busy state of
 * @application that was previously created with
 * g_application_bind_busy_property().
 *
 * Since: 2.44
 */
void
g_application_unbind_busy_property (GApplication *application,
                                    gpointer      object,
                                    const gchar  *property)
{
  guint notify_id;
  GQuark property_quark;
  gulong handler_id;

  g_return_if_fail (G_IS_APPLICATION (application));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  notify_id = g_signal_lookup ("notify", G_TYPE_OBJECT);
  property_quark = g_quark_from_string (property);

  handler_id = g_signal_handler_find (object, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DETAIL | G_SIGNAL_MATCH_FUNC,
                                      notify_id, property_quark, NULL, g_application_notify_busy_binding, NULL);
  if (handler_id == 0)
    {
      g_critical ("%s: '%s' is not bound to the busy state of the application", G_STRFUNC, property);
      return;
    }

  g_signal_handler_disconnect (object, handler_id);
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
