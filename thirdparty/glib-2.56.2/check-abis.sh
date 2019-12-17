#!/bin/sh -e

list_leaked_symbols () {
	nm -D "$1" | grep ' T ' | cut -f 3 -d ' ' | egrep -v "$2"
}

check_symbols () {
	if [ "`list_leaked_symbols "$1" "$2" | wc -l`" -ne 0 ]; then
		echo File "$1" possibly leaking symbols:
		list_leaked_symbols "$1" "$2"
		exit 1
	fi
}

allowed="^_init$|^_fini$|^_ftext$|^g_"
allowed_in_libglib="${allowed}|^glib__private__$|^glib_gettext$|^glib_pgettext$|^glib_check_version$"
allowed_in_libgthread='^_init$|^_fini$|^_ftext$|^g_thread_init$|^g_thread_init_with_errorcheck_mutexes$'

check_symbols glib/.libs/libglib-2.0.so "$allowed_in_libglib"
check_symbols gthread/.libs/libgthread-2.0.so "$allowed_in_libgthread"
for file in gmodule/.libs/libgmodule-2.0.so gobject/.libs/libgobject-2.0.so gio/.libs/libgio-2.0.so; do
	check_symbols "$file" "$allowed"
done
