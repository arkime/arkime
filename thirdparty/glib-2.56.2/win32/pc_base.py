#!/usr/bin/python
#
# Simple utility script to generate the basic info
# needed in a .pc (pkg-config) file, used especially
# for introspection purposes

# This can be used in various projects where
# there is the need to generate .pc files,
# and is copied from GLib's $(srcroot)/win32

# Author: Fan, Chun-wei
# Date: March 10, 2016

import os
import sys
import argparse

class BasePCItems:
    def __init__(self):
        self.base_replace_items = {}
        self.exec_prefix = ''
        self.includedir = ''
        self.libdir = ''
        self.prefix = ''
        self.srcdir = os.path.dirname(__file__)
        self.top_srcdir = self.srcdir + '\\..'
        self.version = ''

    def setup(self, argv, parser=None):
        if parser is None:
            parser = argparse.ArgumentParser(description='Setup basic .pc file info')
        parser.add_argument('--prefix', help='prefix of the installed library',
                            required=True)
        parser.add_argument('--exec-prefix',
                            help='prefix of the installed programs, \
                                  if different from the prefix')
        parser.add_argument('--includedir',
                            help='includedir of the installed library, \
                                  if different from ${prefix}/include')
        parser.add_argument('--libdir',
                            help='libdir of the installed library, \
                                  if different from ${prefix}/lib')
        parser.add_argument('--version', help='Version of the package',
                            required=True)
        args = parser.parse_args()

        self.version = args.version

        # check whether the prefix and exec_prefix are valid
        if not os.path.exists(args.prefix):
            raise SystemExit('Specified prefix \'%s\' is invalid' % args.prefix)

        # use absolute paths for prefix
        self.prefix = os.path.abspath(args.prefix).replace('\\','/')

        # check and setup the exec_prefix
        if getattr(args, 'exec_prefix', None) is None:
            exec_prefix_use_shorthand = True
            self.exec_prefix = '${prefix}'
        else:
            if args.exec_prefix.startswith('${prefix}'):
                exec_prefix_use_shorthand = True
                input_exec_prefix = args.prefix + args.exec_prefix[len('${prefix}'):]
            else:
                exec_prefix_use_shorthand = False
                input_exec_prefix = args.exec_prefix
            if not os.path.exists(input_exec_prefix):
                raise SystemExit('Specified exec_prefix \'%s\' is invalid' %
                                  args.exec_prefix)
            if exec_prefix_use_shorthand is True:
                self.exec_prefix = args.exec_prefix.replace('\\','/')
            else:
                self.exec_prefix = os.path.abspath(input_exec_prefix).replace('\\','/')

        # check and setup the includedir
        if getattr(args, 'includedir', None) is None:
            self.includedir = '${prefix}/include'
        else:
            if args.includedir.startswith('${prefix}'):
                includedir_use_shorthand = True
                input_includedir = args.prefix + args.includedir[len('${prefix}'):]
            else:
                if args.includedir.startswith('${exec_prefix}'):
                    includedir_use_shorthand = True
                    input_includedir = input_exec_prefix + args.includedir[len('${exec_prefix}'):]
                else:
                    includedir_use_shorthand = False
                    input_includedir = args.includedir
            if not os.path.exists(input_includedir):
                raise SystemExit('Specified includedir \'%s\' is invalid' %
                                  args.includedir)
            if includedir_use_shorthand is True:
                self.includedir = args.includedir.replace('\\','/')
            else:
                self.includedir = os.path.abspath(input_includedir).replace('\\','/')

        # check and setup the libdir
        if getattr(args, 'libdir', None) is None:
            self.libdir = '${prefix}/lib'
        else:
            if args.libdir.startswith('${prefix}'):
                libdir_use_shorthand = True
                input_libdir = args.prefix + args.libdir[len('${prefix}'):]
            else:
                if args.libdir.startswith('${exec_prefix}'):
                    libdir_use_shorthand = True
                    input_libdir = input_exec_prefix + args.libdir[len('${exec_prefix}'):]
                else:
                    libdir_use_shorthand = False
                    input_libdir = args.libdir
            if not os.path.exists(input_libdir):
                raise SystemExit('Specified libdir \'%s\' is invalid' %
			                      args.libdir)
            if libdir_use_shorthand is True:
                self.libdir = args.libdir.replace('\\','/')
            else:
                self.libdir = os.path.abspath(input_libdir).replace('\\','/')

        # setup dictionary for replacing items in *.pc.in
        self.base_replace_items.update({'@VERSION@': self.version})
        self.base_replace_items.update({'@prefix@': self.prefix})
        self.base_replace_items.update({'@exec_prefix@': self.exec_prefix})
        self.base_replace_items.update({'@libdir@': self.libdir})
        self.base_replace_items.update({'@includedir@': self.includedir})
