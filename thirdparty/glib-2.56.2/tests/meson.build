# tests

test_env = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'G_DEBUG=gc-friendly',
  'MALLOC_CHECK_=2',
]

test_cargs = ['-DG_LOG_DOMAIN="GLib"']

subdir('gobject')
subdir('refcount')
