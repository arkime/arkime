#! /bin/sh

fail ()
{
  echo "Test failed: $*"
  exit 1
}

echo_v ()
{
  if [ "$verbose" = "1" ]; then
    echo "$*"
  fi
}

error_out=/dev/null
if [ "$1" = "-v" ]; then
  verbose=1
  error_out=/dev/stderr
fi  

if [ -z "$LIBTOOL" ]; then
  if [ -f ../libtool ]; then
    LIBTOOL=../libtool
  else
    LIBTOOL=libtool
  fi
fi

echo_v "Running assert-msg-test"
OUT=$(./assert-msg-test 2>&1) && fail "assert-msg-test should abort"
echo "$OUT" | grep -q '^GLib:ERROR:.*assert-msg-test.c:.*:.*main.*: assertion failed: (42 < 0)' || \
  fail "does not print assertion message"

if ! type gdb >/dev/null 2>&1; then
  echo_v "Skipped (no gdb installed)"
  exit 0
fi

echo_v "Running gdb on assert-msg-test"
OUT=$($LIBTOOL --mode=execute gdb --batch -x ${srcdir:-.}/assert-msg-test.gdb ./assert-msg-test 2> $error_out) || fail "failed to run gdb"

echo_v "Checking if assert message is in __glib_assert_msg"
if ! echo "$OUT" | grep -q '^$1.*"GLib:ERROR:.*assert-msg-test.c:.*:.*main.*: assertion failed: (42 < 0)"'; then
  fail "__glib_assert_msg does not have assertion message"
fi

echo_v "All tests passed."
