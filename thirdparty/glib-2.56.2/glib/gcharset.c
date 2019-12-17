/* gcharset.c - Charset information
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 */

#include "config.h"

#include "gcharsetprivate.h"

#include "garray.h"
#include "genviron.h"
#include "ghash.h"
#include "gmessages.h"
#include "gstrfuncs.h"
#include "gthread.h"
#ifdef G_OS_WIN32
#include "gwin32.h"
#endif

#include "libcharset/libcharset.h"

#include <string.h>
#include <stdio.h>

G_LOCK_DEFINE_STATIC (aliases);

static GHashTable *
get_alias_hash (void)
{
  static GHashTable *alias_hash = NULL;
  const char *aliases;

  G_LOCK (aliases);

  if (!alias_hash)
    {
      alias_hash = g_hash_table_new (g_str_hash, g_str_equal);

      aliases = _g_locale_get_charset_aliases ();
      while (*aliases != '\0')
        {
          const char *canonical;
          const char *alias;
          const char **alias_array;
          int count = 0;

          alias = aliases;
          aliases += strlen (aliases) + 1;
          canonical = aliases;
          aliases += strlen (aliases) + 1;

          alias_array = g_hash_table_lookup (alias_hash, canonical);
          if (alias_array)
            {
              while (alias_array[count])
                count++;
            }

          alias_array = g_renew (const char *, alias_array, count + 2);
          alias_array[count] = alias;
          alias_array[count + 1] = NULL;

          g_hash_table_insert (alias_hash, (char *)canonical, alias_array);
        }
    }

  G_UNLOCK (aliases);

  return alias_hash;
}

/* As an abuse of the alias table, the following routines gets
 * the charsets that are aliases for the canonical name.
 */
const char **
_g_charset_get_aliases (const char *canonical_name)
{
  GHashTable *alias_hash = get_alias_hash ();

  return g_hash_table_lookup (alias_hash, canonical_name);
}

static gboolean
g_utf8_get_charset_internal (const char  *raw_data,
                             const char **a)
{
  const char *charset = g_getenv ("CHARSET");

  if (charset && *charset)
    {
      *a = charset;

      if (charset && strstr (charset, "UTF-8"))
        return TRUE;
      else
        return FALSE;
    }

  /* The libcharset code tries to be thread-safe without
   * a lock, but has a memory leak and a missing memory
   * barrier, so we lock for it
   */
  G_LOCK (aliases);
  charset = _g_locale_charset_unalias (raw_data);
  G_UNLOCK (aliases);

  if (charset && *charset)
    {
      *a = charset;

      if (charset && strstr (charset, "UTF-8"))
        return TRUE;
      else
        return FALSE;
    }

  /* Assume this for compatibility at present.  */
  *a = "US-ASCII";

  return FALSE;
}

typedef struct _GCharsetCache GCharsetCache;

struct _GCharsetCache {
  gboolean is_utf8;
  gchar *raw;
  gchar *charset;
};

static void
charset_cache_free (gpointer data)
{
  GCharsetCache *cache = data;
  g_free (cache->raw);
  g_free (cache->charset);
  g_free (cache);
}

/**
 * g_get_charset:
 * @charset: (out) (optional) (transfer none): return location for character set
 *   name, or %NULL.
 *
 * Obtains the character set for the [current locale][setlocale]; you
 * might use this character set as an argument to g_convert(), to convert
 * from the current locale's encoding to some other encoding. (Frequently
 * g_locale_to_utf8() and g_locale_from_utf8() are nice shortcuts, though.)
 *
 * On Windows the character set returned by this function is the
 * so-called system default ANSI code-page. That is the character set
 * used by the "narrow" versions of C library and Win32 functions that
 * handle file names. It might be different from the character set
 * used by the C library's current locale.
 *
 * On Linux, the character set is found by consulting nl_langinfo() if
 * available. If not, the environment variables `LC_ALL`, `LC_CTYPE`, `LANG`
 * and `CHARSET` are queried in order.
 *
 * The return value is %TRUE if the locale's encoding is UTF-8, in that
 * case you can perhaps avoid calling g_convert().
 *
 * The string returned in @charset is not allocated, and should not be
 * freed.
 *
 * Returns: %TRUE if the returned charset is UTF-8
 */
