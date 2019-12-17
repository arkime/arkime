/* goption.c - Option parser
 *
 *  Copyright (C) 1999, 2003 Red Hat Software
 *  Copyright (C) 2004       Anders Carlsson <andersca@gnome.org>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:option
 * @Short_description: parses commandline options
 * @Title: Commandline option parser
 *
 * The GOption commandline parser is intended to be a simpler replacement
 * for the popt library. It supports short and long commandline options,
 * as shown in the following example:
 *
 * `testtreemodel -r 1 --max-size 20 --rand --display=:1.0 -vb -- file1 file2`
 *
 * The example demonstrates a number of features of the GOption
 * commandline parser:
 *
 * - Options can be single letters, prefixed by a single dash.
 *
 * - Multiple short options can be grouped behind a single dash.
 *
 * - Long options are prefixed by two consecutive dashes.
 *
 * - Options can have an extra argument, which can be a number, a string or
 *   a filename. For long options, the extra argument can be appended with
 *   an equals sign after the option name, which is useful if the extra
 *   argument starts with a dash, which would otherwise cause it to be
 *   interpreted as another option.
 *
 * - Non-option arguments are returned to the application as rest arguments.
 *
 * - An argument consisting solely of two dashes turns off further parsing,
 *   any remaining arguments (even those starting with a dash) are returned
 *   to the application as rest arguments.
 *
 * Another important feature of GOption is that it can automatically
 * generate nicely formatted help output. Unless it is explicitly turned
 * off with g_option_context_set_help_enabled(), GOption will recognize
 * the `--help`, `-?`, `--help-all` and `--help-groupname` options
 * (where `groupname` is the name of a #GOptionGroup) and write a text
 * similar to the one shown in the following example to stdout.
 *
 * |[
 * Usage:
 *   testtreemodel [OPTION...] - test tree model performance
 *  
 * Help Options:
 *   -h, --help               Show help options
 *   --help-all               Show all help options
 *   --help-gtk               Show GTK+ Options
 *  
 * Application Options:
 *   -r, --repeats=N          Average over N repetitions
 *   -m, --max-size=M         Test up to 2^M items
 *   --display=DISPLAY        X display to use
 *   -v, --verbose            Be verbose
 *   -b, --beep               Beep when done
 *   --rand                   Randomize the data
 * ]|
 *
 * GOption groups options in #GOptionGroups, which makes it easy to
 * incorporate options from multiple sources. The intended use for this is
 * to let applications collect option groups from the libraries it uses,
 * add them to their #GOptionContext, and parse all options by a single call
 * to g_option_context_parse(). See gtk_get_option_group() for an example.
 *
 * If an option is declared to be of type string or filename, GOption takes
 * care of converting it to the right encoding; strings are returned in
 * UTF-8, filenames are returned in the GLib filename encoding. Note that
 * this only works if setlocale() has been called before
 * g_option_context_parse().
 *
 * Here is a complete example of setting up GOption to parse the example
 * commandline above and produce the example help output.
 * |[<!-- language="C" --> 
 * static gint repeats = 2;
 * static gint max_size = 8;
 * static gboolean verbose = FALSE;
 * static gboolean beep = FALSE;
 * static gboolean randomize = FALSE;
 *
 * static GOptionEntry entries[] =
 * {
 *   { "repeats", 'r', 0, G_OPTION_ARG_INT, &repeats, "Average over N repetitions", "N" },
 *   { "max-size", 'm', 0, G_OPTION_ARG_INT, &max_size, "Test up to 2^M items", "M" },
 *   { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
 *   { "beep", 'b', 0, G_OPTION_ARG_NONE, &beep, "Beep when done", NULL },
 *   { "rand", 0, 0, G_OPTION_ARG_NONE, &randomize, "Randomize the data", NULL },
 *   { NULL }
 * };
 *
 * int
 * main (int argc, char *argv[])
 * {
 *   GError *error = NULL;
 *   GOptionContext *context;
 *
 *   context = g_option_context_new ("- test tree model performance");
 *   g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
 *   g_option_context_add_group (context, gtk_get_option_group (TRUE));
 *   if (!g_option_context_parse (context, &argc, &argv, &error))
 *     {
 *       g_print ("option parsing failed: %s\n", error->message);
 *       exit (1);
 *     }
 *
 *   ...
 *
 * }
 * ]|
 *
 * On UNIX systems, the argv that is passed to main() has no particular
 * encoding, even to the extent that different parts of it may have
 * different encodings.  In general, normal arguments and flags will be
 * in the current locale and filenames should be considered to be opaque
 * byte strings.  Proper use of %G_OPTION_ARG_FILENAME vs
 * %G_OPTION_ARG_STRING is therefore important.
 *
 * Note that on Windows, filenames do have an encoding, but using
 * #GOptionContext with the argv as passed to main() will result in a
 * program that can only accept commandline arguments with characters
 * from the system codepage.  This can cause problems when attempting to
 * deal with filenames containing Unicode characters that fall outside
 * of the codepage.
 *
 * A solution to this is to use g_win32_get_command_line() and
 * g_option_context_parse_strv() which will properly handle full Unicode
 * filenames.  If you are using #GApplication, this is done
 * automatically for you.
 *
 * The following example shows how you can use #GOptionContext directly
 * in order to correctly deal with Unicode filenames on Windows:
 *
 * |[<!-- language="C" --> 
 * int
 * main (int argc, char **argv)
 * {
 *   GError *error = NULL;
 *   GOptionContext *context;
 *   gchar **args;
 *
 * #ifdef G_OS_WIN32
 *   args = g_win32_get_command_line ();
 * #else
 *   args = g_strdupv (argv);
 * #endif
 *
 *   // set up context
 *
 *   if (!g_option_context_parse_strv (context, &args, &error))
 *     {
 *       // error happened
 *     }
 *
 *   ...
 *
 *   g_strfreev (args);
 *
 *   ...
 * }
 * ]|
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#if defined __OpenBSD__
#include <unistd.h>
#include <sys/sysctl.h>
#endif

#include "goption.h"

#include "gprintf.h"
#include "glibintl.h"

#define TRANSLATE(group, str) (((group)->translate_func ? (* (group)->translate_func) ((str), (group)->translate_data) : (str)))

#define NO_ARG(entry) ((entry)->arg == G_OPTION_ARG_NONE ||       \
                       ((entry)->arg == G_OPTION_ARG_CALLBACK &&  \
                        ((entry)->flags & G_OPTION_FLAG_NO_ARG)))

#define OPTIONAL_ARG(entry) ((entry)->arg == G_OPTION_ARG_CALLBACK &&  \
                       (entry)->flags & G_OPTION_FLAG_OPTIONAL_ARG)

typedef struct
{
  GOptionArg arg_type;
  gpointer arg_data;
  union
  {
    gboolean bool;
    gint integer;
    gchar *str;
    gchar **array;
    gdouble dbl;
    gint64 int64;
  } prev;
  union
  {
    gchar *str;
    struct
    {
      gint len;
      gchar **data;
    } array;
  } allocated;
} Change;

typedef struct
{
  gchar **ptr;
  gchar *value;
} PendingNull;

struct _GOptionContext
{
  GList           *groups;

  gchar           *parameter_string;
  gchar           *summary;
  gchar           *description;

  GTranslateFunc   translate_func;
  GDestroyNotify   translate_notify;
  gpointer         translate_data;

  guint            help_enabled   : 1;
  guint            ignore_unknown : 1;
  guint            strv_mode      : 1;
  guint            strict_posix   : 1;

  GOptionGroup    *main_group;

  /* We keep a list of change so we can revert them */
  GList           *changes;

  /* We also keep track of all argv elements
   * that should be NULLed or modified.
   */
  GList           *pending_nulls;
};

struct _GOptionGroup
{
  gchar           *name;
  gchar           *description;
  gchar           *help_description;

  gint             ref_count;

  GDestroyNotify   destroy_notify;
  gpointer         user_data;

  GTranslateFunc   translate_func;
  GDestroyNotify   translate_notify;
  gpointer         translate_data;

  GOptionEntry    *entries;
  gint             n_entries;

  GOptionParseFunc pre_parse_func;
  GOptionParseFunc post_parse_func;
  GOptionErrorFunc error_func;
};

static void free_changes_list (GOptionContext *context,
                               gboolean        revert);
static void free_pending_nulls (GOptionContext *context,
                                gboolean        perform_nulls);


static int
_g_unichar_get_width (gunichar c)
{
  if (G_UNLIKELY (g_unichar_iszerowidth (c)))
    return 0;

  /* we ignore the fact that we should call g_unichar_iswide_cjk() under
   * some locales (legacy East Asian ones) */
  if (g_unichar_iswide (c))
    return 2;

  return 1;
}

