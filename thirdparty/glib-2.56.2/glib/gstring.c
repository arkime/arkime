/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gstring.h"

#include "gprintf.h"


/**
 * SECTION:strings
 * @title: Strings
 * @short_description: text buffers which grow automatically
 *     as text is added
 *
 * A #GString is an object that handles the memory management of a C
 * string for you.  The emphasis of #GString is on text, typically
 * UTF-8.  Crucially, the "str" member of a #GString is guaranteed to
 * have a trailing nul character, and it is therefore always safe to
 * call functions such as strchr() or g_strdup() on it.
 *
 * However, a #GString can also hold arbitrary binary data, because it
 * has a "len" member, which includes any possible embedded nul
 * characters in the data.  Conceptually then, #GString is like a
 * #GByteArray with the addition of many convenience methods for text,
 * and a guaranteed nul terminator.
 */

/**
 * GString:
 * @str: points to the character data. It may move as text is added.
 *   The @str field is null-terminated and so
 *   can be used as an ordinary C string.
 * @len: contains the length of the string, not including the
 *   terminating nul byte.
 * @allocated_len: the number of bytes that can be stored in the
 *   string before it needs to be reallocated. May be larger than @len.
 *
 * The GString struct contains the public fields of a GString.
 */


#define MY_MAXSIZE ((gsize)-1)

static inline gsize
nearest_power (gsize base, gsize num)
{
  if (num > MY_MAXSIZE / 2)
    {
      return MY_MAXSIZE;
    }
  else
    {
      gsize n = base;

      while (n < num)
        n <<= 1;

      return n;
    }
}

static void
g_string_maybe_expand (GString *string,
                       gsize    len)
{
  if (string->len + len >= string->allocated_len)
    {
      string->allocated_len = nearest_power (1, string->len + len + 1);
      string->str = g_realloc (string->str, string->allocated_len);
    }
}

/**
 * g_string_sized_new:
 * @dfl_size: the default size of the space allocated to
 *     hold the string
 *
 * Creates a new #GString, with enough space for @dfl_size
 * bytes. This is useful if you are going to add a lot of
 * text to the string and don't want it to be reallocated
 * too often.
 *
 * Returns: the new #GString
 */
GString *
g_string_sized_new (gsize dfl_size)
{
  GString *string = g_slice_new (GString);

  string->allocated_len = 0;
  string->len   = 0;
  string->str   = NULL;

  g_string_maybe_expand (string, MAX (dfl_size, 2));
  string->str[0] = 0;

  return string;
}

/**
 * g_string_new:
 * @init: (nullable): the initial text to copy into the string, or %NULL to
 * start with an empty string
 *
 * Creates a new #GString, initialized with the given string.
 *
 * Returns: the new #GString
 */
GString *
g_string_new (const gchar *init)
{
  GString *string;

  if (init == NULL || *init == '\0')
    string = g_string_sized_new (2);
  else
    {
      gint len;

      len = strlen (init);
      string = g_string_sized_new (len + 2);

      g_string_append_len (string, init, len);
    }

  return string;
}

/**
 * g_string_new_len:
 * @init: initial contents of the string
 * @len: length of @init to use
 *
 * Creates a new #GString with @len bytes of the @init buffer.
 * Because a length is provided, @init need not be nul-terminated,
 * and can contain embedded nul bytes.
 *
 * Since this function does not stop at nul bytes, it is the caller's
 * responsibility to ensure that @init has at least @len addressable
 * bytes.
 *
 * Returns: a new #GString
 */
GString *
g_string_new_len (const gchar *init,
                  gssize       len)
{
  GString *string;

  if (len < 0)
    return g_string_new (init);
  else
    {
      string = g_string_sized_new (len);

      if (init)
        g_string_append_len (string, init, len);

      return string;
    }
}

