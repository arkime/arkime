/* gkeyfile.c - key file parser
 *
 *  Copyright 2004  Red Hat, Inc.  
 *  Copyright 2009-2010  Collabora Ltd.
 *  Copyright 2009  Nokia Corporation
 *
 * Written by Ray Strode <rstrode@redhat.com>
 *            Matthias Clasen <mclasen@redhat.com>
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

#include "config.h"

#include "gkeyfile.h"
#include "gutils.h"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>

#undef fstat
#define fstat(a,b) _fstati64(a,b)
#undef stat
#define stat _stati64

#ifndef S_ISREG
#define S_ISREG(mode) ((mode)&_S_IFREG)
#endif

#endif  /* G_OS_WIN23 */

#include "gconvert.h"
#include "gdataset.h"
#include "gerror.h"
#include "gfileutils.h"
#include "ghash.h"
#include "glibintl.h"
#include "glist.h"
#include "gslist.h"
#include "gmem.h"
#include "gmessages.h"
#include "gstdio.h"
#include "gstring.h"
#include "gstrfuncs.h"
#include "gutils.h"


/**
 * SECTION:keyfile
 * @title: Key-value file parser
 * @short_description: parses .ini-like config files
 *
 * #GKeyFile lets you parse, edit or create files containing groups of
 * key-value pairs, which we call "key files" for lack of a better name.
 * Several freedesktop.org specifications use key files now, e.g the
 * [Desktop Entry Specification](http://freedesktop.org/Standards/desktop-entry-spec)
 * and the
 * [Icon Theme Specification](http://freedesktop.org/Standards/icon-theme-spec).
 *
 * The syntax of key files is described in detail in the
 * [Desktop Entry Specification](http://freedesktop.org/Standards/desktop-entry-spec),
 * here is a quick summary: Key files
 * consists of groups of key-value pairs, interspersed with comments.
 *
 * |[
 * # this is just an example
 * # there can be comments before the first group
 *
 * [First Group]
 *
 * Name=Key File Example\tthis value shows\nescaping
 *
 * # localized strings are stored in multiple key-value pairs
 * Welcome=Hello
 * Welcome[de]=Hallo
 * Welcome[fr_FR]=Bonjour
 * Welcome[it]=Ciao
 * Welcome[be@latin]=Hello
 *
 * [Another Group]
 *
 * Numbers=2;20;-200;0
 *
 * Booleans=true;false;true;true
 * ]|
 *
 * Lines beginning with a '#' and blank lines are considered comments.
 *
 * Groups are started by a header line containing the group name enclosed
 * in '[' and ']', and ended implicitly by the start of the next group or
 * the end of the file. Each key-value pair must be contained in a group.
 *
 * Key-value pairs generally have the form `key=value`, with the
 * exception of localized strings, which have the form
 * `key[locale]=value`, with a locale identifier of the
 * form `lang_COUNTRY@MODIFIER` where `COUNTRY` and `MODIFIER`
 * are optional.
 * Space before and after the '=' character are ignored. Newline, tab,
 * carriage return and backslash characters in value are escaped as \n,
 * \t, \r, and \\\\, respectively. To preserve leading spaces in values,
 * these can also be escaped as \s.
 *
 * Key files can store strings (possibly with localized variants), integers,
 * booleans and lists of these. Lists are separated by a separator character,
 * typically ';' or ','. To use the list separator character in a value in
 * a list, it has to be escaped by prefixing it with a backslash.
 *
 * This syntax is obviously inspired by the .ini files commonly met
 * on Windows, but there are some important differences:
 *
 * - .ini files use the ';' character to begin comments,
 *   key files use the '#' character.
 *
 * - Key files do not allow for ungrouped keys meaning only
 *   comments can precede the first group.
 *
 * - Key files are always encoded in UTF-8.
 *
 * - Key and Group names are case-sensitive. For example, a group called
 *   [GROUP] is a different from [group].
 *
 * - .ini files don't have a strongly typed boolean entry type,
 *    they only have GetProfileInt(). In key files, only
 *    true and false (in lower case) are allowed.
 *
 * Note that in contrast to the
 * [Desktop Entry Specification](http://freedesktop.org/Standards/desktop-entry-spec),
 * groups in key files may contain the same
 * key multiple times; the last entry wins. Key files may also contain
 * multiple groups with the same name; they are merged together.
 * Another difference is that keys and group names in key files are not
 * restricted to ASCII characters.
 *
 * Here is an example of loading a key file and reading a value:
 * |[<!-- language="C" -->
 * g_autoptr(GError) error = NULL;
 * g_autoptr(GKeyFile) key_file = g_key_file_new ();
 *
 * if (!g_key_file_load_from_file (key_file, "key-file.ini", flags, &error))
 *   {
 *     if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
 *       g_warning ("Error loading key file: %s", error->message);
 *     return;
 *   }
 *
 * g_autofree gchar *val = g_key_file_get_string (key_file, "Group Name", "SomeKey", &error);
 * if (val == NULL &&
 *     !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
 *   {
 *     g_warning ("Error finding key in key file: %s", error->message);
 *     return;
 *   }
 * else if (val == NULL)
 *   {
 *     // Fall back to a default value.
 *     val = g_strdup ("default-value");
 *   }
 * ]|
 *
 * Here is an example of creating and saving a key file:
 * |[<!-- language="C" -->
 * g_autoptr(GKeyFile) key_file = g_key_file_new ();
 * const gchar *val = …;
 * g_autoptr(GError) error = NULL;
 *
 * g_key_file_set_string (key_file, "Group Name", "SomeKey", val);
 *
 * // Save as a file.
 * if (!g_key_file_save_to_file (key_file, "key-file.ini", &error))
 *   {
 *     g_warning ("Error saving key file: %s", error->message);
 *     return;
 *   }
 *
 * // Or store to a GBytes for use elsewhere.
 * gsize data_len;
 * g_autofree guint8 *data = (guint8 *) g_key_file_to_data (key_file, &data_len, &error);
 * if (data == NULL)
 *   {
 *     g_warning ("Error saving key file: %s", error->message);
 *     return;
 *   }
 * g_autoptr(GBytes) bytes = g_bytes_new_take (g_steal_pointer (&data), data_len);
 * ]|
 */

/**
 * G_KEY_FILE_ERROR:
 *
 * Error domain for key file parsing. Errors in this domain will
 * be from the #GKeyFileError enumeration.
 *
 * See #GError for information on error domains.
 */

/**
 * GKeyFileError:
 * @G_KEY_FILE_ERROR_UNKNOWN_ENCODING: the text being parsed was in
 *     an unknown encoding
 * @G_KEY_FILE_ERROR_PARSE: document was ill-formed
 * @G_KEY_FILE_ERROR_NOT_FOUND: the file was not found
 * @G_KEY_FILE_ERROR_KEY_NOT_FOUND: a requested key was not found
 * @G_KEY_FILE_ERROR_GROUP_NOT_FOUND: a requested group was not found
 * @G_KEY_FILE_ERROR_INVALID_VALUE: a value could not be parsed
 *
 * Error codes returned by key file parsing.
 */

/**
 * GKeyFileFlags:
 * @G_KEY_FILE_NONE: No flags, default behaviour
 * @G_KEY_FILE_KEEP_COMMENTS: Use this flag if you plan to write the
 *     (possibly modified) contents of the key file back to a file;
 *     otherwise all comments will be lost when the key file is
 *     written back.
 * @G_KEY_FILE_KEEP_TRANSLATIONS: Use this flag if you plan to write the
 *     (possibly modified) contents of the key file back to a file;
 *     otherwise only the translations for the current language will be
 *     written back.
 *
 * Flags which influence the parsing.
 */

/**
 * G_KEY_FILE_DESKTOP_GROUP:
 *
 * The name of the main group of a desktop entry file, as defined in the
 * [Desktop Entry Specification](http://freedesktop.org/Standards/desktop-entry-spec).
 * Consult the specification for more
 * details about the meanings of the keys below.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_TYPE:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string
 * giving the type of the desktop entry. Usually
 * #G_KEY_FILE_DESKTOP_TYPE_APPLICATION,
 * #G_KEY_FILE_DESKTOP_TYPE_LINK, or
 * #G_KEY_FILE_DESKTOP_TYPE_DIRECTORY.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_VERSION:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string
 * giving the version of the Desktop Entry Specification used for
 * the desktop entry file.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_NAME:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a localized
 * string giving the specific name of the desktop entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a localized
 * string giving the generic name of the desktop entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a boolean
 * stating whether the desktop entry should be shown in menus.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_COMMENT:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a localized
 * string giving the tooltip for the desktop entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_ICON:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a localized
 * string giving the name of the icon to be displayed for the desktop
 * entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_HIDDEN:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a boolean
 * stating whether the desktop entry has been deleted by the user.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a list of
 * strings identifying the environments that should display the
 * desktop entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a list of
 * strings identifying the environments that should not display the
 * desktop entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_TRY_EXEC:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string
 * giving the file name of a binary on disk used to determine if the
 * program is actually installed. It is only valid for desktop entries
 * with the `Application` type.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_EXEC:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string
 * giving the command line to execute. It is only valid for desktop
 * entries with the `Application` type.
 *
 * Since: 2.14
 */

 /**
  * G_KEY_FILE_DESKTOP_KEY_PATH:
  *
  * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string
  * containing the working directory to run the program in. It is only
  * valid for desktop entries with the `Application` type.
  *
  * Since: 2.14
  */

/**
 * G_KEY_FILE_DESKTOP_KEY_TERMINAL:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a boolean
 * stating whether the program should be run in a terminal window.
 * It is only valid for desktop entries with the
 * `Application` type.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_MIME_TYPE:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a list
 * of strings giving the MIME types supported by this desktop entry.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_CATEGORIES:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a list
 * of strings giving the categories in which the desktop entry
 * should be shown in a menu.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a boolean
 * stating whether the application supports the
 * [Startup Notification Protocol Specification](http://www.freedesktop.org/Standards/startup-notification-spec).
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is string
 * identifying the WM class or name hint of a window that the application
 * will create, which can be used to emulate Startup Notification with
 * older applications.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_URL:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string
 * giving the URL to access. It is only valid for desktop entries
 * with the `Link` type.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_DBUS_ACTIVATABLE:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a boolean set to true
 * if the application is D-Bus activatable.
 *
 * Since: 2.38
 */

/**
 * G_KEY_FILE_DESKTOP_KEY_ACTIONS:
 *
 * A key under #G_KEY_FILE_DESKTOP_GROUP, whose value is a string list
 * giving the available application actions.
 *
 * Since: 2.38
 */

/**
 * G_KEY_FILE_DESKTOP_TYPE_APPLICATION:
 *
 * The value of the #G_KEY_FILE_DESKTOP_KEY_TYPE, key for desktop
 * entries representing applications.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_TYPE_LINK:
 *
 * The value of the #G_KEY_FILE_DESKTOP_KEY_TYPE, key for desktop
 * entries representing links to documents.
 *
 * Since: 2.14
 */

/**
 * G_KEY_FILE_DESKTOP_TYPE_DIRECTORY:
 *
 * The value of the #G_KEY_FILE_DESKTOP_KEY_TYPE, key for desktop
 * entries representing directories.
 *
 * Since: 2.14
 */

typedef struct _GKeyFileGroup GKeyFileGroup;

/**
 * GKeyFile:
 *
 * The GKeyFile struct contains only private data
 * and should not be accessed directly.
 */
struct _GKeyFile
{
  GList *groups;
  GHashTable *group_hash;

  GKeyFileGroup *start_group;
  GKeyFileGroup *current_group;

  GString *parse_buffer; /* Holds up to one line of not-yet-parsed data */

  gchar list_separator;

  GKeyFileFlags flags;

  gchar **locales;

  volatile gint ref_count;
};

typedef struct _GKeyFileKeyValuePair GKeyFileKeyValuePair;

struct _GKeyFileGroup
{
  const gchar *name;  /* NULL for above first group (which will be comments) */

  GKeyFileKeyValuePair *comment; /* Special comment that is stuck to the top of a group */

  GList *key_value_pairs;

  /* Used in parallel with key_value_pairs for
   * increased lookup performance
   */
  GHashTable *lookup_map;
};

struct _GKeyFileKeyValuePair
{
  gchar *key;  /* NULL for comments */
  gchar *value;
};

static gint                  find_file_in_data_dirs            (const gchar            *file,
								const gchar           **data_dirs,
								gchar                 **output_file,
								GError                **error);
static gboolean              g_key_file_load_from_fd           (GKeyFile               *key_file,
								gint                    fd,
								GKeyFileFlags           flags,
								GError                **error);
static GList                *g_key_file_lookup_group_node      (GKeyFile               *key_file,
			                                        const gchar            *group_name);
static GKeyFileGroup        *g_key_file_lookup_group           (GKeyFile               *key_file,
								const gchar            *group_name);

static GList                *g_key_file_lookup_key_value_pair_node  (GKeyFile       *key_file,
			                                             GKeyFileGroup  *group,
                                                                     const gchar    *key);
static GKeyFileKeyValuePair *g_key_file_lookup_key_value_pair       (GKeyFile       *key_file,
                                                                     GKeyFileGroup  *group,
                                                                     const gchar    *key);

static void                  g_key_file_remove_group_node          (GKeyFile      *key_file,
							  	    GList         *group_node);
static void                  g_key_file_remove_key_value_pair_node (GKeyFile      *key_file,
                                                                    GKeyFileGroup *group,
                                                                    GList         *pair_node);

static void                  g_key_file_add_key_value_pair     (GKeyFile               *key_file,
                                                                GKeyFileGroup          *group,
                                                                GKeyFileKeyValuePair   *pair);
static void                  g_key_file_add_key                (GKeyFile               *key_file,
								GKeyFileGroup          *group,
								const gchar            *key,
								const gchar            *value);
static void                  g_key_file_add_group              (GKeyFile               *key_file,
								const gchar            *group_name);
