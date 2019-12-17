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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "ggettext.h"
#include "glibintl.h"
#include "glib-private.h"

#include "galloca.h"
#include "gthread.h"
#include "gmem.h"
#ifdef G_OS_WIN32
#include "gwin32.h"
#include "gfileutils.h"
#include "gstrfuncs.h"
#include "glib-init.h"
#endif

#include <string.h>
#include <locale.h>
#include <libintl.h>

#ifdef G_OS_WIN32

/**
 * _glib_get_locale_dir:
 *
 * Return the path to the share\locale or lib\locale subfolder of the
 * GLib installation folder. The path is in the system codepage. We
 * have to use system codepage as bindtextdomain() doesn't have a
 * UTF-8 interface.
 */
gchar *
_glib_get_locale_dir (void)
{
  gchar *install_dir = NULL, *locale_dir;
  gchar *retval = NULL;

  if (glib_dll != NULL)
    install_dir = g_win32_get_package_installation_directory_of_module (glib_dll);

  if (install_dir)
    {
      /*
       * Append "/share/locale" or "/lib/locale" depending on whether
       * autoconfigury detected GNU gettext or not.
       */
      const char *p = GLIB_LOCALE_DIR + strlen (GLIB_LOCALE_DIR);
      while (*--p != '/')
	;
      while (*--p != '/')
	;

      locale_dir = g_build_filename (install_dir, p, NULL);

      retval = g_win32_locale_filename_from_utf8 (locale_dir);

      g_free (install_dir);
      g_free (locale_dir);
    }

  if (retval)
    return retval;
  else
    return g_strdup ("");
}

#undef GLIB_LOCALE_DIR

#endif /* G_OS_WIN32 */


static void
ensure_gettext_initialized (void)
{
  static gsize initialised;

  if (g_once_init_enter (&initialised))
    {
#ifdef G_OS_WIN32
      gchar *tmp = _glib_get_locale_dir ();
      bindtextdomain (GETTEXT_PACKAGE, tmp);
      g_free (tmp);
#else
      bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);
#endif
#    ifdef HAVE_BIND_TEXTDOMAIN_CODESET
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#    endif
      g_once_init_leave (&initialised, TRUE);
    }
}

/**
 * glib_gettext:
 * @str: The string to be translated
 *
 * Returns the translated string from the glib translations.
 * This is an internal function and should only be used by
 * the internals of glib (such as libgio).
 *
 * Returns: the transation of @str to the current locale
 */
const gchar *
glib_gettext (const gchar *str)
{
  ensure_gettext_initialized ();

  return g_dgettext (GETTEXT_PACKAGE, str);
}

/**
 * glib_pgettext:
 * @msgctxtid: a combined message context and message id, separated
 *   by a \004 character
 * @msgidoffset: the offset of the message id in @msgctxid
 *
 * This function is a variant of glib_gettext() which supports
 * a disambiguating message context. See g_dpgettext() for full
 * details.
 *
 * This is an internal function and should only be used by
 * the internals of glib (such as libgio).
 *
 * Returns: the translation of @str to the current locale
 */
const gchar *
glib_pgettext (const gchar *msgctxtid,
               gsize        msgidoffset)
{
  ensure_gettext_initialized ();

  return g_dpgettext (GETTEXT_PACKAGE, msgctxtid, msgidoffset);
}

/**
 * g_strip_context:
 * @msgid: a string
 * @msgval: another string
 *
 * An auxiliary function for gettext() support (see Q_()).
 *
 * Returns: @msgval, unless @msgval is identical to @msgid
 *     and contains a '|' character, in which case a pointer to
 *     the substring of msgid after the first '|' character is returned.
 *
 * Since: 2.4
 */
const gchar *
g_strip_context (const gchar *msgid,
                 const gchar *msgval)
{
  if (msgval == msgid)
    {
      const char *c = strchr (msgid, '|');
      if (c != NULL)
        return c + 1;
    }

  return msgval;
}

/**
 * g_dpgettext:
 * @domain: (nullable): the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @msgctxtid: a combined message context and message id, separated
 *   by a \004 character
 * @msgidoffset: the offset of the message id in @msgctxid
 *
 * This function is a variant of g_dgettext() which supports
 * a disambiguating message context. GNU gettext uses the
 * '\004' character to separate the message context and
 * message id in @msgctxtid.
 * If 0 is passed as @msgidoffset, this function will fall back to
 * trying to use the deprecated convention of using "|" as a separation
 * character.
 *
 * This uses g_dgettext() internally. See that functions for differences
 * with dgettext() proper.
 *
 * Applications should normally not use this function directly,
 * but use the C_() macro for translations with context.
 *
 * Returns: The translated string
 *
 * Since: 2.16
 */
