/* Unit tests for GPermission
 * Copyright (C) 2012 Red Hat, Inc
 * Author: Matthias Clasen
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gio/gio.h>

static void
acquired (GObject      *source,
          GAsyncResult *res,
          gpointer      user_data)
{
  GPermission *p = G_PERMISSION (source);
  GMainLoop *loop = user_data;
  GError *error = NULL;
  gboolean ret;

  ret = g_permission_acquire_finish (p, res, &error);
  g_assert (!ret);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  g_main_loop_quit (loop);
}

static void
released (GObject      *source,
          GAsyncResult *res,
          gpointer      user_data)
{
  GPermission *p = G_PERMISSION (source);
  GMainLoop *loop = user_data;
  GError *error = NULL;
  gboolean ret;

  ret = g_permission_release_finish (p, res, &error);
  g_assert (!ret);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  g_main_loop_quit (loop);
}

static void
test_simple (void)
{
  GPermission *p;
  gboolean allowed;
  gboolean can_acquire;
  gboolean can_release;
  gboolean ret;
  GError *error = NULL;
  GMainLoop *loop;

  p = g_simple_permission_new (TRUE);

  g_assert (g_permission_get_allowed (p));
  g_assert (!g_permission_get_can_acquire (p));
  g_assert (!g_permission_get_can_release (p));

  g_object_get (p,
                "allowed", &allowed,
                "can-acquire", &can_acquire,
                "can-release", &can_release,
                NULL);

  g_assert (allowed);
  g_assert (!can_acquire);
  g_assert (!can_release);

  ret = g_permission_acquire (p, NULL, &error);
  g_assert (!ret);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  ret = g_permission_release (p, NULL, &error);
  g_assert (!ret);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  loop = g_main_loop_new (NULL, FALSE);
  g_permission_acquire_async (p, NULL, acquired, loop);
  g_main_loop_run (loop);
  g_permission_release_async (p, NULL, released, loop);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  g_object_unref (p);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/permission/simple", test_simple);

  return g_test_run ();
}
