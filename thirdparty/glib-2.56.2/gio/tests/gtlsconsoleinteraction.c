/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora, Ltd.
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <conio.h>
#endif

#include "gtlsconsoleinteraction.h"

/*
 * WARNING: This is not the example you're looking for [slow hand wave]. This
 * is not industrial strength, it's just for testing. It uses embarassing
 * functions like getpass() and does lazy things with threads.
 */

G_DEFINE_TYPE (GTlsConsoleInteraction, g_tls_console_interaction, G_TYPE_TLS_INTERACTION)

#if defined(G_OS_WIN32) || defined(__BIONIC__)
/* win32 doesn't have getpass() */
#include <stdio.h>
#ifndef BUFSIZ
#define BUFSIZ 8192
#endif
static gchar *
getpass (const gchar *prompt)
{
  static gchar buf[BUFSIZ];
  gint i;

  g_printf ("%s", prompt);
  fflush (stdout);

  for (i = 0; i < BUFSIZ - 1; ++i)
    {
#ifdef __BIONIC__
      buf[i] = getc (stdin);
#else
      buf[i] = _getch ();
#endif
      if (buf[i] == '\r')
        break;
    }
  buf[i] = '\0';

  g_printf ("\n");

  return &buf[0];
}
#endif

static GTlsInteractionResult
g_tls_console_interaction_ask_password (GTlsInteraction    *interaction,
                                        GTlsPassword       *password,
                                        GCancellable       *cancellable,
                                        GError            **error)
{
  const gchar *value;
  gchar *prompt;

  prompt = g_strdup_printf ("Password \"%s\"': ", g_tls_password_get_description (password));
  value = getpass (prompt);
  g_free (prompt);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return G_TLS_INTERACTION_FAILED;

  g_tls_password_set_value (password, (guchar *)value, -1);
  return G_TLS_INTERACTION_HANDLED;
}

static void
ask_password_with_getpass (GTask        *task,
                           gpointer      object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  GTlsPassword *password = task_data;
  GError *error = NULL;

  g_tls_console_interaction_ask_password (G_TLS_INTERACTION (object), password,
                                          cancellable, &error);
  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, G_TLS_INTERACTION_HANDLED);
}

static void
g_tls_console_interaction_ask_password_async (GTlsInteraction    *interaction,
                                              GTlsPassword       *password,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data)
{
  GTask *task;

  task = g_task_new (interaction, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (password), g_object_unref);
  g_task_run_in_thread (task, ask_password_with_getpass);
  g_object_unref (task);
}

static GTlsInteractionResult
g_tls_console_interaction_ask_password_finish (GTlsInteraction    *interaction,
                                               GAsyncResult       *result,
                                               GError            **error)
{
  GTlsInteractionResult ret;

  g_return_val_if_fail (g_task_is_valid (result, interaction),
                        G_TLS_INTERACTION_FAILED);

  ret = g_task_propagate_int (G_TASK (result), error);
  if (ret == (GTlsInteractionResult)-1)
    return G_TLS_INTERACTION_FAILED;
  else
    return ret;
}

static void
g_tls_console_interaction_init (GTlsConsoleInteraction *interaction)
{

}

static void
g_tls_console_interaction_class_init (GTlsConsoleInteractionClass *klass)
{
  GTlsInteractionClass *interaction_class = G_TLS_INTERACTION_CLASS (klass);
  interaction_class->ask_password = g_tls_console_interaction_ask_password;
  interaction_class->ask_password_async = g_tls_console_interaction_ask_password_async;
  interaction_class->ask_password_finish = g_tls_console_interaction_ask_password_finish;
}

GTlsInteraction *
g_tls_console_interaction_new (void)
{
  return g_object_new (G_TYPE_TLS_CONSOLE_INTERACTION, NULL);
}
