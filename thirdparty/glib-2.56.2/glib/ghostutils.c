/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2008 Red Hat, Inc.
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
 */

#include "config.h"

#include <string.h>

#include "ghostutils.h"

#include "garray.h"
#include "gmem.h"
#include "gstring.h"
#include "gstrfuncs.h"
#include "glibintl.h"


/**
 * SECTION:ghostutils
 * @short_description: Internet hostname utilities
 *
 * Functions for manipulating internet hostnames; in particular, for
 * converting between Unicode and ASCII-encoded forms of
 * Internationalized Domain Names (IDNs).
 *
 * The
 * [Internationalized Domain Names for Applications (IDNA)](http://www.ietf.org/rfc/rfc3490.txt)
 * standards allow for the use
 * of Unicode domain names in applications, while providing
 * backward-compatibility with the old ASCII-only DNS, by defining an
 * ASCII-Compatible Encoding of any given Unicode name, which can be
 * used with non-IDN-aware applications and protocols. (For example,
 * "Παν語.org" maps to "xn--4wa8awb4637h.org".)
 **/

#define IDNA_ACE_PREFIX     "xn--"
#define IDNA_ACE_PREFIX_LEN 4

/* Punycode constants, from RFC 3492. */

#define PUNYCODE_BASE          36
#define PUNYCODE_TMIN           1
#define PUNYCODE_TMAX          26
#define PUNYCODE_SKEW          38
#define PUNYCODE_DAMP         700
#define PUNYCODE_INITIAL_BIAS  72
#define PUNYCODE_INITIAL_N   0x80

#define PUNYCODE_IS_BASIC(cp) ((guint)(cp) < 0x80)

/* Encode/decode a single base-36 digit */
static inline gchar
encode_digit (guint dig)
{
  if (dig < 26)
    return dig + 'a';
  else
    return dig - 26 + '0';
}

static inline guint
decode_digit (gchar dig)
{
  if (dig >= 'A' && dig <= 'Z')
    return dig - 'A';
  else if (dig >= 'a' && dig <= 'z')
    return dig - 'a';
  else if (dig >= '0' && dig <= '9')
    return dig - '0' + 26;
  else
    return G_MAXUINT;
}

/* Punycode bias adaptation algorithm, RFC 3492 section 6.1 */
static guint
adapt (guint    delta,
       guint    numpoints,
       gboolean firsttime)
{
  guint k;

  delta = firsttime ? delta / PUNYCODE_DAMP : delta / 2;
  delta += delta / numpoints;

  k = 0;
  while (delta > ((PUNYCODE_BASE - PUNYCODE_TMIN) * PUNYCODE_TMAX) / 2)
    {
      delta /= PUNYCODE_BASE - PUNYCODE_TMIN;
      k += PUNYCODE_BASE;
    }

  return k + ((PUNYCODE_BASE - PUNYCODE_TMIN + 1) * delta /
	      (delta + PUNYCODE_SKEW));
}

/* Punycode encoder, RFC 3492 section 6.3. The algorithm is
 * sufficiently bizarre that it's not really worth trying to explain
 * here.
 */
static gboolean
punycode_encode (const gchar *input_utf8,
                 gsize        input_utf8_length,
		 GString     *output)
{
  guint delta, handled_chars, num_basic_chars, bias, j, q, k, t, digit;
  gunichar n, m, *input;
  glong input_length;
  gboolean success = FALSE;

  /* Convert from UTF-8 to Unicode code points */
  input = g_utf8_to_ucs4 (input_utf8, input_utf8_length, NULL,
			  &input_length, NULL);
  if (!input)
    return FALSE;

  /* Copy basic chars */
  for (j = num_basic_chars = 0; j < input_length; j++)
    {
      if (PUNYCODE_IS_BASIC (input[j]))
	{
	  g_string_append_c (output, g_ascii_tolower (input[j]));
	  num_basic_chars++;
	}
    }
  if (num_basic_chars)
    g_string_append_c (output, '-');

  handled_chars = num_basic_chars;

  /* Encode non-basic chars */
  delta = 0;
  bias = PUNYCODE_INITIAL_BIAS;
  n = PUNYCODE_INITIAL_N;
  while (handled_chars < input_length)
    {
      /* let m = the minimum {non-basic} code point >= n in the input */
      for (m = G_MAXUINT, j = 0; j < input_length; j++)
	{
	  if (input[j] >= n && input[j] < m)
	    m = input[j];
	}

      if (m - n > (G_MAXUINT - delta) / (handled_chars + 1))
	goto fail;
      delta += (m - n) * (handled_chars + 1);
      n = m;

      for (j = 0; j < input_length; j++)
	{
	  if (input[j] < n)
	    {
	      if (++delta == 0)
		goto fail;
	    }
	  else if (input[j] == n)
	    {
	      q = delta;
	      for (k = PUNYCODE_BASE; ; k += PUNYCODE_BASE)
		{
		  if (k <= bias)
		    t = PUNYCODE_TMIN;
		  else if (k >= bias + PUNYCODE_TMAX)
		    t = PUNYCODE_TMAX;
		  else
		    t = k - bias;
		  if (q < t)
		    break;
		  digit = t + (q - t) % (PUNYCODE_BASE - t);
		  g_string_append_c (output, encode_digit (digit));
		  q = (q - t) / (PUNYCODE_BASE - t);
		}

	      g_string_append_c (output, encode_digit (q));
	      bias = adapt (delta, handled_chars + 1, handled_chars == num_basic_chars);
	      delta = 0;
	      handled_chars++;
	    }
	}

      delta++;
      n++;
    }

  success = TRUE;

 fail:
  g_free (input);
  return success;
}

