/*
 * Copyright © 2014 Canonical Limited
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

#include <config.h>

#include "gstrfuncs.h"

#include <glib.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

struct mapping_entry
{
  guint16 src;
  guint16 ascii;
};

struct mapping_range
{
  guint16 start;
  guint16 length;
};

struct locale_entry
{
  guint8 name_offset;
  guint8 item_id;
};

#include "gtranslit-data.h"

#define get_src_char(array, encoded, index) ((encoded & 0x8000) ? (array)[((encoded) & 0xfff) + index] : encoded)
#define get_length(encoded)                 ((encoded & 0x8000) ? ((encoded & 0x7000) >> 12) : 1)

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define get_ascii_item(array, encoded)      ((encoded & 0x8000) ? &(array)[(encoded) & 0xfff] : (gpointer) (((char *) &(encoded)) + 1))
#else
#define get_ascii_item(array, encoded)      ((encoded & 0x8000) ? &(array)[(encoded) & 0xfff] : (gpointer) &(encoded))
#endif

static const gchar * lookup_in_item (guint           item_id,
                                     const gunichar *key,
                                     gint           *result_len,
                                     gint           *key_consumed);

static gint
compare_mapping_entry (gconstpointer user_data,
                       gconstpointer data)
{
  const struct mapping_entry *entry = data;
  const gunichar *key = user_data;
  gunichar src_0;

  G_STATIC_ASSERT(MAX_KEY_SIZE == 2);

  src_0 = get_src_char (src_table, entry->src, 0);

  if (key[0] > src_0)
    return 1;
  else if (key[0] < src_0)
    return -1;

  if (get_length (entry->src) > 1)
    {
      gunichar src_1;

      src_1 = get_src_char (src_table, entry->src, 1);

      if (key[1] > src_1)
        return 1;
      else if (key[1] < src_1)
        return -1;
    }
  else if (key[1])
    return 1;

  return 0;
}

static const gchar *
lookup_in_mapping (const struct mapping_entry *mapping,
                   gint                        mapping_size,
                   const gunichar             *key,
                   gint                       *result_len,
                   gint                       *key_consumed)
{
  const struct mapping_entry *hit;

  hit = bsearch (key, mapping, mapping_size, sizeof (struct mapping_entry), compare_mapping_entry);

  if (hit == NULL)
    return NULL;

  *key_consumed = get_length (hit->src);
  *result_len = get_length (hit->ascii);

  return get_ascii_item(ascii_table, hit->ascii);
}

static const gchar *
lookup_in_chain (const guint8   *chain,
                 const gunichar *key,
                 gint           *result_len,
                 gint           *key_consumed)
{
  const gchar *result;

  while (*chain != 0xff)
    {
      result = lookup_in_item (*chain, key, result_len, key_consumed);

      if (result)
        return result;

      chain++;
    }

  return NULL;
}

static const gchar *
lookup_in_item (guint           item_id,
                const gunichar *key,
                gint           *result_len,
                gint           *key_consumed)
{
  if (item_id & 0x80)
    {
      const guint8 *chain = chains_table + chain_starts[item_id & 0x7f];

      return lookup_in_chain (chain, key, result_len, key_consumed);
    }
  else
    {
      const struct mapping_range *range = &mapping_ranges[item_id];

      return lookup_in_mapping (mappings_table + range->start, range->length, key, result_len, key_consumed);
    }
}

static gint
compare_locale_entry (gconstpointer user_data,
                      gconstpointer data)
{
  const struct locale_entry *entry = data;
  const gchar *key = user_data;

  return strcmp (key, &locale_names[entry->name_offset]);
}

static gboolean
lookup_item_id_for_one_locale (const gchar *key,
                               guint       *item_id)
{
  const struct locale_entry *hit;

  hit = bsearch (key, locale_index, G_N_ELEMENTS (locale_index), sizeof (struct locale_entry), compare_locale_entry);

  if (hit == NULL)
    return FALSE;

  *item_id = hit->item_id;
  return TRUE;
}

static guint
lookup_item_id_for_locale (const gchar *locale)
{
  gchar key[MAX_LOCALE_NAME + 1];
  const gchar *language;
  guint language_len;
  const gchar *territory = NULL;
  guint territory_len = 0;
  const gchar *modifier = NULL;
  guint modifier_len = 0;
  const gchar *next_char;
  guint id;

  /* As per POSIX, a valid locale looks like:
   *
   *   language[_territory][.codeset][@modifier]
   */
  language = locale;
  language_len = strcspn (language, "_.@");
  next_char = language + language_len;

  if (*next_char == '_')
    {
      territory = next_char;
      territory_len = strcspn (territory + 1, "_.@") + 1;
      next_char = territory + territory_len;
    }

  if (*next_char == '.')
    {
      const gchar *codeset;
      guint codeset_len;

      codeset = next_char;
      codeset_len = strcspn (codeset + 1, "_.@") + 1;
      next_char = codeset + codeset_len;
    }

  if (*next_char == '@')
    {
      modifier = next_char;
      modifier_len = strcspn (modifier + 1, "_.@") + 1;
      next_char = modifier + modifier_len;
    }

  /* What madness is this? */
  if (language_len == 0 || *next_char)
    return default_item_id;

  /* We are not interested in codeset.
   *
   * For this locale:
   *
   *  aa_BB@cc
   *
   * try in this order:
   *
   * Note: we have no locales of the form aa_BB@cc in the database.
   *
   *  1. aa@cc
   *  2. aa_BB
   *  3. aa
   */

  /* 1. */
  if (modifier_len && language_len + modifier_len <= MAX_LOCALE_NAME)
    {
      memcpy (key, language, language_len);
      memcpy (key + language_len, modifier, modifier_len);
      key[language_len + modifier_len] = '\0';

      if (lookup_item_id_for_one_locale (key, &id))
        return id;
    }

  /* 2. */
  if (territory_len && language_len + territory_len <= MAX_LOCALE_NAME)
    {
      memcpy (key, language, language_len);
      memcpy (key + language_len, territory, territory_len);
      key[language_len + territory_len] = '\0';

      if (lookup_item_id_for_one_locale (key, &id))
        return id;
    }

  /* 3. */
  if (language_len <= MAX_LOCALE_NAME)
    {
      memcpy (key, language, language_len);
      key[language_len] = '\0';

      if (lookup_item_id_for_one_locale (key, &id))
        return id;
    }

  return default_item_id;
}