static glong
_g_utf8_strwidth (const gchar *p)
{
  glong len = 0;
  g_return_val_if_fail (p != NULL, 0);

  while (*p)
    {
      len += _g_unichar_get_width (g_utf8_get_char (p));
      p = g_utf8_next_char (p);
    }

  return len;
}

G_DEFINE_QUARK (g-option-context-error-quark, g_option_error)

/**
 * g_option_context_new:
 * @parameter_string: (nullable): a string which is displayed in
 *    the first line of `--help` output, after the usage summary
 *    `programname [OPTION...]`
 *
 * Creates a new option context.
 *
 * The @parameter_string can serve multiple purposes. It can be used
 * to add descriptions for "rest" arguments, which are not parsed by
 * the #GOptionContext, typically something like "FILES" or
 * "FILE1 FILE2...". If you are using #G_OPTION_REMAINING for
 * collecting "rest" arguments, GLib handles this automatically by
 * using the @arg_description of the corresponding #GOptionEntry in
 * the usage summary.
 *
 * Another usage is to give a short summary of the program
 * functionality, like " - frob the strings", which will be displayed
 * in the same line as the usage. For a longer description of the
 * program functionality that should be displayed as a paragraph
 * below the usage line, use g_option_context_set_summary().
 *
 * Note that the @parameter_string is translated using the
 * function set with g_option_context_set_translate_func(), so
 * it should normally be passed untranslated.
 *
 * Returns: a newly created #GOptionContext, which must be
 *    freed with g_option_context_free() after use.
 *
 * Since: 2.6
 */
GOptionContext *
g_option_context_new (const gchar *parameter_string)

{
  GOptionContext *context;

  context = g_new0 (GOptionContext, 1);

  context->parameter_string = g_strdup (parameter_string);
  context->strict_posix = FALSE;
  context->help_enabled = TRUE;
  context->ignore_unknown = FALSE;

  return context;
}

/**
 * g_option_context_free:
 * @context: a #GOptionContext
 *
 * Frees context and all the groups which have been
 * added to it.
 *
 * Please note that parsed arguments need to be freed separately (see
 * #GOptionEntry).
 *
 * Since: 2.6
 */
void g_option_context_free (GOptionContext *context)
{
  g_return_if_fail (context != NULL);

  g_list_free_full (context->groups, (GDestroyNotify) g_option_group_unref);

  if (context->main_group)
    g_option_group_unref (context->main_group);

  free_changes_list (context, FALSE);
  free_pending_nulls (context, FALSE);

  g_free (context->parameter_string);
  g_free (context->summary);
  g_free (context->description);

  if (context->translate_notify)
    (* context->translate_notify) (context->translate_data);

  g_free (context);
}


/**
 * g_option_context_set_help_enabled:
 * @context: a #GOptionContext
 * @help_enabled: %TRUE to enable `--help`, %FALSE to disable it
 *
 * Enables or disables automatic generation of `--help` output.
 * By default, g_option_context_parse() recognizes `--help`, `-h`,
 * `-?`, `--help-all` and `--help-groupname` and creates suitable
 * output to stdout.
 *
 * Since: 2.6
 */
void g_option_context_set_help_enabled (GOptionContext *context,
                                        gboolean        help_enabled)

{
  g_return_if_fail (context != NULL);

  context->help_enabled = help_enabled;
}

/**
 * g_option_context_get_help_enabled:
 * @context: a #GOptionContext
 *
 * Returns whether automatic `--help` generation
 * is turned on for @context. See g_option_context_set_help_enabled().
 *
 * Returns: %TRUE if automatic help generation is turned on.
 *
 * Since: 2.6
 */
gboolean
g_option_context_get_help_enabled (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return context->help_enabled;
}

/**
 * g_option_context_set_ignore_unknown_options:
 * @context: a #GOptionContext
 * @ignore_unknown: %TRUE to ignore unknown options, %FALSE to produce
 *    an error when unknown options are met
 *
 * Sets whether to ignore unknown options or not. If an argument is
 * ignored, it is left in the @argv array after parsing. By default,
 * g_option_context_parse() treats unknown options as error.
 *
 * This setting does not affect non-option arguments (i.e. arguments
 * which don't start with a dash). But note that GOption cannot reliably
 * determine whether a non-option belongs to a preceding unknown option.
 *
 * Since: 2.6
 **/
void
g_option_context_set_ignore_unknown_options (GOptionContext *context,
                                             gboolean        ignore_unknown)
{
  g_return_if_fail (context != NULL);

  context->ignore_unknown = ignore_unknown;
}

/**
 * g_option_context_get_ignore_unknown_options:
 * @context: a #GOptionContext
 *
 * Returns whether unknown options are ignored or not. See
 * g_option_context_set_ignore_unknown_options().
 *
 * Returns: %TRUE if unknown options are ignored.
 *
 * Since: 2.6
 **/
gboolean
g_option_context_get_ignore_unknown_options (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return context->ignore_unknown;
}

/**
 * g_option_context_set_strict_posix:
 * @context: a #GOptionContext
 * @strict_posix: the new value
 *
 * Sets strict POSIX mode.
 *
 * By default, this mode is disabled.
 *
 * In strict POSIX mode, the first non-argument parameter encountered
 * (eg: filename) terminates argument processing.  Remaining arguments
 * are treated as non-options and are not attempted to be parsed.
 *
 * If strict POSIX mode is disabled then parsing is done in the GNU way
 * where option arguments can be freely mixed with non-options.
 *
 * As an example, consider "ls foo -l".  With GNU style parsing, this
 * will list "foo" in long mode.  In strict POSIX style, this will list
 * the files named "foo" and "-l".
 *
 * It may be useful to force strict POSIX mode when creating "verb
 * style" command line tools.  For example, the "gsettings" command line
 * tool supports the global option "--schemadir" as well as many
 * subcommands ("get", "set", etc.) which each have their own set of
 * arguments.  Using strict POSIX mode will allow parsing the global
 * options up to the verb name while leaving the remaining options to be
 * parsed by the relevant subcommand (which can be determined by
 * examining the verb name, which should be present in argv[1] after
 * parsing).
 *
 * Since: 2.44
 **/
void
g_option_context_set_strict_posix (GOptionContext *context,
                                   gboolean        strict_posix)
{
  g_return_if_fail (context != NULL);

  context->strict_posix = strict_posix;
}

/**
 * g_option_context_get_strict_posix:
 * @context: a #GOptionContext
 *
 * Returns whether strict POSIX code is enabled.
 *
 * See g_option_context_set_strict_posix() for more information.
 *
 * Returns: %TRUE if strict POSIX is enabled, %FALSE otherwise.
 *
 * Since: 2.44
 **/
gboolean
g_option_context_get_strict_posix (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return context->strict_posix;
}

/**
 * g_option_context_add_group:
 * @context: a #GOptionContext
 * @group: (transfer full): the group to add
 *
 * Adds a #GOptionGroup to the @context, so that parsing with @context
 * will recognize the options in the group. Note that this will take
 * ownership of the @group and thus the @group should not be freed.
 *
 * Since: 2.6
 **/
void
g_option_context_add_group (GOptionContext *context,
                            GOptionGroup   *group)
{
  GList *list;

  g_return_if_fail (context != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (group->name != NULL);
  g_return_if_fail (group->description != NULL);
  g_return_if_fail (group->help_description != NULL);

  for (list = context->groups; list; list = list->next)
    {
      GOptionGroup *g = (GOptionGroup *)list->data;

      if ((group->name == NULL && g->name == NULL) ||
          (group->name && g->name && strcmp (group->name, g->name) == 0))
        g_warning ("A group named \"%s\" is already part of this GOptionContext",
                   group->name);
    }

  context->groups = g_list_append (context->groups, group);
}

/**
 * g_option_context_set_main_group:
 * @context: a #GOptionContext
 * @group: (transfer full): the group to set as main group
 *
 * Sets a #GOptionGroup as main group of the @context.
 * This has the same effect as calling g_option_context_add_group(),
 * the only difference is that the options in the main group are
 * treated differently when generating `--help` output.
 *
 * Since: 2.6
 **/
void
g_option_context_set_main_group (GOptionContext *context,
                                 GOptionGroup   *group)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (group != NULL);

  if (context->main_group)
    {
      g_warning ("This GOptionContext already has a main group");

      return;
    }

  context->main_group = group;
}

/**
 * g_option_context_get_main_group:
 * @context: a #GOptionContext
 *
 * Returns a pointer to the main group of @context.
 *
 * Returns: (transfer none): the main group of @context, or %NULL if
 *  @context doesn't have a main group. Note that group belongs to
 *  @context and should not be modified or freed.
 *
 * Since: 2.6
 **/