static gboolean              g_key_file_is_group_name          (const gchar *name);
static gboolean              g_key_file_is_key_name            (const gchar *name);
static void                  g_key_file_key_value_pair_free    (GKeyFileKeyValuePair   *pair);
static gboolean              g_key_file_line_is_comment        (const gchar            *line);
static gboolean              g_key_file_line_is_group          (const gchar            *line);
static gboolean              g_key_file_line_is_key_value_pair (const gchar            *line);
static gchar                *g_key_file_parse_value_as_string  (GKeyFile               *key_file,
								const gchar            *value,
								GSList                **separators,
								GError                **error);
static gchar                *g_key_file_parse_string_as_value  (GKeyFile               *key_file,
								const gchar            *string,
								gboolean                escape_separator);
static gint                  g_key_file_parse_value_as_integer (GKeyFile               *key_file,
								const gchar            *value,
								GError                **error);
static gchar                *g_key_file_parse_integer_as_value (GKeyFile               *key_file,
								gint                    value);
static gdouble               g_key_file_parse_value_as_double  (GKeyFile               *key_file,
                                                                const gchar            *value,
                                                                GError                **error);
static gboolean              g_key_file_parse_value_as_boolean (GKeyFile               *key_file,
								const gchar            *value,
								GError                **error);
static gchar                *g_key_file_parse_boolean_as_value (GKeyFile               *key_file,
								gboolean                value);
static gchar                *g_key_file_parse_value_as_comment (GKeyFile               *key_file,
                                                                const gchar            *value);
static gchar                *g_key_file_parse_comment_as_value (GKeyFile               *key_file,
                                                                const gchar            *comment);
static void                  g_key_file_parse_key_value_pair   (GKeyFile               *key_file,
								const gchar            *line,
								gsize                   length,
								GError                **error);
static void                  g_key_file_parse_comment          (GKeyFile               *key_file,
								const gchar            *line,
								gsize                   length,
								GError                **error);
static void                  g_key_file_parse_group            (GKeyFile               *key_file,
								const gchar            *line,
								gsize                   length,
								GError                **error);
static gchar                *key_get_locale                    (const gchar            *key);
static void                  g_key_file_parse_data             (GKeyFile               *key_file,
								const gchar            *data,
								gsize                   length,
								GError                **error);
static void                  g_key_file_flush_parse_buffer     (GKeyFile               *key_file,
								GError                **error);

G_DEFINE_QUARK (g-key-file-error-quark, g_key_file_error)

static void
g_key_file_init (GKeyFile *key_file)
{  
  key_file->current_group = g_slice_new0 (GKeyFileGroup);
  key_file->groups = g_list_prepend (NULL, key_file->current_group);
  key_file->group_hash = g_hash_table_new (g_str_hash, g_str_equal);
  key_file->start_group = NULL;
  key_file->parse_buffer = g_string_sized_new (128);
  key_file->list_separator = ';';
  key_file->flags = 0;
  key_file->locales = g_strdupv ((gchar **)g_get_language_names ());
}

static void
g_key_file_clear (GKeyFile *key_file)
{
  GList *tmp, *group_node;

  if (key_file->locales) 
    {
      g_strfreev (key_file->locales);
      key_file->locales = NULL;
    }

  if (key_file->parse_buffer)
    {
      g_string_free (key_file->parse_buffer, TRUE);
      key_file->parse_buffer = NULL;
    }

  tmp = key_file->groups;
  while (tmp != NULL)
    {
      group_node = tmp;
      tmp = tmp->next;
      g_key_file_remove_group_node (key_file, group_node);
    }

  if (key_file->group_hash != NULL)
    {
      g_hash_table_destroy (key_file->group_hash);
      key_file->group_hash = NULL;
    }

  g_warn_if_fail (key_file->groups == NULL);
}


/**
 * g_key_file_new:
 *
 * Creates a new empty #GKeyFile object. Use
 * g_key_file_load_from_file(), g_key_file_load_from_data(),
 * g_key_file_load_from_dirs() or g_key_file_load_from_data_dirs() to
 * read an existing key file.
 *
 * Returns: (transfer full): an empty #GKeyFile.
 *
 * Since: 2.6
 **/
GKeyFile *
g_key_file_new (void)
{
  GKeyFile *key_file;

  key_file = g_slice_new0 (GKeyFile);
  key_file->ref_count = 1;
  g_key_file_init (key_file);

  return key_file;
}

/**
 * g_key_file_set_list_separator:
 * @key_file: a #GKeyFile 
 * @separator: the separator
 *
 * Sets the character which is used to separate
 * values in lists. Typically ';' or ',' are used
 * as separators. The default list separator is ';'.
 *
 * Since: 2.6
 */
void
g_key_file_set_list_separator (GKeyFile *key_file,
			       gchar     separator)
{
  g_return_if_fail (key_file != NULL);

  key_file->list_separator = separator;
}


/* Iterates through all the directories in *dirs trying to
 * open file.  When it successfully locates and opens a file it
 * returns the file descriptor to the open file.  It also
 * outputs the absolute path of the file in output_file.
 */
static gint
find_file_in_data_dirs (const gchar   *file,
                        const gchar  **dirs,
                        gchar        **output_file,
                        GError       **error)
{
  const gchar **data_dirs, *data_dir;
  gchar *path;
  gint fd;

  path = NULL;
  fd = -1;

  if (dirs == NULL)
    return fd;

  data_dirs = dirs;

  while (data_dirs && (data_dir = *data_dirs) && fd == -1)
    {
      gchar *candidate_file, *sub_dir;

      candidate_file = (gchar *) file;
      sub_dir = g_strdup ("");
      while (candidate_file != NULL && fd == -1)
        {
          gchar *p;

          path = g_build_filename (data_dir, sub_dir,
                                   candidate_file, NULL);

          fd = g_open (path, O_RDONLY, 0);

          if (fd == -1)
            {
              g_free (path);
              path = NULL;
            }

          candidate_file = strchr (candidate_file, '-');

          if (candidate_file == NULL)
            break;

          candidate_file++;

          g_free (sub_dir);
          sub_dir = g_strndup (file, candidate_file - file - 1);

          for (p = sub_dir; *p != '\0'; p++)
            {
              if (*p == '-')
                *p = G_DIR_SEPARATOR;
            }
        }
      g_free (sub_dir);
      data_dirs++;
    }

  if (fd == -1)
    {
      g_set_error_literal (error, G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_NOT_FOUND,
                           _("Valid key file could not be "
                             "found in search dirs"));
    }

  if (output_file != NULL && fd != -1)
    *output_file = g_strdup (path);

  g_free (path);

  return fd;
}

