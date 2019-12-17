/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "gdummyfile.h"
#include "gfile.h"


static void g_dummy_file_file_iface_init (GFileIface *iface);

typedef struct {
  char *scheme;
  char *userinfo;
  char *host;
  int port; /* -1 => not in uri */
  char *path;
  char *query;
  char *fragment;
} GDecodedUri;

struct _GDummyFile
{
  GObject parent_instance;

  GDecodedUri *decoded_uri;
  char *text_uri;
};

#define g_dummy_file_get_type _g_dummy_file_get_type
G_DEFINE_TYPE_WITH_CODE (GDummyFile, g_dummy_file, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE,
						g_dummy_file_file_iface_init))

#define SUB_DELIM_CHARS  "!$&'()*+,;="

static char *       _g_encode_uri       (GDecodedUri *decoded);
static void         _g_decoded_uri_free (GDecodedUri *decoded);
static GDecodedUri *_g_decode_uri       (const char  *uri);
static GDecodedUri *_g_decoded_uri_new  (void);

static char * unescape_string (const gchar *escaped_string,
			       const gchar *escaped_string_end,
			       const gchar *illegal_characters);

static void g_string_append_encoded (GString    *string, 
                                     const char *encoded,
				     const char *reserved_chars_allowed);

static void
g_dummy_file_finalize (GObject *object)
{
  GDummyFile *dummy;

  dummy = G_DUMMY_FILE (object);

  if (dummy->decoded_uri)
    _g_decoded_uri_free (dummy->decoded_uri);
  
  g_free (dummy->text_uri);

  G_OBJECT_CLASS (g_dummy_file_parent_class)->finalize (object);
}

static void
g_dummy_file_class_init (GDummyFileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_dummy_file_finalize;
}

static void
g_dummy_file_init (GDummyFile *dummy)
{
}

GFile *
_g_dummy_file_new (const char *uri)
{
  GDummyFile *dummy;

  g_return_val_if_fail (uri != NULL, NULL);

  dummy = g_object_new (G_TYPE_DUMMY_FILE, NULL);
  dummy->text_uri = g_strdup (uri);
  dummy->decoded_uri = _g_decode_uri (uri);
  
  return G_FILE (dummy);
}

static gboolean
g_dummy_file_is_native (GFile *file)
{
  return FALSE;
}

static char *
g_dummy_file_get_basename (GFile *file)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);
  
  if (dummy->decoded_uri)
    return g_path_get_basename (dummy->decoded_uri->path);
  return g_strdup (dummy->text_uri);
}

static char *
g_dummy_file_get_path (GFile *file)
{
  return NULL;
}

static char *
g_dummy_file_get_uri (GFile *file)
{
  return g_strdup (G_DUMMY_FILE (file)->text_uri);
}

static char *
g_dummy_file_get_parse_name (GFile *file)
{
  return g_strdup (G_DUMMY_FILE (file)->text_uri);
}

static GFile *
g_dummy_file_get_parent (GFile *file)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);
  GFile *parent;
  char *dirname;
  char *uri;
  GDecodedUri new_decoded_uri;

  if (dummy->decoded_uri == NULL ||
      g_strcmp0 (dummy->decoded_uri->path, "/") == 0)
    return NULL;

  dirname = g_path_get_dirname (dummy->decoded_uri->path);
  
  if (strcmp (dirname, ".") == 0)
    {
      g_free (dirname);
      return NULL;
    }
  
  new_decoded_uri = *dummy->decoded_uri;
  new_decoded_uri.path = dirname;
  uri = _g_encode_uri (&new_decoded_uri);
  g_free (dirname);
  
  parent = _g_dummy_file_new (uri);
  g_free (uri);
  
  return parent;
}

static GFile *
g_dummy_file_dup (GFile *file)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);

  return _g_dummy_file_new (dummy->text_uri);
}

static guint
g_dummy_file_hash (GFile *file)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);
  
  return g_str_hash (dummy->text_uri);
}

static gboolean
g_dummy_file_equal (GFile *file1,
		    GFile *file2)
{
  GDummyFile *dummy1 = G_DUMMY_FILE (file1);
  GDummyFile *dummy2 = G_DUMMY_FILE (file2);

  return g_str_equal (dummy1->text_uri, dummy2->text_uri);
}

static int
safe_strcmp (const char *a, 
             const char *b)
{
  if (a == NULL)
    a = "";
  if (b == NULL)
    b = "";

  return strcmp (a, b);
}

