# Simple Python script to generate the full .schema.xml files

import os
import sys
import argparse

from replace import replace_multi

def main(argv):
    srcroot = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..')
    parser = argparse.ArgumentParser(description='Generate Utility Scripts')
    parser.add_argument('-t', '--type', help='Script Type (glib-mkenums or gdbus-codegen)', required=True)
    parser.add_argument('--version', help='Package Version', required=True)
    args = parser.parse_args()

    replace_items = {'@PYTHON@': 'python',
                     '@GLIB_VERSION@': args.version,
                     '@VERSION@': args.version}

    if args.type == 'glib-mkenums':
        replace_multi(srcroot + '/gobject/glib-mkenums.in',
                      srcroot + '/gobject/glib-mkenums',
                      replace_items)
    elif args.type == 'glib-genmarshal':
        replace_multi(srcroot + '/gobject/glib-genmarshal.in',
                      srcroot + '/gobject/glib-genmarshal',
                      replace_items)
    elif args.type == 'gdbus-codegen':
        replace_multi(srcroot + '/gio/gdbus-2.0/codegen/gdbus-codegen.in',
                      srcroot + '/gio/gdbus-2.0/codegen/gdbus-codegen',
                      replace_items)

    else:
        raise ValueError('Type must be glib-mkenums, glib-genmarshal or gdbus-codegen')

if __name__ == '__main__':
    sys.exit(main(sys.argv))
