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

#include <string.h>

#include "gresource.h"
#include "gresourcefile.h"
#include "gfileattribute.h"
#include <gfileattribute-priv.h>
#include <gfileinfo-priv.h>
#include "gfile.h"
#include "gfilemonitor.h"
#include "gseekable.h"
#include "gfileinputstream.h"
#include "gfileinfo.h"
#include "gfileenumerator.h"
#include "gcontenttype.h"
#include "gioerror.h"
#include <glib/gstdio.h>
#include "glibintl.h"

struct _GResourceFile
{
  GObject parent_instance;

  char *path;
};

struct _GResourceFileEnumerator
{
  GFileEnumerator parent;

  GFileAttributeMatcher *matcher;
  char *path;
  char *attributes;
  GFileQueryInfoFlags flags;
  int index;

  char **children;
};

struct _GResourceFileEnumeratorClass
{
  GFileEnumeratorClass parent_class;
};

typedef struct _GResourceFileEnumerator        GResourceFileEnumerator;
typedef struct _GResourceFileEnumeratorClass   GResourceFileEnumeratorClass;

static void g_resource_file_file_iface_init (GFileIface *iface);

static GFileAttributeInfoList *resource_writable_attributes = NULL;
static GFileAttributeInfoList *resource_writable_namespaces = NULL;

static GType _g_resource_file_enumerator_get_type (void);

#define G_TYPE_RESOURCE_FILE_ENUMERATOR         (_g_resource_file_enumerator_get_type ())
#define G_RESOURCE_FILE_ENUMERATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_RESOURCE_FILE_ENUMERATOR, GResourceFileEnumerator))
#define G_RESOURCE_FILE_ENUMERATOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_RESOURCE_FILE_ENUMERATOR, GResourceFileEnumeratorClass))
#define G_IS_RESOURCE_FILE_ENUMERATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_RESOURCE_FILE_ENUMERATOR))
#define G_IS_RESOURCE_FILE_ENUMERATOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_RESOURCE_FILE_ENUMERATOR))
#define G_RESOURCE_FILE_ENUMERATOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_RESOURCE_FILE_ENUMERATOR, GResourceFileEnumeratorClass))

#define G_TYPE_RESOURCE_FILE_INPUT_STREAM         (_g_resource_file_input_stream_get_type ())
#define G_RESOURCE_FILE_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_RESOURCE_FILE_INPUT_STREAM, GResourceFileInputStream))
#define G_RESOURCE_FILE_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_RESOURCE_FILE_INPUT_STREAM, GResourceFileInputStreamClass))
#define G_IS_RESOURCE_FILE_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_RESOURCE_FILE_INPUT_STREAM))
#define G_IS_RESOURCE_FILE_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_RESOURCE_FILE_INPUT_STREAM))
#define G_RESOURCE_FILE_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_RESOURCE_FILE_INPUT_STREAM, GResourceFileInputStreamClass))

typedef struct _GResourceFileInputStream         GResourceFileInputStream;
typedef struct _GResourceFileInputStreamClass    GResourceFileInputStreamClass;

#define g_resource_file_get_type _g_resource_file_get_type
G_DEFINE_TYPE_WITH_CODE (GResourceFile, g_resource_file, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE,
						g_resource_file_file_iface_init))

#define g_resource_file_enumerator_get_type _g_resource_file_enumerator_get_type
G_DEFINE_TYPE (GResourceFileEnumerator, g_resource_file_enumerator, G_TYPE_FILE_ENUMERATOR)

static GFileEnumerator *_g_resource_file_enumerator_new (GResourceFile *file,
							 const char           *attributes,
							 GFileQueryInfoFlags   flags,
							 GCancellable         *cancellable,
							 GError              **error);


static GType              _g_resource_file_input_stream_get_type (void) G_GNUC_CONST;

static GFileInputStream *_g_resource_file_input_stream_new (GInputStream *stream, GFile *file);


static void
g_resource_file_finalize (GObject *object)
{
  GResourceFile *resource;

  resource = G_RESOURCE_FILE (object);

  g_free (resource->path);

  G_OBJECT_CLASS (g_resource_file_parent_class)->finalize (object);
}

static void
g_resource_file_class_init (GResourceFileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_resource_file_finalize;

  resource_writable_attributes = g_file_attribute_info_list_new ();
  resource_writable_namespaces = g_file_attribute_info_list_new ();
}