static guint
get_default_item_id (void)
{
  static guint item_id;
  static gboolean done;

  /* Doesn't need to be locked -- no harm in doing it twice. */
  if (!done)
    {
      const gchar *locale;

      locale = setlocale (LC_CTYPE, NULL);
      item_id = lookup_item_id_for_locale (locale);
      done = TRUE;
    }

  return item_id;
}

/**
 * g_str_to_ascii:
 * @str: a string, in UTF-8
 * @from_locale: (nullable): the source locale, if known
 *
 * Transliterate @str to plain ASCII.
 *
 * For best results, @str should be in composed normalised form.
 *
 * This function performs a reasonably good set of character
 * replacements.  The particular set of replacements that is done may
 * change by version or even by runtime environment.
 *
 * If the source language of @str is known, it can used to improve the
 * accuracy of the translation by passing it as @from_locale.  It should
 * be a valid POSIX locale string (of the form
 * "language[_territory][.codeset][@modifier]").
 *
 * If @from_locale is %NULL then the current locale is used.
 *
 * If you want to do translation for no specific locale, and you want it
 * to be done independently of the currently locale, specify "C" for
 * @from_locale.
 *
 * Returns: a string in plain ASCII
 *
 * Since: 2.40
 **/
gchar *
g_str_to_ascii (const gchar *str,
                const gchar *from_locale)
{
  GString *result;
  guint item_id;

  g_return_val_if_fail (str != NULL, NULL);

  if (g_str_is_ascii (str))
    return g_strdup (str);

  if (from_locale)
    item_id = lookup_item_id_for_locale (from_locale);
  else
    item_id = get_default_item_id ();

  result = g_string_sized_new (strlen (str));

  while (*str)
    {
      /* We only need to transliterate non-ASCII values... */
      if (*str & 0x80)
        {
          gunichar key[MAX_KEY_SIZE];
          const gchar *r;
          gint consumed;
          gint r_len;
          gunichar c;

          G_STATIC_ASSERT(MAX_KEY_SIZE == 2);

          c = g_utf8_get_char (str);

          /* This is where it gets evil...
           *
           * We know that MAX_KEY_SIZE is 2.  We also know that we
           * only want to try another character if it's non-ascii.
           */
          str = g_utf8_next_char (str);

          key[0] = c;
          if (*str & 0x80)
            key[1] = g_utf8_get_char (str);
          else
            key[1] = 0;

          r = lookup_in_item (item_id, key, &r_len, &consumed);

          /* If we failed to map two characters, try again with one.
           *
           * gconv behaviour is a bit weird here -- it seems to
           * depend in the randomness of the binary search and the
           * size of the input buffer as to what result we get here.
           *
           * Doing it this way is more work, but should be
           * more-correct.
           */
          if (r == NULL && key[1])
            {
              key[1] = 0;
              r = lookup_in_item (item_id, key, &r_len, &consumed);
            }

          if (r != NULL)
            {
              g_string_append_len (result, r, r_len);
              if (consumed == 2)
                /* If it took both then skip again */
                str = g_utf8_next_char (str);
            }
          else /* no match found */
            g_string_append_c (result, '?');
        }
      else if (*str & 0x80) /* Out-of-range non-ASCII case */
        {
          g_string_append_c (result, '?');
          str = g_utf8_next_char (str);
        }
      else /* ASCII case */
        g_string_append_c (result, *str++);
    }

  return g_string_free (result, FALSE);
}