/* From RFC 3454, Table B.1 */
#define idna_is_junk(ch) ((ch) == 0x00AD || (ch) == 0x1806 || (ch) == 0x200B || (ch) == 0x2060 || (ch) == 0xFEFF || (ch) == 0x034F || (ch) == 0x180B || (ch) == 0x180C || (ch) == 0x180D || (ch) == 0x200C || (ch) == 0x200D || ((ch) >= 0xFE00 && (ch) <= 0xFE0F))

/* Scan @str for "junk" and return a cleaned-up string if any junk
 * is found. Else return %NULL.
 */
static gchar *
remove_junk (const gchar *str,
             gint         len)
{
  GString *cleaned = NULL;
  const gchar *p;
  gunichar ch;

  for (p = str; len == -1 ? *p : p < str + len; p = g_utf8_next_char (p))
    {
      ch = g_utf8_get_char (p);
      if (idna_is_junk (ch))
	{
	  if (!cleaned)
	    {
	      cleaned = g_string_new (NULL);
	      g_string_append_len (cleaned, str, p - str);
	    }
	}
      else if (cleaned)
	g_string_append_unichar (cleaned, ch);
    }

  if (cleaned)
    return g_string_free (cleaned, FALSE);
  else
    return NULL;
}

static inline gboolean
contains_uppercase_letters (const gchar *str,
                            gint         len)
{
  const gchar *p;

  for (p = str; len == -1 ? *p : p < str + len; p = g_utf8_next_char (p))
    {
      if (g_unichar_isupper (g_utf8_get_char (p)))
	return TRUE;
    }
  return FALSE;
}

static inline gboolean
contains_non_ascii (const gchar *str,
                    gint         len)
{
  const gchar *p;

  for (p = str; len == -1 ? *p : p < str + len; p++)
    {
      if ((guchar)*p > 0x80)
	return TRUE;
    }
  return FALSE;
}

/* RFC 3454, Appendix C. ish. */
static inline gboolean
idna_is_prohibited (gunichar ch)
{
  switch (g_unichar_type (ch))
    {
    case G_UNICODE_CONTROL:
    case G_UNICODE_FORMAT:
    case G_UNICODE_UNASSIGNED:
    case G_UNICODE_PRIVATE_USE:
    case G_UNICODE_SURROGATE:
    case G_UNICODE_LINE_SEPARATOR:
    case G_UNICODE_PARAGRAPH_SEPARATOR:
    case G_UNICODE_SPACE_SEPARATOR:
      return TRUE;

    case G_UNICODE_OTHER_SYMBOL:
      if (ch == 0xFFFC || ch == 0xFFFD ||
	  (ch >= 0x2FF0 && ch <= 0x2FFB))
	return TRUE;
      return FALSE;

    case G_UNICODE_NON_SPACING_MARK:
      if (ch == 0x0340 || ch == 0x0341)
	return TRUE;
      return FALSE;

    default:
      return FALSE;
    }
}