/**
 * g_string_free:
 * @string: (transfer full): a #GString
 * @free_segment: if %TRUE, the actual character data is freed as well
 *
 * Frees the memory allocated for the #GString.
 * If @free_segment is %TRUE it also frees the character data.  If
 * it's %FALSE, the caller gains ownership of the buffer and must
 * free it after use with g_free().
 *
 * Returns: (nullable): the character data of @string
 *          (i.e. %NULL if @free_segment is %TRUE)
 */
gchar *
g_string_free (GString  *string,
               gboolean  free_segment)
{
  gchar *segment;

  g_return_val_if_fail (string != NULL, NULL);

  if (free_segment)
    {
      g_free (string->str);
      segment = NULL;
    }
  else
    segment = string->str;

  g_slice_free (GString, string);

  return segment;
}

/**
 * g_string_free_to_bytes:
 * @string: (transfer full): a #GString
 *
 * Transfers ownership of the contents of @string to a newly allocated
 * #GBytes.  The #GString structure itself is deallocated, and it is
 * therefore invalid to use @string after invoking this function.
 *
 * Note that while #GString ensures that its buffer always has a
 * trailing nul character (not reflected in its "len"), the returned
 * #GBytes does not include this extra nul; i.e. it has length exactly
 * equal to the "len" member.
 *
 * Returns: (transfer full): A newly allocated #GBytes containing contents of @string; @string itself is freed
 * Since: 2.34
 */
GBytes*
g_string_free_to_bytes (GString *string)
{
  gsize len;
  gchar *buf;

  g_return_val_if_fail (string != NULL, NULL);

  len = string->len;

  buf = g_string_free (string, FALSE);

  return g_bytes_new_take (buf, len);
}

/**
 * g_string_equal:
 * @v: a #GString
 * @v2: another #GString
 *
 * Compares two strings for equality, returning %TRUE if they are equal.
 * For use with #GHashTable.
 *
 * Returns: %TRUE if the strings are the same length and contain the
 *     same bytes
 */
gboolean
g_string_equal (const GString *v,
                const GString *v2)
{
  gchar *p, *q;
  GString *string1 = (GString *) v;
  GString *string2 = (GString *) v2;
  gsize i = string1->len;

  if (i != string2->len)
    return FALSE;

  p = string1->str;
  q = string2->str;
  while (i)
    {
      if (*p != *q)
        return FALSE;
      p++;
      q++;
      i--;
    }
  return TRUE;
}

/**
 * g_string_hash:
 * @str: a string to hash
 *
 * Creates a hash code for @str; for use with #GHashTable.
 *
 * Returns: hash code for @str
 */
guint
g_string_hash (const GString *str)
{
  const gchar *p = str->str;
  gsize n = str->len;
  guint h = 0;

  /* 31 bit hash function */
  while (n--)
    {
      h = (h << 5) - h + *p;
      p++;
    }

  return h;
}

/**
 * g_string_assign:
 * @string: the destination #GString. Its current contents
 *          are destroyed.
 * @rval: the string to copy into @string
 *
 * Copies the bytes from a string into a #GString,
 * destroying any previous contents. It is rather like
 * the standard strcpy() function, except that you do not
 * have to worry about having enough space to copy the string.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_assign (GString     *string,
                 const gchar *rval)
{
  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (rval != NULL, string);

  /* Make sure assigning to itself doesn't corrupt the string. */
  if (string->str != rval)
    {
      /* Assigning from substring should be ok, since
       * g_string_truncate() does not reallocate.
       */
      g_string_truncate (string, 0);
      g_string_append (string, rval);
    }

  return string;
}

