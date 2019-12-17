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

#include <string.h>
#include <glib.h>

/*
 * The string info map is an efficient data structure designed to be
 * used with a small set of items.  It is used by GSettings schemas for
 * three purposes:
 *
 *  1) Implement <choices> with a list of valid strings
 *
 *  2) Implement <alias> by mapping one string to another
 *
 *  3) Implement enumerated types by mapping strings to integer values
 *     (and back).
 *
 * The map is made out of an array of uint32s.  Each entry in the array
 * is an integer value, followed by a specially formatted string value:
 *
 *   The string starts with the byte 0xff or 0xfe, followed by the
 *   content of the string, followed by a nul byte, followed by
 *   additional nul bytes for padding, followed by a 0xff byte.
 *
 *   Padding is added so that the entire formatted string takes up a
 *   multiple of 4 bytes, and not less than 8 bytes.  The requirement
 *   for a string to take up 8 bytes is so that the scanner doesn't lose
 *   synch and mistake a string for an integer value.
 *
 * The first byte of the formatted string depends on if the integer is
 * an enum value (0xff) or an alias (0xfe).  If it is an alias then the
 * number refers to the word offset within the info map at which the
 * integer corresponding to the "target" value is stored.
 *
 * For example, consider the case of the string info map representing an
 * enumerated type of 'foo' (value 1) and 'bar' (value 2) and 'baz'
 * (alias for 'bar').  Note that string info maps are always little
 * endian.
 *
 * x01 x00 x00 x00   xff 'f' 'o' 'o'   x00 x00 x00 xff   x02 x00 x00 x00
 * xff 'b' 'a' 'r'   x00 x00 x00 xff   x03 x00 x00 x00   xfe 'b' 'a' 'z'
 * x00 x00 x00 xff
 *
 *
 * The operations that someone may want to perform with the map:
 *
 *   - lookup if a string is valid (and not an alias)
 *   - lookup the integer value for a enum 'nick'
 *   - lookup the integer value for the target of an alias
 *   - lookup an alias and convert it to its target string
 *   - lookup the enum nick for a given value
 *
 * In order to lookup if a string is valid, it is padded on either side
 * (as described) and scanned for in the array.  For example, you might
 * look for "foo":
 *
 *                   xff 'f' 'o' 'o'   x00 x00 x00 xff
 *
 * In order to lookup the integer value for a nick, the string is padded
 * on either side and scanned for in the array, as above.  Instead of
 * merely succeeding, we look at the integer value to the left of the
 * match.  This is the enum value.
 *
 * In order to lookup an alias and convert it to its target enum value,
 * the string is padded on either side (as described, with 0xfe) and
 * scanned for.  For example, you might look for "baz":
 *
 *                   xfe 'b' 'a' 'z'  x00 x00 x00 xff
 *
 * The integer immediately preceding the match then contains the offset
 * of the integer value of the target.  In our example, that's '3'.
 * This index is dereferenced to find the enum value of '2'.
 *
 * To convert the alias to its target string, 5 bytes just need to be
 * added past the start of the integer value to find the start of the
 * string.
 *
 * To lookup the enum nick for a given value, the value is searched for
 * in the array.  To ensure that the value isn't matching the inside of a
 * string, we must check that it is either the first item in the array or
 * immediately preceded by the byte 0xff.  It must also be immediately
 * followed by the byte 0xff.
 *
 * Because strings always take up a minimum of 2 words, because 0xff or
 * 0xfe never appear inside of a utf-8 string and because no two integer
 * values ever appear in sequence, the only way we can have the
 * sequence:
 *
 *     xff __ __ __ __ xff (or 0xfe)
 *
 * is in the event of an integer nested between two strings.
 *
 * For implementation simplicity/efficiency, strings may not be more
 * than 65 characters in length (ie: 17 32bit words after padding).
 *
 * In the event that we are doing <choices> (ie: not an enum type) then
 * the value of each choice is set to zero and ignored.
 */

#define STRINFO_MAX_WORDS   17
G_GNUC_UNUSED static guint
strinfo_string_to_words (const gchar *string,
                         guint32     *words,
                         gboolean     alias)
{
  guint n_words;
  gsize size;

  size = strlen (string);

  n_words = MAX (2, (size + 6) >> 2);

  if (n_words > STRINFO_MAX_WORDS)
    return FALSE;

  words[0] = GUINT32_TO_LE (alias ? 0xfe : 0xff);
  words[n_words - 1] = GUINT32_TO_BE (0xff);
  memcpy (((gchar *) words) + 1, string, size + 1);

  return n_words;
}

G_GNUC_UNUSED static gint
strinfo_scan (const guint32 *strinfo,
              guint          length,
              const guint32 *words,
              guint          n_words)
{
  guint i = 0;

  if (length < n_words)
    return -1;

  while (i <= length - n_words)
    {
      guint j = 0;

      for (j = 0; j < n_words; j++)
        if (strinfo[i + j] != words[j])
          break;

      if (j == n_words)
        return i;   /* match */

      /* skip at least one word, continue */
      i += j ? j : 1;
    }

  return -1;
}