/* RFC 3491 IDN cleanup algorithm. */
static gchar *
nameprep (const gchar *hostname,
          gint         len,
          gboolean    *is_unicode)
{
  gchar *name, *tmp = NULL, *p;

  /* It would be nice if we could do this without repeatedly
   * allocating strings and converting back and forth between
   * gunichars and UTF-8... The code does at least avoid doing most of
   * the sub-operations when they would just be equivalent to a
   * g_strdup().
   */

  /* Remove presentation-only characters */
  name = remove_junk (hostname, len);
  if (name)
    {
      tmp = name;
      len = -1;
    }
  else
    name = (gchar *)hostname;

  /* Convert to lowercase */
  if (contains_uppercase_letters (name, len))
    {
      name = g_utf8_strdown (name, len);
      g_free (tmp);
      tmp = name;
      len = -1;
    }

  /* If there are no UTF8 characters, we're done. */
  if (!contains_non_ascii (name, len))
    {
      *is_unicode = FALSE;
      if (name == (gchar *)hostname)
        return len == -1 ? g_strdup (hostname) : g_strndup (hostname, len);
      else
        return name;
    }

  *is_unicode = TRUE;

  /* Normalize */
  name = g_utf8_normalize (name, len, G_NORMALIZE_NFKC);
  g_free (tmp);
  tmp = name;

  if (!name)
    return NULL;

  /* KC normalization may have created more capital letters (eg,
   * angstrom -> capital A with ring). So we have to lowercasify a
   * second time. (This is more-or-less how the nameprep algorithm
   * does it. If tolower(nfkc(tolower(X))) is guaranteed to be the
   * same as tolower(nfkc(X)), then we could skip the first tolower,
   * but I'm not sure it is.)
   */
  if (contains_uppercase_letters (name, -1))
    {
      name = g_utf8_strdown (name, -1);
      g_free (tmp);
      tmp = name;
    }

  /* Check for prohibited characters */
  for (p = name; *p; p = g_utf8_next_char (p))
    {
      if (idna_is_prohibited (g_utf8_get_char (p)))
	{
	  name = NULL;
          g_free (tmp);
	  goto done;
	}
    }

  /* FIXME: We're supposed to verify certain constraints on bidi
   * characters, but glib does not appear to have that information.
   */

 done:
  return name;
}

/* RFC 3490, section 3.1 says '.', 0x3002, 0xFF0E, and 0xFF61 count as
 * label-separating dots. @str must be '\0'-terminated.
 */
#define idna_is_dot(str) ( \
  ((guchar)(str)[0] == '.') ||                                                 \
  ((guchar)(str)[0] == 0xE3 && (guchar)(str)[1] == 0x80 && (guchar)(str)[2] == 0x82) || \
  ((guchar)(str)[0] == 0xEF && (guchar)(str)[1] == 0xBC && (guchar)(str)[2] == 0x8E) || \
  ((guchar)(str)[0] == 0xEF && (guchar)(str)[1] == 0xBD && (guchar)(str)[2] == 0xA1) )

static const gchar *
idna_end_of_label (const gchar *str)
{
  for (; *str; str = g_utf8_next_char (str))
    {
      if (idna_is_dot (str))
        return str;
    }
  return str;
}

/**
 * g_hostname_to_ascii:
 * @hostname: a valid UTF-8 or ASCII hostname
 *
 * Converts @hostname to its canonical ASCII form; an ASCII-only
 * string containing no uppercase letters and not ending with a
 * trailing dot.
 *
 * Returns: an ASCII hostname, which must be freed, or %NULL if
 * @hostname is in some way invalid.
 *
 * Since: 2.22
 **/
gchar *
g_hostname_to_ascii (const gchar *hostname)
{
  gchar *name, *label, *p;
  GString *out;
  gssize llen, oldlen;
  gboolean unicode;

  label = name = nameprep (hostname, -1, &unicode);
  if (!name || !unicode)
    return name;

  out = g_string_new (NULL);

  do
    {
      unicode = FALSE;
      for (p = label; *p && !idna_is_dot (p); p++)
	{
	  if ((guchar)*p > 0x80)
	    unicode = TRUE;
	}

      oldlen = out->len;
      llen = p - label;
      if (unicode)
	{
          if (!strncmp (label, IDNA_ACE_PREFIX, IDNA_ACE_PREFIX_LEN))
            goto fail;

	  g_string_append (out, IDNA_ACE_PREFIX);
	  if (!punycode_encode (label, llen, out))
	    goto fail;
	}
      else
        g_string_append_len (out, label, llen);

      if (out->len - oldlen > 63)
	goto fail;

      label += llen;
      if (*label)
        label = g_utf8_next_char (label);
      if (*label)
        g_string_append_c (out, '.');
    }
  while (*label);

  g_free (name);
  return g_string_free (out, FALSE);

 fail:
  g_free (name);
  g_string_free (out, TRUE);
  return NULL;
}

