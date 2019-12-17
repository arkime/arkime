#!/usr/bin/python
#
# Simple utility script to manipulate
# certain types of strings in a file

# This can be used in various projects where
# there is the need to replace strings in files,
# and is copied from GLib's $(srcroot)/win32

# Author: Fan, Chun-wei
# Date: September 03, 2014

import os
import sys
import re
import string
import argparse

valid_actions = ['remove-prefix',
                 'replace-var',
                 'replace-str',
                 'remove-str']

def open_file(filename, mode):
    if sys.version_info[0] < 3:
        return open(filename, mode=mode)
    else:
        return open(filename, mode=mode, encoding='utf-8')

def replace_multi(src, dest, replace_items):
    with open_file(src, 'r') as s:
        with open_file(dest, 'w') as d:
            for line in s:
                replace_dict = dict((re.escape(key), value) \
                               for key, value in replace_items.items())
                replace_pattern = re.compile("|".join(replace_dict.keys()))
                d.write(replace_pattern.sub(lambda m: \
                        replace_dict[re.escape(m.group(0))], line))

def replace(src, dest, instring, outstring):
    replace_item = {instring: outstring}
    replace_multi(src, dest, replace_item)

def check_required_args(args, params):
    for param in params:
        if getattr(args, param, None) is None:
            raise SystemExit('%s: error: --%s argument is required' % (__file__, param))

def warn_ignored_args(args, params):
    for param in params:
        if getattr(args, param, None) is not None:
            print('%s: warning: --%s argument is ignored' % (__file__, param))

def main(argv):

    parser = argparse.ArgumentParser(description='Process strings in a file.')
    parser.add_argument('-a',
                        '--action',
                        help='Action to carry out.  Can be one of:\n'
                             'remove-prefix\n'
                             'replace-var\n'
                             'replace-str\n'
                             'remove-str',
                        choices=valid_actions)
    parser.add_argument('-i', '--input', help='Input file')
    parser.add_argument('-o', '--output', help='Output file')
    parser.add_argument('--instring', help='String to replace or remove')
    parser.add_argument('--var', help='Autotools variable name to replace')
    parser.add_argument('--outstring',
                        help='New String to replace specified string or variable')
    parser.add_argument('--removeprefix', help='Prefix of string to remove')

    args = parser.parse_args()

    input_string = ''
    output_string = ''

    # We must have action, input, output for all operations
    check_required_args(args, ['action','input','output'])

    # Build the arguments by the operation that is to be done,
    # to be fed into replace()

    # Get rid of prefixes from a string
    if args.action == 'remove-prefix':
        check_required_args(args, ['instring','removeprefix'])
        warn_ignored_args(args, ['outstring','var'])
        input_string = args.removeprefix + args.instring
        output_string = args.instring

    # Replace an m4-style variable (those surrounded by @...@)
    if args.action == 'replace-var':
        check_required_args(args, ['var','outstring'])
        warn_ignored_args(args, ['instring','removeprefix'])
        input_string = '@' + args.var + '@'
        output_string = args.outstring

    # Replace a string
    if args.action == 'replace-str':
        check_required_args(args, ['instring','outstring'])
        warn_ignored_args(args, ['var','removeprefix'])
        input_string = args.instring
        output_string = args.outstring

    # Remove a string
    if args.action == 'remove-str':
        check_required_args(args, ['instring'])
        warn_ignored_args(args, ['var','outstring','removeprefix'])
        input_string = args.instring
        output_string = ''

    replace(args.input, args.output, input_string, output_string)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
