gmoduleconf_conf = configuration_data()

g_module_need_uscore = 0
g_module_broken_rtld_global = 0
g_module_have_dlerror = 0

libdl_dep = [ ]
g_module_lib_args = [ ]
g_module_impl = ''

dlopen_dlsym_test_code = '''
#include <dlfcn.h>
int glib_underscore_test (void) { return 42; }
int main (int argc, char ** argv) {
  void *f1 = (void*)0, *f2 = (void*)0, *handle;
  handle = dlopen ((void*)0, 0);
  if (handle) {
    f1 = dlsym (handle, "glib_underscore_test");
    f2 = dlsym (handle, "_glib_underscore_test");
  }
  return (!f2 || f1);
}'''

# On Windows force native WIN32 shared lib loader
if host_system == 'windows'
  g_module_impl = 'G_MODULE_IMPL_WIN32'
# Force native AIX library loader
# dlopen() filepath must be of the form /path/libname.a(libname.so)
elif host_system == 'aix'
  g_module_impl = 'G_MODULE_IMPL_AR'
elif cc.links(dlopen_dlsym_test_code, name : 'dlopen() and dlsym() in system libraries')
  g_module_impl = 'G_MODULE_IMPL_DL'
# NSLinkModule (dyld) in system libraries (Darwin)
elif cc.has_function('NSLinkModule')
  g_module_impl = 'G_MODULE_IMPL_DYLD'
  g_module_need_uscore = 1
elif cc.links(dlopen_dlsym_test_code, args : '-ldl', name : 'dlopen() and dlsym() in libdl')
  g_module_impl = 'G_MODULE_IMPL_DL'
  libdl_dep = cc.find_library('dl')
  g_module_lib_args = '-ldl'
endif

# additional checks for G_MODULE_IMPL_DL
if g_module_impl == 'G_MODULE_IMPL_DL'
  # FIXME: check for OSF1/5.0 RTLD_GLOBAL brokenness (is this still relevant?)

  # Check whether we need preceding underscores
  if cc.get_id() == 'msvc'
    message('Building for MSVC: assuming that symbols are prefixed with underscore')
    g_module_need_uscore = 1
  elif meson.has_exe_wrapper()
    # FIXME: communicate result via stdout instead of return value, so non-0 return is not printed in bold red
    rres = cc.run(dlopen_dlsym_test_code,
                  args : g_module_lib_args,
                  name : 'dlsym() preceding underscores')
    if host_system == 'windows' or rres.returncode() == 0
      g_module_need_uscore = 1
    endif
  else
    message('Cross-compiling: assuming that symbols aren\'t prefixed with underscore')
    g_module_need_uscore = 0
  endif

  if cc.has_function('dlerror', args : g_module_lib_args)
    g_module_have_dlerror = 1
  endif
endif

# Done, have we got an implementation?
if g_module_impl == ''
  g_module_impl = '0'
  message('WARNING: No suitable GModule implementation found!')
endif

gmoduleconf_conf.set('G_MODULE_IMPL', g_module_impl)
gmoduleconf_conf.set('G_MODULE_SUPPORTED', g_module_impl != '0')
gmoduleconf_conf.set('G_MODULE_HAVE_DLERROR', g_module_have_dlerror)
gmoduleconf_conf.set('G_MODULE_NEED_USCORE', g_module_need_uscore)
gmoduleconf_conf.set('G_MODULE_BROKEN_RTLD_GLOBAL', g_module_broken_rtld_global)

gmoduleconf_h = configure_file(input : 'gmoduleconf.h.in',
                               output : 'gmoduleconf.h',
                               configuration : gmoduleconf_conf)

install_headers(['gmodule.h'], subdir : 'glib-2.0')

gmodule_sources = ['gmodule.c']
if host_system == 'windows'
  gmodule_win_rc = configure_file(
    input: 'gmodule.rc.in',
    output: 'gmodule.rc',
    configuration: glibconfig_conf,
  )
  gmodule_win_res = windows.compile_resources(gmodule_win_rc)
  gmodule_sources += [gmodule_win_res]
endif

libgmodule = library('gmodule-2.0',
  sources : gmodule_sources,
  version : library_version,
  soversion : soversion,
  install : true,
  include_directories : [configinc, gmoduleinc],
  dependencies : [libdl_dep, libglib_dep],
  c_args : ['-DG_LOG_DOMAIN="GModule"', '-DG_DISABLE_DEPRECATED'] + glib_hidden_visibility_args)

libgmodule_dep = declare_dependency(link_with : libgmodule,
  include_directories : gmoduleinc)
