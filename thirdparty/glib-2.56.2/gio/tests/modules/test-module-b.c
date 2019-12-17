/* Test module for GIOModule tests
 * Copyright (C) 2013 Red Hat, Inc
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

#include "symbol-visibility.h"

typedef struct _TestB {
  GObject parent;
} TestB;

typedef struct _TestBClass {
  GObjectClass parent_class;
} TestBClass;

GType test_b_get_type (void);

G_DEFINE_TYPE (TestB, test_b, G_TYPE_OBJECT)

static void
test_b_class_init (TestBClass *class)
{
}

static void
test_b_init (TestB *self)
{
}

GLIB_TEST_EXPORT_SYMBOL void
g_io_module_load (GIOModule *module)
{
  g_io_extension_point_implement ("test-extension-point",
                                  test_b_get_type (),
                                  "test-b",
                                  40);
}

GLIB_TEST_EXPORT_SYMBOL void
g_io_module_unload (GIOModule *module)
{
}
