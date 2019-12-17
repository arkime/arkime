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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include <gio/gio.h>
#include <gi18n.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>

#ifdef G_OS_WIN32
#include "glib/glib-private.h"
#endif

static GSettingsSchemaSource   *global_schema_source;
static GSettings               *global_settings;
static GSettingsSchema         *global_schema;
static GSettingsSchemaKey      *global_schema_key;
const gchar                    *global_key;
const gchar                    *global_value;

static gboolean
is_relocatable_schema (GSettingsSchema *schema)
{
  return g_settings_schema_get_path (schema) == NULL;
}

static gboolean
check_relocatable_schema (GSettingsSchema *schema,
                          const gchar     *schema_id)
{
  if (schema == NULL)
    {
      g_printerr (_("No such schema “%s”\n"), schema_id);
      return FALSE;
    }

  if (!is_relocatable_schema (schema))
    {
      g_printerr (_("Schema “%s” is not relocatable "
                    "(path must not be specified)\n"),
                  schema_id);
      return FALSE;
    }

  return TRUE;
}

static gboolean
check_schema (GSettingsSchema *schema,
              const gchar *schema_id)
{
  if (schema == NULL)
    {
      g_printerr (_("No such schema “%s”\n"), schema_id);
      return FALSE;
    }

  if (is_relocatable_schema (schema))
    {
      g_printerr (_("Schema “%s” is relocatable "
                    "(path must be specified)\n"),
                  schema_id);
      return FALSE;
    }

  return TRUE;
}

static gboolean
check_path (const gchar *path)
{
  if (path[0] == '\0')
    {
      g_printerr (_("Empty path given.\n"));
      return FALSE;
    }

  if (path[0] != '/')
    {
      g_printerr (_("Path must begin with a slash (/)\n"));
      return FALSE;
    }

  if (!g_str_has_suffix (path, "/"))
    {
      g_printerr (_("Path must end with a slash (/)\n"));
      return FALSE;
    }

  if (strstr (path, "//"))
    {
      g_printerr (_("Path must not contain two adjacent slashes (//)\n"));
      return FALSE;
    }

  return TRUE;
}

static void
output_list (gchar **list)
{
  gint i;

  for (i = 0; list[i]; i++)
    g_print ("%s\n", list[i]);
}

static void
gsettings_print_version (void)
{
  g_print ("%d.%d.%d\n", glib_major_version, glib_minor_version,
           glib_micro_version);
}

static void
gsettings_list_schemas (void)
{
  gchar **schemas;

  g_settings_schema_source_list_schemas (global_schema_source, TRUE, &schemas, NULL);
  output_list (schemas);
  g_strfreev (schemas);
}

static void
gsettings_list_schemas_with_paths (void)
{
  gchar **schemas;
  gsize i;

  g_settings_schema_source_list_schemas (global_schema_source, TRUE, &schemas, NULL);

  for (i = 0; schemas[i] != NULL; i++)
    {
      GSettingsSchema *schema;
      gchar *schema_name;
      const gchar *schema_path;

      schema_name = g_steal_pointer (&schemas[i]);

      schema = g_settings_schema_source_lookup (global_schema_source, schema_name, TRUE);
      schema_path = g_settings_schema_get_path (schema);

      schemas[i] = g_strconcat (schema_name, " ", schema_path, NULL);

      g_settings_schema_unref (schema);
      g_free (schema_name);
    }

  output_list (schemas);
  g_strfreev (schemas);
}

static void
gsettings_list_relocatable_schemas (void)
{
  gchar **schemas;

  g_settings_schema_source_list_schemas (global_schema_source, TRUE, NULL, &schemas);
  output_list (schemas);
  g_strfreev (schemas);
}

static void
gsettings_list_keys (void)
{
  gchar **keys;

  keys = g_settings_schema_list_keys (global_schema);
  output_list (keys);
  g_strfreev (keys);
}