GOptionGroup *
g_option_context_get_main_group (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);

  return context->main_group;
}

/**
 * g_option_context_add_main_entries:
 * @context: a #GOptionContext
 * @entries: a %NULL-terminated array of #GOptionEntrys
 * @translation_domain: (nullable): a translation domain to use for translating
 *    the `--help` output for the options in @entries
 *    with gettext(), or %NULL
 *
 * A convenience function which creates a main group if it doesn't
 * exist, adds the @entries to it and sets the translation domain.
 *
 * Since: 2.6
 **/
void
g_option_context_add_main_entries (GOptionContext      *context,
                                   const GOptionEntry  *entries,
                                   const gchar         *translation_domain)
{
  g_return_if_fail (entries != NULL);

  if (!context->main_group)
    context->main_group = g_option_group_new (NULL, NULL, NULL, NULL, NULL);

  g_option_group_add_entries (context->main_group, entries);
  g_option_group_set_translation_domain (context->main_group, translation_domain);
}

static gint
calculate_max_length (GOptionGroup *group,
                      GHashTable   *aliases)
{
  GOptionEntry *entry;
  gint i, len, max_length;
  const gchar *long_name;

  max_length = 0;

  for (i = 0; i < group->n_entries; i++)
    {
      entry = &group->entries[i];

      if (entry->flags & G_OPTION_FLAG_HIDDEN)
        continue;

      long_name = g_hash_table_lookup (aliases, &entry->long_name);
      if (!long_name)
        long_name = entry->long_name;
      len = _g_utf8_strwidth (long_name);

      if (entry->short_name)
        len += 4;

      if (!NO_ARG (entry) && entry->arg_description)
        len += 1 + _g_utf8_strwidth (TRANSLATE (group, entry->arg_description));

      max_length = MAX (max_length, len);
    }

  return max_length;
}

static void
print_entry (GOptionGroup       *group,
             gint                max_length,
             const GOptionEntry *entry,
             GString            *string,
             GHashTable         *aliases)
{
  GString *str;
  const gchar *long_name;

  if (entry->flags & G_OPTION_FLAG_HIDDEN)
    return;

  if (entry->long_name[0] == 0)
    return;

  long_name = g_hash_table_lookup (aliases, &entry->long_name);
  if (!long_name)
    long_name = entry->long_name;

  str = g_string_new (NULL);

  if (entry->short_name)
    g_string_append_printf (str, "  -%c, --%s", entry->short_name, long_name);
  else
    g_string_append_printf (str, "  --%s", long_name);

  if (entry->arg_description)
    g_string_append_printf (str, "=%s", TRANSLATE (group, entry->arg_description));

  g_string_append_printf (string, "%s%*s %s\n", str->str,
                          (int) (max_length + 4 - _g_utf8_strwidth (str->str)), "",
                          entry->description ? TRANSLATE (group, entry->description) : "");
  g_string_free (str, TRUE);
}

static gboolean
group_has_visible_entries (GOptionContext *context,
                           GOptionGroup *group,
                           gboolean      main_entries)
{
  GOptionFlags reject_filter = G_OPTION_FLAG_HIDDEN;
  GOptionEntry *entry;
  gint i, l;
  gboolean main_group = group == context->main_group;

  if (!main_entries)
    reject_filter |= G_OPTION_FLAG_IN_MAIN;

  for (i = 0, l = (group ? group->n_entries : 0); i < l; i++)
    {
      entry = &group->entries[i];

      if (main_entries && !main_group && !(entry->flags & G_OPTION_FLAG_IN_MAIN))
        continue;
      if (entry->long_name[0] == 0) /* ignore rest entry */
        continue;
      if (!(entry->flags & reject_filter))
        return TRUE;
    }

  return FALSE;
}

static gboolean
group_list_has_visible_entries (GOptionContext *context,
                                GList          *group_list,
                                gboolean       main_entries)
{
  while (group_list)
    {
      if (group_has_visible_entries (context, group_list->data, main_entries))
        return TRUE;

      group_list = group_list->next;
    }

  return FALSE;
}

static gboolean
context_has_h_entry (GOptionContext *context)
{
  gsize i;
  GList *list;

  if (context->main_group)
    {
      for (i = 0; i < context->main_group->n_entries; i++)
        {
          if (context->main_group->entries[i].short_name == 'h')
            return TRUE;
        }
    }

  for (list = context->groups; list != NULL; list = g_list_next (list))
    {
     GOptionGroup *group;

      group = (GOptionGroup*)list->data;
      for (i = 0; i < group->n_entries; i++)
        {
          if (group->entries[i].short_name == 'h')
            return TRUE;
        }
    }
  return FALSE;
}

/**
 * g_option_context_get_help:
 * @context: a #GOptionContext
 * @main_help: if %TRUE, only include the main group
 * @group: (nullable): the #GOptionGroup to create help for, or %NULL
 *
 * Returns a formatted, translated help text for the given context.
 * To obtain the text produced by `--help`, call
 * `g_option_context_get_help (context, TRUE, NULL)`.
 * To obtain the text produced by `--help-all`, call
 * `g_option_context_get_help (context, FALSE, NULL)`.
 * To obtain the help text for an option group, call
 * `g_option_context_get_help (context, FALSE, group)`.
 *
 * Returns: A newly allocated string containing the help text
 *
 * Since: 2.14
 */
gchar *
g_option_context_get_help (GOptionContext *context,
                           gboolean        main_help,
                           GOptionGroup   *group)
{
  GList *list;
  gint max_length = 0, len;
  gint i;
  GOptionEntry *entry;
  GHashTable *shadow_map;
  GHashTable *aliases;
  gboolean seen[256];
  const gchar *rest_description;
  GString *string;
  guchar token;

  string = g_string_sized_new (1024);

  rest_description = NULL;
  if (context->main_group)
    {

      for (i = 0; i < context->main_group->n_entries; i++)
        {
          entry = &context->main_group->entries[i];
          if (entry->long_name[0] == 0)
            {
              rest_description = TRANSLATE (context->main_group, entry->arg_description);
              break;
            }
        }
    }

  g_string_append_printf (string, "%s\n  %s", _("Usage:"), g_get_prgname ());
  if (context->help_enabled ||
      (context->main_group && context->main_group->n_entries > 0) ||
      context->groups != NULL)
    g_string_append_printf (string, " %s", _("[OPTION…]"));

  if (rest_description)
    {
      g_string_append (string, " ");
      g_string_append (string, rest_description);
    }

  if (context->parameter_string)
    {
      g_string_append (string, " ");
      g_string_append (string, TRANSLATE (context, context->parameter_string));
    }

  g_string_append (string, "\n\n");

  if (context->summary)
    {
      g_string_append (string, TRANSLATE (context, context->summary));
      g_string_append (string, "\n\n");
    }

  memset (seen, 0, sizeof (gboolean) * 256);
  shadow_map = g_hash_table_new (g_str_hash, g_str_equal);
  aliases = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  if (context->main_group)
    {
      for (i = 0; i < context->main_group->n_entries; i++)
        {
          entry = &context->main_group->entries[i];
          g_hash_table_insert (shadow_map,
                               (gpointer)entry->long_name,
                               entry);

          if (seen[(guchar)entry->short_name])
            entry->short_name = 0;
          else
            seen[(guchar)entry->short_name] = TRUE;
        }
    }

  list = context->groups;
  while (list != NULL)
    {
      GOptionGroup *g = list->data;
      for (i = 0; i < g->n_entries; i++)
        {
          entry = &g->entries[i];
          if (g_hash_table_lookup (shadow_map, entry->long_name) &&
              !(entry->flags & G_OPTION_FLAG_NOALIAS))
            {
              g_hash_table_insert (aliases, &entry->long_name,
                                   g_strdup_printf ("%s-%s", g->name, entry->long_name));
            }
          else
            g_hash_table_insert (shadow_map, (gpointer)entry->long_name, entry);

          if (seen[(guchar)entry->short_name] &&
              !(entry->flags & G_OPTION_FLAG_NOALIAS))
            entry->short_name = 0;
          else
            seen[(guchar)entry->short_name] = TRUE;
        }
      list = list->next;
    }

  g_hash_table_destroy (shadow_map);

  list = context->groups;

  if (context->help_enabled)
    {
      max_length = _g_utf8_strwidth ("-?, --help");

      if (list)
        {
          len = _g_utf8_strwidth ("--help-all");
          max_length = MAX (max_length, len);
        }
    }

  if (context->main_group)
    {
      len = calculate_max_length (context->main_group, aliases);
      max_length = MAX (max_length, len);
    }

  while (list != NULL)
    {
      GOptionGroup *g = list->data;

      if (context->help_enabled)
        {
          /* First, we check the --help-<groupname> options */
          len = _g_utf8_strwidth ("--help-") + _g_utf8_strwidth (g->name);
          max_length = MAX (max_length, len);
        }

      /* Then we go through the entries */
      len = calculate_max_length (g, aliases);
      max_length = MAX (max_length, len);

      list = list->next;
    }

  /* Add a bit of padding */
  max_length += 4;

  if (!group && context->help_enabled)
    {
      list = context->groups;

      token = context_has_h_entry (context) ? '?' : 'h';

      g_string_append_printf (string, "%s\n  -%c, --%-*s %s\n",
                              _("Help Options:"), token, max_length - 4, "help",
                              _("Show help options"));

      /* We only want --help-all when there are groups */
      if (list)
        g_string_append_printf (string, "  --%-*s %s\n",
                                max_length, "help-all",
                                _("Show all help options"));

      while (list)
        {
          GOptionGroup *g = list->data;

          if (group_has_visible_entries (context, g, FALSE))
            g_string_append_printf (string, "  --help-%-*s %s\n",
                                    max_length - 5, g->name,
                                    TRANSLATE (g, g->help_description));

          list = list->next;
        }

      g_string_append (string, "\n");
    }

  if (group)
    {
      /* Print a certain group */

      if (group_has_visible_entries (context, group, FALSE))
        {
          g_string_append (string, TRANSLATE (group, group->description));
          g_string_append (string, "\n");
          for (i = 0; i < group->n_entries; i++)
            print_entry (group, max_length, &group->entries[i], string, aliases);
          g_string_append (string, "\n");
        }
    }
  else if (!main_help)
    {
      /* Print all groups */

      list = context->groups;

      while (list)
        {
          GOptionGroup *g = list->data;

          if (group_has_visible_entries (context, g, FALSE))
            {
              g_string_append (string, g->description);
              g_string_append (string, "\n");
              for (i = 0; i < g->n_entries; i++)
                if (!(g->entries[i].flags & G_OPTION_FLAG_IN_MAIN))
                  print_entry (g, max_length, &g->entries[i], string, aliases);

              g_string_append (string, "\n");
            }

          list = list->next;
        }
    }

  /* Print application options if --help or --help-all has been specified */
  if ((main_help || !group) &&
      (group_has_visible_entries (context, context->main_group, TRUE) ||
       group_list_has_visible_entries (context, context->groups, TRUE)))
    {
      list = context->groups;

      if (context->help_enabled || list)
        g_string_append (string,  _("Application Options:"));
      else
        g_string_append (string, _("Options:"));
      g_string_append (string, "\n");
      if (context->main_group)
        for (i = 0; i < context->main_group->n_entries; i++)
          print_entry (context->main_group, max_length,
                       &context->main_group->entries[i], string, aliases);

      while (list != NULL)
        {
          GOptionGroup *g = list->data;

          /* Print main entries from other groups */
          for (i = 0; i < g->n_entries; i++)
            if (g->entries[i].flags & G_OPTION_FLAG_IN_MAIN)
              print_entry (g, max_length, &g->entries[i], string, aliases);

          list = list->next;
        }

      g_string_append (string, "\n");
    }

  if (context->description)
    {
      g_string_append (string, TRANSLATE (context, context->description));
      g_string_append (string, "\n");
    }

  g_hash_table_destroy (aliases);

  return g_string_free (string, FALSE);
}