static gboolean
g_key_file_load_from_fd (GKeyFile       *key_file,
			 gint            fd,
			 GKeyFileFlags   flags,
			 GError        **error)
{
  GError *key_file_error = NULL;
  gssize bytes_read;
  struct stat stat_buf;
  gchar read_buf[4096];
  gchar list_separator;

  if (fstat (fd, &stat_buf) < 0)
    {
      int errsv = errno;
      g_set_error_literal (error, G_FILE_ERROR,
                           g_file_error_from_errno (errsv),
                           g_strerror (errsv));
      return FALSE;
    }

  if (!S_ISREG (stat_buf.st_mode))
    {
      g_set_error_literal (error, G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_PARSE,
                           _("Not a regular file"));
      return FALSE;
    }

  list_separator = key_file->list_separator;
  g_key_file_clear (key_file);
  g_key_file_init (key_file);
  key_file->list_separator = list_separator;
  key_file->flags = flags;

  do
    {
      int errsv;

      bytes_read = read (fd, read_buf, 4096);
      errsv = errno;

      if (bytes_read == 0)  /* End of File */
        break;

      if (bytes_read < 0)
        {
          if (errsv == EINTR || errsv == EAGAIN)
            continue;

          g_set_error_literal (error, G_FILE_ERROR,
                               g_file_error_from_errno (errsv),
                               g_strerror (errsv));
          return FALSE;
        }

      g_key_file_parse_data (key_file,
			     read_buf, bytes_read,
			     &key_file_error);
    }
  while (!key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return FALSE;
    }

  g_key_file_flush_parse_buffer (key_file, &key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * g_key_file_load_from_file:
 * @key_file: an empty #GKeyFile struct
 * @file: (type filename): the path of a filename to load, in the GLib filename encoding
 * @flags: flags from #GKeyFileFlags
 * @error: return location for a #GError, or %NULL
 *
 * Loads a key file into an empty #GKeyFile structure.
 *
 * If the OS returns an error when opening or reading the file, a
 * %G_FILE_ERROR is returned. If there is a problem parsing the file, a
 * %G_KEY_FILE_ERROR is returned.
 *
 * This function will never return a %G_KEY_FILE_ERROR_NOT_FOUND error. If the
 * @file is not found, %G_FILE_ERROR_NOENT is returned.
 *
 * Returns: %TRUE if a key file could be loaded, %FALSE otherwise
 *
 * Since: 2.6
 **/
gboolean
g_key_file_load_from_file (GKeyFile       *key_file,
			   const gchar    *file,
			   GKeyFileFlags   flags,
			   GError        **error)
{
  GError *key_file_error = NULL;
  gint fd;
  int errsv;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (file != NULL, FALSE);

  fd = g_open (file, O_RDONLY, 0);
  errsv = errno;

  if (fd == -1)
    {
      g_set_error_literal (error, G_FILE_ERROR,
                           g_file_error_from_errno (errsv),
                           g_strerror (errsv));
      return FALSE;
    }

  g_key_file_load_from_fd (key_file, fd, flags, &key_file_error);
  close (fd);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * g_key_file_load_from_data:
 * @key_file: an empty #GKeyFile struct
 * @data: key file loaded in memory
 * @length: the length of @data in bytes (or (gsize)-1 if data is nul-terminated)
 * @flags: flags from #GKeyFileFlags
 * @error: return location for a #GError, or %NULL
 *
 * Loads a key file from memory into an empty #GKeyFile structure.  
 * If the object cannot be created then %error is set to a #GKeyFileError. 
 *
 * Returns: %TRUE if a key file could be loaded, %FALSE otherwise
 *
 * Since: 2.6
 **/
gboolean
g_key_file_load_from_data (GKeyFile       *key_file,
			   const gchar    *data,
			   gsize           length,
			   GKeyFileFlags   flags,
			   GError        **error)
{
  GError *key_file_error = NULL;
  gchar list_separator;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (data != NULL || length == 0, FALSE);

  if (length == (gsize)-1)
    length = strlen (data);

  list_separator = key_file->list_separator;
  g_key_file_clear (key_file);
  g_key_file_init (key_file);
  key_file->list_separator = list_separator;
  key_file->flags = flags;

  g_key_file_parse_data (key_file, data, length, &key_file_error);
  
  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return FALSE;
    }

  g_key_file_flush_parse_buffer (key_file, &key_file_error);
  
  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * g_key_file_load_from_bytes:
 * @key_file: an empty #GKeyFile struct
 * @bytes: a #GBytes
 * @flags: flags from #GKeyFileFlags
 * @error: return location for a #GError, or %NULL
 *
 * Loads a key file from the data in @bytes into an empty #GKeyFile structure.
 * If the object cannot be created then %error is set to a #GKeyFileError.
 *
 * Returns: %TRUE if a key file could be loaded, %FALSE otherwise
 *
 * Since: 2.50
 **/
gboolean
g_key_file_load_from_bytes (GKeyFile       *key_file,
                            GBytes         *bytes,
                            GKeyFileFlags   flags,
                            GError        **error)
{
  const guchar *data;
  gsize size;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (bytes != NULL, FALSE);

  data = g_bytes_get_data (bytes, &size);
  return g_key_file_load_from_data (key_file, (const gchar *) data, size, flags, error);
}

/**
 * g_key_file_load_from_dirs:
 * @key_file: an empty #GKeyFile struct
 * @file: (type filename): a relative path to a filename to open and parse
 * @search_dirs: (array zero-terminated=1) (element-type filename): %NULL-terminated array of directories to search
 * @full_path: (out) (type filename) (optional): return location for a string containing the full path
 *   of the file, or %NULL
 * @flags: flags from #GKeyFileFlags
 * @error: return location for a #GError, or %NULL
 *
 * This function looks for a key file named @file in the paths
 * specified in @search_dirs, loads the file into @key_file and
 * returns the file's full path in @full_path.
 *
 * If the file could not be found in any of the @search_dirs,
 * %G_KEY_FILE_ERROR_NOT_FOUND is returned. If
 * the file is found but the OS returns an error when opening or reading the
 * file, a %G_FILE_ERROR is returned. If there is a problem parsing the file, a
 * %G_KEY_FILE_ERROR is returned.
 *
 * Returns: %TRUE if a key file could be loaded, %FALSE otherwise
 *
 * Since: 2.14
 **/
gboolean
g_key_file_load_from_dirs (GKeyFile       *key_file,
                           const gchar    *file,
                           const gchar   **search_dirs,
                           gchar         **full_path,
                           GKeyFileFlags   flags,
                           GError        **error)
{
  GError *key_file_error = NULL;
  const gchar **data_dirs;
  gchar *output_path;
  gint fd;
  gboolean found_file;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (!g_path_is_absolute (file), FALSE);
  g_return_val_if_fail (search_dirs != NULL, FALSE);

  found_file = FALSE;
  data_dirs = search_dirs;
  output_path = NULL;
  while (*data_dirs != NULL && !found_file)
    {
      g_free (output_path);
      output_path = NULL;

      fd = find_file_in_data_dirs (file, data_dirs, &output_path,
                                   &key_file_error);

      if (fd == -1)
        {
          if (key_file_error)
            g_propagate_error (error, key_file_error);
 	  break;
        }

      found_file = g_key_file_load_from_fd (key_file, fd, flags,
	                                    &key_file_error);
      close (fd);

      if (key_file_error)
        {
	  g_propagate_error (error, key_file_error);
	  break;
        }
    }

  if (found_file && full_path)
    *full_path = output_path;
  else
    g_free (output_path);

  return found_file;
}

/**
 * g_key_file_load_from_data_dirs:
 * @key_file: an empty #GKeyFile struct
 * @file: (type filename): a relative path to a filename to open and parse
 * @full_path: (out) (type filename) (optional): return location for a string containing the full path
 *   of the file, or %NULL
 * @flags: flags from #GKeyFileFlags 
 * @error: return location for a #GError, or %NULL
 *
 * This function looks for a key file named @file in the paths 
 * returned from g_get_user_data_dir() and g_get_system_data_dirs(), 
 * loads the file into @key_file and returns the file's full path in 
 * @full_path.  If the file could not be loaded then an %error is
 * set to either a #GFileError or #GKeyFileError.
 *
 * Returns: %TRUE if a key file could be loaded, %FALSE othewise
 * Since: 2.6
 **/
gboolean
g_key_file_load_from_data_dirs (GKeyFile       *key_file,
				const gchar    *file,
				gchar         **full_path,
				GKeyFileFlags   flags,
				GError        **error)
{
  gchar **all_data_dirs;
  const gchar * user_data_dir;
  const gchar * const * system_data_dirs;
  gsize i, j;
  gboolean found_file;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (!g_path_is_absolute (file), FALSE);

  user_data_dir = g_get_user_data_dir ();
  system_data_dirs = g_get_system_data_dirs ();
  all_data_dirs = g_new (gchar *, g_strv_length ((gchar **)system_data_dirs) + 2);

  i = 0;
  all_data_dirs[i++] = g_strdup (user_data_dir);

  j = 0;
  while (system_data_dirs[j] != NULL)
    all_data_dirs[i++] = g_strdup (system_data_dirs[j++]);
  all_data_dirs[i] = NULL;

  found_file = g_key_file_load_from_dirs (key_file,
                                          file,
                                          (const gchar **)all_data_dirs,
                                          full_path,
                                          flags,
                                          error);

  g_strfreev (all_data_dirs);

  return found_file;
}

/**
 * g_key_file_ref: (skip)
 * @key_file: a #GKeyFile
 *
 * Increases the reference count of @key_file.
 *
 * Returns: the same @key_file.
 *
 * Since: 2.32
 **/
GKeyFile *
g_key_file_ref (GKeyFile *key_file)
{
  g_return_val_if_fail (key_file != NULL, NULL);

  g_atomic_int_inc (&key_file->ref_count);

  return key_file;
}

/**
 * g_key_file_free: (skip)
 * @key_file: a #GKeyFile
 *
 * Clears all keys and groups from @key_file, and decreases the
 * reference count by 1. If the reference count reaches zero,
 * frees the key file and all its allocated memory.
 *
 * Since: 2.6
 **/
void
g_key_file_free (GKeyFile *key_file)
{
  g_return_if_fail (key_file != NULL);

  g_key_file_clear (key_file);
  g_key_file_unref (key_file);
}

/**
 * g_key_file_unref:
 * @key_file: a #GKeyFile
 *
 * Decreases the reference count of @key_file by 1. If the reference count
 * reaches zero, frees the key file and all its allocated memory.
 *
 * Since: 2.32
 **/
void
g_key_file_unref (GKeyFile *key_file)
{
  g_return_if_fail (key_file != NULL);

  if (g_atomic_int_dec_and_test (&key_file->ref_count))
    {
      g_key_file_clear (key_file);
      g_slice_free (GKeyFile, key_file);
    }
}

/* If G_KEY_FILE_KEEP_TRANSLATIONS is not set, only returns
 * true for locales that match those in g_get_language_names().
 */
static gboolean
g_key_file_locale_is_interesting (GKeyFile    *key_file,
				  const gchar *locale)
{
  gsize i;

  if (key_file->flags & G_KEY_FILE_KEEP_TRANSLATIONS)
    return TRUE;

  for (i = 0; key_file->locales[i] != NULL; i++)
    {
      if (g_ascii_strcasecmp (key_file->locales[i], locale) == 0)
	return TRUE;
    }

  return FALSE;
}

static void
g_key_file_parse_line (GKeyFile     *key_file,
		       const gchar  *line,
		       gsize         length,
		       GError      **error)
{
  GError *parse_error = NULL;
  gchar *line_start;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (line != NULL);

  line_start = (gchar *) line;
  while (g_ascii_isspace (*line_start))
    line_start++;

  if (g_key_file_line_is_comment (line_start))
    g_key_file_parse_comment (key_file, line, length, &parse_error);
  else if (g_key_file_line_is_group (line_start))
    g_key_file_parse_group (key_file, line_start,
			    length - (line_start - line),
			    &parse_error);
  else if (g_key_file_line_is_key_value_pair (line_start))
    g_key_file_parse_key_value_pair (key_file, line_start,
				     length - (line_start - line),
				     &parse_error);
  else
    {
      gchar *line_utf8 = g_utf8_make_valid (line, length);
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_PARSE,
                   _("Key file contains line “%s” which is not "
                     "a key-value pair, group, or comment"),
                   line_utf8);
      g_free (line_utf8);

      return;
    }

  if (parse_error)
    g_propagate_error (error, parse_error);
}

static void
g_key_file_parse_comment (GKeyFile     *key_file,
			  const gchar  *line,
			  gsize         length,
			  GError      **error)
{
  GKeyFileKeyValuePair *pair;
  
  if (!(key_file->flags & G_KEY_FILE_KEEP_COMMENTS))
    return;
  
  g_warn_if_fail (key_file->current_group != NULL);

  pair = g_slice_new (GKeyFileKeyValuePair);
  pair->key = NULL;
  pair->value = g_strndup (line, length);
  
  key_file->current_group->key_value_pairs =
    g_list_prepend (key_file->current_group->key_value_pairs, pair);
}

static void
g_key_file_parse_group (GKeyFile     *key_file,
			const gchar  *line,
			gsize         length,
			GError      **error)
{
  gchar *group_name;
  const gchar *group_name_start, *group_name_end;
  
  /* advance past opening '['
   */
  group_name_start = line + 1;
  group_name_end = line + length - 1;
  
  while (*group_name_end != ']')
    group_name_end--;

  group_name = g_strndup (group_name_start, 
                          group_name_end - group_name_start);
  
  if (!g_key_file_is_group_name (group_name))
    {
      g_set_error (error, G_KEY_FILE_ERROR,
		   G_KEY_FILE_ERROR_PARSE,
		   _("Invalid group name: %s"), group_name);
      g_free (group_name);
      return;
    }

  g_key_file_add_group (key_file, group_name);
  g_free (group_name);
}

static void
g_key_file_parse_key_value_pair (GKeyFile     *key_file,
				 const gchar  *line,
				 gsize         length,
				 GError      **error)
{
  gchar *key, *value, *key_end, *value_start, *locale;
  gsize key_len, value_len;

  if (key_file->current_group == NULL || key_file->current_group->name == NULL)
    {
      g_set_error_literal (error, G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                           _("Key file does not start with a group"));
      return;
    }

  key_end = value_start = strchr (line, '=');

  g_warn_if_fail (key_end != NULL);

  key_end--;
  value_start++;

  /* Pull the key name from the line (chomping trailing whitespace)
   */
  while (g_ascii_isspace (*key_end))
    key_end--;

  key_len = key_end - line + 2;

  g_warn_if_fail (key_len <= length);

  key = g_strndup (line, key_len - 1);

  if (!g_key_file_is_key_name (key))
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_PARSE,
                   _("Invalid key name: %s"), key);
      g_free (key);
      return; 
    }

  /* Pull the value from the line (chugging leading whitespace)
   */
  while (g_ascii_isspace (*value_start))
    value_start++;

  value_len = line + length - value_start + 1;

  value = g_strndup (value_start, value_len);

  g_warn_if_fail (key_file->start_group != NULL);

  if (key_file->current_group
      && key_file->current_group->name
      && strcmp (key_file->start_group->name,
                 key_file->current_group->name) == 0
      && strcmp (key, "Encoding") == 0)
    {
      if (g_ascii_strcasecmp (value, "UTF-8") != 0)
        {
	  gchar *value_utf8 = g_utf8_make_valid (value, value_len);
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_UNKNOWN_ENCODING,
                       _("Key file contains unsupported "
			 "encoding “%s”"), value_utf8);
	  g_free (value_utf8);

          g_free (key);
          g_free (value);
          return;
        }
    }

  /* Is this key a translation? If so, is it one that we care about?
   */
  locale = key_get_locale (key);

  if (locale == NULL || g_key_file_locale_is_interesting (key_file, locale))
    {
      GKeyFileKeyValuePair *pair;

      pair = g_slice_new (GKeyFileKeyValuePair);
      pair->key = key;
      pair->value = value;

      g_key_file_add_key_value_pair (key_file, key_file->current_group, pair);
    }
  else
    {
      g_free (key);
      g_free (value);
    }

  g_free (locale);
}

static gchar *
key_get_locale (const gchar *key)
{
  gchar *locale;

  locale = g_strrstr (key, "[");

  if (locale && strlen (locale) <= 2)
    locale = NULL;

  if (locale)
    locale = g_strndup (locale + 1, strlen (locale) - 2);

  return locale;
}

static void
g_key_file_parse_data (GKeyFile     *key_file,
		       const gchar  *data,
		       gsize         length,
		       GError      **error)
{
  GError *parse_error;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (data != NULL || length == 0);

  parse_error = NULL;

  i = 0;
  while (i < length)
    {
      if (data[i] == '\n')
        {
	  if (key_file->parse_buffer->len > 0
	      && (key_file->parse_buffer->str[key_file->parse_buffer->len - 1]
		  == '\r'))
	    g_string_erase (key_file->parse_buffer,
			    key_file->parse_buffer->len - 1,
			    1);
	    
          /* When a newline is encountered flush the parse buffer so that the
           * line can be parsed.  Note that completely blank lines won't show
           * up in the parse buffer, so they get parsed directly.
           */
          if (key_file->parse_buffer->len > 0)
            g_key_file_flush_parse_buffer (key_file, &parse_error);
          else
            g_key_file_parse_comment (key_file, "", 1, &parse_error);

          if (parse_error)
            {
              g_propagate_error (error, parse_error);
              return;
            }
          i++;
        }
      else
        {
          const gchar *start_of_line;
          const gchar *end_of_line;
          gsize line_length;

          start_of_line = data + i;
          end_of_line = memchr (start_of_line, '\n', length - i);

          if (end_of_line == NULL)
            end_of_line = data + length;

          line_length = end_of_line - start_of_line;

          g_string_append_len (key_file->parse_buffer, start_of_line, line_length);
          i += line_length;
        }
    }
}

static void
g_key_file_flush_parse_buffer (GKeyFile  *key_file,
			       GError   **error)
{
  GError *file_error = NULL;

  g_return_if_fail (key_file != NULL);

  file_error = NULL;

  if (key_file->parse_buffer->len > 0)
    {
      g_key_file_parse_line (key_file, key_file->parse_buffer->str,
			     key_file->parse_buffer->len,
			     &file_error);
      g_string_erase (key_file->parse_buffer, 0, -1);

      if (file_error)
        {
          g_propagate_error (error, file_error);
          return;
        }
    }
}

/**
 * g_key_file_to_data:
 * @key_file: a #GKeyFile
 * @length: (out) (optional): return location for the length of the
 *   returned string, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * This function outputs @key_file as a string.  
 *
 * Note that this function never reports an error,
 * so it is safe to pass %NULL as @error.
 *
 * Returns: a newly allocated string holding
 *   the contents of the #GKeyFile 
 *
 * Since: 2.6
 **/
gchar *
g_key_file_to_data (GKeyFile  *key_file,
		    gsize     *length,
		    GError   **error)
{
  GString *data_string;
  GList *group_node, *key_file_node;

  g_return_val_if_fail (key_file != NULL, NULL);

  data_string = g_string_new (NULL);

  for (group_node = g_list_last (key_file->groups);
       group_node != NULL;
       group_node = group_node->prev)
    {
      GKeyFileGroup *group;

      group = (GKeyFileGroup *) group_node->data;

      /* separate groups by at least an empty line */
      if (data_string->len >= 2 &&
          data_string->str[data_string->len - 2] != '\n')
        g_string_append_c (data_string, '\n');

      if (group->comment != NULL)
        g_string_append_printf (data_string, "%s\n", group->comment->value);

      if (group->name != NULL)
        g_string_append_printf (data_string, "[%s]\n", group->name);

      for (key_file_node = g_list_last (group->key_value_pairs);
           key_file_node != NULL;
           key_file_node = key_file_node->prev)
        {
          GKeyFileKeyValuePair *pair;

          pair = (GKeyFileKeyValuePair *) key_file_node->data;

          if (pair->key != NULL)
            g_string_append_printf (data_string, "%s=%s\n", pair->key, pair->value);
          else
            g_string_append_printf (data_string, "%s\n", pair->value);
        }
    }

  if (length)
    *length = data_string->len;

  return g_string_free (data_string, FALSE);
}

/**
 * g_key_file_get_keys:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @length: (out) (optional): return location for the number of keys returned, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Returns all keys for the group name @group_name.  The array of
 * returned keys will be %NULL-terminated, so @length may
 * optionally be %NULL. In the event that the @group_name cannot
 * be found, %NULL is returned and @error is set to
 * #G_KEY_FILE_ERROR_GROUP_NOT_FOUND.
 *
 * Returns: (array zero-terminated=1) (transfer full): a newly-allocated %NULL-terminated array of strings.
 *     Use g_strfreev() to free it.
 *
 * Since: 2.6
 **/
