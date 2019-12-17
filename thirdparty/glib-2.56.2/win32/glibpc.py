#!/usr/bin/python
#
# Utility script to generate .pc files for GLib
# for Visual Studio builds, to be used for
# building introspection files

# Author: Fan, Chun-wei
# Date: March 10, 2016

import os
import sys

from replace import replace_multi
from pc_base import BasePCItems

def main(argv):
    base_pc = BasePCItems()

    base_pc.setup(argv)
    pkg_replace_items = {'@G_THREAD_CFLAGS@': '',
                         '@G_THREAD_LIBS@': '',
                         '@CARBON_LIBS@': '',
                         '@COCOA_LIBS@': ''}

    glib_replace_items = {'glib-genmarshal': '${exec_prefix}/bin/glib-genmarshal',
              	          'glib-mkenums': 'perl ${exec_prefix}/bin/glib-mkenums',
              	          'gobject-query': '${exec_prefix}/bin/gobject-query',
              	          '@PCRE_REQUIRES@': '',
              	          '@INTLLIBS@': '-lintl',
              	          '@G_LIBS_EXTRA@': '',
              	          '@PCRE_LIBS@': '',
              	          '@ICONV_LIBS@': '-liconv',
              	          '@GLIB_EXTRA_CFLAGS@': ''}

    pkg_replace_items.update(base_pc.base_replace_items)
						 
    glib_replace_items.update(pkg_replace_items)

    # Generate glib-2.0.pc
    replace_multi(base_pc.top_srcdir + '/glib-2.0.pc.in',
                  base_pc.srcdir + '/glib-2.0.pc',
                  glib_replace_items)

    # Generate gthread-2.0.pc
    replace_multi(base_pc.top_srcdir + '/gthread-2.0.pc.in',
                  base_pc.srcdir + '/gthread-2.0.pc',
                  pkg_replace_items)

    # Generate gmodule*-2.0.pc
    gmodule_replace_items = {'@G_MODULE_SUPPORTED@': 'yes',
              	             '@G_MODULE_LDFLAGS@': '',
              	             '@G_MODULE_LIBS@': ''}
    gmodule_replace_items.update(pkg_replace_items)
    replace_multi(base_pc.top_srcdir + '/gmodule-2.0.pc.in',
                  base_pc.srcdir + '/gmodule-2.0.pc',
                  gmodule_replace_items)
    replace_multi(base_pc.top_srcdir + '/gmodule-export-2.0.pc.in',
                  base_pc.srcdir + '/gmodule-export-2.0.pc',
                  gmodule_replace_items)
    replace_multi(base_pc.top_srcdir + '/gmodule-no-export-2.0.pc.in',
                  base_pc.srcdir + '/gmodule-no-export-2.0.pc',
                  gmodule_replace_items)

    # Generate gobject-2.0.pc
    gobject_replace_items = {'@LIBFFI_LIBS@': ''}
    gobject_replace_items.update(pkg_replace_items)
    replace_multi(base_pc.top_srcdir + '/gobject-2.0.pc.in',
                  base_pc.srcdir + '/gobject-2.0.pc',
                  gobject_replace_items)

    # Generate gio*-2.0.pc
    gio_replace_items = {'@GIO_MODULE_DIR@': '${exec_prefix}/bin/gio/modules',
                         '@ZLIB_LIBS@': '-lzlib1',
                         '@NETWORK_LIBS@': '-lws2_32',
                         '@SELINUX_LIBS@': '',
                         '@LIBMOUNT_LIBS@': '',
                         'glib-compile-schemas': '${exec_prefix}/bin/glib-compile-schemas',
                         'glib-compile-resources': '${exec_prefix}/bin/glib-compile-resources',
                         'gdbus-codegen': 'python ${exec_prefix}/bin/gdbus-codegen'}
    gio_replace_items.update(pkg_replace_items)
    replace_multi(base_pc.top_srcdir + '/gio-2.0.pc.in',
                  base_pc.srcdir + '/gio-2.0.pc',
                  gio_replace_items)
    replace_multi(base_pc.top_srcdir + '/gio-windows-2.0.pc.in',
                  base_pc.srcdir + '/gio-windows-2.0.pc',
                  pkg_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
