/* decomp.c - Character decomposition.
 *
 *  Copyright (C) 1999, 2000 Tom Tromey
 *  Copyright 2000 Red Hat, Inc.
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
 * SECTION:unicode
 * @Title: Unicode Manipulation
 * @Short_description: functions operating on Unicode characters and
 *     UTF-8 strings
 * @See_also: g_locale_to_utf8(), g_locale_from_utf8()
 *
 * This section describes a number of functions for dealing with
 * Unicode characters and strings. There are analogues of the
 * traditional `ctype.h` character classification and case conversion
 * functions, UTF-8 analogues of some string utility functions,
 * functions to perform normalization, case conversion and collation
 * on UTF-8 strings and finally functions to convert between the UTF-8,
 * UTF-16 and UCS-4 encodings of Unicode.
 *
 * The implementations of the Unicode functions in GLib are based
 * on the Unicode Character Data tables, which are available from
 * [www.unicode.org](http://www.unicode.org/).
 * GLib 2.8 supports Unicode 4.0, GLib 2.10 supports Unicode 4.1,
 * GLib 2.12 supports Unicode 5.0, GLib 2.16.3 supports Unicode 5.1,
 * GLib 2.30 supports Unicode 6.0.
 */

#include "config.h"

#include <stdlib.h>

#include "gunicode.h"
#include "gunidecomp.h"
#include "gmem.h"
#include "gunicomp.h"
#include "gunicodeprivate.h"


