NULL =

LDADD = \
	$(top_builddir)/gio/libgio-2.0.la               \
	$(top_builddir)/gobject/libgobject-2.0.la       \
	$(top_builddir)/gmodule/libgmodule-2.0.la       \
	$(top_builddir)/glib/libglib-2.0.la             \
	$(NULL)

AM_CPPFLAGS = \
	$(gio_INCLUDES) $(GLIB_DEBUG_FLAGS) 		\
	-I$(top_builddir)/gio 				\
	-I$(top_srcdir)/gio				\
	$(NULL)

modules =						\
	libtestmodulea.la 				\
	libtestmoduleb.la				\
	$(NULL)

if ENABLE_ALWAYS_BUILD_TESTS
noinst_LTLIBRARIES = $(modules)
else
check_LTLIBRARIES = $(modules)
endif

if ENABLE_INSTALLED_TESTS
testmoduledir = $(installed_testdir)/modules
testmodule_LTLIBRARIES = $(modules)
else
# See comment in Makefile.am one level up
rpath_hack = -rpath /
endif

libtestmodulea_la_SOURCES = test-module-a.c symbol-visibility.h
libtestmodulea_la_LIBADD = $(LDADD)
libtestmodulea_la_LDFLAGS = $(LDFLAGS) -module -no-undefined -avoid-version $(rpath_hack)

libtestmoduleb_la_SOURCES = test-module-b.c symbol-visibility.h
libtestmoduleb_la_LIBADD = $(LDADD)
libtestmoduleb_la_LDFLAGS =$(LDFLAGS) -module -no-undefined -avoid-version $(rpath_hack)
