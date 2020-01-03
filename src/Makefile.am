bin_PROGRAMS = f12
lib_LTLIBRARIES = libfat12.la
libfat12_la_SOURCES = \
	libfat12/directory_entry.c \
	libfat12/error.c \
	libfat12/io.c \
	libfat12/metadata.c \
	libfat12/name.c \
	libfat12/path.c \
	libfat12/libfat12.h
libfat12_la_CFLAGS = $(COVERAGE_CFLAGS)

f12_SOURCES = \
	bpb.c \
	bpb.h \
	f12.c \
	f12.h \
	filesystem.c \
	filesystem.h \
	format.c \
	format.h \
	list.c \
	list.h \
	main.c
f12_LDADD = libfat12.la

f12_CFLAGS = $(COVERAGE_CFLAGS)
f12_LDFLAGS = $(COVERAGE_LDFLAGS)

clean-local:
	rm -f *.gcov
	rm -f *.gcda
	rm -f *.gcno