static gboolean
uri_same_except_path (GDecodedUri *a,
		      GDecodedUri *b)
{
  if (safe_strcmp (a->scheme, b->scheme) != 0)
    return FALSE;
  if (safe_strcmp (a->userinfo, b->userinfo) != 0)
    return FALSE;
  if (safe_strcmp (a->host, b->host) != 0)
    return FALSE;
  if (a->port != b->port)
    return FALSE;

  return TRUE;
}

static const char *
match_prefix (const char *path, 
              const char *prefix)
{
  int prefix_len;

  prefix_len = strlen (prefix);
  if (strncmp (path, prefix, prefix_len) != 0)
    return NULL;
  return path + prefix_len;
}

static gboolean
g_dummy_file_prefix_matches (GFile *parent, GFile *descendant)
{
  GDummyFile *parent_dummy = G_DUMMY_FILE (parent);
  GDummyFile *descendant_dummy = G_DUMMY_FILE (descendant);
  const char *remainder;

  if (parent_dummy->decoded_uri != NULL &&
      descendant_dummy->decoded_uri != NULL)
    {
      if (uri_same_except_path (parent_dummy->decoded_uri,
				descendant_dummy->decoded_uri)) 
        {
	  remainder = match_prefix (descendant_dummy->decoded_uri->path,
                                    parent_dummy->decoded_uri->path);
          if (remainder != NULL && *remainder == '/')
	    {
	      while (*remainder == '/')
	        remainder++;
	      if (*remainder != 0)
	        return TRUE;
	    }
        }
    }
  else
    {
      remainder = match_prefix (descendant_dummy->text_uri,
				parent_dummy->text_uri);
      if (remainder != NULL && *remainder == '/')
	  {
	    while (*remainder == '/')
	      remainder++;
	    if (*remainder != 0)
	      return TRUE;
	  }
    }
  
  return FALSE;
}

static char *
g_dummy_file_get_relative_path (GFile *parent,
				GFile *descendant)
{
  GDummyFile *parent_dummy = G_DUMMY_FILE (parent);
  GDummyFile *descendant_dummy = G_DUMMY_FILE (descendant);
  const char *remainder;

  if (parent_dummy->decoded_uri != NULL &&
      descendant_dummy->decoded_uri != NULL)
    {
      if (uri_same_except_path (parent_dummy->decoded_uri,
				descendant_dummy->decoded_uri)) 
        {
          remainder = match_prefix (descendant_dummy->decoded_uri->path,
                                    parent_dummy->decoded_uri->path);
          if (remainder != NULL && *remainder == '/')
	    {
	      while (*remainder == '/')
	        remainder++;
	      if (*remainder != 0)
	        return g_strdup (remainder);
	    }
        }
    }
  else
    {
      remainder = match_prefix (descendant_dummy->text_uri,
				parent_dummy->text_uri);
      if (remainder != NULL && *remainder == '/')
	  {
	    while (*remainder == '/')
	      remainder++;
	    if (*remainder != 0)
	      return unescape_string (remainder, NULL, "/");
	  }
    }
  
  return NULL;
}


static GFile *
g_dummy_file_resolve_relative_path (GFile      *file,
				    const char *relative_path)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);
  GFile *child;
  char *uri;
  GDecodedUri new_decoded_uri;
  GString *str;

  if (dummy->decoded_uri == NULL)
    {
      str = g_string_new (dummy->text_uri);
      g_string_append (str, "/");
      g_string_append_encoded (str, relative_path, SUB_DELIM_CHARS ":@/");
      child = _g_dummy_file_new (str->str);
      g_string_free (str, TRUE);
    }
  else
    {
      new_decoded_uri = *dummy->decoded_uri;
      
      if (g_path_is_absolute (relative_path))
	new_decoded_uri.path = g_strdup (relative_path);
      else
	new_decoded_uri.path = g_build_filename (new_decoded_uri.path, relative_path, NULL);
      
      uri = _g_encode_uri (&new_decoded_uri);
      g_free (new_decoded_uri.path);
      
      child = _g_dummy_file_new (uri);
      g_free (uri);
    }

  return child;
}

static GFile *
g_dummy_file_get_child_for_display_name (GFile        *file,
					 const char   *display_name,
					 GError      **error)
{
  return g_file_get_child (file, display_name);
}

static gboolean
g_dummy_file_has_uri_scheme (GFile *file,
			     const char *uri_scheme)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);
  
  if (dummy->decoded_uri)
    return g_ascii_strcasecmp (uri_scheme, dummy->decoded_uri->scheme) == 0;
  return FALSE;
}