G_GNUC_NORETURN
static void
print_help (GOptionContext *context,
            gboolean        main_help,
            GOptionGroup   *group)
{
  gchar *help;

  help = g_option_context_get_help (context, main_help, group);
  g_print ("%s", help);
  g_free (help);

  exit (0);
}

static gboolean
parse_int (const gchar *arg_name,
           const gchar *arg,
           gint        *result,
           GError     **error)
{
  gchar *end;
  glong tmp;

  errno = 0;
  tmp = strtol (arg, &end, 0);

  if (*arg == '\0' || *end != '\0')
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Cannot parse integer value “%s” for %s"),
                   arg, arg_name);
      return FALSE;
    }

  *result = tmp;
  if (*result != tmp || errno == ERANGE)
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Integer value “%s” for %s out of range"),
                   arg, arg_name);
      return FALSE;
    }

  return TRUE;
}


static gboolean
parse_double (const gchar *arg_name,
           const gchar *arg,
           gdouble        *result,
           GError     **error)
{
  gchar *end;
  gdouble tmp;

  errno = 0;
  tmp = g_strtod (arg, &end);

  if (*arg == '\0' || *end != '\0')
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Cannot parse double value “%s” for %s"),
                   arg, arg_name);
      return FALSE;
    }
  if (errno == ERANGE)
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Double value “%s” for %s out of range"),
                   arg, arg_name);
      return FALSE;
    }

  *result = tmp;

  return TRUE;
}


static gboolean
parse_int64 (const gchar *arg_name,
             const gchar *arg,
             gint64      *result,
             GError     **error)
{
  gchar *end;
  gint64 tmp;

  errno = 0;
  tmp = g_ascii_strtoll (arg, &end, 0);

  if (*arg == '\0' || *end != '\0')
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Cannot parse integer value “%s” for %s"),
                   arg, arg_name);
      return FALSE;
    }
  if (errno == ERANGE)
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   _("Integer value “%s” for %s out of range"),
                   arg, arg_name);
      return FALSE;
    }

  *result = tmp;

  return TRUE;
}


static Change *
get_change (GOptionContext *context,
            GOptionArg      arg_type,
            gpointer        arg_data)
{
  GList *list;
  Change *change = NULL;

  for (list = context->changes; list != NULL; list = list->next)
    {
      change = list->data;

      if (change->arg_data == arg_data)
        goto found;
    }

  change = g_new0 (Change, 1);
  change->arg_type = arg_type;
  change->arg_data = arg_data;

  context->changes = g_list_prepend (context->changes, change);

 found:

  return change;
}

static void
add_pending_null (GOptionContext *context,
                  gchar         **ptr,
                  gchar          *value)
{
  PendingNull *n;

  n = g_new0 (PendingNull, 1);
  n->ptr = ptr;
  n->value = value;

  context->pending_nulls = g_list_prepend (context->pending_nulls, n);
}

static gboolean
parse_arg (GOptionContext *context,
           GOptionGroup   *group,
           GOptionEntry   *entry,
           const gchar    *value,
           const gchar    *option_name,
           GError        **error)

