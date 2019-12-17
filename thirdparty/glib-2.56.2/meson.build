project('glib', 'c', 'cpp',
  version : '2.56.2',
  meson_version : '>= 0.44.0',
  default_options : [
    'warning_level=1',
    'c_std=gnu89'
  ]
)

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

if cc.get_id() == 'msvc'
  # Ignore several spurious warnings for things glib does very commonly
  # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
  # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
  # NOTE: Only add warnings here if you are sure they're spurious
  add_project_arguments('/wd4035', '/wd4715', '/wd4116',
    '/wd4046', '/wd4068', '/wo4090', '/FImsvc_recommended_pragmas.h',language : 'c')
  # Disable SAFESEH with MSVC for plugins and libs that use external deps that
  # are built with MinGW
  noseh_link_args = ['/SAFESEH:NO']
else
  noseh_link_args = []
  # -mms-bitfields vs -fnative-struct ?
endif

host_system = host_machine.system()

glib_version = meson.project_version()
glib_api_version = '2.0'
version_arr = glib_version.split('.')
major_version = version_arr[0].to_int()
minor_version = version_arr[1].to_int()
micro_version = version_arr[2].to_int()

interface_age = minor_version.is_odd() ? 0 : micro_version
binary_age = 100 * minor_version + micro_version

soversion = 0
# Maintain compatibility with previous libtool versioning
# current = minor * 100 + micro
library_version = '@0@.@1@.@2@'.format(soversion, binary_age - interface_age, interface_age)

configinc = include_directories('.')
glibinc = include_directories('glib')
gobjectinc = include_directories('gobject')
gmoduleinc = include_directories('gmodule')
gioinc = include_directories('gio')

glib_prefix = get_option('prefix')
glib_bindir = join_paths(glib_prefix, get_option('bindir'))
glib_libdir = join_paths(glib_prefix, get_option('libdir'))
glib_datadir = join_paths(glib_prefix, get_option('datadir'))
glib_pkgdatadir = join_paths(glib_datadir, 'glib-2.0')
glib_includedir = join_paths(glib_prefix, get_option('includedir'))
glib_giomodulesdir = get_option('gio_module_dir')
if glib_giomodulesdir == ''
  glib_giomodulesdir = join_paths(glib_libdir, 'gio', 'modules')
endif

glib_pkgconfigreldir = join_paths(glib_libdir, 'pkgconfig')

add_project_arguments('-D_GNU_SOURCE', language: 'c')

# Disable strict aliasing;
# see https://bugzilla.gnome.org/show_bug.cgi?id=791622
if cc.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'c')
endif

########################
# Configuration begins #
########################
glib_conf = configuration_data()
glibconfig_conf = configuration_data()

# accumulated list of defines as we check for them, so we can easily
# use them later in test programs (autoconf does this automatically)
glib_conf_prefix = ''

glib_conf.set('GLIB_VERSION', glib_version)
glib_conf.set('GLIB_MAJOR_VERSION', major_version)
glib_conf.set('GLIB_MINOR_VERSION', minor_version)
glib_conf.set('GLIB_MICRO_VERSION', micro_version)
glib_conf.set('GLIB_INTERFACE_AGE', interface_age)
glib_conf.set('GLIB_BINARY_AGE', binary_age)
glib_conf.set_quoted('GETTEXT_PACKAGE', 'glib20')
glib_conf.set_quoted('PACKAGE_BUGREPORT', 'http://bugzilla.gnome.org/enter_bug.cgi?product=glib')
glib_conf.set_quoted('PACKAGE_NAME', 'glib')
glib_conf.set_quoted('PACKAGE_STRING', 'glib @0@'.format(meson.project_version()))
glib_conf.set_quoted('PACKAGE_TARNAME', 'glib')
glib_conf.set_quoted('PACKAGE_URL', '')
glib_conf.set_quoted('PACKAGE_VERSION', meson.project_version())
glib_conf.set('ENABLE_NLS', 1)

# Variables used in glib-gettextize and pkg-config files
# These should not contain " quotes around the values
glib_conf.set('PACKAGE', 'glib')
glib_conf.set('VERSION', meson.project_version())
glib_conf.set('prefix', glib_prefix)
glib_conf.set('exec_prefix', glib_prefix)
glib_conf.set('libdir', glib_libdir)
glib_conf.set('includedir', glib_includedir)
glib_conf.set('datadir', glib_datadir)
glib_conf.set('datarootdir', glib_datadir)

glib_conf.set('_GNU_SOURCE', 1)

if host_system == 'windows'
  # Poll doesn't work on devices on Windows
  glib_conf.set('BROKEN_POLL', true)
endif

# Detect and set symbol visibility
glib_hidden_visibility_args = []
if get_option('default_library') != 'static'
  if host_system == 'windows'
    glib_conf.set('DLL_EXPORT', true)
    if cc.get_id() == 'msvc'
      glib_conf.set('_GLIB_EXTERN', '__declspec(dllexport) extern')
    elif cc.has_argument('-fvisibility=hidden')
      glib_conf.set('_GLIB_EXTERN', '__attribute__((visibility("default"))) __declspec(dllexport) extern')
      glib_hidden_visibility_args = ['-fvisibility=hidden']
    endif
  elif cc.has_argument('-fvisibility=hidden')
    glib_conf.set('_GLIB_EXTERN', '__attribute__((visibility("default"))) extern')
    glib_hidden_visibility_args = ['-fvisibility=hidden']
  endif
endif

if host_system == 'windows' and get_option('default_library') == 'static'
    glibconfig_conf.set('GLIB_STATIC_COMPILATION', '1')
    glibconfig_conf.set('GOBJECT_STATIC_COMPILATION', '1')
endif

# FIXME: what about Cygwin (G_WITH_CYGWIN)
if host_system == 'windows'
  glib_os = '''#define G_OS_WIN32
#define G_PLATFORM_WIN32'''
else
  glib_os = '#define G_OS_UNIX'
endif
glibconfig_conf.set('glib_os', glib_os)

# We need to know the build type to determine what .lib files we need on Visual Studio
# for dependencies that don't normally come with pkg-config files for Visual Studio builds
buildtype = get_option('buildtype')

# check for header files

headers = [
  'stdlib.h',
  'string.h',
  'strings.h',
  'memory.h',
  'alloca.h',
  'locale.h',
  'xlocale.h',
  'float.h',
  'limits.h',
  'pwd.h',
  'grp.h',
  'poll.h',
  'termios.h',
  'sys/param.h',
  'sys/resource.h',
  'mach/mach_time.h',
  'sys/select.h',
  'stdint.h',
  'inttypes.h',
  'sched.h',
  'malloc.h',
  'sys/vfs.h',
  'sys/vmount.h',
  'sys/statfs.h',
  'sys/statvfs.h',
  'sys/filio.h',
  'mntent.h',
  'sys/mnttab.h',
  'sys/vfstab.h',
  'sys/mntctl.h',
  'fstab.h',
  'linux/magic.h',
  'termios.h',
  'dirent.h', # MSC does not come with this by default
  'sys/time.h', # MSC does not come with this by default
  'sys/times.h',
  'sys/wait.h',
  'unistd.h',
  'values.h',
  'sys/types.h',
  'sys/uio.h',
  'sys/mkdev.h',
  'sys/mount.h',
  'sys/sysctl.h',
  'crt_externs.h',
  'sys/inotify.h',
  'sys/event.h',
  'sys/stat.h',
]