/**
 * g_string_truncate:
 * @string: a #GString
 * @len: the new size of @string
 *
 * Cuts off the end of the GString, leaving the first @len bytes.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_truncate (GString *string,
                   gsize    len)
{
  g_return_val_if_fail (string != NULL, NULL);

  string->len = MIN (len, string->len);
  string->str[string->len] = 0;

  return string;
}

/**
 * g_string_set_size:
 * @string: a #GString
 * @len: the new length
 *
 * Sets the length of a #GString. If the length is less than
 * the current length, the string will be truncated. If the
 * length is greater than the current length, the contents
 * of the newly added area are undefined. (However, as
 * always, string->str[string->len] will be a nul byte.)
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_set_size (GString *string,
                   gsize    len)
{
  g_return_val_if_fail (string != NULL, NULL);

  if (len >= string->allocated_len)
    g_string_maybe_expand (string, len - string->len);

  string->len = len;
  string->str[len] = 0;

  return string;
}

/**
 * g_string_insert_len:
 * @string: a #GString
 * @pos: position in @string where insertion should
 *       happen, or -1 for at the end
 * @val: bytes to insert
 * @len: number of bytes of @val to insert
 *
 * Inserts @len bytes of @val into @string at @pos.
 * Because @len is provided, @val may contain embedded
 * nuls and need not be nul-terminated. If @pos is -1,
 * bytes are inserted at the end of the string.
 *
 * Since this function does not stop at nul bytes, it is
 * the caller's responsibility to ensure that @val has at
 * least @len addressable bytes.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_insert_len (GString     *string,
                     gssize       pos,
                     const gchar *val,
                     gssize       len)
{
  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (len == 0 || val != NULL, string);

  if (len == 0)
    return string;

  if (len < 0)
    len = strlen (val);

  if (pos < 0)
    pos = string->len;
  else
    g_return_val_if_fail (pos <= string->len, string);

  /* Check whether val represents a substring of string.
   * This test probably violates chapter and verse of the C standards,
   * since ">=" and "<=" are only valid when val really is a substring.
   * In practice, it will work on modern archs.
   */
  if (G_UNLIKELY (val >= string->str && val <= string->str + string->len))
    {
      gsize offset = val - string->str;
      gsize precount = 0;

      g_string_maybe_expand (string, len);
      val = string->str + offset;
      /* At this point, val is valid again.  */

      /* Open up space where we are going to insert.  */
      if (pos < string->len)
        memmove (string->str + pos + len, string->str + pos, string->len - pos);

      /* Move the source part before the gap, if any.  */
      if (offset < pos)
        {
          precount = MIN (len, pos - offset);
          memcpy (string->str + pos, val, precount);
        }

      /* Move the source part after the gap, if any.  */
      if (len > precount)
        memcpy (string->str + pos + precount,
                val + /* Already moved: */ precount + /* Space opened up: */ len,
                len - precount);
    }
  else
    {
      g_string_maybe_expand (string, len);

      /* If we aren't appending at the end, move a hunk
       * of the old string to the end, opening up space
       */
      if (pos < string->len)
        memmove (string->str + pos + len, string->str + pos, string->len - pos);

      /* insert the new string */
      if (len == 1)
        string->str[pos] = *val;
      else
        memcpy (string->str + pos, val, len);
    }

  string->len += len;

  string->str[string->len] = 0;

  return string;
}

#define SUB_DELIM_CHARS  "!$&'()*+,;="

static gboolean
is_valid (char        c,
          const char *reserved_chars_allowed)
{
  if (g_ascii_isalnum (c) ||
      c == '-' ||
      c == '.' ||
      c == '_' ||
      c == '~')
    return TRUE;

  if (reserved_chars_allowed &&
      strchr (reserved_chars_allowed, c) != NULL)
    return TRUE;

  return FALSE;
}

static gboolean
gunichar_ok (gunichar c)
{
  return
    (c != (gunichar) -2) &&
    (c != (gunichar) -1);
}

/**
 * g_string_append_uri_escaped:
 * @string: a #GString
 * @unescaped: a string
 * @reserved_chars_allowed: a string of reserved characters allowed
 *     to be used, or %NULL
 * @allow_utf8: set %TRUE if the escaped string may include UTF8 characters
 *
 * Appends @unescaped to @string, escaped any characters that
 * are reserved in URIs using URI-style escape sequences.
 *
 * Returns: (transfer none): @string
 *
 * Since: 2.16
 */