const gchar *
g_dpgettext (const gchar *domain,
             const gchar *msgctxtid,
             gsize        msgidoffset)
{
  const gchar *translation;
  gchar *sep;

  translation = g_dgettext (domain, msgctxtid);

  if (translation == msgctxtid)
    {
      if (msgidoffset > 0)
        return msgctxtid + msgidoffset;
      sep = strchr (msgctxtid, '|');

      if (sep)
        {
          /* try with '\004' instead of '|', in case
           * xgettext -kQ_:1g was used
           */
          gchar *tmp = g_alloca (strlen (msgctxtid) + 1);
          strcpy (tmp, msgctxtid);
          tmp[sep - msgctxtid] = '\004';

          translation = g_dgettext (domain, tmp);

          if (translation == tmp)
            return sep + 1;
        }
    }

  return translation;
}

/* This function is taken from gettext.h
 * GNU gettext uses '\004' to separate context and msgid in .mo files.
 */
/**
 * g_dpgettext2:
 * @domain: (nullable): the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @context: the message context
 * @msgid: the message
 *
 * This function is a variant of g_dgettext() which supports
 * a disambiguating message context. GNU gettext uses the
 * '\004' character to separate the message context and
 * message id in @msgctxtid.
 *
 * This uses g_dgettext() internally. See that functions for differences
 * with dgettext() proper.
 *
 * This function differs from C_() in that it is not a macro and
 * thus you may use non-string-literals as context and msgid arguments.
 *
 * Returns: The translated string
 *
 * Since: 2.18
 */
const gchar *
g_dpgettext2 (const gchar *domain,
              const gchar *msgctxt,
              const gchar *msgid)
{
  size_t msgctxt_len = strlen (msgctxt) + 1;
  size_t msgid_len = strlen (msgid) + 1;
  const char *translation;
  char* msg_ctxt_id;

  msg_ctxt_id = g_alloca (msgctxt_len + msgid_len);

  memcpy (msg_ctxt_id, msgctxt, msgctxt_len - 1);
  msg_ctxt_id[msgctxt_len - 1] = '\004';
  memcpy (msg_ctxt_id + msgctxt_len, msgid, msgid_len);

  translation = g_dgettext (domain, msg_ctxt_id);

  if (translation == msg_ctxt_id)
    {
      /* try the old way of doing message contexts, too */
      msg_ctxt_id[msgctxt_len - 1] = '|';
      translation = g_dgettext (domain, msg_ctxt_id);

      if (translation == msg_ctxt_id)
        return msgid;
    }

  return translation;
}

static gboolean
_g_dgettext_should_translate (void)
{
  static gsize translate = 0;
  enum {
    SHOULD_TRANSLATE = 1,
    SHOULD_NOT_TRANSLATE = 2
  };

  if (G_UNLIKELY (g_once_init_enter (&translate)))
    {
      gboolean should_translate = TRUE;

      const char *default_domain     = textdomain (NULL);
      const char *translator_comment = gettext ("");
#ifndef G_OS_WIN32
      const char *translate_locale   = setlocale (LC_MESSAGES, NULL);
#else
      const char *translate_locale   = g_win32_getlocale ();
#endif
      /* We should NOT translate only if all the following hold:
       *   - user has called textdomain() and set textdomain to non-default
       *   - default domain has no translations
       *   - locale does not start with "en_" and is not "C"
       *
       * Rationale:
       *   - If text domain is still the default domain, maybe user calls
       *     it later. Continue with old behavior of translating.
       *   - If locale starts with "en_", we can continue using the
       *     translations even if the app doesn't have translations for
       *     this locale.  That is, en_UK and en_CA for example.
       *   - If locale is "C", maybe user calls setlocale(LC_ALL,"") later.
       *     Continue with old behavior of translating.
       */
      if (!default_domain || !translator_comment || !translate_locale ||
          (0 != strcmp (default_domain, "messages") &&
          '\0' == *translator_comment &&
          0 != strncmp (translate_locale, "en_", 3) &&
          0 != strcmp (translate_locale, "C")))
        should_translate = FALSE;

      g_once_init_leave (&translate,
                         should_translate ?
                         SHOULD_TRANSLATE :
                         SHOULD_NOT_TRANSLATE);
    }

  return translate == SHOULD_TRANSLATE;
}