foreach h : headers
  if cc.has_header(h)
    define = 'HAVE_' + h.underscorify().to_upper()
    glib_conf.set(define, 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(define)
  endif
endforeach

if cc.has_header('linux/netlink.h')
  glib_conf.set('HAVE_NETLINK', 1)
endif

if glib_conf.has('HAVE_LOCALE_H')
  if cc.has_header_symbol('locale.h', 'LC_MESSAGES')
    glib_conf.set('HAVE_LC_MESSAGES', 1)
  endif
endif

struct_stat_blkprefix = '''
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
'''

struct_members = [
  [ 'stat', 'st_mtimensec' ],
  [ 'stat', 'st_mtim.tv_nsec' ],
  [ 'stat', 'st_atimensec' ],
  [ 'stat', 'st_atim.tv_nsec' ],
  [ 'stat', 'st_ctimensec' ],
  [ 'stat', 'st_ctim.tv_nsec' ],
  [ 'stat', 'st_birthtime' ],
  [ 'stat', 'st_birthtimensec' ],
  [ 'stat', 'st_birthtim' ],
  [ 'stat', 'st_birthtim.tv_nsec' ],
  [ 'stat', 'st_blksize', struct_stat_blkprefix ],
  [ 'stat', 'st_blocks', struct_stat_blkprefix ],
  [ 'statfs', 'f_fstypename', struct_stat_blkprefix ],
  [ 'statfs', 'f_bavail', struct_stat_blkprefix ],
  [ 'dirent', 'd_type', '''#include <sys/types.h>
                           #include <dirent.h>''' ],
]

foreach m : struct_members
  header_check_prefix = glib_conf_prefix
  if m.length() == 3
    header_check_prefix = header_check_prefix + m[2]
  else
    header_check_prefix = header_check_prefix + '#include <sys/stat.h>'
  endif
  if cc.has_member('struct ' + m[0], m[1], prefix : header_check_prefix)
    define = 'HAVE_STRUCT_@0@_@1@'.format(m[0].to_upper(), m[1].underscorify().to_upper())
    glib_conf.set(define, 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(define)
  else
  endif
endforeach

# Compiler flags
if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  test_c_args = [
    '-Wall',
    '-Wduplicated-branches',
    '-Wstrict-prototypes',
    '-Werror=declaration-after-statement',
    '-Werror=format=2',
    '-Werror=format-security',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
  ]
else
  test_c_args = []
endif

add_project_arguments(cc.get_supported_arguments(test_c_args), language: 'c')

# Windows Support (Vista+)
if host_system == 'windows'
  glib_conf.set('_WIN32_WINNT', '0x0601')
endif

functions = [
  'alloca',
  'mmap',
  'posix_memalign',
  'memalign',
  'valloc',
  'fsync',
  'pipe2',
  'issetugid',
  'timegm',
  'gmtime_r',
  'strerror_r',
  'lstat',
  'strsignal',
  'vsnprintf',
  'stpcpy',
  'poll',
  'vasprintf',
  'setenv',
  'unsetenv',
  'getc_unlocked',
  'readlink',
  'symlink',
  'fdwalk',
  'lchmod',
  'lchown',
  'fchmod',
  'fchown',
  'utimes',
  'getresuid',
  'getmntent_r',
  'setmntent',
  'endmntent',
  'hasmntopt',
  'getfsstat',
  'getvfsstat',
  'fallocate',
  'localtime_r',
  'gmtime_r',
  'getpwuid_r',
  'getgrgid_r',
  'prlimit',
  'strnlen',
  'wcslen',
  'wcsnlen',
  'mbrtowc',
  'wcrtomb',
  'newlocale',
  'uselocale',
  'strtod_l',
  'strtoll_l',
  'strtoull_l',
  'inotify_init1',
  'kqueue',
  'kevent',
  'endservent',
  'sendmmsg',
  'recvmmsg',
  'link',
]

if glib_conf.has('HAVE_SYS_STATVFS_H')
  functions += ['statvfs']
else
  have_func_statvfs = false
endif
if glib_conf.has('HAVE_SYS_STATFS_H') or glib_conf.has('HAVE_SYS_MOUNT_H')
  functions += ['statfs']
else
  have_func_statfs = false
endif

if host_system == 'windows'
  iphlpapi_dep = cc.find_library('iphlpapi')
  iphlpapi_funcs = ['if_nametoindex', 'if_indextoname']
  foreach ifunc : iphlpapi_funcs
    if cc.has_function(ifunc,
                       prefix : '#define _WIN32_WINNT 0x0601\n#include <winsock2.h>\n#include <iphlpapi.h>',
                       dependencies : iphlpapi_dep)
      idefine = 'HAVE_' + ifunc.underscorify().to_upper()
      glib_conf.set(idefine, 1)
      glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(idefine)
      set_variable('have_func_' + ifunc, true)
    else
      set_variable('have_func_' + ifunc, false)
    endif
  endforeach
else
  functions += ['if_indextoname', 'if_nametoindex']
endif

# AIX splice is something else
if host_system != 'aix'
  functions += ['splice']
endif

foreach f : functions
  if cc.has_function(f)
    define = 'HAVE_' + f.underscorify().to_upper()
    glib_conf.set(define, 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(define)
    set_variable('have_func_' + f, true)
  else
    set_variable('have_func_' + f, false)
  endif
endforeach

# Check whether strerror_r returns char *
if have_func_strerror_r
  if cc.compiles('''#define _GNU_SOURCE
                    #include <string.h>
                    int func (void) {
                      char error_string[256];
                      char *ptr = strerror_r (-2, error_string, 256);
                      char c = *strerror_r (-2, error_string, 256);
                      return c != 0 && ptr != (void*) 0L;
                    }
                 ''',
                 name : 'strerror_r() returns char *')
    glib_conf.set('STRERROR_R_CHAR_P', 1,
                  description: 'Defined if strerror_r returns char *')
  endif
endif

# Special-case these functions that have alternative names on Windows/MSVC
if cc.has_function('snprintf') or cc.has_header_symbol('stdio.h', 'snprintf')
  glib_conf.set('HAVE_SNPRINTF', 1)
  glib_conf_prefix = glib_conf_prefix + '#define HAVE_SNPRINTF 1\n'
elif cc.has_function('_snprintf') or cc.has_header_symbol('stdio.h', '_snprintf')
  hack_define = '1\n#define snprintf _snprintf'
  glib_conf.set('HAVE_SNPRINTF', hack_define)
  glib_conf_prefix = glib_conf_prefix + '#define HAVE_SNPRINTF ' + hack_define
endif

if cc.has_function('strcasecmp')
  glib_conf.set('HAVE_STRCASECMP', 1)
  glib_conf_prefix = glib_conf_prefix + '#define HAVE_STRCASECMP 1\n'
elif cc.has_function('_stricmp')
  hack_define = '1\n#define strcasecmp _stricmp'
  glib_conf.set('HAVE_STRCASECMP', hack_define)
  glib_conf_prefix = glib_conf_prefix + '#define HAVE_STRCASECMP ' + hack_define
endif

if cc.has_function('strncasecmp')
  glib_conf.set('HAVE_STRNCASECMP', 1)
  glib_conf_prefix = glib_conf_prefix + '#define HAVE_STRNCASECMP 1\n'
elif cc.has_function('_strnicmp')
  hack_define = '1\n#define strncasecmp _strnicmp'
  glib_conf.set('HAVE_STRNCASECMP', hack_define)
  glib_conf_prefix = glib_conf_prefix + '#define HAVE_STRNCASECMP ' + hack_define
endif

if cc.has_header_symbol('sys/sysmacros.h', 'major')
  glib_conf.set('MAJOR_IN_SYSMACROS', 1)
elif cc.has_header_symbol('sys/mkdev.h', 'major')
  glib_conf.set('MAJOR_IN_MKDEV', 1)
endif

if cc.has_header_symbol('dlfcn.h', 'RTLD_LAZY')
  glib_conf.set('HAVE_RTLD_LAZY', 1)
endif

if cc.has_header_symbol('dlfcn.h', 'RTLD_NOW')
  glib_conf.set('HAVE_RTLD_NOW', 1)
endif

if cc.has_header_symbol('dlfcn.h', 'RTLD_GLOBAL')
  glib_conf.set('HAVE_RTLD_GLOBAL', 1)
endif

# Check whether to use statfs or statvfs
# Some systems have both statfs and statvfs, pick the most "native" for these
if have_func_statfs and have_func_statvfs
  # on solaris and irix, statfs doesn't even have the f_bavail field
  if not glib_conf.has('HAVE_STRUCT_STATFS_F_BAVAIL')
    have_func_statfs = false
  else
    # at least on linux, statfs is the actual syscall
    have_func_statvfs = false
  endif
endif
if have_func_statfs
  glib_conf.set('USE_STATFS', 1)
  stat_func_to_use = 'statfs'
elif have_func_statvfs
  glib_conf.set('USE_STATVFS', 1)
  stat_func_to_use = 'statvfs'
else
  stat_func_to_use = 'neither'
endif
message('Checking whether to use statfs or statvfs .. ' + stat_func_to_use)

if host_system == 'linux'
  if cc.has_function('mkostemp',
                     prefix: '''#define _GNU_SOURCE
                                #include <stdlib.h>''')
    glib_conf.set('HAVE_MKOSTEMP', 1)
  endif
endif

platform_ldflags=[]

# Mac OS X Carbon support
glib_have_carbon = cc.compiles('''#include <Carbon/Carbon.h>
                                  #include <CoreServices/CoreServices.h>''',
                               name : 'Mac OS X Carbon support')

glib_have_os_x_9_or_later = false

if glib_have_carbon
  glib_conf.set('HAVE_CARBON', true)
  CARBON_LIBS='-Wl,-framework,Carbon' # FIXME: propagate to .pc files as well
  platform_ldflags += [CARBON_LIBS]
  glib_have_os_x_9_or_later = cc.compiles('''#include <AvailabilityMacros.h>
                                             #if MAC_OS_X_VERSION_MIN_REQUIRED < 1090
                                             #error Compiling for minimum OS X version before 10.9
                                             #endif''', name : 'OS X 9 or later')
else
  CARBON_LIBS=''
endif

# Mac OS X Cocoa support
glib_have_cocoa = cc.compiles('''#include <Cocoa/Cocoa.h>
                                 #ifdef GNUSTEP_BASE_VERSION
                                 #error "Detected GNUstep, not Cocoa"
                                 #endif''',
                              name : 'Mac OS X Cocoa support')

if glib_have_cocoa
  glib_conf.set('HAVE_COCOA', true)
  COCOA_LIBS='-Wl,-framework,Foundation -Wl,-framework,AppKit' # FIXME: propagate to .pc files as well
  platform_ldflags += [COCOA_LIBS]
else
  COCOA_LIBS=''
endif

# Check for futex(2)
if cc.links('''#include <linux/futex.h>
               #include <sys/syscall.h>
               #include <unistd.h>
               int main (int argc, char ** argv) {
                 syscall (__NR_futex, NULL, FUTEX_WAKE, FUTEX_WAIT);
                 return 0;
               }''', name : 'futex(2) system call')
  glib_conf.set('HAVE_FUTEX', 1)
endif

# Check for eventfd(2)
if cc.links('''#include <sys/eventfd.h>
               #include <unistd.h>
               int main (int argc, char ** argv) {
                 eventfd (0, EFD_CLOEXEC);
                 return 0;
               }''', name : 'eventfd(2) system call')
  glib_conf.set('HAVE_EVENTFD', 1)
endif

clock_gettime_test_code = '''
  #include <time.h>
  struct timespec t;
  int main (int argc, char ** argv) {
    return clock_gettime(CLOCK_REALTIME, &t);
  }'''
librt = []
if cc.links(clock_gettime_test_code, name : 'clock_gettime')
  glib_conf.set('HAVE_CLOCK_GETTIME', 1)
elif cc.links(clock_gettime_test_code, args : '-lrt', name : 'clock_gettime in librt')
  glib_conf.set('HAVE_CLOCK_GETTIME', 1)
  librt = cc.find_library('rt')
endif

# if statfs() takes 2 arguments (Posix) or 4 (Solaris)
if have_func_statfs
  if cc.compiles(glib_conf_prefix + '''
                 #include <unistd.h>
                        #ifdef HAVE_SYS_PARAM_H
                        #include <sys/param.h>
                        #endif
                        #ifdef HAVE_SYS_VFS_H
                        #include <sys/vfs.h>
                        #endif
                        #ifdef HAVE_SYS_MOUNT_H
                        #include <sys/mount.h>
                        #endif
                        #ifdef HAVE_SYS_STATFS_H
                        #include <sys/statfs.h>
                        #endif
                        void some_func (void) {
                          struct statfs st;
                          statfs("/", &st);
                        }''', name : 'number of arguments to statfs() (n=2)')
    glib_conf.set('STATFS_ARGS', 2)
  elif cc.compiles(glib_conf_prefix + '''
                   #include <unistd.h>
                          #ifdef HAVE_SYS_PARAM_H
                          #include <sys/param.h>
                          #endif
                          #ifdef HAVE_SYS_VFS_H
                          #include <sys/vfs.h>
                          #endif
                          #ifdef HAVE_SYS_MOUNT_H
                          #include <sys/mount.h>
                          #endif
                          #ifdef HAVE_SYS_STATFS_H
                          #include <sys/statfs.h>
                          #endif
                          void some_func (void) {
                            struct statfs st;
                            statfs("/", &st, sizeof (st), 0);
                          }''', name : 'number of arguments to statfs() (n=4)')
    glib_conf.set('STATFS_ARGS', 4)
  else
    error('Unable to determine number of arguments to statfs()')
  endif
endif

# open takes O_DIRECTORY as an option
#AC_MSG_CHECKING([])
if cc.compiles('''#include <fcntl.h>
                  #include <sys/types.h>
                  #include <sys/stat.h>],
                  void some_func (void) {
                    open(0, O_DIRECTORY, 0);
                  }''', name : 'open() option O_DIRECTORY')
  glib_conf.set('HAVE_OPEN_O_DIRECTORY', 1)
endif

# Check whether there is a vsnprintf() function with C99 semantics installed.
# AC_FUNC_VSNPRINTF_C99
# Check whether there is a snprintf() function with C99 semantics installed.
# AC_FUNC_SNPRINTF_C99

have_good_vsnprintf = false
have_good_snprintf = false

if host_system == 'windows'
  # Unfortunately the mingw and Visual Studio 2015+ implementations of C99-style
  # snprintf and vsnprintf don't seem to be quite good enough, at least not in
  # mingw-runtime-3.14.  (Sorry, I don't know exactly what is the problem,
  # but it is related to floating point formatting and decimal point vs. comma.)
  # The simple tests in AC_FUNC_VSNPRINTF_C99 and AC_FUNC_SNPRINTF_C99 aren't
  # rigorous enough to notice, though.
  glib_conf.set('HAVE_C99_SNPRINTF', false)
  glib_conf.set('HAVE_C99_VSNPRINTF', false)
else
  vsnprintf_c99_test_code = '''
#include <stdio.h>
#include <stdarg.h>

int
doit(char * s, ...)
{
  char buffer[32];
  va_list args;
  int r;

  va_start(args, s);
  r = vsnprintf(buffer, 5, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  /* AIX 5.1 and Solaris seems to have a half-baked vsnprintf()
     implementation. The above will return 7 but if you replace
     the size of the buffer with 0, it borks! */
  va_start(args, s);
  r = vsnprintf(buffer, 0, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit("1234567");
  exit(1);
}'''

  rres = cc.run(vsnprintf_c99_test_code, name : 'C99 vsnprintf')
  if rres.compiled() and rres.returncode() == 0
    glib_conf.set('HAVE_C99_VSNPRINTF', 1)
    have_good_vsnprintf = true
  endif

  snprintf_c99_test_code = '''
#include <stdio.h>
#include <stdarg.h>

int
doit()
{
  char buffer[32];
  va_list args;
  int r;

  r = snprintf(buffer, 5, "1234567");

  if (r != 7)
    exit(1);

  r = snprintf(buffer, 0, "1234567");

  if (r != 7)
    exit(1);

  r = snprintf(NULL, 0, "1234567");

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit();
  exit(1);
}'''

  rres = cc.run(snprintf_c99_test_code, name : 'C99 snprintf')
  if rres.compiled() and rres.returncode() == 0
    glib_conf.set('HAVE_C99_SNPRINTF', 1)
    have_good_snprintf = true
  endif
endif

if host_system == 'windows'
  glib_conf.set_quoted('EXEEXT', '.exe')
else
  glib_conf.set('EXEEXT', '')
endif

if have_good_vsnprintf and have_good_snprintf
  # Our printf is 'good' only if vsnpintf()/snprintf() supports C99 well enough
  glib_conf.set('HAVE_GOOD_PRINTF', 1) # FIXME: Check for HAVE_UNIX98_PRINTF?
else
  glib_conf.set('HAVE_VASPRINTF', 1)
endif

# Check whether the printf() family supports Unix98 %n$ positional parameters
# AC_FUNC_PRINTF_UNIX98
# Nothing uses HAVE_UNIX98_PRINTF


# Check for nl_langinfo and CODESET
# FIXME: Check for HAVE_BIND_TEXTDOMAIN_CODESET
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *codeset = nl_langinfo (CODESET);
                 return 0;
               }''', name : 'nl_langinfo and CODESET')
  glib_conf.set('HAVE_LANGINFO_CODESET', 1)
  glib_conf.set('HAVE_CODESET', 1)
endif

# Check for nl_langinfo and LC_TIME parts that are needed in gdatetime.c
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *str;
                 str = nl_langinfo (PM_STR);
                 str = nl_langinfo (D_T_FMT);
                 str = nl_langinfo (D_FMT);
                 str = nl_langinfo (T_FMT);
                 str = nl_langinfo (T_FMT_AMPM);
                 str = nl_langinfo (MON_1);
                 str = nl_langinfo (ABMON_12);
                 str = nl_langinfo (DAY_1);
                 str = nl_langinfo (ABDAY_7);
                 return 0;
               }''', name : 'nl_langinfo (PM_STR)')
  glib_conf.set('HAVE_LANGINFO_TIME', 1)
endif
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *str;
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT0_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT1_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT2_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT3_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT4_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT5_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT6_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT7_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT8_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT9_MB);
                 return 0;
               }''', name : 'nl_langinfo (_NL_CTYPE_OUTDIGITn_MB)')
  glib_conf.set('HAVE_LANGINFO_OUTDIGIT', 1)
endif

# Check for nl_langinfo and alternative month names
if cc.links('''#ifndef _GNU_SOURCE
              # define _GNU_SOURCE
              #endif
              #include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *str;
                 str = nl_langinfo (ALTMON_1);
                 str = nl_langinfo (ALTMON_2);
                 str = nl_langinfo (ALTMON_3);
                 str = nl_langinfo (ALTMON_4);
                 str = nl_langinfo (ALTMON_5);
                 str = nl_langinfo (ALTMON_6);
                 str = nl_langinfo (ALTMON_7);
                 str = nl_langinfo (ALTMON_8);
                 str = nl_langinfo (ALTMON_9);
                 str = nl_langinfo (ALTMON_10);
                 str = nl_langinfo (ALTMON_11);
                 str = nl_langinfo (ALTMON_12);
                 return 0;
               }''', name : 'nl_langinfo (ALTMON_n)')
  glib_conf.set('HAVE_LANGINFO_ALTMON', 1)
endif

# Check for nl_langinfo and abbreviated alternative month names
if cc.links('''#ifndef _GNU_SOURCE
              # define _GNU_SOURCE
              #endif
              #include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *str;
                 str = nl_langinfo (_NL_ALTMON_1);
                 str = nl_langinfo (_NL_ALTMON_2);
                 str = nl_langinfo (_NL_ALTMON_3);
                 str = nl_langinfo (_NL_ALTMON_4);
                 str = nl_langinfo (_NL_ALTMON_5);
                 str = nl_langinfo (_NL_ALTMON_6);
                 str = nl_langinfo (_NL_ALTMON_7);
                 str = nl_langinfo (_NL_ALTMON_8);
                 str = nl_langinfo (_NL_ALTMON_9);
                 str = nl_langinfo (_NL_ALTMON_10);
                 str = nl_langinfo (_NL_ALTMON_11);
                 str = nl_langinfo (_NL_ALTMON_12);
                 return 0;
               }''', name : 'nl_langinfo (_NL_ALTMON_n)')
  glib_conf.set('HAVE_LANGINFO_ABALTMON', 1)
endif

# Check if C compiler supports the 'signed' keyword
if not cc.compiles('''signed char x;''', name : 'signed')
  glib_conf.set('signed', '/* NOOP */')
endif

# Check if the ptrdiff_t type exists
if cc.has_header_symbol('stddef.h', 'ptrdiff_t')
  glib_conf.set('HAVE_PTRDIFF_T', 1)
endif

# Check for sig_atomic_t type
if cc.links('''#include <signal.h>
               #include <sys/types.h>
               sig_atomic_t val = 42;
               int main (int argc, char ** argv) {
                 return val == 42 ? 0 : 1;
               }''', name : 'sig_atomic_t')
  glib_conf.set('HAVE_SIG_ATOMIC_T', 1)
endif

# Check if 'long long' works and what format can be used to print it
# jm_AC_TYPE_LONG_LONG
# Nothing uses HAVE_LONG_LONG_FORMAT and HAVE_INT64_AND_I64
if cc.compiles('''long long ll = 1LL;
                  int i = 63;
                  int some_func (void) {
                    long long llmax = (long long) -1;
                    return ll << i | ll >> i | llmax / ll | llmax % ll;
                  }''', name : 'long long')
  glib_conf.set('HAVE_LONG_LONG', 1)
  have_long_long = true
else
  have_long_long = false
endif

# Test whether the compiler supports the 'long double' type.
if cc.compiles('''/* The Stardent Vistra knows sizeof(long double), but does not support it.  */
                  long double foo = 0.0;
                  /* On Ultrix 4.3 cc, long double is 4 and double is 8.  */
                  int array [2*(sizeof(long double) >= sizeof(double)) - 1];''',
               name : 'long double')
  glib_conf.set('HAVE_LONG_DOUBLE', 1)
endif

#dnl Test whether <stddef.h> has the 'wchar_t' type.
if cc.has_header_symbol('stddef.h', 'wchar_t')
  glib_conf.set('HAVE_WCHAR_T', 1)
endif

# Test whether <wchar.h> has the 'wint_t' type.
if cc.has_header_symbol('wchar.h', 'wint_t')
  glib_conf.set('HAVE_WINT_T', 1)
endif

found_uintmax_t = false

# Define HAVE_INTTYPES_H_WITH_UINTMAX if <inttypes.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.
# jm_AC_HEADER_INTTYPES_H
if cc.compiles('''#include <sys/types.h>
                  #include <inttypes.h>
                  void some_func (void) {
                    uintmax_t i = (uintmax_t) -1;
                  }''', name : 'uintmax_t in inttypes.h')
  glib_conf.set('HAVE_INTTYPES_H_WITH_UINTMAX', 1)
  found_uintmax_t = true
endif

# Define HAVE_STDINT_H_WITH_UINTMAX if <stdint.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.
# jm_AC_HEADER_STDINT_H
if cc.compiles('''#include <sys/types.h>
                  #include <stdint.h>
                  void some_func (void) {
                    uintmax_t i = (uintmax_t) -1;
                  }''', name : 'uintmax_t in stdint.h')
  glib_conf.set('HAVE_STDINT_H_WITH_UINTMAX', 1)
  found_uintmax_t = true
endif

# Define intmax_t to 'long' or 'long long'
# if it is not already defined in <stdint.h> or <inttypes.h>.
# For simplicity, we assume that a header file defines 'intmax_t' if and
# only if it defines 'uintmax_t'.
if found_uintmax_t
  glib_conf.set('HAVE_INTMAX_T', 1)
elif have_long_long
  glib_conf.set('intmax_t', 'long long')
else
  glib_conf.set('intmax_t', 'long')
endif

char_size = cc.sizeof('char')
short_size = cc.sizeof('short')
int_size = cc.sizeof('int')
voidp_size = cc.sizeof('void*')
long_size = cc.sizeof('long')
if have_long_long
  long_long_size = cc.sizeof('long long')
else
  long_long_size = 0
endif
sizet_size = cc.sizeof('size_t')
if cc.get_id() == 'msvc'
  ssizet_size = cc.sizeof('SSIZE_T', prefix : '#include <BaseTsd.h>')
else
  ssizet_size = cc.sizeof('ssize_t')
endif

# On Windows, MSVC supports both ll and I64 as format specifiers for 64-bit
# integers, but some versions (at least 4.7.x) of MinGW only support I64.
if host_system == 'windows'
  int64_m = 'I64'
else
  int64_m = 'll'
endif

char_align = cc.alignment('char')
short_align = cc.alignment('short')
int_align = cc.alignment('int')
voidp_align = cc.alignment('void*')
long_align = cc.alignment('long')
long_long_align = cc.alignment('long long')
# NOTE: We don't check for size of __int64 because long long is guaranteed to
# be 64-bit in C99, and it is available on all supported compilers
sizet_align = cc.alignment('size_t')

glib_conf.set('ALIGNOF_UNSIGNED_LONG', long_align)

glib_conf.set('SIZEOF_CHAR', char_size)
glib_conf.set('SIZEOF_INT', int_size)
glib_conf.set('SIZEOF_SHORT', short_size)
glib_conf.set('SIZEOF_LONG', long_size)
glib_conf.set('SIZEOF_LONG_LONG', long_long_size)
glib_conf.set('SIZEOF_SIZE_T', sizet_size)
glib_conf.set('SIZEOF_SSIZE_T', ssizet_size)
glib_conf.set('SIZEOF_VOID_P', voidp_size)

if short_size == 2
  gint16 = 'short'
  gint16_modifier='h'
  gint16_format='hi'
  guint16_format='hu'
elif int_size == 2
  gint16 = 'int'
  gint16_modifier=''
  gint16_format='i'
  guint16_format='u'
else
  error('Compiler provides no native 16-bit integer type')
endif
glibconfig_conf.set('gint16', gint16)
glibconfig_conf.set_quoted('gint16_modifier', gint16_modifier)
glibconfig_conf.set_quoted('gint16_format', gint16_format)
glibconfig_conf.set_quoted('guint16_format', guint16_format)

if short_size == 4
  gint32 = 'short'
  gint32_modifier='h'
  gint32_format='hi'
  guint32_format='hu'
  guint32_align = short_align
elif int_size == 4
  gint32 = 'int'
  gint32_modifier=''
  gint32_format='i'
  guint32_format='u'
  guint32_align = int_align
elif long_size == 4
  gint32 = 'long'
  gint32_modifier='l'
  gint32_format='li'
  guint32_format='lu'
  guint32_align = long_align
else
  error('Compiler provides no native 32-bit integer type')
endif
glibconfig_conf.set('gint32', gint32)
glibconfig_conf.set_quoted('gint32_modifier', gint32_modifier)
glibconfig_conf.set_quoted('gint32_format', gint32_format)
glibconfig_conf.set_quoted('guint32_format', guint32_format)
glib_conf.set('ALIGNOF_GUINT32', guint32_align)

if int_size == 8
  gint64 = 'int'
  gint64_modifier=''
  gint64_format='i'
  guint64_format='u'
  glib_extension=''
  gint64_constant='(val)'
  guint64_constant='(val)'
  guint64_align = int_align
elif long_size == 8
  gint64 = 'long'
  glib_extension=''
  gint64_modifier='l'
  gint64_format='li'
  guint64_format='lu'
  gint64_constant='(val##L)'
  guint64_constant='(val##UL)'
  guint64_align = long_align
elif long_long_size == 8
  gint64 = 'long long'
  glib_extension='G_GNUC_EXTENSION '
  gint64_modifier=int64_m
  gint64_format=int64_m + 'i'
  guint64_format=int64_m + 'u'
  gint64_constant='(G_GNUC_EXTENSION (val##LL))'
  guint64_constant='(G_GNUC_EXTENSION (val##ULL))'
  guint64_align = long_long_align
else
  error('Compiler provides no native 64-bit integer type')
endif
glibconfig_conf.set('glib_extension', glib_extension)
glibconfig_conf.set('gint64', gint64)
glibconfig_conf.set_quoted('gint64_modifier', gint64_modifier)
glibconfig_conf.set_quoted('gint64_format', gint64_format)
glibconfig_conf.set_quoted('guint64_format', guint64_format)
glibconfig_conf.set('gint64_constant', gint64_constant)
glibconfig_conf.set('guint64_constant', guint64_constant)
glib_conf.set('ALIGNOF_GUINT64', guint64_align)

if host_system == 'windows'
  glibconfig_conf.set('g_pid_type', 'void*')
  glibconfig_conf.set_quoted('g_pid_format', 'p')
  if host_machine.cpu_family() == 'x86_64'
    glibconfig_conf.set_quoted('g_pollfd_format', '%#I64x')
  else
    glibconfig_conf.set_quoted('g_pollfd_format', '%#x')
  endif
  glibconfig_conf.set('g_dir_separator', '\\\\')
  glibconfig_conf.set('g_searchpath_separator', ';')
else
  glibconfig_conf.set('g_pid_type', 'int')
  glibconfig_conf.set_quoted('g_pid_format', 'i')
  glibconfig_conf.set_quoted('g_pollfd_format', '%d')
  glibconfig_conf.set('g_dir_separator', '/')
  glibconfig_conf.set('g_searchpath_separator', ':')
endif

if sizet_size == short_size
  glibconfig_conf.set('glib_size_type_define', 'short')
  glibconfig_conf.set_quoted('gsize_modifier', 'h')
  glibconfig_conf.set_quoted('gssize_modifier', 'h')
  glibconfig_conf.set_quoted('gsize_format', 'hu')
  glibconfig_conf.set_quoted('gssize_format', 'hi')
  glibconfig_conf.set('glib_msize_type', 'SHRT')
elif sizet_size == int_size
  glibconfig_conf.set('glib_size_type_define', 'int')
  glibconfig_conf.set_quoted('gsize_modifier', '')
  glibconfig_conf.set_quoted('gssize_modifier', '')
  glibconfig_conf.set_quoted('gsize_format', 'u')
  glibconfig_conf.set_quoted('gssize_format', 'i')
  glibconfig_conf.set('glib_msize_type', 'INT')
elif sizet_size == long_size
  glibconfig_conf.set('glib_size_type_define', 'long')
  glibconfig_conf.set_quoted('gsize_modifier', 'l')
  glibconfig_conf.set_quoted('gssize_modifier', 'l')
  glibconfig_conf.set_quoted('gsize_format', 'lu')
  glibconfig_conf.set_quoted('gssize_format', 'li')
  glibconfig_conf.set('glib_msize_type', 'LONG')
elif sizet_size == long_long_size
  glibconfig_conf.set('glib_size_type_define', 'long long')
  glibconfig_conf.set_quoted('gsize_modifier', int64_m)
  glibconfig_conf.set_quoted('gssize_modifier', int64_m)
  glibconfig_conf.set_quoted('gsize_format', int64_m + 'u')
  glibconfig_conf.set_quoted('gssize_format', int64_m + 'i')
  glibconfig_conf.set('glib_msize_type', 'INT64')
else
  error('Could not determine size of size_t.')
endif

if voidp_size == int_size
  glibconfig_conf.set('glib_intptr_type_define', 'int')
  glibconfig_conf.set_quoted('gintptr_modifier', '')
  glibconfig_conf.set_quoted('gintptr_format', 'i')
  glibconfig_conf.set_quoted('guintptr_format', 'u')
  glibconfig_conf.set('glib_gpi_cast', '(gint)')
  glibconfig_conf.set('glib_gpui_cast', '(guint)')
elif voidp_size == long_size
  glibconfig_conf.set('glib_intptr_type_define', 'long')
  glibconfig_conf.set_quoted('gintptr_modifier', 'l')
  glibconfig_conf.set_quoted('gintptr_format', 'li')
  glibconfig_conf.set_quoted('guintptr_format', 'lu')
  glibconfig_conf.set('glib_gpi_cast', '(glong)')
  glibconfig_conf.set('glib_gpui_cast', '(gulong)')
elif voidp_size == long_long_size
  glibconfig_conf.set('glib_intptr_type_define', 'long long')
  glibconfig_conf.set_quoted('gintptr_modifier', int64_m)
  glibconfig_conf.set_quoted('gintptr_format', int64_m + 'i')
  glibconfig_conf.set_quoted('guintptr_format', int64_m + 'u')
  glibconfig_conf.set('glib_gpi_cast', '(gint64)')
  glibconfig_conf.set('glib_gpui_cast', '(guint64)')
else
  error('Could not determine size of void *')
endif

if long_size != 8 and long_long_size != 8 and int_size != 8
  error('GLib requires a 64-bit type. You might want to consider using the GNU C compiler.')
endif

glibconfig_conf.set('gintbits', int_size * 8)
glibconfig_conf.set('glongbits', long_size * 8)
glibconfig_conf.set('gsizebits', sizet_size * 8)
glibconfig_conf.set('gssizebits', ssizet_size * 8)

# FIXME: maybe meson should tell us the libsuffix?
if host_system == 'windows'
  g_module_suffix = 'dll'
elif host_system == 'darwin'
  g_module_suffix = 'dylib'
else
  g_module_suffix = 'so'
endif
glibconfig_conf.set('g_module_suffix', g_module_suffix)

glibconfig_conf.set('GLIB_MAJOR_VERSION', major_version)
glibconfig_conf.set('GLIB_MINOR_VERSION', minor_version)
glibconfig_conf.set('GLIB_MICRO_VERSION', micro_version)
glibconfig_conf.set('GLIB_VERSION', glib_version)

glibconfig_conf.set('glib_void_p', voidp_size)
glibconfig_conf.set('glib_long', long_size)
glibconfig_conf.set('glib_size_t', sizet_size)
glibconfig_conf.set('glib_ssize_t', ssizet_size)
if host_machine.endian() == 'big'
  glibconfig_conf.set('g_byte_order', 'G_BIG_ENDIAN')
  glibconfig_conf.set('g_bs_native', 'BE')
  glibconfig_conf.set('g_bs_alien', 'LE')
else
  glibconfig_conf.set('g_byte_order', 'G_LITTLE_ENDIAN')
  glibconfig_conf.set('g_bs_native', 'LE')
  glibconfig_conf.set('g_bs_alien', 'BE')
endif

# === va_copy checks ===
# we currently check for all three va_copy possibilities, so we get
# all results in config.log for bug reports.

va_copy_func = ''
foreach try_func : [ '__va_copy', 'va_copy' ]
  if cc.compiles('''#include <stdarg.h>
                    #include <stdlib.h>
                    #ifdef _MSC_VER
                    # include "msvc_recommended_pragmas.h"
                    #endif
                    void f (int i, ...) {
                    va_list args1, args2;
                    va_start (args1, i);
                    @0@ (args2, args1);
                    if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
                      exit (1);
                    va_end (args1); va_end (args2);
                    }
                    int main() {
                      f (0, 42);
                      return 0;
                    }'''.format(try_func),
                    name : try_func + ' check')
    va_copy_func = try_func
  endif
endforeach
if va_copy_func != ''
  glib_conf.set('G_VA_COPY', va_copy_func)
  glib_vacopy = '#define G_VA_COPY ' + va_copy_func
else
  glib_vacopy = '/* #undef G_VA_COPY */'
endif

va_list_val_copy_prog = '''
  #include <stdarg.h>
  #include <stdlib.h>
  void f (int i, ...) {
    va_list args1, args2;
    va_start (args1, i);
    args2 = args1;
    if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
      exit (1);
    va_end (args1); va_end (args2);
  }
  int main() {
    f (0, 42);
    return 0;
  }'''

# We do this in two steps so if compilation fails already it looks less alarming
glib_va_val_copy = false
if cc.compiles(va_list_val_copy_prog, name : 'va_lists can be copied as values')
  # FIXME: what to do when cross-compiling?
  if cc.run(va_list_val_copy_prog, name : 'va_lists can be copied as values').returncode() == 0
    glib_va_val_copy = true
  endif
endif
if not glib_va_val_copy
  glib_va_val_copy = false
  glib_vacopy = glib_vacopy + '\n#define G_VA_COPY_AS_ARRAY 1'
  glib_conf.set('G_VA_COPY_AS_ARRAY', 1)
endif
glibconfig_conf.set('glib_vacopy', glib_vacopy)

# check for flavours of varargs macros
g_have_iso_c_varargs = cc.compiles('''
  void some_func (void) {
    int a(int p1, int p2, int p3);
    #define call_a(...) a(1,__VA_ARGS__)
    call_a(2,3);
  }''', name : 'ISO C99 varargs macros in C')

if g_have_iso_c_varargs
  glibconfig_conf.set('g_have_iso_c_varargs', '''
