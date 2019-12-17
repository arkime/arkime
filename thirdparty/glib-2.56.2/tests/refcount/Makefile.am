include $(top_srcdir)/glib.mk

LDADD = $(top_builddir)/glib/libglib-2.0.la $(top_builddir)/gobject/libgobject-2.0.la
AM_CPPFLAGS = $(gmodule_INCLUDES) $(GLIB_DEBUG_FLAGS)
DEFS = -DGLIB_DISABLE_DEPRECATION_WARNINGS
AM_CFLAGS = -g

# These are not gtester tests, so handle them manually
TESTS += \
	closures				\
	objects					\
	objects2				\
	properties				\
	properties2				\
	properties3				\
	properties4				\
	signal1					\
	signal2					\
	signal3					\
	signal4					\
	$(NULL)

AM_TESTS_ENVIRONMENT = srcdir=$(srcdir) \
	LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset

signal1_SOURCES = signals.c
signal1_CFLAGS = -DTESTNUM=1 $(AM_CFLAGS)
signal2_SOURCES = signals.c
signal2_CFLAGS = -DTESTNUM=2 $(AM_CFLAGS)
signal3_SOURCES = signals.c
signal3_CFLAGS = -DTESTNUM=3 $(AM_CFLAGS)
signal4_SOURCES = signals.c
signal4_CFLAGS = -DTESTNUM=4 $(AM_CFLAGS)

check_PROGRAMS += $(TESTS)