gboolean
g_get_charset (const char **charset)
{
  static GPrivate cache_private = G_PRIVATE_INIT (charset_cache_free);
  GCharsetCache *cache = g_private_get (&cache_private);
  const gchar *raw;

  if (!cache)
    {
      cache = g_new0 (GCharsetCache, 1);
      g_private_set (&cache_private, cache);
    }

  G_LOCK (aliases);
  raw = _g_locale_charset_raw ();
  G_UNLOCK (aliases);

  if (!(cache->raw && strcmp (cache->raw, raw) == 0))
    {
      const gchar *new_charset;

      g_free (cache->raw);
      g_free (cache->charset);
      cache->raw = g_strdup (raw);
      cache->is_utf8 = g_utf8_get_charset_internal (raw, &new_charset);
      cache->charset = g_strdup (new_charset);
    }

  if (charset)
    *charset = cache->charset;

  return cache->is_utf8;
}

/**
 * g_get_codeset:
 *
 * Gets the character set for the current locale.
 *
 * Returns: a newly allocated string containing the name
 *     of the character set. This string must be freed with g_free().
 */
gchar *
g_get_codeset (void)
{
  const gchar *charset;

  g_get_charset (&charset);

  return g_strdup (charset);
}

#ifndef G_OS_WIN32

/* read an alias file for the locales */
static void
read_aliases (gchar      *file,
              GHashTable *alias_table)
{
  FILE *fp;
  char buf[256];

  fp = fopen (file,"r");
  if (!fp)
    return;
  while (fgets (buf, 256, fp))
    {
      char *p, *q;

      g_strstrip (buf);

      /* Line is a comment */
      if ((buf[0] == '#') || (buf[0] == '\0'))
        continue;

      /* Reads first column */
      for (p = buf, q = NULL; *p; p++) {
        if ((*p == '\t') || (*p == ' ') || (*p == ':')) {
          *p = '\0';
          q = p+1;
          while ((*q == '\t') || (*q == ' ')) {
            q++;
          }
          break;
        }
      }
      /* The line only had one column */
      if (!q || *q == '\0')
        continue;

      /* Read second column */
      for (p = q; *p; p++) {
        if ((*p == '\t') || (*p == ' ')) {
          *p = '\0';
          break;
        }
      }

      /* Add to alias table if necessary */
      if (!g_hash_table_lookup (alias_table, buf)) {
        g_hash_table_insert (alias_table, g_strdup (buf), g_strdup (q));
      }
    }
  fclose (fp);
}

#endif

static char *
unalias_lang (char *lang)
{
#ifndef G_OS_WIN32
  static GHashTable *alias_table = NULL;
  char *p;
  int i;

  if (g_once_init_enter (&alias_table))
    {
      GHashTable *table = g_hash_table_new (g_str_hash, g_str_equal);
      read_aliases ("/usr/share/locale/locale.alias", table);
      g_once_init_leave (&alias_table, table);
    }

  i = 0;
  while ((p = g_hash_table_lookup (alias_table, lang)) && (strcmp (p, lang) != 0))
    {
      lang = p;
      if (i++ == 30)
        {
          static gboolean said_before = FALSE;
          if (!said_before)
            g_warning ("Too many alias levels for a locale, "
                       "may indicate a loop");
          said_before = TRUE;
          return lang;
        }
    }
#endif
  return lang;
}

/* Mask for components of locale spec. The ordering here is from
 * least significant to most significant
 */
enum
{
  COMPONENT_CODESET =   1 << 0,
  COMPONENT_TERRITORY = 1 << 1,
  COMPONENT_MODIFIER =  1 << 2
};