static void
g_resource_file_init (GResourceFile *resource)
{
}

static inline gchar *
scan_backwards (const gchar *begin,
                const gchar *end,
                gchar        c)
{
  while (end >= begin)
    {
      if (*end == c)
        return (gchar *)end;
      end--;
    }

  return NULL;
}

static inline void
pop_to_previous_part (const gchar  *begin,
                      gchar       **out)
{
  if (*out > begin)
    *out = scan_backwards (begin, *out - 1, '/');
}

/*
 * canonicalize_filename:
 * @in: the path to be canonicalized
 *
 * The path @in may contain non-canonical path pieces such as "../"
 * or duplicated "/". This will resolve those into a form that only
 * contains a single / at a time and resolves all "../". The resulting
 * path must also start with a /.
 *
 * Returns: the canonical form of the path
 */
static char *
canonicalize_filename (const char *in)
{
  gchar *bptr;
  char *out;

  bptr = out = g_malloc (strlen (in) + 2);
  *out = '/';

  while (*in != 0)
    {
      g_assert (*out == '/');

      /* move past slashes */
      while (*in == '/')
        in++;

      /* Handle ./ ../ .\0 ..\0 */
      if (*in == '.')
        {
          /* If this is ../ or ..\0 move up */
          if (in[1] == '.' && (in[2] == '/' || in[2] == 0))
            {
              pop_to_previous_part (bptr, &out);
              in += 2;
              continue;
            }

          /* If this is ./ skip past it */
          if (in[1] == '/' || in[1] == 0)
            {
              in += 1;
              continue;
            }
        }

      /* Scan to the next path piece */
      while (*in != 0 && *in != '/')
        *(++out) = *(in++);

      /* Add trailing /, compress the rest on the next go round. */
      if (*in == '/')
        *(++out) = *(in++);
    }

  /* Trim trailing / from path */
  if (out > bptr && *out == '/')
    *out = 0;
  else
    *(++out) = 0;

  return bptr;
}

static GFile *
g_resource_file_new_for_path (const char *path)
{
  GResourceFile *resource = g_object_new (G_TYPE_RESOURCE_FILE, NULL);

  resource->path = canonicalize_filename (path);

  return G_FILE (resource);
}

GFile *
_g_resource_file_new (const char *uri)
{
  GFile *resource;
  char *path;

  path = g_uri_unescape_string (uri + strlen ("resource:"), NULL);
  resource = g_resource_file_new_for_path (path);
  g_free (path);

  return G_FILE (resource);
}

static gboolean
g_resource_file_is_native (GFile *file)
{
  return FALSE;
}

static gboolean
g_resource_file_has_uri_scheme (GFile      *file,
				const char *uri_scheme)
{
  return g_ascii_strcasecmp (uri_scheme, "resource") == 0;
}

static char *
g_resource_file_get_uri_scheme (GFile *file)
{
  return g_strdup ("resource");
}

static char *
g_resource_file_get_basename (GFile *file)
{
  gchar *base;

  base = strrchr (G_RESOURCE_FILE (file)->path, '/');
  return g_strdup (base + 1);
}

static char *
g_resource_file_get_path (GFile *file)
{
  return NULL;
}

static char *
g_resource_file_get_uri (GFile *file)
{
  char *escaped, *res;
  escaped = g_uri_escape_string (G_RESOURCE_FILE (file)->path, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  res = g_strconcat ("resource://", escaped, NULL);
  g_free (escaped);
  return res;
}

static char *
g_resource_file_get_parse_name (GFile *file)
{
  return g_resource_file_get_uri (file);
}

static GFile *
g_resource_file_get_parent (GFile *file)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);
  GResourceFile *parent;
  gchar *end;

  end = strrchr (resource->path, '/');

  if (end == G_RESOURCE_FILE (file)->path)
    return NULL;

  parent = g_object_new (G_TYPE_RESOURCE_FILE, NULL);
  parent->path = g_strndup (resource->path,
			    end - resource->path);

  return G_FILE (parent);
}

static GFile *
g_resource_file_dup (GFile *file)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);

  return g_resource_file_new_for_path (resource->path);
}

static guint
g_resource_file_hash (GFile *file)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);

  return g_str_hash (resource->path);
}

static gboolean
g_resource_file_equal (GFile *file1,
		       GFile *file2)
{
  GResourceFile *resource1 = G_RESOURCE_FILE (file1);
  GResourceFile *resource2 = G_RESOURCE_FILE (file2);

  return g_str_equal (resource1->path, resource2->path);
}