{
  Change *change;

  g_assert (value || OPTIONAL_ARG (entry) || NO_ARG (entry));

  switch (entry->arg)
    {
    case G_OPTION_ARG_NONE:
      {
        (void) get_change (context, G_OPTION_ARG_NONE,
                           entry->arg_data);

        *(gboolean *)entry->arg_data = !(entry->flags & G_OPTION_FLAG_REVERSE);
        break;
      }
    case G_OPTION_ARG_STRING:
      {
        gchar *data;

#ifdef G_OS_WIN32
        if (!context->strv_mode)
          data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
        else
          data = g_strdup (value);
#else
        data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
#endif

        if (!data)
          return FALSE;

        change = get_change (context, G_OPTION_ARG_STRING,
                             entry->arg_data);

        if (!change->allocated.str)
          change->prev.str = *(gchar **)entry->arg_data;
        else
          g_free (change->allocated.str);

        change->allocated.str = data;

        *(gchar **)entry->arg_data = data;
        break;
      }
    case G_OPTION_ARG_STRING_ARRAY:
      {
        gchar *data;

#ifdef G_OS_WIN32
        if (!context->strv_mode)
          data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
        else
          data = g_strdup (value);
#else
        data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
#endif

        if (!data)
          return FALSE;

        change = get_change (context, G_OPTION_ARG_STRING_ARRAY,
                             entry->arg_data);

        if (change->allocated.array.len == 0)
          {
            change->prev.array = *(gchar ***)entry->arg_data;
            change->allocated.array.data = g_new (gchar *, 2);
          }
        else
          change->allocated.array.data =
            g_renew (gchar *, change->allocated.array.data,
                     change->allocated.array.len + 2);

        change->allocated.array.data[change->allocated.array.len] = data;
        change->allocated.array.data[change->allocated.array.len + 1] = NULL;

        change->allocated.array.len ++;

        *(gchar ***)entry->arg_data = change->allocated.array.data;

        break;
      }

    case G_OPTION_ARG_FILENAME:
      {
        gchar *data;

#ifdef G_OS_WIN32
        if (!context->strv_mode)
          data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
        else
          data = g_strdup (value);

        if (!data)
          return FALSE;
#else
        data = g_strdup (value);
#endif
        change = get_change (context, G_OPTION_ARG_FILENAME,
                             entry->arg_data);

        if (!change->allocated.str)
          change->prev.str = *(gchar **)entry->arg_data;
        else
          g_free (change->allocated.str);

        change->allocated.str = data;

        *(gchar **)entry->arg_data = data;
        break;
      }

    case G_OPTION_ARG_FILENAME_ARRAY:
      {
        gchar *data;

#ifdef G_OS_WIN32
        if (!context->strv_mode)
          data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
        else
          data = g_strdup (value);

        if (!data)
          return FALSE;
#else
        data = g_strdup (value);
#endif
        change = get_change (context, G_OPTION_ARG_STRING_ARRAY,
                             entry->arg_data);

        if (change->allocated.array.len == 0)
          {
            change->prev.array = *(gchar ***)entry->arg_data;
            change->allocated.array.data = g_new (gchar *, 2);
          }
        else
          change->allocated.array.data =
            g_renew (gchar *, change->allocated.array.data,
                     change->allocated.array.len + 2);

        change->allocated.array.data[change->allocated.array.len] = data;
        change->allocated.array.data[change->allocated.array.len + 1] = NULL;

        change->allocated.array.len ++;

        *(gchar ***)entry->arg_data = change->allocated.array.data;

        break;
      }

    case G_OPTION_ARG_INT:
      {
        gint data;

        if (!parse_int (option_name, value,
                        &data,
                        error))
          return FALSE;

        change = get_change (context, G_OPTION_ARG_INT,
                             entry->arg_data);
        change->prev.integer = *(gint *)entry->arg_data;
        *(gint *)entry->arg_data = data;
        break;
      }
    case G_OPTION_ARG_CALLBACK:
      {
        gchar *data;
        gboolean retval;

        if (!value && entry->flags & G_OPTION_FLAG_OPTIONAL_ARG)
          data = NULL;
        else if (entry->flags & G_OPTION_FLAG_NO_ARG)
          data = NULL;
        else if (entry->flags & G_OPTION_FLAG_FILENAME)
          {
#ifdef G_OS_WIN32
            if (!context->strv_mode)
              data = g_locale_to_utf8 (value, -1, NULL, NULL, error);
            else
              data = g_strdup (value);
#else
            data = g_strdup (value);
#endif
          }
        else
          data = g_locale_to_utf8 (value, -1, NULL, NULL, error);

        if (!(entry->flags & (G_OPTION_FLAG_NO_ARG|G_OPTION_FLAG_OPTIONAL_ARG)) &&
            !data)
          return FALSE;

        retval = (* (GOptionArgFunc) entry->arg_data) (option_name, data, group->user_data, error);

        if (!retval && error != NULL && *error == NULL)
          g_set_error (error,
                       G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                       _("Error parsing option %s"), option_name);

        g_free (data);

        return retval;

        break;
      }
    case G_OPTION_ARG_DOUBLE:
      {
        gdouble data;

        if (!parse_double (option_name, value,
                        &data,
                        error))
          {
            return FALSE;
          }

        change = get_change (context, G_OPTION_ARG_DOUBLE,
                             entry->arg_data);
        change->prev.dbl = *(gdouble *)entry->arg_data;
        *(gdouble *)entry->arg_data = data;
        break;
      }
    case G_OPTION_ARG_INT64:
      {
        gint64 data;

        if (!parse_int64 (option_name, value,
                         &data,
                         error))
          {
            return FALSE;
          }

        change = get_change (context, G_OPTION_ARG_INT64,
                             entry->arg_data);
        change->prev.int64 = *(gint64 *)entry->arg_data;
        *(gint64 *)entry->arg_data = data;
        break;
      }
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
parse_short_option (GOptionContext *context,
                    GOptionGroup   *group,
                    gint            idx,
                    gint           *new_idx,
                    gchar           arg,
                    gint           *argc,
                    gchar        ***argv,
                    GError        **error,
                    gboolean       *parsed)
{
  gint j;

  for (j = 0; j < group->n_entries; j++)
    {
      if (arg == group->entries[j].short_name)
        {
          gchar *option_name;
          gchar *value = NULL;

          option_name = g_strdup_printf ("-%c", group->entries[j].short_name);

          if (NO_ARG (&group->entries[j]))
            value = NULL;
          else
            {
              if (*new_idx > idx)
                {
                  g_set_error (error,
                               G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                               _("Error parsing option %s"), option_name);
                  g_free (option_name);
                  return FALSE;
                }

              if (idx < *argc - 1)
                {
                  if (!OPTIONAL_ARG (&group->entries[j]))
                    {
                      value = (*argv)[idx + 1];
                      add_pending_null (context, &((*argv)[idx + 1]), NULL);
                      *new_idx = idx + 1;
                    }
                  else
                    {
                      if ((*argv)[idx + 1][0] == '-')
                        value = NULL;
                      else
                        {
                          value = (*argv)[idx + 1];
                          add_pending_null (context, &((*argv)[idx + 1]), NULL);
                          *new_idx = idx + 1;
                        }
                    }
                }
              else if (idx >= *argc - 1 && OPTIONAL_ARG (&group->entries[j]))
                value = NULL;
              else
                {
                  g_set_error (error,
                               G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                               _("Missing argument for %s"), option_name);
                  g_free (option_name);
                  return FALSE;
                }
            }

          if (!parse_arg (context, group, &group->entries[j],
                          value, option_name, error))
            {
              g_free (option_name);
              return FALSE;
            }

          g_free (option_name);
          *parsed = TRUE;
        }
    }

  return TRUE;
}

static gboolean
parse_long_option (GOptionContext *context,
                   GOptionGroup   *group,
                   gint           *idx,
                   gchar          *arg,
                   gboolean        aliased,
                   gint           *argc,
                   gchar        ***argv,
                   GError        **error,
                   gboolean       *parsed)
{
  gint j;

  for (j = 0; j < group->n_entries; j++)
    {
      if (*idx >= *argc)
        return TRUE;

      if (aliased && (group->entries[j].flags & G_OPTION_FLAG_NOALIAS))
        continue;

      if (NO_ARG (&group->entries[j]) &&
          strcmp (arg, group->entries[j].long_name) == 0)
        {
          gchar *option_name;
          gboolean retval;

          option_name = g_strconcat ("--", group->entries[j].long_name, NULL);
          retval = parse_arg (context, group, &group->entries[j],
                              NULL, option_name, error);
          g_free (option_name);

          add_pending_null (context, &((*argv)[*idx]), NULL);
          *parsed = TRUE;

          return retval;
        }
      else
        {
          gint len = strlen (group->entries[j].long_name);

          if (strncmp (arg, group->entries[j].long_name, len) == 0 &&
              (arg[len] == '=' || arg[len] == 0))
            {
              gchar *value = NULL;
              gchar *option_name;

              add_pending_null (context, &((*argv)[*idx]), NULL);
              option_name = g_strconcat ("--", group->entries[j].long_name, NULL);

              if (arg[len] == '=')
                value = arg + len + 1;
              else if (*idx < *argc - 1)
                {
                  if (!OPTIONAL_ARG (&group->entries[j]))
                    {
                      value = (*argv)[*idx + 1];
                      add_pending_null (context, &((*argv)[*idx + 1]), NULL);
                      (*idx)++;
                    }
                  else
                    {
                      if ((*argv)[*idx + 1][0] == '-')
                        {
                          gboolean retval;
                          retval = parse_arg (context, group, &group->entries[j],
                                              NULL, option_name, error);
                          *parsed = TRUE;
                          g_free (option_name);
                          return retval;
                        }
                      else
                        {
                          value = (*argv)[*idx + 1];
                          add_pending_null (context, &((*argv)[*idx + 1]), NULL);
                          (*idx)++;
                        }
                    }
                }
              else if (*idx >= *argc - 1 && OPTIONAL_ARG (&group->entries[j]))
                {
                    gboolean retval;
                    retval = parse_arg (context, group, &group->entries[j],
                                        NULL, option_name, error);
                    *parsed = TRUE;
                    g_free (option_name);
                    return retval;
                }
              else
                {
                  g_set_error (error,
                               G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                               _("Missing argument for %s"), option_name);
                  g_free (option_name);
                  return FALSE;
                }

              if (!parse_arg (context, group, &group->entries[j],
                              value, option_name, error))
                {
                  g_free (option_name);
                  return FALSE;
                }

              g_free (option_name);
              *parsed = TRUE;
            }
        }
    }

  return TRUE;
}

static gboolean
parse_remaining_arg (GOptionContext *context,
                     GOptionGroup   *group,
                     gint           *idx,
                     gint           *argc,
                     gchar        ***argv,
                     GError        **error,
                     gboolean       *parsed)
{
  gint j;

  for (j = 0; j < group->n_entries; j++)
    {
      if (*idx >= *argc)
        return TRUE;

      if (group->entries[j].long_name[0])
        continue;

      g_return_val_if_fail (group->entries[j].arg == G_OPTION_ARG_CALLBACK ||
                            group->entries[j].arg == G_OPTION_ARG_STRING_ARRAY ||
                            group->entries[j].arg == G_OPTION_ARG_FILENAME_ARRAY, FALSE);

      add_pending_null (context, &((*argv)[*idx]), NULL);

      if (!parse_arg (context, group, &group->entries[j], (*argv)[*idx], "", error))
        return FALSE;

      *parsed = TRUE;
      return TRUE;
    }

  return TRUE;
}

static void
free_changes_list (GOptionContext *context,
                   gboolean        revert)
{
  GList *list;

  for (list = context->changes; list != NULL; list = list->next)
    {
      Change *change = list->data;

      if (revert)
        {
          switch (change->arg_type)
            {
            case G_OPTION_ARG_NONE:
              *(gboolean *)change->arg_data = change->prev.bool;
              break;
            case G_OPTION_ARG_INT:
              *(gint *)change->arg_data = change->prev.integer;
              break;
            case G_OPTION_ARG_STRING:
            case G_OPTION_ARG_FILENAME:
              g_free (change->allocated.str);
              *(gchar **)change->arg_data = change->prev.str;
              break;
            case G_OPTION_ARG_STRING_ARRAY:
            case G_OPTION_ARG_FILENAME_ARRAY:
              g_strfreev (change->allocated.array.data);
              *(gchar ***)change->arg_data = change->prev.array;
              break;
            case G_OPTION_ARG_DOUBLE:
              *(gdouble *)change->arg_data = change->prev.dbl;
              break;
            case G_OPTION_ARG_INT64:
              *(gint64 *)change->arg_data = change->prev.int64;
              break;
            default:
              g_assert_not_reached ();
            }
        }

      g_free (change);
    }

  g_list_free (context->changes);
  context->changes = NULL;
}

static void
free_pending_nulls (GOptionContext *context,
                    gboolean        perform_nulls)
{
  GList *list;

  for (list = context->pending_nulls; list != NULL; list = list->next)
    {
      PendingNull *n = list->data;

      if (perform_nulls)
        {
          if (n->value)
            {
              /* Copy back the short options */
              *(n->ptr)[0] = '-';
              strcpy (*n->ptr + 1, n->value);
            }
          else
            {
              if (context->strv_mode)
                g_free (*n->ptr);

              *n->ptr = NULL;
            }
        }

      g_free (n->value);
      g_free (n);
    }

  g_list_free (context->pending_nulls);
  context->pending_nulls = NULL;
}

/* Use a platform-specific mechanism to look up the first argument to
 * the current process. 
 * Note if you implement this for other platforms, also add it to
 * tests/option-argv0.c
 */
static char *
platform_get_argv0 (void)
{
#if defined __linux
  char *cmdline;
  char *base_arg0;
  gsize len;

  if (!g_file_get_contents ("/proc/self/cmdline",
			    &cmdline,
			    &len,
			    NULL))
    return NULL;
  /* Sanity check for a NUL terminator. */
  if (!memchr (cmdline, 0, len))
    return NULL;
  /* We could just return cmdline, but I think it's better
   * to hold on to a smaller malloc block; the arguments
   * could be large.
   */
  base_arg0 = g_path_get_basename (cmdline);
  g_free (cmdline);
  return base_arg0;
#elif defined __OpenBSD__
  char **cmdline;
  char *base_arg0;
  gsize len;

  int mib[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };

  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &len, NULL, 0) == -1)
      return NULL;

  cmdline = g_malloc0 (len);

  if (sysctl (mib, G_N_ELEMENTS (mib), cmdline, &len, NULL, 0) == -1)
    {
      g_free (cmdline);
      return NULL;
    }

  /* We could just return cmdline, but I think it's better
   * to hold on to a smaller malloc block; the arguments
   * could be large.
   */
  base_arg0 = g_path_get_basename (*cmdline);
  g_free (cmdline);
  return base_arg0;
#endif

  return NULL;
}