/**
 * g_hostname_is_non_ascii:
 * @hostname: a hostname
 *
 * Tests if @hostname contains Unicode characters. If this returns
 * %TRUE, you need to encode the hostname with g_hostname_to_ascii()
 * before using it in non-IDN-aware contexts.
 *
 * Note that a hostname might contain a mix of encoded and unencoded
 * segments, and so it is possible for g_hostname_is_non_ascii() and
 * g_hostname_is_ascii_encoded() to both return %TRUE for a name.
 *
 * Returns: %TRUE if @hostname contains any non-ASCII characters
 *
 * Since: 2.22
 **/
gboolean
g_hostname_is_non_ascii (const gchar *hostname)
{
  return contains_non_ascii (hostname, -1);
}

/* Punycode decoder, RFC 3492 section 6.2. As with punycode_encode(),
 * read the RFC if you want to understand what this is actually doing.
 */
static gboolean
punycode_decode (const gchar *input,
                 gsize        input_length,
                 GString     *output)
{
  GArray *output_chars;
  gunichar n;
  guint i, bias;
  guint oldi, w, k, digit, t;
  const gchar *split;

  n = PUNYCODE_INITIAL_N;
  i = 0;
  bias = PUNYCODE_INITIAL_BIAS;

  split = input + input_length - 1;
  while (split > input && *split != '-')
    split--;
  if (split > input)
    {
      output_chars = g_array_sized_new (FALSE, FALSE, sizeof (gunichar),
					split - input);
      input_length -= (split - input) + 1;
      while (input < split)
	{
	  gunichar ch = (gunichar)*input++;
	  if (!PUNYCODE_IS_BASIC (ch))
	    goto fail;
	  g_array_append_val (output_chars, ch);
	}
      input++;
    }
  else
    output_chars = g_array_new (FALSE, FALSE, sizeof (gunichar));

  while (input_length)
    {
      oldi = i;
      w = 1;
      for (k = PUNYCODE_BASE; ; k += PUNYCODE_BASE)
	{
	  if (!input_length--)
	    goto fail;
	  digit = decode_digit (*input++);
	  if (digit >= PUNYCODE_BASE)
	    goto fail;
	  if (digit > (G_MAXUINT - i) / w)
	    goto fail;
	  i += digit * w;
	  if (k <= bias)
	    t = PUNYCODE_TMIN;
	  else if (k >= bias + PUNYCODE_TMAX)
	    t = PUNYCODE_TMAX;
	  else
	    t = k - bias;
	  if (digit < t)
	    break;
	  if (w > G_MAXUINT / (PUNYCODE_BASE - t))
	    goto fail;
	  w *= (PUNYCODE_BASE - t);
	}

      bias = adapt (i - oldi, output_chars->len + 1, oldi == 0);

      if (i / (output_chars->len + 1) > G_MAXUINT - n)
	goto fail;
      n += i / (output_chars->len + 1);
      i %= (output_chars->len + 1);

      g_array_insert_val (output_chars, i++, n);
    }

  for (i = 0; i < output_chars->len; i++)
    g_string_append_unichar (output, g_array_index (output_chars, gunichar, i));
  g_array_free (output_chars, TRUE);
  return TRUE;

 fail:
  g_array_free (output_chars, TRUE);
  return FALSE;
}

/**
 * g_hostname_to_unicode:
 * @hostname: a valid UTF-8 or ASCII hostname
 *
 * Converts @hostname to its canonical presentation form; a UTF-8
 * string in Unicode normalization form C, containing no uppercase
 * letters, no forbidden characters, and no ASCII-encoded segments,
 * and not ending with a trailing dot.
 *
 * Of course if @hostname is not an internationalized hostname, then
 * the canonical presentation form will be entirely ASCII.
 *
 * Returns: a UTF-8 hostname, which must be freed, or %NULL if
 * @hostname is in some way invalid.
 *
 * Since: 2.22
 **/
gchar *
g_hostname_to_unicode (const gchar *hostname)
{
  GString *out;
  gssize llen;

  out = g_string_new (NULL);

  do
    {
      llen = idna_end_of_label (hostname) - hostname;
      if (!g_ascii_strncasecmp (hostname, IDNA_ACE_PREFIX, IDNA_ACE_PREFIX_LEN))
	{
	  hostname += IDNA_ACE_PREFIX_LEN;
	  llen -= IDNA_ACE_PREFIX_LEN;
	  if (!punycode_decode (hostname, llen, out))
	    {
	      g_string_free (out, TRUE);
	      return NULL;
	    }
	}
      else
        {
          gboolean unicode;
          gchar *canonicalized = nameprep (hostname, llen, &unicode);

          if (!canonicalized)
            {
              g_string_free (out, TRUE);
              return NULL;
            }
          g_string_append (out, canonicalized);
          g_free (canonicalized);
        }

      hostname += llen;
      if (*hostname)
        hostname = g_utf8_next_char (hostname);
      if (*hostname)
        g_string_append_c (out, '.');
    }
  while (*hostname);

  return g_string_free (out, FALSE);
}

