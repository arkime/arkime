/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "config.h"

#include "genviron.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h> /* for _NSGetEnviron */
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "glib-private.h"
#include "gmem.h"
#include "gmessages.h"
#include "gstrfuncs.h"
#include "gunicode.h"
#include "gconvert.h"
#include "gquark.h"

/* Environ array functions {{{1 */
static gint
g_environ_find (gchar       **envp,
                const gchar  *variable)
{
  gint len, i;

  if (envp == NULL)
    return -1;

  len = strlen (variable);

  for (i = 0; envp[i]; i++)
    {
      if (strncmp (envp[i], variable, len) == 0 &&
          envp[i][len] == '=')
        return i;
    }

  return -1;
}

/**
 * g_environ_getenv:
 * @envp: (nullable) (array zero-terminated=1) (transfer none) (element-type filename):
 *     an environment list (eg, as returned from g_get_environ()), or %NULL
 *     for an empty environment list
 * @variable: (type filename): the environment variable to get
 *
 * Returns the value of the environment variable @variable in the
 * provided list @envp.
 *
 * Returns: (type filename): the value of the environment variable, or %NULL if
 *     the environment variable is not set in @envp. The returned
 *     string is owned by @envp, and will be freed if @variable is
 *     set or unset again.
 *
 * Since: 2.32
 */
const gchar *
g_environ_getenv (gchar       **envp,
                  const gchar  *variable)
{
  gint index;

  g_return_val_if_fail (variable != NULL, NULL);

  index = g_environ_find (envp, variable);
  if (index != -1)
    return envp[index] + strlen (variable) + 1;
  else
    return NULL;
}

/**
 * g_environ_setenv:
 * @envp: (nullable) (array zero-terminated=1) (element-type filename) (transfer full):
 *     an environment list that can be freed using g_strfreev() (e.g., as
 *     returned from g_get_environ()), or %NULL for an empty
 *     environment list
 * @variable: (type filename): the environment variable to set, must not
 *     contain '='
 * @value: (type filename): the value for to set the variable to
 * @overwrite: whether to change the variable if it already exists
 *
 * Sets the environment variable @variable in the provided list
 * @envp to @value.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer full):
 *     the updated environment list. Free it using g_strfreev().
 *
 * Since: 2.32
 */
gchar **
g_environ_setenv (gchar       **envp,
                  const gchar  *variable,
                  const gchar  *value,
                  gboolean      overwrite)
{
  gint index;

  g_return_val_if_fail (variable != NULL, NULL);
  g_return_val_if_fail (strchr (variable, '=') == NULL, NULL);
  g_return_val_if_fail (value != NULL, NULL);

  index = g_environ_find (envp, variable);
  if (index != -1)
    {
      if (overwrite)
        {
          g_free (envp[index]);
          envp[index] = g_strdup_printf ("%s=%s", variable, value);
        }
    }
  else
    {
      gint length;

      length = envp ? g_strv_length (envp) : 0;
      envp = g_renew (gchar *, envp, length + 2);
      envp[length] = g_strdup_printf ("%s=%s", variable, value);
      envp[length + 1] = NULL;
    }

  return envp;
}

static gchar **
g_environ_unsetenv_internal (gchar        **envp,
                             const gchar   *variable,
                             gboolean       free_value)
{
  gint len;
  gchar **e, **f;

  len = strlen (variable);

  /* Note that we remove *all* environment entries for
   * the variable name, not just the first.
   */
  e = f = envp;
  while (*e != NULL)
    {
      if (strncmp (*e, variable, len) != 0 || (*e)[len] != '=')
        {
          *f = *e;
          f++;
        }
      else
        {
          if (free_value)
            g_free (*e);
        }

      e++;
    }
  *f = NULL;

  return envp;
}


/**
 * g_environ_unsetenv:
 * @envp: (nullable) (array zero-terminated=1) (element-type filename) (transfer full):
 *     an environment list that can be freed using g_strfreev() (e.g., as
 *     returned from g_get_environ()), or %NULL for an empty environment list
 * @variable: (type filename): the environment variable to remove, must not
 *     contain '='
 *
 * Removes the environment variable @variable from the provided
 * environment @envp.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer full):
 *     the updated environment list. Free it using g_strfreev().
 *
 * Since: 2.32
 */
