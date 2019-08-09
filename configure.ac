# AC_INIT - perform the initialization and verification
# Required arguments: package name and version
# Optional arguments: bug-report address, tarname and url.
AC_INIT([f12], [1.0], [mail@kalehmann.de])
# Is needed to use the archiver.
AM_PROG_AR

# Determine the c compiler to use.
AC_PROG_CC
# Activate libtool
AC_PROG_LIBTOOL

AC_DEFUN([AC_FEAT_COVERAGE], [
	dnl Check for --enable-coverage

	AC_MSG_CHECKING([whether to enable coverage])
	AC_ARG_ENABLE([coverage],
		[AS_HELP_STRING([--enable-coverage],
			[compile with coverage (default is no)])],
		[enable_coverage=yes],
		[enable_coverage=no])

	AM_CONDITIONAL([ENABLECOVERAGE], [test "x${enable_coverage}" = "xyes"])
	AC_MSG_RESULT($enable_coverage)

	AS_IF([test "x${enable_coverage}" = "xyes"], [
		AC_CHECK_TOOL([GCOV], [gcov], [:])
		AS_IF([test "x${GCOV}" = "x:"],
			[AC_MSG_ERROR([gcov is needed for coverage])])

		AC_CHECK_TOOL([LCOV], [lcov], [:])
		AS_IF([test "x${LCOV}" = ":"],
			[AC_MSG_ERROR([lcov is needed for coverage])])

		AC_CHECK_TOOL([GENHTML], [genhtml], [:])
		AS_IF([test "x${GENHTML}" = "x:"],
			[AC_MSG_ERROR([genhtml is needed for coverage])])

		COVERAGE_CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
		COVERAGE_LDFLAGS="-lgcov --coverage -static"

		AC_SUBST([COVERAGE_CFLAGS])
		AC_SUBST([COVERAGE_LDFLAGS])
	])
])

AC_FEAT_COVERAGE

PKG_CHECK_MODULES([CHECK], [check >= 0.9.6])

# Required for the generate makefiles.
# Only argument is a space seperated list of compiler options.
AM_INIT_AUTOMAKE([subdir-objects -Wall foreign])

AC_CONFIG_MACRO_DIRS([m4])

# Creates the files from templates (.in-files)
AC_CONFIG_FILES([
  Makefile
  src/libfat12/Makefile
  src/Makefile
  tests/Makefile
])
# Creates and runs the config.status file
AC_OUTPUT
