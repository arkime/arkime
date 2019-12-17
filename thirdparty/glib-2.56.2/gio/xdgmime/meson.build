xdgmime_sources = files(
  'xdgmime.c',
  'xdgmimealias.c',
  'xdgmimecache.c',
  'xdgmimeglob.c',
  'xdgmimeicon.c',
  'xdgmimeint.c',
  'xdgmimemagic.c',
  'xdgmimeparent.c',
)

xdgmime_lib = static_library('xdgmime',
  sources : xdgmime_sources,
  include_directories : [configinc],
  pic : true,
  c_args : [ '-DXDG_PREFIX=_gio_xdg' ] + glib_hidden_visibility_args)
