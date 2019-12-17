/* guniprop.c - Unicode character properties.
 *
 * Copyright (C) 1999 Tom Tromey
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <locale.h>

#include "gmem.h"
#include "gstring.h"
#include "gtestutils.h"
#include "gtypes.h"
#include "gunicode.h"
#include "gunichartables.h"
#include "gmirroringtable.h"
#include "gscripttable.h"
#include "gunicodeprivate.h"
#ifdef G_OS_WIN32
#include "gwin32.h"
#endif

#define ATTR_TABLE(Page) (((Page) <= G_UNICODE_LAST_PAGE_PART1) \
                          ? attr_table_part1[Page] \
                          : attr_table_part2[(Page) - 0xe00])

#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == G_UNICODE_MAX_TABLE_INDEX) ? 0 : (attr_data[ATTR_TABLE(Page)][Char]))

#define TTYPE_PART1(Page, Char) \
  ((type_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part1[Page]][Char]))

#define TTYPE_PART2(Page, Char) \
  ((type_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part2[Page]][Char]))

#define TYPE(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : G_UNICODE_UNASSIGNED))


#define IS(Type, Class)	(((guint)1 << (Type)) & (Class))
#define OR(Type, Rest)	(((guint)1 << (Type)) | (Rest))



#define ISALPHA(Type)	IS ((Type),				\
			    OR (G_UNICODE_LOWERCASE_LETTER,	\
			    OR (G_UNICODE_UPPERCASE_LETTER,	\
			    OR (G_UNICODE_TITLECASE_LETTER,	\
			    OR (G_UNICODE_MODIFIER_LETTER,	\
			    OR (G_UNICODE_OTHER_LETTER,		0))))))

#define ISALDIGIT(Type)	IS ((Type),				\
			    OR (G_UNICODE_DECIMAL_NUMBER,	\
			    OR (G_UNICODE_LETTER_NUMBER,	\
			    OR (G_UNICODE_OTHER_NUMBER,		\
			    OR (G_UNICODE_LOWERCASE_LETTER,	\
			    OR (G_UNICODE_UPPERCASE_LETTER,	\
			    OR (G_UNICODE_TITLECASE_LETTER,	\
			    OR (G_UNICODE_MODIFIER_LETTER,	\
			    OR (G_UNICODE_OTHER_LETTER,		0)))))))))

#define ISMARK(Type)	IS ((Type),				\
			    OR (G_UNICODE_NON_SPACING_MARK,	\
			    OR (G_UNICODE_SPACING_MARK,	\
			    OR (G_UNICODE_ENCLOSING_MARK,	0))))

#define ISZEROWIDTHTYPE(Type)	IS ((Type),			\
			    OR (G_UNICODE_NON_SPACING_MARK,	\
			    OR (G_UNICODE_ENCLOSING_MARK,	\
			    OR (G_UNICODE_FORMAT,		0))))

/**
 * g_unichar_isalnum:
 * @c: a Unicode character
 * 
 * Determines whether a character is alphanumeric.
 * Given some UTF-8 text, obtain a character value
 * with g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is an alphanumeric character
 **/
gboolean
g_unichar_isalnum (gunichar c)
{
  return ISALDIGIT (TYPE (c)) ? TRUE : FALSE;
}

/**
 * g_unichar_isalpha:
 * @c: a Unicode character
 * 
 * Determines whether a character is alphabetic (i.e. a letter).
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is an alphabetic character
 **/
gboolean
g_unichar_isalpha (gunichar c)
{
  return ISALPHA (TYPE (c)) ? TRUE : FALSE;
}


/**
 * g_unichar_iscntrl:
 * @c: a Unicode character
 * 
 * Determines whether a character is a control character.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a control character
 **/
gboolean
g_unichar_iscntrl (gunichar c)
{
  return TYPE (c) == G_UNICODE_CONTROL;
}

/**
 * g_unichar_isdigit:
 * @c: a Unicode character
 * 
 * Determines whether a character is numeric (i.e. a digit).  This
 * covers ASCII 0-9 and also digits in other languages/scripts.  Given
 * some UTF-8 text, obtain a character value with g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a digit
 **/
gboolean
g_unichar_isdigit (gunichar c)
{
  return TYPE (c) == G_UNICODE_DECIMAL_NUMBER;
}


/**
 * g_unichar_isgraph:
 * @c: a Unicode character
 * 
 * Determines whether a character is printable and not a space
 * (returns %FALSE for control characters, format characters, and
 * spaces). g_unichar_isprint() is similar, but returns %TRUE for
 * spaces. Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is printable unless it's a space
 **/
gboolean
g_unichar_isgraph (gunichar c)
{
  return !IS (TYPE(c),
	      OR (G_UNICODE_CONTROL,
	      OR (G_UNICODE_FORMAT,
	      OR (G_UNICODE_UNASSIGNED,
	      OR (G_UNICODE_SURROGATE,
	      OR (G_UNICODE_SPACE_SEPARATOR,
	     0))))));
}

/**
 * g_unichar_islower:
 * @c: a Unicode character
 * 
 * Determines whether a character is a lowercase letter.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a lowercase letter
 **/
gboolean
g_unichar_islower (gunichar c)
{
  return TYPE (c) == G_UNICODE_LOWERCASE_LETTER;
}


/**
 * g_unichar_isprint:
 * @c: a Unicode character
 * 
 * Determines whether a character is printable.
 * Unlike g_unichar_isgraph(), returns %TRUE for spaces.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is printable
 **/
gboolean
g_unichar_isprint (gunichar c)
{
  return !IS (TYPE(c),
	      OR (G_UNICODE_CONTROL,
	      OR (G_UNICODE_FORMAT,
	      OR (G_UNICODE_UNASSIGNED,
	      OR (G_UNICODE_SURROGATE,
	     0)))));
}

/**
 * g_unichar_ispunct:
 * @c: a Unicode character
 * 
 * Determines whether a character is punctuation or a symbol.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Returns: %TRUE if @c is a punctuation or symbol character
 **/