#ifndef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif''')
endif

g_have_iso_cxx_varargs = cxx.compiles('''
  void some_func (void) {
    int a(int p1, int p2, int p3);
    #define call_a(...) a(1,__VA_ARGS__)
    call_a(2,3);
  }''', name : 'ISO C99 varargs macros in C++')

if g_have_iso_cxx_varargs
  glibconfig_conf.set('g_have_iso_cxx_varargs', '''
#ifdef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif''')
endif

g_have_gnuc_varargs = cc.compiles('''
  void some_func (void) {
    int a(int p1, int p2, int p3);
    #define call_a(params...) a(1,params)
    call_a(2,3);
  }''', name : 'GNUC varargs macros')

if cc.has_header('alloca.h')
  glibconfig_conf.set('GLIB_HAVE_ALLOCA_H', true)
endif
has_syspoll = cc.has_header('sys/poll.h')
has_systypes = cc.has_header('sys/types.h')
if has_syspoll
  glibconfig_conf.set('GLIB_HAVE_SYS_POLL_H', true)
endif
has_winsock2 = cc.has_header('winsock2.h')

if has_syspoll and has_systypes
  templ = '''#include<sys/poll.h>
#include<sys/types.h>
#include<stdio.h>
int main(int argc, char **argv) {
  printf("%d\n", (int)@0@);
  return 0;
}'''
elif has_winsock2
  templ = '''#define _WIN32_WINNT 0x0600
#include <stdio.h>
#include <winsock2.h>
int main(int argc, char **argv) {
  printf("%d\n", (int)@0@);
  return 0;
}'''
else
  # FIXME?
  error('FIX POLL* defines')