gchar **
g_key_file_get_keys (GKeyFile     *key_file,
		     const gchar  *group_name,
		     gsize        *length,
		     GError      **error)
{
  GKeyFileGroup *group;
  GList *tmp;
  gchar **keys;
  gsize i, num_keys;
  
  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  
  group = g_key_file_lookup_group (key_file, group_name);
  
  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name);
      return NULL;
    }

  num_keys = 0;
  for (tmp = group->key_value_pairs; tmp; tmp = tmp->next)
    {
      GKeyFileKeyValuePair *pair;

      pair = (GKeyFileKeyValuePair *) tmp->data;

      if (pair->key)
	num_keys++;
    }
  
  keys = g_new (gchar *, num_keys + 1);

  i = num_keys - 1;
  for (tmp = group->key_value_pairs; tmp; tmp = tmp->next)
    {
      GKeyFileKeyValuePair *pair;

      pair = (GKeyFileKeyValuePair *) tmp->data;

      if (pair->key)
	{
	  keys[i] = g_strdup (pair->key);
	  i--;
	}
    }

  keys[num_keys] = NULL;

  if (length)
    *length = num_keys;

  return keys;
}

/**
 * g_key_file_get_start_group:
 * @key_file: a #GKeyFile
 *
 * Returns the name of the start group of the file. 
 *
 * Returns: The start group of the key file.
 *
 * Since: 2.6
 **/
gchar *
g_key_file_get_start_group (GKeyFile *key_file)
{
  g_return_val_if_fail (key_file != NULL, NULL);

  if (key_file->start_group)
    return g_strdup (key_file->start_group->name);

  return NULL;
}

/**
 * g_key_file_get_groups:
 * @key_file: a #GKeyFile
 * @length: (out) (optional): return location for the number of returned groups, or %NULL
 *
 * Returns all groups in the key file loaded with @key_file.  
 * The array of returned groups will be %NULL-terminated, so 
 * @length may optionally be %NULL.
 *
 * Returns: (array zero-terminated=1) (transfer full): a newly-allocated %NULL-terminated array of strings.
 *   Use g_strfreev() to free it.
 * Since: 2.6
 **/
gchar **
g_key_file_get_groups (GKeyFile *key_file,
		       gsize    *length)
{
  GList *group_node;
  gchar **groups;
  gsize i, num_groups;

  g_return_val_if_fail (key_file != NULL, NULL);

  num_groups = g_list_length (key_file->groups);

  g_return_val_if_fail (num_groups > 0, NULL);

  group_node = g_list_last (key_file->groups);
  
  g_return_val_if_fail (((GKeyFileGroup *) group_node->data)->name == NULL, NULL);

  /* Only need num_groups instead of num_groups + 1
   * because the first group of the file (last in the
   * list) is always the comment group at the top,
   * which we skip
   */
  groups = g_new (gchar *, num_groups);


  i = 0;
  for (group_node = group_node->prev;
       group_node != NULL;
       group_node = group_node->prev)
    {
      GKeyFileGroup *group;

      group = (GKeyFileGroup *) group_node->data;

      g_warn_if_fail (group->name != NULL);

      groups[i++] = g_strdup (group->name);
    }
  groups[i] = NULL;

  if (length)
    *length = i;

  return groups;
}

static void
set_not_found_key_error (const char *group_name,
                         const char *key,
                         GError    **error)
{
  g_set_error (error, G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_KEY_NOT_FOUND,
               _("Key file does not have key “%s” in group “%s”"),
               key, group_name);
}

/**
 * g_key_file_get_value:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @error: return location for a #GError, or %NULL
 *
 * Returns the raw value associated with @key under @group_name. 
 * Use g_key_file_get_string() to retrieve an unescaped UTF-8 string. 
 *
 * In the event the key cannot be found, %NULL is returned and 
 * @error is set to #G_KEY_FILE_ERROR_KEY_NOT_FOUND.  In the 
 * event that the @group_name cannot be found, %NULL is returned 
 * and @error is set to #G_KEY_FILE_ERROR_GROUP_NOT_FOUND.
 *
 *
 * Returns: a newly allocated string or %NULL if the specified 
 *  key cannot be found.
 *
 * Since: 2.6
 **/
gchar *
g_key_file_get_value (GKeyFile     *key_file,
		      const gchar  *group_name,
		      const gchar  *key,
		      GError      **error)
{
  GKeyFileGroup *group;
  GKeyFileKeyValuePair *pair;
  gchar *value = NULL;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);
  
  group = g_key_file_lookup_group (key_file, group_name);

  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name);
      return NULL;
    }

  pair = g_key_file_lookup_key_value_pair (key_file, group, key);

  if (pair)
    value = g_strdup (pair->value);
  else
    set_not_found_key_error (group_name, key, error);

  return value;
}

/**
 * g_key_file_set_value:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: a string
 *
 * Associates a new value with @key under @group_name.  
 *
 * If @key cannot be found then it is created. If @group_name cannot 
 * be found then it is created. To set an UTF-8 string which may contain 
 * characters that need escaping (such as newlines or spaces), use 
 * g_key_file_set_string().
 *
 * Since: 2.6
 **/
void
g_key_file_set_value (GKeyFile    *key_file,
		      const gchar *group_name,
		      const gchar *key,
		      const gchar *value)
{
  GKeyFileGroup *group;
  GKeyFileKeyValuePair *pair;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (g_key_file_is_group_name (group_name));
  g_return_if_fail (g_key_file_is_key_name (key));
  g_return_if_fail (value != NULL);

  group = g_key_file_lookup_group (key_file, group_name);

  if (!group)
    {
      g_key_file_add_group (key_file, group_name);
      group = (GKeyFileGroup *) key_file->groups->data;

      g_key_file_add_key (key_file, group, key, value);
    }
  else
    {
      pair = g_key_file_lookup_key_value_pair (key_file, group, key);

      if (!pair)
        g_key_file_add_key (key_file, group, key, value);
      else
        {
          g_free (pair->value);
          pair->value = g_strdup (value);
        }
    }
}

/**
 * g_key_file_get_string:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @error: return location for a #GError, or %NULL
 *
 * Returns the string value associated with @key under @group_name.
 * Unlike g_key_file_get_value(), this function handles escape sequences
 * like \s.
 *
 * In the event the key cannot be found, %NULL is returned and 
 * @error is set to #G_KEY_FILE_ERROR_KEY_NOT_FOUND.  In the 
 * event that the @group_name cannot be found, %NULL is returned 
 * and @error is set to #G_KEY_FILE_ERROR_GROUP_NOT_FOUND.
 *
 * Returns: a newly allocated string or %NULL if the specified 
 *   key cannot be found.
 *
 * Since: 2.6
 **/
gchar *
g_key_file_get_string (GKeyFile     *key_file,
		       const gchar  *group_name,
		       const gchar  *key,
		       GError      **error)
{
  gchar *value, *string_value;
  GError *key_file_error;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  key_file_error = NULL;

  value = g_key_file_get_value (key_file, group_name, key, &key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return NULL;
    }

  if (!g_utf8_validate (value, -1, NULL))
    {
      gchar *value_utf8 = g_utf8_make_valid (value, -1);
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_UNKNOWN_ENCODING,
                   _("Key file contains key “%s” with value “%s” "
                     "which is not UTF-8"), key, value_utf8);
      g_free (value_utf8);
      g_free (value);

      return NULL;
    }

  string_value = g_key_file_parse_value_as_string (key_file, value, NULL,
						   &key_file_error);
  g_free (value);

  if (key_file_error)
    {
      if (g_error_matches (key_file_error,
                           G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_INVALID_VALUE))
        {
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_INVALID_VALUE,
                       _("Key file contains key “%s” "
                         "which has a value that cannot be interpreted."),
                       key);
          g_error_free (key_file_error);
        }
      else
        g_propagate_error (error, key_file_error);
    }

  return string_value;
}

/**
 * g_key_file_set_string:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @string: a string
 *
 * Associates a new string value with @key under @group_name.  
 * If @key cannot be found then it is created.  
 * If @group_name cannot be found then it is created.
 * Unlike g_key_file_set_value(), this function handles characters
 * that need escaping, such as newlines.
 *
 * Since: 2.6
 **/
void
g_key_file_set_string (GKeyFile    *key_file,
		       const gchar *group_name,
		       const gchar *key,
		       const gchar *string)
{
  gchar *value;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (string != NULL);

  value = g_key_file_parse_string_as_value (key_file, string, FALSE);
  g_key_file_set_value (key_file, group_name, key, value);
  g_free (value);
}

/**
 * g_key_file_get_string_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @length: (out) (optional): return location for the number of returned strings, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Returns the values associated with @key under @group_name.
 *
 * In the event the key cannot be found, %NULL is returned and
 * @error is set to #G_KEY_FILE_ERROR_KEY_NOT_FOUND.  In the
 * event that the @group_name cannot be found, %NULL is returned
 * and @error is set to #G_KEY_FILE_ERROR_GROUP_NOT_FOUND.
 *
 * Returns: (array zero-terminated=1 length=length) (element-type utf8) (transfer full): 
 *  a %NULL-terminated string array or %NULL if the specified 
 *  key cannot be found. The array should be freed with g_strfreev().
 *
 * Since: 2.6
 **/
gchar **
g_key_file_get_string_list (GKeyFile     *key_file,
			    const gchar  *group_name,
			    const gchar  *key,
			    gsize        *length,
			    GError      **error)
{
  GError *key_file_error = NULL;
  gchar *value, *string_value, **values;
  gint i, len;
  GSList *p, *pieces = NULL;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (length)
    *length = 0;

  value = g_key_file_get_value (key_file, group_name, key, &key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return NULL;
    }

  if (!g_utf8_validate (value, -1, NULL))
    {
      gchar *value_utf8 = g_utf8_make_valid (value, -1);
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_UNKNOWN_ENCODING,
                   _("Key file contains key “%s” with value “%s” "
                     "which is not UTF-8"), key, value_utf8);
      g_free (value_utf8);
      g_free (value);

      return NULL;
    }

  string_value = g_key_file_parse_value_as_string (key_file, value, &pieces, &key_file_error);
  g_free (value);
  g_free (string_value);

  if (key_file_error)
    {
      if (g_error_matches (key_file_error,
                           G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_INVALID_VALUE))
        {
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_INVALID_VALUE,
                       _("Key file contains key “%s” "
                         "which has a value that cannot be interpreted."),
                       key);
          g_error_free (key_file_error);
        }
      else
        g_propagate_error (error, key_file_error);

      g_slist_free_full (pieces, g_free);
      return NULL;
    }

  len = g_slist_length (pieces);
  values = g_new (gchar *, len + 1);
  for (p = pieces, i = 0; p; p = p->next)
    values[i++] = p->data;
  values[len] = NULL;

  g_slist_free (pieces);

  if (length)
    *length = len;

  return values;
}

/**
 * g_key_file_set_string_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @list: (array zero-terminated=1 length=length) (element-type utf8): an array of string values
 * @length: number of string values in @list
 *
 * Associates a list of string values for @key under @group_name.
 * If @key cannot be found then it is created.
 * If @group_name cannot be found then it is created.
 *
 * Since: 2.6
 **/
void
g_key_file_set_string_list (GKeyFile            *key_file,
			    const gchar         *group_name,
			    const gchar         *key,
			    const gchar * const  list[],
			    gsize                length)
{
  GString *value_list;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (list != NULL || length == 0);

  value_list = g_string_sized_new (length * 128);
  for (i = 0; i < length && list[i] != NULL; i++)
    {
      gchar *value;

      value = g_key_file_parse_string_as_value (key_file, list[i], TRUE);
      g_string_append (value_list, value);
      g_string_append_c (value_list, key_file->list_separator);

      g_free (value);
    }

  g_key_file_set_value (key_file, group_name, key, value_list->str);
  g_string_free (value_list, TRUE);
}

/**
 * g_key_file_set_locale_string:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @locale: a locale identifier
 * @string: a string
 *
 * Associates a string value for @key and @locale under @group_name.
 * If the translation for @key cannot be found then it is created.
 *
 * Since: 2.6
 **/
void
g_key_file_set_locale_string (GKeyFile     *key_file,
			      const gchar  *group_name,
			      const gchar  *key,
			      const gchar  *locale,
			      const gchar  *string)
{
  gchar *full_key, *value;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (locale != NULL);
  g_return_if_fail (string != NULL);

  value = g_key_file_parse_string_as_value (key_file, string, FALSE);
  full_key = g_strdup_printf ("%s[%s]", key, locale);
  g_key_file_set_value (key_file, group_name, full_key, value);
  g_free (full_key);
  g_free (value);
}

/**
 * g_key_file_get_locale_string:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @locale: (nullable): a locale identifier or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Returns the value associated with @key under @group_name
 * translated in the given @locale if available.  If @locale is
 * %NULL then the current locale is assumed. 
 *
 * If @locale is to be non-%NULL, or if the current locale will change over
 * the lifetime of the #GKeyFile, it must be loaded with
 * %G_KEY_FILE_KEEP_TRANSLATIONS in order to load strings for all locales.
 *
 * If @key cannot be found then %NULL is returned and @error is set 
 * to #G_KEY_FILE_ERROR_KEY_NOT_FOUND. If the value associated
 * with @key cannot be interpreted or no suitable translation can
 * be found then the untranslated value is returned.
 *
 * Returns: a newly allocated string or %NULL if the specified 
 *   key cannot be found.
 *
 * Since: 2.6
 **/
gchar *
g_key_file_get_locale_string (GKeyFile     *key_file,
			      const gchar  *group_name,
			      const gchar  *key,
			      const gchar  *locale,
			      GError      **error)
{
  gchar *candidate_key, *translated_value;
  GError *key_file_error;
  gchar **languages;
  gboolean free_languages = FALSE;
  gint i;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  candidate_key = NULL;
  translated_value = NULL;
  key_file_error = NULL;

  if (locale)
    {
      languages = g_get_locale_variants (locale);
      free_languages = TRUE;
    }
  else
    {
      languages = (gchar **) g_get_language_names ();
      free_languages = FALSE;
    }
  
  for (i = 0; languages[i]; i++)
    {
      candidate_key = g_strdup_printf ("%s[%s]", key, languages[i]);
      
      translated_value = g_key_file_get_string (key_file,
						group_name,
						candidate_key, NULL);
      g_free (candidate_key);

      if (translated_value)
	break;

      g_free (translated_value);
      translated_value = NULL;
   }

  /* Fallback to untranslated key
   */
  if (!translated_value)
    {
      translated_value = g_key_file_get_string (key_file, group_name, key,
						&key_file_error);
      
      if (!translated_value)
        g_propagate_error (error, key_file_error);
    }

  if (free_languages)
    g_strfreev (languages);

  return translated_value;
}