gboolean
g_unichar_ispunct (gunichar c)
{
  return IS (TYPE(c),
	     OR (G_UNICODE_CONNECT_PUNCTUATION,
	     OR (G_UNICODE_DASH_PUNCTUATION,
	     OR (G_UNICODE_CLOSE_PUNCTUATION,
	     OR (G_UNICODE_FINAL_PUNCTUATION,
	     OR (G_UNICODE_INITIAL_PUNCTUATION,
	     OR (G_UNICODE_OTHER_PUNCTUATION,
	     OR (G_UNICODE_OPEN_PUNCTUATION,
	     OR (G_UNICODE_CURRENCY_SYMBOL,
	     OR (G_UNICODE_MODIFIER_SYMBOL,
	     OR (G_UNICODE_MATH_SYMBOL,
	     OR (G_UNICODE_OTHER_SYMBOL,
	    0)))))))))))) ? TRUE : FALSE;
}

/**
 * g_unichar_isspace:
 * @c: a Unicode character
 * 
 * Determines whether a character is a space, tab, or line separator
 * (newline, carriage return, etc.).  Given some UTF-8 text, obtain a
 * character value with g_utf8_get_char().
 *
 * (Note: don't use this to do word breaking; you have to use
 * Pango or equivalent to get word breaking right, the algorithm
 * is fairly complex.)
 *  
 * Returns: %TRUE if @c is a space character
 **/
gboolean
g_unichar_isspace (gunichar c)
{
  switch (c)
    {
      /* special-case these since Unicode thinks they are not spaces */
    case '\t':
    case '\n':
    case '\r':
    case '\f':
      return TRUE;
      break;
      
    default:
      {
	return IS (TYPE(c),
	           OR (G_UNICODE_SPACE_SEPARATOR,
	           OR (G_UNICODE_LINE_SEPARATOR,
                   OR (G_UNICODE_PARAGRAPH_SEPARATOR,
		  0)))) ? TRUE : FALSE;
      }
      break;
    }
}

/**
 * g_unichar_ismark:
 * @c: a Unicode character
 *
 * Determines whether a character is a mark (non-spacing mark,
 * combining mark, or enclosing mark in Unicode speak).
 * Given some UTF-8 text, obtain a character value
 * with g_utf8_get_char().
 *
 * Note: in most cases where isalpha characters are allowed,
 * ismark characters should be allowed to as they are essential
 * for writing most European languages as well as many non-Latin
 * scripts.
 *
 * Returns: %TRUE if @c is a mark character
 *
 * Since: 2.14
 **/
gboolean
g_unichar_ismark (gunichar c)
{
  return ISMARK (TYPE (c));
}

/**
 * g_unichar_isupper:
 * @c: a Unicode character
 * 
 * Determines if a character is uppercase.
 * 
 * Returns: %TRUE if @c is an uppercase character
 **/
gboolean
g_unichar_isupper (gunichar c)
{
  return TYPE (c) == G_UNICODE_UPPERCASE_LETTER;
}

/**
 * g_unichar_istitle:
 * @c: a Unicode character
 * 
 * Determines if a character is titlecase. Some characters in
 * Unicode which are composites, such as the DZ digraph
 * have three case variants instead of just two. The titlecase
 * form is used at the beginning of a word where only the
 * first letter is capitalized. The titlecase form of the DZ
 * digraph is U+01F2 LATIN CAPITAL LETTTER D WITH SMALL LETTER Z.
 * 
 * Returns: %TRUE if the character is titlecase
 **/
gboolean
g_unichar_istitle (gunichar c)
{
  unsigned int i;
  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
    if (title_table[i][0] == c)
      return TRUE;
  return FALSE;
}

/**
 * g_unichar_isxdigit:
 * @c: a Unicode character.
 * 
 * Determines if a character is a hexidecimal digit.
 * 
 * Returns: %TRUE if the character is a hexadecimal digit
 **/
gboolean
g_unichar_isxdigit (gunichar c)
{
  return ((c >= 'a' && c <= 'f')
	  || (c >= 'A' && c <= 'F')
	  || (TYPE (c) == G_UNICODE_DECIMAL_NUMBER));
}

/**
 * g_unichar_isdefined:
 * @c: a Unicode character
 * 
 * Determines if a given character is assigned in the Unicode
 * standard.
 *
 * Returns: %TRUE if the character has an assigned value
 **/
gboolean
g_unichar_isdefined (gunichar c)
{
  return !IS (TYPE(c),
	      OR (G_UNICODE_UNASSIGNED,
	      OR (G_UNICODE_SURROGATE,
	     0)));
}

/**
 * g_unichar_iszerowidth:
 * @c: a Unicode character
 * 
 * Determines if a given character typically takes zero width when rendered.
 * The return value is %TRUE for all non-spacing and enclosing marks
 * (e.g., combining accents), format characters, zero-width
 * space, but not U+00AD SOFT HYPHEN.
 *
 * A typical use of this function is with one of g_unichar_iswide() or
 * g_unichar_iswide_cjk() to determine the number of cells a string occupies
 * when displayed on a grid display (terminals).  However, note that not all
 * terminals support zero-width rendering of zero-width marks.
 *
 * Returns: %TRUE if the character has zero width
 *
 * Since: 2.14
 **/
gboolean
g_unichar_iszerowidth (gunichar c)
{
  if (G_UNLIKELY (c == 0x00AD))
    return FALSE;

  if (G_UNLIKELY (ISZEROWIDTHTYPE (TYPE (c))))
    return TRUE;

  if (G_UNLIKELY ((c >= 0x1160 && c < 0x1200) ||
		  c == 0x200B))
    return TRUE;

  return FALSE;
}

static int
interval_compare (const void *key, const void *elt)
{
  gunichar c = GPOINTER_TO_UINT (key);
  struct Interval *interval = (struct Interval *)elt;

  if (c < interval->start)
    return -1;
  if (c > interval->end)
    return +1;

  return 0;
}

#define G_WIDTH_TABLE_MIDPOINT (G_N_ELEMENTS (g_unicode_width_table_wide) / 2)