static char *
g_dummy_file_get_uri_scheme (GFile *file)
{
  GDummyFile *dummy = G_DUMMY_FILE (file);

  if (dummy->decoded_uri)
    return g_strdup (dummy->decoded_uri->scheme);
    
  return NULL;
}


static void
g_dummy_file_file_iface_init (GFileIface *iface)
{
  iface->dup = g_dummy_file_dup;
  iface->hash = g_dummy_file_hash;
  iface->equal = g_dummy_file_equal;
  iface->is_native = g_dummy_file_is_native;
  iface->has_uri_scheme = g_dummy_file_has_uri_scheme;
  iface->get_uri_scheme = g_dummy_file_get_uri_scheme;
  iface->get_basename = g_dummy_file_get_basename;
  iface->get_path = g_dummy_file_get_path;
  iface->get_uri = g_dummy_file_get_uri;
  iface->get_parse_name = g_dummy_file_get_parse_name;
  iface->get_parent = g_dummy_file_get_parent;
  iface->prefix_matches = g_dummy_file_prefix_matches;
  iface->get_relative_path = g_dummy_file_get_relative_path;
  iface->resolve_relative_path = g_dummy_file_resolve_relative_path;
  iface->get_child_for_display_name = g_dummy_file_get_child_for_display_name;

  iface->supports_thread_contexts = TRUE;
}

/* Uri handling helper functions: */

static int
unescape_character (const char *scanner)
{
  int first_digit;
  int second_digit;
  
  first_digit = g_ascii_xdigit_value (*scanner++);
  if (first_digit < 0)
    return -1;

  second_digit = g_ascii_xdigit_value (*scanner++);
  if (second_digit < 0)
    return -1;

  return (first_digit << 4) | second_digit;
}

static char *
unescape_string (const gchar *escaped_string,
		 const gchar *escaped_string_end,
		 const gchar *illegal_characters)
{
  const gchar *in;
  gchar *out, *result;
  gint character;
  
  if (escaped_string == NULL)
    return NULL;

  if (escaped_string_end == NULL)
    escaped_string_end = escaped_string + strlen (escaped_string);
  
  result = g_malloc (escaped_string_end - escaped_string + 1);
	
  out = result;
  for (in = escaped_string; in < escaped_string_end; in++) 
    {
      character = *in;
      if (*in == '%') 
        {
          in++;
          if (escaped_string_end - in < 2)
	    {
	      g_free (result);
	      return NULL;
	    }
      
          character = unescape_character (in);
      
          /* Check for an illegal character. We consider '\0' illegal here. */
          if (character <= 0 ||
	      (illegal_characters != NULL &&
	       strchr (illegal_characters, (char)character) != NULL))
	    {
	      g_free (result);
	      return NULL;
	    }
          in++; /* The other char will be eaten in the loop header */
        }
      *out++ = (char)character;
    }
  
  *out = '\0';
  g_warn_if_fail (out - result <= strlen (escaped_string));
  return result;
}

void
_g_decoded_uri_free (GDecodedUri *decoded)
{
  if (decoded == NULL)
    return;

  g_free (decoded->scheme);
  g_free (decoded->query);
  g_free (decoded->fragment);
  g_free (decoded->userinfo);
  g_free (decoded->host);
  g_free (decoded->path);
  g_free (decoded);
}

GDecodedUri *
_g_decoded_uri_new (void)
{
  GDecodedUri *uri;

  uri = g_new0 (GDecodedUri, 1);
  uri->port = -1;

  return uri;
}