/**
 * g_key_file_get_locale_for_key:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @locale: (nullable): a locale identifier or %NULL
 *
 * Returns the actual locale which the result of
 * g_key_file_get_locale_string() or g_key_file_get_locale_string_list()
 * came from.
 *
 * If calling g_key_file_get_locale_string() or
 * g_key_file_get_locale_string_list() with exactly the same @key_file,
 * @group_name, @key and @locale, the result of those functions will
 * have originally been tagged with the locale that is the result of
 * this function.
 *
 * Returns: (nullable): the locale from the file, or %NULL if the key was not
 *   found or the entry in the file was was untranslated
 *
 * Since: 2.56
 */
gchar *
g_key_file_get_locale_for_key (GKeyFile    *key_file,
                               const gchar *group_name,
                               const gchar *key,
                               const gchar *locale)
{
  gchar **languages_allocated = NULL;
  const gchar * const *languages;
  gchar *result = NULL;
  gsize i;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (locale != NULL)
    {
      languages_allocated = g_get_locale_variants (locale);
      languages = (const gchar * const *) languages_allocated;
    }
  else
    languages = g_get_language_names ();

  for (i = 0; languages[i] != NULL; i++)
    {
      gchar *candidate_key, *translated_value;

      candidate_key = g_strdup_printf ("%s[%s]", key, languages[i]);
      translated_value = g_key_file_get_string (key_file, group_name, candidate_key, NULL);
      g_free (translated_value);
      g_free (candidate_key);

      if (translated_value != NULL)
        break;
   }

  result = g_strdup (languages[i]);

  g_strfreev (languages_allocated);

  return result;
}

/**
 * g_key_file_get_locale_string_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @locale: (nullable): a locale identifier or %NULL
 * @length: (out) (optional): return location for the number of returned strings or %NULL
 * @error: return location for a #GError or %NULL
 *
 * Returns the values associated with @key under @group_name
 * translated in the given @locale if available.  If @locale is
 * %NULL then the current locale is assumed.
 *
 * If @locale is to be non-%NULL, or if the current locale will change over
 * the lifetime of the #GKeyFile, it must be loaded with
 * %G_KEY_FILE_KEEP_TRANSLATIONS in order to load strings for all locales.
 *
 * If @key cannot be found then %NULL is returned and @error is set 
 * to #G_KEY_FILE_ERROR_KEY_NOT_FOUND. If the values associated
 * with @key cannot be interpreted or no suitable translations
 * can be found then the untranslated values are returned. The 
 * returned array is %NULL-terminated, so @length may optionally 
 * be %NULL.
 *
 * Returns: (array zero-terminated=1 length=length) (element-type utf8) (transfer full): a newly allocated %NULL-terminated string array
 *   or %NULL if the key isn't found. The string array should be freed
 *   with g_strfreev().
 *
 * Since: 2.6
 **/
gchar **
g_key_file_get_locale_string_list (GKeyFile     *key_file,
				   const gchar  *group_name,
				   const gchar  *key,
				   const gchar  *locale,
				   gsize        *length,
				   GError      **error)
{
  GError *key_file_error;
  gchar **values, *value;
  char list_separator[2];
  gsize len;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  key_file_error = NULL;

  value = g_key_file_get_locale_string (key_file, group_name, 
					key, locale,
					&key_file_error);
  
  if (key_file_error)
    g_propagate_error (error, key_file_error);
  
  if (!value)
    {
      if (length)
        *length = 0;
      return NULL;
    }

  len = strlen (value);
  if (value[len - 1] == key_file->list_separator)
    value[len - 1] = '\0';

  list_separator[0] = key_file->list_separator;
  list_separator[1] = '\0';
  values = g_strsplit (value, list_separator, 0);

  g_free (value);

  if (length)
    *length = g_strv_length (values);

  return values;
}

/**
 * g_key_file_set_locale_string_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @locale: a locale identifier
 * @list: (array zero-terminated=1 length=length): a %NULL-terminated array of locale string values
 * @length: the length of @list
 *
 * Associates a list of string values for @key and @locale under
 * @group_name.  If the translation for @key cannot be found then
 * it is created. 
 *
 * Since: 2.6
 **/
void
g_key_file_set_locale_string_list (GKeyFile            *key_file,
				   const gchar         *group_name,
				   const gchar         *key,
				   const gchar         *locale,
				   const gchar * const  list[],
				   gsize                length)
{
  GString *value_list;
  gchar *full_key;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (locale != NULL);
  g_return_if_fail (length != 0);

  value_list = g_string_sized_new (length * 128);
  for (i = 0; i < length && list[i] != NULL; i++)
    {
      gchar *value;
      
      value = g_key_file_parse_string_as_value (key_file, list[i], TRUE);
      g_string_append (value_list, value);
      g_string_append_c (value_list, key_file->list_separator);

      g_free (value);
    }

  full_key = g_strdup_printf ("%s[%s]", key, locale);
  g_key_file_set_value (key_file, group_name, full_key, value_list->str);
  g_free (full_key);
  g_string_free (value_list, TRUE);
}

/**
 * g_key_file_get_boolean:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as a
 * boolean. 
 *
 * If @key cannot be found then %FALSE is returned and @error is set
 * to #G_KEY_FILE_ERROR_KEY_NOT_FOUND. Likewise, if the value
 * associated with @key cannot be interpreted as a boolean then %FALSE
 * is returned and @error is set to #G_KEY_FILE_ERROR_INVALID_VALUE.
 *
 * Returns: the value associated with the key as a boolean, 
 *    or %FALSE if the key was not found or could not be parsed.
 *
 * Since: 2.6
 **/
gboolean
g_key_file_get_boolean (GKeyFile     *key_file,
			const gchar  *group_name,
			const gchar  *key,
			GError      **error)
{
  GError *key_file_error = NULL;
  gchar *value;
  gboolean bool_value;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (group_name != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = g_key_file_get_value (key_file, group_name, key, &key_file_error);

  if (!value)
    {
      g_propagate_error (error, key_file_error);
      return FALSE;
    }

  bool_value = g_key_file_parse_value_as_boolean (key_file, value,
						  &key_file_error);
  g_free (value);

  if (key_file_error)
    {
      if (g_error_matches (key_file_error,
                           G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_INVALID_VALUE))
        {
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_INVALID_VALUE,
                       _("Key file contains key “%s” "
                         "which has a value that cannot be interpreted."),
                       key);
          g_error_free (key_file_error);
        }
      else
        g_propagate_error (error, key_file_error);
    }

  return bool_value;
}

/**
 * g_key_file_set_boolean:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: %TRUE or %FALSE
 *
 * Associates a new boolean value with @key under @group_name.
 * If @key cannot be found then it is created. 
 *
 * Since: 2.6
 **/
void
g_key_file_set_boolean (GKeyFile    *key_file,
			const gchar *group_name,
			const gchar *key,
			gboolean     value)
{
  gchar *result;

  g_return_if_fail (key_file != NULL);

  result = g_key_file_parse_boolean_as_value (key_file, value);
  g_key_file_set_value (key_file, group_name, key, result);
  g_free (result);
}

/**
 * g_key_file_get_boolean_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @length: (out): the number of booleans returned
 * @error: return location for a #GError
 *
 * Returns the values associated with @key under @group_name as
 * booleans. 
 *
 * If @key cannot be found then %NULL is returned and @error is set to
 * #G_KEY_FILE_ERROR_KEY_NOT_FOUND. Likewise, if the values associated
 * with @key cannot be interpreted as booleans then %NULL is returned
 * and @error is set to #G_KEY_FILE_ERROR_INVALID_VALUE.
 *
 * Returns: (array length=length) (element-type gboolean) (transfer container):
 *    the values associated with the key as a list of booleans, or %NULL if the
 *    key was not found or could not be parsed. The returned list of booleans
 *    should be freed with g_free() when no longer needed.
 * 
 * Since: 2.6
 **/
gboolean *
g_key_file_get_boolean_list (GKeyFile     *key_file,
			     const gchar  *group_name,
			     const gchar  *key,
			     gsize        *length,
			     GError      **error)
{
  GError *key_file_error;
  gchar **values;
  gboolean *bool_values;
  gsize i, num_bools;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (length)
    *length = 0;

  key_file_error = NULL;

  values = g_key_file_get_string_list (key_file, group_name, key,
				       &num_bools, &key_file_error);

  if (key_file_error)
    g_propagate_error (error, key_file_error);

  if (!values)
    return NULL;

  bool_values = g_new (gboolean, num_bools);

  for (i = 0; i < num_bools; i++)
    {
      bool_values[i] = g_key_file_parse_value_as_boolean (key_file,
							  values[i],
							  &key_file_error);

      if (key_file_error)
        {
          g_propagate_error (error, key_file_error);
          g_strfreev (values);
          g_free (bool_values);

          return NULL;
        }
    }
  g_strfreev (values);

  if (length)
    *length = num_bools;

  return bool_values;
}

/**
 * g_key_file_set_boolean_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @list: (array length=length): an array of boolean values
 * @length: length of @list
 *
 * Associates a list of boolean values with @key under @group_name.  
 * If @key cannot be found then it is created.
 * If @group_name is %NULL, the start_group is used.
 *
 * Since: 2.6
 **/
void
g_key_file_set_boolean_list (GKeyFile    *key_file,
			     const gchar *group_name,
			     const gchar *key,
			     gboolean     list[],
			     gsize        length)
{
  GString *value_list;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (list != NULL);

  value_list = g_string_sized_new (length * 8);
  for (i = 0; i < length; i++)
    {
      gchar *value;

      value = g_key_file_parse_boolean_as_value (key_file, list[i]);

      g_string_append (value_list, value);
      g_string_append_c (value_list, key_file->list_separator);

      g_free (value);
    }

  g_key_file_set_value (key_file, group_name, key, value_list->str);
  g_string_free (value_list, TRUE);
}

/**
 * g_key_file_get_integer:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as an
 * integer. 
 *
 * If @key cannot be found then 0 is returned and @error is set to
 * #G_KEY_FILE_ERROR_KEY_NOT_FOUND. Likewise, if the value associated
 * with @key cannot be interpreted as an integer, or is out of range
 * for a #gint, then 0 is returned
 * and @error is set to #G_KEY_FILE_ERROR_INVALID_VALUE.
 *
 * Returns: the value associated with the key as an integer, or
 *     0 if the key was not found or could not be parsed.
 *
 * Since: 2.6
 **/
gint
g_key_file_get_integer (GKeyFile     *key_file,
			const gchar  *group_name,
			const gchar  *key,
			GError      **error)
{
  GError *key_file_error;
  gchar *value;
  gint int_value;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  key_file_error = NULL;

  value = g_key_file_get_value (key_file, group_name, key, &key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return 0;
    }

  int_value = g_key_file_parse_value_as_integer (key_file, value,
						 &key_file_error);
  g_free (value);

  if (key_file_error)
    {
      if (g_error_matches (key_file_error,
                           G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_INVALID_VALUE))
        {
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_INVALID_VALUE,
                       _("Key file contains key “%s” in group “%s” "
                         "which has a value that cannot be interpreted."),
                         key, group_name);
          g_error_free (key_file_error);
        }
      else
        g_propagate_error (error, key_file_error);
    }

  return int_value;
}

/**
 * g_key_file_set_integer:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: an integer value
 *
 * Associates a new integer value with @key under @group_name.
 * If @key cannot be found then it is created.
 *
 * Since: 2.6
 **/
void
g_key_file_set_integer (GKeyFile    *key_file,
			const gchar *group_name,
			const gchar *key,
			gint         value)
{
  gchar *result;

  g_return_if_fail (key_file != NULL);

  result = g_key_file_parse_integer_as_value (key_file, value);
  g_key_file_set_value (key_file, group_name, key, result);
  g_free (result);
}

/**
 * g_key_file_get_int64:
 * @key_file: a non-%NULL #GKeyFile
 * @group_name: a non-%NULL group name
 * @key: a non-%NULL key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as a signed
 * 64-bit integer. This is similar to g_key_file_get_integer() but can return
 * 64-bit results without truncation.
 *
 * Returns: the value associated with the key as a signed 64-bit integer, or
 * 0 if the key was not found or could not be parsed.
 *
 * Since: 2.26
 */
gint64
g_key_file_get_int64 (GKeyFile     *key_file,
                      const gchar  *group_name,
                      const gchar  *key,
                      GError      **error)
{
  gchar *s, *end;
  gint64 v;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  s = g_key_file_get_value (key_file, group_name, key, error);

  if (s == NULL)
    return 0;

  v = g_ascii_strtoll (s, &end, 10);

  if (*s == '\0' || *end != '\0')
    {
      g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                   _("Key “%s” in group “%s” has value “%s” "
                     "where %s was expected"),
                   key, group_name, s, "int64");
      g_free (s);
      return 0;
    }

  g_free (s);
  return v;
}

/**
 * g_key_file_set_int64:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: an integer value
 *
 * Associates a new integer value with @key under @group_name.
 * If @key cannot be found then it is created.
 *
 * Since: 2.26
 **/
void
g_key_file_set_int64 (GKeyFile    *key_file,
                      const gchar *group_name,
                      const gchar *key,
                      gint64       value)
{
  gchar *result;

  g_return_if_fail (key_file != NULL);

  result = g_strdup_printf ("%" G_GINT64_FORMAT, value);
  g_key_file_set_value (key_file, group_name, key, result);
  g_free (result);
}