static void
gsettings_list_children (void)
{
  gchar **children;
  gint max = 0;
  gint i;

  children = g_settings_list_children (global_settings);
  for (i = 0; children[i]; i++)
    if (strlen (children[i]) > max)
      max = strlen (children[i]);

  for (i = 0; children[i]; i++)
    {
      GSettings *child;
      GSettingsSchema *schema;
      gchar *path;

      child = g_settings_get_child (global_settings, children[i]);
      g_object_get (child,
                    "settings-schema", &schema,
                    "path", &path,
                    NULL);

      if (g_settings_schema_get_path (schema) != NULL)
        g_print ("%-*s   %s\n", max, children[i], g_settings_schema_get_id (schema));
      else
        g_print ("%-*s   %s:%s\n", max, children[i], g_settings_schema_get_id (schema), path);

      g_object_unref (child);
      g_settings_schema_unref (schema);
      g_free (path);
    }

  g_strfreev (children);
}

static void
enumerate (GSettings *settings)
{
  gchar **keys;
  GSettingsSchema *schema;
  gint i;

  g_object_get (settings, "settings-schema", &schema, NULL);

  keys = g_settings_schema_list_keys (schema);
  for (i = 0; keys[i]; i++)
    {
      GVariant *value;
      gchar *printed;

      value = g_settings_get_value (settings, keys[i]);
      printed = g_variant_print (value, TRUE);
      g_print ("%s %s %s\n", g_settings_schema_get_id (schema), keys[i], printed);
      g_variant_unref (value);
      g_free (printed);
    }

  g_settings_schema_unref (schema);
  g_strfreev (keys);
}

static void
list_recursively (GSettings *settings)
{
  gchar **children;
  gint i;

  enumerate (settings);
  children = g_settings_list_children (settings);
  for (i = 0; children[i]; i++)
    {
      gboolean will_see_elsewhere = FALSE;
      GSettings *child;

      child = g_settings_get_child (settings, children[i]);

      if (global_settings == NULL)
        {
	  /* we're listing all non-relocatable settings objects from the
	   * top-level, so if this one is non-relocatable, don't recurse,
	   * because we will pick it up later on.
	   */

	  GSettingsSchema *child_schema;

	  g_object_get (child, "settings-schema", &child_schema, NULL);
	  will_see_elsewhere = !is_relocatable_schema (child_schema);
	  g_settings_schema_unref (child_schema);
        }

      if (!will_see_elsewhere)
        list_recursively (child);

      g_object_unref (child);
    }

  g_strfreev (children);
}

static void
gsettings_list_recursively (void)
{
  if (global_settings)
    {
      list_recursively (global_settings);
    }
  else
    {
      gchar **schemas;
      gint i;

      g_settings_schema_source_list_schemas (global_schema_source, TRUE, &schemas, NULL);

      for (i = 0; schemas[i]; i++)
        {
          GSettings *settings;

          settings = g_settings_new (schemas[i]);
          list_recursively (settings);
          g_object_unref (settings);
        }

      g_strfreev (schemas);
    }
}

static void
gsettings_description (void)
{
  const gchar *description;
  description = g_settings_schema_key_get_description (global_schema_key);
  if (description == NULL)
    description = g_settings_schema_key_get_summary (global_schema_key);
  g_print ("%s\n", description);
}

static void
gsettings_range (void)
{
  GVariant *range, *detail;
  const gchar *type;

  range = g_settings_schema_key_get_range (global_schema_key);
  g_variant_get (range, "(&sv)", &type, &detail);

  if (strcmp (type, "type") == 0)
    g_print ("type %s\n", g_variant_get_type_string (detail) + 1);

  else if (strcmp (type, "range") == 0)
    {
      GVariant *min, *max;
      gchar *smin, *smax;

      g_variant_get (detail, "(**)", &min, &max);
      smin = g_variant_print (min, FALSE);
      smax = g_variant_print (max, FALSE);

      g_print ("range %s %s %s\n",
               g_variant_get_type_string (min), smin, smax);
      g_variant_unref (min);
      g_variant_unref (max);
      g_free (smin);
      g_free (smax);
    }

  else if (strcmp (type, "enum") == 0 || strcmp (type, "flags") == 0)
    {
      GVariantIter iter;
      GVariant *item;

      g_print ("%s\n", type);

      g_variant_iter_init (&iter, detail);
      while (g_variant_iter_loop (&iter, "*", &item))
        {
          gchar *printed;

          printed = g_variant_print (item, FALSE);
          g_print ("%s\n", printed);
          g_free (printed);
        }
    }

  g_variant_unref (detail);
  g_variant_unref (range);
}