/**
 * g_hostname_is_ascii_encoded:
 * @hostname: a hostname
 *
 * Tests if @hostname contains segments with an ASCII-compatible
 * encoding of an Internationalized Domain Name. If this returns
 * %TRUE, you should decode the hostname with g_hostname_to_unicode()
 * before displaying it to the user.
 *
 * Note that a hostname might contain a mix of encoded and unencoded
 * segments, and so it is possible for g_hostname_is_non_ascii() and
 * g_hostname_is_ascii_encoded() to both return %TRUE for a name.
 *
 * Returns: %TRUE if @hostname contains any ASCII-encoded
 * segments.
 *
 * Since: 2.22
 **/
gboolean
g_hostname_is_ascii_encoded (const gchar *hostname)
{
  while (1)
    {
      if (!g_ascii_strncasecmp (hostname, IDNA_ACE_PREFIX, IDNA_ACE_PREFIX_LEN))
	return TRUE;
      hostname = idna_end_of_label (hostname);
      if (*hostname)
        hostname = g_utf8_next_char (hostname);
      if (!*hostname)
	return FALSE;
    }
}

/**
 * g_hostname_is_ip_address:
 * @hostname: a hostname (or IP address in string form)
 *
 * Tests if @hostname is the string form of an IPv4 or IPv6 address.
 * (Eg, "192.168.0.1".)
 *
 * Returns: %TRUE if @hostname is an IP address
 *
 * Since: 2.22
 **/
gboolean
g_hostname_is_ip_address (const gchar *hostname)
{
  gchar *p, *end;
  gint nsegments, octet;

  /* On Linux we could implement this using inet_pton, but the Windows
   * equivalent of that requires linking against winsock, so we just
   * figure this out ourselves. Tested by tests/hostutils.c.
   */

  p = (char *)hostname;

  if (strchr (p, ':'))
    {
      gboolean skipped;

      /* If it contains a ':', it's an IPv6 address (assuming it's an
       * IP address at all). This consists of eight ':'-separated
       * segments, each containing a 1-4 digit hex number, except that
       * optionally: (a) the last two segments can be replaced by an
       * IPv4 address, and (b) a single span of 1 to 8 "0000" segments
       * can be replaced with just "::".
       */

      nsegments = 0;
      skipped = FALSE;
      while (*p && nsegments < 8)
        {
          /* Each segment after the first must be preceded by a ':'.
           * (We also handle half of the "string starts with ::" case
           * here.)
           */
          if (p != (char *)hostname || (p[0] == ':' && p[1] == ':'))
            {
              if (*p != ':')
                return FALSE;
              p++;
            }

          /* If there's another ':', it means we're skipping some segments */
          if (*p == ':' && !skipped)
            {
              skipped = TRUE;
              nsegments++;

              /* Handle the "string ends with ::" case */
              if (!p[1])
                p++;

              continue;
            }

          /* Read the segment, make sure it's valid. */
          for (end = p; g_ascii_isxdigit (*end); end++)
            ;
          if (end == p || end > p + 4)
            return FALSE;

          if (*end == '.')
            {
              if ((nsegments == 6 && !skipped) || (nsegments <= 6 && skipped))
                goto parse_ipv4;
              else
                return FALSE;
            }

          nsegments++;
          p = end;
        }

      return !*p && (nsegments == 8 || skipped);
    }

 parse_ipv4:

  /* Parse IPv4: N.N.N.N, where each N <= 255 and doesn't have leading 0s. */
  for (nsegments = 0; nsegments < 4; nsegments++)
    {
      if (nsegments != 0)
        {
          if (*p != '.')
            return FALSE;
          p++;
        }

      /* Check the segment; a little tricker than the IPv6 case since
       * we can't allow extra leading 0s, and we can't assume that all
       * strings of valid length are within range.
       */
      octet = 0;
      if (*p == '0')
        end = p + 1;
      else
        {
          for (end = p; g_ascii_isdigit (*end); end++)
            {
              octet = 10 * octet + (*end - '0');

              if (octet > 255)
                break;
            }
        }
      if (end == p || end > p + 3 || octet > 255)
        return FALSE;

      p = end;
    }

  /* If there's nothing left to parse, then it's ok. */
  return !*p;
}