/**
 * g_key_file_get_uint64:
 * @key_file: a non-%NULL #GKeyFile
 * @group_name: a non-%NULL group name
 * @key: a non-%NULL key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as an unsigned
 * 64-bit integer. This is similar to g_key_file_get_integer() but can return
 * large positive results without truncation.
 *
 * Returns: the value associated with the key as an unsigned 64-bit integer,
 * or 0 if the key was not found or could not be parsed.
 *
 * Since: 2.26
 */
guint64
g_key_file_get_uint64 (GKeyFile     *key_file,
                       const gchar  *group_name,
                       const gchar  *key,
                       GError      **error)
{
  gchar *s, *end;
  guint64 v;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  s = g_key_file_get_value (key_file, group_name, key, error);

  if (s == NULL)
    return 0;

  v = g_ascii_strtoull (s, &end, 10);

  if (*s == '\0' || *end != '\0')
    {
      g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                   _("Key “%s” in group “%s” has value “%s” "
                     "where %s was expected"),
                   key, group_name, s, "uint64");
      g_free (s);
      return 0;
    }

  g_free (s);
  return v;
}

/**
 * g_key_file_set_uint64:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: an integer value
 *
 * Associates a new integer value with @key under @group_name.
 * If @key cannot be found then it is created.
 *
 * Since: 2.26
 **/
void
g_key_file_set_uint64 (GKeyFile    *key_file,
                       const gchar *group_name,
                       const gchar *key,
                       guint64      value)
{
  gchar *result;

  g_return_if_fail (key_file != NULL);

  result = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
  g_key_file_set_value (key_file, group_name, key, result);
  g_free (result);
}

/**
 * g_key_file_get_integer_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @length: (out): the number of integers returned
 * @error: return location for a #GError
 *
 * Returns the values associated with @key under @group_name as
 * integers. 
 *
 * If @key cannot be found then %NULL is returned and @error is set to
 * #G_KEY_FILE_ERROR_KEY_NOT_FOUND. Likewise, if the values associated
 * with @key cannot be interpreted as integers, or are out of range for
 * #gint, then %NULL is returned
 * and @error is set to #G_KEY_FILE_ERROR_INVALID_VALUE.
 *
 * Returns: (array length=length) (element-type gint) (transfer container):
 *     the values associated with the key as a list of integers, or %NULL if
 *     the key was not found or could not be parsed. The returned list of
 *     integers should be freed with g_free() when no longer needed.
 *
 * Since: 2.6
 **/
gint *
g_key_file_get_integer_list (GKeyFile     *key_file,
			     const gchar  *group_name,
			     const gchar  *key,
			     gsize        *length,
			     GError      **error)
{
  GError *key_file_error = NULL;
  gchar **values;
  gint *int_values;
  gsize i, num_ints;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (length)
    *length = 0;

  values = g_key_file_get_string_list (key_file, group_name, key,
				       &num_ints, &key_file_error);

  if (key_file_error)
    g_propagate_error (error, key_file_error);

  if (!values)
    return NULL;

  int_values = g_new (gint, num_ints);

  for (i = 0; i < num_ints; i++)
    {
      int_values[i] = g_key_file_parse_value_as_integer (key_file,
							 values[i],
							 &key_file_error);

      if (key_file_error)
        {
          g_propagate_error (error, key_file_error);
          g_strfreev (values);
          g_free (int_values);

          return NULL;
        }
    }
  g_strfreev (values);

  if (length)
    *length = num_ints;

  return int_values;
}

/**
 * g_key_file_set_integer_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @list: (array length=length): an array of integer values
 * @length: number of integer values in @list
 *
 * Associates a list of integer values with @key under @group_name.  
 * If @key cannot be found then it is created.
 *
 * Since: 2.6
 **/
void
g_key_file_set_integer_list (GKeyFile    *key_file,
			     const gchar *group_name,
			     const gchar *key,
			     gint         list[],
			     gsize        length)
{
  GString *values;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (list != NULL);

  values = g_string_sized_new (length * 16);
  for (i = 0; i < length; i++)
    {
      gchar *value;

      value = g_key_file_parse_integer_as_value (key_file, list[i]);

      g_string_append (values, value);
      g_string_append_c (values, key_file->list_separator);

      g_free (value);
    }

  g_key_file_set_value (key_file, group_name, key, values->str);
  g_string_free (values, TRUE);
}

/**
 * g_key_file_get_double:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as a
 * double. If @group_name is %NULL, the start_group is used.
 *
 * If @key cannot be found then 0.0 is returned and @error is set to
 * #G_KEY_FILE_ERROR_KEY_NOT_FOUND. Likewise, if the value associated
 * with @key cannot be interpreted as a double then 0.0 is returned
 * and @error is set to #G_KEY_FILE_ERROR_INVALID_VALUE.
 *
 * Returns: the value associated with the key as a double, or
 *     0.0 if the key was not found or could not be parsed.
 *
 * Since: 2.12
 **/
gdouble
g_key_file_get_double  (GKeyFile     *key_file,
                        const gchar  *group_name,
                        const gchar  *key,
                        GError      **error)
{
  GError *key_file_error;
  gchar *value;
  gdouble double_value;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  key_file_error = NULL;

  value = g_key_file_get_value (key_file, group_name, key, &key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return 0;
    }

  double_value = g_key_file_parse_value_as_double (key_file, value,
                                                  &key_file_error);
  g_free (value);

  if (key_file_error)
    {
      if (g_error_matches (key_file_error,
                           G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_INVALID_VALUE))
        {
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_INVALID_VALUE,
                       _("Key file contains key “%s” in group “%s” "
                         "which has a value that cannot be interpreted."),
                       key, group_name);
          g_error_free (key_file_error);
        }
      else
        g_propagate_error (error, key_file_error);
    }

  return double_value;
}

/**
 * g_key_file_set_double:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: an double value
 *
 * Associates a new double value with @key under @group_name.
 * If @key cannot be found then it is created. 
 *
 * Since: 2.12
 **/
void
g_key_file_set_double  (GKeyFile    *key_file,
                        const gchar *group_name,
                        const gchar *key,
                        gdouble      value)
{
  gchar result[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_if_fail (key_file != NULL);

  g_ascii_dtostr (result, sizeof (result), value);
  g_key_file_set_value (key_file, group_name, key, result);
}

/**
 * g_key_file_get_double_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @length: (out): the number of doubles returned
 * @error: return location for a #GError
 *
 * Returns the values associated with @key under @group_name as
 * doubles. 
 *
 * If @key cannot be found then %NULL is returned and @error is set to
 * #G_KEY_FILE_ERROR_KEY_NOT_FOUND. Likewise, if the values associated
 * with @key cannot be interpreted as doubles then %NULL is returned
 * and @error is set to #G_KEY_FILE_ERROR_INVALID_VALUE.
 *
 * Returns: (array length=length) (element-type gdouble) (transfer container):
 *     the values associated with the key as a list of doubles, or %NULL if the
 *     key was not found or could not be parsed. The returned list of doubles
 *     should be freed with g_free() when no longer needed.
 *
 * Since: 2.12
 **/
gdouble *
g_key_file_get_double_list  (GKeyFile     *key_file,
                             const gchar  *group_name,
                             const gchar  *key,
                             gsize        *length,
                             GError      **error)
{
  GError *key_file_error = NULL;
  gchar **values;
  gdouble *double_values;
  gsize i, num_doubles;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  if (length)
    *length = 0;

  values = g_key_file_get_string_list (key_file, group_name, key,
                                       &num_doubles, &key_file_error);

  if (key_file_error)
    g_propagate_error (error, key_file_error);

  if (!values)
    return NULL;

  double_values = g_new (gdouble, num_doubles);

  for (i = 0; i < num_doubles; i++)
    {
      double_values[i] = g_key_file_parse_value_as_double (key_file,
							   values[i],
							   &key_file_error);

      if (key_file_error)
        {
          g_propagate_error (error, key_file_error);
          g_strfreev (values);
          g_free (double_values);

          return NULL;
        }
    }
  g_strfreev (values);

  if (length)
    *length = num_doubles;

  return double_values;
}

/**
 * g_key_file_set_double_list:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @list: (array length=length): an array of double values
 * @length: number of double values in @list
 *
 * Associates a list of double values with @key under
 * @group_name.  If @key cannot be found then it is created.
 *
 * Since: 2.12
 **/
void
g_key_file_set_double_list (GKeyFile    *key_file,
			    const gchar *group_name,
			    const gchar *key,
			    gdouble      list[],
			    gsize        length)
{
  GString *values;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (list != NULL);

  values = g_string_sized_new (length * 16);
  for (i = 0; i < length; i++)
    {
      gchar result[G_ASCII_DTOSTR_BUF_SIZE];

      g_ascii_dtostr( result, sizeof (result), list[i] );

      g_string_append (values, result);
      g_string_append_c (values, key_file->list_separator);
    }

  g_key_file_set_value (key_file, group_name, key, values->str);
  g_string_free (values, TRUE);
}

static gboolean
g_key_file_set_key_comment (GKeyFile     *key_file,
                            const gchar  *group_name,
                            const gchar  *key,
                            const gchar  *comment,
                            GError      **error)
{
  GKeyFileGroup *group;
  GKeyFileKeyValuePair *pair;
  GList *key_node, *comment_node, *tmp;
  
  group = g_key_file_lookup_group (key_file, group_name);
  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name ? group_name : "(null)");

      return FALSE;
    }

  /* First find the key the comments are supposed to be
   * associated with
   */
  key_node = g_key_file_lookup_key_value_pair_node (key_file, group, key);

  if (key_node == NULL)
    {
      set_not_found_key_error (group->name, key, error);
      return FALSE;
    }

  /* Then find all the comments already associated with the
   * key and free them
   */
  tmp = key_node->next;
  while (tmp != NULL)
    {
      pair = (GKeyFileKeyValuePair *) tmp->data;

      if (pair->key != NULL)
        break;

      comment_node = tmp;
      tmp = tmp->next;
      g_key_file_remove_key_value_pair_node (key_file, group,
                                             comment_node); 
    }

  if (comment == NULL)
    return TRUE;

  /* Now we can add our new comment
   */
  pair = g_slice_new (GKeyFileKeyValuePair);
  pair->key = NULL;
  pair->value = g_key_file_parse_comment_as_value (key_file, comment);
  
  key_node = g_list_insert (key_node, pair, 1);

  return TRUE;
}

static gboolean
g_key_file_set_group_comment (GKeyFile     *key_file,
                              const gchar  *group_name,
                              const gchar  *comment,
                              GError      **error)
{
  GKeyFileGroup *group;
  
  g_return_val_if_fail (g_key_file_is_group_name (group_name), FALSE);

  group = g_key_file_lookup_group (key_file, group_name);
  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name ? group_name : "(null)");

      return FALSE;
    }

  /* First remove any existing comment
   */
  if (group->comment)
    {
      g_key_file_key_value_pair_free (group->comment);
      group->comment = NULL;
    }

  if (comment == NULL)
    return TRUE;

  /* Now we can add our new comment
   */
  group->comment = g_slice_new (GKeyFileKeyValuePair);
  group->comment->key = NULL;
  group->comment->value = g_key_file_parse_comment_as_value (key_file, comment);

  return TRUE;
}

static gboolean
g_key_file_set_top_comment (GKeyFile     *key_file,
                            const gchar  *comment,
                            GError      **error)
{
  GList *group_node;
  GKeyFileGroup *group;
  GKeyFileKeyValuePair *pair;

  /* The last group in the list should be the top (comments only)
   * group in the file
   */
  g_warn_if_fail (key_file->groups != NULL);
  group_node = g_list_last (key_file->groups);
  group = (GKeyFileGroup *) group_node->data;
  g_warn_if_fail (group->name == NULL);

  /* Note all keys must be comments at the top of
   * the file, so we can just free it all.
   */
  g_list_free_full (group->key_value_pairs, (GDestroyNotify) g_key_file_key_value_pair_free);
  group->key_value_pairs = NULL;

  if (comment == NULL)
     return TRUE;

  pair = g_slice_new (GKeyFileKeyValuePair);
  pair->key = NULL;
  pair->value = g_key_file_parse_comment_as_value (key_file, comment);
  
  group->key_value_pairs =
    g_list_prepend (group->key_value_pairs, pair);

  return TRUE;
}

/**
 * g_key_file_set_comment:
 * @key_file: a #GKeyFile
 * @group_name: (nullable): a group name, or %NULL
 * @key: (nullable): a key
 * @comment: a comment
 * @error: return location for a #GError
 *
 * Places a comment above @key from @group_name.
 *
 * If @key is %NULL then @comment will be written above @group_name.
 * If both @key and @group_name  are %NULL, then @comment will be
 * written above the first group in the file.
 *
 * Note that this function prepends a '#' comment marker to
 * each line of @comment.
 *
 * Returns: %TRUE if the comment was written, %FALSE otherwise
 *
 * Since: 2.6
 **/
gboolean
g_key_file_set_comment (GKeyFile     *key_file,
                        const gchar  *group_name,
                        const gchar  *key,
                        const gchar  *comment,
                        GError      **error)
{
  g_return_val_if_fail (key_file != NULL, FALSE);

  if (group_name != NULL && key != NULL) 
    {
      if (!g_key_file_set_key_comment (key_file, group_name, key, comment, error))
        return FALSE;
    } 
  else if (group_name != NULL) 
    {
      if (!g_key_file_set_group_comment (key_file, group_name, comment, error))
        return FALSE;
    } 
  else 
    {
      if (!g_key_file_set_top_comment (key_file, comment, error))
        return FALSE;
    }

  return TRUE;
}