endif

value_POLLIN = cc.run(templ.format('POLLIN'), name : 'POLLIN value').stdout().strip()
value_POLLOUT = cc.run(templ.format('POLLOUT'), name : 'POLLOUT value').stdout().strip()
value_POLLPRI = cc.run(templ.format('POLLPRI'), name : 'POLLPRI value').stdout().strip()
value_POLLERR = cc.run(templ.format('POLLERR'), name : 'POLLERR value').stdout().strip()
value_POLLHUP = cc.run(templ.format('POLLHUP'), name : 'POLLHUP value').stdout().strip()
value_POLLNVAL = cc.run(templ.format('POLLNVAL'), name : 'POLLNVAL value').stdout().strip()

glibconfig_conf.set('g_pollin', value_POLLIN)
glibconfig_conf.set('g_pollout', value_POLLOUT)
glibconfig_conf.set('g_pollpri', value_POLLPRI)
glibconfig_conf.set('g_pollerr', value_POLLERR)
glibconfig_conf.set('g_pollhup', value_POLLHUP)
glibconfig_conf.set('g_pollnval', value_POLLNVAL)

# Internet address families
# FIXME: what about Cygwin (G_WITH_CYGWIN)
if host_system == 'windows'
  glib_inet_includes= '''
#include <winsock2.h>
'''
else
  glib_inet_includes='''
#include <sys/types.h>
#include <sys/socket.h>
'''
endif

