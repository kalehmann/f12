TESTS = check_f12 check_libfat12
check_PROGRAMS = check_f12 check_libfat12
check_libfat12_SOURCES = check_libfat12.c libfat12/check_libfat12_directory.c libfat12/check_libfat12_metadata.c libfat12/check_libfat12_name.c libfat12/check_libfat12_path.c libfat12/tests.h
check_libfat12_CFLAGS = @CHECK_CFLAGS@
check_libfat12_LDADD = $(top_builddir)/src/libfat12/libfat12.la @CHECK_LIBS@

check_f12_SOURCES = check_f12.c check_f12_format.c $(top_builddir)/src/format.c
check_f12_CFLAGS = @CHECK_CFLAGS@
check_f12_LDADD = @CHECK_LIBS@
