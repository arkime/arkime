/* libgplugin_a.c - test plugin for testgmodule
 * Copyright (C) 1998 Tim Janik
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

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include	<gmodule.h>
#include	<stdlib.h>

G_MODULE_EXPORT void gplugin_a_func (void);
G_MODULE_EXPORT void gplugin_clash_func (void);
G_MODULE_EXPORT void g_clash_func (void);
G_MODULE_EXPORT void gplugin_say_boo_func (void);
G_MODULE_EXPORT void gplugin_a_module_func (GModule *module);

G_MODULE_EXPORT gchar* gplugin_a_state;

G_MODULE_EXPORT void
gplugin_a_func (void)
{
  gplugin_a_state = "Hello world";
}

G_MODULE_EXPORT void
gplugin_clash_func (void)
{
  gplugin_a_state = "plugin clash";
}

G_MODULE_EXPORT void
g_clash_func (void)
{
  gplugin_a_state = "global clash";
}

G_MODULE_EXPORT void
gplugin_say_boo_func (void)
{
  gplugin_a_state = "BOOH";
}

G_MODULE_EXPORT void
gplugin_a_module_func (GModule *module)
{
  void *f = NULL;

  if (!g_module_symbol (module, "gplugin_say_boo_func", &f ))
    {
      g_print ("error: %s\n", g_module_error ());
      exit (1);
    }

  ((void(*)(void)) f) ();
}