net_defines = [
  [ 'AF_UNIX', 'g_af_unix' ],
  [ 'AF_INET', 'g_af_inet' ],
  [ 'AF_INET6', 'g_af_inet6' ],
  [ 'MSG_OOB', 'g_msg_oob' ],
  [ 'MSG_PEEK', 'g_msg_peek' ],
  [ 'MSG_DONTROUTE', 'g_msg_dontroute' ],
]
foreach d : net_defines
  templ = '''@0@
#include <stdio.h>
int main(int argc, char **argv) {
  printf("%d\n", (int)@1@);
  return 0;
}'''
  # FIXME: fix for cross-compilation
  if not meson.has_exe_wrapper()
    error('Fix sys define detection for cross build')
  endif
  val = cc.run(templ.format(glib_inet_includes, d[0]), name : d[0] + ' value').stdout().strip()
  glibconfig_conf.set(d[1], val)
endforeach

glibconfig_conf.set('GLIB_USING_SYSTEM_PRINTF', true) # FIXME!

# We need a more robust approach here...
host_cpu_family = host_machine.cpu_family()
if host_cpu_family == 'x86' or host_cpu_family == 'x86_64' or host_cpu_family == 's390' or host_cpu_family == 's390x' or host_cpu_family.startswith('arm') or host_cpu_family.startswith('crisv32') or host_cpu_family.startswith('etrax')
  glib_memory_barrier_needed = false
