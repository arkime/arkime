charsetalias_dir = get_option('charsetalias_dir')
if charsetalias_dir == ''
  charsetalias_dir = get_option('libdir')
endif

charset_lib = static_library('charset', 'localcharset.c',
  include_directories : configinc,
  pic : true,
  c_args : [ '-DGLIB_CHARSETALIAS_DIR="@0@"'.format(charsetalias_dir) ] + glib_hidden_visibility_args)
