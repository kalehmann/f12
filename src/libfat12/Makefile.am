lib_LTLIBRARIES = libfat12.la
libfat12_la_SOURCES = directory_entry.c error.c io.c metadata.c name.c path.c libfat12.h

libfat12_la_CFLAGS = $(COVERAGE_CFLAGS)

clean-local:
	rm -f *.gcov
	rm -f *.gcda
	rm -f *.gcno