/**
 * g_dgettext:
 * @domain: (nullable): the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @msgid: message to translate
 *
 * This function is a wrapper of dgettext() which does not translate
 * the message if the default domain as set with textdomain() has no
 * translations for the current locale.
 *
 * The advantage of using this function over dgettext() proper is that
 * libraries using this function (like GTK+) will not use translations
 * if the application using the library does not have translations for
 * the current locale.  This results in a consistent English-only
 * interface instead of one having partial translations.  For this
 * feature to work, the call to textdomain() and setlocale() should
 * precede any g_dgettext() invocations.  For GTK+, it means calling
 * textdomain() before gtk_init or its variants.
 *
 * This function disables translations if and only if upon its first
 * call all the following conditions hold:
 * 
 * - @domain is not %NULL
 *
 * - textdomain() has been called to set a default text domain
 *
 * - there is no translations available for the default text domain
 *   and the current locale
 *
 * - current locale is not "C" or any English locales (those
 *   starting with "en_")
 *
 * Note that this behavior may not be desired for example if an application
 * has its untranslated messages in a language other than English. In those
 * cases the application should call textdomain() after initializing GTK+.
 *
 * Applications should normally not use this function directly,
 * but use the _() macro for translations.
 *
 * Returns: The translated string
 *
 * Since: 2.18
 */
const gchar *
g_dgettext (const gchar *domain,
            const gchar *msgid)
{
  if (domain && G_UNLIKELY (!_g_dgettext_should_translate ()))
    return msgid;

  return dgettext (domain, msgid);
}

/**
 * g_dcgettext:
 * @domain: (nullable): the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @msgid: message to translate
 * @category: a locale category
 *
 * This is a variant of g_dgettext() that allows specifying a locale
 * category instead of always using `LC_MESSAGES`. See g_dgettext() for
 * more information about how this functions differs from calling
 * dcgettext() directly.
 *
 * Returns: the translated string for the given locale category
 *
 * Since: 2.26
 */
const gchar *
g_dcgettext (const gchar *domain,
             const gchar *msgid,
             gint         category)
{
  if (domain && G_UNLIKELY (!_g_dgettext_should_translate ()))
    return msgid;

  return dcgettext (domain, msgid, category);
}

/**
 * g_dngettext:
 * @domain: (nullable): the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @msgid: message to translate
 * @msgid_plural: plural form of the message
 * @n: the quantity for which translation is needed
 *
 * This function is a wrapper of dngettext() which does not translate
 * the message if the default domain as set with textdomain() has no
 * translations for the current locale.
 *
 * See g_dgettext() for details of how this differs from dngettext()
 * proper.
 *
 * Returns: The translated string
 *
 * Since: 2.18
 */
const gchar *
g_dngettext (const gchar *domain,
             const gchar *msgid,
             const gchar *msgid_plural,
             gulong       n)
{
  if (domain && G_UNLIKELY (!_g_dgettext_should_translate ()))
    return n == 1 ? msgid : msgid_plural;

  return dngettext (domain, msgid, msgid_plural, n);
}


/**
 * SECTION:i18n
 * @title: Internationalization
 * @short_description: gettext support macros
 * @see_also: the gettext manual
 *
 * GLib doesn't force any particular localization method upon its users.
 * But since GLib itself is localized using the gettext() mechanism, it seems
 * natural to offer the de-facto standard gettext() support macros in an
 * easy-to-use form.
 *
 * In order to use these macros in an application, you must include
 * `<glib/gi18n.h>`. For use in a library, you must include
 * `<glib/gi18n-lib.h>`
 * after defining the %GETTEXT_PACKAGE macro suitably for your library:
 * |[<!-- language="C" -->
 * #define GETTEXT_PACKAGE "gtk20"
 * #include <glib/gi18n-lib.h>
 * ]|
 * For an application, note that you also have to call bindtextdomain(),
 * bind_textdomain_codeset(), textdomain() and setlocale() early on in your
 * main() to make gettext() work. For example:
 * |[<!-- language="C" -->
 * #include <glib/gi18n.h>
 * #include <locale.h>
 *
 * int
 * main (int argc, char **argv)
 * {
 *   setlocale (LC_ALL, "");
 *   bindtextdomain (GETTEXT_PACKAGE, DATADIR "/locale");
 *   bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
 *   textdomain (GETTEXT_PACKAGE);
 *
 *   // Rest of your application.
 * }
 * ]|
 * where `DATADIR` is as typically provided by automake.
 *
 * For a library, you only have to call bindtextdomain() and
 * bind_textdomain_codeset() in your initialization function. If your library
 * doesn't have an initialization function, you can call the functions before
 * the first translated message.
 *
 * The
 * [gettext manual](http://www.gnu.org/software/gettext/manual/gettext.html#Maintainers)
 * covers details of how to integrate gettext into a project’s build system and
 * workflow.
 */

