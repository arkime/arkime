include $(top_srcdir)/glib.mk

LDADD = $(top_builddir)/gobject/libgobject-2.0.la $(top_builddir)/glib/libglib-2.0.la
AM_CPPFLAGS = $(gmodule_INCLUDES) $(GLIB_DEBUG_FLAGS)
DEFS = -DGLIB_DISABLE_DEPRECATION_WARNINGS -DG_LOG_DOMAIN=\"GLib\"
AM_CFLAGS = -g

# So far, only two gtester-ified cases
test_programs = \
	gvalue-test				\
	paramspec-test				\
	$(NULL)

# These are not yet gtester-ified, so mark them as for 'installed' only so we
# don't run them under the framework.  We will handle them manually below.
installed_test_programs = \
	deftype					\
	accumulator				\
	defaultiface				\
	dynamictype				\
	override				\
	signals					\
	singleton				\
	references				\
	testgobject				\
	$(NULL)

# Don't install these ones, and keep them out of 'make check' because they take too long...
noinst_PROGRAMS += \
	performance				\
	performance-threaded			\
	$(NULL)

# Run the 'installed' tests manually in-tree.
# This will cause them to be built even if installed tests are disabled.
check_PROGRAMS += $(installed_test_programs) $(installed_test_extra_programs)
TESTS += $(installed_test_programs)
AM_TESTS_ENVIRONMENT = \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset    \
	MALLOC_CHECK_=2                                         \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

accumulator_SOURCES = accumulator.c testmarshal.c testmarshal.h
signals_SOURCES = signals.c
defaultiface_SOURCES = defaultiface.c testmodule.c testmodule.h
dynamictype_SOURCES = dynamictype.c testmodule.c testmodule.h
testgobject_SOURCES = testgobject.c

if ENABLE_TIMELOOP
installed_test_programs += timeloop-closure
endif

# The marshal test requires running a binary, which means we cannot
# build it when cross-compiling
if !CROSS_COMPILING
glib_genmarshal=$(top_builddir)/gobject/glib-genmarshal

testmarshal.h: stamp-testmarshal.h
	@true
stamp-testmarshal.h: testmarshal.list $(glib_genmarshal)
	$(AM_V_GEN) $(glib_genmarshal) --prefix=test_marshal $(srcdir)/testmarshal.list --header >> xgen-gmh \
	&& (cmp -s xgen-gmh testmarshal.h 2>/dev/null || cp xgen-gmh testmarshal.h) \
	&& rm -f xgen-gmh xgen-gmh~ \
	&& echo timestamp > $@
testmarshal.c: testmarshal.h testmarshal.list $(glib_genmarshal)
	$(AM_V_GEN) (echo "#include \"testmarshal.h\""; $(glib_genmarshal) --prefix=test_marshal $(srcdir)/testmarshal.list --body) >> xgen-gmc \
	&& cp xgen-gmc testmarshal.c \
	&& rm -f xgen-gmc xgen-gmc~

BUILT_SOURCES += testmarshal.h testmarshal.c
CLEANFILES += stamp-testmarshal.h
EXTRA_DIST += testcommon.h testmarshal.list
BUILT_EXTRA_DIST += testmarshal.h testmarshal.c
endif # !CROSS_COMPILING

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)';				\
	for f in $$files; do					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi;	\
	  cp $$d/$$f $(distdir) || exit 1; done

distclean-local:
	if test $(srcdir) = .; then :; else	\
	    rm -f $(BUILT_EXTRA_DIST);		\
	fi
