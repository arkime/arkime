/* libgplugin_b.c - test plugin for testgmodule
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

#include        <gmodule.h>

G_MODULE_EXPORT gchar* gplugin_b_state;

G_MODULE_EXPORT const gchar* g_module_check_init (GModule *module);
G_MODULE_EXPORT void   g_module_unload (GModule *module);

G_MODULE_EXPORT void gplugin_b_func (void);
G_MODULE_EXPORT void gplugin_clash_func (void);
G_MODULE_EXPORT void g_clash_func (void);
G_MODULE_EXPORT void gplugin_say_boo_func (void);

G_MODULE_EXPORT const gchar*
g_module_check_init (GModule *module)
{
  gplugin_b_state = "check-init";

  return NULL;
}

G_MODULE_EXPORT void
g_module_unload (GModule *module)
{
  gplugin_b_state = "unloaded";
}

G_MODULE_EXPORT void
gplugin_b_func (void)
{
  gplugin_b_state = "Hello world";
}

G_MODULE_EXPORT void
gplugin_clash_func (void)
{
  gplugin_b_state = "plugin clash";
}

G_MODULE_EXPORT void
g_clash_func (void)
{
  gplugin_b_state = "global clash";
}

G_MODULE_EXPORT void
gplugin_say_boo_func (void)
{
  gplugin_b_state = "BOOH";
}
