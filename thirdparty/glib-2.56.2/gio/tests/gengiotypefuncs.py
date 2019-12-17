#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
import os

debug = os.getenv('GIO_GENTYPEFUNCS_DEBUG') is not None

out_file = sys.argv[1]
in_files = sys.argv[2:]

funcs = []


if debug: print ('Output file: ', out_file)

if debug: print (len(in_files), 'input files')

for filename in in_files:
  if debug: print ('Input file: ', filename)
  with open(filename, "r") as f:
    for line in f:
      line = line.rstrip('\n').rstrip('\r')
      # print line
      match = re.search(r'\bg_[a-zA-Z0-9_]*_get_type\b', line)
      if match:
        func = match.group(0)
        if not func in funcs:
          funcs.append(func)
          if debug: print ('Found ', func)

file_output = 'G_GNUC_BEGIN_IGNORE_DEPRECATIONS\n'

funcs = sorted(funcs)

for f in funcs:
  if f not in ['g_io_extension_get_type', 'g_settings_backend_get_type']:
    file_output += '*tp++ = {0} ();\n'.format(f)

if debug: print (len(funcs), 'functions')

ofile = open(out_file, "w")
ofile.write(file_output)
ofile.close()