static const char *
match_prefix (const char *path,
	      const char *prefix)
{
  int prefix_len;

  prefix_len = strlen (prefix);
  if (strncmp (path, prefix, prefix_len) != 0)
    return NULL;

  /* Handle the case where prefix is the root, so that
   * the IS_DIR_SEPRARATOR check below works */
  if (prefix_len > 0 &&
      prefix[prefix_len-1] == '/')
    prefix_len--;

  return path + prefix_len;
}

static gboolean
g_resource_file_prefix_matches (GFile *parent,
				GFile *descendant)
{
  GResourceFile *parent_resource = G_RESOURCE_FILE (parent);
  GResourceFile *descendant_resource = G_RESOURCE_FILE (descendant);
  const char *remainder;

  remainder = match_prefix (descendant_resource->path, parent_resource->path);
  if (remainder != NULL && *remainder == '/')
    return TRUE;
  return FALSE;
}

static char *
g_resource_file_get_relative_path (GFile *parent,
				   GFile *descendant)
{
  GResourceFile *parent_resource = G_RESOURCE_FILE (parent);
  GResourceFile *descendant_resource = G_RESOURCE_FILE (descendant);
  const char *remainder;

  remainder = match_prefix (descendant_resource->path, parent_resource->path);

  if (remainder != NULL && *remainder == '/')
    return g_strdup (remainder + 1);
  return NULL;
}

static GFile *
g_resource_file_resolve_relative_path (GFile      *file,
				       const char *relative_path)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);
  char *filename;
  GFile *child;

  if (relative_path[0] == '/')
    return g_resource_file_new_for_path (relative_path);

  filename = g_build_path ("/", resource->path, relative_path, NULL);
  child = g_resource_file_new_for_path (filename);
  g_free (filename);

  return child;
}

static GFileEnumerator *
g_resource_file_enumerate_children (GFile                *file,
				    const char           *attributes,
				    GFileQueryInfoFlags   flags,
				    GCancellable         *cancellable,
				    GError              **error)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);
  return _g_resource_file_enumerator_new (resource,
					  attributes, flags,
					  cancellable, error);
}

static GFile *
g_resource_file_get_child_for_display_name (GFile        *file,
					    const char   *display_name,
					    GError      **error)
{
  GFile *new_file;

  new_file = g_file_get_child (file, display_name);

  return new_file;
}

static GFileInfo *
g_resource_file_query_info (GFile                *file,
			    const char           *attributes,
			    GFileQueryInfoFlags   flags,
			    GCancellable         *cancellable,
			    GError              **error)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);
  GError *my_error = NULL;
  GFileInfo *info;
  GFileAttributeMatcher *matcher;
  gboolean res;
  gsize size;
  guint32 resource_flags;
  char **children;
  gboolean is_dir;
  char *base;

  is_dir = FALSE;
  children = g_resources_enumerate_children (resource->path, 0, NULL);
  if (children != NULL)
    {
      g_strfreev (children);
      is_dir = TRUE;
    }

  /* root is always there */
  if (strcmp ("/", resource->path) == 0)
    is_dir = TRUE;

  if (!is_dir)
    {
      res = g_resources_get_info (resource->path, 0, &size, &resource_flags, &my_error);
      if (!res)
	{
	  if (g_error_matches (my_error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND))
	    {
	      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			   _("The resource at “%s” does not exist"),
			   resource->path);
	    }
	  else
	    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                 my_error->message);
	  g_clear_error (&my_error);
	  return FALSE;
	}
    }

  matcher = g_file_attribute_matcher_new (attributes);

  info = g_file_info_new ();
  base = g_resource_file_get_basename (file);
  g_file_info_set_name (info, base);
  g_file_info_set_display_name (info, base);

  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_READ, TRUE);
  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_WRITE, FALSE);
  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_EXECUTE, FALSE);
  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_RENAME, FALSE);
  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_DELETE, FALSE);
  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_TRASH, FALSE);

  if (is_dir)
    {
      g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
    }
  else
    {
      GBytes *bytes;
      char *content_type;

      g_file_info_set_file_type (info, G_FILE_TYPE_REGULAR);
      g_file_info_set_size (info, size);

      if ((_g_file_attribute_matcher_matches_id (matcher, G_FILE_ATTRIBUTE_ID_STANDARD_CONTENT_TYPE) ||
           ((~resource_flags & G_RESOURCE_FLAGS_COMPRESSED) && 
            _g_file_attribute_matcher_matches_id (matcher, G_FILE_ATTRIBUTE_ID_STANDARD_FAST_CONTENT_TYPE))) &&
          (bytes = g_resources_lookup_data (resource->path, 0, NULL)))
        {
          const guchar *data;
          gsize data_size;

          data = g_bytes_get_data (bytes, &data_size);
          content_type = g_content_type_guess (base, data, data_size, NULL);

          g_bytes_unref (bytes);
        }
      else
        content_type = NULL;

      if (content_type)
        {
          _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_CONTENT_TYPE, content_type);
          _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_FAST_CONTENT_TYPE, content_type);

          g_free (content_type);
        }
    }

  g_free (base);
  g_file_attribute_matcher_unref (matcher);

  return info;
}