/* Break an X/Open style locale specification into components
 */
static guint
explode_locale (const gchar *locale,
                gchar      **language,
                gchar      **territory,
                gchar      **codeset,
                gchar      **modifier)
{
  const gchar *uscore_pos;
  const gchar *at_pos;
  const gchar *dot_pos;

  guint mask = 0;

  uscore_pos = strchr (locale, '_');
  dot_pos = strchr (uscore_pos ? uscore_pos : locale, '.');
  at_pos = strchr (dot_pos ? dot_pos : (uscore_pos ? uscore_pos : locale), '@');

  if (at_pos)
    {
      mask |= COMPONENT_MODIFIER;
      *modifier = g_strdup (at_pos);
    }
  else
    at_pos = locale + strlen (locale);

  if (dot_pos)
    {
      mask |= COMPONENT_CODESET;
      *codeset = g_strndup (dot_pos, at_pos - dot_pos);
    }
  else
    dot_pos = at_pos;

  if (uscore_pos)
    {
      mask |= COMPONENT_TERRITORY;
      *territory = g_strndup (uscore_pos, dot_pos - uscore_pos);
    }
  else
    uscore_pos = dot_pos;

  *language = g_strndup (locale, uscore_pos - locale);

  return mask;
}

/*
 * Compute all interesting variants for a given locale name -
 * by stripping off different components of the value.
 *
 * For simplicity, we assume that the locale is in
 * X/Open format: language[_territory][.codeset][@modifier]
 *
 * TODO: Extend this to handle the CEN format (see the GNUlibc docs)
 *       as well. We could just copy the code from glibc wholesale
 *       but it is big, ugly, and complicated, so I'm reluctant
 *       to do so when this should handle 99% of the time...
 */
static void
append_locale_variants (GPtrArray *array,
                        const gchar *locale)
{
  gchar *language = NULL;
  gchar *territory = NULL;
  gchar *codeset = NULL;
  gchar *modifier = NULL;

  guint mask;
  guint i, j;

  g_return_if_fail (locale != NULL);

  mask = explode_locale (locale, &language, &territory, &codeset, &modifier);

  /* Iterate through all possible combinations, from least attractive
   * to most attractive.
   */
  for (j = 0; j <= mask; ++j)
    {
      i = mask - j;

      if ((i & ~mask) == 0)
        {
          gchar *val = g_strconcat (language,
                                    (i & COMPONENT_TERRITORY) ? territory : "",
                                    (i & COMPONENT_CODESET) ? codeset : "",
                                    (i & COMPONENT_MODIFIER) ? modifier : "",
                                    NULL);
          g_ptr_array_add (array, val);
        }
    }

  g_free (language);
  if (mask & COMPONENT_CODESET)
    g_free (codeset);
  if (mask & COMPONENT_TERRITORY)
    g_free (territory);
  if (mask & COMPONENT_MODIFIER)
    g_free (modifier);
}

/**
 * g_get_locale_variants:
 * @locale: a locale identifier
 *
 * Returns a list of derived variants of @locale, which can be used to
 * e.g. construct locale-dependent filenames or search paths. The returned
 * list is sorted from most desirable to least desirable.
 * This function handles territory, charset and extra locale modifiers.
 *
 * For example, if @locale is "fr_BE", then the returned list
 * is "fr_BE", "fr".
 *
 * If you need the list of variants for the current locale,
 * use g_get_language_names().
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly
 *   allocated array of newly allocated strings with the locale variants. Free with
 *   g_strfreev().
 *
 * Since: 2.28
 */
gchar **
g_get_locale_variants (const gchar *locale)
{
  GPtrArray *array;

  g_return_val_if_fail (locale != NULL, NULL);

  array = g_ptr_array_sized_new (8);
  append_locale_variants (array, locale);
  g_ptr_array_add (array, NULL);

  return (gchar **) g_ptr_array_free (array, FALSE);
}

