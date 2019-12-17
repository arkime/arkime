gnulib_lib = static_library('gnulib', 'asnprintf.c', 'printf.c', 'printf-args.c', 'printf-parse.c', 'vasnprintf.c',
  include_directories : [configinc, glibinc],
  pic : true,
  c_args : [ '-DLIBDIR="@0@"'.format(get_option('libdir')), '-DGLIB_COMPILATION', '-DG_LOG_DOMAIN="GLib"' ] + glib_hidden_visibility_args)
