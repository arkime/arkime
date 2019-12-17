run
set print elements 0
# Work around https://sourceware.org/bugzilla/show_bug.cgi?id=22501
print *((char**) &__glib_assert_msg)
quit