/**
 * g_option_context_parse:
 * @context: a #GOptionContext
 * @argc: (inout) (optional): a pointer to the number of command line arguments
 * @argv: (inout) (array length=argc) (optional): a pointer to the array of command line arguments
 * @error: a return location for errors
 *
 * Parses the command line arguments, recognizing options
 * which have been added to @context. A side-effect of
 * calling this function is that g_set_prgname() will be
 * called.
 *
 * If the parsing is successful, any parsed arguments are
 * removed from the array and @argc and @argv are updated
 * accordingly. A '--' option is stripped from @argv
 * unless there are unparsed options before and after it,
 * or some of the options after it start with '-'. In case
 * of an error, @argc and @argv are left unmodified.
 *
 * If automatic `--help` support is enabled
 * (see g_option_context_set_help_enabled()), and the
 * @argv array contains one of the recognized help options,
 * this function will produce help output to stdout and
 * call `exit (0)`.
 *
 * Note that function depends on the [current locale][setlocale] for
 * automatic character set conversion of string and filename
 * arguments.
 *
 * Returns: %TRUE if the parsing was successful,
 *               %FALSE if an error occurred
 *
 * Since: 2.6
 **/
gboolean
g_option_context_parse (GOptionContext   *context,
                        gint             *argc,
                        gchar          ***argv,
                        GError          **error)
{
  gint i, j, k;
  GList *list;

  /* Set program name */
  if (!g_get_prgname())
    {
      gchar *prgname;

      if (argc && argv && *argc)
	prgname = g_path_get_basename ((*argv)[0]);
      else
	prgname = platform_get_argv0 ();

      if (prgname)
	g_set_prgname (prgname);
      else
	g_set_prgname ("<unknown>");

      g_free (prgname);
    }

  /* Call pre-parse hooks */
  list = context->groups;
  while (list)
    {
      GOptionGroup *group = list->data;

      if (group->pre_parse_func)
        {
          if (!(* group->pre_parse_func) (context, group,
                                          group->user_data, error))
            goto fail;
        }

      list = list->next;
    }

  if (context->main_group && context->main_group->pre_parse_func)
    {
      if (!(* context->main_group->pre_parse_func) (context, context->main_group,
                                                    context->main_group->user_data, error))
        goto fail;
    }

  if (argc && argv)
    {
      gboolean stop_parsing = FALSE;
      gboolean has_unknown = FALSE;
      gint separator_pos = 0;

      for (i = 1; i < *argc; i++)
        {
          gchar *arg, *dash;
          gboolean parsed = FALSE;

          if ((*argv)[i][0] == '-' && (*argv)[i][1] != '\0' && !stop_parsing)
            {
              if ((*argv)[i][1] == '-')
                {
                  /* -- option */

                  arg = (*argv)[i] + 2;

                  /* '--' terminates list of arguments */
                  if (*arg == 0)
                    {
                      separator_pos = i;
                      stop_parsing = TRUE;
                      continue;
                    }

                  /* Handle help options */
                  if (context->help_enabled)
                    {
                      if (strcmp (arg, "help") == 0)
                        print_help (context, TRUE, NULL);
                      else if (strcmp (arg, "help-all") == 0)
                        print_help (context, FALSE, NULL);
                      else if (strncmp (arg, "help-", 5) == 0)
                        {
                          list = context->groups;

                          while (list)
                            {
                              GOptionGroup *group = list->data;

                              if (strcmp (arg + 5, group->name) == 0)
                                print_help (context, FALSE, group);

                              list = list->next;
                            }
                        }
                    }

                  if (context->main_group &&
                      !parse_long_option (context, context->main_group, &i, arg,
                                          FALSE, argc, argv, error, &parsed))
                    goto fail;

                  if (parsed)
                    continue;

                  /* Try the groups */
                  list = context->groups;
                  while (list)
                    {
                      GOptionGroup *group = list->data;

                      if (!parse_long_option (context, group, &i, arg,
                                              FALSE, argc, argv, error, &parsed))
                        goto fail;

                      if (parsed)
                        break;

                      list = list->next;
                    }

                  if (parsed)
                    continue;

                  /* Now look for --<group>-<option> */
                  dash = strchr (arg, '-');
                  if (dash)
                    {
                      /* Try the groups */
                      list = context->groups;
                      while (list)
                        {
                          GOptionGroup *group = list->data;

                          if (strncmp (group->name, arg, dash - arg) == 0)
                            {
                              if (!parse_long_option (context, group, &i, dash + 1,
                                                      TRUE, argc, argv, error, &parsed))
                                goto fail;

                              if (parsed)
                                break;
                            }

                          list = list->next;
                        }
                    }

                  if (context->ignore_unknown)
                    continue;
                }
              else
                { /* short option */
                  gint new_i = i, arg_length;
                  gboolean *nulled_out = NULL;
                  gboolean has_h_entry = context_has_h_entry (context);
                  arg = (*argv)[i] + 1;
                  arg_length = strlen (arg);
                  nulled_out = g_newa (gboolean, arg_length);
                  memset (nulled_out, 0, arg_length * sizeof (gboolean));
                  for (j = 0; j < arg_length; j++)
                    {
                      if (context->help_enabled && (arg[j] == '?' ||
                        (arg[j] == 'h' && !has_h_entry)))
                        print_help (context, TRUE, NULL);
                      parsed = FALSE;
                      if (context->main_group &&
                          !parse_short_option (context, context->main_group,
                                               i, &new_i, arg[j],
                                               argc, argv, error, &parsed))
                        goto fail;
                      if (!parsed)
                        {
                          /* Try the groups */
                          list = context->groups;
                          while (list)
                            {
                              GOptionGroup *group = list->data;
                              if (!parse_short_option (context, group, i, &new_i, arg[j],
                                                       argc, argv, error, &parsed))
                                goto fail;
                              if (parsed)
                                break;
                              list = list->next;
                            }
                        }

                      if (context->ignore_unknown && parsed)
                        nulled_out[j] = TRUE;
                      else if (context->ignore_unknown)
                        continue;
                      else if (!parsed)
                        break;
                      /* !context->ignore_unknown && parsed */
                    }
                  if (context->ignore_unknown)
                    {
                      gchar *new_arg = NULL;
                      gint arg_index = 0;
                      for (j = 0; j < arg_length; j++)
                        {
                          if (!nulled_out[j])
                            {
                              if (!new_arg)
                                new_arg = g_malloc (arg_length + 1);
                              new_arg[arg_index++] = arg[j];
                            }
                        }
                      if (new_arg)
                        new_arg[arg_index] = '\0';
                      add_pending_null (context, &((*argv)[i]), new_arg);
                      i = new_i;
                    }
                  else if (parsed)
                    {
                      add_pending_null (context, &((*argv)[i]), NULL);
                      i = new_i;
                    }
                }

              if (!parsed)
                has_unknown = TRUE;

              if (!parsed && !context->ignore_unknown)
                {
                  g_set_error (error,
                               G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                                   _("Unknown option %s"), (*argv)[i]);
                  goto fail;
                }
            }
          else
            {
              if (context->strict_posix)
                stop_parsing = TRUE;

              /* Collect remaining args */
              if (context->main_group &&
                  !parse_remaining_arg (context, context->main_group, &i,
                                        argc, argv, error, &parsed))
                goto fail;

              if (!parsed && (has_unknown || (*argv)[i][0] == '-'))
                separator_pos = 0;
            }
        }

      if (separator_pos > 0)
        add_pending_null (context, &((*argv)[separator_pos]), NULL);

    }

  /* Call post-parse hooks */
  list = context->groups;
  while (list)
    {
      GOptionGroup *group = list->data;

      if (group->post_parse_func)
        {
          if (!(* group->post_parse_func) (context, group,
                                           group->user_data, error))
            goto fail;
        }

      list = list->next;
    }

  if (context->main_group && context->main_group->post_parse_func)
    {
      if (!(* context->main_group->post_parse_func) (context, context->main_group,
                                                     context->main_group->user_data, error))
        goto fail;
    }

  if (argc && argv)
    {
      free_pending_nulls (context, TRUE);

      for (i = 1; i < *argc; i++)
        {
          for (k = i; k < *argc; k++)
            if ((*argv)[k] != NULL)
              break;

          if (k > i)
            {
              k -= i;
              for (j = i + k; j < *argc; j++)
                {
                  (*argv)[j-k] = (*argv)[j];
                  (*argv)[j] = NULL;
                }
              *argc -= k;
            }
        }
    }

  return TRUE;

 fail:

  /* Call error hooks */
  list = context->groups;
  while (list)
    {
      GOptionGroup *group = list->data;

      if (group->error_func)
        (* group->error_func) (context, group,
                               group->user_data, error);

      list = list->next;
    }

  if (context->main_group && context->main_group->error_func)
    (* context->main_group->error_func) (context, context->main_group,
                                         context->main_group->user_data, error);

  free_changes_list (context, TRUE);
  free_pending_nulls (context, FALSE);

  return FALSE;
}