GString *
g_string_append_uri_escaped (GString     *string,
                             const gchar *unescaped,
                             const gchar *reserved_chars_allowed,
                             gboolean     allow_utf8)
{
  unsigned char c;
  const gchar *end;
  static const gchar hex[16] = "0123456789ABCDEF";

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (unescaped != NULL, NULL);

  end = unescaped + strlen (unescaped);

  while ((c = *unescaped) != 0)
    {
      if (c >= 0x80 && allow_utf8 &&
          gunichar_ok (g_utf8_get_char_validated (unescaped, end - unescaped)))
        {
          int len = g_utf8_skip [c];
          g_string_append_len (string, unescaped, len);
          unescaped += len;
        }
      else if (is_valid (c, reserved_chars_allowed))
        {
          g_string_append_c (string, c);
          unescaped++;
        }
      else
        {
          g_string_append_c (string, '%');
          g_string_append_c (string, hex[((guchar)c) >> 4]);
          g_string_append_c (string, hex[((guchar)c) & 0xf]);
          unescaped++;
        }
    }

  return string;
}

/**
 * g_string_append:
 * @string: a #GString
 * @val: the string to append onto the end of @string
 *
 * Adds a string onto the end of a #GString, expanding
 * it if necessary.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_append (GString     *string,
                 const gchar *val)
{
  return g_string_insert_len (string, -1, val, -1);
}

/**
 * g_string_append_len:
 * @string: a #GString
 * @val: bytes to append
 * @len: number of bytes of @val to use
 *
 * Appends @len bytes of @val to @string. Because @len is
 * provided, @val may contain embedded nuls and need not
 * be nul-terminated.
 *
 * Since this function does not stop at nul bytes, it is
 * the caller's responsibility to ensure that @val has at
 * least @len addressable bytes.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_append_len (GString     *string,
                     const gchar *val,
                     gssize       len)
{
  return g_string_insert_len (string, -1, val, len);
}

/**
 * g_string_append_c:
 * @string: a #GString
 * @c: the byte to append onto the end of @string
 *
 * Adds a byte onto the end of a #GString, expanding
 * it if necessary.
 *
 * Returns: (transfer none): @string
 */
#undef g_string_append_c
GString *
g_string_append_c (GString *string,
                   gchar    c)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_string_insert_c (string, -1, c);
}

