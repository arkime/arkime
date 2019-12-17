inotify_sources = [
  'inotify-kernel.c',
  'inotify-sub.c',
  'inotify-path.c',
  'inotify-missing.c',
  'inotify-helper.c',
  'ginotifyfilemonitor.c',
]

inotify_lib = static_library('inotify',
  sources : inotify_sources,
  include_directories : [configinc, glibinc, gmoduleinc],
  dependencies : [gioenumtypes_dep],
  pic : true,
  c_args : [ '-DG_DISABLE_DEPRECATED' ] + gio_c_args)