gchar **
g_environ_unsetenv (gchar       **envp,
                    const gchar  *variable)
{
  g_return_val_if_fail (variable != NULL, NULL);
  g_return_val_if_fail (strchr (variable, '=') == NULL, NULL);

  if (envp == NULL)
    return NULL;

  return g_environ_unsetenv_internal (envp, variable, TRUE);
}

/* UNIX implemention {{{1 */
#ifndef G_OS_WIN32

/**
 * g_getenv:
 * @variable: (type filename): the environment variable to get
 *
 * Returns the value of an environment variable.
 *
 * On UNIX, the name and value are byte strings which might or might not
 * be in some consistent character set and encoding. On Windows, they are
 * in UTF-8.
 * On Windows, in case the environment variable's value contains
 * references to other environment variables, they are expanded.
 *
 * Returns: (type filename): the value of the environment variable, or %NULL if
 *     the environment variable is not found. The returned string
 *     may be overwritten by the next call to g_getenv(), g_setenv()
 *     or g_unsetenv().
 */
const gchar *
g_getenv (const gchar *variable)
{
  g_return_val_if_fail (variable != NULL, NULL);

  return getenv (variable);
}

/**
 * g_setenv:
 * @variable: (type filename): the environment variable to set, must not
 *     contain '='.
 * @value: (type filename): the value for to set the variable to.
 * @overwrite: whether to change the variable if it already exists.
 *
 * Sets an environment variable. On UNIX, both the variable's name and
 * value can be arbitrary byte strings, except that the variable's name
 * cannot contain '='. On Windows, they should be in UTF-8.
 *
 * Note that on some systems, when variables are overwritten, the memory
 * used for the previous variables and its value isn't reclaimed.
 *
 * You should be mindful of the fact that environment variable handling
 * in UNIX is not thread-safe, and your program may crash if one thread
 * calls g_setenv() while another thread is calling getenv(). (And note
 * that many functions, such as gettext(), call getenv() internally.)
 * This function is only safe to use at the very start of your program,
 * before creating any other threads (or creating objects that create
 * worker threads of their own).
 *
 * If you need to set up the environment for a child process, you can
 * use g_get_environ() to get an environment array, modify that with
 * g_environ_setenv() and g_environ_unsetenv(), and then pass that
 * array directly to execvpe(), g_spawn_async(), or the like.
 *
 * Returns: %FALSE if the environment variable couldn't be set.
 *
 * Since: 2.4
 */