/**
 * _:
 * @String: the string to be translated
 *
 * Marks a string for translation, gets replaced with the translated string
 * at runtime.
 *
 * Since: 2.4
 */

/**
 * Q_:
 * @String: the string to be translated, with a '|'-separated prefix
 *     which must not be translated
 *
 * Like _(), but handles context in message ids. This has the advantage
 * that the string can be adorned with a prefix to guarantee uniqueness
 * and provide context to the translator.
 *
 * One use case given in the gettext manual is GUI translation, where one
 * could e.g. disambiguate two "Open" menu entries as "File|Open" and
 * "Printer|Open". Another use case is the string "Russian" which may
 * have to be translated differently depending on whether it's the name
 * of a character set or a language. This could be solved by using
 * "charset|Russian" and "language|Russian".
 *
 * See the C_() macro for a different way to mark up translatable strings
 * with context.
 *
 * If you are using the Q_() macro, you need to make sure that you pass
 * `--keyword=Q_` to xgettext when extracting messages.
 * If you are using GNU gettext >= 0.15, you can also use
 * `--keyword=Q_:1g` to let xgettext split the context
 * string off into a msgctxt line in the po file.
 *
 * Returns: the translated message
 *
 * Since: 2.4
 */

/**
 * C_:
 * @Context: a message context, must be a string literal
 * @String: a message id, must be a string literal
 *
 * Uses gettext to get the translation for @String. @Context is
 * used as a context. This is mainly useful for short strings which
 * may need different translations, depending on the context in which
 * they are used.
 * |[<!-- language="C" -->
 * label1 = C_("Navigation", "Back");
 * label2 = C_("Body part", "Back");
 * ]|
 *
 * If you are using the C_() macro, you need to make sure that you pass
 * `--keyword=C_:1c,2` to xgettext when extracting messages.
 * Note that this only works with GNU gettext >= 0.15.
 *
 * Returns: the translated message
 *
 * Since: 2.16
 */

/**
 * N_:
 * @String: the string to be translated
 *
 * Only marks a string for translation. This is useful in situations
 * where the translated strings can't be directly used, e.g. in string
 * array initializers. To get the translated string, call gettext()
 * at runtime.
 * |[<!-- language="C" -->
 * {
 *   static const char *messages[] = {
 *     N_("some very meaningful message"),
 *     N_("and another one")
 *   };
 *   const char *string;
 *   ...
 *   string
 *     = index &gt; 1 ? _("a default message") : gettext (messages[index]);
 *
 *   fputs (string);
 *   ...
 * }
 * ]|
 *
 * Since: 2.4
 */

/**
 * NC_:
 * @Context: a message context, must be a string literal
 * @String: a message id, must be a string literal
 *
 * Only marks a string for translation, with context.
 * This is useful in situations where the translated strings can't
 * be directly used, e.g. in string array initializers. To get the
 * translated string, you should call g_dpgettext2() at runtime.
 *
 * |[<!-- language="C" -->
 * {
 *   static const char *messages[] = {
 *     NC_("some context", "some very meaningful message"),
 *     NC_("some context", "and another one")
 *   };
 *   const char *string;
 *   ...
 *   string
 *     = index > 1 ? g_dpgettext2 (NULL, "some context", "a default message")
 *                 : g_dpgettext2 (NULL, "some context", messages[index]);
 *
 *   fputs (string);
 *   ...
 * }
 * ]|
 *
 * If you are using the NC_() macro, you need to make sure that you pass
 * `--keyword=NC_:1c,2` to xgettext when extracting messages.
 * Note that this only works with GNU gettext >= 0.15. Intltool has support
 * for the NC_() macro since version 0.40.1.
 *
 * Since: 2.18
 */