static inline gboolean
g_unichar_iswide_bsearch (gunichar ch)
{
  int lower = 0;
  int upper = G_N_ELEMENTS (g_unicode_width_table_wide) - 1;
  static int saved_mid = G_WIDTH_TABLE_MIDPOINT;
  int mid = saved_mid;

  do
    {
      if (ch < g_unicode_width_table_wide[mid].start)
	upper = mid - 1;
      else if (ch > g_unicode_width_table_wide[mid].end)
	lower = mid + 1;
      else
	return TRUE;

      mid = (lower + upper) / 2;
    }
  while (lower <= upper);

  return FALSE;
}

/**
 * g_unichar_iswide:
 * @c: a Unicode character
 * 
 * Determines if a character is typically rendered in a double-width
 * cell.
 * 
 * Returns: %TRUE if the character is wide
 **/
gboolean
g_unichar_iswide (gunichar c)
{
  if (c < g_unicode_width_table_wide[0].start)
    return FALSE;
  else
    return g_unichar_iswide_bsearch (c);
}


/**
 * g_unichar_iswide_cjk:
 * @c: a Unicode character
 * 
 * Determines if a character is typically rendered in a double-width
 * cell under legacy East Asian locales.  If a character is wide according to
 * g_unichar_iswide(), then it is also reported wide with this function, but
 * the converse is not necessarily true. See the
 * [Unicode Standard Annex #11](http://www.unicode.org/reports/tr11/)
 * for details.
 *
 * If a character passes the g_unichar_iswide() test then it will also pass
 * this test, but not the other way around.  Note that some characters may
 * pass both this test and g_unichar_iszerowidth().
 * 
 * Returns: %TRUE if the character is wide in legacy East Asian locales
 *
 * Since: 2.12
 */
gboolean
g_unichar_iswide_cjk (gunichar c)
{
  if (g_unichar_iswide (c))
    return TRUE;

  /* bsearch() is declared attribute(nonnull(1)) so we can't validly search
   * for a NULL key */
  if (c == 0)
    return FALSE;

  if (bsearch (GUINT_TO_POINTER (c), 
               g_unicode_width_table_ambiguous,
               G_N_ELEMENTS (g_unicode_width_table_ambiguous),
               sizeof g_unicode_width_table_ambiguous[0],
	       interval_compare))
    return TRUE;

  return FALSE;
}


/**
 * g_unichar_toupper:
 * @c: a Unicode character
 * 
 * Converts a character to uppercase.
 * 
 * Returns: the result of converting @c to uppercase.
 *               If @c is not an lowercase or titlecase character,
 *               or has no upper case equivalent @c is returned unchanged.
 **/
gunichar
g_unichar_toupper (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_LOWERCASE_LETTER)
    {
      gunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const gchar *p = special_case_table + val - 0x1000000;
          val = g_utf8_get_char (p);
	}
      /* Some lowercase letters, e.g., U+000AA, FEMININE ORDINAL INDICATOR,
       * do not have an uppercase equivalent, in which case val will be
       * zero. 
       */
      return val ? val : c;
    }
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][1] ? title_table[i][1] : c;
	}
    }
  return c;
}

/**
 * g_unichar_tolower:
 * @c: a Unicode character.
 * 
 * Converts a character to lower case.
 * 
 * Returns: the result of converting @c to lower case.
 *               If @c is not an upperlower or titlecase character,
 *               or has no lowercase equivalent @c is returned unchanged.
 **/
gunichar
g_unichar_tolower (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_UPPERCASE_LETTER)
    {
      gunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const gchar *p = special_case_table + val - 0x1000000;
	  return g_utf8_get_char (p);
	}
      else
	{
	  /* Not all uppercase letters are guaranteed to have a lowercase
	   * equivalent.  If this is the case, val will be zero. */
	  return val ? val : c;
	}
    }
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][2];
	}
    }
  return c;
}

/**
 * g_unichar_totitle:
 * @c: a Unicode character
 * 
 * Converts a character to the titlecase.
 * 
 * Returns: the result of converting @c to titlecase.
 *               If @c is not an uppercase or lowercase character,
 *               @c is returned unchanged.
 **/
gunichar
g_unichar_totitle (gunichar c)
{
  unsigned int i;
  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
    {
      if (title_table[i][0] == c || title_table[i][1] == c
	  || title_table[i][2] == c)
	return title_table[i][0];
    }
    
  if (TYPE (c) == G_UNICODE_LOWERCASE_LETTER)
    return g_unichar_toupper (c);

  return c;
}

/**
 * g_unichar_digit_value:
 * @c: a Unicode character
 *
 * Determines the numeric value of a character as a decimal
 * digit.
 *
 * Returns: If @c is a decimal digit (according to
 * g_unichar_isdigit()), its numeric value. Otherwise, -1.
 **/