static void
gsettings_get (void)
{
  GVariant *value;
  gchar *printed;

  value = g_settings_get_value (global_settings, global_key);
  printed = g_variant_print (value, TRUE);
  g_print ("%s\n", printed);
  g_variant_unref (value);
  g_free (printed);
}

static void
gsettings_reset (void)
{
  g_settings_reset (global_settings, global_key);
  g_settings_sync ();
}

static void
reset_all_keys (GSettings *settings)
{
  GSettingsSchema *schema;
  gchar **keys;
  gint i;

  g_object_get (settings, "settings-schema", &schema, NULL);

  keys = g_settings_schema_list_keys (schema);
  for (i = 0; keys[i]; i++)
    {
      g_settings_reset (settings, keys[i]);
    }

  g_settings_schema_unref (schema);
  g_strfreev (keys);
}

static void
gsettings_reset_recursively (void)
{
  gchar **children;
  gint i;

  g_settings_delay (global_settings);

  reset_all_keys (global_settings);
  children = g_settings_list_children (global_settings);
  for (i = 0; children[i]; i++)
    {
      GSettings *child;
      child = g_settings_get_child (global_settings, children[i]);

      reset_all_keys (child);

      g_object_unref (child);
    }

  g_strfreev (children);

  g_settings_apply (global_settings);
  g_settings_sync ();
}

static void
gsettings_writable (void)
{
  g_print ("%s\n",
           g_settings_is_writable (global_settings, global_key) ?
           "true" : "false");
}

static void
value_changed (GSettings   *settings,
               const gchar *key,
               gpointer     user_data)
{
  GVariant *value;
  gchar *printed;

  value = g_settings_get_value (settings, key);
  printed = g_variant_print (value, TRUE);
  g_print ("%s: %s\n", key, printed);
  g_variant_unref (value);
  g_free (printed);
}

static void
gsettings_monitor (void)
{
  if (global_key)
    {
      gchar *name;

      name = g_strdup_printf ("changed::%s", global_key);
      g_signal_connect (global_settings, name, G_CALLBACK (value_changed), NULL);
    }
  else
    g_signal_connect (global_settings, "changed", G_CALLBACK (value_changed), NULL);

  for (;;)
    g_main_context_iteration (NULL, TRUE);
}

static void
gsettings_set (void)
{
  const GVariantType *type;
  GError *error = NULL;
  GVariant *new;
  gchar *freeme = NULL;

  type = g_settings_schema_key_get_value_type (global_schema_key);

  new = g_variant_parse (type, global_value, NULL, NULL, &error);

  /* If that didn't work and the type is string then we should assume
   * that the user is just trying to set a string directly and forgot
   * the quotes (or had them consumed by the shell).
   *
   * If the user started with a quote then we assume that some deeper
   * problem is at play and we want the failure in that case.
   *
   * Consider:
   *
   *   gsettings set x.y.z key "'i don't expect this to work'"
   *
   * Note that we should not just add quotes and try parsing again, but
   * rather assume that the user is providing us with a bare string.
   * Assume we added single quotes, then consider this case:
   *
   *   gsettings set x.y.z key "i'd expect this to work"
   *
   * A similar example could be given for double quotes.
   *
   * Avoid that whole mess by just using g_variant_new_string().
   */
  if (new == NULL &&
      g_variant_type_equal (type, G_VARIANT_TYPE_STRING) &&
      global_value[0] != '\'' && global_value[0] != '"')
    {
      g_clear_error (&error);
      new = g_variant_new_string (global_value);
    }

  if (new == NULL)
    {
      gchar *context;

      context = g_variant_parse_error_print_context (error, global_value);
      g_printerr ("%s", context);
      exit (1);
    }

  if (!g_settings_schema_key_range_check (global_schema_key, new))
    {
      g_printerr (_("The provided value is outside of the valid range\n"));
      g_variant_unref (new);
      exit (1);
    }

  if (!g_settings_set_value (global_settings, global_key, new))
    {
      g_printerr (_("The key is not writable\n"));
      exit (1);
    }

  g_settings_sync ();

  g_free (freeme);
}

