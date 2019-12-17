include $(top_srcdir)/glib-tap.mk

LDADD = ../libgobject-2.0.la $(top_builddir)/glib/libglib-2.0.la
AM_CPPFLAGS = -g $(gobject_INCLUDES) $(GLIB_DEBUG_FLAGS)
DEFS = -DG_LOG_DOMAIN=\"GLib-GObject\"
AM_CFLAGS = $(GLIB_WARN_CFLAGS)

# -----------------------------------------------------------------------------

test_programs = \
	qdata				\
	boxed				\
	enums				\
	param				\
	threadtests			\
	dynamictests			\
	binding				\
	properties			\
	reference			\
	value				\
	type				\
	private				\
	closure				\
	object				\
	signal-handler			\
	$(NULL)

# -----------------------------------------------------------------------------

test_programs += ifaceproperties
ifaceproperties_SOURCES = ifaceproperties.c testcommon.h

# -----------------------------------------------------------------------------

# The marshalers test requires running a binary, so we cannot build it when
# cross-compiling

glib_genmarshal = $(top_builddir)/gobject/glib-genmarshal

test_programs += signals
signals_SOURCES = signals.c
nodist_signals_SOURCES = marshalers.c marshalers.h
signals.o: marshalers.h
CLEANFILES += marshalers.h marshalers.c
EXTRA_DIST += marshalers.list

marshalers.h: $(srcdir)/marshalers.list Makefile.am
	$(AM_V_GEN) $(glib_genmarshal) \
		--prefix=test \
		--valist-marshallers \
		--output=$@ \
		--quiet \
		--header \
		$<

marshalers.c: $(srcdir)/marshalers.list marshalers.h Makefile.am
	$(AM_V_GEN) $(glib_genmarshal) \
		--prefix=test \
		--valist-marshallers \
		--include-header=marshalers.h \
		--output=$@ \
		--quiet \
		--body \
		$<