static GFileInfo *
g_resource_file_query_filesystem_info (GFile         *file,
                                       const char    *attributes,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
  GFileInfo *info;
  GFileAttributeMatcher *matcher;

  info = g_file_info_new ();

  matcher = g_file_attribute_matcher_new (attributes);
  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE))
    g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE, "resource");

  if (g_file_attribute_matcher_matches (matcher, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY))    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY, TRUE);

  g_file_attribute_matcher_unref (matcher);

  return info;
}

static GFileAttributeInfoList *
g_resource_file_query_settable_attributes (GFile         *file,
					   GCancellable  *cancellable,
					   GError       **error)
{
  return g_file_attribute_info_list_ref (resource_writable_attributes);
}

static GFileAttributeInfoList *
g_resource_file_query_writable_namespaces (GFile         *file,
					   GCancellable  *cancellable,
					   GError       **error)
{
  return g_file_attribute_info_list_ref (resource_writable_namespaces);
}

static GFileInputStream *
g_resource_file_read (GFile         *file,
		      GCancellable  *cancellable,
		      GError       **error)
{
  GResourceFile *resource = G_RESOURCE_FILE (file);
  GError *my_error = NULL;
  GInputStream *stream;
  GFileInputStream *res;

  stream = g_resources_open_stream (resource->path, 0, &my_error);

  if (stream == NULL)
    {
      if (g_error_matches (my_error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND))
	{
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
		       _("The resource at “%s” does not exist"),
		       resource->path);
	}
      else
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             my_error->message);
      g_clear_error (&my_error);
      return NULL;
    }

  res = _g_resource_file_input_stream_new (stream, file);
  g_object_unref (stream);
  return res;
}

typedef GFileMonitor GResourceFileMonitor;
typedef GFileMonitorClass GResourceFileMonitorClass;

GType g_resource_file_monitor_get_type (void);

G_DEFINE_TYPE (GResourceFileMonitor, g_resource_file_monitor, G_TYPE_FILE_MONITOR)

static gboolean
g_resource_file_monitor_cancel (GFileMonitor *monitor)
{
  return TRUE;
}

static void
g_resource_file_monitor_init (GResourceFileMonitor *monitor)
{
}

static void
g_resource_file_monitor_class_init (GResourceFileMonitorClass *class)
{
  class->cancel = g_resource_file_monitor_cancel;
}

static GFileMonitor *
g_resource_file_monitor_file (GFile              *file,
                              GFileMonitorFlags   flags,
                              GCancellable       *cancellable,
                              GError            **error)
{
  return g_object_new (g_resource_file_monitor_get_type (), NULL);
}

static void
g_resource_file_file_iface_init (GFileIface *iface)
{
  iface->dup = g_resource_file_dup;
  iface->hash = g_resource_file_hash;
  iface->equal = g_resource_file_equal;
  iface->is_native = g_resource_file_is_native;
  iface->has_uri_scheme = g_resource_file_has_uri_scheme;
  iface->get_uri_scheme = g_resource_file_get_uri_scheme;
  iface->get_basename = g_resource_file_get_basename;
  iface->get_path = g_resource_file_get_path;
  iface->get_uri = g_resource_file_get_uri;
  iface->get_parse_name = g_resource_file_get_parse_name;
  iface->get_parent = g_resource_file_get_parent;
  iface->prefix_matches = g_resource_file_prefix_matches;
  iface->get_relative_path = g_resource_file_get_relative_path;
  iface->resolve_relative_path = g_resource_file_resolve_relative_path;
  iface->get_child_for_display_name = g_resource_file_get_child_for_display_name;
  iface->enumerate_children = g_resource_file_enumerate_children;
  iface->query_info = g_resource_file_query_info;
  iface->query_filesystem_info = g_resource_file_query_filesystem_info;
  iface->query_settable_attributes = g_resource_file_query_settable_attributes;
  iface->query_writable_namespaces = g_resource_file_query_writable_namespaces;
  iface->read_fn = g_resource_file_read;
  iface->monitor_file = g_resource_file_monitor_file;

  iface->supports_thread_contexts = TRUE;
}

