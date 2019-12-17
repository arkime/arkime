kqueue_sources = [
  'gkqueuefilemonitor.c',
  'kqueue-helper.c',
  'kqueue-thread.c',
  'kqueue-sub.c',
  'kqueue-missing.c',
  'kqueue-utils.c',
  'kqueue-exclusions.c',
  'dep-list.c',
  # gkqueuefilemonitor.h includes gio.h which includes this
  gioenumtypes_h,
]

kqueue_lib = static_library('kqueue',
  sources : kqueue_sources,
  include_directories : [configinc, glibinc, gmoduleinc],
  pic : true,
  c_args : [ '-DG_DISABLE_DEPRECATED' ] + gio_c_args)