gboolean
g_setenv (const gchar *variable,
          const gchar *value,
          gboolean     overwrite)
{
  gint result;
#ifndef HAVE_SETENV
  gchar *string;
#endif

  g_return_val_if_fail (variable != NULL, FALSE);
  g_return_val_if_fail (strchr (variable, '=') == NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

#ifdef HAVE_SETENV
  result = setenv (variable, value, overwrite);
#else
  if (!overwrite && getenv (variable) != NULL)
    return TRUE;

  /* This results in a leak when you overwrite existing
   * settings. It would be fairly easy to fix this by keeping
   * our own parallel array or hash table.
   */
  string = g_strconcat (variable, "=", value, NULL);
  result = putenv (string);
#endif
  return result == 0;
}

#ifdef HAVE__NSGETENVIRON
#define environ (*_NSGetEnviron())
#else
/* According to the Single Unix Specification, environ is not
 * in any system header, although unistd.h often declares it.
 */
extern char **environ;
#endif

/**
 * g_unsetenv:
 * @variable: (type filename): the environment variable to remove, must
 *     not contain '='
 *
 * Removes an environment variable from the environment.
 *
 * Note that on some systems, when variables are overwritten, the
 * memory used for the previous variables and its value isn't reclaimed.
 *
 * You should be mindful of the fact that environment variable handling
 * in UNIX is not thread-safe, and your program may crash if one thread
 * calls g_unsetenv() while another thread is calling getenv(). (And note
 * that many functions, such as gettext(), call getenv() internally.) This
 * function is only safe to use at the very start of your program, before
 * creating any other threads (or creating objects that create worker
 * threads of their own).
 * 
 * If you need to set up the environment for a child process, you can
 * use g_get_environ() to get an environment array, modify that with
 * g_environ_setenv() and g_environ_unsetenv(), and then pass that
 * array directly to execvpe(), g_spawn_async(), or the like.
 *
 * Since: 2.4
 */
void
g_unsetenv (const gchar *variable)
{
  g_return_if_fail (variable != NULL);
  g_return_if_fail (strchr (variable, '=') == NULL);

#ifdef HAVE_UNSETENV
  unsetenv (variable);
#else /* !HAVE_UNSETENV */
  /* Mess directly with the environ array.
   * This seems to be the only portable way to do this.
   */
  g_environ_unsetenv_internal (environ, variable, FALSE);
#endif /* !HAVE_UNSETENV */
}

/**
 * g_listenv:
 *
 * Gets the names of all variables set in the environment.
 *
 * Programs that want to be portable to Windows should typically use
 * this function and g_getenv() instead of using the environ array
 * from the C library directly. On Windows, the strings in the environ
 * array are in system codepage encoding, while in most of the typical
 * use cases for environment variables in GLib-using programs you want
 * the UTF-8 encoding that this function and g_getenv() provide.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer full):
 *     a %NULL-terminated list of strings which must be freed with
 *     g_strfreev().
 *
 * Since: 2.8
 */
gchar **
g_listenv (void)
{
  gchar **result, *eq;
  gint len, i, j;

  len = g_strv_length (environ);
  result = g_new0 (gchar *, len + 1);

  j = 0;
  for (i = 0; i < len; i++)
    {
      eq = strchr (environ[i], '=');
      if (eq)
        result[j++] = g_strndup (environ[i], eq - environ[i]);
    }

  result[j] = NULL;

  return result;
}

/**
 * g_get_environ:
 *
 * Gets the list of environment variables for the current process.
 *
 * The list is %NULL terminated and each item in the list is of the
 * form 'NAME=VALUE'.
 *
 * This is equivalent to direct access to the 'environ' global variable,
 * except portable.
 *
 * The return value is freshly allocated and it should be freed with
 * g_strfreev() when it is no longer needed.
 *
 * Returns: (array zero-terminated=1) (element-type filename) (transfer full):
 *     the list of environment variables
 *
 * Since: 2.28
 */
gchar **
g_get_environ (void)
{
  return g_strdupv (environ);
}

/* Win32 implementation {{{1 */
#else   /* G_OS_WIN32 */

const gchar *
g_getenv (const gchar *variable)
{
  GQuark quark;
  gchar *value;
  wchar_t dummy[2], *wname, *wvalue;
  int len;

  g_return_val_if_fail (variable != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (variable, -1, NULL), NULL);

  /* On Windows NT, it is relatively typical that environment
   * variables contain references to other environment variables. If
   * so, use ExpandEnvironmentStrings(). (In an ideal world, such
   * environment variables would be stored in the Registry as
   * REG_EXPAND_SZ type values, and would then get automatically
   * expanded before a program sees them. But there is broken software
   * that stores environment variables as REG_SZ values even if they
   * contain references to other environment variables.)
   */

  wname = g_utf8_to_utf16 (variable, -1, NULL, NULL, NULL);

  len = GetEnvironmentVariableW (wname, dummy, 2);

  if (len == 0)
    {
      g_free (wname);
      if (GetLastError () == ERROR_ENVVAR_NOT_FOUND)
        return NULL;

      quark = g_quark_from_static_string ("");
      return g_quark_to_string (quark);
    }
  else if (len == 1)
    len = 2;

  wvalue = g_new (wchar_t, len);

  if (GetEnvironmentVariableW (wname, wvalue, len) != len - 1)
    {
      g_free (wname);
      g_free (wvalue);
      return NULL;
    }

  if (wcschr (wvalue, L'%') != NULL)
    {
      wchar_t *tem = wvalue;

      len = ExpandEnvironmentStringsW (wvalue, dummy, 2);

      if (len > 0)
        {
          wvalue = g_new (wchar_t, len);

          if (ExpandEnvironmentStringsW (tem, wvalue, len) != len)
            {
              g_free (wvalue);
              wvalue = tem;
            }
          else
            g_free (tem);
        }
    }

  value = g_utf16_to_utf8 (wvalue, -1, NULL, NULL, NULL);

  g_free (wname);
  g_free (wvalue);

  quark = g_quark_from_string (value);
  g_free (value);

  return g_quark_to_string (quark);
}

gboolean
g_setenv (const gchar *variable,
          const gchar *value,
          gboolean     overwrite)
{
  gboolean retval;
  wchar_t *wname, *wvalue, *wassignment;
  gchar *tem;

  g_return_val_if_fail (variable != NULL, FALSE);
  g_return_val_if_fail (strchr (variable, '=') == NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (g_utf8_validate (variable, -1, NULL), FALSE);
  g_return_val_if_fail (g_utf8_validate (value, -1, NULL), FALSE);

  if (!overwrite && g_getenv (variable) != NULL)
    return TRUE;

  /* We want to (if possible) set both the environment variable copy
   * kept by the C runtime and the one kept by the system.
   *
   * We can't use only the C runtime's putenv or _wputenv() as that
   * won't work for arbitrary Unicode strings in a "non-Unicode" app
   * (with main() and not wmain()). In a "main()" app the C runtime
   * initializes the C runtime's environment table by converting the
   * real (wide char) environment variables to system codepage, thus
   * breaking those that aren't representable in the system codepage.
   *
   * As the C runtime's putenv() will also set the system copy, we do
   * the putenv() first, then call SetEnvironmentValueW ourselves.
   */

  wname = g_utf8_to_utf16 (variable, -1, NULL, NULL, NULL);
  wvalue = g_utf8_to_utf16 (value, -1, NULL, NULL, NULL);
  tem = g_strconcat (variable, "=", value, NULL);
  wassignment = g_utf8_to_utf16 (tem, -1, NULL, NULL, NULL);

  g_free (tem);
  _wputenv (wassignment);
  g_free (wassignment);

  retval = (SetEnvironmentVariableW (wname, wvalue) != 0);

  g_free (wname);
  g_free (wvalue);

  return retval;
}

void
g_unsetenv (const gchar *variable)
{
  wchar_t *wname, *wassignment;
  gchar *tem;

  g_return_if_fail (variable != NULL);
  g_return_if_fail (strchr (variable, '=') == NULL);
  g_return_if_fail (g_utf8_validate (variable, -1, NULL));

  wname = g_utf8_to_utf16 (variable, -1, NULL, NULL, NULL);
  tem = g_strconcat (variable, "=", NULL);
  wassignment = g_utf8_to_utf16 (tem, -1, NULL, NULL, NULL);

  g_free (tem);
  _wputenv (wassignment);
  g_free (wassignment);

  SetEnvironmentVariableW (wname, NULL);

  g_free (wname);
}

gchar **
g_listenv (void)
{
  gchar **result, *eq;
  gint len = 0, j;
  wchar_t *p, *q;

  p = (wchar_t *) GetEnvironmentStringsW ();
  if (p != NULL)
    {
      q = p;
      while (*q)
        {
          q += wcslen (q) + 1;
          len++;
        }
    }
  result = g_new0 (gchar *, len + 1);

  j = 0;
  q = p;
  while (*q)
    {
      result[j] = g_utf16_to_utf8 (q, -1, NULL, NULL, NULL);
      if (result[j] != NULL)
        {
          eq = strchr (result[j], '=');
          if (eq && eq > result[j])
            {
              *eq = '\0';
              j++;
            }
          else
            g_free (result[j]);
        }
      q += wcslen (q) + 1;
    }
  result[j] = NULL;
  FreeEnvironmentStringsW (p);

  return result;
}

gchar **
g_get_environ (void)
{
  gunichar2 *strings;
  gchar **result;
  gint i, n;

  strings = GetEnvironmentStringsW ();
  for (n = 0, i = 0; strings[n]; i++)
    n += wcslen (strings + n) + 1;

  result = g_new (char *, i + 1);
  for (n = 0, i = 0; strings[n]; i++)
    {
      result[i] = g_utf16_to_utf8 (strings + n, -1, NULL, NULL, NULL);
      n += wcslen (strings + n) + 1;
    }
  FreeEnvironmentStringsW (strings);
  result[i] = NULL;

  return result;
}

#endif  /* G_OS_WIN32 */

#ifdef G_OS_WIN32

/* Binary compatibility versions. Not for newly compiled code. */

_GLIB_EXTERN const gchar *g_getenv_utf8   (const gchar  *variable);
_GLIB_EXTERN gboolean     g_setenv_utf8   (const gchar  *variable,
                                           const gchar  *value,
                                           gboolean      overwrite);
_GLIB_EXTERN void         g_unsetenv_utf8 (const gchar  *variable);

const gchar *
g_getenv_utf8 (const gchar *variable)
{
  return g_getenv (variable);
}

gboolean
g_setenv_utf8 (const gchar *variable,
               const gchar *value,
               gboolean     overwrite)
{
  return g_setenv (variable, value, overwrite);
}

void
g_unsetenv_utf8 (const gchar *variable)
{
  g_unsetenv (variable);
}

#endif

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