static gchar *
g_key_file_get_key_comment (GKeyFile     *key_file,
                            const gchar  *group_name,
                            const gchar  *key,
                            GError      **error)
{
  GKeyFileGroup *group;
  GKeyFileKeyValuePair *pair;
  GList *key_node, *tmp;
  GString *string;
  gchar *comment;

  g_return_val_if_fail (g_key_file_is_group_name (group_name), NULL);

  group = g_key_file_lookup_group (key_file, group_name);
  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name ? group_name : "(null)");

      return NULL;
    }

  /* First find the key the comments are supposed to be
   * associated with
   */
  key_node = g_key_file_lookup_key_value_pair_node (key_file, group, key);

  if (key_node == NULL)
    {
      set_not_found_key_error (group->name, key, error);
      return NULL;
    }

  string = NULL;

  /* Then find all the comments already associated with the
   * key and concatentate them.
   */
  tmp = key_node->next;
  if (!key_node->next)
    return NULL;

  pair = (GKeyFileKeyValuePair *) tmp->data;
  if (pair->key != NULL)
    return NULL;

  while (tmp->next)
    {
      pair = (GKeyFileKeyValuePair *) tmp->next->data;
      
      if (pair->key != NULL)
        break;

      tmp = tmp->next;
    }

  while (tmp != key_node)
    {
      pair = (GKeyFileKeyValuePair *) tmp->data;
      
      if (string == NULL)
	string = g_string_sized_new (512);
      
      comment = g_key_file_parse_value_as_comment (key_file, pair->value);
      g_string_append (string, comment);
      g_free (comment);
      
      tmp = tmp->prev;
    }

  if (string != NULL)
    {
      comment = string->str;
      g_string_free (string, FALSE);
    }
  else
    comment = NULL;

  return comment;
}

static gchar *
get_group_comment (GKeyFile       *key_file,
		   GKeyFileGroup  *group,
		   GError        **error)
{
  GString *string;
  GList *tmp;
  gchar *comment;

  string = NULL;

  tmp = group->key_value_pairs;
  while (tmp)
    {
      GKeyFileKeyValuePair *pair;

      pair = (GKeyFileKeyValuePair *) tmp->data;

      if (pair->key != NULL)
	{
	  tmp = tmp->prev;
	  break;
	}

      if (tmp->next == NULL)
	break;

      tmp = tmp->next;
    }
  
  while (tmp != NULL)
    {
      GKeyFileKeyValuePair *pair;

      pair = (GKeyFileKeyValuePair *) tmp->data;

      if (string == NULL)
        string = g_string_sized_new (512);

      comment = g_key_file_parse_value_as_comment (key_file, pair->value);
      g_string_append (string, comment);
      g_free (comment);

      tmp = tmp->prev;
    }

  if (string != NULL)
    return g_string_free (string, FALSE);

  return NULL;
}

static gchar *
g_key_file_get_group_comment (GKeyFile     *key_file,
                              const gchar  *group_name,
                              GError      **error)
{
  GList *group_node;
  GKeyFileGroup *group;
  
  group = g_key_file_lookup_group (key_file, group_name);
  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name ? group_name : "(null)");

      return NULL;
    }

  if (group->comment)
    return g_strdup (group->comment->value);
  
  group_node = g_key_file_lookup_group_node (key_file, group_name);
  group_node = group_node->next;
  group = (GKeyFileGroup *)group_node->data;  
  return get_group_comment (key_file, group, error);
}

static gchar *
g_key_file_get_top_comment (GKeyFile  *key_file,
                            GError   **error)
{
  GList *group_node;
  GKeyFileGroup *group;

  /* The last group in the list should be the top (comments only)
   * group in the file
   */
  g_warn_if_fail (key_file->groups != NULL);
  group_node = g_list_last (key_file->groups);
  group = (GKeyFileGroup *) group_node->data;
  g_warn_if_fail (group->name == NULL);

  return get_group_comment (key_file, group, error);
}

/**
 * g_key_file_get_comment:
 * @key_file: a #GKeyFile
 * @group_name: (nullable): a group name, or %NULL
 * @key: a key
 * @error: return location for a #GError
 *
 * Retrieves a comment above @key from @group_name.
 * If @key is %NULL then @comment will be read from above
 * @group_name. If both @key and @group_name are %NULL, then
 * @comment will be read from above the first group in the file.
 *
 * Note that the returned string includes the '#' comment markers.
 *
 * Returns: a comment that should be freed with g_free()
 *
 * Since: 2.6
 **/
gchar * 
g_key_file_get_comment (GKeyFile     *key_file,
                        const gchar  *group_name,
                        const gchar  *key,
                        GError      **error)
{
  g_return_val_if_fail (key_file != NULL, NULL);

  if (group_name != NULL && key != NULL)
    return g_key_file_get_key_comment (key_file, group_name, key, error);
  else if (group_name != NULL)
    return g_key_file_get_group_comment (key_file, group_name, error);
  else
    return g_key_file_get_top_comment (key_file, error);
}

/**
 * g_key_file_remove_comment:
 * @key_file: a #GKeyFile
 * @group_name: (nullable): a group name, or %NULL
 * @key: (nullable): a key
 * @error: return location for a #GError
 *
 * Removes a comment above @key from @group_name.
 * If @key is %NULL then @comment will be removed above @group_name. 
 * If both @key and @group_name are %NULL, then @comment will
 * be removed above the first group in the file.
 *
 * Returns: %TRUE if the comment was removed, %FALSE otherwise
 *
 * Since: 2.6
 **/

gboolean
g_key_file_remove_comment (GKeyFile     *key_file,
                           const gchar  *group_name,
                           const gchar  *key,
                           GError      **error)
{
  g_return_val_if_fail (key_file != NULL, FALSE);

  if (group_name != NULL && key != NULL)
    return g_key_file_set_key_comment (key_file, group_name, key, NULL, error);
  else if (group_name != NULL)
    return g_key_file_set_group_comment (key_file, group_name, NULL, error);
  else
    return g_key_file_set_top_comment (key_file, NULL, error);
}

/**
 * g_key_file_has_group:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 *
 * Looks whether the key file has the group @group_name.
 *
 * Returns: %TRUE if @group_name is a part of @key_file, %FALSE
 * otherwise.
 * Since: 2.6
 **/
gboolean
g_key_file_has_group (GKeyFile    *key_file,
		      const gchar *group_name)
{
  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (group_name != NULL, FALSE);

  return g_key_file_lookup_group (key_file, group_name) != NULL;
}

/* This code remains from a historical attempt to add a new public API
 * which respects the GError rules.
 */
static gboolean
g_key_file_has_key_full (GKeyFile     *key_file,
			 const gchar  *group_name,
			 const gchar  *key,
			 gboolean     *has_key,
			 GError      **error)
{
  GKeyFileKeyValuePair *pair;
  GKeyFileGroup *group;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (group_name != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  group = g_key_file_lookup_group (key_file, group_name);

  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name);

      return FALSE;
    }

  pair = g_key_file_lookup_key_value_pair (key_file, group, key);

  if (has_key)
    *has_key = pair != NULL;
  return TRUE;
}

/**
 * g_key_file_has_key: (skip)
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key name
 * @error: return location for a #GError
 *
 * Looks whether the key file has the key @key in the group
 * @group_name.
 *
 * Note that this function does not follow the rules for #GError strictly;
 * the return value both carries meaning and signals an error.  To use
 * this function, you must pass a #GError pointer in @error, and check
 * whether it is not %NULL to see if an error occurred.
 *
 * Language bindings should use g_key_file_get_value() to test whether
 * or not a key exists.
 *
 * Returns: %TRUE if @key is a part of @group_name, %FALSE otherwise
 *
 * Since: 2.6
 **/
gboolean
g_key_file_has_key (GKeyFile     *key_file,
		    const gchar  *group_name,
		    const gchar  *key,
		    GError      **error)
{
  GError *temp_error = NULL;
  gboolean has_key;

  if (g_key_file_has_key_full (key_file, group_name, key, &has_key, &temp_error))
    {
      return has_key;
    }
  else
    {
      g_propagate_error (error, temp_error);
      return FALSE;
    }
}

static void
g_key_file_add_group (GKeyFile    *key_file,
		      const gchar *group_name)
{
  GKeyFileGroup *group;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (g_key_file_is_group_name (group_name));

  group = g_key_file_lookup_group (key_file, group_name);
  if (group != NULL)
    {
      key_file->current_group = group;
      return;
    }

  group = g_slice_new0 (GKeyFileGroup);
  group->name = g_strdup (group_name);
  group->lookup_map = g_hash_table_new (g_str_hash, g_str_equal);
  key_file->groups = g_list_prepend (key_file->groups, group);
  key_file->current_group = group;

  if (key_file->start_group == NULL)
    key_file->start_group = group;

  g_hash_table_insert (key_file->group_hash, (gpointer)group->name, group);
}

static void
g_key_file_key_value_pair_free (GKeyFileKeyValuePair *pair)
{
  if (pair != NULL)
    {
      g_free (pair->key);
      g_free (pair->value);
      g_slice_free (GKeyFileKeyValuePair, pair);
    }
}

/* Be careful not to call this function on a node with data in the
 * lookup map without removing it from the lookup map, first.
 *
 * Some current cases where this warning is not a concern are
 * when:
 *   - the node being removed is a comment node
 *   - the entire lookup map is getting destroyed soon after
 *     anyway.
 */ 
static void
g_key_file_remove_key_value_pair_node (GKeyFile      *key_file,
                                       GKeyFileGroup *group,
			               GList         *pair_node)
{

  GKeyFileKeyValuePair *pair;

  pair = (GKeyFileKeyValuePair *) pair_node->data;

  group->key_value_pairs = g_list_remove_link (group->key_value_pairs, pair_node);

  g_warn_if_fail (pair->value != NULL);

  g_key_file_key_value_pair_free (pair);

  g_list_free_1 (pair_node);
}

static void
g_key_file_remove_group_node (GKeyFile *key_file,
			      GList    *group_node)
{
  GKeyFileGroup *group;
  GList *tmp;

  group = (GKeyFileGroup *) group_node->data;

  if (group->name)
    g_hash_table_remove (key_file->group_hash, group->name);

  /* If the current group gets deleted make the current group the last
   * added group.
   */
  if (key_file->current_group == group)
    {
      /* groups should always contain at least the top comment group,
       * unless g_key_file_clear has been called
       */
      if (key_file->groups)
        key_file->current_group = (GKeyFileGroup *) key_file->groups->data;
      else
        key_file->current_group = NULL;
    }

  /* If the start group gets deleted make the start group the first
   * added group.
   */
  if (key_file->start_group == group)
    {
      tmp = g_list_last (key_file->groups);
      while (tmp != NULL)
	{
	  if (tmp != group_node &&
	      ((GKeyFileGroup *) tmp->data)->name != NULL)
	    break;

	  tmp = tmp->prev;
	}

      if (tmp)
        key_file->start_group = (GKeyFileGroup *) tmp->data;
      else
        key_file->start_group = NULL;
    }

  key_file->groups = g_list_remove_link (key_file->groups, group_node);

  tmp = group->key_value_pairs;
  while (tmp != NULL)
    {
      GList *pair_node;

      pair_node = tmp;
      tmp = tmp->next;
      g_key_file_remove_key_value_pair_node (key_file, group, pair_node);
    }

  g_warn_if_fail (group->key_value_pairs == NULL);

  if (group->comment)
    {
      g_key_file_key_value_pair_free (group->comment);
      group->comment = NULL;
    }

  if (group->lookup_map)
    {
      g_hash_table_destroy (group->lookup_map);
      group->lookup_map = NULL;
    }

  g_free ((gchar *) group->name);
  g_slice_free (GKeyFileGroup, group);
  g_list_free_1 (group_node);
}

/**
 * g_key_file_remove_group:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @error: return location for a #GError or %NULL
 *
 * Removes the specified group, @group_name, 
 * from the key file. 
 *
 * Returns: %TRUE if the group was removed, %FALSE otherwise
 *
 * Since: 2.6
 **/
gboolean
g_key_file_remove_group (GKeyFile     *key_file,
			 const gchar  *group_name,
			 GError      **error)
{
  GList *group_node;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (group_name != NULL, FALSE);

  group_node = g_key_file_lookup_group_node (key_file, group_name);

  if (!group_node)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
		   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
		   _("Key file does not have group “%s”"),
		   group_name);
      return FALSE;
    }

  g_key_file_remove_group_node (key_file, group_node);

  return TRUE;  
}

static void
g_key_file_add_key_value_pair (GKeyFile             *key_file,
                               GKeyFileGroup        *group,
                               GKeyFileKeyValuePair *pair)
{
  g_hash_table_replace (group->lookup_map, pair->key, pair);
  group->key_value_pairs = g_list_prepend (group->key_value_pairs, pair);
}

static void
g_key_file_add_key (GKeyFile      *key_file,
		    GKeyFileGroup *group,
		    const gchar   *key,
		    const gchar   *value)
{
  GKeyFileKeyValuePair *pair;

  pair = g_slice_new (GKeyFileKeyValuePair);
  pair->key = g_strdup (key);
  pair->value = g_strdup (value);

  g_key_file_add_key_value_pair (key_file, group, pair);
}

/**
 * g_key_file_remove_key:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key name to remove
 * @error: return location for a #GError or %NULL
 *
 * Removes @key in @group_name from the key file. 
 *
 * Returns: %TRUE if the key was removed, %FALSE otherwise
 *
 * Since: 2.6
 **/