/**
 * g_option_group_new:
 * @name: the name for the option group, this is used to provide
 *   help for the options in this group with `--help-`@name
 * @description: a description for this group to be shown in
 *   `--help`. This string is translated using the translation
 *   domain or translation function of the group
 * @help_description: a description for the `--help-`@name option.
 *   This string is translated using the translation domain or translation function
 *   of the group
 * @user_data: (nullable): user data that will be passed to the pre- and post-parse hooks,
 *   the error hook and to callbacks of %G_OPTION_ARG_CALLBACK options, or %NULL
 * @destroy: (nullable): a function that will be called to free @user_data, or %NULL
 *
 * Creates a new #GOptionGroup.
 *
 * Returns: a newly created option group. It should be added
 *   to a #GOptionContext or freed with g_option_group_unref().
 *
 * Since: 2.6
 **/
GOptionGroup *
g_option_group_new (const gchar    *name,
                    const gchar    *description,
                    const gchar    *help_description,
                    gpointer        user_data,
                    GDestroyNotify  destroy)

{
  GOptionGroup *group;

  group = g_new0 (GOptionGroup, 1);
  group->ref_count = 1;
  group->name = g_strdup (name);
  group->description = g_strdup (description);
  group->help_description = g_strdup (help_description);
  group->user_data = user_data;
  group->destroy_notify = destroy;

  return group;
}


/**
 * g_option_group_free:
 * @group: a #GOptionGroup
 *
 * Frees a #GOptionGroup. Note that you must not free groups
 * which have been added to a #GOptionContext.
 *
 * Since: 2.6
 *
 * Deprecated: 2.44: Use g_option_group_unref() instead.
 */
void
g_option_group_free (GOptionGroup *group)
{
  g_option_group_unref (group);
}

/**
 * g_option_group_ref:
 * @group: a #GOptionGroup
 *
 * Increments the reference count of @group by one.
 *
 * Returns: a #GoptionGroup
 *
 * Since: 2.44
 */
GOptionGroup *
g_option_group_ref (GOptionGroup *group)
{
  g_return_val_if_fail (group != NULL, NULL);

  group->ref_count++;

  return group;
}

/**
 * g_option_group_unref:
 * @group: a #GOptionGroup
 *
 * Decrements the reference count of @group by one.
 * If the reference count drops to 0, the @group will be freed.
 * and all memory allocated by the @group is released.
 *
 * Since: 2.44
 */
void
g_option_group_unref (GOptionGroup *group)
{
  g_return_if_fail (group != NULL);

  if (--group->ref_count == 0)
    {
      g_free (group->name);
      g_free (group->description);
      g_free (group->help_description);

      g_free (group->entries);

      if (group->destroy_notify)
        (* group->destroy_notify) (group->user_data);

      if (group->translate_notify)
        (* group->translate_notify) (group->translate_data);

      g_free (group);
    }
}

/**
 * g_option_group_add_entries:
 * @group: a #GOptionGroup
 * @entries: a %NULL-terminated array of #GOptionEntrys
 *
 * Adds the options specified in @entries to @group.
 *
 * Since: 2.6
 **/
void
g_option_group_add_entries (GOptionGroup       *group,
                            const GOptionEntry *entries)
{
  gint i, n_entries;

  g_return_if_fail (entries != NULL);

  for (n_entries = 0; entries[n_entries].long_name != NULL; n_entries++) ;

  group->entries = g_renew (GOptionEntry, group->entries, group->n_entries + n_entries);

  /* group->entries could be NULL in the trivial case where we add no
   * entries to no entries */
  if (n_entries != 0)
    memcpy (group->entries + group->n_entries, entries, sizeof (GOptionEntry) * n_entries);

  for (i = group->n_entries; i < group->n_entries + n_entries; i++)
    {
      gchar c = group->entries[i].short_name;

      if (c == '-' || (c != 0 && !g_ascii_isprint (c)))
        {
          g_warning (G_STRLOC ": ignoring invalid short option '%c' (%d) in entry %s:%s",
              c, c, group->name, group->entries[i].long_name);
          group->entries[i].short_name = '\0';
        }

      if (group->entries[i].arg != G_OPTION_ARG_NONE &&
          (group->entries[i].flags & G_OPTION_FLAG_REVERSE) != 0)
        {
          g_warning (G_STRLOC ": ignoring reverse flag on option of arg-type %d in entry %s:%s",
              group->entries[i].arg, group->name, group->entries[i].long_name);

          group->entries[i].flags &= ~G_OPTION_FLAG_REVERSE;
        }

      if (group->entries[i].arg != G_OPTION_ARG_CALLBACK &&
          (group->entries[i].flags & (G_OPTION_FLAG_NO_ARG|G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_FILENAME)) != 0)
        {
          g_warning (G_STRLOC ": ignoring no-arg, optional-arg or filename flags (%d) on option of arg-type %d in entry %s:%s",
              group->entries[i].flags, group->entries[i].arg, group->name, group->entries[i].long_name);

          group->entries[i].flags &= ~(G_OPTION_FLAG_NO_ARG|G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_FILENAME);
        }
    }

  group->n_entries += n_entries;
}

