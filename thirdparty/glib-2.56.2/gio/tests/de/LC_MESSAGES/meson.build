# Doing this in a subdir, so we get it in the right build de/LC_MESSAGES subdir
# at least in cases where the build directory layout is mirrored.
test_mo =  custom_target('test.mo',
    input :   ['../../de.po'],
    output :  ['test.mo'],
    command : [msgfmt, '-o', '@OUTPUT@', '@INPUT@'])

test_mo_dir = meson.current_build_dir()