static GFileInfo *g_resource_file_enumerator_next_file (GFileEnumerator  *enumerator,
							GCancellable     *cancellable,
							GError          **error);
static gboolean   g_resource_file_enumerator_close     (GFileEnumerator  *enumerator,
							GCancellable     *cancellable,
							GError          **error);

static void
g_resource_file_enumerator_finalize (GObject *object)
{
  GResourceFileEnumerator *resource;

  resource = G_RESOURCE_FILE_ENUMERATOR (object);

  g_strfreev (resource->children);
  g_free (resource->path);
  g_free (resource->attributes);

  G_OBJECT_CLASS (g_resource_file_enumerator_parent_class)->finalize (object);
}

static void
g_resource_file_enumerator_class_init (GResourceFileEnumeratorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GFileEnumeratorClass *enumerator_class = G_FILE_ENUMERATOR_CLASS (klass);

  gobject_class->finalize = g_resource_file_enumerator_finalize;

  enumerator_class->next_file = g_resource_file_enumerator_next_file;
  enumerator_class->close_fn = g_resource_file_enumerator_close;
}

static void
g_resource_file_enumerator_init (GResourceFileEnumerator *resource)
{
}

static GFileEnumerator *
_g_resource_file_enumerator_new (GResourceFile *file,
				 const char           *attributes,
				 GFileQueryInfoFlags   flags,
				 GCancellable         *cancellable,
				 GError              **error)
{
  GResourceFileEnumerator *resource;
  char **children;
  gboolean res;

  children = g_resources_enumerate_children (file->path, 0, NULL);
  if (children == NULL &&
      strcmp ("/", file->path) != 0)
    {
      res = g_resources_get_info (file->path, 0, NULL, NULL, NULL);
      if (res)
	g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY,
		     _("The resource at “%s” is not a directory"),
		     file->path);
      else
	g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
		     _("The resource at “%s” does not exist"),
		     file->path);
      return NULL;
    }

  resource = g_object_new (G_TYPE_RESOURCE_FILE_ENUMERATOR,
			   "container", file,
			   NULL);

  resource->children = children;
  resource->path = g_strdup (file->path);
  resource->attributes = g_strdup (attributes);
  resource->flags = flags;

  return G_FILE_ENUMERATOR (resource);
}

static GFileInfo *
g_resource_file_enumerator_next_file (GFileEnumerator  *enumerator,
				      GCancellable     *cancellable,
				      GError          **error)
{
  GResourceFileEnumerator *resource = G_RESOURCE_FILE_ENUMERATOR (enumerator);
  char *path;
  GFileInfo *info;
  GFile *file;

  if (resource->children == NULL ||
      resource->children[resource->index] == NULL)
    return NULL;

  path = g_build_path ("/", resource->path, resource->children[resource->index++], NULL);
  file = g_resource_file_new_for_path (path);
  g_free (path);

  info = g_file_query_info (file,
			    resource->attributes,
			    resource->flags,
			    cancellable,
			    error);

  g_object_unref (file);

  return info;
}

static gboolean
g_resource_file_enumerator_close (GFileEnumerator  *enumerator,
			       GCancellable     *cancellable,
			       GError          **error)
{
  return TRUE;
}


struct _GResourceFileInputStream
{
  GFileInputStream parent_instance;
  GInputStream *stream;
  GFile *file;
};

struct _GResourceFileInputStreamClass
{
  GFileInputStreamClass parent_class;
};

#define g_resource_file_input_stream_get_type _g_resource_file_input_stream_get_type
G_DEFINE_TYPE (GResourceFileInputStream, g_resource_file_input_stream, G_TYPE_FILE_INPUT_STREAM)

static gssize     g_resource_file_input_stream_read       (GInputStream      *stream,
							   void              *buffer,
							   gsize              count,
							   GCancellable      *cancellable,
							   GError           **error);