elif host_cpu_family.startswith('sparc') or host_cpu_family.startswith('alpha') or host_cpu_family.startswith('powerpc') or host_cpu_family == 'ia64'
  glib_memory_barrier_needed = true
else
  warning('Unknown host cpu: ' + host_cpu_family)
  glib_memory_barrier_needed = true
endif
glibconfig_conf.set('G_ATOMIC_OP_MEMORY_BARRIER_NEEDED', glib_memory_barrier_needed)

# Note that the atomic ops are only available with GCC on x86 when
# using -march=i486 or higher.  If we detect that the atomic ops are
# not available but would be available given the right flags, we want
# to abort and advise the user to fix their CFLAGS.  It's better to do
# that then to silently fall back on emulated atomic ops just because
# the user had the wrong build environment.
atomictest = '''void func() {
  volatile int atomic = 2;
  __sync_bool_compare_and_swap (&atomic, 2, 3);
}
'''
if cc.compiles(atomictest)
  glibconfig_conf.set('G_ATOMIC_LOCK_FREE', true)
else
  if host_machine.cpu_family() == 'x86' and cc.compiles(atomictest, args : '-march=i486')
    error('GLib must be built with -march=i486 or later.')
  endif
  glibconfig_conf.set('G_ATOMIC_LOCK_FREE', false)