/**
 * g_string_append_unichar:
 * @string: a #GString
 * @wc: a Unicode character
 *
 * Converts a Unicode character into UTF-8, and appends it
 * to the string.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_append_unichar (GString  *string,
                         gunichar  wc)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_string_insert_unichar (string, -1, wc);
}

/**
 * g_string_prepend:
 * @string: a #GString
 * @val: the string to prepend on the start of @string
 *
 * Adds a string on to the start of a #GString,
 * expanding it if necessary.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_prepend (GString     *string,
                  const gchar *val)
{
  return g_string_insert_len (string, 0, val, -1);
}

/**
 * g_string_prepend_len:
 * @string: a #GString
 * @val: bytes to prepend
 * @len: number of bytes in @val to prepend
 *
 * Prepends @len bytes of @val to @string.
 * Because @len is provided, @val may contain
 * embedded nuls and need not be nul-terminated.
 *
 * Since this function does not stop at nul bytes,
 * it is the caller's responsibility to ensure that
 * @val has at least @len addressable bytes.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_prepend_len (GString     *string,
                      const gchar *val,
                      gssize       len)
{
  return g_string_insert_len (string, 0, val, len);
}

/**
 * g_string_prepend_c:
 * @string: a #GString
 * @c: the byte to prepend on the start of the #GString
 *
 * Adds a byte onto the start of a #GString,
 * expanding it if necessary.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_prepend_c (GString *string,
                    gchar    c)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_string_insert_c (string, 0, c);
}

/**
 * g_string_prepend_unichar:
 * @string: a #GString
 * @wc: a Unicode character
 *
 * Converts a Unicode character into UTF-8, and prepends it
 * to the string.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_prepend_unichar (GString  *string,
                          gunichar  wc)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_string_insert_unichar (string, 0, wc);
}

/**
 * g_string_insert:
 * @string: a #GString
 * @pos: the position to insert the copy of the string
 * @val: the string to insert
 *
 * Inserts a copy of a string into a #GString,
 * expanding it if necessary.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_insert (GString     *string,
                 gssize       pos,
                 const gchar *val)
{
  return g_string_insert_len (string, pos, val, -1);
}

/**
 * g_string_insert_c:
 * @string: a #GString
 * @pos: the position to insert the byte
 * @c: the byte to insert
 *
 * Inserts a byte into a #GString, expanding it if necessary.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_insert_c (GString *string,
                   gssize   pos,
                   gchar    c)
{
  g_return_val_if_fail (string != NULL, NULL);

  g_string_maybe_expand (string, 1);

  if (pos < 0)
    pos = string->len;
  else
    g_return_val_if_fail (pos <= string->len, string);

  /* If not just an append, move the old stuff */
  if (pos < string->len)
    memmove (string->str + pos + 1, string->str + pos, string->len - pos);

  string->str[pos] = c;

  string->len += 1;

  string->str[string->len] = 0;

  return string;
}

/**
 * g_string_insert_unichar:
 * @string: a #GString
 * @pos: the position at which to insert character, or -1
 *     to append at the end of the string
 * @wc: a Unicode character
 *
 * Converts a Unicode character into UTF-8, and insert it
 * into the string at the given position.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_insert_unichar (GString  *string,
                         gssize    pos,
                         gunichar  wc)
{
  gint charlen, first, i;
  gchar *dest;

  g_return_val_if_fail (string != NULL, NULL);

  /* Code copied from g_unichar_to_utf() */
  if (wc < 0x80)
    {
      first = 0;
      charlen = 1;
    }
  else if (wc < 0x800)
    {
      first = 0xc0;
      charlen = 2;
    }
  else if (wc < 0x10000)
    {
      first = 0xe0;
      charlen = 3;
    }
   else if (wc < 0x200000)
    {
      first = 0xf0;
      charlen = 4;
    }
  else if (wc < 0x4000000)
    {
      first = 0xf8;
      charlen = 5;
    }
  else
    {
      first = 0xfc;
      charlen = 6;
    }
  /* End of copied code */

  g_string_maybe_expand (string, charlen);

  if (pos < 0)
    pos = string->len;
  else
    g_return_val_if_fail (pos <= string->len, string);

  /* If not just an append, move the old stuff */
  if (pos < string->len)
    memmove (string->str + pos + charlen, string->str + pos, string->len - pos);

  dest = string->str + pos;
  /* Code copied from g_unichar_to_utf() */
  for (i = charlen - 1; i > 0; --i)
    {
      dest[i] = (wc & 0x3f) | 0x80;
      wc >>= 6;
    }
  dest[0] = wc | first;
  /* End of copied code */

  string->len += charlen;

  string->str[string->len] = 0;

  return string;
}

/**
 * g_string_overwrite:
 * @string: a #GString
 * @pos: the position at which to start overwriting
 * @val: the string that will overwrite the @string starting at @pos
 *
 * Overwrites part of a string, lengthening it if necessary.
 *
 * Returns: (transfer none): @string
 *
 * Since: 2.14
 */
GString *
g_string_overwrite (GString     *string,
                    gsize        pos,
                    const gchar *val)
{
  g_return_val_if_fail (val != NULL, string);
  return g_string_overwrite_len (string, pos, val, strlen (val));
}

