# Add the po directory to the SUBDIRS variable. This is needed for GNU gettext
SUBDIRS = po

# Include the macros in the m4 directories
ACLOCAL_AMFLAGS = -I m4

NASM = nasm
NASM_FLAGS = -f bin

default_bootcode_sources = boot/default/bootcode.asm
default_bootcode_includes = boot/default/dos_4_0_ebpb.inc
default_bootcode_binary = boot/default/bootcode.bin
default_bootcode_built_sources = boot/default/default_bootcode.c

$(default_bootcode_binary): $(default_bootcode_sources) $(default_bootcode_includes)
	$(NASM) $(NASM_FLAGS) \
		-o $(default_bootcode_binary) \
		-i boot/default/ \
		$(default_bootcode_sources)
$(default_bootcode_built_sources): $(default_bootcode_binary)
	xxd -i $(default_bootcode_binary) > $(default_bootcode_built_sources)

simple_bootloader_binary = boot/simple_bootloader/sibolo/bootloader.bin
simple_bootloader_built_sources = boot/simple_bootloader/simple_bootloader.c

$(simple_bootloader_binary):
	$(MAKE) -C boot/simple_bootloader/sibolo
$(simple_bootloader_built_sources): $(simple_bootloader_binary)
	xxd -i $(simple_bootloader_binary) | \
		sed s/boot_simple_bootloader_sibolo_bootloader_bin/boot_simple_bootloader/g \
		> $(simple_bootloader_built_sources)

# The main program(s) to build
bin_PROGRAMS = bin/f12
# The library to build
# noinst is a special prefix and means that the library is only used for
# building or testing the program and not installed on the target system.
noinst_LTLIBRARIES = src/libfat12/libfat12.la

# Source files for the library to build 
src_libfat12_libfat12_la_SOURCES = \
	src/libfat12/directory_entry.c \
	src/libfat12/error.c \
	src/libfat12/io.c \
	src/libfat12/metadata.c \
	src/libfat12/name.c \
	src/libfat12/path.c \
	src/libfat12/libfat12.h
# Additional compiler flags for the library to build
src_libfat12_libfat12_la_CFLAGS = $(COVERAGE_CFLAGS)

# Source files for the main program
bin_f12_SOURCES = \
	$(default_bootcode_built_sources) \
	$(simple_bootloader_built_sources) \
	boot/default/bootcode.h \
	boot/simple_bootloader/simple_bootloader.h \
	boot/simple_bootloader/sibolo_set_8_3_name.c \
	src/common.c \
	src/common.h \
	src/create.c \
	src/del.c \
	src/f12.h \
	src/get.c \
	src/info.c \
	src/list.c \
	src/list.h \
	src/move.c \
	src/put.c \
	src/main.c
# Additional compiler flags for the main program
bin_f12_CFLAGS = $(COVERAGE_CFLAGS)
# Libraries linked to the main program
bin_f12_LDADD = src/libfat12/libfat12.la
# Additional linker flags for the libraries linked to the main program
bin_f12_LDFLAGS = $(COVERAGE_LDFLAGS)
# Additional dependecies for the main program
EXTRA_bin_f12_DEPENDENCIES = default_bootcode

# All the programs that are build for tests
check_PROGRAMS = \
	tests/check_f12 \
	tests/libfat12/check_libfat12
# The programs that are executed as tests. While they are identical with the
# the check_PROGRAMS here, that may not always be the case, for example
# check_PROGRAMS may include additional programs that are only called by other
# tests and TESTS may include scripts that do not need to be build.
TESTS = $(check_PROGRAMS)

# Sources for the tests of the main program
tests_check_f12_SOURCES = \
	$(default_bootcode_built_sources) \
	$(simple_bootloader_built_sources) \
	boot/default/bootcode.h \
	boot/simple_bootloader/simple_bootloader.h \
	boot/simple_bootloader/sibolo_set_8_3_name.c \
	tests/check_f12.c \
	tests/check_f12_create.c \
	tests/check_f12_common.c \
	tests/check_f12_list.c \
	src/create.c \
	src/common.c \
	src/list.c
# Additional compiler flags for the tests of the main program
tests_check_f12_CFLAGS = @CHECK_CFLAGS@ $(COVERAGE_CFLAGS)
# Additional libraries for the tests of the main program
tests_check_f12_LDADD = \
	src/libfat12/libfat12.la \
	@CHECK_LIBS@ \
	$(COVERAGE_LDFLAGS)
# Sources for the tests of the libfat12 library
tests_libfat12_check_libfat12_SOURCES = \
	tests/libfat12/check_libfat12.c \
	tests/libfat12/check_libfat12_directory.c \
	tests/libfat12/check_libfat12_io.c \
	tests/libfat12/check_libfat12_metadata.c \
	tests/libfat12/check_libfat12_name.c \
	tests/libfat12/check_libfat12_path.c \
	tests/libfat12/tests.h
# Additional compiler flags for the tests of the libfat12 library
tests_libfat12_check_libfat12_CFLAGS = @CHECK_CFLAGS@ $(COVERAGE_CFLAGS)
# Additional libraries for tests of the libfat12 library
tests_libfat12_check_libfat12_LDADD = \
	src/libfat12/libfat12.la \
	@CHECK_LIBS@ \
	$(COVERAGE_LDFLAGS)

# Additional target for generation of coverage reports
if ENABLECOVERAGE
coverage-report:
	lcov --directory . --capture --output-file tests.info
	genhtml -o coverage-html/ tests.info
endif

# Additional target for beautification of the code
if ENABLEBEAUTIFY
beautify:
	indent $$(find . -name "*.[ch]")
endif

# Build and tag the docker image for development
docker-image:
	docker build \
		--tag "registry.kalehmann.de/testing/f12:latest" \
		--pull \
		--file Dockerfile \
		.

# Additional cleanup for coverage reports
clean-local:
	rm -f tests.info
	rm -rf coverage-html
	rm -f $(default_bootcode_binary)
	rm -f $(default_bootcode_built_sources)
	rm -f $(simple_bootloader_binary)
	rm -f $(simple_bootloader_built_sources)
	find . -type f \( -name ".gcov" -o -name "*.gcda" -o -name "*.gcno" \) -delete

.PHONY: beautify coverage-report default_bootcode

EXTRA_DIST = config.rpath m4/ChangeLog
AM_CPPFLAGS = -DLOCALEDIR='"$(localedir)"'