#define CC_PART1(Page, Char) \
  ((combining_class_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (combining_class_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (cclass_data[combining_class_table_part1[Page]][Char]))

#define CC_PART2(Page, Char) \
  ((combining_class_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (combining_class_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (cclass_data[combining_class_table_part2[Page]][Char]))

#define COMBINING_CLASS(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? CC_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? CC_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : 0))

/**
 * g_unichar_combining_class:
 * @uc: a Unicode character
 * 
 * Determines the canonical combining class of a Unicode character.
 * 
 * Returns: the combining class of the character
 *
 * Since: 2.14
 **/
gint
g_unichar_combining_class (gunichar uc)
{
  return COMBINING_CLASS (uc);
}

/* constants for hangul syllable [de]composition */
#define SBase 0xAC00 
#define LBase 0x1100 
#define VBase 0x1161 
#define TBase 0x11A7
#define LCount 19 
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)

/**
 * g_unicode_canonical_ordering:
 * @string: a UCS-4 encoded string.
 * @len: the maximum length of @string to use.
 *
 * Computes the canonical ordering of a string in-place.  
 * This rearranges decomposed characters in the string 
 * according to their combining classes.  See the Unicode 
 * manual for more information. 
 **/
void
g_unicode_canonical_ordering (gunichar *string,
			      gsize     len)
{
  gsize i;
  int swap = 1;

  while (swap)
    {
      int last;
      swap = 0;
      last = COMBINING_CLASS (string[0]);
      for (i = 0; i < len - 1; ++i)
	{
	  int next = COMBINING_CLASS (string[i + 1]);
	  if (next != 0 && last > next)
	    {
	      gsize j;
	      /* Percolate item leftward through string.  */
	      for (j = i + 1; j > 0; --j)
		{
		  gunichar t;
		  if (COMBINING_CLASS (string[j - 1]) <= next)
		    break;
		  t = string[j];
		  string[j] = string[j - 1];
		  string[j - 1] = t;
		  swap = 1;
		}
	      /* We're re-entering the loop looking at the old
		 character again.  */
	      next = last;
	    }
	  last = next;
	}
    }
}

/* http://www.unicode.org/unicode/reports/tr15/#Hangul
 * r should be null or have sufficient space. Calling with r == NULL will
 * only calculate the result_len; however, a buffer with space for three
 * characters will always be big enough. */
static void
decompose_hangul (gunichar s,
                  gunichar *r,
                  gsize *result_len)
{
  gint SIndex = s - SBase;
  gint TIndex = SIndex % TCount;

  if (r)
    {
      r[0] = LBase + SIndex / NCount;
      r[1] = VBase + (SIndex % NCount) / TCount;
    }

  if (TIndex)
    {
      if (r)
	r[2] = TBase + TIndex;
      *result_len = 3;
    }
  else
    *result_len = 2;
}

/* returns a pointer to a null-terminated UTF-8 string */
static const gchar *
find_decomposition (gunichar ch,
		    gboolean compat)
{
  int start = 0;
  int end = G_N_ELEMENTS (decomp_table);
  
  if (ch >= decomp_table[start].ch &&
      ch <= decomp_table[end - 1].ch)
    {
      while (TRUE)
	{
	  int half = (start + end) / 2;
	  if (ch == decomp_table[half].ch)
	    {
	      int offset;

	      if (compat)
		{
		  offset = decomp_table[half].compat_offset;
		  if (offset == G_UNICODE_NOT_PRESENT_OFFSET)
		    offset = decomp_table[half].canon_offset;
		}
	      else
		{
		  offset = decomp_table[half].canon_offset;
		  if (offset == G_UNICODE_NOT_PRESENT_OFFSET)
		    return NULL;
		}
	      
	      return &(decomp_expansion_string[offset]);
	    }
	  else if (half == start)
	    break;
	  else if (ch > decomp_table[half].ch)
	    start = half;
	  else
	    end = half;
	}
    }

  return NULL;
}

/**
 * g_unicode_canonical_decomposition:
 * @ch: a Unicode character.
 * @result_len: location to store the length of the return value.
 *
 * Computes the canonical decomposition of a Unicode character.  
 * 
 * Returns: a newly allocated string of Unicode characters.
 *   @result_len is set to the resulting length of the string.
 *
 * Deprecated: 2.30: Use the more flexible g_unichar_fully_decompose()
 *   instead.
 **/
gunichar *
g_unicode_canonical_decomposition (gunichar ch,
				   gsize   *result_len)
{
  const gchar *decomp;
  const gchar *p;
  gunichar *r;

  /* Hangul syllable */
  if (ch >= SBase && ch < SBase + SCount)
    {
      decompose_hangul (ch, NULL, result_len);
      r = g_malloc (*result_len * sizeof (gunichar));
      decompose_hangul (ch, r, result_len);
    }
  else if ((decomp = find_decomposition (ch, FALSE)) != NULL)
    {
      /* Found it.  */
      int i;
      
      *result_len = g_utf8_strlen (decomp, -1);
      r = g_malloc (*result_len * sizeof (gunichar));
      
      for (p = decomp, i = 0; *p != '\0'; p = g_utf8_next_char (p), i++)
        r[i] = g_utf8_get_char (p);
    }
  else
    {
      /* Not in our table.  */
      r = g_malloc (sizeof (gunichar));
      *r = ch;
      *result_len = 1;
    }

  return r;
}

/* L,V => LV and LV,T => LVT  */
static gboolean
combine_hangul (gunichar a,
                gunichar b,
                gunichar *result)
{
  gint LIndex = a - LBase;
  gint SIndex = a - SBase;

  gint VIndex = b - VBase;
  gint TIndex = b - TBase;

  if (0 <= LIndex && LIndex < LCount
      && 0 <= VIndex && VIndex < VCount)
    {
      *result = SBase + (LIndex * VCount + VIndex) * TCount;
      return TRUE;
    }
  else if (0 <= SIndex && SIndex < SCount && (SIndex % TCount) == 0
           && 0 < TIndex && TIndex < TCount)
    {
      *result = a + TIndex;
      return TRUE;
    }

  return FALSE;
}

#define CI(Page, Char) \
  ((compose_table[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (compose_table[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (compose_data[compose_table[Page]][Char]))

#define COMPOSE_INDEX(Char) \
     (((Char >> 8) > (COMPOSE_TABLE_LAST)) ? 0 : CI((Char) >> 8, (Char) & 0xff))

static gboolean
combine (gunichar  a,
	 gunichar  b,
	 gunichar *result)
{
  gushort index_a, index_b;

  if (combine_hangul (a, b, result))
    return TRUE;

  index_a = COMPOSE_INDEX(a);

  if (index_a >= COMPOSE_FIRST_SINGLE_START && index_a < COMPOSE_SECOND_START)
    {
      if (b == compose_first_single[index_a - COMPOSE_FIRST_SINGLE_START][0])
	{
	  *result = compose_first_single[index_a - COMPOSE_FIRST_SINGLE_START][1];
	  return TRUE;
	}
      else
        return FALSE;
    }
  
  index_b = COMPOSE_INDEX(b);

  if (index_b >= COMPOSE_SECOND_SINGLE_START)
    {
      if (a == compose_second_single[index_b - COMPOSE_SECOND_SINGLE_START][0])
	{
	  *result = compose_second_single[index_b - COMPOSE_SECOND_SINGLE_START][1];
	  return TRUE;
	}
      else
        return FALSE;
    }

  if (index_a >= COMPOSE_FIRST_START && index_a < COMPOSE_FIRST_SINGLE_START &&
      index_b >= COMPOSE_SECOND_START && index_b < COMPOSE_SECOND_SINGLE_START)
    {
      gunichar res = compose_array[index_a - COMPOSE_FIRST_START][index_b - COMPOSE_SECOND_START];

      if (res)
	{
	  *result = res;
	  return TRUE;
	}
    }

  return FALSE;
}

gunichar *
_g_utf8_normalize_wc (const gchar    *str,
		      gssize          max_len,
		      GNormalizeMode  mode)
{
  gsize n_wc;
  gunichar *wc_buffer;
  const char *p;
  gsize last_start;
  gboolean do_compat = (mode == G_NORMALIZE_NFKC ||
			mode == G_NORMALIZE_NFKD);
  gboolean do_compose = (mode == G_NORMALIZE_NFC ||
			 mode == G_NORMALIZE_NFKC);

  n_wc = 0;
  p = str;
  while ((max_len < 0 || p < str + max_len) && *p)
    {
      const gchar *decomp;
      gunichar wc = g_utf8_get_char (p);

      if (wc >= SBase && wc < SBase + SCount)
        {
          gsize result_len;
          decompose_hangul (wc, NULL, &result_len);
          n_wc += result_len;
        }
      else 
        {
          decomp = find_decomposition (wc, do_compat);

          if (decomp)
            n_wc += g_utf8_strlen (decomp, -1);
          else
            n_wc++;
        }

      p = g_utf8_next_char (p);
    }

  wc_buffer = g_new (gunichar, n_wc + 1);

  last_start = 0;
  n_wc = 0;
  p = str;
  while ((max_len < 0 || p < str + max_len) && *p)
    {
      gunichar wc = g_utf8_get_char (p);
      const gchar *decomp;
      int cc;
      gsize old_n_wc = n_wc;
	  
      if (wc >= SBase && wc < SBase + SCount)
        {
          gsize result_len;
          decompose_hangul (wc, wc_buffer + n_wc, &result_len);
          n_wc += result_len;
        }
      else
        {
          decomp = find_decomposition (wc, do_compat);
          
          if (decomp)
            {
              const char *pd;
              for (pd = decomp; *pd != '\0'; pd = g_utf8_next_char (pd))
                wc_buffer[n_wc++] = g_utf8_get_char (pd);
            }
          else
            wc_buffer[n_wc++] = wc;
        }

      if (n_wc > 0)
	{
	  cc = COMBINING_CLASS (wc_buffer[old_n_wc]);

	  if (cc == 0)
	    {
	      g_unicode_canonical_ordering (wc_buffer + last_start, n_wc - last_start);
	      last_start = old_n_wc;
	    }
	}
      
      p = g_utf8_next_char (p);
    }

  if (n_wc > 0)
    {
      g_unicode_canonical_ordering (wc_buffer + last_start, n_wc - last_start);
      last_start = n_wc;
    }
	  
  wc_buffer[n_wc] = 0;

  /* All decomposed and reordered */ 

  if (do_compose && n_wc > 0)
    {
      gsize i, j;
      int last_cc = 0;
      last_start = 0;
      
      for (i = 0; i < n_wc; i++)
	{
	  int cc = COMBINING_CLASS (wc_buffer[i]);

	  if (i > 0 &&
	      (last_cc == 0 || last_cc < cc) &&
	      combine (wc_buffer[last_start], wc_buffer[i],
		       &wc_buffer[last_start]))
	    {
	      for (j = i + 1; j < n_wc; j++)
		wc_buffer[j-1] = wc_buffer[j];
	      n_wc--;
	      i--;
	      
	      if (i == last_start)
		last_cc = 0;
	      else
		last_cc = COMBINING_CLASS (wc_buffer[i-1]);
	      
	      continue;
	    }

	  if (cc == 0)
	    last_start = i;

	  last_cc = cc;
	}
    }

  wc_buffer[n_wc] = 0;

  return wc_buffer;
}

/**
 * g_utf8_normalize:
 * @str: a UTF-8 encoded string.
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * @mode: the type of normalization to perform.
 *
 * Converts a string into canonical form, standardizing
 * such issues as whether a character with an accent
 * is represented as a base character and combining
 * accent or as a single precomposed character. The
 * string has to be valid UTF-8, otherwise %NULL is
 * returned. You should generally call g_utf8_normalize()
 * before comparing two Unicode strings.
 *
 * The normalization mode %G_NORMALIZE_DEFAULT only
 * standardizes differences that do not affect the
 * text content, such as the above-mentioned accent
 * representation. %G_NORMALIZE_ALL also standardizes
 * the "compatibility" characters in Unicode, such
 * as SUPERSCRIPT THREE to the standard forms
 * (in this case DIGIT THREE). Formatting information
 * may be lost but for most text operations such
 * characters should be considered the same.
 *
 * %G_NORMALIZE_DEFAULT_COMPOSE and %G_NORMALIZE_ALL_COMPOSE
 * are like %G_NORMALIZE_DEFAULT and %G_NORMALIZE_ALL,
 * but returned a result with composed forms rather
 * than a maximally decomposed form. This is often
 * useful if you intend to convert the string to
 * a legacy encoding or pass it to a system with
 * less capable Unicode handling.
 *
 * Returns: a newly allocated string, that is the
 *   normalized form of @str, or %NULL if @str is not
 *   valid UTF-8.
 **/
gchar *
g_utf8_normalize (const gchar    *str,
		  gssize          len,
		  GNormalizeMode  mode)
{
  gunichar *result_wc = _g_utf8_normalize_wc (str, len, mode);
  gchar *result;

  result = g_ucs4_to_utf8 (result_wc, -1, NULL, NULL, NULL);
  g_free (result_wc);

  return result;
}

static gboolean
decompose_hangul_step (gunichar  ch,
                       gunichar *a,
                       gunichar *b)
{
  gint SIndex, TIndex;

  if (ch < SBase || ch >= SBase + SCount)
    return FALSE;  /* not a hangul syllable */

  SIndex = ch - SBase;
  TIndex = SIndex % TCount;

  if (TIndex)
    {
      /* split LVT -> LV,T */
      *a = ch - TIndex;
      *b = TBase + TIndex;
    }
  else
    {
      /* split LV -> L,V */
      *a = LBase + SIndex / NCount;
      *b = VBase + (SIndex % NCount) / TCount;
    }

  return TRUE;
}

/**
 * g_unichar_decompose:
 * @ch: a Unicode character
 * @a: return location for the first component of @ch
 * @b: return location for the second component of @ch
 *
 * Performs a single decomposition step of the
 * Unicode canonical decomposition algorithm.
 *
 * This function does not include compatibility
 * decompositions. It does, however, include algorithmic
 * Hangul Jamo decomposition, as well as 'singleton'
 * decompositions which replace a character by a single
 * other character. In the case of singletons *@b will
 * be set to zero.
 *
 * If @ch is not decomposable, *@a is set to @ch and *@b
 * is set to zero.
 *
 * Note that the way Unicode decomposition pairs are
 * defined, it is guaranteed that @b would not decompose
 * further, but @a may itself decompose.  To get the full
 * canonical decomposition for @ch, one would need to
 * recursively call this function on @a.  Or use
 * g_unichar_fully_decompose().
 *
 * See
 * [UAX#15](http://unicode.org/reports/tr15/)
 * for details.
 *
 * Returns: %TRUE if the character could be decomposed
 *
 * Since: 2.30
 */
gboolean
g_unichar_decompose (gunichar  ch,
                     gunichar *a,
                     gunichar *b)
{
  gint start = 0;
  gint end = G_N_ELEMENTS (decomp_step_table);

  if (decompose_hangul_step (ch, a, b))
    return TRUE;

  /* TODO use bsearch() */
  if (ch >= decomp_step_table[start].ch &&
      ch <= decomp_step_table[end - 1].ch)
    {
      while (TRUE)
        {
          gint half = (start + end) / 2;
          const decomposition_step *p = &(decomp_step_table[half]);
          if (ch == p->ch)
            {
              *a = p->a;
              *b = p->b;
              return TRUE;
            }
          else if (half == start)
            break;
          else if (ch > p->ch)
            start = half;
          else
            end = half;
        }
    }

  *a = ch;
  *b = 0;

  return FALSE;
}

/**
 * g_unichar_compose:
 * @a: a Unicode character
 * @b: a Unicode character
 * @ch: return location for the composed character
 *
 * Performs a single composition step of the
 * Unicode canonical composition algorithm.
 *
 * This function includes algorithmic Hangul Jamo composition,
 * but it is not exactly the inverse of g_unichar_decompose().
 * No composition can have either of @a or @b equal to zero.
 * To be precise, this function composes if and only if
 * there exists a Primary Composite P which is canonically
 * equivalent to the sequence <@a,@b>.  See the Unicode
 * Standard for the definition of Primary Composite.
 *
 * If @a and @b do not compose a new character, @ch is set to zero.
 *
 * See
 * [UAX#15](http://unicode.org/reports/tr15/)
 * for details.
 *
 * Returns: %TRUE if the characters could be composed
 *
 * Since: 2.30
 */
gboolean
g_unichar_compose (gunichar  a,
                   gunichar  b,
                   gunichar *ch)
{
  if (combine (a, b, ch))
    return TRUE;

  *ch = 0;
  return FALSE;
}

/**
 * g_unichar_fully_decompose:
 * @ch: a Unicode character.
 * @compat: whether perform canonical or compatibility decomposition
 * @result: (nullable): location to store decomposed result, or %NULL
 * @result_len: length of @result
 *
 * Computes the canonical or compatibility decomposition of a
 * Unicode character.  For compatibility decomposition,
 * pass %TRUE for @compat; for canonical decomposition
 * pass %FALSE for @compat.
 *
 * The decomposed sequence is placed in @result.  Only up to
 * @result_len characters are written into @result.  The length
 * of the full decomposition (irrespective of @result_len) is
 * returned by the function.  For canonical decomposition,
 * currently all decompositions are of length at most 4, but
 * this may change in the future (very unlikely though).
 * At any rate, Unicode does guarantee that a buffer of length
 * 18 is always enough for both compatibility and canonical
 * decompositions, so that is the size recommended. This is provided
 * as %G_UNICHAR_MAX_DECOMPOSITION_LENGTH.
 *
 * See
 * [UAX#15](http://unicode.org/reports/tr15/)
 * for details.
 *
 * Returns: the length of the full decomposition.
 *
 * Since: 2.30
 **/
gsize
g_unichar_fully_decompose (gunichar  ch,
			   gboolean  compat,
			   gunichar *result,
			   gsize     result_len)
{
  const gchar *decomp;
  const gchar *p;

  /* Hangul syllable */
  if (ch >= SBase && ch < SBase + SCount)
    {
      gsize len, i;
      gunichar buffer[3];
      decompose_hangul (ch, result ? buffer : NULL, &len);
      if (result)
        for (i = 0; i < len && i < result_len; i++)
	  result[i] = buffer[i];
      return len;
    }
  else if ((decomp = find_decomposition (ch, compat)) != NULL)
    {
      /* Found it.  */
      gsize len, i;

      len = g_utf8_strlen (decomp, -1);

      for (p = decomp, i = 0; i < len && i < result_len; p = g_utf8_next_char (p), i++)
        result[i] = g_utf8_get_char (p);

      return len;
    }

  /* Does not decompose */
  if (result && result_len >= 1)
    *result = ch;
  return 1;
}