/**
 * g_string_overwrite_len:
 * @string: a #GString
 * @pos: the position at which to start overwriting
 * @val: the string that will overwrite the @string starting at @pos
 * @len: the number of bytes to write from @val
 *
 * Overwrites part of a string, lengthening it if necessary.
 * This function will work with embedded nuls.
 *
 * Returns: (transfer none): @string
 *
 * Since: 2.14
 */
GString *
g_string_overwrite_len (GString     *string,
                        gsize        pos,
                        const gchar *val,
                        gssize       len)
{
  gsize end;

  g_return_val_if_fail (string != NULL, NULL);

  if (!len)
    return string;

  g_return_val_if_fail (val != NULL, string);
  g_return_val_if_fail (pos <= string->len, string);

  if (len < 0)
    len = strlen (val);

  end = pos + len;

  if (end > string->len)
    g_string_maybe_expand (string, end - string->len);

  memcpy (string->str + pos, val, len);

  if (end > string->len)
    {
      string->str[end] = '\0';
      string->len = end;
    }

  return string;
}

/**
 * g_string_erase:
 * @string: a #GString
 * @pos: the position of the content to remove
 * @len: the number of bytes to remove, or -1 to remove all
 *       following bytes
 *
 * Removes @len bytes from a #GString, starting at position @pos.
 * The rest of the #GString is shifted down to fill the gap.
 *
 * Returns: (transfer none): @string
 */
GString *
g_string_erase (GString *string,
                gssize   pos,
                gssize   len)
{
  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (pos >= 0, string);
  g_return_val_if_fail (pos <= string->len, string);

  if (len < 0)
    len = string->len - pos;
  else
    {
      g_return_val_if_fail (pos + len <= string->len, string);

      if (pos + len < string->len)
        memmove (string->str + pos, string->str + pos + len, string->len - (pos + len));
    }

  string->len -= len;

  string->str[string->len] = 0;

  return string;
}

/**
 * g_string_ascii_down:
 * @string: a GString
 *
 * Converts all uppercase ASCII letters to lowercase ASCII letters.
 *
 * Returns: (transfer none): passed-in @string pointer, with all the
 *     uppercase characters converted to lowercase in place,
 *     with semantics that exactly match g_ascii_tolower().
 */
GString *
g_string_ascii_down (GString *string)
{
  gchar *s;
  gint n;

  g_return_val_if_fail (string != NULL, NULL);

  n = string->len;
  s = string->str;

  while (n)
    {
      *s = g_ascii_tolower (*s);
      s++;
      n--;
    }

  return string;
}

/**
 * g_string_ascii_up:
 * @string: a GString
 *
 * Converts all lowercase ASCII letters to uppercase ASCII letters.
 *
 * Returns: (transfer none): passed-in @string pointer, with all the
 *     lowercase characters converted to uppercase in place,
 *     with semantics that exactly match g_ascii_toupper().
 */
GString *
g_string_ascii_up (GString *string)
{
  gchar *s;
  gint n;

  g_return_val_if_fail (string != NULL, NULL);

  n = string->len;
  s = string->str;

  while (n)
    {
      *s = g_ascii_toupper (*s);
      s++;
      n--;
    }

  return string;
}

/**
 * g_string_down:
 * @string: a #GString
 *
 * Converts a #GString to lowercase.
 *
 * Returns: (transfer none): the #GString
 *
 * Deprecated:2.2: This function uses the locale-specific
 *     tolower() function, which is almost never the right thing.
 *     Use g_string_ascii_down() or g_utf8_strdown() instead.
 */
GString *
g_string_down (GString *string)
{
  guchar *s;
  glong n;

  g_return_val_if_fail (string != NULL, NULL);

  n = string->len;
  s = (guchar *) string->str;

  while (n)
    {
      if (isupper (*s))
        *s = tolower (*s);
      s++;
      n--;
    }

  return string;
}