GDecodedUri *
_g_decode_uri (const char *uri)
{
  GDecodedUri *decoded;
  const char *p, *in, *hier_part_start, *hier_part_end, *query_start, *fragment_start;
  char *out;
  char c;

  /* From RFC 3986 Decodes:
   * URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
   */ 

  p = uri;
  
  /* Decode scheme:
     scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  */

  if (!g_ascii_isalpha (*p))
    return NULL;

  while (1)
    {
      c = *p++;

      if (c == ':')
	break;
      
      if (!(g_ascii_isalnum(c) ||
	    c == '+' ||
	    c == '-' ||
	    c == '.'))
	return NULL;
    }

  decoded = _g_decoded_uri_new ();
  
  decoded->scheme = g_malloc (p - uri);
  out = decoded->scheme;
  for (in = uri; in < p - 1; in++)
    *out++ = g_ascii_tolower (*in);
  *out = 0;

  hier_part_start = p;

  query_start = strchr (p, '?');
  if (query_start)
    {
      hier_part_end = query_start++;
      fragment_start = strchr (query_start, '#');
      if (fragment_start)
	{
	  decoded->query = g_strndup (query_start, fragment_start - query_start);
	  decoded->fragment = g_strdup (fragment_start+1);
	}
      else
	{
	  decoded->query = g_strdup (query_start);
	  decoded->fragment = NULL;
	}
    }
  else
    {
      /* No query */
      decoded->query = NULL;
      fragment_start = strchr (p, '#');
      if (fragment_start)
	{
	  hier_part_end = fragment_start++;
	  decoded->fragment = g_strdup (fragment_start);
	}
      else
	{
	  hier_part_end = p + strlen (p);
	  decoded->fragment = NULL;
	}
    }

  /*  3:
      hier-part   = "//" authority path-abempty
                  / path-absolute
                  / path-rootless
                  / path-empty

  */

  if (hier_part_start[0] == '/' &&
      hier_part_start[1] == '/')
    {
      const char *authority_start, *authority_end;
      const char *userinfo_start, *userinfo_end;
      const char *host_start, *host_end;
      const char *port_start;
      
      authority_start = hier_part_start + 2;
      /* authority is always followed by / or nothing */
      authority_end = memchr (authority_start, '/', hier_part_end - authority_start);
      if (authority_end == NULL)
	authority_end = hier_part_end;

      /* 3.2:
	      authority   = [ userinfo "@" ] host [ ":" port ]
      */

      userinfo_end = memchr (authority_start, '@', authority_end - authority_start);
      if (userinfo_end)
	{
	  userinfo_start = authority_start;
	  decoded->userinfo = unescape_string (userinfo_start, userinfo_end, NULL);
	  if (decoded->userinfo == NULL)
	    {
	      _g_decoded_uri_free (decoded);
	      return NULL;
	    }
	  host_start = userinfo_end + 1;
	}
      else
	host_start = authority_start;

      port_start = memchr (host_start, ':', authority_end - host_start);
      if (port_start)
	{
	  host_end = port_start++;

	  decoded->port = atoi(port_start);
	}
      else
	{
	  host_end = authority_end;
	  decoded->port = -1;
	}

      decoded->host = g_strndup (host_start, host_end - host_start);

      hier_part_start = authority_end;
    }

  decoded->path = unescape_string (hier_part_start, hier_part_end, "/");

  if (decoded->path == NULL)
    {
      _g_decoded_uri_free (decoded);
      return NULL;
    }
  
  return decoded;
}

static gboolean
is_valid (char c, const char *reserved_chars_allowed)
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

static void
g_string_append_encoded (GString    *string,
                         const char *encoded,
			 const char *reserved_chars_allowed)
{
  unsigned char c;
  static const gchar hex[16] = "0123456789ABCDEF";

  while ((c = *encoded) != 0)
    {
      if (is_valid (c, reserved_chars_allowed))
	{
	  g_string_append_c (string, c);
	  encoded++;
	}
      else
	{
	  g_string_append_c (string, '%');
	  g_string_append_c (string, hex[((guchar)c) >> 4]);
	  g_string_append_c (string, hex[((guchar)c) & 0xf]);
	  encoded++;
	}
    }
}

static char *
_g_encode_uri (GDecodedUri *decoded)
{
  GString *uri;

  uri = g_string_new (NULL);

  g_string_append (uri, decoded->scheme);
  g_string_append (uri, "://");

  if (decoded->host != NULL)
    {
      if (decoded->userinfo)
	{
	  /* userinfo    = *( unreserved / pct-encoded / sub-delims / ":" ) */
	  g_string_append_encoded (uri, decoded->userinfo, SUB_DELIM_CHARS ":");
	  g_string_append_c (uri, '@');
	}
      
      g_string_append (uri, decoded->host);
      
      if (decoded->port != -1)
	{
	  g_string_append_c (uri, ':');
	  g_string_append_printf (uri, "%d", decoded->port);
	}
    }

  g_string_append_encoded (uri, decoded->path, SUB_DELIM_CHARS ":@/");
  
  if (decoded->query)
    {
      g_string_append_c (uri, '?');
      g_string_append (uri, decoded->query);
    }
    
  if (decoded->fragment)
    {
      g_string_append_c (uri, '#');
      g_string_append (uri, decoded->fragment);
    }

  return g_string_free (uri, FALSE);
}
