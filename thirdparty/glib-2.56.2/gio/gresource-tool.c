/*
 * Copyright © 2012 Red Hat, Inc
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
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <locale.h>

#ifdef HAVE_LIBELF
#include <libelf.h>
#include <gelf.h>
#include <sys/mman.h>
#endif

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gi18n.h>

#ifdef G_OS_WIN32
#include "glib/glib-private.h"
#endif

/* GResource functions {{{1 */
static GResource *
get_resource (const gchar *file)
{
  gchar *content;
  gsize size;
  GResource *resource;
  GBytes *data;

  resource = NULL;

  if (g_file_get_contents (file, &content, &size, NULL))
    {
      data = g_bytes_new_take (content, size);
      resource = g_resource_new_from_data (data, NULL);
      g_bytes_unref (data);
    }

  return resource;
}

static void
list_resource (GResource   *resource,
               const gchar *path,
               const gchar *section,
               const gchar *prefix,
               gboolean     details)
{
  gchar **children;
  gsize size;
  guint32 flags;
  gint i;
  gchar *child;
  GError *error = NULL;
  gint len;

  children = g_resource_enumerate_children (resource, path, 0, &error);
  if (error)
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      return;
    }
  for (i = 0; children[i]; i++)
    {
      child = g_strconcat (path, children[i], NULL);

      len = MIN (strlen (child), strlen (prefix));
      if (strncmp (child, prefix, len) != 0)
        {
          g_free (child);
          continue;
        }

      if (g_resource_get_info (resource, child, 0, &size, &flags, NULL))
        {
          if (details)
            g_print ("%s%s%6"G_GSIZE_FORMAT " %s %s\n", section, section[0] ? " " : "", size, (flags & G_RESOURCE_FLAGS_COMPRESSED) ? "c" : "u", child);
          else
            g_print ("%s\n", child);
        }
      else
        list_resource (resource, child, section, prefix, details);

      g_free (child);
    }
  g_strfreev (children);
}

static void
extract_resource (GResource   *resource,
                  const gchar *path)
{
  GBytes *bytes;

  bytes = g_resource_lookup_data (resource, path, 0, NULL);
  if (bytes != NULL)
    {
      gconstpointer data;
      gsize size, written;

      data = g_bytes_get_data (bytes, &size);
      written = fwrite (data, 1, size, stdout);
      if (written < size)
        g_printerr ("Data truncated\n");
      g_bytes_unref (bytes);
    }
}

/* Elf functions {{{1 */

#ifdef HAVE_LIBELF

static Elf *
get_elf (const gchar *file,
         gint        *fd)
{
  Elf *elf;

  if (elf_version (EV_CURRENT) == EV_NONE )
    return NULL;

  *fd = g_open (file, O_RDONLY, 0);
  if (*fd < 0)
    return NULL;

  elf = elf_begin (*fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      g_close (*fd, NULL);
      *fd = -1;
      return NULL;
    }

  if (elf_kind (elf) != ELF_K_ELF)
    {
      g_close (*fd, NULL);
      *fd = -1;
      return NULL;
    }

  return elf;
}

typedef gboolean (*SectionCallback) (GElf_Shdr   *shdr,
                                     const gchar *name,
                                     gpointer     data);

static void
elf_foreach_resource_section (Elf             *elf,
                              SectionCallback  callback,
                              gpointer         data)
{
  size_t shstrndx, shnum;
  size_t scnidx;
  Elf_Scn *scn;
  GElf_Shdr *shdr, shdr_mem;
  const gchar *section_name;

  elf_getshdrstrndx (elf, &shstrndx);
  g_assert (shstrndx >= 0);

  elf_getshdrnum (elf, &shnum);
  g_assert (shnum >= 0);

  for (scnidx = 1; scnidx < shnum; scnidx++)
    {
      scn = elf_getscn (elf, scnidx);
      if (scn == NULL)
        continue;

      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
        continue;

      if (shdr->sh_type != SHT_PROGBITS)
        continue;

      section_name = elf_strptr (elf, shstrndx, shdr->sh_name);
      if (section_name == NULL ||
          !g_str_has_prefix (section_name, ".gresource."))
        continue;

      if (!callback (shdr, section_name + strlen (".gresource."), data))
        break;
    }
}

static GResource *
resource_from_section (GElf_Shdr *shdr,
                       int        fd)
{
  gsize page_size, page_offset;
  char *contents;
  GResource *resource;

  resource = NULL;

  page_size = sysconf(_SC_PAGE_SIZE);
  page_offset = shdr->sh_offset % page_size;
  contents = mmap (NULL,  shdr->sh_size + page_offset,
                   PROT_READ, MAP_PRIVATE, fd, shdr->sh_offset - page_offset);
  if (contents != MAP_FAILED)
    {
      GBytes *bytes;
      GError *error = NULL;

      bytes = g_bytes_new_static (contents + page_offset, shdr->sh_size);
      resource = g_resource_new_from_data (bytes, &error);
      g_bytes_unref (bytes);
      if (error)
        {
          g_printerr ("%s\n", error->message);
          g_error_free (error);
        }
    }
  else
    {
      g_printerr ("Can't mmap resource section");
    }

  return resource;
}