G_GNUC_UNUSED static gint
strinfo_find_string (const guint32 *strinfo,
                     guint          length,
                     const gchar   *string,
                     gboolean       alias)
{
  guint32 words[STRINFO_MAX_WORDS];
  guint n_words;

  if (length == 0)
    return -1;

  n_words = strinfo_string_to_words (string, words, alias);

  return strinfo_scan (strinfo + 1, length - 1, words, n_words);
}

G_GNUC_UNUSED static gint
strinfo_find_integer (const guint32 *strinfo,
                      guint          length,
                      guint32        value)
{
  guint i;

  for (i = 0; i < length; i++)
    if (strinfo[i] == GUINT32_TO_LE (value))
      {
        const guchar *charinfo = (const guchar *) &strinfo[i];

        /* make sure it has 0xff on either side */
        if ((i == 0 || charinfo[-1] == 0xff) && charinfo[4] == 0xff)
          return i;
      }

  return -1;
}

G_GNUC_UNUSED static gboolean
strinfo_is_string_valid (const guint32 *strinfo,
                         guint          length,
                         const gchar   *string)
{
  return strinfo_find_string (strinfo, length, string, FALSE) != -1;
}

G_GNUC_UNUSED static gboolean
strinfo_enum_from_string (const guint32 *strinfo,
                          guint          length,
                          const gchar   *string,
                          guint         *result)
{
  gint index;

  index = strinfo_find_string (strinfo, length, string, FALSE);

  if (index < 0)
    return FALSE;

  *result = GUINT32_FROM_LE (strinfo[index]);
  return TRUE;
}

G_GNUC_UNUSED static const gchar *
strinfo_string_from_enum (const guint32 *strinfo,
                          guint          length,
                          guint          value)
{
  gint index;

  index = strinfo_find_integer (strinfo, length, value);

  if (index < 0)
    return NULL;

  return 1 + (const gchar *) &strinfo[index + 1];
}

G_GNUC_UNUSED static const gchar *
strinfo_string_from_alias (const guint32 *strinfo,
                           guint          length,
                           const gchar   *alias)
{
  gint index;

  index = strinfo_find_string (strinfo, length, alias, TRUE);

  if (index < 0)
    return NULL;

  return 1 + (const gchar *) &strinfo[GUINT32_TO_LE (strinfo[index]) + 1];
}

G_GNUC_UNUSED static GVariant *
strinfo_enumerate (const guint32 *strinfo,
                   guint          length)
{
  GVariantBuilder builder;
  const gchar *ptr, *end;

  ptr = (gpointer) strinfo;
  end = ptr + 4 * length;

  ptr += 4;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

  while (ptr < end)
    {
      /* don't include aliases */
      if (*ptr == '\xff')
        g_variant_builder_add (&builder, "s", ptr + 1);

      /* find the end of this string */
      ptr = memchr (ptr, '\xff', end - ptr);
      g_assert (ptr != NULL);

      /* skip over the int to the next string */
      ptr += 5;
    }

  return g_variant_builder_end (&builder);
}

G_GNUC_UNUSED static void
strinfo_builder_append_item (GString     *builder,
                             const gchar *string,
                             guint        value)
{
  guint32 words[STRINFO_MAX_WORDS];
  guint n_words;

  value = GUINT32_TO_LE (value);

  n_words = strinfo_string_to_words (string, words, FALSE);
  g_string_append_len (builder, (void *) &value, sizeof value);
  g_string_append_len (builder, (void *) words, 4 * n_words);
}

G_GNUC_UNUSED static gboolean
strinfo_builder_append_alias (GString     *builder,
                              const gchar *alias,
                              const gchar *target)
{
  guint32 words[STRINFO_MAX_WORDS];
  guint n_words;
  guint value;
  gint index;

  index = strinfo_find_string ((const guint32 *) builder->str,
                               builder->len / 4, target, FALSE);

  if (index == -1)
    return FALSE;

  value = GUINT32_TO_LE (index);

  n_words = strinfo_string_to_words (alias, words, TRUE);
  g_string_append_len (builder, (void *) &value, sizeof value);
  g_string_append_len (builder, (void *) words, 4 * n_words);

  return TRUE;
}

G_GNUC_UNUSED static gboolean
strinfo_builder_contains (GString     *builder,
                          const gchar *string)
{
  return strinfo_find_string ((const guint32 *) builder->str,
                              builder->len / 4, string, FALSE) != -1 ||
         strinfo_find_string ((const guint32 *) builder->str,
                              builder->len / 4, string, TRUE) != -1;
}

G_GNUC_UNUSED static gboolean
strinfo_builder_contains_value (GString *builder,
                                guint    value)
{
  return strinfo_string_from_enum ((const guint32 *) builder->str,
                                   builder->len / 4, value) != NULL;
}