gboolean
g_key_file_remove_key (GKeyFile     *key_file,
		       const gchar  *group_name,
		       const gchar  *key,
		       GError      **error)
{
  GKeyFileGroup *group;
  GKeyFileKeyValuePair *pair;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (group_name != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  pair = NULL;

  group = g_key_file_lookup_group (key_file, group_name);
  if (!group)
    {
      g_set_error (error, G_KEY_FILE_ERROR,
                   G_KEY_FILE_ERROR_GROUP_NOT_FOUND,
                   _("Key file does not have group “%s”"),
                   group_name);
      return FALSE;
    }

  pair = g_key_file_lookup_key_value_pair (key_file, group, key);

  if (!pair)
    {
      set_not_found_key_error (group->name, key, error);
      return FALSE;
    }

  group->key_value_pairs = g_list_remove (group->key_value_pairs, pair);
  g_hash_table_remove (group->lookup_map, pair->key);
  g_key_file_key_value_pair_free (pair);

  return TRUE;
}

static GList *
g_key_file_lookup_group_node (GKeyFile    *key_file,
			      const gchar *group_name)
{
  GKeyFileGroup *group;
  GList *tmp;

  for (tmp = key_file->groups; tmp != NULL; tmp = tmp->next)
    {
      group = (GKeyFileGroup *) tmp->data;

      if (group && group->name && strcmp (group->name, group_name) == 0)
        break;
    }

  return tmp;
}

static GKeyFileGroup *
g_key_file_lookup_group (GKeyFile    *key_file,
			 const gchar *group_name)
{
  return (GKeyFileGroup *)g_hash_table_lookup (key_file->group_hash, group_name);
}

static GList *
g_key_file_lookup_key_value_pair_node (GKeyFile       *key_file,
			               GKeyFileGroup  *group,
                                       const gchar    *key)
{
  GList *key_node;

  for (key_node = group->key_value_pairs;
       key_node != NULL;
       key_node = key_node->next)
    {
      GKeyFileKeyValuePair *pair;

      pair = (GKeyFileKeyValuePair *) key_node->data; 

      if (pair->key && strcmp (pair->key, key) == 0)
        break;
    }

  return key_node;
}

static GKeyFileKeyValuePair *
g_key_file_lookup_key_value_pair (GKeyFile      *key_file,
				  GKeyFileGroup *group,
				  const gchar   *key)
{
  return (GKeyFileKeyValuePair *) g_hash_table_lookup (group->lookup_map, key);
}

/* Lines starting with # or consisting entirely of whitespace are merely
 * recorded, not parsed. This function assumes all leading whitespace
 * has been stripped.
 */
static gboolean
g_key_file_line_is_comment (const gchar *line)
{
  return (*line == '#' || *line == '\0' || *line == '\n');
}

static gboolean 
g_key_file_is_group_name (const gchar *name)
{
  gchar *p, *q;

  if (name == NULL)
    return FALSE;

  p = q = (gchar *) name;
  while (*q && *q != ']' && *q != '[' && !g_ascii_iscntrl (*q))
    q = g_utf8_find_next_char (q, NULL);
  
  if (*q != '\0' || q == p)
    return FALSE;

  return TRUE;
}

static gboolean
g_key_file_is_key_name (const gchar *name)
{
  gchar *p, *q;

  if (name == NULL)
    return FALSE;

  p = q = (gchar *) name;
  /* We accept a little more than the desktop entry spec says,
   * since gnome-vfs uses mime-types as keys in its cache.
   */
  while (*q && *q != '=' && *q != '[' && *q != ']')
    q = g_utf8_find_next_char (q, NULL);
  
  /* No empty keys, please */
  if (q == p)
    return FALSE;

  /* We accept spaces in the middle of keys to not break
   * existing apps, but we don't tolerate initial or final
   * spaces, which would lead to silent corruption when
   * rereading the file.
   */
  if (*p == ' ' || q[-1] == ' ')
    return FALSE;

  if (*q == '[')
    {
      q++;
      while (*q && (g_unichar_isalnum (g_utf8_get_char_validated (q, -1)) || *q == '-' || *q == '_' || *q == '.' || *q == '@'))
        q = g_utf8_find_next_char (q, NULL);

      if (*q != ']')
        return FALSE;     

      q++;
    }

  if (*q != '\0')
    return FALSE;

  return TRUE;
}

/* A group in a key file is made up of a starting '[' followed by one
 * or more letters making up the group name followed by ']'.
 */
static gboolean
g_key_file_line_is_group (const gchar *line)
{
  gchar *p;

  p = (gchar *) line;
  if (*p != '[')
    return FALSE;

  p++;

  while (*p && *p != ']')
    p = g_utf8_find_next_char (p, NULL);

  if (*p != ']')
    return FALSE;
 
  /* silently accept whitespace after the ] */
  p = g_utf8_find_next_char (p, NULL);
  while (*p == ' ' || *p == '\t')
    p = g_utf8_find_next_char (p, NULL);
     
  if (*p)
    return FALSE;

  return TRUE;
}

static gboolean
g_key_file_line_is_key_value_pair (const gchar *line)
{
  gchar *p;

  p = (gchar *) g_utf8_strchr (line, -1, '=');

  if (!p)
    return FALSE;

  /* Key must be non-empty
   */
  if (*p == line[0])
    return FALSE;

  return TRUE;
}

static gchar *
g_key_file_parse_value_as_string (GKeyFile     *key_file,
				  const gchar  *value,
				  GSList      **pieces,
				  GError      **error)
{
  gchar *string_value, *p, *q0, *q;

  string_value = g_new (gchar, strlen (value) + 1);

  p = (gchar *) value;
  q0 = q = string_value;
  while (*p)
    {
      if (*p == '\\')
        {
          p++;

          switch (*p)
            {
            case 's':
              *q = ' ';
              break;

            case 'n':
              *q = '\n';
              break;

            case 't':
              *q = '\t';
              break;

            case 'r':
              *q = '\r';
              break;

            case '\\':
              *q = '\\';
              break;

	    case '\0':
	      g_set_error_literal (error, G_KEY_FILE_ERROR,
                                   G_KEY_FILE_ERROR_INVALID_VALUE,
                                   _("Key file contains escape character "
                                     "at end of line"));
	      break;

            default:
	      if (pieces && *p == key_file->list_separator)
		*q = key_file->list_separator;
	      else
		{
		  *q++ = '\\';
		  *q = *p;
		  
		  if (*error == NULL)
		    {
		      gchar sequence[3];
		      
		      sequence[0] = '\\';
		      sequence[1] = *p;
		      sequence[2] = '\0';
		      
		      g_set_error (error, G_KEY_FILE_ERROR,
				   G_KEY_FILE_ERROR_INVALID_VALUE,
				   _("Key file contains invalid escape "
				     "sequence “%s”"), sequence);
		    }
		}
              break;
            }
        }
      else
	{
	  *q = *p;
	  if (pieces && (*p == key_file->list_separator))
	    {
	      *pieces = g_slist_prepend (*pieces, g_strndup (q0, q - q0));
	      q0 = q + 1; 
	    }
	}

      if (*p == '\0')
	break;

      q++;
      p++;
    }

  *q = '\0';
  if (pieces)
  {
    if (q0 < q)
      *pieces = g_slist_prepend (*pieces, g_strndup (q0, q - q0));
    *pieces = g_slist_reverse (*pieces);
  }

  return string_value;
}

static gchar *
g_key_file_parse_string_as_value (GKeyFile    *key_file,
				  const gchar *string,
				  gboolean     escape_separator)
{
  gchar *value, *p, *q;
  gsize length;
  gboolean parsing_leading_space;

  length = strlen (string) + 1;

  /* Worst case would be that every character needs to be escaped.
   * In other words every character turns to two characters
   */
  value = g_new (gchar, 2 * length);

  p = (gchar *) string;
  q = value;
  parsing_leading_space = TRUE;
  while (p < (string + length - 1))
    {
      gchar escaped_character[3] = { '\\', 0, 0 };

      switch (*p)
        {
        case ' ':
          if (parsing_leading_space)
            {
              escaped_character[1] = 's';
              strcpy (q, escaped_character);
              q += 2;
            }
          else
            {
	      *q = *p;
	      q++;
            }
          break;
        case '\t':
          if (parsing_leading_space)
            {
              escaped_character[1] = 't';
              strcpy (q, escaped_character);
              q += 2;
            }
          else
            {
	      *q = *p;
	      q++;
            }
          break;
        case '\n':
          escaped_character[1] = 'n';
          strcpy (q, escaped_character);
          q += 2;
          break;
        case '\r':
          escaped_character[1] = 'r';
          strcpy (q, escaped_character);
          q += 2;
          break;
        case '\\':
          escaped_character[1] = '\\';
          strcpy (q, escaped_character);
          q += 2;
          parsing_leading_space = FALSE;
          break;
        default:
	  if (escape_separator && *p == key_file->list_separator)
	    {
	      escaped_character[1] = key_file->list_separator;
	      strcpy (q, escaped_character);
	      q += 2;
              parsing_leading_space = TRUE;
	    }
	  else 
	    {
	      *q = *p;
	      q++;
              parsing_leading_space = FALSE;
	    }
          break;
        }
      p++;
    }
  *q = '\0';

  return value;
}

static gint
g_key_file_parse_value_as_integer (GKeyFile     *key_file,
				   const gchar  *value,
				   GError      **error)
{
  gchar *eof_int;
  glong long_value;
  gint int_value;
  int errsv;

  errno = 0;
  long_value = strtol (value, &eof_int, 10);
  errsv = errno;

  if (*value == '\0' || (*eof_int != '\0' && !g_ascii_isspace(*eof_int)))
    {
      gchar *value_utf8 = g_utf8_make_valid (value, -1);
      g_set_error (error, G_KEY_FILE_ERROR,
		   G_KEY_FILE_ERROR_INVALID_VALUE,
		   _("Value “%s” cannot be interpreted "
		     "as a number."), value_utf8);
      g_free (value_utf8);

      return 0;
    }

  int_value = long_value;
  if (int_value != long_value || errsv == ERANGE)
    {
      gchar *value_utf8 = g_utf8_make_valid (value, -1);
      g_set_error (error,
		   G_KEY_FILE_ERROR, 
		   G_KEY_FILE_ERROR_INVALID_VALUE,
		   _("Integer value “%s” out of range"), 
		   value_utf8);
      g_free (value_utf8);

      return 0;
    }
  
  return int_value;
}

static gchar *
g_key_file_parse_integer_as_value (GKeyFile *key_file,
				   gint      value)

{
  return g_strdup_printf ("%d", value);
}

static gdouble
g_key_file_parse_value_as_double  (GKeyFile     *key_file,
                                   const gchar  *value,
                                   GError      **error)
{
  gchar *end_of_valid_d;
  gdouble double_value = 0;

  double_value = g_ascii_strtod (value, &end_of_valid_d);

  if (*end_of_valid_d != '\0' || end_of_valid_d == value)
    {
      gchar *value_utf8 = g_utf8_make_valid (value, -1);
      g_set_error (error, G_KEY_FILE_ERROR,
		   G_KEY_FILE_ERROR_INVALID_VALUE,
		   _("Value “%s” cannot be interpreted "
		     "as a float number."), 
		   value_utf8);
      g_free (value_utf8);

      double_value = 0;
    }

  return double_value;
}

static gint
strcmp_sized (const gchar *s1, size_t len1, const gchar *s2)
{
  size_t len2 = strlen (s2);
  return strncmp (s1, s2, MAX (len1, len2));
}

static gboolean
g_key_file_parse_value_as_boolean (GKeyFile     *key_file,
				   const gchar  *value,
				   GError      **error)
{
  gchar *value_utf8;
  gint i, length = 0;

  /* Count the number of non-whitespace characters */
  for (i = 0; value[i]; i++)
    if (!g_ascii_isspace (value[i]))
      length = i + 1;

  if (strcmp_sized (value, length, "true") == 0 || strcmp_sized (value, length, "1") == 0)
    return TRUE;
  else if (strcmp_sized (value, length, "false") == 0 || strcmp_sized (value, length, "0") == 0)
    return FALSE;

  value_utf8 = g_utf8_make_valid (value, -1);
  g_set_error (error, G_KEY_FILE_ERROR,
               G_KEY_FILE_ERROR_INVALID_VALUE,
               _("Value “%s” cannot be interpreted "
		 "as a boolean."), value_utf8);
  g_free (value_utf8);

  return FALSE;
}

static gchar *
g_key_file_parse_boolean_as_value (GKeyFile *key_file,
				   gboolean  value)
{
  if (value)
    return g_strdup ("true");
  else
    return g_strdup ("false");
}

static gchar *
g_key_file_parse_value_as_comment (GKeyFile    *key_file,
                                   const gchar *value)
{
  GString *string;
  gchar **lines;
  gsize i;

  string = g_string_sized_new (512);

  lines = g_strsplit (value, "\n", 0);

  for (i = 0; lines[i] != NULL; i++)
    {
        if (lines[i][0] != '#')
           g_string_append_printf (string, "%s\n", lines[i]);
        else 
           g_string_append_printf (string, "%s\n", lines[i] + 1);
    }
  g_strfreev (lines);

  return g_string_free (string, FALSE);
}

static gchar *
g_key_file_parse_comment_as_value (GKeyFile      *key_file,
                                   const gchar   *comment)
{
  GString *string;
  gchar **lines;
  gsize i;

  string = g_string_sized_new (512);

  lines = g_strsplit (comment, "\n", 0);

  for (i = 0; lines[i] != NULL; i++)
    g_string_append_printf (string, "#%s%s", lines[i], 
                            lines[i + 1] == NULL? "" : "\n");
  g_strfreev (lines);

  return g_string_free (string, FALSE);
}

/**
 * g_key_file_save_to_file:
 * @key_file: a #GKeyFile
 * @filename: the name of the file to write to
 * @error: a pointer to a %NULL #GError, or %NULL
 *
 * Writes the contents of @key_file to @filename using
 * g_file_set_contents().
 *
 * This function can fail for any of the reasons that
 * g_file_set_contents() may fail.
 *
 * Returns: %TRUE if successful, else %FALSE with @error set
 *
 * Since: 2.40
 */
gboolean
g_key_file_save_to_file (GKeyFile     *key_file,
                         const gchar  *filename,
                         GError      **error)
{
  gchar *contents;
  gboolean success;
  gsize length;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  contents = g_key_file_to_data (key_file, &length, NULL);
  g_assert (contents != NULL);

  success = g_file_set_contents (filename, contents, length, error);
  g_free (contents);

  return success;
}