typedef struct
{
  int fd;
  const gchar *section;
  const gchar *path;
  gboolean details;
  gboolean found;
} CallbackData;

static gboolean
list_resources_cb (GElf_Shdr   *shdr,
                   const gchar *section,
                   gpointer     data)
{
  CallbackData *d = data;
  GResource *resource;

  if (d->section && strcmp (section, d->section) != 0)
    return TRUE;

  d->found = TRUE;

  resource = resource_from_section (shdr, d->fd);
  list_resource (resource, "/",
                 d->section ? "" : section,
                 d->path,
                 d->details);
  g_resource_unref (resource);

  if (d->section)
    return FALSE;

  return TRUE;
}

static void
elf_list_resources (Elf         *elf,
                    int          fd,
                    const gchar *section,
                    const gchar *path,
                    gboolean     details)
{
  CallbackData data;

  data.fd = fd;
  data.section = section;
  data.path = path;
  data.details = details;
  data.found = FALSE;

  elf_foreach_resource_section (elf, list_resources_cb, &data);

  if (!data.found)
    g_printerr ("Can't find resource section %s\n", section);
}

static gboolean
extract_resource_cb (GElf_Shdr   *shdr,
                     const gchar *section,
                     gpointer     data)
{
  CallbackData *d = data;
  GResource *resource;

  if (d->section && strcmp (section, d->section) != 0)
    return TRUE;

  d->found = TRUE;

  resource = resource_from_section (shdr, d->fd);
  extract_resource (resource, d->path);
  g_resource_unref (resource);

  if (d->section)
    return FALSE;

  return TRUE;
}

static void
elf_extract_resource (Elf         *elf,
                      int          fd,
                      const gchar *section,
                      const gchar *path)
{
  CallbackData data;

  data.fd = fd;
  data.section = section;
  data.path = path;
  data.found = FALSE;

  elf_foreach_resource_section (elf, extract_resource_cb, &data);

  if (!data.found)
    g_printerr ("Can't find resource section %s\n", section);
}

static gboolean
print_section_name (GElf_Shdr   *shdr,
                    const gchar *name,
                    gpointer     data)
{
  g_print ("%s\n", name);
  return TRUE;
}

#endif /* HAVE_LIBELF */

  /* Toplevel commands {{{1 */

static void
cmd_sections (const gchar *file,
              const gchar *section,
              const gchar *path,
              gboolean     details)
{
  GResource *resource;

#ifdef HAVE_LIBELF

  Elf *elf;
  gint fd;

  if ((elf = get_elf (file, &fd)))
    {
      elf_foreach_resource_section (elf, print_section_name, NULL);
      elf_end (elf);
      close (fd);
    }
  else

#endif

  if ((resource = get_resource (file)))
    {
      /* No sections */
      g_resource_unref (resource);
    }
  else
    {
      g_printerr ("Don't know how to handle %s\n", file);
#ifndef HAVE_LIBELF
      g_printerr ("gresource is built without elf support\n");
#endif
    }
}

static void
cmd_list (const gchar *file,
          const gchar *section,
          const gchar *path,
          gboolean     details)
{
  GResource *resource;

#ifdef HAVE_LIBELF
  Elf *elf;
  int fd;

  if ((elf = get_elf (file, &fd)))
    {
      elf_list_resources (elf, fd, section, path ? path : "", details);
      elf_end (elf);
      close (fd);
    }
  else

#endif

  if ((resource = get_resource (file)))
    {
      list_resource (resource, "/", "", path ? path : "", details);
      g_resource_unref (resource);
    }
  else
    {
      g_printerr ("Don't know how to handle %s\n", file);
#ifndef HAVE_LIBELF
      g_printerr ("gresource is built without elf support\n");
#endif
    }
}

static void
cmd_extract (const gchar *file,
             const gchar *section,
             const gchar *path,
             gboolean     details)
{
  GResource *resource;

#ifdef HAVE_LIBELF

  Elf *elf;
  int fd;

  if ((elf = get_elf (file, &fd)))
    {
      elf_extract_resource (elf, fd, section, path);
      elf_end (elf);
      close (fd);
    }
  else

#endif

  if ((resource = get_resource (file)))
    {
      extract_resource (resource, path);
      g_resource_unref (resource);
    }
  else
    {
      g_printerr ("Don't know how to handle %s\n", file);
#ifndef HAVE_LIBELF
      g_printerr ("gresource is built without elf support\n");
#endif
    }
}

