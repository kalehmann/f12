SUBDIRS = libfat12

bin_PROGRAMS = f12
f12_SOURCES = bpb.c bpb.h f12.c f12.h filesystem.c filesystem.h format.c format.h list.c list.h main.c
f12_LDADD = libfat12/libfat12.la

f12_CFLAGS = $(COVERAGE_CFLAGS)
f12_LDFLAGS = $(COVERAGE_LDFLAGS)

clean-local:
	rm -f *.gcov
	rm -f *.gcda
	rm -f *.gcno