/* The following is (partly) taken from the gettext package.
   Copyright (C) 1995, 1996, 1997, 1998 Free Software Foundation, Inc.  */

static const gchar *
guess_category_value (const gchar *category_name)
{
  const gchar *retval;

  /* The highest priority value is the 'LANGUAGE' environment
     variable.  This is a GNU extension.  */
  retval = g_getenv ("LANGUAGE");
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  /* 'LANGUAGE' is not set.  So we have to proceed with the POSIX
     methods of looking to 'LC_ALL', 'LC_xxx', and 'LANG'.  On some
     systems this can be done by the 'setlocale' function itself.  */

  /* Setting of LC_ALL overwrites all other.  */
  retval = g_getenv ("LC_ALL");
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  /* Next comes the name of the desired category.  */
  retval = g_getenv (category_name);
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

  /* Last possibility is the LANG environment variable.  */
  retval = g_getenv ("LANG");
  if ((retval != NULL) && (retval[0] != '\0'))
    return retval;

#ifdef G_PLATFORM_WIN32
  /* g_win32_getlocale() first checks for LC_ALL, LC_MESSAGES and
   * LANG, which we already did above. Oh well. The main point of
   * calling g_win32_getlocale() is to get the thread's locale as used
   * by Windows and the Microsoft C runtime (in the "English_United
   * States" format) translated into the Unixish format.
   */
  {
    char *locale = g_win32_getlocale ();
    retval = g_intern_string (locale);
    g_free (locale);
    return retval;
  }
#endif

  return NULL;
}

typedef struct _GLanguageNamesCache GLanguageNamesCache;

struct _GLanguageNamesCache {
  gchar *languages;
  gchar **language_names;
};

static void
language_names_cache_free (gpointer data)
{
  GLanguageNamesCache *cache = data;
  g_free (cache->languages);
  g_strfreev (cache->language_names);
  g_free (cache);
}

/**
 * g_get_language_names:
 *
 * Computes a list of applicable locale names, which can be used to
 * e.g. construct locale-dependent filenames or search paths. The returned
 * list is sorted from most desirable to least desirable and always contains
 * the default locale "C".
 *
 * For example, if LANGUAGE=de:en_US, then the returned list is
 * "de", "en_US", "en", "C".
 *
 * This function consults the environment variables `LANGUAGE`, `LC_ALL`,
 * `LC_MESSAGES` and `LANG` to find the list of locales specified by the
 * user.
 *
 * Returns: (array zero-terminated=1) (transfer none): a %NULL-terminated array of strings owned by GLib
 *    that must not be modified or freed.
 *
 * Since: 2.6
 **/
const gchar * const *
g_get_language_names (void)
{
  static GPrivate cache_private = G_PRIVATE_INIT (language_names_cache_free);
  GLanguageNamesCache *cache = g_private_get (&cache_private);
  const gchar *value;

  if (!cache)
    {
      cache = g_new0 (GLanguageNamesCache, 1);
      g_private_set (&cache_private, cache);
    }

  value = guess_category_value ("LC_MESSAGES");
  if (!value)
    value = "C";

  if (!(cache->languages && strcmp (cache->languages, value) == 0))
    {
      GPtrArray *array;
      gchar **alist, **a;

      g_free (cache->languages);
      g_strfreev (cache->language_names);
      cache->languages = g_strdup (value);

      array = g_ptr_array_sized_new (8);

      alist = g_strsplit (value, ":", 0);
      for (a = alist; *a; a++)
        append_locale_variants (array, unalias_lang (*a));
      g_strfreev (alist);
      g_ptr_array_add (array, g_strdup ("C"));
      g_ptr_array_add (array, NULL);

      cache->language_names = (gchar **) g_ptr_array_free (array, FALSE);
    }

  return (const gchar * const *) cache->language_names;
}
