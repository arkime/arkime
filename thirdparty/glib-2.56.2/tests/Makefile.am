include $(top_srcdir)/glib.mk

SUBDIRS = gobject refcount

LDADD = $(top_builddir)/glib/libglib-2.0.la
AM_CPPFLAGS = $(gmodule_INCLUDES) $(GLIB_DEBUG_FLAGS)
DEFS = -DGLIB_DISABLE_DEPRECATION_WARNINGS -DG_LOG_DOMAIN=\"GLib\"
AM_CFLAGS = -g

# Some random programs that appear not to be testcases and not used from any testcases
check_PROGRAMS += \
	testgdateparser				\
	unicode-normalize			\
	$(NULL)

# So far, this is the only gtester-ified testcase here
test_programs = \
	testglib				\
	$(NULL)

# These are not yet gtester-ified, so mark them as for 'installed' only so we
# don't run them under the framework.  We will handle them manually below.
installed_test_programs = \
	testgdate				\
	datetime				\
	atomic-test				\
	bit-test				\
	child-test				\
	completion-test				\
	dirname-test				\
	file-test				\
	env-test				\
	gio-test				\
	mainloop-test				\
	mapping-test				\
	onceinit				\
	asyncqueue-test				\
	qsort-test				\
	relation-test				\
	slice-concurrent			\
	slice-threadinit			\
	sources					\
	thread-test				\
	threadpool-test				\
	type-test				\
	unicode-caseconv			\
	unicode-encoding			\
	$(NULL)

test_extra_programs = \
	slice-test				\
	slice-color				\
	assert-msg-test				\
	unicode-collate				\
	$(NULL)

if OS_UNIX
test_programs += \
	spawn-test				\
	iochannel-test				\
	$(NULL)

# These don't appear to work installed and we don't want to run them under gtester either...
dist_uninstalled_test_extra_scripts = \
	run-collate-tests.sh			\
	run-assert-msg-test.sh			\
	$(NULL)
endif

dist_test_data = \
	iochannel-test-infile			\
	casemap.txt				\
	casefold.txt				\
	utf8.txt				\
	$(NULL)

# Run the 'installed' tests manually in-tree.
# This will cause them to be built even if installed tests are disabled.
check_PROGRAMS += $(installed_test_programs) $(installed_test_extra_programs)
check_SCRIPTS += $(installed_test_scripts)
TESTS += $(installed_test_programs) $(dist_uninstalled_test_extra_scripts)
AM_TESTS_ENVIRONMENT = \
	G_TEST_SRCDIR=$(abs_srcdir)				\
	G_TEST_BUILDDIR=$(abs_builddir)				\
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset	\
	MALLOC_CHECK_=2						\
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

test_programs += module-test
module_test_DEPENDENCIES = libmoduletestplugin_a.la libmoduletestplugin_b.la
module_test_LDADD = $(top_builddir)/gmodule/libgmodule-2.0.la $(LDADD)
module_test_LDFLAGS = $(G_MODULE_LDFLAGS)
slice_test_SOURCES = slice-test.c memchunks.c
slice_color_SOURCES = slice-color.c memchunks.c
slice_threadinit_LDADD = $(top_builddir)/gthread/libgthread-2.0.la $(LDADD)

test_ltlibraries = libmoduletestplugin_a.la libmoduletestplugin_b.la

libmoduletestplugin_a_la_SOURCES = libmoduletestplugin_a.c
libmoduletestplugin_a_la_LDFLAGS = $(G_MODULE_LDFLAGS) -avoid-version -module $(no_undefined)
libmoduletestplugin_a_la_LIBADD = $(G_MODULE_LIBS) ../gmodule/libgmodule-2.0.la ../glib/libglib-2.0.la

libmoduletestplugin_b_la_SOURCES = libmoduletestplugin_b.c
libmoduletestplugin_b_la_LDFLAGS = $(G_MODULE_LDFLAGS) -avoid-version -module $(no_undefined)
libmoduletestplugin_b_la_LIBADD = $(G_MODULE_LIBS) ../gmodule/libgmodule-2.0.la ../glib/libglib-2.0.la

if !ENABLE_INSTALLED_TESTS
# see http://lists.gnu.org/archive/html/bug-libtool/2013-05/msg00009.html and gio/tests/Makefile.am
libmoduletestplugin_a_la_LDFLAGS += -rpath /
libmoduletestplugin_b_la_LDFLAGS += -rpath /
endif

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)';				\
	for f in $$files; do					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi;	\
	  cp $$d/$$f $(distdir) || exit 1;			\
	done
	mkdir $(distdir)/collate;				\
	for f in $(srcdir)/collate/* ; do			\
	  if test -f $$f; then cp $$f $(distdir)/collate; fi;	\
	done

if HAVE_CXX
installed_test_programs += cxx-test
cxx_test_SOURCES = cxx-test.C
endif

if ENABLE_TIMELOOP
installed_test_programs += timeloop
endif

if PLATFORM_WIN32
no_undefined = -no-undefined

module_test_exp = module-test.exp

module-test.exp: module-test.o
	$(DLLTOOL) --output-exp module-test.exp module-test.o

installed_test_extra_programs = spawn-test-win32-gui

spawn_test_win32_gui_LDFLAGS = -mwindows

endif

EXTRA_DIST += \
	$(test_scripts)				\
	gen-casefold-txt.pl			\
	gen-casemap-txt.pl			\
	iochannel-test-infile			\
	timeloop-basic.c			\
	assert-msg-test.gdb

DISTCLEANFILES +=		\
	iochannel-test-outfile	\
	file-test-get-contents	\
	maptest		\
	mapchild		\
	collate.out