endif

# === Threads ===

# Let meson figure out all this business and whether -pthread or whatnot is needed
# FIXME: probably needs more tweaking in meson for things like -D_REENTRANT etc.
thread_dep = dependency('threads')

# Determination of thread implementation
if host_system == 'windows'
  glibconfig_conf.set('g_threads_impl_def', 'WIN32')
  glib_conf.set('THREADS_WIN32', 1)
else
  pthread_prefix = '''
      #ifndef _GNU_SOURCE
      # define _GNU_SOURCE
      #endif
      #include <pthread.h>'''
  glibconfig_conf.set('g_threads_impl_def', 'POSIX')
  glib_conf.set('THREADS_POSIX', 1)
  if cc.has_header_symbol('pthread.h', 'pthread_attr_setstacksize')
    glib_conf.set('HAVE_PTHREAD_ATTR_SETSTACKSIZE', 1)
  endif
  if cc.has_header_symbol('pthread.h', 'pthread_condattr_setclock')
    glib_conf.set('HAVE_PTHREAD_CONDATTR_SETCLOCK', 1)
  endif
  if cc.has_header_symbol('pthread.h', 'pthread_cond_timedwait_relative_np')
    glib_conf.set('HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP', 1)
  endif
  if cc.has_header_symbol('pthread.h', 'pthread_getname_np', prefix : pthread_prefix)
    glib_conf.set('HAVE_PTHREAD_GETNAME_NP', 1)
  endif
  # Assume that pthread_setname_np is available in some form; same as configure
  if cc.links(pthread_prefix + '''
              int main() {
                pthread_setname_np("example");
              }''',
              name : 'pthread_setname_np(const char*)',
              dependencies : thread_dep)
    # macOS and iOS
    glib_conf.set('HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID', 1)
  elif cc.links(pthread_prefix + '''
                int main() {
                  pthread_setname_np(pthread_self(), "example");
                }''',
                name : 'pthread_setname_np(pthread_t, const char*)',
                dependencies : thread_dep)
    # Linux, Solaris, etc.
    glib_conf.set('HAVE_PTHREAD_SETNAME_NP_WITH_TID', 1)
  endif
endif

# FIXME: how to do this when cross-compiling?
# FIXME: we should make it print the result and always return 0, so that
# the output in meson shows up as green
stack_grows_check_prog = '''
  volatile int *a = 0, *b = 0;
  void f (int i) {
    volatile int x = 5;
    if (i == 0)
      b = &x;
    else
      f (i - 1);
  }
  int main () {
    volatile int y = 7;
    a = &y;
    f (100);
    return b > a ? 0 : 1;
  }'''
stack_grows_run_result = cc.run(stack_grows_check_prog, name : 'stack grows check')
if stack_grows_run_result.compiled() and stack_grows_run_result.returncode() == 0
  glibconfig_conf.set('G_HAVE_GROWING_STACK', 1)
else
  glibconfig_conf.set('G_HAVE_GROWING_STACK', 0)
endif

# Tests for iconv
#
# USE_LIBICONV_GNU: Using GNU libiconv
# USE_LIBICONV_NATIVE: Using a native impl of iconv in a separate library
#
# We should never use the MinGW C library's iconv. On Windows we use the
# GNU implementation that ships with MinGW.

# On Windows, just always use the built-in implementation
if host_system == 'windows'
  libiconv = []
  glib_conf.set('USE_LIBICONV_NATIVE', true)
else
  found_iconv = false
  iconv_opt = get_option('iconv')
  if iconv_opt == 'libc'
    if cc.has_function('iconv_open')
      libiconv = []
      found_iconv = true
    endif
  elif iconv_opt == 'gnu'
    if cc.has_header_symbol('iconv.h', 'libiconv_open')
      glib_conf.set('USE_LIBICONV_GNU', true)
      libiconv = [cc.find_library('iconv')]
      found_iconv = true
    endif
  elif iconv_opt == 'native'
    if cc.has_header_symbol('iconv.h', 'iconv_open')
      glib_conf.set('USE_LIBICONV_NATIVE', true)
      libiconv = [cc.find_library('iconv')]
      found_iconv = true
    endif
  endif

  if not found_iconv
    error('No iconv() implementation found in C library or libiconv')
  endif

endif

if get_option('internal_pcre')
  pcre = []
  use_system_pcre = false
else
  pcre = dependency('libpcre', required : false) # Should check for Unicode support, too. FIXME
  if not pcre.found()
    if cc.get_id() == 'msvc'
    # MSVC: Search for the PCRE library by the configuration, which corresponds
    # to the output of CMake builds of PCRE.  Note that debugoptimized
    # is really a Release build with .PDB files.
      if buildtype == 'debug'
        pcre = cc.find_library('pcred', required : false)
      else
        pcre = cc.find_library('pcre', required : false)
      endif
    endif
  endif
  use_system_pcre = pcre.found()
endif
glib_conf.set('USE_SYSTEM_PCRE', use_system_pcre)

use_pcre_static_flag = false

if host_system == 'windows'
  if not use_system_pcre
    use_pcre_static_flag = true
  else
    pcre_static = cc.links('''#define PCRE_STATIC
                              #include <pcre.h>
                              int main() {
                                void *p = NULL;
                                pcre_free(p);
                                return 0;
                              }''',
                           dependencies: pcre,
                           name : 'Windows system PCRE is a static build')
    if pcre_static
      use_pcre_static_flag = true
    endif
  endif
endif

libm = cc.find_library('m', required : false)
libffi_dep = dependency('libffi', version : '>= 3.0.0', fallback : ['libffi', 'ffi_dep'])
zlib_libname = '-lz'
if cc.get_id() != 'msvc'
  libz_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])
else
  # MSVC: Don't use the bundled ZLib sources until we are sure that we can't
  # find the ZLib .lib
  libz_dep = dependency('zlib', required : false)

  # MSVC: Search for the ZLib .lib, which corresponds to the results of
  # of using ZLib's win32/makefile.msc.
  if not libz_dep.found()
    libz_dep = cc.find_library('zlib1', required : false)
    if libz_dep.found()
      zlib_libname = '-lzlib1'
    else
      libz_dep = cc.find_library('zlib', required : false)
      if libz_dep.found()
        zlib_libname = '-lzlib'
      else
        libz_dep = subproject('zlib').get_variable('zlib_dep')
      endif
    endif
  endif
endif

# Only used on non-glibc targets
libintl = cc.find_library('intl', required : false)
if host_system == 'windows' and not libintl.found()
  # Used only when the gettext library is not available (MSVC, not MinGW)
  libintl = subproject('proxy-libintl').get_variable('intl_dep')
  glib_conf.set('HAVE_DCGETTEXT', 1)
else
  glib_conf.set('HAVE_DCGETTEXT', cc.has_header_symbol('libintl.h', 'dcgettext'))
endif
# We require gettext to always be present
glib_conf.set('HAVE_GETTEXT', 1)
glib_conf.set_quoted('GLIB_LOCALE_DIR', join_paths(glib_datadir, 'locale'))
# xgettext is optional (on Windows for instance)
xgettext = find_program('xgettext', required : false)

# libmount is only used by gio, but we need to fetch the libs to generate the
# pkg-config file below
libmount_dep = []
if host_system == 'linux' and get_option('libmount')
  libmount_dep = [dependency('mount', version : '>=2.23', required : true)]
