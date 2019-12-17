include $(top_srcdir)/glib-tap.mk

if USE_SYSTEM_PCRE
pcre_lib = $(PCRE_LIBS)
pcre_inc = $(PCRE_CFLAGS)
else
pcre_lib = $(top_builddir)/glib/pcre/libpcre.la
pcre_inc = -DPCRE_STATIC
endif

LDADD = $(top_builddir)/glib/libglib-2.0.la -lm
AM_CPPFLAGS = -g $(glib_INCLUDES) $(GLIB_DEBUG_FLAGS)
DEFS = -DG_LOG_DOMAIN=\"GLib\" -DEXEEXT=\"$(EXEEXT)\"
AM_CFLAGS = $(GLIB_WARN_CFLAGS) $(pcre_inc)

# The regex test uses pcre_version()
regex_LDADD = $(LDADD) $(pcre_lib)

# These tests corrupt the gcov bookkeeping, so we
# skip them. See bug 682133
spawn_singlethread_LDFLAGS = $(patsubst -lgcov,,$(LDFLAGS))
spawn_multithreaded_LDFLAGS = $(patsubst -lgcov,,$(LDFLAGS))

# -----------------------------------------------------------------------------

dist_test_data = \
	keyfiletest.ini			\
	pages.ini			\
	keyfile.c			\
	empty				\
	4096-random-bytes		\
	$(NULL)

dist_test_extra_scripts = \
	echo-script			\
	echo-script.bat			\
	$(NULL)

test_extra_programs = \
	test-spawn-echo			\
	$(NULL)

test_programs = \
	array-test			\
	asyncqueue			\
	base64				\
	bitlock				\
	bookmarkfile			\
	bytes				\
	cache				\
	checksum			\
	collate				\
	cond				\
	convert				\
	dataset				\
	date				\
	dir				\
	environment			\
	error				\
	fileutils			\
	gdatetime			\
	guuid				\
	gvariant			\
	hash				\
	hmac				\
	hook				\
	hostutils			\
	keyfile				\
	list				\
	logging				\
	mainloop			\
	mappedfile			\
	markup				\
	markup-parse			\
	markup-collect			\
	markup-escape			\
	markup-subparser		\
	mem-overflow			\
	mutex				\
	node				\
	once				\
	option-context			\
	option-argv0			\
	overflow			\
	overflow-fallback		\
	pattern				\
	private				\
	protocol			\
	queue				\
	rand				\
	rec-mutex			\
	regex				\
	rwlock				\
	scannerapi			\
	search-utils			\
	sequence			\
	shell				\
	slice				\
	slist				\
	sort				\
	spawn-multithreaded		\
	spawn-singlethread		\
	strfuncs			\
	string				\
	testing				\
	test-printf			\
	thread				\
	timeout				\
	timer				\
	tree				\
	utf8-performance		\
	utf8-pointer			\
	utf8-validate			\
	utf8-misc			\
	utils				\
	unicode				\
	uri				\
	1bit-mutex			\
	642026				\
	$(NULL)

uninstalled_test_programs = \
	$(NULL)

dist_test_data += \
	bookmarks/fail-01.xbel \
	bookmarks/fail-02.xbel \
	bookmarks/fail-03.xbel \
	bookmarks/fail-04.xbel \
	bookmarks/fail-05.xbel \
	bookmarks/fail-06.xbel \
	bookmarks/fail-07.xbel \
	bookmarks/fail-08.xbel \
	bookmarks/fail-09.xbel \
	bookmarks/fail-10.xbel \
	bookmarks/fail-11.xbel \
	bookmarks/fail-12.xbel \
	bookmarks/fail-13.xbel \
	bookmarks/fail-14.xbel \
	bookmarks/fail-15.xbel \
	bookmarks/fail-16.xbel \
	bookmarks/fail-17.xbel \
	bookmarks/valid-01.xbel \
	bookmarks/valid-02.xbel \
	bookmarks/valid-03.xbel \
	$(NULL)

markup_tests = \
	fail-1 fail-2 fail-3 fail-4 fail-5 \
	fail-6 fail-7 fail-8 fail-9 fail-10 \
	fail-11 fail-12 fail-13 fail-14 fail-15 \
	fail-16 fail-17 fail-18 fail-19 fail-20 \
	fail-21 fail-22 fail-23 fail-24 fail-25 \
	fail-26 fail-27 fail-28 fail-29 fail-30 \
	fail-31 fail-32 fail-33 fail-34 fail-35 \
	fail-36 fail-37 fail-38 fail-39 fail-40 \
	fail-41 fail-42 fail-43 fail-44 fail-45 \
	fail-46 fail-47 fail-48 fail-49 \
	valid-1 valid-2 valid-3 valid-4 valid-5 \
	valid-6 valid-7 valid-8 valid-9 valid-10 \
	valid-11 valid-12 valid-13 valid-14 valid-15 \
	valid-16 \
	$(NULL)

cdata_markup_tests = \
	valid-8 valid-16 \
	$(NULL)

dist_test_data += $(all_markup_data)
all_markup_data = \
	$(addprefix markups/,$(markup_tests:=.gmarkup) $(markup_tests:=.expected)) \
	$(addprefix markups/,$(cdata_markup_tests.=.cdata-as-text))

# -----------------------------------------------------------------------------

test_programs += atomic
atomic_CFLAGS  = $(AM_CFLAGS)
if HAVE_GCC
atomic_CFLAGS += -Wstrict-aliasing=2
endif

overflow_fallback_SOURCES = overflow.c
# FIXME: FLAGS?
overflow_fallback_FALGS = $(AM_CFLAGS) -D_GLIB_TEST_OVERFLOW_FALLBACK

test_programs += 642026-ec
642026_ec_SOURCES = 642026.c
642026_ec_CFLAGS = -DG_ERRORCHECK_MUTEXES

uninstalled_test_programs += 1bit-emufutex
1bit_emufutex_SOURCES  = 1bit-mutex.c
1bit_emufutex_CFLAGS = $(AM_CFLAGS) -DTEST_EMULATED_FUTEX

uninstalled_test_programs += gwakeup
gwakeup_SOURCES = gwakeuptest.c ../../glib/gwakeup.c

# -----------------------------------------------------------------------------

if OS_UNIX
test_programs += \
	unix				\
	include				\
	$(NULL)

# some testing of gtester functionality
XMLLINT = xmllint
GTESTER = $(top_builddir)/glib/gtester

gtester-xmllint-check: # check testreport xml with xmllint if present
	${GTESTER} -k --quiet -o tmpsample.xml --test-arg=--gtester-selftest ${GTESTER}
	${XMLLINT} --version 2>/dev/null; test "$$?" != 0 || ${XMLLINT} --noout tmpsample.xml

CLEANFILES += tmpsample.xml

check-am: gtester-xmllint-check

private_LDFLAGS = @G_THREAD_LIBS@
thread_LDFLAGS = @G_THREAD_LIBS@
endif

if HAVE_GCC
test_programs += \
	autoptr				\
	$(NULL)
endif

if OS_WIN32
test_programs += gpoll
endif

# -----------------------------------------------------------------------------

if HAVE_EVENTFD
uninstalled_test_programs += gwakeup-fallback
gwakeup_fallback_SOURCES = gwakeuptest.c ../../glib/gwakeup.c
gwakeup_fallback_CFLAGS = $(AM_CFLAGS) -DTEST_EVENTFD_FALLBACK
endif

# -----------------------------------------------------------------------------

EXTRA_DIST += bookmarks.xbel