static gint
cmd_help (gboolean     requested,
          const gchar *command)
{
  const gchar *description;
  const gchar *synopsis;
  gchar *option;
  GString *string;

  option = NULL;

  string = g_string_new (NULL);

  if (command == NULL)
    ;

  else if (strcmp (command, "help") == 0)
    {
      description = _("Print help");
      synopsis = _("[COMMAND]");
    }

  else if (strcmp (command, "sections") == 0)
    {
      description = _("List sections containing resources in an elf FILE");
      synopsis = _("FILE");
    }

  else if (strcmp (command, "list") == 0)
    {
      description = _("List resources\n"
                      "If SECTION is given, only list resources in this section\n"
                      "If PATH is given, only list matching resources");
      synopsis = _("FILE [PATH]");
      option = g_strdup_printf ("[--section %s]", _("SECTION"));
    }

  else if (strcmp (command, "details") == 0)
    {
      description = _("List resources with details\n"
                      "If SECTION is given, only list resources in this section\n"
                      "If PATH is given, only list matching resources\n"
                      "Details include the section, size and compression");
      synopsis = _("FILE [PATH]");
      option = g_strdup_printf ("[--section %s]", _("SECTION"));
    }

  else if (strcmp (command, "extract") == 0)
    {
      description = _("Extract a resource file to stdout");
      synopsis = _("FILE PATH");
      option = g_strdup_printf ("[--section %s]", _("SECTION"));
    }

  else
    {
      g_string_printf (string, _("Unknown command %s\n\n"), command);
      requested = FALSE;
      command = NULL;
    }

  if (command == NULL)
    {
      g_string_append (string,
      _("Usage:\n"
        "  gresource [--section SECTION] COMMAND [ARGS…]\n"
        "\n"
        "Commands:\n"
        "  help                      Show this information\n"
        "  sections                  List resource sections\n"
        "  list                      List resources\n"
        "  details                   List resources with details\n"
        "  extract                   Extract a resource\n"
        "\n"
        "Use “gresource help COMMAND” to get detailed help.\n\n"));
    }
  else
    {
      g_string_append_printf (string, _("Usage:\n  gresource %s%s%s %s\n\n%s\n\n"),
                              option ? option : "", option ? " " : "", command, synopsis[0] ? synopsis : "", description);

      g_string_append (string, _("Arguments:\n"));

      if (option)
        g_string_append (string,
                         _("  SECTION   An (optional) elf section name\n"));

      if (strstr (synopsis, _("[COMMAND]")))
        g_string_append (string,
                       _("  COMMAND   The (optional) command to explain\n"));

      if (strstr (synopsis, _("FILE")))
        {
          if (strcmp (command, "sections") == 0)
            g_string_append (string,
                             _("  FILE      An elf file (a binary or a shared library)\n"));
          else
            g_string_append (string,
                             _("  FILE      An elf file (a binary or a shared library)\n"
                               "            or a compiled resource file\n"));
        }

      if (strstr (synopsis, _("[PATH]")))
        g_string_append (string,
                       _("  PATH      An (optional) resource path (may be partial)\n"));
      else if (strstr (synopsis, _("PATH")))
        g_string_append (string,
                       _("  PATH      A resource path\n"));

      g_string_append (string, "\n");
    }

  if (requested)
    g_print ("%s", string->str);
  else
    g_printerr ("%s\n", string->str);

  g_free (option);
  g_string_free (string, TRUE);

  return requested ? 0 : 1;
}

/* main {{{1 */

int
main (int argc, char *argv[])
{
  gchar *section = NULL;
  gboolean details = FALSE;
  void (* function) (const gchar *, const gchar *, const gchar *, gboolean);

#ifdef G_OS_WIN32
  gchar *tmp;
#endif

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);

#ifdef G_OS_WIN32
  tmp = _glib_get_locale_dir ();
  bindtextdomain (GETTEXT_PACKAGE, tmp);
  g_free (tmp);
#else
  bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);
#endif

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  if (argc < 2)
    return cmd_help (FALSE, NULL);

  if (argc > 3 && strcmp (argv[1], "--section") == 0)
    {
      section = argv[2];
      argv = argv + 2;
      argc -= 2;
    }

  if (strcmp (argv[1], "help") == 0)
    return cmd_help (TRUE, argv[2]);

  else if (argc == 4 && strcmp (argv[1], "extract") == 0)
    function = cmd_extract;

  else if (argc == 3 && strcmp (argv[1], "sections") == 0)
    function = cmd_sections;

  else if ((argc == 3 || argc == 4) && strcmp (argv[1], "list") == 0)
    {
      function = cmd_list;
      details = FALSE;
    }
  else if ((argc == 3 || argc == 4) && strcmp (argv[1], "details") == 0)
    {
      function = cmd_list;
      details = TRUE;
    }
  else
    return cmd_help (FALSE, argv[1]);

  (* function) (argv[2], section, argc > 3 ? argv[3] : NULL, details);

  return 0;
}

/* vim:set foldmethod=marker: */