static int
gsettings_help (gboolean     requested,
                const gchar *command)
{
  const gchar *description;
  const gchar *synopsis;
  GString *string;

  string = g_string_new (NULL);

  if (command == NULL)
    ;

  else if (strcmp (command, "help") == 0)
    {
      description = _("Print help");
      synopsis = "[COMMAND]";
    }

  else if (strcmp (command, "--version") == 0)
    {
      description = _("Print version information and exit");
      synopsis = "";
    }

  else if (strcmp (command, "list-schemas") == 0)
    {
      description = _("List the installed (non-relocatable) schemas");
      synopsis = "[--print-paths]";
    }

  else if (strcmp (command, "list-relocatable-schemas") == 0)
    {
      description = _("List the installed relocatable schemas");
      synopsis = "";
    }

  else if (strcmp (command, "list-keys") == 0)
    {
      description = _("List the keys in SCHEMA");
      synopsis = N_("SCHEMA[:PATH]");
    }

  else if (strcmp (command, "list-children") == 0)
    {
      description = _("List the children of SCHEMA");
      synopsis = N_("SCHEMA[:PATH]");
    }

  else if (strcmp (command, "list-recursively") == 0)
    {
      description = _("List keys and values, recursively\n"
                      "If no SCHEMA is given, list all keys\n");
      synopsis = N_("[SCHEMA[:PATH]]");
    }

  else if (strcmp (command, "get") == 0)
    {
      description = _("Get the value of KEY");
      synopsis = N_("SCHEMA[:PATH] KEY");
    }

  else if (strcmp (command, "range") == 0)
    {
      description = _("Query the range of valid values for KEY");
      synopsis = N_("SCHEMA[:PATH] KEY");
    }

  else if (strcmp (command, "describe") == 0)
    {
      description = _("Query the description for KEY");
      synopsis = N_("SCHEMA[:PATH] KEY");
    }

  else if (strcmp (command, "set") == 0)
    {
      description = _("Set the value of KEY to VALUE");
      synopsis = N_("SCHEMA[:PATH] KEY VALUE");
    }

  else if (strcmp (command, "reset") == 0)
    {
      description = _("Reset KEY to its default value");
      synopsis = N_("SCHEMA[:PATH] KEY");
    }

  else if (strcmp (command, "reset-recursively") == 0)
    {
      description = _("Reset all keys in SCHEMA to their defaults");
      synopsis = N_("SCHEMA[:PATH]");
    }

  else if (strcmp (command, "writable") == 0)
    {
      description = _("Check if KEY is writable");
      synopsis = N_("SCHEMA[:PATH] KEY");
    }

  else if (strcmp (command, "monitor") == 0)
    {
      description = _("Monitor KEY for changes.\n"
                    "If no KEY is specified, monitor all keys in SCHEMA.\n"
                    "Use ^C to stop monitoring.\n");
      synopsis = N_("SCHEMA[:PATH] [KEY]");
    }
  else
    {
      g_string_printf (string, _("Unknown command %s\n\n"), command);
      requested = FALSE;
      command = NULL;
    }

  if (command == NULL)
    {
      g_string_append (string,
      _("Usage:\n"
        "  gsettings --version\n"
        "  gsettings [--schemadir SCHEMADIR] COMMAND [ARGS…]\n"
        "\n"
        "Commands:\n"
        "  help                      Show this information\n"
        "  list-schemas              List installed schemas\n"
        "  list-relocatable-schemas  List relocatable schemas\n"
        "  list-keys                 List keys in a schema\n"
        "  list-children             List children of a schema\n"
        "  list-recursively          List keys and values, recursively\n"
        "  range                     Queries the range of a key\n"
        "  describe                  Queries the description of a key\n"
        "  get                       Get the value of a key\n"
        "  set                       Set the value of a key\n"
        "  reset                     Reset the value of a key\n"
        "  reset-recursively         Reset all values in a given schema\n"
        "  writable                  Check if a key is writable\n"
        "  monitor                   Watch for changes\n"
        "\n"
        "Use “gsettings help COMMAND” to get detailed help.\n\n"));
    }
  else
    {
      g_string_append_printf (string, _("Usage:\n  gsettings [--schemadir SCHEMADIR] %s %s\n\n%s\n\n"),
                              command, synopsis[0] ? _(synopsis) : "", description);

      g_string_append (string, _("Arguments:\n"));

      g_string_append (string,
                       _("  SCHEMADIR A directory to search for additional schemas\n"));

      if (strstr (synopsis, "[COMMAND]"))
        g_string_append (string,
                       _("  COMMAND   The (optional) command to explain\n"));

      else if (strstr (synopsis, "SCHEMA"))
        g_string_append (string,
                       _("  SCHEMA    The name of the schema\n"
                         "  PATH      The path, for relocatable schemas\n"));

      if (strstr (synopsis, "[KEY]"))
        g_string_append (string,
                       _("  KEY       The (optional) key within the schema\n"));

      else if (strstr (synopsis, "KEY"))
        g_string_append (string,
                       _("  KEY       The key within the schema\n"));

      if (strstr (synopsis, "VALUE"))
        g_string_append (string,
                       _("  VALUE     The value to set\n"));

      g_string_append (string, "\n");
    }

  if (requested)
    g_print ("%s", string->str);
  else
    g_printerr ("%s\n", string->str);

  g_string_free (string, TRUE);

  return requested ? 0 : 1;
}