endif

if host_system == 'windows'
  winsock2 = cc.find_library('ws2_32')
endif

selinux_dep = []
if host_system == 'linux' and get_option('selinux')
  selinux_dep = [dependency('libselinux')]
  glib_conf.set('SELINUX_LIBS', '-lselinux')
  glib_conf.set('HAVE_SELINUX', 1)
endif

xattr_dep = []
if host_system != 'windows' and get_option('xattr')
  # either glibc or libattr can provide xattr support
  # for both of them, we check for getxattr being in
  # the library and a valid xattr header.

  # try glibc
  if cc.has_function('getxattr') and cc.has_header('sys/xattr.h')
    glib_conf.set('HAVE_SYS_XATTR_H', 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format('HAVE_SYS_XATTR_H')
  #failure. try libattr
  elif cc.has_header_symbol('attr/xattr.h', 'getxattr')
    glib_conf.set('HAVE_ATTR_XATTR_H', 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format('HAVE_ATTR_XATTR_H')
    xattr_dep = [cc.find_library('xattr')]
  else
    error('No getxattr implementation found in C library or libxattr')
  endif

  glib_conf.set('HAVE_XATTR', 1)
  if cc.compiles(glib_conf_prefix + '''
                 #include <stdio.h>
                 #ifdef HAVE_SYS_TYPES_H
                 #include <sys/types.h>
                 #endif
                 #ifdef HAVE_SYS_XATTR_H
                 #include <sys/xattr.h>
                 #elif HAVE_ATTR_XATTR_H
                 #include <attr/xattr.h>
                 #endif

                 int main (void) {
                   ssize_t len = getxattr("", "", NULL, 0, 0, XATTR_NOFOLLOW);
                 }''',
                 name : 'XATTR_NOFOLLOW')
    glib_conf.set('HAVE_XATTR_NOFOLLOW', 1)
  endif
endif

python = import('python3').find_python()

# Determine which user environment-dependent files that we want to install
have_bash = find_program('bash', required : false).found() # For completion scripts
have_m4 = find_program('m4', required : false).found() # For m4 macros
have_sh = find_program('sh', required : false).found() # For glib-gettextize

# FIXME: defines in config.h that are not actually used anywhere
# (we add them for now to minimise the diff)
glib_conf.set('HAVE_DLFCN_H', 1)
glib_conf.set('__EXTENSIONS__', 1)
glib_conf.set('STDC_HEADERS', 1)
# THREADS_NONE
glib_conf.set('SIZEOF___INT64', 8)

# Various substs needed for our pkg-config files
# FIXME: Derive these from the dependency() objects (Meson support needed)
glib_conf.set('ZLIB_LIBS', zlib_libname)
glib_conf.set('LIBFFI_LIBS', '-lffi')
if libintl.found()
  glib_conf.set('INTLLIBS', '-lintl')
endif
if libiconv.length() != 0
  glib_conf.set('ICONV_LIBS', '-liconv')
endif
if use_system_pcre
  glib_conf.set('PCRE_LIBS', '-lpcre')
endif
if libmount_dep.length() != 0
  glib_conf.set('LIBMOUNT_LIBS', '-lmount')
  glib_conf.set('HAVE_LIBMOUNT', 1)
endif
glib_conf.set('GIO_MODULE_DIR', glib_giomodulesdir)
# FIXME: Missing:
# @G_MODULE_LIBS@ @COCOA_LIBS@ @CARBON_LIBS@ @G_LIBS_EXTRA@
# @PCRE_REQUIRES@ @GLIB_EXTRA_CFLAGS@ @G_THREAD_CFLAGS@

# Tracing: dtrace
want_dtrace = get_option('dtrace')
enable_dtrace = false

# Since dtrace support is opt-in we just error out if it was requested but
# is not available. We don't bother with autodetection yet.
if want_dtrace
  if glib_have_carbon
    error('GLib dtrace support not yet compatible with macOS dtrace')
  endif
  dtrace = find_program('dtrace', required : true) # error out if not found
  if not cc.has_header('sys/sdt.h')
    error('dtrace support needs sys/sdt.h header')
  endif
  # FIXME: autotools build also passes -fPIC -DPIC but is it needed in this case?
  dtrace_obj_gen = generator(dtrace,
    output : '@BASENAME@.o',
    arguments : ['-G', '-s', '@INPUT@', '-o', '@OUTPUT@'])
  # FIXME: $(SED) -e "s,define STAP_HAS_SEMAPHORES 1,undef STAP_HAS_SEMAPHORES,"
  #               -e "s,define _SDT_HAS_SEMAPHORES 1,undef _SDT_HAS_SEMAPHORES,"
  dtrace_hdr_gen = generator(dtrace,
    output : '@BASENAME@.h',
    arguments : ['-h', '-s', '@INPUT@', '-o', '@OUTPUT@'])
  glib_conf.set('HAVE_DTRACE', 1)
  enable_dtrace = true
endif

# systemtap
want_systemtap = get_option('systemtap')
enable_systemtap = false

if want_systemtap and enable_dtrace
  tapset_install_dir = get_option('tapset_install_dir')
  if tapset_install_dir == ''
    tapset_install_dir = join_paths(get_option('datadir'), 'systemtap/tapset', host_machine.cpu_family())
  endif
  stp_cdata = configuration_data()
  stp_cdata.set('ABS_GLIB_RUNTIME_LIBDIR', glib_libdir)
  stp_cdata.set('LT_CURRENT', minor_version * 100)
  stp_cdata.set('LT_REVISION', micro_version)
  enable_systemtap = true
endif


windows = import('windows')
subdir('glib')
subdir('gobject')
subdir('gthread')
subdir('gmodule')
subdir('gio')
if xgettext.found()
  subdir('po')
endif
subdir('tests')

# Configure and install pkg-config files
pc_files = [
  'gobject-2.0.pc',
  'glib-2.0.pc',
  'gthread-2.0.pc',
  'gmodule-2.0.pc',
  'gmodule-export-2.0.pc',
  'gmodule-no-export-2.0.pc',
  'gio-2.0.pc',
]
if host_system == 'windows'
  pc_files += ['gio-windows-2.0.pc']
else
  pc_files += ['gio-unix-2.0.pc']
endif

foreach pc : pc_files
  configure_file(input : pc + '.in',
    install : true,
    install_dir : glib_pkgconfigreldir,
    output : pc,
    configuration : glib_conf)
endforeach

# NOTE: We skip glib-zip.in because the filenames it assumes don't match ours

# Install glib-gettextize executable, if a UNIX-style shell is found
if have_sh
  configure_file(input : 'glib-gettextize.in',
    install : true,
    install_dir : 'bin',
    output : 'glib-gettextize',
    configuration : glib_conf)
endif

if have_m4
  # Install m4 macros that other projects use
  install_data('m4macros/glib-2.0.m4', 'm4macros/glib-gettext.m4', 'm4macros/gsettings.m4',
    install_dir : join_paths(get_option('datadir'), 'aclocal'))
endif

if host_system != 'windows'
  # Install Valgrind suppression file (except on Windows,
  # as Valgrind is currently not supported on Windows)
  install_data('glib.supp',
    install_dir : join_paths(get_option('datadir'), 'glib-2.0', 'valgrind'))
endif

configure_file(input : 'config.h.meson',
  output : 'config.h',
  configuration : glib_conf)

if host_system == 'windows'
  install_headers([ 'msvc_recommended_pragmas.h' ], subdir : 'glib-2.0')
endif

if get_option('man')
  xsltproc = find_program('xsltproc', required : true)
  xsltproc_command = [
    xsltproc,
    '--nonet',
    '--stringparam', 'man.output.quietly', '1',
    '--stringparam', 'funcsynopsis.style', 'ansi',
    '--stringparam', 'man.th.extra1.suppress', '1',
    '--stringparam', 'man.authors.section.enabled', '0',
    '--stringparam', 'man.copyright.section.enabled', '0',
    '-o', '@OUTPUT@',
    'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
    '@INPUT@',
  ]
  man1_dir = get_option('mandir') + '/man1'
endif

gnome = import('gnome')
subdir('docs/reference/glib')
subdir('docs/reference/gobject')
subdir('docs/reference/gio')