/**
 * g_option_group_set_parse_hooks:
 * @group: a #GOptionGroup
 * @pre_parse_func: (nullable): a function to call before parsing, or %NULL
 * @post_parse_func: (nullable): a function to call after parsing, or %NULL
 *
 * Associates two functions with @group which will be called
 * from g_option_context_parse() before the first option is parsed
 * and after the last option has been parsed, respectively.
 *
 * Note that the user data to be passed to @pre_parse_func and
 * @post_parse_func can be specified when constructing the group
 * with g_option_group_new().
 *
 * Since: 2.6
 **/
void
g_option_group_set_parse_hooks (GOptionGroup     *group,
                                GOptionParseFunc  pre_parse_func,
                                GOptionParseFunc  post_parse_func)
{
  g_return_if_fail (group != NULL);

  group->pre_parse_func = pre_parse_func;
  group->post_parse_func = post_parse_func;
}

/**
 * g_option_group_set_error_hook:
 * @group: a #GOptionGroup
 * @error_func: a function to call when an error occurs
 *
 * Associates a function with @group which will be called
 * from g_option_context_parse() when an error occurs.
 *
 * Note that the user data to be passed to @error_func can be
 * specified when constructing the group with g_option_group_new().
 *
 * Since: 2.6
 **/
void
g_option_group_set_error_hook (GOptionGroup     *group,
                               GOptionErrorFunc  error_func)
{
  g_return_if_fail (group != NULL);

  group->error_func = error_func;
}


/**
 * g_option_group_set_translate_func:
 * @group: a #GOptionGroup
 * @func: (nullable): the #GTranslateFunc, or %NULL
 * @data: (nullable): user data to pass to @func, or %NULL
 * @destroy_notify: (nullable): a function which gets called to free @data, or %NULL
 *
 * Sets the function which is used to translate user-visible strings,
 * for `--help` output. Different groups can use different
 * #GTranslateFuncs. If @func is %NULL, strings are not translated.
 *
 * If you are using gettext(), you only need to set the translation
 * domain, see g_option_group_set_translation_domain().
 *
 * Since: 2.6
 **/
void
g_option_group_set_translate_func (GOptionGroup   *group,
                                   GTranslateFunc  func,
                                   gpointer        data,
                                   GDestroyNotify  destroy_notify)
{
  g_return_if_fail (group != NULL);

  if (group->translate_notify)
    group->translate_notify (group->translate_data);

  group->translate_func = func;
  group->translate_data = data;
  group->translate_notify = destroy_notify;
}

static const gchar *
dgettext_swapped (const gchar *msgid,
                  const gchar *domainname)
{
  return g_dgettext (domainname, msgid);
}

/**
 * g_option_group_set_translation_domain:
 * @group: a #GOptionGroup
 * @domain: the domain to use
 *
 * A convenience function to use gettext() for translating
 * user-visible strings.
 *
 * Since: 2.6
 **/
void
g_option_group_set_translation_domain (GOptionGroup *group,
                                       const gchar  *domain)
{
  g_return_if_fail (group != NULL);

  g_option_group_set_translate_func (group,
                                     (GTranslateFunc)dgettext_swapped,
                                     g_strdup (domain),
                                     g_free);
}

/**
 * g_option_context_set_translate_func:
 * @context: a #GOptionContext
 * @func: (nullable): the #GTranslateFunc, or %NULL
 * @data: (nullable): user data to pass to @func, or %NULL
 * @destroy_notify: (nullable): a function which gets called to free @data, or %NULL
 *
 * Sets the function which is used to translate the contexts
 * user-visible strings, for `--help` output. If @func is %NULL,
 * strings are not translated.
 *
 * Note that option groups have their own translation functions,
 * this function only affects the @parameter_string (see g_option_context_new()),
 * the summary (see g_option_context_set_summary()) and the description
 * (see g_option_context_set_description()).
 *
 * If you are using gettext(), you only need to set the translation
 * domain, see g_option_context_set_translation_domain().
 *
 * Since: 2.12
 **/
void
g_option_context_set_translate_func (GOptionContext *context,
                                     GTranslateFunc func,
                                     gpointer       data,
                                     GDestroyNotify destroy_notify)
{
  g_return_if_fail (context != NULL);

  if (context->translate_notify)
    context->translate_notify (context->translate_data);

  context->translate_func = func;
  context->translate_data = data;
  context->translate_notify = destroy_notify;
}

/**
 * g_option_context_set_translation_domain:
 * @context: a #GOptionContext
 * @domain: the domain to use
 *
 * A convenience function to use gettext() for translating
 * user-visible strings.
 *
 * Since: 2.12
 **/
void
g_option_context_set_translation_domain (GOptionContext *context,
                                         const gchar     *domain)
{
  g_return_if_fail (context != NULL);

  g_option_context_set_translate_func (context,
                                       (GTranslateFunc)dgettext_swapped,
                                       g_strdup (domain),
                                       g_free);
}

/**
 * g_option_context_set_summary:
 * @context: a #GOptionContext
 * @summary: (nullable): a string to be shown in `--help` output
 *  before the list of options, or %NULL
 *
 * Adds a string to be displayed in `--help` output before the list
 * of options. This is typically a summary of the program functionality.
 *
 * Note that the summary is translated (see
 * g_option_context_set_translate_func() and
 * g_option_context_set_translation_domain()).
 *
 * Since: 2.12
 */
void
g_option_context_set_summary (GOptionContext *context,
                              const gchar    *summary)
{
  g_return_if_fail (context != NULL);

  g_free (context->summary);
  context->summary = g_strdup (summary);
}


/**
 * g_option_context_get_summary:
 * @context: a #GOptionContext
 *
 * Returns the summary. See g_option_context_set_summary().
 *
 * Returns: the summary
 *
 * Since: 2.12
 */
const gchar *
g_option_context_get_summary (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);

  return context->summary;
}

/**
 * g_option_context_set_description:
 * @context: a #GOptionContext
 * @description: (nullable): a string to be shown in `--help` output
 *   after the list of options, or %NULL
 *
 * Adds a string to be displayed in `--help` output after the list
 * of options. This text often includes a bug reporting address.
 *
 * Note that the summary is translated (see
 * g_option_context_set_translate_func()).
 *
 * Since: 2.12
 */
void
g_option_context_set_description (GOptionContext *context,
                                  const gchar    *description)
{
  g_return_if_fail (context != NULL);

  g_free (context->description);
  context->description = g_strdup (description);
}


/**
 * g_option_context_get_description:
 * @context: a #GOptionContext
 *
 * Returns the description. See g_option_context_set_description().
 *
 * Returns: the description
 *
 * Since: 2.12
 */
const gchar *
g_option_context_get_description (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);

  return context->description;
}

/**
 * g_option_context_parse_strv:
 * @context: a #GOptionContext
 * @arguments: (inout) (array null-terminated=1): a pointer to the
 *    command line arguments (which must be in UTF-8 on Windows)
 * @error: a return location for errors
 *
 * Parses the command line arguments.
 *
 * This function is similar to g_option_context_parse() except that it
 * respects the normal memory rules when dealing with a strv instead of
 * assuming that the passed-in array is the argv of the main function.
 *
 * In particular, strings that are removed from the arguments list will
 * be freed using g_free().
 *
 * On Windows, the strings are expected to be in UTF-8.  This is in
 * contrast to g_option_context_parse() which expects them to be in the
 * system codepage, which is how they are passed as @argv to main().
 * See g_win32_get_command_line() for a solution.
 *
 * This function is useful if you are trying to use #GOptionContext with
 * #GApplication.
 *
 * Returns: %TRUE if the parsing was successful,
 *          %FALSE if an error occurred
 *
 * Since: 2.40
 **/
gboolean
g_option_context_parse_strv (GOptionContext   *context,
                             gchar          ***arguments,
                             GError          **error)
{
  gboolean success;
  gint argc;

  context->strv_mode = TRUE;
  argc = g_strv_length (*arguments);
  success = g_option_context_parse (context, &argc, arguments, error);
  context->strv_mode = FALSE;

  return success;
}
