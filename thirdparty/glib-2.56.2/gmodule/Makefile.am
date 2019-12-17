## Process this file with automake to produce Makefile.in
include $(top_srcdir)/glib.mk

AM_CPPFLAGS = 				\
	$(glib_INCLUDES) 		\
	-DG_LOG_DOMAIN=\"GModule\" 	\
	@GLIB_DEBUG_FLAGS@ 		\
	-DG_DISABLE_DEPRECATED

AM_CFLAGS = $(GLIB_WARN_CFLAGS)

EXTRA_DIST +=				\
		gmoduleconf.h.in 	\
		gmodule-dl.c		\
		gmodule-dyld.c		\
		gmodule-win32.c		\
		gmodule-ar.c		\
		gmoduleconf.h.win32 	\
		gmodule.rc.in

BUILT_EXTRA_DIST += \
		gmodule.rc

BUILT_SOURCES += gmoduleconf.h
gmoduleconf.h:	gmoduleconf.h.in

glibincludedir=$(includedir)/glib-2.0
glibinclude_HEADERS = \
		gmodule.h

libglib = $(top_builddir)/glib/libglib-2.0.la

top_builddir_full=`cd \$(top_builddir); pwd`

lib_LTLIBRARIES = libgmodule-2.0.la

if OS_WIN32_AND_DLL_COMPILATION
if MS_LIB_AVAILABLE
noinst_DATA += gmodule-2.0.lib

install_ms_lib_cmd = $(INSTALL) gmodule-2.0.lib $(DESTDIR)$(libdir)
uninstall_ms_lib_cmd = -rm $(DESTDIR)$(libdir)/gmodule-2.0.lib
endif
endif

install-ms-lib:
	$(install_ms_lib_cmd)

uninstall-ms-lib:
	$(uninstall_ms_lib_cmd)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32_AND_DLL_COMPILATION
gmodule_win32_res = gmodule-win32-res.o
gmodule_win32_res_ldflag = -Wl,$(gmodule_win32_res)
endif

libgmodule_2_0_la_CFLAGS = $(AM_CFLAGS) $(GLIB_HIDDEN_VISIBILITY_CFLAGS)
libgmodule_2_0_la_SOURCES = gmodule.c
libgmodule_2_0_la_LDFLAGS = $(GLIB_LINK_FLAGS) \
	$(gmodule_win32_res_ldflag) \
	$(G_MODULE_LDFLAGS) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined)

libgmodule_2_0_la_LIBADD = $(G_MODULE_LIBS_EXTRA) $(G_MODULE_LIBS) $(libglib)

libgmodule_2_0_la_DEPENDENCIES = $(gmodule_win32_res) $(gmodule_def)

gmodule-win32-res.o: gmodule.rc
	$(AM_V_GEN) $(WINDRES) gmodule.rc $@

gmodule.def: libgmodule-2.0.la
	$(AM_V_GEN) dumpbin.exe -exports .libs/libgmodule-2.0-0.dll | awk 'BEGIN { print "EXPORTS" } / +[[:digit:]]+ +[[:xdigit:]]+ +[[:xdigit:]]+/{ print $$4 }' > gmodule.def.tmp && mv gmodule.def.tmp gmodule.def

gmodule-2.0.lib: libgmodule-2.0.la gmodule.def
	$(AM_V_GEN) lib.exe -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgmodule-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:$(builddir)/gmodule.def -out:$@

.PHONY: files release

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`

dist-hook: $(BUILT_EXTRA_DIST)
	@files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  cp $$d/$$f $(distdir) || exit 1; done


install-data-local: install-ms-lib

uninstall-local: uninstall-ms-lib

if HAVE_GLIB_RUNTIME_LIBDIR
install-data-hook:
	mkdir -p $(DESTDIR)$(libdir)/$(GLIB_RUNTIME_LIBDIR)
	mv $(DESTDIR)$(libdir)/libgmodule-2.0.so.0 $(DESTDIR)$(libdir)/$(GLIB_RUNTIME_LIBDIR)
	mv $(DESTDIR)$(libdir)/libgmodule-2.0.so.0.$(LT_CURRENT).$(LT_REVISION) $(DESTDIR)$(libdir)/$(GLIB_RUNTIME_LIBDIR)
	rm -f $(DESTDIR)$(libdir)/libgmodule-2.0.so
	ln -s $(GLIB_RUNTIME_LIBDIR)/libgmodule-2.0.so.0.$(LT_CURRENT).$(LT_REVISION) $(DESTDIR)$(libdir)/libgmodule-2.0.so
endif