/**
 * g_string_up:
 * @string: a #GString
 *
 * Converts a #GString to uppercase.
 *
 * Returns: (transfer none): @string
 *
 * Deprecated:2.2: This function uses the locale-specific
 *     toupper() function, which is almost never the right thing.
 *     Use g_string_ascii_up() or g_utf8_strup() instead.
 */
GString *
g_string_up (GString *string)
{
  guchar *s;
  glong n;

  g_return_val_if_fail (string != NULL, NULL);

  n = string->len;
  s = (guchar *) string->str;

  while (n)
    {
      if (islower (*s))
        *s = toupper (*s);
      s++;
      n--;
    }

  return string;
}

/**
 * g_string_append_vprintf:
 * @string: a #GString
 * @format: the string format. See the printf() documentation
 * @args: the list of arguments to insert in the output
 *
 * Appends a formatted string onto the end of a #GString.
 * This function is similar to g_string_append_printf()
 * except that the arguments to the format string are passed
 * as a va_list.
 *
 * Since: 2.14
 */
void
g_string_append_vprintf (GString     *string,
                         const gchar *format,
                         va_list      args)
{
  gchar *buf;
  gint len;

  g_return_if_fail (string != NULL);
  g_return_if_fail (format != NULL);

  len = g_vasprintf (&buf, format, args);

  if (len >= 0)
    {
      g_string_maybe_expand (string, len);
      memcpy (string->str + string->len, buf, len + 1);
      string->len += len;
      g_free (buf);
    }
}

/**
 * g_string_vprintf:
 * @string: a #GString
 * @format: the string format. See the printf() documentation
 * @args: the parameters to insert into the format string
 *
 * Writes a formatted string into a #GString.
 * This function is similar to g_string_printf() except that
 * the arguments to the format string are passed as a va_list.
 *
 * Since: 2.14
 */
void
g_string_vprintf (GString     *string,
                  const gchar *format,
                  va_list      args)
{
  g_string_truncate (string, 0);
  g_string_append_vprintf (string, format, args);
}

/**
 * g_string_sprintf:
 * @string: a #GString
 * @format: the string format. See the sprintf() documentation
 * @...: the parameters to insert into the format string
 *
 * Writes a formatted string into a #GString.
 * This is similar to the standard sprintf() function,
 * except that the #GString buffer automatically expands
 * to contain the results. The previous contents of the
 * #GString are destroyed.
 *
 * Deprecated: This function has been renamed to g_string_printf().
 */

/**
 * g_string_printf:
 * @string: a #GString
 * @format: the string format. See the printf() documentation
 * @...: the parameters to insert into the format string
 *
 * Writes a formatted string into a #GString.
 * This is similar to the standard sprintf() function,
 * except that the #GString buffer automatically expands
 * to contain the results. The previous contents of the
 * #GString are destroyed.
 */
void
g_string_printf (GString     *string,
                 const gchar *format,
                 ...)
{
  va_list args;

  g_string_truncate (string, 0);

  va_start (args, format);
  g_string_append_vprintf (string, format, args);
  va_end (args);
}

/**
 * g_string_sprintfa:
 * @string: a #GString
 * @format: the string format. See the sprintf() documentation
 * @...: the parameters to insert into the format string
 *
 * Appends a formatted string onto the end of a #GString.
 * This function is similar to g_string_sprintf() except that
 * the text is appended to the #GString.
 *
 * Deprecated: This function has been renamed to g_string_append_printf()
 */

/**
 * g_string_append_printf:
 * @string: a #GString
 * @format: the string format. See the printf() documentation
 * @...: the parameters to insert into the format string
 *
 * Appends a formatted string onto the end of a #GString.
 * This function is similar to g_string_printf() except
 * that the text is appended to the #GString.
 */
void
g_string_append_printf (GString     *string,
                        const gchar *format,
                        ...)
{
  va_list args;

  va_start (args, format);
  g_string_append_vprintf (string, format, args);
  va_end (args);
}