int
main (int argc, char **argv)
{
  void (* function) (void);
  gboolean need_settings, skip_third_arg_test;

#ifdef G_OS_WIN32
  gchar *tmp;
#endif

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);

#ifdef G_OS_WIN32
  tmp = _glib_get_locale_dir ();
  bindtextdomain (GETTEXT_PACKAGE, tmp);
  g_free (tmp);
#else
  bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);
#endif

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  if (argc < 2)
    return gsettings_help (FALSE, NULL);

  global_schema_source = g_settings_schema_source_get_default ();

  if (argc > 3 && g_str_equal (argv[1], "--schemadir"))
    {
      GSettingsSchemaSource *parent = global_schema_source;
      GError *error = NULL;

      global_schema_source = g_settings_schema_source_new_from_directory (argv[2], parent, FALSE, &error);

      if (global_schema_source == NULL)
        {
          g_printerr (_("Could not load schemas from %s: %s\n"), argv[2], error->message);
          g_clear_error (&error);

          return 1;
        }

      /* shift remaining arguments (not correct wrt argv[0], but doesn't matter) */
      argv = argv + 2;
      argc -= 2;
    }
  else if (global_schema_source == NULL)
    {
      g_printerr (_("No schemas installed\n"));
      return 1;
    }
  else
    g_settings_schema_source_ref (global_schema_source);

  need_settings = TRUE;
  skip_third_arg_test = FALSE;

  if (strcmp (argv[1], "help") == 0)
    return gsettings_help (TRUE, argv[2]);

  else if (argc == 2 && strcmp (argv[1], "--version") == 0)
    function = gsettings_print_version;

  else if (argc == 2 && strcmp (argv[1], "list-schemas") == 0)
    function = gsettings_list_schemas;

  else if (argc == 3 && strcmp (argv[1], "list-schemas") == 0
                     && strcmp (argv[2], "--print-paths") == 0)
    {
      skip_third_arg_test = TRUE;
      function = gsettings_list_schemas_with_paths;
    }

  else if (argc == 2 && strcmp (argv[1], "list-relocatable-schemas") == 0)
    function = gsettings_list_relocatable_schemas;

  else if (argc == 3 && strcmp (argv[1], "list-keys") == 0)
    {
      need_settings = FALSE;
      function = gsettings_list_keys;
    }

  else if (argc == 3 && strcmp (argv[1], "list-children") == 0)
    function = gsettings_list_children;

  else if ((argc == 2 || argc == 3) && strcmp (argv[1], "list-recursively") == 0)
    function = gsettings_list_recursively;

  else if (argc == 4 && strcmp (argv[1], "describe") == 0)
    {
      need_settings = FALSE;
      function = gsettings_description;
    }

  else if (argc == 4 && strcmp (argv[1], "range") == 0)
    {
      need_settings = FALSE;
      function = gsettings_range;
    }

  else if (argc == 4 && strcmp (argv[1], "get") == 0)
    function = gsettings_get;

  else if (argc == 5 && strcmp (argv[1], "set") == 0)
    function = gsettings_set;

  else if (argc == 4 && strcmp (argv[1], "reset") == 0)
    function = gsettings_reset;

  else if (argc == 3 && strcmp (argv[1], "reset-recursively") == 0)
    function = gsettings_reset_recursively;

  else if (argc == 4 && strcmp (argv[1], "writable") == 0)
    function = gsettings_writable;

  else if ((argc == 3 || argc == 4) && strcmp (argv[1], "monitor") == 0)
    function = gsettings_monitor;

  else
    return gsettings_help (FALSE, argv[1]);

  if (argc > 2 && !skip_third_arg_test)
    {
      gchar **parts;

      if (argv[2][0] == '\0')
        {
          g_printerr (_("Empty schema name given\n"));
          return 1;
        }

      parts = g_strsplit (argv[2], ":", 2);

      global_schema = g_settings_schema_source_lookup (global_schema_source, parts[0], TRUE);

      if (need_settings)
        {
          if (parts[1])
            {
              if (!check_relocatable_schema (global_schema, parts[0]) || !check_path (parts[1]))
                return 1;

              global_settings = g_settings_new_full (global_schema, NULL, parts[1]);
            }
          else
            {
              if (!check_schema (global_schema, parts[0]))
                return 1;

              global_settings = g_settings_new_full (global_schema, NULL, NULL);
            }
        }
      else
        {
          /* If the user has given a path then we enforce that we have a
           * relocatable schema, but if they didn't give a path then it
           * doesn't matter what type of schema we have (since it's
           * reasonable to ask for introspection information on a
           * relocatable schema without having to give the path).
           */
          if (parts[1])
            {
              if (!check_relocatable_schema (global_schema, parts[0]) || !check_path (parts[1]))
                return 1;
            }
          else
            {
              if (global_schema == NULL)
                {
                  g_printerr (_("No such schema “%s”\n"), parts[0]);
                  return 1;
                }
            }
        }

      g_strfreev (parts);
    }

  if (argc > 3)
    {
      if (!g_settings_schema_has_key (global_schema, argv[3]))
        {
          g_printerr (_("No such key “%s”\n"), argv[3]);
          return 1;
        }

      global_key = argv[3];
      global_schema_key = g_settings_schema_get_key (global_schema, global_key);
    }

  if (argc > 4)
    global_value = argv[4];

  (* function) ();


  g_clear_pointer (&global_schema_source, g_settings_schema_source_unref);
  g_clear_pointer (&global_schema_key, g_settings_schema_key_unref);
  g_clear_pointer (&global_schema, g_settings_schema_unref);
  g_clear_object (&global_settings);

  return 0;
}
