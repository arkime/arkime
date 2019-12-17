# Just a skeleton lib for backwards compatibility since all the functionaliy
# has been moved into glib now

gthread_sources = ['gthread-impl.c']
if host_system == 'windows'
  gthread_win_rc = configure_file(
    input: 'gthread.rc.in',
    output: 'gthread.rc',
    configuration: glibconfig_conf,
  )
  gthread_win_res = windows.compile_resources(gthread_win_rc)
  gthread_sources += [gthread_win_res]
endif

libgthread = library('gthread-2.0',
  sources : gthread_sources,
  version : library_version,
  soversion : soversion,
  install : true,
  dependencies : [libglib_dep],
  c_args : ['-DG_LOG_DOMAIN="GThread"' ] + glib_hidden_visibility_args)