int
g_unichar_digit_value (gunichar c)
{
  if (TYPE (c) == G_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * g_unichar_xdigit_value:
 * @c: a Unicode character
 *
 * Determines the numeric value of a character as a hexidecimal
 * digit.
 *
 * Returns: If @c is a hex digit (according to
 * g_unichar_isxdigit()), its numeric value. Otherwise, -1.
 **/
int
g_unichar_xdigit_value (gunichar c)
{
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (TYPE (c) == G_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * g_unichar_type:
 * @c: a Unicode character
 * 
 * Classifies a Unicode character by type.
 * 
 * Returns: the type of the character.
 **/
GUnicodeType
g_unichar_type (gunichar c)
{
  return TYPE (c);
}

/*
 * Case mapping functions
 */

typedef enum {
  LOCALE_NORMAL,
  LOCALE_TURKIC,
  LOCALE_LITHUANIAN
} LocaleType;

static LocaleType
get_locale_type (void)
{
#ifdef G_OS_WIN32
  char *tem = g_win32_getlocale ();
  char locale[2];

  locale[0] = tem[0];
  locale[1] = tem[1];
  g_free (tem);
#else
  const char *locale = setlocale (LC_CTYPE, NULL);

  if (locale == NULL)
    return LOCALE_NORMAL;
#endif

  switch (locale[0])
    {
   case 'a':
      if (locale[1] == 'z')
	return LOCALE_TURKIC;
      break;
    case 'l':
      if (locale[1] == 't')
	return LOCALE_LITHUANIAN;
      break;
    case 't':
      if (locale[1] == 'r')
	return LOCALE_TURKIC;
      break;
    }

  return LOCALE_NORMAL;
}

static gint
output_marks (const char **p_inout,
	      char        *out_buffer,
	      gboolean     remove_dot)
{
  const char *p = *p_inout;
  gint len = 0;
  
  while (*p)
    {
      gunichar c = g_utf8_get_char (p);
      
      if (ISMARK (TYPE (c)))
	{
	  if (!remove_dot || c != 0x307 /* COMBINING DOT ABOVE */)
	    len += g_unichar_to_utf8 (c, out_buffer ? out_buffer + len : NULL);
	  p = g_utf8_next_char (p);
	}
      else
	break;
    }

  *p_inout = p;
  return len;
}

static gint
output_special_case (gchar *out_buffer,
		     int    offset,
		     int    type,
		     int    which)
{
  const gchar *p = special_case_table + offset;
  gint len;

  if (type != G_UNICODE_TITLECASE_LETTER)
    p = g_utf8_next_char (p);

  if (which == 1)
    p += strlen (p) + 1;

  len = strlen (p);
  if (out_buffer)
    memcpy (out_buffer, p, len);

  return len;
}

static gsize
real_toupper (const gchar *str,
	      gssize       max_len,
	      gchar       *out_buffer,
	      LocaleType   locale_type)
{
  const gchar *p = str;
  const char *last = NULL;
  gsize len = 0;
  gboolean last_was_i = FALSE;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      gunichar c = g_utf8_get_char (p);
      int t = TYPE (c);
      gunichar val;

      last = p;
      p = g_utf8_next_char (p);

      if (locale_type == LOCALE_LITHUANIAN)
	{
	  if (c == 'i')
	    last_was_i = TRUE;
	  else 
	    {
	      if (last_was_i)
		{
		  /* Nasty, need to remove any dot above. Though
		   * I think only E WITH DOT ABOVE occurs in practice
		   * which could simplify this considerably.
		   */
		  gsize decomp_len, i;
		  gunichar decomp[G_UNICHAR_MAX_DECOMPOSITION_LENGTH];

		  decomp_len = g_unichar_fully_decompose (c, FALSE, decomp, G_N_ELEMENTS (decomp));
		  for (i=0; i < decomp_len; i++)
		    {
		      if (decomp[i] != 0x307 /* COMBINING DOT ABOVE */)
			len += g_unichar_to_utf8 (g_unichar_toupper (decomp[i]), out_buffer ? out_buffer + len : NULL);
		    }
		  
		  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, TRUE);

		  continue;
		}

	      if (!ISMARK (t))
		last_was_i = FALSE;
	    }
	}

      if (locale_type == LOCALE_TURKIC && c == 'i')
	{
	  /* i => LATIN CAPITAL LETTER I WITH DOT ABOVE */
	  len += g_unichar_to_utf8 (0x130, out_buffer ? out_buffer + len : NULL); 
	}
      else if (c == 0x0345)	/* COMBINING GREEK YPOGEGRAMMENI */
	{
	  /* Nasty, need to move it after other combining marks .. this would go away if
	   * we normalized first.
	   */
	  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, FALSE);

	  /* And output as GREEK CAPITAL LETTER IOTA */
	  len += g_unichar_to_utf8 (0x399, out_buffer ? out_buffer + len : NULL); 	  
	}
      else if (IS (t,
		   OR (G_UNICODE_LOWERCASE_LETTER,
		   OR (G_UNICODE_TITLECASE_LETTER,
		  0))))
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t,
					  t == G_UNICODE_LOWERCASE_LETTER ? 0 : 1);
	    }
	  else
	    {
	      if (t == G_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
		    {
		      if (title_table[i][0] == c)
			{
			  val = title_table[i][1];
			  break;
			}
		    }
		}

	      /* Some lowercase letters, e.g., U+000AA, FEMININE ORDINAL INDICATOR,
	       * do not have an uppercase equivalent, in which case val will be
	       * zero. */
	      len += g_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  gsize char_len = g_utf8_skip[*(guchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

/**
 * g_utf8_strup:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts all Unicode characters in the string that have a case
 * to uppercase. The exact manner that this is done depends
 * on the current locale, and may result in the number of
 * characters in the string increasing. (For instance, the
 * German ess-zet will be changed to SS.)
 * 
 * Returns: a newly allocated string, with all characters
 *    converted to uppercase.  
 **/
gchar *
g_utf8_strup (const gchar *str,
	      gssize       len)
{
  gsize result_len;
  LocaleType locale_type;
  gchar *result;

  g_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();
  
  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_toupper (str, len, NULL, locale_type);
  result = g_malloc (result_len + 1);
  real_toupper (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

/* traverses the string checking for characters with combining class == 230
 * until a base character is found */
static gboolean
has_more_above (const gchar *str)
{
  const gchar *p = str;
  gint combining_class;

  while (*p)
    {
      combining_class = g_unichar_combining_class (g_utf8_get_char (p));
      if (combining_class == 230)
        return TRUE;
      else if (combining_class == 0)
        break;

      p = g_utf8_next_char (p);
    }

  return FALSE;
}

static gsize
real_tolower (const gchar *str,
	      gssize       max_len,
	      gchar       *out_buffer,
	      LocaleType   locale_type)
{
  const gchar *p = str;
  const char *last = NULL;
  gsize len = 0;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      gunichar c = g_utf8_get_char (p);
      int t = TYPE (c);
      gunichar val;

      last = p;
      p = g_utf8_next_char (p);

      if (locale_type == LOCALE_TURKIC && c == 'I')
	{
          if (g_utf8_get_char (p) == 0x0307)
            {
              /* I + COMBINING DOT ABOVE => i (U+0069) */
              len += g_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL); 
              p = g_utf8_next_char (p);
            }
          else
            {
              /* I => LATIN SMALL LETTER DOTLESS I */
              len += g_unichar_to_utf8 (0x131, out_buffer ? out_buffer + len : NULL); 
            }
        }
      /* Introduce an explicit dot above when lowercasing capital I's and J's
       * whenever there are more accents above. [SpecialCasing.txt] */
      else if (locale_type == LOCALE_LITHUANIAN && 
               (c == 0x00cc || c == 0x00cd || c == 0x0128))
        {
          len += g_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL); 
          len += g_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL); 

          switch (c)
            {
            case 0x00cc: 
              len += g_unichar_to_utf8 (0x0300, out_buffer ? out_buffer + len : NULL); 
              break;
            case 0x00cd: 
              len += g_unichar_to_utf8 (0x0301, out_buffer ? out_buffer + len : NULL); 
              break;
            case 0x0128: 
              len += g_unichar_to_utf8 (0x0303, out_buffer ? out_buffer + len : NULL); 
              break;
            }
        }
      else if (locale_type == LOCALE_LITHUANIAN && 
               (c == 'I' || c == 'J' || c == 0x012e) && 
               has_more_above (p))
        {
          len += g_unichar_to_utf8 (g_unichar_tolower (c), out_buffer ? out_buffer + len : NULL); 
          len += g_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL); 
        }
      else if (c == 0x03A3)	/* GREEK CAPITAL LETTER SIGMA */
	{
	  if ((max_len < 0 || p < str + max_len) && *p)
	    {
	      gunichar next_c = g_utf8_get_char (p);
	      int next_type = TYPE(next_c);

	      /* SIGMA mapps differently depending on whether it is
	       * final or not. The following simplified test would
	       * fail in the case of combining marks following the
	       * sigma, but I don't think that occurs in real text.
	       * The test here matches that in ICU.
	       */
	      if (ISALPHA (next_type)) /* Lu,Ll,Lt,Lm,Lo */
		val = 0x3c3;	/* GREEK SMALL SIGMA */
	      else
		val = 0x3c2;	/* GREEK SMALL FINAL SIGMA */
	    }
	  else
	    val = 0x3c2;	/* GREEK SMALL FINAL SIGMA */

	  len += g_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
	}
      else if (IS (t,
		   OR (G_UNICODE_UPPERCASE_LETTER,
		   OR (G_UNICODE_TITLECASE_LETTER,
		  0))))
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t, 0);
	    }
	  else
	    {
	      if (t == G_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
		    {
		      if (title_table[i][0] == c)
			{
			  val = title_table[i][2];
			  break;
			}
		    }
		}

	      /* Not all uppercase letters are guaranteed to have a lowercase
	       * equivalent.  If this is the case, val will be zero. */
	      len += g_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  gsize char_len = g_utf8_skip[*(guchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

/**
 * g_utf8_strdown:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts all Unicode characters in the string that have a case
 * to lowercase. The exact manner that this is done depends
 * on the current locale, and may result in the number of
 * characters in the string changing.
 * 
 * Returns: a newly allocated string, with all characters
 *    converted to lowercase.  
 **/
gchar *
g_utf8_strdown (const gchar *str,
		gssize       len)
{
  gsize result_len;
  LocaleType locale_type;
  gchar *result;

  g_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();
  
  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_tolower (str, len, NULL, locale_type);
  result = g_malloc (result_len + 1);
  real_tolower (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

/**
 * g_utf8_casefold:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts a string into a form that is independent of case. The
 * result will not correspond to any particular case, but can be
 * compared for equality or ordered with the results of calling
 * g_utf8_casefold() on other strings.
 * 
 * Note that calling g_utf8_casefold() followed by g_utf8_collate() is
 * only an approximation to the correct linguistic case insensitive
 * ordering, though it is a fairly good one. Getting this exactly
 * right would require a more sophisticated collation function that
 * takes case sensitivity into account. GLib does not currently
 * provide such a function.
 * 
 * Returns: a newly allocated string, that is a
 *   case independent form of @str.
 **/
gchar *
g_utf8_casefold (const gchar *str,
		 gssize       len)
{
  GString *result;
  const char *p;

  g_return_val_if_fail (str != NULL, NULL);

  result = g_string_new (NULL);
  p = str;
  while ((len < 0 || p < str + len) && *p)
    {
      gunichar ch = g_utf8_get_char (p);

      int start = 0;
      int end = G_N_ELEMENTS (casefold_table);

      if (ch >= casefold_table[start].ch &&
          ch <= casefold_table[end - 1].ch)
	{
	  while (TRUE)
	    {
	      int half = (start + end) / 2;
	      if (ch == casefold_table[half].ch)
		{
		  g_string_append (result, casefold_table[half].data);
		  goto next;
		}
	      else if (half == start)
		break;
	      else if (ch > casefold_table[half].ch)
		start = half;
	      else
		end = half;
	    }
	}

      g_string_append_unichar (result, g_unichar_tolower (ch));
      
    next:
      p = g_utf8_next_char (p);
    }

  return g_string_free (result, FALSE); 
}

/**
 * g_unichar_get_mirror_char:
 * @ch: a Unicode character
 * @mirrored_ch: location to store the mirrored character
 * 
 * In Unicode, some characters are "mirrored". This means that their
 * images are mirrored horizontally in text that is laid out from right
 * to left. For instance, "(" would become its mirror image, ")", in
 * right-to-left text.
 *
 * If @ch has the Unicode mirrored property and there is another unicode
 * character that typically has a glyph that is the mirror image of @ch's
 * glyph and @mirrored_ch is set, it puts that character in the address
 * pointed to by @mirrored_ch.  Otherwise the original character is put.
 *
 * Returns: %TRUE if @ch has a mirrored character, %FALSE otherwise
 *
 * Since: 2.4
 **/
gboolean
g_unichar_get_mirror_char (gunichar ch,
                           gunichar *mirrored_ch)
{
  gboolean found;
  gunichar mirrored;

  mirrored = GLIB_GET_MIRRORING(ch);

  found = ch != mirrored;
  if (mirrored_ch)
    *mirrored_ch = mirrored;

  return found;

}

#define G_SCRIPT_TABLE_MIDPOINT (G_N_ELEMENTS (g_script_table) / 2)

static inline GUnicodeScript
g_unichar_get_script_bsearch (gunichar ch)
{
  int lower = 0;
  int upper = G_N_ELEMENTS (g_script_table) - 1;
  static int saved_mid = G_SCRIPT_TABLE_MIDPOINT;
  int mid = saved_mid;


  do 
    {
      if (ch < g_script_table[mid].start)
	upper = mid - 1;
      else if (ch >= g_script_table[mid].start + g_script_table[mid].chars)
	lower = mid + 1;
      else
	return g_script_table[saved_mid = mid].script;

      mid = (lower + upper) / 2;
    }
  while (lower <= upper);

  return G_UNICODE_SCRIPT_UNKNOWN;
}

/**
 * g_unichar_get_script:
 * @ch: a Unicode character
 * 
 * Looks up the #GUnicodeScript for a particular character (as defined 
 * by Unicode Standard Annex \#24). No check is made for @ch being a
 * valid Unicode character; if you pass in invalid character, the
 * result is undefined.
 *
 * This function is equivalent to pango_script_for_unichar() and the
 * two are interchangeable.
 * 
 * Returns: the #GUnicodeScript for the character.
 *
 * Since: 2.14
 */
GUnicodeScript
g_unichar_get_script (gunichar ch)
{
  if (ch < G_EASY_SCRIPTS_RANGE)
    return g_script_easy_table[ch];
  else 
    return g_unichar_get_script_bsearch (ch); 
}


/* http://unicode.org/iso15924/ */
static const guint32 iso15924_tags[] =
{
#define PACK(a,b,c,d) ((guint32)((((guint8)(a))<<24)|(((guint8)(b))<<16)|(((guint8)(c))<<8)|((guint8)(d))))

    PACK ('Z','y','y','y'), /* G_UNICODE_SCRIPT_COMMON */
    PACK ('Z','i','n','h'), /* G_UNICODE_SCRIPT_INHERITED */
    PACK ('A','r','a','b'), /* G_UNICODE_SCRIPT_ARABIC */
    PACK ('A','r','m','n'), /* G_UNICODE_SCRIPT_ARMENIAN */
    PACK ('B','e','n','g'), /* G_UNICODE_SCRIPT_BENGALI */
    PACK ('B','o','p','o'), /* G_UNICODE_SCRIPT_BOPOMOFO */
    PACK ('C','h','e','r'), /* G_UNICODE_SCRIPT_CHEROKEE */
    PACK ('C','o','p','t'), /* G_UNICODE_SCRIPT_COPTIC */
    PACK ('C','y','r','l'), /* G_UNICODE_SCRIPT_CYRILLIC */
    PACK ('D','s','r','t'), /* G_UNICODE_SCRIPT_DESERET */
    PACK ('D','e','v','a'), /* G_UNICODE_SCRIPT_DEVANAGARI */
    PACK ('E','t','h','i'), /* G_UNICODE_SCRIPT_ETHIOPIC */
    PACK ('G','e','o','r'), /* G_UNICODE_SCRIPT_GEORGIAN */
    PACK ('G','o','t','h'), /* G_UNICODE_SCRIPT_GOTHIC */
    PACK ('G','r','e','k'), /* G_UNICODE_SCRIPT_GREEK */
    PACK ('G','u','j','r'), /* G_UNICODE_SCRIPT_GUJARATI */
    PACK ('G','u','r','u'), /* G_UNICODE_SCRIPT_GURMUKHI */
    PACK ('H','a','n','i'), /* G_UNICODE_SCRIPT_HAN */
    PACK ('H','a','n','g'), /* G_UNICODE_SCRIPT_HANGUL */
    PACK ('H','e','b','r'), /* G_UNICODE_SCRIPT_HEBREW */
    PACK ('H','i','r','a'), /* G_UNICODE_SCRIPT_HIRAGANA */
    PACK ('K','n','d','a'), /* G_UNICODE_SCRIPT_KANNADA */
    PACK ('K','a','n','a'), /* G_UNICODE_SCRIPT_KATAKANA */
    PACK ('K','h','m','r'), /* G_UNICODE_SCRIPT_KHMER */
    PACK ('L','a','o','o'), /* G_UNICODE_SCRIPT_LAO */
    PACK ('L','a','t','n'), /* G_UNICODE_SCRIPT_LATIN */
    PACK ('M','l','y','m'), /* G_UNICODE_SCRIPT_MALAYALAM */
    PACK ('M','o','n','g'), /* G_UNICODE_SCRIPT_MONGOLIAN */
    PACK ('M','y','m','r'), /* G_UNICODE_SCRIPT_MYANMAR */
    PACK ('O','g','a','m'), /* G_UNICODE_SCRIPT_OGHAM */
    PACK ('I','t','a','l'), /* G_UNICODE_SCRIPT_OLD_ITALIC */
    PACK ('O','r','y','a'), /* G_UNICODE_SCRIPT_ORIYA */
    PACK ('R','u','n','r'), /* G_UNICODE_SCRIPT_RUNIC */
    PACK ('S','i','n','h'), /* G_UNICODE_SCRIPT_SINHALA */
    PACK ('S','y','r','c'), /* G_UNICODE_SCRIPT_SYRIAC */
    PACK ('T','a','m','l'), /* G_UNICODE_SCRIPT_TAMIL */
    PACK ('T','e','l','u'), /* G_UNICODE_SCRIPT_TELUGU */
    PACK ('T','h','a','a'), /* G_UNICODE_SCRIPT_THAANA */
    PACK ('T','h','a','i'), /* G_UNICODE_SCRIPT_THAI */
    PACK ('T','i','b','t'), /* G_UNICODE_SCRIPT_TIBETAN */
    PACK ('C','a','n','s'), /* G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL */
    PACK ('Y','i','i','i'), /* G_UNICODE_SCRIPT_YI */
    PACK ('T','g','l','g'), /* G_UNICODE_SCRIPT_TAGALOG */
    PACK ('H','a','n','o'), /* G_UNICODE_SCRIPT_HANUNOO */
    PACK ('B','u','h','d'), /* G_UNICODE_SCRIPT_BUHID */
    PACK ('T','a','g','b'), /* G_UNICODE_SCRIPT_TAGBANWA */

  /* Unicode-4.0 additions */
    PACK ('B','r','a','i'), /* G_UNICODE_SCRIPT_BRAILLE */
    PACK ('C','p','r','t'), /* G_UNICODE_SCRIPT_CYPRIOT */
    PACK ('L','i','m','b'), /* G_UNICODE_SCRIPT_LIMBU */
    PACK ('O','s','m','a'), /* G_UNICODE_SCRIPT_OSMANYA */
    PACK ('S','h','a','w'), /* G_UNICODE_SCRIPT_SHAVIAN */
    PACK ('L','i','n','b'), /* G_UNICODE_SCRIPT_LINEAR_B */
    PACK ('T','a','l','e'), /* G_UNICODE_SCRIPT_TAI_LE */
    PACK ('U','g','a','r'), /* G_UNICODE_SCRIPT_UGARITIC */

  /* Unicode-4.1 additions */
    PACK ('T','a','l','u'), /* G_UNICODE_SCRIPT_NEW_TAI_LUE */
    PACK ('B','u','g','i'), /* G_UNICODE_SCRIPT_BUGINESE */
    PACK ('G','l','a','g'), /* G_UNICODE_SCRIPT_GLAGOLITIC */
    PACK ('T','f','n','g'), /* G_UNICODE_SCRIPT_TIFINAGH */
    PACK ('S','y','l','o'), /* G_UNICODE_SCRIPT_SYLOTI_NAGRI */
    PACK ('X','p','e','o'), /* G_UNICODE_SCRIPT_OLD_PERSIAN */
    PACK ('K','h','a','r'), /* G_UNICODE_SCRIPT_KHAROSHTHI */

  /* Unicode-5.0 additions */
    PACK ('Z','z','z','z'), /* G_UNICODE_SCRIPT_UNKNOWN */
    PACK ('B','a','l','i'), /* G_UNICODE_SCRIPT_BALINESE */
    PACK ('X','s','u','x'), /* G_UNICODE_SCRIPT_CUNEIFORM */
    PACK ('P','h','n','x'), /* G_UNICODE_SCRIPT_PHOENICIAN */
    PACK ('P','h','a','g'), /* G_UNICODE_SCRIPT_PHAGS_PA */
    PACK ('N','k','o','o'), /* G_UNICODE_SCRIPT_NKO */

  /* Unicode-5.1 additions */
    PACK ('K','a','l','i'), /* G_UNICODE_SCRIPT_KAYAH_LI */
    PACK ('L','e','p','c'), /* G_UNICODE_SCRIPT_LEPCHA */
    PACK ('R','j','n','g'), /* G_UNICODE_SCRIPT_REJANG */
    PACK ('S','u','n','d'), /* G_UNICODE_SCRIPT_SUNDANESE */
    PACK ('S','a','u','r'), /* G_UNICODE_SCRIPT_SAURASHTRA */
    PACK ('C','h','a','m'), /* G_UNICODE_SCRIPT_CHAM */
    PACK ('O','l','c','k'), /* G_UNICODE_SCRIPT_OL_CHIKI */
    PACK ('V','a','i','i'), /* G_UNICODE_SCRIPT_VAI */
    PACK ('C','a','r','i'), /* G_UNICODE_SCRIPT_CARIAN */
    PACK ('L','y','c','i'), /* G_UNICODE_SCRIPT_LYCIAN */
    PACK ('L','y','d','i'), /* G_UNICODE_SCRIPT_LYDIAN */

  /* Unicode-5.2 additions */
    PACK ('A','v','s','t'), /* G_UNICODE_SCRIPT_AVESTAN */
    PACK ('B','a','m','u'), /* G_UNICODE_SCRIPT_BAMUM */
    PACK ('E','g','y','p'), /* G_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS */
    PACK ('A','r','m','i'), /* G_UNICODE_SCRIPT_IMPERIAL_ARAMAIC */
    PACK ('P','h','l','i'), /* G_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI */
    PACK ('P','r','t','i'), /* G_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN */
    PACK ('J','a','v','a'), /* G_UNICODE_SCRIPT_JAVANESE */
    PACK ('K','t','h','i'), /* G_UNICODE_SCRIPT_KAITHI */
    PACK ('L','i','s','u'), /* G_UNICODE_SCRIPT_LISU */
    PACK ('M','t','e','i'), /* G_UNICODE_SCRIPT_MEETEI_MAYEK */
    PACK ('S','a','r','b'), /* G_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN */
    PACK ('O','r','k','h'), /* G_UNICODE_SCRIPT_OLD_TURKIC */
    PACK ('S','a','m','r'), /* G_UNICODE_SCRIPT_SAMARITAN */
    PACK ('L','a','n','a'), /* G_UNICODE_SCRIPT_TAI_THAM */
    PACK ('T','a','v','t'), /* G_UNICODE_SCRIPT_TAI_VIET */

  /* Unicode-6.0 additions */
    PACK ('B','a','t','k'), /* G_UNICODE_SCRIPT_BATAK */
    PACK ('B','r','a','h'), /* G_UNICODE_SCRIPT_BRAHMI */
    PACK ('M','a','n','d'), /* G_UNICODE_SCRIPT_MANDAIC */

  /* Unicode-6.1 additions */
    PACK ('C','a','k','m'), /* G_UNICODE_SCRIPT_CHAKMA */
    PACK ('M','e','r','c'), /* G_UNICODE_SCRIPT_MEROITIC_CURSIVE */
    PACK ('M','e','r','o'), /* G_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS */
    PACK ('P','l','r','d'), /* G_UNICODE_SCRIPT_MIAO */
    PACK ('S','h','r','d'), /* G_UNICODE_SCRIPT_SHARADA */
    PACK ('S','o','r','a'), /* G_UNICODE_SCRIPT_SORA_SOMPENG */
    PACK ('T','a','k','r'), /* G_UNICODE_SCRIPT_TAKRI */

  /* Unicode 7.0 additions */
    PACK ('B','a','s','s'), /* G_UNICODE_SCRIPT_BASSA_VAH */
    PACK ('A','g','h','b'), /* G_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN */
    PACK ('D','u','p','l'), /* G_UNICODE_SCRIPT_DUPLOYAN */
    PACK ('E','l','b','a'), /* G_UNICODE_SCRIPT_ELBASAN */
    PACK ('G','r','a','n'), /* G_UNICODE_SCRIPT_GRANTHA */
    PACK ('K','h','o','j'), /* G_UNICODE_SCRIPT_KHOJKI*/
    PACK ('S','i','n','d'), /* G_UNICODE_SCRIPT_KHUDAWADI */
    PACK ('L','i','n','a'), /* G_UNICODE_SCRIPT_LINEAR_A */
    PACK ('M','a','h','j'), /* G_UNICODE_SCRIPT_MAHAJANI */
    PACK ('M','a','n','u'), /* G_UNICODE_SCRIPT_MANICHAEAN */
    PACK ('M','e','n','d'), /* G_UNICODE_SCRIPT_MENDE_KIKAKUI */
    PACK ('M','o','d','i'), /* G_UNICODE_SCRIPT_MODI */
    PACK ('M','r','o','o'), /* G_UNICODE_SCRIPT_MRO */
    PACK ('N','b','a','t'), /* G_UNICODE_SCRIPT_NABATAEAN */
    PACK ('N','a','r','b'), /* G_UNICODE_SCRIPT_OLD_NORTH_ARABIAN */
    PACK ('P','e','r','m'), /* G_UNICODE_SCRIPT_OLD_PERMIC */
    PACK ('H','m','n','g'), /* G_UNICODE_SCRIPT_PAHAWH_HMONG */
    PACK ('P','a','l','m'), /* G_UNICODE_SCRIPT_PALMYRENE */
    PACK ('P','a','u','c'), /* G_UNICODE_SCRIPT_PAU_CIN_HAU */
    PACK ('P','h','l','p'), /* G_UNICODE_SCRIPT_PSALTER_PAHLAVI */
    PACK ('S','i','d','d'), /* G_UNICODE_SCRIPT_SIDDHAM */
    PACK ('T','i','r','h'), /* G_UNICODE_SCRIPT_TIRHUTA */
    PACK ('W','a','r','a'), /* G_UNICODE_SCRIPT_WARANG_CITI */

  /* Unicode 8.0 additions */
    PACK ('A','h','o','m'), /* G_UNICODE_SCRIPT_AHOM */
    PACK ('H','l','u','w'), /* G_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS */
    PACK ('H','a','t','r'), /* G_UNICODE_SCRIPT_HATRAN */
    PACK ('M','u','l','t'), /* G_UNICODE_SCRIPT_MULTANI */
    PACK ('H','u','n','g'), /* G_UNICODE_SCRIPT_OLD_HUNGARIAN */
    PACK ('S','g','n','w'), /* G_UNICODE_SCRIPT_SIGNWRITING */

  /* Unicode 9.0 additions */
    PACK ('A','d','l','m'), /* G_UNICODE_SCRIPT_ADLAM */
    PACK ('B','h','k','s'), /* G_UNICODE_SCRIPT_BHAIKSUKI */
    PACK ('M','a','r','c'), /* G_UNICODE_SCRIPT_MARCHEN */
    PACK ('N','e','w','a'), /* G_UNICODE_SCRIPT_NEWA */
    PACK ('O','s','g','e'), /* G_UNICODE_SCRIPT_OSAGE */
    PACK ('T','a','n','g'), /* G_UNICODE_SCRIPT_TANGUT */

  /* Unicode 10.0 additions */
    PACK ('G','o','n','m'), /* G_UNICODE_SCRIPT_MASARAM_GONDI */
    PACK ('N','s','h','u'), /* G_UNICODE_SCRIPT_NUSHU */
    PACK ('S','o','y','o'), /* G_UNICODE_SCRIPT_SOYOMBO */
    PACK ('Z','a','n','b'), /* G_UNICODE_SCRIPT_ZANABAZAR_SQUARE */
#undef PACK
};

/**
 * g_unicode_script_to_iso15924:
 * @script: a Unicode script
 *
 * Looks up the ISO 15924 code for @script.  ISO 15924 assigns four-letter
 * codes to scripts.  For example, the code for Arabic is 'Arab'.  The
 * four letter codes are encoded as a @guint32 by this function in a
 * big-endian fashion.  That is, the code returned for Arabic is
 * 0x41726162 (0x41 is ASCII code for 'A', 0x72 is ASCII code for 'r', etc).
 *
 * See
 * [Codes for the representation of names of scripts](http://unicode.org/iso15924/codelists.html)
 * for details.
 *
 * Returns: the ISO 15924 code for @script, encoded as an integer,
 *   of zero if @script is %G_UNICODE_SCRIPT_INVALID_CODE or
 *   ISO 15924 code 'Zzzz' (script code for UNKNOWN) if @script is not understood.
 *
 * Since: 2.30
 */
guint32
g_unicode_script_to_iso15924 (GUnicodeScript script)
{
  if (G_UNLIKELY (script == G_UNICODE_SCRIPT_INVALID_CODE))
    return 0;

  if (G_UNLIKELY (script < 0 || script >= (int) G_N_ELEMENTS (iso15924_tags)))
    return 0x5A7A7A7A;

  return iso15924_tags[script];
}

/**
 * g_unicode_script_from_iso15924:
 * @iso15924: a Unicode script
 *
 * Looks up the Unicode script for @iso15924.  ISO 15924 assigns four-letter
 * codes to scripts.  For example, the code for Arabic is 'Arab'.
 * This function accepts four letter codes encoded as a @guint32 in a
 * big-endian fashion.  That is, the code expected for Arabic is
 * 0x41726162 (0x41 is ASCII code for 'A', 0x72 is ASCII code for 'r', etc).
 *
 * See
 * [Codes for the representation of names of scripts](http://unicode.org/iso15924/codelists.html)
 * for details.
 *
 * Returns: the Unicode script for @iso15924, or
 *   of %G_UNICODE_SCRIPT_INVALID_CODE if @iso15924 is zero and
 *   %G_UNICODE_SCRIPT_UNKNOWN if @iso15924 is unknown.
 *
 * Since: 2.30
 */
GUnicodeScript
g_unicode_script_from_iso15924 (guint32 iso15924)
{
  unsigned int i;

   if (!iso15924)
     return G_UNICODE_SCRIPT_INVALID_CODE;

  for (i = 0; i < G_N_ELEMENTS (iso15924_tags); i++)
    if (iso15924_tags[i] == iso15924)
      return (GUnicodeScript) i;

  return G_UNICODE_SCRIPT_UNKNOWN;
}