static gssize     g_resource_file_input_stream_skip       (GInputStream      *stream,
							   gsize              count,
							   GCancellable      *cancellable,
							   GError           **error);
static gboolean   g_resource_file_input_stream_close      (GInputStream      *stream,
							   GCancellable      *cancellable,
							   GError           **error);
static goffset    g_resource_file_input_stream_tell       (GFileInputStream  *stream);
static gboolean   g_resource_file_input_stream_can_seek   (GFileInputStream  *stream);
static gboolean   g_resource_file_input_stream_seek       (GFileInputStream  *stream,
							   goffset            offset,
							   GSeekType          type,
							   GCancellable      *cancellable,
							   GError           **error);
static GFileInfo *g_resource_file_input_stream_query_info (GFileInputStream  *stream,
							   const char        *attributes,
							   GCancellable      *cancellable,
							   GError           **error);

static void
g_resource_file_input_stream_finalize (GObject *object)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (object);

  g_object_unref (file->stream);
  g_object_unref (file->file);
  G_OBJECT_CLASS (g_resource_file_input_stream_parent_class)->finalize (object);
}

static void
g_resource_file_input_stream_class_init (GResourceFileInputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);
  GFileInputStreamClass *file_stream_class = G_FILE_INPUT_STREAM_CLASS (klass);

  gobject_class->finalize = g_resource_file_input_stream_finalize;

  stream_class->read_fn = g_resource_file_input_stream_read;
  stream_class->skip = g_resource_file_input_stream_skip;
  stream_class->close_fn = g_resource_file_input_stream_close;
  file_stream_class->tell = g_resource_file_input_stream_tell;
  file_stream_class->can_seek = g_resource_file_input_stream_can_seek;
  file_stream_class->seek = g_resource_file_input_stream_seek;
  file_stream_class->query_info = g_resource_file_input_stream_query_info;
}

static void
g_resource_file_input_stream_init (GResourceFileInputStream *info)
{
}

static GFileInputStream *
_g_resource_file_input_stream_new (GInputStream *in_stream, GFile *file)
{
  GResourceFileInputStream *stream;

  stream = g_object_new (G_TYPE_RESOURCE_FILE_INPUT_STREAM, NULL);
  stream->stream = g_object_ref (in_stream);
  stream->file = g_object_ref (file);

  return G_FILE_INPUT_STREAM (stream);
}

static gssize
g_resource_file_input_stream_read (GInputStream  *stream,
				   void          *buffer,
				   gsize          count,
				   GCancellable  *cancellable,
				   GError       **error)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);
  return g_input_stream_read (file->stream,
			      buffer, count, cancellable, error);
}

static gssize
g_resource_file_input_stream_skip (GInputStream  *stream,
				   gsize          count,
				   GCancellable  *cancellable,
				   GError       **error)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);
  return g_input_stream_skip (file->stream,
			      count, cancellable, error);
}

static gboolean
g_resource_file_input_stream_close (GInputStream  *stream,
				    GCancellable  *cancellable,
				    GError       **error)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);
  return g_input_stream_close (file->stream,
			       cancellable, error);
}


static goffset
g_resource_file_input_stream_tell (GFileInputStream *stream)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);

  if (!G_IS_SEEKABLE (file->stream))
      return 0;

  return g_seekable_tell (G_SEEKABLE (file->stream));
}

static gboolean
g_resource_file_input_stream_can_seek (GFileInputStream *stream)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);

  return G_IS_SEEKABLE (file->stream) && g_seekable_can_seek (G_SEEKABLE (file->stream));
}

static gboolean
g_resource_file_input_stream_seek (GFileInputStream  *stream,
				   goffset            offset,
				   GSeekType          type,
				   GCancellable      *cancellable,
				   GError           **error)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);

  if (!G_IS_SEEKABLE (file->stream))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			   _("Input stream doesn’t implement seek"));
      return FALSE;
    }

  return g_seekable_seek (G_SEEKABLE (file->stream),
			  offset, type, cancellable, error);
}

static GFileInfo *
g_resource_file_input_stream_query_info (GFileInputStream  *stream,
					 const char        *attributes,
					 GCancellable      *cancellable,
					 GError           **error)
{
  GResourceFileInputStream *file = G_RESOURCE_FILE_INPUT_STREAM (stream);

  return g_file_query_info (file->file, attributes, 0, cancellable, error);
}
