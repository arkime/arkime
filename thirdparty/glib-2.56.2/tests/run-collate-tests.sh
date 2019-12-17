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
for I in ${srcdir:-.}/collate/*.in; do
  echo_v "Sorting $I"
  name=`basename $I .in`
  ./unicode-collate $I > collate.out
  if [ $? -eq 2 ]; then
    exit 0
  fi   
  diff collate.out ${srcdir:-.}/collate/$name.unicode || 
    fail "unexpected error when using g_utf8_collate() on $I"
  ./unicode-collate --key $I > collate.out
  diff collate.out ${srcdir:-.}/collate/$name.unicode ||
    fail "unexpected error when using g_utf8_collate_key() on $I"
  ./unicode-collate --file $I > collate.out
  diff collate.out ${srcdir:-.}/collate/$name.file ||
    fail "unexpected error when using g_utf8_collate_key_for_filename() on $I"
done

echo_v "All tests passed."
